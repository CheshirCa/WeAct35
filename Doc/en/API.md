# C API reference

[English](API.md) | [Русский](../ru/API.md) | [Install](INSTALL.md) | [CLI](CLI.md) | [Hardware](HARDWARE.md)

This is the API reference for WeAct35 1.0.0.  It describes the installed
headers in `libweact-dev`, not private source headers or command-line-only
features.  The shared object is `libweact.so.1`; the verified ARM64 build has
79 exported symbols.  `pkg-config --modversion weact` reports `1.0.0`.

## Scope and first program

The library has two deliberately different interfaces.

* The legacy `weact_*` interface is a mutable, public `weact_display_t`
  framebuffer interface for the 160x80 FS096 protocol profile.  It does not
  perform the FS35 Linux USB identity/occupancy checks.
* The additive `wx_*` interface uses opaque devices and surfaces, an explicit
  model, a caller-provided transport, and status values.  Its managed POSIX
  initialisation path is for FS35.  It is the appropriate base for new code.

Install development files and compile with the installed metadata:

```sh
sudo apt install ./libweact1_1.0.0_arm64.deb ./libweact-dev_1.0.0_arm64.deb
cc -std=c11 -Wall -Wextra app.c $(pkg-config --cflags --libs weact) -o app
```

Installed headers are below `<weact/>`; do not include a header from the
source-tree `Source/` directory.  The runtime library is installed in the
Debian multiarch library directory, public headers in `/usr/include/weact/`,
and `weact.pc` in the matching `pkgconfig` directory.

### Minimal offline surface

This example allocates and modifies memory only; it neither opens USB nor
sends a display command.

```c
#include <weact/weact_extended.h>
#include <weact/weact_graphics.h>

int main(void) {
    wx_surface_t *s = NULL;
    if (wx_surface_create(160, 80, 0, WX_LEGACY_BRG565_BE, NULL, &s) != WX_OK)
        return 1;
    wx_status_t st = wx_surface_fill_rgb(s, 0, 0, 160, 80, 0, 0, 0);
    if (st == WX_OK) st = wx_surface_set_rgb(s, 10, 10, 255, 255, 255);
    wx_surface_destroy(s);
    return st == WX_OK ? 0 : 1;
}
```

Compile it with the command above.  A surface is host memory, not a display;
create a device and explicitly transmit it only after establishing an
appropriate transport.

## Models, pixels, errors, and ownership

`WX_FS096` is 160x80 in `WX_LEGACY_BRG565_BE`; `WX_FS35` is 480x320 in
`WX_RGB565_LE` when landscape.  `wx_pack_rgb()` produces the packed host
value for the requested profile; surface writers store bytes in that profile's
order.  Do not reuse FS096 BRG565 data as FS35 RGB565 data.

`wx_status_t` values are: `WX_OK`, `WX_INVALID`, `WX_OVERFLOW`, `WX_NOMEM`,
`WX_BOUNDS`, `WX_INTERRUPTED`, `WX_AGAIN`, `WX_TIMEOUT`, `WX_DISCONNECTED`,
`WX_IO`, and `WX_STREAM_FAILED`.  The API has no general `errno` accessor.
POSIX transport callers can obtain its stored OS error with
`wx_posix_transport_last_errno()`; managed initialisation records diagnostic
data in `wx_init_report_t`.  A transfer error seals a `wx_device_t` stream;
do not retry a header or continue transmitting on that device.

Objects returned through an `out` parameter are owned by the caller:
destroy a `wx_surface_t` with `wx_surface_destroy()`, a `wx_device_t` with
`wx_device_destroy()`, and a POSIX backend with `wx_posix_transport_destroy()`.
`wx_device_surface()` and `wx_surface_pixels()` return borrowed storage.
For every extensible public structure, set `struct_size = sizeof object`
before passing it to the library.  Allocator callbacks are either both NULL
(the normal allocator) or both non-NULL; their context must outlive objects
that use it.  Calls that mutate a device/surface or use one transport must be
serialised by the application; no general thread-safety guarantee is made.

## Public declarations

| Header | Public declarations |
|---|---|
| `weact_display.h` | legacy structure, colours, orientation/scroll enums, 23 `weact_*` functions |
| `text_freetype.h` | opaque text context, alignment enum, size macros, 11 `ft_text_*` functions |
| `weact_extended.h` | statuses/models/formats, allocator/transport/surface/device options, surface and basic device functions |
| `weact_graphics.h` | profile inspection, RGB import/fill, regional Bitmap |
| `weact_init.h` | managed-init states, reasons, reports, connector/options, five init functions |
| `weact_posix_init.h` | FS35 guarded connector options and factories |
| `weact_posix_transport.h` | low-level POSIX tty backend |
| `weact_memory_transport.h` | deterministic memory transport for tests/offline programs |
| `weact_protocol.h` | pure packet encoders |
| `weact_fastlz.h`, `weact_fs35.h` | FastLZ encoder and FS35 backlight/compressed-Bitmap features |

### Types, constants, and callback contracts

* `weact_orientation_t`: `WEACT_PORTRAIT=0`, `WEACT_REVERSE_PORTRAIT=1`,
  `WEACT_LANDSCAPE=2`, `WEACT_REVERSE_LANDSCAPE=3`, `WEACT_ROTATE=5`.
  `weact_scroll_dir_t` is `SCROLL_LEFT`, `SCROLL_RIGHT`, `SCROLL_UP`,
  `SCROLL_DOWN`.  The legacy macros are `WEACT_DISPLAY_WIDTH=160`,
  `WEACT_DISPLAY_HEIGHT=80`, `WEACT_MAX_BUFFER_SIZE=25600`, `WEACT_BAUDRATE`
  (`B115200`) and the eight documented BRG565 colour macros `WEACT_RED`,
  `GREEN`, `BLUE`, `WHITE`, `BLACK`, `YELLOW`, `CYAN`, `MAGENTA`.
  `weact_display.h` includes `<termios.h>` itself, so an application may use
  `WEACT_BAUDRATE` after including this public header first.
* `weact_display_t` is intentionally public and ABI-compatible.  Its members
  are `fd`, `port_name[256]`, `is_connected`, `orientation`, `brightness`,
  `display_width`, `display_height`, `frame_buffer`, `back_buffer`, and
  `last_error[512]`.  Applications normally provide storage then call
  `weact_init()`; do not free its buffers yourself.
* `ft_text_context_t` is opaque.  `ft_text_align_t` is
  `FT_TEXT_ALIGN_LEFT`, `CENTER`, `RIGHT`; font-size macros are 8, 12, and 16
  (`FT_FONT_SIZE_SMALL`, `MEDIUM`, `LARGE`).
* `wx_model_t` is `WX_FS096=1` or `WX_FS35=2`; `wx_pixel_format_t` is
  `WX_LEGACY_BRG565_BE=1` or `WX_RGB565_LE=2`.  `wx_device_t` and
  `wx_surface_t` are opaque.  `wx_surface_info_t` contains `width`, `height`,
  `stride`, `capacity`, and `format`.
* `wx_allocator_t` contains `context`, `allocate(context,size)`, and
  `release(context,pointer)`.  `wx_transport_t` contains `context`,
  `write(context,bytes,length,written)`, `now_ms(context)`, and
  `wait_writable(context,deadline_ms)`.  A successful write reports 0 through
  `length` accepted bytes; an error must report zero.  Callbacks must not
  re-enter the device.
* `wx_device_options_t` contains `struct_size`, `model`, explicit
  `width,height,stride`, `orientation`, nonzero `timeout_ms`, `transport`,
  and `allocator`.  Orientation 0/1 is portrait and 2/3 landscape; geometry
  must exactly match model and orientation.
* Managed initialisation uses `wx_init_state_t` (`WX_INIT_CREATED`,
  `WX_INIT_CONNECTED`, `WX_INIT_ORIENTATION_CONFIRMED`,
  `WX_INIT_DISPLAY_CLEARED_TX`, `WX_INIT_READY`, `WX_INIT_READY_PRESERVED`,
  `WX_INIT_FAILED`, `WX_INIT_CLOSED`), `wx_fb_sync_t` (`WX_FB_UNKNOWN`,
  `WX_FB_BLACK_BASELINE_ASSUMED`), `wx_init_stage_t`
  (`WX_INIT_STAGE_VALIDATE`, `WX_INIT_STAGE_ALLOCATE`,
  `WX_INIT_STAGE_ACQUIRE`, `WX_INIT_STAGE_SET_ORIENTATION`,
  `WX_INIT_STAGE_QUERY_TX`, `WX_INIT_STAGE_RX`, `WX_INIT_STAGE_PRE_FILL`,
  `WX_INIT_STAGE_FILL_TX`, `WX_INIT_STAGE_COMMIT`, `WX_INIT_STAGE_COMPLETE`,
  `WX_INIT_STAGE_CLOSE`), `wx_init_reason_t` (`WX_INIT_REASON_NONE`,
  `WX_INIT_INVALID_OPTIONS`, `WX_INIT_NOT_IMPLEMENTED`,
  `WX_INIT_PROFILE_NOT_ENABLED`, `WX_INIT_ALLOCATION_FAILED`,
  `WX_INIT_ACQUIRE_FAILED`, `WX_INIT_INVALID_CHANNEL`,
  `WX_INIT_CLOCK_ERROR`, `WX_INIT_DEADLINE`, `WX_INIT_GEOMETRY_UNCONFIRMED`,
  `WX_INIT_CHANNEL_CLOSED`, `WX_INIT_RELEASE_FAILED`,
  `WX_INIT_IDENTITY_MISMATCH`, `WX_INIT_PORT_BUSY`,
  `WX_INIT_OCCUPANCY_UNKNOWN`, `WX_INIT_EXCLUSIVE_FAILED`,
  `WX_INIT_CONFIGURE_FAILED`, `WX_INIT_SHORT_WRITE`, `WX_INIT_ZERO_WRITE`,
  `WX_INIT_INVALID_RESPONSE`, `WX_INIT_UNEXPECTED_INPUT`,
  `WX_INIT_UNKNOWN_ORIENTATION`, `WX_INIT_ORIENTATION_MISMATCH`,
  `WX_INIT_RX_ZERO_OR_EOF`, `WX_INIT_TRANSPORT_ERROR`), and
  `wx_init_occupancy_t` (`WX_INIT_OCCUPANCY_UNCHECKED`,
  `WX_INIT_OCCUPANCY_CLEAR_VISIBLE`, `WX_INIT_OCCUPANCY_BUSY`,
  `WX_INIT_OCCUPANCY_INCONCLUSIVE`).
  `wx_init_report_t`
  reports stage/status/reason/errno, identity strings, occupancy, accepted
  writes, up to four RX bytes, response flags, Fill state, timestamps, and
  cleanup/state history. `wx_init_info_t` reports managed state, confirmed
  orientation/geometry, framebuffer-sync knowledge, and last reason.
* `wx_init_channel_t` supplies RX, guard, seal and release callbacks in
  addition to `wx_transport_t`; `wx_init_connector_t` supplies a monotonic
  clock and acquire callback. `wx_init_options_t` contains `struct_size`,
  model, orientation, stride, exactly 2000 ms timeout, allocator, and
  connector.  These callbacks and their contexts remain caller-owned.
* POSIX types are opaque `wx_posix_transport_t`; `wx_posix_options_t` has
  `struct_size`, `rtscts`, `hangup_on_close`, allocator.  Guarded selection
  uses `wx_posix_init_options_t` (`struct_size`,
  `allow_unknown_occupancy`, allocator) and `wx_posix_select_options_t`
  (that base plus `device` and `serial`).
* `wx_memory_transport_t` and `wx_memory_action_t` are caller-owned test
  state: bytes/capacity/counters/script/time controls and action `status` plus
  `limit`. `wx_fastlz_stats_t` reports its `struct_size`, timing request,
  input/compressed/block/wire sizes, blocks, and timing counters.
  `WX_FASTLZ_BLOCK_SIZE` is 1920.

## Function reference

Arguments named `d`, `s`, `out`, `report`, `x`, `y`, `w`, and `h` below mean a
device, surface, output pointer, initialisation report, and pixel coordinates
or dimensions.  `out` must point to NULL for factory functions.  `size_t`
geometry is host-side and all widths/heights must be nonzero unless an entry
explicitly says otherwise.

### Legacy display API (`weact_display.h`)

| Function and signature | Contract, result, effects, applicability |
|---|---|
| `uint16_t weact_rgb_to_brg565(uint8_t r,uint8_t g,uint8_t b)` | Converts RGB888 to legacy BRG565; no I/O. Use only with the legacy canvas/profile. |
| `bool weact_init(weact_display_t *display,const char *port_name)` | Zeroes/opens and allocates the legacy display. `display` and a serial path are required. Returns false and records `last_error` on failure; opens hardware and is intended for FS096 compatibility. |
| `void weact_close(weact_display_t *display)` / `void weact_cleanup(weact_display_t *display)` | Close/free legacy connection and buffers; NULL is harmless. `cleanup` calls `close`. No further use until re-init. |
| `void weact_clear_buffer(weact_display_t *display,uint16_t color)` | Fills the back buffer only; no transmission. Requires an initialised back buffer. |
| `void weact_swap_buffers(weact_display_t *display)` | Swaps front/back host buffers; no I/O. |
| `bool weact_flush_buffer(weact_display_t *display)` | Sends the back buffer; requires a connected legacy display. False records an error. |
| `bool weact_update_display(weact_display_t *display)` | Flushes then swaps on success. Same connection requirement/result as flush. |
| `void weact_draw_pixel(weact_display_t *display,int x,int y,uint16_t color)` | Writes an in-bounds pixel into the back buffer; invalid/absent storage is ignored. No I/O. |
| `void weact_draw_line(weact_display_t *display,int x1,int y1,int x2,int y2,uint16_t color)` | Draws a legacy-buffer Bresenham line; pixels outside are handled by pixel bounds checks. No I/O. |
| `void weact_draw_rect(weact_display_t *display,int x,int y,int width,int height,uint16_t color,bool filled)` | Draws outline or filled rectangle into the legacy buffer; no I/O. |
| `void weact_draw_circle(weact_display_t *display,int cx,int cy,int radius,uint16_t color,bool filled)` | Draws outline or filled circle into the legacy buffer; no I/O. |
| `bool weact_set_orientation(weact_display_t *display,weact_orientation_t orientation)` | Sends legacy orientation and changes legacy canvas geometry/buffers on success. Valid enum value and connection required. |
| `bool weact_set_brightness(weact_display_t *display,uint8_t brightness,uint16_t time_ms)` | Sends a brightness command; connection required. The protocol encoder caps time to 5000 ms. |
| `bool weact_fill_screen(weact_display_t *display,uint16_t color)` | Sends a whole-screen Fill command; it changes the LCD, not the host back buffer. |
| `bool weact_system_reset(weact_display_t *display)` | Sends a display reset command. It is a hardware-affecting legacy call; do not use in preserve workflows. |
| `bool weact_is_connected(const weact_display_t *display)` | Returns current legacy connection flag. |
| `int weact_get_display_width(const weact_display_t *display)` / `int weact_get_display_height(const weact_display_t *display)` | Return cached dimensions, or 0 for NULL. |
| `weact_orientation_t weact_get_orientation(const weact_display_t *display)` / `uint8_t weact_get_brightness(const weact_display_t *display)` | Return cached state; NULL returns landscape / 0 respectively. |
| `const char *weact_get_last_error(const weact_display_t *display)` | Borrowed diagnostic string, or a static invalid-display message. Copy it before reinitialising the display. |
| `void weact_get_info(const weact_display_t *display,char *buffer,size_t size)` | Formats cached information into caller buffer. `buffer` and nonzero `size` required; no I/O. |

### FreeType text API (`text_freetype.h`)

All text functions operate on a legacy `weact_display_t` back buffer, not a
`wx_surface_t`; drawing does not transmit until the legacy caller flushes.

| Function and signature | Contract, result, effects |
|---|---|
| `bool ft_text_available(void)` | Checks whether FreeType can initialise. No display I/O. |
| `const char *ft_text_get_default_font(void)` | Returns the default DejaVu Sans path; borrowed static storage. |
| `ft_text_context_t *ft_text_init(weact_display_t *display,const char *font_path,int font_size)` | Creates a text context for an initialised legacy display. NULL font selects default; valid size is 6–32. NULL means failure. |
| `void ft_text_cleanup(ft_text_context_t *ctx)` | Releases context/font resources; NULL is harmless. |
| `bool ft_text_set_size(ft_text_context_t *ctx,int size)` | Changes face size; size must be 6–32. Returns false on invalid context/size or FreeType error. |
| `void ft_text_set_color(ft_text_context_t *ctx,uint16_t color)` | Sets packed legacy BRG565 colour; NULL is ignored. |
| `void ft_text_set_alignment(ft_text_context_t *ctx,ft_text_align_t align)` | Sets left/centre/right alignment; NULL is ignored. |
| `int ft_text_get_width(ft_text_context_t *ctx,const char *text)` | Measures UTF-8 text in pixels; 0 for invalid arguments/error. |
| `int ft_text_get_height(ft_text_context_t *ctx)` | Returns current face line height, or 0 if invalid. |
| `void ft_text_draw(ft_text_context_t *ctx,int x,int y,const char *text)` | Draws UTF-8 text at baseline `x,y` into legacy back buffer; invalid arguments are ignored. |
| `void ft_text_draw_wrapped(ft_text_context_t *ctx,int x,int y,int max_width,int max_height,const char *text)` | Draws UTF-8 word-wrapped text bounded by supplied pixel area; no return status, invalid input is ignored. |

### Surfaces and basic devices (`weact_extended.h`)

| Function and signature | Contract, result, effects, applicability |
|---|---|
| `wx_status_t wx_surface_create(size_t width,size_t height,size_t stride,wx_pixel_format_t format,const wx_allocator_t *allocator,wx_surface_t **out)` | Creates zeroed owned 16-bit surface. `stride=0` selects `width*2`; otherwise it must cover a row. Valid formats only; checks overflow. Returns `WX_OK`, `WX_INVALID`, `WX_OVERFLOW`, or `WX_NOMEM`. |
| `void wx_surface_destroy(wx_surface_t *surface)` | Frees owned surface with its selected allocator; NULL is harmless. |
| `wx_status_t wx_surface_info(const wx_surface_t *surface,wx_surface_info_t *out)` | Copies geometry/format; both pointers required, otherwise `WX_INVALID`. |
| `uint8_t *wx_surface_pixels(wx_surface_t *surface,size_t *length)` | Returns borrowed mutable storage and capacity including row padding, or NULL/0 for NULL surface. Valid until destroy or successful device-orientation change. |
| `wx_status_t wx_surface_set_pixel(wx_surface_t *surface,int x,int y,uint16_t packed_color)` | Stores one profile-packed pixel; strict bounds return `WX_BOUNDS`; no I/O. |
| `wx_status_t wx_surface_set_rgb(wx_surface_t *surface,int x,int y,uint8_t r,uint8_t g,uint8_t b)` | Packs RGB888 for the surface then stores it; same error/bounds rules. |
| `wx_status_t wx_surface_clear(wx_surface_t *surface,uint16_t packed_color)` | Fills host pixels only; `WX_INVALID` for NULL. |
| `uint16_t wx_pack_rgb(wx_pixel_format_t format,uint8_t r,uint8_t g,uint8_t b)` | Packs RGB888 for the named format; invalid format returns 0 (ambiguous with black, so validate first). |
| `wx_status_t wx_device_create(const wx_device_options_t *options,wx_device_t **out)` | Pure host factory: explicit model/geometry/orientation, valid callbacks, allocator and nonzero timeout required. No opening or protocol I/O. Returns normal validation/allocation status. |
| `void wx_device_destroy(wx_device_t *device)` | Releases device, surface and any managed lease; NULL is harmless. Destroy a managed device after `wx_device_init_close()` when explicit cleanup reporting is required. |
| `wx_surface_t *wx_device_surface(wx_device_t *device)` | Returns borrowed canvas. For managed devices, obtaining it marks framebuffer synchronisation unknown. Do not destroy it. |
| `wx_status_t wx_device_set_orientation(wx_device_t *device,uint8_t orientation)` | Allocates replacement canvas before sending setter. Accepts 0–3 for generic device; a managed device permits only 2. On success old canvas pixels are replaced; transmission failure seals stream. |
| `wx_status_t wx_device_set_brightness(wx_device_t *device,uint8_t brightness,uint16_t time_ms)` | Sends brightness packet; protocol limits fade to 5000 ms. No canvas change; transfer failure seals. |
| `wx_status_t wx_device_fill(wx_device_t *device,uint16_t packed_color)` | Sends full-device Fill only; does not alter host pixels. Managed geometry must be confirmed. |
| `wx_status_t wx_device_bitmap(wx_device_t *device)` | Sends complete canvas only. Managed geometry must be confirmed; no swap/partial update. Failure seals. |
| `bool wx_device_stream_failed(const wx_device_t *device)` | True when a transfer failure sealed the stream; recreate only after independently verified recovery. |

### Graphics and regions (`weact_graphics.h`)

| Function and signature | Contract, result, effects, applicability |
|---|---|
| `wx_status_t wx_device_profile(const wx_device_t *device,wx_model_t *model,wx_surface_info_t *info)` | Copies selected model and canvas info; never autodetects hardware. All pointers required. |
| `wx_status_t wx_surface_fill_rgb(wx_surface_t *surface,size_t x,size_t y,size_t width,size_t height,uint8_t r,uint8_t g,uint8_t b)` | Fills strict in-bounds RGB888 rectangle in host memory. Empty rectangle at an in-bounds edge is a no-op; no I/O. |
| `wx_status_t wx_surface_bitmap_rgb(wx_surface_t *surface,size_t x,size_t y,size_t width,size_t height,const uint8_t *rgb,size_t length,size_t stride)` | Copies RGB888 rows to host surface. `stride=0` means `width*3`; source must not overlap, and length covers padding. Strict bounds/overflow checks; no I/O. |
| `wx_status_t wx_device_bitmap_region(wx_device_t *device,size_t x,size_t y,size_t width,size_t height)` | Sends exactly a nonempty strict-bounds rectangle from canvas. No allocation, readback, implicit orientation or host mutation. Managed geometry must be confirmed; failure seals. FS35 regional CLI use is this API after guarded preserve/confirm. |

### Managed FS35 initialisation (`weact_init.h`, `weact_posix_init.h`)

The managed factory requires FS35, configured orientation 2, timeout exactly
2000 ms, correct structure sizes, a valid connector, and stride zero or at
least 960 bytes.  `wx_device_init()` acquires a lease, verifies guards, sends
orientation 02, queries it, validates response framing, then fills black.
It accepts readback 02 only. `wx_device_init_preserve()` acquires without
protocol I/O; call confirmation before Bitmap/Fill. Confirmation reads
orientation and accepts FS35 landscape 02 or 03 as 480x320, rejecting 00/01
and unknown values without changing orientation. It does not establish a
known physical framebuffer outside data subsequently sent.

| Function and signature | Contract, result, effects |
|---|---|
| `wx_status_t wx_posix_init_connector(const wx_posix_init_options_t *options,wx_init_connector_t *out)` | Builds the guarded connector for a configured legacy `/dev/weact35` mapping. It accepts any FS35 that passes VID:PID, manufacturer, Product, CDC ACM, node, and occupancy checks; it does not pin a USB serial. Binding itself does no I/O. Output/options structure and allocator must be valid. |
| `wx_status_t wx_posix_init_connector_select(const wx_posix_select_options_t *options,wx_init_connector_t *out)` | Builds portable guarded FS35 selection connector. `device` may select an exact matching CDC ACM node; `serial` narrows matching devices; NULL selection requires exactly one match. Acquisition later checks VID:PID, Product, interface and occupancy. |
| `wx_status_t wx_device_init(const wx_init_options_t *options,wx_device_t **out,wx_init_report_t *report)` | Full managed FS35 setup above. Optional report must have matching `struct_size`; on failure inspect status/reason/errno, and no unsafe retry occurs. Success owns lease. |
| `wx_status_t wx_device_init_preserve(const wx_init_options_t *options,wx_device_t **out,wx_init_report_t *report)` | Managed acquisition only; no setter, query, fill, read or write during factory. Then use confirm before graphical transmission. |
| `wx_status_t wx_device_confirm_orientation(wx_device_t *device,wx_init_report_t *report)` | Sends guarded orientation query/readback for a managed preserve device. Requires live unsealed managed device; failure closes/seals lease. |
| `wx_status_t wx_device_get_init_info(const wx_device_t *device,wx_init_info_t *out)` | Copies managed state; `out->struct_size` must match. Generic devices report created/unmanaged state. |
| `wx_status_t wx_device_init_close(wx_device_t *device,wx_init_report_t *report)` | Releases managed lease once, without implicit flush/protocol commands. Idempotent close does not retry a failed release; report separates primary and cleanup errors. |

`allow_unknown_occupancy` is a deliberately narrow POSIX connector policy:
it can accept a foreign-process permission failure after identity checks.  It
does not accept known busy ports, namespace/scanner failures, wrong Product,
or a failed exclusive lock.  That guard belongs to the POSIX managed connector,
not to `wx_device_create()` or `wx_posix_transport_open()`.

### Transports and packet encoders

| Function and signature | Contract, result, effects |
|---|---|
| `wx_status_t wx_memory_transport_init(wx_memory_transport_t *memory,uint8_t *storage,size_t capacity)` | Clears and configures caller-owned deterministic memory transport. NULL storage is allowed only with zero capacity; no allocation/I/O. |
| `wx_transport_t wx_memory_transport_bind(wx_memory_transport_t *memory)` | Returns callbacks borrowing `memory`, or all-zero transport for NULL. Useful for offline protocol testing. |
| `wx_status_t wx_posix_transport_open(const char *path,const wx_posix_options_t *options,wx_posix_transport_t **out)` | Opens/configures a Linux tty raw 8N1 at B115200. It is low-level: no identity guard, no reset/flush, and settings are not restored. Opens hardware; check status and `errno`. |
| `wx_transport_t wx_posix_transport_bind(wx_posix_transport_t *transport)` | Returns transport callbacks borrowing live backend, or zero callbacks for NULL. Keep backend alive while device uses it. |
| `wx_status_t wx_posix_transport_close(wx_posix_transport_t *transport)` | Closes once, without flush/drain or retry of Linux `close(EINTR)`; invalidates backend even if close reports error. |
| `void wx_posix_transport_destroy(wx_posix_transport_t *transport)` | Closes if necessary and frees backend. Do not use borrowed callbacks after it. |
| `int wx_posix_transport_last_errno(const wx_posix_transport_t *transport)` | Returns stored OS error, or `EINVAL` for NULL. |
| `wx_status_t wx_encode_bitmap(uint8_t *out,size_t capacity,uint32_t xs,uint32_t ys,uint32_t xe,uint32_t ye)` | Pure 10-byte Bitmap packet encoder; coordinates are inclusive, ordered, and at most `UINT16_MAX`. |
| `wx_status_t wx_encode_bitmap_fastlz(uint8_t *out,size_t capacity,uint32_t xs,uint32_t ys,uint32_t xe,uint32_t ye)` | Same validation/layout but FastLZ Bitmap command. Pure; it does not compress pixels. |
| `wx_status_t wx_encode_fill(uint8_t *out,size_t capacity,uint32_t xs,uint32_t ys,uint32_t xe,uint32_t ye,uint16_t color)` | Pure 12-byte inclusive Fill packet encoder. |
| `wx_status_t wx_encode_orientation(uint8_t *out,size_t capacity,uint8_t orientation)` | Pure 3-byte encoder; accepts 0–3 or 5. |
| `wx_status_t wx_encode_brightness(uint8_t *out,size_t capacity,uint8_t brightness,uint16_t time_ms)` | Pure 5-byte encoder; capacity at least 5, fade capped at 5000 ms. |
| `wx_status_t wx_fastlz_encode(const uint8_t *input,size_t length,uint8_t *output,size_t capacity,size_t *used)` | Bounded level-1 encoder for nonoverlapping FS35 landscape blocks 1–1920 bytes; no allocation/intrinsics. `used` receives encoded bytes on success. |

### FS35 optional operations (`weact_fs35.h`)

| Function and signature | Contract, result, effects |
|---|---|
| `wx_status_t wx_device_set_backlight_percent(wx_device_t *device,unsigned percent,uint16_t fade_ms)` | Managed ready/preserved FS35 only. `percent` is 0–100 and becomes floor(percent×255/100); fade is capped at 5000 ms. Does not change canvas; failure seals. |
| `wx_status_t wx_device_get_backlight(wx_device_t *device,uint8_t *value)` | Guarded managed FS35 query, reading `83 raw 0A`; live ready/preserved device and non-NULL output required. Failure seals. |
| `wx_status_t wx_device_bitmap_fastlz(wx_device_t *device,wx_fastlz_stats_t *stats)` | Sends compressed whole 480x320 FS35 canvas. Managed confirmed landscape required; stats is optional but, when supplied, needs correct `struct_size`. |
| `wx_status_t wx_device_bitmap_fastlz_rect(wx_device_t *device,size_t x,size_t y,size_t width,size_t height,wx_fastlz_stats_t *stats)` | Same FastLZ rules for strict in-bounds FS35 rectangle. All compression/allocation precedes transmission; no uncompressed fallback after failure. |

## Additional complete examples

Every example in this section compiles with the same command (substitute its
file name); compilation does not require a USB device:

```sh
cc -std=c11 -Wall -Wextra example.c $(pkg-config --cflags --libs weact) -o example
```

### Select a guarded FS35 connector without opening it

Binding a connector reads no tty and sends no display command.  Acquisition
does happen only when it is passed to `wx_device_init*()`.

```c
#include <weact/weact_posix_init.h>

int main(void) {
    wx_posix_select_options_t select = {
        .struct_size = sizeof select,
        .base = { .struct_size = sizeof select.base },
        .device = "/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00"
    };
    wx_init_connector_t connector;
    return wx_posix_init_connector_select(&select, &connector) == WX_OK ? 0 : 1;
}
```

### Managed FS35 connection, initialisation, and close

This is a hardware example: run it only after the path, Product, permissions,
and port ownership are deliberately checked.  It selects orientation 02 and
performs a black full-screen Fill; `wx_device_init_close()` then releases the
lease.  Check `report.reason` and `report.primary_errno` on failure.

```c
#include <weact/weact_init.h>
#include <weact/weact_posix_init.h>

int main(void) {
    wx_device_t *device = NULL;
    wx_init_report_t report = { .struct_size = sizeof report };
    wx_posix_select_options_t select = {
        .struct_size = sizeof select,
        .base = { .struct_size = sizeof select.base },
        .device = "/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00"
    };
    wx_init_options_t options = { .struct_size = sizeof options,
        .model = WX_FS35, .orientation = 2, .timeout_ms = 2000 };
    wx_status_t st = wx_posix_init_connector_select(&select, &options.connector);
    if (st == WX_OK) st = wx_device_init(&options, &device, &report);
    if (device) {
        (void)wx_device_init_close(device, &report);
        wx_device_destroy(device);
    }
    return st == WX_OK ? 0 : 1;
}
```

### Legacy FS096 drawing

This compatibility example opens the requested path and therefore is also a
hardware action.  It shows the legacy draw/flush sequence; new FS35 code
should use the managed API instead.

```c
#include <weact/weact_display.h>

int main(void) {
    weact_display_t display;
    if (!weact_init(&display, "/dev/ttyACM0")) return 1;
    weact_clear_buffer(&display, WEACT_BLACK);
    weact_draw_rect(&display, 10, 10, 40, 20, WEACT_WHITE, true);
    int rc = weact_update_display(&display) ? 0 : 1;
    weact_cleanup(&display);
    return rc;
}
```

### QR and barcodes

There is no installed public QR or linear-barcode C API in 1.0.0.  Use
`weact-cli` for its verified code rendering/planning rules, or render an image
in your application, copy it with `wx_surface_bitmap_rgb()`, and transmit the
chosen rectangle with `wx_device_bitmap_region()`.  The library does not
provide a function that preserves or reads unknown pixels outside that region.

## What the C API does not provide

QR/Code 128/EAN/Code 39 layout, quiet-zone planning, CLI syntax, text/image
parsing, and `/init` are application/CLI features, not installed public C
API entry points.  `Include/weact_codes.h` and private headers visible in the
source distribution are not installed by `libweact-dev`; its encoder functions
are not among the 79 `libweact.so.1` exports.  Do not link against them as a
public library contract.  To display an already rendered regional image in C,
write RGB data into a managed confirmed FS35 surface and call
`wx_device_bitmap_region()`; preserving unknown LCD data remains the caller's
responsibility.

## ABI inventory

The list below was obtained from the verified 1.0.0 ARM64 release DSO with
`nm -D --defined-only`; all `T/W` exports are covered above.  The one exception
is deliberately visible but private `wx_device_bitmap_rect_internal`: it is
declared only by non-installed `weact_bitmap_internal.h`, is used for internal
measurement, and is **not** a supported application API. Its exact private
signature is `wx_status_t wx_device_bitmap_rect_internal(wx_device_t *, size_t,
size_t, size_t, size_t, wx_bitmap_stats_t *)`; `wx_bitmap_stats_t` is private
too, so applications must not declare or call it themselves.

| Export group | Symbols | Installed header / documentation status |
|---|---|---|
| FreeType (11) | `ft_text_available`, `cleanup`, `draw`, `draw_wrapped`, `get_default_font`, `get_height`, `get_width`, `init`, `set_alignment`, `set_color`, `set_size` | `text_freetype.h`; documented above |
| Legacy (23) | `weact_cleanup`, `clear_buffer`, `close`, `draw_circle`, `draw_line`, `draw_pixel`, `draw_rect`, `fill_screen`, `flush_buffer`, `get_brightness`, `get_display_height`, `get_display_width`, `get_info`, `get_last_error`, `get_orientation`, `init`, `is_connected`, `rgb_to_brg565`, `set_brightness`, `set_orientation`, `swap_buffers`, `system_reset`, `update_display` | `weact_display.h`; documented above |
| Core surfaces/devices (16) | `wx_surface_create`, `destroy`, `info`, `pixels`, `set_pixel`, `set_rgb`, `clear`, `wx_pack_rgb`, `wx_device_create`, `destroy`, `surface`, `set_orientation`, `set_brightness`, `fill`, `bitmap`, `stream_failed` | `weact_extended.h`; documented above |
| Graphics (4) | `wx_device_profile`, `wx_surface_fill_rgb`, `wx_surface_bitmap_rgb`, `wx_device_bitmap_region` | `weact_graphics.h`; documented above |
| Managed init (5) | `wx_device_init`, `init_preserve`, `confirm_orientation`, `get_init_info`, `init_close` | `weact_init.h`; documented above |
| POSIX/memory (9) | `wx_posix_init_connector`, `connector_select`, `wx_posix_transport_open`, `bind`, `close`, `destroy`, `last_errno`, `wx_memory_transport_init`, `bind` | respective installed headers; documented above |
| Protocol/FastLZ (6) | `wx_encode_bitmap`, `bitmap_fastlz`, `fill`, `orientation`, `brightness`, `wx_fastlz_encode` | `weact_protocol.h`, `weact_fastlz.h`; documented above |
| FS35 extension (4) | `wx_device_bitmap_fastlz`, `bitmap_fastlz_rect`, `get_backlight`, `set_backlight_percent` | `weact_fs35.h`; documented above |
| Private technical (1) | `wx_device_bitmap_rect_internal` | non-installed header; not supported for callers |

Total: 11 + 23 + 16 + 4 + 5 + 9 + 6 + 4 + 1 = **79**.

### Per-symbol export cross-reference

<a id="ft_text_available"></a><a id="ft_text_cleanup"></a><a id="ft_text_draw"></a><a id="ft_text_draw_wrapped"></a><a id="ft_text_get_default_font"></a><a id="ft_text_get_height"></a><a id="ft_text_get_width"></a><a id="ft_text_init"></a><a id="ft_text_set_alignment"></a><a id="ft_text_set_color"></a><a id="ft_text_set_size"></a><a id="weact_cleanup"></a><a id="weact_clear_buffer"></a><a id="weact_close"></a><a id="weact_draw_circle"></a><a id="weact_draw_line"></a><a id="weact_draw_pixel"></a><a id="weact_draw_rect"></a><a id="weact_fill_screen"></a><a id="weact_flush_buffer"></a><a id="weact_get_brightness"></a><a id="weact_get_display_height"></a><a id="weact_get_display_width"></a><a id="weact_get_info"></a><a id="weact_get_last_error"></a><a id="weact_get_orientation"></a><a id="weact_init"></a><a id="weact_is_connected"></a><a id="weact_rgb_to_brg565"></a><a id="weact_set_brightness"></a><a id="weact_set_orientation"></a><a id="weact_swap_buffers"></a><a id="weact_system_reset"></a><a id="weact_update_display"></a><a id="wx_device_bitmap"></a><a id="wx_device_bitmap_fastlz"></a><a id="wx_device_bitmap_fastlz_rect"></a><a id="wx_device_bitmap_rect_internal"></a><a id="wx_device_bitmap_region"></a><a id="wx_device_confirm_orientation"></a><a id="wx_device_create"></a><a id="wx_device_destroy"></a><a id="wx_device_fill"></a><a id="wx_device_get_backlight"></a><a id="wx_device_get_init_info"></a><a id="wx_device_init"></a><a id="wx_device_init_close"></a><a id="wx_device_init_preserve"></a><a id="wx_device_profile"></a><a id="wx_device_set_backlight_percent"></a><a id="wx_device_set_brightness"></a><a id="wx_device_set_orientation"></a><a id="wx_device_stream_failed"></a><a id="wx_device_surface"></a><a id="wx_encode_bitmap"></a><a id="wx_encode_bitmap_fastlz"></a><a id="wx_encode_brightness"></a><a id="wx_encode_fill"></a><a id="wx_encode_orientation"></a><a id="wx_fastlz_encode"></a><a id="wx_memory_transport_bind"></a><a id="wx_memory_transport_init"></a><a id="wx_pack_rgb"></a><a id="wx_posix_init_connector"></a><a id="wx_posix_init_connector_select"></a><a id="wx_posix_transport_bind"></a><a id="wx_posix_transport_close"></a><a id="wx_posix_transport_destroy"></a><a id="wx_posix_transport_last_errno"></a><a id="wx_posix_transport_open"></a><a id="wx_surface_bitmap_rgb"></a><a id="wx_surface_clear"></a><a id="wx_surface_create"></a><a id="wx_surface_destroy"></a><a id="wx_surface_fill_rgb"></a><a id="wx_surface_info"></a><a id="wx_surface_pixels"></a><a id="wx_surface_set_pixel"></a><a id="wx_surface_set_rgb"></a>

| Symbol | Header | Reference section | Documentation status |
|---|---|---|---|
| `ft_text_available` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_cleanup` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_draw` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_draw_wrapped` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_get_default_font` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_get_height` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_get_width` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_init` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_set_alignment` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_set_color` | `text_freetype.h` | FreeType | Documented above |
| `ft_text_set_size` | `text_freetype.h` | FreeType | Documented above |
| `weact_cleanup` | `weact_display.h` | Legacy | Documented above |
| `weact_clear_buffer` | `weact_display.h` | Legacy | Documented above |
| `weact_close` | `weact_display.h` | Legacy | Documented above |
| `weact_draw_circle` | `weact_display.h` | Legacy | Documented above |
| `weact_draw_line` | `weact_display.h` | Legacy | Documented above |
| `weact_draw_pixel` | `weact_display.h` | Legacy | Documented above |
| `weact_draw_rect` | `weact_display.h` | Legacy | Documented above |
| `weact_fill_screen` | `weact_display.h` | Legacy | Documented above |
| `weact_flush_buffer` | `weact_display.h` | Legacy | Documented above |
| `weact_get_brightness` | `weact_display.h` | Legacy | Documented above |
| `weact_get_display_height` | `weact_display.h` | Legacy | Documented above |
| `weact_get_display_width` | `weact_display.h` | Legacy | Documented above |
| `weact_get_info` | `weact_display.h` | Legacy | Documented above |
| `weact_get_last_error` | `weact_display.h` | Legacy | Documented above |
| `weact_get_orientation` | `weact_display.h` | Legacy | Documented above |
| `weact_init` | `weact_display.h` | Legacy | Documented above |
| `weact_is_connected` | `weact_display.h` | Legacy | Documented above |
| `weact_rgb_to_brg565` | `weact_display.h` | Legacy | Documented above |
| `weact_set_brightness` | `weact_display.h` | Legacy | Documented above |
| `weact_set_orientation` | `weact_display.h` | Legacy | Documented above |
| `weact_swap_buffers` | `weact_display.h` | Legacy | Documented above |
| `weact_system_reset` | `weact_display.h` | Legacy | Documented above |
| `weact_update_display` | `weact_display.h` | Legacy | Documented above |
| `wx_device_bitmap` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_device_bitmap_fastlz` | `weact_fs35.h` | FS35 extension | Documented above |
| `wx_device_bitmap_fastlz_rect` | `weact_fs35.h` | FS35 extension | Documented above |
| `wx_device_bitmap_rect_internal` | not installed (`weact_bitmap_internal.h`) | Private technical | Private; explained above |
| `wx_device_bitmap_region` | `weact_graphics.h` | Graphics | Documented above |
| `wx_device_confirm_orientation` | `weact_init.h` | Managed init | Documented above |
| `wx_device_create` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_device_destroy` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_device_fill` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_device_get_backlight` | `weact_fs35.h` | FS35 extension | Documented above |
| `wx_device_get_init_info` | `weact_init.h` | Managed init | Documented above |
| `wx_device_init` | `weact_init.h` | Managed init | Documented above |
| `wx_device_init_close` | `weact_init.h` | Managed init | Documented above |
| `wx_device_init_preserve` | `weact_init.h` | Managed init | Documented above |
| `wx_device_profile` | `weact_graphics.h` | Graphics | Documented above |
| `wx_device_set_backlight_percent` | `weact_fs35.h` | FS35 extension | Documented above |
| `wx_device_set_brightness` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_device_set_orientation` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_device_stream_failed` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_device_surface` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_encode_bitmap` | `weact_protocol.h` | Protocol/FastLZ | Documented above |
| `wx_encode_bitmap_fastlz` | `weact_protocol.h` | Protocol/FastLZ | Documented above |
| `wx_encode_brightness` | `weact_protocol.h` | Protocol/FastLZ | Documented above |
| `wx_encode_fill` | `weact_protocol.h` | Protocol/FastLZ | Documented above |
| `wx_encode_orientation` | `weact_protocol.h` | Protocol/FastLZ | Documented above |
| `wx_fastlz_encode` | `weact_fastlz.h` | Protocol/FastLZ | Documented above |
| `wx_memory_transport_bind` | `weact_memory_transport.h` | POSIX/memory | Documented above |
| `wx_memory_transport_init` | `weact_memory_transport.h` | POSIX/memory | Documented above |
| `wx_pack_rgb` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_posix_init_connector` | `weact_posix_init.h` | POSIX/memory | Documented above |
| `wx_posix_init_connector_select` | `weact_posix_init.h` | POSIX/memory | Documented above |
| `wx_posix_transport_bind` | `weact_posix_transport.h` | POSIX/memory | Documented above |
| `wx_posix_transport_close` | `weact_posix_transport.h` | POSIX/memory | Documented above |
| `wx_posix_transport_destroy` | `weact_posix_transport.h` | POSIX/memory | Documented above |
| `wx_posix_transport_last_errno` | `weact_posix_transport.h` | POSIX/memory | Documented above |
| `wx_posix_transport_open` | `weact_posix_transport.h` | POSIX/memory | Documented above |
| `wx_surface_bitmap_rgb` | `weact_graphics.h` | Graphics | Documented above |
| `wx_surface_clear` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_surface_create` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_surface_destroy` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_surface_fill_rgb` | `weact_graphics.h` | Graphics | Documented above |
| `wx_surface_info` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_surface_pixels` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_surface_set_pixel` | `weact_extended.h` | Core surfaces/devices | Documented above |
| `wx_surface_set_rgb` | `weact_extended.h` | Core surfaces/devices | Documented above |

## Hardware safety

Opening a tty can change modem-line state even though the library sends no
extra reset.  Select a stable `/dev/serial/by-id/...` path, verify it refers to
the intended Product (`Display FS V1` for FS35 or `Display FS 0.96 Inch` for
FS096), ensure permission and no other user of the port, then use guarded
managed FS35 initialisation when it fits the application.  Both products use
VID:PID `1a86:fe0c`; VID:PID alone is insufficient.

Full managed init deliberately changes FS35 orientation to 02 and performs a
black full-screen Fill.  Preserve plus confirmation performs no setter/Fill,
but it cannot read unknown LCD pixels; it is appropriate only when the caller
knows exactly which rectangle it will transmit.  A successful USB write means
the host accepted transmission, not a byte-for-byte observation of the LCD.
Disconnects, timeouts, partial transfers and guard failures must terminate the
operation; do not automatically reconnect or replay a packet.

For hardware-oriented end-to-end examples, use the separately documented CLI
only after deliberate device checks.  The library examples in this reference
compile without a device and are not run as hardware tests.
