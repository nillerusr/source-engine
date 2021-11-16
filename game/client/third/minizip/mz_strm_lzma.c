/* mz_strm_lzma.c -- Stream for lzma inflate/deflate
   Version 2.3.3, June 10, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
      https://github.com/nmoinvaz/minizip

   This program is distributed under the terms of the same license as lzma.
   See the accompanying LICENSE file for the full text of the license.
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <lzma.h>

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_lzma.h"

/***************************************************************************/

#define MZ_LZMA_HEADER_SIZE (4)

/***************************************************************************/

static mz_stream_vtbl mz_stream_lzma_vtbl = {
    mz_stream_lzma_open,
    mz_stream_lzma_is_open,
    mz_stream_lzma_read,
    mz_stream_lzma_write,
    mz_stream_lzma_tell,
    mz_stream_lzma_seek,
    mz_stream_lzma_close,
    mz_stream_lzma_error,
    mz_stream_lzma_create,
    mz_stream_lzma_delete,
    mz_stream_lzma_get_prop_int64,
    mz_stream_lzma_set_prop_int64
};

/***************************************************************************/

typedef struct mz_stream_lzma_s {
    mz_stream   stream;
    lzma_stream lstream;
    int32_t     mode;
    int32_t     error;
    uint8_t     buffer[INT16_MAX];
    int32_t     buffer_len;
    int64_t     total_in;
    int64_t     total_out;
    int64_t     max_total_in;
    int64_t     max_total_out;
    int8_t      initialized;
    uint32_t    preset;
} mz_stream_lzma;

/***************************************************************************/

int32_t mz_stream_lzma_open(void *stream, const char *path, int32_t mode)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;
    lzma_filter filters[LZMA_FILTERS_MAX + 1];
    lzma_options_lzma opt_lzma;
    uint32_t size = 0;
    uint8_t major = 0;
    uint8_t minor = 0;

    MZ_UNUSED(path);

    memset(&opt_lzma, 0, sizeof(opt_lzma));

    lzma->lstream.total_in = 0;
    lzma->lstream.total_out = 0;

    lzma->total_in = 0;
    lzma->total_out = 0;

    if (mode & MZ_OPEN_MODE_WRITE)
    {
        lzma->lstream.next_out = lzma->buffer;
        lzma->lstream.avail_out = sizeof(lzma->buffer);

        if (lzma_lzma_preset(&opt_lzma, lzma->preset))
            return MZ_STREAM_ERROR;

        memset(&filters, 0, sizeof(filters));

        filters[0].id = LZMA_FILTER_LZMA1;
        filters[0].options = &opt_lzma;
        filters[1].id = LZMA_VLI_UNKNOWN;

        lzma_properties_size(&size, (lzma_filter *)&filters);

        mz_stream_write_uint8(lzma->stream.base, LZMA_VERSION_MAJOR);
        mz_stream_write_uint8(lzma->stream.base, LZMA_VERSION_MINOR);
        mz_stream_write_uint16(lzma->stream.base, (uint16_t)size);

        lzma->total_out += MZ_LZMA_HEADER_SIZE;

        lzma->error = lzma_alone_encoder(&lzma->lstream, &opt_lzma);
    }
    else if (mode & MZ_OPEN_MODE_READ)
    {
        lzma->lstream.next_in = lzma->buffer;
        lzma->lstream.avail_in = 0;

        mz_stream_read_uint8(lzma->stream.base, &major);
        mz_stream_read_uint8(lzma->stream.base, &minor);
        mz_stream_read_uint16(lzma->stream.base, (uint16_t *)&size);

        lzma->total_in += MZ_LZMA_HEADER_SIZE;

        lzma->error = lzma_alone_decoder(&lzma->lstream, UINT64_MAX);
    }

    if (lzma->error != LZMA_OK)
        return MZ_STREAM_ERROR;

    lzma->initialized = 1;
    lzma->mode = mode;
    return MZ_OK;
}

int32_t mz_stream_lzma_is_open(void *stream)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;
    if (lzma->initialized != 1)
        return MZ_STREAM_ERROR;
    return MZ_OK;
}

int32_t mz_stream_lzma_read(void *stream, void *buf, int32_t size)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;
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
    int32_t err = LZMA_OK;


    lzma->lstream.next_out = (uint8_t*)buf;
    lzma->lstream.avail_out = (size_t)size;

    do
    {
        if (lzma->lstream.avail_in == 0)
        {
            bytes_to_read = sizeof(lzma->buffer);
            if (lzma->max_total_in > 0)
            {
                if ((lzma->max_total_in - lzma->total_in) < (int64_t)sizeof(lzma->buffer))
                    bytes_to_read = (int32_t)(lzma->max_total_in - lzma->total_in);
            }

            read = mz_stream_read(lzma->stream.base, lzma->buffer, bytes_to_read);

            if (read < 0)
            {
                lzma->error = MZ_STREAM_ERROR;
                break;
            }
            if (read == 0)
                break;

            lzma->lstream.next_in = lzma->buffer;
            lzma->lstream.avail_in = read;
        }

        total_in_before = lzma->lstream.avail_in;
        total_out_before = lzma->lstream.total_out;

        err = lzma_code(&lzma->lstream, LZMA_RUN);

        total_in_after = lzma->lstream.avail_in;
        total_out_after = lzma->lstream.total_out;
        if ((lzma->max_total_out != -1) && (int64_t)total_out_after > lzma->max_total_out)
            total_out_after = lzma->max_total_out;

        in_bytes = (uint32_t)(total_in_before - total_in_after);
        out_bytes = (uint32_t)(total_out_after - total_out_before);

        total_in += in_bytes;
        total_out += out_bytes;

        lzma->total_in += in_bytes;
        lzma->total_out += out_bytes;

        if (err == LZMA_STREAM_END)
            break;
        if (err != LZMA_OK)
        {
            lzma->error = err;
            break;
        }
    }
    while (lzma->lstream.avail_out > 0);

    if (lzma->error != 0)
        return lzma->error;

    return total_out;
}

static int32_t mz_stream_lzma_flush(void *stream)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;
    if (mz_stream_write(lzma->stream.base, lzma->buffer, lzma->buffer_len) != lzma->buffer_len)
        return MZ_STREAM_ERROR;
    return MZ_OK;
}

static int32_t mz_stream_lzma_code(void *stream, int32_t flush)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;
    uint64_t total_out_before = 0;
    uint64_t total_out_after = 0;
    uint32_t out_bytes = 0;
    int32_t err = LZMA_OK;


    do
    {
        if (lzma->lstream.avail_out == 0)
        {
            if (mz_stream_lzma_flush(lzma) != MZ_OK)
            {
                lzma->error = MZ_STREAM_ERROR;
                return MZ_STREAM_ERROR;
            }

            lzma->lstream.avail_out = sizeof(lzma->buffer);
            lzma->lstream.next_out = lzma->buffer;

            lzma->buffer_len = 0;
        }

        total_out_before = lzma->lstream.total_out;
        err = lzma_code(&lzma->lstream, flush);
        total_out_after = lzma->lstream.total_out;

        out_bytes = (uint32_t)(total_out_after - total_out_before);

        if (err != LZMA_OK && err != LZMA_STREAM_END)
        {
            lzma->error = err;
            return MZ_STREAM_ERROR;
        }

        lzma->buffer_len += out_bytes;
        lzma->total_out += out_bytes;
    }
    while (lzma->lstream.avail_in > 0);

    return MZ_OK;
}

int32_t mz_stream_lzma_write(void *stream, const void *buf, int32_t size)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;


    lzma->lstream.next_in = (uint8_t*)(intptr_t)buf;
    lzma->lstream.avail_in = (size_t)size;

    mz_stream_lzma_code(stream, LZMA_RUN);

    lzma->total_in += size;

    return size;
}

int64_t mz_stream_lzma_tell(void *stream)
{
    MZ_UNUSED(stream);

    return MZ_STREAM_ERROR;
}

int32_t mz_stream_lzma_seek(void *stream, int64_t offset, int32_t origin)
{
    MZ_UNUSED(stream);
    MZ_UNUSED(offset);
    MZ_UNUSED(origin);

    return MZ_STREAM_ERROR;
}

int32_t mz_stream_lzma_close(void *stream)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;

    if (lzma->mode & MZ_OPEN_MODE_WRITE)
    {
        mz_stream_lzma_code(stream, LZMA_FINISH);
        mz_stream_lzma_flush(stream);

        lzma_end(&lzma->lstream);
    }
    else if (lzma->mode & MZ_OPEN_MODE_READ)
    {
        lzma_end(&lzma->lstream);
    }

    lzma->initialized = 0;

    if (lzma->error != LZMA_OK)
        return MZ_STREAM_ERROR;
    return MZ_OK;
}

int32_t mz_stream_lzma_error(void *stream)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;
    return lzma->error;
}

int32_t mz_stream_lzma_get_prop_int64(void *stream, int32_t prop, int64_t *value)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;
    switch (prop)
    {
    case MZ_STREAM_PROP_TOTAL_IN:
        *value = lzma->total_in;
        return MZ_OK;
    case MZ_STREAM_PROP_TOTAL_OUT:
        *value = lzma->total_out;
        return MZ_OK;
    case MZ_STREAM_PROP_HEADER_SIZE:
        *value = MZ_LZMA_HEADER_SIZE;
        return MZ_OK;
    }
    return MZ_EXIST_ERROR;
}

int32_t mz_stream_lzma_set_prop_int64(void *stream, int32_t prop, int64_t value)
{
    mz_stream_lzma *lzma = (mz_stream_lzma *)stream;
    switch (prop)
    {
    case MZ_STREAM_PROP_COMPRESS_LEVEL:
        if (value >= 9)
            lzma->preset = LZMA_PRESET_EXTREME;
        else
            lzma->preset = LZMA_PRESET_DEFAULT;
        return MZ_OK;
    case MZ_STREAM_PROP_TOTAL_IN_MAX:
        lzma->max_total_in = value;
        return MZ_OK;
    case MZ_STREAM_PROP_TOTAL_OUT_MAX:
        if (value < -1)
            return MZ_PARAM_ERROR;
        lzma->max_total_out = value;
        return MZ_OK;
    }
    return MZ_EXIST_ERROR;
}

void *mz_stream_lzma_create(void **stream)
{
    mz_stream_lzma *lzma = NULL;

    lzma = (mz_stream_lzma *)MZ_ALLOC(sizeof(mz_stream_lzma));
    if (lzma != NULL)
    {
        memset(lzma, 0, sizeof(mz_stream_lzma));
        lzma->stream.vtbl = &mz_stream_lzma_vtbl;
        lzma->preset = LZMA_PRESET_DEFAULT;
        lzma->max_total_out = -1;
    }
    if (stream != NULL)
        *stream = lzma;

    return lzma;
}

void mz_stream_lzma_delete(void **stream)
{
    mz_stream_lzma *lzma = NULL;
    if (stream == NULL)
        return;
    lzma = (mz_stream_lzma *)*stream;
    if (lzma != NULL)
        MZ_FREE(lzma);
    *stream = NULL;
}

void *mz_stream_lzma_get_interface(void)
{
    return (void *)&mz_stream_lzma_vtbl;
}

static int64_t mz_stream_lzma_crc32(int64_t value, const void *buf, int32_t size)
{
    return (int32_t)lzma_crc32(buf, size, (int32_t)value);
}

void *mz_stream_lzma_get_crc32_table(void)
{
    extern const uint32_t lzma_crc32_table;
    return (void *)lzma_crc32_table;
}

void *mz_stream_lzma_get_crc32_update(void)
{
    return (void *)mz_stream_lzma_crc32;
}
