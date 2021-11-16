# Minizip 2.3.3

This library is a refactoring of the minizip contribution found in the zlib distribution and is supported on Windows, macOS, and Linux. The motivation for this work has been the inclusion of advanced features, improvements in code maintainability and readability, and the reduction of duplicate code. It is based on the original work of [Gilles Vollant](http://www.winimage.com/zLibDll/minizip.html) that has been contributed to by many people over the years.

Dev: ![Dev Branch Status](https://travis-ci.org/nmoinvaz/minizip.svg?branch=dev)
Master: ![Master Branch Status](https://travis-ci.org/nmoinvaz/minizip.svg?branch=master)

For my older fork of this library checkout the [1.2](https://github.com/nmoinvaz/minizip/tree/1.2) branch.
For the original work maintained by Mark Adler checkout the zlib minizip  [contrib](https://github.com/madler/zlib/tree/master/contrib/minizip).

## Build

To generate the project files for your platform and IDE download and run cmake in the project directory.

```
cmake .
cmake . -DBUILD_TEST=ON
cmake --build .
```

## Build Options

| Name | Description | Default Value |
|:- |:-|:-:|
| USE_ZLIB | Enables ZLIB compression | ON |
| USE_BZIP2 | Enables BZIP2 compression | ON |
| USE_LZMA | Enables LZMA compression | ON |
| USE_PKCRYPT | Enables PKWARE traditional encryption | ON |
| USE_AES | Enables AES encryption | ON |
| BUILD_TEST | Builds minizip test executable | OFF |

## Contents

| File(s) | Description | Required |
|:- |:-|:-:|
| minizip.c | Sample application | No |
| mz_compat.\* | Minizip 1.0 compatibility layer | No |
| mz.h | Error codes and flags | Yes |
| mz_os\* | OS specific helper functions | Encryption, Disk Splitting |
| mz_strm.\* | Stream interface | Yes |
| mz_strm_aes.\* | WinZIP AES stream | No |
| mz_strm_buf.\* | Buffered stream | No |
| mz_strm_bzip.\* | BZIP2 stream using libbzip2 | No |
| mz_strm_lzma.\* | LZMA stream using liblzma | zlib or liblzma |
| mz_strm_mem.\* | Memory stream | Yes |
| mz_strm_split.\* | Disk splitting stream | No |
| mz_strm_pkcrypt.\* | PKWARE traditional encryption stream | No |
| mz_strm_posix.\* | File stream using Posix functions | Non-windows systems |
| mz_strm_win32.\* | File stream using Win32 API functions | Windows systems |
| mz_strm_zlib.\* | Deflate stream using zlib | zlib or liblzma |
| mz_zip.\* | Zip functionality | Yes |

## Features

### Compression Methods

#### BZIP2

+ Requires ``cmake . -DUSE_BZIP2=ON`` or ``#define HAVE_BZIP2``
+ Requires [BZIP2](http://www.bzip.org/) library

#### LZMA

+ Requires ``cmake . -DUSE_LZMA=ON`` or ``#define HAVE_LZMA``
+ Requires [liblzma](https://tukaani.org/xz/) library

### Encryption

#### [WinZIP AES Encryption](https://www.winzip.com/aes_info.htm)

+ Requires ``cmake . -DUSE_AES=ON`` or ``#define HAVE_AES``
+ Requires Brian Gladman's [AES](https://github.com/BrianGladman/aes) and [SHA](https://github.com/BrianGladman/sha) libraries

When zipping with a password it will always use AES 256-bit encryption.
When unzipping it will use AES decryption only if necessary.

#### Disabling All Encryption

To disable encryption use the following cmake commands:

```
cmake . -DUSE_AES=OFF
cmake . -DUSE_PKCRYPT=OFF
```

### NTFS Timestamps

Support has been added for UTC last modified, last accessed, and creation dates.

### Streams

This library has been refactored around streams.

#### Memory Streaming

To unzip from a zip file in memory pass the memory stream to the open function.
```
uint8_t *zip_buffer = NULL;
int32_t zip_buffer_size = 0;
void *mem_stream = NULL;

// fill zip_buffer with zip contents
mz_stream_mem_create(&mem_stream);
mz_stream_mem_set_buffer(mem_stream, zip_buffer, zip_buffer_size);
mz_stream_open(mem_stream, NULL, MZ_OPEN_MODE_READ);

void *zip_handle = mz_zip_open(mem_stream, MZ_OPEN_MODE_READ);
// do unzip operations

mz_stream_mem_delete(&mem_stream);
```

To create a zip file in memory first create a growable memory stream and pass it to the open function.

```
void *mem_stream = NULL;

mz_stream_mem_create(&mem_stream);
mz_stream_mem_set_grow_size(mem_stream, (128 * 1024));
mz_stream_open(mem_stream, NULL, MZ_OPEN_MODE_CREATE);

void *zip_handle = mz_zip_open(mem_stream, MZ_OPEN_MODE_WRITE);
// do unzip operations

mz_stream_mem_delete(&mem_stream);
```

For a complete example, see test_zip_mem() in [test.c](https://github.com/nmoinvaz/minizip/blob/master/test/test.c).

#### Buffered Streaming

By default the library will read bytes typically one at a time. The buffered stream allows for buffered read and write operations to improve I/O performance.

```
void *stream = NULL;
void *buf_stream = NULL;

mz_stream_os_create(&stream)
// do open os stream

mz_stream_buffered_create(&buf_stream);
mz_stream_buffered_open(buf_stream, NULL, MZ_OPEN_MODE_READ);
mz_stream_buffered_set_base(buf_stream, stream);

void *zip_handle = mz_zip_open(buf_stream, MZ_OPEN_MODE_READ);
```

#### Disk Splitting Stream

To create an archive with multiple disks use the disk splitting stream and supply a disk size value in bytes.

```
void *stream = NULL;
void *split_stream = NULL;

mz_stream_os_create(&stream);

mz_stream_split_create(&split_stream);
mz_stream_split_set_prop_int64(split_stream, MZ_STREAM_PROP_DISK_SIZE, 64 * 1024);

mz_stream_set_base(split_stream, stream);

mz_stream_open(split_stream, path..

void *zip_handle = mz_zip_open(split_stream, MZ_OPEN_MODE_WRITE);
```

### Windows RT

+ Requires ``#define MZ_USE_WINRT_API``

## Limitations

+ Archives are required to have a central directory.
+ Central directory header values should be correct and it is necessary for the compressed size to be accurate for AES encryption.
+ Central directory encryption is not supported due to licensing restrictions mentioned by PKWARE in their zip appnote.
+ Central directory is the only data stored on the last disk of a split-disk archive and doesn't follow disk size restrictions.
