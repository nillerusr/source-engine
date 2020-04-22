//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================


// Valve includes
#include "appframework/tier3app.h"
#include "datamodel/idatamodel.h"
#include "filesystem.h"
#include "icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "mathlib/mathlib.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"
#include "sfmobjects/sfmsession.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "studio.h"
#include "soundemittersystem/isoundemittersystembase.h"
#include "tier2/soundutils.h"
#include "tier2/fileutils.h"
#include "tier3/choreoutils.h"
#include "tier3/scenetokenprocessor.h"
#include "soundchars.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "choreoevent.h"
#include <ctype.h>

#ifdef _DEBUG
#include <windows.h>
#undef GetCurrentDirectory
#endif

//-----------------------------------------------------------------------------
// Standard spew functions
//-----------------------------------------------------------------------------
static SpewRetval_t SpewStdout( SpewType_t spewType, char const *pMsg )
{
	if ( !pMsg )
		return SPEW_CONTINUE;

#ifdef _DEBUG
	OutputDebugString( pMsg );
#endif

	printf( pMsg );
	fflush( stdout );

	return ( spewType == SPEW_ASSERT ) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CVcdUpdateApp : public CTier3SteamApp
{
	typedef CTier3SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void Destroy() {}

	void PrintHelp( );

private:
	struct VcdUpdateInfo_t
	{
		const char *m_pChangedWAVFile;
		const char *m_pChangedMDLFile;
		bool m_bWarnMissingMDL;
		bool m_bWarnNotMatchingMDL;
	};

	void UpdateVcdFiles( const VcdUpdateInfo_t& info );
	void UpdateVcd( const char *pFullPath, const VcdUpdateInfo_t& info, studiohdr_t *pStudioHdr, float flWavDuration );
	bool UpdateVcd( CChoreoScene *pScene, const VcdUpdateInfo_t& info, studiohdr_t *pStudioHdr, float flWavDuration );
};


DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CVcdUpdateApp );

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CVcdUpdateApp::Create()
{
	SpewOutputFunc( SpewStdout );

	AppSystemInfo_t appSystems[] = 
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "p4lib.dll",				P4_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "soundemittersystem.dll",	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	AddSystems( appSystems );

	IMaterialSystem *pMaterialSystem = reinterpret_cast< IMaterialSystem * >( FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION ) );
	if ( !pMaterialSystem )
	{
		Error( "// ERROR: Unable to connect to material system interface!\n" );
		return false;
	}

	pMaterialSystem->SetShaderAPI( "shaderapiempty.dll" );
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CVcdUpdateApp::PreInit( )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMDLCache )
	{
		Error( "// ERROR: vcdupdate is missing a required interface!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Print help
//-----------------------------------------------------------------------------
void CVcdUpdateApp::PrintHelp( )
{
	Msg( "Usage: vcdupdate [-w <modified .wav file>] [-m <modified .mdl file>] [-e] [-n]\n" );
	Msg( "				[-nop4] [-vproject <path to gameinfo.txt>]\n" );
	Msg( "vcdupdate will fixup all vcd files in the tree to account for other asset changes.\n" );
	Msg( "\t-w\t: Specifies the relative path to a .wav file that has changed.\n" );
	Msg( "\t-m\t: Specifies the relative path to a .mdl file that has changed.\n" );
	Msg( "\t-e\t: [Optional] Displays a warning about all .vcds that don't have actors w/ associated .mdls\n" );
	Msg( "\t\tSuch files cannot be auto-updated by vcdupdate and also can generate bugs.\n" );
	Msg( "\t-n\t: [Optional] Displays a warning about all .vcds whose actor names\n" );
	Msg( "\t\tare suspiciously different from the associated .mdl\n" );
	Msg( "\t-nop4\t: Disables auto perforce checkout/add.\n" );
	Msg( "\t-vproject\t: Specifies path to a gameinfo.txt file (which mod to build for).\n" );
}


//-----------------------------------------------------------------------------
// Checks a single VCD to see if it needs to be updated or not.
//-----------------------------------------------------------------------------
bool CVcdUpdateApp::UpdateVcd( CChoreoScene *pScene, const VcdUpdateInfo_t& info, studiohdr_t *pStudioHdr, float flWavDuration )
{
	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );
	float pPoseParameters[MAXSTUDIOPOSEPARAM];
	memset( pPoseParameters, 0, MAXSTUDIOPOSEPARAM * sizeof(float) ); 

	char pModelPath[MAX_PATH];
	if ( pStudioHdr )
	{
		g_pFullFileSystem->FullPathToRelativePathEx( info.m_pChangedMDLFile, "GAME", pModelPath, sizeof(pModelPath) );
	}

	bool bChanged = false;
	int c = pScene->GetNumEvents();
	for ( int i = 0; i < c; i++ )
	{
		CChoreoEvent *pEvent = pScene->GetEvent( i );
		if ( !pEvent )
			continue;

		switch ( pEvent->GetType() )
		{
		default:
			break;
		case CChoreoEvent::SPEAK:
			{
				if ( !info.m_pChangedWAVFile )
					continue;

				const char *pWavFile = GetSoundForEvent( pEvent, &studioHdr );
				if ( !pWavFile )
					continue;

				char pRelativeWAVFile[MAX_PATH];
				Q_ComposeFileName( "sound", pWavFile, pRelativeWAVFile, sizeof(pRelativeWAVFile) );

				char pFullWAVFile[MAX_PATH];
				g_pFullFileSystem->RelativePathToFullPath( pRelativeWAVFile, "GAME", pFullWAVFile, sizeof(pFullWAVFile) );

				if ( Q_stricmp( pFullWAVFile, info.m_pChangedWAVFile ) )
					continue;

				float flEndTime = pEvent->GetStartTime() + flWavDuration;
				if ( pEvent->GetEndTime() != flEndTime )
				{
					pEvent->SetEndTime( pEvent->GetStartTime() + flEndTime );
					bChanged = true;
				}
			}
			break;

		case CChoreoEvent::SEQUENCE:
			{
				if ( !pStudioHdr )
					continue;

				CChoreoActor *pActor = pEvent->GetActor();
				if ( pActor->GetName()[0] == '!' )
					continue;
				const char *pModelName = pActor->GetFacePoserModelName();
				if ( !pModelName || !pModelName[0] || Q_stricmp( pModelPath, pModelName) )
					continue;

				if ( UpdateSequenceLength( pEvent, &studioHdr, pPoseParameters, false, false ) )
				{
					bChanged = true;
				}
			}
			break;
		case CChoreoEvent::GESTURE:
			{
				if ( !pStudioHdr )
					continue;

				CChoreoActor *pActor = pEvent->GetActor();
				if ( pActor->GetName()[0] == '!' )
					continue;
				const char *pModelName = pActor->GetFacePoserModelName();
				if ( !pModelName || !pModelName[0] || Q_stricmp( pModelPath, pModelName ) )
					continue;

				if ( UpdateGestureLength( pEvent, &studioHdr, pPoseParameters, false ) )
				{
					bChanged = true;
				}
				if ( AutoAddGestureKeys( pEvent, &studioHdr, pPoseParameters, false ) )
				{
					bChanged = true;
				}
			}
			break;
		}
	}

	return bChanged;
}


//-----------------------------------------------------------------------------
// Checks a single VCD to see if it needs to be updated or not.
//-----------------------------------------------------------------------------
void CVcdUpdateApp::UpdateVcd( const char *pFullPath, const VcdUpdateInfo_t& info, studiohdr_t *pStudioHdr, float flWavDuration )
{
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pFullPath, NULL, buf ) )
	{
		Warning( "Unable to load file %s\n", pFullPath );
		return;
	}

	SetTokenProcessorBuffer( (char *)buf.Base() );
	CChoreoScene *pScene = ChoreoLoadScene( pFullPath, NULL, GetTokenProcessor(), NULL );
	if ( !pScene )
	{
		Warning( "Unable to parse file %s\n", pFullPath );
		return;
	}

	// Check for validity.
	if ( info.m_bWarnNotMatchingMDL || info.m_bWarnMissingMDL )
	{
		char pBaseModelName[MAX_PATH];
		int nActorCount = pScene->GetNumActors();
		for ( int i = 0; i < nActorCount; ++i )
		{
			CChoreoActor* pActor = pScene->GetActor( i );
			const char *pActorName = pActor->GetName();
			if ( pActorName[0] == '!' )
				continue;

			const char *pModelName = pActor->GetFacePoserModelName();
			if ( !pModelName || !pModelName[0] )
			{
				if ( info.m_bWarnMissingMDL )
				{
					Warning( "\t*** Missing .mdl association: File \"%s\"\tActor \"%s\"\n", pFullPath, pActorName ); 
				}
				continue;
			}

			if ( info.m_bWarnNotMatchingMDL )
			{
				Q_FileBase( pModelName, pBaseModelName, sizeof(pBaseModelName) );
				if ( !StringHasPrefix( pActorName, pBaseModelName ) )
				{
					Warning( "\t*** File \"%s\": Actor name and .mdl name suspiciously different:\n\t\tMDL \"%s\"\tActor \"%s\"\n", pFullPath, pModelName, pActorName ); 
				}
			}
		}
	}

	if ( UpdateVcd( pScene, info, pStudioHdr, flWavDuration ) )
	{
		Warning( "*** VCD %s requires update.\n", pFullPath );
		CP4AutoEditAddFile checkout( pFullPath ); 
		pScene->SaveToFile( pFullPath );
	}
}


//-----------------------------------------------------------------------------
// Loads up the changed files and builds list of vcds to convert, then converts them
//-----------------------------------------------------------------------------
void CVcdUpdateApp::UpdateVcdFiles( const VcdUpdateInfo_t& info )
{
	studiohdr_t *pStudioHdr = NULL;
	if ( info.m_pChangedMDLFile )
	{
		char pRelativeModelPath[MAX_PATH];
		g_pFullFileSystem->FullPathToRelativePathEx( info.m_pChangedMDLFile, "GAME", pRelativeModelPath, sizeof(pRelativeModelPath) );
		Q_SetExtension( pRelativeModelPath, ".mdl", sizeof(pRelativeModelPath) );
		MDLHandle_t hMDL = g_pMDLCache->FindMDL( pRelativeModelPath );
		if ( hMDL == MDLHANDLE_INVALID )
		{
			Warning( "vcdupdate: Model %s doesn't exist!\n", pRelativeModelPath );
			return;
		}

		pStudioHdr = g_pMDLCache->GetStudioHdr( hMDL );
		if ( !pStudioHdr || g_pMDLCache->IsErrorModel( hMDL ) )
		{
			Warning( "vcdupdate: Model %s doesn't exist!\n", pRelativeModelPath );
			return;
		}
	}

	float flWavDuration = -1.0f;
	if ( info.m_pChangedWAVFile )
	{
		flWavDuration = GetWavSoundDuration( info.m_pChangedWAVFile );
	}

	CUtlVector< CUtlString > dirs;
	CUtlVector< CUtlString > vcds;
	GetSearchPath( dirs, "GAME" );
	int nCount = dirs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		char pScenePath[MAX_PATH];
		Q_ComposeFileName( dirs[i], "scenes", pScenePath, sizeof(pScenePath) );
		AddFilesToList( vcds, pScenePath, NULL, "vcd" );
	}

	int nVCDCount = vcds.Count();
	Msg( "Found %d VCDs to check if update is necessary.\n", nVCDCount );
	for ( int i = 0; i < nVCDCount; ++i )
	{
		UpdateVcd( vcds[i], info, pStudioHdr, flWavDuration );
	}
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CVcdUpdateApp::Main()
{
	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	if ( CommandLine()->CheckParm( "-h" ) || CommandLine()->CheckParm( "-help" ) )
	{
		PrintHelp();
		return 0;
	}

	VcdUpdateInfo_t info;
	info.m_pChangedWAVFile = CommandLine()->ParmValue( "-w" );
	info.m_pChangedMDLFile = CommandLine()->ParmValue( "-m" );
	info.m_bWarnMissingMDL = CommandLine()->ParmValue( "-e" ) ? true : false;
	info.m_bWarnNotMatchingMDL = CommandLine()->ParmValue( "-n" ) ? true : false;

	if ( !info.m_pChangedWAVFile && !info.m_pChangedMDLFile )
	{
		PrintHelp();
		return 0;
	}

	// Determine full paths
	char pFullMDLPath[MAX_PATH];
	char pFullWAVPath[MAX_PATH];
	if ( info.m_pChangedMDLFile )
	{
		char pRelativeMDLPath[MAX_PATH];
		if ( !Q_IsAbsolutePath( info.m_pChangedMDLFile ) )
		{
			Q_ComposeFileName( "models", info.m_pChangedMDLFile, pRelativeMDLPath, sizeof(pRelativeMDLPath) );
			g_pFullFileSystem->RelativePathToFullPath( pRelativeMDLPath, "GAME", pFullMDLPath, sizeof(pFullMDLPath) );
		}
		info.m_pChangedMDLFile = pFullMDLPath;
	}
	if ( info.m_pChangedWAVFile )
	{
		char pRelativeWAVPath[MAX_PATH];
		if ( !Q_IsAbsolutePath( info.m_pChangedWAVFile ) )
		{
			Q_ComposeFileName( "sound", info.m_pChangedWAVFile, pRelativeWAVPath, sizeof(pRelativeWAVPath) );
			g_pFullFileSystem->RelativePathToFullPath( pRelativeWAVPath, "GAME", pFullWAVPath, sizeof(pFullWAVPath) );
		}
		info.m_pChangedWAVFile = pFullWAVPath;
	}

	// Do Perforce Stuff
	if ( CommandLine()->FindParm( "-nop4" ) )
	{
		g_p4factory->SetDummyMode( true );
	}

	g_p4factory->SetOpenFileChangeList( "Automatically Updated VCD files" );

	g_pSoundEmitterSystem->ModInit();
	UpdateVcdFiles( info );
	g_pSoundEmitterSystem->ModShutdown();
	return -1;
}