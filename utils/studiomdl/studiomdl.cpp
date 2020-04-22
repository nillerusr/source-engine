//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


//
// studiomdl.c: generates a studio .mdl file from a .qc script
// models/<scriptname>.mdl.
//


#pragma warning( disable : 4244 )
#pragma warning( disable : 4237 )
#pragma warning( disable : 4305 )

#include <windows.h>
#undef GetCurrentDirectory

#include <Shlwapi.h> // PathCanonicalize
#pragma comment( lib, "shlwapi" )

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <direct.h>
#include "istudiorender.h"
#include "filesystem_tools.h"
#include "tier2/fileutils.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "mathlib/mathlib.h"
#define EXTERN
#include "studio.h"
#include "studiomdl.h"
#include "collisionmodel.h"
#include "optimize.h"
#include "byteswap.h"
#include "studiobyteswap.h"
#include "tier1/strtools.h"
#include "bspflags.h"
#include "tier0/icommandline.h"
#include "utldict.h"
#include "tier1/utlsortvector.h"
#include "bitvec.h"
#include "appframework/appframework.h"
#include "datamodel/idatamodel.h"
#include "materialsystem/materialsystem_config.h"
#include "vstdlib/cvar.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeboneflexdriver.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmemdlmakefile.h"
#include "movieobjects/dmevertexdata.h"
#include "movieobjects/dmecombinationoperator.h"
#include "dmserializers/idmserializers.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"
#include "mdllib/mdllib.h"
#include "perfstats.h"
#include "worldsize.h"

bool g_collapse_bones = false;
bool g_collapse_bones_aggressive = false;
bool g_quiet = false;
bool g_badCollide = false;
bool g_IHVTest = false;
bool g_bCheckLengths = false;
bool g_bPrintBones = false;
bool g_bPerf = false;
bool g_bDumpGraph = false;
bool g_bMultistageGraph = false;
bool g_verbose = false;
bool g_bCreateMakefile = false;
bool g_bHasModelName = false;
bool g_bZBrush = false;
bool g_bVerifyOnly = false;
bool g_bUseBoneInBBox = true;
bool g_bLockBoneLengths = false;
bool g_bOverridePreDefinedBones = false;
int g_minLod = 0;
int g_numAllowedRootLODs = 0;
bool g_bNoWarnings = false;
int g_maxWarnings = -1;
bool g_bX360 = false;
bool g_bBuildPreview = false;
bool g_bCenterBonesOnVerts = false;
bool g_bDumpMaterials = false;
bool g_bStripLods = false;
bool g_bMakeVsi = false;
float g_flDefaultMotionRollback = 0.3f;
int g_minSectionFrameLimit = 120;
int g_sectionFrames = 30;
bool g_bNoAnimblockStall = false;

char g_path[MAX_PATH];
Vector g_vecMinWorldspace = Vector( MIN_COORD_INTEGER, MIN_COORD_INTEGER, MIN_COORD_INTEGER );
Vector g_vecMaxWorldspace = Vector( MAX_COORD_INTEGER, MAX_COORD_INTEGER, MAX_COORD_INTEGER );
DmElementHandle_t g_hDmeBoneFlexDriverList = DMELEMENT_HANDLE_INVALID;

enum RunMode
{
	RUN_MODE_BUILD,
	RUN_MODE_STRIP_MODEL,
	RUN_MODE_STRIP_VHV
} g_eRunMode = RUN_MODE_BUILD;

bool g_bNoP4 = false;


CUtlVector< s_hitboxset > g_hitboxsets;
CUtlVector< char >	g_KeyValueText;
CUtlVector<s_flexcontrollerremap_t> g_FlexControllerRemap;
CCheckUVCmd g_StudioMdlCheckUVCmd;


//-----------------------------------------------------------------------------
// Parsed data from a .qc or .dmx file
//-----------------------------------------------------------------------------
struct IKLock_t
{
	CUtlString m_Name;
	float m_flPosWeight;
	float m_flLocalQWeight;
};

struct SequenceOption_t
{
	bool m_bSnap : 1;
	bool m_bIsDelta : 1;
	bool m_bIsWorldSpace : 1;
	bool m_bIsPost : 1;
	bool m_bIsPreDelta : 1;
	bool m_bIsAutoplay : 1;
	bool m_bIsRealTime : 1;
	bool m_bIsHidden : 1;
	float m_flFadeInTime;
	float m_flFadeOutTime;
	int m_nBlendWidth;
	CUtlVector< CUtlString > m_AutoLayers;
	CUtlVector< IKLock_t > m_IKLocks;
};

struct CmdSequence_t
{
	CUtlString m_Name;
	CUtlString m_FileName;
	SequenceOption_t m_Options;
};


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void AddBodyFlexData( s_source_t *pSource, int imodel );
void AddBodyAttachments( s_source_t *pSource );
void AddBodyFlexRules( s_source_t *pSource );

//-----------------------------------------------------------------------------
//  Stuff for writing a makefile to build models incrementally.
//-----------------------------------------------------------------------------
CUtlVector<CUtlSymbol> m_CreateMakefileDependencies;

void CreateMakefile_AddDependency( const char *pFileName )
{
	EnsureDependencyFileCheckedIn( pFileName );

	if( !g_bCreateMakefile )
	{
		return;
	}

	CUtlSymbol sym( pFileName );
	int i;
	for( i = 0; i < m_CreateMakefileDependencies.Count(); i++ )
	{
		if( m_CreateMakefileDependencies[i] == sym )
		{
			return;
		}
	}
	m_CreateMakefileDependencies.AddToTail( sym );
}

void EnsureDependencyFileCheckedIn( const char *pFileName )
{
	// Early out: if no p4
	if ( g_bNoP4 )
		return;

	char pFullPath[MAX_PATH];
	if ( !GetGlobalFilePath( pFileName, pFullPath, sizeof(pFullPath) ) )
	{
		MdlWarning( "Model dependency file '%s' is missing.\n", pFileName );
		return;
	}

	Q_FixSlashes( pFullPath );
	char bufCanonicalPath[ MAX_PATH ] = {0};
	PathCanonicalize( bufCanonicalPath, pFullPath );
	CP4AutoAddFile p4_add_dep_file( bufCanonicalPath );
}

void StudioMdl_ScriptLoadedCallback( char const *pFilenameLoaded, char const *pIncludedFromFileName, int nIncludeLineNumber )
{
	EnsureDependencyFileCheckedIn( pFilenameLoaded );
}

void CreateMakefile_OutputMakefile( void )
{
	if( !g_bHasModelName )
	{
		MdlError( "Can't write makefile since a target mdl hasn't been specified!" );
	}
	FILE *fp = fopen( "makefile.tmp", "a" );
	if( !fp )
	{
		MdlError( "can't open makefile.tmp!\n" );
	}
	char mdlname[MAX_PATH];
	V_strcpy_safe( mdlname, gamedir );
//	if( *g_pPlatformName )
//	{
//		V_strcat_safe( mdlname, "platform_" );
//		V_strcat_safe( mdlname, g_pPlatformName );
//		V_strcat_safe( mdlname, "/" );	
//	}
	V_strcat_safe( mdlname, "models/" );	
	V_strcat_safe( mdlname, outname );
	Q_StripExtension( mdlname, mdlname, sizeof( mdlname ) );
	V_strcat_safe( mdlname, ".mdl" );
	Q_FixSlashes( mdlname );

	fprintf( fp, "%s:", mdlname );
	int i;
	for( i = 0; i < m_CreateMakefileDependencies.Count(); i++ )
	{
		fprintf( fp, " %s", m_CreateMakefileDependencies[i].String() );
	}
	fprintf( fp, "\n" );
	char mkdirpath[MAX_PATH];
	V_strcpy_safe( mkdirpath, mdlname );
	Q_StripFilename( mkdirpath );
	fprintf( fp, "\tmkdir \"%s\"\n", mkdirpath );
	fprintf( fp, "\t%s -quiet %s\n\n", CommandLine()->GetParm( 0 ), fullpath );
	fclose( fp );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

static bool g_bFirstWarning = true;

void TokenError( const char *fmt, ... )
{
	static char output[1024];
	va_list		args;

	char *pFilename;
	int iLineNumber;

	if (GetTokenizerStatus( &pFilename, &iLineNumber ))
	{
		va_start( args, fmt );
		vsprintf( output, fmt, args );

		MdlError( "%s(%d): - %s", pFilename, iLineNumber, output );
	}
	else
	{
		va_start( args, fmt );
		vsprintf( output, fmt, args );
		MdlError( "%s", output );
	}
}

void MdlError( const char *fmt, ... )
{
	static char output[1024];
	static char *knownExtensions[] = {".mdl", ".ani", ".phy", ".sw.vtx", ".dx80.vtx", ".dx90.vtx", ".vvd"};
	char		fileName[MAX_PATH];
	char		baseName[MAX_PATH];
	va_list		args;

	Assert( 0 );
	if (g_quiet)
	{
		if (g_bFirstWarning)
		{
			printf("%s :\n", fullpath );
			g_bFirstWarning = false;
		}
		printf("\t");
	}

	printf("ERROR: ");
	va_start( args, fmt );
	vprintf( fmt, args );

	// delete premature files
	// unforunately, content is built without verification
	// ensuring that targets are not available, prevents check-in
	if (g_bHasModelName)
	{
		// undescriptive errors in batch processes could be anonymous
		printf("ERROR: Aborted Processing on '%s'\n", outname);

		V_strcpy_safe( fileName, gamedir );
		V_strcat_safe( fileName, "models/" );	
		V_strcat_safe( fileName, outname );
		Q_FixSlashes( fileName );
		Q_StripExtension( fileName, baseName, sizeof( baseName ) );

		for (int i=0; i<ARRAYSIZE(knownExtensions); i++)
		{
			V_strcpy_safe( fileName, baseName);
			V_strcat_safe( fileName, knownExtensions[i] );

			// really need filesystem concept here
//			g_pFileSystem->RemoveFile( fileName );
			unlink( fileName );
		}
	}

	exit( -1 );
}


void MdlWarning( const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	if (g_bNoWarnings || g_maxWarnings == 0)
		return;

	WORD old = SetConsoleTextColor( 1, 1, 0, 1 );

	if (g_quiet)
	{
		if (g_bFirstWarning)
		{
			printf("%s :\n", fullpath );
			g_bFirstWarning = false;
		}
		printf("\t");
	}

	Assert( 0 );

	printf("WARNING: ");
	va_start( args, fmt );
	vprintf( fmt, args );

	if (g_maxWarnings > 0)
		g_maxWarnings--;

	if (g_maxWarnings == 0)
	{
		if (g_quiet)
		{
			printf("\t");
		}
		printf("suppressing further warnings...\n");
	}

	RestoreConsoleTextColor( old );
}

SpewRetval_t MdlSpewOutputFunc( SpewType_t type, char const *pMsg )
{
	if ((( type == SPEW_MESSAGE ) || (type == SPEW_LOG )) && g_quiet)
	{
		// suppress
	}
	else if (type == SPEW_WARNING)
	{
		MdlWarning( "%s", pMsg );
	}
	else
	{
		return CmdLib_SpewOutputFunc( type, pMsg );
	}

	return SPEW_CONTINUE;
}


#ifndef _DEBUG

void MdlHandleCrash( const char *pMessage, bool bAssert )
{
	static LONG crashHandlerCount = 0;
	if ( InterlockedIncrement( &crashHandlerCount ) == 1 )
	{
		MdlError( "'%s' (assert: %d)\n", pMessage, bAssert );
	}

	InterlockedDecrement( &crashHandlerCount );
}


// This is called if we crash inside our crash handler. It just terminates the process immediately.
LONG __stdcall MdlSecondExceptionFilter( struct _EXCEPTION_POINTERS *ExceptionInfo )
{
	TerminateProcess( GetCurrentProcess(), 2 );
	return EXCEPTION_EXECUTE_HANDLER; // (never gets here anyway)
}


void MdlExceptionFilter( unsigned long code )
{
	// This is called if we crash inside our crash handler. It just terminates the process immediately.
	SetUnhandledExceptionFilter( MdlSecondExceptionFilter );

	//DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;

	#define ERR_RECORD( name ) { name, #name }
	struct
	{
		int code;
		char *pReason;
	} errors[] =
	{
		ERR_RECORD( EXCEPTION_ACCESS_VIOLATION ),
		ERR_RECORD( EXCEPTION_ARRAY_BOUNDS_EXCEEDED ),
		ERR_RECORD( EXCEPTION_BREAKPOINT ),
		ERR_RECORD( EXCEPTION_DATATYPE_MISALIGNMENT ),
		ERR_RECORD( EXCEPTION_FLT_DENORMAL_OPERAND ),
		ERR_RECORD( EXCEPTION_FLT_DIVIDE_BY_ZERO ),
		ERR_RECORD( EXCEPTION_FLT_INEXACT_RESULT ),
		ERR_RECORD( EXCEPTION_FLT_INVALID_OPERATION ),
		ERR_RECORD( EXCEPTION_FLT_OVERFLOW ),
		ERR_RECORD( EXCEPTION_FLT_STACK_CHECK ),
		ERR_RECORD( EXCEPTION_FLT_UNDERFLOW ),
		ERR_RECORD( EXCEPTION_ILLEGAL_INSTRUCTION ),
		ERR_RECORD( EXCEPTION_IN_PAGE_ERROR ),
		ERR_RECORD( EXCEPTION_INT_DIVIDE_BY_ZERO ),
		ERR_RECORD( EXCEPTION_INT_OVERFLOW ),
		ERR_RECORD( EXCEPTION_INVALID_DISPOSITION ),
		ERR_RECORD( EXCEPTION_NONCONTINUABLE_EXCEPTION ),
		ERR_RECORD( EXCEPTION_PRIV_INSTRUCTION ),
		ERR_RECORD( EXCEPTION_SINGLE_STEP ),
		ERR_RECORD( EXCEPTION_STACK_OVERFLOW ),
		ERR_RECORD( EXCEPTION_ACCESS_VIOLATION ),
	};

	int nErrors = sizeof( errors ) / sizeof( errors[0] );
	{
		int i;
		for ( i=0; i < nErrors; i++ )
		{
			if ( errors[i].code == code )
				MdlHandleCrash( errors[i].pReason, true );
		}

		if ( i == nErrors )
		{
			MdlHandleCrash( "Unknown reason", true );
		}
	}

	TerminateProcess( GetCurrentProcess(), 1 );
}

#endif

/*
=================
=================
*/

int k_memtotal;
void *kalloc( int num, int size )
{
	// printf( "calloc( %d, %d )\n", num, size );
	// printf( "%d ", num * size );
	int nMemSize = num * size;
	k_memtotal += nMemSize;

	// ensure memory alignment on maximum of ALIGN
	nMemSize += 511;
	void *ptr = malloc( nMemSize );
	memset( ptr, 0, nMemSize );
	ptr = (byte *)((int)((byte *)ptr + 511) & ~511);
	return ptr;
}

void kmemset( void *ptr, int value, int size )
{
	// printf( "kmemset( %x, %d, %d )\n", ptr, value, size );
	memset( ptr, value, size );
	return;
}


int verify_atoi( const char *token )
{
	if (token[0] != '-' && (token[0] < '0' || token[0] > '9'))
	{
		TokenError( "expecting number, got \"%s\"\n", token );
	}
	return atoi( token );
}

float verify_atof( const char *token )
{
	if (token[0] != '-' && token[0] != '.' && (token[0] < '0' || token[0] > '9'))
	{
		TokenError( "expecting number, got \"%s\"\n", token );
	}
	return atof( token );
}

float verify_atof_with_null( const char *token )
{
	if (strcmp( token, ".." ) == 0)
		return -1;

	if (token[0] != '-' && token[0] != '.' && (token[0] < '0' || token[0] > '9'))
	{
		TokenError( "expecting number, got \"%s\"\n", token );
	}
	return atof( token );
}

//-----------------------------------------------------------------------------
// Key value block
//-----------------------------------------------------------------------------
static void AppendKeyValueText( CUtlVector< char > *pKeyValue, const char *pString )
{
	int nLen = strlen(pString);
	int nFirst = pKeyValue->AddMultipleToTail( nLen );
	memcpy( pKeyValue->Base() + nFirst, pString, nLen );
}

int	KeyValueTextSize( CUtlVector< char > *pKeyValue )
{
	return pKeyValue->Count();
}

const char *KeyValueText( CUtlVector< char > *pKeyValue )
{
	return pKeyValue->Base();
}

void Option_KeyValues( CUtlVector< char > *pKeyValue );

//-----------------------------------------------------------------------------
// Read global input into common string
//-----------------------------------------------------------------------------

bool GetLineInput( void )
{
	while (fgets( g_szLine, sizeof( g_szLine ), g_fpInput ) != NULL) 
	{
		g_iLinecount++;
		// skip comments
		if (g_szLine[0] == '/' && g_szLine[1] == '/')
			continue;

		return true;
	}
	return false;
}


/*
=================
=================
*/




int lookupControl( char *string )
{
	if (stricmp(string,"X")==0) return STUDIO_X;
	if (stricmp(string,"Y")==0) return STUDIO_Y;
	if (stricmp(string,"Z")==0) return STUDIO_Z;
	if (stricmp(string,"XR")==0) return STUDIO_XR;
	if (stricmp(string,"YR")==0) return STUDIO_YR;
	if (stricmp(string,"ZR")==0) return STUDIO_ZR;

	if (stricmp(string,"LX")==0) return STUDIO_LX;
	if (stricmp(string,"LY")==0) return STUDIO_LY;
	if (stricmp(string,"LZ")==0) return STUDIO_LZ;
	if (stricmp(string,"LXR")==0) return STUDIO_LXR;
	if (stricmp(string,"LYR")==0) return STUDIO_LYR;
	if (stricmp(string,"LZR")==0) return STUDIO_LZR;

	if (stricmp(string,"LM")==0) return STUDIO_LINEAR;
	if (stricmp(string,"LQ")==0) return STUDIO_QUADRATIC_MOTION;

	return -1;
}



/*
=================
=================
*/

int LookupPoseParameter( char *name )
{
	int i;
	for ( i = 0; i < g_numposeparameters; i++)
	{
		if (!stricmp( name, g_pose[i].name))
		{
			return i;
		}
	}
	V_strcpy_safe( g_pose[i].name, name );
	g_numposeparameters = i + 1;

	if (g_numposeparameters > MAXSTUDIOPOSEPARAM)
	{
		TokenError( "too many pose parameters (max %d)\n", MAXSTUDIOPOSEPARAM );
	}

	return i;
}


//-----------------------------------------------------------------------------
// Stuff for writing a makefile to build models incrementally.
//-----------------------------------------------------------------------------
s_sourceanim_t *FindSourceAnim( s_source_t *pSource, const char *pAnimName )
{
	int nCount = pSource->m_Animations.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		s_sourceanim_t *pAnim = &pSource->m_Animations[i];
		if ( !Q_stricmp( pAnimName, pAnim->animationname ) )
			return pAnim;
	}
	return NULL;
}

const s_sourceanim_t *FindSourceAnim( const s_source_t *pSource, const char *pAnimName )
{
	if ( !pAnimName[0] )
		return NULL;

	int nCount = pSource->m_Animations.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const s_sourceanim_t *pAnim = &pSource->m_Animations[i];
		if ( !Q_stricmp( pAnimName, pAnim->animationname ) )
			return pAnim;
	}
	return NULL;
}

s_sourceanim_t *FindOrAddSourceAnim( s_source_t *pSource, const char *pAnimName )
{
	if ( !pAnimName[0] )
		return NULL;

	int nCount = pSource->m_Animations.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		s_sourceanim_t *pAnim = &pSource->m_Animations[i];
		if ( !Q_stricmp( pAnimName, pAnim->animationname ) )
			return pAnim;
	}

	int nIndex = pSource->m_Animations.AddToTail();
	s_sourceanim_t *pAnim = &pSource->m_Animations[nIndex];
	memset( pAnim, 0, sizeof(s_sourceanim_t) );
	Q_strncpy( pAnim->animationname, pAnimName, sizeof(pAnim->animationname) );
	return pAnim;
}


//-----------------------------------------------------------------------------
// Purpose: Handle the $boneflexdriver command
// QC: $boneflexdriver <bone name> <tx|ty|tz> <flex controller name> <min> <max>
//-----------------------------------------------------------------------------
void Cmd_BoneFlexDriver()
{
	CDisableUndoScopeGuard undoDisable;	// Turn of Dme undo

	// Find or create the DmeBoneFlexDriverList
	CDmeBoneFlexDriverList *pDmeBoneFlexDriverList = GetElement< CDmeBoneFlexDriverList >( g_hDmeBoneFlexDriverList );
	if ( !pDmeBoneFlexDriverList )
	{
		pDmeBoneFlexDriverList = CreateElement< CDmeBoneFlexDriverList >( "boneDriverFlexList", DMFILEID_INVALID );
		if ( pDmeBoneFlexDriverList )
		{
			g_hDmeBoneFlexDriverList = pDmeBoneFlexDriverList->GetHandle();
		}
	}

	if ( !pDmeBoneFlexDriverList )
	{
		MdlError( "%s: Couldn't find or create DmeBoneDriverFlexList\n", "$boneflexdriver" );
		return;
	}

	// <bone name>
	GetToken( false );
	CDmeBoneFlexDriver *pDmeBoneFlexDriver = pDmeBoneFlexDriverList->FindOrCreateBoneFlexDriver( token );
	if ( !pDmeBoneFlexDriver )
	{
		MdlError( "%s: Couldn't find or create DmeBoneFlexDriver for bone \"%s\"\n", "$boneflexdriver", token );
		return;
	}

	// <tx|ty|tz|rx|ry|rz>
	GetToken( false );
	const char *ppszComponentTypeList[] = { "tx", "ty", "tz" };
	int nBoneComponent = -1;
	for ( int i = 0; i < ARRAYSIZE( ppszComponentTypeList ); ++i )
	{
		if ( StringHasPrefix( token, ppszComponentTypeList[i] ) )
		{
			nBoneComponent = i;
			break;
		}
	}

	if ( nBoneComponent < STUDIO_BONE_FLEX_TX || nBoneComponent > STUDIO_BONE_FLEX_TZ )
	{
		TokenError( "%s: Invalid bone component, must be one of <tx|ty|tz>\n", "$boneflexdriver" );
		return;
	}

	// <flex controller name>
	GetToken( false );
	CDmeBoneFlexDriverControl *pDmeBoneFlexDriverControl = pDmeBoneFlexDriver->FindOrCreateControl( token );
	if ( !pDmeBoneFlexDriverControl )
	{
		MdlError( "%s: Couldn't find or create DmeBoneFlexDriverControl for bone \"%s\"\n", "$boneflexdriver", token );
		return;
	}

	pDmeBoneFlexDriverControl->m_nBoneComponent = nBoneComponent;

	// <min>
	GetToken( false );
	pDmeBoneFlexDriverControl->m_flMin = verify_atof( token );

	// <max>
	GetToken( false );
	pDmeBoneFlexDriverControl->m_flMax = verify_atof( token );
} 

//-----------------------------------------------------------------------------
// Purpose: Handle the $checkuv command
// QC: $checkuv [0to1] [overlap] [inverse] [gutter <res> <min>]
//-----------------------------------------------------------------------------
void Cmd_CheckUV()
{
	g_StudioMdlCheckUVCmd.ClearCheck( CCheckUVCmd::CHECK_UV_ALL_FLAGS );

	while ( TokenAvailable() && GetToken( false ) )
	{
		if ( !V_stricmp( token, "0to1" ) )
		{
			g_StudioMdlCheckUVCmd.SetCheck( CCheckUVCmd::CHECK_UV_FLAG_NORMALIZED );
		}
		else if ( !V_stricmp( token, "overlap" ) )
		{
			g_StudioMdlCheckUVCmd.SetCheck( CCheckUVCmd::CHECK_UV_FLAG_OVERLAP );
		}
		else if ( !V_stricmp( token, "inverse" ) )
		{
			g_StudioMdlCheckUVCmd.SetCheck( CCheckUVCmd::CHECK_UV_FLAG_INVERSE );
		}
		else if ( !V_stricmp( token, "gutter" ) )
		{
			g_StudioMdlCheckUVCmd.SetCheck( CCheckUVCmd::CHECK_UV_FLAG_GUTTER );
			if ( TokenAvailable() && GetToken( false ) )
			{
				if ( V_isdigit( *token ) )
				{
					const int nOptRes = V_atoi( token );
					if ( nOptRes <= 0 )
					{
						MdlError( "$checkuv: Invalid resolution, \"%s\", for gutter check specified, must be > 0\n", token );
						return;
					}

					g_StudioMdlCheckUVCmd.m_nOptGutterTexWidth = nOptRes;
					g_StudioMdlCheckUVCmd.m_nOptGutterTexHeight = nOptRes;

					if ( TokenAvailable() && GetToken( false ) )
					{
						if ( V_isdigit( *token ) )
						{
							const int nOptMin = V_atoi( token );
							if ( nOptMin <= 0 )
							{
								MdlError( "$checkuv: Invalid minimum, \"%s\", for gutter check specified, must be > 0\n", token );
								return;
							}

							g_StudioMdlCheckUVCmd.m_nOptGutterMin = nOptMin;
						}
						else
						{
							UnGetToken();
						}
					}
				}
				else
				{
					UnGetToken();
				}
			}
		}
		else
		{
			MdlError( "$checkuv: Unknown argument \"%s\", expected one of [ 0to1, overlap, inverse, gutter ]\n", token );
			return;
		}
	}

	if ( !g_StudioMdlCheckUVCmd.DoAnyCheck() )
	{
		g_StudioMdlCheckUVCmd.SetCheck( CCheckUVCmd::CHECK_UV_ALL_FLAGS );
	}
}


void Cmd_PoseParameter( )
{
	if ( g_numposeparameters >= MAXSTUDIOPOSEPARAM )
	{
		TokenError( "too many pose parameters (max %d)\n", MAXSTUDIOPOSEPARAM );
	}

	int i = LookupPoseParameter( token );

	// name
	GetToken (false);
	V_strcpy_safe( g_pose[i].name, token );

	if ( TokenAvailable() )
	{
		// min
		GetToken (false);
		g_pose[i].min = verify_atof (token);
	}

	if ( TokenAvailable() )
	{
		// max
		GetToken (false);
		g_pose[i].max = verify_atof (token);
	}

	while ( TokenAvailable() )
	{
		GetToken (false);

		if ( !Q_stricmp( token, "wrap" ) )
		{
			g_pose[i].flags |= STUDIO_LOOPING;
			g_pose[i].loop = g_pose[i].max - g_pose[i].min;
		}
		else if ( !Q_stricmp( token, "loop" ) )
		{
			g_pose[i].flags |= STUDIO_LOOPING;
			GetToken (false);
			g_pose[i].loop = verify_atof( token );
		}
	}
}


/*
=================
=================
*/

int LookupTexture( const char *pTextureName, bool bRelativePath )
{
	char pTextureNoExt[MAX_PATH];
	char pTextureBase[MAX_PATH];
	char pTextureBase2[MAX_PATH];
	Q_StripExtension( pTextureName, pTextureNoExt, sizeof(pTextureNoExt) );
	Q_FileBase( pTextureName, pTextureBase, sizeof(pTextureBase) );

	int nFlags = bRelativePath ? RELATIVE_TEXTURE_PATH_SPECIFIED : 0;
	int i;
	for ( i = 0; i < g_numtextures; i++ ) 
	{
		if ( g_texture[i].flags == nFlags )
		{
			if ( !Q_stricmp( pTextureNoExt, g_texture[i].name ) )
				return i;
			continue;
		}

		// Comparing relative vs non-relative
		if ( bRelativePath )
		{
			if ( !Q_stricmp( pTextureBase, g_texture[i].name ) )
				return i;
			continue;
		}

		// Comparing non-relative vs relative
		Q_FileBase( g_texture[i].name, pTextureBase2, sizeof(pTextureBase2) );
		if ( !Q_stricmp( pTextureNoExt, pTextureBase2 ) )
			return i;
	}

	if ( i >= MAXSTUDIOSKINS )
	{
		MdlError("Too many materials used, max %d\n", ( int )MAXSTUDIOSKINS );
	}

	Q_strncpy( g_texture[i].name, pTextureNoExt, sizeof(g_texture[i].name) );
	g_texture[i].material = -1;
	g_texture[i].flags = nFlags;
	g_numtextures++;
	return i;
}


void Cmd_RenameMaterial( void )
{
	char from[256];
	char to[256];

	GetToken( false );
	V_strcpy_safe( from, token );

	GetToken( false );
	V_strcpy_safe( to, token );

	int i;
	for (i = 0; i < g_numtextures; i++) 
	{
		if (stricmp( g_texture[i].name, from ) == 0) 
		{
			V_strcpy_safe( g_texture[i].name, to );
			return;
		}
	}
	MdlError( "unknown material \"%s\" in rename\n", from );
}


int UseTextureAsMaterial( int textureindex )
{
	if ( g_texture[textureindex].material == -1 )
	{
		if (g_bDumpMaterials)
		{
			printf("material %d %d %s\n", textureindex, g_nummaterials, g_texture[textureindex].name );
		}
		g_material[g_nummaterials] = textureindex;
		g_texture[textureindex].material = g_nummaterials++;
	}

	return g_texture[textureindex].material;
}

int MaterialToTexture( int material )
{
	int i;
	for (i = 0; i < g_numtextures; i++)
	{
		if (g_texture[i].material == material)
		{
			return i;
		}
	}
	return -1;
}

//Wrong name for the use of it.
void scale_vertex( Vector &org )
{
	org[0] = org[0] * g_currentscale;
	org[1] = org[1] * g_currentscale;
	org[2] = org[2] * g_currentscale;
}



void SetSkinValues( )
{
	int			i, j;
	int			index;

	// Check all textures to see if we have relative paths specified
	for (i = 0; i < g_numtextures; i++)
	{
		if ( g_texture[i].flags & RELATIVE_TEXTURE_PATH_SPECIFIED )
		{
			// Add an empty path to prepend if anything specifies a relative path
			cdtextures[numcdtextures] = 0;
			++numcdtextures;
			break;
		}
	}

	if ( numcdtextures == 0 )
	{
		char szName[MAX_PATH];

		// strip down till it finds "models"
		V_strcpy_safe( szName, fullpath );
		while (szName[0] != '\0' && strnicmp( "models", szName, 6 ) != 0)
		{
			strcpy( &szName[0], &szName[1] );
		}
		if (szName[0] != '\0')
		{
			Q_StripFilename( szName );
			V_strcat_safe( szName, "/" );
		}
		else
		{
//			if( *g_pPlatformName )
//			{
//				V_strcat_safe( szName, "platform_" );
//				V_strcat_safe( szName, g_pPlatformName );
//				V_strcat_safe( szName, "/" );	
//			}
			V_strcpy_safe( szName, "models/" );	
			V_strcat_safe( szName, outname );
			Q_StripExtension( szName, szName, sizeof( szName ) );
			V_strcat_safe( szName, "/" );
		}
		cdtextures[0] = strdup( szName );
		numcdtextures = 1;
	}

	for (i = 0; i < g_numtextures; i++)
	{
		char szName[256];
		Q_StripExtension( g_texture[i].name, szName, sizeof( szName ) );
		Q_strncpy( g_texture[i].name, szName, sizeof( g_texture[i].name ) );
	}

	// build texture groups
	for (i = 0; i < MAXSTUDIOSKINS; i++)
	{
		for (j = 0; j < MAXSTUDIOSKINS; j++)
		{
			g_skinref[i][j] = j;
		}
	}
	index = 0;
	for (i = 0; i < g_numtexturelayers[0]; i++)
	{
		for (j = 0; j < g_numtexturereps[0]; j++)
		{
			g_skinref[i][g_texturegroup[0][0][j]] = g_texturegroup[0][i][j];
		}
	}

	if (i != 0)
	{
		g_numskinfamilies = i;
	}
	else
	{
		g_numskinfamilies = 1;
	}
	g_numskinref = g_numtextures;

	// printf ("width: %i  height: %i\n",width, height);
	/*
	printf ("adjusted width: %i height: %i  top : %i  left: %i\n",
			pmesh->skinwidth, pmesh->skinheight, pmesh->skintop, pmesh->skinleft );
	*/
}

/*
=================
=================
*/


int LookupXNode( char *name )
{
	int i;
	for ( i = 1; i <= g_numxnodes; i++)
	{
		if (stricmp( name, g_xnodename[i] ) == 0)
		{
			return i;
		}
	}
	g_xnodename[i] = strdup( name );
	g_numxnodes = i;
	return i;
}


/*
=================
=================
*/

char	g_szFilename[1024];
FILE	*g_fpInput;
char	g_szLine[4096];
int		g_iLinecount;


void Build_Reference( s_source_t *pSource, const char *pAnimName )
{
	int		i, parent;
	Vector	angle;

	s_sourceanim_t *pReferenceAnim = FindSourceAnim( pSource, pAnimName );
	for (i = 0; i < pSource->numbones; i++)
	{
		matrix3x4_t m;
		if ( pReferenceAnim )
		{
			AngleMatrix( pReferenceAnim->rawanim[0][i].rot, m );
			m[0][3] = pReferenceAnim->rawanim[0][i].pos[0];
			m[1][3] = pReferenceAnim->rawanim[0][i].pos[1];
			m[2][3] = pReferenceAnim->rawanim[0][i].pos[2];
		}
		else
		{
			SetIdentityMatrix( m );
		}

		parent = pSource->localBone[i].parent;
		if (parent == -1) 
		{
			// scale the done pos.
			// calc rotational matrices
			MatrixCopy( m, pSource->boneToPose[i] );
		}
		else 
		{
			// calc compound rotational matrices
			// FIXME : Hey, it's orthogical so inv(A) == transpose(A)
			Assert( parent < i );
			ConcatTransforms( pSource->boneToPose[parent], m, pSource->boneToPose[i] );
		}
		// printf("%3d %f %f %f\n", i, psource->bonefixup[i].worldorg[0], psource->bonefixup[i].worldorg[1], psource->bonefixup[i].worldorg[2] );
		/*
		AngleMatrix( angle, m );
		printf("%8.4f %8.4f %8.4f\n", m[0][0], m[1][0], m[2][0] );
		printf("%8.4f %8.4f %8.4f\n", m[0][1], m[1][1], m[2][1] );
		printf("%8.4f %8.4f %8.4f\n", m[0][2], m[1][2], m[2][2] );
		*/
	}
}




int Grab_Nodes( s_node_t *pnodes )
{
	int index;
	char name[1024];
	int parent;
	int numbones = 0;

	for (index = 0; index < MAXSTUDIOSRCBONES; index++)
	{
		pnodes[index].parent = -1;
	}

	while (GetLineInput()) 
	{
		if (sscanf( g_szLine, "%d \"%[^\"]\" %d", &index, name, &parent ) == 3)
		{
			// check for duplicated bones
			/*
			if (strlen(pnodes[index].name) != 0)
			{
				MdlError( "bone \"%s\" exists more than once\n", name );
			}
			*/
			
			V_strcpy_safe( pnodes[index].name, name );
			pnodes[index].parent = parent;
			if (index > numbones)
			{
				numbones = index;
			}
		}
		else 
		{
			return numbones + 1;
		}
	}
	MdlError( "Unexpected EOF at line %d\n", g_iLinecount );
	return 0;
}




void clip_rotations( RadianEuler& rot )
{
	int j;
	// clip everything to : -M_PI <= x < M_PI

	for (j = 0; j < 3; j++) {
		while (rot[j] >= M_PI) 
			rot[j] -= M_PI*2;
		while (rot[j] < -M_PI) 
			rot[j] += M_PI*2;
	}
}


void clip_rotations( Vector& rot )
{
	int j;
	// clip everything to : -180 <= x < 180

	for (j = 0; j < 3; j++) {
		while (rot[j] >= 180) 
			rot[j] -= 180*2;
		while (rot[j] < -180) 
			rot[j] += 180*2;
	}
}



/*
=================
Cmd_Eyeposition
=================
*/
void Cmd_Eyeposition (void)
{
// rotate points into frame of reference so g_model points down the positive x
// axis
	//	FIXME: these coords are bogus
	GetToken (false);
	eyeposition[1] = verify_atof (token);

	GetToken (false);
	eyeposition[0] = -verify_atof (token);

	GetToken (false);
	eyeposition[2] = verify_atof (token);
}


//-----------------------------------------------------------------------------
// Cmd_MaxEyeDeflection
//-----------------------------------------------------------------------------
void Cmd_MaxEyeDeflection()
{
	GetToken( false );
	g_flMaxEyeDeflection = cosf( verify_atof( token ) * M_PI / 180.0f );
}


//-----------------------------------------------------------------------------
// Cmd_Illumposition
//-----------------------------------------------------------------------------
void Cmd_Illumposition( void )
{
	GetToken( false );
	illumposition[0] = verify_atof( token );

	GetToken( false );
	illumposition[1] = verify_atof( token );

	GetToken( false );
	illumposition[2] = verify_atof( token );

	if ( TokenAvailable() )
	{
		GetToken( false );

		Q_strncpy( g_attachment[g_numattachments].name, "__illumPosition", sizeof(g_attachment[g_numattachments].name) );
		Q_strncpy( g_attachment[g_numattachments].bonename, token, sizeof(g_attachment[g_numattachments].bonename) );
		AngleMatrix( QAngle( 0, 0, 0 ), illumposition, g_attachment[g_numattachments].local );
		g_attachment[g_numattachments].type |= IS_RIGID;

		g_illumpositionattachment = g_numattachments + 1;
		++g_numattachments;
	}
	else
	{
		g_illumpositionattachment = 0;

		// rotate points into frame of reference so 
		// g_model points down the positive x axis
		// FIXME: these coords are bogus
		float flTemp = illumposition[0];
		illumposition[0] = -illumposition[1];
		illumposition[1] = flTemp;
	}

	illumpositionset = true;
}


//-----------------------------------------------------------------------------
// Process Cmd_Modelname
//-----------------------------------------------------------------------------
void ProcessModelName( const char *pModelName )
{
	// Abort early if modelname is too big
	// - actually that's okay, it's just an identifier and can be truncated

	g_bHasModelName = true;
	Q_strncpy( outname, pModelName, sizeof( outname ) );
}


//-----------------------------------------------------------------------------
// Parse Cmd_Modelname
//-----------------------------------------------------------------------------
void Cmd_Modelname (void)
{
	GetToken (false);
	if ( token[0] == '/' || token[0] == '\\' )
	{
		MdlWarning( "$modelname key has slash as first character. Removing.\n" );
		ProcessModelName( &token[1] );
	}
	else
	{
		ProcessModelName( token );
	}
}

void Cmd_Autocenter()
{
	g_centerstaticprop = true;
}

/*
===============
===============
*/


//-----------------------------------------------------------------------------
// Parse the body command from a .qc file
//-----------------------------------------------------------------------------
void ProcessOptionStudio( s_model_t *pmodel, const char *pFullPath, CDmeSourceSkin *pSkin )
{
	Q_strncpy( pmodel->filename, pFullPath, sizeof(pmodel->filename) );

	if ( pSkin->m_flScale != 0.0f )
	{
		pmodel->scale = g_currentscale = pSkin->m_flScale;
	}
	else
	{
		pmodel->scale = g_currentscale = g_defaultscale;
	}

	// load source
	pmodel->source = Load_Source( pmodel->filename, "", pSkin->m_bFlipTriangles, true );

	// Reset currentscale to whatever global we currently have set
	// g_defaultscale gets set in Cmd_ScaleUp everytime the $scale command is used.
	g_currentscale = g_defaultscale;
}


//-----------------------------------------------------------------------------
// Parse the studio options from a .qc file
//-----------------------------------------------------------------------------
bool ParseOptionStudio( CDmeSourceSkin *pSkin )
{
	if ( !GetToken( false ) ) 
		return false;

	pSkin->SetRelativeFileName( token );
	while ( TokenAvailable() )
	{
		GetToken(false);
		if ( !Q_stricmp( "reverse", token ) )
		{
			pSkin->m_bFlipTriangles = true;
			continue;
		}

		if ( !Q_stricmp( "scale", token ) )
		{
			GetToken(false);
			pSkin->m_flScale = verify_atof( token );
			continue;
		}

		if ( !Q_stricmp( "faces", token ) )
		{
			GetToken( false );
			GetToken( false );
			continue;
		}

		if ( !Q_stricmp( "bias", token ) )
		{
			GetToken( false );
			continue;
		}

		if ( !Q_stricmp( "{", token ) )
		{
			UnGetToken( );
			break;
		}

		MdlError("unknown command \"%s\"\n", token );
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Parse + process the studio options from a .qc file
//-----------------------------------------------------------------------------
void Option_Studio( s_model_t *pmodel )
{
	CDmeSourceSkin *pSourceSkin = CreateElement< CDmeSourceSkin >( "" );

	// Set defaults
	pSourceSkin->m_flScale = g_defaultscale;

	if ( ParseOptionStudio( pSourceSkin ) )
	{
		ProcessOptionStudio( pmodel, pSourceSkin->GetRelativeFileName(), pSourceSkin );
	}
	DestroyElement( pSourceSkin );
}


int Option_Blank( )
{
	g_model[g_nummodels] = (s_model_t *)kalloc( 1, sizeof( s_model_t ) );

	g_source[g_numsources] = (s_source_t *)kalloc( 1, sizeof( s_source_t ) );
	g_model[g_nummodels]->source = g_source[g_numsources];
	g_numsources++;

	g_bodypart[g_numbodyparts].pmodel[g_bodypart[g_numbodyparts].nummodels] = g_model[g_nummodels];

	V_strcpy_safe( g_model[g_nummodels]->name, "blank" );

	g_bodypart[g_numbodyparts].nummodels++;
	g_nummodels++;
	return 0;
}


void Cmd_Bodygroup( )
{
	int is_started = 0;

	if ( !GetToken( false ) ) 
		return;

	if (g_numbodyparts == 0) 
	{
		g_bodypart[g_numbodyparts].base = 1;
	}
	else 
	{
		g_bodypart[g_numbodyparts].base = g_bodypart[g_numbodyparts-1].base * g_bodypart[g_numbodyparts-1].nummodels;
	}
	V_strcpy_safe( g_bodypart[g_numbodyparts].name, token );

	do
	{
		GetToken (true);
		if (endofscript)
			return;
		else if (token[0] == '{')
		{
			is_started = 1;
		}
		else if (token[0] == '}')
		{
			break;
		}
		else if (stricmp("studio", token ) == 0)
		{
			g_model[g_nummodels] = (s_model_t *)kalloc( 1, sizeof( s_model_t ) );
			g_bodypart[g_numbodyparts].pmodel[g_bodypart[g_numbodyparts].nummodels] = g_model[g_nummodels];
			g_bodypart[g_numbodyparts].nummodels++;
		
			Option_Studio( g_model[g_nummodels] );

			// Body command should add any flex commands in the source loaded
			if ( g_model[g_nummodels]->source )
			{
				AddBodyFlexData( g_model[g_nummodels]->source, g_nummodels );
				AddBodyAttachments( g_model[g_nummodels]->source );
			}

			g_nummodels++;
		}
		else if (stricmp("blank", token ) == 0)
		{
			Option_Blank( );
		}
		else
		{
			MdlError("unknown bodygroup option: \"%s\"\n", token );
		}
	} while (1);

	g_numbodyparts++;
	return;
}


//-----------------------------------------------------------------------------
// Add A Body Flex Rule
//-----------------------------------------------------------------------------
void AddBodyFlexFetchRule(
	s_source_t *pSource,
	s_flexrule_t *pRule,
	int rawIndex,
	const CUtlVector< int > &pRawIndexToRemapSourceIndex,
	const CUtlVector< int > &pRawIndexToRemapLocalIndex,
	const CUtlVector< int > &pRemapSourceIndexToGlobalFlexControllerIndex )
{
	// Lookup the various indices of the requested input to fetch
	// Relative to the remapped controls in the current s_source_t
	const int remapSourceIndex = pRawIndexToRemapSourceIndex[ rawIndex ];
	// Relative to the specific remapped control
	const int remapLocalIndex = pRawIndexToRemapLocalIndex[ rawIndex ];
	// The global flex controller index that the user ultimately twiddles
	const int globalFlexControllerIndex = pRemapSourceIndexToGlobalFlexControllerIndex[ remapSourceIndex ];

	// Get the Remap record
	s_flexcontrollerremap_t &remap = pSource->m_FlexControllerRemaps[ remapSourceIndex ];
	switch ( remap.m_RemapType )
	{
	case FLEXCONTROLLER_REMAP_PASSTHRU:
		// Easy As!
		pRule->op[ pRule->numops ].op = STUDIO_FETCH1;
		pRule->op[ pRule->numops ].d.index = globalFlexControllerIndex;
		pRule->numops++;
		break;

	case FLEXCONTROLLER_REMAP_EYELID:
		if ( remapLocalIndex == 0 )
		{
			pRule->op[ pRule->numops ].op = STUDIO_CONST;
			pRule->op[ pRule->numops ].d.value = remap.m_EyesUpDownFlexController >= 0 ? remap.m_EyesUpDownFlexController : -1;
			pRule->numops++;

			pRule->op[ pRule->numops ].op = STUDIO_CONST;
			pRule->op[ pRule->numops ].d.value = remap.m_BlinkController >= 0 ? remap.m_BlinkController : -1;
			pRule->numops++;

			pRule->op[ pRule->numops ].op = STUDIO_CONST;
			pRule->op[ pRule->numops ].d.value = globalFlexControllerIndex;	// CloseLid
			pRule->numops++;

			pRule->op[ pRule->numops ].op = STUDIO_DME_LOWER_EYELID;
			pRule->op[ pRule->numops ].d.index = remap.m_MultiIndex;	// CloseLidV
			pRule->numops++;
		}
		else
		{
			pRule->op[ pRule->numops ].op = STUDIO_CONST;
			pRule->op[ pRule->numops ].d.value = remap.m_EyesUpDownFlexController >= 0 ? remap.m_EyesUpDownFlexController : -1;
			pRule->numops++;

			pRule->op[ pRule->numops ].op = STUDIO_CONST;
			pRule->op[ pRule->numops ].d.value = remap.m_BlinkController >= 0 ? remap.m_BlinkController : -1;
			pRule->numops++;

			pRule->op[ pRule->numops ].op = STUDIO_CONST;
			pRule->op[ pRule->numops ].d.value = globalFlexControllerIndex;	// CloseLid
			pRule->numops++;

			pRule->op[ pRule->numops ].op = STUDIO_DME_UPPER_EYELID;
			pRule->op[ pRule->numops ].d.index = remap.m_MultiIndex;	// CloseLidV
			pRule->numops++;
		}
		break;

	case FLEXCONTROLLER_REMAP_2WAY:
		// A little trickier... local index 0 is on the left, local index 1 is on the right
		// Left Equivalent RemapVal( -1.0, 0.0, 0.0, 1.0 )
		// Right Equivalent RemapVal( 0.0, 1.0, 0.0, 1.0 )
		if ( remapLocalIndex == 0 )
		{
			pRule->op[ pRule->numops ].op = STUDIO_2WAY_0;
			pRule->op[ pRule->numops ].d.index = globalFlexControllerIndex;
			pRule->numops++;
		}
		else
		{
			pRule->op[ pRule->numops ].op = STUDIO_2WAY_1;
			pRule->op[ pRule->numops ].d.index = globalFlexControllerIndex;
			pRule->numops++;
		}
		break;

	case FLEXCONTROLLER_REMAP_NWAY:
		{
			int nRemapCount = remap.m_RawControls.Count();
			float flStep = ( nRemapCount > 2 ) ? 2.0f / ( nRemapCount - 1 ) : 0.0f;

			if ( remapLocalIndex == 0 )
			{
				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = -11.0f;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = -10.0f;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = -1.0f;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = -1.0f + flStep;
				pRule->numops++;
			}
			else if ( remapLocalIndex == nRemapCount - 1 )
			{
				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = 1.0f - flStep;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = 1.0f;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = 10.0f;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = 11.0f;
				pRule->numops++;
			}
			else
			{
				float flPeak = remapLocalIndex * flStep - 1.0f;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = flPeak - flStep;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = flPeak;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = flPeak;
				pRule->numops++;

				pRule->op[ pRule->numops ].op = STUDIO_CONST;
				pRule->op[ pRule->numops ].d.value = flPeak + flStep;
				pRule->numops++;
			}

			pRule->op[ pRule->numops ].op = STUDIO_CONST;
			pRule->op[ pRule->numops ].d.value = remap.m_MultiIndex;
			pRule->numops++;

			pRule->op[ pRule->numops ].op = STUDIO_NWAY;
			pRule->op[ pRule->numops ].d.index = globalFlexControllerIndex;
			pRule->numops++;
		}
		break;
	default:
		Assert( 0 );
		// This is an error condition
		pRule->op[ pRule->numops ].op = STUDIO_CONST;
		pRule->op[ pRule->numops ].d.value = 1.0f;
		pRule->numops++;
		break;
	}
}


//-----------------------------------------------------------------------------
// Add A Body Flex Rule
//-----------------------------------------------------------------------------
void AddBodyFlexRule(
	s_source_t *pSource,
	s_combinationrule_t &rule,
	int nFlexDesc,
	const CUtlVector< int > &pRawIndexToRemapSourceIndex,
	const CUtlVector< int > &pRawIndexToRemapLocalIndex,
	const CUtlVector< int > &pRemapSourceIndexToGlobalFlexControllerIndex )
{
	if ( g_numflexrules >= MAXSTUDIOFLEXRULES )
		MdlError( "Line %d: Too many flex rules, max %d",
			g_iLinecount, MAXSTUDIOFLEXRULES );

	s_flexrule_t *pRule = &g_flexrule[g_numflexrules++];
	pRule->flex = nFlexDesc;

	// This will multiply the combination together
	const int nCombinationCount = rule.m_Combination.Count();
	if ( nCombinationCount )
	{
		for ( int j = 0; j < nCombinationCount; ++j )
		{
			// Handle any controller remapping
			AddBodyFlexFetchRule( pSource, pRule, rule.m_Combination[ j ],
				pRawIndexToRemapSourceIndex, pRawIndexToRemapLocalIndex,
				pRemapSourceIndexToGlobalFlexControllerIndex );
		}

		if ( nCombinationCount > 1 )
		{
			pRule->op[ pRule->numops ].op = STUDIO_COMBO;
			pRule->op[ pRule->numops ].d.index = nCombinationCount;
			pRule->numops++;
		}
	}

	// This will multiply in the suppressors
	int nDominators = rule.m_Dominators.Count();
	for ( int j = 0; j < nDominators; ++j )
	{
		const int nFactorCount = rule.m_Dominators[j].Count();
		if ( nFactorCount )
		{
			for ( int k = 0; k < nFactorCount; ++k )
			{
				AddBodyFlexFetchRule( pSource, pRule, rule.m_Dominators[ j ][ k ],
					pRawIndexToRemapSourceIndex, pRawIndexToRemapLocalIndex,
					pRemapSourceIndexToGlobalFlexControllerIndex );
			}

			pRule->op[ pRule->numops ].op = STUDIO_DOMINATE;
			pRule->op[ pRule->numops ].d.index = nFactorCount;
			pRule->numops++;
		}
	}
}
	

//-----------------------------------------------------------------------------
// Adds flex controller data to a particular source
//-----------------------------------------------------------------------------
void AddFlexControllers(
	s_source_t *pSource )
{
	CUtlVector< int > &r2s = pSource->m_rawIndexToRemapSourceIndex;
	CUtlVector< int > &r2l = pSource->m_rawIndexToRemapLocalIndex;
	CUtlVector< int > &l2i = pSource->m_leftRemapIndexToGlobalFlexControllIndex;
	CUtlVector< int > &r2i = pSource->m_rightRemapIndexToGlobalFlexControllIndex;

	// Number of Raw controls in this source
	const int nRawControlCount = pSource->m_CombinationControls.Count();
	// Initialize rawToRemapIndices
	r2s.SetSize( nRawControlCount );
	r2l.SetSize( nRawControlCount );
	for ( int i = 0; i < nRawControlCount; ++i )
	{
		r2s[ i ] = -1;
		r2l[ i ] = -1;
	}

	// Number of Remapped Controls in this source
	const int nRemappedControlCount = pSource->m_FlexControllerRemaps.Count();
	l2i.SetSize( nRemappedControlCount );
	r2i.SetSize( nRemappedControlCount );

	for ( int i = 0; i < nRemappedControlCount; ++i )
	{
		s_flexcontrollerremap_t &remapControl = pSource->m_FlexControllerRemaps[ i ];

		// Number of Raw Controls In This Remapped Control
		const int nRemappedRawControlCount = remapControl.m_RawControls.Count();

		// Figure out the mapping from raw to remapped
		for ( int j = 0; j < nRemappedRawControlCount; ++j )
		{
			for ( int k = 0; k < nRawControlCount; ++k )
			{
				if ( remapControl.m_RawControls[ j ] == pSource->m_CombinationControls[ k ].name )
				{
					Assert( r2s[ k ] == -1 );
					Assert( r2l[ k ] == -1 );
					r2s[ k ] = i;	// The index of the remapped control
					r2l[ k ] = j;	// The index of which control this is in the remap
					break;
				}
			}
		}

		if ( remapControl.m_bIsStereo )
		{
			// The controls have to be named 'right_' and 'left_' and right has to be first for
			// hlfaceposer to recognize them

			// See if we can add two more flex controllers
			if ( ( g_numflexcontrollers + 1 ) >= MAXSTUDIOFLEXCTRL)
				MdlError( "Line %d: Too many flex controllers, max %d, cannot add split control %s from source %s",
					g_iLinecount, MAXSTUDIOFLEXCTRL, remapControl.m_Name.Get(), pSource->filename );

			s_flexcontroller_t *pController;

			int nLen = remapControl.m_Name.Length();
			char *pTemp = (char*)_alloca( nLen + 7 );	// 'left_' && 'right_'

			memcpy( pTemp + 6, remapControl.m_Name.Get(), nLen + 1 );
			memcpy( pTemp, "right_", 6 );
			pTemp[nLen + 6] = '\0';

			remapControl.m_RightIndex = g_numflexcontrollers;
			r2i[ i ] = g_numflexcontrollers;
			pController = &g_flexcontroller[g_numflexcontrollers++];	
			Q_strncpy( pController->name, pTemp, sizeof( pController->name ) );
			Q_strncpy( pController->type, pTemp, sizeof( pController->type ) );

			if ( remapControl.m_RemapType == FLEXCONTROLLER_REMAP_2WAY || remapControl.m_RemapType == FLEXCONTROLLER_REMAP_EYELID )
			{
				pController->min = -1.0f;
				pController->max = 1.0f;
			}
			else
			{
				pController->min = 0.0f;
				pController->max = 1.0f;
			}

			memcpy( pTemp + 5, remapControl.m_Name.Get(), nLen + 1 );
			memcpy( pTemp, "left_", 5 );
			pTemp[nLen + 5] = '\0';

			remapControl.m_LeftIndex = g_numflexcontrollers;
			l2i[ i ] = g_numflexcontrollers;
			pController = &g_flexcontroller[g_numflexcontrollers++];	
			Q_strncpy( pController->name, pTemp, sizeof( pController->name ) );
			Q_strncpy( pController->type, pTemp, sizeof( pController->type ) );

			if ( remapControl.m_RemapType == FLEXCONTROLLER_REMAP_2WAY || remapControl.m_RemapType == FLEXCONTROLLER_REMAP_EYELID )
			{
				pController->min = -1.0f;
				pController->max = 1.0f;
			}
			else
			{
				pController->min = 0.0f;
				pController->max = 1.0f;
			}
		}
		else
		{
			// See if we can add one more flex controller
			if ( g_numflexcontrollers >= MAXSTUDIOFLEXCTRL)
				MdlError( "Line %d: Too many flex controllers, max %d, cannot add control %s from source %s",
					g_iLinecount, MAXSTUDIOFLEXCTRL, remapControl.m_Name.Get(), pSource->filename );

			remapControl.m_Index = g_numflexcontrollers;
			r2i[ i ] = g_numflexcontrollers;
			l2i[ i ] = g_numflexcontrollers;
			s_flexcontroller_t *pController = &g_flexcontroller[g_numflexcontrollers++];	
			Q_strncpy( pController->name, remapControl.m_Name.Get(), sizeof( pController->name ) );
			Q_strncpy( pController->type, remapControl.m_Name.Get(), sizeof( pController->type ) );

			if ( remapControl.m_RemapType == FLEXCONTROLLER_REMAP_2WAY || remapControl.m_RemapType == FLEXCONTROLLER_REMAP_EYELID )
			{
				pController->min = -1.0f;
				pController->max = 1.0f;
			}
			else
			{
				pController->min = 0.0f;
				pController->max = 1.0f;
			}
		}

		if ( remapControl.m_RemapType == FLEXCONTROLLER_REMAP_NWAY || remapControl.m_RemapType == FLEXCONTROLLER_REMAP_EYELID )
		{
			if ( g_numflexcontrollers >= MAXSTUDIOFLEXCTRL)
				MdlError( "Line %d: Too many flex controllers, max %d, cannot add value control for nWay %s from source %s",
				g_iLinecount, MAXSTUDIOFLEXCTRL, remapControl.m_Name.Get(), pSource->filename );

			remapControl.m_MultiIndex = g_numflexcontrollers;
			s_flexcontroller_t *pController = &g_flexcontroller[g_numflexcontrollers++];	
			const int nLen = remapControl.m_Name.Length();
			char *pTemp = ( char * )_alloca( nLen + 6 + 1 ); // 'multi_' + 1 for the NULL

			memcpy( pTemp, "multi_", 6 );
			memcpy( pTemp + 6, remapControl.m_Name.Get(), nLen + 1 );
			pTemp[nLen+6] = '\0';
			Q_strncpy( pController->name, pTemp, sizeof( pController->name ) );
			Q_strncpy( pController->type, pTemp, sizeof( pController->type ) );

			pController->min = -1.0f;
			pController->max = 1.0f;
		}

		if ( remapControl.m_RemapType == FLEXCONTROLLER_REMAP_EYELID )
		{
			// Make a blink controller

			if ( g_numflexcontrollers >= MAXSTUDIOFLEXCTRL)
				MdlError( "Line %d: Too many flex controllers, max %d, cannot add value control for nWay %s from source %s",
				g_iLinecount, MAXSTUDIOFLEXCTRL, remapControl.m_Name.Get(), pSource->filename );

			remapControl.m_BlinkController = g_numflexcontrollers;
			s_flexcontroller_t *pController = &g_flexcontroller[g_numflexcontrollers++];	

			Q_strncpy( pController->name, "blink", sizeof( pController->name ) );
			Q_strncpy( pController->type, "blink", sizeof( pController->type ) );

			pController->min = 0.0f;
			pController->max = 1.0f;
		}
	}

#ifdef _DEBUG
	for ( int j = 0; j != nRawControlCount; ++j )
	{
		Assert( r2s[ j ] != -1 );
		Assert( r2l[ j ] != -1 );
	}
#endif // def _DEBUG
}


//-----------------------------------------------------------------------------
// Adds flex controller remappers
//-----------------------------------------------------------------------------
void AddBodyFlexRemaps( s_source_t *pSource )
{
	int nCount = pSource->m_FlexControllerRemaps.Count();
	for( int i = 0; i < nCount; ++i )
	{
		int k = g_FlexControllerRemap.AddToTail();
		s_flexcontrollerremap_t &remap = g_FlexControllerRemap[k];
		remap = pSource->m_FlexControllerRemaps[i];
	}
}					 


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void AddBodyFlexRules( s_source_t *pSource )
{
	const int nRemapCount = pSource->m_FlexControllerRemaps.Count();
	for ( int i = 0; i < nRemapCount; ++i )
	{
		s_flexcontrollerremap_t &remap = pSource->m_FlexControllerRemaps[ i ];
		if ( remap.m_RemapType == FLEXCONTROLLER_REMAP_EYELID && !remap.m_EyesUpDownFlexName.IsEmpty() )
		{
			for ( int j = 0; j < g_numflexcontrollers; ++j )
			{
				if ( !Q_strcmp( g_flexcontroller[ j ].name, remap.m_EyesUpDownFlexName.Get() ) )
				{
					Assert( remap.m_EyesUpDownFlexController == -1 );
					remap.m_EyesUpDownFlexController = j;
					break;
				}
			}
		}
	}

	const int nCount = pSource->m_CombinationRules.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		s_combinationrule_t &rule = pSource->m_CombinationRules[i];
		s_flexkey_t &flexKey = g_flexkey[ pSource->m_nKeyStartIndex + rule.m_nFlex ];
		AddBodyFlexRule( pSource, rule, flexKey.flexdesc,
			pSource->m_rawIndexToRemapSourceIndex, pSource->m_rawIndexToRemapLocalIndex, pSource->m_leftRemapIndexToGlobalFlexControllIndex );
		if ( flexKey.flexpair != 0 )
		{
			AddBodyFlexRule( pSource, rule, flexKey.flexpair,
				pSource->m_rawIndexToRemapSourceIndex, pSource->m_rawIndexToRemapLocalIndex, pSource->m_rightRemapIndexToGlobalFlexControllIndex );
		}
	}
}


//-----------------------------------------------------------------------------
// Process a body command
//-----------------------------------------------------------------------------
void AddBodyFlexData( s_source_t *pSource, int imodel )
{
	pSource->m_nKeyStartIndex = g_numflexkeys;

	// Add flex keys
	int nCount = pSource->m_FlexKeys.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		s_flexkey_t &key = pSource->m_FlexKeys[i];

		if ( g_numflexkeys >= MAXSTUDIOFLEXKEYS )
			MdlError( "Line %d: Too many flex keys, max %d, cannot add flexKey %s from source %s",
				g_iLinecount, MAXSTUDIOFLEXKEYS, key.animationname, pSource->filename );

		memcpy( &g_flexkey[g_numflexkeys], &key, sizeof(s_flexkey_t) );
		g_flexkey[g_numflexkeys].imodel = imodel;

		// flexpair was set up in AddFlexKey
		if ( key.flexpair )
		{
			char mod[512];
			Q_snprintf( mod, sizeof(mod), "%sL", key.animationname );
			g_flexkey[g_numflexkeys].flexdesc = Add_Flexdesc( mod );
			Q_snprintf( mod, sizeof(mod), "%sR", key.animationname );
			g_flexkey[g_numflexkeys].flexpair = Add_Flexdesc( mod );
		}
		else
		{
			g_flexkey[g_numflexkeys].flexdesc = Add_Flexdesc( key.animationname );
			g_flexkey[g_numflexkeys].flexpair = 0;
		}

		++g_numflexkeys;
	}

	AddFlexControllers( pSource );

	AddBodyFlexRemaps( pSource );
}

//-----------------------------------------------------------------------------
// Comparison operator for s_attachment_t
//-----------------------------------------------------------------------------
bool s_attachment_t::operator==( const s_attachment_t &rhs ) const
{
	if ( Q_strcmp( name, rhs.name ) )
		return false;

	if ( Q_stricmp( bonename, rhs.bonename ) ||
		bone != rhs.bone ||
		type != rhs.type ||
		flags != rhs.flags ||
		Q_memcmp( local.Base(), rhs.local.Base(), sizeof( local ) ) )
	{
		RadianEuler iEuler, jEuler;
		Vector iPos, jPos;
		MatrixAngles( local, iEuler, iPos );
		MatrixAngles( rhs.local, jEuler, jPos );
		MdlWarning(
			"Attachments with the same name but different parameters found\n"
			"  %s: ParentBone: %s Type: %d Flags: 0x%08x P: %6.2f %6.2f %6.2f R: %6.2f %6.2f %6.2f\n"
			"  %s: ParentBone: %s Type: %d Flags: 0x%08x P: %6.2f %6.2f %6.2f R: %6.2f %6.2f %6.2f\n",
			name, bonename, type, flags,
			iPos.x, iPos.y, iPos.z, RAD2DEG( iEuler.x ), RAD2DEG( iEuler.y ), RAD2DEG( iEuler.z ),
			rhs.name, rhs.bonename, rhs.type, rhs.flags,
			jPos.x, jPos.y, jPos.z, RAD2DEG( jEuler.x ), RAD2DEG( jEuler.y ), RAD2DEG( jEuler.z ) );

		return false;
	}

	return true;
}



//-----------------------------------------------------------------------------
// Add attachments from the s_source_t that aren't already present in the
// global attachment list.  At this point, the attachments aren't linked
// to the bone, but since that is done by string matching on the bone name
// the test for an attachment being a duplicate is still valid this early.
//-----------------------------------------------------------------------------
void AddBodyAttachments( s_source_t *pSource )
{
	for ( int i = 0; i < pSource->m_Attachments.Count(); ++i )
	{
		const s_attachment_t &sourceAtt = pSource->m_Attachments[i];

		bool bDuplicate = false;

		for ( int j = 0; j < g_numattachments; ++j )
		{
			if ( sourceAtt == g_attachment[j] )
			{
				bDuplicate = true;
				break;
			}
		}

		if ( bDuplicate )
			continue;

		if ( g_numattachments >= ARRAYSIZE( g_attachment ) )
		{
			MdlWarning( "Too Many Attachments (Max %d), Ignoring Attachment %s:%s\n",
				ARRAYSIZE( g_attachment ), pSource->filename, pSource->m_Attachments[i].name );
			continue;;
		}

		memcpy( &g_attachment[g_numattachments], &( pSource->m_Attachments[i] ), sizeof( s_attachment_t ) );
		++g_numattachments;
	}
}


//-----------------------------------------------------------------------------
// Process a body command
//-----------------------------------------------------------------------------
void ProcessCmdBody( const char *pFullPath, CDmeSourceSkin *pSkin )
{
	if ( g_numbodyparts == 0 ) 
	{
		g_bodypart[g_numbodyparts].base = 1;
	}
	else 
	{
		g_bodypart[g_numbodyparts].base = g_bodypart[g_numbodyparts-1].base * g_bodypart[g_numbodyparts-1].nummodels;
	}
	Q_strncpy( g_bodypart[g_numbodyparts].name, pSkin->m_SkinName.Get(), sizeof(g_bodypart[g_numbodyparts].name) );

	g_model[g_nummodels] = (s_model_t *)kalloc( 1, sizeof( s_model_t ) );
	g_bodypart[g_numbodyparts].pmodel[g_bodypart[g_numbodyparts].nummodels] = g_model[g_nummodels];
	g_bodypart[g_numbodyparts].nummodels = 1;

	ProcessOptionStudio( g_model[g_nummodels], pFullPath, pSkin );

	// Body command should add any flex commands in the source loaded
	if ( g_model[g_nummodels]->source )
	{
		AddBodyFlexData( g_model[g_nummodels]->source, g_nummodels );
		AddBodyAttachments( g_model[g_nummodels]->source );
		AddBodyFlexRules( g_model[ g_nummodels ]->source );
	}

	g_nummodels++;
	g_numbodyparts++;
}


//-----------------------------------------------------------------------------
// Parse the body command from a .qc file
//-----------------------------------------------------------------------------
void Cmd_Body( )
{
	if ( !GetToken(false) ) 
		return;

	CDmeSourceSkin *pSourceSkin = CreateElement< CDmeSourceSkin >( "" );

	// Set defaults
	pSourceSkin->m_flScale = g_defaultscale;

	pSourceSkin->m_SkinName = token;
	if ( ParseOptionStudio( pSourceSkin ) )
	{
		ProcessCmdBody( pSourceSkin->GetRelativeFileName(), pSourceSkin );
	}
	DestroyElement( pSourceSkin );
}



/*
===============
===============
*/

void Grab_Animation( s_source_t *pSource, const char *pAnimName )
{
	Vector pos;
	RadianEuler rot;
	char cmd[1024];
	int index;
	int	t = -99999999;
	int size;

	s_sourceanim_t *pAnim = FindOrAddSourceAnim( pSource, pAnimName );
	pAnim->startframe = -1;

	size = pSource->numbones * sizeof( s_bone_t );

	while ( GetLineInput() ) 
	{
		if ( sscanf( g_szLine, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2] ) == 7 )
		{
			if ( pAnim->startframe < 0 )
			{
				MdlError( "Missing frame start(%d) : %s", g_iLinecount, g_szLine );
			}

			scale_vertex( pos );
			VectorCopy( pos, pAnim->rawanim[t][index].pos );
			VectorCopy( rot, pAnim->rawanim[t][index].rot );

			clip_rotations( rot ); // !!!
			continue;
		}
		
		if ( sscanf( g_szLine, "%1023s %d", cmd, &index ) == 0 )
		{
			MdlError( "MdlError(%d) : %s", g_iLinecount, g_szLine );
			continue;
		}

		if ( !Q_stricmp( cmd, "time" ) ) 
		{
			t = index;
			if ( pAnim->startframe == -1 )
			{
				pAnim->startframe = t;
			}
			if ( t < pAnim->startframe )
			{
				MdlError( "Frame MdlError(%d) : %s", g_iLinecount, g_szLine );
			}
			if ( t > pAnim->endframe )
			{
				pAnim->endframe = t;
			}
			t -= pAnim->startframe;

			if ( t >= pAnim->rawanim.Count())
			{
				s_bone_t *ptr = NULL;
				pAnim->rawanim.AddMultipleToTail( t - pAnim->rawanim.Count() + 1, &ptr );
			}

			if ( pAnim->rawanim[t] != NULL )
			{
				continue;
			}

			pAnim->rawanim[t] = (s_bone_t *)kalloc( 1, size );

			// duplicate previous frames keys
			if ( t > 0 && pAnim->rawanim[t-1] )
			{
				for ( int j = 0; j < pSource->numbones; j++ )
				{
					VectorCopy( pAnim->rawanim[t-1][j].pos, pAnim->rawanim[t][j].pos );
					VectorCopy( pAnim->rawanim[t-1][j].rot, pAnim->rawanim[t][j].rot );
				}
			}
			continue;
		}
		
		if ( !Q_stricmp( cmd, "end" ) ) 
		{
			pAnim->numframes = pAnim->endframe - pAnim->startframe + 1;

			for ( t = 0; t < pAnim->numframes; t++ )
			{
				if ( pAnim->rawanim[t] == NULL)
				{
					MdlError( "%s is missing frame %d\n", pSource->filename, t + pAnim->startframe );
				}
			}

			Build_Reference( pSource, pAnimName );
			return;
		}

		MdlError( "MdlError(%d) : %s", g_iLinecount, g_szLine );
	}

	MdlError( "unexpected EOF: %s\n", pSource->filename );
}





int Option_Activity( s_sequence_t *psequence )
{
	qboolean found;

	found = false;

	GetToken(false);
	V_strcpy_safe( psequence->activityname, token );

	GetToken(false);
	psequence->actweight = verify_atoi(token);

	if ( psequence->actweight == 0 )
	{
		TokenError( "Activity %s has a zero weight (weights must be integers > 0)\n", psequence->activityname );
	}

	return 0;
}


int Option_ActivityModifier( s_sequence_t *psequence )
{
	GetToken(false);
	V_strcpy_safe( psequence->activitymodifier[ psequence->numactivitymodifiers++ ].name, token );

	return 0;
}


/*
===============
===============
*/


int Option_Event ( s_sequence_t *psequence )
{
	if (psequence->numevents + 1 >= MAXSTUDIOEVENTS)
	{
		TokenError("too many events\n");
	}

	GetToken (false);
	
	V_strcpy_safe( psequence->event[psequence->numevents].eventname, token );

	GetToken( false );
	psequence->event[psequence->numevents].frame = verify_atoi( token );

	psequence->numevents++;

	// option token
	if (TokenAvailable())
	{
		GetToken( false );
		if (token[0] == '}') // opps, hit the end
			return 1;
		// found an option
		V_strcpy_safe( psequence->event[psequence->numevents-1].options, token );
	}

	return 0;
}



void Option_IKRule( s_ikrule_t *pRule )
{
	// chain
	GetToken( false );

	int i;
	for ( i = 0; i < g_numikchains; i++)
	{
		if (stricmp( token, g_ikchain[i].name ) == 0)
		{
			break;
		}
	}
	if (i >= g_numikchains)
	{
		TokenError( "unknown chain \"%s\" in ikrule\n", token );
	}
	pRule->chain = i;
	// default slot
	pRule->slot = i;

	// type
	GetToken( false );
	if (stricmp( token, "touch" ) == 0)
	{
		pRule->type = IK_SELF;

		// bone
		GetToken( false );
		V_strcpy_safe( pRule->bonename, token );
	}
	else if (stricmp( token, "footstep" ) == 0)
	{
		pRule->type = IK_GROUND;

		pRule->height = g_ikchain[pRule->chain].height;
		pRule->floor = g_ikchain[pRule->chain].floor;
		pRule->radius = g_ikchain[pRule->chain].radius;
	}
	else if (stricmp( token, "attachment" ) == 0)
	{
		pRule->type = IK_ATTACHMENT;

		// name of attachment
		GetToken( false );
		V_strcpy_safe( pRule->attachment, token );
	}
	else if (stricmp( token, "release" ) == 0)
	{
		pRule->type = IK_RELEASE;
	}
	else if (stricmp( token, "unlatch" ) == 0)
	{
		pRule->type = IK_UNLATCH;
	}

	pRule->contact = -1;

	while (TokenAvailable())
	{
		GetToken( false );
		if (stricmp( token, "height" ) == 0)
		{
			GetToken( false );
			pRule->height = verify_atof( token );
		}
		else if (stricmp( token, "target" ) == 0)
		{
			// slot
			GetToken( false );
			pRule->slot = verify_atoi( token );
		}
		else if (stricmp( token, "range" ) == 0)
		{
			// ramp
			GetToken( false );
			if (token[0] == '.')
				pRule->start = -1;
			else
				pRule->start = verify_atoi( token );

			GetToken( false );
			if (token[0] == '.')
				pRule->peak = -1;
			else
				pRule->peak = verify_atoi( token );
	
			GetToken( false );
			if (token[0] == '.')
				pRule->tail = -1;
			else
				pRule->tail = verify_atoi( token );

			GetToken( false );
			if (token[0] == '.')
				pRule->end = -1;
			else
				pRule->end = verify_atoi( token );
		}
		else if (stricmp( token, "floor" ) == 0)
		{
			GetToken( false );
			pRule->floor = verify_atof( token );
		}
		else if (stricmp( token, "pad" ) == 0)
		{
			GetToken( false );
			pRule->radius = verify_atof( token ) / 2.0f;
		}
		else if (stricmp( token, "radius" ) == 0)
		{
			GetToken( false );
			pRule->radius = verify_atof( token );
		}
		else if (stricmp( token, "contact" ) == 0)
		{
			GetToken( false );
			pRule->contact = verify_atoi( token );
		}
		else if (stricmp( token, "usesequence" ) == 0)
		{
			pRule->usesequence = true;
			pRule->usesource = false;
		}
		else if (stricmp( token, "usesource" ) == 0)
		{
			pRule->usesequence = false;
			pRule->usesource = true;
		}
		else if (stricmp( token, "fakeorigin" ) == 0)
		{
			GetToken( false );
			pRule->pos.x = verify_atof( token );
			GetToken( false );
			pRule->pos.y = verify_atof( token );
			GetToken( false );
			pRule->pos.z = verify_atof( token );

			pRule->bone = -1;
		}
		else if (stricmp( token, "fakerotate" ) == 0)
		{
			QAngle ang;

			GetToken( false );
			ang.x = verify_atof( token );
			GetToken( false );
			ang.y = verify_atof( token );
			GetToken( false );
			ang.z = verify_atof( token );

			AngleQuaternion( ang, pRule->q );

			pRule->bone = -1;
		}
		else if (stricmp( token, "bone" ) == 0)
		{
			V_strcpy_safe( pRule->bonename, token );
		}
		else
		{
			UnGetToken();
			return;
		}
	}
}


/*
=================
Cmd_Origin
=================
*/
void Cmd_Origin (void)
{
	GetToken (false);
	g_defaultadjust.x = verify_atof (token);

	GetToken (false);
	g_defaultadjust.y = verify_atof (token);

	GetToken (false);
	g_defaultadjust.z = verify_atof (token);

	if (TokenAvailable())
	{
		GetToken (false);
		g_defaultrotation.z = DEG2RAD( verify_atof( token ) + 90);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the default root rotation so that the Y axis is up instead of the Z axis (for Maya)
//-----------------------------------------------------------------------------
void ProcessUpAxis( const RadianEuler &angles )
{
	g_defaultrotation = angles;
}


//-----------------------------------------------------------------------------
// Purpose: Set the default root rotation so that the Y axis is up instead of the Z axis (for Maya)
//-----------------------------------------------------------------------------
void Cmd_UpAxis( void )
{
	// We want to create a rotation that rotates from the art space
	// (specified by the up direction) to a z up space
	// Note: x, -x, -y are untested
	RadianEuler angles( 0.0f, 0.0f, M_PI / 2.0f );
	GetToken (false);
	if (!Q_stricmp( token, "x" ))
	{
		// rotate 90 degrees around y to move x into z
		angles.x = 0.0f;
		angles.y = M_PI / 2.0f;
	}
	else if (!Q_stricmp( token, "-x" ))
	{
		// untested
		angles.x = 0.0f;
		angles.y = -M_PI / 2.0f;
	}
	else if (!Q_stricmp( token, "y" ))
	{
		// rotate 90 degrees around x to move y into z
		angles.x = M_PI / 2.0f;
		angles.y = 0.0f;
	}
	else if (!Q_stricmp( token, "-y" ))
	{
		// untested
		angles.x = -M_PI / 2.0f;
		angles.y = 0.0f;
	}
	else if (!Q_stricmp( token, "z" ))
	{
		// there's still a built in 90 degree Z rotation :(
		angles.x = 0.0f;
		angles.y = 0.0f;
	}
	else if (!Q_stricmp( token, "-z" ))
	{
		// there's still a built in 90 degree Z rotation :(
		angles.x = 0.0f;
		angles.y = 0.0f;
	}
	else
	{
		TokenError( "unknown $upaxis option: \"%s\"\n", token );
		return;
	}

	ProcessUpAxis( angles );
}


/*
=================
=================
*/
void Cmd_ScaleUp (void)
{
	GetToken (false);
	g_defaultscale = verify_atof (token);

	g_currentscale = g_defaultscale;
}


//-----------------------------------------------------------------------------
// Purpose: Sets how what size chunks to cut the animations into
//-----------------------------------------------------------------------------
void Cmd_AnimBlockSize( void )
{
	GetToken( false );
	g_animblocksize = verify_atoi( token );
	if (g_animblocksize < 1024)
	{
		g_animblocksize *= 1024;
	}
	while (TokenAvailable())
	{
		GetToken( false );
		if (!Q_stricmp( token, "nostall" ))
		{
			g_bNoAnimblockStall = true;
		}
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static void FlipFacing( s_source_t *pSrc )
{
	unsigned short tmp;

	int i, j;
	for( i = 0; i < pSrc->nummeshes; i++ )
	{
		s_mesh_t *pMesh = &pSrc->mesh[i];
		for( j = 0; j < pMesh->numfaces; j++ )
		{
			s_face_t &f = pSrc->face[pMesh->faceoffset + j];
			tmp = f.b;  f.b  = f.c;  f.c  = tmp;
		}
	}
}


// Processes source comment line and extracts information about the data file
void ProcessSourceComment( s_source_t *psource, const char *pCommentString )
{
	if ( char const *szSceneComment = StringAfterPrefix( pCommentString, "// SCENE=" ) )
	{
		char szScene[1024];
		Q_strncpy( szScene, szSceneComment, ARRAYSIZE( szScene ) );

		Q_FixSlashes( szScene );

		ProcessOriginalContentFile( psource->filename, szScene );
	}
}

// Processes original content file "szOriginalContentFile" that was used to generate
// data file "szDataFile"
void ProcessOriginalContentFile( char const *szDataFile, char const *szOriginalContentFile )
{
	// Early out: if no p4
	if ( g_bNoP4 )
		return;

	char const *szContentDirRootEnd = strstr( szDataFile, "\\content\\" );
	char const *szSceneName = strstr( szOriginalContentFile, "\\content\\" );
	if ( szContentDirRootEnd && szSceneName )
	{
		char chScenePath[ MAX_PATH ] = {0};
		Q_snprintf( chScenePath, sizeof( chScenePath ) - 1, "%.*s%s",
			szContentDirRootEnd - szDataFile, szDataFile, szSceneName );
		EnsureDependencyFileCheckedIn( chScenePath );
	}
	else if ( szContentDirRootEnd && !szSceneName )
	{
		// Assume relative path
		char chScenePath[ MAX_PATH ] = {0};
		Q_snprintf( chScenePath, sizeof( chScenePath ) - 1, "%.*s%s",
			max( strrchr( szDataFile, '\\' ), strrchr( szDataFile, '/' ) ) + 1 - szDataFile,
			szDataFile, szOriginalContentFile );
		EnsureDependencyFileCheckedIn( chScenePath );
	}
	else
	{
		MdlWarning( "ProcessOriginalContentFile for '%s' cannot detect scene source file from '%s'!\n", szDataFile, szOriginalContentFile );
	}
}


//-----------------------------------------------------------------------------
// Checks to see if the model source was already loaded
//-----------------------------------------------------------------------------
static s_source_t *FindCachedSource( const char* name, const char* xext )
{
	int i;

	if( xext[0] )
	{
		// we know what extension is necessary. . look for it.
		Q_snprintf( g_szFilename, sizeof(g_szFilename), "%s%s.%s", cddir[numdirs], name, xext );
		for (i = 0; i < g_numsources; i++)
		{
			if ( !Q_stricmp( g_szFilename, g_source[i]->filename ) )
				return g_source[i];
		}
	}
	else
	{
		// we don't know what extension to use, so look for all of 'em.
		Q_snprintf( g_szFilename, sizeof(g_szFilename), "%s%s.vrm", cddir[numdirs], name );
		for (i = 0; i < g_numsources; i++)
		{
			if ( !Q_stricmp( g_szFilename, g_source[i]->filename ) )
				return g_source[i];
		}
		Q_snprintf (g_szFilename, sizeof(g_szFilename), "%s%s.smd", cddir[numdirs], name );
		for (i = 0; i < g_numsources; i++)
		{
			if ( !Q_stricmp( g_szFilename, g_source[i]->filename ) )
				return g_source[i];
		}
		Q_snprintf (g_szFilename, sizeof(g_szFilename), "%s%s.dmx", cddir[numdirs], name );
		for (i = 0; i < g_numsources; i++)
		{
			if ( !Q_stricmp( g_szFilename, g_source[i]->filename ) )
				return g_source[i];
		}
		Q_snprintf (g_szFilename, sizeof(g_szFilename), "%s%s.xml", cddir[numdirs], name );
		for (i = 0; i < g_numsources; i++)
		{
			if ( !Q_stricmp( g_szFilename, g_source[i]->filename ) )
				return g_source[i];
		}
		Q_snprintf (g_szFilename, sizeof(g_szFilename), "%s%s.obj", cddir[numdirs], name );
		for (i = 0; i < g_numsources; i++)
		{
			if ( !Q_stricmp( g_szFilename, g_source[i]->filename ) )
				return g_source[i];
		}
		/*
		sprintf (g_szFilename, "%s%s.vta", cddir[numdirs], name );
		for (i = 0; i < g_numsources; i++)
		{
			if (stricmp( g_szFilename, g_source[i]->filename ) == 0)
				return g_source[i];
		}
		*/
	}

	// Not found
	return 0;
}


//-----------------------------------------------------------------------------
// Loads an animation/model source
//-----------------------------------------------------------------------------
s_source_t *Load_Source( const char *name, const char *ext, bool reverse, bool isActiveModel )
{
	if ( g_numsources >= MAXSTUDIOSEQUENCES )
		TokenError( "Load_Source( %s ) - overflowed g_numsources.", name );

	Assert(name);
	int namelen = strlen(name) + 1;
	char* pTempName = (char*)_alloca( namelen );
	char xext[32];
	int result = false;

	V_strncpy( pTempName, name, namelen );
	Q_ExtractFileExtension( pTempName, xext, sizeof( xext ) );

	if (xext[0] == '\0')
	{
		V_strcpy_safe( xext, ext );
	}
	else
	{
		Q_StripExtension( pTempName, pTempName, namelen );
	}

	s_source_t* pSource = FindCachedSource( pTempName, xext );
	if (pSource)
	{
		if (isActiveModel)
			pSource->isActiveModel = true;
		
		return pSource;
	}

	g_source[g_numsources] = (s_source_t *)kalloc( 1, sizeof( s_source_t ) );
	V_strcpy_safe( g_source[g_numsources]->filename, g_szFilename );


	if (isActiveModel)
	{
		g_source[g_numsources]->isActiveModel = true;
	}

	char const * load_extensions[] = { "vrm", "smd", "sma", "phys", "vta", "obj", "dmx", "xml" };
	int ( *load_procs[] )( s_source_t * ) = { Load_VRM, Load_SMD, Load_SMD, Load_SMD, Load_VTA, Load_OBJ, Load_DMX, Load_DMX };

	Assert( ARRAYSIZE(load_extensions) == ARRAYSIZE(load_procs) );
	for ( int kk = 0; kk < ARRAYSIZE( load_extensions ); ++ kk )
	{
		if ( ( !result && xext[0] == '\0' ) || Q_stricmp( xext, load_extensions[kk] ) == 0)
		{
			Q_snprintf( g_szFilename, sizeof(g_szFilename), "%s%s.%s", cddir[numdirs], pTempName, load_extensions[kk] );
			V_strcpy_safe( g_source[g_numsources]->filename, g_szFilename );
			result = (load_procs[kk])( g_source[g_numsources] );
			
			if ( result )
				EnsureDependencyFileCheckedIn( g_source[g_numsources]->filename );
		}
	}

	if (!g_bCreateMakefile && !result)
	{
		if (xext[0] == '\0')
			TokenError( "could not load file '%s%s'\n", cddir[numdirs], pTempName );
		else
			TokenError( "could not load file '%s%s.%s'\n", cddir[numdirs], pTempName, xext );
	}

	if ( g_source[g_numsources]->numbones == 0 )
	{
		TokenError( "missing all bones in file '%s'\n", g_source[g_numsources]->filename );
	}

	// copy over default settings of when the model was loaded (since there's no actual animation for some of the systems)
	VectorCopy( g_defaultadjust, g_source[g_numsources]->adjust );
	g_source[g_numsources]->scale = 1.0f;
	g_source[g_numsources]->rotation = g_defaultrotation;


	g_numsources++;
	if( reverse )
	{
		FlipFacing( g_source[g_numsources-1] );
	}

	return g_source[g_numsources-1];
}


s_sequence_t *LookupSequence( const char *name )
{
	int i;
	for ( i = 0; i < g_sequence.Count(); ++i )
	{
		if ( !Q_stricmp( g_sequence[i].name, name ) )
			return &g_sequence[i];
	}
	return NULL;
}


s_animation_t *LookupAnimation( const char *name )
{
	int i;
	for ( i = 0; i < g_numani; i++)
	{
		if ( !Q_stricmp( g_panimation[i]->name, name ) )
			return g_panimation[i];
	}

	s_sequence_t *pseq = LookupSequence( name );
	return pseq ? pseq->panim[0][0] : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: parse order dependant s_animcmd_t token for $animations
//-----------------------------------------------------------------------------
int ParseCmdlistToken( int &numcmds, s_animcmd_t *cmds )
{
	if (numcmds >= MAXSTUDIOCMDS)
	{
		return false;
	}
	s_animcmd_t *pcmd = &cmds[numcmds];
	if (stricmp("fixuploop", token ) == 0)
	{
		pcmd->cmd = CMD_FIXUP;

		GetToken( false );
		pcmd->u.fixuploop.start = verify_atoi( token );
		GetToken( false );
		pcmd->u.fixuploop.end = verify_atoi( token );
	}
	else if (strnicmp("weightlist", token, 6 ) == 0)
	{
		GetToken( false );

		int i;
		for ( i = 1; i < g_numweightlist; i++)
		{
			if (stricmp( g_weightlist[i].name, token ) == 0)
			{
				break;
			}
		}
		if (i == g_numweightlist)
		{
			TokenError( "unknown weightlist '%s\'\n", token );
		}
		pcmd->cmd = CMD_WEIGHTS;
		pcmd->u.weightlist.index = i;
	}
	else if (stricmp("subtract", token ) == 0)
	{
		pcmd->cmd = CMD_SUBTRACT;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown subtract animation '%s\'\n", token );
		}

		pcmd->u.subtract.ref = extanim;

		GetToken( false );
		pcmd->u.subtract.frame = verify_atoi( token );

		pcmd->u.subtract.flags |= STUDIO_POST;
	}
	else if (stricmp("presubtract", token ) == 0) // FIXME: rename this to something better
	{
		pcmd->cmd = CMD_SUBTRACT;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown presubtract animation '%s\'\n", token );
		}

		pcmd->u.subtract.ref = extanim;

		GetToken( false );
		pcmd->u.subtract.frame = verify_atoi( token );
	}
	else if (stricmp( "alignto", token ) == 0)
	{
		pcmd->cmd = CMD_AO;

		pcmd->u.ao.pBonename = NULL;

		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown alignto animation '%s\'\n", token );
		}

		pcmd->u.ao.ref = extanim;
		pcmd->u.ao.motiontype = STUDIO_X | STUDIO_Y;
		pcmd->u.ao.srcframe = 0;
		pcmd->u.ao.destframe = 0;
	}
	else if (stricmp( "align", token ) == 0)
	{
		pcmd->cmd = CMD_AO;

		pcmd->u.ao.pBonename = NULL;

		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown align animation '%s\'\n", token );
		}

		pcmd->u.ao.ref = extanim;

		// motion type to match
		pcmd->u.ao.motiontype = 0;
		GetToken( false );
		int ctrl;
		while ((ctrl = lookupControl( token )) != -1)
		{
			pcmd->u.ao.motiontype |= ctrl;
			GetToken( false );
		}
		if (pcmd->u.ao.motiontype == 0)
		{
			TokenError( "missing controls on align\n" );
		}

		// frame of reference animation to match
		pcmd->u.ao.srcframe = verify_atoi( token );

		// against what frame of the current animation
		GetToken( false );
		pcmd->u.ao.destframe = verify_atoi( token );
	}
	else if (stricmp( "alignboneto", token ) == 0)
	{
		pcmd->cmd = CMD_AO;

		GetToken( false );
		pcmd->u.ao.pBonename = strdup( token );
		
		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown alignboneto animation '%s\'\n", token );
		}

		pcmd->u.ao.ref = extanim;
		pcmd->u.ao.motiontype = STUDIO_X | STUDIO_Y;
		pcmd->u.ao.srcframe = 0;
		pcmd->u.ao.destframe = 0;
	}
	else if (stricmp( "alignbone", token ) == 0)
	{
		pcmd->cmd = CMD_AO;

		GetToken( false );
		pcmd->u.ao.pBonename = strdup( token );
		
		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown alignboneto animation '%s\'\n", token );
		}

		pcmd->u.ao.ref = extanim;

		// motion type to match
		pcmd->u.ao.motiontype = 0;
		GetToken( false );
		int ctrl;
		while ((ctrl = lookupControl( token )) != -1)
		{
			pcmd->u.ao.motiontype |= ctrl;
			GetToken( false );
		}
		if (pcmd->u.ao.motiontype == 0)
		{
			TokenError( "missing controls on align\n" );
		}

		// frame of reference animation to match
		pcmd->u.ao.srcframe = verify_atoi( token );

		// against what frame of the current animation
		GetToken( false );
		pcmd->u.ao.destframe = verify_atoi( token );
	}
	else if (stricmp( "match", token ) == 0)
	{
		pcmd->cmd = CMD_MATCH;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown match animation '%s\'\n", token );
		}

		pcmd->u.match.ref = extanim;
	}
	else if (stricmp( "matchblend", token ) == 0)
	{
		pcmd->cmd = CMD_MATCHBLEND;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			MdlError( "unknown match animation '%s\'\n", token );
		}

		pcmd->u.match.ref = extanim;

		// frame of reference animation to match
		GetToken( false );
		pcmd->u.match.srcframe = verify_atoi( token );

		// against what frame of the current animation
		GetToken( false );
		pcmd->u.match.destframe = verify_atoi( token );

		// backup and starting match in here
		GetToken( false );
		pcmd->u.match.destpre = verify_atoi( token );

		// continue blending match till here
		GetToken( false );
		pcmd->u.match.destpost = verify_atoi( token );

	}
	else if (stricmp( "worldspaceblend", token ) == 0)
	{
		pcmd->cmd = CMD_WORLDSPACEBLEND;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown worldspaceblend animation '%s\'\n", token );
		}

		pcmd->u.world.ref = extanim;
		pcmd->u.world.startframe = 0;
		pcmd->u.world.loops = false;
	}
	else if (stricmp( "worldspaceblendloop", token ) == 0)
	{
		pcmd->cmd = CMD_WORLDSPACEBLEND;

		GetToken( false );

		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown worldspaceblend animation '%s\'\n", token );
		}

		pcmd->u.world.ref = extanim;

		GetToken( false );
		pcmd->u.world.startframe = atoi( token );

		pcmd->u.world.loops = true;
	}
	else if (stricmp( "rotateto", token ) == 0)
	{
		pcmd->cmd = CMD_ANGLE;

		GetToken( false );
		pcmd->u.angle.angle = verify_atof( token );
	}
	else if (stricmp( "ikrule", token ) == 0)
	{
		pcmd->cmd = CMD_IKRULE;

		pcmd->u.ikrule.pRule = (s_ikrule_t *)kalloc( 1, sizeof( s_ikrule_t ) );

		Option_IKRule( pcmd->u.ikrule.pRule );
	}
	else if (stricmp( "ikfixup", token ) == 0)
	{
		pcmd->cmd = CMD_IKFIXUP;

		pcmd->u.ikfixup.pRule = (s_ikrule_t *)kalloc( 1, sizeof( s_ikrule_t ) );

		Option_IKRule( pcmd->u.ikrule.pRule );
	}
	else if (stricmp( "walkframe", token ) == 0)
	{
		pcmd->cmd = CMD_MOTION;

		// frame
		GetToken( false );
		pcmd->u.motion.iEndFrame = verify_atoi( token );

		// motion type to match
		pcmd->u.motion.motiontype = 0;
		while (TokenAvailable())
		{
			GetToken( false );
			int ctrl = lookupControl( token );
			if (ctrl != -1)
			{
				pcmd->u.motion.motiontype |= ctrl;
			}
			else
			{
				UnGetToken();
				break;
			}
		}

		/*
		GetToken( false ); // X
		pcmd->u.motion.x = verify_atof( token );

		GetToken( false ); // Y
		pcmd->u.motion.y = verify_atof( token );

		GetToken( false ); // A
		pcmd->u.motion.zr = verify_atof( token );
		*/
	}
	else if (stricmp( "walkalignto", token ) == 0)
	{
		pcmd->cmd = CMD_REFMOTION;

		GetToken( false );
		pcmd->u.motion.iEndFrame = verify_atoi( token );

		pcmd->u.motion.iSrcFrame = pcmd->u.motion.iEndFrame;

		GetToken( false ); // reference animation
		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown alignto animation '%s\'\n", token );
		}
		pcmd->u.motion.pRefAnim = extanim;

		pcmd->u.motion.iRefFrame = 0;

		// motion type to match
		pcmd->u.motion.motiontype = 0;
		while (TokenAvailable())
		{
			GetToken( false );
			int ctrl = lookupControl( token );
			if (ctrl != -1)
			{
				pcmd->u.motion.motiontype |= ctrl;
			}
			else
			{
				UnGetToken();
				break;
			}
		}


		/*
		GetToken( false ); // X
		pcmd->u.motion.x = verify_atof( token );

		GetToken( false ); // Y
		pcmd->u.motion.y = verify_atof( token );

		GetToken( false ); // A
		pcmd->u.motion.zr = verify_atof( token );
		*/
	}
	else if (stricmp( "walkalign", token ) == 0)
	{
		pcmd->cmd = CMD_REFMOTION;

		// end frame to apply motion over
		GetToken( false );
		pcmd->u.motion.iEndFrame = verify_atoi( token );

		// reference animation
		GetToken( false );
		s_animation_t *extanim = LookupAnimation( token );
		if (extanim == NULL)
		{
			TokenError( "unknown alignto animation '%s\'\n", token );
		}
		pcmd->u.motion.pRefAnim = extanim;

		// motion type to match
		pcmd->u.motion.motiontype = 0;
		while (TokenAvailable())
		{
			GetToken( false );
			int ctrl = lookupControl( token );
			if (ctrl != -1)
			{
				pcmd->u.motion.motiontype |= ctrl;
			}
			else
			{
				break;
			}
		}
		if (pcmd->u.motion.motiontype == 0)
		{
			TokenError( "missing controls on walkalign\n" );
		}

		// frame of reference animation to match
		pcmd->u.motion.iRefFrame = verify_atoi( token );

		// against what frame of the current animation
		GetToken( false );
		pcmd->u.motion.iSrcFrame = verify_atoi( token );
	}
	else if (stricmp("derivative", token ) == 0)
	{
		pcmd->cmd = CMD_DERIVATIVE;

		// get scale
		GetToken( false );
		pcmd->u.derivative.scale = verify_atof( token );
	}
	else if (stricmp("noanimation", token ) == 0)
	{
		pcmd->cmd = CMD_NOANIMATION;
	}
	else if (stricmp("lineardelta", token ) == 0)
	{
		pcmd->cmd = CMD_LINEARDELTA;
		pcmd->u.linear.flags |= STUDIO_AL_POST;
	}
	else if (stricmp("splinedelta", token ) == 0)
	{
		pcmd->cmd = CMD_LINEARDELTA;
		pcmd->u.linear.flags |= STUDIO_AL_POST;
		pcmd->u.linear.flags |= STUDIO_AL_SPLINE;
	}
	else if (stricmp("compress", token ) == 0)
	{
		pcmd->cmd = CMD_COMPRESS;

		// get frames to skip
		GetToken( false );
		pcmd->u.compress.frames = verify_atoi( token );
	}
	else if (stricmp("numframes", token ) == 0)
	{
		pcmd->cmd = CMD_NUMFRAMES;

		// get frames to force
		GetToken( false );
		pcmd->u.compress.frames = verify_atoi( token );
	}
	else if (stricmp("counterrotate", token ) == 0)
	{
		pcmd->cmd = CMD_COUNTERROTATE;

		// get bone name
		GetToken( false );
		pcmd->u.counterrotate.pBonename = strdup( token );
	}
	else if (stricmp("counterrotateto", token ) == 0)
	{
		pcmd->cmd = CMD_COUNTERROTATE;

		pcmd->u.counterrotate.bHasTarget = true;

		// get pitch
		GetToken( false );
		pcmd->u.counterrotate.targetAngle[0] = verify_atof( token );

		// get yaw
		GetToken( false );
		pcmd->u.counterrotate.targetAngle[1] = verify_atof( token );

		// get roll
		GetToken( false );
		pcmd->u.counterrotate.targetAngle[2] = verify_atof( token );

		// get bone name
		GetToken( false );
		pcmd->u.counterrotate.pBonename = strdup( token );
	}
	else if (stricmp("localhierarchy", token ) == 0)
	{
		pcmd->cmd = CMD_LOCALHIERARCHY;

		// get bone name
		GetToken( false );
		pcmd->u.localhierarchy.pBonename = strdup( token );

		// get parent name
		GetToken( false );
		pcmd->u.localhierarchy.pParentname = strdup( token );

		pcmd->u.localhierarchy.start = -1;
		pcmd->u.localhierarchy.peak = -1;
		pcmd->u.localhierarchy.tail = -1;
		pcmd->u.localhierarchy.end = -1;

		if (TokenAvailable())
		{
			GetToken( false );
			if (stricmp( token, "range" ) == 0)
			{
				//
				GetToken( false );
				pcmd->u.localhierarchy.start = verify_atof_with_null( token );

				//
				GetToken( false );
				pcmd->u.localhierarchy.peak = verify_atof_with_null( token );

				//
				GetToken( false );
				pcmd->u.localhierarchy.tail = verify_atof_with_null( token );

				//
				GetToken( false );
				pcmd->u.localhierarchy.end = verify_atof_with_null( token );
			}
			else
			{
				UnGetToken();
			}
		}
	}
	else
	{
		return false;
	}
	numcmds++;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: parse order independant s_animation_t token for $animations
//-----------------------------------------------------------------------------
bool ParseAnimationToken( s_animation_t *panim )
{
	if ( !Q_stricmp( "if", token ) )
	{
		// fixme: add expression evaluation
		GetToken( false );
		if (atoi( token ) == 0 && stricmp( token, "true" ) != 0)
		{
			GetToken(true);
			if (token[0] == '{')
			{
				int depth = 1;
				while (TokenAvailable() && depth > 0)
				{
					GetToken( true );
					if (stricmp("{", token ) == 0)
					{
						depth++;
					}
					else if (stricmp("}", token ) == 0)
					{
						depth--;
					}
				}
			}
		}
		return true;
	}

	if ( !Q_stricmp( "fps", token ) )
	{
		GetToken( false );
		panim->fps = verify_atof( token );
		if ( panim->fps <= 0.0f )
		{
			TokenError( "ParseAnimationToken:  fps (%f from '%s') <= 0.0\n", panim->fps, token );
		}
		return true;
	}
	
	if ( !Q_stricmp( "origin", token ) )
	{
		GetToken (false);
		panim->adjust.x = verify_atof (token);

		GetToken (false);
		panim->adjust.y = verify_atof (token);

		GetToken (false);
		panim->adjust.z = verify_atof (token);
		return true;
	}
	
	if ( !Q_stricmp( "rotate", token ) )
	{
		GetToken( false );
		// FIXME: broken for Maya
		panim->rotation.z = DEG2RAD( verify_atof( token ) + 90 );
		return true;
	}
	
	if ( !Q_stricmp( "angles", token ) )
	{
		GetToken( false );
		panim->rotation.x = DEG2RAD( verify_atof( token ) );
		GetToken( false );
		panim->rotation.y = DEG2RAD( verify_atof( token ) );
		GetToken( false );
		panim->rotation.z = DEG2RAD( verify_atof( token ) + 90.0f);
		return true;
	}
	
	if ( !Q_stricmp( "scale", token ) )
	{
		GetToken( false );
		panim->scale = verify_atof( token );
		return true;
	}
	
	if ( !Q_strnicmp( "loop", token, 4 ) )
	{
		panim->flags |= STUDIO_LOOPING;
		return true;
	}
	
	if ( !Q_strnicmp( "startloop", token, 5 ) )
	{
		GetToken( false );
		panim->looprestart = verify_atoi( token );
		panim->flags |= STUDIO_LOOPING;
		return true;
	}
	
	if ( !Q_stricmp( "fudgeloop", token ) )
	{
		panim->fudgeloop = true;
		panim->flags |= STUDIO_LOOPING;
		return true;
	}
	
	if ( !Q_strnicmp( "snap", token, 4 ) )
	{
		panim->flags |= STUDIO_SNAP;
		return true;
	}
	
	if ( !Q_strnicmp( "frame", token, 5 ) )
	{
		GetToken( false );
		panim->startframe = verify_atoi( token );
		GetToken( false );
		panim->endframe = verify_atoi( token );

		// NOTE: This always affects the first source anim read in
		s_sourceanim_t *pSourceAnim = FindSourceAnim( panim->source, panim->animationname );
		if ( pSourceAnim )
		{
			if ( panim->startframe < pSourceAnim->startframe )
			{
				panim->startframe = pSourceAnim->startframe;
			}

			if ( panim->endframe > pSourceAnim->endframe )
			{
				panim->endframe = pSourceAnim->endframe;
			}
		}

		if ( !g_bCreateMakefile && panim->endframe < panim->startframe )
		{
			TokenError( "end frame before start frame in %s", panim->name );
		}

		panim->numframes = panim->endframe - panim->startframe + 1;

		return true;
	}

	if ( !Q_stricmp( "blockname", token ) )
	{
		GetToken( false );
		s_sourceanim_t *pSourceAnim = FindSourceAnim( panim->source, token );

		// NOTE: This always affects the first source anim read in
		if ( pSourceAnim )
		{
			panim->startframe = pSourceAnim->startframe;
			panim->endframe = pSourceAnim->endframe;

			if ( !g_bCreateMakefile && panim->endframe < panim->startframe )
			{
				TokenError( "end frame before start frame in %s", panim->name );
			}

			panim->numframes = panim->endframe - panim->startframe + 1;
			Q_strncpy( panim->animationname, token, sizeof(panim->animationname) );
		}
		else
		{
			MdlError( "Requested unknown animation block name %s\n", token );
		}
		return true;
	}

	if ( !Q_stricmp( "post", token ) )
	{
		panim->flags |= STUDIO_POST;
		return true;
	}
	
	if ( !Q_stricmp( "noautoik", token ) )
	{
		panim->noAutoIK = true;
		return true;
	}
	
	if ( !Q_stricmp( "autoik", token ) )
	{
		panim->noAutoIK = false;
		return true;
	}
	
	if ( ParseCmdlistToken( panim->numcmds, panim->cmds ) )
		return true;

	if ( !Q_stricmp( "cmdlist", token ) )
	{
		GetToken( false ); // A

		int i;
		for ( i = 0; i < g_numcmdlists; i++)
		{
			if (stricmp( g_cmdlist[i].name, token) == 0)
			{
				break;
			}
		}
		if (i == g_numcmdlists)
			TokenError( "unknown cmdlist %s\n", token );

		for (int j = 0; j < g_cmdlist[i].numcmds; j++)
		{
			if (panim->numcmds >= MAXSTUDIOCMDS)
			{
				TokenError("Too many cmds in %s\n", panim->name );
			}
			panim->cmds[panim->numcmds++] = g_cmdlist[i].cmds[j];
		}
		return true;
	}

	if ( !Q_stricmp( "motionrollback", token ) )
	{
		GetToken( false );
		panim->motionrollback = atof( token );
		return true;
	}

	if ( !Q_stricmp( "noanimblock", token ) )
	{
		panim->disableAnimblocks = true;
		return true;
	}

	if ( !Q_stricmp( "noanimblockstall", token ) )
	{
		panim->isFirstSectionLocal = true;
		return true;
	}

	if ( lookupControl( token ) != -1 )
	{
		panim->motiontype |= lookupControl( token );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: create named order dependant s_animcmd_t blocks, used as replicated token list for $animations
//-----------------------------------------------------------------------------

void Cmd_Cmdlist( )
{
	int depth = 0;

	// name
	GetToken(false);
	V_strcpy_safe( g_cmdlist[g_numcmdlists].name, token );

	while (1)
	{
		if (depth > 0)
		{
			if(!GetToken(true)) 
			{
				break;
			}
		}
		else
		{
			if (!TokenAvailable()) 
			{
				break;
			}
			else 
			{
				GetToken (false);
			}
		}

		if (endofscript)
		{
			if (depth != 0)
			{
				TokenError("missing }\n" );
			}
			return;
		}
		if (stricmp("{", token ) == 0)
		{
			depth++;
		}
		else if (stricmp("}", token ) == 0)
		{
			depth--;
		}
		else if (ParseCmdlistToken( g_cmdlist[g_numcmdlists].numcmds, g_cmdlist[g_numcmdlists].cmds ))
		{

		}
		else
		{
			TokenError( "unknown command: %s\n", token );
		}

		if (depth < 0)
		{
			TokenError("missing {\n");
		}
	};

	g_numcmdlists++;
}

int ParseAnimation( s_animation_t *panim, bool isAppend );
int ParseEmpty( void );


//-----------------------------------------------------------------------------
// Purpose: allocate an entry for $animation
//-----------------------------------------------------------------------------
void Cmd_Animation( )
{
	// name
	GetToken(false);

	s_animation_t *panim = LookupAnimation( token );

	if (panim != NULL)
	{
		if (!panim->isOverride)
		{
			TokenError( "Duplicate animation name \"%s\"\n", token );
		}
		else
		{
			panim->doesOverride = true;
			ParseEmpty();
			return;
		}
	}

	// allocate animation entry
	g_panimation[g_numani] = (s_animation_t *)kalloc( 1, sizeof( s_animation_t ) );
	g_panimation[g_numani]->index = g_numani;
	panim = g_panimation[g_numani];
	V_strcpy_safe( panim->name, token );
	g_numani++;

	// filename
	GetToken(false);
	V_strcpy_safe( panim->filename, token );

	panim->source = Load_Source( panim->filename, "" );
	if ( panim->source->m_Animations.Count() )
	{
		s_sourceanim_t *pSourceAnim = &panim->source->m_Animations[0];
		panim->startframe = pSourceAnim->startframe;
		panim->endframe = pSourceAnim->endframe;
		Q_strncpy( panim->animationname, pSourceAnim->animationname, sizeof(panim->animationname) );
	}
	else
	{
		panim->startframe = 0;
		panim->endframe = 0;
		Q_strncpy( panim->animationname, "", sizeof(panim->animationname) );
	}
	VectorCopy( g_defaultadjust, panim->adjust );
	panim->rotation = g_defaultrotation;
	panim->scale = 1.0f;
	panim->fps = 30.0;
	panim->motionrollback = g_flDefaultMotionRollback;

	ParseAnimation( panim, false );

	panim->numframes = panim->endframe - panim->startframe + 1;

	//CheckAutoShareAnimationGroup( panim->name );
}

//-----------------------------------------------------------------------------
// Purpose: wrapper for parsing $animation tokens
//-----------------------------------------------------------------------------

int ParseAnimation( s_animation_t *panim, bool isAppend )
{
	int depth = 0;

	while (1)
	{
		if (depth > 0)
		{
			if(!GetToken(true)) 
			{
				break;
			}
		}
		else
		{
			if (!TokenAvailable()) 
			{
				break;
			}
			else 
			{
				GetToken (false);
			}
		}

		if (endofscript)
		{
			if (depth != 0)
			{
				TokenError("missing }\n" );
			}
			return 1;
		}
		if (stricmp("{", token ) == 0)
		{
			depth++;
		}
		else if (stricmp("}", token ) == 0)
		{
			depth--;
		}
		else if (ParseAnimationToken( panim ))
		{

		}
		else
		{
			TokenError( "Unknown animation option\'%s\'\n", token );
		}

		if (depth < 0)
		{
			TokenError("missing {\n");
		}
	};

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: create a virtual $animation command from a $sequence reference
//-----------------------------------------------------------------------------
s_animation_t *Cmd_ImpliedAnimation( s_sequence_t *psequence, const char *filename )
{
	// allocate animation entry
	g_panimation[g_numani] = (s_animation_t *)kalloc( 1, sizeof( s_animation_t ) );
	g_panimation[g_numani]->index = g_numani;
	s_animation_t *panim = g_panimation[g_numani];
	g_numani++;

	panim->isImplied = true;

	panim->startframe = 0;
	panim->endframe = MAXSTUDIOANIMFRAMES - 1;

	V_strcpy_safe( panim->name, "@" );
	V_strcat_safe( panim->name, psequence->name );
	V_strcpy_safe( panim->filename, filename );

	VectorCopy( g_defaultadjust, panim->adjust );
	panim->scale = 1.0f;
	panim->rotation = g_defaultrotation;
	panim->fps = 30;
	panim->motionrollback = g_flDefaultMotionRollback;

	//panim->source = Load_Source( panim->filename, "smd" );
	panim->source = Load_Source( panim->filename, "" );
	if ( panim->source->m_Animations.Count() )
	{
		s_sourceanim_t *pSourceAnim = &panim->source->m_Animations[0];
		Q_strncpy( panim->animationname, panim->source->m_Animations[0].animationname, sizeof(panim->animationname) );
		if ( panim->startframe < pSourceAnim->startframe )
		{
			panim->startframe = pSourceAnim->startframe;
		}
		
		if ( panim->endframe > pSourceAnim->endframe )
		{
			panim->endframe = pSourceAnim->endframe;
		}
	}
	else
	{
		Q_strncpy( panim->animationname, "", sizeof( panim->animationname ) );
	}

	if ( !g_bCreateMakefile && panim->endframe < panim->startframe )
	{
		TokenError( "end frame before start frame in %s", panim->name );
	}

	panim->numframes = panim->endframe - panim->startframe + 1;

	//CheckAutoShareAnimationGroup( panim->name );

	return panim;
}


//-----------------------------------------------------------------------------
// Purpose: copy globally reavent $animation options from one $animation to another
//-----------------------------------------------------------------------------

void CopyAnimationSettings( s_animation_t *pdest, s_animation_t *psrc )
{
	pdest->fps = psrc->fps;

	VectorCopy( psrc->adjust, pdest->adjust );
	pdest->scale = psrc->scale;
	pdest->rotation = psrc->rotation;

	pdest->motiontype = psrc->motiontype;

	//Adrian - Hey! Revisit me later.
	/*if (pdest->startframe < psrc->startframe)
		pdest->startframe = psrc->startframe;
	
	if (pdest->endframe > psrc->endframe)
		pdest->endframe = psrc->endframe;
	
	if (pdest->endframe < pdest->startframe)
		TokenError( "fixedup end frame before start frame in %s", pdest->name );

	pdest->numframes = pdest->endframe - pdest->startframe + 1;*/

	for (int i = 0; i < psrc->numcmds; i++)
	{
		if (pdest->numcmds >= MAXSTUDIOCMDS)
		{
			TokenError("Too many cmds in %s\n", pdest->name );
		}
		pdest->cmds[pdest->numcmds++] = psrc->cmds[i];
	}
}

int ParseSequence( s_sequence_t *pseq, bool isAppend );


//-----------------------------------------------------------------------------
// Purpose: allocate an entry for $sequence
//-----------------------------------------------------------------------------
s_sequence_t *ProcessCmdSequence( const char *pSequenceName )
{
	s_animation_t *panim = LookupAnimation( pSequenceName );

	// allocate sequence
	if ( panim != NULL )
	{
		if ( !panim->isOverride )
		{
			TokenError( "Duplicate sequence name \"%s\"\n", pSequenceName );
		}
		else
		{
			panim->doesOverride = true;
			return NULL;
		}
	}

	if ( g_sequence.Count() >= MAXSTUDIOSEQUENCES )
	{
		TokenError("Too many sequences (%d max)\n", MAXSTUDIOSEQUENCES );
	}

	s_sequence_t *pseq = &g_sequence[ g_sequence.AddToTail() ];
	memset( pseq, 0, sizeof( s_sequence_t ) );

	// initialize sequence
	Q_strncpy( pseq->name, pSequenceName, sizeof(pseq->name) );

	pseq->actweight = 0;
	pseq->activityname[0] = '\0';
	pseq->activity = -1; // -1 is the default for 'no activity'

	pseq->paramindex[0] = -1;
	pseq->paramindex[1] = -1;

	pseq->groupsize[0] = 0;
	pseq->groupsize[1] = 0;

	pseq->fadeintime = 0.2;
	pseq->fadeouttime = 0.2;
	return pseq;
}


//-----------------------------------------------------------------------------
// Process the sequence command
//-----------------------------------------------------------------------------
void Cmd_Sequence( )
{
	if ( !GetToken(false) ) 
		return;

	// Find existing sequences
	const char *pSequenceName = token;
	s_animation_t *panim = LookupAnimation( pSequenceName );
	if ( panim != NULL && panim->isOverride )
	{
		ParseEmpty( );
	}

	s_sequence_t *pseq = ProcessCmdSequence( pSequenceName );
	if ( pseq )
	{
		ParseSequence( pseq, false );
	}
}


//-----------------------------------------------------------------------------
// Performs processing on a sequence
//-----------------------------------------------------------------------------
void ProcessSequence( s_sequence_t *pseq, int numblends, s_animation_t **animations, bool isAppend )
{
	if (isAppend)
		return;

	if ( numblends == 0 )
	{
		TokenError("no animations found\n");
	}

	if ( pseq->groupsize[0] == 0 )
	{
		if (numblends < 4)
		{
			pseq->groupsize[0] = numblends;
			pseq->groupsize[1] = 1;
		}
		else
		{
			int i = sqrt( (float) numblends );
			if (i * i == numblends)
			{
				pseq->groupsize[0] = i;
				pseq->groupsize[1] = i;
			}
			else
			{
				TokenError( "non-square (%d) number of blends without \"blendwidth\" set\n", numblends );
			}
		}
	}
	else
	{
		pseq->groupsize[1] = numblends / pseq->groupsize[0];

		if (pseq->groupsize[0] * pseq->groupsize[1] != numblends)
		{
			TokenError( "missing animation blends. Expected %d, found %d\n", 
				pseq->groupsize[0] * pseq->groupsize[1], numblends );
		}
	}

	for (int i = 0; i < numblends; i++)
	{
		int j = i % pseq->groupsize[0];
		int k = i / pseq->groupsize[0];

		pseq->panim[j][k] = animations[i];

		if (i > 0 && animations[i]->isImplied)
		{
			CopyAnimationSettings( animations[i], animations[0] );
		}
		animations[i]->isImplied = false; // don't copy any more commands
		pseq->flags |= animations[i]->flags;
	}

	pseq->numblends = numblends;
}

//-----------------------------------------------------------------------------
// Purpose: parse options unique to $sequence
//-----------------------------------------------------------------------------
int ParseSequence( s_sequence_t *pseq, bool isAppend )
{
	int depth = 0;
	s_animation_t *animations[64];
	int i, j, n;
	int numblends = 0;

	if (isAppend)
	{
		animations[0] = pseq->panim[0][0];
	}

	while (1)
	{
		if (depth > 0)
		{
			if(!GetToken(true)) 
			{
				break;
			}
		}
		else
		{
			if (!TokenAvailable()) 
			{
				break;
			}
			else 
			{
				GetToken (false);
			}
		}

		if (endofscript)
		{
			if (depth != 0)
			{
				TokenError("missing }\n" );
			}
			return 1;
		}
		if (stricmp("{", token ) == 0)
		{
			depth++;
		}
		else if (stricmp("}", token ) == 0)
		{
			depth--;
		}
		/*
		else if (stricmp("deform", token ) == 0)
		{
			Option_Deform( pseq );
		}
		*/

		else if (stricmp("event", token ) == 0)
		{
			depth -= Option_Event( pseq );
		}
		else if (stricmp("activity", token ) == 0)
		{
			Option_Activity( pseq );
		}
		else if (stricmp("activitymodifier", token ) == 0)
		{
			Option_ActivityModifier( pseq );
		}
		else if (strnicmp( token, "ACT_", 4 ) == 0)
		{
			UnGetToken( );
			Option_Activity( pseq );
		}

		else if (stricmp("snap", token ) == 0)
		{
			pseq->flags |= STUDIO_SNAP;
		}

		else if (stricmp("blendwidth", token ) == 0)
		{
			GetToken( false );
			pseq->groupsize[0] = verify_atoi( token );
		}

		else if (stricmp("blend", token ) == 0)
		{
			i = 0;
			if (pseq->paramindex[0] != -1)
			{
				i = 1;
			}

			GetToken( false );
			j = LookupPoseParameter( token );
			pseq->paramindex[i] = j;
			pseq->paramattachment[i] = -1;
			GetToken( false );
			pseq->paramstart[i] = verify_atof( token );
			GetToken( false );
			pseq->paramend[i] = verify_atof( token );

			g_pose[j].min  = min( g_pose[j].min, pseq->paramstart[i] );
			g_pose[j].min  = min( g_pose[j].min, pseq->paramend[i] );
			g_pose[j].max  = max( g_pose[j].max, pseq->paramstart[i] );
			g_pose[j].max  = max( g_pose[j].max, pseq->paramend[i] );
		}
		else if (stricmp("calcblend", token ) == 0)
		{
			i = 0;
			if (pseq->paramindex[0] != -1)
			{
				i = 1;
			}

			GetToken( false );
			j = LookupPoseParameter( token );
			pseq->paramindex[i] = j;

			GetToken( false );
			pseq->paramattachment[i] = LookupAttachment( token );
			if (pseq->paramattachment[i] == -1)
			{
				TokenError( "Unknown calcblend attachment \"%s\"\n", token );
			}

			GetToken( false );
			pseq->paramcontrol[i] = lookupControl( token );
		}
		else if (stricmp("blendref", token ) == 0)
		{
			GetToken( false );
			pseq->paramanim = LookupAnimation( token );
			if (pseq->paramanim == NULL)
			{
				TokenError( "Unknown blendref animation \"%s\"\n", token );
			}
		}
		else if (stricmp("blendcomp", token ) == 0)
		{
			GetToken( false );
			pseq->paramcompanim = LookupAnimation( token );
			if (pseq->paramcompanim == NULL)
			{
				TokenError( "Unknown blendcomp animation \"%s\"\n", token );
			}
		}
		else if (stricmp("blendcenter", token ) == 0)
		{
			GetToken( false );
			pseq->paramcenter = LookupAnimation( token );
			if (pseq->paramcenter == NULL)
			{
				TokenError( "Unknown blendcenter animation \"%s\"\n", token );
			}
		}
		else if (stricmp("node", token ) == 0)
		{
			GetToken( false );
			pseq->entrynode = pseq->exitnode = LookupXNode( token );
		}
		else if (stricmp("transition", token ) == 0)
		{
			GetToken( false );
			pseq->entrynode = LookupXNode( token );
			GetToken( false );
			pseq->exitnode = LookupXNode( token );
		}
		else if (stricmp("rtransition", token ) == 0)
		{
			GetToken( false );
			pseq->entrynode = LookupXNode( token );
			GetToken( false );
			pseq->exitnode = LookupXNode( token );
			pseq->nodeflags |= 1;
		}
		else if (stricmp("exitphase", token ) == 0)
		{
			GetToken( false );
			pseq->exitphase = verify_atof( token );
		}
		else if (stricmp("delta", token) == 0)
		{
			pseq->flags |= STUDIO_DELTA;
			pseq->flags |= STUDIO_POST;
		}
		else if (stricmp("worldspace", token) == 0)
		{
			pseq->flags |= STUDIO_WORLD;
			pseq->flags |= STUDIO_POST;
		}
		else if (stricmp("post", token) == 0) // remove
		{
			pseq->flags |= STUDIO_POST; 
		}
		else if (stricmp("predelta", token) == 0)
		{
			pseq->flags |= STUDIO_DELTA;
		}
		else if (stricmp("autoplay", token) == 0)
		{
			pseq->flags |= STUDIO_AUTOPLAY;
		}
		else if (stricmp( "fadein", token ) == 0)
		{
			GetToken( false );
			pseq->fadeintime = verify_atof( token );
		}
		else if (stricmp( "fadeout", token ) == 0)
		{
			GetToken( false );
			pseq->fadeouttime = verify_atof( token );
		}
		else if (stricmp( "realtime", token ) == 0)
		{
			pseq->flags |= STUDIO_REALTIME;
		}
		else if (stricmp( "posecycle", token ) == 0)
		{
			pseq->flags |= STUDIO_CYCLEPOSE;

			GetToken( false );
			pseq->cycleposeindex = LookupPoseParameter( token );
		}
		else if (stricmp( "hidden", token ) == 0)
		{
			pseq->flags |= STUDIO_HIDDEN;
		}
		else if (stricmp( "addlayer", token ) == 0)
		{
			GetToken( false );
			V_strcpy_safe( pseq->autolayer[pseq->numautolayers].name, token );
			pseq->numautolayers++;
		}
		else if (stricmp( "iklock", token ) == 0)
		{
			GetToken(false);
			V_strcpy_safe( pseq->iklock[pseq->numiklocks].name, token );

			GetToken(false);
			pseq->iklock[pseq->numiklocks].flPosWeight = verify_atof( token );

			GetToken(false);
			pseq->iklock[pseq->numiklocks].flLocalQWeight = verify_atof( token );
		
			pseq->numiklocks++;
		}
		else if (stricmp( "keyvalues", token ) == 0)
		{
			Option_KeyValues( &pseq->KeyValue );
		}
		else if (stricmp( "blendlayer", token ) == 0)
		{
			pseq->autolayer[pseq->numautolayers].flags = 0;

			GetToken( false );
			V_strcpy_safe( pseq->autolayer[pseq->numautolayers].name, token );

			GetToken( false );
			pseq->autolayer[pseq->numautolayers].start = verify_atof( token );

			GetToken( false );
			pseq->autolayer[pseq->numautolayers].peak = verify_atof( token );

			GetToken( false );
			pseq->autolayer[pseq->numautolayers].tail = verify_atof( token );

			GetToken( false );
			pseq->autolayer[pseq->numautolayers].end = verify_atof( token );

			while (TokenAvailable( ))
			{
				GetToken( false );
				if (stricmp( "xfade", token ) == 0)
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_XFADE;
				}
				else if (stricmp( "spline", token ) == 0)
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_SPLINE;
				}
				else if (stricmp( "noblend", token ) == 0)
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_NOBLEND;
				}
				else if (stricmp( "poseparameter", token ) == 0)
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_POSE;
					GetToken( false );
					pseq->autolayer[pseq->numautolayers].pose = LookupPoseParameter( token );
				}
				else if (stricmp( "local", token ) == 0)
				{
					pseq->autolayer[pseq->numautolayers].flags |= STUDIO_AL_LOCAL;
					pseq->flags |= STUDIO_LOCAL;
				}
				else
				{
					UnGetToken();
					break;
				}
			}

			pseq->numautolayers++;
		}
		else if ((numblends || isAppend) && ParseAnimationToken( animations[0] ))
		{

		}
		else if (!isAppend)
		{
			// assume it's an animation reference
			// first look up an existing animation
			for (n = 0; n < g_numani; n++)
			{
				if (stricmp( token, g_panimation[n]->name ) == 0)
				{
					animations[numblends++] = g_panimation[n];
					break;
				}
			}

			if (n >= g_numani)
			{
				// assume it's an implied animation
				animations[numblends++] = Cmd_ImpliedAnimation( pseq, token );
			}
			// hack to allow animation commands to refer to same sequence
			if (numblends == 1)
			{
				pseq->panim[0][0] = animations[0];
			}

		}
		else
		{
			TokenError( "unknown command \"%s\"\n", token );
		}

		if (depth < 0)
		{
			TokenError("missing {\n");
		}
	}

	ProcessSequence( pseq, numblends, animations, isAppend );
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: throw away all the options for a specific sequence or animation
//-----------------------------------------------------------------------------

int ParseEmpty( )
{
	int depth = 0;

	while (1)
	{
		if (depth > 0)
		{
			if(!GetToken(true)) 
			{
				break;
			}
		}
		else
		{
			if (!TokenAvailable()) 
			{
				break;
			}
			else 
			{
				GetToken (false);
			}
		}

		if (endofscript)
		{
			if (depth != 0)
			{
				TokenError("missing }\n" );
			}
			return 1;
		}
		if (stricmp("{", token ) == 0)
		{
			depth++;
		}
		else if (stricmp("}", token ) == 0)
		{
			depth--;
		}

		if (depth < 0)
		{
			TokenError("missing {\n");
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: append commands to either a sequence or an animation
//-----------------------------------------------------------------------------
void Cmd_Append( )
{
	GetToken(false);


	s_sequence_t *pseq = LookupSequence( token );

	if (pseq)
	{
		ParseSequence( pseq, true );
		return;
	}
	else
	{
		s_animation_t *panim = LookupAnimation( token );

		if (panim)
		{
			ParseAnimation( panim, true );
			return;
		}
	}
	TokenError( "unknown append animation %s\n", token );
}



void Cmd_Prepend( )
{
	GetToken(false);

	s_sequence_t *pseq = LookupSequence( token );
	int count = 0;
	s_animation_t *panim = NULL;
	int iRet =  false;

	if (pseq)
	{
		panim = pseq->panim[0][0];
		count = panim->numcmds;
		iRet = ParseSequence( pseq, true );
	}
	else
	{
		panim = LookupAnimation( token );
		if (panim)
		{
			count = panim->numcmds;
			iRet = ParseAnimation( panim, true );
		}
	}
	if (panim && count != panim->numcmds)
	{
		s_animcmd_t tmp;
		tmp = panim->cmds[panim->numcmds - 1];
		int i;
		for (i = panim->numcmds - 1; i > 0; i--)
		{
			panim->cmds[i] = panim->cmds[i-1];
		}
		panim->cmds[0] = tmp;
		return;
	}
	TokenError( "unknown prepend animation \"%s\"\n", token );
}

void Cmd_Continue( )
{
	GetToken(false);

	s_sequence_t *pseq = LookupSequence( token );

	if (pseq)
	{
		GetToken(true);
		UnGetToken();
		if (token[0] != '$')
			ParseSequence( pseq, true );
		return;
	}
	else
	{
		s_animation_t *panim = LookupAnimation( token );

		if (panim)
		{
			GetToken(true);
			UnGetToken();
			if (token[0] != '$')
				ParseAnimation( panim, true );
			return;
		}
	}
	TokenError( "unknown continue animation %s\n", token );
}

//-----------------------------------------------------------------------------
// Purpose: foward declare an empty sequence
//-----------------------------------------------------------------------------

void Cmd_DeclareSequence( void )
{
	if (g_sequence.Count() >= MAXSTUDIOSEQUENCES)
	{
		TokenError("Too many sequences (%d max)\n", MAXSTUDIOSEQUENCES );
	}

	s_sequence_t *pseq = &g_sequence[ g_sequence.AddToTail() ];
	memset( pseq, 0, sizeof( s_sequence_t ) );
	pseq->flags = STUDIO_OVERRIDE;

	// initialize sequence
	GetToken( false );
	V_strcpy_safe( pseq->name, token );
}


//-----------------------------------------------------------------------------
// Purpose: foward declare an empty sequence
//-----------------------------------------------------------------------------
void Cmd_DeclareAnimation( void )
{
	if (g_numani >= MAXSTUDIOANIMS)
	{
		TokenError("Too many animations (%d max)\n", MAXSTUDIOANIMS );
	}

	// allocate animation entry
	s_animation_t *panim = (s_animation_t *)kalloc( 1, sizeof( s_animation_t ) );
	g_panimation[g_numani] = panim;
	panim->index = g_numani;
	panim->flags = STUDIO_OVERRIDE;
	g_numani++;
	
	// initialize animation
	GetToken( false );
	V_strcpy_safe( panim->name, token );
}


//-----------------------------------------------------------------------------
// Purpose: create named list of boneweights
//-----------------------------------------------------------------------------
void Option_Weightlist( s_weightlist_t *pweightlist )
{
	int depth = 0;
	int i;

	pweightlist->numbones = 0;

	while (1)
	{
		if (depth > 0)
		{
			if(!GetToken(true)) 
			{
				break;
			}
		}
		else
		{
			if (!TokenAvailable()) 
			{
				break;
			}
			else 
			{
				GetToken (false);
			}
		}

		if (endofscript)
		{
			if (depth != 0)
			{
				TokenError("missing }\n" );
			}
			return;
		}
		if (stricmp("{", token ) == 0)
		{
			depth++;
		}
		else if (stricmp("}", token ) == 0)
		{
			depth--;
		}
		else if (stricmp("posweight", token ) == 0)
		{
			i = pweightlist->numbones - 1;
			if (i < 0)
			{
				MdlError( "Error with specifing bone Position weight \'%s:%s\'\n", pweightlist->name, pweightlist->bonename[i] );
			}
			GetToken( false );
			pweightlist->boneposweight[i] = verify_atof( token );
			if (pweightlist->boneweight[i] == 0 && pweightlist->boneposweight[i] > 0)
			{
				MdlError( "Non-zero Position weight with zero Rotation weight not allowed \'%s:%s %f %f\'\n", 
					pweightlist->name, pweightlist->bonename[i], pweightlist->boneweight[i], pweightlist->boneposweight[i] );
			}
		}
		else
		{
			i = pweightlist->numbones++;
			if (i >= MAXWEIGHTSPERLIST)
			{
				TokenError("Too many bones (%d) in weightlist '%s'\n", i, pweightlist->name );
			}
			pweightlist->bonename[i] = strdup( token );
			GetToken( false );
			pweightlist->boneweight[i] = verify_atof( token );
			pweightlist->boneposweight[i] = pweightlist->boneweight[i];
		}

		if (depth < 0)
		{
			TokenError("missing {\n");
		}
	};
}


void Cmd_Weightlist( )
{
	int i;

	if (!GetToken(false)) 
		return;

	if (g_numweightlist >= MAXWEIGHTLISTS)
	{
		TokenError( "Too many weightlist commands (%d)\n", MAXWEIGHTLISTS );
	}

	for (i = 1; i < g_numweightlist; i++)
	{
		if (stricmp( g_weightlist[i].name, token ) == 0)
		{
			TokenError( "Duplicate weightlist '%s'\n", token );
		}
	}

	V_strcpy_safe( g_weightlist[i].name, token );

	Option_Weightlist( &g_weightlist[g_numweightlist] );

	g_numweightlist++;
}

void Cmd_DefaultWeightlist( )
{
	Option_Weightlist( &g_weightlist[0] );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Option_Eyeball( s_model_t *pmodel )
{
	Vector	tmp;
	int i, j;
	int mesh_material;
	char szMeshMaterial[256];

	s_eyeball_t *eyeball = &(pmodel->eyeball[pmodel->numeyeballs++]);

	// name
	GetToken (false);
	V_strcpy_safe( eyeball->name, token );

	// bone name
	GetToken (false);
	for (i = 0; i < pmodel->source->numbones; i++)
	{
		if ( !Q_stricmp( pmodel->source->localBone[i].name, token ) )
		{
			eyeball->bone = i;
			break;
		}
	}
	if (!g_bCreateMakefile && i >= pmodel->source->numbones)
	{
		TokenError( "unknown eyeball bone \"%s\"\n", token );
	}

	// X
	GetToken (false);
	tmp[0] = verify_atof (token);

	// Y
	GetToken (false);
	tmp[1] = verify_atof (token);

	// Z
	GetToken (false);
	tmp[2] = verify_atof (token);

	// mesh material 
	GetToken (false);
	Q_strncpy( szMeshMaterial, token, sizeof(szMeshMaterial) );
	mesh_material = UseTextureAsMaterial( LookupTexture( token ) );

	// diameter
	GetToken (false);
	eyeball->radius = verify_atof (token) / 2.0;

	// Z angle offset
	GetToken (false);
	eyeball->zoffset = tan( DEG2RAD( verify_atof (token) ) );

	// iris material (no longer used, but we need to remove the token)
	GetToken (false);

	// pupil scale
	GetToken (false);
	eyeball->iris_scale = 1.0 / verify_atof( token );
	
	VectorCopy( tmp, eyeball->org );

	for (i = 0; i < pmodel->source->nummeshes; i++)
	{
		j = pmodel->source->meshindex[i]; // meshes are internally stored by material index

		if (j == mesh_material)
		{
			eyeball->mesh = i; // FIXME: should this be pre-adjusted?
			break;
		}
	}

	if (!g_bCreateMakefile && i >= pmodel->source->nummeshes)
	{
		TokenError("can't find eyeball texture \"%s\" on model\n", szMeshMaterial );
	}
	
	// translate eyeball into bone space
	VectorITransform( tmp, pmodel->source->boneToPose[eyeball->bone], eyeball->org );

	matrix3x4_t vtmp;
	AngleMatrix( g_defaultrotation, vtmp );

	VectorIRotate( Vector( 0, 0, 1 ), vtmp, tmp );
	VectorIRotate( tmp, pmodel->source->boneToPose[eyeball->bone], eyeball->up );

	VectorIRotate( Vector( 1, 0, 0 ), vtmp, tmp );
	VectorIRotate( tmp, pmodel->source->boneToPose[eyeball->bone], eyeball->forward );

	// these get overwritten by "eyelid" data
	eyeball->upperlidflexdesc = -1;
	eyeball->lowerlidflexdesc = -1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Option_Spherenormals( s_source_t *psource )
{
	Vector	pos;
	int i, j;
	int mesh_material;
	char szMeshMaterial[256];

	// mesh material 
	GetToken (false);
	V_strcpy_safe( szMeshMaterial, token );
	mesh_material = UseTextureAsMaterial( LookupTexture( token ) );

	// X
	GetToken (false);
	pos[0] = verify_atof (token);

	// Y
	GetToken (false);
	pos[1] = verify_atof (token);

	// Z
	GetToken (false);
	pos[2] = verify_atof (token);

	for (i = 0; i < psource->nummeshes; i++)
	{
		j = psource->meshindex[i]; // meshes are internally stored by material index

		if (j == mesh_material)
		{
			s_vertexinfo_t *vertex = &psource->vertex[psource->mesh[i].vertexoffset];

			for (int k = 0; k < psource->mesh[i].numvertices; k++)
			{
				Vector n = vertex[k].position - pos;
				VectorNormalize( n );
				if (DotProduct( n, vertex[k].normal ) < 0.0)
				{
					vertex[k].normal = -1 * n;
				}
				else
				{
					vertex[k].normal = n;
				}
#if 0
				vertex[k].normal[0] += 0.5f * ( 2.0f * ( ( float )rand() ) / ( float )VALVE_RAND_MAX ) - 1.0f;
				vertex[k].normal[1] += 0.5f * ( 2.0f * ( ( float )rand() ) / ( float )VALVE_RAND_MAX ) - 1.0f;
				vertex[k].normal[2] += 0.5f * ( 2.0f * ( ( float )rand() ) / ( float )VALVE_RAND_MAX ) - 1.0f;
				VectorNormalize( vertex[k].normal );
#endif
			}
			break;
		}
	}

	if (i >= psource->nummeshes)
	{
		TokenError("can't find spherenormal texture \"%s\" on model\n", szMeshMaterial );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int Add_Flexdesc( const char *name )
{
	int flexdesc;
	for ( flexdesc = 0; flexdesc < g_numflexdesc; flexdesc++)
	{
		if (stricmp( name, g_flexdesc[flexdesc].FACS ) == 0)
		{
			break;
		}
	}

	if (flexdesc >= MAXSTUDIOFLEXDESC)
	{
		TokenError( "Too many flex types, max %d\n", MAXSTUDIOFLEXDESC );
	}

	if (flexdesc == g_numflexdesc)
	{
		V_strcpy_safe( g_flexdesc[flexdesc].FACS, name );

		g_numflexdesc++;
	}
	return flexdesc;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Option_Flex( char *name, char *vtafile, int imodel, float pairsplit )
{
	if (g_numflexkeys >= MAXSTUDIOFLEXKEYS)
	{
		TokenError( "Too many flexes, max %d\n", MAXSTUDIOFLEXKEYS );
	}

	int flexdesc, flexpair;
	
	if (pairsplit != 0)
	{
		char mod[256];
		sprintf( mod, "%sR", name );
		flexdesc = Add_Flexdesc( mod );

		sprintf( mod, "%sL", name );
		flexpair = Add_Flexdesc( mod );
	}
	else
	{
		flexdesc = Add_Flexdesc( name );
		flexpair = 0;
	}

	// initialize
	g_flexkey[g_numflexkeys].imodel = imodel;
	g_flexkey[g_numflexkeys].flexdesc = flexdesc;
	g_flexkey[g_numflexkeys].target0 = 0.0;
	g_flexkey[g_numflexkeys].target1 = 1.0;
	g_flexkey[g_numflexkeys].target2 = 10;
	g_flexkey[g_numflexkeys].target3 = 11;
	g_flexkey[g_numflexkeys].split = pairsplit;
	g_flexkey[g_numflexkeys].flexpair = flexpair;
	g_flexkey[g_numflexkeys].decay = 1.0;

	while (TokenAvailable())
	{
		GetToken(false);

		if (stricmp( token, "frame") == 0)
		{
			GetToken (false);

			g_flexkey[g_numflexkeys].frame = verify_atoi( token );
		}
		else if (stricmp( token, "position") == 0)
		{
			GetToken (false);
			g_flexkey[g_numflexkeys].target1 = verify_atof( token );
		}
		else if (stricmp( token, "split") == 0)
		{
			GetToken (false);
			g_flexkey[g_numflexkeys].split = verify_atof( token );
		}
		else if (stricmp( token, "decay") == 0)
		{
			GetToken (false);
			g_flexkey[g_numflexkeys].decay = verify_atof( token );
		}
		else
		{
			TokenError( "unknown option: %s", token );
		}

	}

	if (g_numflexkeys > 1)
	{
		if (g_flexkey[g_numflexkeys-1].flexdesc == g_flexkey[g_numflexkeys].flexdesc)
		{
			g_flexkey[g_numflexkeys-1].target2 = g_flexkey[g_numflexkeys-1].target1;
			g_flexkey[g_numflexkeys-1].target3 = g_flexkey[g_numflexkeys].target1;
			g_flexkey[g_numflexkeys].target0 = g_flexkey[g_numflexkeys-1].target1;
		}
	}

	// link to source
	s_source_t *pSource = Load_Source( vtafile, "vta" );
	g_flexkey[g_numflexkeys].source = pSource;
	if ( pSource->m_Animations.Count() )
	{
		Q_strncpy( g_flexkey[g_numflexkeys].animationname, pSource->m_Animations[0].animationname, sizeof( g_flexkey[g_numflexkeys].animationname ) );
	}
	else
	{
		g_flexkey[g_numflexkeys].animationname[0] = 0;
	}
	g_numflexkeys++;
	// this needs to be per model.
}


//-----------------------------------------------------------------------------
// Adds combination data to the source
//-----------------------------------------------------------------------------
int FindSourceFlexKey( s_source_t *pSource, const char *pName )
{
	int nCount = pSource->m_FlexKeys.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pSource->m_FlexKeys[i].animationname, pName ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Adds flexkey data to a particular source
//-----------------------------------------------------------------------------
void AddFlexKey( s_source_t *pSource, CDmeCombinationOperator *pComboOp, const char *pFlexKeyName )
{
	// See if the delta state is already accounted for
	if ( FindSourceFlexKey( pSource, pFlexKeyName ) >= 0 )
		return;

	int i = pSource->m_FlexKeys.AddToTail();

	s_flexkey_t &key = pSource->m_FlexKeys[i];
	memset( &key, 0, sizeof(key) );

	key.target0 = 0.0f;
	key.target1 = 1.0f;
	key.target2 = 10.0f;
	key.target3 = 11.0f;
	key.decay = 1.0f;
	key.source = pSource;

	Q_strncpy( key.animationname, pFlexKeyName, sizeof(key.animationname) );
	key.flexpair = pComboOp->IsDeltaStateStereo( pFlexKeyName );	// Signal used by AddBodyFlexData
}


//-----------------------------------------------------------------------------
// Adds combination data to the source
//-----------------------------------------------------------------------------
void AddCombination( s_source_t *pSource, CDmeCombinationOperator *pCombination )
{
	// Define the remapped controls
	int nControlCount = pCombination->GetRawControlCount();
	for ( int i = 0; i < nControlCount; ++i )
	{
		int m = pSource->m_CombinationControls.AddToTail();
		s_combinationcontrol_t &control = pSource->m_CombinationControls[m];
		Q_strncpy( control.name, pCombination->GetRawControlName( i ), sizeof(control.name) );
	}

	// Define the combination + domination rules
	int nTargetCount = pCombination->GetOperationTargetCount();
	for ( int i = 0; i < nTargetCount; ++i )
	{
		int nOpCount = pCombination->GetOperationCount( i );
		for ( int j = 0; j < nOpCount; ++j )
		{
			CDmElement *pDeltaState = pCombination->GetOperationDeltaState( i, j );
			if ( !pDeltaState )
				continue;

			int nFlex = FindSourceFlexKey( pSource, pDeltaState->GetName() );
			if ( nFlex < 0 )
				continue;

			int k = pSource->m_CombinationRules.AddToTail();
			s_combinationrule_t &rule = pSource->m_CombinationRules[k];
			rule.m_nFlex = nFlex;
			rule.m_Combination = pCombination->GetOperationControls( i, j );
			int nDominatorCount = pCombination->GetOperationDominatorCount( i, j );
			for ( int l = 0; l < nDominatorCount; ++l )
			{
				int m = rule.m_Dominators.AddToTail();
				rule.m_Dominators[m] = pCombination->GetOperationDominator( i, j, l );
			}
		}
	}

	// Define the remapping controls
	nControlCount = pCombination->GetControlCount();
	for ( int i = 0; i < nControlCount; ++i )
	{
		int k = pSource->m_FlexControllerRemaps.AddToTail();
		s_flexcontrollerremap_t &remap = pSource->m_FlexControllerRemaps[k];
		remap.m_Name = pCombination->GetControlName( i );
		remap.m_bIsStereo = pCombination->IsStereoControl( i );
		remap.m_Index = -1;			// Don't know this right now
		remap.m_LeftIndex = -1;		// Don't know this right now
		remap.m_RightIndex = -1;	// Don't know this right now
		remap.m_MultiIndex = -1;	// Don't know this right now
		remap.m_EyesUpDownFlexController = -1;
		remap.m_BlinkController = -1;

		int nRemapCount = pCombination->GetRawControlCount( i );
		if ( pCombination->IsEyelidControl( i ) )
		{
			remap.m_RemapType = FLEXCONTROLLER_REMAP_EYELID;

			// Save the eyes_updown flex for later
			const char *pEyesUpDownFlexName = pCombination->GetEyesUpDownFlexName( i );
			remap.m_EyesUpDownFlexName = pEyesUpDownFlexName ? pEyesUpDownFlexName : "eyes_updown";
		}
		else
		{
			switch( nRemapCount )
			{
			case 0:
			case 1:
				remap.m_RemapType = FLEXCONTROLLER_REMAP_PASSTHRU;
				break;
			case 2:
				remap.m_RemapType = FLEXCONTROLLER_REMAP_2WAY;
				break;
			default:
				remap.m_RemapType = FLEXCONTROLLER_REMAP_NWAY;
				break;
			}
		}

		Assert( nRemapCount != 0 );
		for ( int j = 0; j < nRemapCount; ++j )
		{
			const char *pRemapName = pCombination->GetRawControlName( i, j );
			remap.m_RawControls.AddToTail( pRemapName );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Option_Eyelid( int imodel )
{
	char type[256];
	char vtafile[256];

	// type
	GetToken (false);
	V_strcpy_safe( type, token );

	// source
	GetToken (false);
	V_strcpy_safe( vtafile, token );

	int lowererframe = 0;
	int neutralframe = 0;
	int raiserframe = 0;
	float lowerertarget = 0.0f;
	float neutraltarget = 0.0f;
	float raisertarget = 0.0f;
	int lowererdesc = 0;
	int neutraldesc = 0;
	int raiserdesc = 0;
	int basedesc;
	float split = 0;
	char szEyeball[64] = {""};

	basedesc = g_numflexdesc;
	V_strcpy_safe( g_flexdesc[g_numflexdesc++].FACS, type );

	while (TokenAvailable())
	{
		GetToken(false);

		char localdesc[256];
		V_strcpy_safe( localdesc, type );
		V_strcat_safe( localdesc, "_" );
		V_strcat_safe( localdesc, token );

		if (stricmp( token, "lowerer") == 0)
		{
			GetToken (false);
			lowererframe = verify_atoi( token );
			GetToken (false);
			lowerertarget = verify_atof( token );
			lowererdesc = g_numflexdesc;
			V_strcpy_safe( g_flexdesc[g_numflexdesc++].FACS, localdesc );
		}
		else if (stricmp( token, "neutral") == 0)
		{
			GetToken (false);
			neutralframe = verify_atoi( token );
			GetToken (false);
			neutraltarget = verify_atof( token );
			neutraldesc = g_numflexdesc;
			V_strcpy_safe( g_flexdesc[g_numflexdesc++].FACS, localdesc );
		}
		else if (stricmp( token, "raiser") == 0)
		{
			GetToken (false);
			raiserframe = verify_atoi( token );
			GetToken (false);
			raisertarget = verify_atof( token );
			raiserdesc = g_numflexdesc;
			V_strcpy_safe( g_flexdesc[g_numflexdesc++].FACS, localdesc );
		}
		else if (stricmp( token, "split") == 0)
		{
			GetToken (false);
			split = verify_atof( token );
		}
		else if (stricmp( token, "eyeball") == 0)
		{
			GetToken (false);
			V_strcpy_safe( szEyeball, token );
		}
		else
		{
			TokenError( "unknown option: %s", token );
		}
	}

	s_source_t *pSource = Load_Source( vtafile, "vta" );
	g_flexkey[g_numflexkeys+0].source = pSource;
	g_flexkey[g_numflexkeys+0].frame = lowererframe;
	g_flexkey[g_numflexkeys+0].flexdesc = basedesc;
	g_flexkey[g_numflexkeys+0].imodel = imodel;
	g_flexkey[g_numflexkeys+0].split = split;
	g_flexkey[g_numflexkeys+0].target0 = -11;
	g_flexkey[g_numflexkeys+0].target1 = -10;
	g_flexkey[g_numflexkeys+0].target2 = lowerertarget;
	g_flexkey[g_numflexkeys+0].target3 = neutraltarget;
	g_flexkey[g_numflexkeys+0].decay = 0.0;
	if ( pSource->m_Animations.Count() > 0 )
	{
		Q_strncpy( g_flexkey[g_numflexkeys+0].animationname, pSource->m_Animations[0].animationname, sizeof(g_flexkey[g_numflexkeys+0].animationname) );
	}
	else
	{
		g_flexkey[g_numflexkeys+0].animationname[0] = 0;
	}

	g_flexkey[g_numflexkeys+1].source = g_flexkey[g_numflexkeys+0].source;
	Q_strncpy( g_flexkey[g_numflexkeys+1].animationname, g_flexkey[g_numflexkeys+0].animationname, sizeof(g_flexkey[g_numflexkeys+1].animationname) );
	g_flexkey[g_numflexkeys+1].frame = neutralframe;
	g_flexkey[g_numflexkeys+1].flexdesc = basedesc;
	g_flexkey[g_numflexkeys+1].imodel = imodel;
	g_flexkey[g_numflexkeys+1].split = split;
	g_flexkey[g_numflexkeys+1].target0 = lowerertarget;
	g_flexkey[g_numflexkeys+1].target1 = neutraltarget;
	g_flexkey[g_numflexkeys+1].target2 = neutraltarget;
	g_flexkey[g_numflexkeys+1].target3 = raisertarget;
	g_flexkey[g_numflexkeys+1].decay = 0.0;

	g_flexkey[g_numflexkeys+2].source = g_flexkey[g_numflexkeys+0].source;
	Q_strncpy( g_flexkey[g_numflexkeys+2].animationname, g_flexkey[g_numflexkeys+0].animationname, sizeof(g_flexkey[g_numflexkeys+2].animationname) );
	g_flexkey[g_numflexkeys+2].frame = raiserframe;
	g_flexkey[g_numflexkeys+2].flexdesc = basedesc;
	g_flexkey[g_numflexkeys+2].imodel = imodel;
	g_flexkey[g_numflexkeys+2].split = split;
	g_flexkey[g_numflexkeys+2].target0 = neutraltarget;
	g_flexkey[g_numflexkeys+2].target1 = raisertarget;
	g_flexkey[g_numflexkeys+2].target2 = 10;
	g_flexkey[g_numflexkeys+2].target3 = 11;
	g_flexkey[g_numflexkeys+2].decay = 0.0;
	g_numflexkeys += 3;

	s_model_t *pmodel = g_model[imodel];
	for (int i = 0; i < pmodel->numeyeballs; i++)
	{
		s_eyeball_t *peyeball = &(pmodel->eyeball[i]);

		if (szEyeball[0] != '\0')
		{
			if (stricmp( peyeball->name, szEyeball ) != 0)
				continue;
		}

		if (fabs( lowerertarget ) > peyeball->radius)
		{
			TokenError( "Eyelid \"%s\" lowerer out of range (+-%.1f)\n", type, peyeball->radius );
		}
		if (fabs( neutraltarget ) > peyeball->radius)
		{
			TokenError( "Eyelid \"%s\" neutral out of range (+-%.1f)\n", type, peyeball->radius );
		}
		if (fabs( raisertarget ) > peyeball->radius)
		{
			TokenError( "Eyelid \"%s\" raiser  out of range (+-%.1f)\n", type, peyeball->radius );
		}

		switch( type[0] )
		{
		case 'u':
			peyeball->upperlidflexdesc	= basedesc;
			peyeball->upperflexdesc[0]	= lowererdesc; 
			peyeball->uppertarget[0]	= lowerertarget;
			peyeball->upperflexdesc[1]	= neutraldesc; 
			peyeball->uppertarget[1]	= neutraltarget;
			peyeball->upperflexdesc[2]	= raiserdesc; 
			peyeball->uppertarget[2]	= raisertarget;
			break;
		case 'l':
			peyeball->lowerlidflexdesc	= basedesc;
			peyeball->lowerflexdesc[0]	= lowererdesc; 
			peyeball->lowertarget[0]	= lowerertarget;
			peyeball->lowerflexdesc[1]	= neutraldesc; 
			peyeball->lowertarget[1]	= neutraltarget;
			peyeball->lowerflexdesc[2]	= raiserdesc; 
			peyeball->lowertarget[2]	= raisertarget;
			break;
		}
	}
}

/*
=================
=================
*/
int Option_Mouth( s_model_t *pmodel )
{
	// index
	GetToken (false);
	int index = verify_atoi( token );
	if (index >= g_nummouths)
		g_nummouths = index + 1;

	// flex controller name
	GetToken (false);
	g_mouth[index].flexdesc = Add_Flexdesc( token );

	// bone name
	GetToken (false);
	V_strcpy_safe( g_mouth[index].bonename, token );

	// vector
	GetToken (false);
	g_mouth[index].forward[0] = verify_atof( token );
	GetToken (false);
	g_mouth[index].forward[1] = verify_atof( token );
	GetToken (false);
	g_mouth[index].forward[2] = verify_atof( token );
	return 0;
}



void Option_Flexcontroller( s_model_t *pmodel )
{
	char type[256];
	float range_min = 0.0f;
	float range_max = 1.0f;

	// g_flex
	GetToken (false);
	V_strcpy_safe( type, token );

	while (TokenAvailable())
	{
		GetToken(false);

		if (stricmp( token, "range") == 0)
		{
			GetToken(false);
			range_min = verify_atof( token );

			GetToken(false);
			range_max = verify_atof( token );
		}
		else
		{
			if (g_numflexcontrollers >= MAXSTUDIOFLEXCTRL)
			{
				TokenError( "Too many flex controllers, max %d\n", MAXSTUDIOFLEXCTRL );
			}

			V_strcpy_safe( g_flexcontroller[g_numflexcontrollers].name, token );
			V_strcpy_safe( g_flexcontroller[g_numflexcontrollers].type, type );
			g_flexcontroller[g_numflexcontrollers].min = range_min;
			g_flexcontroller[g_numflexcontrollers].max = range_max;
			g_numflexcontrollers++;
		}
	}

	// this needs to be per model.
}

void Option_Flexrule( s_model_t *pmodel, char *name )
{
	int precedence[32];
	precedence[ STUDIO_CONST ] = 	0;
	precedence[ STUDIO_FETCH1 ] =	0;
	precedence[ STUDIO_FETCH2 ] =	0;
	precedence[ STUDIO_ADD ] =		1;
	precedence[ STUDIO_SUB ] =		1;
	precedence[ STUDIO_MUL ] =		2;
	precedence[ STUDIO_DIV ] =		2;
	precedence[ STUDIO_NEG ] =		4;
	precedence[ STUDIO_EXP ] =		3;
	precedence[ STUDIO_OPEN ] =		0;	// only used in token parsing
	precedence[ STUDIO_CLOSE ] =	0;
	precedence[ STUDIO_COMMA ] =	0;
	precedence[ STUDIO_MAX ] =		5;
	precedence[ STUDIO_MIN ] =		5;

	s_flexop_t stream[MAX_OPS];
	int i = 0;
	s_flexop_t stack[MAX_OPS];
	int j = 0;
	int k = 0;

	s_flexrule_t *pRule = &g_flexrule[g_numflexrules++];

	if (g_numflexrules > MAXSTUDIOFLEXRULES)
	{
		TokenError( "Too many flex rules (max %d)\n", MAXSTUDIOFLEXRULES );
	}

	int flexdesc;
	for ( flexdesc = 0; flexdesc < g_numflexdesc; flexdesc++)
	{
		if (stricmp( name, g_flexdesc[flexdesc].FACS ) == 0)
		{
			break;
		}
	}

	if (flexdesc >= g_numflexdesc)
	{
		TokenError( "Rule for unknown flex %s\n", name );
	}

	pRule->flex = flexdesc;
	pRule->numops = 0;

	// = 
	GetToken(false);

	// parse all the tokens
	bool linecontinue = false;
	while ( linecontinue || TokenAvailable())
	{
		GetExprToken(linecontinue);

		linecontinue = false;

		if ( token[0] == '\\' )
		{
			if (!GetToken(false) || token[0] != '\\')
			{
				TokenError( "unknown expression token '\\%s\n", token );
			}
			linecontinue = true;
		}
		else if ( token[0] == '(' )
		{
			stream[i++].op = STUDIO_OPEN;
		}
		else if ( token[0] == ')' )
		{
			stream[i++].op = STUDIO_CLOSE;
		}
		else if ( token[0] == '+' )
		{
			stream[i++].op = STUDIO_ADD;
		}
		else if ( token[0] == '-' )
		{
			stream[i].op = STUDIO_SUB;
			if (i > 0)
			{
				switch( stream[i-1].op )
				{
				case STUDIO_OPEN:
				case STUDIO_ADD:
				case STUDIO_SUB:
				case STUDIO_MUL:
				case STUDIO_DIV:
				case STUDIO_COMMA:
					// it's a unary if it's preceded by a "(+-*/,"?
					stream[i].op = STUDIO_NEG;
					break;
				}
			}
			i++;
		}
		else if ( token[0] == '*' )
		{
			stream[i++].op = STUDIO_MUL;
		}
		else if ( token[0] == '/' )
		{
			stream[i++].op = STUDIO_DIV;
		}
		else if ( V_isdigit( token[0] ))
		{
			stream[i].op = STUDIO_CONST;
			stream[i++].d.value = verify_atof( token );
		}
		else if ( token[0] == ',' )
		{
			stream[i++].op = STUDIO_COMMA;
		}
		else if ( stricmp( token, "max" ) == 0)
		{
			stream[i++].op = STUDIO_MAX;
		}
		else if ( stricmp( token, "min" ) == 0)
		{
			stream[i++].op = STUDIO_MIN;
		}
		else 
		{
			if (token[0] == '%')
			{
				GetExprToken(false);

				for (k = 0; k < g_numflexdesc; k++)
				{
					if (stricmp( token, g_flexdesc[k].FACS ) == 0)
					{
						stream[i].op = STUDIO_FETCH2;
						stream[i++].d.index = k;
						break;
					}
				}
				if (k >= g_numflexdesc)
				{
					TokenError( "unknown flex %s\n", token );
				}
			}
			else
			{
				for (k = 0; k < g_numflexcontrollers; k++)
				{
					if (stricmp( token, g_flexcontroller[k].name ) == 0)
					{
						stream[i].op = STUDIO_FETCH1;
						stream[i++].d.index = k;
						break;
					}
				}
				if (k >= g_numflexcontrollers)
				{
					TokenError( "unknown controller %s\n", token );
				}
			}
		}
	}

	if (i > MAX_OPS)
	{
		TokenError("expression %s too complicated\n", g_flexdesc[pRule->flex].FACS );
	}

	if (0)
	{
		printf("%s = ", g_flexdesc[pRule->flex].FACS );
		for ( k = 0; k < i; k++)
		{
			switch( stream[k].op )
			{
			case STUDIO_CONST: printf("%f ", stream[k].d.value ); break;
			case STUDIO_FETCH1: printf("%s ", g_flexcontroller[stream[k].d.index].name ); break;
			case STUDIO_FETCH2: printf("[%d] ", stream[k].d.index ); break;
			case STUDIO_ADD: printf("+ "); break;
			case STUDIO_SUB: printf("- "); break;
			case STUDIO_MUL: printf("* "); break;
			case STUDIO_DIV: printf("/ "); break;
			case STUDIO_NEG: printf("neg "); break;
			case STUDIO_MAX: printf("max "); break;
			case STUDIO_MIN: printf("min "); break;
			case STUDIO_COMMA: 	printf(", "); break; // error
			case STUDIO_OPEN: 	printf("( " ); break; // error
			case STUDIO_CLOSE: 	printf(") " ); break; // error
			default:
				printf("err%d ", stream[k].op ); break;
			}
		}
		printf("\n");
		// exit(1);
	}

	j = 0;
	for (k = 0; k < i; k++)
	{
		if (j >= MAX_OPS)
		{
			TokenError("expression %s too complicated\n", g_flexdesc[pRule->flex].FACS );
		}
		switch( stream[k].op )
		{
		case STUDIO_CONST:
		case STUDIO_FETCH1:
		case STUDIO_FETCH2:
			pRule->op[pRule->numops++] = stream[k];
			break;
		case STUDIO_OPEN:
			stack[j++] = stream[k];
			break;
		case STUDIO_CLOSE:
			// pop all operators off of the stack until an open paren
			while (j > 0 && stack[j-1].op != STUDIO_OPEN)
			{
				pRule->op[pRule->numops++] = stack[j-1];
				j--;
			}
			if (j == 0)
			{
				TokenError( "unmatched closed parentheses\n" );
			}
			if (j > 0) 
				j--;
			break;
		case STUDIO_COMMA:
			// pop all operators off of the stack until an open paren
			while (j > 0 && stack[j-1].op != STUDIO_OPEN)
			{
				pRule->op[pRule->numops++] = stack[j-1];
				j--;
			}
			// push operator onto the stack
			stack[j++] = stream[k];
			break;
		case STUDIO_ADD:
		case STUDIO_SUB:
		case STUDIO_MUL:
		case STUDIO_DIV:
			// pop all operators off of the stack that have equal or higher precedence
			while (j > 0 && precedence[stream[k].op] <= precedence[stack[j-1].op])
			{
				pRule->op[pRule->numops++] = stack[j-1];
				j--;
			}
			// push operator onto the stack
			stack[j++] = stream[k];
			break;
		case STUDIO_NEG:
			if (stream[k+1].op == STUDIO_CONST)
			{
				// change sign of constant, skip op
				stream[k+1].d.value = -stream[k+1].d.value;
			}
			else
			{
				// push operator onto the stack
				stack[j++] = stream[k];
			}
			break;
		case STUDIO_MAX:
		case STUDIO_MIN:
			// push operator onto the stack
			stack[j++] = stream[k];
			break;
		}
		if (pRule->numops >= MAX_OPS)
			TokenError("expression for \"%s\" too complicated\n", g_flexdesc[pRule->flex].FACS );
	}
	// pop all operators off of the stack
	while (j > 0)
	{
		pRule->op[pRule->numops++] = stack[j-1];
		j--;
		if (pRule->numops >= MAX_OPS)
			TokenError("expression for \"%s\" too complicated\n", g_flexdesc[pRule->flex].FACS );
	}

	// reprocess the operands, eating commas for all functions
	int numCommas = 0;
	j = 0;
	for (k = 0; k < pRule->numops; k++)
	{
		switch( pRule->op[k].op )
		{
		case STUDIO_MAX:
		case STUDIO_MIN:
			if (pRule->op[j-1].op != STUDIO_COMMA)
			{
				TokenError( "missing comma\n");
			}
			// eat the comma operator
			numCommas--;
			pRule->op[j-1] = pRule->op[k];
			break;
		case STUDIO_COMMA:
			numCommas++;
			pRule->op[j++] = pRule->op[k];
			break;
		default:
			pRule->op[j++] = pRule->op[k];
			break;
		}
	}
	pRule->numops = j;
	if (numCommas != 0)
	{
		TokenError( "too many comma's\n" );
	}

	if (pRule->numops > MAX_OPS)
	{
		TokenError("expression %s too complicated\n", g_flexdesc[pRule->flex].FACS );
	}

	if (0)
	{
		printf("%s = ", g_flexdesc[pRule->flex].FACS );
		for ( i = 0; i < pRule->numops; i++)
		{
			switch( pRule->op[i].op )
			{
			case STUDIO_CONST: printf("%f ", pRule->op[i].d.value ); break;
			case STUDIO_FETCH1: printf("%s ", g_flexcontroller[pRule->op[i].d.index].name ); break;
			case STUDIO_FETCH2: printf("[%d] ", pRule->op[i].d.index ); break;
			case STUDIO_ADD: printf("+ "); break;
			case STUDIO_SUB: printf("- "); break;
			case STUDIO_MUL: printf("* "); break;
			case STUDIO_DIV: printf("/ "); break;
			case STUDIO_NEG: printf("neg "); break;
			case STUDIO_MAX: printf("max "); break;
			case STUDIO_MIN: printf("min "); break;
			case STUDIO_COMMA: 	printf(", "); break; // error
			case STUDIO_OPEN: 	printf("( " ); break; // error
			case STUDIO_CLOSE: 	printf(") " ); break; // error
			default:
				printf("err%d ", pRule->op[i].op ); break;
			}
		}
		printf("\n");
		// exit(1);
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_Model( )
{
	g_model[g_nummodels] = (s_model_t *)kalloc( 1, sizeof( s_model_t ) );
	
	// name
	if (!GetToken(false)) 
		return;
	V_strcpy_safe( g_model[g_nummodels]->name, token );

	// fake g_bodypart stuff
	if (g_numbodyparts == 0) 
	{
		g_bodypart[g_numbodyparts].base = 1;
	}
	else 
	{
		g_bodypart[g_numbodyparts].base = g_bodypart[g_numbodyparts-1].base * g_bodypart[g_numbodyparts-1].nummodels;
	}
	V_strcpy_safe( g_bodypart[g_numbodyparts].name, token );

	g_bodypart[g_numbodyparts].pmodel[g_bodypart[g_numbodyparts].nummodels] = g_model[g_nummodels];
	g_bodypart[g_numbodyparts].nummodels = 1;
	g_numbodyparts++;

	Option_Studio( g_model[g_nummodels] );

	if ( g_model[g_nummodels]->source )
	{
		// Body command should add any flex commands in the source loaded
		AddBodyFlexData( g_model[g_nummodels]->source, g_nummodels );
		AddBodyAttachments( g_model[g_nummodels]->source );
	}
	
	int depth = 0;
	while (1)
	{
		char FAC[256], vtafile[256];
		if (depth > 0)
		{
			if( !GetToken(true) ) 
				break;
		}
		else
		{
			if ( !TokenAvailable() ) 
			{
				break;
			}
			else 
			{
				GetToken (false);
			}
		}

		if ( endofscript )
		{
			if (depth != 0)
			{
				TokenError("missing }\n" );
			}
			return;
		}
		if ( !Q_stricmp("{", token ) )
		{
			depth++;
		}
		else if ( !Q_stricmp("}", token ) )
		{
			depth--;
		}
		else if ( !Q_stricmp( "eyeball", token ) )
		{
			Option_Eyeball( g_model[g_nummodels] );
		}
		else if ( !Q_stricmp( "eyelid", token ) )
		{
			Option_Eyelid( g_nummodels );
		}
		else if ( !Q_stricmp( "flex", token ) )
		{
			// g_flex
			GetToken (false);
			V_strcpy_safe( FAC, token );
			if (depth == 0)
			{
				// file
				GetToken (false);
				V_strcpy_safe( vtafile, token );
			}
			Option_Flex( FAC, vtafile, g_nummodels, 0.0 ); // FIXME: this needs to point to a model used, not loaded!!!
		}
		else if ( !Q_stricmp( "flexpair", token ) )
		{
			// g_flex
			GetToken (false);
			V_strcpy_safe( FAC, token );

			GetToken( false );
			float split = atof( token );

			if (depth == 0)
			{
				// file
				GetToken (false);
				V_strcpy_safe( vtafile, token );
			}
			Option_Flex( FAC, vtafile, g_nummodels, split ); // FIXME: this needs to point to a model used, not loaded!!!
		}
		else if ( !Q_stricmp( "defaultflex", token ) )
		{
			if (depth == 0)
			{
				// file
				GetToken (false);
				V_strcpy_safe( vtafile, token );
			}

			// g_flex
			Option_Flex( "default", vtafile, g_nummodels, 0.0 ); // FIXME: this needs to point to a model used, not loaded!!!
			g_defaultflexkey = &g_flexkey[g_numflexkeys-1];
		}
		else if ( !Q_stricmp( "flexfile", token ) )
		{
			// file
			GetToken (false);
			V_strcpy_safe( vtafile, token );
		}
		else if ( !Q_stricmp( "localvar", token ) )
		{
			while (TokenAvailable())
			{
				GetToken( false );
				Add_Flexdesc( token );
			}
		}
		else if ( !Q_stricmp( "mouth", token ) )
		{
			Option_Mouth( g_model[g_nummodels] );
		}
		else if ( !Q_stricmp( "flexcontroller", token ) )
		{
			Option_Flexcontroller( g_model[g_nummodels] );
		}
		else if ( token[0] == '%' )
		{
			Option_Flexrule( g_model[g_nummodels], &token[1] );
		}
		else if ( !Q_stricmp("attachment", token ) )
		{
		// 	Option_Attachment( g_model[g_nummodels] );
		}
		else if ( !Q_stricmp( token, "spherenormals" ) )
		{
			Option_Spherenormals( g_model[g_nummodels]->source );
		}
		else
		{
			TokenError( "unknown model option \"%s\"\n", token );
		}

		if (depth < 0)
		{
			TokenError("missing {\n");
		}
	};

	// Actually connect up the expressions between the Dme Flex Controllers & Flex Descriptors
	// In case there was data added by some other eyeball command (like eyelid)
	AddBodyFlexRules( g_model[ g_nummodels ]->source );

	g_nummodels++;
}


void Cmd_FakeVTA( void )
{
	int depth = 0;

	GetToken( false );

	s_source_t *psource = (s_source_t *)kalloc( 1, sizeof( s_source_t ) );
	g_source[g_numsources] = psource;
	V_strcpy_safe( g_source[g_numsources]->filename, token );
	g_numsources++;

	while (1)
	{
		if (depth > 0)
		{
			if(!GetToken(true)) 
			{
				break;
			}
		}
		else
		{
			if (!TokenAvailable()) 
			{
				break;
			}
			else 
			{
				GetToken (false);
			}
		}

		if (endofscript)
		{
			if (depth != 0)
			{
				TokenError("missing }\n" );
			}
			return;
		}
		if (stricmp("{", token ) == 0)
		{
			depth++;
		}
		else if (stricmp("}", token ) == 0)
		{
			depth--;
		}
		else if (stricmp("appendvta", token ) == 0)
		{
			char filename[256];
			// file
			GetToken (false);
			V_strcpy_safe( filename, token );
			
			GetToken( false );
			int frame = verify_atoi( token );

			AppendVTAtoOBJ( psource, filename, frame );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_IKChain( )
{
	if (!GetToken(false)) 
		return;

	int i;
	for ( i = 0; i < g_numikchains; i++)
	{
		if (stricmp( token, g_ikchain[i].name ) == 0)
		{
			break;
		}
	}
	if (i < g_numikchains)
	{
		if (!g_quiet)
		{
			printf("duplicate ikchain \"%s\" ignored\n", token );
		}
		while (TokenAvailable())
		{
			GetToken(false);
		}
		return;
	}

	V_strcpy_safe( g_ikchain[g_numikchains].name, token );

	GetToken(false);
	V_strcpy_safe( g_ikchain[g_numikchains].bonename, token );

	g_ikchain[g_numikchains].axis = STUDIO_Z;
	g_ikchain[g_numikchains].value = 0.0;
	g_ikchain[g_numikchains].height = 18.0;
	g_ikchain[g_numikchains].floor = 0.0;
	g_ikchain[g_numikchains].radius = 0.0;

	while (TokenAvailable())
	{
		GetToken(false);

		if (lookupControl( token ) != -1)
		{
			g_ikchain[g_numikchains].axis = lookupControl( token );
			GetToken(false);
			g_ikchain[g_numikchains].value = verify_atof( token );
		}
		else if (stricmp( "height", token ) == 0)
		{
			GetToken(false);
			g_ikchain[g_numikchains].height = verify_atof( token );
		}
		else if (stricmp( "pad", token ) == 0)
		{
			GetToken(false);
			g_ikchain[g_numikchains].radius = verify_atof( token ) / 2.0;
		}
		else if (stricmp( "floor", token ) == 0)
		{
			GetToken(false);
			g_ikchain[g_numikchains].floor = verify_atof( token );
		}
		else if (stricmp( "knee", token ) == 0)
		{
			GetToken(false);
			g_ikchain[g_numikchains].link[0].kneeDir.x = verify_atof( token );
			GetToken(false);
			g_ikchain[g_numikchains].link[0].kneeDir.y = verify_atof( token );
			GetToken(false);
			g_ikchain[g_numikchains].link[0].kneeDir.z = verify_atof( token );
		}
		else if (stricmp( "center", token ) == 0)
		{
			GetToken(false);
			g_ikchain[g_numikchains].center.x = verify_atof( token );
			GetToken(false);
			g_ikchain[g_numikchains].center.y = verify_atof( token );
			GetToken(false);
			g_ikchain[g_numikchains].center.z = verify_atof( token );
		}
	}
	g_numikchains++;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


void Cmd_IKAutoplayLock( )
{
	GetToken(false);
	V_strcpy_safe( g_ikautoplaylock[g_numikautoplaylocks].name, token );

	GetToken(false);
	g_ikautoplaylock[g_numikautoplaylocks].flPosWeight = verify_atof( token );

	GetToken(false);
	g_ikautoplaylock[g_numikautoplaylocks].flLocalQWeight = verify_atof( token );
	
	g_numikautoplaylocks++;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_Root ()
{
	if (GetToken (false))
	{
		V_strcpy_safe( rootname, token );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_Controller (void)
{
	if (GetToken (false))
	{
		if (!stricmp("mouth",token))
		{
			g_bonecontroller[g_numbonecontrollers].inputfield = 4;
		}
		else
		{
			g_bonecontroller[g_numbonecontrollers].inputfield = verify_atoi(token);
		}
		if (GetToken(false))
		{
			V_strcpy_safe( g_bonecontroller[g_numbonecontrollers].name, token );
			GetToken(false);
			if ((g_bonecontroller[g_numbonecontrollers].type = lookupControl(token)) == -1) 
			{
				MdlWarning("unknown g_bonecontroller type '%s'\n", token );
				return;
			}
			GetToken(false);
			g_bonecontroller[g_numbonecontrollers].start = verify_atof( token );
			GetToken(false);
			g_bonecontroller[g_numbonecontrollers].end = verify_atof( token );

			if (g_bonecontroller[g_numbonecontrollers].type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
			{
				if (((int)(g_bonecontroller[g_numbonecontrollers].start + 360) % 360) == ((int)(g_bonecontroller[g_numbonecontrollers].end + 360) % 360))
				{
					g_bonecontroller[g_numbonecontrollers].type |= STUDIO_RLOOP;
				}
			}
			g_numbonecontrollers++;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

// Debugging function that enumerate all a models bones to stdout.
static void SpewBones()
{
	MdlWarning("g_numbones %i\n",g_numbones);

	for ( int i = g_numbones; --i >= 0; )
	{
		printf("%s\n",g_bonetable[i].name);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_ScreenAlign ( void )
{
	if (GetToken (false))
	{
		
		Assert( g_numscreenalignedbones < MAXSTUDIOSRCBONES );

		V_strcpy_safe( g_screenalignedbone[g_numscreenalignedbones].name, token );
		g_screenalignedbone[g_numscreenalignedbones].flags = BONE_SCREEN_ALIGN_SPHERE;

		if( GetToken( false ) )
		{
			if( !stricmp( "sphere", token )  )
			{
				g_screenalignedbone[g_numscreenalignedbones].flags = BONE_SCREEN_ALIGN_SPHERE;				
			}
			else if( !stricmp( "cylinder", token ) )
			{
				g_screenalignedbone[g_numscreenalignedbones].flags = BONE_SCREEN_ALIGN_CYLINDER;				
			}
		}

		g_numscreenalignedbones++;

	} else
	{
		TokenError( "$screenalign: expected bone name\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_BBox (void)
{
	GetToken (false);
	bbox[0][0] = verify_atof( token );

	GetToken (false);
	bbox[0][1] = verify_atof( token );

	GetToken (false);
	bbox[0][2] = verify_atof( token );

	GetToken (false);
	bbox[1][0] = verify_atof( token );

	GetToken (false);
	bbox[1][1] = verify_atof( token );

	GetToken (false);
	bbox[1][2] = verify_atof( token );

	g_wrotebbox = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_CBox (void)
{
	GetToken (false);
	cbox[0][0] = verify_atof( token );

	GetToken (false);
	cbox[0][1] = verify_atof( token );

	GetToken (false);
	cbox[0][2] = verify_atof( token );

	GetToken (false);
	cbox[1][0] = verify_atof( token );

	GetToken (false);
	cbox[1][1] = verify_atof( token );

	GetToken (false);
	cbox[1][2] = verify_atof( token );

	g_wrotecbox = true;
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_Gamma (void)
{
	GetToken (false);
	g_gamma = verify_atof( token );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_TextureGroup( )
{
	if( g_bCreateMakefile )
	{
		return;
	}
	int i;
	int depth = 0;
	int index = 0;
	int group = 0;


	if (!GetToken(false)) 
		return;

	if (g_numskinref == 0)
		g_numskinref = g_numtextures;

	while (1)
	{
		if(!GetToken(true)) 
		{
			break;
		}

		if (endofscript)
		{
			if (depth != 0)
			{
				TokenError("missing }\n" );
			}
			return;
		}
		if (token[0] == '{')
		{
			depth++;
		}
		else if (token[0] == '}')
		{
			depth--;
			if (depth == 0)
				break;
			group++;
			index = 0;
		}
		else if (depth == 2)
		{
			i = UseTextureAsMaterial( LookupTexture( token ) );
			g_texturegroup[g_numtexturegroups][group][index] = i;
			if (group != 0)
				g_texture[i].parent = g_texturegroup[g_numtexturegroups][0][index];
			index++;
			g_numtexturereps[g_numtexturegroups] = index;
			g_numtexturelayers[g_numtexturegroups] = group + 1;
		}
	}

	g_numtexturegroups++;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_Hitgroup( )
{
	GetToken (false);
	g_hitgroup[g_numhitgroups].group = verify_atoi( token );
	GetToken (false);
	V_strcpy_safe( g_hitgroup[g_numhitgroups].name, token );
	g_numhitgroups++;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_Hitbox( )
{
	bool autogenerated = false;
	if ( g_hitboxsets.Size() == 0 )
	{
		g_hitboxsets.AddToTail();
		autogenerated = true;
	}

	// Last one
	s_hitboxset *set = &g_hitboxsets[ g_hitboxsets.Size() - 1 ];
	if ( autogenerated )
	{
		memset( set, 0, sizeof( *set ) );

		// fill in name if it wasn't specified in the .qc
		V_strcpy_safe( set->hitboxsetname, "default" );
	}

	GetToken (false);
	set->hitbox[set->numhitboxes].group = verify_atoi( token );
	
	// Grab the bone name:
	GetToken (false);
	V_strcpy_safe( set->hitbox[set->numhitboxes].name, token );

	GetToken (false);
	set->hitbox[set->numhitboxes].bmin[0] = verify_atof( token );
	GetToken (false);
	set->hitbox[set->numhitboxes].bmin[1] = verify_atof( token );
	GetToken (false);
	set->hitbox[set->numhitboxes].bmin[2] = verify_atof( token );
	GetToken (false);
	set->hitbox[set->numhitboxes].bmax[0] = verify_atof( token );
	GetToken (false);
	set->hitbox[set->numhitboxes].bmax[1] = verify_atof( token );
	GetToken (false);
	set->hitbox[set->numhitboxes].bmax[2] = verify_atof( token );

	//Scale hitboxes
	scale_vertex( set->hitbox[set->numhitboxes].bmin );
	scale_vertex( set->hitbox[set->numhitboxes].bmax );
	// clear out the hitboxname:
	memset( set->hitbox[set->numhitboxes].hitboxname, 0, sizeof( set->hitbox[set->numhitboxes].hitboxname ) );

	// Grab the hit box name if present:
	if( TokenAvailable() )
	{
		GetToken (false);
		V_strcpy_safe( set->hitbox[set->numhitboxes].hitboxname, token );
	}


	set->numhitboxes++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_HitboxSet( void )
{
	// Add a new hitboxset
	s_hitboxset *set = &g_hitboxsets[ g_hitboxsets.AddToTail() ];
	GetToken( false );
	memset( set, 0, sizeof( *set ) );
	V_strcpy_safe( set->hitboxsetname, token );
}


//-----------------------------------------------------------------------------
// Assigns a default surface property to the entire model
//-----------------------------------------------------------------------------
struct SurfacePropName_t
{
	char m_pJointName[128];
	char m_pSurfaceProp[128];
};

static char								s_pDefaultSurfaceProp[128] = {"default"};
static CUtlVector<SurfacePropName_t>	s_JointSurfaceProp;

//-----------------------------------------------------------------------------
// Assigns a default surface property to the entire model
//-----------------------------------------------------------------------------
void Cmd_SurfaceProp ()
{
	GetToken (false);
	V_strcpy_safe( s_pDefaultSurfaceProp, token );
}	


//-----------------------------------------------------------------------------
// Assigns a surface property to a particular joint
//-----------------------------------------------------------------------------
void Cmd_JointSurfaceProp ()
{
	// Get joint name...
	GetToken (false);

	// Search for the name in our list
	int i;
	for ( i = s_JointSurfaceProp.Count(); --i >= 0; )
	{
		if (!stricmp(s_JointSurfaceProp[i].m_pJointName, token))
		{
			break;
		}
	}

	// Add new entry if we haven't seen this name before
	if (i < 0)
	{
		i = s_JointSurfaceProp.AddToTail();
		V_strcpy_safe( s_JointSurfaceProp[i].m_pJointName, token );
	}

	// surface property name
	GetToken(false);
	V_strcpy_safe( s_JointSurfaceProp[i].m_pSurfaceProp, token );
}


//-----------------------------------------------------------------------------
// Returns the default surface prop name
//-----------------------------------------------------------------------------
char* GetDefaultSurfaceProp ( )
{
	return s_pDefaultSurfaceProp;
}


//-----------------------------------------------------------------------------
// Returns surface property for a given joint
//-----------------------------------------------------------------------------
static char* FindSurfaceProp ( const char* pJointName )
{
	for ( int i = s_JointSurfaceProp.Count(); --i >= 0; )
	{
		if (!stricmp(s_JointSurfaceProp[i].m_pJointName, pJointName))
		{
			return s_JointSurfaceProp[i].m_pSurfaceProp;
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Returns surface property for a given joint
//-----------------------------------------------------------------------------
char* GetSurfaceProp ( const char* pJointName )
{
	while( pJointName )
	{
		// First try to find this joint
		char* pSurfaceProp = FindSurfaceProp( pJointName );
		if (pSurfaceProp)
			return pSurfaceProp;

		// If we can't find the joint, then find it's parent...
		if (!g_numbones)
			return s_pDefaultSurfaceProp;

		int i = findGlobalBone( pJointName );

		if ((i >= 0) && (g_bonetable[i].parent >= 0))
		{
			pJointName = g_bonetable[g_bonetable[i].parent].name;
		}
		else
		{
			pJointName = 0;
		}
	}

	// No match, return the default one
	return s_pDefaultSurfaceProp;
}


//-----------------------------------------------------------------------------
// Returns surface property for a given joint
//-----------------------------------------------------------------------------
void ConsistencyCheckSurfaceProp ( )
{
	for ( int i = s_JointSurfaceProp.Count(); --i >= 0; )
	{
		int j = findGlobalBone( s_JointSurfaceProp[i].m_pJointName );

		if (j < 0)
		{
			MdlWarning("You specified a joint surface property for joint\n"
				"    \"%s\" which either doesn't exist or was optimized out.\n", s_JointSurfaceProp[i].m_pJointName );
		}
	}
}


//-----------------------------------------------------------------------------
// Assigns a default contents to the entire model
//-----------------------------------------------------------------------------
struct ContentsName_t
{
	char m_pJointName[128];
	int m_nContents;
};

static int s_nDefaultContents = CONTENTS_SOLID;
static CUtlVector<ContentsName_t>	s_JointContents;


//-----------------------------------------------------------------------------
// Parse contents flags
//-----------------------------------------------------------------------------
static void ParseContents( int *pAddFlags, int *pRemoveFlags )
{
	*pAddFlags = 0;
	*pRemoveFlags = 0;
	do 
	{
		GetToken (false);

		if ( !stricmp( token, "grate" ) )
		{
			*pAddFlags |= CONTENTS_GRATE;
			*pRemoveFlags |= CONTENTS_SOLID;
		}
		else if ( !stricmp( token, "ladder" ) )
		{
			*pAddFlags |= CONTENTS_LADDER;
		}
		else if ( !stricmp( token, "solid" ) )
		{
			*pAddFlags |= CONTENTS_SOLID;
		}
		else if ( !stricmp( token, "monster" ) )
		{
			*pAddFlags |= CONTENTS_MONSTER;
		}
		else if ( !stricmp( token, "notsolid" ) )
		{
			*pRemoveFlags |= CONTENTS_SOLID;
		}
	} while (TokenAvailable());
}


//-----------------------------------------------------------------------------
// Assigns a default contents to the entire model
//-----------------------------------------------------------------------------
void Cmd_Contents()
{
	int nAddFlags, nRemoveFlags;
	ParseContents( &nAddFlags, &nRemoveFlags );
	s_nDefaultContents |= nAddFlags;
	s_nDefaultContents &= ~nRemoveFlags;
}


//-----------------------------------------------------------------------------
// Assigns contents to a particular joint
//-----------------------------------------------------------------------------
void Cmd_JointContents ()
{
	// Get joint name...
	GetToken (false);

	// Search for the name in our list
	int i;
	for ( i = s_JointContents.Count(); --i >= 0; )
	{
		if (!stricmp(s_JointContents[i].m_pJointName, token))
		{
			break;
		}
	}

	// Add new entry if we haven't seen this name before
	if (i < 0)
	{
		i = s_JointContents.AddToTail();
		V_strcpy_safe( s_JointContents[i].m_pJointName, token );
	}

	int nAddFlags, nRemoveFlags;
	ParseContents( &nAddFlags, &nRemoveFlags );
	s_JointContents[i].m_nContents = CONTENTS_SOLID;
	s_JointContents[i].m_nContents |= nAddFlags;
	s_JointContents[i].m_nContents &= ~nRemoveFlags;
}


//-----------------------------------------------------------------------------
// Returns the default contents
//-----------------------------------------------------------------------------
int GetDefaultContents( )
{
	return s_nDefaultContents;
}


//-----------------------------------------------------------------------------
// Returns contents for a given joint
//-----------------------------------------------------------------------------
static int FindContents( const char* pJointName )
{
	for ( int i = s_JointContents.Count(); --i >= 0; )
	{
		if (!stricmp(s_JointContents[i].m_pJointName, pJointName))
		{
			return s_JointContents[i].m_nContents;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Returns contents for a given joint
//-----------------------------------------------------------------------------
int GetContents( const char* pJointName )
{
	while( pJointName )
	{
		// First try to find this joint
		int nContents = FindContents( pJointName );
		if (nContents != -1)
			return nContents;

		// If we can't find the joint, then find it's parent...
		if (!g_numbones)
			return s_nDefaultContents;

		int i = findGlobalBone( pJointName );

		if ((i >= 0) && (g_bonetable[i].parent >= 0))
		{
			pJointName = g_bonetable[g_bonetable[i].parent].name;
		}
		else
		{
			pJointName = 0;
		}
	}

	// No match, return the default one
	return s_nDefaultContents;
}


//-----------------------------------------------------------------------------
// Checks specified contents
//-----------------------------------------------------------------------------
void ConsistencyCheckContents( )
{
	for ( int i = s_JointContents.Count(); --i >= 0; )
	{
		int j = findGlobalBone( s_JointContents[i].m_pJointName );

		if (j < 0)
		{
			MdlWarning("You specified a joint contents for joint\n"
				"    \"%s\" which either doesn't exist or was optimized out.\n", s_JointSurfaceProp[i].m_pJointName );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_BoneMerge( )
{
	if( g_bCreateMakefile )
		return;

	int nIndex = g_BoneMerge.AddToTail();

	// bone name
	GetToken (false);
	V_strcpy_safe( g_BoneMerge[nIndex].bonename, token );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_Attachment( )
{
	if( g_bCreateMakefile )
		return;

	// name
	GetToken (false);
	V_strcpy_safe( g_attachment[g_numattachments].name, token );

	// bone name
	GetToken (false);
	V_strcpy_safe( g_attachment[g_numattachments].bonename, token );

	Vector tmp;

	// position
	GetToken (false);
	tmp.x = verify_atof( token );
	GetToken (false);
	tmp.y = verify_atof( token );
	GetToken (false);
	tmp.z = verify_atof( token );

	scale_vertex( tmp );
	// identity matrix
	AngleMatrix( QAngle( 0, 0, 0 ), g_attachment[g_numattachments].local );

	while (TokenAvailable())
	{
		GetToken (false);

		if (stricmp(token,"absolute") == 0)
		{
			g_attachment[g_numattachments].type |= IS_ABSOLUTE;
			AngleIMatrix( g_defaultrotation, g_attachment[g_numattachments].local );
			// AngleIMatrix( Vector( 0, 0, 0 ), g_attachment[g_numattachments].local );
		}
		else if (stricmp(token,"rigid") == 0)
		{
			g_attachment[g_numattachments].type |= IS_RIGID;
		}
		else if (stricmp(token,"world_align") == 0)
		{
			g_attachment[g_numattachments].flags |= ATTACHMENT_FLAG_WORLD_ALIGN;
		}
		else if (stricmp(token,"rotate") == 0)
		{
			QAngle angles;
			for (int i = 0; i < 3; ++i)
			{
				if (!TokenAvailable())
					break;

				GetToken(false);
				angles[i] = verify_atof( token );
			}
			AngleMatrix( angles, g_attachment[g_numattachments].local );
		}
		else if (stricmp(token,"x_and_z_axes") == 0)
		{
			int i;
			Vector xaxis, yaxis, zaxis;
			for (i = 0; i < 3; ++i)
			{
				if (!TokenAvailable())
					break;

				GetToken(false);
				xaxis[i] = verify_atof( token );
			}
			for (i = 0; i < 3; ++i)
			{
				if (!TokenAvailable())
					break;

				GetToken(false);
				zaxis[i] = verify_atof( token );
			}
			VectorNormalize( xaxis );
			VectorMA( zaxis, -DotProduct( zaxis, xaxis ), xaxis, zaxis );
			VectorNormalize( zaxis );
			CrossProduct( zaxis, xaxis, yaxis );
			MatrixSetColumn( xaxis, 0, g_attachment[g_numattachments].local );
			MatrixSetColumn( yaxis, 1, g_attachment[g_numattachments].local );
			MatrixSetColumn( zaxis, 2, g_attachment[g_numattachments].local );
			MatrixSetColumn( vec3_origin, 3, g_attachment[g_numattachments].local );
		}
		else
		{
			TokenError("unknown attachment (%s) option: ", g_attachment[g_numattachments].name, token );
		}
	}

	g_attachment[g_numattachments].local[0][3] = tmp.x;
	g_attachment[g_numattachments].local[1][3] = tmp.y;
	g_attachment[g_numattachments].local[2][3] = tmp.z;

	g_numattachments++;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int LookupAttachment( char *name )
{
	int i;
	for (i = 0; i < g_numattachments; i++)
	{
		if (stricmp( g_attachment[i].name, name ) == 0)
		{
			return i;
		}
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Cmd_Renamebone( )
{
	// from
	GetToken (false);
	V_strcpy_safe( g_renamedbone[g_numrenamedbones].from, token );

	// to
	GetToken (false);
	V_strcpy_safe( g_renamedbone[g_numrenamedbones].to, token );

	g_numrenamedbones++;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void Cmd_Skiptransition( )
{
	int nskips = 0;
	int list[10];

	while (TokenAvailable())
	{
		GetToken (false);
		list[nskips++] = LookupXNode( token );
	}

	for (int i = 0; i < nskips; i++)
	{
		for (int j = 0; j < nskips; j++)
		{
			if (list[i] != list[j])
			{
				g_xnodeskip[g_numxnodeskips][0] = list[i];
				g_xnodeskip[g_numxnodeskips][1] = list[j];
				g_numxnodeskips++;
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
// The following code is all related to LODs
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Parse replacemodel command, causes an LOD to use a new model
//-----------------------------------------------------------------------------

static void Cmd_ReplaceModel( LodScriptData_t& lodData )
{
	int i = lodData.modelReplacements.AddToTail();
	CLodScriptReplacement_t& newReplacement = lodData.modelReplacements[i];

	// from
	GetToken( false );

	// Strip off extensions for the source...
	char* pDot = strrchr( token, '.' );
	if (pDot)
	{
		*pDot = 0;
	}

	if (!FindCachedSource( token, "" ))
	{
		// must have prior knowledge of the from
		TokenError( "Unknown replace model '%s'\n", token );
	}

	newReplacement.SetSrcName( token );

	// to
	GetToken( false );
	newReplacement.SetDstName( token );

	// check for "reverse"
	bool reverse =  false;
	if( TokenAvailable() && GetToken( false ) )
	{
		if( stricmp( "reverse", token ) == 0 )
		{
			reverse = true;
		}
		else
		{
			TokenError( "\"%s\" unexpected\n", token );
		}
	}

	// If the LOD system tells us to replace "blank", let's forget
	// we ever read this. Have to do it here so parsing works
	if( !stricmp( newReplacement.GetSrcName(), "blank" ) )
	{
		lodData.modelReplacements.FastRemove( i );
		return;
	}

	// Load the source right here baby! That way its bones will get converted
	if ( !lodData.IsStrippedFromModel() )
	{
		newReplacement.m_pSource = Load_Source( newReplacement.GetDstName(), "smd", reverse, false );
	}
	else if ( !g_quiet )
	{
		printf( "Stripped lod \"%s\" @ %.1f\n", newReplacement.GetDstName(), lodData.switchValue );
	}
}

//-----------------------------------------------------------------------------
// Parse removemodel command, causes an LOD to stop using a model
//-----------------------------------------------------------------------------

static void Cmd_RemoveModel( LodScriptData_t& lodData )
{
	int i = lodData.modelReplacements.AddToTail();
	CLodScriptReplacement_t& newReplacement = lodData.modelReplacements[i];

	// from
	GetToken( false );

	// Strip off extensions...
	char* pDot = strrchr( token, '.' );
	if (pDot)
		*pDot = 0;

	newReplacement.SetSrcName( token );

	// to
	newReplacement.SetDstName( "" );

	// If the LOD system tells us to replace "blank", let's forget
	// we ever read this. Have to do it here so parsing works
	if( !stricmp( newReplacement.GetSrcName(), "blank" ) )
	{
		lodData.modelReplacements.FastRemove( i );
	}
}

//-----------------------------------------------------------------------------
// Parse replacebone command, causes a part of an LOD model to use a different bone
//-----------------------------------------------------------------------------

static void Cmd_ReplaceBone( LodScriptData_t& lodData )
{
	int i = lodData.boneReplacements.AddToTail();
	CLodScriptReplacement_t& newReplacement = lodData.boneReplacements[i];

	// from
	GetToken( false );
	newReplacement.SetSrcName( token );

	// to
	GetToken( false );
	newReplacement.SetDstName( token );
}

//-----------------------------------------------------------------------------
// Parse bonetreecollapse command, causes the entire subtree to use the same bone as the node
//-----------------------------------------------------------------------------

static void Cmd_BoneTreeCollapse( LodScriptData_t& lodData )
{
	int i = lodData.boneTreeCollapses.AddToTail();
	CLodScriptReplacement_t& newCollapse = lodData.boneTreeCollapses[i];

	// from
	GetToken( false );
	newCollapse.SetSrcName( token );
}

//-----------------------------------------------------------------------------
// Parse replacematerial command, causes a material to be used in place of another
//-----------------------------------------------------------------------------

static void Cmd_ReplaceMaterial( LodScriptData_t& lodData )
{
	int i = lodData.materialReplacements.AddToTail();
	CLodScriptReplacement_t& newReplacement = lodData.materialReplacements[i];

	// from
	GetToken( false );
	newReplacement.SetSrcName( token );

	// to
	GetToken( false );
	newReplacement.SetDstName( token );

	if ( !lodData.IsStrippedFromModel() )
	{
		// make sure it goes into the master list
		UseTextureAsMaterial( LookupTexture( token ) );
	}
}

//-----------------------------------------------------------------------------
// Parse removemesh command, causes a mesh to not be used anymore
//-----------------------------------------------------------------------------

static void Cmd_RemoveMesh( LodScriptData_t& lodData )
{
	int i = lodData.meshRemovals.AddToTail();
	CLodScriptReplacement_t& newReplacement = lodData.meshRemovals[i];

	// from
	GetToken( false );
	Q_FixSlashes( token );
	newReplacement.SetSrcName( token );
}

void Cmd_LOD( const char *cmdname )
{
	if ( gflags & STUDIOHDR_FLAGS_HASSHADOWLOD )
	{
		MdlError( "Model can only have one $shadowlod and it must be the last lod in the .qc (%d) : %s\n", g_iLinecount, g_szLine );
	}

	int i = g_ScriptLODs.AddToTail();
	LodScriptData_t& newLOD = g_ScriptLODs[i];

	if( g_ScriptLODs.Count() > MAX_NUM_LODS )
	{
		MdlError( "Too many LODs (MAX_NUM_LODS==%d)\n", ( int )MAX_NUM_LODS );
	}

	// Shadow lod reserves -1 as switch value
	// which uniquely identifies a shadow lod
	newLOD.switchValue = -1.0f;

	bool isShadowCall = ( !stricmp( cmdname, "$shadowlod" ) ) ? true : false;

	if ( isShadowCall )
	{
		if ( TokenAvailable() )
		{
			GetToken( false );
			MdlWarning( "(%d) : %s:  Ignoring switch value on %s command line\n", g_iLinecount, cmdname, g_szLine );
		}

		// Disable facial animation by default
		newLOD.EnableFacialAnimation( false );
	}
	else
	{
		if ( TokenAvailable() )
		{
			GetToken( false );
			newLOD.switchValue = verify_atof( token );
			if ( newLOD.switchValue < 0.0f )
			{
				MdlError( "Negative switch value reserved for $shadowlod (%d) : %s\n", g_iLinecount, g_szLine );
			}
		}
		else
		{
			MdlError( "Expected LOD switch value (%d) : %s\n", g_iLinecount, g_szLine );
		}
	}

	GetToken( true );
	if( stricmp( "{", token ) != 0 )
	{
		MdlError( "\"{\" expected while processing %s (%d) : %s", cmdname, g_iLinecount, g_szLine );
	}

	// In case we are stripping all lods and it's not Lod0, strip it
	if ( i && g_bStripLods )
		newLOD.StripFromModel( true );

	while( 1 )
	{
		GetToken( true );
		if( stricmp( "replacemodel", token ) == 0 )
		{
			Cmd_ReplaceModel(newLOD);
		}
		else if( stricmp( "removemodel", token ) == 0 )
		{
			Cmd_RemoveModel(newLOD);
		}
		else if( stricmp( "replacebone", token ) == 0 )
		{
			Cmd_ReplaceBone( newLOD );
		}
		else if( stricmp( "bonetreecollapse", token ) == 0 )
		{
			Cmd_BoneTreeCollapse( newLOD );
		}
		else if( stricmp( "replacematerial", token ) == 0 )
		{
			Cmd_ReplaceMaterial( newLOD );
		}
		else if( stricmp( "removemesh", token ) == 0 )
		{
			Cmd_RemoveMesh( newLOD );
		}
		else if( stricmp( "nofacial", token ) == 0 )
		{
			newLOD.EnableFacialAnimation( false );
		}
		else if( stricmp( "facial", token ) == 0 )
		{
			if (isShadowCall)
			{
				// facial animation has no reasonable purpose on a shadow lod
				TokenError( "Facial animation is not allowed for $shadowlod\n" );
			}

			newLOD.EnableFacialAnimation( true );
		}
		else if ( stricmp( "use_shadowlod_materials", token ) == 0 )
		{
			if (isShadowCall)
			{
				gflags |= STUDIOHDR_FLAGS_USE_SHADOWLOD_MATERIALS;
			}
		}
		else if( stricmp( "}", token ) == 0 )
		{
			break;
		}
		else
		{
			MdlError( "invalid input while processing %s (%d) : %s", cmdname, g_iLinecount, g_szLine );
		}
	}

	// If the LOD is stripped, then forget we saw it
	if ( newLOD.IsStrippedFromModel() )
	{
		g_ScriptLODs.FastRemove( i );
	}
}

void Cmd_ShadowLOD( void )
{
	if (!g_quiet)
	{
		printf( "Processing $shadowlod\n" );
	}

	// Act like it's a regular lod entry
	Cmd_LOD( "$shadowlod" );

	// Mark .mdl as having shadow lod (we also check above that we have only one of these
	// and that it's the last entered lod )
	gflags |= STUDIOHDR_FLAGS_HASSHADOWLOD;
}


//-----------------------------------------------------------------------------
// A couple commands related to translucency sorting
//-----------------------------------------------------------------------------
void Cmd_Opaque( )
{
	// Force Opaque has precedence
	gflags |= STUDIOHDR_FLAGS_FORCE_OPAQUE;
	gflags &= ~STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS;
}

void Cmd_TranslucentTwoPass( )
{
	// Force Opaque has precedence
	if ((gflags & STUDIOHDR_FLAGS_FORCE_OPAQUE) == 0)
	{
		gflags |= STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS;
	}
}

//-----------------------------------------------------------------------------
// Indicates the model be rendered with ambient boost heuristic (first used on Alyx in Episode 1)
//-----------------------------------------------------------------------------
void Cmd_AmbientBoost()
{
	gflags |= STUDIOHDR_FLAGS_AMBIENT_BOOST;
}

//-----------------------------------------------------------------------------
// Indicates the model should not cast shadows (useful for first-person models as used in L4D)
//-----------------------------------------------------------------------------
void Cmd_DoNotCastShadows()
{
	gflags |= STUDIOHDR_FLAGS_DO_NOT_CAST_SHADOWS;
}

//-----------------------------------------------------------------------------
// Indicates the model should cast texutre-based shadows in vrad (NOTE: only applicable to prop_static)
//-----------------------------------------------------------------------------
void Cmd_CastTextureShadows()
{
	gflags |= STUDIOHDR_FLAGS_CAST_TEXTURE_SHADOWS;
}


//-----------------------------------------------------------------------------
// Indicates the model should not fade out even if the level or fallback settings say to
//-----------------------------------------------------------------------------
void Cmd_NoForcedFade()
{
	gflags |= STUDIOHDR_FLAGS_NO_FORCED_FADE;
}


//-----------------------------------------------------------------------------
// Indicates the model should not use the bone origin when calculating bboxes, sequence boxes, etc.
//-----------------------------------------------------------------------------
void Cmd_SkipBoneInBBox()
{
	g_bUseBoneInBBox = false;
}


//-----------------------------------------------------------------------------
// Indicates the model will lengthen the viseme check to always include two phonemes
//-----------------------------------------------------------------------------
void Cmd_ForcePhonemeCrossfade()
{
	gflags |= STUDIOHDR_FLAGS_FORCE_PHONEME_CROSSFADE;
}

//-----------------------------------------------------------------------------
// Indicates the model should keep pre-defined bone lengths regardless of animation changes
//-----------------------------------------------------------------------------
void Cmd_LockBoneLengths()
{
	g_bLockBoneLengths = true;
}

//-----------------------------------------------------------------------------
// Indicates the model should replace pre-defined bone lengths and default orientations
//-----------------------------------------------------------------------------
void Cmd_UnlockDefineBones()
{
	g_bOverridePreDefinedBones = true;
}

//-----------------------------------------------------------------------------
// Mark this model as obsolete so that it'll show the obsolete material in game.
//-----------------------------------------------------------------------------
void Cmd_Obsolete( )
{
	// Force Opaque has precedence
	gflags |= STUDIOHDR_FLAGS_OBSOLETE;
}

//-----------------------------------------------------------------------------
// The bones should be moved so that they center themselves on the verts they own.
//-----------------------------------------------------------------------------
void Cmd_CenterBonesOnVerts( )
{
	// force centering on bones
	g_bCenterBonesOnVerts = true;
}

//-----------------------------------------------------------------------------
// How far back should simple motion extract pull back from the last frame
//-----------------------------------------------------------------------------
void Cmd_MotionExtractionRollBack( )
{
	GetToken( false );
	g_flDefaultMotionRollback = atof( token );
}

//-----------------------------------------------------------------------------
// rules for breaking up long animations into multiple sub anims
//-----------------------------------------------------------------------------
void Cmd_SectionFrames( )
{
	GetToken( false );
	g_sectionFrames = atof( token );
	GetToken( false );
	g_minSectionFrameLimit = atoi( token );
}


//-----------------------------------------------------------------------------
// world space clamping boundaries for animations
//-----------------------------------------------------------------------------
void Cmd_ClampWorldspace( )
{
	GetToken (false);
	g_vecMinWorldspace[0] = verify_atof( token );

	GetToken (false);
	g_vecMinWorldspace[1] = verify_atof( token );

	GetToken (false);
	g_vecMinWorldspace[2] = verify_atof( token );

	GetToken (false);
	g_vecMaxWorldspace[0] = verify_atof( token );

	GetToken (false);
	g_vecMaxWorldspace[1] = verify_atof( token );

	GetToken (false);
	g_vecMaxWorldspace[2] = verify_atof( token );
}

//-----------------------------------------------------------------------------
// Key value block!
//-----------------------------------------------------------------------------
void Option_KeyValues( CUtlVector< char > *pKeyValue )
{
	// Simply read in the block between { }s as text 
	// and plop it out unchanged into the .mdl file. 
	// Make sure to respect the fact that we may have nested {}s
	int nLevel = 1;

	if ( !GetToken( true ) )
		return;

	if ( token[0] != '{' )
		return;

	AppendKeyValueText( pKeyValue, "mdlkeyvalue\n{\n" );

	while ( GetToken(true) )
	{
		if ( !stricmp( token, "}" ) )
		{
			nLevel--;
			if ( nLevel <= 0 )
				break;
			AppendKeyValueText( pKeyValue, " }\n" );
		}
		else if ( !stricmp( token, "{" ) )
		{
			AppendKeyValueText( pKeyValue, "{\n" );
			nLevel++;
		}
		else
		{
			// tokens inside braces are quoted
			if ( nLevel > 1 )
			{
				AppendKeyValueText( pKeyValue, "\"" );
				AppendKeyValueText( pKeyValue, token );
				AppendKeyValueText( pKeyValue, "\" " );
			}
			else
			{
				AppendKeyValueText( pKeyValue, token );
				AppendKeyValueText( pKeyValue, " " );
			}
		}
	}

	if ( nLevel >= 1 )
	{
		TokenError( "Keyvalue block missing matching braces.\n" );
	}

	AppendKeyValueText( pKeyValue, "}\n" );
}



//-----------------------------------------------------------------------------
// Purpose: force a specific parent child relationship
//-----------------------------------------------------------------------------

void Cmd_ForcedHierarchy( )
{
	// child name
	GetToken (false);
	V_strcpy_safe( g_forcedhierarchy[g_numforcedhierarchy].childname, token );

	// parent name
	GetToken (false);
	V_strcpy_safe( g_forcedhierarchy[g_numforcedhierarchy].parentname, token );

	g_numforcedhierarchy++;
}


//-----------------------------------------------------------------------------
// Purpose: insert a virtual bone between a child and parent (currently unsupported)
//-----------------------------------------------------------------------------

void Cmd_InsertHierarchy( )
{
	// child name
	GetToken (false);
	V_strcpy_safe( g_forcedhierarchy[g_numforcedhierarchy].childname, token );

	// subparent name
	GetToken (false);
	V_strcpy_safe( g_forcedhierarchy[g_numforcedhierarchy].subparentname, token );

	// parent name
	GetToken (false);
	V_strcpy_safe( g_forcedhierarchy[g_numforcedhierarchy].parentname, token );

	g_numforcedhierarchy++;
}


//-----------------------------------------------------------------------------
// Purpose: rotate a specific bone
//-----------------------------------------------------------------------------

void Cmd_ForceRealign( )
{
	// bone name
	GetToken (false);
	V_strcpy_safe( g_forcedrealign[g_numforcedrealign].name, token );

	// skip
	GetToken (false);

	// X axis
	GetToken (false);
	g_forcedrealign[g_numforcedrealign].rot.x = DEG2RAD( verify_atof( token ) );

	// Y axis
	GetToken (false);
	g_forcedrealign[g_numforcedrealign].rot.y = DEG2RAD( verify_atof( token ) );

	// Z axis
	GetToken (false);
	g_forcedrealign[g_numforcedrealign].rot.z = DEG2RAD( verify_atof( token ) );

	g_numforcedrealign++;
}


//-----------------------------------------------------------------------------
// Purpose: specify a bone to allow > 180 but < 360 rotation (forces a calculated "mid point" to rotation)
//-----------------------------------------------------------------------------

void Cmd_LimitRotation( )
{
	// bone name
	GetToken (false);
	V_strcpy_safe( g_limitrotation[g_numlimitrotation].name, token );

	while (TokenAvailable())
	{
		// sequence name
		GetToken (false);
		// This was a call to strcpyn but since sequencename is an array of char*
		// it was passing sizeof(char*) as the number of characters to copy, which
		// makes no sense. Commenting out until a better idea comes along.
		Assert( 0 );
		//V_strcpy_safe( g_limitrotation[g_numlimitrotation].sequencename[g_limitrotation[g_numlimitrotation].numseq++], token );
	}

	g_numlimitrotation++;
}


//-----------------------------------------------------------------------------
// Purpose: specify bones to store, even if nothing references them
//-----------------------------------------------------------------------------

void Cmd_DefineBone( )
{
	// bone name
	GetToken (false);
	V_strcpy_safe( g_importbone[g_numimportbones].name, token );

	// parent name
	GetToken (false);
	V_strcpy_safe( g_importbone[g_numimportbones].parent, token );

	Vector pos;
	QAngle angles;

	// default pos
	GetToken (false);
	pos.x = verify_atof( token );
	GetToken (false);
	pos.y = verify_atof( token );
	GetToken (false);
	pos.z = verify_atof( token );
	GetToken (false);
	angles.x = verify_atof( token );
	GetToken (false);
	angles.y = verify_atof( token );
	GetToken (false);
	angles.z = verify_atof( token );
	AngleMatrix( angles, pos, g_importbone[g_numimportbones].rawLocal );

	if (TokenAvailable())
	{
		g_importbone[g_numimportbones].bPreAligned = true;
		// realign pos
		GetToken (false);
		pos.x = verify_atof( token );
		GetToken (false);
		pos.y = verify_atof( token );
		GetToken (false);
		pos.z = verify_atof( token );
		GetToken (false);
		angles.x = verify_atof( token );
		GetToken (false);
		angles.y = verify_atof( token );
		GetToken (false);
		angles.z = verify_atof( token );

		AngleMatrix( angles, pos, g_importbone[g_numimportbones].srcRealign );
	}
	else
	{
		SetIdentityMatrix( g_importbone[g_numimportbones].srcRealign );
	}

	g_numimportbones++;
}


//----------------------------------------------------------------------------------------------
float ParseJiggleStiffness( void )
{
	if ( !GetToken( false ) )
	{
		MdlError( "$jigglebone: expecting stiffness value\n" );
		return 0.0f;
	}

	float stiffness = verify_atof( token );
	
	const float minStiffness = 0.0f;
	const float maxStiffness = 1000.0f;

	return clamp( stiffness, minStiffness, maxStiffness );
}


//----------------------------------------------------------------------------------------------
float ParseJiggleDamping( void )
{
	if ( !GetToken( false ) )
	{
		MdlError( "$jigglebone: expecting damping value\n" );
		return 0.0f;
	}

	float damping = verify_atof( token );

	const float minDamping = 0.0f;
	const float maxDamping = 10.0f;

	return clamp( damping, minDamping, maxDamping );
}


//----------------------------------------------------------------------------------------------
bool ParseJiggleAngleConstraint( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= JIGGLE_HAS_ANGLE_CONSTRAINT;

	if ( !GetToken( false ) )
	{
		MdlError( "$jigglebone: expecting angle value\n" );
		return false;
	}
	
	jiggleInfo->data.angleLimit = verify_atof( token ) * M_PI / 180.0f;
	
	return true;
}


//----------------------------------------------------------------------------------------------
bool ParseJiggleYawConstraint( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= JIGGLE_HAS_YAW_CONSTRAINT;
	
	if ( !GetToken( false ) )
	{
		MdlError( "$jigglebone: expecting minimum yaw value\n" );
		return false;	
	}

	jiggleInfo->data.minYaw = verify_atof( token ) * M_PI / 180.0f;

	if ( !GetToken( false ) )
	{
		MdlError( "$jigglebone: expecting maximum yaw value\n" );
		return false;
	}

	jiggleInfo->data.maxYaw = verify_atof( token ) * M_PI / 180.0f;
	
	return true;
}


//----------------------------------------------------------------------------------------------
bool ParseJigglePitchConstraint( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= JIGGLE_HAS_PITCH_CONSTRAINT;

	if ( !GetToken( false ) )
	{
		MdlError( "$jigglebone: expecting minimum pitch value\n" );
		return false;	
	}

	jiggleInfo->data.minPitch = verify_atof( token ) * M_PI / 180.0f;

	if ( !GetToken( false ) )
	{
		MdlError( "$jigglebone: expecting maximum pitch value\n" );
		return false;
	}

	jiggleInfo->data.maxPitch = verify_atof( token ) * M_PI / 180.0f;

	return true;
}


//----------------------------------------------------------------------------------------------
/**
 * Parse common parameters.
 * This assumes a token has already been read, and returns true if
 * the token is recognized and parsed.
 */
bool ParseCommonJiggle( s_jigglebone_t *jiggleInfo )
{
	if (!stricmp( token, "tip_mass" ))
	{
		if ( !GetToken( false ) )
		{
			return false;
		}

		jiggleInfo->data.tipMass = verify_atof( token );
	}
	else if (!stricmp( token, "length" ))
	{
		if ( !GetToken( false ) )
		{
			return false;
		}

		jiggleInfo->data.length = verify_atof( token );
	}
	else if (!stricmp( token, "angle_constraint" ))
	{
		if (ParseJiggleAngleConstraint( jiggleInfo ) == false)
		{
			return false;
		}
	}
	else if (!stricmp( token, "yaw_constraint" ))
	{
		if (ParseJiggleYawConstraint( jiggleInfo ) == false)
		{
			return false;
		}
	}
	else if (!stricmp( token, "yaw_friction" ))
	{
		if ( !GetToken( false ) )
		{
			return false;
		}

		jiggleInfo->data.yawFriction = verify_atof( token );
	}
	else if (!stricmp( token, "yaw_bounce" ))
	{
		if ( !GetToken( false ) )
		{
			return false;
		}

		jiggleInfo->data.yawBounce = verify_atof( token );
	}
	else if (!stricmp( token, "pitch_constraint" ))
	{
		if (ParseJigglePitchConstraint( jiggleInfo ) == false)
		{
			return false;
		}
	}
	else if (!stricmp( token, "pitch_friction" ))
	{
		if ( !GetToken( false ) )
		{
			return false;		
		}

		jiggleInfo->data.pitchFriction = verify_atof( token );
	}
	else if (!stricmp( token, "pitch_bounce" ))
	{
		if ( !GetToken( false ) )
		{
			return false;		
		}

		jiggleInfo->data.pitchBounce = verify_atof( token );
	}
	else
	{
		// unknown token
		MdlError( "$jigglebone: invalid syntax '%s'\n", token );
		return false;
	}
	
	return true;
}


//----------------------------------------------------------------------------------------------
/**
 * Parse parameters for is_flexible subsection
 */
bool ParseFlexibleJiggle( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= (JIGGLE_IS_FLEXIBLE | JIGGLE_HAS_LENGTH_CONSTRAINT);
	
	bool gotOpenBracket = false;	
	while (true)
	{
		if (GetToken( true ) == false)
		{
			MdlError( "$jigglebone:is_flexible: parse error\n" );
			return false;
		}

		if (!stricmp( token, "{" ))
		{
			gotOpenBracket = true;
		}
		else if (!gotOpenBracket)
		{
			MdlError( "$jigglebone:is_flexible: missing '{'\n" );
			return false;
		}
		else if (!stricmp( token, "}" ))
		{
			// definition complete
			break;
		}
		else if (!stricmp( token, "yaw_stiffness" ))
		{
			jiggleInfo->data.yawStiffness = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "yaw_damping" ))
		{
			jiggleInfo->data.yawDamping = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "pitch_stiffness" ))
		{
			jiggleInfo->data.pitchStiffness = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "pitch_damping" ))
		{
			jiggleInfo->data.pitchDamping = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "along_stiffness" ))
		{
			jiggleInfo->data.alongStiffness = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "along_damping" ))
		{
			jiggleInfo->data.alongDamping = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "allow_length_flex" ))
		{
			jiggleInfo->data.flags &= ~JIGGLE_HAS_LENGTH_CONSTRAINT;
		}
		else if (ParseCommonJiggle( jiggleInfo ) == false)
		{
			MdlError( "$jigglebone:is_flexible: invalid syntax '%s'\n", token );
			return false;
		}
	}
	
	return true;
}


//----------------------------------------------------------------------------------------------
/**
 * Parse parameters for is_rigid subsection
 */
bool ParseRigidJiggle( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= (JIGGLE_IS_RIGID | JIGGLE_HAS_LENGTH_CONSTRAINT);

	bool gotOpenBracket = false;	
	while (true)
	{
		if (GetToken( true ) == false)
		{
			MdlError( "$jigglebone:is_rigid: parse error\n" );
			return false;
		}

		if (!stricmp( token, "{" ))
		{
			gotOpenBracket = true;
		}
		else if (!gotOpenBracket)
		{
			MdlError( "$jigglebone:is_rigid: missing '{'\n" );
			return false;
		}
		else if (!stricmp( token, "}" ))
		{
			// definition complete
			break;
		}
		else if (ParseCommonJiggle( jiggleInfo ) == false)
		{
			MdlError( "$jigglebone:is_rigid: invalid syntax '%s'\n", token );
			return false;
		}
	}

	return true;
}


//----------------------------------------------------------------------------------------------
/**
 * Parse parameters for has_base_spring subsection
 */
bool ParseBaseSpringJiggle( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= JIGGLE_HAS_BASE_SPRING;

	bool gotOpenBracket = false;	
	while (true)
	{
		if (GetToken( true ) == false)
		{
			MdlError( "$jigglebone:has_base_spring: parse error\n" );
			return false;
		}

		if (!stricmp( token, "{" ))
		{
			gotOpenBracket = true;
		}
		else if (!gotOpenBracket)
		{
			MdlError( "$jigglebone:has_base_spring: missing '{'\n" );
			return false;
		}
		else if (!stricmp( token, "}" ))
		{
			// definition complete
			break;
		}
		else if (!stricmp( token, "stiffness" ))
		{
			jiggleInfo->data.baseStiffness = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "damping" ))
		{
			jiggleInfo->data.baseDamping = ParseJiggleStiffness();
		}
		else if (!stricmp( token, "left_constraint" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMinLeft = verify_atof( token );

			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMaxLeft = verify_atof( token );
		}
		else if (!stricmp( token, "left_friction" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseLeftFriction = verify_atof( token );
		}
		else if (!stricmp( token, "up_constraint" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMinUp = verify_atof( token );

			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMaxUp = verify_atof( token );
		}
		else if (!stricmp( token, "up_friction" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseUpFriction = verify_atof( token );
		}
		else if (!stricmp( token, "forward_constraint" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMinForward = verify_atof( token );

			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMaxForward = verify_atof( token );
		}
		else if (!stricmp( token, "forward_friction" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseForwardFriction = verify_atof( token );
		}
		else if (!stricmp( token, "base_mass" ))
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.baseMass = verify_atof( token );
		}
		else if (ParseCommonJiggle( jiggleInfo ) == false)
		{
			MdlError( "$jigglebone:has_base_spring: invalid syntax '%s'\n", token );
			return false;
		}
	}

	return true;
}


//----------------------------------------------------------------------------------------------
/**
 * Parse parameters for is_boing subsection
 */
bool ParseBoing( s_jigglebone_t *jiggleInfo )
{
	jiggleInfo->data.flags |= JIGGLE_IS_BOING;

	// default values
	jiggleInfo->data.boingImpactSpeed = 100.0f;
	jiggleInfo->data.boingImpactAngle = 0.7071f;
	jiggleInfo->data.boingDampingRate = 0.25f;
	jiggleInfo->data.boingFrequency = 30.0f;
	jiggleInfo->data.boingAmplitude = 0.35f;

	bool gotOpenBracket = false;	
	while ( true )
	{
		if ( GetToken( true ) == false )
		{
			MdlError( "$jigglebone:is_boing: parse error\n" );
			return false;
		}

		if ( !stricmp( token, "{" ) )
		{
			gotOpenBracket = true;
		}
		else if ( !gotOpenBracket )
		{
			MdlError( "$jigglebone:is_boing: missing '{'\n" );
			return false;
		}
		else if ( !stricmp( token, "}" ) )
		{
			// definition complete
			break;
		}
		else if ( !stricmp( token, "impact_speed" ) )
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.boingImpactSpeed = verify_atof( token );
		}
		else if ( !stricmp( token, "impact_angle" ) )
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.boingImpactAngle = cos( DEG2RAD( verify_atof( token ) ) );
		}
		else if ( !stricmp( token, "damping_rate" ) )
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.boingDampingRate = verify_atof( token );
		}
		else if ( !stricmp( token, "frequency" ) )
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.boingFrequency = verify_atof( token );
		}
		else if ( !stricmp( token, "amplitude" ) )
		{
			if ( !GetToken( false ) )
			{
				return false;
			}

			jiggleInfo->data.boingAmplitude = verify_atof( token );
		}
	}

	return true;
}


//----------------------------------------------------------------------------------------------
/**
 * Parse $jigglebone parameters
 */
void Cmd_JiggleBone( void )
{
	struct s_jigglebone_t *jiggleInfo = &g_jigglebones[ g_numjigglebones ];

	// bone name
	GetToken( false );
	V_strcpy_safe( jiggleInfo->bonename, token );

	// default values
	memset( &jiggleInfo->data, 0, sizeof( mstudiojigglebone_t ) );
	jiggleInfo->data.length = 10.0f;
	jiggleInfo->data.yawStiffness = 100.0f;
	jiggleInfo->data.pitchStiffness = 100.0f;
	jiggleInfo->data.alongStiffness = 100.0f;
	jiggleInfo->data.baseStiffness = 100.0f;
	jiggleInfo->data.baseMinUp = -100.0f;
	jiggleInfo->data.baseMaxUp = 100.0f;
	jiggleInfo->data.baseMinLeft = -100.0f;
	jiggleInfo->data.baseMaxLeft = 100.0f;
	jiggleInfo->data.baseMinForward = -100.0f;
	jiggleInfo->data.baseMaxForward = 100.0f;

	bool gotOpenBracket = false;	
	while (true)
	{
		if (GetToken( true ) == false)
		{
			MdlError( "$jigglebone: parse error\n" );
			return;
		}
		
		if (!stricmp( token, "{" ))
		{
			gotOpenBracket = true;
		}
		else if (!gotOpenBracket)
		{
			MdlError( "$jigglebone: missing '{'\n" );
			return;				
		}
		else if (!stricmp( token, "}" ))
		{
			// definition complete
			break;
		}
		else if (!stricmp( token, "is_flexible" ))
		{
			if (ParseFlexibleJiggle( jiggleInfo ) == false)
			{
				return;
			}
		}
		else if (!stricmp( token, "is_rigid" ))
		{
			if (ParseRigidJiggle( jiggleInfo ) == false)
			{
				return;
			}
		}
		else if (!stricmp( token, "has_base_spring" ))
		{
			if (ParseBaseSpringJiggle( jiggleInfo ) == false)
			{
				return;
			}
		}	
		else if ( !stricmp( token, "is_boing" ) )
		{
			if ( ParseBoing( jiggleInfo ) == false )
			{
				return;
			}
		}	
		else
		{
			MdlError( "$jigglebone: invalid syntax '%s'\n", token );
			return;				
		}
	}

	if (!g_quiet)
		Msg( "Marking bone %s as a jiggle bone\n", jiggleInfo->bonename );

	g_numjigglebones++;
}



//-----------------------------------------------------------------------------
// Purpose: specify bones to store, even if nothing references them
//-----------------------------------------------------------------------------

void Cmd_IncludeModel( )
{
	GetToken( false );
	V_strcpy_safe( g_includemodel[g_numincludemodels].name, "models/" );
	V_strcat_safe( g_includemodel[g_numincludemodels].name, token );
	g_numincludemodels++;
}


/*
=================
=================
*/

void Grab_Vertexanimation( s_source_t *psource, const char *pAnimName )
{
	char	cmd[1024];
	int		index;
	Vector	pos;
	Vector	normal;
	int		t = -1;
	int		count = 0;
	static s_vertanim_t	tmpvanim[MAXSTUDIOVERTS*4];

	s_sourceanim_t *pAnim = FindSourceAnim( psource, pAnimName );
	if ( !pAnim )
	{
		MdlError( "Unknown animation %s(%d) : %s\n", pAnimName, g_iLinecount, g_szLine );
	}

	while (GetLineInput()) 
	{
		if (sscanf( g_szLine, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &normal[0], &normal[1], &normal[2] ) == 7)
		{
			if ( pAnim->startframe < 0 )
			{
				MdlError( "Missing frame start(%d) : %s", g_iLinecount, g_szLine );
			}

			if (t < 0)
			{
				MdlError( "VTA Frame Sync (%d) : %s", g_iLinecount, g_szLine );
			}

			tmpvanim[count].vertex = index;
			VectorCopy( pos, tmpvanim[count].pos );
			VectorCopy( normal, tmpvanim[count].normal );
			count++;

			if ( index >= psource->numvertices )
			{
				psource->numvertices = index + 1;
			}
		}
		else
		{
			// flush data

			if (count)
			{
				pAnim->numvanims[t] = count;

				pAnim->vanim[t] = (s_vertanim_t *)kalloc( count, sizeof( s_vertanim_t ) );

				memcpy( pAnim->vanim[t], tmpvanim, count * sizeof( s_vertanim_t ) );
			}
			else if (t > 0)
			{
				pAnim->numvanims[t] = 0;
			}

			// next command
			if (sscanf( g_szLine, "%1023s %d", cmd, &index ))
			{
				if (stricmp( cmd, "time" ) == 0) 
				{
					t = index;
					count = 0;

					if ( t < pAnim->startframe )
					{
						MdlError( "Frame MdlError(%d) : %s", g_iLinecount, g_szLine );
					}
					if ( t > pAnim->endframe )
					{
						MdlError( "Frame MdlError(%d) : %s", g_iLinecount, g_szLine );
					}

					t -= pAnim->startframe;
				}
				else if ( !Q_stricmp( cmd, "end" ) ) 
				{
					pAnim->numframes = pAnim->endframe - pAnim->startframe + 1;
					return;
				}
				else
				{
					MdlError( "MdlError(%d) : %s", g_iLinecount, g_szLine );
				}
			}
			else
			{
				MdlError( "MdlError(%d) : %s", g_iLinecount, g_szLine );
			}
		}
	}
	MdlError( "unexpected EOF: %s\n", psource->filename );
}

bool GetGlobalFilePath( const char *pSrc, char *pFullPath, int nMaxLen )
{
	char	pFileName[1024];
	Q_strncpy( pFileName, ExpandPath( (char*)pSrc ), sizeof(pFileName) );

	// This is kinda gross. . . doing the same work in cmdlib on SafeOpenRead.
	int nPathLength;
	if( CmdLib_HasBasePath( pFileName, nPathLength ) )
	{
		char tmp[1024];
		int i;

		int nNumBasePaths = CmdLib_GetNumBasePaths();
		for( i = 0; i < nNumBasePaths; i++ )
		{
			V_strcpy_safe( tmp, CmdLib_GetBasePath( i ) );
			V_strcat_safe( tmp, pFileName + nPathLength );

			struct _stat buf;
			int rt = _stat( tmp, &buf );
			if ( rt != -1 && ( buf.st_size > 0 ) && ( ( buf.st_mode & _S_IFDIR ) == 0 ) )
			{
				Q_strncpy( pFullPath, tmp, nMaxLen );
				return true;
			}
		}
		return false;
	}

	struct _stat buf;
	int rt = _stat( pFileName, &buf );
	if ( rt != -1 && ( buf.st_size > 0 ) && ( ( buf.st_mode & _S_IFDIR ) == 0 )	)
	{
		Q_strncpy( pFullPath, pFileName, nMaxLen );
		return true;
	}
	return false;
}


int OpenGlobalFile( char *src )
{
	int		time1;
	char	filename[1024];

	V_strcpy_safe( filename, ExpandPath( src ) );

	int pathLength;
	int numBasePaths = CmdLib_GetNumBasePaths();
	// This is kinda gross. . . doing the same work in cmdlib on SafeOpenRead.
	if( CmdLib_HasBasePath( filename, pathLength ) )
	{
		char tmp[1024];
		int i;
		for( i = 0; i < numBasePaths; i++ )
		{
			V_strcpy_safe( tmp, CmdLib_GetBasePath( i ) );
			V_strcat_safe( tmp, filename + pathLength );
			if( g_bCreateMakefile )
			{
				CreateMakefile_AddDependency( tmp );
				return 0;
			}
			
			time1 = FileTime( tmp );
			if( time1 != -1 )
			{
				if ((g_fpInput = fopen(tmp, "r" ) ) == 0) 
				{
					MdlWarning( "reader: could not open file '%s'\n", src );
					return 0;
				}
				else
				{
					return 1;
				}
			}
		}
		return 0;
	}
	else
	{
		time1 = FileTime (filename);
		if (time1 == -1)
			return 0;

		if( g_bCreateMakefile )
		{
			CreateMakefile_AddDependency( filename );
			return 0;
		}
		if ((g_fpInput = fopen(filename, "r" ) ) == 0) 
		{
			MdlWarning( "reader: could not open file '%s'\n", src );
			return 0;
		}

		return 1;
	}
}



int Load_VTA( s_source_t *psource )
{
	char	cmd[1024];
	int		option;

	if (!OpenGlobalFile( psource->filename ))
		return 0;

	if (!g_quiet)
		printf ("VTA MODEL %s\n", psource->filename);

	g_iLinecount = 0;
	while (GetLineInput()) 
	{
		g_iLinecount++;
		sscanf( g_szLine, "%s %d", cmd, &option );
		if (stricmp( cmd, "version" ) == 0) 
		{
			if (option != 1) 
			{
				MdlError("bad version\n");
			}
		}
		else if (stricmp( cmd, "nodes" ) == 0) 
		{
			psource->numbones = Grab_Nodes( psource->localBone );
		}
		else if (stricmp( cmd, "skeleton" ) == 0) 
		{
			Grab_Animation( psource, "VertexAnimation" );
		}
		else if (stricmp( cmd, "vertexanimation" ) == 0) 
		{
			Grab_Vertexanimation( psource, "VertexAnimation" );
		}
		else 
		{
			MdlWarning("unknown studio command \"%s\"\n", cmd );
		}
	}
	fclose( g_fpInput );

	return 1;
}


void Grab_AxisInterpBones( )
{
	char	cmd[1024], tmp[1025];
	Vector	basepos;
	s_axisinterpbone_t *pAxis = NULL;
	s_axisinterpbone_t *pBone = &g_axisinterpbones[g_numaxisinterpbones];

	while (GetLineInput()) 
	{
		if (IsEnd( g_szLine )) 
		{
			return;
		}
		int i = sscanf( g_szLine, "%1023s \"%[^\"]\" \"%[^\"]\" \"%[^\"]\" \"%[^\"]\" %d", cmd, pBone->bonename, tmp, pBone->controlname, tmp, &pBone->axis );
		if (i == 6 && stricmp( cmd, "bone") == 0)
		{
			// printf( "\"%s\" \"%s\" \"%s\" \"%s\"\n", cmd, pBone->bonename, tmp, pBone->controlname );
			pAxis = pBone;
			pBone->axis = pBone->axis - 1;	// MAX uses 1..3, engine 0..2
			g_numaxisinterpbones++;
			pBone = &g_axisinterpbones[g_numaxisinterpbones];
		}
		else if (stricmp( cmd, "display" ) == 0)
		{
			// skip all display info
		}
		else if (stricmp( cmd, "type" ) == 0)
		{
			// skip all type info
		}
		else if (stricmp( cmd, "basepos" ) == 0)
		{
			i = sscanf( g_szLine, "basepos %f %f %f", &basepos.x, &basepos.y, &basepos.z );
			// skip all type info
		}
		else if (stricmp( cmd, "axis" ) == 0)
		{
			Vector pos;
			QAngle rot;
			int j;
			i = sscanf( g_szLine, "axis %d %f %f %f %f %f %f", &j, &pos[0], &pos[1], &pos[2], &rot[2], &rot[0], &rot[1] );
			if (i == 7)
			{
				VectorAdd( basepos, pos, pAxis->pos[j] );
				AngleQuaternion( rot, pAxis->quat[j] );
			}
		}
	}
}


bool Grab_AimAtBones( )
{
	s_aimatbone_t *pAimAtBone( &g_aimatbones[g_numaimatbones] );

	// Already know it's <aimconstraint> in the first string, otherwise wouldn't be here
	if ( sscanf( g_szLine, "%*s %127s %127s %127s", pAimAtBone->bonename, pAimAtBone->parentname, pAimAtBone->aimname ) == 3 )
	{
		g_numaimatbones++;

		char	cmd[1024];
		Vector	vector;

		while ( GetLineInput() ) 
		{
			g_iLinecount++;

			if (IsEnd( g_szLine )) 
			{
				return false;
			}

			if ( sscanf( g_szLine, "%1024s %f %f %f", cmd, &vector[0], &vector[1], &vector[2] ) != 4 )
			{
				// Allow blank lines to be skipped without error
				bool allSpace( true );
				for ( const char *pC( g_szLine ); *pC != '\0' && pC < ( g_szLine + 4096 ); ++pC )
				{
					if ( !V_isspace( *pC ) )
					{
						allSpace = false;
						break;
					}
				}

				if ( allSpace )
				{
					continue;
				}

				return true;
			}

			if ( stricmp( cmd, "<aimvector>" ) == 0)
			{
				// Make sure these are unit length on read
				VectorNormalize( vector );
				pAimAtBone->aimvector = vector;
			}
			else if ( stricmp( cmd, "<upvector>" ) == 0)
			{
				// Make sure these are unit length on read
				VectorNormalize( vector );
				pAimAtBone->upvector = vector;
			}
			else if ( stricmp( cmd, "<basepos>" ) == 0)
			{
				pAimAtBone->basepos = vector;
			}
			else
			{
				return true;
			}
		}
	}

	// If we get here, we're at EOF
	return false;
}



void Grab_QuatInterpBones( )
{
	char	cmd[1024];
	Vector	basepos;
	RadianEuler	rotateaxis( 0.0f, 0.0f, 0.0f );
	RadianEuler	jointorient( 0.0f, 0.0f, 0.0f );
	s_quatinterpbone_t *pAxis = NULL;
	s_quatinterpbone_t *pBone = &g_quatinterpbones[g_numquatinterpbones];

	while (GetLineInput()) 
	{
		g_iLinecount++;
		if (IsEnd( g_szLine )) 
		{
			return;
		}

		int i = sscanf( g_szLine, "%s %s %s %s %s", cmd, pBone->bonename, pBone->parentname, pBone->controlparentname, pBone->controlname );

		while ( i == 4 && stricmp( cmd, "<aimconstraint>" ) == 0 )
		{
			// If Grab_AimAtBones() returns false, there file is at EOF
			if ( !Grab_AimAtBones() )
			{
				return;
			}

			// Grab_AimAtBones will read input into g_szLine same as here until it gets a line it doesn't understand, at which point
			// it will exit leaving that line in g_szLine, so check for the end and scan the current buffer again and continue on with 
			// the normal QuatInterpBones process

			i = sscanf( g_szLine, "%s %s %s %s %s", cmd, pBone->bonename, pBone->parentname, pBone->controlparentname, pBone->controlname );
		}

		if (i == 5 && stricmp( cmd, "<helper>") == 0)
		{
			// printf( "\"%s\" \"%s\" \"%s\" \"%s\"\n", cmd, pBone->bonename, tmp, pBone->controlname );
			pAxis = pBone;
			g_numquatinterpbones++;
			pBone = &g_quatinterpbones[g_numquatinterpbones];
		}
		else if ( i > 0 )
		{
			// There was a bug before which could cause the same command to be parsed twice
			// because if the sscanf above completely fails, it will return 0 and not 
			// change the contents of cmd, so i should be greater than 0 in order for
			// any of these checks to be valid... Still kind of buggy as these checks
			// do case insensitive stricmp but then the sscanf does case sensitive
			// matching afterwards... Should probably change those to
			// sscanf( g_szLine, "%*s %f ... ) etc...

			if ( stricmp( cmd, "<display>" ) == 0)
			{
				// skip all display info
				Vector size;
				float distance;

				i = sscanf( g_szLine, "<display> %f %f %f %f", 
					&size[0], &size[1], &size[2],
					&distance );

				if (i == 4)
				{
					pAxis->percentage = distance / 100.0;
					pAxis->size = size;
				}
				else
				{
					MdlError( "Line %d: Unable to parse procedual <display> bone: %s", g_iLinecount, g_szLine );
				}
			}
			else if ( stricmp( cmd, "<basepos>" ) == 0)
			{
				i = sscanf( g_szLine, "<basepos> %f %f %f", &basepos.x, &basepos.y, &basepos.z );
				// skip all type info
			}
			else if ( stricmp( cmd, "<rotateaxis>" ) == 0)
			{
				i = sscanf( g_szLine, "%*s %f %f %f", &rotateaxis.x, &rotateaxis.y, &rotateaxis.z );
				rotateaxis.x = DEG2RAD( rotateaxis.x );
				rotateaxis.y = DEG2RAD( rotateaxis.y );
				rotateaxis.z = DEG2RAD( rotateaxis.z );
			}
			else if ( stricmp( cmd, "<jointorient>" ) == 0)
			{
				i = sscanf( g_szLine, "%*s %f %f %f", &jointorient.x, &jointorient.y, &jointorient.z );
				jointorient.x = DEG2RAD( jointorient.x );
				jointorient.y = DEG2RAD( jointorient.y );
				jointorient.z = DEG2RAD( jointorient.z );
			}
			else if ( stricmp( cmd, "<trigger>" ) == 0)
			{
				float tolerance;
				RadianEuler trigger;
				Vector pos;
				RadianEuler ang;

				QAngle rot;
				int j;
				i = sscanf( g_szLine, "<trigger> %f %f %f %f %f %f %f %f %f %f", 
					&tolerance,
					&trigger.x, &trigger.y, &trigger.z,
					&ang.x, &ang.y, &ang.z,
					&pos.x, &pos.y, &pos.z );

				if (i == 10)
				{
					trigger.x = DEG2RAD( trigger.x );
					trigger.y = DEG2RAD( trigger.y );
					trigger.z = DEG2RAD( trigger.z );
					ang.x = DEG2RAD( ang.x );
					ang.y = DEG2RAD( ang.y );
					ang.z = DEG2RAD( ang.z );

					Quaternion q;
					AngleQuaternion( ang, q );

					if ( rotateaxis.x != 0.0 || rotateaxis.y != 0.0 || rotateaxis.z != 0.0 )
					{
						Quaternion q1;
						Quaternion q2;
						AngleQuaternion( rotateaxis, q1 );
						QuaternionMult( q1, q, q2 );
						q = q2;
					}

					if ( jointorient.x != 0.0 || jointorient.y != 0.0 || jointorient.z != 0.0 )
					{
						Quaternion q1;
						Quaternion q2;
						AngleQuaternion( jointorient, q1 );
						QuaternionMult( q, q1, q2 );
						q = q2;
					}

					j = pAxis->numtriggers++;
					pAxis->tolerance[j] = DEG2RAD( tolerance );
					AngleQuaternion( trigger, pAxis->trigger[j] );
					VectorAdd( basepos, pos, pAxis->pos[j] );
					pAxis->quat[j] = q;
				}
				else
				{
					MdlError( "Line %d: Unable to parse procedual <trigger> bone: %s", g_iLinecount, g_szLine );
				}
			}
			else
			{
				MdlError( "Line %d: Unable to parse procedual bone data: %s", g_iLinecount, g_szLine );
			}
		}
		else
		{
			// Allow blank lines to be skipped without error
			bool allSpace( true );
			for ( const char *pC( g_szLine ); *pC != '\0' && pC < ( g_szLine + 4096 ); ++pC )
			{
				if ( !V_isspace( *pC ) )
				{
					allSpace = false;
					break;
				}
			}

			if ( !allSpace )
			{
				MdlError( "Line %d: Unable to parse procedual bone data: %s", g_iLinecount, g_szLine );
			}
		}
	}
}


void Load_ProceduralBones( )
{
	char	filename[256];
	char	cmd[1024];
	int		option;

	GetToken( false );
	V_strcpy_safe( filename, token );

	if (!OpenGlobalFile( filename ))
		return;

	g_iLinecount = 0;

	char ext[32];
	Q_ExtractFileExtension( filename, ext, sizeof( ext ) );

	if (stricmp( ext, "vrd") == 0)
	{
		Grab_QuatInterpBones( );
	}
	else
	{
		while (GetLineInput()) 
		{
			g_iLinecount++;
			sscanf( g_szLine, "%s %d", cmd, &option );
			if (stricmp( cmd, "version" ) == 0) 
			{
				if (option != 1) 
				{
					MdlError("bad version\n");
				}
			}
			else if (stricmp( cmd, "proceduralbones" ) == 0) 
			{
				Grab_AxisInterpBones( );
			}
		}
	}
	fclose( g_fpInput );
}


void Cmd_CD()
{
	if (cdset)
		MdlError ("Two $cd in one model");
	cdset = true;
	GetToken (false);
	V_strcpy_safe (cddir[0], token);
	V_strcat_safe (cddir[0], "/" );
	numdirs = 0;
}


void Cmd_CDMaterials()
{
	while (TokenAvailable())
	{
		GetToken (false);
		
		char szPath[512];
		Q_strncpy( szPath, token, sizeof( szPath ) );

		int len = strlen( szPath );
		if ( len > 0 && szPath[len-1] != '/' && szPath[len-1] != '\\' )
		{
			Q_strncat( szPath, "/", sizeof( szPath ), COPY_ALL_CHARACTERS );
		}

		Q_FixSlashes( szPath );
		cdtextures[numcdtextures] = strdup( szPath );
		numcdtextures++;
	}
}


void Cmd_Pushd()
{
	GetToken(false);

	V_strcpy_safe( cddir[numdirs+1], cddir[numdirs] );
	V_strcat_safe( cddir[numdirs+1], token );
	V_strcat_safe( cddir[numdirs+1], "/" );
	numdirs++;
}

void Cmd_Popd()
{
	if (numdirs > 0)
		numdirs--;
}

void Cmd_CollisionModel()
{
	DoCollisionModel( false );
}

void Cmd_CollisionJoints()
{
	DoCollisionModel( true );
}

void Cmd_ExternalTextures()
{
	MdlWarning( "ignoring $externaltextures, obsolete..." );
}

void Cmd_ClipToTextures()
{
	clip_texcoords = 1;
}

void Cmd_CollapseBones()
{
	g_collapse_bones = true;
}

void Cmd_CollapseBonesAggressive()
{
	g_collapse_bones = true;
	g_collapse_bones_aggressive = true;
}

void Cmd_AlwaysCollapse()
{
	g_collapse_bones = true;
	GetToken(false);
	g_collapse.AddToTail( strdup( token ) );
}

void Cmd_CalcTransitions()
{
	g_bMultistageGraph = true;
}

void Cmd_StaticProp()
{
	g_staticprop = true;
	gflags |= STUDIOHDR_FLAGS_STATIC_PROP;
}

void Cmd_ZBrush()
{
	g_bZBrush = true;
}

void Cmd_RealignBones()
{
	g_realignbones = true;
}

void Cmd_BaseLOD()
{
	Cmd_LOD( "$lod" );
}

void Cmd_KeyValues()
{
	Option_KeyValues( &g_KeyValueText );
}

void Cmd_ConstDirectionalLight()
{
	gflags |= STUDIOHDR_FLAGS_CONSTANT_DIRECTIONAL_LIGHT_DOT;

	GetToken (false);
	g_constdirectionalightdot = (byte)( verify_atof(token) * 255.0f );
}

void Cmd_MinLOD()
{
	GetToken( false );
	g_minLod = atoi( token );

	// "minlod" rules over "allowrootlods"
	if ( g_numAllowedRootLODs > 0 && g_numAllowedRootLODs < g_minLod )
	{
		MdlWarning( "$minlod %d overrides $allowrootlods %d, proceeding with $allowrootlods %d.\n", g_minLod, g_numAllowedRootLODs, g_minLod );
		g_numAllowedRootLODs = g_minLod;
	}
}

void Cmd_AllowRootLODs()
{
	GetToken( false );
	g_numAllowedRootLODs = atoi( token );

	// Root LOD restriction has to obey "minlod" request
	if ( g_numAllowedRootLODs > 0 && g_numAllowedRootLODs < g_minLod )
	{
		MdlWarning( "$allowrootlods %d is conflicting with $minlod %d, proceeding with $allowrootlods %d.\n", g_numAllowedRootLODs, g_minLod, g_minLod );
		g_numAllowedRootLODs = g_minLod;
	}
}


void Cmd_BoneSaveFrame( )
{
	s_bonesaveframe_t tmp;

	// bone name
	GetToken( false );
	V_strcpy_safe( tmp.name, token );

	tmp.bSavePos = false;
	tmp.bSaveRot = false;
	while (TokenAvailable(  ))
	{
		GetToken( false );
		if (stricmp( "position", token ) == 0)
		{
			tmp.bSavePos = true;
		}
		else if (stricmp( "rotation", token ) == 0)
		{
			tmp.bSaveRot = true;
		}
		else
		{
			MdlError( "unknown option \"%s\" on $bonesaveframe : %s\n", token, tmp.name );
		}
	}

	g_bonesaveframe.AddToTail( tmp );
}


//
// This is the master list of the commands a QC file supports.
// To add a new command to the QC files, add it here.
//
struct
{
	char *m_pName;
	void (*m_pCmd)();
} g_Commands[] =
{
	{ "$cd", Cmd_CD },
	{ "$modelname", Cmd_Modelname },
	{ "$cdmaterials", Cmd_CDMaterials },
	{ "$pushd", Cmd_Pushd },
	{ "$popd", Cmd_Popd },
	{ "$scale", Cmd_ScaleUp },
	{ "$root", Cmd_Root },
	{ "$controller", Cmd_Controller },
	{ "$screenalign", Cmd_ScreenAlign },
	{ "$model", Cmd_Model },
	{ "$collisionmodel", Cmd_CollisionModel },
	{ "$collisionjoints", Cmd_CollisionJoints },
	{ "$collisiontext", Cmd_CollisionText },
	{ "$body", Cmd_Body },
	{ "$bodygroup", Cmd_Bodygroup },
	{ "$animation", Cmd_Animation },
	{ "$autocenter", Cmd_Autocenter },
	{ "$sequence", Cmd_Sequence },
	{ "$append", Cmd_Append },
	{ "$prepend", Cmd_Prepend  },
	{ "$continue", Cmd_Continue  },
	{ "$declaresequence", Cmd_DeclareSequence  },
	{ "$declareanimation", Cmd_DeclareAnimation },
	{ "$cmdlist", Cmd_Cmdlist },
	{ "$animblocksize", Cmd_AnimBlockSize },
	{ "$weightlist", Cmd_Weightlist },
	{ "$defaultweightlist", Cmd_DefaultWeightlist },
	{ "$ikchain", Cmd_IKChain },
	{ "$ikautoplaylock", Cmd_IKAutoplayLock },
	{ "$eyeposition", Cmd_Eyeposition },
	{ "$illumposition", Cmd_Illumposition },
	{ "$origin", Cmd_Origin },
	{ "$upaxis", Cmd_UpAxis },
	{ "$bbox", Cmd_BBox },
	{ "$cbox", Cmd_CBox },
	{ "$gamma", Cmd_Gamma },
	{ "$texturegroup", Cmd_TextureGroup },
	{ "$hgroup", Cmd_Hitgroup },
	{ "$hbox", Cmd_Hitbox },
	{ "$hboxset", Cmd_HitboxSet },
	{ "$surfaceprop", Cmd_SurfaceProp },
	{ "$jointsurfaceprop", Cmd_JointSurfaceProp },
	{ "$contents", Cmd_Contents },
	{ "$jointcontents", Cmd_JointContents },
	{ "$attachment", Cmd_Attachment },
	{ "$bonemerge", Cmd_BoneMerge },
	{ "$externaltextures", Cmd_ExternalTextures },
	{ "$cliptotextures", Cmd_ClipToTextures },
	{ "$renamebone", Cmd_Renamebone },
	{ "$collapsebones", Cmd_CollapseBones },
	{ "$collapsebonesaggressive", Cmd_CollapseBonesAggressive },
	{ "$alwayscollapse", Cmd_AlwaysCollapse },
	{ "$proceduralbones", Load_ProceduralBones },
	{ "$skiptransition", Cmd_Skiptransition },
	{ "$calctransitions", Cmd_CalcTransitions },
	{ "$staticprop", Cmd_StaticProp },
	{ "$zbrush", Cmd_ZBrush },
	{ "$realignbones", Cmd_RealignBones },
	{ "$forcerealign", Cmd_ForceRealign },
	{ "$lod", Cmd_BaseLOD },
	{ "$shadowlod", Cmd_ShadowLOD },
	{ "$poseparameter", Cmd_PoseParameter },
	{ "$heirarchy", Cmd_ForcedHierarchy },
	{ "$hierarchy", Cmd_ForcedHierarchy },
	{ "$insertbone", Cmd_InsertHierarchy },
	{ "$limitrotation", Cmd_LimitRotation },
	{ "$definebone", Cmd_DefineBone },
	{ "$jigglebone", Cmd_JiggleBone },
	{ "$includemodel", Cmd_IncludeModel },
	{ "$opaque", Cmd_Opaque },
	{ "$mostlyopaque", Cmd_TranslucentTwoPass },
//	{ "$platform", Cmd_Platform },
	{ "$keyvalues", Cmd_KeyValues },
	{ "$obsolete", Cmd_Obsolete },
	{ "$renamematerial", Cmd_RenameMaterial },
	{ "$fakevta", Cmd_FakeVTA },
	{ "$noforcedfade", Cmd_NoForcedFade },
	{ "$skipboneinbbox", Cmd_SkipBoneInBBox },
	{ "$forcephonemecrossfade", Cmd_ForcePhonemeCrossfade },
	{ "$lockbonelengths", Cmd_LockBoneLengths },
	{ "$unlockdefinebones", Cmd_UnlockDefineBones },
	{ "$constantdirectionallight", Cmd_ConstDirectionalLight },
	{ "$minlod", Cmd_MinLOD },
	{ "$allowrootlods", Cmd_AllowRootLODs },
	{ "$bonesaveframe", Cmd_BoneSaveFrame },
	{ "$ambientboost", Cmd_AmbientBoost },
	{ "$centerbonesonverts", Cmd_CenterBonesOnVerts },
	{ "$donotcastshadows", Cmd_DoNotCastShadows },
	{ "$casttextureshadows", Cmd_CastTextureShadows },
	{ "$motionrollback", Cmd_MotionExtractionRollBack },
	{ "$sectionframes", Cmd_SectionFrames },
	{ "$clampworldspace", Cmd_ClampWorldspace },
	{ "$maxeyedeflection", Cmd_MaxEyeDeflection },
	{ "$boneflexdriver", Cmd_BoneFlexDriver },
	{ "$checkuv", Cmd_CheckUV }
};


/*
===============
ParseScript
===============
*/
void ParseScript (void)
{
	while (1)
	{
		GetToken (true);
		if (endofscript)
			return;

		// Check all the commands we know about.
		int i;
		for ( i=0; i < ARRAYSIZE( g_Commands ); i++ )
		{
			if ( !stricmp( g_Commands[i].m_pName, token ) )
			{
				g_Commands[i].m_pCmd();
				break;
			}
		}
		if ( i == ARRAYSIZE( g_Commands ) )
		{
			if( !g_bCreateMakefile )
			{
				TokenError("bad command %s\n", token);
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Generate the model name
//-----------------------------------------------------------------------------
bool GenerateModelName( CDmeMDLMakefile *pMDLMakeFile )
{
	// The model name is implicit in the makefile name
	// NOTE: Model name is relative to the 'models' directory
	char pOutputFullPath[MAX_PATH];
	pMDLMakeFile->GetOutputName( pOutputFullPath, sizeof(pOutputFullPath) );
	Q_SetExtension( pOutputFullPath, ".mdl", sizeof( pOutputFullPath) );

	char pModelSubDir[MAX_PATH];
	GetModSubdirectory( "models", pModelSubDir, sizeof(pModelSubDir) );

	char pRelativePath[MAX_PATH];
	if ( !Q_MakeRelativePath( pOutputFullPath, pModelSubDir, pRelativePath, sizeof(pRelativePath) ) )
	{
		MdlError( "Makefile \"%s\" doesn't lie under the correct vproject \"%s\"!\n",
			pOutputFullPath, pModelSubDir );
		return false;
	}

	ProcessModelName( pRelativePath );
	return true;
}


//-----------------------------------------------------------------------------
// Process skins
//-----------------------------------------------------------------------------
bool GenerateSkin( CDmeMDLMakefile *pMDLMakeFile )
{
	CUtlVector< CDmeHandle< CDmeSourceSkin > > bodies;
	pMDLMakeFile->GetSources< CDmeSourceSkin >( bodies );
	int nCount = bodies.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !bodies[i] )
			continue;

		char pFullPath[MAX_PATH];
		pMDLMakeFile->GetSourceFullPath( bodies[i], pFullPath, sizeof(pFullPath) );

		// Empty strings are ignored
		if ( !pFullPath[0] )
			continue;

		ProcessCmdBody( pFullPath, bodies[i] );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Process animations
//-----------------------------------------------------------------------------
bool GenerateAnimations( CDmeMDLMakefile *pMDLMakeFile )
{
	CUtlVector< CDmeHandle< CDmeSourceAnimation > > animationFiles;
	pMDLMakeFile->GetSources< CDmeSourceAnimation >( animationFiles );

	int nCount = animationFiles.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !animationFiles[i] )
			continue;

		char pFullPath[MAX_PATH];
		pMDLMakeFile->GetSourceFullPath( animationFiles[i], pFullPath, sizeof(pFullPath) );

		// Empty strings are ignored
		if ( !pFullPath[0] )
			continue;

		// Totally spoof the animation info.. not sure where to get it from yet
		// assume it's an animation reference
		// first look up an existing animation
		s_sequence_t *pseq = ProcessCmdSequence( animationFiles[i]->m_AnimationName );
		if ( !pseq )
			continue;

		int n;
		s_animation_t *animations[64];
		int numblends = 0;
		for ( n = 0; n < g_numani; n++ )
		{
			if ( !Q_stricmp( pFullPath, g_panimation[n]->name ) )
			{
				animations[numblends++] = g_panimation[n];
				break;
			}
		}

		if ( n >= g_numani )
		{
			// assume it's an implied animation
			animations[numblends++] = Cmd_ImpliedAnimation( pseq, pFullPath );
		}
		// hack to allow animation commands to refer to same sequence
		if ( numblends == 1 )
		{
			pseq->panim[0][0] = animations[0];
		}

		// Look up the source animation from the animation name
		for ( int j = 0; j < numblends; ++j )
		{
			s_sourceanim_t *pSourceAnim = FindSourceAnim( animations[j]->source, animationFiles[i]->m_SourceAnimationName );

			// NOTE: This always affects the first source anim read in
			if ( pSourceAnim )
			{
				animations[j]->startframe = pSourceAnim->startframe;
				animations[j]->endframe = pSourceAnim->endframe;

				if ( !g_bCreateMakefile && animations[j]->endframe < animations[j]->startframe )
				{
					TokenError( "end frame before start frame in %s", animations[j]->name );
				}

				animations[j]->numframes = animations[j]->endframe - animations[j]->startframe + 1;
				Q_strncpy( animations[j]->animationname, animationFiles[i]->m_SourceAnimationName, sizeof(animations[j]->animationname) );
			}
			else
			{
				MdlError( "Requested unknown animation block name %s\n", animationFiles[i]->m_SourceAnimationName.Get() );
			}
		}

		ProcessSequence( pseq, numblends, animations, false );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Parse the MDL makefile
//-----------------------------------------------------------------------------
void ParseMDLMakeFile( CDmeMDLMakefile *pMDLMakeFile )
{
	if ( !GenerateModelName( pMDLMakeFile ) )
		return;

	// All DMX files have Y as the up axis
	RadianEuler angles( M_PI / 2.0f, 0.0f, M_PI / 2.0f );
	ProcessUpAxis( angles );

	// Process bodies
	if ( !GenerateSkin( pMDLMakeFile ) )
		return;

	// Process animations
	if ( !GenerateAnimations( pMDLMakeFile ) )
		return;
}


// Used by the CheckSurfaceProps.py script.
// They specify the .mdl file and it prints out all the surface props that the model uses.
bool HandlePrintSurfaceProps( int &returnValue )
{
	const char *pFilename = CommandLine()->ParmValue( "-PrintSurfaceProps", (const char*)NULL );
	if ( pFilename )
	{
		CUtlVector<char> buf;

		FILE *fp = fopen( pFilename, "rb" );
		if ( fp )
		{
			fseek( fp, 0, SEEK_END );
			buf.SetSize( ftell( fp ) );
			fseek( fp, 0, SEEK_SET );
			fread( buf.Base(), 1, buf.Count(), fp );

			fclose( fp );

			studiohdr_t *pHdr = (studiohdr_t*)buf.Base();

			Studio_ConvertStudioHdrToNewVersion( pHdr );

			if ( pHdr->version == STUDIO_VERSION )
			{
				for ( int i=0; i < pHdr->numbones; i++ )
				{
					mstudiobone_t *pBone = pHdr->pBone( i );
					printf( "%s\n", pBone->pszSurfaceProp() );
				}

				returnValue = 0;
			}
			else
			{
				printf( "-PrintSurfaceProps: '%s' is wrong version (%d should be %d).\n", 
					pFilename, pHdr->version, STUDIO_VERSION );
				returnValue = 1;
			}
		}
		else
		{
			printf( "-PrintSurfaceProps: can't open '%s'\n", pFilename );
			returnValue = 1;
		}

		return true;
	}
	else
	{
		return false;
	}
}

// Used by the modelstats.pl script.
// They specify the .mdl file and it prints out perf info.
bool HandleMdlReport( int &returnValue )
{
	const char *pFilename = CommandLine()->ParmValue( "-mdlreport", (const char*)NULL );
	if ( pFilename )
	{
		CUtlVector<char> buf;

		FILE *fp = fopen( pFilename, "rb" );
		if ( fp )
		{
			fseek( fp, 0, SEEK_END );
			buf.SetSize( ftell( fp ) );
			fseek( fp, 0, SEEK_SET );
			fread( buf.Base(), 1, buf.Count(), fp );

			fclose( fp );

			studiohdr_t *pHdr = (studiohdr_t*)buf.Base();

			Studio_ConvertStudioHdrToNewVersion( pHdr );

			if ( pHdr->version == STUDIO_VERSION )
			{
				int flags = SPEWPERFSTATS_SHOWPERF;
				if( CommandLine()->CheckParm( "-mdlreportspreadsheet", NULL ) )
				{
					flags |= SPEWPERFSTATS_SPREADSHEET;
				}
				SpewPerfStats( pHdr, pFilename, flags );

				returnValue = 0;
			}
			else
			{
				printf( "-mdlreport: '%s' is wrong version (%d should be %d).\n", 
					pFilename, pHdr->version, STUDIO_VERSION );
				returnValue = 1;
			}
		}
		else
		{
			printf( "-mdlreport: can't open '%s'\n", pFilename );
			returnValue = 1;
		}

		return true;
	}
	else
	{
		return false;
	}
}

void UsageAndExit()
{
	MdlError( "Bad or missing options\n"
		"usage: studiomdl [options] <file.qc>\n"
		"options:\n"
		"[-a <normal_blend_angle>]\n"
		"[-checklengths]\n"
		"[-d] - dump glview files\n"
		"[-definebones]\n"
		"[-f] - flip all triangles\n"
		"[-fullcollide] - don't truncate really big collisionmodels\n"
		"[-game <gamedir>]\n"
		"[-h] - dump hboxes\n"
		"[-i] - ignore warnings\n"
		"[-minlod <lod>] - truncate to highest detail <lod>\n"
		"[-n] - tag bad normals\n"
		"[-perf] report perf info upon compiling model\n"
		"[-printbones]\n"
		"[-printgraph]\n"
		"[-quiet] - operate silently\n"
		"[-r] - tag reversed\n"
		"[-t <texture>]\n"
		"[-x360] - generate xbox360 output\n"
		"[-nox360] - disable xbox360 output(default)\n"
		"[-nowarnings] - disable warnings\n"
		"[-dumpmaterials] - dump out material names\n"
		"[-mdlreport] model.mdl - report perf info\n"
		"[-mdlreportspreadsheet] - report perf info as a comma-delimited spreadsheet\n"
		"[-striplods] - use only lod0\n"
		"[-overridedefinebones] - equivalent to specifying $unlockdefinebones in .qc file\n"
		"[-stripmodel] - process binary model files and strip extra lod data\n"
		"[-stripvhv] - strip hardware verts to match the stripped model\n"
		"[-vsi] - generate stripping information .vsi file - can be used on .mdl files too\n"
		);
}

#ifndef _DEBUG

LONG __stdcall VExceptionFilter( struct _EXCEPTION_POINTERS *ExceptionInfo )
{
	MdlExceptionFilter( ExceptionInfo->ExceptionRecord->ExceptionCode );
	return EXCEPTION_EXECUTE_HANDLER; // (never gets here anyway)
}

#endif
/*
==============
main
==============
*/


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CStudioMDLApp : public CDefaultAppSystemGroup< CSteamAppSystemGroup >
{
	typedef CDefaultAppSystemGroup< CSteamAppSystemGroup > BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown();

private:
	int Main_StripModel();
	int Main_StripVhv();
	int Main_MakeVsi();

private:
	bool ParseArguments();
};

static bool CStudioMDLApp_SuggestGameInfoDirFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	const char *pProcessFileName = NULL;
	int nParmCount = CommandLine()->ParmCount();
	if ( nParmCount > 1 )
	{
		pProcessFileName = CommandLine()->GetParm( nParmCount - 1 );
	}

	if ( pProcessFileName )
	{
		Q_MakeAbsolutePath( pchPathBuffer, nBufferLength, pProcessFileName );

		if ( pbBubbleDirectories )
			*pbBubbleDirectories = true;

		return true;
	}

	return false;
}

int main( int argc, char **argv )
{
	SetSuggestGameInfoDirFn( CStudioMDLApp_SuggestGameInfoDirFn );

	CStudioMDLApp s_ApplicationObject;
	CSteamApplication s_SteamApplicationObject( &s_ApplicationObject );
	return AppMain( argc, argv, &s_SteamApplicationObject );
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CStudioMDLApp::Create()
{
	InstallSpewFunction();
	// override the default spew function
	SpewOutputFunc( MdlSpewOutputFunc );

 	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

#ifndef _DEBUG
	SetUnhandledExceptionFilter( VExceptionFilter );
#endif

	if ( CommandLine()->ParmCount() == 1 )
	{
		UsageAndExit();
		return false;
	}

	int nReturnValue;
	if ( HandlePrintSurfaceProps( nReturnValue ) )
		return false;

	if ( !ParseArguments() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "vstdlib.dll",			PROCESS_UTILS_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "mdllib.dll",				MDLLIB_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};
	
	AddSystem( g_pDataModel, VDATAMODEL_INTERFACE_VERSION );
	AddSystem( g_pDmElementFramework, VDMELEMENTFRAMEWORK_VERSION );
	AddSystem( g_pDmSerializers, DMSERIALIZERS_INTERFACE_VERSION );

	// Add in the locally-defined studio data cache
	AppModule_t	studioDataCacheModule = LoadModule( Sys_GetFactoryThis() );
	AddSystem( studioDataCacheModule, STUDIO_DATA_CACHE_INTERFACE_VERSION );

	// Add the P4 module separately so that if it is absent (say in the SDK) then the other system will initialize properly
	if ( !CommandLine()->FindParm( "-nop4" ) )
	{
		AppModule_t p4Module = LoadModule( "p4lib.dll" );
		AddSystem( p4Module, P4_INTERFACE_VERSION );
	}

	bool bOk = AddSystems( appSystems );
	if ( !bOk )
		return false;

	IMaterialSystem *pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
		return false;

	pMaterialSystem->SetShaderAPI( "shaderapiempty.dll" );

	return true;
}

bool CStudioMDLApp::PreInit( )
{
	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	ConnectTier3Libraries( &factory, 1 );

	if ( !g_pFullFileSystem || !g_pDataModel || !g_pMaterialSystem || !g_pStudioRender )
	{
		Warning( "StudioMDL is missing a required interface!\n" );
		return false;
	}

	if ( !SetupSearchPaths( g_path, false, true ) )
		return false;

	// NOTE: This is necessary to get the cmdlib filesystem stuff to work.
	g_pFileSystem = g_pFullFileSystem;

	// NOTE: This is stuff copied out of cmdlib necessary to get 
	// the tools in cmdlib working
	FileSystem_SetupStandardDirectories( g_path, GetGameInfoPath() );
	return true;
}


void CStudioMDLApp::PostShutdown()
{
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}


//-----------------------------------------------------------------------------
// Method which parses arguments
//-----------------------------------------------------------------------------
bool CStudioMDLApp::ParseArguments()
{
	g_currentscale = g_defaultscale = 1.0;
	g_defaultrotation = RadianEuler( 0, 0, M_PI / 2 );

	// skip weightlist 0
	g_numweightlist = 1;

	eyeposition = Vector( 0, 0, 0 );
	gflags = 0;
	numrep = 0;
	tag_reversed = 0;
	tag_normals = 0;

	normal_blend = cos( DEG2RAD( 2.0 ));

	g_gamma = 2.2;

	g_staticprop = false;
	g_centerstaticprop = false;

	g_realignbones = false;
	g_constdirectionalightdot = 0;

	g_bDumpGLViewFiles = false;
	g_quiet = false;

	g_illumpositionattachment = 0;
	g_flMaxEyeDeflection = 0.0f;

	int argc = CommandLine()->ParmCount();
	int i;
	for ( i = 1; i < argc - 1; i++ ) 
	{
		const char *pArgv = CommandLine()->GetParm( i );
		if ( pArgv[0] != '-' ) 
			continue;

		if ( !Q_stricmp( pArgv, "-allowdebug" ) )
		{
			// Ignore, used by interface system to catch debug builds checked into release tree
			continue;
		}

		if ( !Q_stricmp( pArgv, "-mdlreport" ) )
		{
			// Will reparse later, ignore rest of arguments.
			return true;
		}

		if ( !Q_stricmp( pArgv, "-mdlreportspreadsheet" ) )
		{
			// Will reparse later, ignore for now.
			continue;
		}

		if ( !Q_stricmp( pArgv, "-ihvtest" ) )
		{
			++i;
			g_IHVTest = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-overridedefinebones" ) )
		{
			g_bOverridePreDefinedBones = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-striplods" ) )
		{
			g_bStripLods = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-stripmodel" ) )
		{
			g_eRunMode = RUN_MODE_STRIP_MODEL;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-stripvhv" ) )
		{
			g_eRunMode = RUN_MODE_STRIP_VHV;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-vsi" ) )
		{
			g_bMakeVsi = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-quiet" ) )
		{
			g_quiet = true;
			g_verbose = false;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-verbose" ) )
		{
			g_quiet = false;
			g_verbose = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-fullcollide" ) )
		{
			g_badCollide = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-checklengths" ) )
		{
			g_bCheckLengths = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-printbones" ) )
		{
			g_bPrintBones = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-perf" ) )
		{
			g_bPerf = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-printgraph" ) )
		{
			g_bDumpGraph = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-definebones" ) )
		{
			g_definebones = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-makefile" ) )
		{
			g_bCreateMakefile = true;
			g_quiet = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-verify" ) )
		{
			g_bVerifyOnly = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-minlod" ) )
		{
			g_minLod = atoi( CommandLine()->GetParm( ++i ) );
			continue;
		}

		if (!Q_stricmp( pArgv, "-x360"))
		{
			StudioByteSwap::ActivateByteSwapping( true ); // Set target to big endian
			g_bX360  = true;
			continue;
		}

		if (!Q_stricmp( pArgv, "-nox360"))
		{
			g_bX360  = false;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-nowarnings" ) )
		{
			g_bNoWarnings = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-maxwarnings" ) )
		{
			g_maxWarnings = atoi( CommandLine()->GetParm( ++i ) );
			continue;
		}

		if ( !Q_stricmp( pArgv, "-preview" ) )
		{
			g_bBuildPreview = true;
			continue;
		}

		if ( !Q_stricmp( pArgv, "-dumpmaterials" ) )
		{
			g_bDumpMaterials = true;
			continue;
		}

		if ( pArgv[1] && pArgv[2] == '\0' )
		{
			switch( pArgv[1] )
			{
			case 't':
				i++;
				V_strcpy_safe( defaulttexture[numrep], pArgv );
				if (i < argc - 2 && CommandLine()->GetParm(i + 1)[0] != '-') 
				{
					i++;
					V_strcpy_safe( sourcetexture[numrep], pArgv );
					printf("Replacing %s with %s\n", sourcetexture[numrep], defaulttexture[numrep] );
				}
				printf( "Using default texture: %s\n", defaulttexture[numrep] );
				numrep++;
				break;
			case 'r':
				tag_reversed = 1;
				break;
			case 'n':
				tag_normals = 1;
				break;
			case 'a':
				i++;
				normal_blend = cos( DEG2RAD( verify_atof( pArgv ) ) );
				break;
			case 'h':
				dump_hboxes = 1;
				break;
			case 'i':
				ignore_warnings = 1;
				break;
			case 'd':
				g_bDumpGLViewFiles = true;
				break;
//			case 'p':
//				i++;
//				V_strcpy_safe( qproject, pArgv );
//				break;
			}
		}
	}	

	if ( i >= argc )
	{
		// misformed arguments
		// otherwise generating unintended results
		UsageAndExit();
		return false;
	}
	
	const char *pArgv = CommandLine()->GetParm( i );
	Q_strncpy( g_path, pArgv, sizeof(g_path) );
	if ( Q_IsAbsolutePath( g_path ) )
	{
		// Set the working directory to be the path of the qc file
		// so the relative-file fopen code works
		char pQCDir[MAX_PATH];
		Q_ExtractFilePath( g_path, pQCDir, sizeof(pQCDir) );
		_chdir( pQCDir );
	}
	Q_StripExtension( pArgv, outname, sizeof( outname ) );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: search through the "GamePath" key and create a mirrored version in the content path searches
//-----------------------------------------------------------------------------

void AddContentPaths( )
{
	// look for the "content" in the path to the initial QC file
	char *match = "content\\";
	char *sp = strstr( qdir, match );
	if (!sp)
		return;

	// copy off everything before and including "content"
	char pre[1024];
	strncpy( pre, qdir, sp - qdir + strlen( match ) );
	pre[sp - qdir + strlen( match )] = '\0';
	sp = sp + strlen( match );

	// copy off everything folling the word after "content"
	char post[1024];
	sp = strstr( sp+1, "\\" );
	V_strcpy_safe( post, sp );

	// get a copy of the game search paths
	char paths[1024];
	g_pFullFileSystem->GetSearchPath( "GAME", false, paths, sizeof( paths ) );
	if (!g_quiet)
		printf("all paths:%s\n", paths );

	// pull out the game names and insert them into a content path string
	sp = strstr( paths, "game\\" );
	while (sp)
	{
		char temp[1024];
		sp = sp + 5;
		char *sz = strstr( sp, "\\" );
		if (!sz)
			return;

		V_strcpy_safe( temp, pre );
		strncat( temp, sp, sz - sp );
		V_strcat_safe( temp, post );
		sp = sz;
		sp = strstr( sp, "game\\" );
		CmdLib_AddBasePath( temp );
		if (!g_quiet)
			printf("content:%s\n", temp );
	}
}


	
//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CStudioMDLApp::Main()
{
	const bool bP4DLLExists = g_pFullFileSystem->FileExists( "p4lib.dll", "EXECUTABLE_PATH" );

	// No p4 mode if specified on the command line or no p4lib.dll found
	if ( ( CommandLine()->FindParm( "-nop4" ) ) || ( !bP4DLLExists ) )
	{
		g_bNoP4 = true;
		g_p4factory->SetDummyMode( true );
	}

	// Set the named changelist
	g_p4factory->SetOpenFileChangeList( "StudioMDL Auto Checkout" );

	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	MaterialSystem_Config_t config;
	g_pMaterialSystem->OverrideConfig( config, false );

	int nReturnValue;
	if ( HandleMdlReport( nReturnValue ) )
		return false;

	// Don't bother with undo here
	g_pDataModel->SetUndoEnabled( false );

	// look for the "content\hl2x" string in the qdir and add what should be the correct path as an alternate
	// FIXME: add these to an envvar if folks are using complicated directory mappings instead of defaults
	char *match = "content\\hl2x\\";
	char *sp = strstr( qdir, match );
	if (sp)
	{
		char temp[1024];
		strncpy( temp, qdir, sp - qdir + strlen( match ) );
		temp[sp - qdir + strlen( match )] = '\0';
		CmdLib_AddBasePath( temp );
		V_strcat_safe( temp, "..\\..\\..\\..\\main\\content\\hl2\\" );
		CmdLib_AddBasePath( temp );
	}

	AddContentPaths();

	if (!g_quiet)
	{
		printf("qdir:    \"%s\"\n", qdir );
		printf("gamedir: \"%s\"\n", gamedir );
		printf("g_path:  \"%s\"\n", g_path );
	}

	switch ( g_eRunMode )
	{
	case RUN_MODE_STRIP_MODEL:
		return Main_StripModel();
	
	case RUN_MODE_STRIP_VHV:
		return Main_StripVhv();
	
	case RUN_MODE_BUILD:
	default:
		break;
	}

	const char *pExt = Q_GetFileExtension( g_path );

	// Look for the presence of a .mdl file (only -vsi is currently supported for .mdl files)
	if ( pExt && !Q_stricmp( pExt, "mdl" ) )
	{
		if ( g_bMakeVsi )
			return Main_MakeVsi();
		
		printf( "ERROR: .qc or .dmx file should be specified to build.\n" );
		return 1;
	}


	if ( !g_quiet )
		printf( "Building binary model files...\n" );

	// Look for the presence of a .dmx file of the same name
	// If so, load it first
	CDmeMDLMakefile *pMDLMakeFile = NULL;
	
	if ( pExt && !Q_stricmp( pExt, "dmx" ) )
	{
		CDmElement *pRoot;
		if ( g_pDataModel->RestoreFromFile( g_path, NULL, NULL, &pRoot ) != DMFILEID_INVALID )
		{
			pMDLMakeFile = CastElement<CDmeMDLMakefile>( pRoot );
		}
	};

	Q_FileBase( g_path, g_path, sizeof( g_path ) );
	Q_DefaultExtension( g_path, pMDLMakeFile ? ".dmx" : ".qc", sizeof( g_path ) );
	if (!g_quiet)
	{
		printf( "Working on \"%s\"\n", g_path );
	}

	// Turn on checking for special single character tokens while parsing
	SetCheckSingleCharTokens( true );
	SetSingleCharTokenList( "{}()," );

	// Set up script loading callback, discarding default callback
	( void ) SetScriptLoadedCallback( StudioMdl_ScriptLoadedCallback );

	// load the script
	if ( !pMDLMakeFile )
	{
		LoadScriptFile(g_path);
	}

	V_strcpy_safe( fullpath, g_path );
	V_strcpy_safe( fullpath, ExpandPath( fullpath ) );
	V_strcpy_safe( fullpath, ExpandArg( fullpath ) );
	
	// default to having one entry in the LOD list that doesn't do anything so
	// that we don't have to do any special cases for the first LOD.
	g_ScriptLODs.Purge();
	g_ScriptLODs.AddToTail(); // add an empty one
	g_ScriptLODs[0].switchValue = 0.0f;
	
	//
	// parse it
	//
	ClearModel();

//	V_strcpy_safe( g_pPlatformName, "" );
	if ( pMDLMakeFile )
	{
		ParseMDLMakeFile( pMDLMakeFile );
	}
	else
	{
		ParseScript();
	}

	if ( !g_bCreateMakefile )
	{
		SetSkinValues();

		SimplifyModel();

		ConsistencyCheckSurfaceProp();
		ConsistencyCheckContents();

		CollisionModel_Build();

		// ValidateSharedAnimationGroups();

		WriteModelFiles();
	}

	if ( pMDLMakeFile )
	{
		DestroyElement( pMDLMakeFile );
		pMDLMakeFile = NULL;
	}

	if ( g_bCreateMakefile )
	{
		CreateMakefile_OutputMakefile();
	}
	else if ( g_bMakeVsi )
	{
		Q_snprintf( g_path, ARRAYSIZE( g_path ), "%smodels/%s", gamedir, outname );
		Main_MakeVsi();
	}

	if (!g_quiet)
	{
		printf("\nCompleted \"%s\"\n", g_path);
	}

	g_pDataModel->UnloadFile( DMFILEID_INVALID );

	return 0;
}





//
// WriteFileToDisk
//	Equivalent to g_pFullFileSystem->WriteFile( pFileName, pPath, buf ), but works
//	for relative paths.
//
bool WriteFileToDisk( const char *pFileName, const char *pPath, CUtlBuffer &buf )
{
	// For some reason calling full filesystem will write into hl2 root dir
	// return g_pFullFileSystem->WriteFile( pFileName, pPath, buf );

	FILE *f = fopen( pFileName, "wb" );
	if ( !f )
		return false;

	fwrite( buf.Base(), 1, buf.TellPut(), f );
	fclose( f );
	return true;
}

//
// WriteBufferToFile
//	Helper to concatenate file base and extension.
//
bool WriteBufferToFile( CUtlBuffer &buf, const char *szFilebase, const char *szExt )
{
	char szFilename[ 1024 ];
	Q_snprintf( szFilename, ARRAYSIZE( szFilename ), "%s%s", szFilebase, szExt );
	return WriteFileToDisk( szFilename, NULL, buf );
}


//
// LoadBufferFromFile
//	Loads the buffer from file, return true on success, false otherwise.
//  If bError is true prints an error upon failure.
//
bool LoadBufferFromFile( CUtlBuffer &buffer, char const *szFilebase, char const *szExt, bool bError = true )
{
	char szFilename[1024];
	Q_snprintf( szFilename, ARRAYSIZE( szFilename ), "%s%s", szFilebase, szExt );

	if ( g_pFullFileSystem->ReadFile( szFilename, NULL, buffer ) )
		return true;

	if ( bError )
		MdlError( "Failed to open '%s'!\n", szFilename );

	return false;
}


bool Load3ModelBuffers( CUtlBuffer &bufMDL, CUtlBuffer &bufVVD, CUtlBuffer &bufVTX, char const *szFilebase )
{
	// Load up the mdl file
	if ( !LoadBufferFromFile( bufMDL, szFilebase, ".mdl" ) )
		return false;

	// Load up the vvd file
	if ( !LoadBufferFromFile( bufVVD, szFilebase, ".vvd" ) )
		return false;

	// Load up the dx90.vtx file
	if ( !LoadBufferFromFile( bufVTX, szFilebase, ".dx90.vtx" ) )
		return false;

	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Studiomdl hooks to call the stripping routines:
//	Main_StripVhv
//	Main_StripModel
//
//////////////////////////////////////////////////////////////////////////

int CStudioMDLApp::Main_StripVhv()
{
	if ( !g_quiet )
	{
		printf( "Stripping vhv data...\n" );
	}

	if ( !mdllib )
	{
		printf( "ERROR: mdllib is not available!\n" );
		return 1;
	}

	Q_StripExtension( g_path, g_path, sizeof( g_path ) );
	char *pExt = g_path + strlen( g_path );
	*pExt = 0;

	//
	// ====== Load files
	//

	// Load up the vhv file
	CUtlBuffer bufVHV;
	if ( !LoadBufferFromFile( bufVHV, g_path, ".vhv" ) )
		return 1;

	// Load up the info.strip file
	CUtlBuffer bufRemapping;
	if ( !LoadBufferFromFile( bufRemapping, g_path, ".info.strip", false ) &&
		 !LoadBufferFromFile( bufRemapping, g_path, ".vsi" ) )
		return 1;

	//
	// ====== Process file contents
	//

	bool bResult = false;
	{
		SpewActivate( "mdllib", 3 );

		IMdlStripInfo *pMdlStripInfo = NULL;
		
		if ( mdllib->CreateNewStripInfo( &pMdlStripInfo ) )
		{
			pMdlStripInfo->UnSerialize( bufRemapping );
			bResult = pMdlStripInfo->StripHardwareVertsBuffer( bufVHV );
		}

		if ( pMdlStripInfo )
			pMdlStripInfo->DeleteThis();
	}

	if ( !bResult )
	{
		printf( "ERROR: stripping failed!\n" );
		return 1;
	}

	//
	// ====== Save out processed data
	//

	// Save vhv
	if ( !WriteBufferToFile( bufVHV, g_path, ".vhv.strip" ) )
	{
		printf( "ERROR: Failed to save '%s'!\n", g_path );
		return 1;
	}

	return 0;
}

int CStudioMDLApp::Main_MakeVsi()
{
	if ( !mdllib )
	{
		printf( "ERROR: mdllib is not available!\n" );
		return 1;
	}

	Q_StripExtension( g_path, g_path, sizeof( g_path ) );
	char *pExt = g_path + strlen( g_path );
	*pExt = 0;

	// Load up the files
	CUtlBuffer bufMDL;
	CUtlBuffer bufVVD;
	CUtlBuffer bufVTX;
	if ( !Load3ModelBuffers( bufMDL, bufVVD, bufVTX, g_path ) )
		return 1;

	//
	// ====== Process file contents
	//

	CUtlBuffer bufMappingTable;
	bool bResult = false;
	{
		if ( !g_quiet )
		{
			printf( "---------------------\n" );
			printf( "Generating .vsi stripping information...\n" );
			
			SpewActivate( "mdllib", 3 );
		}

		IMdlStripInfo *pMdlStripInfo = NULL;

		bResult =
			mdllib->StripModelBuffers( bufMDL, bufVVD, bufVTX, &pMdlStripInfo ) &&
			pMdlStripInfo->Serialize( bufMappingTable );

		if ( pMdlStripInfo )
			pMdlStripInfo->DeleteThis();
	}

	if ( !bResult )
	{
		printf( "ERROR: stripping failed!\n" );
		return 1;
	}

	//
	// ====== Save out processed data
	//

	// Save remapping data using "P4 edit -> save -> P4 add"  approach
	sprintf( pExt, ".vsi" );
	CP4AutoEditAddFile _auto_edit_vsi( g_path );
	
	if ( !WriteFileToDisk( g_path, NULL, bufMappingTable ) )
	{
		printf( "ERROR: Failed to save '%s'!\n", g_path );
		return 1;
	}
	else if ( !g_quiet )
	{
		printf( "Generated .vsi stripping information.\n" );
	}

	return 0;
}

int CStudioMDLApp::Main_StripModel()
{
	if ( !g_quiet )
	{
		printf( "Stripping binary model files...\n" );
	}

	if ( !mdllib )
	{
		printf( "ERROR: mdllib is not available!\n" );
		return 1;
	}

	Q_FileBase( g_path, g_path, sizeof( g_path ) );
	char *pExt = g_path + strlen( g_path );
	*pExt = 0;

	// Load up the files
	CUtlBuffer bufMDL;
	CUtlBuffer bufVVD;
	CUtlBuffer bufVTX;
	if ( !Load3ModelBuffers( bufMDL, bufVVD, bufVTX, g_path ) )
		return 1;

	//
	// ====== Process file contents
	//

	CUtlBuffer bufMappingTable;
	bool bResult = false;
	{
		SpewActivate( "mdllib", 3 );

		IMdlStripInfo *pMdlStripInfo = NULL;
		
		bResult =
			mdllib->StripModelBuffers( bufMDL, bufVVD, bufVTX, &pMdlStripInfo ) &&
			pMdlStripInfo->Serialize( bufMappingTable );

		if ( pMdlStripInfo )
			pMdlStripInfo->DeleteThis();
	}

	if ( !bResult )
	{
		printf( "ERROR: stripping failed!\n" );
		return 1;
	}

	//
	// ====== Save out processed data
	//

	// Save mdl
	sprintf( pExt, ".mdl.strip" );
	if ( !WriteFileToDisk( g_path, NULL, bufMDL ) )
	{
		printf( "ERROR: Failed to save '%s'!\n", g_path );
		return 1;
	}

	// Save vvd
	sprintf( pExt, ".vvd.strip" );
	if ( !WriteFileToDisk( g_path, NULL, bufVVD ) )
	{
		printf( "ERROR: Failed to save '%s'!\n", g_path );
		return 1;
	}

	// Save vtx
	sprintf( pExt, ".vtx.strip" );
	if ( !WriteFileToDisk( g_path, NULL, bufVTX ) )
	{
		printf( "ERROR: Failed to save '%s'!\n", g_path );
		return 1;
	}

	// Save remapping data
	sprintf( pExt, ".info.strip" );
	if ( !WriteFileToDisk( g_path, NULL, bufMappingTable ) )
	{
		printf( "ERROR: Failed to save '%s'!\n", g_path );
		return 1;
	}

	return 0;
}
