// Copyright 2025 Divy Srivastava. All rights reserved. MIT license.
//
// Public header for backend-common. Each backend (CEF, webview) pre-parses
// its backend-specific wef_value_t into the plain structs declared here,
// then calls into the shared per-platform implementations.

#ifndef WEF_BACKEND_COMMON_H_
#define WEF_BACKEND_COMMON_H_

#include <wef.h>

#include <cstdint>
#include <string>
#include <vector>

// Forward-declare AppKit types for Obj-C++ callers. Must live outside
// any namespace because `@class` is only valid at global scope.
#if defined(__APPLE__) && defined(__OBJC__)
@class NSMenu;
#endif

namespace wef_common {

// ---------------------------------------------------------------------------
// Notifications
// ---------------------------------------------------------------------------

struct NotificationAction {
  std::string id;
  std::string title;
};

struct NotificationOptions {
  std::string title;
  std::string body;
  // If non-empty, replaces any existing notification with the same tag —
  // matches Web Notifications "tag" semantics.
  std::string tag;
  bool silent = false;
  bool require_interaction = false;
  std::vector<NotificationAction> actions;
  // PNG bytes; used by the Windows balloon. Ignored on other platforms
  // until macOS gets image-attachment support.
  std::vector<uint8_t> icon_png;
};

// Parses a wef_value_t dict into a plain NotificationOptions. Takes
// ownership of `options` (calls `api->value_free` before returning).
// Returns a default-initialized NotificationOptions if `options` is null
// or not a dict.
NotificationOptions ParseNotificationOptions(wef_value_t* options,
                                             const wef_backend_api_t* api);

#ifdef __APPLE__
// UNUserNotificationCenter-backed (10.14+). Requires the process to run
// inside a bundled .app with a CFBundleIdentifier; without one, delivery
// fails and CLOSED is fired synthetically. Action buttons supported via
// UNNotificationCategory.
uint32_t ShowNotificationMac(const NotificationOptions& opts,
                             wef_notification_event_fn on_event,
                             void* user_data);
void CloseNotificationMac(uint32_t notification_id);
#endif

#ifdef __linux__
// Shells out to `notify-send`. Fire-and-forget — only synthesizes
// SHOWN (synchronously after spawn) and CLOSED (from the matching
// CloseNotificationLinux call). Click / action events are not surfaced
// because notify-send has no IPC channel back.
uint32_t ShowNotificationLinux(const NotificationOptions& opts,
                               wef_notification_event_fn on_event,
                               void* user_data);
void CloseNotificationLinux(uint32_t notification_id);
#endif

#ifdef _WIN32
// Shell_NotifyIcon balloon notification. On Windows 10/11 the shell
// renders the balloon as a system toast (with grouping, Action Center
// entry, etc.). Click → CLICKED; dismiss / timeout → CLOSED. Action
// buttons aren't supported by NIIF balloons — `opts.actions` is ignored
// on Windows.
uint32_t ShowNotificationWin(const NotificationOptions& opts,
                             wef_notification_event_fn on_event,
                             void* user_data);
void CloseNotificationWin(uint32_t notification_id);
#endif

// ---------------------------------------------------------------------------
// Dialogs (alert / confirm / prompt)
// ---------------------------------------------------------------------------
//
// `dialog_type` is one of WEF_DIALOG_* from wef.h. All implementations
// block until the user dismisses the dialog. The native modal pumps OS
// events so other wef windows keep responding.
//
// Returns 1 if OK / confirmed, 0 otherwise. For WEF_DIALOG_PROMPT, on a
// confirmed result `*out_input_value` is set to a strdup'd UTF-8 string
// the caller must free with `free()`.

#ifdef __APPLE__
int ShowDialogMac(int dialog_type, const std::string& title,
                  const std::string& message,
                  const std::string& default_value, char** out_input_value);
#endif

#ifdef _WIN32
int ShowDialogWin(int dialog_type, const std::string& title,
                  const std::string& message,
                  const std::string& default_value, char** out_input_value);
#endif

#ifdef __linux__
int ShowDialogLinux(int dialog_type, const std::string& title,
                    const std::string& message,
                    const std::string& default_value, char** out_input_value);
#endif

// ---------------------------------------------------------------------------
// Permissions / runtime authorization
// ---------------------------------------------------------------------------
//
// `kind` is one of WEF_PERMISSION_* from wef.h. Results are reported via
// the callback (status one of WEF_PERMISSION_STATUS_*).

#ifdef __APPLE__
// UNUserNotificationCenter-backed for WEF_PERMISSION_NOTIFICATIONS.
// Reports UNSUPPORTED if the process isn't running inside a bundled
// .app, or if `kind` is anything other than notifications.
void QueryPermissionMac(int kind, wef_permission_callback_fn cb,
                        void* user_data);
void RequestPermissionMac(int kind, wef_permission_callback_fn cb,
                          void* user_data);
#endif

// Windows + Linux stub: notify-send (Linux) and Shell_NotifyIcon balloons
// (Windows) have no permission model, so we report GRANTED synchronously
// for WEF_PERMISSION_NOTIFICATIONS and UNSUPPORTED for anything else.
void QueryPermissionStub(int kind, wef_permission_callback_fn cb,
                         void* user_data);
void RequestPermissionStub(int kind, wef_permission_callback_fn cb,
                           void* user_data);

// ---------------------------------------------------------------------------
// Keyboard event key/code mapping (W3C UI Events)
// ---------------------------------------------------------------------------
//
// `key` is the logical value (e.g. "a", "Enter", " "). `code` is the
// physical position (e.g. "KeyA", "Enter", "Space").
//
// CEF normalizes keyboard events to Windows VK codes on every platform,
// so the VK-based mappings below cover CEF Mac / Win / Linux as well as
// the webview Windows backend. The webview macOS and Linux backends use
// platform-native event types and have their own helpers.

// Windows VK → "key" (logical).
//   `character`: the Unicode codepoint typed (0 if not a char event /
//                unknown). When set to a printable ASCII byte, returned
//                directly (matches CEF behavior).
//   `shift_held` / `caps_on`: Windows shift/caps state for case
//                determination when `character` is 0 (matches webview
//                Windows behavior). Pass false on non-Windows callers.
std::string VkToKey(int vk, uint32_t character, bool shift_held,
                    bool caps_on);

// Windows VK → "code" (physical).
//   `is_extended`: WM_KEYDOWN lParam bit 24, distinguishes NumpadEnter
//                  vs Enter, ControlRight/Left, AltRight/Left.
//   `scancode`: WM_KEYDOWN lParam bits 16-23. Used on Windows only via
//               MapVirtualKey to distinguish ShiftLeft vs ShiftRight;
//               0 means default to ShiftLeft (CEF path).
std::string VkToCode(int vk, bool is_extended, uint32_t scancode);

#ifdef __APPLE__
// Cocoa NSEvent key code → W3C "key". `event` is an `NSEvent*` from
// keyboard NSEvents; declared as `void*` so non-Obj-C++ callers can also
// pass through without an Obj-C runtime dependency.
std::string NSEventKeyToKey(void* event_nsevent);
std::string NSEventKeyToCode(unsigned short key_code);

// ---------------------------------------------------------------------------
// Dock / taskbar (macOS app-scoped operations)
// ---------------------------------------------------------------------------
//
// macOS-only dock primitives. The Windows/Linux fallbacks (title-prefix
// badge, FlashWindowEx, GTK urgency hint) need per-window iteration and
// stay in each backend.

// Sets the application dock badge. nullptr or "" clears it.
void SetDockBadgeMac(const char* badge_or_null);

// `type` is one of WEF_DOCK_BOUNCE_INFORMATIONAL / WEF_DOCK_BOUNCE_CRITICAL.
void BounceDockMac(int type);

// true → NSApplicationActivationPolicyRegular (dock + menu bar)
// false → NSApplicationActivationPolicyAccessory (background, no dock)
void SetDockVisibleMac(bool visible);

// ---------------------------------------------------------------------------
// NSMenu builder (macOS)
// ---------------------------------------------------------------------------
//
// Walks a wef_value_t menu template and produces an NSMenu. Click events
// on non-role items invoke on_click(on_click_data, window_id, item_id).
// Role items (copy/paste/cut/quit/minimize/...) wire to First
// Responder selectors and don't reach on_click.
//
// Returns the menu through an opaque void* on non-Obj-C callers and as
// NSMenu* on Obj-C++ callers — header double-declared so the typed form
// flows through .mm files without forcing AppKit on plain .cc.
// (NSMenu is forward-declared at the top of this header.)
#ifdef __OBJC__
NSMenu* BuildNSMenuFromValue(wef_value_t* val, const wef_backend_api_t* api,
                             wef_menu_click_fn on_click, void* on_click_data,
                             uint32_t window_id);
#else
void* BuildNSMenuFromValue(wef_value_t* val, const wef_backend_api_t* api,
                           wef_menu_click_fn on_click, void* on_click_data,
                           uint32_t window_id);
#endif

// ---------------------------------------------------------------------------
// Tray / status-bar icon (macOS, NSStatusItem)
// ---------------------------------------------------------------------------

uint32_t CreateTrayIconMac();
void DestroyTrayIconMac(uint32_t tray_id);
void SetTrayIconMac(uint32_t tray_id, const void* png_bytes, size_t len);
void SetTrayIconDarkMac(uint32_t tray_id, const void* png_bytes, size_t len);
void SetTrayTooltipMac(uint32_t tray_id, const char* tooltip_or_null);
void SetTrayMenuMac(uint32_t tray_id, wef_value_t* menu_template,
                     const wef_backend_api_t* api,
                     wef_menu_click_fn on_click, void* on_click_data);
void SetTrayClickHandlerMac(uint32_t tray_id, wef_tray_click_fn handler,
                             void* user_data);
void SetTrayDoubleClickHandlerMac(uint32_t tray_id,
                                   wef_tray_click_fn handler, void* user_data);
#endif

#ifdef __linux__
// GDK keyval → W3C "key". `keyval` is a GDK keyval (gdk_event_key.keyval).
// `evdev_keycode` is GDK's evdev hardware keycode (gdk_event_key.hardware_keycode),
// used for "code" mapping.
std::string GdkKeyvalToKey(unsigned int keyval);
std::string GdkKeycodeToCode(unsigned int evdev_keycode);

// Build a GtkMenu (or GtkMenuBar when `is_menu_bar` is true) from a
// wef_value_t menu template. Non-role items trigger
// on_click(on_click_data, window_id, item_id). Role items map to
// labels and forward the role as the item id. Returns NULL if `val`
// is null or not a list.
//
// Returned as void* on non-GTK callers and as GtkWidget* on GTK-aware
// translation units (gtk.h must be included before this header for the
// typed form).
#ifdef __GTK_H__
GtkWidget* BuildGtkMenuFromValue(wef_value_t* val,
                                  const wef_backend_api_t* api,
                                  uint32_t window_id,
                                  wef_menu_click_fn on_click,
                                  void* on_click_data, bool is_menu_bar);
#else
void* BuildGtkMenuFromValue(wef_value_t* val, const wef_backend_api_t* api,
                            uint32_t window_id, wef_menu_click_fn on_click,
                            void* on_click_data, bool is_menu_bar);
#endif

// ---------------------------------------------------------------------------
// Tray / status-bar icon (Linux, libappindicator)
// ---------------------------------------------------------------------------
//
// All functions must be called on the GTK main thread; backends with
// off-main-thread callers should marshal first (CEF uses CefPostTask).
// When libappindicator is not available (WEF_HAVE_APPINDICATOR not
// defined), CreateTrayIconLinux returns 0 and all other calls no-op.
//
// AppIndicator has no left-click event — a click anywhere pops the
// indicator's menu — so SetTrayClickHandlerLinux and
// SetTrayDoubleClickHandlerLinux accept handlers for API symmetry but
// don't surface clicks back.

uint32_t CreateTrayIconLinux();
void DestroyTrayIconLinux(uint32_t tray_id);
void SetTrayIconLinux(uint32_t tray_id, const void* png_bytes, size_t len);
void SetTrayIconDarkLinux(uint32_t tray_id, const void* png_bytes, size_t len);
void SetTrayTooltipLinux(uint32_t tray_id, const char* tooltip_or_null);
void SetTrayMenuLinux(uint32_t tray_id, wef_value_t* menu_template,
                       const wef_backend_api_t* api,
                       wef_menu_click_fn on_click, void* on_click_data);
void SetTrayClickHandlerLinux(uint32_t tray_id, wef_tray_click_fn handler,
                                void* user_data);
void SetTrayDoubleClickHandlerLinux(uint32_t tray_id,
                                      wef_tray_click_fn handler,
                                      void* user_data);
#endif

}  // namespace wef_common

#endif  // WEF_BACKEND_COMMON_H_
