// SPDX-License-Identifier: Apache-2.0

#include "SimpleButton.h"

#include <android-base/logging.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace aidl::android::hardware::simplebutton {

ndk::ScopedAStatus SimpleButton::getValue(int32_t* _aidl_return) {
    if (_aidl_return == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    int fd = open(kSysfsValue, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LOG(ERROR) << "Failed to open " << kSysfsValue << ": " << strerror(errno);
        return ndk::ScopedAStatus::fromServiceSpecificError(errno);
    }

    char buf[16] = {};
    ssize_t bytes_read = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytes_read <= 0) {
        LOG(ERROR) << "Failed to read from " << kSysfsValue << ": " << strerror(errno);
        return ndk::ScopedAStatus::fromServiceSpecificError(EIO);
    }

    *_aidl_return = static_cast<int32_t>(strtol(buf, nullptr, 10));
    LOG(DEBUG) << "getValue() returned " << *_aidl_return;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SimpleButton::triggerClick() {
    int fd = open(kSysfsTrigger, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        LOG(ERROR) << "Failed to open " << kSysfsTrigger << ": " << strerror(errno);
        return ndk::ScopedAStatus::fromServiceSpecificError(errno);
    }

    const char trigger_val[] = "1";
    ssize_t bytes_written = write(fd, trigger_val, sizeof(trigger_val) - 1);
    close(fd);

    if (bytes_written < 0) {
        LOG(ERROR) << "Failed to write to " << kSysfsTrigger << ": " << strerror(errno);
        return ndk::ScopedAStatus::fromServiceSpecificError(errno);
    }

    LOG(INFO) << "triggerClick() wrote to sysfs trigger node";
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::simplebutton
