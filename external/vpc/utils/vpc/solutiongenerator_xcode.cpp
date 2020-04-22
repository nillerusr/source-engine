//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"
#include "baseprojectdatacollector.h"
#include "utlsortvector.h"
#include "checksum_md5.h"

#ifdef WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#define getcwd _getcwd
#define snprintf _snprintf
typedef unsigned __int64 uint64_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef signed __int16 int16_t;
#else
#include <stdint.h>
#endif

static const int k_nShellScriptPhasesPerAggregateTarget = 8;

#ifndef STEAM
bool V_StrSubstInPlace( char *pchInOut, int cchInOut, const char *pMatch, const char *pReplaceWith, bool bCaseSensitive );
#endif

static const char *k_pchProjects = "Projects";
static const char *k_pchLegacyTarget = "build with make";
static const char *k_rgchConfigNames[] = { "Debug", "Release" };

static const char *g_pOption_AdditionalDependencies = "$AdditionalDependencies";
static const char *g_pOption_BufferSecurityCheck = "$BufferSecurityCheck";
static const char *g_pOption_BuildMultiArch = "$BuildMultiArch";
static const char *g_pOption_BuildX64Only = "$BuildX64Only";
static const char *g_pOption_CompileAs = "$CompileAs";
static const char *g_pOption_PreBuildEventCommandLine = "$PreBuildEvent/$CommandLine";
static const char *g_pOption_CustomBuildStepCommandLine = "$CustomBuildStep/$CommandLine";
static const char *g_pOption_PostBuildEventCommandLine = "$PostBuildEvent/$CommandLine";
static const char *g_pOption_ConfigurationType = "$ConfigurationType";
static const char *g_pOption_Description = "$Description";
static const char *g_pOption_ExtraCompilerFlags = "$GCC_ExtraCompilerFlags";
static const char *g_pOption_ExtraLinkerFlags = "$GCC_ExtraLinkerFlags";
static const char *g_pOption_ForceInclude = "$ForceIncludes";
static const char *g_pOption_LinkAsBundle = "$LinkAsBundle";
static const char *g_pOption_LocalFrameworks = "$LocalFrameworks";
static const char *g_pOption_LowerCaseFileNames = "$LowerCaseFileNames";
static const char *g_pOption_OptimizerLevel = "$OptimizerLevel";
static const char *g_pOption_OutputDirectory = "$OutputDirectory";
static const char *g_pOption_Outputs = "$Outputs";
static const char *g_pOption_PostBuildEvent = "$PostBuildEvent";
static const char *g_pOption_PrecompiledHeader = "$Create/UsePrecompiledHeader";
static const char *g_pOption_PrecompiledHeaderFile = "$PrecompiledHeaderFile";
static const char *g_pOption_SymbolVisibility = "$SymbolVisibility";
static const char *g_pOption_SystemFrameworks = "$SystemFrameworks";
static const char *g_pOption_SystemLibraries = "$SystemLibraries";
static const char *g_pOption_UsePCHThroughFile = "$Create/UsePCHThroughFile";
static const char *g_pOption_AdditionalLibraryDirectories = "$AdditionalLibraryDirectories";
static const char *g_pOption_TargetCopies = "$TargetCopies";
static const char *g_pOption_TreatWarningsAsErrors = "$TreatWarningsAsErrors";

// These are the only properties we care about for xcodeprojects.
static const char *g_pRelevantProperties[] =
{
    g_pOption_AdditionalDependencies,
    g_pOption_AdditionalIncludeDirectories,
    g_pOption_AdditionalLibraryDirectories,
    g_pOption_BufferSecurityCheck,
    g_pOption_CompileAs,
    g_pOption_OptimizerLevel,
    g_pOption_OutputFile,
    g_pOption_GameOutputFile,
    g_pOption_SymbolVisibility,
    g_pOption_PreprocessorDefinitions,
    g_pOption_ConfigurationType,
    g_pOption_ImportLibrary,
    g_pOption_LinkAsBundle,
    g_pOption_PrecompiledHeader,
    g_pOption_UsePCHThroughFile,
    g_pOption_PrecompiledHeaderFile,
    g_pOption_PreBuildEventCommandLine,
    g_pOption_CustomBuildStepCommandLine,
    g_pOption_PostBuildEventCommandLine,
    g_pOption_OutputDirectory,
    g_pOption_Outputs,
    g_pOption_Description,
    g_pOption_SystemLibraries,
    g_pOption_SystemFrameworks,
    g_pOption_LocalFrameworks,
    g_pOption_BuildMultiArch,
    g_pOption_BuildX64Only,
    g_pOption_ExtraCompilerFlags,
    g_pOption_ExtraLinkerFlags,
    g_pOption_ForceInclude,
    g_pOption_TargetCopies,
    g_pOption_TreatWarningsAsErrors,
};


static CRelevantPropertyNames g_RelevantPropertyNames =
{
    g_pRelevantProperties,
    V_ARRAYSIZE( g_pRelevantProperties )
};


static const char *k_rgchXCConfigFiles[] = { "debug.xcconfig", "release.xcconfig", "base.xcconfig" };

static int k_oidBuildConfigList = 0xc0de;
static int k_rgOidBuildConfigs[] = { 0x1c0de, 0x1c0e0 };

class CProjectGenerator_Xcode : public CBaseProjectDataCollector
{
    typedef CBaseProjectDataCollector BaseClass;
public:
    CProjectGenerator_Xcode() : BaseClass( &g_RelevantPropertyNames )
    {
        m_bIsCurrent = false;
        m_nShellScriptPhases = 0;
        m_nCustomBuildRules = 0;
        m_nPreBuildEvents = 0;
    }

    virtual void Setup() {}

    virtual const char *GetProjectFileExtension() { return ""; }

    virtual void EndProject()
    {
        m_OutputFilename = g_pVPC->GetOutputFilename();
        // we need the "project file" to exist for crc checking
        if ( !Sys_Exists( m_OutputFilename ) )
            Sys_Touch( m_OutputFilename );

        // remember if we needed rebuild according to vpc
        m_bIsCurrent = g_pVPC->IsProjectCurrent( g_pVPC->GetOutputFilename(), false );

        // and update the mod time on the file if we needed rebuild
        if ( !m_bIsCurrent )
            Sys_Touch( m_OutputFilename );

        extern CUtlVector<CBaseProjectDataCollector*> g_vecPGenerators;
        g_vecPGenerators.AddToTail( this );

        extern IBaseProjectGenerator *g_pGenerator;
        g_pVPC->SetProjectGenerator( new CProjectGenerator_Xcode() );
    }

    bool m_bIsCurrent;
    int  m_nShellScriptPhases;
    int  m_nCustomBuildRules;
    int  m_nPreBuildEvents;
    CUtlString m_OutputFilename;
};

// we assume (and assert) that the order of this vector is the same as the order of the projects vector
extern CUtlVector<CBaseProjectDataCollector*> g_vecPGenerators;

class CSolutionGenerator_Xcode : public IBaseSolutionGenerator
{
public:
    virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects );
private:
    void XcodeFileTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf );
    void XcodeProductTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf );
    void EmitBuildSettings( const char *pszProjectName, const char *pszProjectDir, CUtlDict<CFileConfig *,int> *pDictFiles, KeyValues *pConfigKV, KeyValues *pReleaseKV, bool bIsDebug );
    void WriteFilesFolder( uint64_t oid, const char *pFolderName, const char *pExtensions, CBaseProjectDataCollector *pProject );

    void Write( const char *pMsg, ... );
    FILE *m_fp;
    int m_nIndent;

};

enum EOIDType
{
    EOIDTypeProject = 0x00001d00,
    EOIDTypeGroup,
    EOIDTypeFileReference,
    EOIDTypeBuildFile,
    EOIDTypeSourcesBuildPhase,
    EOIDTypeFrameworksBuildPhase,
    EOIDTypeCopyFilesBuildPhase,
    EOIDTypeHeadersBuildPhase,
    EOIDTypePreBuildPhase,
    EOIDTypeShellScriptBuildPhase,
    EOIDTypePostBuildPhase,
    EOIDTypeNativeTarget,
    EOIDTypeAggregateTarget,
    EOIDTypeTargetDependency,
    EOIDTypeContainerItemProxy,
    EOIDTypeBuildConfiguration,
    EOIDTypeConfigurationList,
    EOIDTypeCustomBuildRule,
};

// Make an OID from raw data. You probably want makeoid/makeoid2 below.
uint64_t makeoid_raw( const char *pData, int nDataLen, EOIDType type, int16_t ordinal = 0 )
{
    static unsigned int unOIDSalt = 0;
    static bool bOIDSaltSet = false;
    // Since the string passed to makeoid() doesn't change based on all parameters of the object it
    // is representing, we need to regenerate them per-run.  There isn't currently much value in
    // perserving OIDs that refer to the same object in terms of xcode functionality.  If we wanted
    // deterministic OIDs, we should be hashing all parameters of the object that they represent,
    // which is a non-trivial refactor.
    if ( !bOIDSaltSet )
    {
        // Define this to generate xcode projects with deterministic OIDs (based on solution/project
        // names, see makeoid callers). See above comment for why this is for debugging only.
#if defined( VPC_DETERMINISTIC_XCODE_OIDS )
        unOIDSalt = 0;
        Msg( "DEBUG: Generating deterministic OIDs per project/solution name (salt -> 0).\n" );
#else
        // You'd think our random API would be better here, but it doesn't actually have a function
        // to return a full-range int and uses the below code to seed itself. Except in tools where
        // it is un-seeded without a warning that that is the case.
        float flAppTime = Plat_FloatTime();
        ThreadId_t threadId = ThreadGetCurrentId();
        COMPILE_TIME_ASSERT( sizeof( flAppTime ) <= sizeof( unOIDSalt ) );
        memcpy( &unOIDSalt, &flAppTime, sizeof( float ) );
        unOIDSalt ^= threadId;
#endif // defined( VPC_DETERMINISTIC_XCODE_OIDS )

#ifdef VPC_DEBUG_XCODE_OIDS
        Msg( "XCode Solution: Using random salt for OIDs of %u\n", unOIDSalt );
#endif
        bOIDSaltSet = true;
    }

    // Lower 32bits of OID is hash of identifier string + salt, upper 32bits is type and ordinal.
    MD5Context_t md5Context;
    unsigned char hash[ MD5_DIGEST_LENGTH ] = { 0 };
    MD5Init( &md5Context );
    MD5Update( &md5Context, (const unsigned char *)&unOIDSalt, sizeof( unOIDSalt ) );
    MD5Update( &md5Context, (const unsigned char *)pData, nDataLen );
    MD5Final( hash, &md5Context );

    // Take lower 32bits of md5
    COMPILE_TIME_ASSERT( MD5_DIGEST_LENGTH >= sizeof( uint32_t ) );
    uint32_t lowerHash = 0;
    memcpy( &lowerHash, hash, sizeof( lowerHash ) );

    uint64_t oid = (uint64_t)lowerHash + ((uint64_t)type << 32) + ((uint64_t)(ordinal+1) << 52);
#ifdef VPC_DEBUG_XCODE_OIDS
    Msg( "XCode Solution: Produced OID 0x%llx for \"%s\" with salt %u\n", oid, pszIdentifier, unOIDSalt );
#endif
    return oid;
}

// Make an oid for a unique string identifier, per type, per ordinal
uint64_t makeoid( const char *pszIdentifier, EOIDType type, int16_t ordinal = 0 )
{
    CFmtStr oidStr( "oid1.%s", pszIdentifier );
    return makeoid_raw( oidStr.Access(), oidStr.Length(), type, ordinal );
}

// Make an oid for a unique string tuple, per type, per ordinal
uint64_t makeoid2( const char *pszIdentifierA, const char *pszIdentifierB, EOIDType type, int16_t ordinal = 0 )
{
    CFmtStr oidStr( "oid2.%s.%s", pszIdentifierA, pszIdentifierB );
    return makeoid_raw( oidStr.Access(), oidStr.Length(), type, ordinal );
}

// Make an oid for a unique string tuple, per type, per ordinal
uint64_t makeoid3( const char *pszIdentifierA, const char *pszIdentifierB, const char *pszIdentifierC, EOIDType type, int16_t ordinal = 0 )
{
    CFmtStr oidStr( "oid3.%s.%s.%s", pszIdentifierA, pszIdentifierB, pszIdentifierC );
    return makeoid_raw( oidStr.Access(), oidStr.Length(), type, ordinal );
}

static bool IsStaticLibrary( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false;
    else if ( ! V_stricmp( pchExtension, "a" ) )
        return true;
    return false;
}


static bool IsDynamicLibrary( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false;
    else if ( ! V_stricmp( pchExtension, "dylib" ) )
        return true;
    return false;
}

static const char* EscapeQuotes( const char *pStr )
{
    int len = V_strlen( pStr );
    static char str[4096];
    int i = 0,j = 0;
    for ( ;i <= len,j < V_ARRAYSIZE(str); )
    {
        if ( pStr[i] == '"' )
        {
            str[j++] = '\\';
            str[j++] = '\\';
        }
        str[j++] = pStr[i++];
    }
    str[j] = '\0';

    return str;
}

static void UsePOSIXSlashes( const char *pStr, char *pOut, int nOutSize )
{
  int len = V_strlen( pStr ) + 2;
  char *str = pOut;
  AssertFatal( len <= nOutSize );

    V_strncpy( str, pStr, len );
    for ( int i = 0; i < len; i++ )
    {
        if ( str[i] == '\\' )
        {
            // allow escaping of bash special characters
            if ( i+1 < len && ( str[i+1] != '"' && str[i+1] != '$' &&
                                str[i+1] != '\'' && str[i+1] != '\\' ) )
            {
              str[i] = '/';
            }
        }
        if ( str[i] == '\0' )
            break;
    }
}

// Auto-allocating (not in-place) version.  Caller is responsible for free'ing (or leaking) the allocated buffer. Most
// users leak. Is bad. :-/
static char* UsePOSIXSlashes( const char *pStr )
{
    int len = V_strlen( pStr ) + 2;
    char *str = (char*)malloc(len*sizeof(char));
    UsePOSIXSlashes( pStr, str, len*sizeof(char) );
    return str;
}

// Finds the file name component of a path, and prepends 'lib' to it if necesssary
//   foo/bar/foo.a -> foo/bar/libfoo.a
//   foo/bar/libfoo.a -> unchanged
static void EnforceLibPrefix( char *pInOutStr, int nOutSize )
{
    char *pFile = V_UnqualifiedFileName( pInOutStr );
    if ( pFile && V_strncmp( pFile, "lib", 3 ) != 0 )
    {
        char szOriginalName[MAX_PATH] = { 0 };
        V_strncpy( szOriginalName, pFile, sizeof( szOriginalName ) );

        *pFile = '\0';
        V_strncat( pInOutStr, "lib", nOutSize );
        V_strncat( pInOutStr, szOriginalName, nOutSize );
    }
}

// Get the output file with the output directory prepended
static CUtlString OutputFileWithDirectoryFromConfig( KeyValues *pConfigKV )
{
    char szOutputFile[MAX_PATH] = { 0 };
    char szOutputDir[MAX_PATH] = { 0 };
    UsePOSIXSlashes( pConfigKV->GetString( g_pOption_OutputFile, "" ), szOutputFile, sizeof( szOutputFile ) );
    UsePOSIXSlashes( pConfigKV->GetString( g_pOption_OutputDirectory, "" ), szOutputDir, sizeof( szOutputDir ) );

    // Our output file is relative to BUILT_PRODUCTS_DIR already.  This is a workaround for VPC files expecting Makefile
    // semantics -- you shouldn't be using $() in VPC strings.
    const char szObjDir[] = "$(OBJ_DIR)";
    V_StrSubstInPlace( szOutputDir, sizeof( szOutputDir ), szObjDir, ".", true );
    V_StrSubstInPlace( szOutputFile, sizeof( szOutputFile ), szObjDir, ".", true );

    // VPC files have snuck in hard-coding of random variables expecting the Makefile backend to expand them.
    if ( V_strstr( szOutputDir, "$" ) || V_strstr( szOutputFile, "$" ) )
    {
        g_pVPC->VPCWarning( "$OutputDirectory '%s' or $OutputFile directive '%s' contains what looks like a shell variable reference -- "
                            "this will be treated literally in most circumstances by the Xcode build system and is likely not intended.",
                            szOutputDir, szOutputFile );
    }

    char szFormat[MAX_PATH] = { 0 };
    V_ComposeFileName( szOutputDir, szOutputFile, szFormat, sizeof( szFormat ) );
    V_RemoveDotSlashes( szFormat );

    // For the output files, which are in the build/"Products" directory, Xcode expects static libs to be named
    // "libfoo.a" so it can generate a "-lfoo" command to link with projects that depend upon them (vs passing just
    // ./path/to/foo.a to the linker, which it doesn't want to do for static libraries generated as dependencies, but
    // will do for random external static libs... I don't know either)
    if ( IsStaticLibrary( szFormat ) )
    {
        EnforceLibPrefix( szFormat, sizeof( szFormat ) );
    }

    // Some VPC files are giving output files that are trying to reference a path outside the build directory -- this is
    // what GameOutputFile is for. This actually doesn't seem to break xcode, but isn't really proper and might break
    // later.
    if ( V_IsAbsolutePath( szFormat ) || V_strncmp( szFormat, "../", 3 ) == 0 )
    {
        g_pVPC->VPCWarning( "Final output file '%s' (composited from $OutputDirectory '%s' and $OutputFile '%s') escapes the relative "
                            "BUILT_PRODUCTS_DIR used by Xcode -- this may break things, and should be achieved by using $GameOutputFile "
                            "to copy built output to the proper tree location",
                            szFormat, szOutputDir, szOutputFile );
    }

    CUtlString ret( szFormat );
    return ret;
}

static CUtlString GameOutputFileFromConfig( KeyValues *pConfigKV )
{
    char szGameOutputFile[MAX_PATH] = { 0 };
    UsePOSIXSlashes( pConfigKV->GetString( g_pOption_GameOutputFile, "" ), szGameOutputFile, sizeof( szGameOutputFile ) );
    V_RemoveDotSlashes( szGameOutputFile );

    // VPC files have snuck in hard-coding of random variables expecting the Makefile backend to expand them.
    if ( V_strstr( szGameOutputFile, "$" ) )
    {
        g_pVPC->VPCWarning( "$GameOutputFile '%s' contains what looks like a shell variable reference -- "
                            "this will be treated literally in most circumstances by the Xcode build system and is likely not intended.",
                            szGameOutputFile );
    }

    CUtlString ret( szGameOutputFile );
    return ret;
}

static const char* SkipLeadingWhitespace( const char *pStr )
{
    if ( !pStr )
        return NULL;
    while ( *pStr != '\0' && isspace(*pStr) )
        pStr++;
    return pStr;
}

static bool ProjectProducesBinary( const CBaseProjectDataCollector *pProjectDataCollector )
{
    return ( pProjectDataCollector->m_BaseConfigData.m_Configurations[0]->GetOption( g_pOption_OutputFile ) ||
             pProjectDataCollector->m_BaseConfigData.m_Configurations[0]->GetOption( g_pOption_GameOutputFile ) );
}


static bool NeedsBuildFileEntry( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false;
    else if ( ! V_stricmp( pchExtension, "cpp" ) || ! V_stricmp( pchExtension, "cxx" ) || ! V_stricmp( pchExtension, "cc" ) || ! V_stricmp( pchExtension, "c" ) || ! V_stricmp( pchExtension, "m" ) || ! V_stricmp( pchExtension, "mm" ) || ! V_stricmp( pchExtension, "cc" )  )
        return true;
    else if ( ! V_stricmp( pchExtension, "a" ) || ! V_stricmp( pchExtension, "dylib" ) )
        return true;
    return false;
}


static bool IsSourceFile( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false;
    else if ( ! V_stricmp( pchExtension, "cpp" ) || ! V_stricmp( pchExtension, "cc" ) || ! V_stricmp( pchExtension, "cxx" ) || ! V_stricmp( pchExtension, "c" ) || ! V_stricmp( pchExtension, "m" ) || ! V_stricmp( pchExtension, "mm" ) || ! V_stricmp( pchExtension, "cc" ) )
        return true;
    return false;
}

static bool FileBuildsWithCustomBuildRule( const CBaseProjectDataCollector *pProjectDataCollector, CFileConfig *pFileConfig )
{
    if ( !pFileConfig || !pProjectDataCollector )
        return false;
    CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( pProjectDataCollector->m_BaseConfigData.m_Configurations[1]->GetConfigName(),
                                                                         pProjectDataCollector->m_BaseConfigData.m_Configurations[1] );
    return ( pFileSpecificData->GetOption( g_pOption_CustomBuildStepCommandLine ) != NULL &&
            pFileSpecificData->GetOption( g_pOption_AdditionalDependencies ) == NULL );

}

static bool AppearsInSourcesBuildPhase( const CBaseProjectDataCollector *pProjectDataCollector, CFileConfig *pFileConfig )
{
    if ( !pFileConfig )
        return false;
    return ( IsSourceFile( pFileConfig->m_Filename.String() ) ||
            ( FileBuildsWithCustomBuildRule( pProjectDataCollector, pFileConfig ) && ProjectProducesBinary( pProjectDataCollector ) ) );
}

static bool IsCreatedByCustomBuildStep( const CBaseProjectDataCollector *pProjectDataCollector, CFileConfig *pDynamicFileConfig )
{
    for ( int i=pProjectDataCollector->m_Files.First(); i != pProjectDataCollector->m_Files.InvalidIndex(); i=pProjectDataCollector->m_Files.Next(i) )
    {
        CFileConfig *pFileConfig = pProjectDataCollector->m_Files[i];
        CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( pProjectDataCollector->m_BaseConfigData.m_Configurations[1]->GetConfigName(),
                                                                            pProjectDataCollector->m_BaseConfigData.m_Configurations[1] );

        if ( !pFileSpecificData->GetOption( g_pOption_Outputs) )
            continue;

        CUtlString sOutputFiles;
        sOutputFiles.SetLength( MAX_PATH );
        CUtlString sInputFile;
        sInputFile.SetLength( MAX_PATH );
        V_snprintf( sInputFile.Get(), MAX_PATH, "%s/%s", pProjectDataCollector->m_ProjectName.String(), UsePOSIXSlashes( pDynamicFileConfig->m_Filename.String() ) );
        V_RemoveDotSlashes( sInputFile.Get() );

        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pFileSpecificData->GetOption( g_pOption_Outputs), sInputFile, sOutputFiles.Get(), MAX_PATH );
        V_StrSubstInPlace( sOutputFiles.Get(), MAX_PATH, "$(OBJ_DIR)", "${OBJECT_FILE_DIR_normal}", false );

        if ( V_stristr( sOutputFiles, V_UnqualifiedFileName( pDynamicFileConfig->GetName() ) ) && !pFileSpecificData->GetOption( g_pOption_AdditionalDependencies ) )
        {
            g_pVPC->VPCWarning( "Not adding '%s' to the build sources list in project '%s', it's a dyanmic file and seems to be created by building '%s'\n",
                               pDynamicFileConfig->GetName(),
                               pProjectDataCollector->m_ProjectName.String(),
                               pFileConfig->GetName() );
            return true;
        }
    }
    return false;
}

void ResolveAdditionalProjectDependencies( CDependency_Project *pCurProject, CUtlVector<CDependency_Project*> &projects, CUtlVector<CDependency_Project*> &additionalProjectDependencies )
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
            g_pVPC->VPCError( "Project %s lists '%s' in its $AdditionalProjectDependencies, but there is no project by that name.", pCurProject->GetName(), pLookingFor );

        additionalProjectDependencies.AddToTail( projects[j] );
    }
}


void CSolutionGenerator_Xcode::WriteFilesFolder( uint64_t oid, const char *pFolderName, const char *pExtensions, CBaseProjectDataCollector *pProject )
{
    const CUtlDict<CFileConfig *,int> &files = pProject->m_Files;

    CUtlVector<char*> extensions;
    V_SplitString( pExtensions, ";", extensions );

    Write( "%024llX /* %s */ = {\n", oid, pFolderName );
    ++m_nIndent;
    Write( "isa = PBXGroup;\n" );
    Write( "children = (\n" );
    ++m_nIndent;

    for ( int i=files.First(); i != files.InvalidIndex(); i=files.Next( i ) )
    {
        const char *pFileName = files[i]->GetName();

        // Check for duplicates -- projects may reference a file twice, and xcode does not enjoy any
        // oid being a member of a group twice.
        int idxDupe = files.Find( pFileName );
        if ( files.IsValidIndex( idxDupe ) && idxDupe != i )
        {
            continue;
        }

        // Make sure this file's extension is one of the extensions they're asking for.
        bool bValidExt = false;
        const char *pFileExtension = V_GetFileExtension( V_UnqualifiedFileName( pFileName ) );
        if ( pFileExtension )
        {
            for ( int iExt=0; iExt < extensions.Count(); iExt++ )
            {
                const char *pTestExt = extensions[iExt];

                if ( pTestExt[0] == '*' && pTestExt[1] == '.' && V_stricmp( pTestExt+2, pFileExtension ) == 0 )
                {
                    bValidExt = true;
                    break;
                }
            }
        }

        if ( bValidExt )
        {
            Write( "%024llX /* %s in %s */,\n", makeoid2( pProject->GetProjectName(), pFileName, EOIDTypeFileReference ), UsePOSIXSlashes( pFileName ), pProject->GetProjectName().String() );
        }
    }

    --m_nIndent;
    Write( ");\n" );
    Write( "name = \"%s\";\n", pFolderName );
    Write( "sourceTree = \"<group>\";\n" );
    --m_nIndent;
    Write( "};\n" );
}


void CSolutionGenerator_Xcode::XcodeFileTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.executable" );
    else if ( ! V_stricmp( pchExtension, "cpp" ) || ! V_stricmp( pchExtension, "cxx" ) || ! V_stricmp( pchExtension, "cc" ) || ! V_stricmp( pchExtension, "h" ) || ! V_stricmp( pchExtension, "hxx" ) || ! V_stricmp( pchExtension, "cc" ) )
        snprintf( pchOutBuf, cchOutBuf, "sourcecode.cpp.%s", pchExtension );
    else if ( ! V_stricmp( pchExtension, "c" ) )
        snprintf( pchOutBuf, cchOutBuf, "sourcecode.cpp.cpp" );
    else if ( ! V_stricmp( pchExtension, "m" ) || ! V_stricmp( pchExtension, "mm" ) )
        snprintf( pchOutBuf, cchOutBuf, "sourcecode.objc.%s", pchExtension );
    else if ( ! V_stricmp( pchExtension, "a" ) )
        snprintf( pchOutBuf, cchOutBuf, "archive.ar" );
    else if ( ! V_stricmp( pchExtension, "dylib" ) )
    {
        const char *pszLibName = V_UnqualifiedFileName( pszFileName );
        if ( pszLibName[0] == 'l' && pszLibName[1] == 'i' && pszLibName[2] == 'b' )
            snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.dylib" );
        else
            snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.bundle" );
    }
    else if ( ! V_stricmp( pchExtension, "pl" ) )
        snprintf( pchOutBuf, cchOutBuf, "text.script.perl" );
    else
        snprintf( pchOutBuf, cchOutBuf, "text.plain" );
}


void CSolutionGenerator_Xcode::XcodeProductTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.tool" );
    else if ( ! V_stricmp( pchExtension, "a" ) )
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.library.static" );
    else if ( ! V_stricmp( pchExtension, "dylib" ) )
    {
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.library.dynamic" );
#if 0
        const char *pszLibName = V_UnqualifiedFileName( pszFileName );
        if ( pszLibName[0] != 'l' || pszLibName[1] != 'i' || pszLibName[2] != 'b' )
            snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.bundle" );
#endif
    }
    else
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.unknown" );
}

// GetBool only groks 1/0, we want more flexibility
bool IsTrue(const char *str)
{
    if ( V_strlen(str) && ( !V_stricmp( str, "yes" ) || !V_stricmp( str, "true" ) || !V_stricmp( str, "1" ) ) )
        return true;
    return false;
}

void CSolutionGenerator_Xcode::EmitBuildSettings( const char *pszProjectName, const char *pszProjectDir, CUtlDict<CFileConfig *,int> *pDictFiles, KeyValues *pConfigKV, KeyValues *pFirstConfigKV, bool bIsDebug )
{
    if ( !pConfigKV )
    {
        Write( "PRODUCT_NAME = \"%s\";\n", pszProjectName );
        return;
    }

    // KeyValuesDumpAsDevMsg( pConfigKV, 0, 0 );

    //  Write( "CC = \"$(SOURCE_ROOT)/devtools/bin/osx32/xcode_ccache_wrapper\";\n" );
    //  Write( "LDPLUSPLUS = \"$(DT_TOOLCHAIN_DIR)/usr/bin/clang++\";\n" );

    Write( "ARCHS = (\n" );
    {
        ++m_nIndent;
        bool bBuildX64 = IsTrue(pConfigKV->GetString( g_pOption_BuildX64Only, "" )) || IsTrue(pConfigKV->GetString( g_pOption_BuildMultiArch, "" ) );
        bool bBuildi386 = !IsTrue( pConfigKV->GetString( g_pOption_BuildX64Only, "" ) );
        if ( bBuildi386 )
            Write( "i386,\n" );
        if ( bBuildX64 )
        {
            Write( "x86_64,\n" );
        }
        --m_nIndent;
    }
    Write( ");\n" );

    // We do not handle Output file names changing between configurations, and use the first configuration for such
    // things all over the generator.  I think we would need to duplicate all the targets to be release and debug
    // targets. Instead, when generating configurations, just warn that this isn't supported.
    CUtlString sBuildOutputFile = OutputFileWithDirectoryFromConfig( pFirstConfigKV );
    CUtlString sGameOutputFile = GameOutputFileFromConfig( pFirstConfigKV );
    for ( int iConfig = 1; iConfig < V_ARRAYSIZE(k_rgchXCConfigFiles); iConfig++ )
    {
        CUtlString sConfigOutputFile = OutputFileWithDirectoryFromConfig( pConfigKV );
        CUtlString sConfigGameOutputFile = GameOutputFileFromConfig( pConfigKV );
        if ( sConfigOutputFile != sBuildOutputFile || sConfigGameOutputFile != sGameOutputFile )
        {
            g_pVPC->VPCWarning( "Config '%s' for project '%s' has effective output files:\n"
                                "    $OutputDirectory/$OutputFile: %s\n"
                                "    $GameOutputFile:              %s\n"
                                "  This differs from the first configuration's output files:\n"
                                "    $OutputDirectory/$OutputFile: %s\n"
                                "    $GameOutputFile:              %s\n"
                                "  XCode does not support having differing product names per config,\n"
                                "  the first configuration's names will be used for all configs.",
                                k_rgchConfigNames[iConfig], pszProjectName,
                                sConfigOutputFile.String(), sConfigGameOutputFile.String(),
                                sBuildOutputFile.String(), sGameOutputFile.String() );
        }
    }

    if ( sGameOutputFile.Length() )
    {
        Write( "PRODUCT_NAME = \"%s\";\n", pszProjectName );
        Write( "EXECUTABLE_NAME = \"%s\";\n", sBuildOutputFile.String() );

        if ( V_strlen( pConfigKV->GetString( g_pOption_ExtraLinkerFlags, "" ) ) )
            Write( "OTHER_LDFLAGS = \"%s\";\n", pConfigKV->GetString( g_pOption_ExtraLinkerFlags ) );


        CUtlString sOtherCompilerCFlags = "OTHER_CFLAGS = \"$(OTHER_CFLAGS) ";
        CUtlString sOtherCompilerCPlusFlags = "OTHER_CPLUSPLUSFLAGS = \"$(OTHER_CPLUSPLUSFLAGS) ";

        // Buffer overflow checks default to on so only change things
        // if we need to turn them off.
        bool bBufferSecurityCheck = Sys_StringToBool( pConfigKV->GetString( g_pOption_BufferSecurityCheck, "Yes" ) );
        if ( !bBufferSecurityCheck )
        {
            sOtherCompilerCFlags += "-fno-stack-protector ";
            sOtherCompilerCPlusFlags += "-fno-stack-protector ";
        }

        if ( V_strlen( pConfigKV->GetString( g_pOption_ExtraCompilerFlags, "" ) ) )
        {
            sOtherCompilerCFlags += pConfigKV->GetString( g_pOption_ExtraCompilerFlags );
            sOtherCompilerCPlusFlags += pConfigKV->GetString( g_pOption_ExtraCompilerFlags );
        }

        if ( V_strlen( pConfigKV->GetString( g_pOption_ForceInclude, "" ) ) )
        {

            CSplitString outStrings( pConfigKV->GetString( g_pOption_ForceInclude ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
            for ( int i=0; i < outStrings.Count(); i++ )
            {
                if ( V_strlen( outStrings[i] ) > 2 )
                {
                    //char sIncludeDir[ MAX_PATH ];
                    char szIncludeLine[ MAX_PATH ];
                    /*V_snprintf( sIncludeDir, sizeof( sIncludeDir ), "%s/%s", pszProjectDir, outStrings[i] );
                     V_FixSlashes( sIncludeDir, '/' );
                     V_RemoveDotSlashes( sIncludeDir );
                     #ifdef STEAM
                     V_StripPrecedingAndTrailingWhitespace( sIncludeDir );
                     #endif        */
                    V_snprintf( szIncludeLine, sizeof(szIncludeLine), " -include %s", UsePOSIXSlashes( outStrings[i] ) );
                    sOtherCompilerCFlags += szIncludeLine;
                    sOtherCompilerCPlusFlags += szIncludeLine;
                }
            }
        }

        sOtherCompilerCFlags += "\";\n" ;
        sOtherCompilerCPlusFlags += "\";\n" ;

        Write( sOtherCompilerCFlags );
        Write( sOtherCompilerCPlusFlags );

        if ( IsDynamicLibrary( sGameOutputFile ) )
        {
            char szBaseName[MAX_PATH] = { 0 };
            V_StripExtension( V_UnqualifiedFileName( sGameOutputFile ), szBaseName, sizeof( szBaseName ) );

            if ( Sys_StringToBool( pConfigKV->GetString( g_pOption_LinkAsBundle, "No" ) ) )
            {
                Write( "MACH_O_TYPE = mh_bundle;\n" );
                // Bundles can't have versions and they're defaulted to 1
                // so make sure we have our own no-version properties.
                Write( "DYLIB_COMPATIBILITY_VERSION = \"\";\n" );
                Write( "DYLIB_CURRENT_VERSION = \"\";\n" );
            }
            else if ( szBaseName[0] != 'l' || szBaseName[1] != 'i' || szBaseName[2] == 'b' )
            {
                //if ( !pConfigKV->GetString( g_pOption_LocalFrameworks, NULL ) )
                //    Write( "OTHER_LDFLAGS = \"-flat_namespace\";\n" );
                // Write( "MACH_O_TYPE = mh_bundle;\n" );
                // Write( "EXECUTABLE_EXTENSION = dylib;\n" );
                // Write( "OTHER_LDFLAGS = \"-flat_namespace -undefined suppress\";\n" );
            }
            else
            {
                Write( "MACH_O_TYPE = mh_dylib;\n" );
            }

            Write( "LD_DYLIB_INSTALL_NAME = \"@loader_path/%s.dylib\";\n", szBaseName );
        }

        if ( IsStaticLibrary( sGameOutputFile ) )
        {
            Write( "DEBUG_INFORMATION_FORMAT = dwarf;\n" );
        }
    }
    else
        Write( "PRODUCT_NAME = \"%s\";\n", pszProjectName );

    // add our header search paths
    CSplitString outStrings( pConfigKV->GetString( g_pOption_AdditionalIncludeDirectories ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    if ( outStrings.Count() )
    {
        char sIncludeDir[MAX_PATH];

        // start the iquote list with the project directory
        V_snprintf( sIncludeDir, sizeof( sIncludeDir ), "%s", pszProjectDir );
        V_FixSlashes( sIncludeDir, '/' );
        V_RemoveDotSlashes( sIncludeDir );
#ifdef STEAM
        V_StripPrecedingAndTrailingWhitespace( sIncludeDir );
#endif

        Write( "USER_HEADER_SEARCH_PATHS = (\n" );
        ++m_nIndent;
#ifdef STEAM
        Write( "\"%s\",\n", sIncludeDir );
#endif
        for ( int i=0; i < outStrings.Count(); i++ )
        {
            char sExpandedOutString[MAX_PATH];

            CBaseProjectDataCollector::DoStandardVisualStudioReplacements( outStrings[i], CFmtStr( "%s/dummy.txt", pszProjectDir ).Access(), sExpandedOutString, sizeof( sExpandedOutString ) );
            V_StrSubstInPlace( sExpandedOutString, sizeof( sExpandedOutString ), "$(OBJ_DIR)", "\"${OBJECT_FILE_DIR_normal}\"", false );
            V_StrSubstInPlace( sExpandedOutString, sizeof( sExpandedOutString ), "\"", "\\\"", false );

            if ( V_IsAbsolutePath( sExpandedOutString ) || V_strncmp( sExpandedOutString, "$", 1 ) == 0 )
                V_snprintf( sIncludeDir, sizeof( sExpandedOutString ), "%s", sExpandedOutString );
            else
            {
                V_snprintf( sIncludeDir, sizeof( sExpandedOutString ), "%s/%s", pszProjectDir, sExpandedOutString );
                V_RemoveDotSlashes( sIncludeDir );
            }
#ifdef STEAM
            V_StripPrecedingAndTrailingWhitespace( sIncludeDir );
#endif
            Write( "\"%s\",\n", sIncludeDir );
        }
        --m_nIndent;
        Write( ");\n" );
    }


    // add local frameworks we link against to the compiler framework search paths
    CSplitString localFrameworks( pConfigKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    if ( localFrameworks.Count() )
    {
        Write( "FRAMEWORK_SEARCH_PATHS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(inherited)\",\n" );
            for ( int i=0; i < localFrameworks.Count(); i++ )
            {
                char rgchFrameworkPath[MAX_PATH];
                V_snprintf( rgchFrameworkPath, sizeof( rgchFrameworkPath ), "%s/%s", pszProjectDir, localFrameworks[i] );
                rgchFrameworkPath[ V_strlen( rgchFrameworkPath ) - V_strlen( V_UnqualifiedFileName( localFrameworks[i] ) ) ] = '\0';
                V_RemoveDotSlashes( rgchFrameworkPath );

                Write( "\"%s\",\n", rgchFrameworkPath );
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }

    // add our needed preprocessor definitions
    CSplitString preprocessorDefines( pConfigKV->GetString( g_pOption_PreprocessorDefinitions ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    CUtlVector< macro_t* > vpcMacroDefines;
    g_pVPC->GetMacrosMarkedForCompilerDefines( vpcMacroDefines );
    if ( preprocessorDefines.Count() || vpcMacroDefines.Count() )
    {
        Write( "GCC_PREPROCESSOR_DEFINITIONS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(GCC_PREPROCESSOR_DEFINITIONS)\",\n" );
            for ( int i=0; i < preprocessorDefines.Count(); i++ )
            {
                Write( "\"%s\",\n", preprocessorDefines[i] );
            }
            for ( int i=0; i < vpcMacroDefines.Count(); i++ )
            {
                Write( "\"%s=%s\",\n", vpcMacroDefines[i]->name.String(), vpcMacroDefines[i]->value.String() );
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }

    bool bTreatWarningsAsErrors = Sys_StringToBool( pConfigKV->GetString( g_pOption_TreatWarningsAsErrors, "false" ) );
    Write( "GCC_TREAT_WARNINGS_AS_ERRORS = %s;\n", bTreatWarningsAsErrors ? "YES" : "NO" );

    CUtlMap<const char *, bool> librarySearchPaths( StringLessThan );
    if ( pDictFiles )
    {
        // libraries we consume (specified in our files list)
        for ( int i=pDictFiles->First(); i != pDictFiles->InvalidIndex(); i=pDictFiles->Next(i) )
        {
            const char *pFileName = (*pDictFiles)[i]->m_Filename.String();
            if ( IsStaticLibrary( pFileName ) || IsDynamicLibrary( pFileName ) )
            {
                char rgchLibPath[MAX_PATH];
                V_snprintf( rgchLibPath, sizeof( rgchLibPath ), "%s/%s", pszProjectDir, pFileName );
                V_RemoveDotSlashes( rgchLibPath );
                V_StripFilename( rgchLibPath );
                int nIndex = librarySearchPaths.Find( rgchLibPath );
                if ( nIndex == librarySearchPaths.InvalidIndex() )
                {
                    char *pszLibPath = new char[MAX_PATH];
                    V_strncpy( pszLibPath, rgchLibPath, MAX_PATH );
                    nIndex = librarySearchPaths.Insert( pszLibPath );
                }
            }
        }
    }

    // add additional library search paths
    CSplitString additionalLibraryDirectories( pConfigKV->GetString( g_pOption_AdditionalLibraryDirectories ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    for ( int i = 0; i < additionalLibraryDirectories.Count(); i++ )
    {
        int nIndex = librarySearchPaths.Find( additionalLibraryDirectories[i] );
        if ( nIndex == librarySearchPaths.InvalidIndex() )
        {
            // we need to dup the string so the map can free it later
            char *pszLibPath = new char[MAX_PATH];
            V_strncpy( pszLibPath, additionalLibraryDirectories[i], MAX_PATH );
            nIndex = librarySearchPaths.Insert( pszLibPath );
        }
    }
    if ( librarySearchPaths.Count() )
    {
        char sIncludeDir[MAX_PATH];

        // add the library path we know we need to reference
        Write( "LIBRARY_SEARCH_PATHS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(inherited)\",\n" );
            FOR_EACH_MAP_FAST( librarySearchPaths, i )
            {
                char sExpandedOutString[MAX_PATH];
                V_strncpy( sExpandedOutString, librarySearchPaths.Key(i), sizeof( sExpandedOutString ) );
                V_StrSubstInPlace( sExpandedOutString, sizeof( sExpandedOutString ), "\"", "\\\"", false );

                if ( V_IsAbsolutePath( sExpandedOutString ) || V_strncmp( sExpandedOutString, "$", 1 ) == 0 )
                    V_snprintf( sIncludeDir, sizeof( sExpandedOutString ), "%s", sExpandedOutString );
                else
                {
                    V_snprintf( sIncludeDir, sizeof( sExpandedOutString ), "%s/%s", pszProjectDir, sExpandedOutString );
                    V_RemoveDotSlashes( sIncludeDir );
                }

                Write( "\"%s\",\n", sIncludeDir );
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }

    while ( librarySearchPaths.Count() )
    {
        const char *key = librarySearchPaths.Key( librarySearchPaths.FirstInorder() );
        librarySearchPaths.Remove( key );
        delete [] key;
    }
}

class CStringLess
{
public:
    bool Less( const char *lhs, const char *rhs, void *pCtx )
    {
        return ( strcmp( lhs, rhs ) < 0 ? true : false );
    }
};

void CSolutionGenerator_Xcode::GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects )
{
    CFmtStr oidStrSolutionRoot( "solutionroot.%s", pSolutionFilename );
    CFmtStr oidStrProjectsRoot( "projectsroot.%s", pSolutionFilename );

    Assert( projects.Count() == g_vecPGenerators.Count() );

    char sPbxProjFile[MAX_PATH];
    sprintf( sPbxProjFile, "%s.xcodeproj", pSolutionFilename );
    mkdir( sPbxProjFile, 0777 );
    sprintf( sPbxProjFile, "%s.xcodeproj/project.pbxproj", pSolutionFilename );

    char sProjProjectListFile[MAX_PATH];
    V_snprintf( sProjProjectListFile, sizeof( sProjProjectListFile ), "%s.projects", sPbxProjFile );

    bool bUpToDate = !g_pVPC->IsForceGenerate() && Sys_Exists( sPbxProjFile );
    int64 llSize = 0, llModTime = 0, llLastModTime = 0;

    FOR_EACH_VEC( projects, iProject )
    {
        AssertFatal( !V_strcmp( projects[iProject]->m_ProjectName, g_vecPGenerators[iProject]->GetProjectName() ) );
        // the solution is up-to-date only if all the projects in it were up to date
        // and the solution was built after the mod time on all the project outputs
        CProjectGenerator_Xcode *pProjGen = dynamic_cast<CProjectGenerator_Xcode*>(g_vecPGenerators[iProject]);
        bUpToDate &= pProjGen->m_bIsCurrent;
        if ( bUpToDate && Sys_FileInfo( pProjGen->m_OutputFilename, llSize, llModTime ) && llModTime > llLastModTime )
        {
            llLastModTime = llModTime;
        }
    }

    // regenerate pbxproj if it is older than the latest of the project output files
    if ( bUpToDate && ( !Sys_FileInfo( sPbxProjFile, llSize, llModTime ) || llModTime < llLastModTime ) )
    {
        bUpToDate = false;
    }

    // now go see if our project list agrees with the one on disk
    if ( bUpToDate )
    {
        FILE *fp = fopen( sProjProjectListFile, "r+t" );
        if ( !fp )
            bUpToDate = false;

        char line[2048];
        char *pLine;
        if ( bUpToDate )
        {
            pLine = fgets( line, sizeof(line), fp );
            if (!pLine)
                bUpToDate = false;
            if ( stricmp( line, VPCCRCCHECK_FILE_VERSION_STRING "\n" ) )
                bUpToDate = false;
        }

        int cProjectsPreviously = 0;
        while (bUpToDate)
        {
            pLine = fgets( line, sizeof(line), fp );
            if ( !pLine )
                break;

            ++cProjectsPreviously;

            int len = strlen( line ) - 1;
            while ( line[len] == '\n' || line[len] == '\r' )
            {
                line[len] = '\0';
                len--;
            }

            // N^2 sucks, but N is small
            bool bProjectFound = false;
            FOR_EACH_VEC( g_vecPGenerators, iGenerator )
            {
                CProjectGenerator_Xcode *pGenerator = (CProjectGenerator_Xcode*)g_vecPGenerators[iGenerator];
                if ( stricmp( pGenerator->m_ProjectName.String(), pLine ) == 0 )
                {
                    bProjectFound = true;
                    break;
                }
            }
            if ( !bProjectFound )
            {
                // fprintf( stderr, "%s has vanished from the project, regenerating...\n",  pLine );
                bUpToDate = false;
                break;
            }
        }
        if ( g_vecPGenerators.Count() != cProjectsPreviously )
        {
            // fprintf( stderr, "Project count has changed (%d/%d), regenerating...\n",  cProjectsPreviously, g_vecPGenerators.Count() );
            bUpToDate = false;
        }
        fclose(fp);
    }

    if ( bUpToDate )
    {
        g_pVPC->VPCStatus( true, "Xcode Project %s.xcodeproj looks up-to-date, not generating", pSolutionFilename );
        return;
    }

    m_fp = fopen( sPbxProjFile, "wt" );
    m_nIndent = 0;

    Msg( "\nWriting master Xcode project %s.xcodeproj.\n\n", pSolutionFilename );

    /** header **/
    Write( "// !$*UTF8*$!\n{\n" );
    ++m_nIndent;
    {
        /**
         **
         ** preamble
         **
         **/
        Write( "archiveVersion = 1;\n" );
        Write( "classes = {\n" );
        Write( "};\n" );
        Write( "objectVersion = 44;\n" );
        Write( "objects = {\n" );
        {
            /**
             **
             ** buildfiles - any file that's involved in, or the output of, a build phase
             **
             **/
            Write( "\n/* Begin PBXBuildFile section */" );
            ++m_nIndent;
            {
                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {
                    // poke into the project we're looking @ in the dependency projects vector to figure out it's location on disk
                    char rgchProjectDir[MAX_PATH]; rgchProjectDir[0] = '\0';
                    V_strncpy( rgchProjectDir, projects[iGenerator]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                    V_StripFilename( rgchProjectDir );

                    // the files this project references
                    for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
                    {
                        char rgchFilePath[MAX_PATH];
                        V_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s/%s", rgchProjectDir, g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() );
                        V_RemoveDotSlashes( rgchFilePath );

                        CFileConfig *pFileConfig = g_vecPGenerators[iGenerator]->m_Files[i];
                        const char *pFileName = pFileConfig->m_Filename.String();

                        bool bExcluded = true;
                        for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                        {
                            bExcluded &= ( pFileConfig->IsExcludedFrom( k_rgchConfigNames[iConfig] ) );
                        }

                        if ( bExcluded )
                        {
                            g_pVPC->VPCStatus( false, "xcode: excluding File %s\n", pFileName );
                            continue;
                        }


                        // dynamic files - generated as part of the build - may be automatically added to the build set by xcode,
                        // if we add them twice, bad things (duplicate symbols) happen.
                        bool bIsDynamicFile = pFileConfig->IsDynamicFile( k_rgchConfigNames[1] );

                        // if we have a custom build step, we need to include this file in the build set
                        if ( ( !bIsDynamicFile || !IsCreatedByCustomBuildStep( g_vecPGenerators[iGenerator], pFileConfig ) ) && AppearsInSourcesBuildPhase( g_vecPGenerators[iGenerator], g_vecPGenerators[iGenerator]->m_Files[i] ) )
                        {
                            Write( "\n" );
                            CUtlString sCompilerFlags = NULL;
                            // on mac we can only globally specify common (debug and release) per-file compiler flags
                            for ( int i=pFileConfig->m_Configurations.First(); i != pFileConfig->m_Configurations.InvalidIndex(); i=pFileConfig->m_Configurations.Next(i) )
                            {
                                sCompilerFlags += pFileConfig->m_Configurations[i]->m_pKV->GetString( g_pOption_ExtraCompilerFlags );
                            }
                            // File reference OIDs are unique per project per file
                            Write( "%024llX /* %s in Sources */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; ", makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pFileName, EOIDTypeBuildFile ), V_UnqualifiedFileName( pFileName ), makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pFileName, EOIDTypeFileReference ), pFileName );
                            if ( !sCompilerFlags.IsEmpty() )
                            {
                                Write( "settings = { COMPILER_FLAGS = \"%s\"; };", sCompilerFlags.String() );
                            }
                            Write( " };" );
                        }

                        if ( IsDynamicLibrary( pFileName ) )
                        {
                            Write( "\n" );
                            Write( "%024llX /* %s in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };", makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pFileName, EOIDTypeBuildFile ), V_UnqualifiedFileName( pFileName ), makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pFileName, EOIDTypeFileReference ), pFileName );
                        }

                        if ( IsStaticLibrary( pFileName ) )
                        {
                            Write( "\n" );
                            Write( "%024llX /* %s in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };",
                                   makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pFileName, EOIDTypeBuildFile ), pFileName,
                                   makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pFileName, EOIDTypeFileReference ), pFileName );
                        }
                    }

                    // system libraries we link against
                    KeyValues *pKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < libs.Count(); i++ )
                    {
                        Write( "\n" );
                        Write( "%024llX /* lib%s.dylib in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* lib%s.dylib */; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeBuildFile, i ), libs[i],
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeFileReference, i ), libs[i] );
                    }

                    // system frameworks we link against
                    CSplitString sysFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < sysFrameworks.Count(); i++ )
                    {
                        Write( "\n" );
                        Write( "%024llX /* %s.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s.framework */; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeBuildFile, i ), sysFrameworks[i],
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeFileReference, i ), sysFrameworks[i] );
                    }

                    // local frameworks we link against
                    CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < localFrameworks.Count(); i++ )
                    {
                        char rgchFrameworkName[MAX_PATH];
                        V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );

                        Write( "\n" );
                        Write( "%024llX /* %s.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s.framework */; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeBuildFile, i ), rgchFrameworkName,
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeFileReference, i ), rgchFrameworkName );
                    }


                    // look at everyone who depends on us, and emit a build file pointing at our output file for each of
                    // them to depend upon.  We use the OutputFile (products directory) so XCode's dependency/linking
                    // logic works right.
                    //
                    // The oid has the project in question as the ordinal, so we have a unique build file OID for each
                    // project that wants to depend on us -- they all point to the same file reference.
                    CDependency_Project *pCurProject = projects[iGenerator];
                    CUtlString sGameOutputFile = GameOutputFileFromConfig( pKV );
                    CUtlString sOutputFile = OutputFileWithDirectoryFromConfig( pKV );

                    if ( sOutputFile.Length() && ( IsStaticLibrary( sOutputFile ) || IsDynamicLibrary( sOutputFile ) ) )
                    {
                        for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                        {
                            if ( iGenerator == iTestProject )
                                continue;

                            CDependency_Project *pTestProject = projects[iTestProject];

                            CUtlVector<CDependency_Project*> additionalProjectDependencies;
                            ResolveAdditionalProjectDependencies( pTestProject, projects, additionalProjectDependencies );

                            int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                            if ( pTestProject->DependsOn( pCurProject, dependsOnFlags ) || additionalProjectDependencies.Find( pCurProject ) != additionalProjectDependencies.InvalidIndex() )
                            {
                                Write( "\n" );
                                Write( "%024llX /* (lib)%s */ = {isa = PBXBuildFile; fileRef = %024llX /* (lib)%s - depended on by %s */; };",
                                       makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sOutputFile, EOIDTypeBuildFile, iTestProject ),
                                       sOutputFile.String(),
                                       makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sOutputFile, EOIDTypeFileReference ),
                                       sOutputFile.String(),
                                       pTestProject->m_ProjectName.String() );
                            }
                        }
                    }

                    // Add our our output file and game output file -1 OID for ourselves.
                    if ( sGameOutputFile.Length() )
                    {
                        Write( "\n" );
                        Write( "%024llX /* %s */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sGameOutputFile, EOIDTypeBuildFile, -1 ),
                               sGameOutputFile.String(),
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sGameOutputFile, EOIDTypeFileReference ),
                               sGameOutputFile.String() );
                    }

                    if ( sOutputFile.Length() )
                    {
                        Write( "\n" );
                        Write( "%024llX /* %s in Products */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sOutputFile, EOIDTypeBuildFile, -1 ),
                               sOutputFile.String(),
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sOutputFile, EOIDTypeFileReference ),
                               sOutputFile.String() );
                    }
                }
            }
            --m_nIndent;
            Write( "\n/* End PBXBuildFile section */\n" );

            Write( "\n/*Begin PBXBuildRule section */\n" );
            ++m_nIndent;
            {
                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {
                    CProjectGenerator_Xcode *pGenerator = (CProjectGenerator_Xcode*)g_vecPGenerators[iGenerator];

                    if ( !ProjectProducesBinary( pGenerator ) )
                        continue;

                    char rgchProjectDir[MAX_PATH]; rgchProjectDir[0] = '\0';
                    V_strncpy( rgchProjectDir, projects[iGenerator]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                    V_StripFilename( rgchProjectDir );

                    // we don't have an output file - wander the list of files, looking for custom build steps
                    // if we find any, magic up shell scripts to run them
                    for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
                    {

                        CFileConfig *pFileConfig = g_vecPGenerators[iGenerator]->m_Files[i];
                        CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1]->GetConfigName(),
                                                                                             g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1] );

                        // custom build rules with additional dependencies don't map to pbxbuildrules, we handle them
                        // as custom script phases
                        if ( pFileSpecificData->GetOption( g_pOption_AdditionalDependencies ) )
                            continue;

                        CUtlString sCustomBuildCommandLine = pFileSpecificData->GetOption( g_pOption_CustomBuildStepCommandLine );
                        CUtlString sOutputFiles = pFileSpecificData->GetOption( g_pOption_Outputs );
                        CUtlString sCommand;

                        if ( sOutputFiles.Length() && !sCustomBuildCommandLine.IsEmpty() )
                        {
                            CUtlString sInputFile;
                            sInputFile.SetLength( MAX_PATH );

                            int cCommand = MAX( sCustomBuildCommandLine.Length() * 2, 8 * 1024 );
                            sCommand.SetLength( cCommand );

                            Write( "\n" );
                            Write( "%024llX /* PBXbuildRule */ = {\n", makeoid( projects[iGenerator]->m_ProjectName, EOIDTypeCustomBuildRule, pGenerator->m_nCustomBuildRules++ ) );
                            ++m_nIndent;
                            {
                                Write( "isa = PBXBuildRule;\n" );
                                Write( "compilerSpec = com.apple.compilers.proxy.script;\n" );

                                // DoStandardVisualStudioReplacements needs to know where the file is, so make sure it's got a path on it
                                if ( V_IsAbsolutePath( UsePOSIXSlashes( pFileConfig->m_Filename.String() ) ) )
                                    V_snprintf( sInputFile.Get(), MAX_PATH, "%s", UsePOSIXSlashes( pFileConfig->m_Filename.String() ) );
                                else
                                {
                                    V_snprintf( sInputFile.Get(), MAX_PATH, "%s/%s", rgchProjectDir, UsePOSIXSlashes( pFileConfig->m_Filename.String() ) );
                                    V_RemoveDotSlashes( sInputFile.Get() );
                                }
                                Write( "filePatterns = \"%s\";\n", sInputFile.String() );
                                Write( "fileType = pattern.proxy;\n" );
                                Write( "isEditable = 1;\n" );

                                Write( "outputFiles = (\n" );
                                ++m_nIndent;
                                {
                                    CSplitString outFiles( sOutputFiles, ";" );
                                    for ( int i = 0; i < outFiles.Count(); i ++ )
                                    {
                                        CUtlString sOutputFile;
                                        sOutputFile.SetLength( MAX_PATH );
                                        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( outFiles[i], sInputFile, sOutputFile.Get(), MAX_PATH );
                                        V_StrSubstInPlace( sOutputFile.Get(), MAX_PATH, "$(OBJ_DIR)", "${OBJECT_FILE_DIR_normal}", false );

                                        CUtlString sOutputPath;
                                        sOutputPath.SetLength( MAX_PATH );

                                        if ( V_IsAbsolutePath( sOutputFile ) || V_strncmp( outFiles[i], "$", 1 ) == 0 )
                                            V_snprintf( sOutputPath.Get(), MAX_PATH, "%s", sOutputFile.String() );
                                        else
                                        {
                                            V_snprintf( sOutputPath.Get(), MAX_PATH, "%s/%s", rgchProjectDir, sOutputFile.String() );
                                            V_RemoveDotSlashes( sOutputPath.Get() );
                                        }
                                        Write( "\"%s\",\n", sOutputPath.String() );
                                    }
                                }
                                --m_nIndent;
                                Write( ");\n");
                                CBaseProjectDataCollector::DoStandardVisualStudioReplacements( sCustomBuildCommandLine, sInputFile, sCommand.Get(), cCommand );
                                V_StrSubstInPlace( sCommand.Get(), cCommand, "$(OBJ_DIR)", "\"${OBJECT_FILE_DIR_normal}\"", false );
                                V_StrSubstInPlace( sCommand.Get(), cCommand, ";", ";\\n", false );
                                V_StrSubstInPlace( sCommand.Get(), cCommand, "\"", "\\\"", false );

                                Write( "script = \"#!/bin/bash\\n"
                                      "cd %s\\n"
                                      "%s\\n"
                                      "exit $?\";\n", rgchProjectDir, sCommand.String() );
                            }
                            --m_nIndent;
                            Write( "};" );

                        }
                    }
                }
            }
            --m_nIndent;
            Write( "\n/*End PBXBuildRule section */\n" );

            /**
             **
             ** file references - any file that appears in the project browser
             **
             **/
            Write( "\n/* Begin PBXFileReference section */" );
            ++m_nIndent;
            {
                // include the xcconfig files
                for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchXCConfigFiles); iConfig++ )
                {
                    char rgchFilePath[MAX_PATH];
                    V_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s.xcodeproj/../devtools/%s", pSolutionFilename, k_rgchXCConfigFiles[iConfig] );
                    V_RemoveDotSlashes( rgchFilePath );

                    Write( "\n" );
                    Write( "%024llX /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = text.xcconfig; name = \"%s\"; path = \"%s\"; sourceTree = \"<absolute>\"; };",
                          makeoid2( oidStrSolutionRoot, k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ),
                          k_rgchXCConfigFiles[iConfig],
                          k_rgchXCConfigFiles[iConfig],
                          rgchFilePath
                          );
                }

                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {
                    // find the project we're looking @ in the dependency projects vector to figure out it's location on disk
                    char rgchProjectDir[MAX_PATH]; rgchProjectDir[0] = '\0';
                    V_strncpy( rgchProjectDir, projects[iGenerator]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                    V_StripFilename( rgchProjectDir );


                    for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
                    {
                        char rgchFilePath[MAX_PATH];
                        V_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s/%s", rgchProjectDir, g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() );
                        V_RemoveDotSlashes( rgchFilePath );

                        const char *pFileName = V_UnqualifiedFileName( g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() );

                        char rgchFileType[MAX_PATH];

                        // Can't support compiling as different types in different configurations, but that would be insane anyway, right!? Grab the Release settings.
                        CSpecificConfig *pFileSpecificData = g_vecPGenerators[iGenerator]->m_Files[i]->GetOrCreateConfig(
                                                                g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1]->GetConfigName(),
                                                                g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1] );
                        const char *pCompileAsOption = pFileSpecificData->GetOption( g_pOption_CompileAs );
                        if ( pCompileAsOption && strstr( pCompileAsOption, "(/TC)" ) ) // Compile as C Code (/TC)
                        {
                            strcpy( rgchFileType, "sourcecode.c.c" );
                        }
                        else
                        {
                            XcodeFileTypeFromFileName( pFileName, rgchFileType, sizeof( rgchFileType ) );
                        }

                        Write( "\n" );
                        Write( "%024llX /* %s */ = {isa = PBXFileReference; fileEncoding = 4; explicitFileType = \"%s\"; name = \"%s\"; path = \"%s\"; sourceTree = \"<absolute>\"; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename, EOIDTypeFileReference ),
                               pFileName,
                               rgchFileType,
                               pFileName,
                               rgchFilePath );
                    }
                    KeyValues *pKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;

                    // system libraries we link against
                    CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < libs.Count(); i++ )
                    {
                        Write( "\n" );
                        Write( "%024llX /* lib%s.dylib */ = {isa = PBXFileReference; lastKnownFileType = \"compiled.mach-o.dylib\"; name = \"lib%s.dylib\"; path = \"usr/lib/lib%s.dylib\"; sourceTree = SDKROOT; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeFileReference, i ), libs[i], libs[i], libs[i] );
                    }

                    // system frameworks we link against
                    CSplitString sysFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < sysFrameworks.Count(); i++ )
                    {
                        Write( "\n" );
                        Write( "%024llX /* %s.framework */ = {isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = \"%s.framework\"; path = \"System/Library/Frameworks/%s.framework\"; sourceTree = SDKROOT; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeFileReference, i ), sysFrameworks[i], sysFrameworks[i], sysFrameworks[i] );
                    }

                    // local frameworks we link against
                    CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < localFrameworks.Count(); i++ )
                    {
                        char rgchFrameworkName[MAX_PATH];
                        V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );

                        char rgchFrameworkPath[MAX_PATH];
                        V_snprintf( rgchFrameworkPath, sizeof( rgchFrameworkPath ), "%s/%s", rgchProjectDir, localFrameworks[i] );
                        V_RemoveDotSlashes( rgchFrameworkPath );

                        Write( "\n" );
                        Write( "%024llX /* %s.framework */ = {isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = \"%s.framework\"; path = \"%s\"; sourceTree = \"<absolute>\"; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeFileReference, i ), rgchFrameworkName, rgchFrameworkName, rgchFrameworkPath );
                    }


                    // include the output files (build products) We don't support these changing between configs -- We
                    // check for and warn about this in EmitBuildSettings
                    KeyValues *pConfigKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    const char *pszConfigName = k_rgchConfigNames[0];
                    CUtlString sOutputFile = OutputFileWithDirectoryFromConfig( pConfigKV );
                    if ( sOutputFile.Length() )
                    {
                        char rgchFileType[MAX_PATH];
                        XcodeFileTypeFromFileName( sOutputFile, rgchFileType, sizeof( rgchFileType ) );

                        Write( "\n" );
                        Write( "%024llX /* %s */ = {isa = PBXFileReference; explicitFileType = \"%s\"; includeInIndex = 0; path = \"%s\"; sourceTree = BUILT_PRODUCTS_DIR; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sOutputFile, EOIDTypeFileReference ), sOutputFile.String(), rgchFileType, sOutputFile.String() );
                    }

                    // and the gameoutputfile
                    CUtlString sGameOutputFile = GameOutputFileFromConfig( pKV );
                    if ( sGameOutputFile.Length() )
                    {
                        char rgchFilePath[MAX_PATH];
                        V_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s/%s", rgchProjectDir, sGameOutputFile.String() );
                        V_RemoveDotSlashes( rgchFilePath );

                        char rgchFileType[MAX_PATH];
                        XcodeFileTypeFromFileName( sGameOutputFile, rgchFileType, sizeof( rgchFileType ) );

                        Write( "\n" );
                        Write( "%024llX /* %s */ = {isa = PBXFileReference; explicitFileType = \"%s\"; includeInIndex = 0; path = \"%s\"; sourceTree = \"<absolute>\"; };",
                               makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sGameOutputFile, EOIDTypeFileReference ), sGameOutputFile.String(), rgchFileType, rgchFilePath );
                    }
                }
            }
            --m_nIndent;
            Write( "\n/* End PBXFileReference section */\n" );

            /**
             **
             ** groups - the file hierarchy displayed in the project
             **
             **/
            Write( "\n/* Begin PBXGroup section */\n" );
            ++m_nIndent;
            {
                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {
                    CUtlVector<char*> folderNames;
                    V_SplitString( "Source Files;Header Files;Resources;VPC Files", ";", folderNames );

                    static const char* folderExtensions[] =
                    {
                        "*.c;*.C;*.cc;*.cpp;*.cp;*.cxx;*.c++;*.prg;*.pas;*.dpr;*.asm;*.s;*.bas;*.java;*.cs;*.sc;*.e;*.cob;*.html;*.tcl;*.py;*.pl;*.m;*.mm",
                        "*.h;*.H;*.hh;*.hpp;*.hxx;*.inc;*.sh;*.cpy;*.if",
                        "*.plist;*.strings;*.xib;*.rc;*.proto;*.nut",
                        "*.vpc"
                    };

                    FOR_EACH_VEC( folderNames, iFolder )
                    {
                        WriteFilesFolder( makeoid( g_vecPGenerators[iGenerator]->m_ProjectName, EOIDTypeGroup, iFolder+1 ), folderNames[iFolder], folderExtensions[iFolder], g_vecPGenerators[iGenerator] );
                    }

                    Write( "%024llX /* %s */ = {\n", makeoid( g_vecPGenerators[iGenerator]->m_ProjectName, EOIDTypeGroup ), g_vecPGenerators[iGenerator]->GetProjectName().String() );
                    ++m_nIndent;
                    {
                        Write( "isa = PBXGroup;\n" );
                        Write( "children = (\n" );

                        ++m_nIndent;
                        {
                            FOR_EACH_VEC( folderNames, iFolder )
                            {
                                Write( "%024llX /* %s */,\n", makeoid( g_vecPGenerators[iGenerator]->m_ProjectName, EOIDTypeGroup, iFolder+1 ), folderNames[iFolder] );
                            }

                            // XCode does not easily support having differing membership/output names per config. We'll
                            // only output the file names for release, then warn below that they are not shifting.
                            KeyValues *pKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;

                            // system libraries we link against
                            CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                            for ( int i=0; i < libs.Count(); i++ )
                            {
                                Write( "%024llX /* lib%s.dylib (system library) */,\n", makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeFileReference, i ), libs[i] );
                            }

                            // system frameworks we link against
                            CSplitString sysFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                            for ( int i=0; i < sysFrameworks.Count(); i++ )
                            {
                                Write( "%024llX /* %s.framework (system framework) */,\n", makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeFileReference, i ), sysFrameworks[i] );
                            }

                            // local frameworks we link against
                            CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                            for ( int i=0; i < localFrameworks.Count(); i++ )
                            {
                                char rgchFrameworkName[MAX_PATH];
                                V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );

                                Write( "%024llX /* %s.framework (local framework) */,\n", makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeFileReference, i ), rgchFrameworkName );
                            }

                            // libraries we consume (specified in our files list)
                            for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
                            {
                                CUtlString sFileName = UsePOSIXSlashes( g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() );
                                bool bInclude = IsDynamicLibrary( sFileName );
                                if ( IsStaticLibrary( sFileName ) )
                                {
                                    char szAbsoluteFileName[MAX_PATH] = { 0 };
                                    V_MakeAbsolutePath( szAbsoluteFileName, sizeof( szAbsoluteFileName ),
                                                        UsePOSIXSlashes( g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() ),
                                                        projects[iGenerator]->m_szStoredCurrentDirectory );

                                    bInclude = true;
                                    FOR_EACH_VEC( g_vecPGenerators, iGenerator2 )
                                    {
                                        // don't include static libs generated by other projects - we'll pull them out of the built products tree
                                        KeyValues *pKV = g_vecPGenerators[iGenerator2]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                                        char szAbsoluteGameOutputFile[MAX_PATH] = { 0 };
                                        V_MakeAbsolutePath( szAbsoluteGameOutputFile, sizeof( szAbsoluteGameOutputFile ),
                                                            GameOutputFileFromConfig( pKV ).String(),
                                                            projects[iGenerator2]->m_szStoredCurrentDirectory );
                                        if ( !V_stricmp( szAbsoluteFileName, szAbsoluteGameOutputFile ) )
                                        {
                                            bInclude = false;
                                            break;
                                        }
                                    }
                                }

                                if ( bInclude )
                                {
                                    Write( "%024llX /* %s in Frameworks (explicit) */,\n", makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename, EOIDTypeFileReference ), sFileName.String() );
                                }
                            }

                            CUtlString sOutputFile = OutputFileWithDirectoryFromConfig( pKV );
                            if ( sOutputFile.Length() )
                                Write( "%024llX /* %s */,\n", makeoid2( g_vecPGenerators[iGenerator]->GetProjectName(), sOutputFile.String(), EOIDTypeFileReference ), sOutputFile.String() );
                        }

                        --m_nIndent;

                        Write( ");\n" );
                        Write( "name = \"%s\";\n", g_vecPGenerators[iGenerator]->GetProjectName().String() );
                        Write( "sourceTree = \"<group>\";\n" );
                    }
                    --m_nIndent;
                    Write( "};\n" );

                }

                // root group - the top of the displayed hierarchy
                Write( "%024llX = {\n", makeoid( oidStrProjectsRoot, EOIDTypeGroup ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXGroup;\n" );
                    Write( "children = (\n" );

                    // sort the projects by name before we emit the list
                    CUtlSortVector< CUtlString, CStringLess > vecSortedProjectNames;
                    FOR_EACH_VEC( g_vecPGenerators, iGen )
                    {
                        // fprintf( stderr, "inserting %s (%p)\n",  g_vecPGenerators[iGen]->GetProjectName().String(), &g_vecPGenerators[iGen]->GetProjectName() );
                        vecSortedProjectNames.Insert( g_vecPGenerators[iGen]->GetProjectName() );
                    }

                    ++m_nIndent;
                    {
                        FOR_EACH_VEC( vecSortedProjectNames, iProjectName )
                        {
                            // fprintf( stderr, "looking for %s\n", vecSortedProjectNames[iProjectName].String() );
                            // and each project's group (of groups)
                            FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                            {
                                if ( strcmp( g_vecPGenerators[iGenerator]->m_ProjectName.String(), vecSortedProjectNames[iProjectName] ) )
                                {
                                    // fprintf( stderr, "   skipping '%s' (%p) != '%s' (%p) (%d, %d)\n", g_vecPGenerators[iGenerator]->GetProjectName().String(), g_vecPGenerators[iGenerator]->GetProjectName().String(), vecSortedProjectNames[iProjectName].String(), vecSortedProjectNames[iProjectName].String(), iGenerator, iProjectName );
                                    continue;
                                }
                                // fprintf( stderr, "emitting %s (%d, %d)\n", g_vecPGenerators[iGenerator]->GetProjectName().String(), iGenerator, iProjectName );

                                Write( "%024llX /* %s */,\n", makeoid( g_vecPGenerators[iGenerator]->m_ProjectName, EOIDTypeGroup ), g_vecPGenerators[iGenerator]->GetProjectName().String() );
                                break;
                            }
                        }

                        // add the build config (.xcconfig) files
                        for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchXCConfigFiles); iConfig++ )
                        {
                            Write( "%024llX /* %s */, \n", makeoid2( oidStrSolutionRoot, k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                        }
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "sourceTree = \"<group>\";\n" );
                    // make the project follow our coding standards, and use tabs.
                    Write( "usesTabs = 1;\n" );
                }
                --m_nIndent;
                Write( "};" );
            }
            m_nIndent--;
            Write( "\n/* End PBXGroup section */\n" );


            /**
             **
             ** the sources build phases - each target that compiles source references on of these, it in turn references the source files to be compiled
             **
             **/
            Write( "\n/* Begin PBXSourcesBuildPhase section */" );
            ++m_nIndent;
            FOR_EACH_VEC( projects, iProject )
            {
                Write( "\n" );
                Write( "%024llX /* Sources */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeSourcesBuildPhase ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXSourcesBuildPhase;\n" );
                    Write( "buildActionMask = 2147483647;\n" );
                    Write( "files = (\n" );
                    ++m_nIndent;
                    {
                        for ( int i=g_vecPGenerators[iProject]->m_Files.First(); i != g_vecPGenerators[iProject]->m_Files.InvalidIndex(); i=g_vecPGenerators[iProject]->m_Files.Next(i) )
                        {
                            const char *pFileName = g_vecPGenerators[iProject]->m_Files[i]->m_Filename.String();
                            CFileConfig *pFileConfig = g_vecPGenerators[iProject]->m_Files[i];

                            if ( AppearsInSourcesBuildPhase( g_vecPGenerators[iProject], pFileConfig ) )
                            {
                                Write( "%024llX /* %s in Sources */,\n", makeoid2( g_vecPGenerators[iProject]->GetProjectName(), pFileName, EOIDTypeBuildFile ), V_UnqualifiedFileName( UsePOSIXSlashes( pFileName ) ) );
                            }
                        }
                    }
                    --m_nIndent;
                    Write( ");\n");
                    Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                }
                --m_nIndent;
                Write( "};" );
            }
            --m_nIndent;
            Write( "\n/* End PBXSourcesBuildPhase section */\n" );


            /**
             **
             ** the frameworks build phases - each target that links libraries (static, dyamic, framework) has one of these, it references the linked thing
             **
             **/
            Write( "\n/* Begin PBXFrameworksBuildPhase section */" );
            ++m_nIndent;
            FOR_EACH_VEC( projects, iProject )
            {
                Write( "\n" );
                Write( "%024llX /* Frameworks */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeFrameworksBuildPhase ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXFrameworksBuildPhase;\n" );
                    Write( "buildActionMask = 2147483647;\n" );
                    Write( "files = (\n" );
                    ++m_nIndent;
                    {
                        // libraries we consume (specified in our files list)
                        for ( int i=g_vecPGenerators[iProject]->m_Files.First(); i != g_vecPGenerators[iProject]->m_Files.InvalidIndex(); i=g_vecPGenerators[iProject]->m_Files.Next(i) )
                        {
                            const char *pFileName = g_vecPGenerators[iProject]->m_Files[i]->m_Filename.String();
                            if ( IsStaticLibrary( UsePOSIXSlashes( pFileName ) ) || IsDynamicLibrary( UsePOSIXSlashes( pFileName ) ) )
                            {
                                char szAbsoluteFileName[MAX_PATH] = { 0 };
                                V_MakeAbsolutePath( szAbsoluteFileName, sizeof( szAbsoluteFileName ),
                                                    UsePOSIXSlashes( g_vecPGenerators[iProject]->m_Files[i]->m_Filename.String() ),
                                                    projects[iProject]->m_szStoredCurrentDirectory );
                                bool bInclude = true;
                                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                                {
                                    // Don't include libs generated by other projects - we'll pull them out of the built
                                    // products tree.  Resolve the absolute path of both, since they are relative to
                                    // different projects.
                                    KeyValues *pKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                                    char szAbsoluteGameOutputFile[MAX_PATH] = { 0 };
                                    V_MakeAbsolutePath( szAbsoluteGameOutputFile, sizeof( szAbsoluteGameOutputFile ),
                                                        GameOutputFileFromConfig( pKV ).String(),
                                                        projects[iGenerator]->m_szStoredCurrentDirectory );

                                    if ( !V_stricmp( szAbsoluteFileName, szAbsoluteGameOutputFile ) )
                                    {
                                        bInclude = false;
                                        break;
                                    }
                                }

                                if ( bInclude )
                                {
                                    Write( "%024llX /* %s in Frameworks (explicit) */,\n", makeoid2( g_vecPGenerators[iProject]->GetProjectName(), pFileName, EOIDTypeBuildFile ), pFileName );
                                }

                            }
                        }

                        // libraries from projects we depend on
                        CDependency_Project *pCurProject = projects[iProject];

                        CUtlVector<CDependency_Project*> additionalProjectDependencies;
                        ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );

                        for ( int iTestProject=projects.Count()-1; iTestProject >= 0; --iTestProject )
                        {
                            if ( iProject == iTestProject )
                                continue;

                            CDependency_Project *pTestProject = projects[iTestProject];
                            int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                            if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                            {
                                // In the PBXBuildFile section each of our dependencies generated an OID pointing to
                                // their output file with our project index as the ordinal.  We use the PRODUCTS
                                // directory build file as depending on the final GameOutputFile confuses XCode's linker
                                // logic, and it should not matter (since GameOutputFile is just copying it to a final
                                // destination, so we can depend/link on the products directory intermediate)
                                KeyValues *pKV = g_vecPGenerators[iTestProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                                CUtlString sOutputFile = OutputFileWithDirectoryFromConfig( pKV );
                                if ( sOutputFile.Length() && ( IsStaticLibrary( sOutputFile ) || IsDynamicLibrary( sOutputFile ) ) )
                                {
                                    // The project in question will have generated a BuildFile dependency for us under its name with ordinal set to our index
                                    Write( "%024llX /* (lib)%s (dependency) */,\n",
                                           makeoid2( g_vecPGenerators[iTestProject]->GetProjectName(), sOutputFile, EOIDTypeBuildFile, iProject ), sOutputFile.String() );
                                }
                            }
                        }

                        KeyValues *pKV = g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;

                        // local frameworks we link against
                        CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                        for ( int i=0; i < localFrameworks.Count(); i++ )
                        {
                            char rgchFrameworkName[MAX_PATH];
                            V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );

                            Write( "%024llX /* %s in Frameworks (local framework) */,\n", makeoid2( g_vecPGenerators[iProject]->GetProjectName(), pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeBuildFile, i ), rgchFrameworkName );
                        }

                        // system frameworks we link against
                        CSplitString sysFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                        for ( int i=0; i < sysFrameworks.Count(); i++ )
                        {
                            Write( "%024llX /* %s in Frameworks (system framework) */,\n", makeoid2( g_vecPGenerators[iProject]->GetProjectName(), pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeBuildFile, i ), sysFrameworks[i] );
                        }

                        // system libraries we link against
                        CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
                        for ( int i=0; i < libs.Count(); i++ )
                        {
                            Write( "%024llX /* %s in Frameworks (system library) */,\n", makeoid2( g_vecPGenerators[iProject]->GetProjectName(), pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeBuildFile, i ), libs[i] );
                        }

                    }
                    --m_nIndent;
                    Write( ");\n");
                    Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                }
                --m_nIndent;
                Write( "};" );
            }
            --m_nIndent;
            Write( "\n/* End PBXFrameworksBuildPhase section */\n" );


            /**
             **
             ** the shell script (pre/post build step) build phases - each target that generates a "gameoutputfile" has one of these,
             ** to p4 edit the target and copy the build result there.
             **
             **/
            Write( "\n/* Begin PBXShellScriptBuildPhase section */" );
            ++m_nIndent;
            {
                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {
                    CProjectGenerator_Xcode *pGenerator = (CProjectGenerator_Xcode*)g_vecPGenerators[iGenerator];
                    char rgchProjectDir[MAX_PATH]; rgchProjectDir[0] = '\0';
                    V_strncpy( rgchProjectDir, projects[iGenerator]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                    V_StripFilename( rgchProjectDir );

                    CUtlString sPreBuildCommandLine = g_vecPGenerators[iGenerator]->m_Files[0]->GetOrCreateConfig( g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1]->GetConfigName(),
                                                                                            g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1] )->GetOption( g_pOption_PreBuildEventCommandLine );
                    if ( sPreBuildCommandLine.Length() )
                    {
                        CUtlString sCommand;
                        int cCommand = MAX( sPreBuildCommandLine.Length() * 2, 8 * 1024 );
                        sCommand.SetLength( cCommand );

                        Write( "\n" );
                        Write( "%024llX /* ShellScript */ = {\n", makeoid( projects[iGenerator]->m_ProjectName, EOIDTypePreBuildPhase, pGenerator->m_nPreBuildEvents++ ) );
                        ++m_nIndent;
                        {
                            Write( "isa = PBXShellScriptBuildPhase;\n" );
                            Write( "buildActionMask = 2147483647;\n" );
                            Write( "files = (\n" );
                            Write( ");\n" );
                            Write( "inputPaths = (\n);\n" );
                            Write( "name = \"%s\";\n", CFmtStr( "PreBuild Event for %s", projects[iGenerator]->m_ProjectName.String() ).Access() );
                            Write( "outputPaths = (\n);\n" );
                            Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                            Write( "shellPath = /bin/bash;\n" );

                            CBaseProjectDataCollector::DoStandardVisualStudioReplacements( sPreBuildCommandLine, CFmtStr( "%s/dummy.txt", rgchProjectDir ).Access(), sCommand.Get(), cCommand );
                            V_StrSubstInPlace( sCommand.Get(), cCommand, "$(OBJ_DIR)", "\"${OBJECT_FILE_DIR_normal}\"", false );
                            V_StrSubstInPlace( sCommand.Get(), cCommand, ";", ";\\n", false );
                            V_StrSubstInPlace( sCommand.Get(), cCommand, "\"", "\\\"", false );

                            // xcode wants to run your custom shell scripts anytime the pbxproj has changed (which makes some sense - the script might
                            // be different) - we can't and don't want to early out in this case.
                            Write( "shellScript = \"cd %s\\n"
                                  "%s\";\n", rgchProjectDir, sCommand.String() );
                        }
                        --m_nIndent;
                        Write( "};" );
                    }
                    // we don't have an output file - wander the list of files, looking for custom build steps
                    // if we find any, magic up shell scripts to run them
                    for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
                    {

                        CFileConfig *pFileConfig = g_vecPGenerators[iGenerator]->m_Files[i];
                        CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1]->GetConfigName(),
                                                                                            g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1] );

                        CUtlString sCustomBuildCommandLine = pFileSpecificData->GetOption( g_pOption_CustomBuildStepCommandLine );
                        CUtlString sOutputFiles = pFileSpecificData->GetOption( g_pOption_Outputs );
                        CUtlString sAdditionalDeps = pFileSpecificData->GetOption( g_pOption_AdditionalDependencies );
                        CUtlString sCommand;

                        // if the project produces a binary, it's a native target and we'll handle this custom build step
                        // as a build rule unless the custom build has additional dependencies
                        if ( ProjectProducesBinary( pGenerator ) && !sAdditionalDeps.Length() )
                            continue;

                        if ( sOutputFiles.Length() && !sCustomBuildCommandLine.IsEmpty() )
                        {
                            CUtlString sInputFile;
                            sInputFile.SetLength( MAX_PATH );

                            int cCommand = MAX( sCustomBuildCommandLine.Length() * 2, 8 * 1024 );
                            sCommand.SetLength( cCommand );

                            Write( "\n" );
                            Write( "%024llX /* ShellScript */ = {\n", makeoid( projects[iGenerator]->m_ProjectName, EOIDTypeShellScriptBuildPhase, pGenerator->m_nShellScriptPhases++ ) );
                            ++m_nIndent;
                            {
                                Write( "isa = PBXShellScriptBuildPhase;\n" );
                                Write( "buildActionMask = 2147483647;\n" );
                                Write( "files = (\n" );
                                Write( ");\n" );
                                Write( "inputPaths = (\n" );
                                ++m_nIndent;
                                {
                                    // DoStandardVisualStudioReplacements needs to know where the file is, so make sure it's got a path on it
                                    if ( V_IsAbsolutePath( UsePOSIXSlashes( pFileConfig->m_Filename.String() ) ) )
                                        V_snprintf( sInputFile.Get(), MAX_PATH, "%s", UsePOSIXSlashes( pFileConfig->m_Filename.String() ) );
                                    else
                                    {
                                        V_snprintf( sInputFile.Get(), MAX_PATH, "%s/%s", rgchProjectDir, UsePOSIXSlashes( pFileConfig->m_Filename.String() ) );
                                        V_RemoveDotSlashes( sInputFile.Get() );
                                    }
                                    Write( "\"%s\",\n", sInputFile.String() );

                                    CSplitString additionalDeps( sAdditionalDeps, ";" );
                                    FOR_EACH_VEC( additionalDeps, i )
                                    {
                                        const char *pchOneFile = additionalDeps[i];
                                        if ( *pchOneFile != '\0' )
                                        {
                                            char szDependency[MAX_PATH];
                                            // DoStandardVisualStudioReplacements needs to know where the file is, so make sure it's got a path on it
                                            if ( V_IsAbsolutePath( UsePOSIXSlashes( pchOneFile ) ) )
                                                V_snprintf( szDependency, MAX_PATH, "%s", UsePOSIXSlashes( pchOneFile ) );
                                            else
                                            {
                                                V_snprintf( szDependency, MAX_PATH, "%s/%s", rgchProjectDir, UsePOSIXSlashes( pchOneFile ) );
                                                V_RemoveDotSlashes( szDependency );
                                            }
                                            Write( "\"%s\",\n", szDependency );
                                        }
                                    }

                                }
                                --m_nIndent;
                                Write( ");\n" );

                                CUtlString sDescription;
                                if ( pFileSpecificData->GetOption( g_pOption_Description ) )
                                {
                                    int cDescription = V_strlen( pFileSpecificData->GetOption( g_pOption_Description ) ) * 2;
                                    sDescription.SetLength( cDescription );
                                    CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pFileSpecificData->GetOption( g_pOption_Description ), sInputFile, sDescription.Get(), cDescription );
                                }
                                else
                                    sDescription = CFmtStr( "Custom Build Step for %s", pFileConfig->m_Filename.String() ).Access();

                                Write( "name = \"%s\";\n", sDescription.String() );

                                Write( "outputPaths = (\n" );
#define TELL_XCODE_ABOUT_OUTPUT_FILES 1
#ifdef TELL_XCODE_ABOUT_OUTPUT_FILES
                                // telling xcode about the output files used to cause it's dependency evaluation to
                                // assume that those files had changed anytime the script had run, even if the script
                                // doesn't change them, which caused us to rebuild a bunch of stuff we didn't need to rebuild
                                // but testing with Xcode 6 suggests they fixed that bug, and lying less to the build system is
                                // generally good.
                                ++m_nIndent;
                                {
                                    CSplitString outFiles( sOutputFiles, ";" );
                                    for ( int i = 0; i < outFiles.Count(); i ++ )
                                    {
                                        CUtlString sOutputFile;
                                        sOutputFile.SetLength( MAX_PATH );
                                        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( outFiles[i], sInputFile, sOutputFile.Get(), MAX_PATH );
                                        V_StrSubstInPlace( sOutputFile.Get(), MAX_PATH, "$(OBJ_DIR)", "${OBJECT_FILE_DIR_normal}", false );

                                        CUtlString sOutputPath;
                                        sOutputPath.SetLength( MAX_PATH );

                                        if ( V_IsAbsolutePath( sOutputFile ) || V_strncmp( outFiles[i], "$", 1 ) == 0 )
                                            V_snprintf( sOutputPath.Get(), MAX_PATH, "%s", sOutputFile.String() );
                                        else
                                        {
                                            V_snprintf( sOutputPath.Get(), MAX_PATH, "%s/%s", rgchProjectDir, sOutputFile.String() );
                                            V_RemoveDotSlashes( sOutputPath.Get() );
                                        }
                                        Write( "\"%s\",\n", sOutputPath.String() );
                                    }
                                }
                                --m_nIndent;
#endif
                                Write( ");\n");
                                Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                                Write( "shellPath = /bin/bash;\n" );

                                CBaseProjectDataCollector::DoStandardVisualStudioReplacements( sCustomBuildCommandLine, sInputFile, sCommand.Get(), cCommand );
                                V_StrSubstInPlace( sCommand.Get(), cCommand, "$(OBJ_DIR)", "\"${OBJECT_FILE_DIR_normal}\"", false );
                                V_StrSubstInPlace( sCommand.Get(), cCommand, ";", ";\\n", false );
                                V_StrSubstInPlace( sCommand.Get(), cCommand, "\"", "\\\"", false );

                                // this is something of a dirty ugly hack.  it seems that xcode wants to run your custom shell
                                // scripts anytime the pbxproj has changed (which makes some sense - the script might be different)
                                // since we generate one big project, any vpc change means we'll run all the custom
                                // build steps again, which will generate code, and link code, and generally take time
                                // so if this project was up-to-date (i.e. no vpc changes), add an early out that checks if
                                // all the output files are newer than the input files and early out if that's the case
                                CUtlString sConditionalBlock = CFmtStr( "export CANARY_FILE=\\\"%s\\\";\\n", pGenerator->m_OutputFilename.String() ).Access();
                                // uncomment this line to debug the embedded shell script
                                // sConditionalBlock += "set -x\\n";
                                sConditionalBlock += "EARLY_OUT=1\\n"
                                "let LI=$SCRIPT_INPUT_FILE_COUNT-1\\n"
                                "let LO=$SCRIPT_OUTPUT_FILE_COUNT-1\\n"
                                "for j in $(seq 0 $LO); do\\n"
                                "    OUTPUT=SCRIPT_OUTPUT_FILE_$j\\n"
                                "    if [ \\\"${CANARY_FILE}\\\" -nt \\\"${!OUTPUT}\\\" ]; then\\n"
                                "        EARLY_OUT=0\\n"
                                "        break\\n"
                                "    fi\\n"
                                "    for i in $(seq 0 $LI); do\\n"
                                "        INPUT=SCRIPT_INPUT_FILE_$i\\n"
                                "        if [ \\\"${!INPUT}\\\" -nt \\\"${!OUTPUT}\\\" ]; then\\n"
                                "            EARLY_OUT=0\\n"
                                "            break 2\\n"
                                "        fi\\n"
                                "    done\\n"
                                "done\\n";

                                sConditionalBlock += "if [ $EARLY_OUT -eq 1 ]; then\\n"
                                "    echo \\\"outputs are newer than input, skipping execution...\\\"\\n"
                                "    exit 0\\n"
                                "fi\\n";
                                Write( "shellScript = \"cd %s\\n"
                                      "%s"
                                      "%s\";\n", rgchProjectDir, sConditionalBlock.String(), sCommand.String() );
                            }
                            --m_nIndent;
                            Write( "};" );

                        }
                    }

                    KeyValues *pDebugKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    CUtlString sDebugGameOutputFile = GameOutputFileFromConfig( pDebugKV );

                    KeyValues *pReleaseKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1]->m_pKV;
                    CUtlString sReleaseGameOutputFile = GameOutputFileFromConfig( pReleaseKV );

                    if ( sDebugGameOutputFile.Length() || sReleaseGameOutputFile.Length() )
                    {
                        char rgchDebugFilePath[MAX_PATH];
                        V_snprintf( rgchDebugFilePath, sizeof( rgchDebugFilePath ), "%s/%s", rgchProjectDir, sDebugGameOutputFile.String() );
                        V_RemoveDotSlashes( rgchDebugFilePath );

                        char rgchReleaseFilePath[MAX_PATH];
                        V_snprintf( rgchReleaseFilePath, sizeof( rgchReleaseFilePath ), "%s/%s", rgchProjectDir, sReleaseGameOutputFile.String() );
                        V_RemoveDotSlashes( rgchReleaseFilePath );

                        Write( "\n" );
                        Write( "%024llX /* ShellScript */ = {\n", makeoid( projects[iGenerator]->m_ProjectName, EOIDTypePostBuildPhase, 0 ) );
                        ++m_nIndent;
                        {
                            Write( "isa = PBXShellScriptBuildPhase;\n" );
                            Write( "buildActionMask = 2147483647;\n" );
                            Write( "files = (\n" );
                            Write( ");\n" );
                            Write( "inputPaths = (\n" );
                            ++m_nIndent;
                            {
                                Write( "\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}\",\n" );
                            }
                            --m_nIndent;
                            Write( ");\n" );
                            Write( "name = \"Post-Build Step\";\n" );
                            Write( "outputPaths = (\n" );
                            ++m_nIndent;
                            {
                                V_StrSubstInPlace( rgchDebugFilePath, sizeof( rgchDebugFilePath ), "/lib/osx32/debug/", "/lib/osx32/${CONFIGURATION}/", false );
                                CBaseProjectDataCollector::DoStandardVisualStudioReplacements( rgchDebugFilePath, rgchDebugFilePath, rgchDebugFilePath, sizeof( rgchDebugFilePath ) );
                                V_StrSubstInPlace( rgchReleaseFilePath, sizeof( rgchReleaseFilePath ), "/lib/osx32/release/", "/lib/osx32/${CONFIGURATION}/", false );
                                CBaseProjectDataCollector::DoStandardVisualStudioReplacements( rgchReleaseFilePath, rgchReleaseFilePath, rgchReleaseFilePath, sizeof( rgchReleaseFilePath ) );

                                Write( "\"%s\",\n", rgchDebugFilePath );
                                if ( V_strcmp( rgchDebugFilePath, rgchReleaseFilePath ) )
                                    Write( "\"%s\",\n", rgchReleaseFilePath );
                            }
                            --m_nIndent;
                            Write( ");\n");
                            Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                            Write( "shellPath = /bin/bash;\n" );

                            CUtlString strScript( CFmtStr( "shellScript = \"cd %s;\\n", rgchProjectDir ) );

                            CUtlString strScriptExtra;
                            bool bHasReleasePostBuildCmd = V_strlen( SkipLeadingWhitespace( pReleaseKV->GetString( g_pOption_PostBuildEventCommandLine, "" ) ) ) > 0;
                            bool bHasDebugPostBuildCmd = V_strlen( SkipLeadingWhitespace( pDebugKV->GetString( g_pOption_PostBuildEventCommandLine, "" ) ) ) > 0;
                            strScriptExtra.Format(
                                                  "if [ -z \\\"$CONFIGURATION\\\" -a -n \\\"$BUILD_STYLE\\\" ]; then\\n"
                                                  "  CONFIGURATION=${BUILD_STYLE}\\n"
                                                  "fi\\n"
                                                  "if [ -z \\\"$CONFIGURATION\\\"  ]; then\\n"
                                                  "  echo \\\"Could not determine build configuration.\\\";\\n"
                                                  "  exit 1; \\n"
                                                  "fi\\n"
                                                  "CONFIGURATION=$(echo $CONFIGURATION | tr [A-Z] [a-z])\\n"
                                                  "OUTPUTFILE=\\\"%s\\\"\\n"
                                                  "if [ -z \\\"$VALVE_NO_AUTO_P4\\\" ]; then\\n"
                                                  "  P4_EDIT_CHANGELIST_CMD=\\\"p4 changes -c $(p4 client -o | grep ^Client | cut -f 2) -s pending | fgrep 'POSIX Auto Checkout' | cut -d' ' -f 2 | tail -n 1\\\"\\n"
                                                  "  P4_EDIT_CHANGELIST=$(eval \\\"$P4_EDIT_CHANGELIST_CMD\\\")\\n"
                                                  "  if [ -z \\\"$P4_EDIT_CHANGELIST\\\" ]; then\\n"
                                                  "    P4_EDIT_CHANGELIST=$(echo -e \\\"Change: new\\\\nDescription: POSIX Auto Checkout\\\" | p4 change -i | cut -f 2 -d ' ')\\n"
                                                  "  fi\\n"
                                                  "fi\\n"
                                                  "if [ -f \\\"$OUTPUTFILE\\\" -o -d \\\"$OUTPUTFILE.dSYM\\\" ]; then\\n"
                                                  "  if [ -z \\\"$VALVE_NO_AUTO_P4\\\" ]; then\\n"
                                                  "    p4 edit -c $P4_EDIT_CHANGELIST \\\"$OUTPUTFILE...\\\" | grep -v \\\"also opened\\\"\\n"
                                                  "  else\\n"
                                                  "    if [ -f \\\"$OUTPUTFILE\\\" ]; then\\n"
                                                  "      chmod -f +w \\\"$OUTPUTFILE\\\"\\n"
                                                  "    fi\\n"
                                                  "    if [ -d \\\"$OUTPUTFILE.dSYM\\\" ]; then\\n"
                                                  "      chmod -R -f +w \\\"$OUTPUTFILE.dSYM\\\"\\n"
                                                  "    fi\\n"
                                                  "  fi\\n"
                                                  "fi\\n"
                                                  "set -eu\\n"
                                                  "if [ -d \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}.dSYM\\\" ]; then\\n"
                                                  "  echo \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}.dSYM -> ${OUTPUTFILE}.dSYM\\\"\\n"
                                                  "  rm -rf \\\"${OUTPUTFILE}.dSYM\\\"\\n"
                                                  "  cp -pR \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}.dSYM\\\" \\\"${OUTPUTFILE}.dSYM\\\"\\n"
                                                  "fi\\n"
                                                  "cp -pv \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}\\\" \\\"$OUTPUTFILE\\\"\\n"
                                                  "# POST-BUILD:\\n"
                                                  "set -e\\n"
                                                  "if [ ${CONFIGURATION} == \\\"release\\\" ]; then\\n"
                                                  "  %s\\n"
                                                  "elif [ ${CONFIGURATION} == \\\"debug\\\" ]; then\\n"
                                                  "  %s\\n"
                                                  "fi\\n"
                                                  "\";\n",
                                                  rgchReleaseFilePath,
                                                  bHasReleasePostBuildCmd ? UsePOSIXSlashes( pReleaseKV->GetString( g_pOption_PostBuildEventCommandLine, "true" ) ) : "true",
                                                  bHasDebugPostBuildCmd ? UsePOSIXSlashes( pDebugKV->GetString( g_pOption_PostBuildEventCommandLine, "true" ) ) : "true" );

                            strScript += strScriptExtra;
                            Write( strScript.Get() );
                        }
                        --m_nIndent;
                        Write( "};" );
                    }
                }
            }
            --m_nIndent;
            Write( "\n/* End PBXShellScriptBuildPhase section */\n" );

            /**
             **
             ** nativetargets section - build targets, which ultimately reference build phases
             **
             **/
            Write( "\n/* Begin PBXNativeTarget section */" );
            ++m_nIndent;
            FOR_EACH_VEC( projects, iProject )
            {
                CProjectGenerator_Xcode *pGenerator = (CProjectGenerator_Xcode*)g_vecPGenerators[iProject];

                KeyValues *pKV = g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                CUtlString sGameOutputFile = GameOutputFileFromConfig( pKV );
                if ( !sGameOutputFile.Length() )
                    continue;

                Write( "\n" );
                Write( "%024llX /* %s */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                ++m_nIndent;
                {
                    Write( "isa = PBXNativeTarget;\n" );

                    Write( "buildConfigurationList = %024llX /* Build configuration list for PBXNativeTarget \"%s\" */;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeConfigurationList ), projects[iProject]->m_ProjectName.String() );
                    Write( "buildPhases = (\n" );
                    ++m_nIndent;
                    {
                        for ( int i = 0; i < pGenerator->m_nPreBuildEvents; i++ )
                            Write( "%024llX /* PreBuildEvent */,\n",  makeoid( projects[iProject]->m_ProjectName, EOIDTypePreBuildPhase, i ) );
                        for ( int i = 0; i < pGenerator->m_nShellScriptPhases; i++ )
                            Write( "%024llX /* ShellScript */,\n",  makeoid( projects[iProject]->m_ProjectName, EOIDTypeShellScriptBuildPhase, i ) );
                        Write( "%024llX /* Sources */,\n",  makeoid( projects[iProject]->m_ProjectName, EOIDTypeSourcesBuildPhase ) );
                        Write( "%024llX /* Frameworks */,\n",  makeoid( projects[iProject]->m_ProjectName, EOIDTypeFrameworksBuildPhase ) );
                        Write( "%024llX /* PostBuildPhase */,\n",  makeoid( projects[iProject]->m_ProjectName, EOIDTypePostBuildPhase, 0 ) );
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "buildRules = (\n" );
                    ++m_nIndent;
                    {
                        for ( int i = 0; i < pGenerator->m_nCustomBuildRules; i++ )
                            Write( "%024llX /* PBXBuildRule */,\n",  makeoid( projects[iProject]->m_ProjectName, EOIDTypeCustomBuildRule, i ) );
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "dependencies = (\n" );
                    ++m_nIndent;
                    {
                        // these dependencies point to the dependency objects, which reference other projects through the container item proxy objects
                        CDependency_Project *pCurProject = projects[iProject];

                        CUtlVector<CDependency_Project*> additionalProjectDependencies;
                        ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );

                        for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                        {
                            if ( iProject == iTestProject )
                                continue;

                            CDependency_Project *pTestProject = projects[iTestProject];
                            int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                            if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                            {
                                Write( "%024llX /* %s */,\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeTargetDependency, (uint16_t)iTestProject ), pTestProject->GetName() );
                            }
                        }
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "productName = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                    Write( "name = \"%s\";\n", projects[iProject]->m_ProjectName.String() );

                    if ( sGameOutputFile.Length() )
                    {
                        Write( "productReference = %024llX /* %s */;\n", makeoid2( projects[iProject]->m_ProjectName, sGameOutputFile, EOIDTypeFileReference ), sGameOutputFile.String() );
                    }

                    char rgchProductType[MAX_PATH];
                    XcodeProductTypeFromFileName( V_UnqualifiedFileName( sGameOutputFile ), rgchProductType, sizeof( rgchProductType ) );
                    Write( "productType = \"%s\";\n", rgchProductType );
                }
                --m_nIndent;
                Write( "};" );
            }
            --m_nIndent;
            Write( "\n/* End PBXNativeTarget section */\n" );

            /**
             **
             ** aggregate targets - for targets that have no output files (i.e. are scripts)
             ** and the "all" target
             **
             **/
            Write( "\n/* Begin PBXAggregateTarget section */\n" );
            ++m_nIndent;
            {
                Write( "%024llX /* All */ = {\n", makeoid( oidStrSolutionRoot, EOIDTypeAggregateTarget ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXAggregateTarget;\n" );
                    Write( "buildConfigurationList = %024llX /* Build configuration list for PBXAggregateTarget \"All\" */;\n", makeoid( oidStrSolutionRoot, EOIDTypeConfigurationList, 1 ) );
                    Write( "buildPhases = (\n" );
                    Write( ");\n" );
                    Write( "dependencies = (\n" );
                    ++m_nIndent;
                    {
                        FOR_EACH_VEC( projects, iProject )
                        {
                            // note the sneaky -1 ordinal here, is we can later generate a dependency block for the target thats not tied to any other targets dependency.
                            Write( "%024llX /* PBXProjectDependency */,\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeTargetDependency, -1 ) );
                        }
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "name = All;\n" );
                    Write( "productName = All;\n" );
                }
                --m_nIndent;
                Write( "};\n" );

                FOR_EACH_VEC( projects, iProject )
                {
                    CProjectGenerator_Xcode *pGenerator = (CProjectGenerator_Xcode*)g_vecPGenerators[iProject];
                    KeyValues *pKV = g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    CUtlString sOutputFile = OutputFileWithDirectoryFromConfig( pKV );
                    if ( sOutputFile.Length() )
                        continue;

                    // NOTE: the use of EOIDTypeNativeTarget here is intentional - a project will never appear as both, and this makes things link up without
                    // having to special case in dependencies and aggregate targets
                    // NOTE: the driving loop is the number of shell script phases we have - aggregate targets with > 1 shell script phase
                    // get broken into N aggregate targets, each with 1 shell script phase so xcode can execute them in parallel
                    int cSubAggregateTargets = ( pGenerator->m_nShellScriptPhases / k_nShellScriptPhasesPerAggregateTarget ) + ( pGenerator->m_nShellScriptPhases % k_nShellScriptPhasesPerAggregateTarget ? 1 : 0);
                    for ( int i = 0; i < cSubAggregateTargets; i++ )
                    {
                        Write( "%024llX /* %s_%d */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeNativeTarget, i ), projects[iProject]->m_ProjectName.String(), i );
                        ++m_nIndent;
                        {
                            Write( "isa = PBXAggregateTarget;\n" );

                            Write( "buildConfigurationList = %024llX /* Build configuration list for PBXAggregateTarget \"%s\" */;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeConfigurationList ), projects[iProject]->m_ProjectName.String() );
                            Write( "buildPhases = (\n" );
                            ++m_nIndent;
                            {
                                for (int j = 0; j < k_nShellScriptPhasesPerAggregateTarget; j++)
                                    Write( "%024llX /* ShellScript %d/%d*/,\n",  makeoid( projects[iProject]->m_ProjectName, EOIDTypeShellScriptBuildPhase, i*k_nShellScriptPhasesPerAggregateTarget+j ), i*k_nShellScriptPhasesPerAggregateTarget+j+1, pGenerator->m_nShellScriptPhases );
                            }
                            --m_nIndent;
                            Write( ");\n" );

                            Write( "buildRules = (\n" );
                            ++m_nIndent;
                            {
                                // Aggregate targets don't get build rules
                            }
                            --m_nIndent;
                            Write( ");\n" );
                            Write( "dependencies = (\n" );
                            ++m_nIndent;
                            {
                                // these dependencies point to the dependency objects, which reference other projects through the container item proxy objects
                                CDependency_Project *pCurProject = projects[iProject];

                                CUtlVector<CDependency_Project*> additionalProjectDependencies;
                                ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );

                                for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                                {
                                    if ( iProject == iTestProject )
                                    {
                                        // the "parent" aggregate depends on all the subaggregates, so the vpc dependency structure doesn't need to
                                        // change.
                                        if ( i == 0)
                                            for ( int j = 1; j < cSubAggregateTargets; j++)
                                                // the 0-(j+1) is to avoid colliding with the All aggregate dependency at -1
                                                Write( "%024llX /* %s_%d (subproject) */,\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeTargetDependency, 0-(j+1) ), pCurProject->m_ProjectName.String(), j );
                                        continue;
                                    }

                                    CDependency_Project *pTestProject = projects[iTestProject];
                                    int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                                    if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                                    {
                                        Write( "%024llX /* %s */,\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeTargetDependency, (uint16_t)iTestProject ), pTestProject->GetName() );
                                    }
                                }
                            }
                            --m_nIndent;
                            Write( ");\n" );
                            if ( i == 0 )
                            {
                                Write( "name = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                                Write( "productName = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                            }
                            else
                            {
                                Write( "name = \"%s_%d\";\n", projects[iProject]->m_ProjectName.String(), i );
                                Write( "productName = \"%s_%d\";\n", projects[iProject]->m_ProjectName.String(), i );
                            }
                        }
                        --m_nIndent;
                        Write( "};\n" );
                    }
                }
            }
            --m_nIndent;
            Write( "\n/* End PBXAggregateTarget section */\n" );

            /**
             **
             ** project section - the top-level object that ties all the bits (targets, groups, ...) together
             **
             **/
            Write( "\n/* Begin PBXProject section */\n" );
            ++m_nIndent;
            Write( "%024llX /* project object */ = {\n", makeoid( oidStrSolutionRoot, EOIDTypeProject ) );
            ++m_nIndent;
            {
                Write( "isa = PBXProject;\n" );
                Write( "attributes = {\n" );
                ++m_nIndent;
                {
                    Write( "BuildIndependentTargetsInParallel = YES;\n" );
                }
                --m_nIndent;
                Write( "};\n" );
                Write( "buildConfigurationList = %024llX /* Build configuration list for PBXProject \"%s\" */;\n", makeoid( oidStrSolutionRoot, EOIDTypeConfigurationList ), V_UnqualifiedFileName( UsePOSIXSlashes( pSolutionFilename ) ) );
                Write( "compatibilityVersion = \"Xcode 3.0\";\n" );
                Write( "hasScannedForEncodings = 0;\n" );
                Write( "mainGroup = %024llX;\n", makeoid( oidStrProjectsRoot, EOIDTypeGroup ) );
                Write( "productRefGroup = %024llX /* Products */;\n", makeoid( oidStrSolutionRoot, EOIDTypeGroup ) );
                Write( "projectDirPath = \"\";\n" );
                Write( "projectRoot = \"\";\n" );
                Write( "targets = (\n" );
                ++m_nIndent;
                {
                    Write( "%024llX /* All */,\n", makeoid( oidStrSolutionRoot, EOIDTypeAggregateTarget ) );
                    // sort the projects by name before we emit the list
                    CUtlSortVector< CUtlString, CStringLess > vecSortedProjectNames;
                    FOR_EACH_VEC( g_vecPGenerators, iGen )
                    {
                        vecSortedProjectNames.Insert( g_vecPGenerators[iGen]->GetProjectName() );
                    }
                    FOR_EACH_VEC( vecSortedProjectNames, iProjectName )
                    {
                        FOR_EACH_VEC( projects, iProject )
                        {
                            if ( strcmp( projects[iProject]->m_ProjectName.String(), vecSortedProjectNames[iProjectName] ) )
                            {
                                continue;
                            }
                            Write( "%024llX /* %s */,\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                            // if this is an aggregate target with more than one shell script, emit the "child" aggregates
                            if ( !ProjectProducesBinary( ((CProjectGenerator_Xcode*)g_vecPGenerators[iProject]) ) )
                            {
                                int cSubAggregateTargets = ( ((CProjectGenerator_Xcode*)g_vecPGenerators[iProject])->m_nShellScriptPhases / k_nShellScriptPhasesPerAggregateTarget ) + ( ((CProjectGenerator_Xcode*)g_vecPGenerators[iProject])->m_nShellScriptPhases % k_nShellScriptPhasesPerAggregateTarget ? 1 : 0 );
                                for ( int i=1; i < cSubAggregateTargets; i++ )
                                    Write( "%024llX /* %s_%d */,\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeNativeTarget, i ), projects[iProject]->m_ProjectName.String(), i );
                            }
                        }
                    }
                }
                --m_nIndent;
                Write( ");\n" );
            }
            --m_nIndent;
            Write( "};" );
            Write( "\n/* End PBXProject section */\n" );

            /**
             **
             ** container item proxies (no clue, I just work here...) - they sit between projects when expressing dependencies
             **
             **/
            Write( "\n/* Begin PBXContainerItemProxy section */" );
            {
                FOR_EACH_VEC( projects, iProject )
                {

                    // for the aggregate target
                    Write( "\n" );
                    Write( "%024llX /* PBXContainerItemProxy */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeContainerItemProxy, -1 ) );
                    ++m_nIndent;
                    {
                        Write( "isa = PBXContainerItemProxy;\n" );
                        // it looks like if you cross ref between xcodeprojs, this is the oid for the other xcode proj
                        Write( "containerPortal = %024llX; /* Project object */\n", makeoid( oidStrSolutionRoot, EOIDTypeProject ) );
                        Write( "proxyType = 1;\n" );
                        Write( "remoteGlobalIDString = %024llX;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeNativeTarget ) );
                        Write( "remoteInfo = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                    }
                    --m_nIndent;
                    Write( "};" );

                    // for each project, figure out what projects it depends on, and spit out a containeritemproxy for that dependency
                    // of particular note is that there are many item proxies for a given project, so we make their oids with the ordinal
                    // of the project they depend on - this must be consistent within the generated solution
                    CDependency_Project *pCurProject = projects[iProject];

                    CUtlVector<CDependency_Project*> additionalProjectDependencies;
                    ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );

                    for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                    {
                        if ( iProject == iTestProject )
                        {
                            int cSubAggregateTargets = ( ((CProjectGenerator_Xcode*)g_vecPGenerators[iProject])->m_nShellScriptPhases / k_nShellScriptPhasesPerAggregateTarget ) + ( ((CProjectGenerator_Xcode*)g_vecPGenerators[iProject])->m_nShellScriptPhases % k_nShellScriptPhasesPerAggregateTarget ? 1 : 0 );
                            for ( int i = 1; i < cSubAggregateTargets; i++ )
                            {
                                Write( "\n" );
                                Write( "%024llX /* PBXContainerItemProxy (subproject) */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeContainerItemProxy, 0-(i+1) ) );
                                ++m_nIndent;
                                {
                                    Write( "isa = PBXContainerItemProxy;\n" );
                                    // it looks like if you cross ref between xcodeprojs, this is the oid for the other xcode proj
                                    Write( "containerPortal = %024llX; /* Project object */\n", makeoid( oidStrSolutionRoot, EOIDTypeProject ) );
                                    Write( "proxyType = 1;\n" );
                                    Write( "remoteGlobalIDString = %024llX;\n", makeoid( projects[iTestProject]->m_ProjectName, EOIDTypeNativeTarget, i ) );
                                    Write( "remoteInfo = \"%s\";\n", projects[iTestProject]->m_ProjectName.String() );
                                }
                                --m_nIndent;
                                Write( "};" );
                            }
                            continue;
                        }

                        CDependency_Project *pTestProject = projects[iTestProject];
                        int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                        if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                        {
                            Write( "\n" );
                            Write( "%024llX /* PBXContainerItemProxy */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeContainerItemProxy, (uint16_t)iTestProject ) );
                            ++m_nIndent;
                            {
                                Write( "isa = PBXContainerItemProxy;\n" );
                                // it looks like if you cross ref between xcodeprojs, this is the oid for the other xcode proj
                                Write( "containerPortal = %024llX; /* Project object */\n", makeoid( oidStrSolutionRoot, EOIDTypeProject ) );
                                Write( "proxyType = 1;\n" );
                                Write( "remoteGlobalIDString = %024llX;\n", makeoid( projects[iTestProject]->m_ProjectName, EOIDTypeNativeTarget ) );
                                Write( "remoteInfo = \"%s\";\n", projects[iTestProject]->m_ProjectName.String() );
                            }
                            --m_nIndent;
                            Write( "};" );
                        }
                    }
                }
            }
            Write( "\n/* End PBXContainerItemProxy section */\n" );

            /**
             **
             ** target dependencies - referenced by each project, in turn references the proxy container objects to express dependencies between targets
             **
             **/
            Write( "\n/* Begin PBXTargetDependency section */" );
            FOR_EACH_VEC( projects, iProject )
            {
                Write( "\n" );
                Write( "%024llX /* PBXTargetDependency */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeTargetDependency, -1 ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXTargetDependency;\n" );
                    Write( "target = %024llX /* %s */;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                    Write( "targetProxy = %024llX /* PBXContainerItemProxy */;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeContainerItemProxy, -1 ) );
                }
                --m_nIndent;
                Write( "};" );

                CDependency_Project *pCurProject = projects[iProject];

                CUtlVector<CDependency_Project*> additionalProjectDependencies;
                ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );

                for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                {
                    if ( iProject == iTestProject )
                    {
                        for ( int i = 1; i < ((CProjectGenerator_Xcode*)g_vecPGenerators[iProject])->m_nShellScriptPhases; i++ )
                        {
                            Write( "\n" );
                            Write( "%024llX /* PBXTargetDependency */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeTargetDependency, 0-(i+1) ) );
                            ++m_nIndent;
                            {
                                Write( "isa = PBXTargetDependency;\n" );
                                Write( "target = %024llX /* %s_%d */;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeNativeTarget, i ), projects[iProject]->m_ProjectName.String(), i );
                                Write( "targetProxy = %024llX /* PBXContainerItemProxy */;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeContainerItemProxy, 0-(i+1) ) );
                            }
                            --m_nIndent;
                            Write( "};" );
                        }
                        continue;
                    }

                    CDependency_Project *pTestProject = projects[iTestProject];
                    int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                    if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                    {
                        // project_t *pTestProjectT = &g_projects[ pTestProject->m_iProjectIndex ];
                        Write( "\n" );
                        Write( "%024llX /* PBXTargetDependency */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeTargetDependency, (uint16_t)iTestProject ) );
                        ++m_nIndent;
                        {
                            Write( "isa = PBXTargetDependency;\n" );
                            Write( "target = %024llX /* %s */;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                            Write( "targetProxy = %024llX /* PBXContainerItemProxy */;\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeContainerItemProxy, (uint16_t)iTestProject ) );
                        }
                        --m_nIndent;
                        Write( "};" );
                    }
                }
            }
            --m_nIndent;
            Write( "\n/* End PBXTargetDependency section */\n" );


            /**
             **
             ** build configurations - each target (and the project) has a set of build configurations (one release, one debug), each with their own set of build settings
             ** the "baseConfigurationReference" points back to the appropriate .xcconfig file that gets referenced by the project and has all the non-target specific settings
             **
             **/

            Write( "\n/* Begin XCBuildConfiguration section */" );
            ++m_nIndent;
            {
                // project and aggregate "all" target
                for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                {
                    bool bIsDebug = !V_stristr( k_rgchConfigNames[iConfig], "release" );

                    Write( "\n" );
                    Write( "%024llX /* %s */ = {\n", makeoid2( oidStrSolutionRoot, k_rgchConfigNames[iConfig], EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
                    ++m_nIndent;
                    {
                        Write( "isa = XCBuildConfiguration;\n" );
                        Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid2( oidStrSolutionRoot, k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                        Write( "buildSettings = {\n" );
                        ++m_nIndent;
                        {
                            EmitBuildSettings( "All", NULL, NULL, NULL, NULL, bIsDebug );
                        }
                        --m_nIndent;
                        Write( "};\n" );
                        Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );
                    }
                    --m_nIndent;
                    Write( "};" );

                    Write( "\n" );
                    Write( "%024llX /* %s */ = {\n", makeoid2( oidStrSolutionRoot, k_rgchConfigNames[iConfig], EOIDTypeBuildConfiguration, 1 ), k_rgchConfigNames[iConfig] );
                    ++m_nIndent;
                    {
                        Write( "isa = XCBuildConfiguration;\n" );
                        Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid2( oidStrSolutionRoot, k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                        Write( "buildSettings = {\n" );
                        ++m_nIndent;
                        {
                            EmitBuildSettings( "All", NULL, NULL, NULL, NULL, bIsDebug );
                        }
                        --m_nIndent;
                        Write( "};\n" );
                        Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );
                    }
                    --m_nIndent;
                    Write( "};" );
                }

                FOR_EACH_VEC( projects, iProject )
                {
                    KeyValues *pReleaseKV = g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                    {
                        bool bIsDebug = !V_stristr( k_rgchConfigNames[iConfig], "release" );

                        Write( "\n" );
                        Write( "%024llX /* %s */ = {\n", makeoid3( oidStrSolutionRoot, projects[iProject]->m_ProjectName, k_rgchConfigNames[iConfig], EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
                        ++m_nIndent;
                        {
                            Write( "isa = XCBuildConfiguration;\n" );
                            Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid2( oidStrSolutionRoot, k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                            Write( "buildSettings = {\n" );
                            ++m_nIndent;
                            {
                                KeyValues *pConfigKV = g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations[iConfig]->m_pKV;
                                char rgchProjectDir[MAX_PATH];
                                V_strncpy( rgchProjectDir, projects[iProject]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                                V_StripFilename( rgchProjectDir );

                                EmitBuildSettings( projects[iProject]->m_ProjectName, rgchProjectDir, &(g_vecPGenerators[iProject]->m_Files), pConfigKV, pReleaseKV, bIsDebug );
                            }
                            --m_nIndent;
                            Write( "};\n" );
                            Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );
                        }
                        --m_nIndent;
                        Write( "};" );
                    }
                }
            }
            --m_nIndent;
            Write( "\n/* End XCBuildConfiguration section */\n" );

            /**
             **
             ** configuration lists - aggregates the build configurations above into sets, which are referenced by the individual targets.
             **
             **/
            Write( "\n/* Begin XCConfigurationList section */\n" );
            ++m_nIndent;
            {
                Write( "%024llX /* Build configuration list for PBXProject \"%s\" */ = {\n", makeoid( oidStrSolutionRoot, EOIDTypeConfigurationList ), V_UnqualifiedFileName( UsePOSIXSlashes( pSolutionFilename ) ) );
                ++m_nIndent;
                {
                    Write( "isa = XCConfigurationList;\n" );
                    Write( "buildConfigurations = (\n" );
                    ++m_nIndent;
                    for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                    {
                        Write( "%024llX /* %s */,\n", makeoid2( oidStrSolutionRoot, k_rgchConfigNames[iConfig], EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "defaultConfigurationIsVisible = 0;\n" );
                    Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[0] );
                }
                --m_nIndent;
                Write( "};\n" );

                Write( "%024llX /* Build configuration list for PBXAggregateTarget \"All\" */ = {\n", makeoid( oidStrSolutionRoot, EOIDTypeConfigurationList, 1 ) );
                ++m_nIndent;
                {
                    Write( "isa = XCConfigurationList;\n" );
                    Write( "buildConfigurations = (\n" );
                    ++m_nIndent;
                    for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                    {
                        Write( "%024llX /* %s */,\n", makeoid2( oidStrSolutionRoot, k_rgchConfigNames[iConfig], EOIDTypeBuildConfiguration, 1 ), k_rgchConfigNames[iConfig] );
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "defaultConfigurationIsVisible = 0;\n" );
                    Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[0] );
                }
                --m_nIndent;
                Write( "};" );

                FOR_EACH_VEC( projects, iProject )
                {
                    Write( "\n" );
                    Write( "%024llX /* Build configuration list for PBXNativeTarget \"%s\" */ = {\n", makeoid( projects[iProject]->m_ProjectName, EOIDTypeConfigurationList ), projects[iProject]->m_ProjectName.String() );
                    ++m_nIndent;
                    {
                        Write( "isa = XCConfigurationList;\n" );
                        Write( "buildConfigurations = (\n" );
                        ++m_nIndent;
                        for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                        {
                            Write( "%024llX /* %s */,\n", makeoid3( oidStrSolutionRoot, projects[iProject]->m_ProjectName, k_rgchConfigNames[iConfig], EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
                        }
                        --m_nIndent;
                        Write( ");\n" );
                        Write( "defaultConfigurationIsVisible = 0;\n" );
                        Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[0] );
                    }
                    --m_nIndent;
                    Write( "};" );
                }
            }
            --m_nIndent;
            Write( "\n/* End XCConfigurationList section */\n" );
        }
        Write( "};\n" ); // objects = { ...

        /**
         **
         ** root object in the graph
         **
         **/
        Write( "rootObject = %024llX /* Project object */;\n", makeoid( oidStrSolutionRoot, EOIDTypeProject ) );
    }
    --m_nIndent;

    Write( "}\n" );
    fclose( m_fp );

    // and now write a .projects file inside the xcode project so we can detect the list of projects changing
    // (specifically a vpc project dissapearing from our target list)
    FILE *fp = fopen( sProjProjectListFile, "wt" );
    if ( !fp )
    {
        g_pVPC->VPCError( "Unable to open %s to write projects into.", sProjProjectListFile );
    }
    // we don't need to be quite as careful as project script, as we're only looking to catch cases where the rest of VPC thinks we're up-to-date
    fprintf( fp, "%s\n", VPCCRCCHECK_FILE_VERSION_STRING );
    FOR_EACH_VEC( g_vecPGenerators, iGenerator )
    {
        CProjectGenerator_Xcode *pGenerator = (CProjectGenerator_Xcode*)g_vecPGenerators[iGenerator];

        fprintf( fp, "%s\n", pGenerator->m_ProjectName.String() );
    }
    fclose( fp );
}

void CSolutionGenerator_Xcode::Write( const char *pMsg, ... )
{
    for ( int i=0; i < m_nIndent; i++ )
        fprintf( m_fp, "\t" );

    va_list marker;
    va_start( marker, pMsg );
    vfprintf( m_fp, pMsg, marker );
    va_end( marker );
}

static CSolutionGenerator_Xcode g_SolutionGenerator_Xcode;
IBaseSolutionGenerator* GetXcodeSolutionGenerator()
{
    return &g_SolutionGenerator_Xcode;
}

static CProjectGenerator_Xcode g_ProjectGenerator_Xcode;
IBaseProjectGenerator* GetXcodeProjectGenerator()
{
    return &g_ProjectGenerator_Xcode;
}
