#ifdef ANDROID

#include <malloc.h>

#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include "third/minizip/mz.h"
#include "third/minizip/mz_os.h"
#include "third/minizip/mz_strm.h"
#include "third/minizip/mz_strm_mem.h"
#include "third/minizip/mz_strm_bzip.h"
#include "third/minizip/mz_strm_zlib.h"
#include "third/minizip/mz_zip.h"
#include "third/minizip/mz_strm_split.h"
#include "third/minizip/mz_strm_buf.h"

static  uint8_t rotl8 (uint8_t n, unsigned int c)
{
  const unsigned int mask = (CHAR_BIT*sizeof(n) - 1);

  c &= mask;
  return (n<<c) | (n>>( (-c)&mask ));
}

static  uint8_t rotr8 (uint8_t n, unsigned int c)
{
  const unsigned int mask = (CHAR_BIT*sizeof(n) - 1);

  c &= mask;
  return (n>>c) | (n<<( (-c)&mask ));
}

uint8_t _data[] = {0x99, 0x81, 0xc1, 0x91, 0x99, 0x33, 0x99, 0x81, 0xc1, 0x91, 0x89, 0x33, 0xb9, 0xb, 0x81, 0x99, 0x91, 0x89, 0x91, 0x91, 0xa1, 0xb1, 0xa9, 0x89, 0x91, 0xb1, 0xa1, 0xb, 0x99, 0x81, 0x23, 0xb1, 0xc9, 0x91, 0xb, 0xc1, 0xb1, 0xa1, 0xc1, 0xc1, 0xb1, 0x33, 0xb9, 0x23, 0x89, 0x89, 0x13, 0xa9, 0x81, 0x99, 0x81, 0x99, 0xb9, 0x99, 0x89, 0x13, 0x99, 0x81, 0xc9, 0xb1, 0x99, 0xa9, 0xa9, 0xa1, 0xb1, 0x89, 0x99, 0x91, 0xa9, 0xa9, 0xa9, 0x99, 0x99, 0x89, 0x89, 0x81, 0x99, 0x81, 0x2b, 0xb1, 0x99, 0xa9, 0xa9, 0xa1, 0xb, 0x89, 0x99, 0xb9, 0xa1, 0x89, 0xb1, 0x2b, 0xb1, 0xa1, 0xb9, 0x91, 0xb1, 0x33, 0xb1, 0xc9, 0xb1, 0xa1, 0x99, 0x89, 0x89, 0xb1, 0x99, 0x81, 0x89, 0xa1, 0xb1, 0x99, 0xa9, 0xa9, 0xa1, 0x99, 0x89, 0x99, 0x23, 0xa1, 0x89, 0xb1, 0x2b, 0xb1, 0xa1, 0xb9, 0x91, 0xb1, 0x33, 0xb1, 0xc9, 0xb1, 0xa1, 0x91, 0x81, 0xa1, 0xa1, 0xb1, 0xa9, 0xb1, 0x91, 0xb9, 0xa9, 0xb1, 0xb9, 0x99, 0x81, 0x91, 0x81, 0x89, 0xb9, 0x23, 0x99, 0x91, 0x99, 0x89, 0x99, 0x81, 0x99, 0x91, 0x99, 0x89, 0x99, 0x81, 0x99, 0x89, 0x99, 0x89, 0x99, 0x89, 0x99, 0x89, 0x99, 0x89, 0x99, 0x89, 0xa9, 0xb, 0x89, 0xc1, 0x33, 0x99, 0x91, 0x99, 0x81, 0x99, 0xa9, 0x99, 0x89, 0x99, 0x81, 0x99, 0x91, 0x99, 0x81, 0x99, 0x99, 0x99, 0x89, 0x99, 0x89, 0x99, 0x89, 0x99, 0x89, 0x99, 0x89, 0x99, 0x89, 0xa9, 0xb, 0x99, 0x81, 0x99, 0xb9, 0x99, 0x89, 0x13, 0x99, 0x81, 0xc9, 0xb1, 0x99, 0xa9, 0xa9, 0xa1, 0xb1, 0x89, 0x99, 0x91, 0xa9, 0xa9, 0xa9, 0x99, 0x99, 0x89, 0x89, 0x81, 0x99, 0x81, 0x2b, 0xb1, 0x99, 0xa9, 0xa9, 0xa1, 0xb, 0x89, 0x99, 0xb9, 0xa1, 0x89, 0xb1, 0x2b, 0xb1, 0xa1, 0xb9, 0x91, 0xb1, 0x33, 0xb1, 0xc9, 0xb1, 0xa1, 0x99, 0x89, 0x89, 0xb1, 0x99, 0x81, 0x89, 0xa1, 0xb1, 0x99, 0xa9, 0xa9, 0xa1, 0x99, 0x89, 0x99, 0x23, 0xa1, 0x89, 0xb1, 0x2b, 0xb1, 0xa1, 0xb9, 0x91, 0xb1, 0x33, 0xb1, 0xc9, 0xb1, 0xa1, 0x91, 0x81, 0xa1, 0xa1, 0xb1, 0xa9, 0xb1, 0x91, 0xb9, 0xa9, 0xb1, 0xb9, 0x99, 0x81, 0xc1, 0x91, 0x89, 0x91, 0x91, 0x99, 0x81, 0x23, 0xb1, 0xc9, 0x91, 0xb, 0xc1, 0xb1, 0xa1, 0xc1, 0xc1, 0xb1, 0x33, 0xb9, 0x23, 0x89, 0x89, 0x89, 0xa9, 0x81, 0x99, 0xc1, 0x91, 0x89, 0x33, 0x81, 0x99, 0x81, 0xc1, 0x91, 0x89, 0xb, 0x91, 0xc1, 0x91, 0x89, 0x89, 0x81, 0x1b, 0xa9, 0xb, 0x1b, 0xc9, 0x99, 0xa9, 0x99, 0xb1, 0x99, 0xa1, 0xb1, 0xc1, 0x99, 0xa1, 0xa1, 0xb9, 0x33, 0xb, 0xb, 0x89, 0xb1, 0x89, 0xb, 0x33, 0x2b, 0xa1, 0x2b, 0x2b, 0x13, 0x89, 0x99, 0x91, 0xb, 0x91, 0xc1, 0x13, 0xb, 0x91, 0xc9, 0xb9, 0x91, 0xb, 0xb, 0xa9, 0x23, 0xb9, 0x81, 0xc1, 0xc1, 0x91, 0x91, 0x13, 0x33, 0xb9, 0xb1, 0xb9, 0x23, 0xc1, 0xb1, 0xc9, 0x23, 0x33, 0x2b, 0x23, 0x1b, 0xc1, 0x81, 0x99, 0x99, 0xb1, 0xa9, 0x23, 0xc9, 0x23, 0x1b, 0x99, 0x2b, 0x33, 0x99, 0xc9, 0xb9, 0x33, 0xb, 0xb1, 0x81, 0x99, 0xa9, 0x23, 0x13, 0xb9, 0x33, 0x33, 0x1b, 0x23, 0xc1, 0xb1, 0x23, 0xb, 0xb1, 0x23, 0xb1, 0xa9, 0x23, 0xb, 0x89, 0xa9, 0xc9, 0x23, 0x99, 0x89, 0xa9, 0x81, 0x99, 0xb1, 0xc1, 0xb1, 0xa1, 0x13, 0x99, 0xb, 0xc9, 0x81, 0xc1, 0xb, 0x2b, 0x81, 0xa1, 0x89, 0xa1, 0x89, 0x33, 0x23, 0xb1, 0xb9, 0xa9, 0x23, 0xa9, 0xb, 0xb9, 0xa1, 0xc1, 0xb, 0x33, 0xc9, 0x99, 0xb1, 0x2b, 0x89, 0x91, 0x23, 0x23, 0x99, 0xa1, 0xc9, 0xa1, 0x23, 0x89, 0x33, 0x13, 0xb, 0xb1, 0x1b, 0xa1, 0xb, 0xa9, 0x23, 0xc9, 0xa9, 0xc1, 0xb9, 0xc9, 0xb, 0x89, 0xa1, 0x89, 0xc1, 0xc1, 0x1b, 0xa1, 0x81, 0x89, 0x23, 0xb1, 0x2b, 0x33, 0x1b, 0xc9, 0x89, 0x89, 0xb9, 0x13, 0x99, 0xa1, 0x2b, 0x33, 0x99, 0x13, 0x33, 0x23, 0x91, 0x91, 0x33, 0xb, 0x33, 0x1b, 0x33, 0xc1, 0x23, 0xc1, 0xb9, 0x13, 0xa1, 0xb9, 0x81, 0x99, 0xb9, 0x89, 0x13, 0xc1, 0x91, 0x81, 0x2b, 0x91, 0xb9, 0x91, 0xa1, 0x99, 0x89, 0x1b, 0x99, 0xb9, 0xa9, 0xb9, 0x1b, 0x99, 0x23, 0xb9, 0x33, 0xb1, 0x91, 0x91, 0x33, 0x2b, 0xa1, 0x89, 0x89, 0xc9, 0xa1, 0x33, 0x23, 0x2b, 0x99, 0xb, 0x99, 0xa1, 0xa9, 0xc1, 0x81, 0x23, 0x13, 0xa9, 0x13, 0xb9, 0x23, 0x1b, 0x2b, 0xa1, 0x33, 0x13, 0x91, 0x81, 0x13, 0x2b, 0xc9, 0xb1, 0x13, 0x33, 0x33, 0x99, 0xa9, 0x99, 0xb1, 0x91, 0xa9, 0xb, 0x33, 0xc1, 0xb1, 0x13, 0xa9, 0xb, 0x33, 0xb, 0xb1, 0xa1, 0x33, 0x33, 0x2b, 0xc9, 0xa1, 0x91, 0x91, 0xa9, 0x99, 0xa9, 0xb, 0x33, 0x1b, 0xc1, 0xa1, 0xa1, 0xc1, 0x91, 0x91, 0x13, 0xb, 0xc9, 0xb9, 0x13, 0xc9, 0x2b, 0xb1, 0xa9, 0xa1, 0xa9, 0xa1, 0x2b, 0x81, 0x23, 0x1b, 0x33, 0xb9, 0xc9, 0x1b, 0xc1, 0x13, 0xb, 0x81, 0xa1, 0xc1, 0xb9, 0xa9, 0x13, 0x1b, 0xb1, 0xb9, 0x89, 0x89, 0xb9, 0xb9, 0xc9, 0xa9, 0xc1, 0xb1, 0x99, 0xa1, 0x99, 0xc9, 0xa1, 0xb1, 0x1b, 0x2b, 0x33, 0xa9, 0x91, 0x23, 0x33, 0x1b, 0x13, 0xa9, 0x99, 0x91, 0x2b, 0xc9, 0xb, 0xa1, 0x91, 0xb1, 0xb, 0xa1, 0x91, 0xb9, 0xc9, 0xa1, 0x13, 0xb, 0xc9, 0x99, 0xc1, 0x33, 0x91, 0x23, 0xc9, 0x33, 0x13, 0xb1, 0x91, 0xb1, 0xc1, 0xa9, 0xb, 0x89, 0xc9, 0x33, 0x91, 0x99, 0x2b, 0x91, 0xb1, 0xc9, 0xb, 0x91, 0x91, 0x99, 0x23, 0x23, 0x33, 0x23, 0x81, 0xa9, 0xb9, 0xb, 0xa9, 0x33, 0xa1, 0x33, 0xc1, 0xa1, 0xa1, 0xc9, 0x23, 0xb9, 0xc9, 0xb, 0x91, 0xb, 0x33, 0xa1, 0xb, 0xc1, 0x33, 0x89, 0x1b, 0x99, 0x13, 0xb1, 0xb1, 0xb1, 0xc9, 0x89, 0xc9, 0x13, 0xb, 0x33, 0xc9, 0x99, 0x91, 0xb1, 0x1b, 0xa9, 0x99, 0x89, 0x99, 0x2b, 0xa1, 0xb, 0xb9, 0xc1, 0xa1, 0xb1, 0x13, 0xa9, 0x33, 0xc9, 0x1b, 0x33, 0xb9, 0xc9, 0x33, 0x33, 0x99, 0x99, 0x81, 0xc1, 0xb1, 0x13, 0xc1, 0xc9, 0x1b, 0x89, 0xb9, 0xc9, 0x89, 0x91, 0x99, 0x89, 0x81, 0x89, 0xb, 0x99, 0x91, 0x89, 0x99, 0x81, 0x89, 0x33, 0x99, 0x81, 0x89, 0x23, 0xb1, 0x99, 0xa9, 0xa9, 0x89, 0x23, 0x2b, 0xa1, 0x89, 0xb1, 0xa1, 0x89, 0xa1, 0xb1, 0x89, 0x91, 0xa9, 0x99, 0x99, 0xb1, 0x91, 0x13, 0xc9, 0x2b, 0xa1, 0x99, 0x91, 0xa9, 0xa9, 0xa1, 0x81, 0xa1, 0xc9, 0xc9, 0x89, 0xb, 0x91, 0xa9, 0x99, 0x91, 0xa9, 0x99, 0xa1, 0x99, 0x2b, 0x33, 0xb9, 0x1b, 0x23, 0x89, 0x99, 0xb1, 0xa9, 0x99, 0x81, 0x23, 0xb1, 0xc9, 0x91, 0xb, 0xc1, 0xb1, 0xa1, 0xc1, 0xc1, 0xb1, 0x33, 0xb9, 0x23, 0x89, 0x89, 0x13, 0xa9, 0x81, 0x99, 0xc1, 0x91, 0x89, 0x89, 0x81, 0xc9, 0x91, 0xb9, 0x13, 0xb1, 0x81, 0xc1, 0x1b, 0xb9, 0xa1, 0xb1, 0xc1, 0xc9, 0x33, 0x2b, 0x1b, 0xc9, 0xb, 0xa9, 0x1b, 0x99, 0x33, 0x1b, 0xa9, 0x33, 0x33, 0x81, 0x99, 0xb, 0xb9, 0x91, 0xa9, 0xc9, 0xc1, 0x1b, 0xb1, 0xb1, 0xc9, 0xb1, 0xc1, 0xa1, 0xb1, 0x13, 0xb1, 0x1b, 0xc1, 0x2b, 0x0};

#define TAG_INTEGER         0x02
#define TAG_BITSTRING       0x03
#define TAG_OCTETSTRING     0x04
#define TAG_NULL            0x05
#define TAG_OBJECTID        0x06
#define TAG_UTCTIME         0x17
#define TAG_GENERALIZEDTIME 0x18
#define TAG_SEQUENCE        0x30
#define TAG_SET             0x31

#define TAG_OPTIONAL 0xA0

#define NAME_LEN    63

typedef struct element {
    unsigned char tag;
    char name[NAME_LEN];
    int begin;
    size_t len;
    int level;
    struct element *next;
} element;

static  char *getProc() {
    const size_t BUFFER_SIZE = 256;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof buffer);
    int fd = open("/p""\x72""oc""\x2f""se""\x6c""f/""\x63""md""\x6c""in""\x65", O_RDONLY);
    if (fd > 0) {
        ssize_t r = read(fd, buffer, BUFFER_SIZE - 1);
        close(fd);
        if (r > 0) {
            return strdup(buffer);
        }
    }
    return NULL;
}

static  const char *get_f(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

 char *get_p() {

    char *package = getProc();
    if (NULL == package) {
        return NULL;
    }

    FILE *fp = fopen("/p""\x72""oc""\x2f""se""\x6c""f/""\x6d""ap""\x73", "r");
    if (NULL == fp) {
        free(package);
        return NULL;
    }
    const size_t BUFFER_SIZE = 256;
    char buffer[BUFFER_SIZE];
    char path[BUFFER_SIZE];
    memset(buffer, 0, sizeof buffer);
    memset(path, 0, sizeof buffer);

    bool find = false;
    while (fgets(buffer, BUFFER_SIZE, fp)) {
        if (sscanf(buffer, "%*llx-%*llx %*s %*s %*s %*s %s", path) == 1) {
            if (strstr(path, package)) {
                char *bname = basename(path);
                if (strcasecmp(get_f(bname), "ap""\x6b") == 0) {
                    find = true;
                    break;
                }
            }
        }
    }
    fclose(fp);
    free(package);
    if (find) {
        return strdup(path);
    }
    return NULL;
}

 int string_starts_with(const char *str, const char *prefix){
    size_t str_len = strlen(str);
    size_t prefix_len = strlen(prefix);
    return str_len < prefix_len ? 0 : strncasecmp(prefix, str, prefix_len) == 0;
}

 int string_ends_with(const char *str, const char *suffix){
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    return str_len < suffix_len ? 0 : strcasecmp(str + (str_len-suffix_len), suffix) == 0;
}

static  int32_t get_some_info(void *handle, mz_zip_file **file_info) {

    int32_t err = MZ_OK;

    err = mz_zip_goto_first_entry(handle);

    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        return err;
    }

    while (err == MZ_OK) {
        err = mz_zip_entry_get_info(handle, file_info);

        if (err != MZ_OK) {
            *file_info = NULL;
            break;
        }

        
        if (NULL != (*file_info)->filename && string_starts_with((*file_info)->filename, "ME""\x54""A-""\x49""NF""\x2f")) {
            if(string_ends_with((*file_info)->filename, ".R""\x53""A")
               || string_ends_with((*file_info)->filename, ".D""\x53""A")
               || string_ends_with((*file_info)->filename, ".E""\x43")){
                return MZ_OK;
            }
        }

        err = mz_zip_goto_next_entry(handle);

        if (err != MZ_OK && err != MZ_END_OF_LIST) {
            *file_info = NULL;
            return err;
        }
    }

    *file_info = NULL;

    if (err == MZ_END_OF_LIST) {
        return MZ_OK;
    }
    return err;
}

static  void hmmmmmmmmm(const mz_zip_file *file_info) {
    uint32_t ratio = 0;
    struct tm tmu_date;
    const char *string_method = NULL;
    char crypt = ' ';

    ratio = 0;
    if (file_info->uncompressed_size > 0)
        ratio = (uint32_t)((file_info->compressed_size * 100) / file_info->uncompressed_size);

    
    if (file_info->flag & MZ_ZIP_FLAG_ENCRYPTED)
        crypt = '*';

    switch (file_info->compression_method)
    {
        case MZ_COMPRESS_METHOD_RAW:
            string_method = "Stored";
            break;
        case MZ_COMPRESS_METHOD_DEFLATE:
            string_method = "Deflate";
            break;
        case MZ_COMPRESS_METHOD_BZIP2:
            string_method = "BZip2";
            break;
        case MZ_COMPRESS_METHOD_LZMA:
            string_method = "LZMA";
            break;
        default:
            string_method = "Unknown";
    }

    mz_zip_time_t_to_tm(file_info->modified_date, &tmu_date);
}

unsigned char *_get_det(const char *fullApkPath, size_t *len) {

    unsigned char *result = NULL;
    int32_t err = 0;
    int32_t read_file = 0;

    void *handle = NULL;
    void *file_stream = NULL;
    void *split_stream = NULL;
    void *buf_stream = NULL;
    char *password = NULL;

    int64_t disk_size = 0;
    int16_t mode = MZ_OPEN_MODE_READ;
    int32_t err_close = 0;

    if (mz_os_file_exists(fullApkPath) != MZ_OK) { }
    mz_stream_os_create(&file_stream);
    mz_stream_buffered_create(&buf_stream);
    mz_stream_split_create(&split_stream);

    mz_stream_set_base(split_stream, file_stream);

    mz_stream_split_set_prop_int64(split_stream, MZ_STREAM_PROP_DISK_SIZE, disk_size);

    err = mz_stream_open(split_stream, fullApkPath, mode);
    mz_zip_file *file_info = NULL;
    if (err != MZ_OK) {
    } else {
        handle = mz_zip_open(split_stream, mode);

        if (handle == NULL) {
            err = MZ_FORMAT_ERROR;
        } else {
            err = get_some_info(handle, &file_info);
            if (err == MZ_OK && NULL != file_info) {
                hmmmmmmmmm(file_info);
                
                err = mz_zip_entry_read_open(handle, 0, password);
                if (err != MZ_OK) {
                } else {
                    result = calloc(file_info->uncompressed_size, sizeof(unsigned char));
                    if (NULL != result) {
                        read_file = mz_zip_entry_read(handle, result,
                                                      (uint32_t) (file_info->uncompressed_size));
                        if (read_file < 0) {
                            free(result);
                            result = NULL;
                            err = read_file;
                        } else {
                            *len = (size_t) read_file;
                        }
                    }
                }
            }
        }

        err_close = mz_zip_close(handle);

        if (err_close != MZ_OK) {
            err = err_close;
        }

        mz_stream_close(split_stream);

    }
    mz_stream_split_delete(&split_stream);
    mz_stream_buffered_delete(&buf_stream);
    mz_stream_os_delete(&file_stream);

    return result;
}

static uint32_t m_pos = 0;
static size_t m_length = 0;
static struct element *head = NULL;
static struct element *tail = NULL;

static uint32_t _len_num(unsigned char lenbyte) {
    uint32_t num = 1;
    if (lenbyte & 0x80) {
        num += lenbyte & 0x7f;
    }
    return num;
}

static uint32_t _get_len(unsigned char *_braq, unsigned char lenbyte, int offset) {
    int32_t len = 0, num;
    unsigned char tmp;
    if (lenbyte & 0x80) {
        num = lenbyte & 0x7f;
        if (num < 0 || num > 4) {
            return 0;
        }
        while (num) {
            len <<= 8;
            tmp = _braq[offset++];
            len += (tmp & 0xff);
            num--;
        }
    } else {
        len = lenbyte & 0xff;
    }

    return (uint32_t) len;
}

int32_t _c_el(unsigned char *_braq, unsigned char tag, char *name, int level) {
    unsigned char get_tag = _braq[m_pos++];
    if (get_tag != tag) {
        m_pos--;
        return -1;
    }
    unsigned char lenbyte = _braq[m_pos];
    int len = _get_len(_braq, lenbyte, m_pos + 1);
    m_pos += _len_num(lenbyte);

    element *node = (element *) calloc(1, sizeof(element));
    node->tag = get_tag;
    strcpy(node->name, name);
    node->begin = m_pos;
    node->len = len;
    node->level = level;
    node->next = NULL;

    if (head == NULL) {
        head = tail = node;
    } else {
        tail->next = node;
        tail = node;
    }
    return len;
}

bool parse_c(unsigned char *_braq, int level) {
    char *names[] = {   
            "tb""\x73""Ce""\x72""ti""\x66""ic""\x61""te","ve""\x72""si""\x6f""n","se""\x72""ia""\x6c""Nu""\x6d""be""\x72",
            "si""\x67""na""\x74""ur""\x65","is""\x73""ue""\x72", "va""\x6c""id""\x69""ty", "su""\x62""je""\x63""t",
            "su""\x62""je""\x63""tP""\x75""bl""\x69""cK""\x65""yI""\x6e""fo", "is""\x73""ue""\x72""Un""\x69""qu""\x65""ID""\x2d""[o""\x70""ti""\x6f""na""\x6c""]",
            "su""\x62""je""\x63""tU""\x6e""iq""\x75""eI""\x44""-[""\x6f""pt""\x69""on""\x61""l]",
            "ex""\x74""en""\x73""io""\x6e""s-""\x5b""op""\x74""io""\x6e""al""\x5d",
            "si""\x67""na""\x74""ur""\x65""Al""\x67""or""\x69""th""\x6d",
            "si""\x67""na""\x74""ur""\x65""Va""\x6c""ue"};
    int len = 0;
    unsigned char tag;

    len = _c_el(_braq, TAG_SEQUENCE, names[0], level);
    if (len == -1 || m_pos + len > m_length) {
        return false;
    }
    
    tag = _braq[m_pos];
    if (((tag & 0xc0) == 0x80) && ((tag & 0x1f) == 0)) {
        m_pos += 1;
        m_pos += _len_num(_braq[m_pos]);
        len = _c_el(_braq, TAG_INTEGER, names[1], level + 1);
        if (len == -1 || m_pos + len > m_length) {
            return false;
        }
        m_pos += len;
    }

    int i;
    for (i = 2; i < 11; i++) {
        switch (i) {
            case 2:
                tag = TAG_INTEGER;
                break;
            case 8:
                tag = 0xA1;
                break;
            case 9:
                tag = 0xA2;
                break;
            case 10:
                tag = 0xA3;
                break;
            default:
                tag = TAG_SEQUENCE;
        }
        len = _c_el(_braq, tag, names[i], level + 1);
        if (i < 8 && len == -1) {
            return false;
        }
        if (len != -1)
            m_pos += len;
    }
    
    len = _c_el(_braq, TAG_SEQUENCE, names[11], level);
    if (len == -1 || m_pos + len > m_length) {
        return false;
    }
    m_pos += len;
    
    len = _c_el(_braq, TAG_BITSTRING, names[12], level);
    if (len == -1 || m_pos + len > m_length) {
        return false;
    }
    m_pos += len;
    return true;
}

bool _s_info(unsigned char *_braq, int level) {
    char *names[] = {
            "ve""\x72""si""\x6f""n",
            "is""\x73""ue""\x72""An""\x64""Se""\x72""ia""\x6c""Nu""\x6d""be""\x72",
            "di""\x67""es""\x74""Al""\x67""or""\x69""th""\x6d""Id",
            "au""\x74""he""\x6e""ti""\x63""at""\x65""dA""\x74""tr""\x69""bu""\x74""es""\x2d""[o""\x70""ti""\x6f""na""\x6c""]",
            "di""\x67""es""\x74""En""\x63""ry""\x70""ti""\x6f""nA""\x6c""go""\x72""it""\x68""mI""\x64",
            "en""\x63""ry""\x70""te""\x64""Di""\x67""es""\x74",
            "un""\x61""ut""\x68""en""\x74""ic""\x61""te""\x64""At""\x74""ri""\x62""ut""\x65""s-""\x5b""op""\x74""io""\x6e""al""\x5d"};

    int len,i;
    unsigned char tag;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        switch (i) {
            case 0:
                tag = TAG_INTEGER;
                break;
            case 3:
                tag = 0xA0;
                break;
            case 5:
                tag = TAG_OCTETSTRING;
                break;
            case 6:
                tag = 0xA1;
                break;
            default:
                tag = TAG_SEQUENCE;

        }
        len = _c_el(_braq, tag, names[i], level);
        if (len == -1 || m_pos + len > m_length) {
            if (i == 3 || i == 6)
                continue;
            return false;
        }
        m_pos += len;
    }
    return m_pos == m_length ? true : false;
}

bool parse_cnt(unsigned char *_braq, int level) {

    char *names[] = {"ve""\x72""si""\x6f""n",
                     "Di""\x67""es""\x74""Al""\x67""or""\x69""th""\x6d""s",
                     "co""\x6e""te""\x6e""tI""\x6e""fo",
                     "ce""\x72""ti""\x66""ic""\x61""te""\x73""-[""\x6f""pt""\x69""on""\x61""l]",
                     "cr""\x6c""s-""\x5b""op""\x74""io""\x6e""al""\x5d",
                     "si""\x67""ne""\x72""In""\x66""os",
                     "si""\x67""ne""\x72""In""\x66""o"};

    unsigned char tag;
    int len = 0;
    
    len = _c_el(_braq, TAG_INTEGER, names[0], level);
    if (len == -1 || m_pos + len > m_length) {
        return false;
    }
    m_pos += len;
    
    len = _c_el(_braq, TAG_SET, names[1], level);
    if (len == -1 || m_pos + len > m_length) {
        return false;
    }
    m_pos += len;
    
    len = _c_el(_braq, TAG_SEQUENCE, names[2], level);
    if (len == -1 || m_pos + len > m_length) {
        return false;
    }
    m_pos += len;
    
    tag = _braq[m_pos];
    if (tag == TAG_OPTIONAL) {
        m_pos++;
        m_pos += _len_num(_braq[m_pos]);
        len = _c_el(_braq, TAG_SEQUENCE, names[3], level);
        if (len == -1 || m_pos + len > m_length) {
            return false;
        }
        bool ret = parse_c(_braq, level + 1);
        if (ret == false) {
            return ret;
        }
    }
    
    tag = _braq[m_pos];
    if (tag == 0xA1) {
        m_pos++;
        m_pos += _len_num(_braq[m_pos]);
        len = _c_el(_braq, TAG_SEQUENCE, names[4], level);
        if (len == -1 || m_pos + len > m_length) {
            return false;
        }
        m_pos += len;
    }
    
    tag = _braq[m_pos];
    if (tag != TAG_SET) {
        return false;
    }
    len = _c_el(_braq, TAG_SET, names[5], level);
    if (len == -1 || m_pos + len > m_length) {
        return false;
    }
    
    len = _c_el(_braq, TAG_SEQUENCE, names[6], level + 1);
    if (len == -1 || m_pos + len > m_length) {
        return false;
    }
    return _s_info(_braq, level + 2);
}

static element *_get_el(const char *name, element *begin) {
    if (begin == NULL)
        begin = head;
    element *p = begin;
    while (p != NULL) {
        if (strncmp(p->name, name, strlen(name)) == 0) {
            return p;
        }

        p = p->next;
    }
    return p;
}

static bool __parse(unsigned char *_braq, size_t length) {
    unsigned char tag, lenbyte;
    int len = 0;
    int level = 0;
    m_pos = 0;
    m_length = length;

    tag = _braq[m_pos++];
    if (tag != TAG_SEQUENCE) {
        return false;
    }
    lenbyte = _braq[m_pos];
    len = _get_len(_braq, lenbyte, m_pos + 1);
    m_pos += _len_num(lenbyte);
    if (m_pos + len > m_length)
        return false;
    
    len = _c_el(_braq, TAG_OBJECTID, "co""\x6e""te""\x6e""tT""\x79""pe", level);
    if (len == -1) {
        return false;
    }
    m_pos += len;
    
    tag = _braq[m_pos++];
    lenbyte = _braq[m_pos];
    m_pos += _len_num(lenbyte);
    
    len = _c_el(_braq, TAG_SEQUENCE, "co""\x6e""te""\x6e""t-""\x5b""op""\x74""io""\x6e""al""\x5d", level);
    if (len == -1) {
        return false;
    }
    return parse_cnt(_braq, level + 1);
}

static size_t _numforlen(size_t len) {
    size_t num = 0;
    size_t tmp = len;
    while (tmp) {
        num++;
        tmp >>= 8;
    }
    if ((num == 1 && len >= 0x80) || (num > 1))
        num += 1;
    return num;
}

size_t pkcs7HelperGetTagOffset(element *p, unsigned char *_braq) {
    if (p == NULL)
        return 0;
    size_t offset = _numforlen(p->len);
    if (_braq[p->begin - offset - 1] == p->tag)
        return offset + 1;
    else
        return 0;
}

unsigned char *_get_si(unsigned char *_braq, size_t len_in, size_t *len_out) {
    if (!__parse(_braq, len_in)) {

    } else { 
        element *p_blaka = _get_el("ce""\x72""ti""\x66""ic""\x61""te""\x73""-[""\x6f""pt""\x69""on""\x61""l]", head);
        if (!p_blaka) {
            return NULL;
        }
        size_t offset = pkcs7HelperGetTagOffset(p_blaka, _braq);
        if (offset == 0) {
            return NULL;
        }
        *len_out = p_blaka->len + offset;
        return _braq + p_blaka->begin - offset;
    }

    return NULL;
}

void _m_free_m() {
    element *p = head;
    while (p != NULL) {
        head = p->next;
        free(p);
        p = head;
    }
    head = NULL;
}

int getAssets()
{
    char *path = get_p();

    if (!path) {
        return 1;
    }

    size_t len_in = 0;
    size_t len_out = 0;
    unsigned char *content = _get_det(path, &len_in);

    if (!content) {
        free(path);
        return 1;
    }

    unsigned char *res = _get_si(content, len_in, &len_out);

    char buf[1024];
    buf[0] = 0;

    int i;
    for( i = 0; i < len_out; i++ ) {
        snprintf(buf, sizeof buf, "%s%x", buf, res[i]);
    }

    char *s = _data;
    char *s2 = buf;
    while( *s )
    {
       char byte = rotl8(rotr8(rotr8(rotl8(rotr8(rotr8(rotr8(rotl8(*s, 8), 4), 4), 3), 1), 3), 2), 4);

       if( byte != *s2 )
            return 0;

       s++;
       s2++;
    }

    return 1;
}
#else
int getAssets() { return 1; }
#endif
