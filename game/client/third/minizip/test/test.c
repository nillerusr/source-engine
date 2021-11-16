#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_mem.h"
#include "mz_strm_bzip.h"
#include "mz_strm_crypt.h"
#include "mz_strm_aes.h"
#include "mz_strm_zlib.h"
#include "mz_zip.h"


/***************************************************************************/

void test_encrypt(char *method, mz_stream_create_cb crypt_create, char *password)
{
    char buf[UINT16_MAX];
    int16_t read = 0;
    int16_t written = 0;
    void *out_stream = NULL;
    void *in_stream = NULL;
    void *crypt_out_stream = NULL;
    char filename[120];

    mz_stream_os_create(&in_stream);

    if (mz_stream_os_open(in_stream, "LICENSE", MZ_OPEN_MODE_READ) == MZ_OK)
    {
        read = mz_stream_os_read(in_stream, buf, UINT16_MAX);
        mz_stream_os_close(in_stream);
    }

    mz_stream_os_delete(&in_stream);
    mz_stream_os_create(&out_stream);

    snprintf(filename, sizeof(filename), "LICENSE.encrypt.%s", method);
    if (mz_stream_os_open(out_stream, filename, MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE) == MZ_OK)
    {
        crypt_create(&crypt_out_stream);

        mz_stream_set_base(crypt_out_stream, out_stream);

        if (mz_stream_open(crypt_out_stream, password, MZ_OPEN_MODE_WRITE) == MZ_OK)
        {
            written = mz_stream_write(crypt_out_stream, buf, read);
            mz_stream_close(crypt_out_stream);
        }

        mz_stream_delete(&crypt_out_stream);

        mz_stream_os_close(out_stream);

        printf("%s encrypted %d\n", filename, written);
    }
    
    mz_stream_os_delete(&out_stream);
    mz_stream_os_create(&in_stream);

    if (mz_stream_os_open(in_stream, filename, MZ_OPEN_MODE_READ) == MZ_OK)
    {
        crypt_create(&crypt_out_stream);

        mz_stream_set_base(crypt_out_stream, in_stream);

        if (mz_stream_open(crypt_out_stream, password, MZ_OPEN_MODE_READ) == MZ_OK)
        {
            read = mz_stream_read(crypt_out_stream, buf, read);
            mz_stream_close(crypt_out_stream);
        }
        
        mz_stream_delete(&crypt_out_stream);

        mz_stream_os_close(in_stream);

        printf("%s decrypted %d\n", filename, read);
    }

    mz_stream_os_delete(&in_stream);
    mz_stream_os_create(&out_stream);

    snprintf(filename, sizeof(filename), "LICENSE.decrypt.%s", method);
    if (mz_stream_os_open(out_stream, filename, MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE) == MZ_OK)
    {
        mz_stream_os_write(out_stream, buf, read);
        mz_stream_os_close(out_stream);
    }

    mz_stream_os_delete(&out_stream);
}

void test_compress(char *method, mz_stream_create_cb create_compress)
{
    char buf[UINT16_MAX];
    int16_t read = 0;
    uint64_t total_in = 0;
    uint64_t total_out = 0;
    void *crc_in_stream = NULL;
    void *in_stream = NULL;
    void *out_stream = NULL;
    void *deflate_stream = NULL;
    void *inflate_stream = NULL;
    uint32_t crc32 = 0;
    char filename[120];

    printf("Testing compress %s\n", method);

    mz_stream_os_create(&in_stream);

    if (mz_stream_os_open(in_stream, "LICENSE", MZ_OPEN_MODE_READ) == MZ_OK)
    {

        mz_stream_crc32_create(&crc_in_stream);
        mz_stream_set_base(crc_in_stream, in_stream);
        mz_stream_crc32_open(crc_in_stream, NULL, MZ_OPEN_MODE_READ);
        read = mz_stream_read(crc_in_stream, buf, UINT16_MAX);
        crc32 = mz_stream_crc32_get_value(crc_in_stream);
        mz_stream_close(crc_in_stream);
        mz_stream_crc32_delete(&crc_in_stream);

        mz_stream_os_close(in_stream);
    }

    mz_stream_os_delete(&in_stream);

    if (read < 0)
    {
        printf("Failed to read LICENSE\n");
        return;
    }

    printf("LICENSE crc 0x%08x\n", crc32);

    mz_stream_os_create(&out_stream);

    snprintf(filename, sizeof(filename), "LICENSE.deflate.%s", method);
    if (mz_stream_os_open(out_stream, filename, MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE) == MZ_OK)
    {
        create_compress(&deflate_stream);
        mz_stream_set_base(deflate_stream, out_stream);

        mz_stream_open(deflate_stream, NULL, MZ_OPEN_MODE_WRITE);
        mz_stream_write(deflate_stream, buf, read);
        mz_stream_close(deflate_stream);

        mz_stream_get_prop_int64(deflate_stream, MZ_STREAM_PROP_TOTAL_IN, &total_in);
        mz_stream_get_prop_int64(deflate_stream, MZ_STREAM_PROP_TOTAL_OUT, &total_out);

        mz_stream_delete(&deflate_stream);

        printf("%s compressed from %u to %u\n", filename, (uint32_t)total_in, (uint32_t)total_out);

        mz_stream_os_close(out_stream);
    }
    
    mz_stream_os_delete(&out_stream);
    mz_stream_os_create(&in_stream);

    if (mz_stream_os_open(in_stream, filename, MZ_OPEN_MODE_READ) == MZ_OK)
    {
        create_compress(&inflate_stream);
        mz_stream_set_base(inflate_stream, in_stream);

        mz_stream_open(inflate_stream, NULL, MZ_OPEN_MODE_READ);
        read = mz_stream_read(inflate_stream, buf, UINT16_MAX);
        mz_stream_close(inflate_stream);

        mz_stream_get_prop_int64(inflate_stream, MZ_STREAM_PROP_TOTAL_IN, &total_in);
        mz_stream_get_prop_int64(inflate_stream, MZ_STREAM_PROP_TOTAL_OUT, &total_out);

        mz_stream_delete(&inflate_stream);

        mz_stream_os_close(in_stream);

        printf("%s uncompressed from %u to %u\n", filename, (uint32_t)total_in, (uint32_t)total_out);
    }

    mz_stream_os_delete(&in_stream);
    mz_stream_os_create(&out_stream);

    snprintf(filename, sizeof(filename), "LICENSE.inflate.%s", method);
    if (mz_stream_os_open(out_stream, filename, MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE) == MZ_OK)
    {
        mz_stream_crc32_create(&crc_in_stream);
        mz_stream_crc32_open(crc_in_stream, NULL, MZ_OPEN_MODE_WRITE);

        mz_stream_set_base(crc_in_stream, in_stream);
        if (mz_stream_write(crc_in_stream, buf, read) != read)
            printf("Failed to write %s\n", filename);

        crc32 = mz_stream_crc32_get_value(crc_in_stream);

        mz_stream_close(crc_in_stream);
        mz_stream_delete(&crc_in_stream);

        mz_stream_os_close(out_stream);

        printf("%s crc 0x%08x\n", filename, crc32);
    }
    
    mz_stream_os_delete(&out_stream);
}

/***************************************************************************/

void test_aes()
{
    test_encrypt("aes", mz_stream_aes_create, "hello");
}

void test_crypt()
{
    test_encrypt("crypt", mz_stream_crypt_create, "hello");
}

void test_zlib()
{
    test_compress("zlib", mz_stream_zlib_create);
}

void test_bzip()
{
    test_compress("bzip", mz_stream_bzip_create);
}

/***************************************************************************/

void test_zip_mem()
{
    mz_zip_file file_info = { 0 };
    void *read_mem_stream = NULL;
    void *write_mem_stream = NULL;
    void *os_stream = NULL;
    void *zip_handle = NULL;
    int32_t written = 0;
    int32_t read = 0;
    int32_t text_size = 0;
    int32_t buffer_size = 0;
    int32_t err = MZ_OK;
    char *buffer_ptr = NULL;
    char *password = "1234";
    char *text_name = "test";
    char *text_ptr = "test string";
    char temp[120];


    text_size = (int32_t)strlen(text_ptr);

    // Write zip to memory stream
    mz_stream_mem_create(&write_mem_stream);
    mz_stream_mem_set_grow_size(write_mem_stream, 128 * 1024);
    mz_stream_open(write_mem_stream, NULL, MZ_OPEN_MODE_CREATE);

    zip_handle = mz_zip_open(write_mem_stream, MZ_OPEN_MODE_READWRITE);

    if (zip_handle != NULL)
    {
        file_info.version_madeby = MZ_VERSION_MADEBY;
        file_info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
        file_info.filename = text_name;
        file_info.uncompressed_size = text_size;
        file_info.aes_version = MZ_AES_VERSION;

        err = mz_zip_entry_write_open(zip_handle, &file_info, MZ_COMPRESS_LEVEL_DEFAULT, password);
        if (err == MZ_OK)
        {
            written = mz_zip_entry_write(zip_handle, text_ptr, text_size);
            if (written < MZ_OK)
                err = written;
            mz_zip_entry_close(zip_handle);
        }

        mz_zip_close(zip_handle);
    }
    else
    {
        err = MZ_INTERNAL_ERROR;
    }

    mz_stream_mem_get_buffer(write_mem_stream, (void **)&buffer_ptr);
    mz_stream_mem_seek(write_mem_stream, 0, MZ_SEEK_END);
    buffer_size = (int32_t)mz_stream_mem_tell(write_mem_stream);

    if (err == MZ_OK)
    {
        // Create a zip file on disk for inspection
        mz_stream_os_create(&os_stream);
        mz_stream_os_open(os_stream, "mytest.zip", MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE);
        mz_stream_os_write(os_stream, buffer_ptr, buffer_size);
        mz_stream_os_close(os_stream);
        mz_stream_os_delete(&os_stream);
    }

    if (err == MZ_OK)
    {
        // Read from a memory stream
        mz_stream_mem_create(&read_mem_stream);
        mz_stream_mem_set_buffer(read_mem_stream, buffer_ptr, buffer_size);
        mz_stream_open(read_mem_stream, NULL, MZ_OPEN_MODE_READ);

        zip_handle = mz_zip_open(read_mem_stream, MZ_OPEN_MODE_READ);

        if (zip_handle != NULL)
        {
            err = mz_zip_goto_first_entry(zip_handle);
            if (err == MZ_OK)
                err = mz_zip_entry_read_open(zip_handle, 0, password);
            if (err == MZ_OK)
                read = mz_zip_entry_read(zip_handle, temp, sizeof(temp));

            mz_zip_entry_close(zip_handle);
            mz_zip_close(zip_handle);
        }

        mz_stream_mem_close(&read_mem_stream);
        mz_stream_mem_delete(&read_mem_stream);
        read_mem_stream = NULL;
    }

    mz_stream_mem_close(write_mem_stream);
    mz_stream_mem_delete(&write_mem_stream);
    write_mem_stream = NULL;
}


/***************************************************************************/
