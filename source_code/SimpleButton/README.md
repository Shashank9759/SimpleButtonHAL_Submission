# SimpleButton HAL Project

Full-stack Android (AOSP) hardware button simulation for the Simple Energy assignment.

## Project Structure

```
SimpleButton/
├── kernel/          Layer 1 — Linux kernel misc device driver
├── hal/             Layers 2-3 — AIDL HAL interface + C++ implementation
├── sepolicy/        Layer 4 — SELinux policies
├── framework/       Layer 5 — Java system service + Manager API
├── app/             Layer 6 — Privileged Kotlin test app
├── ARCHITECTURE.md  System design document
├── EXECUTION_REPORT.md  Build & test instructions
└── AOSP_INTEGRATION.md  Patches for existing AOSP files
```

## Quick Start

1. Copy each folder into the corresponding AOSP source tree (see `EXECUTION_REPORT.md`)
2. Apply integration patches from `AOSP_INTEGRATION.md`
3. Build: `source build/envsetup.sh && lunch <target>-userdebug && m -j$(nproc)`
4. Test: `adb shell cat /sys/class/misc/simplebutton/value`

## Stack Flow

```
App → SimpleButtonManager → SimpleButtonService → ISimpleButton HAL → sysfs → kernel driver
```

## Target

- Android 14 (API 34)
- AIDL HAL (not HIDL)
- Soong build system (Android.bp)
