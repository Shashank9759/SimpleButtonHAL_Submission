# SimpleButton — Execution Report

## Build Instructions

### Step 1: Place files in the AOSP tree

| Project path | AOSP destination |
|-------------|-----------------|
| `kernel/drivers/misc/simplebutton/` | `$(KERNEL_SRC)/drivers/misc/simplebutton/` |
| `hal/hardware/interfaces/simplebutton/` | `hardware/interfaces/simplebutton/` |
| `sepolicy/*.te`, `file_contexts`, etc. | `device/<vendor>/<device>/sepolicy/vendor/` |
| `framework/frameworks/base/core/java/android/os/*` | `frameworks/base/core/java/android/os/` |
| `framework/frameworks/base/services/core/java/com/android/server/SimpleButtonService.java` | `frameworks/base/services/core/java/com/android/server/` |
| `app/packages/apps/SimpleButtonApp/` | `packages/apps/SimpleButtonApp/` |

### Step 2: Modify existing AOSP files

See `AOSP_INTEGRATION.md` for exact snippets. Summary:

1. **`drivers/misc/Kconfig`** — add `source "drivers/misc/simplebutton/Kconfig"`
2. **`drivers/misc/Makefile`** — add `obj-y += simplebutton/`
3. **`<defconfig>`** — add `CONFIG_SIMPLEBUTTON=y`
4. **`frameworks/base/core/java/android/content/Context.java`** — add `SIMPLE_BUTTON_SERVICE`
5. **`frameworks/base/core/java/android/app/SystemServiceRegistry.java`** — register manager
6. **`frameworks/base/services/java/com/android/server/SystemServer.java`** — start service
7. **`frameworks/base/Android.bp`** — add AIDL to sources
8. **`frameworks/base/services/Android.bp`** — add `SimpleButtonService.java`
9. **`device/<vendor>/<device>/device.mk`** — add packages
10. **`device/<vendor>/<device>/manifest.xml`** — declare HAL
11. **SELinux** — merge policy files

### Step 3: Build

```bash
# Set up build environment
source build/envsetup.sh
lunch <target>-userdebug          # e.g. aosp_arm64-userdebug

# Build individual components (incremental)
m android.hardware.simplebutton-service
m SimpleButtonApp
m framework
m sepolicy

# Or full system image
m -j$(nproc)
```

### Step 4: Flash (on target hardware)

```bash
fastboot flashall -w
# Or flash specific partitions:
fastboot flash vendor vendor.img
fastboot flash system system.img
```

---

## Test Instructions

### Layer 1 — Kernel (no Android needed)

```bash
# Load module (if built as module)
adb shell insmod /vendor/lib/modules/simplebutton.ko

# Verify sysfs nodes exist
adb shell ls -la /sys/class/misc/simplebutton/

# Read button value
adb shell cat /sys/class/misc/simplebutton/value
# Expected output: 0

# Trigger from shell
adb shell "echo 1 > /sys/class/misc/simplebutton/trigger"

# Check dmesg for kernel log
adb shell dmesg | grep simplebutton
# Expected: simplebutton: clicked

# Value should be back to 0
adb shell cat /sys/class/misc/simplebutton/value
# Expected output: 0
```

### Layer 2-3 — HAL service

```bash
# Check HAL service is running
adb shell ps -A | grep simplebutton

# Check service registration
adb shell service list | grep simplebutton

# Manual HAL test via lshal (if available)
adb shell lshal debug android.hardware.simplebutton@1.0::ISimpleButton/default
```

### Layer 4 — SELinux

```bash
# Check for denials
adb shell dmesg | grep "avc: denied" | grep simplebutton

# Verify sysfs label
adb shell ls -laZ /sys/class/misc/simplebutton/

# Verify HAL binary label
adb shell ls -laZ /vendor/bin/hw/android.hardware.simplebutton-service
```

### Layer 5 — Framework service

```bash
# Check framework service is registered
adb shell service list | grep simplebutton

# Check system_server logs
adb logcat -s SimpleButtonService SimpleButtonManager
```

### Layer 6 — App (standalone demo — tested)

The app layer was built and tested as a **standalone Gradle project** in Android Studio on an **Android API 34 emulator** (no AOSP system service required for this demo). Logcat proof was captured during testing and is attached to the submission.

```bash
# Install and launch the standalone demo app
adb install -r SimpleButtonApp.apk
adb shell am start -n com.simpleenergy.buttonapp/.MainActivity

# Capture logcat proof (filter by tag used in MainActivity.kt)
adb logcat -s SimpleButton
```

**Observed results on API 34 emulator:**

| Action | UI (status TextView) | Logcat output |
|--------|----------------------|---------------|
| Press **READ VALUE** | Toggles `Button value: 0` ↔ `Button value: 1` | (no log on READ in standalone demo) |
| Press **TRIGGER CLICK** | `Triggered! Check logcat` | `D/SimpleButton: simplebutton: clicked` |

**Example logcat proof (attached to submission):**
```
D/SimpleButton: simplebutton: clicked
D/SimpleButton: simplebutton: clicked
```

> **Note:** On a full AOSP build with all 6 layers integrated, TRIGGER would additionally produce `simplebutton: clicked` in **dmesg** via the kernel driver. The standalone demo simulates the app-layer behaviour and logcat output only.

### Layer 6 — App (full AOSP end-to-end — requires target hardware)

```bash
# Install/launch app (pre-installed in system image)
adb shell am start -n com.simpleenergy.buttonapp/.MainActivity

# Watch all SimpleButton logs
adb logcat | grep -E "SimpleButton|simplebutton"

# Press READ VALUE in app → expect:
#   UI: "Button value: 0" or "Button value: 1" (from kernel via full stack)
#   logcat: SimpleButtonService / HAL debug logs

# Press TRIGGER CLICK in app → expect:
#   logcat: D/SimpleButton: simplebutton: clicked (or framework/HAL equivalent)
#   dmesg:  simplebutton: clicked
#   UI: "Triggered! Check logcat"
```

### Quick validation script

```bash
#!/bin/bash
echo "=== Sysfs ==="
adb shell cat /sys/class/misc/simplebutton/value

echo "=== Trigger via sysfs ==="
adb shell "echo 1 > /sys/class/misc/simplebutton/trigger"

echo "=== dmesg ==="
adb shell dmesg | grep simplebutton | tail -5

echo "=== HAL process ==="
adb shell ps -A | grep simplebutton

echo "=== Framework service ==="
adb shell service list | grep simplebutton

echo "=== Launch app ==="
adb shell am start -n com.simpleenergy.buttonapp/.MainActivity
```

---

## Development Environment Note

**Host machine:** macOS Apple Silicon (M1, 16 GB RAM)

**What was tested locally:**
- **Layer 6 (App)** — built and run in Android Studio on an **Android API 34 emulator**. READ/TRIGGER button behaviour and logcat output (`D/SimpleButton: simplebutton: clicked`) were verified. Logcat proof is attached to the submission.

**What could not be tested locally:**
- **Layers 1–5 (Kernel → SELinux → HAL → Framework)** — full AOSP source compilation and end-to-end kernel/HAL testing were **not** performed on this machine.

**Why:**
Full AOSP builds require an **x86-64 Linux host**. Apple Silicon (ARM) Macs cannot natively compile the complete AOSP tree. While cross-compilation and remote Linux VMs are possible workarounds, this project was developed and validated at the code/architecture level on macOS, with **only the app layer exercised on a live device (emulator)**.

**What is required for full end-to-end validation:**
- Linux x86-64 build host (physical machine, VM, or cloud instance)
- AOSP source tree with all SimpleButton files integrated per `AOSP_INTEGRATION.md`
- Target embedded hardware (or AOSP-compatible device image) flashed with the custom build
- Kernel driver enabled (`CONFIG_SIMPLEBUTTON=y`) on the target device

All Layers 1–5 code follows AOSP conventions and was cross-checked against official documentation and reference HAL implementations (e.g. vibrator, sensors). The architecture is structured for integration; runtime validation of the lower stack is deferred to a Linux x86-64 + target hardware environment.

---

## Challenges & Trade-offs

### Challenges

1. **Cross-compilation environment** — Full AOSP builds cannot run natively on Apple Silicon macOS. A Linux x86-64 VM or cloud instance is required for end-to-end validation. Code was structured per AOSP conventions and cross-checked against reference HALs.

2. **SELinux policy completeness** — SELinux rules are device-tree-specific. The provided policies cover the core domains but may need additional `allow` rules depending on the exact SoC vendor sepolicy base (e.g., Qualcomm vs MediaTek have different `system_server` attribute sets).

3. **Kernel sysfs path stability** — The sysfs path `/sys/class/misc/simplebutton/` depends on the misc device registering successfully. If `CONFIG_SIMPLEBUTTON` is not enabled in the kernel defconfig, all upper layers will fail with file-not-found errors that look like HAL bugs.

4. **Framework `@hide` API access** — `SimpleButtonManager` is a hidden API. The production AOSP app must be platform-signed with `sharedUserId=android.uid.system`. The standalone demo app avoids this by simulating behaviour locally in Android Studio.

### Trade-offs

| Decision | Chose | Over | Reason |
|----------|-------|------|--------|
| HAL IPC | AIDL (stable) | HIDL | HIDL deprecated; AIDL is current standard for Android 13+ |
| Driver type | misc device | platform_device + char dev | Simpler for single-device simulation; less boilerplate |
| HAL mode | Binderized | Passthrough | Treble compliance, SELinux isolation, crash containment |
| Trigger semantics | Set-to-1 then reset-to-0 | Latch until read | Matches assignment spec; printk fires on trigger |
| Framework pattern | Direct ServiceManager | SystemService lifecycle | Simpler for learning assignment; production would use `SystemService` class |
| App testing | Standalone demo on API 34 emulator | Full AOSP on-device test | macOS M1 cannot build/flash AOSP; emulator proves app UI + logcat path |

### Limitation: macOS M1 development environment

Because development was on **macOS Apple Silicon (M1)**, only the **app layer (Layer 6)** could be run and verified on a live Android runtime (API 34 emulator). The kernel driver, HAL service, SELinux policies, and framework system service were **written and structured for AOSP integration** but were **not runtime-tested** on this machine.

**What the full test would look like on real embedded hardware:**

1. **Build** — On a Linux x86-64 host: `source build/envsetup.sh && lunch <target>-userdebug && m -j$(nproc)`
2. **Flash** — `fastboot flashall -w` onto target EV/automotive board
3. **Kernel (Layer 1)** — `adb shell cat /sys/class/misc/simplebutton/value` returns `0`; `echo 1 > .../trigger` prints `simplebutton: clicked` in `adb shell dmesg`
4. **HAL (Layers 2–3)** — `adb shell ps -A | grep simplebutton` shows HAL process running; `service list` shows `android.hardware.simplebutton.ISimpleButton/default`
5. **SELinux (Layer 4)** — `adb shell dmesg | grep "avc: denied" | grep simplebutton` returns no denials
6. **Framework (Layer 5)** — `adb shell service list | grep simplebutton` shows framework service registered
7. **App (Layer 6)** — Launch `SimpleButtonApp` (platform-signed, full stack): READ returns real kernel value; TRIGGER produces both logcat **and** dmesg `simplebutton: clicked`

This end-to-end path validates that a button press in the UI travels through Binder → system_server → HAL → sysfs → kernel driver — the complete 6-layer stack as designed in `ARCHITECTURE.md`.

---

## Submission Checklist

**Completed on macOS M1 + API 34 emulator:**
- [x] App layer built and tested in Android Studio (standalone demo)
- [x] READ button toggles value 0/1 in UI
- [x] TRIGGER button logs `D/SimpleButton: simplebutton: clicked` to logcat
- [x] Logcat proof captured and attached to submission

**Requires Linux x86-64 host + target hardware:**
- [ ] All files placed in correct AOSP paths
- [ ] `AOSP_INTEGRATION.md` patches applied
- [ ] `CONFIG_SIMPLEBUTTON=y` in kernel defconfig
- [ ] SELinux policies merged
- [ ] `PRODUCT_PACKAGES` includes HAL service + app
- [ ] VINTF manifest declares HAL
- [ ] Full build succeeds (`m -j$(nproc)`)
- [ ] Sysfs nodes accessible on device
- [ ] `dmesg` shows "simplebutton: clicked" on trigger
- [ ] App READ and TRIGGER buttons work end-to-end through full stack
- [ ] No SELinux `avc: denied` in `dmesg`
