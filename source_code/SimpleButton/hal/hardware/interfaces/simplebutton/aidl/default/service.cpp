// SPDX-License-Identifier: Apache-2.0

#include "SimpleButton.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::android::hardware::simplebutton::SimpleButton;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    std::shared_ptr<SimpleButton> service = ndk::SharedRefBase::make<SimpleButton>();

    const std::string instance = std::string() + SimpleButton::descriptor + "/default";
    binder_status_t status = AServiceManager_addService(service->asBinder().get(),
                                                        instance.c_str());
    if (status != STATUS_OK) {
        LOG(FATAL) << "Failed to register SimpleButton HAL service: " << status;
        return EXIT_FAILURE;
    }

    LOG(INFO) << "SimpleButton HAL service registered as " << instance;
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // joinThreadPool should not return
}
