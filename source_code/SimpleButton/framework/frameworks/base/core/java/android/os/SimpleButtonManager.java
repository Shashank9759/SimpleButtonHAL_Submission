// SPDX-License-Identifier: Apache-2.0
package android.os;

import android.annotation.SystemService;
import android.content.Context;
import android.os.RemoteException;

/**
 * Manager class for interacting with the simulated hardware button.
 *
 * @hide
 */
@SystemService(Context.SIMPLE_BUTTON_SERVICE)
public class SimpleButtonManager {
    private static final String TAG = "SimpleButtonManager";

    private final ISimpleButtonService mService;

    /** @hide */
    public SimpleButtonManager(ISimpleButtonService service) {
        mService = service;
    }

    /**
     * Read the current button value (0 or 1) from the kernel via the full stack.
     *
     * @return current button state
     */
    public int getButtonValue() {
        try {
            return mService.getButtonValue();
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Trigger a simulated button click. The kernel driver logs "clicked" to dmesg.
     */
    public void triggerClick() {
        try {
            mService.triggerButtonClick();
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }
}
