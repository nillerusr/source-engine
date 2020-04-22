#
# TF Stats Makefile for Linux 2.0
#
# Jul '99 by Mike Harrington <mike@valvesoftware.com>
# (modified by Wes Cumberland <wesc@valvesoftware.com>)
#
#

VERSION=1.0.0.0
VERSION_FN=$(VERSION)$(GLIBC)
RPM_RELEASE=0

ifneq (,$(findstring libc6,$(shell if [ -e /lib/libc.so.6* ];then echo libc6;fi)))
GLIBC=-glibc
else
GLIBC=
endif

ifneq (,$(findstring alpha,$(shell uname -m)))
ARCH=axp
RPMARCH=alpha
else
ARCH=i386
RPMARCH=i386
endif

MOUNT_DIR=/momma

BUILD_DEBUG_DIR=$(MOUNT_DIR)/tfstats/debug
BUILD_RELEASE_DIR=$(MOUNT_DIR)/tfstats/release
TFSTATS_DIR=$(MOUNT_DIR)/tfstats

CC=/usr/bin/g++
STDCPP_INC=	#/usr/local/lib/gcc-lib/H-libstdc++/include/g++-v3
STDCPP_LIB=	#/usr/local/lib/gcc-lib/H-libstdc++/lib
BASE_CFLAGS=-Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Iregexp/include

RELEASE_CFLAGS=$(BASE_CFLAGS) -m486 -O1
RELEASE_NOOP_CFLAGS=$(BASE_CFLAGS) -m486  

DEBUG_CFLAGS=$(BASE_CFLAGS) -g -D_DEBUG -DDEBUG -Wall 

LDFLAGS= -static /momma/tfstats/regexp/lib/libregex++.a -ldl -lm \
-lstdc++

AR=ar
RANLIB=ranlib

DO_CC_NOOPT=$(CC) -DNO_NAMESPACE -D_WONCRYPT_NOEXCEPTIONS -w $(CFLAGS_NOOPT) -o $@ -c $<
DO_CC=$(CC) -DNO_NAMESPACE -D_WONCRYPT_NOEXCEPTIONS -w $(CFLAGS) -o $@ -c $<
DO_W_CC=$(CC) -DNO_NAMESPACE -D_WONCRYPT_NOEXCEPTIONS $(CFLAGS) -Wall -o $@ -c $<
DO_CRYPT_CC=$(CC)  -x c++ -DNO_NAMESPACE -D_WONCRYPT_NOEXCEPTIONS $(CFLAGS) -I$(WON_INC) -o $@ -c $<

#############################################################################
# SETUP AND BUILD
#############################################################################

default: build_debug

TARGETS=\
	$(BUILDDIR)/tfstats_l


make_build_dir:
	for x in $(BUILDDIR) \
	$(BUILDDIR)/tfstats ; do \
	if [ ! -d $$x ];then mkdir $$x;fi;done

build_debug:
	$(MAKE) targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)" CFLAGS_NOOPT="$(DEBUG_CFLAGS)"

build_release:
	$(MAKE) targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)" CFLAGS_NOOPT="$(RELEASE_NOOP_CFLAGS)"

all: build_debug build_release

targets: $(TARGETS)

clean:
	rm -f $(BUILD_DEBUG_DIR)/*.o

#############################################################################
# TF Stats Program
#############################################################################

TFSTATS_OBJS = \
	$(BUILDDIR)/Argument.o \
	$(BUILDDIR)/EventList.o \
	$(BUILDDIR)/LogEvent.o \
	$(BUILDDIR)/LogEventIOStreams.o \
	$(BUILDDIR)/CureAward.o \
	$(BUILDDIR)/KamikazeAward.o \
	$(BUILDDIR)/SentryRebuildAward.o \
	$(BUILDDIR)/SharpshooterAward.o \
	$(BUILDDIR)/SurvivalistAward.o \
	$(BUILDDIR)/TalkativeAward.o \
	$(BUILDDIR)/TeamKillAward.o \
	$(BUILDDIR)/WeaponAwards.o \
	$(BUILDDIR)/CustomAward.o \
	$(BUILDDIR)/CustomAwardList.o \
	$(BUILDDIR)/CustomAwardTriggers.o \
	$(BUILDDIR)/Award.o \
	$(BUILDDIR)/CVars.o \
	$(BUILDDIR)/DialogueReadout.o \
	$(BUILDDIR)/MatchResults.o \
	$(BUILDDIR)/scoreboard.o \
	$(BUILDDIR)/WhoKilledWho.o \
	$(BUILDDIR)/Report.o \
	$(BUILDDIR)/HTML.o \
	$(BUILDDIR)/main.o \
	$(BUILDDIR)/TextFile.o \
	$(BUILDDIR)/util.o \
	$(BUILDDIR)/PlayerSpecifics.o \
	$(BUILDDIR)/StaticOutputFiles.o \
	$(BUILDDIR)/TFStatsReport.o  \
	$(BUILDDIR)/Player.o \
	$(BUILDDIR)/MatchInfo.o \
	$(BUILDDIR)/memdbg.o \
	$(BUILDDIR)/pid.o \
	$(BUILDDIR)/binresources.o \
	$(BUILDDIR)/tfstatsapplication.o \
	$(BUILDDIR)/plrpersist.o \
	$(BUILDDIR)/tfstatsosinterface.o \
	$(BUILDDIR)/allplayersstats.o \
	$(BUILDDIR)/playerreport.o 



$(BUILDDIR)/tfstats_l : $(TFSTATS_OBJS) 
	$(CC) $(CFLAGS) -L/usr/local/lib -o $@ $(TFSTATS_OBJS) $(LDFLAGS)

$(BUILDDIR)/buildnum.o :   $(TFSTATS_DIR)/buildnum.cpp
	$(DO_CC)

$(BUILDDIR)/Argument.o : $(TFSTATS_DIR)/Argument.cpp
	$(DO_CC)

$(BUILDDIR)/EventList.o : $(TFSTATS_DIR)/EventList.cpp
	$(DO_CC)

$(BUILDDIR)/LogEvent.o : $(TFSTATS_DIR)/LogEvent.cpp
	$(DO_CC)

$(BUILDDIR)/Player.o : $(TFSTATS_DIR)/Player.cpp
	$(DO_CC)

$(BUILDDIR)/MatchInfo.o : $(TFSTATS_DIR)/MatchInfo.cpp
	$(DO_CC)

$(BUILDDIR)/LogEventIOStreams.o : $(TFSTATS_DIR)/LogEventIOStreams.cpp
	$(DO_CC)

$(BUILDDIR)/CureAward.o : $(TFSTATS_DIR)/CureAward.cpp
	$(DO_CC)

$(BUILDDIR)/KamikazeAward.o : $(TFSTATS_DIR)/KamikazeAward.cpp
	$(DO_CC)

$(BUILDDIR)/SentryRebuildAward.o : $(TFSTATS_DIR)/SentryRebuildAward.cpp
	$(DO_CC)

$(BUILDDIR)/SharpshooterAward.o : $(TFSTATS_DIR)/SharpshooterAward.cpp
	$(DO_CC)

$(BUILDDIR)/SurvivalistAward.o : $(TFSTATS_DIR)/SurvivalistAward.cpp
	$(DO_CC)

$(BUILDDIR)/TalkativeAward.o : $(TFSTATS_DIR)/TalkativeAward.cpp
	$(DO_CC)

$(BUILDDIR)/TeamKillAward.o : $(TFSTATS_DIR)/TeamKillAward.cpp
	$(DO_CC)

$(BUILDDIR)/WeaponAwards.o : $(TFSTATS_DIR)/WeaponAwards.cpp
	$(DO_CC)

$(BUILDDIR)/CustomAward.o : $(TFSTATS_DIR)/CustomAward.cpp
	$(DO_CC)

$(BUILDDIR)/CustomAwardList.o : $(TFSTATS_DIR)/CustomAwardList.cpp
	$(DO_CC)

$(BUILDDIR)/CustomAwardTriggers.o : $(TFSTATS_DIR)/CustomAwardTriggers.cpp
	$(DO_CC)

$(BUILDDIR)/Award.o : $(TFSTATS_DIR)/Award.cpp
	$(DO_CC)

$(BUILDDIR)/CVars.o : $(TFSTATS_DIR)/CVars.cpp
	$(DO_CC)

$(BUILDDIR)/DialogueReadout.o : $(TFSTATS_DIR)/DialogueReadout.cpp
	$(DO_CC)

$(BUILDDIR)/MatchResults.o : $(TFSTATS_DIR)/MatchResults.cpp
	$(DO_CC)

$(BUILDDIR)/scoreboard.o : $(TFSTATS_DIR)/scoreboard.cpp
	$(DO_CC)

$(BUILDDIR)/WhoKilledWho.o : $(TFSTATS_DIR)/WhoKilledWho.cpp
	$(DO_CC)

$(BUILDDIR)/Report.o : $(TFSTATS_DIR)/Report.cpp
	$(DO_CC)

$(BUILDDIR)/HTML.o : $(TFSTATS_DIR)/HTML.cpp
	$(DO_CC)

$(BUILDDIR)/main.o : $(TFSTATS_DIR)/main.cpp
	$(DO_CC)

$(BUILDDIR)/TextFile.o : $(TFSTATS_DIR)/TextFile.cpp
	$(DO_CC)

$(BUILDDIR)/util.o : $(TFSTATS_DIR)/util.cpp
	$(DO_CC)

$(BUILDDIR)/binresources.o : $(TFSTATS_DIR)/binresources.cpp
	$(DO_CC)

$(BUILDDIR)/PlayerSpecifics.o : $(TFSTATS_DIR)/PlayerSpecifics.cpp
	$(DO_CC)

$(BUILDDIR)/StaticOutputFiles.o : $(TFSTATS_DIR)/StaticOutputFiles.cpp
	$(DO_CC)

$(BUILDDIR)/TFStatsReport.o : $(TFSTATS_DIR)/TFStatsReport.cpp
	$(DO_CC)

$(BUILDDIR)/memdbg.o : $(TFSTATS_DIR)/memdbg.cpp
	$(DO_CC)

$(BUILDDIR)/pid.o : $(TFSTATS_DIR)/pid.cpp
	$(DO_CC)

$(BUILDDIR)/tfstatsapplication.o : $(TFSTATS_DIR)/tfstatsapplication.cpp
	$(DO_CC)

$(BUILDDIR)/playerreport.o : $(TFSTATS_DIR)/playerreport.cpp
	$(DO_CC)

$(BUILDDIR)/plrpersist.o : $(TFSTATS_DIR)/plrpersist.cpp
	$(DO_CC)

$(BUILDDIR)/tfstatsosinterface.o : $(TFSTATS_DIR)/tfstatsosinterface.cpp
	$(DO_CC)

$(BUILDDIR)/allplayersstats.o : $(TFSTATS_DIR)/allplayersstats.cpp
	$(DO_CC)

