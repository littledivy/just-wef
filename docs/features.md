# Features

Each capability is exposed through the [C ABI](c-abi.md) and implemented per
backend. Where a backend or platform can't express a feature, the relevant
function pointer is left `NULL` and the runtime degrades gracefully. The README
holds the at-a-glance support matrix; this is the behavioral reference.

## Window management

`create_window` / `create_window_ex` / `close_window`, `navigate`, `set_title`,
`set_window_size` / `get_window_size`, `set_window_position` /
`get_window_position`, `set_resizable` / `is_resizable`, `set_always_on_top` /
`is_always_on_top`, `show` / `hide` / `is_visible`, `focus`, `quit`,
`post_ui_task`.

Windows are identified by an opaque `uint32_t`. Positions and sizes are in
top-left-origin, density-independent pixels. `create_window_ex` takes
creation-time style flags that can't be changed later —
`WEF_WINDOW_FLAG_FRAMELESS` (no OS chrome) and `WEF_WINDOW_FLAG_NO_ACTIVATE`
(panel that doesn't steal focus); post-creation properties use their setters.
Winit supports windowing but not `navigate` / JS (no engine).

## JavaScript interop

`set_js_namespace`, `set_js_call_handler`, `js_call_respond`,
`invoke_js_callback`, `release_js_callback`, `execute_js`.

The runtime publishes a namespace object in every page (default `Wef`). Page
calls to `Wef.method(args)` arrive at the call handler with a `call_id`, name,
and arguments; the runtime answers with `js_call_respond`, settling the
page-side promise. Function arguments arrive as `_callback` values — invoke them
later with `invoke_js_callback` and free with `release_js_callback`.
`execute_js` evaluates a script in a window and returns its result/error. Not
available on Winit.

## Menus

`set_application_menu`, `show_context_menu`, `open_devtools`.

A menu template is a `wef_value_t` list of item dicts (`label`, `submenu`,
`role`, `type`, `id`, `accelerator`); clicking a custom item (one with an `id`)
fires the click callback with that id. The application menu is the global menu
bar on macOS (swapped on window focus) and a per-window menu on Windows/Linux.
Application menus don't work on CEF/Winit Linux (see
[backends.md](backends.md)); context menus work everywhere because popups need
no window container.

## Native dialogs

`show_dialog` (+ `string_free`).

Shows a modal `WEF_DIALOG_ALERT` / `CONFIRM` / `PROMPT` and **blocks** until
dismissed, returning 1 for OK/Yes and 0 otherwise. For prompt, the entered text
is written to `out_input_value` as a heap string freed via `string_free` (routed
through the backend so the right allocator is used — matters on Windows MSVC).
Backends use the platform modal loop (`runModal` / `MessageBoxW` /
`gtk_dialog_run`), which pumps OS events so other windows keep rendering.

## Input events

`set_keyboard_event_handler`, `set_mouse_click_handler`,
`set_mouse_move_handler`, `set_wheel_handler`, `set_cursor_enter_leave_handler`.

Handlers are global and receive the `window_id`. Keyboard events carry W3C
`key`/`code` strings and a `WEF_MOD_*` modifier bitmask; mouse events carry a
`WEF_MOUSE_BUTTON_*`, press/release state, position, modifiers, and click count;
wheel events carry deltas plus a `WEF_WHEEL_DELTA_*` mode. Each backend maps its
native event source (CEF, NSEvent, GDK, Win32) into these.

## Window events

`set_resize_handler`, `set_move_handler`, `set_focused_handler`,
`set_close_requested_handler`.

Fire on geometry change, focus/blur, and a close request (so the runtime can
intercept the OS close button).

## Window handles (GPU surfaces)

`get_window_handle`, `get_display_handle`, `get_window_handle_type`.

Return the raw platform handles needed to create a GPU rendering surface:
`get_window_handle_type` reports a `WEF_WINDOW_HANDLE_*` constant (AppKit /
Win32 / X11 / Wayland), and the handle pair is `NSView*` (AppKit), `HWND`
(Win32), `Window` + `Display*` (X11), or `wl_surface*` + `wl_display*`
(Wayland). The Winit backend exists primarily to expose these.

## Dock / taskbar

`set_dock_badge`, `bounce_dock`, `set_dock_menu`, `set_dock_visible`,
`set_dock_reopen_handler`.

App-scoped on macOS (the process Dock tile); focused-window-scoped on
Windows/Linux. The badge is a native red overlay on macOS
(`NSDockTile.badgeLabel`), a GDI+ overlay icon on Windows
(`ITaskbarList3::SetOverlayIcon`), and a `"(N) "` title prefix on Linux.
`bounce_dock` bounces (macOS), flashes the taskbar button (Windows), or sets the
urgency hint (Linux). The dock menu, dock visibility, and reopen callback are
macOS-only — backends leave them `NULL` elsewhere.

## Tray / status bar

`create_tray_icon` / `destroy_tray_icon`, `set_tray_icon` /
`set_tray_icon_dark`, `set_tray_tooltip`, `set_tray_menu`,
`set_tray_click_handler` / `set_tray_double_click_handler`,
`get_tray_icon_bounds`.

A persistent icon in the OS status area — macOS menu-bar extras (NSStatusItem),
Windows system tray (Shell_NotifyIcon), Linux AppIndicator. Each has a PNG image
(plus an optional dark-mode variant the backend swaps with system appearance), a
tooltip, a right-click menu, and click handlers. `get_tray_icon_bounds` reports
the icon rectangle in the same coordinate space as window positions, so a
popover panel can be anchored under it. On Linux, AppIndicator swallows click
events and has no double-click.

## Notifications

`show_notification`, `close_notification`.

App-scoped. `options` is a `wef_value_t` dict mirroring the Web Notification
API: `title` (required), `body`, `icon` (PNG bytes), `tag` (replaces a same-tag
notification), `silent`, `require_interaction`, and `actions` (id/title button
list). The backend takes ownership of `options`. Returns a `notification_id`;
`on_event` reports `WEF_NOTIFICATION_SHOWN` / `CLICKED` / `CLOSED` / `ACTION`
(some OS APIs report only a subset). Backends route through UN (macOS), the Win
toast/NIIF path (Windows), and notify-send (Linux).

## Permissions

`query_permission`, `request_permission`.

`query_permission` reads the OS authorization status for a `WEF_PERMISSION_*`
capability (e.g. notifications) without prompting; `request_permission` shows
the system prompt when the status is `PROMPT`, otherwise returns the cached
decision (the OS won't re-prompt a decided capability). Both deliver one of
`WEF_PERMISSION_STATUS_GRANTED` / `DENIED` / `PROMPT` / `UNSUPPORTED` on the UI
thread; a `NULL` pointer means every capability reports `UNSUPPORTED`.
