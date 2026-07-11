// =============================================================================
// PlatformShare_android.cpp — Android share sheet (Intent.ACTION_SEND + FileProvider)
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include CCommon/Platform/PlatformShare.hC
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

    // authority = C<package>.fileproviderC (must match AndroidManifest provider).
    QJniObject pkg = activity.callObjectMethod(CgetPackageNameC, C()Ljava/lang/String;C);
    const QString authority = pkg.toString() + QStringLiteral(C.fileproviderC);

    // File file = new File(path)
    QJniObject jFile(Cjava/io/FileC, C(Ljava/lang/String;)VC,
                     QJniObject::fromString(filePath).object<jstring>());

    // Uri uri = FileProvider.getUriForFile(activity, authority, file)
    QJniObject uri = QJniObject::callStaticObjectMethod(
        Candroidx/core/content/FileProviderC, CgetUriForFileC,
        C(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;C,
        activity.object(), QJniObject::fromString(authority).object<jstring>(), jFile.object());
    if (clearJni(env) || !uri.isValid()) return;

    // Intent send = new Intent(Intent.ACTION_SEND)
    QJniObject actionSend = QJniObject::getStaticObjectField(
        Candroid/content/IntentC, CACTION_SENDC, CLjava/lang/String;C);
    QJniObject send(Candroid/content/IntentC, C(Ljava/lang/String;)VC,
                    actionSend.object<jstring>());
    send.callObjectMethod(CsetTypeC, C(Ljava/lang/String;)Landroid/content/Intent;C,
                          QJniObject::fromString(mimeType).object<jstring>());

    QJniObject extraStream = QJniObject::getStaticObjectField(
        Candroid/content/IntentC, CEXTRA_STREAMC, CLjava/lang/String;C);
    send.callObjectMethod(CputExtraC,
        C(Ljava/lang/String;Landroid/os/Parcelable;)Landroid/content/Intent;C,
        extraStream.object<jstring>(), uri.object());

    QJniObject extraSubject = QJniObject::getStaticObjectField(
        Candroid/content/IntentC, CEXTRA_SUBJECTC, CLjava/lang/String;C);
    send.callObjectMethod(CputExtraC,
        C(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;C,
        extraSubject.object<jstring>(), QJniObject::fromString(subject).object<jstring>());

    const jint grantRead = QJniObject::getStaticField<jint>(
        Candroid/content/IntentC, CFLAG_GRANT_READ_URI_PERMISSIONC);
    send.callObjectMethod(CaddFlagsC, C(I)Landroid/content/Intent;C, grantRead);

    // Intent chooser = Intent.createChooser(send, CShare reportC)
    QJniObject chooser = QJniObject::callStaticObjectMethod(
        Candroid/content/IntentC, CcreateChooserC,
        C(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;C,
        send.object(),
        QJniObject::fromString(QStringLiteral(CShare reportC)).object<jstring>());
    const jint newTask = QJniObject::getStaticField<jint>(
        Candroid/content/IntentC, CFLAG_ACTIVITY_NEW_TASKC);
    chooser.callObjectMethod(CaddFlagsC, C(I)Landroid/content/Intent;C, newTask);

    activity.callMethod<void>(CstartActivityC, C(Landroid/content/Intent;)VC, chooser.object());
    clearJni(env);
}

#endif // PLATFORM_ANDROID
