/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * Minimal declarations reviewed against libqrencode v4.1.1 qrencode.h:
 * https://github.com/fukuchi/libqrencode/blob/v4.1.1/qrencode.h
 * Retained upstream notice for these declarations:
 * Copyright (C) 2006-2017 Kentaro Fukuchi <kentaro@fukuchi.org>
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or any later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * See /usr/share/common-licenses/LGPL-2.1 on Debian.
 */
#ifndef WEACT_QRENCODE_ABI_H
#define WEACT_QRENCODE_ABI_H
typedef enum { QR_MODE_NUL=-1, QR_MODE_NUM, QR_MODE_AN, QR_MODE_8,
    QR_MODE_KANJI, QR_MODE_STRUCTURE, QR_MODE_ECI, QR_MODE_FNC1FIRST,
    QR_MODE_FNC1SECOND } QRencodeMode;
typedef enum { QR_ECLEVEL_L, QR_ECLEVEL_M, QR_ECLEVEL_Q, QR_ECLEVEL_H } QRecLevel;
typedef struct _QRinput QRinput;
typedef struct { int version, width; unsigned char *data; } QRcode;
extern QRinput *QRinput_new2(int version, QRecLevel level);
extern int QRinput_appendECIheader(QRinput *input, unsigned int ecinum);
extern int QRinput_append(QRinput *, QRencodeMode, int, const unsigned char *);
extern void QRinput_free(QRinput *);
extern QRcode *QRcode_encodeInput(QRinput *);
extern void QRcode_free(QRcode *);
extern void QRcode_APIVersion(int *, int *, int *);
#endif
