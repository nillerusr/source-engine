/* mz_os_win32.h -- System functions for Windows
   Version 2.3.3, June 10, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#ifndef MZ_OS_WIN32_H
#define MZ_OS_WIN32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

#define MZ_VERSION_MADEBY_HOST_SYSTEM   (MZ_HOST_SYSTEM_WINDOWS_NTFS)

/***************************************************************************/

struct dirent {
    char d_name[256];
};
typedef void* DIR;

/***************************************************************************/

int32_t mz_win32_rand(uint8_t *buf, int32_t size);
int32_t mz_win32_file_exists(const char *path);
int64_t mz_win32_get_file_size(const char *path);
int32_t mz_win32_get_file_date(const char *path, time_t *modified_date, time_t *accessed_date, time_t *creation_date);
int32_t mz_win32_set_file_date(const char *path, time_t modified_date, time_t accessed_date, time_t creation_date);
int32_t mz_win32_get_file_attribs(const char *path, uint32_t *attributes);
int32_t mz_win32_set_file_attribs(const char *path, uint32_t attributes);
int32_t mz_win32_make_dir(const char *path);
DIR*    mz_win32_open_dir(const char *path);
struct
dirent* mz_win32_read_dir(DIR *dir);
int32_t mz_win32_close_dir(DIR *dir);
int32_t mz_win32_is_dir(const char *path);

/***************************************************************************/

#define mz_os_rand              mz_win32_rand
#define mz_os_file_exists       mz_win32_file_exists
#define mz_os_get_file_size     mz_win32_get_file_size
#define mz_os_get_file_date     mz_win32_get_file_date
#define mz_os_set_file_date     mz_win32_set_file_date
#define mz_os_get_file_attribs  mz_win32_get_file_attribs
#define mz_os_set_file_attribs  mz_win32_set_file_attribs
#define mz_os_make_dir          mz_win32_make_dir
#define mz_os_open_dir          mz_win32_open_dir
#define mz_os_read_dir          mz_win32_read_dir
#define mz_os_close_dir         mz_win32_close_dir
#define mz_os_is_dir            mz_win32_is_dir

/***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
