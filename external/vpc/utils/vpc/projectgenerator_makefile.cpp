//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "vpc.h"
#include "baseprojectdatacollector.h"
#include "tier1/utlstack.h"
#include "projectgenerator_codelite.h"

static const char *k_pszBase_Makefile = "$(SRCROOT)/devtools/makefile_base_posix.mak";

static const char *g_pOption_BufferSecurityCheck = "$BufferSecurityCheck";
static const char *g_pOption_CustomBuildStepCommandLine = "$CustomBuildStep/$CommandLine";
static const char *g_pOption_PostBuildEventCommandLine = "$PostBuildEvent/$CommandLine";
static const char *g_pOption_CompileAs = "$CompileAs";
static const char *g_pOption_ConfigurationType = "$ConfigurationType";
static const char *g_pOption_Description = "$Description";
static const char *g_pOption_EntryPoint = "$EntryPoint";
static const char *g_pOption_ExtraCompilerFlags = "$GCC_ExtraCompilerFlags";
static const char *g_pOption_ExtraLinkerFlags = "$GCC_ExtraLinkerFlags";
static const char *g_pOption_CustomVersionScript = "$GCC_CustomVersionScript";
static const char *g_pOption_ForceInclude = "$ForceIncludes";
static const char *g_pOption_IgnoreAllDefaultLibraries = "$IgnoreAllDefaultLibraries";
static const char *g_pOption_LocalFrameworks = "$LocalFrameworks";
static const char *g_pOption_LowerCaseFileNames = "$LowerCaseFileNames";
static const char *g_pOption_OptimizerLevel = "$OptimizerLevel";
static const char *g_pOption_AdditionalDependencies = "$AdditionalDependencies";
static const char *g_pOption_Outputs = "$Outputs";
static const char *g_pOption_PrecompiledHeader = "$Create/UsePrecompiledHeader";
static const char *g_pOption_PrecompiledHeaderFile = "$PrecompiledHeaderFile";
static const char *g_pOption_SymbolVisibility = "$SymbolVisibility";
static const char *g_pOption_SystemFrameworks = "$SystemFrameworks";
static const char *g_pOption_SystemLibraries = "$SystemLibraries";
static const char *g_pOption_UsePCHThroughFile = "$Create/UsePCHThroughFile";
static const char *g_pOption_TargetCopies = "$TargetCopies";
static const char *g_pOption_TreatWarningsAsErrors = "$TreatWarningsAsErrors";

// These are the only properties we care about for makefiles.
static const char *g_pRelevantProperties[] =
{
  g_pOption_AdditionalIncludeDirectories,
  g_pOption_CompileAs,
  g_pOption_OptimizerLevel,
  g_pOption_OutputFile,
  g_pOption_GameOutputFile,
  g_pOption_SymbolVisibility,
  g_pOption_PreprocessorDefinitions,
  g_pOption_ConfigurationType,
  g_pOption_ImportLibrary,
  g_pOption_PrecompiledHeader,
  g_pOption_UsePCHThroughFile,
  g_pOption_PrecompiledHeaderFile,
  g_pOption_CustomBuildStepCommandLine,
  g_pOption_PostBuildEventCommandLine,
  g_pOption_AdditionalDependencies,
  g_pOption_Outputs,
  g_pOption_Description,
  g_pOption_SystemLibraries,
  g_pOption_SystemFrameworks,
  g_pOption_LocalFrameworks,
  g_pOption_ExtraCompilerFlags,
  g_pOption_ExtraLinkerFlags,
  g_pOption_CustomVersionScript,
  g_pOption_EntryPoint,
  g_pOption_IgnoreAllDefaultLibraries,
  g_pOption_BufferSecurityCheck,
  g_pOption_ForceInclude,
  g_pOption_TargetCopies,
  g_pOption_TreatWarningsAsErrors,
  g_pOption_LowerCaseFileNames,
};


static CRelevantPropertyNames g_RelevantPropertyNames =
{
  g_pRelevantProperties,
  V_ARRAYSIZE( g_pRelevantProperties )
};

void MakeFriendlyProjectName( char *pchProject );

void V_MakeAbsoluteCygwinPath( char *pOut, int outLen, const char *pRelativePath )
{
  // While generating makefiles under Win32, we must translate drive letters like c:\
  // to Cygwin-style paths like /cygdrive/c/
#ifdef _WIN32
  char tmp[MAX_PATH];
  V_MakeAbsolutePath( tmp, sizeof( tmp ), pRelativePath );
  V_FixSlashes( tmp, '/' );

  if ( tmp[0] != 0 && tmp[1] == ':' && tmp[2] == '/' )
  {
    // Ok, this is an absolute path
    V_snprintf( pOut, outLen, "/cygdrive/%c/", tmp[0] );
    V_strncat( pOut, &tmp[3], outLen );
  }
  else
#endif // _WIN32
  {
    V_MakeAbsolutePath( pOut, outLen, pRelativePath );
    V_RemoveDotSlashes( pOut );
  }
}

// caller is responsible for free'ing (or leaking) the allocated buffer
static const char* UsePOSIXSlashes( const char *pStr )
{
    int len = V_strlen( pStr ) + 2;
    char *str = (char*)malloc(len*sizeof(char));
    V_strncpy( str, pStr, len );
    for ( int i = 0; i < len; i++ )
    {
        if ( str[i] == '\\' )
        {
            // allow escaping of bash special characters
            if ( i+1 < len && ( str[i+1] != '"' && str[i+1] != '$' &&
                               str[i+1] != '\'' && str[i+1] != '\\' ) )
                str[i] = '/';
        }
        if ( str[i] == '\0' )
            break;
    }
    return str;
}


// pExt should be the bare extension without the . in front. i.e. "h", "cpp", "lib".
static inline bool CheckExtension( const char *pFilename, const char *pExt )
{
  Assert( pExt[0] != '.' );

  int nFilenameLen = V_strlen( pFilename );
  int nExtensionLen = V_strlen( pExt );

  return (nFilenameLen > nExtensionLen && pFilename[nFilenameLen-nExtensionLen-1] == '.' && V_stricmp( &pFilename[nFilenameLen-nExtensionLen], pExt ) == 0 );
}


static bool CheckExtensions( const char *pFilename, const char **ppExtensions )
{
  for ( int i=0; ppExtensions[i] != NULL; i++ )
  {
    if ( CheckExtension( pFilename, ppExtensions[i] ) )
      return true;
  }
  return false;
}


static void GetObjFilenameForFile( const char *pConfigName, const char *pFilename, char *pOut, int maxLen )
{
  char sBaseFilename[MAX_PATH];
  V_FileBase( pFilename, sBaseFilename, sizeof( sBaseFilename ) );
  //V_strlower( sBaseFilename );

  // We had .cxx files -> .oxx, but this macro:
  //   GENDEP_CXXFLAGS = -MD -MP -MF $(@:.o=.P)
  // was turning into -MF blah.oxx, which led to blah.oxx containing the file dependencies.
  // This obviously failed to link. Quick fix is to just have .cxx -> .o like everything else.
  //$ const char *pObjExtension = (CheckExtension( pFilename, "cxx" ) ? "oxx" : "o");
  const char *pObjExtension = "o";

  V_snprintf( pOut, maxLen, "$(OBJ_DIR)/%s.%s", sBaseFilename, pObjExtension );
}


// This class drastically accelerates looking up which file creates which precompiled header.
class CPrecompiledHeaderAccel
{
public:
  void Setup( CUtlDict<CFileConfig*,int> &files, CFileConfig *pBaseConfig )
  {
    for ( int i=files.First(); i != files.InvalidIndex(); i=files.Next( i ) )
    {
      CFileConfig *pFile = files[i];

      for ( int iSpecific=pFile->m_Configurations.First(); iSpecific != pFile->m_Configurations.InvalidIndex(); iSpecific=pFile->m_Configurations.Next( iSpecific ) )
      {
        CSpecificConfig *pSpecific = pFile->m_Configurations[iSpecific];
        if ( pSpecific->m_bFileExcluded )
          continue;

        // Does this file create a precompiled header?
        const char *pPrecompiledHeaderOption = pSpecific->GetOption( g_pOption_PrecompiledHeader );
        if ( pPrecompiledHeaderOption && V_stristr( pPrecompiledHeaderOption, "Create" ) )
        {
          // Ok, which header do we scan through?
          const char *pUsePCHThroughFile = pSpecific->GetOption( g_pOption_UsePCHThroughFile );
          if ( !pUsePCHThroughFile )
          {
            g_pVPC->VPCError( "File %s creates a precompiled header in config %s but no UsePCHThroughFile option specified.", pFile->m_Filename.String(), pSpecific->GetConfigName() );
          }

          char sLookup[1024];
          V_snprintf( sLookup, sizeof( sLookup ), "%s__%s", pSpecific->GetConfigName(), pUsePCHThroughFile );

          if ( m_Lookup.Find( sLookup ) != m_Lookup.InvalidIndex() )
          {
            g_pVPC->VPCError( "File %s has UsePCHThroughFile of %s but another file already does.", pFile->m_Filename.String(), pUsePCHThroughFile );
          }

          m_Lookup.Insert( sLookup, pFile );
        }
      }
    }
  }

  CFileConfig* FindFileThatCreatesPrecompiledHeader( const char *pConfigName, const char *pUsePCHThroughFile )
  {
    char sLookup[1024];
    V_snprintf( sLookup, sizeof( sLookup ), "%s__%s", pConfigName, pUsePCHThroughFile );

    int i = m_Lookup.Find( sLookup );
    if ( i == m_Lookup.InvalidIndex() )
      return NULL;
    else
      return m_Lookup[i];
  }

private:
  // This indexes whatever file creates a certain precompiled header for a certain config.
  // These are indexed as <config name>_<pchthroughfile>.
  // So an entry might look like release_cbase.h
  CUtlDict<CFileConfig*,int> m_Lookup;
};



class CProjectGenerator_Makefile : public CBaseProjectDataCollector
{
public:

  typedef CBaseProjectDataCollector BaseClass;

  CProjectGenerator_Makefile() : BaseClass( &g_RelevantPropertyNames )
  {
  }

  virtual void Setup()
  {
  }

  virtual const char* GetProjectFileExtension()
  {
    return "mak";
  }

  virtual void EndProject()
  {
    const char *pMakefileFilename = g_pVPC->GetOutputFilename();

    CUtlString strProjectName = GetProjectName();
    bool bProjectIsCurrent = g_pVPC->IsProjectCurrent( pMakefileFilename, false );
    if ( g_pVPC->IsForceGenerate() || !bProjectIsCurrent )
    {
      g_pVPC->VPCStatus( true, "Saving makefile project for: '%s' File: '%s'", strProjectName.String(), g_pVPC->GetOutputFilename() );
      WriteMakefile( pMakefileFilename );
    }

    const char *pTargetPlatformName = g_pVPC->GetTargetPlatformName();
    if ( !pTargetPlatformName )
      g_pVPC->VPCError( "GetTargetPlatformName failed." );

    if ( !V_stricmp( pTargetPlatformName, "LINUX32" )  || !V_stricmp( pTargetPlatformName, "LINUX64" ) )
    {
      if ( g_pVPC->IsForceGenerate() || !bProjectIsCurrent )
      {
        // Write a CodeLite project as well.
        char sFilename[MAX_PATH];
        V_StripExtension( g_pVPC->GetOutputFilename(), sFilename, sizeof( sFilename ) );
        CProjectGenerator_CodeLite codeLiteGenerator;
        codeLiteGenerator.GenerateCodeLiteProject( this, sFilename, pMakefileFilename );
      }
      Term();
    }
    else
    {
      Term();
    }
  }

  void WriteSourceFilesList( FILE *fp, const char *pListName, const char **pExtensions, const char *pConfigName )
  {
    fprintf( fp, "%s= \\\n", pListName );
    for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
    {
      CFileConfig *pFileConfig = m_Files[i];
      if ( pFileConfig->IsExcludedFrom( pConfigName ) )
        continue;

      const char *pFilename = m_Files[i]->m_Filename.String();
      if ( CheckExtensions( pFilename, pExtensions ) )
      {
        if ( m_bForceLowerCaseFileName )
          fprintf( fp, "    %s \\\n", UsePOSIXSlashes( V_strlower((char *)pFilename) ) );
        else
          fprintf( fp, "    %s \\\n", UsePOSIXSlashes( pFilename ) );
      }
    }
    fprintf( fp, "\n\n" );
  }

  void WriteNonConfigSpecificStuff( FILE *fp )
  {
    fprintf( fp, "ifneq \"$(LINUX_TOOLS_PATH)\" \"\"\n" );
    fprintf( fp, "TOOL_PATH = $(LINUX_TOOLS_PATH)/\n" );
    fprintf( fp, "endif\n\n" );

    // NAME
    char szName[256];
    V_strncpy( szName, m_ProjectName.String(), sizeof(szName) );
    MakeFriendlyProjectName( szName );
    fprintf( fp, "NAME=%s\n", szName );

    // SRCDIR
    char sSrcRootRelative[MAX_PATH];
    g_pVPC->ResolveMacrosInString( "$SRCDIR", sSrcRootRelative, sizeof( sSrcRootRelative ) );

    fprintf( fp, "SRCROOT=%s\n", UsePOSIXSlashes( sSrcRootRelative ) );

    // TargetPlatformName
    const char *pTargetPlatformName;
    // forestw: if PLATFORM macro exists we should use its value, this accommodates overrides of PLATFORM in .vpc files
    macro_t *pMacro = g_pVPC->FindOrCreateMacro( "PLATFORM", false, NULL );
    if ( pMacro )
      pTargetPlatformName = pMacro->value.String();
    else
      pTargetPlatformName = g_pVPC->GetTargetPlatformName();
    if ( !pTargetPlatformName )
      g_pVPC->VPCError( "GetTargetPlatformName failed." );
    fprintf( fp, "TARGET_PLATFORM=%s\n", pTargetPlatformName );
    fprintf( fp, "TARGET_PLATFORM_EXT=%s\n", g_pVPC->IsDedicatedBuild() ? "_srv" : "" );
    fprintf( fp, "USE_VALVE_BINDIR=%s\n", ( g_pVPC->UseValveBinDir() ? "1" : "0" ) );

    fprintf( fp, "PWD:=$(shell $(TOOL_PATH)pwd)\n" );


    // Select debug config if no config is specified.
    fprintf( fp, "# If no configuration is specified, \"release\" will be used.\n" );
    fprintf( fp, "ifeq \"$(CFG)\" \"\"\n" );
    fprintf( fp, "\tCFG = release\n" );
    fprintf( fp, "endif\n\n" );
  }

  struct Filename_t
  {
    char szFilename[MAX_PATH];
    int iBasename;
  };

  class FileSortSortFunc
  {
  public:
    bool Less( const CFileConfig * const & src1, const CFileConfig * const & src2, void *pCtx )
    {
      return src1->m_nInsertOrder < src2->m_nInsertOrder;
    }
  };

  void WriteVpcMacroDefines( CSpecificConfig *pConfig, FILE *fp )
  {
    // Add VPC macros marked to become defines.
    CUtlVector< macro_t* > macroDefines;
    g_pVPC->GetMacrosMarkedForCompilerDefines( macroDefines );
    for ( int i=0; i < macroDefines.Count(); i++ )
    {
      macro_t *pMacro = macroDefines[i];
      fprintf( fp, "-D%s=%s ", pMacro->name.String(), pMacro->value.String() );
    }
  }

  void WriteConfigSpecificStuff( CSpecificConfig *pConfig, FILE *fp, CPrecompiledHeaderAccel *pAccel, CSpecificConfig *pConfig1 )
  {
    KeyValues *pKV = pConfig->m_pKV;

    // If we've got a pConfig1, then that means pConfig0 == pConfig1, except for $PreprocessorDefinitions. So don't special
    // case anything other than that one section.
    if( !pConfig1 )
    {
      fprintf( fp, "#\n#\n# CFG=%s\n#\n#\n\n", pConfig->GetConfigName() );
      fprintf( fp, "ifeq \"$(CFG)\" \"%s\"\n\n", pConfig->GetConfigName() );
    }

    // GCC_ExtraCompilerFlags
    // Hopefully, they don't ever need to use backslashes because we're turning them into forward slashes here.
    // If that does become a problem, we can put some token around the pathnames we need to be fixed up and leave the rest alone.
    fprintf( fp, "GCC_ExtraCompilerFlags=%s\n", UsePOSIXSlashes( pKV->GetString( g_pOption_ExtraCompilerFlags, "" ) ) );

    // GCC_ExtraLinkerFlags
    fprintf( fp, "GCC_ExtraLinkerFlags=%s\n", pKV->GetString( g_pOption_ExtraLinkerFlags, "" ) );

    // GCC_CustomVersionScript
    fprintf( fp, "GCC_CustomVersionScript=%s\n", pKV->GetString( g_pOption_CustomVersionScript, "" ) );

    // EntryPoint
    fprintf( fp, "EntryPoint=%s\n", pKV->GetString( g_pOption_EntryPoint, "" ) );

    // IgnoreAllDefaultLibraries
    fprintf( fp, "IgnoreAllDefaultLibraries=%s\n", pKV->GetString( g_pOption_IgnoreAllDefaultLibraries, "no" ) );

    // BufferSecurityCheck
    fprintf( fp, "BufferSecurityCheck=%s\n", pKV->GetString( g_pOption_BufferSecurityCheck, "Yes" ) );

    // SymbolVisibility
    fprintf( fp, "SymbolVisibility=%s\n", pKV->GetString( g_pOption_SymbolVisibility, "hidden" ) );

    // TreatWarningsAsErrors
    fprintf( fp, "TreatWarningsAsErrors=%s\n", pKV->GetString( g_pOption_TreatWarningsAsErrors, "false" ) );

    // OptimizerLevel
    fprintf( fp, "OptimizerLevel=%s\n", pKV->GetString( g_pOption_OptimizerLevel, "$(SAFE_OPTFLAGS_GCC_422)" ) );

    // system libraries
    {
      fprintf( fp, "SystemLibraries=" );
      {
        CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
        for ( int i=0; i < libs.Count(); i++ )
        {
          fprintf( fp, "-l%s ", libs[i] );
        }
      }
      if ( !V_stricmp( g_pVPC->GetTargetPlatformName(), "OSX32" )  || !V_stricmp( g_pVPC->GetTargetPlatformName(), "OSX64" ) )
      {
        char rgchFrameworkCompilerFlags[1024]; rgchFrameworkCompilerFlags[0] = '\0';
        CSplitString systemFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
        for ( int i=0; i < systemFrameworks.Count(); i++ )
        {
          fprintf( fp, "-framework %s ", systemFrameworks[i] );
        }
        CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
        for ( int i=0; i < localFrameworks.Count(); i++ )
        {
          char rgchFrameworkName[MAX_PATH];
          V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );
          V_StripFilename( localFrameworks[i] );
          fprintf( fp, "-F%s ", localFrameworks[i] );
          fprintf( fp, "-framework %s ", rgchFrameworkName );
          strcat( rgchFrameworkCompilerFlags, "-F" );
          strcat( rgchFrameworkCompilerFlags, localFrameworks[i] );
        }
        fprintf( fp, "\n" );
        if ( rgchFrameworkCompilerFlags[0] )
          // the colon here is important - and should probably get percolated to more places in our generated
          // makefiles - it means to perform the assignment once, rather than at evaluation time
          fprintf( fp, "GCC_ExtraCompilerFlags:=$(GCC_ExtraCompilerFlags) %s\n", rgchFrameworkCompilerFlags );
      }
      else
        fprintf( fp, "\n" );
    }

    macro_t *pMacro = g_pVPC->FindOrCreateMacro( "_DLL_EXT", false, NULL );
    if ( pMacro )
      fprintf( fp, "DLL_EXT=%s\n", pMacro->value.String() );

    pMacro = g_pVPC->FindOrCreateMacro( "_SYM_EXT", false, NULL );
    if ( pMacro )
      fprintf( fp, "SYM_EXT=%s\n", pMacro->value.String() );

    // ForceIncludes
    {
      CSplitString outStrings( pKV->GetString( g_pOption_ForceInclude ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
      fprintf( fp, "FORCEINCLUDES= " );
      for ( int i=0; i < outStrings.Count(); i++ )
      {
        if ( V_strlen( outStrings[i] ) > 2 )
          fprintf( fp, "-include %s ", UsePOSIXSlashes( outStrings[i] ) );
      }
    }
    fprintf( fp, "\n" );

    // DEFINES
    if( !pConfig1 )
    {
      CSplitString outStrings( pKV->GetString( g_pOption_PreprocessorDefinitions ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
      fprintf( fp, "DEFINES= " );
      for ( int i=0; i < outStrings.Count(); i++ )
      {
        fprintf( fp, "-D%s ", outStrings[i] );
      }

      WriteVpcMacroDefines( pConfig, fp );
      fprintf( fp, "\n" );
    }
    else
    {
      // pConfig0 and pConfig1 are the same other than this section. So write just this one out something like:
      //   ifeq "$(CFG)" "debug"
      //   DEFINES += -DDEBUG -D_DEBUG -DPOSIX ...
      //   else
      //   DEFINES += -DNDEBUG -DPOSIX ...
      //   endif
      fprintf( fp, "ifeq \"$(CFG)\" \"%s\"\n", pConfig->GetConfigName() );

      CSplitString outStrings0( pKV->GetString( g_pOption_PreprocessorDefinitions ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
      fprintf( fp, "DEFINES += " );
      for ( int i=0; i < outStrings0.Count(); i++ )
      {
        fprintf( fp, "-D%s ", outStrings0[i] );
      }
      WriteVpcMacroDefines( pConfig, fp );

      fprintf( fp, "\nelse\n" );

      CSplitString outStrings1( pConfig1->m_pKV->GetString( g_pOption_PreprocessorDefinitions ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
      fprintf( fp, "DEFINES += " );
      for ( int i=0; i < outStrings1.Count(); i++ )
      {
        fprintf( fp, "-D%s ", outStrings1[i] );
      }
      WriteVpcMacroDefines( pConfig1, fp );

      fprintf( fp, "\nendif\n" );
    }

    // INCLUDEDIRS
    {
      CSplitString outStrings( pKV->GetString( g_pOption_AdditionalIncludeDirectories ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
      fprintf( fp, "INCLUDEDIRS += " );
      for ( int i=0; i < outStrings.Count(); i++ )
      {
        char sDir[MAX_PATH];
        V_strncpy( sDir, outStrings[i], sizeof( sDir ) );
        if ( !V_stricmp( sDir, "$(IntDir)" ) )
          V_strncpy( sDir, "$(OBJ_DIR)", sizeof( sDir ) );

        V_FixSlashes( sDir, '/' );
        fprintf( fp, "%s ", sDir );
      }
      fprintf( fp, "\n" );
    }
    // CONFTYPE
    if ( V_stristr( pKV->GetString( g_pOption_ConfigurationType ), "dll" ) )
    {
      fprintf( fp, "CONFTYPE=dll\n" );

      // Write ImportLibrary for dll (so) builds.
      const char *pRelative = pKV->GetString( g_pOption_ImportLibrary, "" );
      fprintf( fp, "IMPORTLIBRARY=%s\n", UsePOSIXSlashes( pRelative ) );
    }
    else if ( V_stristr( pKV->GetString( g_pOption_ConfigurationType ), "lib" ) )
    {
      fprintf( fp, "CONFTYPE=lib\n" );
    }
    else if ( V_stristr( pKV->GetString( g_pOption_ConfigurationType ), "exe" ) )
    {
      fprintf( fp, "CONFTYPE=exe\n" );
    }
    else
    {
      fprintf( fp, "CONFTYPE=***UNKNOWN***\n" );
    }

    // GameOutputFile is where it copies OutputFile to.
    fprintf( fp, "GAMEOUTPUTFILE=%s\n", UsePOSIXSlashes( pKV->GetString( g_pOption_GameOutputFile, "" ) ) );

    // TargetCopies are where OutputFile copies are placed.
    fprintf( fp, "TARGETCOPIES=%s\n", UsePOSIXSlashes( pKV->GetString( g_pOption_TargetCopies, "" ) ) );

    // OutputFile is where it builds to.
    char szFixedOutputFile[MAX_PATH];
    V_strncpy( szFixedOutputFile, pKV->GetString( g_pOption_OutputFile ), sizeof( szFixedOutputFile ) );
    V_FixSlashes( szFixedOutputFile, '/' );
    // This file uses a custom build step.
    char sFormattedOutputFile[MAX_PATH];
    char szAbsPath[MAX_PATH];
    V_MakeAbsolutePath( szAbsPath, sizeof(szAbsPath), szFixedOutputFile );
    DoStandardVisualStudioReplacements( szFixedOutputFile, szAbsPath, sFormattedOutputFile, sizeof( sFormattedOutputFile ) );

    fprintf( fp, "OUTPUTFILE=%s\n", sFormattedOutputFile );

    fprintf( fp, "\n\n" );

    // post build event
    char rgchPostBuildCommand[2048]; rgchPostBuildCommand[0] = '\0';
    if ( pKV->GetString( g_pOption_PostBuildEventCommandLine, NULL ) )
    {
      V_strncpy( rgchPostBuildCommand, pKV->GetString( g_pOption_PostBuildEventCommandLine, NULL ), sizeof( rgchPostBuildCommand ) );
      // V_StripPrecedingAndTrailingWhitespace( rgchPostBuildCommand );
    }
    if ( V_strlen( rgchPostBuildCommand ) )
      fprintf( fp, "POSTBUILDCOMMAND=%s\n", UsePOSIXSlashes( rgchPostBuildCommand ) );
    else
      fprintf( fp, "POSTBUILDCOMMAND=/bin/true\n" );

    fprintf( fp, "\n\n" );


    // Write all the filenames.
    const char *sSourceFileExtensions[] = { "cpp", "cxx", "cc", "c", "mm", NULL };
    fprintf( fp, "\n" );
    WriteSourceFilesList( fp, "CPPFILES", (const char**)sSourceFileExtensions, pConfig->GetConfigName() );

    // LIBFILES
    char sImportLibraryFile[MAX_PATH];
    const char *pRelative = pKV->GetString( g_pOption_ImportLibrary, "" );
    V_strncpy( sImportLibraryFile, UsePOSIXSlashes( pRelative ), sizeof( sImportLibraryFile ) );
    V_RemoveDotSlashes( sImportLibraryFile );

    char sOutputFile[MAX_PATH];
    const char *pOutputFile = pKV->GetString( g_pOption_OutputFile, "" );
    V_strncpy( sOutputFile, UsePOSIXSlashes( pOutputFile ), sizeof( sOutputFile ) );
    V_RemoveDotSlashes( sOutputFile );

    fprintf( fp, "LIBFILES = \\\n" );

    // Get original order the link files were specified in the .vpc files. See:
    //  http://stackoverflow.com/questions/45135/linker-order-gcc
    // TL;DR. Gcc does a single pass through the list of libraries to resolve references.
    //  If library A depends on symbols in library B, library A should appear first so we
    //  need to restore the original order to allow users to control link order via
    //  their .vpc files.
    CUtlSortVector< CFileConfig *, FileSortSortFunc > OriginalSort;

    for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
    {
      CFileConfig *pFileConfig = m_Files[i];
      OriginalSort.InsertNoSort( pFileConfig );
    }
    OriginalSort.RedoSort();


    CUtlVector< Filename_t > ImportLib;
    CUtlVector< Filename_t > StaticLib;

    for ( int i=0; i < OriginalSort.Count(); i++ )
    {
      CFileConfig *pFileConfig = OriginalSort[i];
      if ( pFileConfig->IsExcludedFrom( pConfig->GetConfigName() ) )
        continue;

      Filename_t Filename;

      Filename.iBasename = 0;

      V_strncpy( Filename.szFilename, UsePOSIXSlashes( pFileConfig->m_Filename.String() ), sizeof( Filename.szFilename ) );
      const char *pFilename = Filename.szFilename;
      if ( IsLibraryFile( pFilename ) )
      {
        char *pchFileName = (char*)V_strrchr( pFilename, '/' ) + 1;
        if (( !sImportLibraryFile[0] || V_stricmp( sImportLibraryFile, pFilename )) && // only link this as a library if it isn't our own output!
            ( !sOutputFile[0] || V_stricmp( sOutputFile, pFilename)))
        {
          char szExt[32];
          V_ExtractFileExtension( pFilename, szExt, sizeof(szExt) );
          if (!( !V_stricmp( g_pVPC->GetTargetPlatformName(), "OSX32" )  || !V_stricmp( g_pVPC->GetTargetPlatformName(), "OSX64" ) ) &&
              IsLibraryFile( pFilename ) && (pchFileName-1) && pchFileName[0] == 'l' && pchFileName[1] == 'i' && pchFileName[2] == 'b'
              && szExt[0] != 'a' ) // its a lib ext but not an archive file, link like a library
          {
            *(pchFileName-1) = 0;

            // Cygwin import libraries use ".dll.a", so get rid of any file extensions here.
            char *pExt;
            while ( 1 )
            {
              pExt = (char*)V_strrchr( pchFileName, '.' );
              if ( !pExt || V_strrchr( pchFileName, '/' ) > pExt || V_strrchr( pchFileName, '\\' ) > pExt )
                break;

              *pExt = 0;
            }

            // +3 to dodge the lib ext
            Filename.iBasename = ( pchFileName - pFilename ) + 3;
            ImportLib.AddToTail( Filename );
          }
          else
          {
            StaticLib.AddToTail( Filename );
          }
        }
      }
    }

    // Spew static libs out first, then import libraries. Otherwise things like bsppack
    //	will fail to link because libvstdlib.so came before tier1.a.
    for( int i = 0; i < StaticLib.Count(); i++ )
    {
      fprintf( fp, "    %s \\\n", StaticLib[ i ].szFilename );
    }
    for( int i = 0; i < ImportLib.Count(); i++ )
    {
      fprintf( fp, "    -L%s -l%s \\\n", ImportLib[ i ].szFilename, &ImportLib[ i ].szFilename[ ImportLib[ i ].iBasename ] ); // +3 to dodge the lib ext
    }

    fprintf( fp, "\n\n" );

    fprintf( fp, "LIBFILENAMES = \\\n" );
    for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
    {
      CFileConfig *pFileConfig = m_Files[i];
      if ( pFileConfig->IsExcludedFrom( pConfig->GetConfigName() ) )
        continue;

      const char *pFilename = pFileConfig->m_Filename.String();
      if ( IsLibraryFile( pFilename ) )
      {
        if (( !sImportLibraryFile[0] || V_stricmp( sImportLibraryFile, pFilename ) ) &&  // only link this as a library if it isn't our own output!
            ( !sOutputFile[0] || V_stricmp( sOutputFile, pFilename)))
        {
          fprintf( fp, "    %s \\\n", UsePOSIXSlashes( pFilename ) );
        }
      }
    }

    fprintf( fp, "\n\n" );


    CUtlVector< CUtlString > otherDependencies;
    static const char *sDependenciesSeparators[] = { ";", "\r", "\n" };

    // Scan the list of files for any generated dependencies so we can pull them up front
    for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
    {
      CFileConfig *pFileConfig = m_Files[i];
      CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( pConfig->GetConfigName(), pConfig );

      if ( pFileConfig->IsExcludedFrom( pConfig->GetConfigName() ) )
      {
        continue;
      }

      const char *pCustomBuildCommandLine = pFileSpecificData->GetOption( g_pOption_CustomBuildStepCommandLine );
      const char *pOutputFile = pFileSpecificData->GetOption( g_pOption_Outputs );
      if ( pOutputFile && pCustomBuildCommandLine && V_strlen( pCustomBuildCommandLine ) > 0 )
      {
        char szTempFilename[MAX_PATH];
        V_strncpy( szTempFilename, UsePOSIXSlashes( pFileConfig->m_Filename.String() ), sizeof( szTempFilename ) );
        const char *pFilename = szTempFilename;

        // This file uses a custom build step.
        char sFormattedOutputFile[8192];
        char szAbsPath[MAX_PATH];
        V_MakeAbsolutePath( szAbsPath, sizeof(szAbsPath), pFilename );
        DoStandardVisualStudioReplacements( pOutputFile, szAbsPath, sFormattedOutputFile, sizeof( sFormattedOutputFile ) );

        CSplitString outFiles( sFormattedOutputFile, sDependenciesSeparators, sizeof( sDependenciesSeparators ) / sizeof( sDependenciesSeparators[ 0 ] ) );

        for ( int i = 0; i < outFiles.Count(); i ++ )
        {
          const char *pchOneFile = outFiles[ i ];
          if ( *pchOneFile == '\0' )
          {
            continue;
          }
          // Remember this as a dependency so the executable will depend on it.
          if ( otherDependencies.Find( pchOneFile ) == otherDependencies.InvalidIndex() )
            otherDependencies.AddToTail( pchOneFile );
        }
      }
    }

    WriteOtherDependencies( fp, otherDependencies );
    fprintf( fp, "\n\n" );

    // Include the base makefile before the rules to build the .o files.
    // Do this after we output otherDependencies definition since $(OTHER_DEPENDENCIES) is a dependency of all: target
    const char *sMakeFileDependency = "";
    bool bCondPOSIX = g_pVPC->FindOrCreateConditional( "POSIX", false, CONDITIONAL_NULL ) != NULL;
    fprintf( fp, "# Include the base makefile now.\n" );
    if ( bCondPOSIX )
    {
      sMakeFileDependency = k_pszBase_Makefile;
      fprintf( fp, "include %s\n\n\n", sMakeFileDependency );
    }

    // Now write the rules to build the .o files.
    // .o files go in [project dir]/obj/[config]/[base filename]
    for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
    {
      CFileConfig *pFileConfig = m_Files[i];
      CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( pConfig->GetConfigName(), pConfig );

      if ( pFileConfig->IsExcludedFrom( pConfig->GetConfigName() ) )
      {
        continue;
      }

      char szTempFilename[MAX_PATH];
      V_strncpy( szTempFilename, UsePOSIXSlashes( pFileConfig->m_Filename.String() ), sizeof( szTempFilename ) );
      const char *pFilename = szTempFilename;

      // Custom build steps??
      const char *pCustomBuildCommandLine = pFileSpecificData->GetOption( g_pOption_CustomBuildStepCommandLine );
      const char *pOutputFile = pFileSpecificData->GetOption( g_pOption_Outputs );
      if ( pOutputFile && pCustomBuildCommandLine && V_strlen( pCustomBuildCommandLine ) > 0 )
      {
        // This file uses a custom build step.
        char sFormattedOutputFile[8192];
        char sFormattedCommandLine[8192];
        char sFormattedDependencies[8192];
        DoStandardVisualStudioReplacements( pCustomBuildCommandLine, UsePOSIXSlashes( pFilename ), \
                                            sFormattedCommandLine, sizeof( sFormattedCommandLine ) );
        DoStandardVisualStudioReplacements( pOutputFile, UsePOSIXSlashes( pFilename ), \
                                            sFormattedOutputFile, sizeof( sFormattedOutputFile ) );


        // AdditionalDependencies only applies to custom build steps, not normal compilation steps
        const char *pAdditionalDeps = pFileSpecificData->GetOption( g_pOption_AdditionalDependencies );
        if ( pAdditionalDeps )
          DoStandardVisualStudioReplacements( pAdditionalDeps, szAbsPath, sFormattedDependencies, sizeof( sFormattedDependencies ) );
        else
          sFormattedDependencies[0] = 0;

        CSplitString additionalDeps( sFormattedDependencies, sDependenciesSeparators, sizeof( sDependenciesSeparators ) / sizeof( sDependenciesSeparators[0] ) );
        FOR_EACH_VEC_BACK( additionalDeps, i )
        {
          const char *pchOneFile = additionalDeps[i];
          if ( *pchOneFile == '\0' )
          {
            additionalDeps.Remove( i );
          }
        }

        CSplitString outFiles( sFormattedOutputFile, sDependenciesSeparators, sizeof( sDependenciesSeparators ) / sizeof( sDependenciesSeparators[ 0 ] ) );
        FOR_EACH_VEC_BACK( outFiles, i )
        {
          const char *pchOneFile = outFiles[ i ];
          if ( *pchOneFile == '\0' )
          {
            outFiles.Remove( i );
          }
        }

        char rgchIntermediateFile[MAX_PATH];
        rgchIntermediateFile[0] = 0;

        // ABSPATH NOTE
        //
        // Make will not do what we expect with ../../this/dir/foo.cpp, and these are often the result of nested macros
        // in VPC files, so ensure we are giving make absolute paths for our targets via $(abspath ...)

        if ( outFiles.Count() == 1 )
        {
          // one output file: create a standard rule --  output : input \n \t command
          fprintf( fp, "\n$(abspath %s) ", outFiles[0] );
        }
        else
        {
          // multiple output files: DO NOT DO THIS --  output output output : input \n \t command
          // as this will cause up to three parallel invocations of command!
          // best-practice workaround is to create a temp intermediate file
          static int s_uniqueId = 0;
          V_snprintf( rgchIntermediateFile, sizeof(rgchIntermediateFile), "$(OBJ_DIR)/_custombuildstep_%d.touchfile", s_uniqueId );
          fprintf( fp, "\n%s ", rgchIntermediateFile );
          ++s_uniqueId;

          V_strcat( sFormattedCommandLine, CFmtStrMax( "\n@touch %s", rgchIntermediateFile ), sizeof(sFormattedCommandLine) );
        }
        // Outputs dependent on input file and .mak file
        fprintf( fp, ": $(abspath %s) %s", UsePOSIXSlashes( pFilename ), g_pVPC->GetOutputFilename() );
        FOR_EACH_VEC( additionalDeps, i )
        {
          fprintf( fp, " %s", additionalDeps[i] );
        }
        /// XXX(JohnS): Was this double-added as an accident, or is there some arcane make reason to have it be the
        ///             first and last dep?
        fprintf( fp, " $(abspath %s)\n", UsePOSIXSlashes( pFilename ) );
        const char *pDescription = pFileSpecificData->GetOption( g_pOption_Description );
        DoStandardVisualStudioReplacements( pDescription, UsePOSIXSlashes( pFilename ), \
                                            sFormattedOutputFile, sizeof( sFormattedOutputFile ) );

        fprintf( fp, "\t @echo \"%s\";mkdir -p $(OBJ_DIR) 2> /dev/null;\n", sFormattedOutputFile );

        static const char *sSeparators[] = { "\r", "\n" };
        CSplitString outLines( sFormattedCommandLine, sSeparators, sizeof( sSeparators ) / sizeof( sSeparators[ 0 ] ) );
        for ( int i = 0; i < outLines.Count(); ++i )
        {
          const char *pchOneLine = outLines[ i ];
          if ( *pchOneLine == '\0' )
          {
            continue;
          }
          fprintf( fp, "\t %s\n", pchOneLine );
        }
        fprintf( fp, "\n" );

        // for multiple output files, create a dependency between output files and intermediate file + makefile
        if ( outFiles.Count() > 1 )
        {
          FOR_EACH_VEC( outFiles, i )
          {
            // See ABSPATH NOTE above
            fprintf( fp, "$(abspath %s) : %s %s\n\t @touch %s\n\n", outFiles[i], rgchIntermediateFile, g_pVPC->GetOutputFilename(), outFiles[i] );
          }
        }
      }
      else if ( CheckExtensions( pFilename, (const char**)sSourceFileExtensions ) )
      {
        char sObjFilename[MAX_PATH];
        GetObjFilenameForFile( pConfig->GetConfigName(), pFilename, sObjFilename, sizeof( sObjFilename ) );

        // Get the base obj filename for the .P file.
        char sPFileBase[MAX_PATH];
        V_StripExtension( sObjFilename, sPFileBase, sizeof( sPFileBase ) );

        fprintf( fp, "\nifneq (clean, $(findstring clean, $(MAKECMDGOALS)))\n" );
        fprintf( fp, "-include %s.P\n", sPFileBase );
        fprintf( fp, "endif\n" );

        bool bUsedPrecompiledHeader = false;

        // Handle precompiled header options.
        const char *pPrecompiledHeaderOption = pFileSpecificData->GetOption( g_pOption_PrecompiledHeader );
        const char *pUsePCHThroughFile = pFileSpecificData->GetOption( g_pOption_UsePCHThroughFile );

        if ( !g_pVPC->IsPosixPCHDisabled() && pPrecompiledHeaderOption && pUsePCHThroughFile )
        {
          char sIncludeFilename[MAX_PATH];
          const char *pHeaderFileName = V_GetFileName( pUsePCHThroughFile );
          V_snprintf( sIncludeFilename, sizeof( sIncludeFilename ), "$(OBJ_DIR)/%s", pHeaderFileName );

          // Note that we use $(abspath ...) for most things, but the provided PCH file (pUsePCHThroughFile)
          // is actually a include filename -- that is, it is meant to be found the same way #include "pch"
          // would be.  We use make's vpath directive to tell it to resolve this file relative to the
          // INCLUDEDIRS list we conveniently have, (starting with PWD to mimic c/c++ include searching), but
          // that means we have to be careful to always reference it as a dependency the same way.
          //
          // However, since we explicitly opt-in to files using said PCH, we can just specifically set the PCH
          // as a dependency of every file that has opted-in to use it, so we don't run into issues with
          // things like the compiler's -MD dependencies referring to it differently.
          if ( V_stristr( pPrecompiledHeaderOption, "Not Using" ) )
          {
            // Don't do anything special if this file doesn't want to use a precompiled header.
          }
          else if ( V_stristr( pPrecompiledHeaderOption, "Create" ) )
          {
            // Compile pUsePCHThroughFile and output it to obj/<config>/filename.h.gch
            fprintf( fp, "\n%s.gch : %s $(PWD)/%s %s $(OTHER_DEPENDENCIES)\n",
                     sIncludeFilename, pUsePCHThroughFile, g_pVPC->GetOutputFilename(), sMakeFileDependency );
            fprintf( fp, "\t$(PRE_COMPILE_FILE)\n" );
            fprintf( fp, "\t$(COMPILE_PCH) $(POST_COMPILE_FILE)\n" );

            // include the .P it spits out, ensuring it is marked as depending on the gch build finishing so
            // we don't include a stale one.
            fprintf( fp, "\n%s.P : %s.gch\n", sIncludeFilename, sIncludeFilename );
            fprintf( fp, "\nvpath %s . $(INCLUDEDIRS)\n", pUsePCHThroughFile );
            fprintf( fp, "\nifneq (clean, $(findstring clean, $(MAKECMDGOALS)))\n" );
            fprintf( fp, "include %s.P\n", sIncludeFilename );
            fprintf( fp, "endif\n" );

            // Create obj/<config>/filename.h as well, depending on the GCH so it is re-copied when we
            // recompile it.
            //
            // Because we pass this as -include <foo> to the compiler, this allows conditions where the PCH
            // cannot be used to fall back to the compiler simply using the .h, rather than failing
            // entirely.
            fprintf( fp, "\n%s : %s %s.gch $(PWD)/%s %s\n",
                     sIncludeFilename, pUsePCHThroughFile, sIncludeFilename, g_pVPC->GetOutputFilename(), sMakeFileDependency );
            fprintf( fp, "\tcp -f $< %s\n", sIncludeFilename );
          }
          else if ( V_stristr( pPrecompiledHeaderOption, "Use" ) )
          {
            CFileConfig *pCreator = pAccel->FindFileThatCreatesPrecompiledHeader( pConfig->GetConfigName(), pUsePCHThroughFile );
            if ( pCreator && !pCreator->IsExcludedFrom( pConfig->GetConfigName() ) )
            {
              const char *pCompileAsOption = pFileSpecificData->GetOption( g_pOption_CompileAs );
              fprintf( fp, "\n%s : TARGET_PCH_FILE = %s\n", sObjFilename, sIncludeFilename );
              fprintf( fp, "%s : $(abspath %s) %s.gch %s $(PWD)/%s %s\n",
                       sObjFilename, UsePOSIXSlashes( pFilename ), sIncludeFilename, sIncludeFilename,
                       g_pVPC->GetOutputFilename(), sMakeFileDependency );
              fprintf( fp, "\t$(PRE_COMPILE_FILE)\n" );
              if ( pCompileAsOption && strstr( pCompileAsOption, "(/TC)" ) ) // Compile as C code (/TC)
              {
                fprintf( fp, "\t$(COMPILE_FILE_WITH_PCH_C) $(POST_COMPILE_FILE)\n" );
              }
              else
              {
                fprintf( fp, "\t$(COMPILE_FILE_WITH_PCH) $(POST_COMPILE_FILE)\n" );
              }
              bUsedPrecompiledHeader = true;
            }
          }
        }

        if ( !bUsedPrecompiledHeader )
        {
          const char *pCompileAsOption = pFileSpecificData->GetOption( g_pOption_CompileAs );
          fprintf( fp, "\n%s : $(abspath %s) $(PWD)/%s %s $(OTHER_DEPENDENCIES)\n",
                   sObjFilename, UsePOSIXSlashes( pFilename ), g_pVPC->GetOutputFilename(), sMakeFileDependency );
          fprintf( fp, "\t$(PRE_COMPILE_FILE)\n" );
          if ( pCompileAsOption && strstr( pCompileAsOption, "(/TC)" ) ) // Compile as C code (/TC)
          {
            fprintf( fp, "\t$(COMPILE_FILE_C) $(POST_COMPILE_FILE)\n" );
          }
          else
          {
            fprintf( fp, "\t$(COMPILE_FILE) $(POST_COMPILE_FILE)\n" );
          }
        }
      }
    }

    if( !pConfig1 )
    {
      fprintf( fp, "\n\nendif # (CFG=%s)\n\n", pConfig->GetConfigName() );
      fprintf( fp, "\n\n" );
    }
  }

  void WriteOtherDependencies( FILE *fp, CUtlVector< CUtlString > &otherDependencies )
  {
    fprintf( fp, "\nOTHER_DEPENDENCIES = \\\n" );
    for ( int i=0; i < otherDependencies.Count(); i++ )
    {
      fprintf( fp, "\t$(abspath %s)%s\n", UsePOSIXSlashes( otherDependencies[i].String() ),
              (i == otherDependencies.Count()-1) ? "" : " \\" );
    }
    fprintf( fp, "\n\n" );
    fprintf( fp, "-include $(OBJ_DIR)/_other_deps.P\n" );
  }

  bool CheckReleaseDebugConfigsAreSame()
  {
    if ( g_pVPC->IsVerboseMakefile() )
      return false;

    // Go through two configs and check to see that all the strings except the defines are the same.
    // If so, we can write the entire makefile and only special case the $PreprocessorDefinitions portion.
    // Makes for a much simpler makefile to read and modify when testing various flags, etc.
    if( m_BaseConfigData.m_Configurations.Count() == 2 )
    {
      CSpecificConfig *pConfig0 = m_BaseConfigData.m_Configurations[0];
      CSpecificConfig *pConfig1 = m_BaseConfigData.m_Configurations[1];
      KeyValues *pKV0 = pConfig0->m_pKV;
      KeyValues *pKV1 = pConfig1->m_pKV;

      KeyValues *val0 = pKV0->GetFirstValue();
      KeyValues *val1 = pKV1->GetFirstValue();
      for( ;; )
      {
        // If one has run out and the other hasn't, bail.
        if( !val0 != !val1 )
          break;

        if( !val0 )
        {
          // We've hit the end of both keyvalues, and everything was the same.
          Assert( !val1 );
          return true;
        }

        // If the datatypes aren't strings or aren't the same, bail.
        if( ( val0->GetDataType() != KeyValues::TYPE_STRING ) || ( val0->GetDataType() != val1->GetDataType() ) )
          break;

        // If the keynames differ, bail.
        if( V_strcmp( val0->GetName(), val1->GetName() ) )
          break;

        // If this isn't the $PreprocessorDefinitions key, check the values.
        if( V_strcmp( val0->GetName(), g_pOption_PreprocessorDefinitions ) )
        {
          if( V_strcmp( val0->GetString(), val1->GetString() ) )
            break;

          // look for visual studio macros and assume those evaluate to config specific values
          if ( V_strstr( val0->GetString(), "$(" ) )
            break;
        }

        // Next.
        val0 = val0->GetNextValue();
        val1 = val1->GetNextValue();
      }
    }

    return false;
  }

  void WriteMakefile( const char *pFilename )
  {
    FILE *fp = fopen( pFilename, "wt" );

    CPrecompiledHeaderAccel accel;
    accel.Setup( m_Files, &m_BaseConfigData );

    m_bForceLowerCaseFileName = false;

    // Write all the non-config-specific stuff.
    WriteNonConfigSpecificStuff( fp );

    bool bReleaseDebugAreSame = CheckReleaseDebugConfigsAreSame();
    if( bReleaseDebugAreSame )
    {
      // If the two configs are the same except for $PreprocessorDefinitions, then call
      // WriteConfigSpecificStuff() once with both configs.
      Assert( m_BaseConfigData.m_Configurations.Count() == 2 );
      CSpecificConfig *pConfig0 = m_BaseConfigData.m_Configurations[0];
      CSpecificConfig *pConfig1 = m_BaseConfigData.m_Configurations[1];

      // Make sure we always have "release" second (ie, the default case).
      if( !V_stricmp( pConfig0->GetConfigName(), "release" ) )
      {
        CSpecificConfig *pConfigTemp = pConfig0;
        pConfig0 = pConfig1;
        pConfig1 = pConfigTemp;
      }

      m_bForceLowerCaseFileName = pConfig0->m_pKV->GetBool( g_pOption_LowerCaseFileNames, false );
      WriteConfigSpecificStuff( pConfig0, fp, &accel, pConfig1 );
    }
    else
    {
      // Write each config out.
      for ( int i=m_BaseConfigData.m_Configurations.First(); i != m_BaseConfigData.m_Configurations.InvalidIndex(); i=m_BaseConfigData.m_Configurations.Next( i ) )
      {
        CSpecificConfig *pConfig = m_BaseConfigData.m_Configurations[i];
        m_bForceLowerCaseFileName = pConfig->m_pKV->GetBool( g_pOption_LowerCaseFileNames, false );
        WriteConfigSpecificStuff( pConfig, fp, &accel, NULL );
      }
    }

    fclose( fp );
    Sys_CopyToMirror( pFilename );
  }

  bool m_bForceLowerCaseFileName;
};

static CProjectGenerator_Makefile g_ProjectGenerator_Makefile;
IBaseProjectGenerator* GetMakefileProjectGenerator()
{
  return &g_ProjectGenerator_Makefile;
}
