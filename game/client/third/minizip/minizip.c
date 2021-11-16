/* minizip.c
   Version 2.3.3, June 10, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip
   Copyright (C) 2009-2010 Mathias Svensson
     Modifications for Zip64 support
     http://result42.com
   Copyright (C) 2007-2008 Even Rouault
     Modifications of Unzip for Zip64
   Copyright (C) 1998-2010 Gilles Vollant
     http://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>

#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"
#include "mz_strm_split.h"
#include "mz_zip.h"

/***************************************************************************/

void minizip_banner(void)
{
    printf("Minizip %s - https://github.com/nmoinvaz/minizip\n", MZ_VERSION);
    printf("---------------------------------------------------\n");
}

void minizip_help(void)
{
    printf("Usage : minizip [-x -d dir|-l] [-o] [-a] [-j] [-0 to -9] [-b|-m] [-k 512] [-p pwd] [-s] file.zip [files]\n\n" \
           "  -x  Extract files\n" \
           "  -l  List files\n" \
           "  -d  Destination directory\n" \
           "  -o  Overwrite existing files\n" \
           "  -a  Append to existing zip file\n" \
           "  -u  Buffered reading and writing\n" \
           "  -i  Include full path of files\n" \
           "  -0  Store only\n" \
           "  -1  Compress faster\n" \
           "  -9  Compress better\n" \
           "  -k  Disk size in KB\n" \
           "  -p  Encryption password\n");
#ifdef HAVE_AES
    printf("  -s  AES encryption\n");
#endif
#ifdef HAVE_BZIP2
    printf("  -b  BZIP2 compression\n");
#endif
#ifdef HAVE_LZMA
    printf("  -m  LZMA compression\n");
#endif
    printf("\n");
}

/***************************************************************************/

typedef struct minizip_opt_s {
    int16_t include_path;
    int16_t compress_level;
    int16_t compress_method;
    int16_t overwrite;
#ifdef HAVE_AES
    int16_t aes;
#endif
} minizip_opt;

/***************************************************************************/

int32_t minizip_add_path(void *handle, const char *path, const char *filenameinzip, const char *password, int16_t is_dir, minizip_opt *options)
{
    mz_zip_file file_info;
    int32_t read = 0;
    int32_t written = 0;
    int32_t err = MZ_OK;
    int32_t err_close = MZ_OK;
    void *stream = NULL;
    char buf[INT16_MAX];


    memset(&file_info, 0, sizeof(file_info));

    // The path name saved, should not include a leading slash.
    // If it did, windows/xp and dynazip couldn't read the zip file.

    if (filenameinzip == NULL)
        filenameinzip = path;
    while (filenameinzip[0] == '\\' || filenameinzip[0] == '/')
        filenameinzip += 1;

    // Get information about the file on disk so we can store it in zip
    printf("Adding: %s\n", filenameinzip);

    file_info.version_madeby = MZ_VERSION_MADEBY;
    file_info.compression_method = options->compress_method;
    file_info.filename = filenameinzip;
    file_info.uncompressed_size = mz_os_get_file_size(path);

#ifdef HAVE_AES
    if (options->aes)
        file_info.aes_version = MZ_AES_VERSION;
#endif

    mz_os_get_file_date(path, &file_info.modified_date, &file_info.accessed_date,
        &file_info.creation_date);
    mz_os_get_file_attribs(path, &file_info.external_fa);

    // Add to zip
    err = mz_zip_entry_write_open(handle, &file_info, options->compress_level, password);
    if (err != MZ_OK)
    {
        printf("Error in opening %s in zip file (%d)\n", filenameinzip, err);
        return err;
    }

    if (!is_dir)
    {
        mz_stream_os_create(&stream);

        err = mz_stream_os_open(stream, path, MZ_OPEN_MODE_READ);

        if (err == MZ_OK)
        {
            // Read contents of file and write it to zip
            do
            {
                read = mz_stream_os_read(stream, buf, sizeof(buf));
                if (read < 0)
                {
                    err = mz_stream_os_error(stream);
                    printf("Error %d in reading %s\n", err, filenameinzip);
                    break;
                }
                if (read == 0)
                    break;

                written = mz_zip_entry_write(handle, buf, read);
                if (written != read)
                {
                    err = mz_stream_os_error(stream);
                    printf("Error in writing %s in the zip file (%d)\n", filenameinzip, err);
                    break;
                }
            } while (err == MZ_OK);

            mz_stream_os_close(stream);
        }
        else
        {
            printf("Error in opening %s for reading\n", path);
        }

        mz_stream_os_delete(&stream);
    }

    err_close = mz_zip_entry_close(handle);
    if (err_close != MZ_OK)
    {
        printf("Error in closing %s in the zip file (%d)\n", filenameinzip, err_close);
        err = err_close;
    }

    return err;
}

int32_t minizip_add(void *handle, const char *path, const char *root_path, const char *password, minizip_opt *options, uint8_t recursive)
{
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    int32_t err =  MZ_OK;
    int16_t is_dir = 0;
    char full_path[320];
    const char *filename = NULL;
    const char *filenameinzip = path;


    if (mz_os_is_dir(path) == MZ_OK)
        is_dir = 1;

    // Construct the filename that our file will be stored in the zip as
    if (root_path == NULL)
        root_path = path;

    // Should the file be stored with any path info at all?
    if (!options->include_path)
    {
        if (!is_dir && root_path == path)
        {
            if (mz_path_get_filename(filenameinzip, &filename) == MZ_OK)
                filenameinzip = filename;
        }
        else
        {
            filenameinzip += strlen(root_path);
        }
    }

    if (*filenameinzip != 0)
        err = minizip_add_path(handle, path, filenameinzip, password, is_dir, options);

    if (!is_dir)
        return err;

    dir = mz_os_open_dir(path);

    if (dir == NULL)
    {
        printf("Cannot enumerate directory %s\n", path);
        return MZ_EXIST_ERROR;
    }

    while ((entry = mz_os_read_dir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        full_path[0] = 0;
        mz_path_combine(full_path, path, sizeof(full_path));
        mz_path_combine(full_path, entry->d_name, sizeof(full_path));

        if (!recursive && mz_os_is_dir(full_path))
            continue;

        err = minizip_add(handle, full_path, root_path, password, options, recursive);
        if (err != MZ_OK)
            return err;
    }

    mz_os_close_dir(dir);
    return MZ_OK;
}

/***************************************************************************/

int32_t minizip_list(void *handle)
{
    mz_zip_file *file_info = NULL;
    uint32_t ratio = 0;
    int16_t level = 0;
    int32_t err = MZ_OK;
    struct tm tmu_date;
    const char *string_method = NULL;
    char crypt = ' ';


    err = mz_zip_goto_first_entry(handle);

    if (err != MZ_OK && err != MZ_END_OF_LIST)
    {
        printf("Error %d going to first entry in zip file\n", err);
        return err;
    }

    printf("  Length  Method     Size  Attribs Ratio   Date    Time   CRC-32     Name\n");
    printf("  ------  ------     ----  ------- -----   ----    ----   ------     ----\n");

    while (err == MZ_OK)
    {
        err = mz_zip_entry_get_info(handle, &file_info);

        if (err != MZ_OK)
        {
            printf("Error %d getting entry info in zip file\n", err);
            break;
        }

        ratio = 0;
        if (file_info->uncompressed_size > 0)
            ratio = (uint32_t)((file_info->compressed_size * 100) / file_info->uncompressed_size);

        // Display a '*' if the file is encrypted
        if (file_info->flag & MZ_ZIP_FLAG_ENCRYPTED)
            crypt = '*';

        switch (file_info->compression_method)
        {
        case MZ_COMPRESS_METHOD_RAW:
            string_method = "Stored";
            break;
        case MZ_COMPRESS_METHOD_DEFLATE:
            level = (int16_t)((file_info->flag & 0x6) / 2);
            if (level == 0)
                string_method = "Defl:N";
            else if (level == 1)
                string_method = "Defl:X";
            else if ((level == 2) || (level == 3))
                string_method = "Defl:F"; // 2: fast , 3: extra fast
            else
                string_method = "Defl:?";
            break;
        case MZ_COMPRESS_METHOD_BZIP2:
            string_method = "BZip2";
            break;
        case MZ_COMPRESS_METHOD_LZMA:
            string_method = "LZMA";
            break;
        default:
            string_method = "Unknwn";
        }

        mz_zip_time_t_to_tm(file_info->modified_date, &tmu_date);

        printf(" %7"PRIu64"  %6s%c %7"PRIu64" %8"PRIx32" %3"PRIu32"%%  %2.2"PRIu32"-%2.2"PRIu32\
               "-%2.2"PRIu32"  %2.2"PRIu32":%2.2"PRIu32"  %8.8"PRIx32"   %s\n",
                file_info->uncompressed_size, string_method, crypt,
                file_info->compressed_size, file_info->external_fa, ratio,
                (uint32_t)tmu_date.tm_mon + 1, (uint32_t)tmu_date.tm_mday,
                (uint32_t)tmu_date.tm_year % 100,
                (uint32_t)tmu_date.tm_hour, (uint32_t)tmu_date.tm_min,
                file_info->crc, file_info->filename);

        err = mz_zip_goto_next_entry(handle);

        if (err != MZ_OK && err != MZ_END_OF_LIST)
        {
            printf("Error %d going to next entry in zip file\n", err);
            return err;
        }
    }

    if (err == MZ_END_OF_LIST)
        return MZ_OK;

    return err;
}

/***************************************************************************/

int32_t minizip_extract_currentfile(void *handle, const char *destination, const char *password, minizip_opt *options)
{
    mz_zip_file *file_info = NULL;
    uint8_t buf[INT16_MAX];
    int32_t read = 0;
    int32_t written = 0;
    int32_t err = MZ_OK;
    int32_t err_close = MZ_OK;
    void *stream = NULL;
    const char *filename = NULL;
    char out_path[512];
    char directory[512];


    err = mz_zip_entry_get_info(handle, &file_info);

    if (err != MZ_OK)
    {
        printf("Error %d getting entry info in zip file\n", err);
        return err;
    }

    if (mz_path_get_filename(file_info->filename, &filename) != MZ_OK)
        filename = file_info->filename;

    // Construct output path
    out_path[0] = 0;
    if ((*file_info->filename == '/') || (file_info->filename[1] == ':'))
    {
        strncpy(out_path, file_info->filename, sizeof(out_path));
    }
    else
    {
        if (destination != NULL)
            mz_path_combine(out_path, destination, sizeof(out_path));
        mz_path_combine(out_path, file_info->filename, sizeof(out_path));
    }

    // If zip entry is a directory then create it on disk
    if (mz_zip_attrib_is_dir(file_info->external_fa, file_info->version_madeby) == MZ_OK)
    {
        printf("Creating directory: %s\n", out_path);
        mz_make_dir(out_path);
        return err;
    }

    err = mz_zip_entry_read_open(handle, 0, password);

    if (err != MZ_OK)
    {
        printf("Error %d opening entry in zip file\n", err);
        return err;
    }

    // Determine if the file should be overwritten or not and ask the user if needed
    if ((err == MZ_OK) && (options->overwrite == 0) && (mz_os_file_exists(out_path) == MZ_OK))
    {
        char rep = 0;
        do
        {
            char answer[128];
            printf("The file %s exists. Overwrite ? [y]es, [n]o, [A]ll: ", out_path);
            if (scanf("%1s", answer) != 1)
                exit(EXIT_FAILURE);
            rep = answer[0];
            if ((rep >= 'a') && (rep <= 'z'))
                rep -= 0x20;
        }
        while ((rep != 'Y') && (rep != 'N') && (rep != 'A'));

        if (rep == 'N')
            return err;
        if (rep == 'A')
            options->overwrite = 1;
    }

    mz_stream_os_create(&stream);

    // Create the file on disk so we can unzip to it
    if (err == MZ_OK)
    {
        // Some zips don't contain directory alone before file
        if ((mz_stream_os_open(stream, out_path, MZ_OPEN_MODE_CREATE) != MZ_OK) &&
            (filename != file_info->filename))
        {
            // Create the directory of the output path
            strncpy(directory, out_path, sizeof(directory));

            mz_path_remove_filename(directory);
            mz_make_dir(directory);

            mz_stream_os_open(stream, out_path, MZ_OPEN_MODE_CREATE);
        }
    }

    // Read from the zip, unzip to buffer, and write to disk
    if (mz_stream_os_is_open(stream) == MZ_OK)
    {
        printf(" Extracting: %s\n", out_path);
        while (1)
        {
            read = mz_zip_entry_read(handle, buf, sizeof(buf));
            if (read < 0)
            {
                err = read;
                printf("Error %d reading entry in zip file\n", err);
                break;
            }
            if (read == 0)
                break;
            written = mz_stream_os_write(stream, buf, read);
            if (written != read)
            {
                err = mz_stream_os_error(stream);
                printf("Error %d in writing extracted file\n", err);
                break;
            }
        }

        mz_stream_os_close(stream);

        // Set the time of the file that has been unzipped
        if (err == MZ_OK)
            mz_os_set_file_date(out_path, file_info->modified_date, file_info->accessed_date,
                file_info->creation_date);
    }
    else
    {
        printf("Error opening %s\n", out_path);
    }

    mz_stream_os_delete(&stream);

    err_close = mz_zip_entry_close(handle);
    if (err_close != MZ_OK)
    {
        printf("Error %d closing entry in zip file\n", err_close);
        err = err_close;
    }

    return err;
}

int32_t minizip_extract_all(void *handle, const char *destination, const char *password, minizip_opt *options)
{
    int32_t err = MZ_OK;

    err = mz_zip_goto_first_entry(handle);

    if (err == MZ_END_OF_LIST)
        return MZ_OK;
    if (err != MZ_OK)
        printf("Error %d going to first entry in zip file\n", err);

    while (err == MZ_OK)
    {
        err = minizip_extract_currentfile(handle, destination, password, options);

        if (err != MZ_OK)
            break;

        err = mz_zip_goto_next_entry(handle);

        if (err == MZ_END_OF_LIST)
            return MZ_OK;
        if (err != MZ_OK)
            printf("Error %d going to next entry in zip file\n", err);
    }

    return err;
}

int32_t minizip_extract_onefile(void *handle, const char *filename, const char *destination, const char *password, minizip_opt *options)
{
    int32_t err = mz_zip_locate_entry(handle, filename, NULL);

    if (err != MZ_OK)
    {
        printf("File %s not found in the zip file\n", filename);
        return err;
    }

    return minizip_extract_currentfile(handle, destination, password, options);
}

/***************************************************************************/

#ifndef NOMAIN
int main(int argc, char *argv[])
{
    void *handle = NULL;
    void *file_stream = NULL;
    void *split_stream = NULL;
    void *buf_stream = NULL;
    char *path = NULL;
    char *password = NULL;
    char *destination = NULL;
    char *filename_to_extract = NULL;
    minizip_opt options;
    int64_t disk_size = 0;
    int32_t path_arg = 0;
    uint8_t do_list = 0;
    uint8_t do_extract = 0;
    uint8_t buffered = 0;
    int16_t mode = 0;
    uint8_t append = 0;
    int32_t err_close = 0;
    int32_t err = 0;
    int32_t i = 0;

    minizip_banner();
    if (argc == 1)
    {
        minizip_help();
        return 0;
    }

    memset(&options, 0, sizeof(options));

    options.compress_method = MZ_COMPRESS_METHOD_DEFLATE;
    options.compress_level = MZ_COMPRESS_LEVEL_DEFAULT;

    // Parse command line options
    for (i = 1; i < argc; i += 1)
    {
        if ((*argv[i]) == '-')
        {
            const char *p = argv[i] + 1;

            while ((*p) != '\0')
            {
                char c = *(p++);
                if ((c == 'l') || (c == 'L'))
                    do_list = 1;
                if ((c == 'x') || (c == 'X'))
                    do_extract = 1;
                if ((c == 'a') || (c == 'A'))
                    append = 1;
                if ((c == 'u') || (c == 'U'))
                    buffered = 1;
                if ((c == 'o') || (c == 'O'))
                    options.overwrite = 1;
                if ((c == 'i') || (c == 'I'))
                    options.include_path = 1;
                if ((c >= '0') && (c <= '9'))
                {
                    options.compress_level = (c - '0');
                    if (options.compress_level == 0)
                        options.compress_method = MZ_COMPRESS_METHOD_RAW;
                }

#ifdef HAVE_BZIP2
                if ((c == 'b') || (c == 'B'))
                    options.compress_method = MZ_COMPRESS_METHOD_BZIP2;
#endif
#ifdef HAVE_LZMA
                if ((c == 'm') || (c == 'M'))
                    options.compress_method = MZ_COMPRESS_METHOD_LZMA;
#endif
#ifdef HAVE_AES
                if ((c == 's') || (c == 'S'))
                    options.aes = 1;
#endif
                if (((c == 'k') || (c == 'K')) && (i + 1 < argc))
                {
                    disk_size = atoi(argv[i + 1]) * 1024;
                    i += 1;
                }
                if (((c == 'd') || (c == 'D')) && (i + 1 < argc))
                {
                    destination = argv[i + 1];
                    i += 1;
                }
                if (((c == 'p') || (c == 'P')) && (i + 1 < argc))
                {
                    password = argv[i + 1];
                    i += 1;
                }
            }

            continue;
        }

        if (path_arg == 0)
            path_arg = i;
    }

    if (path_arg == 0)
    {
        minizip_help();
        return 0;
    }

    path = argv[path_arg];

    mode = MZ_OPEN_MODE_READ;

    if ((do_list == 0) && (do_extract == 0))
    {
        mode |= MZ_OPEN_MODE_WRITE;

        if (mz_os_file_exists(path) != MZ_OK)
        {
            // If the file doesn't exist, we don't append file
            mode |= MZ_OPEN_MODE_CREATE;
        }
        else if (append == 1)
        {
            mode |= MZ_OPEN_MODE_APPEND;
        }
        else if (options.overwrite == 0)
        {
            // If ask the user what to do because append and overwrite args not set
            char rep = 0;
            do
            {
                char answer[128];
                printf("The file %s exists. Overwrite ? [y]es, [n]o, [a]ppend : ", path);
                if (scanf("%1s", answer) != 1)
                    exit(EXIT_FAILURE);
                rep = answer[0];

                if ((rep >= 'a') && (rep <= 'z'))
                    rep -= 0x20;
            }
            while ((rep != 'Y') && (rep != 'N') && (rep != 'A'));

            if (rep == 'A')
            {
                mode |= MZ_OPEN_MODE_APPEND;
            }
            else if (rep == 'Y')
            {
                mode |= MZ_OPEN_MODE_CREATE;
            }
            else if (rep == 'N')
            {
                minizip_help();
                return 0;
            }
        }
    }

    mz_stream_os_create(&file_stream);
    mz_stream_buffered_create(&buf_stream);
    mz_stream_split_create(&split_stream);

    if (buffered)
    {
        mz_stream_set_base(buf_stream, file_stream);
        mz_stream_set_base(split_stream, buf_stream);
    }
    else
    {
        mz_stream_set_base(split_stream, file_stream);
    }

    mz_stream_split_set_prop_int64(split_stream, MZ_STREAM_PROP_DISK_SIZE, disk_size);

    err = mz_stream_open(split_stream, path, mode);

    if (err != MZ_OK)
    {
        printf("Error opening file %s\n", path);
    }
    else
    {
        handle = mz_zip_open(split_stream, mode);

        if (handle == NULL)
        {
            printf("Error opening zip %s\n", path);
            err = MZ_FORMAT_ERROR;
        }

        if (do_list)
        {
            err = minizip_list(handle);
        }
        else if (do_extract)
        {
            // Create target directory if it doesn't exist
            if (destination != NULL)
                mz_make_dir(destination);

            if (argc > path_arg + 1)
                filename_to_extract = argv[path_arg + 1];

            if (filename_to_extract == NULL)
                err = minizip_extract_all(handle, destination, password, &options);
            else
                err = minizip_extract_onefile(handle, filename_to_extract, destination, password, &options);
        }
        else
        {
            printf("Creating %s\n", path);

            // Go through command line args looking for files to add to zip
            for (i = path_arg + 1; (i < argc) && (err == MZ_OK); i += 1)
                err = minizip_add(handle, argv[i], NULL, password, &options, 1);

            mz_zip_set_version_madeby(handle, MZ_VERSION_MADEBY);
        }

        err_close = mz_zip_close(handle);

        if (err_close != MZ_OK)
        {
            printf("Error in closing %s (%d)\n", path, err_close);
            err = err_close;
        }

        mz_stream_close(split_stream);
    }

    mz_stream_split_delete(&split_stream);
    mz_stream_buffered_delete(&buf_stream);
    mz_stream_os_delete(&file_stream);

    return err;
}
#endif
