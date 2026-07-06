// SPDX-License-Identifier: Apache-2.0
package android.os;

/** @hide */
interface ISimpleButtonService {
    int getButtonValue();
    void triggerButtonClick();
}
