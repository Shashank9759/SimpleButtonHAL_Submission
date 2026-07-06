// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <aidl/android/hardware/simplebutton/BnSimpleButton.h>

namespace aidl::android::hardware::simplebutton {

class SimpleButton : public BnSimpleButton {
public:
    ndk::ScopedAStatus getValue(int32_t* _aidl_return) override;
    ndk::ScopedAStatus triggerClick() override;

private:
    static constexpr char kSysfsValue[] = "/sys/class/misc/simplebutton/value";
    static constexpr char kSysfsTrigger[] = "/sys/class/misc/simplebutton/trigger";
};

}  // namespace aidl::android::hardware::simplebutton
