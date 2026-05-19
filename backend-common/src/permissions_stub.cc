// Copyright 2025 Divy Srivastava. All rights reserved. MIT license.
//
// Stub permission implementation for Windows and Linux. Both platforms'
// notification systems (Shell_NotifyIcon balloons / notify-send) have no
// permission model, so we report GRANTED synchronously for
// WEF_PERMISSION_NOTIFICATIONS and UNSUPPORTED for anything else.

#include "wef_backend_common.h"

namespace wef_common {

void QueryPermissionStub(int kind, wef_permission_callback_fn cb,
                         void* user_data) {
  if (!cb) return;
  cb(user_data, kind == WEF_PERMISSION_NOTIFICATIONS
                    ? WEF_PERMISSION_STATUS_GRANTED
                    : WEF_PERMISSION_STATUS_UNSUPPORTED);
}

void RequestPermissionStub(int kind, wef_permission_callback_fn cb,
                           void* user_data) {
  if (!cb) return;
  cb(user_data, kind == WEF_PERMISSION_NOTIFICATIONS
                    ? WEF_PERMISSION_STATUS_GRANTED
                    : WEF_PERMISSION_STATUS_UNSUPPORTED);
}

}  // namespace wef_common
