# VideoCore-IV (vc4) enablement and zero-copy display on the Broadcom BCM23550 "Kona"

**Device:** Samsung Galaxy Grand Neo GT-I9060 (codename `samsung-baffinlite`)
**SoC:** Broadcom BCM23550 ("Kona" family), ARM Cortex-A7 quad, VideoCore-IV V3D GPU
**OS:** postmarketOS (mainline-based fork, branch `kona/7.1`), XFCE desktop
**Kernel base:** `7cfd77f5dd154214ae6453320d31f79ac0110c62`
**Scope of this commit:** the in-tree kernel changes that bring up the V3D GPU as a
render-only DRM device (`/dev/dri/renderD128`) on Kona. The out-of-tree display
shim and the desktop-stability findings are documented here for context but are
**not** part of this commit (see §4, §5).

> Status: the GPU renders on hardware, GPU→panel zero-copy scanout has been
> visually confirmed by an operator at the device, and the XFCE desktop runs on
> the GPU. This document records how each piece works, how it was validated, and
> the one runtime setting required for stability under sustained GL load.

---

## 1. Executive summary

- Mainline had **no** GPU support for the Broadcom Kona mobile SoCs. The V3D core
  is the same VideoCore-IV "gen 4" IP the mainline `vc4` driver already targets
  through its Cygnus binding, but three things were missing: a way to build
  `vc4` without the Raspberry Pi firmware dependency, a clock provider for the
  multimedia (MM) island that gates V3D, device-tree nodes, and a small `vc4`
  core fix so a *render-only* instance (no display pipeline) does not crash the
  kernel or blank the console at boot.
- This commit supplies exactly those pieces (9 files, ~216 insertions). After it,
  `vc4` binds at boot (`vc4-drm gpu: bound 3c00b000.v3d`), `renderD128` appears,
  and Mesa's shipped `vc4_dri.so` drives GLES on the V3D hardware
  (`GL_RENDERER = VC4 V3D 2.6`, not `llvmpipe`).
- On top of this, an out-of-tree display shim scans a GPU-rendered buffer out to
  the panel through the Broadcom **AXIPV** engine with **zero CPU copy**; this was
  proven at three evidence levels including a visual confirmation on the panel.
  That driver is intentionally kept separate from this commit (§4).
- Under **sustained** GL load with a compositing desktop at the shipped 256 MiB
  CMA size, the contiguous pool **fragments** and the DRM atomic flip pipeline
  wedges (`Failed to allocate from GEM DMA helper`). The fix that keeps 3D on the
  GPU, keeps zero-copy scanout, and touches neither the kernel nor the CMA size
  is to **disable the xfwm4 compositor** (§5). It is a per-user runtime setting,
  reversible with no X restart.

---

## 2. Platform background

The BCM23550 is a Kona-family SoC. Kona chips share **only** the VideoCore-IV
lineage with the Raspberry Pi's Broadcom parts; the surrounding SoC (clocks,
power islands, display path) is entirely different and mobile-oriented:

- **V3D GPU:** register block at `0x3C00B000` (len `0x1000`), `V3D_IDENT0 =
  0x02443356`, IRQ `GIC_SPI 148` (downstream `BCM_INT_ID_RESERVED148`, raw hwirq
  180 → SPI 148, level-high). This is the block `vc4` recognises as `VC4_GEN_4`
  via `brcm,cygnus-v3d` / `brcm,cygnus-vc4`.
- **MM (multimedia) CCU:** clock manager at `0x3C000000` gating the MM island
  (camera / DSI / V3D). The V3D AXI clock is `V3D_CLKGATE` at offset `0x270`;
  the MM AXI switch is `MM_AXI_SWITCH_CLKGATE` at `0x200`.
- **MM power island:** left **on by the bootloader**. There is no mainline MM
  power-domain (genpd) provider, so GPU bring-up depends on the bootloader
  leaving the island powered. If V3D register reads abort or `IDENT0` is wrong,
  the MM domain is the first suspect.
- **Display:** the panel is lit by the bootloader and mainline sees it as
  `simpledrm` on the `9e980000.framebuffer` carveout. The real display engine is
  Broadcom **AXIPV → PixelValve → DSI**, *not* the Raspberry Pi HVS/PixelValve
  that `vc4` KMS drives — which is precisely why `vc4` runs render-only here.
- **No RaspberryPi firmware:** Kona is `ARCH_BCM_MOBILE`, has no
  `raspberrypi,bcm2835-firmware` node, and `raspberrypi-firmware.h` provides
  no-op stubs when `CONFIG_RASPBERRYPI_FIRMWARE` is unset.

Ground truth for register offsets, IRQs and clock names came from the downstream
Samsung `cm-12.1` kernel for this exact phone and the Broadcom RDB headers
(§10).

---

## 3. The kernel enablement — what this commit contains

Diffstat (vs base `7cfd77f5`):

```
 .gitignore                                         |  1 -
 arch/arm/boot/dts/broadcom/bcm2166x-common.dtsi    | 34 +++++++++
 arch/arm/boot/dts/broadcom/bcm23550-samsung-baffinlite.dts | 19 +++++
 arch/arm/boot/dts/broadcom/bcm23550.dtsi           | 30 ++++++++
 drivers/clk/bcm/clk-bcm21664.c                     | 83 ++++++++++++++++++++++
 drivers/gpu/drm/vc4/Kconfig                        | 12 +++-
 drivers/gpu/drm/vc4/vc4_debugfs.c                  |  7 +-
 drivers/gpu/drm/vc4/vc4_drv.c                      | 34 +++++++--
 include/dt-bindings/clock/bcm21664.h               |  6 ++
 9 files changed, 216 insertions(+), 10 deletions(-)
```

### 3.1 `drivers/gpu/drm/vc4/Kconfig` — unblock `DRM_VC4` on Kona

Upstream gates `DRM_VC4` behind `RASPBERRYPI_FIRMWARE` (or `COMPILE_TEST`). Kona
has the same V3D but no RPi firmware, so the dependency is widened to include
`ARCH_BCM_MOBILE`:

```
depends on RASPBERRYPI_FIRMWARE || ((ARCH_BCM_MOBILE || COMPILE_TEST) && !RASPBERRYPI_FIRMWARE)
```

The `!RASPBERRYPI_FIRMWARE` guard is kept so `DRM_VC4=y` can never be paired with
`RASPBERRYPI_FIRMWARE=m` (a link failure). No `vc4_drv.c` change is needed for
the firmware handoff itself — it is skipped at runtime because the firmware DT
node is absent and the header stubs are no-ops.

### 3.2 MM CCU clock provider (`clk-bcm21664.c`, `bcm21664.h`)

Mainline had **no** MM CCU provider anywhere; it was written from scratch and
validated on hardware. Only the two clocks needed to bring up V3D are modelled:
`mm_switch_axi` (the MM AXI switch, parent) and `v3d_axi` (the V3D AXI clock).

Key design decision — **gate-only, no policy engine, no divider commit:**

- The Kona CCU *policy engine* (`LVM_EN` / `POLICY_CTL`) is coupled to the MM
  power-island DFS/voltage tables, which are not modelled. Driving it times out
  in a bounded way (`mm_ccu policy engine never started`) — it errors, it does
  not hang. The bootloader already programs the MM `POLICY*_MASK` bits (V3D +
  MM_SWITCH), so `clk-kona` can gate these clocks **directly**. This mirrors
  `root_ccu`, which likewise carries no policy control.
- Similarly the selector/divider are deliberately not modelled: a divider commit
  goes through `DIV_TRIG` and waits on the same never-clearing status bit. The
  bootloader's values are left in place (`var_312m`, `AXI_DIV PLL_SELECT=2`). The
  rate reported to the framework is therefore `var_312m` undivided — cosmetic;
  V3D bring-up does not depend on it.

New IDs in `bcm21664.h`: `BCM21664_DT_MM_CCU_COMPAT = "brcm,bcm21664-mm-ccu"`,
`BCM21664_MM_CCU_MM_SWITCH_AXI = 0`, `BCM21664_MM_CCU_V3D_AXI = 1`.

> **Do not "improve" this into a full policy/divider implementation** without
> first modelling the MM DFS/voltage context — that is the exact path that timed
> out on hardware. Two build-breakers were already fixed here and must not
> reappear: a stray `*/` inside a prose comment, and a blanket `*.bc` line in
> `.gitignore` (see §3.5).

### 3.3 Device tree

**`bcm2166x-common.dtsi`** — adds the `mm_ccu@3c000000` provider plus two
fixed-clock sources (`var_312m`, `var_500m`) modelled the same way the existing
`var_*` clocks are, on the assumption the bootloader has configured the PLLs.
(TODO: model these as real root-CCU gated PLL channels.)

**`bcm23550.dtsi`** — adds the `v3d@3c00b000` node (`brcm,cygnus-v3d`, clocked by
`mm_ccu BCM21664_MM_CCU_V3D_AXI`, IRQ `GIC_SPI 148`) and the register-less
`gpu` component-master node (`brcm,cygnus-vc4`) that makes `vc4` bind in
render-only mode. **Both are `status = "disabled"` by default** — they only work
where the MM power domain is powered, which is board/bootloader dependent.

**`bcm23550-samsung-baffinlite.dts`** — enables both nodes for this board
(`&v3d { status = "okay"; }`, `&gpu { status = "okay"; }`). This does **not**
touch the panel/scanout path; the console stays on `simpledrm`.

### 3.4 Render-only `vc4` support (`vc4_drv.c`, `vc4_debugfs.c`)

This is the core fix. A render-only instance binds no HVS or CRTC, so
`mode_config.num_crtc` stays `0` and there is no display pipeline. Upstream
`vc4_drm_bind()` unconditionally ran display-only steps, which on this instance:

1. **evicted the console framebuffer** — `aperture_remove_all_conflicting_devices()`
   ran before components bound and removed `simpledrm`'s aperture, blanking the
   console (`Console: switching to colour dummy device 80x30`, `/dev/fb*` gone)
   with nothing to replace it;
2. **oopsed the kernel** — `drm_client_setup_with_fourcc()` started fbdev
   emulation whose initial modeset entered `vc4_atomic_commit_tail()`, which
   dereferences a NULL `vc4->hvs` (fault at offset `0x8`);
3. warned every boot — `vc4_debugfs.c` called `drm_WARN_ON(vc4_hvs_debugfs_init())`
   with no HVS present.

The fix, applied after `vc4_kms_load()`:

```c
/* Render-only: no HVS/CRTC -> don't advertise KMS, and the DRM core will
 * reject modeset/atomic ioctls before they reach the NULL-hvs commit path.
 * The DRIVER_RENDER node is unaffected. */
if (drm->mode_config.num_crtc == 0)
        drm->driver_features &= ~(DRIVER_MODESET | DRIVER_ATOMIC);

if (drm->mode_config.num_crtc > 0) {
        ret = aperture_remove_all_conflicting_devices(driver->name);
        if (ret)
                goto err;
}

ret = drm_dev_register(drm, 0);
...
if (drm->mode_config.num_crtc > 0)
        drm_client_setup_with_fourcc(drm, DRM_FORMAT_RGB565);
```

and in `vc4_debugfs.c`, guarding the HVS debugfs init with `if (vc4->hvs)`
(mirroring the existing `if (vc4->v3d)` guard).

Why this is safe, verified against the DRM core source:

- `driver_features` is masked **per device** (`drm->driver_features`), never the
  shared `const struct drm_driver`.
- Clearing `DRIVER_MODESET` makes the DRM core reject KMS/atomic ioctls with
  `-EOPNOTSUPP` before they reach `vc4` (`drm_ioctl.c`).
- `drm_dev_register()` cleanly skips `drm_modeset_register_all()`, and the
  `drm_mode_config` teardown has no MODESET gate (no leak).
- The primary-minor allocation is unconditional and the `DRIVER_RENDER` minor is
  kept → both `card1` and `renderD128` appear. The poll worker is safe (0
  connectors) and V3D render paths never touch `hvs`.

> **Faulting line (`faddr2line`-resolved).** The boot oops PC resolves to
> `vc4_atomic_commit_tail+0x380` → `vc4_hvs_pv_muxing_commit at vc4_kms.c:239
> (inlined by) vc4_atomic_commit_tail at vc4_kms.c:462`. That routine opens with
> `struct vc4_hvs *hvs = vc4->hvs;` and drives the HVS display-commit path;
> `vc4->hvs` is NULL on a render-only instance, so this confirms — no longer
> merely infers from `0x8 == regs` — that the crash is the NULL-`hvs` display
> commit. `vc4_kms.c` is byte-identical to the base commit, so the function's
> compiled offset is valid for the build that oopsed. This is upstream-candidate
> material (render-only `vc4`); search dri-devel for prior art before submitting.

### 3.5 `.gitignore`

Removes the blanket `*.bc` line. `pmbootstrap build --src` copies the tree with
`rsync --exclude-from=.gitignore` (which has no `!negation` support); `*.bc`
dropped `kernel/time/timeconst.bc`, breaking the build
(`No rule to make target 'kernel/time/timeconst.bc'`).

### 3.6 Validation of the enablement

- `dmesg`: `vc4-drm gpu: bound 3c00b000.v3d`; no oops, no WARN, `simpledrm`
  console preserved.
- Nodes: `card0 = simpledrm`, `card1 = vc4-drm` (`brcm,cygnus-vc4`),
  `renderD128` present.
- `ioctl` V3D param test: `V3D_IDENT0 = 0x02443356` exact; `IDENT1 = 0xc1102426`;
  `IDENT2 = 0x00078121`; `CREATE_BO` + `MMAP_BO` pass.
- GLES: `GL_RENDERER = VC4 V3D 2.6 (Broadcom)` — **not** `llvmpipe`. An offscreen
  GLES2 clear+triangle reads back byte-exact (checksum `0x8d8d11e6`) and the V3D
  IRQ count climbs during render while a forced-software run fires none.
- The pivotal live `/dev/mem` experiment (mission 1, not to be repeated):
  writing `V3D_CLKGATE = 0x303` (SW + EN) made `IDENT0` read `0x02443356`;
  restoring `0x302` gated it back — exactly what `HW_SW_GATE(0x0270,16,0,1)`
  drives. The MM power island was confirmed **on at boot**, so no genpd/PMU
  driver is needed for a first render.

Kernel config lives in the pmaports APKBUILD, **not** in a `.config` in the tree
(the APKBUILD's `prepare()` overwrites `.config` on every build):
`device/testing/linux-postmarketos-brcm-kona/config-postmarketos-brcm-kona.armv7`
must set `CONFIG_DRM_VC4=m` (with `CONFIG_DRM=y`, `SND`/`SND_SOC=m`, standalone
`DRM_V3D` **not** set — vc4's built-in V3D is used).

---

## 4. Zero-copy GPU→panel scanout (AXIPV) — context, not in this commit

Mainline `vc4` KMS cannot drive this panel (the display engine is Broadcom
AXIPV/PixelValve/DSI, not the RPi HVS). A separate **out-of-tree** display path
was built and proven, and is documented here only for context. It is **not** part
of this commit and its source is kept outside this repo.

- The AXIPV scanout engine is at `0x3C006000`. The scanout base register at
  offset `+0x00` holds the framebuffer **physical address verbatim** (direct 1:1
  map, no IOMMU); `+0x04` (`CUR`) reports the currently-scanned frame address and
  `+0x30` (`STATUS`) carries `CUR_LINE` in bits 16–25.
- A small kernel shim allocates a linear CMA buffer, exports it as a dma-buf for
  the GPU to render into, and page-flips by writing the buffer's physical address
  to the AXIPV base register.
- Proven at three evidence levels: **L1** CPU checksum of GPU-written bytes
  matches the rendered gradient; **L2** AXIPV latched the GPU buffer's physical
  address as scanout at the frame boundary; **L3** with the operator present, the
  GPU-rendered gradient appeared on the physical panel and then cleanly restored.
  A 20-cycle soak passed with clean dma-buf/refcount teardown and no CMA leak.
- Full XFCE compositing then ran on this path (`card2`, via the display driver),
  with `ftrace` showing zero copy hits in the scanout path. Mesa auto-pairs the
  `vc4` render node with the separate display device via **kmsro** (its
  display-driver list is name-based).

---

## 5. Stability under sustained GL: the 256 MiB CMA fragmentation wedge

This is the main **operational** finding of the desktop-hardening work. It is a
runtime/configuration result — **no kernel change and no CMA-size change.**

### 5.1 Symptom

With the XFCE compositor **on** at the shipped 256 MiB CMA size, sustained GL
load wedges the display:

- **1 `glxgears`** (300×300, compositing on): wedges at **~6 min**. The kernel
  logs a burst of exactly **3** `vc4-drm gpu: [drm] *ERROR* Failed to allocate
  from GEM DMA helper`, `glxgears` FPS collapses 57 → 1, and the AXIPV `CUR`
  address goes static (the flip pipeline is stuck; `CUR_LINE` still sweeps, so
  the scanout engine itself is alive).
- **2–3 `glxgears`**: wedge in **~10 s**. So it is fragmentation-over-time from
  sustained compositing churn, not a fixed client count.
- Failures happen with **~190–210 MiB still free** in CMA → it is contiguous-
  allocation **fragmentation**, not total exhaustion.

Recovery is a full X restart (`systemctl restart lightdm`); `xfwm4 --replace`
alone does not clear it (the wedge is in the X/DRM atomic-flip path, below the
WM). On X teardown CMA reclaims to ~255/262 MiB → **no leak**, `panic_on_oops`
stays 1, zero oops.

### 5.2 Root cause

The **compositor** is the churn driver, not GLES itself. `xfwm4`'s compositor
redirects every window into an off-screen pixmap and maintains double-buffered
composite targets; on the zero-copy path these are contiguous CMA BOs. Sustained
allocation/free of those buffers fragments the pool until no contiguous run large
enough for a scanout/composite BO remains, and the DRM atomic commit fails at
`drm_gem_dma` allocation.

### 5.3 What did NOT work (and must not be retried)

- `echo 1 > /proc/sys/vm/compact_memory` before launch: does not prevent the
  wedge and *lowers* `CmaFree` (the kernel migrates movable pages into the CMA
  region).
- `vm.compaction_proactiveness = 100`: swaps the hard GEM-alloc wedge for
  self-recovering **migration-induced multi-second display freezes** plus jitter
  (a `frames=0` stall coincident with a `pgmigrate` burst, no GEM error). Fails
  the "no severe stutter" bar. `pgmigrate_fail` stayed persistently nonzero =
  memory resists compaction.

Net: at 256 MiB CMA, **no compaction strategy** makes sustained GL compositing
stable.

### 5.4 The fix: disable the xfwm4 compositor

```sh
xfconf-query -c xfwm4 -p /general/use_compositing -s false
```

- **Live and reversible**, no X restart, no login drop.
- Per-user xfconf setting → **persists across reboot** for that user with no
  drop-in file and no kernel change.
- `glamor` stays **on**, so DRI3/Present is intact and GL clients keep the V3D
  render node (`renderD128`, 0 `llvmpipe`). Removing the compositor removes its
  per-window redirect + double-buffered composite-target CMA-BO churn — i.e. the
  fragmentation driver — while leaving hardware GL untouched.
- Trade-off: no compositor shadows/transparency/vsync-composite; window chrome is
  CPU-drawn. Operator pre-accepted this.

### 5.5 Soak evidence (2026-09-23, attended)

Both runs: 256 MiB CMA unchanged, compositing off, `compaction_proactiveness`
untouched, 10 min, sampled ~15 s.

| Run | Before (compositing ON) | After (compositing OFF) |
|---|---|---|
| 1 × glxgears | wedged ~6 min | **clean full 10 min**, ~60 FPS, GEMerr flat |
| 2 × glxgears | wedged ~10 s | **clean full 10 min**, ~60 FPS each, GEMerr flat |

In both after-runs: **zero** new GEM alloc failures (GEMerr flat at its baseline
of 15 pre-existing lines), `0` `llvmpipe` threads (clients stayed on V3D), scanout
`CUR_LINE` sweeping throughout, `CmaFree` recovered at the end (no leak), Xorg
never restarted, `panic_on_oops = 1`. Logs:
`~/baffinlite-logs/20260923-task6-guard/soak-co-1c10m.log` and `soak-co-2c10m.log`.

### 5.6 Why not `Option "AccelMethod" "none"`

The originally-planned "disable glamor" approach is **rejected**. On the X.org
`modesetting` driver, DRI3/DRI2/Present GL acceleration is provided **by glamor**.
Disabling glamor would drop GL clients to `DRISWRAST`/`llvmpipe` — violating the
non-negotiable "3D stays on V3D" bar — **and** requires a login-dropping X
restart. Compositing-off reaches the same stability goal with neither cost.

### 5.7 Measurement methodology and a gotcha

Under compositing-off the desktop uses a single, in-place-updated **front
buffer** → there are **no page flips** → the AXIPV `CUR` *address* is static **by
design** (while `CUR_LINE` still sweeps, proving the engine is alive). The
CUR-flip wedge probe used with compositing-on is therefore **invalid** here.
Wedge is instead judged by `glxgears` FPS-collapse + a GEM-alloc-failure burst.
Tooling (in `~/baffinlite-tests/kona-guard/`): `soak_sample_co.sh` (per-sample
telemetry), `soak-monitor-co.sh` (host driver), `konax_frametime.py` (CUR-change
probe, compositing-**on** only).

---

## 6. Video: hardware limitation (stated honestly)

The BCM23550 Kona exposes **no hardware video decoder** — no `/dev/video*`, no
VAAPI, no `libgstlibav`. Video **decode is CPU-only**. The GPU can accelerate
video *present/scale* via GLES (`glimagesink`; `libgstopengl.so` is present), but
`gst-launch`/`mpv`/`ffmpeg` are not installed (only `gst-play-1.0`), so a full
`glimagesink` present-proof is still pending. **Do not claim hardware video
acceleration that does not exist on this SoC.**

---

## 7. WiFi (brcmfmac) — operational note

The BCM4330 WiFi is an SDIO chip driven by `brcmfmac`. At boot it sometimes fails
SDIO bring-up (`brcmf_bus_started: failed: -110` / "dongle is not responding"),
so `wlan0` is never created. A module reload re-runs the SDIO download/attach and
recovers it:

```sh
rmmod brcmfmac_wcc; rmmod brcmfmac; modprobe brcmfmac
```

Residual `-110` on info-query dcmds afterwards is benign. There is no NVRAM MAC
(a random MAC is used) and no `clm_blob` (limited channel set). This is an
operational recovery, not a kernel fix; it may recur on reboot (a small systemd
unit that reloads `brcmfmac` when `wlan0` is absent would auto-recover it).

---

## 8. Reproduction / operational runbook

1. Build the kernel with `CONFIG_DRM_VC4=m` via the pmaports APKBUILD (see §3.6);
   apply this commit's tree. Deliver modules via `apk` (not `flash_kernel` alone
   — that does not update `/lib/modules`), and verify the installed `vc4.ko.zst`
   matches the built one by SHA-256 (a stale module was a real bug once).
2. Boot. Confirm `vc4-drm gpu: bound 3c00b000.v3d`, `renderD128` present,
   `GL_RENDERER = VC4 V3D 2.6`.
3. For the desktop stability fix, as the desktop user:
   `xfconf-query -c xfwm4 -p /general/use_compositing -s false` (persists).
4. If `wlan0` is missing after boot: `rmmod brcmfmac_wcc; rmmod brcmfmac;
   modprobe brcmfmac` (§7).

Safety invariants observed throughout (attended sessions): `panic_on_oops = 1` at
rest; only one Xorg holds the display node; stop `lightdm` before loading/
unloading display modules; no Download Mode / boot-partition writes; ask before
any X/session restart that could drop the operator's login.

---

## 9. Known limitations and TODO

- **MM power domain** is not modelled (relies on bootloader). A genpd provider is
  future work; without it, GPU bring-up is bootloader-dependent.
- **MM CCU** is gate-only: policy engine and divider are intentionally not driven
  (§3.2). Reported clock rate is cosmetically wrong. Modelling them needs the MM
  DFS/voltage context first.
- **Render-only `vc4`** patch is upstream-candidate; the oops's faulting site is
  now `faddr2line`-resolved to the NULL-`hvs` `vc4_hvs_pv_muxing_commit` path
  (§3.4).
- **Panel/DSI driver** (mainline, re-init-capable) is deferred; the panel is lit
  by bootloader handoff and the zero-copy display path is out-of-tree (§4).
- **Sustained GL** at 256 MiB CMA is stable only with the compositor off; a
  larger CMA or a compositor that does not churn contiguous BOs would lift that
  constraint but is out of scope (no CMA/kernel change allowed here).
- **Video** decode is CPU-only (§6); GPU present-path proof pending.

---

## 10. Sources and references

### Project infrastructure / precedent
- Broadcom Kona mainlining wiki — https://wiki.dissonant.dev/wiki/Mainline:Broadcom_Kona
  (and `/Clocks`, `/Power_Manager` subpages: CCU policy engine, six power islands)
- postmarketOS Broadcom Kona — https://wiki.postmarketos.org/wiki/Broadcom_Kona
  (Kona shares only VideoCore-IV with RPi's Broadcom chips)
- postmarketOS device page — https://wiki.postmarketos.org/wiki/Samsung_Galaxy_Grand_Neo_(samsung-baffinlite)
- `bcm-kona-mainline/linux` fork + issue #1 "Hardware 3D acceleration" (unstarted before this work) — https://github.com/bcm-kona-mainline/linux
- BCM21664T "kylepro" CCU bring-up failure (precedent for the policy-engine
  timeout) — https://gitlab.com/postmarketOS/pmaports/issues/314
- Downstream Samsung kernel (register/IRQ/clock ground truth) —
  https://github.com/knuxdroid/android_kernel_samsung_baffinlite

### Hardware / register documentation
- Broadcom "VideoCore IV 3D Architecture Guide" (AG100-R) — https://docs.broadcom.com/doc/12358545
- VideoCore-IV Programmer's Manual (community) — https://github.com/hermanhermitage/videocoreiv/wiki/VideoCore-IV-Programmers-Manual
- `brcm,cygnus-v3d` / `brcm,cygnus-vc4` binding origin — Patchwork "ARM: dts:
  bcm-cygnus: Add 911360's V3D device"

### Kernel clock framework (Kona CCU / bus clocks)
- Artur Weber "clk: bcm: kona: Add bus clock support" series (reviewed by Florian
  Fainelli, Alex Elder) — https://patchwork.kernel.org/project/linux-clk/
- In-tree commits supplying the macros: `a95ec2dd50e3`, `232308ecfe88`,
  `8ec20922302e` (`HW_SW_GATE`, `KONA_CLK(..., bus)`, policy-bit config)

### vc4 / aperture / DRM core
- vc4 → `aperture_remove_all_conflicting_devices()` conversion —
  https://gitlab.freedesktop.org/drm/misc/kernel/-/commit/7e89e4365fd36ca006670788ec2c1527bfd0c61c
- Mainline `vc4_drv.c` — https://code.woboq.org/linux/linux/drivers/gpu/drm/vc4/vc4_drv.c.html
- `raspberrypi-firmware.h` no-op stubs when `CONFIG_RASPBERRYPI_FIRMWARE` unset
  (why the Kconfig-only fix works) — kernel source `include/linux/firmware/raspberrypi.h`
- Kernel DRM docs read in-tree: `Documentation/gpu/{drm-kms,drm-kms-helpers,drm-mm,vc4}.rst`;
  `drivers/gpu/drm/drm_gem_dma_helper.c`, `drivers/video/aperture.c`

### Mesa kmsro (render-only + separate display)
- KMSRO Mesa infrastructure — https://www.phoronix.com/news/KMSRO-Mesa-Infrastructure-Patch
- kmsro name-based display list — https://lore.ptxdist.org/ptxdist/20201130133915.29533-1-l.stach@pengutronix.de/T/
- Mesa lima docs (render-only + KMS display concept) — https://docs.mesa3d.org/drivers/lima.html

### Display / DSI (for the out-of-tree AXIPV path, §4)
- Downstream `kona_fb.c` / `dsi.c` (AXIPV, `id_seq` panel-ID, reset timing) —
  https://android.googlesource.com/kernel/bcm/ (tetra/wear branches)
- Mainline VC4 DSI register-naming reference — https://elixir.bootlin.com/linux/v5.6/source/drivers/gpu/drm/vc4/vc4_dsi.c

### Build / delivery tooling
- pmbootstrap usage (`--src`) — https://docs.postmarketos.org/pmbootstrap/usage.html
- postmarketOS kernel aport — https://gitlab.postmarketos.org/postmarketOS/pmaports/-/tree/main/device/testing/linux-postmarketos-brcm-kona

### Authoritative references for the mechanisms discussed (verified 2026-09-23)
- **Linux CMA / DMA contiguous alloc / DRM GEM DMA helper** —
  https://docs.kernel.org/gpu/drm-mm.html (GEM DMA / `drm_gem_dma_helper`:
  buffers presented "as a contiguous chunk of memory" for devices without
  scatter-gather/IOMMU); https://docs.kernel.org/core-api/dma-api.html
  (coherent/contiguous allocation, streaming-map failure on non-contiguous
  memory). CMA's purpose and the fragmentation problem it faces:
  https://elinux.org/images/2/23/LinuxCMA-cewg43.pdf
- **vc4 driver / V3D** — https://docs.kernel.org/gpu/vc4.html (Broadcom
  VideoCore-IV, "OpenGL ES 2.0-compatible 3D engine called V3D"). *Note:* the
  kernel page does not use the term "render-only"; the render/display split is
  stated authoritatively by Mesa (below).
- **Render-only V3D + separate KMS display (kmsro)** —
  https://docs.mesa3d.org/drivers/v3d.html ("the kernel uses the VC4 DRM driver
  for display support" while "executing rendering on the V3D kernel module,"
  coordinated by kmsro — exactly this SoC's split);
  https://docs.mesa3d.org/drivers/lima.html (clearest general statement of the
  render-node-paired-with-separate-display-driver pattern via `kmsro`);
  kmsro origin patch series — https://lists.freedesktop.org/archives/mesa-dev/2019-January/214067.html
- **X.org modesetting + glamor / AccelMethod** —
  https://man.archlinux.org/man/modesetting.4 (glamor is the acceleration path;
  `Option "AccelMethod"` is `glamor`|`none`, default `glamor`; DRI3/PageFlip/
  Present; without glamor a shadow/software framebuffer is used);
  https://wiki.archlinux.org/title/Intel_graphics (modesetting accelerates "via
  glamor"). *Note:* the freedesktop Glamor wiki returned HTTP 403 and is not
  cited; the man page + Arch wiki substantiate the "AccelMethod none → software"
  claim.
- **xfwm4 compositor** — https://docs.xfce.org/xfce/xfwm4/wmtweaks (xfwm4 ships
  its own Compositor); https://forum.xfce.org/viewtopic.php?id=14405 (the exact
  `xfconf-query -c xfwm4 -p /general/use_compositing` toggle). *Note:* per-window
  buffer redirection / double buffering is standard X Composite redirection; the
  Xfce docs confirm the compositor and the property but do not spell out the
  buffering mechanism in prose.
- **brcmfmac SDIO bring-up / CLM blob** —
  https://wireless.docs.kernel.org/en/latest/en/users/drivers/brcm80211.html
  (firmware/NVRAM layout under `/lib/firmware/brcm`; SDIO needs a supplied NVRAM
  file). The CLM-blob role and the exact `brcmf_bus_started: failed` / `-110
  (ETIMEDOUT) "dongle is not responding"` string are **not** in the kernel docs;
  cite the driver source instead —
  https://github.com/torvalds/linux/blob/master/drivers/net/wireless/broadcom/brcm80211/brcmfmac/common.c
  (`brcmf_c_process_clm_blob`, "no clm_blob available … device may have limited
  channels") and the `brcmfmac/` SDIO tree for the bring-up/timeout path.

The local archive `~/baffinlite-REFERENCE.md` §D carries the complete mission
bibliography (community wikis, downstream sources, panel/DSI references,
build-tooling links).


