## Process this file with automake to produce Makefile.in. -*-Makefile-*-

# $Id: Makefile.am,v 1.49 2003/03/17 22:40:57 jm Exp $

# Disable automatic dependency tracking if using other tools than gcc and gmake
#AUTOMAKE_OPTIONS = no-dependencies

lib_LTLIBRARIES = libspeex.la

# Sources for compilation in the library
libspeex_la_SOURCES = nb_celp.c \
	sb_celp.c \
	lpc.c \
	ltp.c \
	lsp.c \
	quant_lsp.c \
	lsp_tables_nb.c \
	gain_table.c \
	gain_table_lbr.c \
	cb_search.c \
	filters.c \
	bits.c \
	modes.c \
	vq.c \
	high_lsp_tables.c \
	vbr.c \
	hexc_table.c \
	exc_5_256_table.c \
	exc_5_64_table.c \
	exc_8_128_table.c \
	exc_10_32_table.c \
	exc_10_16_table.c \
	exc_20_32_table.c \
	hexc_10_32_table.c \
	misc.c \
	speex_header.c \
	speex_callbacks.c \
	math_approx.c \
	stereo.c


include_HEADERS =  speex.h \
	speex_bits.h \
	speex_header.h \
	speex_callbacks.h \
	speex_stereo.h

noinst_HEADERS = lsp.h \
	nb_celp.h \
	lpc.h \
	ltp.h \
	quant_lsp.h \
	cb_search.h \
	filters.h \
	stack_alloc.h \
	vq.h \
	modes.h \
	sb_celp.h \
	vbr.h \
	misc.h \
	ltp_sse.h \
	filters_sse.h \
	math_approx.h
	
	
libspeex_la_LDFLAGS = -version-info @SPEEX_LT_CURRENT@:@SPEEX_LT_REVISION@:@SPEEX_LT_AGE@

noinst_PROGRAMS = testenc testenc_wb testenc_uwb

testenc_SOURCES = testenc.c
testenc_LDADD = libspeex.la

testenc_wb_SOURCES = testenc_wb.c
testenc_wb_LDADD = libspeex.la

testenc_uwb_SOURCES = testenc_uwb.c
testenc_uwb_LDADD = libspeex.la
