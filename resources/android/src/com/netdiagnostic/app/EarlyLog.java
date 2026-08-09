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
// 5WHY (Android 11+ invisible log): the app-scoped external dir is NOT
// browsable by stock file managers on Android 11+ (scoped storage) — the log
// exists but the user can't find it without USB/adb.  MediaStore.Downloads
// (API 29+, targetSdk 29+) creates a file in the PUBLIC Download folder with
// NO storage permission.  Every line is therefore also mirrored to
// Download/NetDiagnostics/NetDiagnostics_startup.log (see appendToDownloads()).
// On API 26-28 the C++ raw-path fallback (AndroidDownloadLog.h) writes the
// same Download file using the WRITE_EXTERNAL_STORAGE runtime grant.
//
// appendToDownloads() is PUBLIC so native code (StartupLog.h →
// AndroidDownloadLog.h) can feed its lines into the SAME Download file —
// the MediaStore plumbing lives here in one place, not duplicated in C++.
//
// Logging is best-effort: it must NEVER crash or block app startup.
// =============================================================================
package com.netdiagnostic.app;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public final class EarlyLog {
    private static final String TAG = "NetDiagnostics";
    private static final String LOG_SUBDIR = "NetDiagnostics";
    private static final String LOG_FILE = "NetDiagnostics_startup.log";
    // Public mirror: Download/NetDiagnostics/NetDiagnostics_startup.log
    private static final String DL_REL_PATH = "Download/NetDiagnostics/";

    private static Context sAppContext = null;
    private static boolean sFileInitialized = false;
    private static boolean sDownloadsInitialized = false;
    private static Uri sDownloadsUri = null;

    private EarlyLog() {}

    /** Called from NetDiagApplication.onCreate before any logging — earliest hook. */
    static void init(Context ctx) {
        sAppContext = ctx;
    }

    /** Write a startup marker to logcat + app-scoped file + public Download mirror. */
    static void write(Context ctx, String msg) {
        Log.i(TAG, "[JAVA] " + msg);
        String line = "[" + new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)
                .format(new Date()) + "] [JAVA] " + msg + "\n";
        writeAppDir(ctx, line);
        appendToDownloads(line);
    }

    private static void writeAppDir(Context ctx, String line) {
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

    /**
     * Append one line to the PUBLIC Download mirror
     * (Download/NetDiagnostics/NetDiagnostics_startup.log) via MediaStore.
     * Android 10+ (API 29+) — there an app-created MediaStore.Downloads row
     * needs NO storage permission (targetSdk 29+).  On API 26-28 this
     * silently no-ops and the C++ raw-path fallback
     * (AndroidDownloadLog.h, needs the WRITE_EXTERNAL_STORAGE grant) handles
     * the Download mirror.
     *
     * Also called from native C++ (StartupLog.h → AndroidDownloadLog.h) so the
     * native lines land in the SAME Download file.  Runs on the main thread
     * only (startup), so the shared static state needs no synchronization.
     *
     * Best-effort: never throws into callers.
     */
    public static void appendToDownloads(String line) {
        // 5WHY: MediaStore.Downloads exists since API 29 (Q); app-created
        // rows need no permission from API 29 with targetSdk 29+.  Guard
        // before touching any MediaStore symbol so API 26-28 devices can't
        // hit NoSuchFieldError — they use the C++ raw-path fallback instead.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q)
            return;
        if (sAppContext == null || line == null)
            return;
        try {
            ContentResolver cr = sAppContext.getContentResolver();
            if (!sDownloadsInitialized) {
                sDownloadsInitialized = true;
                // Fresh per-launch file: drop the previous launch's mirror
                // (DISPLAY_NAME alone can repeat; match path too) then insert
                // a new row so each launch starts clean.
                String where = MediaStore.Downloads.DISPLAY_NAME + "=? AND "
                        + MediaStore.Downloads.RELATIVE_PATH + "=?";
                cr.delete(MediaStore.Downloads.EXTERNAL_CONTENT_URI, where,
                        new String[]{LOG_FILE, DL_REL_PATH});
                ContentValues cv = new ContentValues();
                cv.put(MediaStore.Downloads.DISPLAY_NAME, LOG_FILE);
                cv.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
                cv.put(MediaStore.Downloads.RELATIVE_PATH, DL_REL_PATH);
                sDownloadsUri = cr.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv);
            }
            if (sDownloadsUri == null)
                return;
            // "wa" = write-append; each line is opened/closed on its own so a
            // crash mid-startup can't leave an unflushed stream dangling.
            OutputStream os = cr.openOutputStream(sDownloadsUri, "wa");
            if (os == null)
                return;
            try {
                os.write(line.getBytes(StandardCharsets.UTF_8));
                os.flush();
            } finally {
                os.close();
            }
        } catch (Exception e) {
            // Never let logging break app startup.
            Log.w(TAG, "EarlyLog Downloads mirror failed: " + e);
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
