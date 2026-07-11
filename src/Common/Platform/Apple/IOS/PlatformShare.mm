// =============================================================================
// PlatformShare_ios.mm — iOS share sheet (UIActivityViewController), MRC
// =============================================================================
#if defined(PLATFORM_IOS)

#include "Common/Platform/PlatformShare.h"
#include <QString>
#import <UIKit/UIKit.h>

void platformShareFile(const QString& filePath, const QString& mimeType, const QString& subject) {
    Q_UNUSED(mimeType);
    // Capture plain C++ copies; convert to NSString INSIDE the main-queue block so
    // no autoreleased Objective-C object ever crosses a thread boundary.
    const QString path = filePath;
    const QString subj = subject;

    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            NSURL* url = [NSURL fileURLWithPath:path.toNSString()];
            UIActivityViewController* avc =
                [[UIActivityViewController alloc] initWithActivityItems:@[url]
                                                 applicationActivities:nil];
            [avc setValue:subj.toNSString() forKey:@"subject"];

            // Present from the top-most view controller.
            UIWindow* keyWin = nil;
            for (UIWindow* w in UIApplication.sharedApplication.windows) {
                if (w.isKeyWindow) { keyWin = w; break; }
            }
            if (!keyWin) keyWin = UIApplication.sharedApplication.windows.firstObject;
            UIViewController* root = keyWin.rootViewController;
            while (root.presentedViewController) root = root.presentedViewController;

            // iPad requires a popover anchor (centre of the screen).
            UIPopoverPresentationController* pop = avc.popoverPresentationController;
            if (pop) {
                pop.sourceView = root.view;
                pop.sourceRect = CGRectMake(CGRectGetMidX(root.view.bounds),
                                            CGRectGetMidY(root.view.bounds), 0, 0);
                pop.permittedArrowDirections = 0;
            }
            [root presentViewController:avc animated:YES completion:nil];
            [avc release]; // MRC: UIKit retains during presentation
        }
    });
}

#endif // PLATFORM_IOS
