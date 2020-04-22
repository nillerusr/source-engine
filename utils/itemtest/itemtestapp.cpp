//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


// Valve includes
#include "filesystem.h"
#include "itemtest/itemtest.h"
#include "tier0/icommandline.h"
#include "tier2/p4helpers.h"


// Local includes
#include "itemtestapp.h"


// Last include
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
char CItemTestApp::s_szName[] = "itemtest";


char CItemTestApp::s_szDesc[] =
	"itemtest is a command line utility to assist with packaging data "
	"intended to be submitted to Valve.";


char *CItemTestApp::s_pszFlags[][4] = {
	{ "-h",		"-help",		NULL,		"Print this information." },
	{ "-s",		"-steamid",		NULL,		"Print the itemtest steam id for the current user or \"<unknown>\" and exit" },
	{ "-b",		"-batch",		NULL,		"Run in batch mode, i.e. no GUI, all parameters must be specified on the command line." },
	{ "-c",		"-class",		"<class>",	"Specify the class of the item.  One of: demo, engineer, heavy, medic, pyro, scout, sniper, soldier, spy" },
	{ "-n",		"-name",		"<name>",	"Specify the name of the item" },
	{ "-d",		"-dev",			NULL,		"Turn on dev mode" },
	{ "-nop4",	"-nop4",		NULL,		"If -dev mode is specified optionally turn off perforce.  NOTE: Perforce is only enabled in dev mode." },
	{ "-as",	"-autoskin",	NULL,		"Turn on auto skinning of the geometry to bip_head." },
	{ "-l",		"-lod",			"<file>",	"Specify the LOD, can be specified multiple times, first is LOD0, 2nd is LOD1, etc.." },
	{ "-lm",	"-listmats",	NULL,		"If some lod files are specified and this option is specified, the names of the materials found in the LOD files are printed to stdout and the program exits, all texture flags are ignored.  The material names are the arguments needed to -mat" },
	{ "-m",		"-mat",			"<name>",	"Specify a material.  <name> is the name of the material from the geometry file" },
	{ "-mt",	"-mattype",		"<type>",	"Category of previously specified material, one of: primary, secondary, duplicate_primary, duplicate_secondary" },
	{ "-t",		"-tex",			"<file>",	"Specify a texture to use with the previously specified material" },
	{ "-tt",	"-textype",		"<type>",	"Category of previously specified texture, one of: common, red, blue, normal" },
	{ "-at",	"-alphatype",	"<type>",	"Category of alpha data of previously specified texture, one of: none, transparency, paintable, spec" },
	{ "-v",		"-view",		NULL,		"Run hlmv on the compiled model if successful" },
	{ "-ex",	"-explore",		NULL,		"Open explorer window on the compiled zip if successful" },
	{ "-o",		"-output",		"<file>",	"Save output to a specific file" }
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestApp::PreInit()
{
	// This just has to go in the right scope so it can access kFlagsCount
	COMPILE_TIME_ASSERT( kFlagsCount == ARRAYSIZE( s_pszFlags ) );

	if ( !BaseClass::PreInit() )
		return false;

	MathLib_Init();
	
	// Set Dev mode appropriately
	DoDevMode();

	CUtlString sSourceSDKPath, sSourceSDKBin;

	{
		CUtlString sSteamAppInstallLocation;

		// See if we have to do special initialization because TF & the SourceSDK are configured oddly
		// GetSourceSDKFromExe() will fail if it's not that specific configuration and so normal
		// initialization will proceed in that case

		// TODO: These are specific values for TF but hopefully this code will only be needed for TF & SourceSDK
		const char *pszMod = "tf";
		const int nAppId = 440;

		if ( CItemUpload::GetSourceSDKFromExe( sSourceSDKPath, sSourceSDKBin ) && CItemUpload::GetSteamAppInstallLocation( sSteamAppInstallLocation, nAppId ) )
		{
			char szGameDir[ MAX_PATH ] = "";
			V_ComposeFileName( sSteamAppInstallLocation.String(), pszMod, szGameDir, ARRAYSIZE( szGameDir ) );

			if ( !SetupSearchPaths( szGameDir, true, true ) )
			{
				return false;
			}
		}
		else if ( !SetupSearchPaths( NULL, false, true ) )
		{
			return false;
		}
	}
	
	const char *pszGameInfoPath = GetGameInfoPath();

	char szGameInfoParent[ MAX_PATH ] = "";

	V_ExtractFilePath( pszGameInfoPath, szGameInfoParent, ARRAYSIZE( szGameInfoParent ) );
	char szPlatformDir[ MAX_PATH ] = "";
	V_ComposeFileName( szGameInfoParent, "platform", szPlatformDir, ARRAYSIZE( szPlatformDir ) );

	g_pFullFileSystem->AddSearchPath( szPlatformDir, "PLATFORM" );

	if ( !sSourceSDKPath.IsEmpty() )
	{
		V_ComposeFileName( sSourceSDKPath.String(), "platform", szPlatformDir, ARRAYSIZE( szPlatformDir ) );
		g_pFullFileSystem->AddSearchPath( szPlatformDir, "PLATFORM" );
	}

	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD );

	CreateInterfaceFn factory = GetFactory();
	if ( !ConnectDataModel( factory ) )
		return false;

	const InitReturnVal_t nRetVal = InitDataModel();

#ifdef _DEBUG

	{
		g_pFullFileSystem->PrintSearchPaths();

		Msg( "// itemtest paths:\n" );

		CUtlString sTmp;
		if ( CItemUpload::GetVProjectDir( sTmp ) )
		{
			Msg( "// VProject: %s\n", sTmp.String() );
		}

		if ( CItemUpload::GetVMod( sTmp ) )
		{
			Msg( "// Mod:      %s\n", sTmp.String() );
		}

		if ( CItemUpload::GetContentDir( sTmp ) )
		{
			Msg( "// Content:  %s\n", sTmp.String() );
		}

		if ( CItemUpload::GetBinDirectory( sTmp ) )
		{
			Msg( "// Bin:      %s\n", sTmp.String() );
		}
	}
#endif // _DEBUG

	return ( nRetVal == INIT_OK );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemTestApp::PostShutdown()
{
	ShutdownDataModel();
	DisconnectDataModel();
	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemTestApp::DoDevMode()
{
	if ( FindParam( kDev ) )
	{
		CItemUpload::SetDevMode( true );

		const bool bP4DLLExists = g_pFullFileSystem->FileExists( "p4lib.dll", "EXECUTABLE_PATH" );

		// No p4 mode if specified on the command line or no p4lib.dll found
		if ( !bP4DLLExists || FindParam( kNoP4 ) )
		{
			g_p4factory->SetDummyMode( true );

			CItemUpload::SetP4( false );
		}
		else
		{
			CItemUpload::SetP4( true );
		}

		// Set the named changelist
		g_p4factory->SetOpenFileChangeList( "itemtest Auto Checkout" );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestApp::ProcessCommandLine( CAsset *pAsset, bool bDoListMats )
{
	const char *pszName = ParmValue( kName );
	if ( pszName )
	{
		pAsset->SetName( pszName );
	}

	CAssetTF *pAssetTF = dynamic_cast< CAssetTF * >( pAsset );
	if ( pAssetTF )
	{
		const char *pszUserClass = ParmValue( kClass );
		const char *pszClass = GetClassString( pszUserClass );

		if ( pszClass )
		{
			pAssetTF->SetClass( pszClass );
		}
	}

	const char *pszOutput = ParmValue( kOutput );
	if ( pszOutput )
	{
		pAsset->SetArchivePath( pszOutput );
	}
	else
	{
		CUtlString sFileName;
		pAsset->GetName( sFileName );

		char pszWorkshopPath[ MAX_PATH ];
		if ( g_pFullFileSystem->RelativePathToFullPath( "workshop", "GAME", pszWorkshopPath, sizeof(pszWorkshopPath) ) && !sFileName.IsEmpty() )
		{
			sFileName += ".zip";
			V_ComposeFileName( pszWorkshopPath, sFileName, pszWorkshopPath, sizeof(pszWorkshopPath) );
			pAsset->SetArchivePath( pszWorkshopPath );
		}
	}

	pAsset->SetSkinToBipHead( FindParam( kAutoSkin ) > 0 );

	ICommandLine *pCmdLine = CommandLine();
	const int nParamCount = pCmdLine->ParmCount();

	for ( int i = 0; i < nParamCount; ++i )
	{
		const char *pszFlag = pCmdLine->GetParm( i );
		if ( !V_stricmp( pszFlag, s_pszFlags[kLod][kShortFlag] ) || !V_stricmp( pszFlag, s_pszFlags[kLod][kLongFlag] ) )
		{
			if ( i + 1 < nParamCount )
			{
				const char *pszLod = pCmdLine->GetParm( i + 1 );
				pAsset->AddTargetDMX( pszLod );
			}
			else
			{
				Warning( "Error! Command line switch \"%s\" specified without parameter value\n", pszFlag );
			}
		}
	}

	if ( bDoListMats )
	{
		bool bRetVal = false;

		if ( pAsset->TargetDMXCount() <= 0 )
		{
			Warning( "Error! %s specified but no no geometry files specified via %s\n", s_pszFlags[kListMats][kLongFlag], s_pszFlags[kLod][kLongFlag] );
		}
		else
		{
			const int nTargetVMTCount = pAsset->GetTargetVMTCount();

			if ( nTargetVMTCount <= 0 )
			{
				Warning( "Error! %s specified but no materials found in specified LODs\n", s_pszFlags[kListMats][kLongFlag] );
			}
			else
			{
				CUtlString sTargetVMT;

				for ( int i = 0; i < nTargetVMTCount; ++i )
				{
					pAsset->GetTargetVMT( i )->GetMaterialId( sTargetVMT );
					Msg( "Material: %s\n", sTargetVMT.String() );
				}

				bRetVal = true;
			}
		}

		return bRetVal;
	}

	CSmartPtr< CTargetVMT > pTargetVmt = NULL;
	int nTexParmIndex = -1;
	bool bTexNormal = false;

	for ( int i = 0; i < nParamCount; ++i )
	{
		const char *pszFlag = pCmdLine->GetParm( i );
		if ( !V_stricmp( pszFlag, s_pszFlags[kMat][kShortFlag] ) || !V_stricmp( pszFlag, s_pszFlags[kMat][kLongFlag] ) )
		{
			if ( i + 1 < nParamCount )
			{
				const char *pszMatName = pCmdLine->GetParm( i + 1 );
				pTargetVmt = pAsset->FindOrAddMaterial( pszMatName, CItemUpload::Manifest()->GetDefaultMaterialType() );
				nTexParmIndex = -1;
				bTexNormal = false;
			}
			else
			{
				Warning( "Error! Command line switch \"%s\" specified without parameter value\n", pszFlag );
			}
		}
		else if ( !V_stricmp( pszFlag, s_pszFlags[kMatType][kShortFlag] ) || !V_stricmp( pszFlag, s_pszFlags[kMatType][kLongFlag] ) )
		{
			if ( !pTargetVmt )
			{
				Warning( "Error! No -mat specified before \"%s\"\n", pszFlag );
				continue;
			}

			const char *pszMatType = GetParm( i + 1 );
			if ( !pszMatType )
			{
				Warning( "Error! Command line switch \"%s\" specified without parameter value\n", pszFlag );
				continue;
			}

			bool bDuplicate = false;

			if ( StringHasPrefix( pszMatType, "duplicate_" ) )
			{
				pszMatType += 10;
				bDuplicate = true;
			}
			else if ( StringHasPrefix( pszMatType, "d" ) )
			{
				pszMatType += 1;
				bDuplicate = true;
			}

			int nMatType = CTargetVMT::StringToMaterialType( pszMatType );
			if ( nMatType != kInvalidMaterialType )
			{
				if ( bDuplicate )
				{
					pTargetVmt->SetDuplicate( nMatType );
				}
				else
				{
					pTargetVmt->SetMaterialType( nMatType );
				}
			}
			else
			{
				Warning( "Error! Invalid Parameter Value: \"%s\" \"%s\", expected one of primary, secondary, duplicate_primary, duplicate_secondary\n", pszFlag, pszMatType );
			}
		}
		else if ( !V_stricmp( pszFlag, s_pszFlags[kTex][kShortFlag] ) || !V_stricmp( pszFlag, s_pszFlags[kTex][kLongFlag] ) )
		{
			if ( !pTargetVmt )
			{
				Warning( "Error! No -mat specified before \"%s\"\n", pszFlag );
				continue;
			}

			if ( pTargetVmt->GetDuplicate() )
			{
				Warning( "Error! Previous material specified as duplicate before \"%s\", ignoring\n", pszFlag );
				continue;
			}

			const char *pszTex = GetParm( i + 1 );
			if ( !pszTex )
			{
				Warning( "Error! Command line switch \"%s\" specified without parameter value\n", pszFlag );
				continue;
			}

			nTexParmIndex = i + 1;
		}
		else if ( !V_stricmp( pszFlag, s_pszFlags[kTexType][kShortFlag] ) || !V_stricmp( pszFlag, s_pszFlags[kTexType][kLongFlag] ) )
		{
			if ( !pTargetVmt )
			{
				Warning( "Error! No -mat specified before \"%s\"\n", pszFlag );
				continue;
			}

			if ( pTargetVmt->GetDuplicate() )
			{
				Warning( "Error! Previous material specified as duplicate before \"%s\", ignoring\n", pszFlag );
				continue;
			}

			const char *pszTex = GetParm( nTexParmIndex );
			if ( !pszTex )
			{
				Warning( "Error! No -tex specified before \"%s\"\n", pszFlag );
				continue;
			}

			const char *pszTexType = GetParm( i + 1 );
			if ( !pszTex )
			{
				Warning( "Error! Command line switch \"%s\" specified without parameter value\n", pszFlag );
				continue;
			}

			if ( StringHasPrefix( pszTexType, "C" ) )
			{
				pTargetVmt->SetTargetVTF( "_color", pszTex );	// Comes from texture_types in manifest
			}
			else if ( StringHasPrefix( pszTexType, "N" ) )
			{
				pTargetVmt->SetTargetVTF( "_normal", pszTex );	// Comes from texture_types in manifest
				bTexNormal = true;
			}
			else if ( StringHasPrefix( pszTexType, "R" ) )
			{
				pTargetVmt->SetTargetVTF( "_color", pszTex, CItemUpload::Manifest()->GetMaterialSkin( "red" ) );	// Comes from texture_types in manifest
			}
			else if ( StringHasPrefix( pszTexType, "B" ) )
			{
				pTargetVmt->SetTargetVTF( "_color", pszTex, CItemUpload::Manifest()->GetMaterialSkin( "blue" ) );	// Comes from texture_types in manifest
			}
		}
		else if ( !V_stricmp( pszFlag, s_pszFlags[kAlphaType][kShortFlag] ) || !V_stricmp( pszFlag, s_pszFlags[kAlphaType][kLongFlag] ) )
		{
			if ( !pTargetVmt )
			{
				Warning( "Error! No -mat specified before \"%s\"\n", pszFlag );
				continue;
			}

			if ( pTargetVmt->GetDuplicate() )
			{
				Warning( "Error! Previous material specified as duplicate before \"%s\", ignoring\n", pszFlag );
				continue;
			}

			const char *pszTex = GetParm( nTexParmIndex );
			if ( !pszTex )
			{
				Warning( "Error! No -tex specified before \"%s\"\n", pszFlag );
				continue;
			}

			const char *pszTexType = GetParm( i + 1 );
			if ( !pszTex )
			{
				Warning( "Error! Command line switch \"%s\" specified without parameter value\n", pszFlag );
				continue;
			}

			if ( StringHasPrefix( pszTexType, "N" ) )
			{
				if ( bTexNormal )
				{
					pTargetVmt->SetNormalAlphaType( CTargetVMT::kNoNormalAlpha );
				}
				else
				{
					pTargetVmt->SetColorAlphaType( CTargetVMT::kNoColorAlpha );
				}
			}
			else if ( StringHasPrefix( pszTexType, "T" ) )
			{
				if ( bTexNormal )
				{
					Warning( "Error! Command line switch \"%s\" \"%s\" specified after a \"normal\" texture, only applies to color textures\n", pszFlag, pszTexType );
					continue;
				}
				else
				{
					pTargetVmt->SetColorAlphaType( CTargetVMT::kTransparency );
				}
			}
			else if ( StringHasPrefix( pszTexType, "P" ) )
			{
				if ( bTexNormal )
				{
					Warning( "Error! Command line switch \"%s\" \"%s\" specified after a \"normal\" texture, only applies to color textures\n", pszFlag, pszTexType );
					continue;
				}
				else
				{
					pTargetVmt->SetColorAlphaType( CTargetVMT::kPaintable );
				}
			}
			else if ( StringHasPrefix( pszTexType, "S" ) )
			{
				if ( bTexNormal )
				{
					pTargetVmt->SetNormalAlphaType( CTargetVMT::kNormalSpecPhong );
				}
				else
				{
					pTargetVmt->SetColorAlphaType( CTargetVMT::kColorSpecPhong );
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemTestApp::PrintHelp()
{
	// TODO: Find with of current output device
	static const int nLineLength = 79;

	Msg( "\n" );
	Msg( "\n" "NAME" "\n" );

	CUtlString sLine = "   ";
	CUtlString sWord;
	sLine += s_szName;

	CFmtStr sFmt;
	CFmtStr sIndent;

	sFmt.sprintf( "%%%ds", sLine.Length() + 1 );
	sIndent.sprintf( sFmt, " " );

	for ( int i = 0; i < kFlagsCount; ++i )
	{
		const int nSpaceLeft = nLineLength - sLine.Length();

		sWord = "[";
		sWord += s_pszFlags[i][kShortFlag];
		sWord += " | ";
		sWord += s_pszFlags[i][kLongFlag];
		if ( s_pszFlags[i][kArgDesc] )
		{
			sWord += " ";
			sWord += s_pszFlags[i][kArgDesc];
		}
		sWord += "]";

		const int nWordWidth = sWord.Length();

		if ( nWordWidth > nSpaceLeft )
		{
			Msg( "%s\n", sLine.Get() );
			sLine = sIndent;
		}
		else
		{
			sLine += " ";
		}

		sLine += sWord;
	}

	if ( sLine.Length() )
	{
		Msg( "%s\n", sLine.Get() );
	}

	Msg( "\n" "DESCRIPTION" "\n" );

	sIndent = "   ";
	sLine = "  ";	// One less space than indent to start because space gets added below

	CUtlVector< char *, CUtlMemory< char *, int > > outStrings;
	const char *pszSeparators[] = { " ", "\t", "\n", "\r", "\v", "\f" };

	V_SplitString2( s_szDesc, pszSeparators, ARRAYSIZE( pszSeparators ), outStrings );

	for ( int j = 0; j < outStrings.Count(); ++j )
	{
		const int nSpaceLeft = nLineLength - sLine.Length();
		const int nWordWidth = V_strlen( outStrings[j] ) + 1;

		if ( nWordWidth > nSpaceLeft )
		{
			Msg( "%s\n", sLine.Get() );
			sLine = sIndent;
		}
		else
		{
			sLine += " ";
		}

		sLine += outStrings[j];
	}

	if ( sLine.Length() )
	{
		Msg( "%s\n", sLine.Get() );
	}

	Msg( "\n" "OPTIONS" "\n" );

	int nLineLen[ kFlagsCount ];
	int nMaxLen = 0;

	for ( int i = 0; i < kFlagsCount; ++i )
	{
		nLineLen[i] = 3;																				// Space prefix
		nLineLen[i] += V_strlen( s_pszFlags[i][kShortFlag] );											// Short flag
		nLineLen[i] += 3;																				// " | "
		nLineLen[i] += V_strlen( s_pszFlags[i][kLongFlag] );											// Long flag
		nLineLen[i] += s_pszFlags[i][kArgDesc] ? V_strlen( s_pszFlags[i][kArgDesc] ) + 1 : 0;	// Optional parameter
		nMaxLen = MAX( nMaxLen, nLineLen[i] );
	}
	nMaxLen += 2;

	char szElipsis[ BUFSIZ ];
	CFmtStr sFlags;

	for ( int i = 0; i < kFlagsCount; ++i )
	{
		int nParmsLen = nMaxLen - nLineLen[i];
		if ( nParmsLen % 2 == 0 )
		{
			V_snprintf( szElipsis, nParmsLen, ". . . . . . . . . . . . ." );
		}
		else
		{
			V_snprintf( szElipsis, nParmsLen, " . . . . . . . . . . . . ." );
		}

		sFmt.sprintf( "   %%s | %%s%%s%%s %-s", szElipsis );
		sFlags.sprintf( sFmt, s_pszFlags[i][kShortFlag], s_pszFlags[i][kLongFlag], s_pszFlags[i][kArgDesc] ? " " : "", s_pszFlags[i][kArgDesc] ? s_pszFlags[i][kArgDesc] : "" );

		sLine = sFlags;

		sFmt.sprintf( "%%%ds", sLine.Length() + 1 );
		sIndent.sprintf( sFmt, " " );

		V_SplitString2( s_pszFlags[i][kFlagDesc], pszSeparators, ARRAYSIZE( pszSeparators ), outStrings );
		for ( int j = 0; j < outStrings.Count(); ++j )
		{
			const int nSpaceLeft = nLineLength - sLine.Length();
			const int nWordWidth = V_strlen( outStrings[j] ) + 1;

			if ( nWordWidth > nSpaceLeft )
			{
				Msg( "%s\n", sLine.Get() );
				sLine = sIndent;
			}
			else
			{
				sLine += " ";
			}

			sLine += outStrings[j];
		}

		if ( sLine.Length() )
		{
			Msg( "%s\n", sLine.Get() );
		}
	}

	Msg( "\n" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CItemTestApp::FindParam( Flags_t nFlag )
{
	int nRet = CommandLine()->FindParm( s_pszFlags[nFlag][kShortFlag] );

	if ( nRet )
		return nRet;

	return CommandLine()->FindParm( s_pszFlags[nFlag][kLongFlag] );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CItemTestApp::ParmValue( Flags_t nFlag )
{
	const char *pszParmValue = CommandLine()->ParmValue( s_pszFlags[nFlag][kShortFlag] );
	if ( pszParmValue )
		return pszParmValue;

	return CommandLine()->ParmValue( s_pszFlags[nFlag][kLongFlag] );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CItemTestApp::GetParm( int nParmIndex )
{
	if ( nParmIndex >= 0 || nParmIndex < CommandLine()->ParmCount() )
		return CommandLine()->GetParm( nParmIndex );

	return NULL;
}