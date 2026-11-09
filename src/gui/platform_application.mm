#include "gui/platform_application.hpp"
#import <Cocoa/Cocoa.h>
namespace gui {
void setMacApplicationName() {
    @autoreleasepool { [[NSProcessInfo processInfo] setProcessName:@"sattrak"]; }
}
void setMacApplicationIcon() {
    @autoreleasepool {
        NSImage* icon=[[NSImage alloc] initWithContentsOfFile:@"assets/branding/sattrak.png"];
        if(icon)[NSApplication sharedApplication].applicationIconImage=icon;
    }
}
}
