# AOSP Integration Patches

This file documents the exact changes required in existing AOSP source files
to integrate the SimpleButton framework service. Copy each snippet into the
corresponding file in your AOSP tree.

---

## 1. `frameworks/base/core/java/android/content/Context.java`

Add the service constant near other `*_SERVICE` constants:

```java
    /**
     * Use with {@link #getSystemService(String)} to retrieve a
     * {@link android.os.SimpleButtonManager} for accessing the simulated
     * hardware button.
     *
     * @see android.os.SimpleButtonManager
     * @hide
     */
    public static final String SIMPLE_BUTTON_SERVICE = "simplebutton";
```

Also register the string in `getSystemServiceName()` if your AOSP version
uses the switch-based lookup (Android 14 pattern in `SystemServiceRegistry`).

---

## 2. `frameworks/base/core/java/android/app/SystemServiceRegistry.java`

In the static initializer block, add:

```java
        registerService(Context.SIMPLE_BUTTON_SERVICE, SimpleButtonManager.class,
                new CachedServiceFetcher<SimpleButtonManager>() {
            @Override
            public SimpleButtonManager createService(ContextImpl ctx)
                    throws ServiceNotFoundException {
                IBinder b = ServiceManager.getServiceOrThrow(Context.SIMPLE_BUTTON_SERVICE);
                ISimpleButtonService service = ISimpleButtonService.Stub.asInterface(b);
                return new SimpleButtonManager(service);
            }
        });
```

Add imports at the top of the file:

```java
import android.os.ISimpleButtonService;
import android.os.SimpleButtonManager;
```

---

## 3. `frameworks/base/services/java/com/android/server/SystemServer.java`

In `startOtherServices()`, after other hardware-related services are started,
add:

```java
            try {
                traceBeginAndSlog("StartSimpleButtonService");
                mSystemServiceManager.startService(SimpleButtonService.class);
                traceEnd();
            } catch (Throwable e) {
                reportWtf("starting SimpleButtonService", e);
            }
```

Also publish the binder so apps can reach it via `getSystemService()`:

In `startBootstrapServices()` or wherever services are published to
ServiceManager, add (or do this inside `SimpleButtonService` constructor
via a `SystemService` lifecycle — the pattern below is the direct approach):

```java
            ServiceManager.addService(Context.SIMPLE_BUTTON_SERVICE,
                    new SimpleButtonService());
```

**Recommended Android 14 pattern** — make `SimpleButtonService` extend
`SystemService` instead, and use `mSystemServiceManager.startService()`.
For this assignment, the direct `ServiceManager.addService()` call in
`SystemServer.startOtherServices()` is sufficient:

```java
        t.traceBegin("StartSimpleButtonService");
        try {
            SimpleButtonService simpleButtonService = new SimpleButtonService();
            ServiceManager.addService(Context.SIMPLE_BUTTON_SERVICE, simpleButtonService);
            Slog.i(TAG, "SimpleButton Service started");
        } catch (Throwable e) {
            Slog.e(TAG, "Failure starting SimpleButton Service", e);
        }
        t.traceEnd();
```

Add import:

```java
import com.android.server.SimpleButtonService;
```

---

## 4. `frameworks/base/Android.bp`

Add the framework AIDL to the `framework-minus-apex` or `framework-non-updatable`
sources list. Find the `filegroup` named `framework-aidl-export` or the
`aidl` section in `frameworks/base/Android.bp` and add:

```
        "core/java/android/os/ISimpleButtonService.aidl",
```

Also ensure `SimpleButtonManager.java` is included in the framework sources
(it will be picked up automatically if placed in `core/java/android/os/`).

---

## 5. `frameworks/base/services/Android.bp`

Add `SimpleButtonService.java` to the `services` module `srcs` list:

```
        "core/java/com/android/server/SimpleButtonService.java",
```

---

## 6. Kernel integration

### `drivers/misc/Kconfig`

Add:

```
source "drivers/misc/simplebutton/Kconfig"
```

### `drivers/misc/Makefile`

Add:

```
obj-y += simplebutton/
```

(or `obj-$(CONFIG_SIMPLEBUTTON) += simplebutton/`)

### `arch/<target>/configs/<defconfig>`

Enable the driver:

```
CONFIG_SIMPLEBUTTON=y
```

---

## 7. SELinux integration

Copy all `.te` files into `device/<vendor>/<device>/sepolicy/vendor/` or
merge into your device's sepolicy tree.

Append entries from `file_contexts`, `service_contexts`, and
`hwservice_contexts` to the corresponding device policy files.

Add `hal_simplebutton` to `device/<vendor>/<device>/manifest.xml` or include
the HAL manifest from `default/manifest.xml`.

---

## 8. Product integration

### `device/<vendor>/<device>/device.mk`

```makefile
PRODUCT_PACKAGES += \
    android.hardware.simplebutton-service \
    SimpleButtonApp
```

### `device/<vendor>/<device>/manifest.xml`

Include the HAL:

```xml
<manifest version="1.0" type="device">
    <hal format="aidl">
        <name>android.hardware.simplebutton</name>
        <version>1</version>
        <fqname>ISimpleButton/default</fqname>
    </hal>
</manifest>
```

---

## 9. VINTF compatibility matrix

### `device/<vendor>/<device>/compatibility_matrix.xml`

```xml
<compatibility-matrix version="1.0" type="framework">
    <hal format="aidl" optional="false">
        <name>android.hardware.simplebutton</name>
        <version>1</version>
        <interface>
            <name>ISimpleButton</name>
            <instance>default</instance>
        </interface>
    </hal>
</compatibility-matrix>
```
