/*
 * Copyright (C) 2017 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

/**
 * @file iconv.h
 * @brief Character encoding conversion.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

/* If we just use void* in the typedef, the compiler exposes that in error messages. */
struct __iconv_t;

/**
 * The `iconv_t` type that represents an instance of a converter.
 */
typedef struct __iconv_t* iconv_t;

/**
 * [iconv_open(3)](http://man7.org/linux/man-pages/man3/iconv_open.3.html) allocates a new converter
 * from `__src_encoding` to `__dst_encoding`.
 *
 * Returns a new `iconv_t` on success and returns `((iconv_t) -1)` and sets `errno` on failure.
 *
 * Available since API level 28.
 */

#if __ANDROID_API__ >= 28
iconv_t iconv_open(const char* __src_encoding, const char* __dst_encoding) __INTRODUCED_IN(28);

/**
 * [iconv(3)](http://man7.org/linux/man-pages/man3/iconv.3.html) converts characters from one
 * encoding to another.
 *
 * Android supports the `utf8`, `ascii`, `usascii`, `utf16be`, `utf16le`, `utf32be`, `utf32le`,
 * and `wchart` encodings. Android also supports the GNU `//IGNORE` and `//TRANSLIT` extensions.
 *
 * Returns the number of characters converted on success and returns `((size_t) -1)` and
 * sets `errno` on failure.
 *
 * Available since API level 28.
 */
size_t iconv(iconv_t __converter, char** __src_buf, size_t* __src_bytes_left, char** __dst_buf, size_t* __dst_bytes_left) __INTRODUCED_IN(28);

/**
 * [iconv_close(3)](http://man7.org/linux/man-pages/man3/iconv_close.3.html) deallocates a converter
 * returned by iconv_open().
 *
 * Returns 0 on success and returns -1 and sets `errno` on failure.
 *
 * Available since API level 28.
 */
int iconv_close(iconv_t __converter) __INTRODUCED_IN(28);
#endif /* __ANDROID_API__ >= 28 */


__END_DECLS
