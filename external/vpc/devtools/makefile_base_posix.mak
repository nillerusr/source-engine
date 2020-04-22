#
# Base makefile for Linux and OSX
#
# !!!!! Note to future editors !!!!!
# 
# before you make changes, make sure you grok:
# 1. the difference between =, :=, +=, and ?= 
# 2. how and when this base makefile gets included in the generated makefile(s)
# 

OS := $(shell uname)
HOSTNAME := $(shell hostname)

-include $(SRCROOT)/devtools/steam_def.mak

ifeq ($(CFG), release)
	OptimizerLevel_CompilerSpecific = -O3 -fno-strict-aliasing
else
	OptimizerLevel_CompilerSpecific = -O0
	#-O1 -finline-functions
endif

# CPPFLAGS == "c/c++ *preprocessor* flags" - not "cee-plus-plus flags"
ARCH_FLAGS = 
BUILDING_MULTI_ARCH = 0
CPPFLAGS = $(DEFINES) $(addprefix -I, $(abspath $(INCLUDEDIRS) ))
CFLAGS = $(ARCH_FLAGS) $(CPPFLAGS) $(WARN_FLAGS) -fvisibility=$(SymbolVisibility) $(OptimizerLevel) -fPIC -pipe $(GCC_ExtraCompilerFlags) -Usprintf -Ustrncpy -UPROTECTED_THINGS_ENABLE
CXXFLAGS = $(CFLAGS)
DEFINES += -DVPROF_LEVEL=1 -DGNUC 
LDFLAGS = $(CFLAGS) $(GCC_ExtraLinkerFlags) $(OptimizerLevel)
GENDEP_CXXFLAGS = -MD -MP -MF $(@:.o=.P) 

ifeq ($(STEAM_BRANCH),1)
	WARN_FLAGS = -Wall -Wextra -Wshadow -Wno-invalid-offsetof
else
	WARN_FLAGS = -Wno-write-strings
endif

WARN_FLAGS += -Wno-unknown-pragmas -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-sign-compare -Wno-reorder -Wno-invalid-offsetof -Wno-float-equal -fdiagnostics-show-option


ifeq ($(OS),Linux)
	CCACHE := $(SRCROOT)/devtools/bin/linux/ccache

	GCC_VER=4.5
	ifeq ($(origin AR), default)
		AR = $(VALVE_BINDIR)/ar rs
	endif
	ifeq ($(origin CC),default)
		CC = $(CCACHE) $(VALVE_BINDIR)/gcc-$(GCC_VER)	
	endif
	ifeq ($(origin CXX), default)
		CXX = $(CCACHE) $(VALVE_BINDIR)/g++-$(GCC_VER)
	endif
	LINK ?= $(CC)

	ifeq ($(TARGET_PLATFORM),linux64)
		VALVE_BINDIR = /valve/bin64
		# nocona = pentium4 + 64bit + MMX, SSE, SSE2, SSE3 - no SSSE3 (that's three s's - added in core2)
		ARCH_FLAGS += -march=nocona 
		LD_SO = ld-linux-x86_64.so.2
		LIBSTDCXX := $(shell $(CXX) -print-file-name=libstdc++.a)
		LIBSTDCXXPIC := $(shell $(CXX) -print-file-name=libstdc++-pic.a)
	else
		VALVE_BINDIR = /valve/bin
		# pentium4 = MMX, SSE, SSE2 - no SSE3 (added in prescott)
		ARCH_FLAGS += -m32 -march=pentium4
		LD_SO = ld-linux.so.2
		LIBSTDCXX := $(shell $(CXX) -print-file-name=libstdc++.so)
		LIBSTDCXXPIC := $(shell $(CXX) -print-file-name=libstdc++.so)
	endif

	GEN_SYM ?= $(SRCROOT)/devtools/gendbg.sh
	ifeq ($(CFG),release)
		STRIP ?= strip -x -S
	#	CFLAGS += -ffunction-sections -fdata-sections
	#	LDFLAGS += -Wl,--gc-sections -Wl,--print-gc-sections
	else
		STRIP ?= true
	endif
	VSIGN ?= true

	SHLIBLDFLAGS = -shared $(LDFLAGS) -Wl,--no-undefined

	ifeq ($(STEAM_BRANCH),1)
		_WRAP := -Xlinker --wrap=
		PATHWRAP = $(_WRAP)fopen $(_WRAP)freopen $(_WRAP)open    $(_WRAP)creat    $(_WRAP)access  $(_WRAP)__xstat \
	    		   $(_WRAP)stat  $(_WRAP)lstat   $(_WRAP)fopen64 $(_WRAP)open64   $(_WRAP)opendir $(_WRAP)__lxstat \
			   $(_WRAP)chmod $(_WRAP)chown   $(_WRAP)lchown  $(_WRAP)symlink  $(_WRAP)link    $(_WRAP)__lxstat64 \
			   $(_WRAP)mknod $(_WRAP)utimes  $(_WRAP)unlink  $(_WRAP)rename   $(_WRAP)utime   $(_WRAP)__xstat64 \
			   $(_WRAP)mount $(_WRAP)mkfifo  $(_WRAP)mkdir   $(_WRAP)rmdir    $(_WRAP)scandir $(_WRAP)realpath
	endif

	LIB_START_EXE = $(PATHWRAP) -static-libgcc -Wl,--start-group
	LIB_END_EXE = -Wl,--end-group -lm -ldl $(LIBSTDCXX) -lpthread 

	LIB_START_SHLIB = $(PATHWRAP) -static-libgcc -Wl,--start-group
	LIB_END_SHLIB = -Wl,--end-group -lm -ldl $(LIBSTDCXXPIC) -lpthread -l:$(LD_SO) -Wl,--version-script=$(SRCROOT)/devtools/version_script.linux.txt

endif

ifeq ($(OS),Darwin)
    	OSXVER := $(shell sw_vers -productVersion)
	CCACHE := $(SRCROOT)/devtools/bin/osx32/ccache
	DEVELOPER_DIR := $(shell /usr/bin/xcode-select -print-path)

	ifeq (,$(findstring 10.7, $(OSXVER))) 
		BUILDING_ON_LION := 0
		COMPILER_BIN_DIR := $(DEVELOPER_DIR)/usr/bin
		SDK_DIR := $(DEVELOPER_DIR)/SDKs
	else
		BUILDING_ON_LION := 1
		COMPILER_BIN_DIR := $(DEVELOPER_DIR)/Toolchains/XcodeDefault.xctoolchain/usr/bin
		SDK_DIR := $(DEVELOPER_DIR)/Platforms/MacOSX.platform/Developer/SDKs
	endif

	SDKROOT ?= $(SDK_DIR)/MacOSX10.6.sdk

	ifeq ($(origin AR), default)
		AR = libtool -static -o
	endif
	ifeq ($(origin CC), default)
		CC = $(COMPILER_BIN_DIR)/clang -Qunused-arguments
	endif
	ifeq ($(origin CXX), default)
		CXX = $(COMPILER_BIN_DIR)/clang++ -Qunused-arguments
	endif
	LINK ?= $(CXX)

	ifeq (($TARGET_PLATFORM),osx64)
		ARCH_FLAGS += -arch x86_64 -m64 -march=core2 
	else ifeq (,$(findstring -arch x86_64,$(GCC_ExtraCompilerFlags)))
		ARCH_FLAGS += -arch i386 -m32 -march=prescott 
	else
		# dirty hack to build a universal binary - don't specify the architecture
		ARCH_FLAGS += -arch i386 -Xarch_i386 -march=prescott -Xarch_x86_64 -march=core2
		BUILDING_MULTI_ARCH=1
	endif

	#FIXME: NOTE:Full path specified because the xcode 4.0 preview has a terribly broken dsymutil, so ref the 3.2 one
	GEN_SYM ?= $(DEVELOPER_DIR)/usr/bin/dsymutil
	ifeq ($(CFG),release)
		STRIP ?= strip -S
	else
		STRIP ?= true
	endif
	VSIGN ?= $(SRCROOT)/devtools/bin/vsign

	SDKROOT ?= $(SDK_DIR)/MacOSX10.6.sdk

	CPPFLAGS += -I$(SDKROOT)/usr/include/malloc
	CFLAGS += -isysroot $(SDKROOT) -mmacosx-version-min=10.5 -fasm-blocks

	LIB_START_EXE = -lm -ldl -lpthread
	LIB_END_EXE = 

	LIB_START_SHLIB = 
	LIB_END_SHLIB = 

	SHLIBLDFLAGS = $(LDFLAGS) -bundle -flat_namespace -undefined suppress -Wl,-dead_strip -Wl,-no_dead_strip_inits_and_terms 

	ifeq (lib,$(findstring lib,$(GAMEOUTPUTFILE)))
		SHLIBLDFLAGS = $(LDFLAGS) -dynamiclib -current_version 1.0 -compatibility_version 1.0 -install_name @loader_path/$(basename $(notdir $(GAMEOUTPUTFILE))).dylib $(SystemLibraries) -Wl,-dead_strip -Wl,-no_dead_strip_inits_and_terms 
	endif

endif

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

OBJ_DIR = ./obj_$(NAME)_$(TARGET_PLATFORM)/$(CFG)
CPP_TO_OBJ = $(CPPFILES:.cpp=.o)
CXX_TO_OBJ = $(CPP_TO_OBJ:.cxx=.oxx)
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

ifeq ($(BUILDING_MULTI_ARCH),1)
	SINGLE_ARCH_CXXFLAGS=$(subst -arch x86_64,,$(CXXFLAGS))
	COMPILE_FILE = \
		$(QUIET_PREFIX) \
		echo "---- COMPILING $(lastword $(subst /, ,$<)) as MULTIARCH----";\
		mkdir -p $(OBJ_DIR) && \
		$(CXX) $(SINGLE_ARCH_CXXFLAGS) $(GENDEP_CXXFLAGS) -o $@ -c $< && \
		$(CXX) $(CXXFLAGS) -o $@ -c $<
else
	COMPILE_FILE = \
		$(QUIET_PREFIX) \
		echo "---- COMPILING $(lastword $(subst /, ,$<)) ----";\
		mkdir -p $(OBJ_DIR) && \
		$(CXX) $(CXXFLAGS) $(GENDEP_CXXFLAGS) -o $@ -c $<
endif

ifneq "$(origin VALVE_NO_AUTO_P4)" "undefined"
	P4_EDIT_START = chmod -R +w
	P4_EDIT_END = || true
	P4_REVERT_START = true
	P4_REVERT_END =
else
	P4_EDIT_START := for f in
	P4_EDIT_END := ; do if [ -n $$f ]; then if [ -d $$f ]; then find $$f -type f -print | p4 -x - edit; else p4 edit $$f; fi; fi; done $(QUIET_ECHO_POSTFIX)
	P4_REVERT_START := for f in  
	P4_REVERT_END := ; do if [ -n $$f ]; then if [ -d $$f ]; then find $$f -type f -print | p4 -x - revert; else p4 revert $$f; fi; fi; done $(QUIET_ECHO_POSTFIX) 
endif

ifeq ($(CONFTYPE),dll)
all: $(OTHER_DEPENDENCIES) $(OBJS) $(GAMEOUTPUTFILE)
	@echo $(GAMEOUTPUTFILE) $(QUIET_ECHO_POSTFIX)
else
all: $(OTHER_DEPENDENCIES) $(OBJS) $(OUTPUTFILE)
	@echo $(OUTPUTFILE) $(QUIET_ECHO_POSTFIX)
endif

.PHONY: clean cleantargets rebuild relink RemoveOutputFile SingleFile


rebuild :
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) clean
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
	$(QUIET_PREFIX) echo "removing $(OBJ_DIR)"
	$(QUIET_PREFIX) rm -rf $(OBJ_DIR)
endif
ifneq "$(OUTPUTFILE)" ""
	$(QUIET_PREFIX) if [ -e $(OUTPUTFILE) ]; then \
		echo "cleaning $(OUTPUTFILE)"; \
		$(P4_REVERT_START) $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT) $(P4_REVERT_END); \
	fi;
endif
ifneq "$(OTHER_DEPENDENCIES)" ""
	$(QUIET_PREFIX) rm -f $(OTHER_DEPENDENCIES)
endif
ifneq "$(GAMEOUTPUTFILE)" ""
	$(QUIET_PREFIX) echo "reverting $(GAMEOUTPUTFILE)"
	$(QUIET_PREFIX) $(P4_REVERT_START) $(GAMEOUTPUTFILE) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_REVERT_END)
endif

# This just deletes the final targets so it'll do a relink next time we build.
cleantargets:
	$(QUIET_PREFIX) rm -f $(OUTPUTFILE) $(GAMEOUTPUTFILE)


$(LIB_File): $(OTHER_DEPENDENCIES) $(OBJS) 
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(LIB_File) $(P4_EDIT_END); 
	$(QUIET_PREFIX) $(AR) $(LIB_File) $(OBJS) $(LIBFILES);

SO_GameOutputFile = $(GAMEOUTPUTFILE)

$(SO_GameOutputFile): $(SO_File)
	$(QUIET_PREFIX) \
	$(P4_EDIT_START) $(GAMEOUTPUTFILE) $(P4_EDIT_END) && \
	echo "----" $(QUIET_ECHO_POSTFIX);\
	echo "---- COPYING TO $@ ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(GAMEOUTPUTFILE) $(P4_EDIT_END);
	$(QUIET_PREFIX) -mkdir -p `dirname $(GAMEOUTPUTFILE)` > /dev/null;
	$(QUIET_PREFIX) cp -v $(OUTPUTFILE) $(GAMEOUTPUTFILE) $(QUIET_ECHO_POSTFIX);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END);
	$(QUIET_PREFIX) $(GEN_SYM) $(GAMEOUTPUTFILE); 
	$(QUIET_PREFIX) -$(STRIP) $(GAMEOUTPUTFILE);
	$(QUIET_PREFIX) $(VSIGN) -signvalve $(GAMEOUTPUTFILE);
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
	echo "---- LINKING $@ ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);\
	\
	$(LINK) $(SHLIBLDFLAGS) $(PROFILE_LINKER_FLAG) -o $(OUTPUTFILE) $(LIB_START_SHLIB) $(OBJS) $(LIBFILES) $(SystemLibraries) $(LIB_END_SHLIB);
	$(VSIGN) -signvalve $(OUTPUTFILE);


$(EXE_File) : $(OTHER_DEPENDENCIES) $(OBJS) $(LIBFILENAMES)
	$(QUIET_PREFIX) \
	echo "----" $(QUIET_ECHO_POSTFIX);\
	echo "---- LINKING EXE $@ ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);\
	\
	$(P4_EDIT_START) $(OUTPUTFILE) $(P4_EDIT_END);\
	$(LINK) $(LDFLAGS) $(PROFILE_LINKER_FLAG) -o $(OUTPUTFILE) $(LIB_START_EXE) $(OBJS) $(LIBFILES) $(SystemLibraries) $(LIB_END_EXE);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(OUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END);
	$(QUIET_PREFIX) $(GEN_SYM) $(OUTPUTFILE);
	$(QUIET_PREFIX) -$(STRIP) $(OUTPUTFILE);
	$(QUIET_PREFIX) $(VSIGN) -signvalve $(OUTPUTFILE);
