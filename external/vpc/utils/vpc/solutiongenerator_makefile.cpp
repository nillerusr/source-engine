//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"


extern void V_MakeAbsoluteCygwinPath( char *pOut, int outLen, const char *pRelativePath );

extern void MakeFriendlyProjectName( char *pchProject )
{
	int strLen =  V_strlen( pchProject );
	for ( int j = 0; j < strLen; j++ )
	{
		if ( pchProject[j] == ' ' )
			pchProject[j] = '_';
		if ( pchProject[j] == '(' || pchProject[j] == ')' )
		{
			V_memmove( pchProject+j, pchProject+j+1, strLen - j );
			strLen--;
		}
	}
}


class CSolutionGenerator_Makefile : public IBaseSolutionGenerator
{
private:
	void GenerateProjectNames( CUtlVector<CUtlString> &projNames, CUtlVector<CDependency_Project*> &projects )
	{
		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];
			char szFriendlyName[256];
			V_strncpy( szFriendlyName, pCurProject->m_ProjectName.String(), sizeof(szFriendlyName) );
			MakeFriendlyProjectName( szFriendlyName );
			projNames[ projNames.AddToTail() ] = szFriendlyName;
		}
	}

public:
	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects )
	{
		// Default extension.
		char szTmpSolutionFilename[MAX_PATH];
		if ( !V_GetFileExtension( pSolutionFilename ) )
		{
			V_snprintf( szTmpSolutionFilename, sizeof( szTmpSolutionFilename ), "%s.mak", pSolutionFilename );
			pSolutionFilename = szTmpSolutionFilename;
		}

		const char *pTargetPlatformName;
		// forestw: if PLATFORM macro exists we should use its value, this accommodates overrides of PLATFORM in .vpc files
		macro_t *pMacro = g_pVPC->FindOrCreateMacro( "PLATFORM", false, NULL );
		if ( pMacro )
		  pTargetPlatformName = pMacro->value.String();
		else
		  pTargetPlatformName = g_pVPC->GetTargetPlatformName();

		Msg( "\nWriting master makefile %s.\n\n", pSolutionFilename );

		// Write the file.
		FILE *fp = fopen( pSolutionFilename, "wt" );
		if ( !fp )
			g_pVPC->VPCError( "Can't open %s for writing.", pSolutionFilename );

		fprintf( fp, "# VPC MASTER MAKEFILE\n\n" );

		fprintf( fp, "# Disable built-in rules/variables. We don't depend on them, and they slow down make processing.\n" );
		fprintf( fp, "MAKEFLAGS += --no-builtin-rules --no-builtin-variables\n" );
		fprintf( fp, "ifeq ($(MAKE_VERBOSE),)\n" );
		fprintf( fp, "MAKEFLAGS += --no-print-directory\n" );
		fprintf( fp, "endif\n\n" );

		fprintf( fp, "ifneq \"$(LINUX_TOOLS_PATH)\" \"\"\n" );
		fprintf( fp, "    TOOL_PATH = $(LINUX_TOOLS_PATH)/\n" );
		fprintf( fp, "    SHELL := $(TOOL_PATH)bash\n" );
		fprintf( fp, "else\n" );
		fprintf( fp, "    SHELL := /bin/bash\n" );
		fprintf( fp, "endif\n" );

		fprintf( fp, "ifndef NO_CHROOT\n" );
		if ( V_stristr( pTargetPlatformName, "64" ) )
		{
			fprintf( fp, "    export CHROOT_NAME ?= $(subst /,_,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))amd64\n");
			fprintf( fp, "    RUNTIME_NAME ?= steamrt_scout_amd64\n");
			fprintf( fp, "    CHROOT_PERSONALITY ?= linux\n");
		}
		else if ( V_stristr( pTargetPlatformName, "32" ) )
		{
			fprintf( fp, "    export CHROOT_NAME ?= $(subst /,_,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))\n");
			fprintf( fp, "    RUNTIME_NAME ?= steamrt_scout_i386\n");
			fprintf( fp, "    CHROOT_PERSONALITY ?= linux32\n");
		}
		else
		{
			g_pVPC->VPCError( "TargetPlatform (%s) doesn't seem to be 32 or 64 bit, can't configure chroot parameters", pTargetPlatformName );
		}
		fprintf( fp, "    CHROOT_CONF := /etc/schroot/chroot.d/$(CHROOT_NAME).conf\n");
		fprintf( fp, "    CHROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/tools/runtime/linux)\n\n");

		fprintf( fp, "    export MAKE_CHROOT = 1\n" );
		fprintf( fp, "    ifneq (\"$(SCHROOT_CHROOT_NAME)\", \"$(CHROOT_NAME)\")\n");
		fprintf( fp, "        SHELL := schroot --chroot $(CHROOT_NAME) -- /bin/bash\n");
		fprintf( fp, "    endif\n");
		fprintf( fp, "endif\n\n" ); // NO_CHROOT

		fprintf( fp, "ECHO = $(TOOL_PATH)echo\n" );
		fprintf( fp, "ETAGS = $(TOOL_PATH)etags\n" );
		fprintf( fp, "FIND = $(TOOL_PATH)find\n" );
		fprintf( fp, "UNAME = $(TOOL_PATH)uname\n" );
		fprintf( fp, "XARGS = $(TOOL_PATH)xargs\n" );
		fprintf( fp, "\n");

		fprintf( fp, "# to control parallelism, set the MAKE_JOBS environment variable\n" );
		fprintf( fp, "ifeq ($(strip $(MAKE_JOBS)),)\n");
		fprintf( fp, "    ifeq ($(shell $(UNAME)),Darwin)\n" );
		fprintf( fp, "        CPUS := $(shell /usr/sbin/sysctl -n hw.ncpu)\n" );
		fprintf( fp, "    endif\n");
		fprintf( fp, "    ifeq ($(shell $(UNAME)),Linux)\n" );
		fprintf( fp, "        CPUS := $(shell $(TOOL_PATH)grep processor /proc/cpuinfo | $(TOOL_PATH)wc -l)\n" );
		fprintf( fp, "    endif\n");
		fprintf( fp, "    MAKE_JOBS := $(CPUS)\n" );
		fprintf( fp, "endif\n\n" );

		fprintf( fp, "ifeq ($(strip $(MAKE_JOBS)),)\n");
		fprintf( fp, "    MAKE_JOBS := 8\n" );
		fprintf( fp, "endif\n\n" );
		// Handle VALVE_NO_PROJECT_DEPS
		fprintf( fp, "# make VALVE_NO_PROJECT_DEPS 1 or empty (so VALVE_NO_PROJECT_DEPS=0 works as expected)\n");
		fprintf( fp, "ifeq ($(strip $(VALVE_NO_PROJECT_DEPS)),1)\n");
		fprintf( fp, "\tVALVE_NO_PROJECT_DEPS := 1\n");
		fprintf( fp, "else\n");
		fprintf( fp, "\tVALVE_NO_PROJECT_DEPS :=\n");
		fprintf( fp, "endif\n\n");

		// Handle VALVE_NO_PROJECT_DEPS
		fprintf( fp, "# make VALVE_NO_PROJECT_DEPS 1 or empty (so VALVE_NO_PROJECT_DEPS=0 works as expected)\n");
		fprintf( fp, "ifeq ($(strip $(VALVE_NO_PROJECT_DEPS)),1)\n");
		fprintf( fp, "\tVALVE_NO_PROJECT_DEPS := 1\n");
		fprintf( fp, "else\n");
		fprintf( fp, "\tVALVE_NO_PROJECT_DEPS :=\n");
		fprintf( fp, "endif\n\n");

		// First, make a target with all the project names.
		fprintf( fp, "# All projects (default target)\n" );
		fprintf( fp, "all: $(CHROOT_CONF)\n" );
		fprintf( fp, "\t$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets\n\n" );

		fprintf( fp, "all-targets : " );

		CUtlVector<CUtlString> projNames;
		GenerateProjectNames( projNames, projects );

		for ( int i=0; i < projects.Count(); i++ )
		{
			fprintf( fp, "%s ", projNames[i].String() );
		}

		fprintf( fp, "\n\n\n# Individual projects + dependencies\n\n" );

		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];

			CUtlVector<CDependency_Project*> additionalProjectDependencies;
			ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );

			fprintf( fp, "%s : $(if $(VALVE_NO_PROJECT_DEPS),,$(CHROOT_CONF) ", projNames[i].String() );

			for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
			{
				if ( i == iTestProject )
					continue;

				CDependency_Project *pTestProject = projects[iTestProject];
				int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
				if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
				{
					fprintf( fp, "%s ", projNames[iTestProject].String() );
				}
			}

			fprintf( fp, ")" ); // Closing $(if) above

			// Now add the code to build this thing.
			char sDirTemp[MAX_PATH], sDir[MAX_PATH];
			V_strncpy( sDirTemp, pCurProject->m_ProjectFilename.String(), sizeof( sDirTemp ) );
			V_StripFilename( sDirTemp );
			V_MakeAbsoluteCygwinPath( sDir, sizeof( sDir ), sDirTemp );

			const char *pFilename = V_UnqualifiedFileName( pCurProject->m_ProjectFilename.String() );

			fprintf( fp, "\n\t@echo \"Building: %s\"", projNames[i].String());
			fprintf( fp, "\n\t@+cd %s && $(MAKE) -f %s $(SUBMAKE_PARAMS) $(CLEANPARAM)", sDir, pFilename );

			fprintf( fp, "\n\n" );
		}

		fprintf( fp, "# this is a bit over-inclusive, but the alternative (actually adding each referenced c/cpp/h file to\n" );
		fprintf( fp, "# the tags file) seems like more work than it's worth.  feel free to fix that up if it bugs you. \n" );
		fprintf( fp, "TAGS:\n" );
		fprintf( fp, "\t@rm -f TAGS\n" );
		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];
			char sDirTemp[MAX_PATH], sDir[MAX_PATH];
			V_strncpy( sDirTemp, pCurProject->m_ProjectFilename.String(), sizeof( sDirTemp ) );
			V_StripFilename( sDirTemp );
			V_MakeAbsoluteCygwinPath( sDir, sizeof( sDir ), sDirTemp );
			fprintf( fp, "\t@$(FIND) %s -name \'*.cpp\' -print0 | $(XARGS) -0 $(ETAGS) --declarations --ignore-indentation --append\n", sDir );
			fprintf( fp, "\t@$(FIND) %s -name \'*.h\' -print0 | $(XARGS) -0 $(ETAGS) --language=c++ --declarations --ignore-indentation --append\n", sDir );
			fprintf( fp, "\t@$(FIND) %s -name \'*.c\' -print0 | $(XARGS) -0 $(ETAGS) --declarations --ignore-indentation --append\n", sDir );
		}
		fprintf( fp, "\n\n" );

		fprintf( fp, "\n# Mark all the projects as phony or else make will see the directories by the same name and think certain targets \n\n" );
		fprintf( fp, ".PHONY: TAGS showtargets regen showregen clean cleantargets cleanandremove relink " );
		for ( int i=0; i < projects.Count(); i++ )
		{
			fprintf( fp, "%s ", projNames[i].String() );
		}
		fprintf( fp, "\n\n\n" );

		fprintf( fp, "\n# The standard clean command to clean it all out.\n" );
		fprintf( fp, "\nclean: \n" );
		fprintf( fp, "\t@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets CLEANPARAM=clean\n\n\n" );

		fprintf( fp, "\n# clean targets, so we re-link next time.\n" );
		fprintf( fp, "\ncleantargets: \n" );
		fprintf( fp, "\t@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets CLEANPARAM=cleantargets\n\n\n" );

		fprintf( fp, "\n# p4 edit and remove targets, so we get an entirely clean build.\n" );
		fprintf( fp, "\ncleanandremove: \n" );
		fprintf( fp, "\t@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets CLEANPARAM=cleanandremove\n\n\n" );

		fprintf( fp, "\n#relink\n" );
		fprintf( fp, "\nrelink: cleantargets \n" );
		fprintf( fp, "\t@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets\n\n\n" );



		// Create the showtargets target.
		fprintf( fp, "\n# Here's a command to list out all the targets\n\n" );
		fprintf( fp, "\nshowtargets: \n" );
		fprintf( fp, "\t@$(ECHO) '-------------------' && \\\n" );
		fprintf( fp, "\t$(ECHO) '----- TARGETS -----' && \\\n" );
		fprintf( fp, "\t$(ECHO) '-------------------' && \\\n" );
		fprintf( fp, "\t$(ECHO) 'clean' && \\\n" );
		fprintf( fp, "\t$(ECHO) 'regen' && \\\n" );
		fprintf( fp, "\t$(ECHO) 'showregen' && \\\n" );
		for ( int i=0; i < projects.Count(); i++ )
		{
			fprintf( fp, "\t$(ECHO) '%s'", projNames[i].String() );
			if ( i != projects.Count()-1 )
				fprintf( fp, " && \\" );
			fprintf( fp, "\n" );
		}
		fprintf( fp, "\n\n" );


		// Create the regen target.
		fprintf( fp, "\n# Here's a command to regenerate this makefile\n\n" );
		fprintf( fp, "\nregen: \n" );
		fprintf( fp, "\t" );
		ICommandLine *pCommandLine = CommandLine();
		for ( int i=0; i < pCommandLine->ParmCount(); i++ )
		{
			fprintf( fp, "%s ", pCommandLine->GetParm( i ) );
		}
		fprintf( fp, "\n\n" );


		// Create the showregen target.
		fprintf( fp, "\n# Here's a command to list out all the targets\n\n" );
		fprintf( fp, "\nshowregen: \n" );
		fprintf( fp, "\t@$(ECHO) " );
		for ( int i=0; i < pCommandLine->ParmCount(); i++ )
		{
			fprintf( fp, "%s ", pCommandLine->GetParm( i ) );
		}
		fprintf( fp, "\n\n" );

		// Auto-create the chroot if it's not there
		fprintf( fp, "ifdef CHROOT_CONF\n"
				"$(CHROOT_CONF): $(CHROOT_DIR)/$(RUNTIME_NAME)/timestamp\n"
				"$(CHROOT_CONF): SHELL = /bin/bash\n"
				"$(CHROOT_DIR)/$(RUNTIME_NAME)/timestamp: $(CHROOT_DIR)/$(RUNTIME_NAME).tar.xz\n"
				"\t@echo \"Configuring schroot at $(CHROOT_DIR) (requires sudo)\"\n"
				"\tsudo $(CHROOT_DIR)/configure_runtime.sh ${CHROOT_NAME} $(RUNTIME_NAME) $(CHROOT_PERSONALITY)\n"
				"endif\n");


				fclose( fp );
	}

	void ResolveAdditionalProjectDependencies(
		CDependency_Project *pCurProject,
		CUtlVector<CDependency_Project*> &projects,
		CUtlVector<CDependency_Project*> &additionalProjectDependencies )
	{
		for ( int i=0; i < pCurProject->m_AdditionalProjectDependencies.Count(); i++ )
		{
			const char *pLookingFor = pCurProject->m_AdditionalProjectDependencies[i].String();

			int j;
			for ( j=0; j < projects.Count(); j++ )
			{
				if ( V_stricmp( projects[j]->m_ProjectName.String(), pLookingFor ) == 0 )
					break;
			}

			if ( j == projects.Count() )
				g_pVPC->VPCError( "Project %s lists '%s' in its $AdditionalProjectDependencies, but there is no project by that name in the selected projects.", pCurProject->GetName(), pLookingFor );

			additionalProjectDependencies.AddToTail( projects[j] );
		}
	}

	const char* FindInFile( const char *pFilename, const char *pFileData, const char *pSearchFor )
	{
		const char *pPos = V_stristr( pFileData, pSearchFor );
		if ( !pPos )
			g_pVPC->VPCError( "Can't find ProjectGUID in %s.", pFilename );

		return pPos + V_strlen( pSearchFor );
	}
};


static CSolutionGenerator_Makefile g_SolutionGenerator_Makefile;
IBaseSolutionGenerator* GetMakefileSolutionGenerator()
{
	return &g_SolutionGenerator_Makefile;
}



