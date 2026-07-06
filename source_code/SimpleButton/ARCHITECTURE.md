# SimpleButton — System Architecture

## 1. System Overview

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
│  Enforces which process/domain can access sysfs, HAL, services    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ (policy gates all layers below)
┌──────────────────────────────▼──────────────────────────────────────┐
│  LAYER 3 — NATIVE SERVICE (C++)                                     │
│  android.hardware.simplebutton-service                              │
│  SimpleButton.cpp reads/writes sysfs nodes                          │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ open() / read() / write()
┌──────────────────────────────▼──────────────────────────────────────┐
│  LAYER 2 — HAL INTERFACE (AIDL)                                   │
│  ISimpleButton.aidl — stable vendor interface contract            │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ sysfs file I/O
┌──────────────────────────────▼──────────────────────────────────────┐
│  LAYER 1 — KERNEL DRIVER (C)                                        │
│  simplebutton misc driver                                           │
│  /sys/class/misc/simplebutton/value                                 │
│  /sys/class/misc/simplebutton/trigger                               │
└─────────────────────────────────────────────────────────────────────┘
```

| Layer | One-line responsibility |
|-------|------------------------|
| 1. Kernel | Owns button state in kernel memory; exposes sysfs + ioctl + poll |
| 2. HAL AIDL | Defines the stable contract between framework and vendor code |
| 3. Native Service | Implements the HAL by talking to sysfs on behalf of upper layers |
| 4. SELinux | Labels every object and enforces who can touch what |
| 5. Framework | Wraps HAL in a Java system service and Manager API for apps |
| 6. App | Provides UI to read and trigger the simulated button |

---

## 2. Component Responsibilities

### Layer 1 — Kernel Driver (`simplebutton.c`)

| Aspect | Detail |
|--------|--------|
| **What it does** | Maintains `button_value` (0/1), exposes sysfs nodes, handles ioctl/poll |
| **Technology** | Linux kernel C, miscdevice framework, sysfs, spinlocks, wait queues |
| **Why chosen** | Misc device is the simplest way to register a single-purpose character device without managing major/minor numbers manually |

### Layer 2 — HAL AIDL Interface (`ISimpleButton.aidl`)

| Aspect | Detail |
|--------|--------|
| **What it does** | Defines `getValue()` and `triggerClick()` as a versioned, stable API |
| **Technology** | Stable AIDL with `@VintfStability` |
| **Why chosen** | AIDL is the modern HAL IPC standard (HIDL is deprecated); VINTF stability ensures framework-vendor compatibility across OTAs |

### Layer 3 — Native HAL Service (`SimpleButton.cpp`, `service.cpp`)

| Aspect | Detail |
|--------|--------|
| **What it does** | Runs as a separate process, implements AIDL methods by reading/writing sysfs |
| **Technology** | C++17, NDK Binder (`libbinder_ndk`), `AServiceManager_addService()` |
| **Why chosen** | Binderized HAL provides process isolation required by Project Treble; separate process prevents HAL crashes from taking down system_server |

### Layer 4 — SELinux Policies (`.te` files)

| Aspect | Detail |
|--------|--------|
| **What it does** | Labels sysfs nodes, HAL binary, framework service; grants minimal permissions |
| **Technology** | SELinux Type Enforcement (TE) policy files |
| **Why chosen** | Android enforces Mandatory Access Control — without correct labels and `allow` rules, even root-equivalent code gets `EACCES` |

### Layer 5 — Framework Service (`SimpleButtonService.java`, `SimpleButtonManager.java`)

| Aspect | Detail |
|--------|--------|
| **What it does** | Registers `"simplebutton"` service, proxies app calls to HAL |
| **Technology** | Java system service, AIDL stub, `ServiceManager` |
| **Why chosen** | Matches Android conventions (`SensorManager`, `VibratorManager`); hides HAL details from apps |

### Layer 6 — Privileged App (`SimpleButtonApp`)

| Aspect | Detail |
|--------|--------|
| **What it does** | UI with READ and TRIGGER buttons calling `SimpleButtonManager` |
| **Technology** | Kotlin, platform-signed privileged app |
| **Why chosen** | `@hide` Manager APIs are not available to normal third-party apps; platform cert + `sharedUserId=system` grants system-level access |

---

## 3. Key Design Decisions & Reasoning

### AIDL over HIDL
HIDL was introduced for Project Treble but is **deprecated since Android 11**. AIDL is simpler (no separate `.hal` language), has better Java/C++ tooling, and is the required format for new HALs on Android 13+.

### Misc device over full character device
A misc device auto-assigns a minor number and integrates cleanly with sysfs via `struct miscdevice`. For a single simulated button, this avoids boilerplate for `register_chrdev_region()` and custom device class creation.

### Binderized HAL over passthrough
Passthrough HALs run in the caller's process (insecure, breaks Treble boundaries). A binderized HAL runs in its own process (`/vendor/bin/hw/android.hardware.simplebutton-service`), giving SELinux a distinct domain (`hal_simplebutton_default`) and crash isolation.

### Java system service for framework layer
Android's `SystemServer` already hosts hundreds of Java services. Adding `SimpleButtonService` here follows the established pattern and integrates naturally with `Context.getSystemService()`.

### Privileged app
`SimpleButtonManager` is `@hide` and the service is registered as a system service. Only platform-signed apps with `android.uid.system` can reliably access these APIs in production builds.

---

## 4. Data Flow

### READ path (user presses "READ VALUE")

```
MainActivity.kt
  → getSystemService("simplebutton")
  → SimpleButtonManager.getButtonValue()
  → ISimpleButtonService.getButtonValue()          [Binder IPC → system_server]
  → SimpleButtonService.getButtonValue()
  → ISimpleButton.getValue()                       [Binder IPC → HAL process]
  → SimpleButton::getValue()
  → open("/sys/class/misc/simplebutton/value")
  → read() → returns "0" or "1"
  → value propagated back up the stack
  → statusText shows "Button value: 0"
```

### TRIGGER path (user presses "TRIGGER CLICK")

```
MainActivity.kt
  → getSystemService("simplebutton")
  → SimpleButtonManager.triggerClick()
  → ISimpleButtonService.triggerButtonClick()      [Binder IPC → system_server]
  → SimpleButtonService.triggerButtonClick()
  → ISimpleButton.triggerClick()                   [Binder IPC → HAL process]
  → SimpleButton::triggerClick()
  → open("/sys/class/misc/simplebutton/trigger")
  → write("1")
  → simplebutton_trigger_store()                   [kernel]
  → printk("simplebutton: clicked")                [dmesg]
  → button_value reset to 0
  → wake_up_interruptible(&button_waitq)           [poll waiters notified]
```

---

## 5. SELinux Policy Summary

| Domain / Type | Object | Permission | Why needed |
|---------------|--------|------------|------------|
| `hal_simplebutton_default` | `simplebutton_sysfs:file` | read, write, open, getattr | HAL must read value and write trigger sysfs nodes |
| `hal_simplebutton_default` | `hal_simplebutton` (service) | add, find | HAL registers with hwservicemanager / servicemanager |
| `hal_simplebutton_default` | binder | use | HAL participates in Binder IPC |
| `system_server` | `simplebutton_service:service_manager` | add, find | Framework publishes and looks up the service |
| `system_server` | `hal_simplebutton` (HAL) | call | Framework service calls HAL methods |
| `simplebutton_sysfs` | sysfs files | (type label) | Labels `/sys/class/misc/simplebutton/*` nodes |
| `hal_simplebutton_default_exec` | HAL binary | execute | Labels `/vendor/bin/hw/android.hardware.simplebutton-service` |
| `simplebutton_service` | servicemanager | add | Labels the `"simplebutton"` framework service |

Without these rules, typical failure modes are:
- HAL gets `avc: denied { read }` on sysfs → `getValue()` returns error
- `system_server` denied HAL call → `RemoteException` in framework
- Service not found → `getSystemService()` returns null or throws

---

## 6. File Map

```
SimpleButton/
├── kernel/drivers/misc/simplebutton/     → $KERNEL_SRC/drivers/misc/simplebutton/
├── hal/hardware/interfaces/simplebutton/ → $AOSP/hardware/interfaces/simplebutton/
├── sepolicy/                             → device/<vendor>/<device>/sepolicy/
├── framework/frameworks/base/            → $AOSP/frameworks/base/
├── app/packages/apps/SimpleButtonApp/    → $AOSP/packages/apps/SimpleButtonApp/
├── AOSP_INTEGRATION.md                   → Patch instructions for existing AOSP files
├── ARCHITECTURE.md                       → This file
└── EXECUTION_REPORT.md                   → Build & test instructions
```
