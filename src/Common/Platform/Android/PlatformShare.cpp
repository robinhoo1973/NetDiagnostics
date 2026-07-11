// =============================================================================
// PlatformShare_android.cpp — Android share sheet (Intent.ACTION_SEND + FileProvider)
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include "Common/Platform/PlatformShare.h"
#include <QJniObject>
#include <QJniEnvironment>
#include <QCoreApplication>
#include <QString>

static bool clearJni(QJniEnvironment& env) {
    if (env->ExceptionCheck()) { env->ExceptionClear(); return true; }
    return false;
}

void platformShareFile(const QString& filePath, const QString& mimeType, const QString& subject) {
    QJniEnvironment env;

    // Activity/Context.
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return;

    // authority = "<package>.fileprovider" (must match AndroidManifest provider).
    QJniObject pkg = activity.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    const QString authority = pkg.toString() + QStringLiteral(".fileprovider");

    // File file = new File(path)
    QJniObject jFile("java/io/File", "(Ljava/lang/String;)V",
                     QJniObject::fromString(filePath).object<jstring>());

    // Uri uri = FileProvider.getUriForFile(activity, authority, file)
    QJniObject uri = QJniObject::callStaticObjectMethod(
        "androidx/core/content/FileProvider", "getUriForFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
        activity.object(), QJniObject::fromString(authority).object<jstring>(), jFile.object());
    if (clearJni(env) || !uri.isValid()) return;

    // Intent send = new Intent(Intent.ACTION_SEND)
    QJniObject actionSend = QJniObject::getStaticObjectField(
        "android/content/Intent", "ACTION_SEND", "Ljava/lang/String;");
    QJniObject send("android/content/Intent", "(Ljava/lang/String;)V",
                    actionSend.object<jstring>());
    send.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;",
                          QJniObject::fromString(mimeType).object<jstring>());

    QJniObject extraStream = QJniObject::getStaticObjectField(
        "android/content/Intent", "EXTRA_STREAM", "Ljava/lang/String;");
    send.callObjectMethod("putExtra",
        "(Ljava/lang/String;Landroid/os/Parcelable;)Landroid/content/Intent;",
        extraStream.object<jstring>(), uri.object());

    QJniObject extraSubject = QJniObject::getStaticObjectField(
        "android/content/Intent", "EXTRA_SUBJECT", "Ljava/lang/String;");
    send.callObjectMethod("putExtra",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
        extraSubject.object<jstring>(), QJniObject::fromString(subject).object<jstring>());

    const jint grantRead = QJniObject::getStaticField<jint>(
        "android/content/Intent", "FLAG_GRANT_READ_URI_PERMISSION");
    send.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", grantRead);

    // Intent chooser = Intent.createChooser(send, "Share report")
    QJniObject chooser = QJniObject::callStaticObjectMethod(
        "android/content/Intent", "createChooser",
        "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
        send.object(),
        QJniObject::fromString(QStringLiteral("Share report")).object<jstring>());
    const jint newTask = QJniObject::getStaticField<jint>(
        "android/content/Intent", "FLAG_ACTIVITY_NEW_TASK");
    chooser.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", newTask);

    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", chooser.object());
    clearJni(env);
}

#endif // PLATFORM_ANDROID
