// =============================================================================
// PlatformCapture.mm — iOS screenshot implementation (UIGraphicsImageRenderer)
// =============================================================================
#if defined(PLATFORM_IOS)

#include "Common/Platform/PlatformCapture.h"
#include <QString>
#include <QFileInfo>
#include <QDir>
#import <UIKit/UIKit.h>

// 5WHY: the same capture logic ran in two branches (main-thread direct
// vs background-thread dispatch_async).  Extracted into a single block
// so bugfixes apply uniformly and there are no "I forgot the second copy"
// regressions.
static UIImage* captureKeyWindowImage() {
    UIWindow* keyWindow = nil;
    for (UIWindow* w in UIApplication.sharedApplication.windows) {
        if (w.isKeyWindow) { keyWindow = w; break; }
    }
    if (!keyWindow) keyWindow = UIApplication.sharedApplication.windows.firstObject;
    if (!keyWindow) return nil;

    UIView* view = keyWindow.rootViewController.view;
    if (!view) return nil;

    // 5WHY: renderInContext only captures the CALayer tree, missing UIKit
    // compositing effects (blur, vibrancy).  drawViewHierarchyInRect
    // with afterScreenUpdates:YES captures the complete rendered output.
    UIGraphicsImageRendererFormat* format = [[UIGraphicsImageRendererFormat alloc] init];
    format.scale = UIScreen.mainScreen.scale;   // native resolution (2x/3x)
    format.opaque = NO;

    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc]
        initWithSize:view.bounds.size format:format];
    return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [view drawViewHierarchyInRect:view.bounds afterScreenUpdates:YES];
    }];
}

bool platformCaptureScreenshot(const QString& filePath) {
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    __block bool success = false;
    __block NSData* pngData = nil;

    if ([NSThread isMainThread]) {
        UIImage* image = captureKeyWindowImage();
        if (image) {
            pngData = UIImagePNGRepresentation(image);
            success = (pngData != nil);
        }
    } else {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        dispatch_async(dispatch_get_main_queue(), ^{
            @autoreleasepool {
                UIImage* image = captureKeyWindowImage();
                if (image) {
                    pngData = [UIImagePNGRepresentation(image) retain];
                    success = (pngData != nil);
                }
            }
            dispatch_semaphore_signal(sem);
        });
        // 5WHY: DISPATCH_TIME_FOREVER could hang if the main queue is
        // deadlocked. 30s timeout is generous — if we can't capture in
        // 30s the device is likely unresponsive anyway.
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));
    }

    if (!success || !pngData) return false;

    NSString* nsPath = [NSString stringWithUTF8String:filePath.toUtf8().constData()];
    BOOL wrote = [pngData writeToFile:nsPath atomically:YES];
    // 5WHY: retain was needed for the dispatch_async path (ARC would
    // release when the block exits). Release here to balance.
    if (![NSThread isMainThread] && pngData) {
        [pngData release];
    }
    return wrote;
}

#endif // PLATFORM_IOS
