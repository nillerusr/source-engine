//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


#include <windows.h>

// Valve includes
#include "itemtest/itemtest.h"
#include "p4lib/ip4.h"
#include "tier0/icommandline.h"


// Local includes
#include "itemtestapp.h"
#include "runexe.h"


// Last include
#include <tier0/memdbgon.h>


//=============================================================================
//
//=============================================================================
class CItemTestConApp : public CItemTestApp
{
	typedef CItemTestApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual int Main();

protected:
	static bool DoSteamId();

	static bool DoHelp();
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CItemTestConApp );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestConApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "", "" }	// Required to terminate the list
	};

	if ( FindParam( kDev ) && !FindParam( kNoP4 ) )
	{
		AppModule_t p4Module = LoadModule( "p4lib.dll" );
		AddSystem( p4Module, P4_INTERFACE_VERSION );
	}

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool View( CAsset &asset )
{
	CUtlString sBinDir;
	if ( !CItemUpload::GetBinDirectory( sBinDir ) )
	{
		Warning( "Cannot determine bin directory\n" );
		return false;
	}

	CSmartPtr< CTargetMDL > pTargetMDL = asset.GetTargetMDL();
	if ( !pTargetMDL.IsValid() )
	{
		Warning( "No target MDL\n" );
		return false;
	}

	CUtlString sMdlPath;
	if ( !pTargetMDL->GetOutputPath( sMdlPath ) )
	{
		Warning( "Cannot determine path to MDL\n" );
		return false;
	}

	CFmtStrMax sHlmvCmd;

	if ( CItemUpload::GetDevMode() )
	{
		sHlmvCmd.sprintf( "\"%s\\hlmv.exe\" \"%s\"", sBinDir.Get(), sMdlPath.Get() );
	}
	else
	{
		CUtlString sVProjectDir;
		if ( CItemUpload::GetVProjectDir( sVProjectDir ) )
		{
			sHlmvCmd.sprintf( "\"%s\\hlmv.exe\" -allowdebug -game \"%s\" -nop4 \"%s\"", sBinDir.Get(), sVProjectDir.Get(), sMdlPath.Get() );
		}
		else
		{
			Warning( "Cannot determine VPROJECT (gamedir)\n" );
			return false;
		}
	}

	char szBinLaunchDir[ MAX_PATH ];
	V_ExtractFilePath( sBinDir.String(), szBinLaunchDir, ARRAYSIZE( szBinLaunchDir ) );

	return CItemUpload::RunCommandLine( sHlmvCmd.String(), szBinLaunchDir, false );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Explore( CAsset &asset )
{
	CUtlString sBinDir;
	if ( !CItemUpload::GetBinDirectory( sBinDir ) )
	{
		Warning( "Cannot determine bin directory\n" );
		return false;
	}

	CUtlString sDir;
	asset.GetAbsoluteDir( sDir, NULL, &asset );

	char szBuf[ MAX_PATH ];
	V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sDir.Get() );

	char szBinLaunchDir[ MAX_PATH ];
	V_ExtractFilePath( sBinDir.String(), szBinLaunchDir, ARRAYSIZE( szBinLaunchDir ) );

	ShellExecute( NULL, "explore", NULL, NULL, szBuf, SW_SHOWNORMAL );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CItemTestConApp::Main()
{
	if ( DoHelp() )
		return 0;

	if ( DoSteamId() )
		return 0;

	CItemUpload::InitManifest();

	// If launched with -batch stay in console app and run from parameters specified on the command line only

	const bool bListMats = FindParam( kListMats );

	if ( bListMats || FindParam( kBatch ) )
	{
		CAssetTF assetTF;
		if ( !ProcessCommandLine( &assetTF, bListMats ) )
			return 1;

		if ( !bListMats )
		{
			if ( !assetTF.Compile() )
			{
				Warning( "Error! Compile Failed\n" );
				return 1;
			}

			Msg( "Compile OK!\n" );

			if ( FindParam( kView ) )
			{
				View( assetTF );
			}

			if ( FindParam( kExplore ) )
			{
				Explore( assetTF );
			}
		}
	}
	else
	{
		// If launched without -batch then launch the .exe with the same name in the same directory as this .com executable
		RunExe();
	}

	return 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestConApp::DoHelp()
{
	if ( !FindParam( kHelp ) )
		return false;

	PrintHelp();

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestConApp::DoSteamId()
{
	if ( !FindParam( kSteamId ) )
		return false;

	CUtlString sSteamId;

	if ( CItemUpload::GetSteamId( sSteamId ) )
	{
		Msg( "%s\n", sSteamId.String() );
	}
	else
	{
		Warning( "Error! Couldn't determine SteamId\n" );
	}

	return true;
}

