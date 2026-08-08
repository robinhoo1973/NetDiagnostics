// =============================================================================
// EarlyLog.java — Java-side early startup log for NetDiagnostics (Android)
// =============================================================================
// 5WHY (Android no-log bug): native STARTUP_LOG in C++ only begins once the
// native library is loaded and main() runs.  If the app crashes EARLIER — in
// Java Application.onCreate(), during .so loading, in JNI_OnLoad, or inside
// Qt's platform-plugin init — there was zero diagnostic trail.  This class
// writes timestamped markers to the USER-VISIBLE app-scoped external dir
// (/storage/emulated/0/Android/data/<pkg>/files/NetDiagnostics/, no storage
// permission needed, reachable via USB/MTP) from the very first Java hook.
//
// It writes to the SAME file the native code uses (NetDiagnostics_startup.log)
// so the full crash trail is in one place.  Each process launch truncates the
// file (Application.onCreate runs once per process, before any native code),
// so a fresh launch always starts with a clean log.
//
// Logging is best-effort: it must NEVER crash or block app startup.
// =============================================================================
package com.netdiagnostic.app;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

final class EarlyLog {
    private static final String TAG = "NetDiagnostics";
    private static final String LOG_SUBDIR = "NetDiagnostics";
    private static final String LOG_FILE = "NetDiagnostics_startup.log";

    private static boolean sFileInitialized = false;

    private EarlyLog() {}

    /** Write a startup marker to logcat + the user-visible log file. */
    static void write(Context ctx, String msg) {
        Log.i(TAG, "[JAVA] " + msg);
        try {
            File dir = resolveLogDir(ctx);
            if (dir == null)
                return;
            if (!dir.exists() && !dir.mkdirs()) {
                Log.e(TAG, "EarlyLog: cannot create " + dir);
                return;
            }
            File f = new File(dir, LOG_FILE);
            if (!sFileInitialized) {
                // One fresh log per process launch (native code appends).
                //noinspection ResultOfMethodCallIgnored
                f.delete();
                sFileInitialized = true;
            }
            String line = "[" + new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)
                    .format(new Date()) + "] [JAVA] " + msg + "\n";
            try (FileOutputStream fos = new FileOutputStream(f, true);
                 OutputStreamWriter w = new OutputStreamWriter(fos, StandardCharsets.UTF_8)) {
                w.write(line);
                w.flush();
            }
        } catch (Exception e) {
            // Never let logging break app startup.
            Log.e(TAG, "EarlyLog.write failed: " + e);
        }
    }

    private static File resolveLogDir(Context ctx) {
        if (ctx == null)
            return null;
        File ext = ctx.getExternalFilesDir(null);
        if (ext != null)
            return new File(ext, LOG_SUBDIR);
        // External storage not ready — deterministic app-scoped path.
        return new File("/storage/emulated/0/Android/data/"
                + ctx.getPackageName() + "/files/" + LOG_SUBDIR);
    }
}
