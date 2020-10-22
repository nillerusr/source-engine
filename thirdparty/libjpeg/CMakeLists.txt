# Copyright (C) 2007-2012 LuaDist.
# Created by Peter Kapec, David Manura
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the COPYRIGHT file distributed with LuaDist.
# Please note that the package source code is licensed under its own license.

project ( libjpeg C )
cmake_minimum_required ( VERSION 2.8 )
include ( cmake/dist.cmake )

OPTION(BUILD_STATIC OFF)
OPTION(BUILD_EXECUTABLES ON)
OPTION(BUILD_TESTS ON)

include ( CheckIncludeFile )
check_include_file ( stddef.h HAVE_STDDEF_H )
check_include_file ( stdlib.h HAVE_STDLIB_H )
if ( WIN32 AND NOT CYGWIN )
  #improve? see jconfig.*
  set ( TWO_FILE_COMMANDLINE true )
  # jconfig.h
endif ( )
configure_file ( jconfig.h.cmake jconfig.h )

include_directories ( ${CMAKE_CURRENT_BINARY_DIR} )
# jconfig.h

set ( HEADERS jerror.h jmorecfg.h jpeglib.h ${CMAKE_CURRENT_BINARY_DIR}/jconfig.h )

set ( SRC jmemnobs.c jaricom.c jcapimin.c jcapistd.c jcarith.c jccoefct.c jccolor.c 
  jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c 
  jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdarith.c jdatadst.c jdatasrc.c 
  jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c 
  jdmerge.c jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c 
  jidctflt.c jidctfst.c jidctint.c jquant1.c jquant2.c jutils.c jmemmgr.c cderror.h 
  cdjpeg.h jdct.h jinclude.h jmemsys.h jpegint.h jversion.h transupp.h )

if ( BUILD_STATIC )
  add_library ( jpeg STATIC ${SRC} ${HEADERS} )
else ()
  add_library ( jpeg ${SRC} ${HEADERS} )
endif()

if ( BUILD_EXECUTABLES )
  add_executable ( cjpeg cdjpeg.c cjpeg.c rdbmp.c rdgif.c rdppm.c rdrle.c rdtarga.c 
    rdswitch.c )
  add_executable ( djpeg cdjpeg.c djpeg.c wrbmp.c wrgif.c wrppm.c wrrle.c wrtarga.c 
    rdcolmap.c )
  add_executable ( jpegtran jpegtran.c cdjpeg.c rdswitch.c transupp.c )
  add_executable ( rdjpgcom rdjpgcom.c )
  add_executable ( wrjpgcom wrjpgcom.c )
  target_link_libraries ( cjpeg jpeg )
  target_link_libraries ( djpeg jpeg )
  target_link_libraries ( jpegtran jpeg )
endif ()

if ( BUILD_EXECUTABLES )
  install_executable ( cjpeg djpeg jpegtran rdjpgcom wrjpgcom )
endif ()

install_library ( jpeg )
install_header ( ${HEADERS} )
install_doc ( README install.txt usage.txt wizard.txt example.c libjpeg.txt structure.txt 
  coderules.txt filelist.txt change.log )

if ( BUILD_TESTS )
  # tests
  enable_testing ( )
  macro ( mytest name target args input output )
  get_target_property ( _cmdpath ${target} LOCATION )
  add_test ( ${name} ${CMAKE_COMMAND} "-DCOMMAND=${_cmdpath} ${args}" "-DINPUT=${input}" 
    "-DOUTPUT=${output}" -P ${CMAKE_CURRENT_SOURCE_DIR}/jpeg_test.cmake )
  endmacro ( )
  set ( _src "${CMAKE_CURRENT_SOURCE_DIR}" )
  mytest ( t1 djpeg "-dct int -ppm -outfile testout.ppm ${_src}/testorig.jpg" "${_src}/testimg.ppm" 
    testout.ppm )
  mytest ( t2 djpeg "-dct int -bmp -colors 256 -outfile testout.bmp ${_src}/testorig.jpg" 
    ${_src}/testimg.bmp testout.bmp )
  mytest ( t3 cjpeg "-dct int -outfile testout.jpg ${_src}/testimg.ppm" ${_src}/testimg.jpg 
    testout.jpg )
  mytest ( t4 djpeg "-dct int -ppm -outfile testoutp.ppm ${_src}/testprog.jpg" ${_src}/testimg.ppm 
    testoutp.ppm )
  mytest ( t5 cjpeg "-dct int -progressive -opt -outfile testoutp.jpg ${_src}/testimg.ppm" 
    ${_src}/testimgp.jpg testoutp.jpg )
  mytest ( t6 jpegtran "-outfile testoutt.jpg ${_src}/testprog.jpg" ${_src}/testorig.jpg 
    testoutt.jpg )
endif ()