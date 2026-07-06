// SPDX-License-Identifier: Apache-2.0
package com.android.server;

import android.hardware.simplebutton.ISimpleButton;
import android.os.Binder;
import android.os.IBinder;
import android.os.ISimpleButtonService;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Slog;

/**
 * System service that bridges the Android framework to the SimpleButton HAL.
 *
 * @hide
 */
public class SimpleButtonService extends ISimpleButtonService.Stub {
    private static final String TAG = "SimpleButtonService";
    private static final String HAL_SERVICE_NAME =
            "android.hardware.simplebutton.ISimpleButton/default";

    private final Object mHalLock = new Object();
    private ISimpleButton mHal;

    public SimpleButtonService() {
        Slog.i(TAG, "SimpleButtonService starting");
    }

    @Override
    public int getButtonValue() {
        enforceCallingPermission();

        try {
            ISimpleButton hal = getHal();
            return hal.getValue();
        } catch (RemoteException e) {
            Slog.e(TAG, "getButtonValue failed", e);
            return -1;
        }
    }

    @Override
    public void triggerButtonClick() {
        enforceCallingPermission();

        try {
            ISimpleButton hal = getHal();
            hal.triggerClick();
        } catch (RemoteException e) {
            Slog.e(TAG, "triggerButtonClick failed", e);
        }
    }

    private ISimpleButton getHal() throws RemoteException {
        synchronized (mHalLock) {
            if (mHal != null && mHal.asBinder().isBinderAlive()) {
                return mHal;
            }

            IBinder binder = ServiceManager.waitForService(HAL_SERVICE_NAME);
            if (binder == null) {
                throw new RemoteException("HAL service not available: " + HAL_SERVICE_NAME);
            }

            mHal = ISimpleButton.Stub.asInterface(binder);
            if (mHal == null) {
                throw new RemoteException("Failed to get ISimpleButton interface");
            }
            return mHal;
        }
    }

    private void enforceCallingPermission() {
        final int uid = Binder.getCallingUid();
        if (uid != android.os.Process.SYSTEM_UID
                && uid != android.os.Process.SHELL_UID) {
            throw new SecurityException("Caller is not permitted to access SimpleButton");
        }
    }
}
