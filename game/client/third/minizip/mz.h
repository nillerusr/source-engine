/* mz.h -- Errors codes, zip flags and magic
   Version 2.3.3, June 10, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_H
#define MZ_H

#ifdef __cplusplus
extern "C" {
#endif

//mz_zip.c:1215:10: error: ZLIB or LZMA required for CRC32
#define HAVE_ZLIB true

/***************************************************************************/

// MZ_VERSION
#define MZ_VERSION                      ("2.3.3")

// MZ_ERROR
#define MZ_OK                           (0)
#define MZ_STREAM_ERROR                 (-1)
#define MZ_DATA_ERROR                   (-3)
#define MZ_MEM_ERROR                    (-4)
#define MZ_END_OF_LIST                  (-100)
#define MZ_END_OF_STREAM                (-101)
#define MZ_PARAM_ERROR                  (-102)
#define MZ_FORMAT_ERROR                 (-103)
#define MZ_INTERNAL_ERROR               (-104)
#define MZ_CRC_ERROR                    (-105)
#define MZ_CRYPT_ERROR                  (-106)
#define MZ_EXIST_ERROR                  (-107)
#define MZ_PASSWORD_ERROR               (-108)

// MZ_OPEN
#define MZ_OPEN_MODE_READ               (0x01)
#define MZ_OPEN_MODE_WRITE              (0x02)
#define MZ_OPEN_MODE_READWRITE          (MZ_OPEN_MODE_READ | MZ_OPEN_MODE_WRITE)
#define MZ_OPEN_MODE_APPEND             (0x04)
#define MZ_OPEN_MODE_CREATE             (0x08)
#define MZ_OPEN_MODE_EXISTING           (0x10)

// MZ_SEEK
#define MZ_SEEK_SET                     (0)
#define MZ_SEEK_CUR                     (1)
#define MZ_SEEK_END                     (2)

// MZ_COMPRESS
#define MZ_COMPRESS_METHOD_RAW          (0)
#define MZ_COMPRESS_METHOD_DEFLATE      (8)
#define MZ_COMPRESS_METHOD_BZIP2        (12)
#define MZ_COMPRESS_METHOD_LZMA         (14)
#define MZ_COMPRESS_METHOD_AES          (99)

#define MZ_COMPRESS_LEVEL_DEFAULT       (-1)
#define MZ_COMPRESS_LEVEL_FAST          (2)
#define MZ_COMPRESS_LEVEL_NORMAL        (6)
#define MZ_COMPRESS_LEVEL_BEST          (9)

// MZ_ZIP
#define MZ_ZIP_FLAG_ENCRYPTED           (1 << 0)
#define MZ_ZIP_FLAG_LZMA_EOS_MARKER     (1 << 1)
#define MZ_ZIP_FLAG_DEFLATE_MAX         (1 << 1)
#define MZ_ZIP_FLAG_DEFLATE_NORMAL      (0)
#define MZ_ZIP_FLAG_DEFLATE_FAST        (1 << 2)
#define MZ_ZIP_FLAG_DEFLATE_SUPER_FAST  (MZ_ZIP_FLAG_DEFLATE_FAST | \
                                         MZ_ZIP_FLAG_DEFLATE_MAX)
#define MZ_ZIP_FLAG_DATA_DESCRIPTOR     (1 << 3)

// MZ_ZIP64
#define MZ_ZIP64_AUTO                   (0)
#define MZ_ZIP64_FORCE                  (1)
#define MZ_ZIP64_DISABLE                (2)

// MZ_HOST_SYSTEM
#define MZ_HOST_SYSTEM_MSDOS            (0)
#define MZ_HOST_SYSTEM_UNIX             (3)
#define MZ_HOST_SYSTEM_WINDOWS_NTFS     (10)
#define MZ_HOST_SYSTEM_OSX_DARWIN       (19)

// MZ_AES
#define MZ_AES_VERSION                  (1)
#define MZ_AES_ENCRYPTION_MODE_128      (0x01)
#define MZ_AES_ENCRYPTION_MODE_192      (0x02)
#define MZ_AES_ENCRYPTION_MODE_256      (0x03)

// MZ_UTILITY
#define MZ_UNUSED(SYMBOL)               ((void)SYMBOL)

#ifndef MZ_CUSTOM_ALLOC
#define MZ_ALLOC(SIZE)                  (malloc(SIZE))
#endif
#ifndef MZ_CUSTOM_FREE
#define MZ_FREE(PTR)                    (free(PTR))
#endif

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
