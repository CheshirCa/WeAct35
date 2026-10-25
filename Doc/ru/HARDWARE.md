# Оборудование и безопасная работа

[English](../en/HARDWARE.md) | [Русский](HARDWARE.md) | [Установка](INSTALL.md) | [CLI](CLI.md)

## Поддерживаемые дисплеи

| Свойство | FS35 | FS096 |
|---|---:|---:|
| Product | `Display FS V1` | `Display FS 0.96 Inch` |
| USB VID:PID | `1a86:fe0c` | `1a86:fe0c` |
| Landscape | 480×320 | 160×80 |
| Формат библиотеки | RGB565 little-endian | legacy BRG565/MSB |
| Регион QR/Barcode | Да | Нет |
| `weactterm` | Да | Нет |

Упаковка цвета различается: FS35 RGB565 LE и FS096 BRG565/MSB нельзя
подменять друг другом. Разрешите библиотеке выбрать модель.

## Идентификация и порты

Сначала используйте `weact-list --verbose`. Он читает sysfs и показывает
распознанные FS35/FS096 и неизвестные устройства с тем же VID:PID.
`weact-cli` принимает `/p:N` для `/dev/ttyACMN` или
`/p:/dev/serial/by-id/...`; предпочтителен by-id.

До записи подтвердите Product, соответствие пути CDC ACM, права пользователя
и отсутствие видимого владельца. Guard проверяет USB/CDC identity, эксклюзив
и occupancy. `--allow-unknown-occupancy` допустим только для уже
подтверждённого устройства без видимого владельца при неокончательном
результате scan; известную занятость или identity failure он не отключает.

## FS35: запуск и регионы

После отключения USB FS35 может показывать циклическую встроенную заставку с
меняющейся ориентацией. Безопасная последовательность:

```sh
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 /init
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/qr:https://example.com' /x:40 /y:40 /w:160 /h:160
weact-cli /p:/dev/serial/by-id/usb-WeAct_Studio_Display_FS_V1_...-if00 '/barcode:code128:ABC123' /x:220 /y:80 /w:240 /h:100
```

`/init` — самостоятельная операция: устанавливает orientation 02, проверяет
ответ и очищает весь FS35 в чёрный. Она не меняет яркость, не делает Reset или
Free. Региональный preserve path принимает landscape readback 02 либо 03,
требует 480×320 и передаёт только заданный прямоугольник. Не вызывайте `/init`
между регионами.

Фон области белый, код чёрный, quiet zone и integer scaling остаются внутри.
Неизвестное содержимое LCD не читается. Offline-проверки проверяют buffers и
packets; аппаратное подтверждение визуальное, не побайтный захват LCD.

## FS096

Коды FS096 выводятся fullscreen 160×80. Проверенного readback ориентации для
preserve-регионов нет, поэтому региональный режим отклоняется до открытия
порта. Код сохраняет ориентацию и требует уже landscape FS096. Обычный
text/fullscreen путь использует legacy FS096 setup. При no-fit CLI сообщает
ошибку вместо обрезания; EAN использует один пиксель на модуль.

## Передача и отказ

Кадры передаются по serial и не являются мгновенным изменением LCD. Успех
host TX сам по себе не доказывает визуальный результат. Ошибки identity,
geometry, encoder, orientation confirmation и transport fail-stop: небезопасный
fallback и повтор частичного вывода не выполняются.

FastLZ — явная опция только для scrolling text FS35, не общий переключатель
сжатия. Scroll statistics описывает host-side работу и передачу, а не
гарантированную частоту обновления LCD.

Аппаратно подтверждены FS35 `/init`, независимые региональные Code 128/QR и
FS096 fullscreen Code 128/QR. Внешнее сканирование, непроверенные firmware
features, характеристика цвета и физическая попиксельная гарантия не заявляются.
