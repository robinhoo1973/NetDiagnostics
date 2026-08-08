// =============================================================================
// NetDiagApplication.java — earliest Java startup hook for NetDiagnostics
// =============================================================================
// 5WHY (Android no-log bug / startup crash): crashes that happen before the
// native library runs produced no log at all.  Android instantiates the
// <application> class before ANY Activity, so overriding onCreate() here is
// the earliest hook in the app lifecycle.  super.onCreate() loads the Qt
// native libraries — the exact point where "闪退" (instant crash) failures
// occur (missing .so, JNI_OnLoad failure, Qt plugin init).  We write a marker
// BEFORE calling super so even a load crash leaves a user-visible log trail.
//
// The manifest must declare this class:
//   <application android:name="com.netdiagnostic.app.NetDiagApplication" ...>
// =============================================================================
// 5WHY (Android startup crash): the bindings are in package
// org.qtproject.qt.android.bindings — Qt kept the Qt5-era Java package name
// in Qt 6.5 (verified from the qtbase 6.5.3 package: QtApplication.java
// declares `package org.qtproject.qt.android.bindings;`).  The manifest used
// to reference org.qtproject.qt6.android.bindings.*, which does NOT exist in
// the APK → ClassNotFoundException on launch → instant crash with no log.
// Both the manifest and this import must use the `qt` (not `qt6`) package.
package com.netdiagnostic.app;

import org.qtproject.qt.android.bindings.QtApplication;

public class NetDiagApplication extends QtApplication {

    @Override
    public void onCreate() {
        // MUST log BEFORE super.onCreate(): super loads the Qt native
        // libraries, which is where early startup crashes happen.
        EarlyLog.write(this, "Java Application.onCreate — BEFORE Qt native libs load");
        try {
            super.onCreate();
        } catch (Throwable t) {
            // Record even a failed Qt init, then rethrow so the OS reports it.
            EarlyLog.write(this, "Java Application.onCreate — EXCEPTION loading Qt libs: "
                    + t.getClass().getName() + ": " + t.getMessage());
            throw t;
        }
        EarlyLog.write(this, "Java Application.onCreate — Qt native libs loaded OK");
    }
}
