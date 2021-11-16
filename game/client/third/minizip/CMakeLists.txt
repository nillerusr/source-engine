#***************************************************************************
# Copyright:  Matthias Schmieder,
# E-Mail:     schmieder.matthias@gmail.com
# Year:       2016
#***************************************************************************
cmake_minimum_required(VERSION 2.8)

option(USE_ZLIB "Enables ZLIB compression" ON)
option(USE_BZIP2 "Enables BZIP2 compression" ON)
option(USE_LZMA "Enables LZMA compression" ON)
option(USE_PKCRYPT "Enables PKWARE traditional encryption" ON)
option(USE_AES "Enables AES encryption" ON)
option(BUILD_TEST "Builds minizip test executable" OFF)

# Set a consistent MACOSX_RPATH default across all CMake versions.
# When CMake 2.8.12 is required, change this default to 1.
# When CMake 3.0.0 is required, remove this block (see CMP0042).
if(NOT DEFINED CMAKE_MACOSX_RPATH)
    set(CMAKE_MACOSX_RPATH 0)
endif()

project("minizip")

include(GNUInstallDirs)

set(INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Installation directory for executables")
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE PATH "Installation directory for headers")
set(INSTALL_MAN_DIR ${CMAKE_INSTALL_MANDIR} CACHE PATH "Installation directory for manual pages")
set(INSTALL_PKGCONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig CACHE PATH "Installation directory for pkgconfig (.pc) files")
set(INSTALL_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/minizip CACHE PATH "Installation directory for cmake files.")

set(VERSION "2.3.3")

# Set cmake debug postfix to d
set(CMAKE_DEBUG_POSTFIX "d")

# Ensure correct version of zlib is referenced
if(USE_ZLIB)
    set(ZLIB_ROOT ${DEF_ZLIB_ROOT} CACHE PATH "Parent directory of zlib installation")
    find_package(ZLIB REQUIRED)
    if(ZLIB_FOUND)
        include_directories(${ZLIB_INCLUDE_DIRS})
    endif()
    add_definitions(-DHAVE_ZLIB)
endif()

set(MINIZIP_PC ${CMAKE_CURRENT_BINARY_DIR}/minizip.pc)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/minizip.pc.cmakein ${MINIZIP_PC} @ONLY)

set(PROJECT_NAME libminizip)

set(MINIZIP_SRC
    mz_os.c
    mz_compat.c
    mz_strm.c
    mz_strm_buf.c
    mz_strm_mem.c
    mz_strm_posix.c
    mz_strm_split.c
    mz_zip.c)

set(MINIZIP_PUBLIC_HEADERS
    mz.h
    mz_os.h
    mz_compat.h
    mz_strm.h
    mz_strm_buf.h
    mz_strm_mem.h
    mz_strm_posix.h
    mz_strm_split.h
    mz_zip.h)

if(WIN32)
    list(APPEND MINIZIP_SRC "mz_os_win32.c" "mz_strm_win32.c")
    list(APPEND MINIZIP_PUBLIC_HEADERS "mz_os_win32.h" "mz_strm_win32.h")

    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsStore")
    add_definitions(-DMZ_USE_WINRT_API)
endif()

if(UNIX)
    add_compile_options(-O3)

    list(APPEND MINIZIP_SRC "mz_os_posix.c")
    list(APPEND MINIZIP_PUBLIC_HEADERS "mz_os_posix.h")

    set(define_lfs_macros TRUE)

    if(ANDROID)
        string(REGEX REPLACE "android-([0-9+])" "\\1"
            android_api "${ANDROID_PLATFORM}")
        if(${android_api} LESS 24)
            set(define_lfs_macros FALSE)
        endif()
    endif()

    if(define_lfs_macros)
        add_definitions(-D__USE_FILE_OFFSET64)
        add_definitions(-D__USE_LARGEFILE64)
        add_definitions(-D_LARGEFILE64_SOURCE)
        add_definitions(-D_FILE_OFFSET_BITS=64)
    endif()

    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        find_package(PkgConfig REQUIRED)

        pkg_check_modules(LIBBSD libbsd REQUIRED)

        include_directories(${LIBBSD_INCLUDE_DIRS})
        link_directories(${LIBBSD_LIBRARY_DIRS})
    endif()
endif()

if(USE_PKCRYPT)
    add_definitions(-DHAVE_PKCRYPT)

    list(APPEND MINIZIP_SRC "mz_strm_pkcrypt.c")
    list(APPEND MINIZIP_PUBLIC_HEADERS "mz_strm_pkcrypt.h")
endif()

if(USE_AES)
    add_definitions(-DHAVE_AES)

    list(APPEND MINIZIP_SRC "mz_strm_aes.c")
    list(APPEND MINIZIP_PUBLIC_HEADERS "mz_strm_aes.h")

    set(AES_SRC
        lib/aes/aescrypt.c
        lib/aes/aeskey.c
        lib/aes/aestab.c
        lib/aes/hmac.c
        lib/aes/pwd2key.c
        lib/aes/sha1.c)

    set(AES_PUBLIC_HEADERS
        lib/aes/aes.h
        lib/aes/aesopt.h
        lib/aes/aestab.h
        lib/aes/brg_endian.h
        lib/aes/brg_types.h
        lib/aes/hmac.h
        lib/aes/pwd2key.h
        lib/aes/sha1.h)

    include_directories(lib/aes)

    source_group("AES" FILES ${AES_SRC} ${AES_PUBLIC_HEADERS})
endif()

if(USE_ZLIB)
    add_definitions(-DHAVE_ZLIB)

    list(APPEND MINIZIP_SRC "mz_strm_zlib.c")
    list(APPEND MINIZIP_PUBLIC_HEADERS "mz_strm_zlib.h")

    include(CheckFunctionExists)
    set(CMAKE_REQUIRED_LIBRARIES ZLIB::ZLIB)
    CHECK_FUNCTION_EXISTS(z_get_crc_table
        NEEDS_Z_PREFIX)

    if(NEEDS_Z_PREFIX)
        add_definitions(-DZ_PREFIX)
    endif()
endif()

if(USE_BZIP2)
    add_definitions(-DHAVE_BZIP2)
    add_definitions(-DBZ_NO_STDIO)

    list(APPEND MINIZIP_SRC "mz_strm_bzip.c")
    list(APPEND MINIZIP_PUBLIC_HEADERS "mz_strm_bzip.h")

    set(BZIP2_SRC
        lib/bzip2/blocksort.c
        lib/bzip2/bzlib.c
        lib/bzip2/compress.c
        lib/bzip2/crctable.c
        lib/bzip2/decompress.c
        lib/bzip2/huffman.c
        lib/bzip2/randtable.c)

    set(BZIP2_PUBLIC_HEADERS
        lib/bzip2/bzlib.h
        lib/bzip2/bzlib_private.h)

    include_directories(lib/bzip2)

    source_group("BZip2" FILES ${BZIP2_SRC} ${BZIP2_PUBLIC_HEADERS})
endif()

if(USE_LZMA)
    add_definitions(-DHAVE_LZMA)
    add_definitions(-DHAVE_CONFIG_H)
    add_definitions(-DLZMA_API_STATIC)

    list(APPEND MINIZIP_SRC "mz_strm_lzma.c")
    list(APPEND MINIZIP_PUBLIC_HEADERS "mz_strm_lzma.h")

    set(LZMA_CHECK_SRC
        lib/liblzma/check/check.c
        lib/liblzma/check/crc32_fast.c
        lib/liblzma/check/crc32_table.c)
    set(LZMA_COMMON_SRC
        lib/liblzma/common/alone_decoder.c
        lib/liblzma/common/alone_encoder.c
        lib/liblzma/common/common.c
        lib/liblzma/common/filter_encoder.c)
    set(LZMA_LZ_SRC
        lib/liblzma/lz/lz_decoder.c
        lib/liblzma/lz/lz_encoder.c
        lib/liblzma/lz/lz_encoder_mf.c)
    set(LZMA_LZMA_SRC
        lib/liblzma/lzma/fastpos.h
        lib/liblzma/lzma/fastpos_table.c
        lib/liblzma/lzma/lzma_decoder.c
        lib/liblzma/lzma/lzma_encoder.c
        lib/liblzma/lzma/lzma_encoder_optimum_fast.c
        lib/liblzma/lzma/lzma_encoder_optimum_normal.c
        lib/liblzma/lzma/lzma_encoder_presets.c)
    set(LZMA_RANGECODER_SRC
        lib/liblzma/rangecoder/price_table.c)

    set(LZMA_CONFIG_HEADERS
        lib/liblzma/config.h)
    set(LZMA_API_HEADERS
        lib/liblzma/api/lzma.h
        lib/liblzma/api/lzma/base.h
        lib/liblzma/api/lzma/check.h
        lib/liblzma/api/lzma/container.h
        lib/liblzma/api/lzma/filter.h
        lib/liblzma/api/lzma/lzma12.h
        lib/liblzma/api/lzma/version.h
        lib/liblzma/api/lzma/vli.h)
    set(LZMA_CHECK_HEADERS
        lib/liblzma/check/check.h
        lib/liblzma/check/crc32_table_be.h
        lib/liblzma/check/crc32_table_le.h
        lib/liblzma/check/crc_macros.h)
    set(LZMA_COMMON_HEADERS
        lib/liblzma/common/alone_decoder.h
        lib/liblzma/common/common.h
        lib/liblzma/common/filter_encoder.h
        lib/liblzma/common/index.h
        lib/liblzma/common/memcmplen.h
        lib/liblzma/common/sysdefs.h
        lib/liblzma/common/tuklib_common.h
        lib/liblzma/common/tuklib_config.h
        lib/liblzma/common/tuklib_integer.h)
    set(LZMA_LZ_HEADERS
        lib/liblzma/lz/lz_decoder.h
        lib/liblzma/lz/lz_encoder.h
        lib/liblzma/lz/lz_encoder_hash.h
        lib/liblzma/lz/lz_encoder_hash_table.h)
    set(LZMA_LZMA_HEADERS
        lib/liblzma/lzma/lzma2_encoder.h
        lib/liblzma/lzma/lzma_common.h
        lib/liblzma/lzma/lzma_decoder.h
        lib/liblzma/lzma/lzma_encoder.h
        lib/liblzma/lzma/lzma_encoder_private.h)
    set(LZMA_RANGECODER_HEADERS
        lib/liblzma/rangecoder/price.h
        lib/liblzma/rangecoder/range_common.h
        lib/liblzma/rangecoder/range_decoder.h
        lib/liblzma/rangecoder/range_encoder.h)

    set(LZMA_PUBLIC_HEADERS
        ${LZMA_CONFIG_HEADERS}
        ${LZMA_API_HEADERS}
        ${LZMA_CHECK_HEADERS}
        ${LZMA_COMMON_HEADERS}
        ${LZMA_LZ_HEADERS}
        ${LZMA_LZMA_HEADERS}
        ${LZMA_RANGECODER_HEADERS})

    set(LZMA_SRC
        ${LZMA_CHECK_SRC}
        ${LZMA_COMMON_SRC}
        ${LZMA_LZ_SRC}
        ${LZMA_LZMA_SRC}
        ${LZMA_RANGECODER_SRC})

    include_directories(lib/liblzma
                        lib/liblzma/api
                        lib/liblzma/check
                        lib/liblzma/common
                        lib/liblzma/lz
                        lib/liblzma/lzma
                        lib/liblzma/rangecoder)

    source_group("LZMA" FILES ${LZMA_CONFIG_HEADERS})
    source_group("LZMA\\API" FILES ${LZMA_API_HEADERS})
    source_group("LZMA\\Check" FILES ${LZMA_CHECK_SRC} ${LZMA_CHECK_HEADERS})
    source_group("LZMA\\Common" FILES ${LZMA_COMMON_SRC} ${LZMA_COMMON_HEADERS})
    source_group("LZMA\\LZ" FILES ${LZMA_LZ_SRC} ${LZMA_LZ_HEADERS})
    source_group("LZMA\\LZMA" FILES ${LZMA_LZMA_SRC} ${LZMA_LZMA_HEADERS})
    source_group("LZMA\\RangeCoder" FILES ${LZMA_RANGECODER_SRC} ${LZMA_RANGECODER_HEADERS})
endif()

# Enable x86 optimizations if supported
if(CMAKE_C_COMPILER MATCHES ".*clang")
    include(CheckCCompilerFlag)
    macro(enable_option_if_supported option variable)
        check_c_compiler_flag("-Werror=unused-command-line-argument ${option}" ${variable})
        if(${variable})
            add_compile_options(${option})
        endif()
    endmacro()

    enable_option_if_supported(-msse3 check_opt_sse3)
    enable_option_if_supported(-msse4.1 check_opt_sse41)
    enable_option_if_supported(-maes check_opt_aes)
endif()

# Create minizip library
source_group("Minizip" FILES ${MINIZIP_SRC} ${MINIZIP_PUBLIC_HEADERS})

add_library(${PROJECT_NAME}
                ${MINIZIP_SRC} ${MINIZIP_PUBLIC_HEADERS}
                ${AES_SRC} ${AES_PUBLIC_HEADERS}
                ${BZIP2_SRC} ${BZIP2_PUBLIC_HEADERS}
                ${LZMA_SRC} ${LZMA_PUBLIC_HEADERS})

if (MINGW AND BUILD_SHARED_LIBS)
  set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_NAME "minizip")
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C PREFIX ""
                      POSITION_INDEPENDENT_CODE 1)
if(USE_ZLIB)
    target_link_libraries(${PROJECT_NAME} ZLIB::ZLIB)
endif()
if(USE_LZMA)
    set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 99)
endif()
if(UNIX)
    target_link_libraries(${PROJECT_NAME} ${LIBBSD_LIBRARIES})
endif()

target_include_directories(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${INSTALL_INC_DIR}>)

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
        INCLUDES DESTINATION "${INSTALL_INC_DIR}"
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}"
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}")

install(EXPORT ${PROJECT_NAME}
        DESTINATION "${INSTALL_CMAKE_DIR}"
        NAMESPACE "MINIZIP::")

install(FILES ${MINIZIP_PUBLIC_HEADERS} DESTINATION "${INSTALL_INC_DIR}")
install(FILES ${MINIZIP_PC} DESTINATION "${INSTALL_PKGCONFIG_DIR}")

if(BUILD_TEST)
    add_executable(minizip "minizip.c")
    target_link_libraries(minizip ${PROJECT_NAME})

    install(TARGETS minizip
            RUNTIME DESTINATION "bin")
endif()
