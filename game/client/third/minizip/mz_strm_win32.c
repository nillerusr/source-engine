/* mz_strm_win32.c -- Stream for filesystem access for windows
   Version 2.3.3, June 10, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip
   Copyright (C) 2009-2010 Mathias Svensson
     Modifications for Zip64 support
     http://result42.com
   Copyright (C) 1998-2010 Gilles Vollant
     http://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include <stdlib.h>

#include <windows.h>

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_win32.h"

/***************************************************************************/

#ifndef INVALID_HANDLE_VALUE
#  define INVALID_HANDLE_VALUE (0xFFFFFFFF)
#endif

#ifndef INVALID_SET_FILE_POINTER
#  define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

#if defined(WINAPI_FAMILY_ONE_PARTITION) && !defined(MZ_USE_WINRT_API)
#  if WINAPI_FAMILY_ONE_PARTITION(WINAPI_FAMILY, WINAPI_PARTITION_APP)
#    define MZ_USE_WINRT_API 1
#  endif
#endif

/***************************************************************************/

static mz_stream_vtbl mz_stream_win32_vtbl = {
    mz_stream_win32_open,
    mz_stream_win32_is_open,
    mz_stream_win32_read,
    mz_stream_win32_write,
    mz_stream_win32_tell,
    mz_stream_win32_seek,
    mz_stream_win32_close,
    mz_stream_win32_error,
    mz_stream_win32_create,
    mz_stream_win32_delete,
    NULL,
    NULL
};

/***************************************************************************/

typedef struct mz_stream_win32_s
{
    mz_stream       stream;
    HANDLE          handle;
    int32_t         error;
} mz_stream_win32;

/***************************************************************************/

int32_t mz_stream_win32_open(void *stream, const char *path, int32_t mode)
{
    mz_stream_win32 *win32 = (mz_stream_win32 *)stream;
    uint32_t desired_access = 0;
    uint32_t creation_disposition = 0;
    uint32_t share_mode = FILE_SHARE_READ;
    uint32_t flags_attribs = FILE_ATTRIBUTE_NORMAL;
    wchar_t *path_wide = NULL;
    uint32_t path_wide_size = 0;


    if (path == NULL)
        return MZ_STREAM_ERROR;

    if ((mode & MZ_OPEN_MODE_READWRITE) == MZ_OPEN_MODE_READ)
    {
        desired_access = GENERIC_READ;
        creation_disposition = OPEN_EXISTING;
    }
    else if (mode & MZ_OPEN_MODE_APPEND)
    {
        desired_access = GENERIC_WRITE | GENERIC_READ;
        creation_disposition = OPEN_EXISTING;
    }
    else if (mode & MZ_OPEN_MODE_CREATE)
    {
        desired_access = GENERIC_WRITE | GENERIC_READ;
        creation_disposition = CREATE_ALWAYS;
    }
    else
    {
        return MZ_STREAM_ERROR;
    }

    path_wide_size = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    path_wide = (wchar_t *)MZ_ALLOC((path_wide_size + 1) * sizeof(wchar_t));
    memset(path_wide, 0, sizeof(wchar_t) * (path_wide_size + 1));

    MultiByteToWideChar(CP_UTF8, 0, path, -1, path_wide, path_wide_size);

#ifdef MZ_USE_WINRT_API
    win32->handle = CreateFile2W(path_wide, desired_access, share_mode, creation_disposition, NULL);
#else
    win32->handle = CreateFileW(path_wide, desired_access, share_mode, NULL, creation_disposition, flags_attribs, NULL);
#endif

    MZ_FREE(path_wide);

    if (mz_stream_win32_is_open(stream) != MZ_OK)
    {
        win32->error = GetLastError();
        return MZ_STREAM_ERROR;
    }

    if (mode & MZ_OPEN_MODE_APPEND)
        return mz_stream_win32_seek(stream, 0, MZ_SEEK_END);

    return MZ_OK;
}

int32_t mz_stream_win32_is_open(void *stream)
{
    mz_stream_win32 *win32 = (mz_stream_win32 *)stream;
    if (win32->handle == NULL || win32->handle == INVALID_HANDLE_VALUE)
        return MZ_STREAM_ERROR;
    return MZ_OK;
}

int32_t mz_stream_win32_read(void *stream, void *buf, int32_t size)
{
    mz_stream_win32 *win32 = (mz_stream_win32 *)stream;
    uint32_t read = 0;

    if (mz_stream_win32_is_open(stream) != MZ_OK)
        return MZ_STREAM_ERROR;

    if (!ReadFile(win32->handle, buf, size, (DWORD *)&read, NULL))
    {
        win32->error = GetLastError();
        if (win32->error == ERROR_HANDLE_EOF)
            win32->error = 0;
    }

    return read;
}

int32_t mz_stream_win32_write(void *stream, const void *buf, int32_t size)
{
    mz_stream_win32 *win32 = (mz_stream_win32 *)stream;
    uint32_t written = 0;

    if (mz_stream_win32_is_open(stream) != MZ_OK)
        return MZ_STREAM_ERROR;

    if (!WriteFile(win32->handle, buf, size, (DWORD *)&written, NULL))
    {
        win32->error = GetLastError();
        if (win32->error == ERROR_HANDLE_EOF)
            win32->error = 0;
    }

    return written;
}

static int32_t mz_stream_win32_seekinternal(HANDLE handle, LARGE_INTEGER large_pos, LARGE_INTEGER *new_pos, uint32_t move_method)
{
#ifdef MZ_USE_WINRT_API
    return SetFilePointerEx(handle, pos, newPos, dwMoveMethod);
#else
    LONG high_part = large_pos.HighPart;
    uint32_t pos = SetFilePointer(handle, large_pos.LowPart, &high_part, move_method);

    if ((pos == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
        return MZ_STREAM_ERROR;

    if (new_pos != NULL)
    {
        new_pos->LowPart = pos;
        new_pos->HighPart = high_part;
    }

    return MZ_OK;
#endif
}

int64_t mz_stream_win32_tell(void *stream)
{
    mz_stream_win32 *win32 = (mz_stream_win32 *)stream;
    LARGE_INTEGER large_pos;

    if (mz_stream_win32_is_open(stream) != MZ_OK)
        return MZ_STREAM_ERROR;

    large_pos.QuadPart = 0;

    if (mz_stream_win32_seekinternal(win32->handle, large_pos, &large_pos, FILE_CURRENT) != MZ_OK)
        win32->error = GetLastError();

    return large_pos.QuadPart;
}

int32_t mz_stream_win32_seek(void *stream, int64_t offset, int32_t origin)
{
    mz_stream_win32 *win32 = (mz_stream_win32 *)stream;
    uint32_t move_method = 0xFFFFFFFF;
    LARGE_INTEGER large_pos;


    if (mz_stream_win32_is_open(stream) == MZ_STREAM_ERROR)
        return MZ_STREAM_ERROR;

    switch (origin)
    {
        case MZ_SEEK_CUR:
            move_method = FILE_CURRENT;
            break;
        case MZ_SEEK_END:
            move_method = FILE_END;
            break;
        case MZ_SEEK_SET:
            move_method = FILE_BEGIN;
            break;
        default:
            return MZ_STREAM_ERROR;
    }

    large_pos.QuadPart = offset;

    if (mz_stream_win32_seekinternal(win32->handle, large_pos, NULL, move_method) != MZ_OK)
    {
        win32->error = GetLastError();
        return MZ_STREAM_ERROR;
    }

    return MZ_OK;
}

int mz_stream_win32_close(void *stream)
{
    mz_stream_win32 *win32 = (mz_stream_win32 *)stream;

    if (win32->handle != NULL)
        CloseHandle(win32->handle);
    win32->handle = NULL;
    return MZ_OK;
}

int mz_stream_win32_error(void *stream)
{
    mz_stream_win32 *win32 = (mz_stream_win32 *)stream;
    return win32->error;
}

void *mz_stream_win32_create(void **stream)
{
    mz_stream_win32 *win32 = NULL;

    win32 = (mz_stream_win32 *)MZ_ALLOC(sizeof(mz_stream_win32));
    if (win32 != NULL)
    {
        memset(win32, 0, sizeof(mz_stream_win32));
        win32->stream.vtbl = &mz_stream_win32_vtbl;
    }
    if (stream != NULL)
        *stream = win32;

    return win32;
}

void mz_stream_win32_delete(void **stream)
{
    mz_stream_win32 *win32 = NULL;
    if (stream == NULL)
        return;
    win32 = (mz_stream_win32 *)*stream;
    if (win32 != NULL)
        MZ_FREE(win32);
    *stream = NULL;
}

void *mz_stream_win32_get_interface(void)
{
    return (void *)&mz_stream_win32_vtbl;
}
