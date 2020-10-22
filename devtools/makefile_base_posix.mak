#
# Base makefile for Linux.
#
# !!!!! Note to future editors !!!!!
# 
# before you make changes, make sure you grok:
# 1. the difference between =, :=, +=, and ?= 
# 2. how and when this base makefile gets included in the generated makefile(s)
#  ( see http://www.gnu.org/software/make/manual/make.html#Flavors )
#
# Command line prefixes:
#  -	errors are ignored
#  @	command is not printed to stdout before being executed
#  +	command is executed even if Make is invoked in "do not exec" mode


ifneq "$(LINUX_TOOLS_PATH)" ""
TOOL_PATH = $(LINUX_TOOLS_PATH)/
endif

# Allow redirection to an arbitrary location for the tools we use.
# This helps with cross-compilation by not requiring Linux tools
# to be on the path and possibly overriding Windows tools.
CP = $(TOOL_PATH)cp
CUT = $(TOOL_PATH)cut
DIRNAME = $(TOOL_PATH)dirname
ECHO = $(TOOL_PATH)echo
FGREP = $(TOOL_PATH)fgrep
FIND = $(TOOL_PATH)find
GREP = $(TOOL_PATH)grep
MKDIR = $(TOOL_PATH)mkdir
RM = $(TOOL_PATH)rm
TAIL = $(TOOL_PATH)tail
SHELL = $(TOOL_PATH)bash

OS := $($(SHELL) $(TOOL_PATH)uname)
HOSTNAME := $($(SHELL) $(TOOL_PATH)hostname)

-include $(SRCROOT)/devtools/steam_def.mak
-include $(SRCROOT)/devtools/sourcesdk_def.mak

# To build with clang, set the following in your environment:
#   CC = clang
#   CXX = clang++
ifneq (,$(findstring clang,$(CXX)))
	CLANG_BUILD = 1
endif

ifeq ($(OS),Darwin)
    $(error This file should never be used for Mac - use base.xconfig)
endif

# Optimization flags specific to compiler/CFG combination
ifeq ($(CFG), release)
	# With gcc 4.6.3, engine.so went from 7,383,765 to 8,429,109 when building with -O3.
	#  There also was no speed difference running at 1280x1024. May 2012, mikesart.
	#  tonyp: The size increase was likely caused by -finline-functions and -fipa-cp-clone getting switched on with -O3.
	# -fno-omit-frame-pointer: need this for stack traces with perf.
	OptimizerLevel_CompilerSpecific = -O2 -fno-strict-aliasing -ffast-math -fno-omit-frame-pointer -ftree-vectorize
	ifeq ($(CLANG_BUILD),1)
		OptimizerLevel_CompilerSpecific += -funswitch-loops
	else
		OptimizerLevel_CompilerSpecific += -fpredictive-commoning -funswitch-loops
	endif
else
	OptimizerLevel_CompilerSpecific = -O0
	#-O1 -finline-functions
endif

# When we move to a modern toolchain, this will be necessary for early testing
# until we can ensure that every user has libraries built against the new C++11
# ABI. Further reading here:
# https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html
DEFINES += -D_GLIBCXX_USE_CXX11_ABI=0

# CPPFLAGS == "c/c++ *preprocessor* flags" - not "cee-plus-plus flags"
ARCH_FLAGS = 
BUILDING_MULTI_ARCH = 0
# Preserve cflags set in environment
ENV_CFLAGS := $(CFLAGS)
ENV_CXXFLAGS := $(CXXFLAGS) -fpermissive
CPPFLAGS = $(DEFINES) $(addprefix -I, $(abspath $(INCLUDEDIRS) ))
BASE_CFLAGS = $(ARCH_FLAGS) $(CPPFLAGS) $(WARN_FLAGS) -fvisibility=$(SymbolVisibility) $(OptimizerLevel) -pipe $(GCC_ExtraCompilerFlags) -Usprintf -Ustrncpy -UPROTECTED_THINGS_ENABLE
BASE_CXXFLAGS = -std=c++11
# Additional CXXFLAGS when compiling PCH files
PCH_CXXFLAGS =

# Compiler specific flags
ifeq ($(CLANG_BUILD),1)
	# The template depth for C++11 should be 1024, but compilers differ greatly. We go beyond 256 in at least one
	# spot, so turn it up to 900 which is GCC's default as of this writing
	BASE_CXXFLAGS += -ftemplate-depth=900
	# Needed for older versions of clang (newer versions are compatible with gcc syntax)
	PCH_CXXFLAGS += -emit-pch
else
	# GCC specific - better PCH behavior w/ccache and better debugging information
	BASE_CFLAGS += -fpch-preprocess -fvar-tracking-assignments
endif

DEFINES += -DVPROF_LEVEL=1 -DGNUC -DNO_HOOK_MALLOC

## TODO: This cases build errors in cstrike/bin right now. Need to debug.
# This causes all filesystem interfaces to default to their 64bit versions on
# 32bit systems, which means we don't break on filesystems with inodes > 32bit.
# DEFINES += -D_FILE_OFFSET_BITS=64

# Final CFLAGS/CXXFLAGS
CFLAGS = $(BASE_CFLAGS) $(ENV_CFLAGS)
CXXFLAGS = $(BASE_CFLAGS) $(BASE_CXXFLAGS) $(ENV_CXXFLAGS)
LDFLAGS = $(CFLAGS) $(GCC_ExtraLinkerFlags) $(OptimizerLevel)
GENDEP_CXXFLAGS = -MMD -MP -MF $(patsubst %.gch,%.P,$(@:.o=.P)) -MT $@
MAP_FLAGS =
Srv_GAMEOUTPUTFILE = 
COPY_DLL_TO_SRV = 0

# We should always specify -Wl,--build-id, as documented at:
# http://linux.die.net/man/1/ld and http://fedoraproject.org/wiki/Releases/FeatureBuildId.http://fedoraproject.org/wiki/Releases/FeatureBuildId
LDFLAGS += -Wl,--build-id

#
# If we should be running in a chroot, check to see if we are. If not, then prefix everything with the 
# required chroot
#
ifdef MAKE_CHROOT
	export STEAM_RUNTIME_PATH := /usr
	ifneq ("$(SCHROOT_CHROOT_NAME)", "$(CHROOT_NAME)")
        $(info '$(SCHROOT_CHROOT_NAME)' is not '$(CHROOT_NAME)')
        $(error This makefile should be run from within a chroot. 'schroot --chroot $(CHROOT_NAME) -- $(MAKE) $(MAKEFLAGS)')  
	endif
	GCC_VER = -4.8
	P4BIN = $(SRCROOT)/devtools/bin/linux/p4
	CRYPTOPPDIR=ubuntu12_32_gcc48
else ifeq ($(USE_VALVE_BINDIR),1)
	# Using /valve/bin directory.
	export STEAM_RUNTIME_PATH ?= /valve
	GCC_VER = -4.6
	P4BIN = $(SRCROOT)/devtools/bin/linux/p4
	CRYPTOPPDIR=linux32
else
	# Not using chroot, use old steam-runtime. (gcc 4.6.3)
	export STEAM_RUNTIME_PATH ?= /valve/steam-runtime
	GCC_VER =
	P4BIN = true
	CRYPTOPPDIR=ubuntu12_32
endif

ifeq ($(TARGET_PLATFORM),linux64)
	MARCH_TARGET = core2
else
	MARCH_TARGET = pentium4
endif

ifeq ($(USE_VALVE_BINDIR),1)
	# On dedicated servers, some plugins depend on global variable symbols in addition to functions.
	# So symbols like _Z16ClearMultiDamagev should show up when you do "nm server_srv.so" in TF2.
	STRIP_FLAGS =
else
	# Linux desktop client (or client/dedicated server in chroot).
	STRIP_FLAGS = -x
endif

ifeq ($(CLANG_BUILD),1)
	# Clang does not support -mfpmath=sse because it uses whatever
	# instruction set extensions are available by default.
	SSE_GEN_FLAGS = -msse2
else
	SSE_GEN_FLAGS = -msse2 -mfpmath=sse
endif

CCACHE := $(SRCROOT)/devtools/bin/linux/ccache

# - pch_defines,time_macros,file_macro needed for precompiled headers to be beneficial. See additional ".defines" wrapper
#   for PCH below to prevent this from biting us (in theory)
# - include_file_ctime/mtime causes re-generated but identical headers, like protobuf, to cause a cache miss. I couldn't
#   find the justification for this setting being on by default, perhaps to not waste time preprocessing the files?
#   Let's find out the painful way.
export CCACHE_SLOPPINESS := $(CCACHE_SLOPPINESS),time_macros,file_macro,include_file_ctime,include_file_mtime,pch_defines
# Needed for clang to take advantage of PCH, we include the PCH in the
# source files and clang doesn't support -fpch-preprocess properly
ifeq ($(CLANG_BUILD),1)
	export CCACHE_CPP2 = 1
endif

# If not specified by environment, use steam runtime compilers + in-tree ccache
ifneq ($(filter default undefined,$(origin AR)),)
	AR = ar crs
endif
ifneq ($(filter default undefined,$(origin CC)),)
	CC = gcc -m32
endif
ifneq ($(filter default undefined,$(origin CXX)),)
	CXX = g++ -m32
endif

LINK ?= $(CC)

ifeq ($(STEAM_BRANCH),1)
	WARN_FLAGS = -Wall -Wextra -Wshadow -Wno-invalid-offsetof
else
	WARN_FLAGS = -Wall -Wno-invalid-offsetof -Wno-multichar -Wno-overloaded-virtual
	WARN_FLAGS += -Wno-write-strings
	WARN_FLAGS += -Wno-unused-variable
	WARN_FLAGS += -Wno-unused-function
	# Additional warnings to -Wall
	WARN_FLAGS += -Winvalid-pch -Wswitch
endif

ifeq ($(CLANG_BUILD),1)
	# Clang specific flags
	WARN_FLAGS += -Wno-unused-const-variable -Wno-unused-local-typedef
else ifeq ($(GCC_VER),-4.8)
	WARN_FLAGS += -Wno-unused-result
	WARN_FLAGS += -Wno-unused-but-set-variable
	# WARN_FLAGS += -Wno-unused-function
endif

WARN_FLAGS += -Wno-unknown-pragmas -Wno-unused-parameter -Wno-unused-value
WARN_FLAGS += -Wno-invalid-offsetof -Wno-float-equal -Wno-reorder -Werror=return-type
WARN_FLAGS += -fdiagnostics-show-option -Wformat -Wformat-security

ifeq ($(TARGET_PLATFORM),linux64)
	# nocona = pentium4 + 64bit + MMX, SSE, SSE2, SSE3 - no SSSE3 (that's three s's - added in core2)
	ARCH_FLAGS += -march=$(MARCH_TARGET) -mtune=core2
	LD_SO = ld-linux-x86_64.so.2
	LIBSTDCXX := $(shell $(CXX) -print-file-name=libstdc++.a)
	LIBSTDCXXPIC := $(shell $(CXX) -print-file-name=libstdc++-pic.a)
else
	# pentium4 = MMX, SSE, SSE2 - no SSE3 (added in prescott) # -msse3 -mfpmath=sse
	ARCH_FLAGS += -m32 -march=$(MARCH_TARGET) -mtune=core2 $(SSE_GEN_FLAGS)
	LD_SO = ld-linux.so.2
	LIBSTDCXX := $(shell $(CXX) $(ARCH_FLAGS) -print-file-name=libstdc++.so)
	LIBSTDCXXPIC := $(shell $(CXX) $(ARCH_FLAGS) -print-file-name=libstdc++.so)
	LDFLAGS += -m32
endif

GEN_SYM ?= $(SRCROOT)/devtools/gendbg.sh
ifeq ($(CFG),release)
	STRIP ?= strip $(STRIP_FLAGS) -S
#	CFLAGS += -ffunction-sections -fdata-sections
#	LDFLAGS += -Wl,--gc-sections -Wl,--print-gc-sections
else
	STRIP ?= true
endif
VSIGN ?= true

ifeq ($(SOURCE_SDK), 1)
	Srv_GAMEOUTPUTFILE := $(GAMEOUTPUTFILE:.so=_srv.so)
	COPY_DLL_TO_SRV := 1
endif

LINK_MAP_FLAGS = -Wl,-Map,$(@:.so=).map

SHLIBLDFLAGS = -shared $(LDFLAGS) -Wl,--no-undefined

_WRAP := -Xlinker --wrap=
PATHWRAP = $(_WRAP)fopen $(_WRAP)freopen $(_WRAP)open    $(_WRAP)creat    $(_WRAP)access  $(_WRAP)__xstat \
	   $(_WRAP)stat  $(_WRAP)lstat   $(_WRAP)fopen64 $(_WRAP)open64   $(_WRAP)opendir $(_WRAP)__lxstat \
	   $(_WRAP)chmod $(_WRAP)chown   $(_WRAP)lchown  $(_WRAP)symlink  $(_WRAP)link    $(_WRAP)__lxstat64 \
	   $(_WRAP)mknod $(_WRAP)utimes  $(_WRAP)unlink  $(_WRAP)rename   $(_WRAP)utime   $(_WRAP)__xstat64 \
	   $(_WRAP)mount $(_WRAP)mkfifo  $(_WRAP)mkdir   $(_WRAP)rmdir    $(_WRAP)scandir $(_WRAP)realpath

ifeq "$(BufferSecurityCheck)" "No"
    CFLAGS += -fno-stack-protector
else
    ifeq ($(shell $(CC) -fstack-protector-strong -xc -c /dev/null -o /dev/null 2>/dev/null && echo yes || echo no),yes)
        # Newer GCC versions have a better stack protector available
        # http://lwn.net/Articles/584225/
        STACK_PROTECTOR := -fstack-protector-strong
    else
        STACK_PROTECTOR := -fstack-protector
    endif
    CFLAGS += $(STACK_PROTECTOR)
endif


LIB_START_EXE = $(PATHWRAP) -static-libgcc -Wl,--start-group
LIB_END_EXE = -Wl,--end-group -lm -ldl $(LIBSTDCXX) -lpthread

LIB_START_SHLIB = $(PATHWRAP) -static-libgcc -Wl,--start-group
LIB_END_SHLIB = -Wl,--end-group -lm -ldl $(LIBSTDCXXPIC) -lpthread -l:$(LD_SO) -Wl,--version-script=$(SRCROOT)/devtools/version_script.linux.txt

#
# Profile-directed optimizations.
# Note: Last time these were tested 3/5/08, it actually slowed down the server benchmark by 5%!
#
# First, uncomment these, build, and test. It will generate .gcda and .gcno files where the .o files are.
# PROFILE_LINKER_FLAG=-fprofile-arcs
# PROFILE_COMPILER_FLAG=-fprofile-arcs
#
# Then, comment the above flags out again and rebuild with this flag uncommented:
# PROFILE_COMPILER_FLAG=-fprofile-use
#

#############################################################################
# The compiler command lne for each src code file to compile
#############################################################################

OBJ_DIR = ./obj_$(NAME)_$(TARGET_PLATFORM)$(TARGET_PLATFORM_EXT)/$(CFG)
CPP_TO_OBJ = $(CPPFILES:.cpp=.o)
CXX_TO_OBJ = $(CPP_TO_OBJ:.cxx=.o)
CC_TO_OBJ = $(CXX_TO_OBJ:.cc=.o)
MM_TO_OBJ = $(CC_TO_OBJ:.mm=.o)
C_TO_OBJ = $(MM_TO_OBJ:.c=.o)
OBJS = $(addprefix $(OBJ_DIR)/, $(notdir $(C_TO_OBJ)))

ifeq ($(MAKE_VERBOSE),1)
	QUIET_PREFIX = 
	QUIET_ECHO_POSTFIX = 
else
	QUIET_PREFIX = @
	QUIET_ECHO_POSTFIX = > /dev/null
endif

ifeq ($(MAKE_CC_VERBOSE),1)
CC += -v
endif

ifeq ($(CONFTYPE),lib)
  LIB_File = $(OUTPUTFILE)
endif

ifeq ($(CONFTYPE),dll)
  SO_File = $(OUTPUTFILE)
endif

ifeq ($(CONFTYPE),exe)
  EXE_File = $(OUTPUTFILE)
endif

# we generate dependencies as a side-effect of compilation now
GEN_DEP_FILE=

PRE_COMPILE_FILE = 

POST_COMPILE_FILE =

ifeq ($(CLANG_BUILD),1)
	# https://bugzilla.samba.org/show_bug.cgi?id=11251

	# ccache bug 11251: Clang PCH files always have the timestamps of their deps baked in, so if
	# make thinks a file needs to be rebuilt, ccache pulling an older one from cache will cause
	# an error.

	# Unfortunately, this means any touching of headers in PCH files will cause it to be rebuilt
	# and all files using that PCH to cache-miss.

	# One specific case we could fix is otherwise-no-op *clean* builds missing the cache: We
	# could use a similar trick to the the CCACHE_EXTRAFILES hack used below to bake timestamps
	# into the PCH, so that at least clean builds could pull a PCH from cache.
	PCH_CXX = CCACHE_DISABLE=1 $(CXX)
	# Newer clang supports the -include $PCH_FILE syntax for finding the associated GCH, but for compatibility with
	# older versions we'll use the more explicit -include-pch option
	INCLUDE_PCH_FILE = -include-pch $(TARGET_PCH_FILE).gch
else
	PCH_CXX = $(CXX)
	INCLUDE_PCH_FILE = -include $(abspath $(TARGET_PCH_FILE))
endif

ifeq ($(BUILDING_MULTI_ARCH),1)
	SINGLE_ARCH_CXXFLAGS=$(subst -arch x86_64,,$(CXXFLAGS))
	_MULTI_ARCH_FIRST_FILE = \
		mkdir -p $(OBJ_DIR) && \
		$(CXX) $(SINGLE_ARCH_CXXFLAGS) $(GENDEP_CXXFLAGS) -o $@ -c $<
	_MULTI_ARCH_SECOND_FILE = $(CXX) $(CXXFLAGS) -o $@ -c $<

	COMPILE_FILE = $(QUIET_PREFIX) \
		echo "--M- $(lastword $(subst /, ,$<)) ----";\
		$(_MULTI_ARCH_FIRST_FILE) $(_MULTI_ARCH_SECOND_FILE)

	COMPILE_FILE_WITH_PCH = \
		$(QUIET_PREFIX) \
		echo "-PM- $(lastword $(subst /, ,$<)) ----";\
		$(COMPILE_MULTI_ARCH_FIRST_FILE) $(INCLUDE_PCH_FILE) \
		$(COMPILE_MULTI_ARCH_SECOND_FILE) $(INCLUDE_PCH_FILE)

	# Compile for PCH - the VPC code handles the deps for these
	# (See EXTRAFILES note below in non-multiarch compile)
	PCH_FINAL_CXXFLAGS = $(filter-out -gsplit-dwarf,$(SINGLE_ARCH_CXXFLAGS) $(GENDEP_CXXFLAGS) $(PCH_CXXFLAGS))
	PCH_FINAL_CXXFLAGS_MULTI = $(filter-out -gsplit-dwarf,$(CXXFLAGS) $(PCH_CXXFLAGS))
	COMPILE_PCH = \
		$(QUIET_PREFIX) \
		echo "-M-- $(lastword $(subst /, ,$<)) [PCH] ----";\
		mkdir -p $(OBJ_DIR) && \
		$(CXX)  -x c++-header -E -dM -o $@.defines -c $< && \
		CCACHE_EXTRAFILES=$@.defines $(PCH_CXX) $(PCH_FINAL_CXXFLAGS) -x c++-header -o $@ -c $< && \
		$(CXX) $(CXXFLAGS) $(PCH_CXXFLAGS) -x c++-header -E -dM -o $@.defines -c $< && \
		CCACHE_EXTRAFILES=$@.defines $(PCH_CXX) $(PCH_FINAL_CXXFLAGS_MULTI) -x c++-header -o $@ -c $<
else
	COMPILE_FILE_BASE = \
		mkdir -p $(OBJ_DIR) && \
		$(CXX) $(CXXFLAGS) $(GENDEP_CXXFLAGS) -o $@ -c $<

	COMPILE_FILE = \
		$(QUIET_PREFIX) echo "---- $(lastword $(subst /, ,$<)) ----"; \
		$(COMPILE_FILE_BASE)

	COMPILE_FILE_WITH_PCH = \
		$(QUIET_PREFIX) \
		echo "-P-- $(lastword $(subst /, ,$<)) ----"; \
		$(COMPILE_FILE_BASE) $(INCLUDE_PCH_FILE)

	# Compile for PCH - the VPC code handles the deps for these.
	#
	# Because we set CCACHE_SLOPPINESS=pch_defines (or ccache would recompile it every time), we'll miss define-only
	# changes in dep files. As a dirty workaround, when we compile a PCH file, first run with
	# { -E -dM -o $@.defines } to build a file that represents all of the defines in that file, then pass that
	# .defines to CCACHE_EXTRAFILES for the real compile.
	#
	# This allows ccache to properly re-compile when defines change, so we can still cache PCH files (except for
	# clang, see PCH_CXX definition above). This is a huge compile speedup for clean builds and other changes that
	# don't affect the preprocessed result of the PCH.
	#
	# Adding to the fun things-that-break-ccache list, -gsplit-dwarf on PCH compiles -> 100% miss rate (and thus
	# 100% miss on all files using it
	PCH_FINAL_CXXFLAGS = $(filter-out -gsplit-dwarf,$(CXXFLAGS) $(GENDEP_CXXFLAGS) $(PCH_CXXFLAGS))
	COMPILE_PCH = \
		$(QUIET_PREFIX) echo "---- $(lastword $(subst /, ,$<)) [PCH] ----";\
		mkdir -p $(OBJ_DIR) && \
		$(PCH_CXX) $(PCH_FINAL_CXXFLAGS) -x c++-header -E -dM -o $@.defines $< && \
		CCACHE_EXTRAFILES=$@.defines $(PCH_CXX) $(PCH_FINAL_CXXFLAGS) -x c++-header -o $@ -c $<
endif

ifneq "$(origin VALVE_NO_AUTO_P4)" "undefined"
	P4_EDIT_START = chmod -R +w
	P4_EDIT_END = || true
	P4_REVERT_START = true
	P4_REVERT_END =
else
	# You can use an environment variable to specify what changelist to check the Linux Binaries out into. Normally the default
	# setting is best, but here is an alternate example:
	# export P4_EDIT_CHANGELIST_CMD="echo 1424335"
	# ?= means that if P4_EDIT_CHANGELIST_CMD is already set it won't be changed.
	P4_EDIT_CHANGELIST_CMD ?= $(P4BIN) changes -c `$(P4BIN) client -o | grep ^Client | cut -f 2` -s pending | fgrep 'POSIX Auto Checkout' | cut -d' ' -f 2 | tail -n 1
	ifndef P4_EDIT_CHANGELIST
		P4_EDIT_CHANGELIST := $(shell $(P4_EDIT_CHANGELIST_CMD))
	endif
	ifeq ($(P4_EDIT_CHANGELIST),)
		# If we haven't found a changelist to check out to then create one. The name must match the one from a few
		# lines above or else a new changelist will be created each time.
		# Warning: the behavior of 'echo' is not consistent. In bash you need the "-e" option in order for \n to be
		# interpreted as a line-feed, but in dash you do not, and if "-e" is passed along then it is printed, which
		# confuses p4. So, if you run this command from the bash shell don't forget to add "-e" to the echo command.
		P4_EDIT_CHANGELIST = $(shell /bin/echo -e "Change: new\nDescription: POSIX Auto Checkout" | $(P4BIN) change -i | cut -f 2 -d ' ')
	endif

	P4_EDIT_START := for f in
	P4_EDIT_END := ; do if [ -n $$f ]; then if [ -d $$f ]; then find $$f -type f -print | $(P4BIN) -x - edit -c $(P4_EDIT_CHANGELIST); else $(P4BIN) edit -c $(P4_EDIT_CHANGELIST) $$f; fi; fi; done $(QUIET_ECHO_POSTFIX)
	P4_REVERT_START := for f in  
	P4_REVERT_END := ; do if [ -n $$f ]; then if [ -d $$f ]; then find $$f -type f -print | $(P4BIN) -x - revert; else $(P4BIN) revert $$f; fi; fi; done $(QUIET_ECHO_POSTFIX) 
endif

ifneq "$(GAMEOUTPUTFILE)" ""
all: $(OTHER_DEPENDENCIES) $(OBJS) $(GAMEOUTPUTFILE)
	@echo $(GAMEOUTPUTFILE) $(QUIET_ECHO_POSTFIX)
else
all: $(OTHER_DEPENDENCIES) $(OBJS) $(OUTPUTFILE)
	@echo $(OUTPUTFILE) $(QUIET_ECHO_POSTFIX)
endif

.PHONY: clean cleantargets cleanandremove rebuild relink RemoveOutputFile SingleFile


rebuild :
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) cleanandremove
	$(MAKE) -f $(firstword $(MAKEFILE_LIST))


# Use the relink target to force to relink the project.
relink: RemoveOutputFile all

RemoveOutputFile:
	rm -f $(OUTPUTFILE)


# This rule is so you can say "make SingleFile SingleFilename=/home/myname/valve_main/src/engine/language.cpp" and have it only build that file.
# It basically just translates the full filename to create a dependency on the appropriate .o file so it'll build that.
SingleFile : RemoveSingleFile $(OBJ_DIR)/$(basename $(notdir $(SingleFilename))).o
	@echo ""

RemoveSingleFile:
	$(QUIET_PREFIX) rm -f $(OBJ_DIR)/$(basename $(notdir $(SingleFilename))).o

clean:
ifneq "$(OBJ_DIR)" ""
	$(QUIET_PREFIX) echo "rm -rf $(OBJ_DIR)"
	$(QUIET_PREFIX) rm -rf $(OBJ_DIR)
endif
ifneq "$(OUTPUTFILE)" ""
	$(QUIET_PREFIX) if [ -e $(OUTPUTFILE) ]; then \
		echo "$(P4BIN) revert $(OUTPUTFILE)"; \
		$(P4_REVERT_START) $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT) $(P4_REVERT_END); \
	fi;
endif
ifneq "$(OTHER_DEPENDENCIES)" ""
	$(QUIET_PREFIX) echo "rm -f $(OTHER_DEPENDENCIES)"
	$(QUIET_PREFIX) rm -f $(OTHER_DEPENDENCIES)
endif
ifneq "$(GAMEOUTPUTFILE)" ""
	$(QUIET_PREFIX) echo "$(P4BIN) revert $(GAMEOUTPUTFILE)"
	$(QUIET_PREFIX) $(P4_REVERT_START) $(GAMEOUTPUTFILE) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_REVERT_END)
endif


# Do the above cleaning, except with p4 edit and rm. Reason being ar crs adds and replaces obj files to the
# archive. However if you've renamed or deleted a source file, $(AR) won't remove it. This can leave
# us with archive files that have extra unused symbols, and also potentially cause compilation errors
# when you rename a file and have many duplicate symbols.
cleanandremove:
ifneq "$(OBJ_DIR)" ""
	$(QUIET_PREFIX) echo "rm -rf $(OBJ_DIR)"
	$(QUIET_PREFIX) -rm -rf $(OBJ_DIR)
endif
ifneq "$(OUTPUTFILE)" ""
	$(QUIET_PREFIX) if [ -e $(OUTPUTFILE) ]; then \
		echo "$(P4BIN) edit and rm -f $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT)"; \
		$(P4_EDIT_START) $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END); \
	fi;
	$(QUIET_PREFIX) -rm -f $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT);
endif
ifneq "$(OTHER_DEPENDENCIES)" ""
	$(QUIET_PREFIX) echo "rm -f $(OTHER_DEPENDENCIES)"
	$(QUIET_PREFIX) -rm -f $(OTHER_DEPENDENCIES)
endif
ifneq "$(GAMEOUTPUTFILE)" ""
	$(QUIET_PREFIX) echo "$(P4BIN) edit and rm -f $(GAMEOUTPUTFILE) $(GAMEOUTPUTFILE)$(SYM_EXT)"
	$(QUIET_PREFIX) $(P4_EDIT_START) $(GAMEOUTPUTFILE) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END)
	$(QUIET_PREFIX) -rm -f $(GAMEOUTPUTFILE)
endif


# This just reverts the final targets so it'll do a relink next time we build.
cleantargets:
ifneq "$(GAMEOUTPUTFILE)" ""
	$(QUIET_PREFIX) echo "$(P4BIN) revert $(GAMEOUTPUTFILE)"
	$(QUIET_PREFIX) $(P4_REVERT_START) $(GAMEOUTPUTFILE) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_REVERT_END)
endif
ifneq "$(OUTPUTFILE)" ""
	$(QUIET_PREFIX) echo "$(P4BIN) revert $(OUTPUTFILE)"
	$(QUIET_PREFIX) $(P4_REVERT_START) $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT) $(P4_REVERT_END)
endif

$(LIB_File): $(OTHER_DEPENDENCIES) $(OBJS) 
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(LIB_File) $(P4_EDIT_END); 
	$(QUIET_PREFIX) $(AR) $(LIB_File) $(OBJS) $(LIBFILES);
	$(SHELL) -c "$(POSTBUILDCOMMAND)"

SO_GameOutputFile = $(GAMEOUTPUTFILE)

# Remove the target before installing a file over it; this prevents existing
# instances of the game from crashing due to the overwrite.
$(GAMEOUTPUTFILE): $(OUTPUTFILE)
	$(QUIET_PREFIX) \
	$(P4_EDIT_START) $(GAMEOUTPUTFILE) $(P4_EDIT_END) && \
	echo "----" $(QUIET_ECHO_POSTFIX);\
	echo "---- COPYING TO $@ [$(CFG)] ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(GAMEOUTPUTFILE) $(P4_EDIT_END);
	$(QUIET_PREFIX) -mkdir -p `dirname $(GAMEOUTPUTFILE)` > /dev/null;
	$(QUIET_PREFIX) rm -f $(GAMEOUTPUTFILE) $(QUIET_ECHO_POSTFIX);
	$(QUIET_PREFIX) cp -v $(OUTPUTFILE) $(GAMEOUTPUTFILE) $(QUIET_ECHO_POSTFIX);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END);
	$(QUIET_PREFIX) $(GEN_SYM) $(GAMEOUTPUTFILE); 
	$(QUIET_PREFIX) -$(STRIP) $(GAMEOUTPUTFILE);
	$(QUIET_PREFIX) $(VSIGN) -signvalve $(GAMEOUTPUTFILE);
	$(QUIET_PREFIX) if [ "$(COPY_DLL_TO_SRV)" = "1" ]; then\
		echo "----" $(QUIET_ECHO_POSTFIX);\
		echo "---- COPYING TO $(Srv_GAMEOUTPUTFILE) ----";\
		echo "----" $(QUIET_ECHO_POSTFIX);\
		cp -v $(GAMEOUTPUTFILE) $(Srv_GAMEOUTPUTFILE) $(QUIET_ECHO_POSTFIX);\
		cp -v $(GAMEOUTPUTFILE)$(SYM_EXT) $(Srv_GAMEOUTPUTFILE)$(SYM_EXT) $(QUIET_ECHO_POSTFIX);\
	fi;
	$(QUIET_PREFIX) if [ "$(IMPORTLIBRARY)" != "" ]; then\
		echo "----" $(QUIET_ECHO_POSTFIX);\
		echo "---- COPYING TO IMPORT LIBRARY $(IMPORTLIBRARY) ----";\
		echo "----" $(QUIET_ECHO_POSTFIX);\
		$(P4_EDIT_START) $(IMPORTLIBRARY) $(P4_EDIT_END) && \
		mkdir -p `dirname $(IMPORTLIBRARY)` > /dev/null && \
		cp -v $(OUTPUTFILE) $(IMPORTLIBRARY); \
	fi;


$(SO_File): $(OTHER_DEPENDENCIES) $(OBJS) $(LIBFILENAMES)
	$(QUIET_PREFIX) \
	echo "----" $(QUIET_ECHO_POSTFIX);\
	echo "---- LINKING $@ [$(CFG)] ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);\
	\
	$(LINK) $(LINK_MAP_FLAGS) $(SHLIBLDFLAGS) $(PROFILE_LINKER_FLAG) -o $(OUTPUTFILE) $(LIB_START_SHLIB) $(OBJS) $(LIBFILES) $(SystemLibraries) $(LIB_END_SHLIB);
	$(QUIET_PREFIX) $(VSIGN) -signvalve $(OUTPUTFILE);
	$(QUIET_PREFIX) $(SHELL) -c "$(POSTBUILDCOMMAND)"



$(EXE_File) : $(OTHER_DEPENDENCIES) $(OBJS) $(LIBFILENAMES)
	$(QUIET_PREFIX) \
	echo "----" $(QUIET_ECHO_POSTFIX);\
	echo "---- LINKING EXE $@ [$(CFG)] ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);\
	\
	$(P4_EDIT_START) $(OUTPUTFILE) $(P4_EDIT_END);\
	$(LINK) $(LINK_MAP_FLAGS) $(LDFLAGS) $(PROFILE_LINKER_FLAG) -o $(OUTPUTFILE) $(LIB_START_EXE) $(OBJS) $(LIBFILES) $(SystemLibraries) $(LIB_END_EXE);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(OUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END);
	$(QUIET_PREFIX) $(GEN_SYM) $(OUTPUTFILE);
	$(QUIET_PREFIX) -$(STRIP) $(OUTPUTFILE);
	$(QUIET_PREFIX) $(VSIGN) -signvalve $(OUTPUTFILE);
	$(QUIET_PREFIX) $(SHELL) -c "$(POSTBUILDCOMMAND)"


tags:
	etags -a -C -o $(SRCROOT)/TAGS *.cpp *.cxx *.h *.hxx
