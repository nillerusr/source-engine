//========= Copyright 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"
#include "dependencies.h"
#include "p4sln.h"
#include "ilaunchabledll.h"
#include <time.h>

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_VPC, "VPC" );

CVPC						*g_pVPC;

class CBaseProjectDataCollector;
CUtlVector<CBaseProjectDataCollector*> g_vecPGenerators;

// Stuff that we might encounter in a vpc file that parts of vpc care about
const char					*g_pOption_ImportLibrary = "$ImportLibrary";
const char					*g_pOption_OutputFile = "$OutputFile";
const char					*g_pOption_GameOutputFile = "$GameOutputFile";
const char					*g_pOption_AdditionalIncludeDirectories = "$AdditionalIncludeDirectories";
const char					*g_pOption_AdditionalProjectDependencies = "$AdditionalProjectDependencies";
const char					*g_pOption_AdditionalOutputFiles = "$AdditionalOutputFiles";
const char					*g_pOption_PreprocessorDefinitions = "$PreprocessorDefinitions";
char						*g_IncludeSeparators[2] = {";",","};

#ifdef POSIX
#define _unlink unlink
#define _stat stat
#endif

CVPC::CVPC()
{
	m_pP4Module = NULL;
	m_pFilesystemModule = NULL;

	m_nArgc = 0;
	m_ppArgv = NULL;

	m_bVerbose = false;
	m_bQuiet = false;
	m_bUsageOnly = false;
	m_bHelp = false;
	m_bSpewPlatforms = false;
	m_bSpewGames = false;
	m_bSpewGroups = false;
	m_bSpewProjects = false;
	m_bIgnoreRedundancyWarning = false;
	m_bSpewProperties = false;
	m_bTestMode = false;
	m_bGeneratedProject = false;
	m_bAnyProjectQualified = false;
	m_bForceGenerate = false;
	m_bNoPosixPCH = false;
	m_bEnableVpcGameMacro = true;
	m_bDecorateProject = false;
	m_bShowDeps = false;
	m_bP4AutoAdd = false;
	m_bP4SlnCheckEverything = false;
	m_bInMkSlnPass = false;
	m_bShowCaseIssues = false;
	m_bVerboseMakefile = false;
	m_bP4SCC = false;
	m_b32BitTools = false;

#ifdef VPC_SCC_INTEGRATION
	m_bP4SCC = true;
#endif
	if ( getenv( "VPC_SRCCTL" ) != NULL )
	{
		m_bP4SCC = V_atoi( getenv( "VPC_SRCCTL" ) ) != 0;
	}

#ifdef WIN32
	m_eVSVersion = k_EVSVersion_2015;
	m_bUseVS2010FileFormat = true;
	m_bUseUnity = false;
#else
	m_eVSVersion = k_EVSVersion_Invalid;
	m_bUseVS2010FileFormat = false;
	m_bUseUnity = false;
#endif

	m_FilesMissing = 0;

	// need to check files by default, otherwise dependency failure (due to missing file) cause needles rebuilds
	m_bCheckFiles = true;

	m_pProjectGenerator = NULL;
	m_pSolutionGenerator = NULL;

#ifdef OSX
	m_bForceIterate = true;
#else
	m_bForceIterate = false;
#endif

	m_pPhase1Projects = NULL;
}

CVPC::~CVPC()
{
	// BUGBUG: There is probably some actual cleanup to be done here.
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::Init( int argc, char **argv )
{
	m_nArgc = argc;
	m_ppArgv = argv;

	// vpc operates tersely by preferred company opinion
	// verbosity necessary for debugging
	m_bVerbose = ( HasCommandLineParameter( "/v" ) || HasCommandLineParameter( "/verbose" ) );
	m_bQuiet = ( HasCommandLineParameter( "/q" ) ||
				 HasCommandLineParameter( "/quiet" ) ||
				 ( getenv( "VPC_QUIET" ) && V_stricmp( getenv( "VPC_QUIET" ), "0" ) ) );

#ifndef STEAM
	// We don't really need to pop the logging state since the process will terminate when we're done.
	LoggingSystem_PushLoggingState();

	m_LoggingListener.m_bQuietPrintf = m_bQuiet;
	LoggingSystem_RegisterLoggingListener( &m_LoggingListener );
#endif

	// needs to occur early and before any other expensive setup, a crc check just exits with an error code used by caller
	InProcessCRCCheck();

	LoadPerforceInterface();

	// vpc may have been run from wrong location, restart self
	bool bIsRestart = false;
	if ( RestartFromCorrectLocation( &bIsRestart ) )
	{
		// successfully ran under restart condition, all done
		return false;
	}
	if ( bIsRestart )
	{
		// this process is the restart child, cull the internal private restart guard option
		// otherwise it gets confused as a build option
		m_nArgc--;
	}

	Log_Msg( LOG_VPC, "VPC - Valve Project Creator For " );
	Log_Msg( LOG_VPC, "Visual Studio, Xbox 360, PlayStation 3, " );
	Log_Msg( LOG_VPC, "Xcode and Make (Build: %s %s)\n", __DATE__, __TIME__ );
	Log_Msg( LOG_VPC, "(C) Copyright 1996-2015, Valve Corporation, All rights reserved.\n" );
	Log_Msg( LOG_VPC, "\n" );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::Shutdown( bool bHasError )
{
	if ( !bHasError )
	{
		GetScript().EnsureScriptStackEmpty();
	}

	if ( !m_TempGroupScriptFilename.IsEmpty() )
	{
		// delete temp work file
		_unlink( m_TempGroupScriptFilename.Get() );
		m_TempGroupScriptFilename.Clear();
	}

	UnloadPerforceInterface();
}

#if defined( STANDALONE_VPC )
class CP4;
extern CP4 s_p4;
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::LoadPerforceInterface()
{
	if ( p4 )
	{
		// already loaded
		return true;
	}

#if defined( STANDALONE_VPC )
	p4 = (IP4*)&s_p4;
	return (p4 != NULL);
#else

	//
	// Try to load p4lib.dll and the filesystem since the p4lib relies on it
	//
	char p4libdll[MAX_PATH];
	char filesystemdll[MAX_PATH];

#ifdef _WIN32
	// Don't require them to have game\bin in their path. Since we know where vpc.exe is,
	// point directly to p4lib.dll in its rightful place.
	char szModuleBinPath[MAX_PATH];
	GetModuleFileName( NULL, szModuleBinPath, sizeof( szModuleBinPath ) );
	V_ExtractFilePath( szModuleBinPath, p4libdll, sizeof( p4libdll ) );
	V_AppendSlash( p4libdll, sizeof( p4libdll ) );
	V_strncpy( filesystemdll, p4libdll, sizeof( filesystemdll ) );
	V_strncat( p4libdll, "..\\..\\..\\game\\bin\\p4lib.dll", sizeof( p4libdll ) );
	V_strncat( filesystemdll, "..\\..\\..\\game\\bin\\filesystem_stdio.dll", sizeof( filesystemdll ) );
#else
	V_strncpy( p4libdll, "p4lib", sizeof( p4libdll ) );
	V_strncpy( filesystemdll, "filesystem_stdio", sizeof( filesystemdll ) );
#endif

	if ( !Sys_LoadInterface( p4libdll, P4_INTERFACE_VERSION, &m_pP4Module, (void**)&p4 ) )
	{
#ifdef _WIN32
		// This always fails on non-Windows build machines -- the warning is
		// annoying and not helpful.
		VPCWarning( "Unable to get Perforce interface from p4lib.dll." );
#endif
		return false;
	}

	// Let the P4 module get its interface to the filesystem - hate this

	// This method is not available in portal2, but is in source2.
	//	p4->SetVerbose( false );
	m_pFilesystemModule = Sys_LoadModule( filesystemdll );
	p4->Connect( Sys_GetFactory( m_pFilesystemModule ) );

	return true;
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::UnloadPerforceInterface()
{
	// Unload P4 if it was loaded
	if ( m_pP4Module )
	{
		Sys_UnloadModule( m_pP4Module );
		m_pP4Module = NULL;
	}

	if ( m_pFilesystemModule )
	{
		Sys_UnloadModule( m_pFilesystemModule );
		m_pFilesystemModule = NULL;
	}
}


bool VPC_Config_IgnoreOption( const char *pPropertyName )
{
	char buff[MAX_SYSTOKENCHARS];
	g_pVPC->GetScript().ParsePropertyValue( NULL, buff, sizeof( buff ) );
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::VPCError( const char* format, ... )
{
	va_list		argptr;
	char		msg[MAX_SYSPRINTMSG];

	va_start( argptr,format );
	vsprintf( msg,format,argptr );
	va_end( argptr );

	// spew in red
	Log_Warning( LOG_VPC, Color( 255, 0, 0, 255 ), "ERROR: %s\n", msg );

	// dump the script stack to assist in user understading of the include chain
	GetScript().SpewScriptStack();

	// do proper shutdown in an error context
	Shutdown( true );

	// errors are expected to be fatal by all calling code
	// otherwise it would have been a warning
	exit( 1 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::VPCSyntaxError( const char* format, ... )
{
	va_list		argptr;
	char		msg[MAX_SYSPRINTMSG];

	va_start( argptr, format );
	if ( format )
	{
		vsprintf( msg, format, argptr );
	}
	va_end( argptr );

	if ( format )
	{
		Log_Warning( LOG_VPC, Color( 255, 0, 0, 255 ), "Bad Syntax: %s\n", msg );
	}

	// syntax errors are fatal
	VPCError( "Bad Syntax in '%s' line:%d\n", GetScript().GetName(), GetScript().GetLine() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::VPCWarning( const char* format, ... )
{
	va_list		argptr;
	char		msg[MAX_SYSPRINTMSG];

	va_start( argptr,format );
	vsprintf( msg,format,argptr );
	va_end( argptr );

	if ( m_bIgnoreRedundancyWarning )
	{
		if ( V_stristr( msg, "matches default setting" ) )
			return;
		if ( V_stristr( msg, "already exists in project" ) )
			return;
	}

	Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "WARNING: %s\n", msg );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::VPCStatus( bool bAlwaysSpew, const char* format, ... )
{
	if ( m_bQuiet )
		return;

	va_list		argptr;
	char		msg[MAX_SYSPRINTMSG];

	va_start( argptr,format );
	vsprintf( msg,format,argptr );
	va_end( argptr );

	if ( bAlwaysSpew || m_bVerbose )
	{
		Log_Msg( LOG_VPC, "%s\n", msg );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CVPC::GetProjectsInGroup( CUtlVector< projectIndex_t > &projectList, const char *pGroupName )
{
	projectList.RemoveAll();

	// Find the specified group
	groupTagIndex_t groupTagIndex = VPC_Group_FindOrCreateGroupTag( pGroupName, false );

	if ( groupTagIndex != INVALID_INDEX )
	{
		FOR_EACH_VEC( m_GroupTags[groupTagIndex].groups, m )
		{
			FOR_EACH_VEC( m_Groups[m_GroupTags[groupTagIndex].groups[m]].projects, n )
			{
				projectList.AddToTail( m_Groups[m_GroupTags[groupTagIndex].groups[m]].projects[n] );
			}
		}
	}

	return projectList.Count();
}

//-----------------------------------------------------------------------------
// Checks to ensure the bin path is in the same tree as the vpc_scripts
// Returns true if bin path valid
//-----------------------------------------------------------------------------
#if !defined( POSIX )
bool CVPC::CheckBinPath( char *pOutBinPath, int outBinPathSize )
{
	char	szScriptPath[MAX_PATH];
	char	szDirectory[MAX_PATH];
	char	szLastDirectory[MAX_PATH];

	// non destructively determine the vpc_scripts directory
	bool bFound = false;
	szLastDirectory[0] = '\0';
	szScriptPath[0] = '\0';
	V_GetCurrentDirectory( szDirectory, sizeof( szDirectory ) );
	while ( 1 )
	{
		V_ComposeFileName( szDirectory, "vpc_scripts", szScriptPath, sizeof( szScriptPath ) );
		struct _stat statBuf;
		if ( _stat( szScriptPath, &statBuf ) != -1 )
		{
			bFound = true;
			break;
		}

		// previous dir
		V_ComposeFileName( szDirectory, "..", szScriptPath, sizeof( szScriptPath ) );

		char fullPath[MAX_PATH];
		if ( _fullpath( fullPath, szScriptPath, sizeof( fullPath ) ) )
		{
			V_strncpy( szDirectory, fullPath, sizeof( szDirectory ) );
		}

		if ( !V_stricmp( szDirectory, szLastDirectory ) )
		{
			// can back up no further
			break;
		}
		strcpy( szLastDirectory, szDirectory );
	}

	if ( !bFound )
	{
		VPCError( "Failed to determine source directory from current path. Expecting 'vpc_scripts' in source path." );
	}

	char szSourcePath[MAX_PATH];
	strcpy( szSourcePath, szDirectory );

	// check to ensure that executeable and src directory are in the same tree
	// executeable needs to be tightly bound to its vpc_scripts
	char szModuleBinPath[MAX_PATH];
	GetModuleFileName( NULL, szModuleBinPath, sizeof( szModuleBinPath ) );

	// cannot trust output from GetModuleFileName(), occasionally has ./ or ../ screwing up comparisons
	V_RemoveDotSlashes( szModuleBinPath, '\\' );
	V_strlower( szModuleBinPath );
	V_strncpy( pOutBinPath, szModuleBinPath, outBinPathSize );

	// allowed to run from a root "devbin", for use with junctions
	if ( Sys_StringPatternMatch( "?:\\devbin\\vpc.exe", szModuleBinPath ) )
		return true;

	char *pString = V_stristr( szModuleBinPath, "\\devtools\\bin\\" );
	if ( pString )
	{
		// source dirs should match
		char chSave = *pString;
		*pString = '\0';
		bool bSame = V_stricmp( szSourcePath, szModuleBinPath ) == 0;
		*pString = chSave;

		if ( bSame )
		{
			return true;
		}
	}
	else
	{
		VPCError( "Executable not running from 'devtools/bin' but from unexpected directory '%s'", szModuleBinPath );
	}

	// mismatched, wierd bin patch could have been a result of user's environment path
	// use expected source path which is based on user's cwd to get the real bin path
	V_strncpy( pOutBinPath, szSourcePath, outBinPathSize );
	V_strncat( pOutBinPath, "\\devtools\\bin\\vpc.exe", outBinPathSize );
	struct _stat statBuf;
	if ( _stat( pOutBinPath, &statBuf ) == -1 )
	{
		VPCError( "Correct executeable missing, should be at '%s'", pOutBinPath );
	}

	// yikes, wrong executeable was started, agreed behavior was to restart based on user's cwd
	// REALLY want users to see this, it indicates a possible hazard of using the wrong vpc
	Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "********************************************************************************\n" );
	Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "Wrong Executable '%s' Running!\nRestarting at '%s'\n", szModuleBinPath, pOutBinPath );
	Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "********************************************************************************\n" );

	return false;
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::DetermineSourcePath()
{
	char	szSourcePath[MAX_PATH];
	char	szLastDirectory[MAX_PATH];

	char szOldPath[MAX_PATH];
	V_GetCurrentDirectory( szOldPath, sizeof( szOldPath ) );

	// find vpc_scripts from cwd
	szLastDirectory[0] = '\0';
	bool bFound = false;
	while ( 1 )
	{
		V_GetCurrentDirectory( szSourcePath, sizeof( szSourcePath ) );
		if ( !V_stricmp( szSourcePath, szLastDirectory ) )
		{
			// can back up no further
			break;
		}
		V_strncpy( szLastDirectory, szSourcePath, sizeof( szLastDirectory ) );

		char szTestDir[MAX_PATH];
		V_ComposeFileName( szSourcePath, "vpc_scripts", szTestDir, sizeof( szTestDir ) );
		struct _stat statBuf;
		if ( _stat( szTestDir, &statBuf ) != -1 )
		{
			bFound = true;
			break;
		}

		// previous dir
		char szPrevDir[MAX_PATH];
		V_ComposeFileName( szSourcePath, "..", szPrevDir, sizeof( szPrevDir ) );
		V_SetCurrentDirectory( szPrevDir );
	}

	if ( !bFound )
	{
		VPCError( "Failed to determine source directory from current path. Expecting 'vpc_scripts' in source path." );
	}

	// Remember the source path and restore the path to where it was.
	m_SourcePath = szSourcePath;
	V_SetCurrentDirectory( szOldPath );

	// always emit source path, identifies MANY redundant user problems
	// users can easily run from an unintended place due to botched path, mangled directories, etc
	Log_Msg( LOG_VPC, "Source Path: %s\n", m_SourcePath.Get() );
}

//-----------------------------------------------------------------------------
// Sets the working directory to .../vpc_scripts as all scripts are
// guaranteed relative to the vpc script directory.
//-----------------------------------------------------------------------------
void CVPC::SetDefaultSourcePath()
{
	V_SetCurrentDirectory( m_SourcePath.Get() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::IsProjectCurrent( const char *pOutputFilename, bool bSpewStatus )
{
	// default is project is stale
	if ( !Sys_Exists( pOutputFilename ) )
	{
		return false;
	}

	if ( Is2010() && !Sys_Exists( CFmtStr( "%s.filters", pOutputFilename ) ) )
	{
		return false;
	}

	char errorString[1024];
	bool bCRCValid = VPC_CheckProjectDependencyCRCs( pOutputFilename, m_SupplementalCRCString.Get(), errorString, sizeof( errorString ) );

	if ( bSpewStatus )
	{
		if ( bCRCValid )
		{
			VPCStatus( true, "Valid: '%s' Passes CRC Checks.", pOutputFilename );
		}
		else
		{
			VPCStatus( true, "Stale: '%s' Requires Rebuild. [%s]", pOutputFilename, errorString );
		}
	}

	return bCRCValid;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::SpewUsage( void )
{
	// skip header if user requested specific detail
	bool bNoHeader =
		m_bSpewPlatforms ||
		m_bSpewGames ||
		m_bSpewProjects ||
		m_bSpewGroups ||
		m_bSpewProperties ||
		m_BuildCommands.Count();

	if ( !bNoHeader )
	{
		Log_Msg( LOG_VPC, "\n" );

		if ( !m_bHelp )
		{
			// terse
			Log_Msg( LOG_VPC, "Type vpc /h for help...\n" );
		}
		else
		{
			// verbose
			Log_Msg( LOG_VPC, "usage: vpc [options] <+/-/*project or group>\n");

			Log_Msg( LOG_VPC, "\n" );
			Log_Msg( LOG_VPC, "Examples:\n" );

			Log_Msg( LOG_VPC, "\n" );
			Log_Msg( LOG_VPC, "  Single .vcproj generation:\n" );
			Log_Msg( LOG_VPC, "    vpc +client /hl2     <-- Creates a Win32 .vcproj for the HL2 client.\n" );
			Log_Msg( LOG_VPC, "    vpc +shaderapi /x360 <-- Creates a Xbox360 .vcproj for the shaderapi.\n" );

			Log_Msg( LOG_VPC, "\n" );
			Log_Msg( LOG_VPC, "  Multiple .vcproj generation - Multiple Projects for Games and Platforms:\n" );
			Log_Msg( LOG_VPC, "    vpc +client /hl2 /tf           <-- Creates ALL the Win32 .vcprojs for the HL2 and TF client.\n" );
			Log_Msg( LOG_VPC, "    vpc +gamedlls /allgames        <-- Creates ALL the Win32 .vcprojs for client and server for all GAMES.\n" );
			Log_Msg( LOG_VPC, "    vpc +tools -tier0 /win32 /x360 <-- Creates ALL the Win32 and Xbox360 .vcprojs for the tool projects but not the tier0 project.\n" );

			Log_Msg( LOG_VPC, "\n" );
			Log_Msg( LOG_VPC, "  Use +/- to add or remove projects or groups.\n");
			Log_Msg( LOG_VPC, "  Use *   to add a project and all projects that depend on it.\n");
			Log_Msg( LOG_VPC, "  Use @   to add a project and all projects that it depends on.\n");
			Log_Msg( LOG_VPC, "  Use /h spew final target build set only (no .vcproj created).\n");
			Log_Msg( LOG_VPC, "\n" );
			Log_Msg( LOG_VPC, "  Further details can be found on Valve Internal Wiki on VPC.\n" );

			Log_Msg( LOG_VPC, "\n--- OPTIONS ---\n" );
			Log_Msg( LOG_VPC, "[/q]:          Quiet mode (quiet mode is automatically on if the VPC_QUIET environment variable is set)\n" );
			Log_Msg( LOG_VPC, "[/v]:          Verbose\n" );
			Log_Msg( LOG_VPC, "[/f]:          Force generate .vcproj, otherwise use crc checks\n" );
			Log_Msg( LOG_VPC, "[/dp]:         Decorate project names with platform\n" );
			Log_Msg( LOG_VPC, "[/testmode]:   Override output .vcproj file to be named 'test.vcproj'\n" );
#ifdef VPC_SCC_INTEGRATION
			Log_Msg( LOG_VPC, "[/nosrcctl]:   Disable P4SCC source control integration - can also set environment variable VPC_SRCCTL to 0\n" );
#else
			Log_Msg( LOG_VPC, "[/srcctl]:     Enable P4SCC source control integration - can also set environment variable VPC_SRCCTL to 1\n" );
#endif
			Log_Msg( LOG_VPC, "[/mirror]:     <path> - Mirror output files to specified path. Used for A:B testing.\n" );
			Log_Msg( LOG_VPC, "[/2015]:       Generate projects and solutions for Visual Studio 2015 [default]\n" );
			Log_Msg( LOG_VPC, "[/2013]:       Generate projects and solutions for Visual Studio 2013\n" );
			Log_Msg( LOG_VPC, "[/2012]:       Generate projects and solutions for Visual Studio 2012\n" );
			Log_Msg( LOG_VPC, "[/2010]:       Generate projects and solutions for Visual Studio 2010\n" );
			Log_Msg( LOG_VPC, "[/2005]:       Generate projects and solutions for Visual Studio 2005\n" );
			Log_Msg( LOG_VPC, "[/2008]:       Generate projects and solutions for Visual Studio 2008\n" );
			Log_Msg( LOG_VPC, "[/windows]:    Generate projects for both Win32 and Win64\n" );
			Log_Msg( LOG_VPC, "[/unity]:      Enable unity file generation\n" );
			Log_Msg( LOG_VPC, "[/32bittools]: Specify 32-bit toolchain in VC++ even when compiling 64 bit target\n" );

			Log_Msg( LOG_VPC, "\n--- Help ---\n" );
			Log_Msg( LOG_VPC, "[/h]:          Help\n" );
			Log_Msg( LOG_VPC, "[/?]:          Help\n" );
			Log_Msg( LOG_VPC, "[/platforms]:  Spew Platforms\n" );
			Log_Msg( LOG_VPC, "[/games]:      Spew Games\n" );
			Log_Msg( LOG_VPC, "[/projects]:   Spew Projects\n" );
			Log_Msg( LOG_VPC, "[/groups]:     Spew Groups\n" );
			Log_Msg( LOG_VPC, "[/properties]: Spew VS2005 Properties\n" );

			Log_Msg( LOG_VPC, "\n--- Conditionals ---\n" );
			Log_Msg( LOG_VPC, "[/profile]:    Set Reserved $PROFILE=1\n" );
			Log_Msg( LOG_VPC, "[/retail]:     Set Reserved $RETAIL=1\n" );
			Log_Msg( LOG_VPC, "[/callcap]:    Set Reserved $CALLCAP=1\n" );
			Log_Msg( LOG_VPC, "[/fastcap]:    Set Reserved $FASTCAP=1\n" );
			Log_Msg( LOG_VPC, "[/memtest]:    Set Reserved $MEMTEST=1\n" );
			Log_Msg( LOG_VPC, "[/nofpo]:      Set Reserved $NOFPO=1\n" );
			Log_Msg( LOG_VPC, "[/lv]:         Set Reserved $LV=1\n" );
			Log_Msg( LOG_VPC, "[/demo]:       Set Reserved $DEMO=1\n" );
			Log_Msg( LOG_VPC, "[/no_steam]:   Set Reserved $NO_STEAM=1\n" );
			Log_Msg( LOG_VPC, "[/qtdebug]:    Set Reserved $QTDEBUG=1\n" );
			Log_Msg( LOG_VPC, "[/no_ceg]:	  Set Reserved $NO_CEG=1\n" );
			Log_Msg( LOG_VPC, "[/upload_ceg]: Set Reserved $UPLOAD_CEG=1\n" );

			Log_Msg( LOG_VPC, "\n--- Other ---\n" );
			Log_Msg( LOG_VPC, "[/mksln]:      <.sln filename> - make a solution file\n" );
			Log_Msg( LOG_VPC, "[/p4sln]:      <.sln filename> <changelists...> - make a solution file based on\n" );
			Log_Msg( LOG_VPC, "               the changelist. Changelists can be specific numbers, 0 or \"default\"\n" );
			Log_Msg( LOG_VPC, "               for the default changelist, or \"all\" for all active changelists.\n" );
			Log_Msg( LOG_VPC, "[/nop4add]:    Don't automatically add project files to Perforce\n" );
			Log_Msg( LOG_VPC, "[/slnitems]:   <filename> - adds all files listed in <filename> to generated\n" );
			Log_Msg( LOG_VPC, "               solutions\n" );
			Log_Msg( LOG_VPC, "[/showdeps]:   Show an example dependency chain for each project that depends\n" );
			Log_Msg( LOG_VPC, "               on your p4 change list(s).  Use with /p4sln.\n" );
			Log_Msg( LOG_VPC, "[/checkfiles]: Check for the existence of files in $file commands. For debugging vpc files.\n" );
			Log_Msg( LOG_VPC, "               Only works if the currrent directory is the project directory.\n" );
//			Log_Msg( LOG_VPC, "[/novpcgame]:  Disable reserved vpc macro $VPCGAME and $VPCGAMECAPS.\n" );
//			Log_Msg( LOG_VPC, "               By default if a single game is specified on command line, then that specified\n" );
//			Log_Msg( LOG_VPC, "               game name will be used as a value for $VPCGAME and $VPCGAMECAPS macros.\n" );
			Log_Msg( LOG_VPC, "[/define:xxx]: Enable a custom conditional $XXX to use for quick testing in VPC files.\n" );
		}
	}

	if ( m_Conditionals.Count() && m_bSpewPlatforms )
	{
		bool bFirstDefine = false;
		for ( int i = 0; i < m_Conditionals.Count(); i++ )
		{
			if ( m_Conditionals[i].type != CONDITIONAL_PLATFORM )
				continue;
			if ( !bFirstDefine )
			{
				Log_Msg( LOG_VPC, "\n--- PLATFORMS ---\n" );
				bFirstDefine = true;
			}
			Log_Msg( LOG_VPC, "%s%s\n", m_Conditionals[i].upperCaseName.String(), m_Conditionals[i].m_bDefined ? " = 1" : "" );
		}
	}

	if ( m_Conditionals.Count() && m_bSpewGames )
	{
		bool bFirstGame = false;
		for ( int i = 0; i < m_Conditionals.Count(); i++ )
		{
			if ( m_Conditionals[i].type != CONDITIONAL_GAME )
				continue;
			if ( !bFirstGame )
			{
				Log_Msg( LOG_VPC, "\n--- GAMES ---\n" );
				bFirstGame = true;
			}
			Log_Msg( LOG_VPC, "%s%s\n", m_Conditionals[i].upperCaseName.String(), m_Conditionals[i].m_bDefined ? " = 1" : "" );
		}
	}

	if ( m_Projects.Count() && m_bSpewProjects )
	{
		// spew all sorted projects
		Log_Msg( LOG_VPC, "\n--- PROJECTS ---\n" );
		CUtlRBTree< const char * > sorted( 0, 0, CaselessStringLessThan );
		for ( int i = 0; i < m_Projects.Count(); i++ )
		{
			sorted.Insert( m_Projects[i].name.String() );
		}
		for ( int i = sorted.FirstInorder(); i != sorted.InvalidIndex(); i = sorted.NextInorder( i ) )
		{
			Log_Msg( LOG_VPC, "[+/-] %s\n", sorted[i] );
		}
	}

	if ( g_pVPC->m_GroupTags.Count() && m_bSpewGroups )
	{
		// spew all sorted groups
		Log_Msg( LOG_VPC, "\n--- GROUPS ---\n" );
		CUtlRBTree< const char * > sorted( 0, 0, CaselessStringLessThan );
		for ( int i = 0; i < g_pVPC->m_GroupTags.Count(); i++ )
		{
			if ( !g_pVPC->m_GroupTags[i].bSameAsProject )
			{
				sorted.Insert( g_pVPC->m_GroupTags[i].name.String() );
			}
		}
		for ( int i = sorted.FirstInorder(); i != sorted.InvalidIndex(); i = sorted.NextInorder( i ) )
		{
			Log_Msg( LOG_VPC, "[+/-] %s\n", sorted[i] );
		}
	}

#if 0
#if defined( _WIN32 )
	if ( m_bSpewProperties )
	{
		for ( int i = 0; i < KEYWORD_MAX; i++ )
		{
			VPC_Config_SpewProperties( (configKeyword_e)i );
		}
	}
#endif
#endif

	if ( m_BuildCommands.Count() )
	{
		// spew details about each command
		Log_Msg( LOG_VPC, "\nUser Build Commands:\n" );
		Log_Msg( LOG_VPC, "--------------------\n" );
		for ( int i = 0; i < m_BuildCommands.Count(); i++ )
		{
			Log_Msg( LOG_VPC, "%s\n", m_BuildCommands[i].String() );
			groupTagIndex_t groupTagIndex = VPC_Group_FindOrCreateGroupTag( m_BuildCommands[i].Get()+1, false );
			if ( groupTagIndex == INVALID_INDEX )
			{
				Log_Msg( LOG_VPC, "   ??? (Unknown Group)\n" );
			}
			else
			{
				groupTag_t *pGroupTag = &g_pVPC->m_GroupTags[groupTagIndex];
				for (int j=0; j<pGroupTag->groups.Count(); j++ )
				{
					group_t *pGroup = &m_Groups[pGroupTag->groups[j]];
					for ( int k=0; k<pGroup->projects.Count(); k++ )
					{
						Log_Msg( LOG_VPC, "   %s\n",  m_Projects[pGroup->projects[k]].name.String() );
					}
				}
			}
		}

		Log_Msg( LOG_VPC, "\nTarget Projects:\n" );
		Log_Msg( LOG_VPC, "----------------\n" );
		if ( m_TargetProjects.Count() )
		{
			for ( int i=0; i<m_TargetProjects.Count(); i++ )
			{
				Log_Msg( LOG_VPC, "%s\n", m_Projects[m_TargetProjects[i]].name.String() );
			}
		}
		else
		{
			Log_Msg( LOG_VPC, "Empty Set (no output)\n" );
		}

		Log_Msg( LOG_VPC, "\nTarget Games:\n" );
		Log_Msg( LOG_VPC, "-------------\n" );
		bool bHasDefine = false;
		for ( int i=0; i<m_Conditionals.Count(); i++ )
		{
			if ( m_Conditionals[i].type != CONDITIONAL_GAME )
				continue;
			if ( m_Conditionals[i].m_bDefined )
			{
				Log_Msg( LOG_VPC, "$%s = 1\n", m_Conditionals[i].upperCaseName.String() );
				bHasDefine = true;
			}
		}
		if ( !bHasDefine )
		{
			Log_Msg( LOG_VPC, "No Game Set!\n" );
		}

		Log_Msg( LOG_VPC, "\nTarget Platforms:\n" );
		Log_Msg( LOG_VPC, "-----------------\n" );
		bHasDefine = false;
		for ( int i=0; i<m_Conditionals.Count(); i++ )
		{
			if ( m_Conditionals[i].type != CONDITIONAL_PLATFORM )
				continue;
			if ( m_Conditionals[i].m_bDefined )
			{
				Log_Msg( LOG_VPC, "$%s = 1\n", m_Conditionals[i].upperCaseName.String() );
				bHasDefine = true;
			}
		}
		if ( !bHasDefine )
		{
			Log_Msg( LOG_VPC, "No Platform Set!\n" );
		}

		Log_Msg( LOG_VPC, "\nCustom Conditionals:\n" );
		Log_Msg( LOG_VPC, "---------------------\n" );
		bHasDefine = false;
		for ( int i=0; i<m_Conditionals.Count(); i++ )
		{
			if ( m_Conditionals[i].type != CONDITIONAL_CUSTOM )
				continue;
			if ( m_Conditionals[i].m_bDefined )
			{
				Log_Msg( LOG_VPC, "$%s = 1\n", m_Conditionals[i].upperCaseName.String() );
				bHasDefine = true;
			}
		}
		if ( !bHasDefine )
		{
			Log_Msg( LOG_VPC, "No Custom Defines Set!\n" );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::HandleSingleCommandLineArg( const char *pArg )
{
	if ( ( pArg[0] == '-' ) || ( pArg[0] == '/' ) )
	{
		// skip past arg prefix
		const char *pArgName = pArg+1;

		// check options
		if ( !V_stricmp( pArgName, "h" ) || !V_stricmp( pArgName, "?" ) || !V_stricmp( pArgName, "help" ) )
		{
			m_bHelp = true;
			m_bUsageOnly = true;
		}
		else if ( !V_stricmp( pArgName, "v" ) || !V_stricmp( pArgName, "verbose" ) )
		{
			m_bVerbose = true;
		}
		else if ( !V_stricmp( pArgName, "testmode" ) || !V_stricmp( pArgName, "test" ) )
		{
			m_bTestMode = true;
		}
		else if ( !V_stricmp( pArgName, "f" ) || !V_stricmp( pArgName, "force" ) )
		{
			m_bForceGenerate = true;
		}
		else if ( !V_stricmp( pArgName, "no_posix_pch" ) )
		{
			// Not implemented for win32 generators since those just pass through raw options :-/
			m_bNoPosixPCH = true;
			// Ensure this changes CRC. Ideally it would only change if anything ended up using the PCH conditional
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "novpcgame" ) )
		{
			m_bEnableVpcGameMacro = false;
		}
		else if ( !V_stricmp( pArgName, "checkfiles" ) )
		{
			m_bCheckFiles = true;
		}
		else if ( !V_stricmp( pArgName, "nocheckfiles" ) )
		{
			m_bCheckFiles = false;
		}
		else if ( !V_stricmp( pArgName, "showcaseissues" ) || !V_stricmp( pArgName, "showcase" ) )
		{
			m_bShowCaseIssues = true;
		}
		else if ( !V_stricmp( pArgName, "dp" ) )
		{
			m_bDecorateProject = true;
		}
		else if ( char const *szActualDecorateName = StringAfterPrefix( pArgName, "decorate:" ) )
		{
			m_bDecorateProject = true;
			m_strDecorate = szActualDecorateName;
		}
		else if ( !V_stricmp( pArgName, "dedicated" ) )
		{
			m_bDedicatedBuild = true;
			m_bAppendSrvToDedicated = true;
			m_bUseValveBinDir = true;
		}
		else if ( !V_stricmp( pArgName, "use_valve_bin" ) )
		{
			m_bUseValveBinDir = true;
		}
		else if ( !V_stricmp( pArgName, "platforms" ) || !V_stricmp( pArgName, "plats" ) )
		{
			m_bSpewPlatforms = true;
			m_bUsageOnly = true;
		}
		else if ( !V_stricmp( pArgName, "games" ) )
		{
			m_bSpewGames = true;
			m_bUsageOnly = true;
		}
		else if ( !V_stricmp( pArgName, "projects" ) )
		{
			m_bSpewProjects = true;
			m_bUsageOnly = true;
		}
		else if ( !V_stricmp( pArgName, "groups" ) )
		{
			m_bSpewGroups = true;
			m_bUsageOnly = true;
		}
		else if ( !V_stricmp( pArgName, "properties" ) )
		{
			m_bSpewProperties = true;
			m_bUsageOnly = true;
		}
		else if ( !V_stricmp( pArgName, "allgames" ) )
		{
			// shortcut for all games defined
			for ( int j=0; j<m_Conditionals.Count(); j++ )
			{
				if ( m_Conditionals[j].type == CONDITIONAL_GAME )
				{
					m_Conditionals[j].m_bDefined = true;
				}
			}
		}
		else if ( !V_stricmp( pArgName, "showdeps" ) )
		{
			m_bShowDeps = true;
		}
		else if ( !V_stricmp( pArgName, "nop4add" ) )
		{
			m_bP4AutoAdd = false;
		}
		else if ( !V_stricmp( pArgName, "2005" ) )
		{
			m_eVSVersion = k_EVSVersion_2005;
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "2008" ) )
		{
			m_eVSVersion = k_EVSVersion_2008;
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "2010" ) )
		{
			m_eVSVersion = k_EVSVersion_2010;
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "2012" ) )
		{
			m_eVSVersion = k_EVSVersion_2012;
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "2013" ) )
		{
			m_eVSVersion = k_EVSVersion_2013;
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "2015" ) )
		{
			m_eVSVersion = k_EVSVersion_2015;
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "nounity" ) )
		{
			m_bUseUnity = false;
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "unity" ) )
		{
			m_bUseUnity = true;
			m_ExtraOptionsCRCString += pArgName;
		}
		else if ( !V_stricmp( pArgName, "verbosemakefile" ) )
		{
			m_bVerboseMakefile = true;
		}
		else if ( char const *szActualDefineName = StringAfterPrefix( pArgName, "define:" ) )
		{
			// allow setting custom defines straight from command line
			conditional_t *pConditional = FindOrCreateConditional( szActualDefineName, true, CONDITIONAL_CUSTOM );
			if ( pConditional )
			{
				pConditional->m_bDefined = true;

				m_ExtraOptionsCRCString += "/define:"; // force this into additional CRC string
				m_ExtraOptionsCRCString += pConditional->name.Get(); // force this into additional CRC string
			}
		}
		else if ( !V_stricmp( pArgName, "32bittools" ) )
		{
			m_b32BitTools = true;									// use 32-bit toolchain even when building 64-bit targets
			m_ExtraOptionsCRCString += pArgName;					// Make sure these options affect the CRC.
		}
		else if ( !V_stricmp( pArgName, "nosrcctl" ) )
		{
			m_bP4SCC = false;
			m_ExtraOptionsCRCString += pArgName;					// Make sure these options affect the CRC.
		}
		else if ( !V_stricmp( pArgName, "srcctl" ) )
		{
			m_bP4SCC = true;
			m_ExtraOptionsCRCString += pArgName;					// Make sure these options affect the CRC.
		}
		else
		{
			// not a recognized option, try conditionals
			// find in list of conditionals
			conditional_t *pConditional = FindOrCreateConditional( pArgName, false, CONDITIONAL_NULL );
			if ( !pConditional )
			{
				// not a recognized conditional, add to build commands
				int index = m_BuildCommands.AddToTail();
				m_BuildCommands[index] = pArg;
			}
			else
			{
				// found conditional, mark as defined
				pConditional->m_bDefined = true;
			}
		}
	}
	else if ( pArg[0] == '+' || pArg[0] == '*' || pArg[0] == '@' )
	{
		// add to build commands
		int index = m_BuildCommands.AddToTail();
		m_BuildCommands[index] = pArg;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::ParseBuildOptions( int argc, char *argv[] )
{
	m_bDedicatedBuild = false;
	m_bAppendSrvToDedicated = false;
	m_bUseValveBinDir = false;

	// parse options
	// prefer +??? or -??? prefix syntax for groups and /??? for options because less confusing for new vpc users
	// for ease we will support -??? prefix syntax for matched options as well
	for ( int i = 1; i < argc; i++ )
	{
		const char *pArg = argv[i];

		if ( !V_stricmp( pArg, "/mksln" ) )
		{
			if ( !m_P4SolutionFilename.IsEmpty() )
			{
				VPCError( "Can't use /mksln with /p4sln." );
			}

			if ( (i+1) >= argc )
			{
				VPCError( "/mksln requires a filename after it." );
			}

			// If the next parameter is a standard + or - or / or * parameter, then we take that to be the name of the solution file.
			// So vpc /mksln +engine would generate engine.sln.
			if ( argv[i+1][0] == '+' || argv[i+1][0] == '-' || argv[i+1][0] == '/' || argv[i+1][0] == '*' || argv[i+1][0] == '@' )
			{
				m_MKSolutionFilename = &argv[i+1][1];
			}
			else
			{
				m_MKSolutionFilename = argv[i+1];
				++i;
			}
		}
		else if ( !V_stricmp( pArg, "/p4sln" ) )
		{
			if ( !m_MKSolutionFilename.IsEmpty() )
			{
				VPCError( "Can't use /mksln with /p4sln." );
			}

			// Get the solution filename.
			++i;
			if ( i >= argc || argv[i][0] == '+' || argv[i][0] == '-' || argv[i][0] == '/' || argv[i][0] == '*' || argv[i][0] == '@' )
			{
				VPCError( "%s <solution filename> <changelist number> [ [restrict_to_group] ].", pArg );
			}

			m_P4SolutionFilename = argv[i];

			// Get the changelist number.
			while ( 1 )
			{
				++i;

				// No more args?
				if ( i >= argc )
					break;

				// Special syntax for including all changelists.
				if ( V_stricmp( argv[i], "all" ) == 0 )
				{
					m_iP4Changelists.AddToTail( -1 );
					continue;
				}

				// Special syntax for including default changelists.
				if ( V_stricmp( argv[i], "default" ) == 0 )
				{
					m_iP4Changelists.AddToTail( 0 );
					continue;
				}

				// This arg isn't a changelist number?
				if ( argv[i][0] < '0' || argv[i][0] > '9' )
				{
					--i;
					break;
				}

				// Add the changelist number.
				m_iP4Changelists.AddToTail( atoi( argv[i] ) );
			}

			// Make sure at least one changelist number was specified.
			if ( m_iP4Changelists.Count() == 0 )
			{
				VPCError( "%s <solution filename> <changelist number> [additional changelist numbers] [ [restrict_to_group] ].", pArg );
			}

			// Get the group restriction.
			while ( 1 )
			{
				++i;

				// No more args?
				if ( i >= argc )
					break;

				if ( argv[i][0] != '[' || argv[i][V_strlen( argv[i] ) - 1] != ']' )
				{
					// This arg isn't a group name
					--i;
					break;
				}

				// strip the braces
				CUtlString groupName = argv[i];
				int nLastChar = groupName.Length() - 1;
				groupName = groupName.Slice( 1, nLastChar );

				// Add the restricted group name
				m_P4GroupRestrictions.AddToTail( groupName );
			}
		}
		else if ( !V_stricmp( pArg, "/slnitems" ) )
		{
			// Get the solution items filename
			++i;
			if ( i >= argc || argv[i][0] == '+' || argv[i][0] == '-' || argv[i][0] == '/' || argv[i][0] == '*' || argv[i][0] == '@' )
			{
				VPCError( "/slnitems <solution items filename>." );
			}

			m_SolutionItemsFilename = argv[i];
		}
		else if ( !V_stricmp( pArg, "/mirror" ) )
		{
			// force an output mirror, used for A:B comparison runs
			++i;
			if ( i >= argc || argv[i][0] == '+' || argv[i][0] == '-' || argv[i][0] == '/' || argv[i][0] == '*' || argv[i][0] == '@' )
			{
				VPCError( "/mirror <absolute path>." );
			}

			m_OutputMirrorString = argv[i];
			if ( !m_OutputMirrorString.IsEmpty() && !V_IsAbsolutePath( m_OutputMirrorString.Get() ) )
			{
				VPCError( "/mirror <path> requires an absolute path specification." );
			}
		}
		else
		{
			HandleSingleCommandLineArg( pArg );
		}
	}

	// If they did /p4sln but didn't specify any build commands, then have it check everything.
	if ( m_iP4Changelists.Count() > 0 && m_BuildCommands.Count() == 0 )
	{
		m_bP4SlnCheckEverything = true;
	}

	CheckForInstalledXDK();
}

//-----------------------------------------------------------------------------
//	Generate a string supplemental to CRC data, derived from command-line options,
//	so varying certain command-line options can cause .VCPROJ rebuilds.
//-----------------------------------------------------------------------------
void CVPC::GenerateOptionsCRCString()
{
	m_SupplementalCRCString = "_";

	conditional_t *pConditional	= FindOrCreateConditional( "PROFILE", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Pr";
	}

	pConditional = FindOrCreateConditional( "RETAIL", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Rt";
	}

	pConditional = FindOrCreateConditional( "CALLCAP", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Cc";
	}

	pConditional = FindOrCreateConditional( "FASTCAP", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Fc";
	}

	pConditional = FindOrCreateConditional( "MEMTEST", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Mt";
	}

	pConditional = FindOrCreateConditional( "NOFPO", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Nf";
	}

	pConditional = FindOrCreateConditional( "LV", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Lv";
	}

	pConditional = FindOrCreateConditional( "DEMO", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Dm";
	}

	pConditional = FindOrCreateConditional( "NO_STEAM", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Ns";
	}

	pConditional = FindOrCreateConditional( "QTDEBUG", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Qt";
	}

	pConditional = FindOrCreateConditional( "NO_CEG", false, CONDITIONAL_NULL );

	//
	// !!NEVER INTEGRATE THIS CHANGE TO REL BRANCHES!!
	// !!THIS TURNS OF CEG FOR NON-REL BRANCHES!!
	// -Jeep & AaronN
	//
	//if ( pConditional && pConditional->m_bDefined )
	if ( pConditional )
	{
		pConditional->m_bDefined = true;
		m_SupplementalCRCString += "Nc";
	}

	pConditional = FindOrCreateConditional( "UPLOAD_CEG", false, CONDITIONAL_NULL );
	if ( pConditional && pConditional->m_bDefined )
	{
		m_SupplementalCRCString += "Uc";
	}

	if ( !m_ExtraOptionsCRCString.IsEmpty() )
	{
		m_SupplementalCRCString += CFmtStr( "_%s_", m_ExtraOptionsCRCString.Get() );
	}
}

//-----------------------------------------------------------------------------
//	Restart self from correct location and re-run. Returns FALSE if not applicable,
//	otherwise TRUE if restart occurred.
//-----------------------------------------------------------------------------
bool CVPC::RestartFromCorrectLocation( bool *pIsChild )
{
#if defined( POSIX )
	return false;
#else
	// recursive restart guard
	// restart is a hidden internal param, always the last argument
	// presence identifies spawned process
	bool bIsRestart = false;
	if ( !V_stricmp( m_ppArgv[m_nArgc-1], "/restart" ) )
	{
		bIsRestart = true;
	}
	*pIsChild = bIsRestart;

	char szBinPath[MAX_PATH];
	if ( !CheckBinPath( szBinPath, sizeof( szBinPath ) ) )
	{
		if ( bIsRestart )
		{
			VPCError( "Cyclical Restart: Tell A Programmer!, Aborting." );
		}

		// replicate arguments, add -restart as a recursion guard for the new process
		char *newArgs[128];
		if ( m_nArgc >= V_ARRAYSIZE( newArgs ) - 2 )
		{
			VPCError( "Excessive Arguments: Tell A Programmer!, Aborting." );
		}
		int i;
		for ( i = 0; i < m_nArgc; i++ )
		{
			newArgs[i] = m_ppArgv[i];
		}
		newArgs[i++] = "/restart";
		newArgs[i++] = NULL;

		// restart using synchronous semantic, async semantic causes wierd hang
		int status = _spawnv( _P_WAIT, szBinPath, newArgs );
		if ( !status )
		{
			// called process exited normally
			return true;
		}
		else if ( status > 0 )
		{
			// called process exited with error, pass it along
			exit( status );
		}

		// called process could not be started
		VPCError( "Restart of '%s' failed\n", szBinPath );
	}

	// process is running from correct location
	return false;
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::CheckForInstalledXDK()
{
#ifndef POSIX
	if ( !IsPlatformDefined( "X360" ) )
	{
		// caller not doing any 360 work, so ignore
		return;
	}

	// quick and dirty early check for 360 XDK ability
	// can only detect simplistic condition, VPC can't validate a perfect XDK/MSDEV installation
	bool bHasXDK = false;
	const char *pXDK = getenv( "XEDK" );
	if ( pXDK && pXDK[0] )
	{
		// look for expected compiler
		char fullPath[MAX_PATH];
		V_strncpy( fullPath, pXDK, sizeof( fullPath ) );
		V_AppendSlash( fullPath, sizeof( fullPath ) );
		V_strncat( fullPath, "bin\\win32\\cl.exe", sizeof( fullPath ) );
		int fileSize = Sys_FileLength( fullPath, false );
		if ( fileSize > 0 )
		{
			bHasXDK = true;
		}
	}
	if ( !bHasXDK )
	{
		VPCError( "Cannot Build For Xbox 360, XDK is missing or damaged. Remove /x360 from command line." );
	}
#endif
}

void CVPC::CreateOutputFilename( project_t *pProject, const char *pchPlatform, const char *pchPhase, const char *pGameName, const char *pchExtension )
{
	const char *pProjectFileNamePrefix = m_bTestMode ? "test" : pProject->name.String();

	m_OutputFilename = pProjectFileNamePrefix;

	if ( pchPlatform && pchPlatform[0] )
	{
		// non-pc platforms get decorated
		m_OutputFilename += "_";
		m_OutputFilename += pchPlatform;
	}

	if( m_bAppendSrvToDedicated )
	{
		// Add _srv to the temp lib path. Ie: tier0/obj_tier0_linux32_srv
		m_OutputFilename += "_srv";
	}

	if ( pchPhase && pchPhase[0] )
	{
		m_OutputFilename += "_";
		m_OutputFilename += pchPhase;
	}

	if ( pGameName && pGameName[0] )
	{
		// game projects get decorated
		m_OutputFilename += "_";
		m_OutputFilename += pGameName;
	}

	if ( m_bDecorateProject )
	{
		char rgchDecorate[256] = "";
		DecorateProjectName( rgchDecorate );
		m_OutputFilename += rgchDecorate;
	}

	if ( pchExtension && pchExtension[0] )
	{
		m_OutputFilename += ".";
		m_OutputFilename += pchExtension;
	}
}

bool CVPC::BuildTargetProject( IProjectIterator *pIterator, projectIndex_t projectIndex, script_t *pProjectScript, const char *pGameName )
{
	// evaluate the project's script conditional which determines game/platform
	if ( !EvaluateConditionalExpression( pProjectScript->m_condition.String() ) )
	{
		// conditionals prevent this project from consideration
		return false;
	}

	// set once anything is expected to output
	m_bAnyProjectQualified = true;

	if ( !m_Projects.IsValidIndex( projectIndex ) )
	{
		// unexpected bad project index
		Assert( 0 );
		return false;
	}
	project_t *pProject = &m_Projects[projectIndex];

	// track the internal project name, unaffected by user name mangling
	m_ProjectName = pProject->name.String();
	m_LoadAddressName = pProject->name.String();

	// win32 projects are the most prevalent, so by popular demand they have no decoration
	// all other platforms use their platform name as a suffix
	const char *pPlatformName = NULL;
	if ( !IsPlatformDefined( "win32" ) )
	{
		pPlatformName = GetTargetPlatformName();
	}

	//
	// If we're doing multiple build phases, decorate the project
	// names to indicate which phase they build in, and always include
	// the platform.
	//
	const char *pPhaseName = NULL;
	if ( FindOrCreateConditional( "phase2", false, CONDITIONAL_CUSTOM ) )
	{
		pPlatformName = GetTargetPlatformName();
		pPhaseName = "phase2";
	}
	else if ( FindOrCreateConditional( "phase1", false, CONDITIONAL_CUSTOM ) )
	{
		pPlatformName = GetTargetPlatformName();
		pPhaseName = "phase1";
	}

	// create a decorated project filename based on project/game/platform/etc
	CreateOutputFilename(
		pProject,
		pPlatformName,
		pPhaseName,
		pGameName,
		g_pVPC->GetProjectGenerator()->GetProjectFileExtension() );

	// each vpc script is written with paths relative to their base
	// force each script needs to start relative to their script location
	// this allows vpc to be invoked anywhere, but the groups resolve their projects correctly
	char szScriptPath[MAX_PATH];
	V_ComposeFileName( g_pVPC->GetStartDirectory(), pProjectScript->name.String(), szScriptPath, sizeof( szScriptPath ) );
	V_StripFilename( szScriptPath );
	m_ProjectPath = szScriptPath;
	V_SetCurrentDirectory( szScriptPath );

	// build it
	char szScriptName[MAX_PATH];
	Sys_StripPath( pProjectScript->name.String(), szScriptName );
	return pIterator->VisitProject( projectIndex, szScriptName );
}

//-----------------------------------------------------------------------------
//	Iterate and build each of the projects. Game projects can themselves be
//	auto-iterated to apply each of their mod variant.
//-----------------------------------------------------------------------------
void CVPC::IterateTargetProjects( CUtlVector<projectIndex_t> &projectList, IProjectIterator *pIterator )
{
	m_bGeneratedProject = false;
	m_bAnyProjectQualified = false;

	if ( !projectList.Count() )
	{
		// nothing to do
		return;
	}

	for ( int nProject = 0; nProject < projectList.Count(); nProject++ )
	{
		project_t *pProject = &m_Projects[projectList[nProject]];

		// each project can have 1 or more scripts that are predicated by game/platform conditionals (i.e. client or server)
		for ( int nScript = 0; nScript < pProject->scripts.Count(); nScript++ )
		{
			script_t *pProjectScript = &pProject->scripts[nScript];

			// occurrence of game condition(s) dictates iteration behavior
			// client/server would have multiple game conditions
			bool bHasGameCondition = g_pVPC->ConditionHasDefinedType( pProjectScript->m_condition.String(), CONDITIONAL_GAME );

			if ( !bHasGameCondition )
			{
				// no game condition
				BuildTargetProject( pIterator, projectList[nProject], pProjectScript, NULL );
			}
			else
			{
				// auto iterate through all defined game conditionals, setting each in turn
				// this provides for building say client for all mod(s) that it can support
				for ( int nTargetGame = 0; nTargetGame < m_Conditionals.Count(); nTargetGame++ )
				{
					if ( m_Conditionals[nTargetGame].type != CONDITIONAL_GAME || !m_Conditionals[nTargetGame].m_bDefined )
					{
						// the game conditions must be defined to be considered
						// i.e. the user has specified to build /hl2 /tf2, but not /portal
						continue;
					}

					// only one game condition is active during project generation
					for ( int k = 0; k < m_Conditionals.Count(); k++ )
					{
						// unmark all game conditionals
						if ( m_Conditionals[k].type == CONDITIONAL_GAME )
						{
							m_Conditionals[k].m_bGameConditionActive = false;
						}
					}
					m_Conditionals[nTargetGame].m_bGameConditionActive = true;

					BuildTargetProject( pIterator, projectList[nProject], pProjectScript, m_Conditionals[nTargetGame].name.String() );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Build all the projects in m_targetProjects.
//-----------------------------------------------------------------------------
bool CVPC::BuildTargetProjects()
{
	class CDefaultProjectIterator : public IProjectIterator
	{
	public:
		virtual bool VisitProject( projectIndex_t iProject, const char *pScriptPath )
		{
			Log_Msg( LOG_VPC, "\n" );

			// check project's crc signature
			if ( !g_pVPC->IsForceGenerate() && g_pVPC->IsProjectCurrent( g_pVPC->GetOutputFilename(), true ) && !g_pVPC->IsForceIterate() )
			{
				// valid, does not need to build
				return false;
			}

			return g_pVPC->ParseProjectScript( pScriptPath, 0, false, true );
		}
	};

	if ( !m_TargetProjects.Count() )
	{
		VPCError( "No recognized project(s) to build.  Use /h or /projects or /groups to spew more info." );
	}

	/* At this point we might've built the project dependency graph, which increments the missing file count
	 * if *any* project in your VPC directory has a missing file, so we reset it here to get the accurate count.
	 */
	ResetMissingFilesCount();

	CDefaultProjectIterator iterator;
	IterateTargetProjects( m_TargetProjects, &iterator );

	if ( GetMissingFilesCount() > 0 )
	{
		VPCError( "%d files missing. VPC failed.\n", GetMissingFilesCount() );
	}

	// Catch user attention to notify lack of any expected output
	// Novice users would not be aware of expected conditionals
	if ( !m_bGeneratedProject && !m_bAnyProjectQualified )
	{
		Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "\n" );
		Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "----------------------------\n" );
		Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "!!! No Project Generated !!!\n" );
		Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "----------------------------\n" );
		Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "\n" );
		Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "Possibly missing game, platform, or other conditional expected by script.\n" );
		Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "Use /h verify desired target build set.\n" );

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//	Find the project that corresponds to the specified vcproj and setup
//  to build that project.
//-----------------------------------------------------------------------------
void CVPC::FindProjectFromVCPROJ( const char *pScriptNameVCProj )
{
	// caller is specifying the output vcproj, i.e. via tool shortcut from within MSDEV to re-gen
	// use the vpc standardized output vcproj name to determine re-gen parameters
	// mod and platform will be seperated by '_' after the project name
	// resolve to correct project, best will be longest match, due to project names like foo_? and foo_bar_?
	char szProject[MAX_PATH];
	unsigned int bestLen = 0;
	for ( int i = 0; i < m_Projects.Count(); i++ )
	{
		if ( V_stristr( pScriptNameVCProj, m_Projects[i].name.String() ) )
		{
			if ( bestLen < strlen( m_Projects[i].name.String() ) )
			{
				bestLen = strlen( m_Projects[i].name.String() );
				strcpy( szProject, m_Projects[i].name.String() );
			}
		}
	}
	if ( bestLen == 0 )
	{
		VPCError( "Could not resolve '%s' to any known projects", pScriptNameVCProj );
	}

	// skip past known project
	char szBuffer[MAX_PATH];
	V_StripExtension( pScriptNameVCProj + strlen( szProject ), szBuffer, sizeof( szBuffer ) );

	// each token is seperated by '_'
	int numTokens = 0;
	char *pToken = szBuffer;
	char *pStart = pToken;
	char szTokens[2][MAX_PATH];
	while ( numTokens < 2 )
	{
		if ( pStart[0] == '_' )
		{
			pStart++;
			pToken = strchr( pStart, '_' );
			if ( !pToken )
			{
				strcpy( szTokens[numTokens++], pStart );
				break;
			}
			else
			{
				strncpy( szTokens[numTokens], pStart, pToken-pStart );
				szTokens[numTokens][pToken-pStart] = '\0';
				numTokens++;
				pStart = pToken;
			}
		}
		else
		{
			break;
		}
	}

	// re-build a commandline
	int localArgc = 0;
	char *localArgv[16];
	char argBuffers[16][MAX_PATH];
	for ( int i=0; i<V_ARRAYSIZE( localArgv ); i++ )
	{
		localArgv[i] = argBuffers[i];
	}
	strcpy( localArgv[localArgc++], "vpc.exe" );
	sprintf( localArgv[localArgc++], "+%s", szProject );
	for ( int i=0; i<numTokens; i++ )
	{
		sprintf( localArgv[localArgc++], "/%s", szTokens[i] );
	}

	ParseBuildOptions( localArgc, localArgv );
}

//-----------------------------------------------------------------------------
// This sets up various defines that are funneled into the .vpc script and the #defines in the engine.
//
// VPC makes a distinction between defines and macros (defines are just binary on/off things that are used like this:
//      $File "blah.cpp" [$somedefine]
//
// macros are used for substitutions like this:
//		$File "blah.$SOMEMACRO"
//-----------------------------------------------------------------------------
void CVPC::SetMacrosAndConditionals()
{
	// Find the target platform.
	conditional_t *pPlatformConditional = NULL;
	for ( int i = 0; i < m_Conditionals.Count(); i++ )
	{
		if ( m_Conditionals[i].type == CONDITIONAL_PLATFORM && m_Conditionals[i].m_bDefined )
		{
			pPlatformConditional = &m_Conditionals[i];
			break;
		}
	}

	// Only one platform is allowed to be defined.
	for ( int i = 0; i < m_Conditionals.Count(); i++ )
	{
		if ( &m_Conditionals[i] != pPlatformConditional && m_Conditionals[i].type == CONDITIONAL_PLATFORM && m_Conditionals[i].m_bDefined )
		{
			// no no no, the user is not allowed to build multiple platforms simultaneously
			// this prior feature really confused/crapped up the code, so absolutely not supporting that
			VPCWarning( "Detected multiple target platforms...Disabling '%s'", m_Conditionals[i].name.String() );
			m_Conditionals[i].m_bDefined = false;
		}
	}

	if ( !pPlatformConditional )
	{
		// no user specified platform defined, defaults to primary vpc.exe built platform
#if defined( WIN32 )
		pPlatformConditional = FindOrCreateConditional( "WIN32", false, CONDITIONAL_PLATFORM );
#elif defined( OSX )
		pPlatformConditional = FindOrCreateConditional( "OSX32", false, CONDITIONAL_PLATFORM );
#elif defined( LINUX )
		pPlatformConditional = FindOrCreateConditional( "LINUX32", false, CONDITIONAL_PLATFORM );
#elif defined( CYGWIN )
		pPlatformConditional = FindOrCreateConditional( "CYGWIN", false, CONDITIONAL_PLATFORM );
#else
#error "Unsupported platform."
#endif
		pPlatformConditional->m_bDefined = true;
	}

	// Cache the platform name so that we can use it without dereferencing pPlatformConditional
	CUtlString cVPCPlatform = pPlatformConditional->name;
	// pPlatformConditional is a pointer into a container and it may be
	// invalidated by future changes. NULLing the pointer is the only sure
	// way to prevent programmers from using the invalid pointer.
	pPlatformConditional = NULL;
	VPCStatus( true, "Target Platform: %s", cVPCPlatform.String() );

	// src_main doesn't want this #define because it conflicts with Python's SDK.
	// It really should be called something else that won't conflict with the rest of the world.
	bool bIncludePlatformDefineInProjects = false;
#ifdef STEAM
	bIncludePlatformDefineInProjects = true;
#endif

	SetMacro( "PLATFORM", cVPCPlatform.String(), bIncludePlatformDefineInProjects );

	// DO NOT INTEGRATE OR TAKE THIS - THIS IS TEMP PORTING GLUE.
	// This define will not exist in the Source2 branch.
	SetMacro( "SOURCE1", "1", true );

	// create reserved $QUOTE - used for embedding quotes, or use msdev's &quot
	SetMacro( "QUOTE", "\"", false );

	if ( !V_stricmp( cVPCPlatform.String(), "WIN32" ) || !V_stricmp( cVPCPlatform.String(), "WIN64" ) || !V_stricmp( cVPCPlatform.String(), "X360" ) )
	{
		// VS2010 is strictly win32/xbox360
		switch ( m_eVSVersion )
		{
		case k_EVSVersion_2015:
			m_ExtraOptionsCRCString += "VS2015";
			SetConditional( "VS2015", true );

			// temporarily allow VS2013 conditionals also as there are many. Will fix.
			SetConditional( "VS2013", true );

			m_bUseVS2010FileFormat = true;
			break;

		case k_EVSVersion_2013:
			m_ExtraOptionsCRCString += "VS2013";
			SetConditional( "VS2013", true );
			m_bUseVS2010FileFormat = true;
			break;

		case k_EVSVersion_2012:
			m_ExtraOptionsCRCString += "VS2012";
			SetConditional( "VS2012", true );
			m_bUseVS2010FileFormat = true;
			break;

		case k_EVSVersion_2010:
			m_ExtraOptionsCRCString += "VS2010";
			SetConditional( "VS2010", true );
			m_bUseVS2010FileFormat = true;
			break;

		case k_EVSVersion_2008:
			m_ExtraOptionsCRCString += "VS2008";
			SetConditional( "VS2005", true );			// use 2005 defines
			m_bUseVS2010FileFormat = false;
			break;

		default:
			m_ExtraOptionsCRCString += "VS2005";
			SetConditional( "VS2005", true );
			m_bUseVS2010FileFormat = false;
			break;
		}
	}

	// create and define various other platform related helper conditionals andmacros
	if ( V_stricmp( cVPCPlatform.String(), "WIN32" ) == 0 || V_stricmp( cVPCPlatform.String(), "WIN64" ) == 0 )
	{
		if ( V_stricmp( cVPCPlatform.String(), "WIN32" ) == 0 )
		{
			SetMacro( "PLATSUBDIR", "\\win32", false );
		}
		else
		{
			SetMacro( "PLATSUBDIR", "\\win64", false );
		}
		SetConditional( "WINDOWS" );

		SetMacro( "_DLL_EXT", ".dll", true );
		SetMacro( "_IMPLIB_EXT", ".lib", false );

		SetMacro( "_IMPLIB_PREFIX", "", false );
		SetMacro( "_IMPLIB_DLL_PREFIX", "", false );

		SetMacro( "_STATICLIB_PREFIX", "", false );
		SetMacro( "_STATICLIB_EXT", ".lib", false );

		SetMacro( "_EXE_EXT", ".exe", false );

		SetMacro( "_EXTERNAL_DLL_EXT", ".dll", true );
		SetMacro( "_EXTERNAL_IMPLIB_EXT", ".lib", false );
		SetMacro( "_EXTERNAL_STATICLIB_EXT", ".lib", false );

	}
	else if ( V_stricmp( cVPCPlatform.String(), "X360" ) == 0 )
	{
		SetMacro( "PLATSUBDIR", "\\x360", false );

		SetMacro( "_DLL_EXT", "_360.dll", true );
		SetMacro( "_IMPLIB_EXT", "_360.lib", false );

		SetMacro( "_IMPLIB_PREFIX", "", false );

		SetMacro( "_IMPLIB_DLL_PREFIX", "", false );

		SetMacro( "_STATICLIB_PREFIX", "", false );
		SetMacro( "_STATICLIB_EXT", "_360.lib", false );

		SetMacro( "_EXE_EXT", ".exe", false );
	}
	else if ( V_stricmp( cVPCPlatform.String(), "PS3" ) == 0 )
	{
		SetMacro( "PLATSUBDIR", "\\ps3", false );

		SetMacro( "_DLL_EXT", "_ps3.sprx", true );
		SetMacro( "_IMPLIB_EXT", "_ps3.lib", false );

		SetMacro( "_IMPLIB_PREFIX", "", false );
		SetMacro( "_IMPLIB_DLL_PREFIX", "", false );

		SetMacro( "_STATICLIB_PREFIX", "", false );
		SetMacro( "_STATICLIB_EXT", "_ps3.lib", false );

		SetMacro( "_EXE_EXT", ".self", false );
	}
	else if ( V_stricmp( cVPCPlatform.String(), "LINUX32" ) == 0 || V_stricmp( cVPCPlatform.String(), "LINUX64" ) == 0 )
	{
		bool IsLinux32 = ( V_stricmp( cVPCPlatform.String(), "LINUX32" ) == 0 );

		SetMacro( "PLATSUBDIR", IsLinux32 ? "\\linux32" : "\\linux64", false );

		SetConditional( "LINUXALL" );
		if ( m_bDedicatedBuild )
		{
			SetConditional( "DEDICATED" );
		}
		SetConditional( "POSIX" );

		SetMacro( "LINUX", "1", true );
		SetMacro( "_LINUX", "1", true );
		SetMacro( "POSIX", "1", true );
		SetMacro( "_POSIX", "1", true );

		const char *str3264 = IsLinux32 ? "" : "64";
		const char *strSrv = m_bAppendSrvToDedicated ? "_srv" : "";
		CFmtStrN<128> strDso( "%s%s.so", strSrv, str3264 );
		CFmtStrN<128> strLib( "%s%s.a", strSrv, str3264 );

		SetMacro( "_DLL_EXT", strDso.Access(), true );
		SetMacro( "_IMPLIB_EXT", strDso.Access(), false );
		SetMacro( "_STATICLIB_EXT", strLib.Access(), false );

		// Extensions for external dependencies like libsteam_api.so (not libsteam_api_ds.so).
		// VPC_Keyword_Folder in projectscript.cpp will check for ImpLibExternal or LibExternal and
		// use these prefixes instead of _ds.so if they exist.
		SetMacro( "_EXTERNAL_DLL_EXT", ".so", true );
		SetMacro( "_EXTERNAL_IMPLIB_EXT", ".so", false );
		SetMacro( "_EXTERNAL_STATICLIB_EXT", ".a", false );

		//SetMacro( "_STATICLIB_PREFIX", "lib", false );
		SetMacro( "_STATICLIB_PREFIX", "", false );

		SetMacro( "_IMPLIB_PREFIX", "lib", false );
		SetMacro( "_IMPLIB_DLL_PREFIX", "lib", false );
		SetMacro( "_EXE_EXT", "", false );
		SetMacro( "_SYM_EXT", ".dbg", false );

		SetConditional( "GL" );
	}
	else if ( V_stricmp( cVPCPlatform.String(), "OSX32" ) == 0 || V_stricmp( cVPCPlatform.String(), "OSX64" ) == 0 )
	{
		if ( V_stricmp( cVPCPlatform.String(), "OSX32" ) == 0 )
		{
			SetMacro( "PLATSUBDIR", "\\osx32", false );
		}
		else
		{
			SetMacro( "PLATSUBDIR", "\\osx64", false );
		}

		SetConditional( "OSXALL" );
		if ( m_bDedicatedBuild )
		{
			SetConditional( "DEDICATED" );
		}
		SetConditional( "POSIX" );
		SetMacro( "_POSIX", "1", true );

		SetMacro( "_DLL_EXT", ".dylib", true );
		SetMacro( "_IMPLIB_EXT", ".dylib", false );

		SetMacro( "_IMPLIB_PREFIX", "lib", false );
		SetMacro( "_IMPLIB_DLL_PREFIX", "lib", false );

		//SetMacro( "_STATICLIB_PREFIX", "lib", false );
		SetMacro( "_STATICLIB_PREFIX", "", false );
		SetMacro( "_STATICLIB_EXT", ".a", false );

		SetMacro( "_EXE_EXT", "", false );
		SetMacro( "_SYM_EXT", ".dSYM", false );

		SetMacro( "_EXTERNAL_DLL_EXT", ".dylib", true );
		SetMacro( "_EXTERNAL_IMPLIB_EXT", ".dylib", false );
		SetMacro( "_EXTERNAL_STATICLIB_EXT", ".a", false );

		// Mac defaults to GL on
		SetConditional( "GL" );
	}
	else if ( V_stricmp( cVPCPlatform.String(), "IOS" ) == 0 )
	{
		SetConditional( "OSXALL" );
		if ( m_bDedicatedBuild )
		{
			SetConditional( "DEDICATED" );
		}
		SetConditional( "POSIX" );
		SetMacro( "_POSIX", "1", true );

		SetConditional( "IOS" );
		SetMacro( "_IOS", "1", true );
		SetMacro( "IOS", "1", true );

		SetMacro( "_DLL_EXT", "_ios.dylib", true );
		SetMacro( "_IMPLIB_EXT", "_ios.dylib", false );

		SetMacro( "_IMPLIB_PREFIX", "lib", false );
		SetMacro( "_IMPLIB_DLL_PREFIX", "lib", false );

		//SetMacro( "_STATICLIB_PREFIX", "lib", false );
		SetMacro( "_STATICLIB_PREFIX", "", false );
		SetMacro( "_STATICLIB_EXT", "_ios.a", false );

		SetMacro( "_EXE_EXT", "", false );

		SetMacro( "_EXTERNAL_DLL_EXT", "_ios.dylib", true );
		SetMacro( "_EXTERNAL_IMPLIB_EXT", "_ios.dylib", false );
		SetMacro( "_EXTERNAL_STATICLIB_EXT", "_ios.a", false );
	}
	else if ( V_stricmp( cVPCPlatform.String(), "ANDROID" ) == 0 )
	{
		SetConditional( "LINUXALL" );
		if ( m_bDedicatedBuild )
		{
			SetConditional( "DEDICATED" );
		}
		SetConditional( "POSIX" );
		SetConditional( "ANDROID" );

		SetMacro( "LINUX", "1", true );
		SetMacro( "_LINUX", "1", true );
		SetMacro( "POSIX", "1", true );
		SetMacro( "_POSIX", "1", true );
		SetMacro( "ANDROID", "1", true );
		SetMacro( "_ANDROID", "1", true );

		SetMacro( "_DLL_EXT", "_an.so", true );
		SetMacro( "_IMPLIB_EXT", "_an.so", false );

		SetMacro( "_IMPLIB_PREFIX", "lib", false );
		SetMacro( "_IMPLIB_DLL_PREFIX", "lib", false );

		SetMacro( "_STATICLIB_PREFIX", "lib", false );
		SetMacro( "_STATICLIB_EXT", "_an.a", false );

		SetMacro( "_EXE_EXT", "", false );

		SetMacro( "_EXTERNAL_DLL_EXT", "_an.so", true );
		SetMacro( "_EXTERNAL_IMPLIB_EXT", "_an.so", false );
		SetMacro( "_EXTERNAL_STATICLIB_EXT", "_an.a", false );

		// and is a cross-compiled target
		SetConditional( "CROSS_COMPILED" );
		SetMacro( "CROSS_COMPILED", "1", true );
		SetMacro( "_CROSS_COMPILED", "1", true );

		SetConditional( "GL" );
	}
	else if ( V_stricmp( cVPCPlatform.String(), "CYGWIN" ) == 0 )
	{
		SetMacro( "PLATSUBDIR", "\\cygwin", false );

		SetConditional( "CYGWIN" );
		SetConditional( "CYGWIN_WINDOWS_TARGET" );
		SetConditional( "DEDICATED" );
		SetConditional( "POSIX" );

		SetMacro( "CYGWIN", "1", true );
		SetMacro( "_CYGWIN", "1", true );
		SetMacro( "CYGWIN_WINDOWS_TARGET", "1", true );
		SetMacro( "_CYGWIN_WINDOWS_TARGET", "1", true );
		SetMacro( "POSIX", "1", true );
		SetMacro( "_POSIX", "1", true );

		SetMacro( "_DLL_EXT", ".dll", true );
		SetMacro( "_IMPLIB_EXT", ".dll.a", false );

		SetMacro( "_IMPLIB_DLL_PREFIX", "", false );
		SetMacro( "_IMPLIB_PREFIX", "lib", false );

		//SetMacro( "_STATICLIB_PREFIX", "lib", false );
		SetMacro( "_STATICLIB_PREFIX", "", false );
		SetMacro( "_STATICLIB_EXT", ".a", false );

		SetMacro( "_EXE_EXT", ".exe", false );
	}
	
	// DO NOT INTEGRATE OR TAKE THIS - THIS IS TEMP PORTING GLUE.
	{
		// CERT has been decided to be a platform permutation of RETAIL.
		// The DOTA S1 scripts are not in a clean enough condition to place this logic there.
		// The S2 scripts have it there along with similar common concepts.
		conditional_t *pRetailConditional = FindOrCreateConditional( "RETAIL", false, CONDITIONAL_CUSTOM );
		if ( pRetailConditional && pRetailConditional->m_bDefined && ( !V_stricmp( cVPCPlatform.String(), "X360" ) || !V_stricmp( cVPCPlatform.String(), "PS3" ) ) )
		{
			// CERT is a restricted console RETAIL concept, with publisher dictated rules, there is no CERT process for non-console platforms.
			SetConditional( "CERT" );
		}
	}


	// Set VPCGAME macro based on target game
	if ( m_bEnableVpcGameMacro )
	{
		int nGameDefineIndex = -1;
		for ( int iOtherGameDefine = 0; iOtherGameDefine < m_Conditionals.Count(); ++ iOtherGameDefine )
		{
			if ( m_Conditionals[iOtherGameDefine].type == CONDITIONAL_GAME &&
				m_Conditionals[iOtherGameDefine].m_bDefined )
			{
				if ( nGameDefineIndex == -1 )
				{
					nGameDefineIndex = iOtherGameDefine;
				}
				else
				{
					// uh-oh, multiple games defined for target build
					// can't set VPCGAME accurately
					nGameDefineIndex = -2;
				}
			}
		}

		SetMacro( "VPCGAME", ( nGameDefineIndex >= 0 ) ? m_Conditionals[nGameDefineIndex].name.Get() : "valve", true );
		SetMacro( "VPCGAMECAPS", ( nGameDefineIndex >= 0 ) ? m_Conditionals[nGameDefineIndex].upperCaseName.Get() : "VALVE", true );

		// force this into additional CRC string
		m_ExtraOptionsCRCString += CFmtStr( "/vpcgame:%s", GetMacroValue( "VPCGAME" ) );
	}
}


//-----------------------------------------------------------------------------
// Decorate project name
//-----------------------------------------------------------------------------
void CVPC::DecorateProjectName( char *pchProjectName )
{
	macro_t *pMacro = g_pVPC->FindOrCreateMacro( "PLATFORM", false, NULL );
	if ( pMacro )
	{
		char szPlatform[MAX_PATH];
		sprintf( szPlatform, " (%s)", pMacro->value.String() );
		strcat( pchProjectName, szPlatform );
	}

	const char *pchDecorate = g_pVPC->GetDecorateString();
	if ( pchDecorate && V_strlen( pchDecorate ) > 0 )
	{
		strcat( pchProjectName, pchDecorate );
	}
}


//-----------------------------------------------------------------------------
// Checks for command line /params ( +/- used for projects, so ICommandLine() not suitable)
//-----------------------------------------------------------------------------
bool CVPC::HasCommandLineParameter( const char *pParamName )
{
	for ( int i=1; i < m_nArgc; i++ )
	{
		if ( V_stricmp( m_ppArgv[i], pParamName ) == 0 )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::HasP4SLNCommand()
{
	return HasCommandLineParameter( "/p4sln" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::HandleP4SLN( IBaseSolutionGenerator *pSolutionGenerator )
{
#ifdef WIN32
	// If they want to generate a solution based on a Perforce changelist, adjust m_targetProjects and set it up like /mksln had been passed in.
	if ( m_iP4Changelists.Count() == 0 )
		return false;

	if ( !pSolutionGenerator )
	{
		VPCError( "No solution generator exists for this platform." );
	}

	// Figure out where to put the solution file.
	char szFullSolutionPath[MAX_PATH];
	if ( V_IsAbsolutePath( m_P4SolutionFilename.Get() ) )
	{
		V_strncpy( szFullSolutionPath, m_P4SolutionFilename.Get(), sizeof( szFullSolutionPath ) );
	}
	else
	{
		V_ComposeFileName( g_pVPC->GetStartDirectory(), m_P4SolutionFilename.Get(), szFullSolutionPath, sizeof( szFullSolutionPath ) );
	}

	CProjectDependencyGraph dependencyGraph;
	GenerateSolutionForPerforceChangelist( dependencyGraph, m_iP4Changelists, pSolutionGenerator, szFullSolutionPath );
	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::GetProjectDependencies( CUtlVector<CDependency_Project *> &referencedProjects )
{
	bool bInSlnPass = m_bInMkSlnPass;

	m_bInMkSlnPass = true;

	// Find out what depends on what.
	if ( !m_dependencyGraph.HasGeneratedDependencies() )
	{
		m_dependencyGraph.BuildProjectDependencies( 0, m_pPhase1Projects );
	}

	// GenerateBuildSet basically generates what we want, except it uses projectIndex_t's, meaning that
	// we don't know what subset of games we should use until we've called VPC_IterateTargetProjects.
	m_dependencyGraph.TranslateProjectIndicesToDependencyProjects( m_TargetProjects, referencedProjects );

	m_bInMkSlnPass = bInSlnPass;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::HandleMKSLN( IBaseSolutionGenerator *pSolutionGenerator )
{
	if ( m_MKSolutionFilename.IsEmpty() )
	{
		return;
	}

	m_bInMkSlnPass = true;

	if ( !pSolutionGenerator )
	{
		VPCError( "No solution generator exists for this platform." );
	}

	// Find out what depends on what.
	if ( !m_dependencyGraph.HasGeneratedDependencies() )
	{
		m_dependencyGraph.BuildProjectDependencies( 0 );
	}

	// GenerateBuildSet basically generates what we want, except it uses projectIndex_t's, meaning that
	// we don't know what subset of games we should use until we've called VPC_IterateTargetProjects.
	CUtlVector<CDependency_Project*> referencedProjects;
	m_dependencyGraph.TranslateProjectIndicesToDependencyProjects( m_TargetProjects, referencedProjects );

	// Generate a solution file.
	char szFullSolutionPath[MAX_PATH];
	if ( V_IsAbsolutePath( m_MKSolutionFilename.Get() ) )
	{
		V_strncpy( szFullSolutionPath, m_MKSolutionFilename.Get(), sizeof( szFullSolutionPath ) );
	}
	else
	{
		V_ComposeFileName( g_pVPC->GetStartDirectory(), m_MKSolutionFilename.Get(), szFullSolutionPath, sizeof( szFullSolutionPath ) );
	}

	if ( m_pPhase1Projects != NULL )
	{
		referencedProjects.AddVectorToTail( *m_pPhase1Projects );
	}

	pSolutionGenerator->GenerateSolutionFile( szFullSolutionPath, referencedProjects );

	m_bInMkSlnPass = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::SetupGenerators()
{
	extern IBaseSolutionGenerator	*GetSolutionGenerator_Win32();
	extern IBaseProjectGenerator	*GetWin32ProjectGenerator();
	extern IBaseProjectGenerator	*GetWin32ProjectGenerator_2010();
	extern IBaseProjectGenerator	*GetPS3ProjectGenerator();
	extern IBaseProjectGenerator	*GetXbox360ProjectGenerator();
	extern IBaseProjectGenerator	*GetXbox360ProjectGenerator_2010();
	extern IBaseProjectGenerator	*GetMakefileProjectGenerator();
	extern IBaseSolutionGenerator	*GetMakefileSolutionGenerator();
	extern IBaseProjectGenerator	*GetXcodeProjectGenerator();
	extern IBaseSolutionGenerator	*GetXcodeSolutionGenerator();

	bool bIsLinux = IsPlatformDefined( "LINUX32" ) || IsPlatformDefined( "LINUX64" );
	bool bIsOSX = IsPlatformDefined( "OSX32" ) || IsPlatformDefined( "OSX64" );

#if defined( WIN32 )
	// Under Windows we have the ability to generate makefiles so if they specified a linux config,
	// or if they're building the (non-SRCDS) dedicated server, then use the makefile generator
	conditional_t *pConditional = FindOrCreateConditional( "DEDICATED", false, CONDITIONAL_CUSTOM );

	bool bUseMakefile = bIsLinux || ( pConditional && pConditional->m_bDefined );
	bool bUseXcode = bIsOSX;

	if ( bUseMakefile )
	{
		Log_Msg( LOG_VPC, "\n** Detected Linux platform. Using Makefile generator.\n" );
	}

	if ( bUseMakefile )
	{
		m_pProjectGenerator = GetMakefileProjectGenerator();
		m_pSolutionGenerator = GetMakefileSolutionGenerator();
	}
	else if ( bUseXcode )
	{
		m_pProjectGenerator = GetXcodeProjectGenerator();
		m_pSolutionGenerator = GetXcodeSolutionGenerator();
		m_bForceIterate = true;
	}
	else
	{
		if ( IsPlatformDefined( "PS3" ) )
		{
			m_pProjectGenerator = GetPS3ProjectGenerator();
			m_pSolutionGenerator = GetSolutionGenerator_Win32();
		}
		else if ( IsPlatformDefined( "X360" ) )
		{
			if ( m_bUseVS2010FileFormat )
			{
				Log_Msg( LOG_VPC, Color( 0, 255, 255, 255 ), "Generating for Visual Studio 2010.\n" );
				m_pProjectGenerator = GetXbox360ProjectGenerator_2010();
			}
			else
			{
				m_pProjectGenerator = GetXbox360ProjectGenerator();
			}
			m_pSolutionGenerator = GetSolutionGenerator_Win32();
		}
		else
		{
			// spew what we are generating
			const char *pchLogLine = "Generating for Visual Studio 2005.\n";
			if ( m_eVSVersion == k_EVSVersion_2015 )
				pchLogLine = "Generating for Visual Studio 2015.\n";
			else if ( m_eVSVersion == k_EVSVersion_2013 )
				pchLogLine = "Generating for Visual Studio 2013.\n";
			else if ( m_eVSVersion == k_EVSVersion_2012 )
				pchLogLine = "Generating for Visual Studio 2012.\n";
			else if ( m_eVSVersion == k_EVSVersion_2010 )
				pchLogLine = "Generating for Visual Studio 2010.\n";

			Log_Msg( LOG_VPC, Color( 0, 255, 255, 255 ), pchLogLine );

			// pick a project generator
			if ( m_bUseVS2010FileFormat )
				m_pProjectGenerator = GetWin32ProjectGenerator_2010();
			else
				m_pProjectGenerator = GetWin32ProjectGenerator();

			m_pSolutionGenerator = GetSolutionGenerator_Win32();
		}
	}
#else
	if ( bIsLinux )
	{
	// Linux always uses the makefile project generator.
	m_pProjectGenerator = GetMakefileProjectGenerator();
		m_pSolutionGenerator = GetMakefileSolutionGenerator();
	}
	if ( bIsOSX )
	{
		m_pProjectGenerator = GetXcodeProjectGenerator();
		m_pSolutionGenerator = GetXcodeSolutionGenerator();
	}
#endif
}


//-----------------------------------------------------------------------------
// Since Steam's VPC builds tier0 and vstdlib directly in, Steam uses vpc.exe as the CRC checker.
// Source uses vpccrccheck.exe to do this.
//-----------------------------------------------------------------------------
void CVPC::InProcessCRCCheck()
{
	for ( int i = 1; i<m_nArgc; i++ )
	{
		if ( !V_stricmp( m_ppArgv[i], "-crc" ) || !V_stricmp( m_ppArgv[i], "-crc2" ) )
		{
			// caller wants the crc check only
			int ret = VPC_CommandLineCRCChecks( m_nArgc, m_ppArgv );
			exit( ret );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CVPC::BuildTempGroupScript( const char *pVPCScriptName )
{
	char projectName[MAX_PATH];
	V_StripExtension( pVPCScriptName, projectName, sizeof( projectName ) );

	char szCurrentDirectory[MAX_PATH];
	V_GetCurrentDirectory( szCurrentDirectory, sizeof( szCurrentDirectory ) );

	// caller is specifying an explicit VPC, i.e. not a project from the default group
	// create a temporary group file that mimics a VGC, that points to the current dir's VPC

	// Generate a really crappy temp filename
	uint32 tmpHash = rand();
	for ( const char *c = pVPCScriptName ; *c ; ++c )
		tmpHash = tmpHash * 257 + *c;
	uint32 tmpTime = time( NULL );

	char tempGroupScriptFilename[MAX_PATH];
	V_ComposeFileName( szCurrentDirectory, CFmtStr( "%08x%08xvgc.tmp", tmpHash, tmpTime ), tempGroupScriptFilename, sizeof( tempGroupScriptFilename ) );
	m_TempGroupScriptFilename = tempGroupScriptFilename;

	// build the temp group script
	FILE *fp = fopen( m_TempGroupScriptFilename.Get(), "w+t" );
	if ( !fp )
	{
		VPCError( "Could not open temp file '%s'. Tell a Programmer.\n", m_TempGroupScriptFilename.Get() );
	}

	char vpcScriptFilename[MAX_PATH];
	V_ComposeFileName( szCurrentDirectory, pVPCScriptName, vpcScriptFilename, sizeof( vpcScriptFilename ) );

	// the actual vpc must be relative to the source path
	const char *pVPCFilename = StringAfterPrefix( vpcScriptFilename, m_SourcePath.Get() );
	if ( !pVPCFilename )
	{
		VPCError( "Script %s is not in source path %s\n", vpcScriptFilename, m_SourcePath.Get() );
	}

	if ( pVPCFilename[0] == '\\' )
	{
		pVPCFilename++;
	}

	fprintf( fp, "$Project \"%s\"\n", projectName );
	fprintf( fp, "{\n" );
	fprintf( fp, "\"%s\"\n", pVPCFilename );
	fprintf( fp, "}\n" );
	fclose( fp );

	// fake a build command
	char buildCommand[MAX_PATH];
	V_snprintf( buildCommand, sizeof( buildCommand ), "+%s", projectName );
	int index = m_BuildCommands.AddToTail();
	m_BuildCommands[index] = buildCommand;

	return m_TempGroupScriptFilename.Get();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CVPC::ProcessCommandLine()
{
	SetupDefaultConditionals();

	DetermineSourcePath();

	// possible extensions determine operation mode beyond expected normal user case
	bool bScriptIsVGC = false;
	bool bScriptIsVPC = false;
	bool bScriptIsVCProj = false;
	bool bHasBuildCommand = false;
	const char *pScriptName = NULL;
	const char *pScriptNameVCProj = NULL;

	for ( int i = 1; i < m_nArgc; i++ )
	{
		const char *pArg = m_ppArgv[i];
		if ( V_stristr( pArg, ".vgc" ) )
		{
			// caller explicitly providing group
			pScriptName = pArg;
			bScriptIsVGC = true;
			bHasBuildCommand = true;
			break;
		}
		else if ( V_stristr( pArg, ".vpc" ) )
		{
			// caller is using a local vpc, i.e. one that is not hooked into the groups
			pScriptName = pArg;
			bScriptIsVPC = true;
			bHasBuildCommand = true;
			break;
		}
		else
		{
			if ( V_stristr( pArg, ".vcproj" ) || V_stristr( pArg, ".vcxproj" ) )
			{
				// caller wants to re-gen the vcproj, this is commonly used by MSDEV to re-gen
				pScriptNameVCProj = pArg;
				bScriptIsVCProj = true;
				bHasBuildCommand = true;
				break;
			}
		}
	}

	for ( int i = 1; i < m_nArgc; i++ )
	{
		if ( m_ppArgv[i][0] == '-' || m_ppArgv[i][0] == '+' || m_ppArgv[i][0] == '*' || m_ppArgv[i][0] == '@' )
		{
			bHasBuildCommand = true;
			break;
		}
	}

	if ( bScriptIsVPC )
	{
		pScriptName = BuildTempGroupScript( pScriptName );
		bScriptIsVPC = false;
		bScriptIsVGC = true;
	}

	if ( !bScriptIsVGC )
	{
		// no script, use default group
		pScriptName = "vpc_scripts\\default.vgc";
		bScriptIsVGC = true;
	}

	// set the current directory, it is to be expected src, i.e. .\vpc_scripts\..
	SetDefaultSourcePath();

	char szCurrentDirectory[MAX_PATH];
	V_GetCurrentDirectory( szCurrentDirectory, sizeof( szCurrentDirectory ) );
	m_StartDirectory = szCurrentDirectory;

	// parse and build tables from group script that options will reference
	if ( bScriptIsVGC )
	{
		VPC_ParseGroupScript( pScriptName );
	}

	if ( bScriptIsVCProj )
	{
		// this is commonly used as an extern tool in MSDEV to re-vpc in place
		// caller is msdev providing the vcproj name, solve to determine which project and generate
		FindProjectFromVCPROJ( pScriptNameVCProj );
	}
	else
	{
		ParseBuildOptions( m_nArgc, m_ppArgv );
	}

	// set macros and conditionals derived from command-line options
	SetMacrosAndConditionals();

	// generate a CRC string derived from command-line options
	GenerateOptionsCRCString();

	SetupGenerators();

	// filter user's build commands
	// generate list of build targets
	CProjectDependencyGraph dependencyGraph;
	GenerateBuildSet( dependencyGraph );

	if ( !bHasBuildCommand && !HasP4SLNCommand() )
	{
		// spew usage
		m_bUsageOnly = true;
	}

	if ( m_bUsageOnly )
	{
		// spew only
		SpewUsage();
		return 0;
	}

#ifdef WIN32
	if ( HandleP4SLN( m_pSolutionGenerator ) )
	{
		return 0;
	}
#endif

	// iterate and build target projects
	if ( !BuildTargetProjects() )
	{
		// build failure
		return 0;
	}

	// now that we have valid project files, can generate solution
	HandleMKSLN( m_pSolutionGenerator );

	return 0;
}

//-----------------------------------------------------------------------------
//	main
//
//-----------------------------------------------------------------------------

// VPC is a DLL in Source.
#if defined(STANDALONE_VPC) || defined(OSX) || defined(LINUX)
int main( int argc, char **argv )
#else
int vpcmain( int argc, char **argv )
#endif
{
	bool bWindowsTwoPhase = false;

	for ( int i = 0; i < argc; i++ )
	{
		if ( V_stricmp( "/windows", argv[i] ) == 0 )
		{
			bWindowsTwoPhase = true;
			break;
		}
	}

	int nRetVal;
	g_pVPC = new CVPC();

	if ( bWindowsTwoPhase )
	{
		//
		// We're going to do some grotesque hackery by manipulating
		// the command line arguments.
		//
		// First we're going to generate a VPC object to generate all the
		// 32-bit .vcxproj files but not write a solution file.  Then
		// we'll run it for the 64-bit .vcxproj files, then write a solution
		// file containing both (if a solution file was requested).
		//
		// To accomplish this, subtract the /windows parameter (and /MKSLN if present), run, extract
		// the list of generated project files, add a /WIN64 (and put /MKSLN back if necessary)
		// and pass in the generated project file list.
		//
		bool bBuildSolution = false;
		char szSolutionFile[_MAX_PATH];
		nRetVal = -1;

		char *newargs[128] = { NULL };
		int cParms = 0;

		//
		// Do the 32-bit VPC, capturing the list of project files produced.
		//
		Assert( argc <= V_ARRAYSIZE( newargs ) );
		for ( int i = 0; i < argc; i++ )
		{
			char *pszArg = argv[i];

			if ( !V_stricmp( pszArg, "/windows" ) || !V_stricmp( pszArg, "/dp" ) )
			{
				// Skip
			}
			else if ( !V_stricmp( pszArg, "/mksln" ) && ( i + 1 < argc ) )
			{
				// If the next parameter is a standard + or - or / or * parameter, then we take that to be the name of the solution file.
				// So vpc /mksln +engine would generate engine.sln.
				if ( argv[i+1][0] == '+' || argv[i+1][0] == '-' || argv[i+1][0] == '/' || argv[i+1][0] == '*' || argv[i+1][0] == '@' )
				{
					V_strncpy( szSolutionFile, &argv[i + 1][1], sizeof(szSolutionFile) );
				}
				else
				{
					V_strncpy( szSolutionFile, argv[i + 1], sizeof(szSolutionFile) );
					i++;
				}
				bBuildSolution = true;

			}
			else
			{
				newargs[ cParms++ ] = pszArg;
			}
		}

		//
		// Add a /dp parameter to decorate the 32-bit project names with (win32). This
		// makes it easier to differentiate between projects in the solution explorer.
		//
		newargs[ cParms++ ] = "/dp";

		//
		// Add a define for PHASE1 for any VPC files that need to be aware of that sort
		// of thing.
		//
		newargs[ cParms++ ] = "/define:PHASE1";

		if ( !g_pVPC->Init( cParms, newargs ) )
		{
			return 0;
		}

		nRetVal = g_pVPC->ProcessCommandLine();

		if ( nRetVal != 0 )
		{
			return nRetVal;
		}

		//
		// Grab a list of all the project files that got generated by this pass,
		// we will stuff these into the solution file later.  If we aren't
		// building a solution, then don't bother.
		//
		CUtlVector<CDependency_Project*> referencedProjects;
		if ( bBuildSolution )
		{
			g_pVPC->GetProjectDependencies( referencedProjects );
		}

		//
		// Now reconstruct the command line with the 64-bit flag and the solution file added
		// back in.
		//
		// Decrement cParms to eat the /dp and /define we added before.
		//
		cParms--;
		cParms--;
		newargs[ cParms++ ] = "/win64";

		//
		// Add a define for PHASE2 for any VPC files that need to be aware of that sort
		// of thing.  Example: Projects that generate headers-only and are not bitness
		// specific.
		//
		newargs[ cParms++ ] = "/define:PHASE2";

		if ( bBuildSolution )
		{
			newargs[ cParms++ ] = "/mksln";
			newargs[ cParms++ ] = szSolutionFile;
		}

		g_pVPC->Shutdown();
		delete g_pVPC;
	g_pVPC = new CVPC();

		if ( !g_pVPC->Init( cParms, newargs ) )
		{
			return 0;
		}

		if ( bBuildSolution )
		{
			g_pVPC->SetPhase1Projects( &referencedProjects );
		}

		nRetVal = g_pVPC->ProcessCommandLine();
	}
	else
	{
	if ( !g_pVPC->Init( argc, argv ) )
	{
		return 0;
	}

		nRetVal = g_pVPC->ProcessCommandLine();
	}

	g_pVPC->Shutdown();

	return nRetVal;
}

// VPC is a DLL in Source.
#if !(defined(STANDALONE_VPC) || defined(OSX) || defined(LINUX))
#include "ilaunchabledll.h"

// VPC is launched by vpc.exe, which is a copy of binlaunch.exe.
// All binlaunch does is setup the path to game\bin and load an ILaunchableDLL
// interface out of a DLL with the same name as the exe.
class CVPCLaunchableDLL : public ILaunchableDLL
{
public:
	// All vpc.exe does is load the vpc DLL and run this.
	virtual int	main( int argc, char **argv )
	{
		return vpcmain( argc, argv );
	}
};

EXPOSE_SINGLE_INTERFACE( CVPCLaunchableDLL, ILaunchableDLL, LAUNCHABLE_DLL_INTERFACE_VERSION );
#endif


