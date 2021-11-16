/* mz_strm_bzip.c -- Stream for bzip inflate/deflate
   Version 2.3.3, June 10, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
      https://github.com/nmoinvaz/minizip

   This program is distributed under the terms of the same license as bzip.
   See the accompanying LICENSE file for the full text of the license.
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "bzlib.h"

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_bzip.h"

/***************************************************************************/

static mz_stream_vtbl mz_stream_bzip_vtbl = {
    mz_stream_bzip_open,
    mz_stream_bzip_is_open,
    mz_stream_bzip_read,
    mz_stream_bzip_write,
    mz_stream_bzip_tell,
    mz_stream_bzip_seek,
    mz_stream_bzip_close,
    mz_stream_bzip_error,
    mz_stream_bzip_create,
    mz_stream_bzip_delete,
    mz_stream_bzip_get_prop_int64,
    mz_stream_bzip_set_prop_int64
};

/***************************************************************************/

typedef struct mz_stream_bzip_s {
    mz_stream   stream;
    bz_stream   bzstream;
    int32_t     mode;
    int32_t     error;
    uint8_t     buffer[INT16_MAX];
    int32_t     buffer_len;
    int16_t     stream_end;
    int64_t     total_in;
    int64_t     total_out;
    int64_t     max_total_in;
    int8_t      initialized;
    int16_t     level;
} mz_stream_bzip;

/***************************************************************************/

int32_t mz_stream_bzip_open(void *stream, const char *path, int32_t mode)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;

    MZ_UNUSED(path);

    bzip->bzstream.bzalloc = 0;
    bzip->bzstream.bzfree = 0;
    bzip->bzstream.opaque = 0;
    bzip->bzstream.total_in_lo32 = 0;
    bzip->bzstream.total_in_hi32 = 0;
    bzip->bzstream.total_out_lo32 = 0;
    bzip->bzstream.total_out_hi32 = 0;

    bzip->total_in = 0;
    bzip->total_out = 0;

    if (mode & MZ_OPEN_MODE_WRITE)
    {
        bzip->bzstream.next_out = (char *)bzip->buffer;
        bzip->bzstream.avail_out = sizeof(bzip->buffer);

        bzip->error = BZ2_bzCompressInit(&bzip->bzstream, bzip->level, 0, 0);
    }
    else if (mode & MZ_OPEN_MODE_READ)
    {
        bzip->bzstream.next_in = (char *)bzip->buffer;
        bzip->bzstream.avail_in = 0;

        bzip->error = BZ2_bzDecompressInit(&bzip->bzstream, 0, 0);
    }

    if (bzip->error != BZ_OK)
        return MZ_STREAM_ERROR;

    bzip->initialized = 1;
    bzip->stream_end = 0;
    bzip->mode = mode;
    return MZ_OK;
}

int32_t mz_stream_bzip_is_open(void *stream)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;
    if (bzip->initialized != 1)
        return MZ_STREAM_ERROR;
    return MZ_OK;
}

int32_t mz_stream_bzip_read(void *stream, void *buf, int32_t size)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;
    uint64_t total_in_before = 0;
    uint64_t total_out_before = 0;
    uint64_t total_in_after = 0;
    uint64_t total_out_after = 0;
    uint32_t total_in = 0;
    uint32_t total_out = 0;
    uint32_t in_bytes = 0;
    uint32_t out_bytes = 0;
    int32_t bytes_to_read = 0;
    int32_t read = 0;
    int32_t err = BZ_OK;


    if (bzip->stream_end)
        return 0;

    bzip->bzstream.next_out = (char *)buf;
    bzip->bzstream.avail_out = (unsigned int)size;

    do
    {
        if (bzip->bzstream.avail_in == 0)
        {
            bytes_to_read = sizeof(bzip->buffer);
            if (bzip->max_total_in > 0)
            {
                if ((bzip->max_total_in - bzip->total_in) < (int64_t)sizeof(bzip->buffer))
                    bytes_to_read = (int32_t)(bzip->max_total_in - bzip->total_in);
            }

            read = mz_stream_read(bzip->stream.base, bzip->buffer, bytes_to_read);

            if (read < 0)
            {
                bzip->error = BZ_IO_ERROR;
                break;
            }
            if (read == 0)
                break;

            bzip->bzstream.next_in = (char *)bzip->buffer;
            bzip->bzstream.avail_in = read;
        }

        total_in_before = bzip->bzstream.avail_in;
        total_out_before = bzip->bzstream.total_out_lo32 +
                (((uint64_t)bzip->bzstream.total_out_hi32) << 32);

        err = BZ2_bzDecompress(&bzip->bzstream);

        total_in_after = bzip->bzstream.avail_in;
        total_out_after = bzip->bzstream.total_out_lo32 +
                (((uint64_t)bzip->bzstream.total_out_hi32) << 32);

        in_bytes = (uint32_t)(total_in_before - total_in_after);
        out_bytes = (uint32_t)(total_out_after - total_out_before);

        total_in += in_bytes;
        total_out += out_bytes;

        bzip->total_in += in_bytes;
        bzip->total_out += out_bytes;

        if (err == BZ_STREAM_END)
        {
            bzip->stream_end = 1;
            break;
        }
        if (err != BZ_OK && err != BZ_RUN_OK)
        {
            bzip->error = err;
            break;
        }
    }
    while (bzip->bzstream.avail_out > 0);

    if (bzip->error != 0)
        return bzip->error;

    return total_out;
}

static int32_t mz_stream_bzip_flush(void *stream)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;
    if (mz_stream_write(bzip->stream.base, bzip->buffer, bzip->buffer_len) != bzip->buffer_len)
        return MZ_STREAM_ERROR;
    return MZ_OK;
}

static int32_t mz_stream_bzip_compress(void *stream, int flush)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;
    uint64_t total_out_before = 0;
    uint64_t total_out_after = 0;
    uint32_t out_bytes = 0;
    int32_t err = BZ_OK;

    do
    {
        if (bzip->bzstream.avail_out == 0)
        {
            if (mz_stream_bzip_flush(bzip) != MZ_OK)
            {
                bzip->error = BZ_DATA_ERROR;
                return MZ_STREAM_ERROR;
            }

            bzip->bzstream.avail_out = sizeof(bzip->buffer);
            bzip->bzstream.next_out = (char *)bzip->buffer;

            bzip->buffer_len = 0;
        }

        total_out_before = bzip->bzstream.total_out_lo32 +
                (((uint64_t)bzip->bzstream.total_out_hi32) << 32);

        err = BZ2_bzCompress(&bzip->bzstream, flush);

        total_out_after = bzip->bzstream.total_out_lo32 +
                (((uint64_t)bzip->bzstream.total_out_hi32) << 32);

        out_bytes = (uint32_t)(total_out_after - total_out_before);

        bzip->buffer_len += out_bytes;
        bzip->total_out += out_bytes;

        if (err == BZ_STREAM_END)
            break;
        if (err < 0)
        {
            bzip->error = err;
            return MZ_STREAM_ERROR;
        }
    }
    while ((bzip->bzstream.avail_in > 0) || (flush == BZ_FINISH && err == BZ_FINISH_OK));

    return MZ_OK;
}

int32_t mz_stream_bzip_write(void *stream, const void *buf, int32_t size)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;


    bzip->bzstream.next_in = (char *)(intptr_t)buf;
    bzip->bzstream.avail_in = (unsigned int)size;

    mz_stream_bzip_compress(stream, BZ_RUN);

    bzip->total_in += size;

    return size;
}

int64_t mz_stream_bzip_tell(void *stream)
{
    MZ_UNUSED(stream);

    return MZ_STREAM_ERROR;
}

int32_t mz_stream_bzip_seek(void *stream, int64_t offset, int32_t origin)
{
    MZ_UNUSED(stream);
    MZ_UNUSED(offset);
    MZ_UNUSED(origin);

    return MZ_STREAM_ERROR;
}

int32_t mz_stream_bzip_close(void *stream)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;

    if (bzip->mode & MZ_OPEN_MODE_WRITE)
    {
        mz_stream_bzip_compress(stream, BZ_FINISH);
        mz_stream_bzip_flush(stream);

        BZ2_bzCompressEnd(&bzip->bzstream);
    }
    else if (bzip->mode & MZ_OPEN_MODE_READ)
    {
        BZ2_bzDecompressEnd(&bzip->bzstream);
    }

    bzip->initialized = 0;

    if (bzip->error != BZ_OK)
        return MZ_STREAM_ERROR;
    return MZ_OK;
}

int32_t mz_stream_bzip_error(void *stream)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;
    return bzip->error;
}

int32_t mz_stream_bzip_get_prop_int64(void *stream, int32_t prop, int64_t *value)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;
    switch (prop)
    {
    case MZ_STREAM_PROP_TOTAL_IN:
        *value = bzip->total_in;
        return MZ_OK;
    case MZ_STREAM_PROP_TOTAL_OUT:
        *value = bzip->total_out;
        return MZ_OK;
    case MZ_STREAM_PROP_HEADER_SIZE:
        *value = 0;
        return MZ_OK;
    }
    return MZ_EXIST_ERROR;
}

int32_t mz_stream_bzip_set_prop_int64(void *stream, int32_t prop, int64_t value)
{
    mz_stream_bzip *bzip = (mz_stream_bzip *)stream;
    switch (prop)
    {
    case MZ_STREAM_PROP_COMPRESS_LEVEL:
        if (value < 0)
            bzip->level = 6;
        else
            bzip->level = (int16_t)value;
        return MZ_OK;
    case MZ_STREAM_PROP_TOTAL_IN_MAX:
        bzip->max_total_in = value;
        return MZ_OK;
    }
    return MZ_EXIST_ERROR;
}

void *mz_stream_bzip_create(void **stream)
{
    mz_stream_bzip *bzip = NULL;

    bzip = (mz_stream_bzip *)MZ_ALLOC(sizeof(mz_stream_bzip));
    if (bzip != NULL)
    {
        memset(bzip, 0, sizeof(mz_stream_bzip));
        bzip->stream.vtbl = &mz_stream_bzip_vtbl;
        bzip->level = 6;
    }
    if (stream != NULL)
        *stream = bzip;

    return bzip;
}

void mz_stream_bzip_delete(void **stream)
{
    mz_stream_bzip *bzip = NULL;
    if (stream == NULL)
        return;
    bzip = (mz_stream_bzip *)*stream;
    if (bzip != NULL)
        MZ_FREE(bzip);
    *stream = NULL;
}

void *mz_stream_bzip_get_interface(void)
{
    return (void *)&mz_stream_bzip_vtbl;
}

extern void bz_internal_error(int errcode)
{
    MZ_UNUSED(errcode);
}
