// SPDX-License-Identifier: Apache-2.0
package android.hardware.simplebutton;

@VintfStability
interface ISimpleButton {
    /**
     * Read the current button state from the kernel driver.
     * @return 0 or 1 representing button state.
     */
    int getValue();

    /**
     * Trigger a simulated button click.
     * Writes to the kernel sysfs trigger node, which logs to dmesg.
     */
    void triggerClick();
}
