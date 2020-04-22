OS=$(shell uname)

AR=ar

ifeq ($(OS),CYGWIN_NT-5.1)
_CYGWIN=1
endif

ifeq ($(OS),Linux)
CXX=g++
endif

ifeq ($(OS),Darwin)
CXX=g++-4.2
endif

ifeq ($(_CYGWIN),1)
CXX=/valve/gcc-4.2.4-windows/bin/g++
AR=/gcc/ar
RUN_RANLIB=ranlib libjpeg.a
endif

HOST_CFLAGS=-O3 -DfPIC
JPEG_CFLAGS := 
JPEG_HCFLAGS := $(JPEG_CFLAGS) -msse -mfpmath=sse

JPEG_SRCS := \
	jcapimin.c \
	jcapistd.c \
	jccoefct.c \
	jccolor.c \
	jcdctmgr.c \
	jchuff.c \
	jcinit.c \
	jcmainct.c \
	jcmarker.c \
	jcmaster.c \
	jcomapi.c \
	jcparam.c \
	jcphuff.c \
	jcprepct.c \
	jcsample.c \
	jctrans.c \
	jdapimin.c\
	jdapistd.c \
	jdatadst.c \
	jdatasrc.c \
	jdcoefct.c \
	jdcolor.c \
	jddctmgr.c \
	jdhuff.c \
	jdinput.c \
	jdmainct.c \
	jdmarker.c \
	jdmaster.c \
	jdmerge.c \
	jdphuff.c \
	jdpostct.c \
	jdsample.c \
	jdtrans.c \
	jerror.c \
	jfdctflt.c \
	jfdctfst.c \
	jfdctint.c \
	jidctflt.c \
	jidctfst.c \
	jidctint.c \
	jidctred.c \
	jmemmgr.c \
	jmemnobs.c \
	jquant1.c \
	jquant2.c \
	jutils.c 


JPEG_TARGETS := djpeg

OBJDIRS += jpeg
CLEAN_FILES += $(JPEG_DOBJS) $(JPEG_NOBJS) $(JPEG_TARGETS)

JPEG_HDOBJS := $(patsubst %.c,%.o,$(JPEG_SRCS))
CLEAN_FILES += $(JPEG_HDOBJS) jpeg/hdjpeg

hdjpeg: $(JPEG_HDOBJS)
	$(AR) rs libjpeg.a $(JPEG_HDOBJS);
	$(RUN_RANLIB)

%.o: %.c
	$(CXX) -o $@ $(HOST_CFLAGS) $(JPEG_HCFLAGS) -c $<

clean:
	rm *.o

all: $(JPEG_TARGETS)

