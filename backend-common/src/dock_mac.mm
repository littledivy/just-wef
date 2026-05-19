// Copyright 2025 Divy Srivastava. All rights reserved. MIT license.
//
// App-scoped macOS dock primitives. All run on the main queue because
// NSApp / NSDockTile interactions must happen on main.

#include "wef_backend_common.h"

#import <AppKit/AppKit.h>

namespace wef_common {

void SetDockBadgeMac(const char* badge_or_null) {
  NSString* ns = (badge_or_null && *badge_or_null)
                     ? [NSString stringWithUTF8String:badge_or_null]
                     : nil;
  dispatch_async(dispatch_get_main_queue(), ^{
    NSDockTile* tile = [NSApp dockTile];
    [tile setBadgeLabel:ns];
    [tile display];
  });
}

void BounceDockMac(int type) {
  NSRequestUserAttentionType t = (type == WEF_DOCK_BOUNCE_CRITICAL)
                                     ? NSCriticalRequest
                                     : NSInformationalRequest;
  dispatch_async(dispatch_get_main_queue(), ^{
    [NSApp requestUserAttention:t];
  });
}

void SetDockVisibleMac(bool visible) {
  NSApplicationActivationPolicy policy =
      visible ? NSApplicationActivationPolicyRegular
              : NSApplicationActivationPolicyAccessory;
  dispatch_async(dispatch_get_main_queue(), ^{
    [NSApp setActivationPolicy:policy];
  });
}

}  // namespace wef_common
