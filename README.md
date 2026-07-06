# SimpleButton HAL — Full-Stack Android Hardware Button Simulation

Repository: [https://github.com/Shashank9759/SimpleButtonHAL_Submission.git](https://github.com/Shashank9759/SimpleButtonHAL_Submission.git)

A complete 6-layer Android Open Source Project (AOSP) stack that simulates a hardware button — from a Linux kernel misc device driver up through a privileged Kotlin test application. Built for Android 14 (API 34) using stable AIDL HAL, the Soong build system (`Android.bp`), and SELinux type enforcement.

---

## Repository Layout

```
SimpleButtonHAL_Submission/
├── README.md                  ← This file (project overview and end-to-end flow)
├── app_screenshot.png         ← UI screenshot of SimpleButtonApp on emulator
├── logcat_proof.png           ← Logcat proof of TRIGGER button output
└── source_code/
    └── SimpleButton/
        ├── kernel/            Layer 1 — Linux kernel misc device driver
        ├── hal/               Layers 2–3 — AIDL HAL interface + C++ implementation
        ├── sepolicy/          Layer 4 — SELinux policies
        ├── framework/         Layer 5 — Java system service + Manager API
        ├── app/               Layer 6 — Privileged Kotlin test app
        ├── ARCHITECTURE.md    Detailed system design
        ├── AOSP_INTEGRATION.md  Patches for existing AOSP source files
        └── EXECUTION_REPORT.md  Build, test, and validation notes
```

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│  LAYER 6 — APP (Kotlin)                                             │
│  SimpleButtonApp: READ / TRIGGER buttons                            │
│  getSystemService("simplebutton") → SimpleButtonManager             │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ Binder IPC
┌──────────────────────────────▼──────────────────────────────────────┐
│  LAYER 5 — FRAMEWORK (Java)                                         │
│  SimpleButtonManager → ISimpleButtonService → SimpleButtonService   │
│  (runs inside system_server process)                                │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ Binder IPC
┌──────────────────────────────▼──────────────────────────────────────┐
│  LAYER 4 — SELINUX                                                │
│  Enforces which process/domain can access sysfs, HAL, services      │
└──────────────────────────────┬────────────────────────────────────┘
                               │ (policy gates all layers below)
┌──────────────────────────────▼──────────────────────────────────────┐
│  LAYER 3 — NATIVE SERVICE (C++)                                     │
│  android.hardware.simplebutton-service                              │
│  SimpleButton.cpp reads/writes sysfs nodes                          │
└──────────────────────────────┬────────────────────────────────────┘
                               │ open() / read() / write()
┌──────────────────────────────▼──────────────────────────────────────┐
│  LAYER 2 — HAL INTERFACE (AIDL)                                   │
│  ISimpleButton.aidl — stable vendor interface contract              │
└──────────────────────────────┬────────────────────────────────────┘
                               │ sysfs file I/O
┌──────────────────────────────▼──────────────────────────────────────┐
│  LAYER 1 — KERNEL DRIVER (C)                                        │
│  simplebutton misc driver                                           │
│  /sys/class/misc/simplebutton/value                                 │
│  /sys/class/misc/simplebutton/trigger                               │
└─────────────────────────────────────────────────────────────────────┘
```

| Layer | Component | Responsibility |
|-------|-----------|----------------|
| 1 | `kernel/drivers/misc/simplebutton/simplebutton.c` | Owns button state in kernel memory; exposes sysfs, ioctl, and poll |
| 2 | `hal/.../ISimpleButton.aidl` | Defines stable `getValue()` and `triggerClick()` HAL contract |
| 3 | `hal/.../SimpleButton.cpp`, `service.cpp` | Binderized HAL process that reads/writes sysfs on behalf of upper layers |
| 4 | `sepolicy/*.te`, `file_contexts`, `service_contexts` | Labels sysfs, HAL binary, and services; grants minimal permissions |
| 5 | `SimpleButtonService.java`, `SimpleButtonManager.java` | Registers `"simplebutton"` system service; proxies app calls to HAL |
| 6 | `SimpleButtonApp` (`MainActivity.kt`) | UI with READ and TRIGGER buttons for end-to-end testing |

---

## End-to-End Data Flow

### READ path — user presses **READ VALUE**

```
MainActivity.kt
  → Context.getSystemService("simplebutton")
  → SimpleButtonManager.getButtonValue()
  → ISimpleButtonService.getButtonValue()              [Binder IPC → system_server]
  → SimpleButtonService.getButtonValue()
  → ISimpleButton.getValue()                           [Binder IPC → HAL process]
  → SimpleButton::getValue()
  → open("/sys/class/misc/simplebutton/value", O_RDONLY)
  → read() → returns "0" or "1"
  → value propagated back up the stack
  → statusText shows "Button value: 0" or "Button value: 1"
```

### TRIGGER path — user presses **TRIGGER CLICK**

```
MainActivity.kt
  → Context.getSystemService("simplebutton")
  → SimpleButtonManager.triggerClick()
  → ISimpleButtonService.triggerButtonClick()          [Binder IPC → system_server]
  → SimpleButtonService.triggerButtonClick()
  → ISimpleButton.triggerClick()                       [Binder IPC → HAL process]
  → SimpleButton::triggerClick()
  → open("/sys/class/misc/simplebutton/trigger", O_WRONLY)
  → write("1")
  → simplebutton_trigger_store()                         [kernel]
  → printk("simplebutton: clicked")                      [dmesg]
  → button_value reset to 0
  → wake_up_interruptible(&button_waitq)                 [poll waiters notified]
  → statusText shows "Triggered! Check logcat"
```

### One-line stack summary

```
App → SimpleButtonManager → SimpleButtonService → ISimpleButton HAL → sysfs → kernel driver
```

---

## Layer-by-Layer Detail

### Layer 1 — Kernel Driver

**File:** `source_code/SimpleButton/kernel/drivers/misc/simplebutton/simplebutton.c`

- Registers a Linux **misc device** named `simplebutton`.
- Maintains `button_value` (0 or 1) protected by a spinlock.
- Exposes two sysfs nodes:
  - `/sys/class/misc/simplebutton/value` — read-only; returns current button state.
  - `/sys/class/misc/simplebutton/trigger` — write-only; writing `1` triggers a click.
- On trigger: sets value to 1, logs `simplebutton: clicked` via `pr_info()`, resets value to 0, and wakes poll waiters.
- Also supports ioctl (`SIMPLEBUTTON_IOCTL_READ`, `SIMPLEBUTTON_IOCTL_TRIGGER`) and `poll()` for userspace event notification.

### Layer 2 — HAL AIDL Interface

**File:** `source_code/SimpleButton/hal/hardware/interfaces/simplebutton/aidl/android/hardware/simplebutton/ISimpleButton.aidl`

```aidl
@VintfStability
interface ISimpleButton {
    int getValue();
    void triggerClick();
}
```

Stable AIDL with `@VintfStability` ensures framework–vendor compatibility across OTAs. AIDL is used instead of deprecated HIDL.

### Layer 3 — Native HAL Service

**Files:**
- `source_code/SimpleButton/hal/hardware/interfaces/simplebutton/aidl/default/SimpleButton.cpp`
- `source_code/SimpleButton/hal/hardware/interfaces/simplebutton/aidl/default/service.cpp`
- `source_code/SimpleButton/hal/hardware/interfaces/simplebutton/aidl/default/simplebutton.rc`

- Runs as a **binderized** vendor process: `/vendor/bin/hw/android.hardware.simplebutton-service`.
- `getValue()` opens and reads `/sys/class/misc/simplebutton/value`.
- `triggerClick()` opens and writes `"1"` to `/sys/class/misc/simplebutton/trigger`.
- Registers with servicemanager as `android.hardware.simplebutton.ISimpleButton/default`.

### Layer 4 — SELinux

**Directory:** `source_code/SimpleButton/sepolicy/`

| Policy file | Purpose |
|-------------|---------|
| `hal_simplebutton.te` | Grants HAL domain read/write on sysfs nodes |
| `hal_simplebutton_hwservice.te` | Allows HAL to register with hwservicemanager |
| `simplebutton_service.te` | Labels the framework `"simplebutton"` service |
| `simplebutton_driver.te` | Labels sysfs nodes under `/sys/class/misc/simplebutton/` |
| `file_contexts` | Maps sysfs paths and HAL binary to SELinux types |
| `service_contexts` | Maps framework service name to type |
| `hwservice_contexts` | Maps HAL interface to hwservice type |

Without these rules, typical failures include `avc: denied { read }` on sysfs, `RemoteException` from framework, or `getSystemService()` returning null.

### Layer 5 — Framework Service

**Files:**
- `source_code/SimpleButton/framework/frameworks/base/core/java/android/os/SimpleButtonManager.java`
- `source_code/SimpleButton/framework/frameworks/base/core/java/android/os/ISimpleButtonService.aidl`
- `source_code/SimpleButton/framework/frameworks/base/services/core/java/com/android/server/SimpleButtonService.java`

- `SimpleButtonService` runs inside `system_server` and is registered as `"simplebutton"` via `ServiceManager`.
- `SimpleButtonManager` is the public (hidden) API accessed via `Context.getSystemService("simplebutton")`.
- `SimpleButtonService` connects to HAL via `ServiceManager.waitForService("android.hardware.simplebutton.ISimpleButton/default")`.
- Only `SYSTEM_UID` and `SHELL_UID` callers are permitted.

Integration into existing AOSP files (`Context.java`, `SystemServiceRegistry.java`, `SystemServer.java`, `Android.bp`) is documented in `source_code/SimpleButton/AOSP_INTEGRATION.md`.

### Layer 6 — Privileged App

**Files:**
- `source_code/SimpleButton/app/packages/apps/SimpleButtonApp/src/main/java/com/simpleenergy/buttonapp/MainActivity.kt`
- `source_code/SimpleButton/app/packages/apps/SimpleButtonApp/AndroidManifest.xml`

- Package: `com.simpleenergy.buttonapp`
- Signed as platform app with `android:sharedUserId="android.uid.system"`.
- UI provides two buttons:
  - **READ VALUE** — calls `SimpleButtonManager.getButtonValue()` (full AOSP build) or toggles simulated value (standalone demo).
  - **TRIGGER CLICK** — calls `SimpleButtonManager.triggerClick()` (full AOSP build) or logs to logcat (standalone demo).

---

## AOSP Integration

Copy each folder from `source_code/SimpleButton/` into the corresponding AOSP source tree:

| Project path | AOSP destination |
|-------------|-----------------|
| `kernel/drivers/misc/simplebutton/` | `$(KERNEL_SRC)/drivers/misc/simplebutton/` |
| `hal/hardware/interfaces/simplebutton/` | `hardware/interfaces/simplebutton/` |
| `sepolicy/*.te`, `file_contexts`, etc. | `device/vendor/device/sepolicy/vendor/` |
| `framework/frameworks/base/core/java/android/os/*` | `frameworks/base/core/java/android/os/` |
| `framework/frameworks/base/services/core/java/com/android/server/SimpleButtonService.java` | `frameworks/base/services/core/java/com/android/server/` |
| `app/packages/apps/SimpleButtonApp/` | `packages/apps/SimpleButtonApp/` |

Apply the patches described in `source_code/SimpleButton/AOSP_INTEGRATION.md`:

1. `drivers/misc/Kconfig` — add `source "drivers/misc/simplebutton/Kconfig"`
2. `drivers/misc/Makefile` — add `obj-y += simplebutton/`
3. Kernel defconfig — add `CONFIG_SIMPLEBUTTON=y`
4. `frameworks/base/core/java/android/content/Context.java` — add `SIMPLE_BUTTON_SERVICE = "simplebutton"`
5. `frameworks/base/core/java/android/app/SystemServiceRegistry.java` — register `SimpleButtonManager`
6. `frameworks/base/services/java/com/android/server/SystemServer.java` — start and publish `SimpleButtonService`
7. `frameworks/base/Android.bp` — add `ISimpleButtonService.aidl` to framework sources
8. `frameworks/base/services/Android.bp` — add `SimpleButtonService.java`
9. `device/vendor/device/device.mk` — add `android.hardware.simplebutton-service` and `SimpleButtonApp` to `PRODUCT_PACKAGES`
10. `device/vendor/device/manifest.xml` — declare `android.hardware.simplebutton` HAL
11. `device/vendor/device/compatibility_matrix.xml` — declare HAL dependency
12. Merge SELinux policy files

---

## Build

Requires a Linux x86-64 host with a full AOSP source tree.

```bash
source build/envsetup.sh
lunch aosp_arm64-userdebug

# Incremental component builds
m android.hardware.simplebutton-service
m SimpleButtonApp
m framework
m sepolicy

# Full system image
m -j$(nproc)
```

Flash to target hardware:

```bash
fastboot flashall -w
```

---

## Test

### Layer 1 — Kernel (direct sysfs)

```bash
adb shell ls -la /sys/class/misc/simplebutton/
adb shell cat /sys/class/misc/simplebutton/value
# Expected: 0

adb shell "echo 1 > /sys/class/misc/simplebutton/trigger"
adb shell dmesg | grep simplebutton
# Expected: simplebutton: clicked

adb shell cat /sys/class/misc/simplebutton/value
# Expected: 0
```

### Layers 2–3 — HAL service

```bash
adb shell ps -A | grep simplebutton
adb shell service list | grep simplebutton
```

### Layer 4 — SELinux

```bash
adb shell dmesg | grep "avc: denied" | grep simplebutton
adb shell ls -laZ /sys/class/misc/simplebutton/
adb shell ls -laZ /vendor/bin/hw/android.hardware.simplebutton-service
```

### Layer 5 — Framework service

```bash
adb shell service list | grep simplebutton
adb logcat -s SimpleButtonService SimpleButtonManager
```

### Layer 6 — App (full AOSP stack)

```bash
adb shell am start -n com.simpleenergy.buttonapp/.MainActivity
adb logcat | grep -E "SimpleButton|simplebutton"
```

| Action | Expected UI | Expected logs |
|--------|-------------|---------------|
| Press **READ VALUE** | `Button value: 0` or `Button value: 1` | Framework/HAL debug logs |
| Press **TRIGGER CLICK** | `Triggered! Check logcat` | logcat: `D/SimpleButton: simplebutton: clicked`; dmesg: `simplebutton: clicked` |

### Layer 6 — App (standalone demo on API 34 emulator)

The app was also built and tested as a standalone Gradle project in Android Studio on an Android API 34 emulator. In this mode the app simulates button behaviour locally without the full AOSP stack.

```bash
adb install -r SimpleButtonApp.apk
adb shell am start -n com.simpleenergy.buttonapp/.MainActivity
adb logcat -s SimpleButton
```

| Action | Expected UI | Expected logcat |
|--------|-------------|-----------------|
| Press **READ VALUE** | Toggles `Button value: 0` ↔ `Button value: 1` | (no log on READ) |
| Press **TRIGGER CLICK** | `Triggered! Check logcat` | `D/SimpleButton: simplebutton: clicked` |

Proof screenshots are included in this repository:
- `app_screenshot.png` — app UI on emulator
- `logcat_proof.png` — logcat output after TRIGGER

---

## Key Design Decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| HAL IPC | Stable AIDL | HIDL is deprecated; AIDL is required for new HALs on Android 13+ |
| Driver type | Misc device | Simplest registration for a single-purpose character device |
| HAL mode | Binderized | Project Treble compliance, SELinux isolation, crash containment |
| Trigger semantics | Write 1, log, reset to 0 | Matches assignment spec; `printk` fires on every trigger |
| Framework pattern | `ServiceManager.addService()` | Direct registration in `SystemServer` for clarity |
| App access | Platform-signed, `sharedUserId=system` | Required for hidden `@SystemService` APIs |

---

## Development Environment

- **Target:** Android 14 (API 34)
- **Host tested:** macOS Apple Silicon (M1, 16 GB RAM)
- **App layer validated:** Android Studio on API 34 emulator — READ/TRIGGER UI and logcat output verified
- **Layers 1–5:** Code written per AOSP conventions; full end-to-end runtime validation requires a Linux x86-64 build host and target hardware with the custom AOSP image flashed

---

## Further Reading

| Document | Location |
|----------|----------|
| System architecture (detailed) | `source_code/SimpleButton/ARCHITECTURE.md` |
| AOSP file patches | `source_code/SimpleButton/AOSP_INTEGRATION.md` |
| Build, test, and validation report | `source_code/SimpleButton/EXECUTION_REPORT.md` |
| Component README | `source_code/SimpleButton/README.md` |
