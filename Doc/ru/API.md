# Справочник C API

[English](../en/API.md) | [Русский](API.md) | [Установка](INSTALL.md) | [CLI](CLI.md) | [Оборудование](HARDWARE.md)

Это справочник API WeAct35 1.0.0. Он описывает заголовки, устанавливаемые
пакетом `libweact-dev`, а не private-заголовки из исходного дерева или
возможности только CLI. SONAME библиотеки — `libweact.so.1`; проверенная
ARM64-сборка имеет 79 экспортируемых символов. `pkg-config --modversion weact`
возвращает `1.0.0`.

## Назначение и первая программа

В библиотеке намеренно существуют два разных интерфейса.

* Legacy-интерфейс `weact_*` работает с изменяемой публичной структурой
  `weact_display_t` и буфером FS096 160x80. Он не выполняет Linux-проверки
  USB identity/occupancy, нужные для FS35.
* Новый `wx_*` использует непрозрачные device/surface, явную модель,
  транспорт, заданный вызывающим кодом, и `wx_status_t`. Managed POSIX-путь
  инициализации предназначен для FS35; это основа нового приложения.

Установите development-файлы и компилируйте через metadata пакета:

```sh
sudo apt install ./libweact1_1.0.0_arm64.deb ./libweact-dev_1.0.0_arm64.deb
cc -std=c11 -Wall -Wextra app.c $(pkg-config --cflags --libs weact) -o app
```

Установленные заголовки подключаются как `<weact/...>`; не подключайте
заголовок из `Source/` исходного дерева. Библиотека находится в Debian
multiarch-каталоге, заголовки — в `/usr/include/weact/`, `weact.pc` — в
соответствующем каталоге `pkgconfig`.

### Минимальная offline-поверхность

Следующий пример выделяет и меняет только память: USB не открывается и
команды дисплею не передаются.

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

Компилируйте командой выше. Surface — память хоста, не дисплей; для передачи
создайте device и явно вызовите функцию Bitmap только после настройки
подходящего транспорта.

## Модели, пиксели, ошибки и владение

`WX_FS096` имеет 160x80 и `WX_LEGACY_BRG565_BE`; landscape `WX_FS35` —
480x320 и `WX_RGB565_LE`. `wx_pack_rgb()` возвращает значение для профиля,
а surface-writer кладёт байты в порядке этого профиля. Нельзя передавать
FS096 BRG565-данные как FS35 RGB565.

Значения `wx_status_t`: `WX_OK`, `WX_INVALID`, `WX_OVERFLOW`, `WX_NOMEM`,
`WX_BOUNDS`, `WX_INTERRUPTED`, `WX_AGAIN`, `WX_TIMEOUT`, `WX_DISCONNECTED`,
`WX_IO`, `WX_STREAM_FAILED`. Единого accessor для `errno` нет. Для POSIX
транспорта есть `wx_posix_transport_last_errno()`, а managed init записывает
диагностику в `wx_init_report_t`. Ошибка передачи запечатывает `wx_device_t`:
не повторяйте заголовок и не продолжайте передачу этим device.

Объекты через `out` принадлежат вызывающему: освобождайте `wx_surface_t`
через `wx_surface_destroy()`, `wx_device_t` через `wx_device_destroy()`,
POSIX backend через `wx_posix_transport_destroy()`. Возвраты
`wx_device_surface()` и `wx_surface_pixels()` заимствованы. Передача любой
расширяемой public-структуры требует `struct_size = sizeof object`.
Callbacks allocator должны быть либо оба NULL, либо оба заданы; их context
живёт дольше объекта. Приложение сериализует операции с device/surface и
transport: общей гарантии thread safety нет.

## Публичные объявления

| Заголовок | Публичные объявления |
|---|---|
| `weact_display.h` | legacy-структура, цвета, orientation/scroll enum, 23 функции `weact_*` |
| `text_freetype.h` | непрозрачный text context, alignment enum, size macros, 11 функций `ft_text_*` |
| `weact_extended.h` | status/model/format, allocator/transport/surface/device options, surface и базовые device функции |
| `weact_graphics.h` | profile, RGB import/fill, regional Bitmap |
| `weact_init.h` | состояния/reasons/reports managed init, connector/options, пять функций init |
| `weact_posix_init.h` | параметры и factories guarded FS35 connector |
| `weact_posix_transport.h` | низкоуровневый POSIX tty backend |
| `weact_memory_transport.h` | детерминированный memory transport для offline-тестов |
| `weact_protocol.h` | чистые packet encoders |
| `weact_fastlz.h`, `weact_fs35.h` | FastLZ encoder и FS35 backlight/compressed Bitmap |

### Типы, константы и callback-контракты

* `weact_orientation_t`: `WEACT_PORTRAIT=0`, `WEACT_REVERSE_PORTRAIT=1`,
  `WEACT_LANDSCAPE=2`, `WEACT_REVERSE_LANDSCAPE=3`, `WEACT_ROTATE=5`.
  `weact_scroll_dir_t`: `SCROLL_LEFT`, `SCROLL_RIGHT`, `SCROLL_UP`,
  `SCROLL_DOWN`. Макросы legacy: `WEACT_DISPLAY_WIDTH=160`,
  `WEACT_DISPLAY_HEIGHT=80`, `WEACT_MAX_BUFFER_SIZE=25600`,
  `WEACT_BAUDRATE` (`B115200`) и BRG565-цвета `WEACT_RED`, `GREEN`, `BLUE`,
  `WHITE`, `BLACK`, `YELLOW`, `CYAN`, `MAGENTA`.
  `weact_display.h` сам включает `<termios.h>`, поэтому application может
  вычислять `WEACT_BAUDRATE`, подключив этот public header первым.
* `weact_display_t` публична и ABI-совместима. Поля: `fd`, `port_name[256]`,
  `is_connected`, `orientation`, `brightness`, `display_width`,
  `display_height`, `frame_buffer`, `back_buffer`, `last_error[512]`.
  Обычно приложение выделяет структуру и вызывает `weact_init()`; не
  освобождайте её буферы самостоятельно.
* `ft_text_context_t` непрозрачен. `ft_text_align_t`: `FT_TEXT_ALIGN_LEFT`,
  `CENTER`, `RIGHT`; размерные макросы 8, 12, 16: `SMALL`, `MEDIUM`, `LARGE`.
* `wx_model_t`: `WX_FS096=1`, `WX_FS35=2`; `wx_pixel_format_t`:
  `WX_LEGACY_BRG565_BE=1`, `WX_RGB565_LE=2`. `wx_device_t` и `wx_surface_t`
  непрозрачны. `wx_surface_info_t` содержит `width`, `height`, `stride`,
  `capacity`, `format`.
* `wx_allocator_t`: `context`, `allocate(context,size)`,
  `release(context,pointer)`. `wx_transport_t`: `context`,
  `write(context,bytes,length,written)`, `now_ms(context)`,
  `wait_writable(context,deadline_ms)`. Успешный write сообщает 0..length
  принятых байтов, ошибочный — ноль; callback не должен re-enter device.
* `wx_device_options_t`: `struct_size`, `model`, явные
  `width,height,stride`, `orientation`, ненулевой `timeout_ms`, `transport`,
  `allocator`. 0/1 — portrait, 2/3 — landscape; geometry обязана совпасть с
  моделью и ориентацией.
* Managed init использует `wx_init_state_t` (`WX_INIT_CREATED`,
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
  `WX_INIT_RX_ZERO_OR_EOF`, `WX_INIT_TRANSPORT_ERROR`) и
  `wx_init_occupancy_t` (`WX_INIT_OCCUPANCY_UNCHECKED`,
  `WX_INIT_OCCUPANCY_CLEAR_VISIBLE`, `WX_INIT_OCCUPANCY_BUSY`,
  `WX_INIT_OCCUPANCY_INCONCLUSIVE`).
  `wx_init_report_t`
  содержит stage/status/reason/errno, строки identity, occupancy, принятые
  writes, до четырёх RX байтов, флаги ответа/Fill, timestamps и историю
  cleanup/state. `wx_init_info_t` содержит managed state, подтверждённую
  orientation/geometry, знание framebuffer и last reason.
* `wx_init_channel_t` добавляет RX/guard/seal/release callbacks к
  `wx_transport_t`; `wx_init_connector_t` даёт monotonic clock и acquire.
  `wx_init_options_t`: `struct_size`, model, orientation, stride, ровно 2000
  ms timeout, allocator, connector. Callbacks/context принадлежат вызывающему.
* `wx_posix_transport_t` непрозрачен; `wx_posix_options_t` содержит
  `struct_size`, `rtscts`, `hangup_on_close`, allocator. Guarded selection:
  `wx_posix_init_options_t` (`struct_size`, `allow_unknown_occupancy`,
  allocator), `wx_posix_select_options_t` (base плюс `device`, `serial`).
* `wx_memory_transport_t` и `wx_memory_action_t` — пользовательское тестовое
  состояние: bytes/capacity/counters/script/time и action `status`, `limit`.
  `wx_fastlz_stats_t` содержит `struct_size`, запрос timing, размеры,
  blocks и timing counters. `WX_FASTLZ_BLOCK_SIZE` равен 1920.

## Справочник функций

`d`, `s`, `out`, `report`, `x`, `y`, `w`, `h` ниже означают device, surface,
output pointer, init report, координаты и размеры пикселей. Для factory `out`
указывает на NULL. Размеры `size_t` ненулевые, кроме явно оговорённых случаев.

### Legacy display API (`weact_display.h`)

| Функция и сигнатура | Контракт, результат, эффект, применимость |
|---|---|
| `uint16_t weact_rgb_to_brg565(uint8_t r,uint8_t g,uint8_t b)` | Преобразует RGB888 в legacy BRG565; I/O нет. Только legacy profile/canvas. |
| `bool weact_init(weact_display_t *display,const char *port_name)` | Обнуляет, открывает, выделяет legacy display. Нужны `display` и serial path. false и `last_error` при ошибке; открывает hardware, предназначена для совместимости FS096. |
| `void weact_close(weact_display_t *display)` / `void weact_cleanup(weact_display_t *display)` | Закрывает/освобождает legacy connection и buffers; NULL допустим. `cleanup` вызывает `close`; повторно использовать только после init. |
| `void weact_clear_buffer(weact_display_t *display,uint16_t color)` | Заполняет только back buffer, без передачи. Нужен initialised back buffer. |
| `void weact_swap_buffers(weact_display_t *display)` | Меняет host front/back buffers; I/O нет. |
| `bool weact_flush_buffer(weact_display_t *display)` | Отправляет back buffer; нужен connected legacy display. false записывает ошибку. |
| `bool weact_update_display(weact_display_t *display)` | Flush, затем swap при успехе; те же условия/результат. |
| `void weact_draw_pixel(weact_display_t *display,int x,int y,uint16_t color)` | Пишет pixel в bounds в back buffer; отсутствующий storage/неверная точка игнорируются. I/O нет. |
| `void weact_draw_line(weact_display_t *display,int x1,int y1,int x2,int y2,uint16_t color)` | Рисует Bresenham line в legacy buffer; выход за край обрабатывается pixel bounds checks. |
| `void weact_draw_rect(weact_display_t *display,int x,int y,int width,int height,uint16_t color,bool filled)` | Рисует outline или filled rectangle в legacy buffer, без I/O. |
| `void weact_draw_circle(weact_display_t *display,int cx,int cy,int radius,uint16_t color,bool filled)` | Рисует outline/filled circle в legacy buffer, без I/O. |
| `bool weact_set_orientation(weact_display_t *display,weact_orientation_t orientation)` | Посылает legacy orientation и при успехе меняет geometry/buffers. Нужны valid enum и connection. |
| `bool weact_set_brightness(weact_display_t *display,uint8_t brightness,uint16_t time_ms)` | Посылает brightness; connection обязателен. Encoder ограничивает time 5000 ms. |
| `bool weact_fill_screen(weact_display_t *display,uint16_t color)` | Посылает full-screen Fill; меняет LCD, не host back buffer. |
| `bool weact_system_reset(weact_display_t *display)` | Посылает display reset; hardware-операция legacy, не используйте в preserve workflow. |
| `bool weact_is_connected(const weact_display_t *display)` | Возвращает cached legacy connection flag. |
| `int weact_get_display_width(const weact_display_t *display)` / `int weact_get_display_height(const weact_display_t *display)` | Cached dimensions, либо 0 для NULL. |
| `weact_orientation_t weact_get_orientation(const weact_display_t *display)` / `uint8_t weact_get_brightness(const weact_display_t *display)` | Cached state; NULL возвращает landscape / 0. |
| `const char *weact_get_last_error(const weact_display_t *display)` | Borrowed diagnostic string либо static invalid-display message; скопируйте до reinit. |
| `void weact_get_info(const weact_display_t *display,char *buffer,size_t size)` | Форматирует cached info в caller buffer. Нужны `buffer` и ненулевой `size`; I/O нет. |

### FreeType text API (`text_freetype.h`)

Эти функции рисуют в legacy `weact_display_t` back buffer, не в `wx_surface_t`;
передача произойдёт только после legacy flush.

| Функция и сигнатура | Контракт, результат, эффект |
|---|---|
| `bool ft_text_available(void)` | Проверяет инициализацию FreeType; display I/O нет. |
| `const char *ft_text_get_default_font(void)` | Путь default DejaVu Sans, borrowed static storage. |
| `ft_text_context_t *ft_text_init(weact_display_t *display,const char *font_path,int font_size)` | Создаёт text context для initialised legacy display. NULL font — default; valid size 6–32. NULL при ошибке. |
| `void ft_text_cleanup(ft_text_context_t *ctx)` | Освобождает context/font; NULL допустим. |
| `bool ft_text_set_size(ft_text_context_t *ctx,int size)` | Меняет size 6–32; false при неверном context/size или FreeType error. |
| `void ft_text_set_color(ft_text_context_t *ctx,uint16_t color)` | Задаёт packed legacy BRG565 color; NULL игнорируется. |
| `void ft_text_set_alignment(ft_text_context_t *ctx,ft_text_align_t align)` | Задаёт left/centre/right; NULL игнорируется. |
| `int ft_text_get_width(ft_text_context_t *ctx,const char *text)` | Измеряет UTF-8 в pixels; 0 при invalid/error. |
| `int ft_text_get_height(ft_text_context_t *ctx)` | Высота текущего face; 0 при invalid. |
| `void ft_text_draw(ft_text_context_t *ctx,int x,int y,const char *text)` | Рисует UTF-8 по baseline `x,y` в legacy buffer; invalid input игнорируется. |
| `void ft_text_draw_wrapped(ft_text_context_t *ctx,int x,int y,int max_width,int max_height,const char *text)` | UTF-8 word-wrap в заданной области; status не возвращает, invalid input игнорируется. |

### Surface и базовый device (`weact_extended.h`)

| Функция и сигнатура | Контракт, результат, эффект, применимость |
|---|---|
| `wx_status_t wx_surface_create(size_t width,size_t height,size_t stride,wx_pixel_format_t format,const wx_allocator_t *allocator,wx_surface_t **out)` | Создаёт zeroed owned 16-bit surface. `stride=0` даёт `width*2`; иначе не меньше row. Только valid format; проверяет overflow. Возврат OK/INVALID/OVERFLOW/NOMEM. |
| `void wx_surface_destroy(wx_surface_t *surface)` | Освобождает surface выбранным allocator; NULL допустим. |
| `wx_status_t wx_surface_info(const wx_surface_t *surface,wx_surface_info_t *out)` | Копирует geometry/format; оба указателя обязательны, иначе INVALID. |
| `uint8_t *wx_surface_pixels(wx_surface_t *surface,size_t *length)` | Borrowed mutable storage и capacity с padding, либо NULL/0; до destroy или удачной device orientation change. |
| `wx_status_t wx_surface_set_pixel(wx_surface_t *surface,int x,int y,uint16_t packed_color)` | Пишет профильный packed pixel; строгий bounds — BOUNDS; I/O нет. |
| `wx_status_t wx_surface_set_rgb(wx_surface_t *surface,int x,int y,uint8_t r,uint8_t g,uint8_t b)` | Pack RGB888 для surface и store; те же error/bounds. |
| `wx_status_t wx_surface_clear(wx_surface_t *surface,uint16_t packed_color)` | Заполняет только host pixels; NULL — INVALID. |
| `uint16_t wx_pack_rgb(wx_pixel_format_t format,uint8_t r,uint8_t g,uint8_t b)` | Упаковывает RGB888; invalid format возвращает 0 (неотличимо от black — предварительно валидируйте). |
| `wx_status_t wx_device_create(const wx_device_options_t *options,wx_device_t **out)` | Pure host factory: explicit model/geometry/orientation, valid callbacks/allocator, nonzero timeout. Не открывает и не пишет. |
| `void wx_device_destroy(wx_device_t *device)` | Освобождает device/surface/managed lease; NULL допустим. Для наблюдаемой cleanup-диагностики сначала `wx_device_init_close()`. |
| `wx_surface_t *wx_device_surface(wx_device_t *device)` | Borrowed canvas; для managed device помечает framebuffer sync unknown. Не destroy. |
| `wx_status_t wx_device_set_orientation(wx_device_t *device,uint8_t orientation)` | До setter выделяет replacement canvas. Generic принимает 0–3, managed — только 2. Успех меняет canvas; transfer error seal. |
| `wx_status_t wx_device_set_brightness(wx_device_t *device,uint8_t brightness,uint16_t time_ms)` | Посылает brightness; fade cap 5000 ms. Canvas не меняется; ошибка seal. |
| `wx_status_t wx_device_fill(wx_device_t *device,uint16_t packed_color)` | Посылает full-device Fill, host pixels не меняет. Managed geometry должна быть confirmed. |
| `wx_status_t wx_device_bitmap(wx_device_t *device)` | Отправляет весь canvas; managed geometry должна быть confirmed; swap/partial нет; ошибка seal. |
| `bool wx_device_stream_failed(const wx_device_t *device)` | True при sealed transfer failure; recreate только после independently verified recovery. |

### Graphics и регионы (`weact_graphics.h`)

| Функция и сигнатура | Контракт, результат, эффект, применимость |
|---|---|
| `wx_status_t wx_device_profile(const wx_device_t *device,wx_model_t *model,wx_surface_info_t *info)` | Копирует выбранную model и canvas info; hardware не autodetect. Все указатели обязательны. |
| `wx_status_t wx_surface_fill_rgb(wx_surface_t *surface,size_t x,size_t y,size_t width,size_t height,uint8_t r,uint8_t g,uint8_t b)` | Заполняет strict in-bounds RGB888 rect в host memory. Empty rect на in-bounds edge — no-op; I/O нет. |
| `wx_status_t wx_surface_bitmap_rgb(wx_surface_t *surface,size_t x,size_t y,size_t width,size_t height,const uint8_t *rgb,size_t length,size_t stride)` | Копирует RGB888 rows в host surface. `stride=0` = `width*3`; source не overlap, length покрывает padding; строгие bounds/overflow, I/O нет. |
| `wx_status_t wx_device_bitmap_region(wx_device_t *device,size_t x,size_t y,size_t width,size_t height)` | Отправляет ровно strict nonempty rect canvas. Без allocation/readback/implicit orientation/host mutation. Managed geometry confirmed; ошибка seal. FS35 regional CLI использует её после guarded preserve/confirm. |

### Managed FS35 init (`weact_init.h`, `weact_posix_init.h`)

Managed factory требует FS35, configured orientation 2, timeout ровно 2000 ms,
корректные sizes, connector и stride 0 либо не менее 960. `wx_device_init()`
acquire lease, проверяет guards, посылает orientation 02, query/readback и
black Fill; readback допускается только 02. `wx_device_init_preserve()`
acquire без protocol I/O; confirm обязателен до Bitmap/Fill. Confirm читает
orientation и принимает FS35 landscape 02 или 03 как 480x320, отвергает
00/01 и unknown, orientation не меняет. Он не знает физических pixels вне
областей, которые затем передаст приложение.

| Функция и сигнатура | Контракт, результат, эффект |
|---|---|
| `wx_status_t wx_posix_init_connector(const wx_posix_init_options_t *options,wx_init_connector_t *out)` | Строит guarded connector для configured legacy mapping `/dev/weact35`. Он принимает любой FS35, прошедший VID:PID, manufacturer, Product, CDC ACM, node и occupancy checks; USB serial не фиксируется. Binding без I/O; нужны valid output/options sizes/allocator. |
| `wx_status_t wx_posix_init_connector_select(const wx_posix_select_options_t *options,wx_init_connector_t *out)` | Строит portable guarded FS35 selection. `device` выбирает matching CDC ACM, `serial` сужает, NULL требует ровно один match. Acquisition проверяет VID:PID, Product, interface, occupancy. |
| `wx_status_t wx_device_init(const wx_init_options_t *options,wx_device_t **out,wx_init_report_t *report)` | Полный managed FS35 setup. Optional report имеет правильный `struct_size`; при ошибке читайте status/reason/errno, unsafe retry нет. Успех владеет lease. |
| `wx_status_t wx_device_init_preserve(const wx_init_options_t *options,wx_device_t **out,wx_init_report_t *report)` | Только managed acquisition; factory не делает setter/query/fill/read/write. Перед графикой — confirm. |
| `wx_status_t wx_device_confirm_orientation(wx_device_t *device,wx_init_report_t *report)` | Guarded orientation query/readback для managed preserve. Нужен live unsealed device; ошибка закрывает/seal lease. |
| `wx_status_t wx_device_get_init_info(const wx_device_t *device,wx_init_info_t *out)` | Копирует managed state; `out->struct_size` совпадает. Generic device сообщает created/unmanaged. |
| `wx_status_t wx_device_init_close(wx_device_t *device,wx_init_report_t *report)` | Один раз release managed lease без implicit flush/protocol. Idempotent close не повторяет failed release; report разделяет primary/cleanup errors. |

`allow_unknown_occupancy` — узкая POSIX-политика: допускает только foreign-
process permission failure после identity checks. Known busy, namespace/scanner
failure, wrong Product и failed exclusive lock она не допускает. Это guard
managed POSIX connector, не `wx_device_create()` и не `wx_posix_transport_open()`.

### Transports и packet encoders

| Функция и сигнатура | Контракт, результат, эффект |
|---|---|
| `wx_status_t wx_memory_transport_init(wx_memory_transport_t *memory,uint8_t *storage,size_t capacity)` | Очищает/configures caller-owned deterministic memory transport. NULL storage допустим только при 0 capacity; allocation/I/O нет. |
| `wx_transport_t wx_memory_transport_bind(wx_memory_transport_t *memory)` | Возвращает callbacks, заимствующие `memory`, либо zero transport для NULL. Для offline protocol testing. |
| `wx_status_t wx_posix_transport_open(const char *path,const wx_posix_options_t *options,wx_posix_transport_t **out)` | Открывает Linux tty raw 8N1 B115200. Низкоуровневый путь: identity guard/reset/flush нет, settings не восстанавливаются. Открывает hardware; проверяйте status и `errno`. |
| `wx_transport_t wx_posix_transport_bind(wx_posix_transport_t *transport)` | Возвращает callbacks, заимствующие live backend, либо zero для NULL; backend живёт до device. |
| `wx_status_t wx_posix_transport_close(wx_posix_transport_t *transport)` | Закрывает один раз без flush/drain/retry Linux `close(EINTR)`; invalidates backend даже при error. |
| `void wx_posix_transport_destroy(wx_posix_transport_t *transport)` | Закрывает при необходимости и free backend; borrowed callbacks далее нельзя. |
| `int wx_posix_transport_last_errno(const wx_posix_transport_t *transport)` | Stored OS error или `EINVAL` для NULL. |
| `wx_status_t wx_encode_bitmap(uint8_t *out,size_t capacity,uint32_t xs,uint32_t ys,uint32_t xe,uint32_t ye)` | Pure 10-byte Bitmap encoder; coordinates inclusive, ordered, не выше `UINT16_MAX`. |
| `wx_status_t wx_encode_bitmap_fastlz(uint8_t *out,size_t capacity,uint32_t xs,uint32_t ys,uint32_t xe,uint32_t ye)` | Та же validation/layout, FastLZ Bitmap command; pixels не compress. |
| `wx_status_t wx_encode_fill(uint8_t *out,size_t capacity,uint32_t xs,uint32_t ys,uint32_t xe,uint32_t ye,uint16_t color)` | Pure 12-byte inclusive Fill encoder. |
| `wx_status_t wx_encode_orientation(uint8_t *out,size_t capacity,uint8_t orientation)` | Pure 3-byte encoder; 0–3 или 5. |
| `wx_status_t wx_encode_brightness(uint8_t *out,size_t capacity,uint8_t brightness,uint16_t time_ms)` | Pure 5-byte encoder; capacity минимум 5, fade максимум 5000 ms. |
| `wx_status_t wx_fastlz_encode(const uint8_t *input,size_t length,uint8_t *output,size_t capacity,size_t *used)` | Bounded level-1 encoder nonoverlap FS35 landscape blocks 1–1920 bytes; allocation/intrinsics нет; `used` при успехе. |

### Дополнительные FS35-операции (`weact_fs35.h`)

| Функция и сигнатура | Контракт, результат, эффект |
|---|---|
| `wx_status_t wx_device_set_backlight_percent(wx_device_t *device,unsigned percent,uint16_t fade_ms)` | Только managed ready/preserved FS35. `percent` 0–100 преобразуется floor(percent×255/100), fade cap 5000 ms. Canvas не меняет; ошибка seal. |
| `wx_status_t wx_device_get_backlight(wx_device_t *device,uint8_t *value)` | Guarded managed FS35 query `83 raw 0A`; нужен live ready/preserved и output; ошибка seal. |
| `wx_status_t wx_device_bitmap_fastlz(wx_device_t *device,wx_fastlz_stats_t *stats)` | Compressed full 480x320 FS35 canvas. Нужен managed confirmed landscape; stats optional, но с верным `struct_size`. |
| `wx_status_t wx_device_bitmap_fastlz_rect(wx_device_t *device,size_t x,size_t y,size_t width,size_t height,wx_fastlz_stats_t *stats)` | Те же FastLZ rules для strict FS35 rect. Compression/allocation до TX; после failure no uncompressed fallback. |

## Дополнительные законченные примеры

Каждый пример этого раздела компилируется одной командой (подставьте имя
файла); для компиляции USB-дисплей не нужен:

```sh
cc -std=c11 -Wall -Wextra example.c $(pkg-config --cflags --libs weact) -o example
```

### Выбор guarded FS35 connector без открытия порта

Привязка connector не открывает tty и не посылает команд дисплею. Acquisition
начнётся только при передаче его в `wx_device_init*()`.

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

### Managed FS35 connection, init и close

Это hardware-пример: запускайте его только после осознанной проверки path,
Product, прав и владельца порта. Он задаёт orientation 02 и делает black
full-screen Fill; `wx_device_init_close()` освобождает lease. При ошибке
проверяйте `report.reason` и `report.primary_errno`.

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

Этот compatibility-пример открывает заданный path и является hardware action.
Он показывает legacy draw/flush sequence; новый FS35-код должен использовать
managed API.

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

### QR и Barcode

В 1.0.0 нет установленного public C API QR или линейных штрихкодов. Для
проверенных правил rendering/planning используйте `weact-cli`, либо рендерьте
image в приложении, копируйте его `wx_surface_bitmap_rgb()` и передавайте
выбранный rect через `wx_device_bitmap_region()`. Библиотека не предоставляет
функцию сохранения или чтения неизвестных pixels вне этого rect.

## Чего C API не предоставляет

QR/Code 128/EAN/Code 39 layout, quiet-zone planning, CLI syntax, text/image
parsing и `/init` — возможности приложения/CLI, а не установленные public C
entry points. `Include/weact_codes.h` и private headers, видимые в source
bundle, не устанавливаются пакетом `libweact-dev`; их encoder functions не
входят в 79 exports `libweact.so.1`. Не используйте их как public contract.
Чтобы C-приложение передало готовое regional image, запишите RGB в surface
managed confirmed FS35 и вызовите `wx_device_bitmap_region()`; сохранность
неизвестных LCD pixels вне передаваемого прямоугольника — ответственность
приложения.

## ABI inventory

Список получен из проверенного 1.0.0 DSO через
`nm -D --defined-only`; все `T/W` exports описаны выше. Исключение — видимый,
но private `wx_device_bitmap_rect_internal`: он объявлен только в
неустанавливаемом `weact_bitmap_internal.h`, служит внутренним измерениям и
не является поддерживаемым application API. Его точная private-сигнатура:
`wx_status_t wx_device_bitmap_rect_internal(wx_device_t *, size_t, size_t,
size_t, size_t, wx_bitmap_stats_t *)`; `wx_bitmap_stats_t` также private,
поэтому application не должно объявлять или вызывать эту функцию.

| Группа export | Символы | Установленный header / статус |
|---|---|---|
| FreeType (11) | `ft_text_available`, `cleanup`, `draw`, `draw_wrapped`, `get_default_font`, `get_height`, `get_width`, `init`, `set_alignment`, `set_color`, `set_size` | `text_freetype.h`; описаны выше |
| Legacy (23) | `weact_cleanup`, `clear_buffer`, `close`, `draw_circle`, `draw_line`, `draw_pixel`, `draw_rect`, `fill_screen`, `flush_buffer`, `get_brightness`, `get_display_height`, `get_display_width`, `get_info`, `get_last_error`, `get_orientation`, `init`, `is_connected`, `rgb_to_brg565`, `set_brightness`, `set_orientation`, `swap_buffers`, `system_reset`, `update_display` | `weact_display.h`; описаны выше |
| Core surface/device (16) | `wx_surface_create`, `destroy`, `info`, `pixels`, `set_pixel`, `set_rgb`, `clear`, `wx_pack_rgb`, `wx_device_create`, `destroy`, `surface`, `set_orientation`, `set_brightness`, `fill`, `bitmap`, `stream_failed` | `weact_extended.h`; описаны выше |
| Graphics (4) | `wx_device_profile`, `wx_surface_fill_rgb`, `wx_surface_bitmap_rgb`, `wx_device_bitmap_region` | `weact_graphics.h`; описаны выше |
| Managed init (5) | `wx_device_init`, `init_preserve`, `confirm_orientation`, `get_init_info`, `init_close` | `weact_init.h`; описаны выше |
| POSIX/memory (9) | `wx_posix_init_connector`, `connector_select`, `wx_posix_transport_open`, `bind`, `close`, `destroy`, `last_errno`, `wx_memory_transport_init`, `bind` | соответствующие installed headers; описаны выше |
| Protocol/FastLZ (6) | `wx_encode_bitmap`, `bitmap_fastlz`, `fill`, `orientation`, `brightness`, `wx_fastlz_encode` | `weact_protocol.h`, `weact_fastlz.h`; описаны выше |
| FS35 extension (4) | `wx_device_bitmap_fastlz`, `bitmap_fastlz_rect`, `get_backlight`, `set_backlight_percent` | `weact_fs35.h`; описаны выше |
| Private technical (1) | `wx_device_bitmap_rect_internal` | non-installed header; не поддерживается |

Итого: 11 + 23 + 16 + 4 + 5 + 9 + 6 + 4 + 1 = **79**.

### Построчная сверка экспортов

<a id="ft_text_available"></a><a id="ft_text_cleanup"></a><a id="ft_text_draw"></a><a id="ft_text_draw_wrapped"></a><a id="ft_text_get_default_font"></a><a id="ft_text_get_height"></a><a id="ft_text_get_width"></a><a id="ft_text_init"></a><a id="ft_text_set_alignment"></a><a id="ft_text_set_color"></a><a id="ft_text_set_size"></a><a id="weact_cleanup"></a><a id="weact_clear_buffer"></a><a id="weact_close"></a><a id="weact_draw_circle"></a><a id="weact_draw_line"></a><a id="weact_draw_pixel"></a><a id="weact_draw_rect"></a><a id="weact_fill_screen"></a><a id="weact_flush_buffer"></a><a id="weact_get_brightness"></a><a id="weact_get_display_height"></a><a id="weact_get_display_width"></a><a id="weact_get_info"></a><a id="weact_get_last_error"></a><a id="weact_get_orientation"></a><a id="weact_init"></a><a id="weact_is_connected"></a><a id="weact_rgb_to_brg565"></a><a id="weact_set_brightness"></a><a id="weact_set_orientation"></a><a id="weact_swap_buffers"></a><a id="weact_system_reset"></a><a id="weact_update_display"></a><a id="wx_device_bitmap"></a><a id="wx_device_bitmap_fastlz"></a><a id="wx_device_bitmap_fastlz_rect"></a><a id="wx_device_bitmap_rect_internal"></a><a id="wx_device_bitmap_region"></a><a id="wx_device_confirm_orientation"></a><a id="wx_device_create"></a><a id="wx_device_destroy"></a><a id="wx_device_fill"></a><a id="wx_device_get_backlight"></a><a id="wx_device_get_init_info"></a><a id="wx_device_init"></a><a id="wx_device_init_close"></a><a id="wx_device_init_preserve"></a><a id="wx_device_profile"></a><a id="wx_device_set_backlight_percent"></a><a id="wx_device_set_brightness"></a><a id="wx_device_set_orientation"></a><a id="wx_device_stream_failed"></a><a id="wx_device_surface"></a><a id="wx_encode_bitmap"></a><a id="wx_encode_bitmap_fastlz"></a><a id="wx_encode_brightness"></a><a id="wx_encode_fill"></a><a id="wx_encode_orientation"></a><a id="wx_fastlz_encode"></a><a id="wx_memory_transport_bind"></a><a id="wx_memory_transport_init"></a><a id="wx_pack_rgb"></a><a id="wx_posix_init_connector"></a><a id="wx_posix_init_connector_select"></a><a id="wx_posix_transport_bind"></a><a id="wx_posix_transport_close"></a><a id="wx_posix_transport_destroy"></a><a id="wx_posix_transport_last_errno"></a><a id="wx_posix_transport_open"></a><a id="wx_surface_bitmap_rgb"></a><a id="wx_surface_clear"></a><a id="wx_surface_create"></a><a id="wx_surface_destroy"></a><a id="wx_surface_fill_rgb"></a><a id="wx_surface_info"></a><a id="wx_surface_pixels"></a><a id="wx_surface_set_pixel"></a><a id="wx_surface_set_rgb"></a>

| Символ | Header | Раздел справочника | Статус документации |
|---|---|---|---|
| `ft_text_available` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_cleanup` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_draw` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_draw_wrapped` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_get_default_font` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_get_height` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_get_width` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_init` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_set_alignment` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_set_color` | `text_freetype.h` | FreeType | Описан выше |
| `ft_text_set_size` | `text_freetype.h` | FreeType | Описан выше |
| `weact_cleanup` | `weact_display.h` | Legacy | Описан выше |
| `weact_clear_buffer` | `weact_display.h` | Legacy | Описан выше |
| `weact_close` | `weact_display.h` | Legacy | Описан выше |
| `weact_draw_circle` | `weact_display.h` | Legacy | Описан выше |
| `weact_draw_line` | `weact_display.h` | Legacy | Описан выше |
| `weact_draw_pixel` | `weact_display.h` | Legacy | Описан выше |
| `weact_draw_rect` | `weact_display.h` | Legacy | Описан выше |
| `weact_fill_screen` | `weact_display.h` | Legacy | Описан выше |
| `weact_flush_buffer` | `weact_display.h` | Legacy | Описан выше |
| `weact_get_brightness` | `weact_display.h` | Legacy | Описан выше |
| `weact_get_display_height` | `weact_display.h` | Legacy | Описан выше |
| `weact_get_display_width` | `weact_display.h` | Legacy | Описан выше |
| `weact_get_info` | `weact_display.h` | Legacy | Описан выше |
| `weact_get_last_error` | `weact_display.h` | Legacy | Описан выше |
| `weact_get_orientation` | `weact_display.h` | Legacy | Описан выше |
| `weact_init` | `weact_display.h` | Legacy | Описан выше |
| `weact_is_connected` | `weact_display.h` | Legacy | Описан выше |
| `weact_rgb_to_brg565` | `weact_display.h` | Legacy | Описан выше |
| `weact_set_brightness` | `weact_display.h` | Legacy | Описан выше |
| `weact_set_orientation` | `weact_display.h` | Legacy | Описан выше |
| `weact_swap_buffers` | `weact_display.h` | Legacy | Описан выше |
| `weact_system_reset` | `weact_display.h` | Legacy | Описан выше |
| `weact_update_display` | `weact_display.h` | Legacy | Описан выше |
| `wx_device_bitmap` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_device_bitmap_fastlz` | `weact_fs35.h` | Расширение FS35 | Описан выше |
| `wx_device_bitmap_fastlz_rect` | `weact_fs35.h` | Расширение FS35 | Описан выше |
| `wx_device_bitmap_rect_internal` | not installed (`weact_bitmap_internal.h`) | Private technical | Private; пояснён выше |
| `wx_device_bitmap_region` | `weact_graphics.h` | Graphics | Описан выше |
| `wx_device_confirm_orientation` | `weact_init.h` | Managed init | Описан выше |
| `wx_device_create` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_device_destroy` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_device_fill` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_device_get_backlight` | `weact_fs35.h` | Расширение FS35 | Описан выше |
| `wx_device_get_init_info` | `weact_init.h` | Managed init | Описан выше |
| `wx_device_init` | `weact_init.h` | Managed init | Описан выше |
| `wx_device_init_close` | `weact_init.h` | Managed init | Описан выше |
| `wx_device_init_preserve` | `weact_init.h` | Managed init | Описан выше |
| `wx_device_profile` | `weact_graphics.h` | Graphics | Описан выше |
| `wx_device_set_backlight_percent` | `weact_fs35.h` | Расширение FS35 | Описан выше |
| `wx_device_set_brightness` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_device_set_orientation` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_device_stream_failed` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_device_surface` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_encode_bitmap` | `weact_protocol.h` | Protocol/FastLZ | Описан выше |
| `wx_encode_bitmap_fastlz` | `weact_protocol.h` | Protocol/FastLZ | Описан выше |
| `wx_encode_brightness` | `weact_protocol.h` | Protocol/FastLZ | Описан выше |
| `wx_encode_fill` | `weact_protocol.h` | Protocol/FastLZ | Описан выше |
| `wx_encode_orientation` | `weact_protocol.h` | Protocol/FastLZ | Описан выше |
| `wx_fastlz_encode` | `weact_fastlz.h` | Protocol/FastLZ | Описан выше |
| `wx_memory_transport_bind` | `weact_memory_transport.h` | POSIX/memory | Описан выше |
| `wx_memory_transport_init` | `weact_memory_transport.h` | POSIX/memory | Описан выше |
| `wx_pack_rgb` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_posix_init_connector` | `weact_posix_init.h` | POSIX/memory | Описан выше |
| `wx_posix_init_connector_select` | `weact_posix_init.h` | POSIX/memory | Описан выше |
| `wx_posix_transport_bind` | `weact_posix_transport.h` | POSIX/memory | Описан выше |
| `wx_posix_transport_close` | `weact_posix_transport.h` | POSIX/memory | Описан выше |
| `wx_posix_transport_destroy` | `weact_posix_transport.h` | POSIX/memory | Описан выше |
| `wx_posix_transport_last_errno` | `weact_posix_transport.h` | POSIX/memory | Описан выше |
| `wx_posix_transport_open` | `weact_posix_transport.h` | POSIX/memory | Описан выше |
| `wx_surface_bitmap_rgb` | `weact_graphics.h` | Graphics | Описан выше |
| `wx_surface_clear` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_surface_create` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_surface_destroy` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_surface_fill_rgb` | `weact_graphics.h` | Graphics | Описан выше |
| `wx_surface_info` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_surface_pixels` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_surface_set_pixel` | `weact_extended.h` | Core surface/device | Описан выше |
| `wx_surface_set_rgb` | `weact_extended.h` | Core surface/device | Описан выше |

## Безопасность оборудования

Открытие tty может изменить modem-line state, хотя библиотека не посылает
дополнительный reset. Используйте стабильный `/dev/serial/by-id/...`,
проверьте Product (`Display FS V1` для FS35, `Display FS 0.96 Inch` для
FS096), права и отсутствие другого владельца, затем применяйте guarded
managed FS35 init. У обоих VID:PID `1a86:fe0c`; одного VID:PID недостаточно.

Full managed init намеренно задаёт FS35 orientation 02 и делает black
full-screen Fill. Preserve+confirm setter/Fill не делают, но не читают
неизвестные LCD pixels; этот путь годится, когда приложение точно знает
передаваемый rect. Успешный USB write означает принятую передачу на host,
не побайтное наблюдение LCD. Disconnect, timeout, partial transfer и guard
failure прекращают операцию: автоматически reconnect/replay packet нельзя.

Hardware end-to-end примеры документированы для CLI и требуют осознанной
проверки устройства. Примеры C в этом справочнике компилируются без дисплея и
не запускаются как hardware tests.
