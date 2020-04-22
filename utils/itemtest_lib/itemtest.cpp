//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


// Standard includes
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>


// Valve includes
#include "itemtest/itemtest.h"
#include "bitmap/bitmap.h"
#include "bitmap/imageformat.h"
#include "bitmap/psd.h"
#include "bitmap/tgaloader.h"
#include "bitmap/tgawriter.h"
#include "vtf/vtf.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmelement.h"
#include "datamodel/idatamodel.h"
#include "fbxutils/dmfbxserializer.h"
#include "filesystem.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmobjserializer.h"
#include "movieobjects/dmsmdserializer.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmelog.h"
#include "steam/steam_api.h"
#include "tier1/fmtstr.h"
#include "tier1/utlsymbol.h"
#include "tier2/fileutils.h"
#include "tier2/p4helpers.h"
#include "../public/zip_utils.h"


// Last include
#include "tier0/memdbgon.h"

#ifdef BEGIN_DEFINE_LOGGING_CHANNEL
BEGIN_DEFINE_LOGGING_CHANNEL( LOG_ITEMTEST, "ItemTest", LCF_CONSOLE_ONLY, LS_MESSAGE );
ADD_LOGGING_CHANNEL_TAG( "ItemTest" );
END_DEFINE_LOGGING_CHANNEL();
#endif

// This isn't available in the TF runtime (yet?)
#ifndef FUNCTION_LINE_STRING
#define FUNCTION_LINE_STRINGIFY(x) #x
#define FUNCTION_LINE_TOSTRING(x) FUNCTION_LINE_STRINGIFY(x)
#define FUNCTION_LINE_STRING __FUNCTION__ "(" FUNCTION_LINE_TOSTRING(__LINE__) "): "
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
enum
{
	k64KB = 65536
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CSteamAPIContext g_SteamAPIContext;
bool CItemUpload::m_bDev = false;
bool CItemUpload::m_bIgnoreEnvVars = false;
bool CItemUpload::m_bP4 = false;
CUtlString CItemUpload::m_szForcedSteamID = "";
CItemTestManifest *CItemUpload::m_pItemTestManifest = NULL;
static bool g_bCompilePreview = false;
static const char* kVMT = "VMT%d";

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
inline bool UtlStringLessThan( const CUtlString &sLhs, const CUtlString &sRhs )
{
	return CaselessStringLessThanIgnoreSlashes( sLhs.String(), sRhs.String() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemLog::Msg( const char *pszFormat, ... ) const
{
	va_list args;
	va_start( args, pszFormat );
	CFmtStrMax str;
	str.AppendFormatV( pszFormat, args );
	Log( kItemtest_Log_Info, str );
	va_end( args );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemLog::Warning( const char *pszFormat, ... ) const
{
	va_list args;
	va_start( args, pszFormat );
	CFmtStrMax str;
	str.AppendFormatV( pszFormat, args );
	Log( kItemtest_Log_Warning, str );
	va_end( args );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemLog::Error( const char *pszFormat, ... ) const
{
	va_list args;
	va_start( args, pszFormat );
	CFmtStrMax str;
	str.AppendFormatV( pszFormat, args );
	Log( kItemtest_Log_Error, str );
	va_end( args );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemLog::Log( ItemtestLogLevel_t nLogLevel, const char *pszMessage ) const
{
	if ( m_pItemLog && m_pItemLog != this )
	{
		m_pItemLog->Log( nLogLevel, pszMessage );
		return;
	}

	switch ( nLogLevel )
	{
	case kItemtest_Log_Info:
		Log_Msg( LOG_ITEMTEST, "%s", pszMessage );
		break;
	case kItemtest_Log_Warning:
		Log_Warning( LOG_ITEMTEST, "%s", pszMessage );
		break;
	case kItemtest_Log_Error:
		Log_Error( LOG_ITEMTEST, "%s", pszMessage );
		break;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::InitManifest( void )
{
	if ( m_pItemTestManifest )
		return true;

	m_pItemTestManifest = new CItemTestManifest( "scripts/itemtest_manifest.txt", new CItemLog() );
	return m_pItemTestManifest->IsValid();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::SanitizeName( const char *pszName, CUtlString &sCleanName )
{
	char pszTemp[MAX_PATH];

	V_strcpy_safe( pszTemp, pszName );

	// Convert to lowercase, strip punctuation and turn spaces into underscores
	char *pszSrc = pszTemp;
	char *pszDst = pszTemp;
	while ( *pszSrc )
	{
		char c = *pszSrc++;

		if ( c >= 'a' && c <= 'z' )
		{
			*pszDst++ = c;
		}
		else if ( c >= 'A' && c <= 'Z' )
		{
			*pszDst++ = c - 'A' + 'a';
		}
		else if ( c >= '0' && c <= '9' )
		{
			*pszDst++ = c;
		}
		else if ( V_isspace(c) || c == '_' )
		{
			*pszDst++ = '_';
		}
		else
		{
			// Punctuation or non-ASCII characters, skip 'em!
		}
	}
	*pszDst = '\0';

	sCleanName = pszTemp;

	return !sCleanName.IsEmpty();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CItemTestManifest::CItemTestManifest( const char *pszManifestFile, CItemLog *pItemLog ) :
	m_pItemLog( pItemLog ),
	m_vecVMTTextureRemaps( UtlStringLessThan )
{
	m_pManifestKV = NULL;
	m_pItemDirectory = "";
	m_pAnimationDirectory = "";
	m_pIconDirectory = "";
	m_pZipSourceDirectory = "";
	m_pZipOutputDirectory = "";
	m_pQCTemplate = "";
	m_pQCITemplate = "";
	m_bTerseMessages = false;
	m_bItemPathUsesSteamId = true;

	m_pManifestKV = new KeyValues( pszManifestFile );
	if ( !m_pManifestKV->LoadFromFile(g_pFullFileSystem, pszManifestFile, "MOD") )
	{
		m_pManifestKV->deleteThis();
		m_pManifestKV = NULL;

		m_pItemLog->Warning( "ERROR: Failed to load manifest file: %s\n", pszManifestFile );
		return;
	}

	// Class list
	if ( !ParseStringsFromManifest( m_pManifestKV, "classes", m_vecClasses ) )
		return;

	// MDL Extensions
	if ( !ParseStringsFromManifest( m_pManifestKV, "mdl_files", m_vecMDLExtensions ) )
		return;

	// Animation MDL Extensions
	if ( !ParseStringsFromManifest( m_pManifestKV, "animation_mdl_files", m_vecAnimationMDLExtensions ) )
		return;

	// Material types
	KeyValues *pKVMaterialTypes = m_pManifestKV->FindKey("material_types");
	if ( !pKVMaterialTypes )
	{
		m_pItemLog->Warning( "ERROR: Failed to find a 'material_types' section in manifest file: %s\n", pszManifestFile );
		return;
	}
	FOR_EACH_SUBKEY( pKVMaterialTypes, pKVMaterialType )
	{
		const char *pszString = pKVMaterialType->GetName();
		int nIdx = m_vecMaterialTypes.AddToTail();
		m_vecMaterialTypes[nIdx].pszMaterialType = pszString;
	}

	const char *pszDefaultMatTypeString = m_pManifestKV->GetString("default_material_type");
	if ( !pszDefaultMatTypeString || !pszDefaultMatTypeString[0] )
	{
		m_pItemLog->Warning( "ERROR: Failed to find a 'default_material_type' string in manifest file: %s\n", pszManifestFile );
		return;
	}
	m_nDefaultMaterialType = GetMaterialType( pszDefaultMatTypeString );
	if ( m_nDefaultMaterialType == kInvalidMaterialType )
	{
		m_pItemLog->Warning( "ERROR: Default material type '%s' wasn't found in the material type list in manifest file: %s\n", m_nDefaultMaterialType, pszManifestFile );
		return;
	}

	// Material skins
	KeyValues *pKVMaterialSkins = m_pManifestKV->FindKey("material_skins");
	if ( pKVMaterialSkins )
	{
		FOR_EACH_SUBKEY( pKVMaterialSkins, pKVMaterialSkin )
		{
			const char *pszString = pKVMaterialSkin->GetName();
			int nIdx = m_vecMaterialSkins.AddToTail();
			m_vecMaterialSkins[nIdx].pszMaterialSkin = pszString;
			m_vecMaterialSkins[nIdx].pszFilenameAppend = pKVMaterialSkin->GetString("file_append");
		}

		const char *pszDefaultMatSkinString = m_pManifestKV->GetString("default_material_skin");
		if ( !pszDefaultMatSkinString || !pszDefaultMatSkinString[0] )
		{
			m_pItemLog->Warning( "ERROR: Failed to find a 'default_material_skin' string in manifest file: %s\n", pszManifestFile );
			return;
		}
		m_nDefaultMaterialSkin = GetMaterialSkin( pszDefaultMatSkinString );
		if ( m_nDefaultMaterialSkin == kInvalidMaterialSkin )
		{
			m_pItemLog->Warning( "ERROR: Default material skin '%s' wasn't found in the material skin list in manifest file: %s\n", pszDefaultMatSkinString, pszManifestFile );
			m_nDefaultMaterialSkin = 0;
			return;
		}
	}
	else
	{
		m_nDefaultMaterialSkin = 0;
	}

	// Texture types
	KeyValues *pKVTextureTypes = m_pManifestKV->FindKey("texture_types");
	if ( !pKVTextureTypes )
	{
		m_pItemLog->Warning( "ERROR: Failed to find a 'texture_types' section in manifest file: %s\n", pszManifestFile );
		return;
	}
	FOR_EACH_SUBKEY( pKVTextureTypes, pKVTexture )
	{
		const char *pszString = pKVTexture->GetName();
		int nIdx = m_vecTextureTypes.AddToTail();
		m_vecTextureTypes[nIdx].pszTextureType = pszString;
		m_vecTextureTypes[nIdx].bOptional = pKVTexture->GetBool( "optional" );
		m_vecTextureTypes[nIdx].pkvAddToVTEXConfig = pKVTexture->FindKey("add_to_vtex_config");
	}

	// VMT templates
	KeyValues *pKVTemplates = m_pManifestKV->FindKey("vmt_templates");
	if ( !pKVTemplates )
	{
		m_pItemLog->Warning( "ERROR: Failed to find a 'vmt_templates' section in manifest file: %s\n", pszManifestFile );
		return;
	}

	KeyValues *pKVClassTemplates = pKVTemplates->FindKey("classes");
	if ( pKVClassTemplates )
	{
		m_vecClassTemplates.SetCount( m_vecClasses.Count() );
		for ( int i = 0; i < m_vecClassTemplates.Count(); ++i )
		{
			m_vecClassTemplates[ i ] = NULL;
		}

		FOR_EACH_SUBKEY( pKVClassTemplates, pKVClassTemplate )
		{
			const char *pszHero = pKVClassTemplate->GetName();
			const char *pszTemplate = pKVClassTemplate->GetString();

			int iClass = m_vecClasses.Find( pszHero );
			if ( iClass == m_vecClasses.InvalidIndex() )
			{
				m_pItemLog->Warning( "ERROR: Found an invalid class '%s' in the vmt_templates entries in manifest file: %s\n", pszHero, pszManifestFile );
				return;
			}

			m_vecClassTemplates[iClass] = pszTemplate;
		}
	}

	KeyValues *pKVVMTRemaps = pKVTemplates->FindKey("vmt_texture_settings");
	if ( pKVVMTRemaps )
	{
		FOR_EACH_SUBKEY( pKVVMTRemaps, pKVRemap )
		{
			const char *pszVMTVar = pKVRemap->GetName();
			const char *pszTexture = pKVRemap->GetString();

			m_vecVMTTextureRemaps.Insert( pszTexture, pszVMTVar );
		}
	}

	// Icon types
	KeyValues *pKVIconTypes = m_pManifestKV->FindKey("icon_types");
	if ( pKVIconTypes )
	{
		FOR_EACH_SUBKEY( pKVIconTypes, pKVIcon )
		{
			const char *pszString = pKVIcon->GetName();
			int nIdx = m_vecIconTypes.AddToTail();
			m_vecIconTypes[nIdx].pszIconType = pszString;
			m_vecIconTypes[nIdx].nWidth = pKVIcon->GetInt( "width" );
			m_vecIconTypes[nIdx].nHeight = pKVIcon->GetInt( "height" );
			m_vecIconTypes[nIdx].pszFilenameAppend = pKVIcon->GetString("file_append");
			m_vecIconTypes[nIdx].pkvAddToVTEXConfig = pKVIcon->FindKey("add_to_vtex_config");

			KeyValues *pKVVMT = pKVIcon->FindKey("vmt_template");
			if ( pKVVMT )
			{
				m_vecIconTypes[nIdx].pkvVMTTemplate = pKVVMT->GetFirstSubKey();
			}
			else
			{
				m_vecIconTypes[nIdx].pkvVMTTemplate = NULL;
			}
		}
	}

	m_pItemDirectory = m_pManifestKV->GetString("item_directory");
	m_pAnimationDirectory = m_pManifestKV->GetString("animation_directory");
	m_pIconDirectory = m_pManifestKV->GetString("icon_directory");
	m_pZipSourceDirectory = m_pManifestKV->GetString("archive_source_path");
	m_pZipOutputDirectory = m_pManifestKV->GetString("archive_output_path");

	m_pQCTemplate = m_pManifestKV->GetString("qc_template");
	if ( V_strlen( m_pQCTemplate ) == 0 )
	{
		m_pItemLog->Warning( "ERROR: qc_template not defined in manifest file: %s\n", pszManifestFile );
		return;
	}

	m_pQCITemplate = m_pManifestKV->GetString( "qci_template" );
	if ( V_strlen( m_pQCITemplate ) == 0 )
	{
		m_pItemLog->Warning( "ERROR: qci_template not defined in manifest file: %s\n", pszManifestFile );
		return;
	}

	const char *pszQCLODDistances = m_pManifestKV->GetString("qc_lod_distances");
	CUtlVector<char*> vecQCLODDistances;
	V_SplitString( pszQCLODDistances, ",", vecQCLODDistances );
	m_vecQCLODDistances.SetCount( vecQCLODDistances.Count() );
	for ( int i = 0; i < vecQCLODDistances.Count(); ++i )
	{
		m_vecQCLODDistances[i] = V_atoi( vecQCLODDistances[i] );
	}

	m_bTerseMessages = m_pManifestKV->GetBool( "terse_messages", false );
	m_bItemPathUsesSteamId = m_pManifestKV->GetBool( "item_path_has_steamid", true );

}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestManifest::ParseStringsFromManifest( KeyValues *pKV, const char *pszKeyName, CUtlVector< CUtlString > &vecList )
{
	KeyValues *pKVSub = pKV->FindKey(pszKeyName);
	if ( !pKVSub )
	{
		m_pItemLog->Warning( "ERROR: Failed to find a '%s' section in manifest file.\n", pszKeyName );
		return false;
	}

	FOR_EACH_SUBKEY( pKVSub, pKVSubKey )
	{
		const char *pszString = pKVSubKey->GetName();
		vecList.AddToTail( pszString );
	}

	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int	CItemTestManifest::GetMaterialType( const char *pszMaterialType )
{
	FOR_EACH_VEC( m_vecMaterialTypes, i )
	{
		if ( V_stricmp(m_vecMaterialTypes[i].pszMaterialType, pszMaterialType) == 0 )
			return i;
	}

	return kInvalidMaterialType;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int	CItemTestManifest::GetMaterialSkin( const char *pszMaterialSkin )
{
	FOR_EACH_VEC( m_vecMaterialSkins, i )
	{
		if ( V_stricmp(m_vecMaterialSkins[i].pszMaterialSkin, pszMaterialSkin) == 0 )
			return i;
	}

	return kInvalidMaterialSkin;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int	CItemTestManifest::GetTextureType( const char *pszTextureType )
{
	FOR_EACH_VEC( m_vecTextureTypes, i )
	{
		if ( V_stricmp(m_vecTextureTypes[i].pszTextureType, pszTextureType) == 0 )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
KeyValues *CItemTestManifest::GetTextureAddToVTEXConfig( const char *pszTextureType )
{
	int nIdx = GetTextureType(pszTextureType);
	if ( nIdx == -1 )
		return NULL;

	return m_vecTextureTypes[nIdx].pkvAddToVTEXConfig;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int	CItemTestManifest::GetIconType( const char *pszIconType )
{
	FOR_EACH_VEC( m_vecIconTypes, i )
	{
		if ( V_stricmp(m_vecIconTypes[i].pszIconType, pszIconType) == 0 )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestManifest::GetIconDimensions( int nIcon, int &nWidth, int &nHeight )
{
	if ( nIcon >= 0 && nIcon < m_vecIconTypes.Count() )
	{
		nWidth = m_vecIconTypes[nIcon].nWidth;
		nHeight = m_vecIconTypes[nIcon].nHeight;
		return true;
	}
	else
	{
		nWidth = 0;
		nHeight = 0;
		return false;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CItemTestManifest::GetVMTVarForTextureType( const char *pszTexture ) 
{ 
	int nIndex = m_vecVMTTextureRemaps.Find(pszTexture);
	if ( nIndex == m_vecVMTTextureRemaps.InvalidIndex() )
		return NULL;

	return m_vecVMTTextureRemaps[nIndex];
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int GetClassCount()
{
	return CItemUpload::Manifest()->GetNumClasses();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *GetClassString( int i )
{
	return ( i < 0 || i >= GetClassCount() ) ? NULL : CItemUpload::Manifest()->GetClass(i);
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *GetClassString( const char *pszClassString )
{
	if ( !pszClassString || V_strlen( pszClassString ) <= 0 )
		return NULL;

	// Make sure it exists in our manifest file
	for ( int i = 0; i < CItemUpload::Manifest()->GetNumClasses(); i++ )
	{
		const char *pszHero = CItemUpload::Manifest()->GetClass(i);
		if ( V_stricmp(pszHero, pszClassString) == 0 )
			return pszHero;
	}

	//Log_Warning( LOG_ITEMTEST, "Invalid class specified: %s\n", pszClassString );

	return NULL;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int GetClassIndex( const char *pszClassString )
{
	const char *pszCleanClassString = GetClassString( pszClassString );

	if ( !pszCleanClassString )
		return -1;

	for ( int i = 0; i < GetClassCount(); ++i )
	{
		if ( !V_stricmp( pszCleanClassString, GetClassString( i ) ) )
		{
			return i;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T >
const Vector &CItemUploadGame< T >::GetBipHead( int i )
{
	static const Vector vOrigin( 0, 0, 0 );

	return ( i < 0 || i >= GetClassCount() ) ? vOrigin : CItemUploadGame< T >::s_vBipHead[i];
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T >
const RadianEuler &CItemUploadGame< T >::GetBipHeadRotation( int i )
{
	static const RadianEuler eOrigin( 0, 0, 0 );

	return ( i < 0 || i >= GetClassCount() ) ? eOrigin : CItemUploadGame< T >::s_eBipHead[i];
}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const Vector CItemUploadGame< CItemUploadTF >::s_vBipHead[] =
{
	Vector(  0,				76.142968, -0.39608 ),	// demo
	Vector(  0,				69.030248, -1.264691 ),	// engineer
	Vector( -0.000138993,	79.541796, -3.352982 ),	// heavy
	Vector( -0.000111273,	76.504372, -0.565035 ),	// medic
	Vector( -0.000102534,	71.788881,  2.145585 ),	// pyro
	Vector( 0,				73.501752, -1.429994 ),	// scout
	Vector( 0,				75.982279, -3.858408 ),	// sniper
	Vector( 0,				75.194376, -1.120618 ),	// soldier
	Vector( 0,				75.679732, -2.87915  )	// spy
};

COMPILE_TIME_ASSERT( ARRAYSIZE( CItemUploadGame< CItemUploadTF >::s_vBipHead ) == CItemUploadTF::kClassCount );

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const RadianEuler CItemUploadGame< CItemUploadTF >::s_eBipHead[] =
{
	RadianEuler( DEG2RAD( -180.0 ),   0, 0 ),	// demo
	RadianEuler( DEG2RAD( -170.459 ), 0, 0 ),	// engineer
	RadianEuler( DEG2RAD( -180.0 ),   0, 0 ),	// heavy
	RadianEuler( DEG2RAD( -180.0 ),   0, 0 ),	// medic
	RadianEuler( DEG2RAD( -154.175 ), 0, 0 ),	// pyro
	RadianEuler( DEG2RAD( -173.451 ), 0, 0 ),	// scout
	RadianEuler( DEG2RAD( -172.722 ), 0, 0 ),	// sniper
	RadianEuler( DEG2RAD( -179.729 ), 0, 0 ),	// soldier
	RadianEuler( DEG2RAD( -180.0 ),   0, 0 )	// spy
};

COMPILE_TIME_ASSERT( ARRAYSIZE( CItemUploadGame< CItemUploadTF >::s_eBipHead ) == CItemUploadTF::kClassCount );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template class CItemUploadGame< CItemUploadTF >;


//-----------------------------------------------------------------------------
//
// Try and get the Steam Account ID and return it as a 10 character hex
// string prefixed with 0x.
//
//-----------------------------------------------------------------------------
bool CItemUpload::GetSteamId( CUtlString &sSteamId )
{
	if ( GetDevMode() )
	{
		sSteamId = "";
		return true;
	}

	const char *pszForcedSteamID = GetForcedSteamID();
	if ( pszForcedSteamID && pszForcedSteamID[0] )
	{
		sSteamId = pszForcedSteamID;
		return true;
	}

	bool bRetVal = false;

	char szBuf[ BUFSIZ ];
	szBuf[0] = '\0';

	// Try to query steam directly, this will fail if steam isn't running or the
	// process calling this function wasn't launched through steam (or through a
	// process launched through steam) or there isn't a steam_appid.txt in
	// the same directory as the executable running this (use 440 for TF).

	bool shutdownSteam = false;
	if ( !SteamClient() )
	{
		SteamAPI_InitSafe();
		SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
		shutdownSteam = true;
	}

	if ( SteamAPI_IsSteamRunning() )
	{
		g_SteamAPIContext.Init();

		ISteamUser *pSteamUser = g_SteamAPIContext.SteamUser();

		if ( pSteamUser )
		{
			CSteamID cSteamID = pSteamUser->GetSteamID();
			const uint32 nAccountID = cSteamID.GetAccountID();
			V_snprintf( szBuf, ARRAYSIZE( szBuf ), "0x%08x", nAccountID );

			bRetVal = true;
		}
	}

	if ( shutdownSteam )
	{
		SteamAPI_Shutdown();
	}

	sSteamId = szBuf;

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::GetVProjectDir( CUtlString &sVProjectDir )
{
	char szVProject[ BUFSIZ ] = "";

	GetModSubdirectory( "", szVProject, ARRAYSIZE( szVProject ) );
	V_StripTrailingSlash( szVProject );

	sVProjectDir = szVProject;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::GetVMod( CUtlString &sVMod )
{
	sVMod = "";

	CUtlString sVProjectDir;
	if ( !GetVProjectDir( sVProjectDir ) )
		return false;

	char szBuf[ k64KB ];
	V_FileBase( sVProjectDir.String(), szBuf, ARRAYSIZE( szBuf ) );

	sVMod = szBuf;

	return true;
}


//-----------------------------------------------------------------------------
// Guess where the SourceSDK root is based on executable directory...
// If there's a /bin/orangebox/bin/ in the executable path we know 
// the SourceSDK is above it, otherwise we don't know anything and false is
// returned
//
// If DevMode (-dev) then always returns false
//
// TODO: This is for TF & SourceSDK only and likely this is the only
//       weird configuration this hack needs to be done with.  If it's
//       a normal game/content tree then none of this hacky stuff
//       should be needed
//-----------------------------------------------------------------------------
bool CItemUpload::GetSourceSDKFromExe( CUtlString &sSourceSDK, CUtlString &sSourceSDKBin )
{
	if ( GetDevMode() )
		return false;

	CUtlString sCurrentExecutableFileName;
	GetCurrentExecutableFileName( sCurrentExecutableFileName );

	// Special hack for executables running out of the orange box SDK
	sCurrentExecutableFileName.FixSlashes( '/' );
	const char *pszBinOrangeBoxBin = V_strstr( sCurrentExecutableFileName.String(), "/bin/orangebox/bin/" );
	if ( pszBinOrangeBoxBin )
	{
		sSourceSDK.SetDirect( sCurrentExecutableFileName.String(), pszBinOrangeBoxBin - sCurrentExecutableFileName.String() );

		char szBinDir[ MAX_PATH ];
		V_ExtractFilePath( sCurrentExecutableFileName.String(), szBinDir, ARRAYSIZE( szBinDir ) );
		sSourceSDKBin = szBinDir;
		return true;
	}

	// Get Source SDK path
	HKEY hKey;
	char szSDKPath[ k64KB ];
	char szEngineVersion[ k64KB ];
	szSDKPath[0] = szEngineVersion[0] = '\0';

	GetEnvironmentVariable( "SOURCESDK", szSDKPath, sizeof( szSDKPath ) );

	if ( ERROR_SUCCESS == RegOpenKey( HKEY_CURRENT_USER, "Software\\Valve\\Source SDK", &hKey ) ) 
	{
		DWORD dwSize = sizeof( szEngineVersion );
		RegQueryValueEx( hKey, "EngineVer", NULL, NULL, (LPBYTE)szEngineVersion, &dwSize );
		RegCloseKey( hKey );
	}
	else
	{
		// Let's assume orange box for now
		V_strcpy_safe( szEngineVersion, "orangebox" );
	}

	if ( *szSDKPath )
	{
		// Normalize slashes to be consistent with the orange box SDK code above
		V_FixSlashes( szSDKPath, '/' );

		sSourceSDK = szSDKPath;
		sSourceSDKBin.Format( "%s/bin/%s/bin", szSDKPath, szEngineVersion );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Returns $SOURCESDK_content/FileName( $VPROJECT ) if it SOURCESDK is set
// otherwise returns VCONTENT if it is set, otherwise returns
// $VPROJECT/../
//-----------------------------------------------------------------------------
static bool CheckContentPath( CUtlString &sContentDir )
{
	char szContentDir[ MAX_PATH ];
	V_FixupPathName( szContentDir, ARRAYSIZE( szContentDir ), sContentDir );
	V_StripTrailingSlash( szContentDir );
	sContentDir = szContentDir;

	return g_pFullFileSystem->IsDirectory( sContentDir );
}
bool CItemUpload::GetContentDir( CUtlString &sContentDir )
{
	// Without VPROJECT set, can't figure anything out
	CUtlString sVMod;
	if ( !GetVMod( sVMod ) )
		return false;

	char szBuf[ k64KB ];

	// The game includes its own content directory?
	if ( IgnoreEnvironmentVariables() )
	{
		CUtlString sVProject;
		if ( !GetVProjectDir( sVProject ) )
			return false;
		sVProject.FixSlashes( '/' );

		// When we run in Steam, we get something like this back:
		//		"u:/steambeta/steamapps/common/[staging] dota 2/dota"
		// We need to trim off the game name, and append content.
		V_strcpy_safe( szBuf, sVProject );
		if ( !V_StripLastDir( szBuf, ARRAYSIZE( szBuf ) ) )
			return false;

		sContentDir = szBuf;
		sContentDir += "content/";
		sContentDir += sVMod;
		return true;
	}

	CUtlString sSourceSDK, sSourceSDKBin;

	// Check for the game/content layout in dev builds
	CUtlString sVProject;
	if ( GetVProjectDir( sVProject ) )
	{
		sVProject.FixSlashes( '/' );
		const char *pszGame = V_stristr( sVProject.String(), "/game/" );
		if ( pszGame )
		{
			sContentDir = sVProject;
			sContentDir.SetLength( pszGame - sVProject.String() );
			sContentDir += "/content/";
			sContentDir += sVMod;

			V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sContentDir.String() );
			sContentDir = szBuf;

			if ( CheckContentPath( sContentDir ) )
			{
				return true;
			}
		}
		else
		{
			// try to look for workshop/content in the mod dir
			sContentDir = sVProject;
			sContentDir += "/workshop/content";

			V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sContentDir.String() );
			sContentDir = szBuf;

			if ( CheckContentPath( sContentDir ) )
			{
				return true;
			}
		}
	}

	// Check for the VCONTENT environment variable
	if ( GetEnvironmentVariable( "VCONTENT", szBuf, ARRAYSIZE( szBuf ) ) != 0 )
	{
		sContentDir = szBuf;
		sContentDir += "/";
		sContentDir += sVMod;
		V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sContentDir.String() );
		sContentDir = szBuf;

		if ( CheckContentPath( sContentDir ) )
		{
			return true;
		}
	}

	// Check for the Source SDK in steam builds
	if ( GetSourceSDKFromExe( sSourceSDK, sSourceSDKBin ) )
	{
		sContentDir = sSourceSDK;
		sContentDir += "_content\\";
		sContentDir += sVMod;

		V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sContentDir.String() );
		sContentDir = szBuf;

		if ( CheckContentPath( sContentDir ) )
		{
			return true;
		}
	}

	return false;
}



//-----------------------------------------------------------------------------
//
// Find the directory for the binaries
//
//-----------------------------------------------------------------------------
static bool CheckToolPath( CUtlString &sBinDir )
{
	char szBinDir[ MAX_PATH ];
	V_FixupPathName( szBinDir, ARRAYSIZE( szBinDir ), sBinDir );
	V_StripTrailingSlash( szBinDir );
	sBinDir = szBinDir;

	char szVtexFileName[ MAX_PATH ];
	V_ComposeFileName( sBinDir, "vtex.exe", szVtexFileName, ARRAYSIZE( szVtexFileName ) );

	char szStudiomdlFileName[ MAX_PATH ];
	V_ComposeFileName( sBinDir, "studiomdl.exe", szStudiomdlFileName, ARRAYSIZE( szStudiomdlFileName ) );

	return g_pFullFileSystem->FileExists( szVtexFileName ) && g_pFullFileSystem->FileExists( szStudiomdlFileName );
}
bool CItemUpload::GetBinDirectory( CUtlString &sBinDir )
{
	// Get the full path to the executable this code is running in
	// this should be the 'bin' directory we want... just to be sure
	// make sure vtex.exe and studiomdl.exe exist in that directory

	CUtlString sCurrentExecutableFileName;
	GetCurrentExecutableFileName( sCurrentExecutableFileName );

	char szBinDir[ MAX_PATH ];
	V_ExtractFilePath( sCurrentExecutableFileName.String(), szBinDir, ARRAYSIZE( szBinDir ) );
	sBinDir = szBinDir;

	if ( CheckToolPath( sBinDir ) )
	{
		return true;
	}

	// Check for the game/bin directory
	CUtlString sVProject;
	if ( GetVProjectDir( sVProject ) && !sVProject.IsEmpty() )
	{
		sVProject.FixSlashes( '/' );
		const char *pszGame = V_stristr( sVProject.String(), "/game/" );
		if ( pszGame )
		{
			sBinDir = sVProject;
			sBinDir.SetLength( pszGame - sVProject.String() );
			sBinDir += "/game/bin";

			if ( CheckToolPath( sBinDir ) )
			{
				return true;
			}
		}

		// When we run in Steam, we get something like this back:
		//		"u:/steambeta/steamapps/common/[staging] dota 2/dota"
		// We need to trim off the game name, and append bin.
		V_strcpy_safe( szBinDir, sVProject );
		if ( !V_StripLastDir( szBinDir, ARRAYSIZE( szBinDir ) ) )
			return false;

		sBinDir = szBinDir;
		sBinDir += "/bin";

		if ( CheckToolPath( sBinDir ) )
		{
			return true;
		}
	}

	// Check for the Source SDK in steam builds
	CUtlString sSourceSDK;
	if ( !GetDevMode() && GetSourceSDKFromExe( sSourceSDK, sBinDir ) )
	{
		if ( CheckToolPath( sBinDir ) )
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::FileExists( const char *pszFilename )
{
	DWORD attribs = ::GetFileAttributesA( pszFilename );
	if ( attribs == INVALID_FILE_ATTRIBUTES )
		return false;

	return ( ( attribs & FILE_ATTRIBUTE_DIRECTORY ) == 0 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::CopyFiles( const char *pszSourceDir, const char *pszPattern, const char *pszDestDir )
{
	char szFindPattern[ k64KB ];
	bool bAllSucceeded = true;

	V_snprintf( szFindPattern, sizeof( szFindPattern ), "%s%s", pszSourceDir, pszPattern );

	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile( szFindPattern, &findData );
	if ( hFind == INVALID_HANDLE_VALUE )
	{
		return false;
	}
	else
	{
		do
		{
			char szSrcPath[ k64KB ];
			char szDestPath[ k64KB ];

			V_snprintf( szSrcPath, sizeof( szSrcPath ), "%s%s", pszSourceDir, findData.cFileName );
			V_snprintf( szDestPath, sizeof( szDestPath ), "%s\\%s", pszDestDir, findData.cFileName );

			DeleteFile( szDestPath );
			::CopyFile( szSrcPath, szDestPath, false );
			bAllSucceeded &= FileExists( szDestPath );

		} while ( FindNextFile( hFind, &findData ) );
		FindClose( hFind );

		return bAllSucceeded;
	}
}


static bool DoFileCopy( const char *pszSourceFile, const char *pszDestFile )
{
	int             remaining, count;
	char			buf[4096];
	FileHandle_t	in, out;

	in = g_pFullFileSystem->Open( pszSourceFile, "rb" );

	AssertMsg( in, "DoFileCopy: Input file failed to open" );

	if ( in == FILESYSTEM_INVALID_HANDLE )
		return false;
		
	// create directories up to the cache file
	char szDestPath[MAX_PATH];
	V_ExtractFilePath( pszDestFile, szDestPath, sizeof( szDestPath ) );
	g_pFullFileSystem->CreateDirHierarchy( szDestPath );

	out = g_pFullFileSystem->Open( pszDestFile, "wb" );

	AssertMsg( out, "DoFileCopy: Output file failed to open" );
	
	if ( out == FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Close( in );
		return false;
	}
		
	remaining = g_pFullFileSystem->Size( in );
	while ( remaining > 0 )
	{
		if (remaining < sizeof(buf))
		{
			count = remaining;
		}
		else
		{
			count = sizeof(buf);
		}
		g_pFullFileSystem->Read( buf, count, in );
		g_pFullFileSystem->Write( buf, count, out );
		remaining -= count;
	}

	g_pFullFileSystem->Close( in );
	g_pFullFileSystem->Close( out );   
	
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemUpload::CopyFile( const char *pszSourceFile, const char *pszDestFile )
{
	if ( ::CopyFile( pszSourceFile, pszDestFile, false ) == 0 )
		return false;

	DWORD nFileAttr = GetFileAttributes( pszDestFile );
	if ( nFileAttr == INVALID_FILE_ATTRIBUTES )
		return false;

	nFileAttr &= ~FILE_ATTRIBUTE_READONLY;
	SetFileAttributes( pszDestFile, nFileAttr );

	return true;
}


//=============================================================================
//
//=============================================================================
static bool RemoveTextBlock( const char *str, char const *search, char *pszOutBuf, int nSizeofOutBuf )
{
	if ( str != pszOutBuf )
	{
		V_strncpy( pszOutBuf, str, nSizeofOutBuf );
	}

	bool changed = false;
	if ( !V_strstr( str, search ) )
	{
		return false;
	}

	int offset = 0;
	while ( true )
	{
		char* pos = V_strstr( str + offset, search );
		if ( !pos )
		{
			break;
		}

		CUtlString temp = str;

		// Found an instance
		int left = pos - str;
		CUtlString strLeft = temp.Slice( 0, left );

		pos = V_strstr( str + left, "}" );
		if ( !pos )
		{
			AssertMsg( pos, "cannot find end of text block\n" );
			return false;
		}

		int right = pos - str + 1;
		CUtlString strRight = temp.Slice( right );

		temp = strLeft;
		temp += strRight;

		// Replace entire string
		V_strncpy( pszOutBuf, temp.String(), nSizeofOutBuf );

		offset = right;

		changed = true;
	}

	return changed;
}


//=============================================================================
//
//=============================================================================
CTargetBase::CTargetBase( CAsset *pAsset, const CTargetBase *pTargetParent )
: CItemLog( pAsset )
, m_pAsset( pAsset )
, m_nRefCount( 0 )
, m_pTargetParent( pTargetParent )
, m_bIgnoreP4( false )
, m_kvCustomKeys( new KeyValues( "custom keys" ) )
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetBase::Compile()
{
	CUtlString sName;
	if ( !GetOutputPath( sName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
	{
		Warning( "CTarget%s::Compile - GetOutputPath failed\n", GetTypeString() );
		return false;
	}

	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( "CTarget%s::Compile( %s ) - Not Valid: %s\n", GetTypeString(), sName.String(), sTmp.String() );
		return false;
	}

	// Compile all inputs first

	if ( !CreateOutputDirectory() )
	{
		Warning( "CTarget%s::Compile - CreateOutputDirectory failed\n", GetTypeString() );
		return false;
	}

	CUtlVector< CTargetBase * > inputs;
	bool bRet = GetInputs( inputs );

	if ( !bRet )
	{
		Warning( "CTarget%s::Compile - GetInputs failed\n", GetTypeString() );
		return bRet;
	}

	for ( int i = 0; i < inputs.Count(); ++i )
	{
		CTargetBase *pTargetBase = inputs.Element( i );

		if ( !pTargetBase )
		{
			Warning( "WARNING: CTarget%s::Compile - Target %d NULL\n", GetTypeString(), i );
			continue;
		}

		if ( !pTargetBase->Compile() )
		{
			Warning( "WARNING: CTarget%s::Compile - Target %d Compile Failed\n", GetTypeString(), i );
			bRet = false;
			break;
		}
	}

	return bRet;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CTargetBase::GetItemDirectory() const
{
	if ( m_pTargetParent )
	{
		return m_pTargetParent->GetItemDirectory();
	}
	else
	{
		return CItemUpload::Manifest()->GetItemDirectory();
	}
}


//-----------------------------------------------------------------------------
// Return the number of files that are output from this CTarget
//-----------------------------------------------------------------------------
int CTargetBase::GetOutputCount() const
{
	const ExtensionList *pList = GetExtensionsAndCount();
	return (pList ? pList->Count() : 0);
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetBase::GetOutputPath(
	CUtlString &sOutputPath,
	int nIndex /* = 0 */,
	uint nPathFlags /* = PATH_FLAG_ALL */ ) const
{
	sOutputPath.Clear();

	if ( nIndex < 0 )
		return false;

	CUtlVector< CUtlString > sOutputPaths;
	if ( !GetOutputPaths( sOutputPaths, nPathFlags ) )
		return false;

	if ( nIndex >= sOutputPaths.Count() )
		return false;

	sOutputPath = sOutputPaths.Element( nIndex );

	return ( sOutputPath.Length() > 0 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetBase::GetOutputPaths(
	CUtlVector< CUtlString > &sOutputPaths,
	uint nPathFlags /* = PATH_FLAG_ALL */,
	bool bRecurse /* = false */ ) const
{
	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	CUtlString sDirName;

	if ( nPathFlags & PATH_FLAG_PATH )
	{
		if ( !GetDirName( sDirName, nPathFlags ) )
			return false;
	}

	const ExtensionList *pvecExtensions = GetExtensionsAndCount();
	int nExtCount = pvecExtensions ? pvecExtensions->Count() : 0;

	for ( int i = 0; i < nExtCount; ++i )
	{
		CUtlString &sOutputPath = sOutputPaths.Element( sOutputPaths.AddToTail() );

		if ( nPathFlags & PATH_FLAG_PATH )
		{
			sOutputPath = sDirName;
		}

		if ( nPathFlags & PATH_FLAG_FILE )
		{
			if ( sOutputPath.Length() > 0 )
			{
				sOutputPath += "/";
			}

			CUtlString sName;
			GetName( sName );

			sOutputPath += sName;

			if ( nPathFlags & PATH_FLAG_EXTENSION )
			{
				sOutputPath += pvecExtensions->Element(i);
			}
		}

		sOutputPath.FixSlashes();
	}

	bool bRet = sOutputPaths.Count() > 0;

	if ( bRecurse )
	{
		CUtlVector< CTargetBase * > inputs;
		if ( GetInputs( inputs ) )
		{
			for ( int i = 0; i < inputs.Count(); ++i )
			{
				CTargetBase *pTargetBase = inputs.Element( i );
				if ( !pTargetBase )
					continue;

				bRet = pTargetBase->GetOutputPaths( sOutputPaths, nPathFlags, bRecurse );
			}
		}
		else
		{
			bRet = false;
		}
	}

	return bRet;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetBase::GetOutputPaths(
	CUtlVector< CUtlString > &sOutputPaths,
	bool bRelative /* = true */,
	bool bRecurse /* = true */,
	bool bExtension /* = true */,
	bool bPrefix /* = true */ ) const
{
	CUtlString sTmp;

	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	CUtlString sDirA;

	if ( bRelative )
	{
		if ( !Asset()->GetRelativeDir( sDirA, bPrefix ? GetPrefix() : NULL, this ) )
			return false;
	}
	else
	{
		if ( !Asset()->GetAbsoluteDir( sDirA, bPrefix ? GetPrefix() : NULL, this ) )
			return false;
	}

	const ExtensionList *pvecExtensions = GetExtensionsAndCount();
	int nExtCount = pvecExtensions ? pvecExtensions->Count() : 0;

	for ( int i = 0; i < nExtCount; ++i )
	{
		CUtlString &sRelativePath = sOutputPaths.Element( sOutputPaths.AddToTail() );

		CUtlString sName;
		GetName( sName );

		sRelativePath = sDirA;
		sRelativePath += "/";
		sRelativePath += sName;

		if ( bExtension )
		{
			sRelativePath += pvecExtensions->Element(i);
		}

		sRelativePath.FixSlashes();
	}

	bool bRet = sOutputPaths.Count() > 0;

	if ( bRecurse )
	{
		CUtlVector< CTargetBase * > inputs;
		if ( GetInputs( inputs ) )
		{
			for ( int i = 0; i < inputs.Count(); ++i )
			{
				CTargetBase *pTargetBase = inputs.Element( i );
				if ( !pTargetBase )
					continue;

				bRet = pTargetBase->GetOutputPaths( sOutputPaths, bRelative, bRecurse, bExtension ) && bRet;
			}
		}
		else
		{
			bRet = false;
		}
	}

	return bRet;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetBase::GetInputPaths(
	CUtlVector< CUtlString > &sInputPaths,
	bool bRelative /* = true */,
	bool bRecurse /* = true */,
	bool bExtension /* = true */ )
{
	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	CUtlVector< CTargetBase * > inputs;
	if ( !GetInputs( inputs ) )
		return false;

	bool bRet = true;

	for ( int i = 0; bRet && i < inputs.Count(); ++i )
	{
		CTargetBase *pTargetBase = inputs.Element( i );
		if ( !pTargetBase )
			continue;

		bRet = pTargetBase->GetOutputPaths( sInputPaths, bRelative, false, bExtension ) && bRet;

		if ( bRecurse )
		{
			bRet = pTargetBase->GetInputPaths( sInputPaths, bRelative, bRecurse, bExtension ) && bRet;
		}
	}

	return bRet;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetBase::GetName( CUtlString &sName ) const
{
	if ( !GetCustomOutputName().IsEmpty() )
	{
		sName = GetCustomOutputName();
		sName += GetNameSuffix();
		return;
	}

	// don't take parent's custom output name
	if ( m_pTargetParent && m_pTargetParent->GetCustomOutputName().IsEmpty() )
	{
		m_pTargetParent->GetName( sName );
	}
	else
	{
		sName = GetAssetName();
	}

	sName += GetNameSuffix();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const CUtlString &CTargetBase::GetAssetName() const
{
	return Asset()->GetAssetName();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CAsset *CTargetBase::Asset() const
{
	return m_pAsset;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetBase::CheckFile( const char *pszFilename ) const
{
	if ( _access( pszFilename, 06 ) == 0 )
		return true;

	Warning( "CTarget%s::Compile NO FILE! - %s\n", GetTypeString(), pszFilename );

	return false;
}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetBase::GetDirName(
	CUtlString &sDirName,
	uint nPathFlags /* = PATH_FLAG_ALL */ ) const
{
	sDirName.Clear();

	CAsset *pAsset = Asset();
	if ( !pAsset )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - NULL Asset\n", GetTypeString() );
		return false;
	}

	CUtlString sTmp;
	if ( !pAsset->IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	if ( nPathFlags & PATH_FLAG_ABSOLUTE )
	{
		if ( IsContent() )
		{
			if ( !CItemUpload::GetContentDir( sDirName ) )
				return false;
		}
		else
		{
			if ( !CItemUpload::GetVProjectDir( sDirName ) )
				return false;
		}

		sDirName += "/";

		// ABSOLUTE implies PREFIX & MODELS
		nPathFlags |= PATH_FLAG_PREFIX;
		nPathFlags |= PATH_FLAG_MODELS;
	}

	if ( nPathFlags & PATH_FLAG_ZIP )
	{
		const char *pszZipPrefix;

		if ( IsContent() )
		{
			pszZipPrefix = CItemUpload::Manifest()->GetZipSourceDirectory();
		}
		else
		{
			pszZipPrefix = CItemUpload::Manifest()->GetZipOutputDirectory();
		}

		if ( *pszZipPrefix )
		{
			sDirName += pszZipPrefix;
			sDirName += "/";
		}
	}

	if ( ( nPathFlags & PATH_FLAG_PREFIX ) && ( nPathFlags & PATH_FLAG_MODELS ) )
	{
		const char *pszPrefix = GetPrefix();
		if ( pszPrefix )
		{
			sDirName += pszPrefix;
			sDirName += "/";
		}

		// PREFIX implies MODELS
		nPathFlags |= PATH_FLAG_MODELS;
	}

	if ( nPathFlags & PATH_FLAG_MODELS && IsModelPath() )
	{
		// If not starting with prefix, then optionally start with models
		sDirName += "models/";
	}

	if ( GetCustomRelativeDir() )
	{
		sDirName += GetCustomRelativeDir();
	}
	else
	{
		sDirName += GetItemDirectory();
		sDirName += pAsset->GetClass();
		sDirName += "/";

		if ( CItemUpload::Manifest()->GetItemPathUsesSteamId() )
		{
			const char *pszSteamId = pAsset->GetSteamId();
			if ( pszSteamId )
			{
				sDirName += pszSteamId;
				sDirName += "/";
			}
		}
		
		sDirName += pAsset->GetAssetName();
	}

	char szBuf[ k64KB ];

	V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sDirName.String() );
	sDirName = szBuf;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetBase::CreateOutputDirectory() const
{
	CUtlString sDir;
	if ( !GetOutputPath( sDir, 0, PATH_FLAG_PATH | PATH_FLAG_ABSOLUTE ) )
		return false;

	if ( !CItemUpload::CreateDirectory( sDir.String() ) )
	{
		Warning( "CTarget%s::CreateDirectory( %s ) - Failed\n", GetTypeString(), sDir.String() );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetBase::AddOrEditP4File( const char *pszFilePath )
{
	if ( m_bIgnoreP4 )
		return;

	if ( CItemUpload::GetP4() )
	{
		char szCorrectCaseFilePath[MAX_PATH];
		g_pFullFileSystem->GetCaseCorrectFullPath( pszFilePath, szCorrectCaseFilePath );
		CP4AutoEditAddFile a( szCorrectCaseFilePath );
	}
}


//=============================================================================
//
//=============================================================================
bool VTFGetInfo( const char *fileName, int *width, int *height, ImageFormat *imageFormat, float *sourceGamma )
{
	// Just load the whole file into a memory buffer
	CUtlBuffer bufFileContents;
	if ( !g_pFullFileSystem->ReadFile( fileName, NULL, bufFileContents ) )
	{
		return false;
	}

	IVTFTexture *pVTFTexture = CreateVTFTexture();
	if ( !pVTFTexture->Unserialize( bufFileContents, true ) )
	{
		return false;
	}

	*width = pVTFTexture->Width();
	*height = pVTFTexture->Height();
	// It's not actually RGBA, but it will be when we decompress and load it...
	*imageFormat = IMAGE_FORMAT_RGBA8888;
	*sourceGamma = 0.0f;

	DestroyVTFTexture( pVTFTexture );

	return true;
}

//=============================================================================
//
//=============================================================================
CTargetTGA::CTargetTGA( CAsset *pAsset, const CTargetVMT *pTargetVMT )
: CTargetBase( pAsset, pTargetVMT )
, m_pTargetVMT( pTargetVMT )
, m_nSrcImageType( IMAGE_FILE_UNKNOWN )
, m_nWidth( 0 )
, m_nHeight( 0 )
, m_nChannelCount( 0 )
, m_bNoNiceFiltering( false )
, m_bAlpha( false )
, m_bPowerOfTwo( false )
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CTargetTGA::~CTargetTGA()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetTGA::IsOk( CUtlString &sMsg ) const
{
	// No input file specified
	if ( m_sInputFile.Length() <= 0 )
	{
		sMsg = "No input file specified";
		return false;
	}

	// Input file invalid size
	if ( m_nWidth <= 0 )
	{
		sMsg = "Invalid image width (";
		sMsg += m_nWidth;
		sMsg += ")";
		return false;
	}

	if ( m_nHeight <= 0 )
	{
		sMsg = "Invalid image height (";
		sMsg += m_nHeight;
		sMsg += ")";
		return false;
	}

	// TODO: Maximum size?

	// Only 3 or 4 channel images ok
	if ( m_nChannelCount != 3 && m_nChannelCount != 4 )
	{
		sMsg = "Invalid number of channels (";
		sMsg += m_nChannelCount;
		sMsg = ") only 3 (RGB) and 4 (RGBA) channel images allowed";
		return false;
	}

	// Has to be a power of two
	if ( !m_bPowerOfTwo )
	{
		sMsg = "Image dimensions are not a power of two";
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetTGA::IsModelPath() const
{
	return m_pTargetVMT->IsModelPath();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetTGA::GetInputPaths(
	CUtlVector< CUtlString > &sInputPaths,
	bool bRelative /* = true */,
	bool bRecurse /* = true */,
	bool bExtension /* = true */ )
{
	sInputPaths.AddToTail( m_sInputFile );

	return CTargetBase::GetInputPaths( sInputPaths, bRelative, bRecurse, bExtension );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const ExtensionList *CTargetTGA::GetExtensionsAndCount( void ) const
{
	static ExtensionList vecExtensions;
	if ( !vecExtensions.Count() )
	{
		vecExtensions.AddToTail( ".tga" );
		vecExtensions.AddToTail( ".txt" );
	}

	return &vecExtensions;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CTargetTGA::GetPrefix() const
{
	if ( m_sPrefix.IsEmpty() ) {
		CUtlString strParentPrefix = m_pTargetVMT->GetPrefix();
		m_sPrefix = strParentPrefix.Replace( "materials", "materialsrc" );
	}
	return m_sPrefix.String();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetTGA::GetName( CUtlString &sName ) const
{
	if ( GetCustomOutputName().IsEmpty() )
	{
		CTargetBase::GetName( sName );
	}
	else
	{
		sName = GetCustomOutputName();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetTGA::Compile()
{
	// 'reset' the input file from the current value to force a reload on compile, make a temp copy as it's going to be overwritten
	if ( !SetInputFile( CUtlString( GetInputFile() ).String() ) )
		return false;

	if ( !CTargetBase::Compile() )
		return false;

	CUtlString sName;
	if ( !GetOutputPath( sName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
	{
		Warning( "CTarget%s::Compile - GetOutputPath Failed\n", GetTypeString() );
		return false;
	}

	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( "CTarget%s::Compile( %s ) - Not Valid: %s\n", GetTypeString(), sName.String(), sTmp.String() );
		return false;
	}

	CUtlString sAbsPath;
	GetOutputPath( sAbsPath, 0 );

	CUtlString sRelPath;
	GetOutputPath( sRelPath, 0, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE );

	if ( sAbsPath.IsEmpty() || sRelPath.IsEmpty() )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPath failed\n", GetTypeString(), sName.String() );
		return false;
	}

	Msg( "Compiling %s: %s\n", GetTypeString(), sRelPath.String() );

	if ( CItemUpload::IsSameFile( m_sInputFile.String(), sAbsPath.String() ) )
	{
		Warning( "CTarget%s::Compile( %s ) - Same File, No Work To Do!\n", GetTypeString(), sName.String() );
		return true;
	}

	Bitmap_t bitmap;
	CUtlMemory< unsigned char > tgaBits;

	switch ( m_nSrcImageType )
	{
		case IMAGE_FILE_TGA:
		{
			int nWidth = 0, nHeight = 0;
			if ( !TGALoader::LoadRGBA8888( m_sInputFile.String(), tgaBits, nWidth, nHeight ) )
			{
				Warning( "CTarget%s::Compile( %s ) - Couldn't Load TGA\n", GetTypeString(), sName.String() );
				return false;
			}

			bitmap.SetBuffer( nWidth, nHeight, IMAGE_FORMAT_RGBA8888, tgaBits.Base(), false, nWidth*4 );
		}
		break;

		case IMAGE_FILE_PSD:
		{
			if ( !PSDReadFileRGBA8888( m_sInputFile.String(), NULL, bitmap ) )
			{
				Warning( "CTarget%s::Compile( %s ) - Couldn't Load PSD\n", GetTypeString(), sName.String() );
				return false;
			}
		}
		break;

		case IMAGE_FILE_VTF:
		{
			// Just load the whole file into a memory buffer
			CUtlBuffer bufFileContents;
			if ( !g_pFullFileSystem->ReadFile( m_sInputFile.String(), NULL, bufFileContents ) )
			{
				Warning( "CTarget%s::Compile( %s ) - Couldn't Load VTF\n", GetTypeString(), sName.String() );
				return false;
			}

			IVTFTexture *pVTFTexture = CreateVTFTexture();
			if ( !pVTFTexture->Unserialize( bufFileContents ) )
			{
				Warning( "CTarget%s::Compile( %s ) - Couldn't Load VTF\n", GetTypeString(), sName.String() );
				return false;
			}

			int nWidth = pVTFTexture->Width();
			int nHeight = pVTFTexture->Height();
			pVTFTexture->ConvertImageFormat( IMAGE_FORMAT_RGBA8888, false );

			int nMemSize = ImageLoader::GetMemRequired( nWidth, nHeight, 1, IMAGE_FORMAT_RGBA8888, false );
			tgaBits.EnsureCapacity( nMemSize );
			Q_memcpy( tgaBits.Base(), pVTFTexture->ImageData(), nMemSize );

			DestroyVTFTexture( pVTFTexture );

			bitmap.SetBuffer( nWidth, nHeight, IMAGE_FORMAT_RGBA8888, tgaBits.Base(), false, nWidth*4 );
		}
		break;
	}

	if ( bitmap.Format() != IMAGE_FORMAT_RGBA8888 || bitmap.Width() != m_nWidth || bitmap.Height() != m_nHeight )
	{
		Warning( "CTarget%s::Compile( %s ) - Invalid Bitmap Size, Expected %d x %d x %d (%d)\n",
			GetTypeString(), sName.String(), m_nWidth, m_nHeight, 4, m_nWidth * m_nHeight * 4 );
		return false;
	}

	CUtlBuffer bufVTEXConfig( 0, 0, CUtlBuffer::TEXT_BUFFER );

	// If we're supposed to resize these to another resolution, setup the VTEX config file to do so.
	int nTargetWidth = m_pTargetVMT->GetTargetWidth();
	int nTargetHeight = m_pTargetVMT->GetTargetHeight();
	if ( nTargetWidth && nTargetHeight )
	{
		// We want to use "reduce", not "maxwidth" & "maxheight", so we throw away the higher resolution mips.
		// Determine the right amount of reduction based on the texture's width & height (and fail if it's the wrong aspect ratio)
		int nFactor = (m_nWidth / nTargetWidth);
		if ( nFactor <= 0 )
		{
			Warning( "CTarget%s::Compile( %s ) - Failed to determine reduction factor (target width is %d, texture is %d)\n", GetTypeString(), sName.String(), nTargetWidth, m_nWidth );
			return false;
		}
		int nHeightFactor = (m_nHeight / nTargetHeight);
		if ( nFactor != nHeightFactor )
		{
			Warning( "CTarget%s::Compile( %s ) - Failed to determine reduction factor (target size aspect ratio (%dx%d) doesn't match texture's aspect ratio (%dx%d))\n", GetTypeString(), sName.String(), nTargetWidth, nTargetHeight, m_nWidth, m_nHeight );
			return false;
		}

		if ( nFactor > 1 )
		{
			bufVTEXConfig.PutString( CFmtStr("reduce %d\n", nFactor) );
		}
	}

	KeyValues *pKV = m_pTargetVMT->GetTextureAddToVTEXConfigForTGA( this );
	if ( pKV )
	{
		FOR_EACH_SUBKEY( pKV, pKVEntry )
		{
			bufVTEXConfig.Printf( "%s %s\n", pKVEntry->GetName(), pKVEntry->GetString() );
		}
	}

	// check if we should modify abspath
	CUtlString sTGAOutput = Asset()->CheckRedundantOutputFilePath( GetInputFile().String(), bufVTEXConfig.String(), sAbsPath.String() );
	AddOrEditP4File( sTGAOutput.String() );

	char szVTEXConfigFilename[MAX_PATH];
	V_strcpy_safe( szVTEXConfigFilename, sTGAOutput.String() );
	V_SetExtension( szVTEXConfigFilename, ".txt", sizeof(szVTEXConfigFilename) );
	AddOrEditP4File( szVTEXConfigFilename );
	g_pFullFileSystem->WriteFile( szVTEXConfigFilename, NULL, bufVTEXConfig );

	// TODO: Don't write alpha if VMT using this doesn't specify the alpha is used for anything
	CUtlBuffer outBuf;
	const bool bWriteTGA = TGAWriter::WriteToBuffer( bitmap.GetBits(), outBuf, bitmap.Width(), bitmap.Height(), bitmap.Format(), m_bAlpha ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_BGR888 );

	if ( !bWriteTGA )
	{
		Warning( "CTarget%s::Compile( %s ) - Couldn't write TGA to buffer\n", GetTypeString(), sName.String() );
		return false;
	}

	if ( !g_pFullFileSystem->WriteFile( sTGAOutput.String(), NULL, outBuf ) )
	{
		Warning( "CTarget%s::Compile( %s ) - Couldn't write TGA to file \"%s\"\n", GetTypeString(), sName.String(), sTGAOutput.String() );
		return false;
	}

	if ( !CheckFile( sTGAOutput.String() ) )
	{
		Warning( "CTarget%s::Compile( %s ) - File Check Failed - \"%s\"\n", GetTypeString(), sName.String(), sTGAOutput.String() );
		return false;
	}

	if ( CItemUpload::Manifest()->UseTerseMessages() )
	{
		Msg( " - Compilation successful.\n" );
	}
	else
	{
		Msg( "CTarget%s::Compile OK! - %s\n", GetTypeString(), sRelPath.String() );
	}

	// store the output name to use it to output VTF file
	char szFileName[FILENAME_MAX];
	V_StripExtension( V_GetFileName( szVTEXConfigFilename ), szFileName, ARRAYSIZE( szFileName ) );
	SetCustomOutputName( szFileName );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetTGA::Clear()
{
	m_sInputFile = "";
	m_sFileBase = "";
	m_sExtension = "";

	m_nWidth = 0;
	m_nHeight = 0;
	m_nChannelCount = 0;
	m_bNoNiceFiltering = false;
	m_bAlpha = false;
	m_bPowerOfTwo = false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetTGA::SetInputFile( const char *pszFilename )
{
	Clear();

	if ( !pszFilename || V_strlen( pszFilename ) <= 0 )
	{
		Warning( "ERROR: Empty filename specified for TGA file\n" );
		return false;
	}

	char szBuf[ k64KB ];

	m_sInputFile = pszFilename;

	V_FileBase( pszFilename, szBuf, ARRAYSIZE( szBuf ) );
	m_sFileBase = szBuf;

	// Try to automatically handle the suffixes for properly named assets
	static const char *szSuffixes[] =
	{
		//"_color",
		"_normal",
		"_height",
		"_specmask"
		"_specexp",
		"_trans",
		"_illum",
		"_color_red",
		"_color_blue"
	};

	CUtlString sTmp;

	for ( int i = 0; i < ARRAYSIZE( szSuffixes ); ++i )
	{
		const char *pszSuffix = szSuffixes[i];

		sTmp = GetAssetName();
		sTmp += pszSuffix;
		if ( m_sFileBase == sTmp )
		{
			SetNameSuffix( pszSuffix );
		}
	}
	
	V_ExtractFileExtension( pszFilename, szBuf, ARRAYSIZE( szBuf ) );
	m_sExtension = szBuf;

	ImageFormat imageFormat;
	float flSourceGamma = 0;

	if ( TGALoader::GetInfo( pszFilename, &m_nWidth, &m_nHeight, &imageFormat, &flSourceGamma ) )
	{
		m_nSrcImageType = IMAGE_FILE_TGA;
	}
	else if ( PSDGetInfo( pszFilename, NULL, &m_nWidth, &m_nHeight, &imageFormat, &flSourceGamma ) )
	{
		m_nSrcImageType = IMAGE_FILE_PSD;
	}
	else if ( VTFGetInfo( pszFilename, &m_nWidth, &m_nHeight, &imageFormat, &flSourceGamma ) )
	{
		m_nSrcImageType = IMAGE_FILE_VTF;
	}
	else
	{
		Warning( "ERROR: Specified file is not a TGA (Targa) or PSD File: \"%s\"\n", szBuf );

		Clear();

		return false;
	}

	// ImageFormat can only be one of:
	switch ( imageFormat )
	{
	case IMAGE_FORMAT_I8:
		m_nChannelCount = 1;
		m_bAlpha = false;
		break;
	case IMAGE_FORMAT_ABGR8888:
	case IMAGE_FORMAT_BGRA8888:
		m_nChannelCount = 4;
		m_bAlpha = true;
		break;
	case IMAGE_FORMAT_BGR888:
		m_nChannelCount = 3;
		m_bAlpha = false;
		break;
	case IMAGE_FORMAT_RGBA8888:
		m_nChannelCount = 4;
		m_bAlpha = true;
		break;
	case IMAGE_FORMAT_RGB888:
		m_nChannelCount = 3;
		m_bAlpha = false;
		break;
	default:
		break;
	}

	const int nWidthPow = NearestPowerOfTwo( m_nWidth );
	const int nHeightPow = NearestPowerOfTwo( m_nHeight );

	if ( nWidthPow == m_nWidth && nHeightPow == m_nHeight )
	{
		m_bPowerOfTwo = true;
	}
	else
	{
		Warning( "ERROR: Specified texture file (%s) Size %dx%d dimensions not powers of two, perhaps resize to %dx%d\n",
			m_sInputFile.String(),
			m_nWidth, m_nHeight,
			nWidthPow, nHeightPow );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const CUtlString &CTargetTGA::GetInputFile() const
{
	return m_sInputFile;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T >
T CTargetTGA::NearestPowerOfTwo( T v )
{
	if (v == 0)
		return static_cast< T >( 1 );

	T k;
	for ( k = sizeof( T ) * 8 - 1; ( ( static_cast< T >( 1U ) << k) & v ) == 0; --k);

	if ( ( ( static_cast< T >( 1U ) << ( k - 1 ) ) & v ) == 0 )
		return static_cast< T >( 1U ) << k;

	return static_cast< T >( 1U ) << ( k + 1 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetTGA::UpdateManifest( KeyValues *pKv )
{
	pKv->SetString( "filename", m_sInputFile.String() );
	CUtlString sOutName;
	if ( GetOutputPath( sOutName, 0, PATH_FLAG_PATH | PATH_FLAG_FILE | PATH_FLAG_PREFIX | PATH_FLAG_MODELS | PATH_FLAG_EXTENSION ) )
	{
		pKv->SetString( "out_filename", sOutName );
	}
	pKv->SetInt( "width", m_nWidth );
	pKv->SetInt( "height", m_nHeight );
	pKv->SetInt( "channels", m_nChannelCount );
	pKv->SetInt( "nonice", m_bNoNiceFiltering );
	pKv->SetBool( "alpha", m_bAlpha );

	const char *pszTextureType = m_pTargetVMT->GetTextureTypeForTGA( this );
	if ( pszTextureType )
	{
		KeyValues *pVTEXKV = CItemUpload::Manifest()->GetTextureAddToVTEXConfig( pszTextureType );
		if ( pVTEXKV )
		{
			KeyValues *pTmpKey = new KeyValues( pVTEXKV->GetName() );
			pKv->AddSubKey( pTmpKey );
			pVTEXKV->CopySubkeys( pTmpKey );
		}
	}
}


//=============================================================================
//
//=============================================================================
CTargetVTF::CTargetVTF( CAsset *pAsset, const CTargetVMT *pTargetVMT )
: CTargetBase( pAsset, pTargetVMT )
, m_pTargetVMT( pTargetVMT )
{
	m_pTargetTGA = Asset()->NewTarget< CTargetTGA >( m_pTargetVMT );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CTargetVTF::~CTargetVTF()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVTF::IsModelPath() const
{
	return m_pTargetVMT->IsModelPath();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVTF::Compile()
{
	if ( !CTargetBase::Compile() )
		return false;

	CUtlString sName;
	if ( !GetOutputPath( sName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
		return false;

	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( "CTarget%s::Compile( %s ) - Not Valid: %s\n", GetTypeString(), sName.String(), sTmp.String() );
		return false;
	}

	CUtlString sBinDir;
	if ( !CItemUpload::GetBinDirectory( sBinDir ) )
	{
		Warning( "CTarget%s::Compile( %s ) - GetBinDirectory Failed\n", GetTypeString(), sName.String() );
		return false;
	}

	CUtlString sAbsPath;
	GetOutputPath( sAbsPath, 0 );

	CUtlString sRelPath;
	GetOutputPath( sRelPath, 0, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE );

	if ( sAbsPath.IsEmpty() || sRelPath.IsEmpty() )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPath failed\n", GetTypeString(), sName.String() );
		return false;
	}

	Msg( "Compiling %s: %s\n", GetTypeString(), sRelPath.String() );

	AddOrEditP4File( sAbsPath.String() );

	CUtlVector< CUtlString > sAbsInputPaths;

	if ( !GetInputPaths( sAbsInputPaths, false, false ) )
	{
		Warning( "CTarget%s::Compile( %s ) - GetInputPaths failed\n", GetTypeString(), sName.String() );
		return false;
	}

	if ( sAbsInputPaths.Count() != 2 )
	{
		Warning( "CTarget%s::Compile( %s ) - GetPaths returned %d paths, expected 2\n", GetTypeString(), sName.String(), sAbsInputPaths.Count() );
		return false;
	}

	CFmtStrN< k64KB > sCmd;

	if ( CItemUpload::IgnoreEnvironmentVariables() )
	{
		// We can't rely on environment variables in VTEX. So tell it to just built it on the spot, and we'll move it afterwards.
		sCmd.sprintf( "\"%s\\vtex.exe\" -nop4 -nopause -dontusegamedir \"%s\"", sBinDir.String(), sAbsInputPaths.Element( 0 ).String() );
	}
	else
	{
		CUtlString sVProject;
		CItemUpload::GetVProjectDir( sVProject );
		sCmd.sprintf( "\"%s\\vtex.exe\" -nop4 -nopause -vproject \"%s\" \"%s\"", sBinDir.String(), sVProject.String(), sAbsInputPaths.Element( 0 ).String() );
	}

	if ( !CItemUpload::RunCommandLine( sCmd.Access(), sBinDir.String(), this ) )
	{
		Warning( "CTarget%s::Compile( %s ) - RunCommandLine Failed - \"%s\"\n", GetTypeString(), sName.String(), sCmd.Access() );
		return false;
	}

	if ( CItemUpload::IgnoreEnvironmentVariables() )
	{
		char sVTFName[MAX_PATH];
		V_strcpy_safe( sVTFName, sAbsInputPaths.Element( 0 ).String() );
		V_SetExtension( sVTFName, ".vtf", sizeof(sVTFName) );

		// We built the VTF in the directory with the content. Now move it to the out dir.
		if ( ::MoveFileEx( sVTFName, sAbsPath.String(), MOVEFILE_REPLACE_EXISTING ) == 0 )
		{
			Warning( "CTarget%s::Compile( %s ) - Failed to move \"%s\" to \"%s\"\n", GetTypeString(), sName.String(), sVTFName, sAbsPath.String() );
			return false;
		}
	}

	if ( !CheckFile( sAbsPath.String() ) )
	{
		Warning( "CTarget%s::Compile( %s ) - File Check Failed - \"%s\"\n", GetTypeString(), sName.String(), sAbsPath.String() );
		return false;
	}

	if ( CItemUpload::Manifest()->UseTerseMessages() )
	{
		Msg( " - Compilation successful.\n" );
	}
	else
	{
		Msg( "CTarget%s::Compile OK! - %s\n", GetTypeString(), sRelPath.String() );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVTF::GetInputs( CUtlVector< CTargetBase * > &inputs ) const
{
	Assert( m_pTargetTGA.IsValid() );

	inputs.AddToTail( m_pTargetTGA.GetObject() );

	return inputs.Count() > 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const ExtensionList *CTargetVTF::GetExtensionsAndCount( void ) const
{
	static ExtensionList vecExtensions;
	if ( !vecExtensions.Count() )
	{
		vecExtensions.AddToTail( ".vtf" );
	}

	return &vecExtensions;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CTargetVTF::GetPrefix() const
{
	return m_pTargetVMT->GetPrefix();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetVTF::GetName( CUtlString &sName ) const
{
	Assert( m_pTargetTGA.IsValid() );

	m_pTargetTGA->GetName( sName );
}


//=============================================================================
//
//=============================================================================
CTargetVMT::CTargetVMT( CAsset *pAsset, const CTargetBase *pTargetParent )
: CTargetBase( pAsset, pTargetParent )
, m_pVMTKV( NULL )
, m_nColorAlphaType( kNoColorAlpha )
, m_nNormalAlphaType( kNoNormalAlpha )
, m_nMaterialType( kInvalidMaterialType )
, m_bDuplicate( false )
, m_nTargetWidth( 0 )
, m_nTargetHeight( 0 )
{
	m_vecTargetVTFs.SetCount( CItemUpload::Manifest()->GetNumMaterialSkins() );
	FOR_EACH_VEC( m_vecTargetVTFs, i )
	{
		m_vecTargetVTFs[i].SetCount( CItemUpload::Manifest()->GetNumTextureTypes() );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CTargetVMT::~CTargetVMT()
{
	if ( m_pVMTKV )
	{
		m_pVMTKV->deleteThis();
	}

	CUtlString sMaterialId;
	GetMaterialId( sMaterialId );

	Asset()->RemoveMaterial( sMaterialId );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVMT::Compile()
{
	if ( GetDuplicate() )
		return true;

	if ( !CTargetBase::Compile() )
		return g_bCompilePreview ? true : false;

	CAsset *pAsset = Asset();
	if ( !pAsset )
		return false;

	CUtlString sName;
	if ( !GetOutputPath( sName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
		return false;

	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( "CTarget%s::Compile( %s ) - Not Valid: %s\n", GetTypeString(), sName.String(), sTmp.String() );
		return false;
	}

	int nVmtCount = GetOutputCount();

	CUtlVector< CUtlString > sAbsOutputPaths;
	CUtlVector< CUtlString > sRelOutputPaths;

	if ( !GetOutputPaths( sAbsOutputPaths, false, false, true ) || !GetOutputPaths( sRelOutputPaths, true, false, true ) )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPaths failed\n", GetTypeString(), sName.String() );
		return false;
	}

	if ( sAbsOutputPaths.Count() != nVmtCount || sAbsOutputPaths.Count() != sRelOutputPaths.Count() )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPaths returned %d paths, expected %d\n", GetTypeString(), sName.String(), sAbsOutputPaths.Count(), 1 );
		return false;
	}

	Assert( nVmtCount == sAbsOutputPaths.Count() );
	Assert( nVmtCount == sRelOutputPaths.Count() );

	char szBuf[ k64KB ];

	for ( int i = 0; i < nVmtCount; ++i )
	{
		CUtlVector< CUtlString > sChildRelOutputPaths;

		if ( !GetOutputPath( sName, i, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
		{
			Warning( "CTarget%s::Compile( %s ) - Can't GetOutputPath %d\n", GetTypeString(), sName.String(), i );
			return false;
		}

		Msg( "Compiling %s: %s\n", GetTypeString(), sRelOutputPaths.Element( i ).String() );

		AddOrEditP4File( sAbsOutputPaths.Element( i ).String() );

		// Load the template in.
		KeyValues *pVMTKV = GetVMTKV( i );
		if ( !pVMTKV )
		{
			// We've already printed a warning, so we're done
			return false;
		}

		// Set any remapped VMT vars.
		FOR_EACH_VEC( m_vecTargetVTFs[i], iVTF )
		{
			if ( m_vecTargetVTFs[i][iVTF].IsValid() )
			{
				const char *pszVMTVar = CItemUpload::Manifest()->GetVMTVarForTextureType( CItemUpload::Manifest()->GetTextureType(iVTF) );
				if ( pszVMTVar && pszVMTVar[0] )
				{
					sChildRelOutputPaths.RemoveAll();
					m_vecTargetVTFs[i][iVTF]->GetOutputPaths( sChildRelOutputPaths, true, false, false, false );

					if ( sChildRelOutputPaths.Count() > 0 )
					{
						V_strncpy( szBuf, sChildRelOutputPaths.Element( 0 ).String(), ARRAYSIZE( szBuf ) );
						V_FixSlashes( szBuf, '/' );

						if ( pVMTKV->FindKey( pszVMTVar ) )
						{
							pVMTKV->SetString( pszVMTVar, szBuf );
						}
					}
				}
			}
		}

		CUtlBuffer fileBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		pVMTKV->RecursiveSaveToFile( fileBuf, 0 );
		g_pFullFileSystem->WriteFile( sAbsOutputPaths.Element( i ).String(), "MOD", fileBuf );

		Msg( "CTarget%s::Compile OK! - %s\n", GetTypeString(), sRelOutputPaths.Element( i ).String() );
	}

	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CTargetVMT::MaterialTypeToString( int nMaterialType )
{
	if ( nMaterialType == kInvalidMaterialType )
		return "InvalidMaterialType";

	const char *pszMaterialName = CItemUpload::Manifest()->GetMaterialType( nMaterialType );
	if ( pszMaterialName )
		return pszMaterialName;

	return "Unknown";
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CTargetVMT::StringToMaterialType( const char *pszUserData )
{
	return CItemUpload::Manifest()->GetMaterialType( pszUserData );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVMT::IsOk( CUtlString &sMsg ) const
{
	if ( GetMaterialType() == kInvalidMaterialType )
	{
		sMsg += "\"";
		sMsg += m_sMaterialId;
		sMsg += "\": ";
		sMsg += "Invalid material type, must be one of Primary or Secondary";
		return false;
	}

	if ( GetDuplicate() )
		return true;

	if ( !g_bCompilePreview )
	{
		// Make sure we have all the required textures.
		int nSkinIndex = CItemUpload::Manifest()->GetDefaultMaterialSkin();
		FOR_EACH_VEC( m_vecTargetVTFs[ nSkinIndex ], i )
		{
			if ( !m_vecTargetVTFs[nSkinIndex][i].IsValid() && CItemUpload::Manifest()->IsTextureTypeRequired( i ) )
			{
				sMsg += "\"";
				sMsg += m_sMaterialId;
				sMsg += "\": ";
				sMsg += "Missing required texture type";
				Warning( "Missing texture of type '%s'\n", CItemUpload::Manifest()->GetTextureType( i ) );
				return false;
			}
		}
	}
	

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const ExtensionList *CTargetVMT::GetExtensionsAndCount( void ) const
{
	if ( GetDuplicate() )
		return NULL;

	m_vecExtensions.SetCount( 0 );
	FOR_EACH_VEC( m_vecTargetVTFs, i )
	{
		FOR_EACH_VEC( m_vecTargetVTFs[i], j )
		{
			if ( m_vecTargetVTFs[i][j].IsValid() )
			{
				CUtlString sExtension = CItemUpload::Manifest()->GetMaterialSkinFilenameAppend( i );
				sExtension += ".vmt";
				m_vecExtensions.AddToTail( sExtension );
				break;
			}
		}
	}

	return &m_vecExtensions;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVMT::GetInputs( CUtlVector< CTargetBase * > &inputs ) const
{
	CUtlString sTmp;

	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	FOR_EACH_VEC( m_vecTargetVTFs, i )
	{
		FOR_EACH_VEC( m_vecTargetVTFs[i], j )
		{
			if ( m_vecTargetVTFs[i][j].IsValid() )
			{
				inputs.AddToTail( m_vecTargetVTFs[i][j].GetObject() );
			}
		}
	}

	return inputs.Count() > 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetVMT::GetName( CUtlString &sName ) const
{
	sName.Clear();

	if ( GetDuplicate() )
	{
	}
	else
	{
		CTargetBase::GetName( sName );

		if ( m_nMaterialType == kInvalidMaterialType )
		{
			sName += "_INVALID";
		}
		else
		{
			if ( m_nMaterialType > 0 )
			{
				sName += "_";
				sName += m_nMaterialType;
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetVMT::SetMaterialId( const char *pszMaterialId )
{
	m_sMaterialId = pszMaterialId;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVMT::SetMaterialType( int nMaterialType )
{
	CAsset *pAsset = Asset();
	if ( !pAsset )
		return false;

	if ( nMaterialType < 0 || nMaterialType >= CItemUpload::Manifest()->GetNumMaterialTypes() )
		return false;

	m_bDuplicate = false;
	m_nMaterialType = kInvalidMaterialType;

	if ( nMaterialType == kInvalidMaterialType )
		return true;

	// Recursively increment other non-duplicate materials which match this material
	// and so on...
	for ( int i = 0; i < pAsset->GetTargetVMTCount(); ++i )
	{
		CTargetVMT *pTargetVMT = pAsset->GetTargetVMT( i );
		if ( !pTargetVMT )
			continue;

		if ( !pTargetVMT->GetDuplicate() && pTargetVMT->GetMaterialType() == nMaterialType )
		{
			pTargetVMT->SetMaterialType( ( ( nMaterialType + 1 ) % CItemUpload::Manifest()->GetNumMaterialTypes() ) );
		}
	}

	m_nMaterialType = nMaterialType;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CTargetVMT::GetMaterialType() const
{
	return m_nMaterialType;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetVMT::SetDuplicate( int nMaterialType )
{
	if ( nMaterialType < 0 || nMaterialType >= CItemUpload::Manifest()->GetNumMaterialTypes() )
		return;

	m_bDuplicate = true;
	m_nMaterialType = nMaterialType;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVMT::SetTargetVTF( const char *pszTextureType, const char *pszFilename, int nSkinIndex )
{
	// Make sure it's a valid texture type
	int nTextureType = CItemUpload::Manifest()->GetTextureType( pszTextureType );
	if ( nTextureType == -1 )
	{
		Warning( "Invalid texture type specified: '%s'\n", pszTextureType );
		return false;
	}
	if ( nSkinIndex == kInvalidMaterialSkin )
	{
		Warning( "Invalid skin type specified: '%d'\n", nSkinIndex );
		return false;
	}

	CUtlString sSuffix;
	sSuffix += CItemUpload::Manifest()->GetMaterialSkinFilenameAppend( nSkinIndex );
	sSuffix += pszTextureType;

	return SetTargetVTF( m_vecTargetVTFs[nSkinIndex][nTextureType], pszFilename, sSuffix.String() );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVMT::SetTargetVTF(
	CSmartPtr< CTargetVTF > &pTargetVTF,
	const char *pszFilename,
	const char *pszSuffix ) const
{
	if ( !pTargetVTF )
	{
		pTargetVTF = Asset()->NewTarget< CTargetVTF >( this );
	}

	if ( !pTargetVTF )
		return false;

	pTargetVTF->SetNameSuffix( pszSuffix );

	bool bResult = pTargetVTF->SetInputFile( pszFilename );

	return bResult;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetVMT::SetVMTKV( const KeyValues *pKV, int nSkinIndex /*= 0*/ )
{
	if ( !m_pVMTKV )
	{
		m_pVMTKV = new KeyValues( "data" );	
	}

	const char* pszKeyName = CFmtStr( kVMT, nSkinIndex );
	if ( KeyValues *pPreviousKey = m_pVMTKV->FindKey( pszKeyName ) )
	{
		m_pVMTKV->RemoveSubKey( pPreviousKey );
	}

	KeyValues *pNewKey = new KeyValues( pszKeyName );
	pNewKey->AddSubKey( pKV->MakeCopy() );
	m_pVMTKV->AddSubKey( pNewKey );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
KeyValues *CTargetVMT::GetVMTKV( int nSkinIndex )
{
	if ( !m_pVMTKV )
	{
		m_pVMTKV = new KeyValues( "data" );	
	}

	const char* pszKeyName = CFmtStr( kVMT, nSkinIndex );
	KeyValues* pKey = m_pVMTKV->FindKey( pszKeyName );
	if ( pKey )
	{
		return pKey->GetFirstSubKey();
	}

	KeyValues *pVMTKV = new KeyValues("VMTTemplate");

	if ( CItemUpload::Manifest()->HasClassVMTTemplates() )
	{
		const int nClassIndex = GetClassIndex( Asset()->GetClass() );
		const char *pszVMTTemplate = CItemUpload::Manifest()->GetClassVMTTemplate( nClassIndex );
		if ( !pszVMTTemplate )
		{
			Warning( "Could not find a VMT template for class '%s'\n", Asset()->GetClass() );
			pVMTKV->deleteThis();
			return NULL;
		}

		if ( !pVMTKV->LoadFromFile( g_pFullFileSystem, pszVMTTemplate, "MOD" ) )
		{
			Warning( "Failed to load specified VMT template '%s' for class '%s'.\n", pszVMTTemplate, Asset()->GetClass() );
			pVMTKV->deleteThis();
			return NULL;
		}
	}
	else
	{
		CreateLegacyTemplate( pVMTKV );
	}

	KeyValues *pNewKey = new KeyValues( pszKeyName );
	pNewKey->AddSubKey( pVMTKV );
	m_pVMTKV->AddSubKey( pNewKey );

	return pVMTKV;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetVMT::CreateLegacyTemplate( KeyValues *pVMTKV )
{
	pVMTKV->SetName( "VertexlitGeneric" );

	for ( int iVTF = 0; iVTF < CItemUpload::Manifest()->GetNumTextureTypes(); ++iVTF )
	{
		const char *pszVMTVar = CItemUpload::Manifest()->GetVMTVarForTextureType( CItemUpload::Manifest()->GetTextureType( iVTF ) );
		if ( pszVMTVar && *pszVMTVar )
		{
			pVMTKV->SetString( pszVMTVar, "" );
		}
	}

	// Wearables usually use this lightwarp: "models/player/pyro/pyro_lightwarp"
	// Weapons usually use this lightwarp: "models/lightwarps/weapon_lightwarp"
	// Weapons are the more custom case, so we'll default to a good wearable lightwarp
	pVMTKV->SetString( "$lightwarptexture", "models/player/pyro/pyro_lightwarp" );
	pVMTKV->SetString( "$phong", "1" );
	pVMTKV->SetString( "$phongexponent", "25" );
	pVMTKV->SetString( "$phongboost", "0.1" );
	pVMTKV->SetString( "$phongfresnelranges", "[.25 .5 1]" );

	pVMTKV->SetString( "$rimlight", "1" );			// To enable rim lighting (requires phong)
	pVMTKV->SetString( "$rimlightexponent", "4" );	// Exponent for phong component of rim 
	pVMTKV->SetString( "$rimlightboost", "2" );		// Boost for ambient cube component of rim lighting

	switch ( m_nColorAlphaType )
	{
	case kNoColorAlpha:
		break;
	case kTransparency:
		pVMTKV->SetString( "$translucent", "1" );
		break;
	case kPaintable:
		pVMTKV->SetString( "$blendtintbybasealpha", "1" );
		pVMTKV->SetString( "$blendtintcoloroverbase", "0" );   // between 0-1 determines how much blended by tinting vs. replacing the color

		pVMTKV->SetString( "$colortint_base", "{ 255 255 255 }" );	// put the RGB value of whats being colored when no paint is present, if $blendtintcoloroverbase is 0 then put [255 255 255] here.
		pVMTKV->SetString( "$color2", "{ 255 255 255 }" );
		pVMTKV->SetString( "$colortint_tmp", "[0 0 0]" );
		break;
	case kColorSpecPhong:
		pVMTKV->SetString( "$basemapalphaphongmask", "1" );
		break;
	}

	switch ( m_nNormalAlphaType )
	{
	case kNoNormalAlpha:
		break;
	case kNormalSpecPhong:
		pVMTKV->SetString( "$bumpmapalphaphongmask", "1" );
		break;
	}

	// Variables for the cloak effect
	pVMTKV->SetString( "$cloakPassEnabled", "1" );

	// Variables for the burning effect
	pVMTKV->SetString( "$detail", "effects/tiledfire/fireLayeredSlowTiled512" );
	pVMTKV->SetString( "$detailscale", "5" );
	pVMTKV->SetString( "$detailblendfactor", "0" );
	pVMTKV->SetString( "$detailblendmode", "6" );

	// Variables for the jarate effect
	pVMTKV->SetString( "$yellow", "0" );

	// The order of the proxies is important!
	KeyValues *pProxies = pVMTKV->FindKey( "Proxies", true );

	// Proxies for the cloak effect
	KeyValues *pProxiesWeaponInvis = pProxies->FindKey( "invis", true );
	pProxiesWeaponInvis->FindKey( "temp_key_wont_print_dont_worry", true );

	// Proxies for the burning effect
	KeyValues *pProxiesAnimatedTexture = pProxies->FindKey( "AnimatedTexture", true );
	pProxiesAnimatedTexture->SetString( "animatedtexturevar", "$detail" );
	pProxiesAnimatedTexture->SetString( "animatedtextureframenumvar", "$detailframe" );
	pProxiesAnimatedTexture->SetString( "animatedtextureframerate", "30" );

	KeyValues *pProxiesBurnLevel = pProxies->FindKey( "BurnLevel", true );
	pProxiesBurnLevel->SetString( "resultVar", "$detailblendfactor" );

	// Proxies for paintable items
	if ( m_nColorAlphaType == kPaintable )
	{
		KeyValues *pProxiesItemTintColor = pProxies->FindKey( "ItemTintColor", true );
		pProxiesItemTintColor->SetString( "resultVar", "$colortint_tmp" );

		KeyValues *pProxiesSelectFirstIfNonZero = pProxies->FindKey( "SelectFirstIfNonZero", true );
		pProxiesSelectFirstIfNonZero->SetString( "srcVar1", "$colortint_tmp" );
		pProxiesSelectFirstIfNonZero->SetString( "srcVar2", "$colortint_base" );
		pProxiesSelectFirstIfNonZero->SetString( "resultVar", "$color2" );

		// Proxies for the jarate effect
		KeyValues *pProxiesYellowLevel = pProxies->FindKey( "YellowLevel", true );
		pProxiesYellowLevel->SetString( "resultVar", "$yellow" );

		KeyValues *pProxiesMultiply = pProxies->FindKey( "Multiply", true );
		pProxiesMultiply->SetString( "srcVar1", "$color2" );
		pProxiesMultiply->SetString( "srcVar2", "$yellow" );
		pProxiesMultiply->SetString( "resultVar", "$color2" );
	}
	else
	{
		// Proxies for the jarate effect
		KeyValues *pProxiesYellowLevel = pProxies->FindKey( "YellowLevel", true );
		pProxiesYellowLevel->SetString( "resultVar", "$yellow" );

		KeyValues *pProxiesEquals = pProxies->FindKey( "Equals", true );
		pProxiesEquals->SetString( "srcVar1", "$yellow" );
		pProxiesEquals->SetString( "resultVar", "$color2" );
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetVMT::UpdateManifest( KeyValues *pKV )
{
	pKV->SetString( "materialId", m_sMaterialId.String() );
	pKV->SetString( "materialType", MaterialTypeToString( GetMaterialType() ) );
	pKV->SetInt( "duplicate", GetDuplicate() );
	pKV->SetInt( "target_width", m_nTargetWidth );
	pKV->SetInt( "target_height", m_nTargetHeight );

	if ( !GetDuplicate() )
	{
		KeyValues *pColorSubKey = new KeyValues( "color" );
		pKV->AddSubKey( pColorSubKey );

		pColorSubKey->SetString( "alphaType", ColorAlphaTypeToString( GetColorAlphaType() ) );

		FOR_EACH_VEC( m_vecTargetVTFs, i )
		{
			KeyValues *pSkinSubKey;
			const char *pszSkinType = CItemUpload::Manifest()->GetMaterialSkin(i);
			if ( pszSkinType )
				pSkinSubKey = new KeyValues( pszSkinType );
			else
				pSkinSubKey = pColorSubKey;

			FOR_EACH_VEC( m_vecTargetVTFs[i], j )
			{
				if ( m_vecTargetVTFs[i][j].IsValid() )
				{
					const char *pszTextureType = CItemUpload::Manifest()->GetTextureType(j);
					KeyValues *pSubKey = new KeyValues( pszTextureType );
					pSkinSubKey->AddSubKey( pSubKey );
					m_vecTargetVTFs[i][j]->UpdateManifest( pSubKey );
				}
			}
		}
	}
}


//=============================================================================
//
//=============================================================================
CTargetIcon::CTargetIcon( CAsset *pAsset, int nIconType )
: CTargetVMT( pAsset, pAsset )
, m_nIconType( nIconType )
{
	int nWidth, nHeight;
	CItemUpload::Manifest()->GetIconDimensions( m_nIconType, nWidth, nHeight );
	SetTargetResolution( nWidth, nHeight );

	const char *pszSuffix = CItemUpload::Manifest()->GetIconFilenameAppend( m_nIconType );
	SetNameSuffix( pszSuffix );

	KeyValues *pVMTKV = CItemUpload::Manifest()->GetIconVMTTemplate( m_nIconType );
	if ( pVMTKV )
	{
		SetVMTKV( pVMTKV );
	}

	SetMaterialType( CItemUpload::Manifest()->GetDefaultMaterialType() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetIcon::SetTargetVTF( const char *pszFilename )
{
	return CTargetVMT::SetTargetVTF( m_vecTargetVTFs[0][0], pszFilename, "" );
}


//=============================================================================
//
//=============================================================================
CTargetDMX::CTargetDMX( CAsset *pAsset, const CTargetQC *pTargetQC )
	: CTargetBase( pAsset, pTargetQC )
	, m_pDmRoot( NULL )
	, m_nLod( -1 )
	, m_flAnimationLoopStartTime( -1.f )
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CTargetDMX::~CTargetDMX()
{
	Clear();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::IsOk( CUtlString &sMsg ) const
{
	if ( m_nLod < 0 )
	{
		sMsg = "Invalid LOD (";
		sMsg += m_nLod;
		sMsg += ")";
		return false;
	}

	if ( m_sInputFile.IsEmpty() || m_sExtension.IsEmpty() )
	{
		sMsg = "No input file specified";
		return false;
	}

	if ( !CItemUpload::FileExists( m_sInputFile.String() ) )
	{
		sMsg = "Input file: \"";
		sMsg += m_sInputFile;
		sMsg += "\" doesn't exist";
		return false;
	}

	if ( !( IsInputSmd() || IsInputObj() || IsInputDmx() || IsInputFbx() ) )
	{
		sMsg = "Input file: \"";
		sMsg += m_sInputFile;
		sMsg += "\" is not a DMX, OBJ or SMD file";
		return false;
	}

	if ( !m_pDmRoot )
	{
		sMsg = "Input file: \"";
		sMsg += m_sInputFile;
		sMsg += "\" couldn't be read";
		return false;
	}

	const int nPolyCount = GetPolyCount();
	if ( nPolyCount <= 0 && m_strQCITemplate.IsEmpty() )
	{
		sMsg = "Invalid polygon count (";
		sMsg += nPolyCount;
		sMsg += ")";
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::Compile()
{
	if ( !ReloadFile() )
		return false;

	if ( !CTargetBase::Compile() )
		return false;

	CUtlString sName;
	if ( !GetOutputPath( sName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
		return false;

	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( "CTarget%s::Compile( %s ) - Not Valid: %s\n", GetTypeString(), sName.String(), sTmp.String() );
		return false;
	}

	CUtlString sAbsPath;
	GetOutputPath( sAbsPath, 0 );

	CUtlString sRelPath;
	GetOutputPath( sRelPath, 0, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE );

	if ( sAbsPath.IsEmpty() || sRelPath.IsEmpty() )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPath failed\n", GetTypeString(), sName.String() );
		return false;
	}

	Msg( "Compiling %s: %s\n", GetTypeString(), sRelPath.String() );

	AddOrEditP4File( sAbsPath.String() );

	CUndoScopeGuard undo( "replaceMaterial" );

	ReplaceMaterials();

	SkinToBipHead();

#ifdef _DEBUG
	const bool bRet = g_pDataModel->SaveToFile( sAbsPath.String(), NULL, "keyvalues2", "model", m_pDmRoot );
#else // ifdef _DEBUG
	const bool bRet = g_pDataModel->SaveToFile( sAbsPath.String(), NULL, "binary", "model", m_pDmRoot );
#endif // _DEBUG

	undo.Abort();

	if ( !bRet )
	{
		Msg( "CTarget%s::Compile( %s ) - SaveToFile failed - \"%s\"\n", GetTypeString(), sName.String(), sAbsPath.String() );
		return false;
	}

	if ( !CheckFile( sAbsPath.String() ) )
	{
		Warning( "CTarget%s::Compile( %s ) - File Check Failed - \"%s\"\n", GetTypeString(), sName.String(), sAbsPath.String() );
		return false;
	}

	if ( !OutputQCIFile() )
	{
		return false;
	}

	if ( CItemUpload::Manifest()->UseTerseMessages() )
	{
		Msg( " - Compilation successful.\n" );
	}
	else
	{
		Msg( "CTarget%s::Compile OK! - %s\n", GetTypeString(), sRelPath.String() );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::GetInputPaths(
	CUtlVector< CUtlString > &sInputPaths,
	bool bRelative /* = true */,
	bool bRecurse /* = true */,
	bool bExtension /* = true */ )
{
	sInputPaths.AddToTail( m_sInputFile );

	return CTargetBase::GetInputPaths( sInputPaths, bRelative, bRecurse, bExtension );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const ExtensionList *CTargetDMX::GetExtensionsAndCount( void ) const
{
	static ExtensionList vecExtensions;
	vecExtensions.RemoveAll();
	vecExtensions.AddToTail( ".dmx" );
	if ( !m_strQCITemplate.IsEmpty() )
	{
		vecExtensions.AddToTail( ".qci" );
	}

	return &vecExtensions;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetDMX::GetName( CUtlString &sName ) const
{
	CTargetBase::GetName( sName );

	if ( GetLod() > 0 )
	{
		sName += "_lod";
		sName += GetLod();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CTargetDMX::GetPolyCount() const
{
	if ( !m_pDmRoot )
		return 0;

	int nPolyCount = 0;

	const DmFileId_t hRootFileId = m_pDmRoot->GetFileId();

	for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = g_pDataModel->NextAllocatedElement( hElement ) )
	{
		if ( hElement == DMELEMENT_HANDLE_INVALID )
			continue;

		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( g_pDataModel->GetElement( hElement ) );
		if ( !pDmeMesh || pDmeMesh->GetFileId() != hRootFileId )
			continue;

		for ( int i = 0; i < pDmeMesh->FaceSetCount(); ++i )
		{
			CDmeFaceSet *pDmeFaceSet = pDmeMesh->GetFaceSet( i );
			if ( !pDmeFaceSet )
				continue;

			nPolyCount += pDmeFaceSet->GetFaceCount();
		}
	}

	return nPolyCount;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CTargetDMX::GetTriangleCount() const
{
	if ( !m_pDmRoot )
		return 0;

	int nTriangleCount = 0;

	const DmFileId_t hRootFileId = m_pDmRoot->GetFileId();

	for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = g_pDataModel->NextAllocatedElement( hElement ) )
	{
		if ( hElement == DMELEMENT_HANDLE_INVALID )
			continue;

		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( g_pDataModel->GetElement( hElement ) );
		if ( !pDmeMesh || pDmeMesh->GetFileId() != hRootFileId )
			continue;

		for ( int i = 0; i < pDmeMesh->FaceSetCount(); ++i )
		{
			CDmeFaceSet *pDmeFaceSet = pDmeMesh->GetFaceSet( i );
			if ( !pDmeFaceSet )
				continue;

			nTriangleCount += pDmeFaceSet->GetTriangulatedIndexCount() / 3;
		}
	}

	return nTriangleCount;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CTargetDMX::GetVertexCount() const
{
	if ( !m_pDmRoot )
		return 0;

	int nVertexCount = 0;

	const DmFileId_t hRootFileId = m_pDmRoot->GetFileId();

	for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = g_pDataModel->NextAllocatedElement( hElement ) )
	{
		if ( hElement == DMELEMENT_HANDLE_INVALID )
			continue;

		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( g_pDataModel->GetElement( hElement ) );
		if ( !pDmeMesh || pDmeMesh->GetFileId() != hRootFileId )
			continue;

		CDmeVertexData *pVertexData = pDmeMesh->GetBindBaseState();
		if ( !pVertexData )
			continue;

		nVertexCount += pVertexData->VertexCount();
	}

	return nVertexCount;
}


bool CTargetDMX::GetAnimationFrameInfo( float& flFrameRate, int& nFrameCount ) const
{
	if ( !m_pDmRoot )
		return false;

	CDmeAnimationList *pDmeAnimationList = m_pDmRoot->GetValueElement< CDmeAnimationList >( "animationList" );
	CDmeChannelsClip *pDmeChannelsClip = NULL;
 
	if ( pDmeAnimationList )
	{
		if ( pDmeAnimationList->GetAnimationCount() < 0 )
		{
			Warning( "No DmeChannelsClips found in root.animationList of DMX file: \"%s\"\n", m_sInputFile.String() );
			return false;
		}
 
		pDmeChannelsClip = pDmeAnimationList->GetAnimation( 0 );
		if ( !pDmeChannelsClip )
		{
			Warning( "No DmeChannelsClip found in root.animationList[0] of DMX file: \"%s\"\n", m_sInputFile.String() );
			return false;
		}
 
		flFrameRate = pDmeChannelsClip->GetValue< int >( "frameRate", 30 );
		nFrameCount = (int)( ( pDmeChannelsClip->GetDuration().GetSeconds() * flFrameRate ) + 0.5f ) + 1;

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Returns true if all meshes in the DMX data have skinning information
//         false if any mesh doesn't have skinning data
//-----------------------------------------------------------------------------
bool CTargetDMX::AreAllMeshesSkinned() const
{
	if ( !m_pDmRoot )
		return false;			// No DMX data

	const DmFileId_t hRootFileId = m_pDmRoot->GetFileId();

	int nMeshCount = 0;

	for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = g_pDataModel->NextAllocatedElement( hElement ) )
	{
		if ( hElement == DMELEMENT_HANDLE_INVALID )
			continue;

		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( g_pDataModel->GetElement( hElement ) );
		if ( !pDmeMesh || pDmeMesh->GetFileId() != hRootFileId )
			continue;

		CDmeVertexData *pDmeVertexData = pDmeMesh->GetBindBaseState();
		if ( !pDmeVertexData )
			continue;

		// If even one mesh doesn't have skinning data, abort
		if ( !pDmeVertexData->HasSkinningData() )
			return false;
	}

	// Make sure we saw at least one mesh
	return ( nMeshCount > 0 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const CUtlString &CTargetDMX::GetInputFile() const
{
	return m_sInputFile;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetDMX::Clear()
{
	if ( m_pDmRoot )
	{
		CDisableUndoScopeGuard noUndo;

		m_targetVmtList.RemoveAll();

		g_pDataModel->UnloadFile( m_pDmRoot->GetFileId() );
		m_pDmRoot = NULL;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::SetInputFile( const char *pszFilename )
{
	// Because of the way datamodel works, bad things will happen if the same file is loaded twice
	Clear();

	m_sInputFile = pszFilename;

	char szBuf[ k64KB ];

	V_ExtractFileExtension( pszFilename, szBuf, ARRAYSIZE( szBuf ) );
	m_sExtension = szBuf;

	DmFileId_t hRootFileId = g_pDataModel->GetFileId( pszFilename );
	DmElementHandle_t hRootElement = g_pDataModel->GetFileRoot( hRootFileId );
	CDmElement *pDmRoot = g_pDataModel->GetElement( hRootElement );

	if ( !pDmRoot )
	{
		pDmRoot = LoadDmx();
	}

	if ( !pDmRoot )
	{
		pDmRoot = LoadFbx();
	}

	if ( !pDmRoot )
	{
		pDmRoot = LoadSmd();
	}

	if ( !pDmRoot )
	{
		pDmRoot = LoadObj();
	}

	if ( !pDmRoot )
	{
		Warning( "ERROR: Don't Know What Type Of Input File Is\n" );
		return false;
	}

	m_pDmRoot = pDmRoot;

	// TODO: Change to walking from root?

	hRootFileId = m_pDmRoot->GetFileId();

	// we must store the root handle so we don't reload the same file multiple time
	g_pDataModel->SetFileRoot( hRootFileId, m_pDmRoot->GetHandle() );

	int nMaterialType = 0;
	for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = g_pDataModel->NextAllocatedElement( hElement ) )
	{
		if ( hElement == DMELEMENT_HANDLE_INVALID )
			continue;

		CDmeMaterial *pDmeMaterial = CastElement< CDmeMaterial >( g_pDataModel->GetElement( hElement ) );
		if ( !pDmeMaterial || pDmeMaterial->GetFileId() != hRootFileId )
			continue;

		CSmartPtr< CTargetVMT > pTargetVMT = Asset()->FindOrAddMaterial( pDmeMaterial->GetMaterialName(), nMaterialType );
		if ( pTargetVMT.IsValid() )
		{
			m_targetVmtList.AddToTail( pTargetVMT );
			++nMaterialType;
		}
	}

	CUtlString sTmp;
	if ( IsOk( sTmp ) )
		return true;

	Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::ReloadFile()
{
	// Keep references to all target VMT's to prevent them from being destroyed by a reload of the same file
	CUtlVector< CSmartPtr< CTargetVMT > > tmpVmtCopy;
	tmpVmtCopy = m_targetVmtList;

	// 'reset' the input file from the current value to force a reload on compile, make a temp copy as it's going to be overwritten
	if ( !SetInputFile( CUtlString( GetInputFile() ).String() ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::IsInputObj() const
{
	// TODO: Perhaps look at magic in the start of the file

	return !V_stricmp( "obj", m_sExtension.String() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::IsInputSmd() const
{
	// TODO: Perhaps look at magic in the start of the file

	return !V_stricmp( "smd", m_sExtension.String() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::IsInputDmx() const
{
	// TODO: Perhaps look at magic in the start of the file

	return !V_stricmp( "dmx", m_sExtension.String() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetDMX::IsInputFbx() const
{
	// TODO: Perhaps look at magic in the start of the file

	return !V_stricmp( "fbx", m_sExtension.String() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmElement *CTargetDMX::LoadObj()
{
	if ( !IsInputObj() )
		return NULL;

	CDisableUndoScopeGuard noUndo;

	CDmObjSerializer dmObjSerializer;

	return dmObjSerializer.ReadOBJ( m_sInputFile.String() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmElement *CTargetDMX::LoadSmd()
{
	if ( !IsInputSmd() )
		return NULL;

	CDisableUndoScopeGuard noUndo;

	CDmSmdSerializer dmSmdSerializer;

	dmSmdSerializer.SetIsAnimation( !m_strQCITemplate.IsEmpty() );

	return dmSmdSerializer.ReadSMD( m_sInputFile.String() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmElement *CTargetDMX::LoadDmx()
{
	if ( !IsInputDmx() )
		return NULL;

	CDisableUndoScopeGuard noUndo;

	CDmElement *pDmRoot = NULL;

	g_pDataModel->RestoreFromFile( m_sInputFile.String(), NULL, NULL, &pDmRoot );

	return pDmRoot;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmElement *CTargetDMX::LoadFbx()
{
	if ( !IsInputFbx() )
		return NULL;

	CDisableUndoScopeGuard noUndo;

	CDmFbxSerializer dmFbxSerializer;

	return dmFbxSerializer.ReadFBX( m_sInputFile.String() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetDMX::ReplaceMaterials() const
{
	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return;
	}

	DmFileId_t hRootFileId = m_pDmRoot->GetFileId();
	if ( hRootFileId == DMFILEID_INVALID )
		return;

	CAsset *pAsset = Asset();
	if ( !pAsset )
		return;

	CUtlMap< CUtlString, CUtlString > materialMap( UtlStringLessThan );

	CUtlString sRelativeDir;
	pAsset->GetRelativeDir( sRelativeDir, NULL, this );

	char szBuf[ k64KB ];

	for ( int i = 0; i < pAsset->GetTargetVMTCount(); ++i )
	{
		CTargetVMT *pTargetVMT = pAsset->GetTargetVMT( i );
		if ( !pTargetVMT )
			continue;

		CUtlString sMaterialId;
		pTargetVMT->GetMaterialId( sMaterialId );

		if ( sMaterialId.IsEmpty() || materialMap.IsValidIndex( materialMap.Find( sMaterialId ) ) )
			continue;

		CTargetVMT *pSrcTargetVMT = pTargetVMT;

		if ( pTargetVMT->GetDuplicate() )
		{
			const int nMaterialType = pTargetVMT->GetMaterialType();
			if ( nMaterialType < 0 || nMaterialType >= CItemUpload::Manifest()->GetNumMaterialTypes() )
			{
				Warning( "CTarget%s::ReplaceMaterials - Duplicate VMT (%s) with bad material type (%d)\n", GetTypeString(), sMaterialId.String(), nMaterialType );
				continue;
			}

			for ( int j = 0; j < pAsset->GetTargetVMTCount(); ++j )
			{
				CTargetVMT *pTmpTargetVMT = pAsset->GetTargetVMT( j );
				if ( !pTmpTargetVMT || pTmpTargetVMT->GetDuplicate() )
					continue;

				if ( pTmpTargetVMT->GetMaterialType() == nMaterialType )
				{
					pSrcTargetVMT = pTmpTargetVMT;
					break;
				}
			}

			if ( pTargetVMT == pSrcTargetVMT )
			{
				Warning( "CTarget%s::ReplaceMaterials - Duplicate VMT (%s) Cannot Find Source Material Type (%d)\n", GetTypeString(), sMaterialId.String(), nMaterialType );
				continue;
			}
		}

		sMaterialId.FixSlashes( '/' );

		// The full extension can be _red.vmt or _blue.vmt so we need to keep the _red but not the .vmt
		CUtlString sMaterialPath;
		pSrcTargetVMT->GetOutputPath( sMaterialPath, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION | PATH_FLAG_PATH | PATH_FLAG_MODELS );

		V_StripExtension( sMaterialPath, szBuf, ARRAYSIZE( szBuf ) );
		sMaterialPath.FixSlashes( '/' );

		sMaterialPath = szBuf;

		materialMap.InsertOrReplace( sMaterialId, sMaterialPath.String() );
	}


	for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = g_pDataModel->NextAllocatedElement( hElement ) )
	{
		if ( hElement == DMELEMENT_HANDLE_INVALID )
			continue;

		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( g_pDataModel->GetElement( hElement ) );
		if ( !pDmeMesh || pDmeMesh->GetFileId() != hRootFileId )
			continue;

		FOR_EACH_MAP_FAST( materialMap, mmi )
		{
			pDmeMesh->ReplaceMaterial( materialMap.Key( mmi ), materialMap.Element( mmi ) );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetDMX::SkinToBipHead()
{
	CAssetTF *pAssetTF = dynamic_cast< CAssetTF * >( Asset() );
	if ( !pAssetTF )
		return;

	if ( !pAssetTF->SkinToBipHead() )
		return;

	CDisableUndoScopeGuard undoGuard;

	CDmeModel *pSrcModel = m_pDmRoot->GetValueElement< CDmeModel >( "model" );
	if ( !pSrcModel )
		return;

	const DmFileId_t nFileId = m_pDmRoot->GetFileId();

	CDmElement *pDstRoot = CreateElement< CDmElement >( m_pDmRoot->GetName(), nFileId );

	CDmeModel *pDstModel = CreateElement< CDmeModel >( pSrcModel->GetName(), nFileId );
	pDstModel->GetTransform()->SetName( pDstModel->GetName() );

	pDstRoot->SetValue( "model", pDstModel );
	pDstRoot->SetValue( "skeleton", pDstModel );

	CDmeJoint *pBipHead = CreateElement< CDmeJoint >( "bip_head", nFileId );
	pBipHead->GetTransform()->SetName( "bip_head" );

	pDstModel->AddChild( pBipHead );
	pDstModel->AddJoint( pBipHead );

	const Vector &vBipHead = pAssetTF->GetBipHead();
	const RadianEuler &eBipHead = pAssetTF->GetBipHeadRotation();
	const Quaternion qBipHead = eBipHead;

	pBipHead->GetTransform()->SetOrientation( qBipHead );

	CUtlStack< CDmeDag * > depthFirstStack;
	depthFirstStack.Push( pSrcModel );

	while ( depthFirstStack.Count() )
	{
		CDmeDag *pDmeDag = NULL;

		depthFirstStack.Pop( pDmeDag );

		if ( !pDmeDag )
			continue;

		SkinToBipHead_R( pDstModel, pDmeDag, vBipHead );

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			depthFirstStack.Push( pDmeDag->GetChild( i ) );
		}
	}

	pDstModel->CaptureJointsToBaseState( "bind" );

	DestroyElement( m_pDmRoot, TD_ALL );
	m_pDmRoot = pDstRoot;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetDMX::SkinToBipHead_R( CDmeModel *pDstModel, CDmeDag *pSrcDag, const Vector &vBipHead )
{
	if ( !pDstModel || !pSrcDag )
		return;

	CDmeMesh *pSrcMesh = CastElement< CDmeMesh >( pSrcDag->GetShape() );
	if ( !pSrcMesh )
		return;

	CDmeVertexData *pSrcBind = pSrcMesh->GetBindBaseState();
	if ( !pSrcBind )
		return;

	CDmeDag *pDstDag = CreateElement< CDmeDag >( pSrcDag->GetName(), pDstModel->GetFileId() );
	if ( !pDstDag )
		return;

	pDstDag->GetTransform()->SetName( pDstDag->GetName() );

	CDmeMesh *pDstMesh = pSrcMesh->Copy();
	if ( !pDstMesh )
		return;

	// Current state is left empty, not sure why
	pDstMesh->SetBindBaseState( pDstMesh->GetCurrentBaseState() );

	pDstMesh->Resolve();

	pDstDag->SetShape( pDstMesh );
	pDstModel->AddChild( pDstDag );

	matrix3x4_t mIdentity;	// TODO: Is there a static identity matrix like quat_identity?
	SetIdentityMatrix( mIdentity );
	matrix3x4_t mSrcAbs;
	//	pSrcDag->GetAbsTransform( mSrcAbs );

	{
		matrix3x4_t parentToWorld;
		pSrcDag->GetParentWorldMatrix( parentToWorld );

		matrix3x4_t localMatrix;
		pSrcDag->GetLocalMatrix( localMatrix );

		ConcatTransforms( parentToWorld, localMatrix, mSrcAbs );
	}

	matrix3x4_t mNormal;
	MatrixInverseTranspose( mSrcAbs, mNormal );

	bool bConvertToWorldSpace = !MatricesAreEqual( mIdentity, mSrcAbs );

	for ( int i = 0; i < pDstMesh->BaseStateCount(); ++i )
	{
		CDmeVertexData *pDstVertexData = pDstMesh->GetBaseState( i );
		if ( !pDstVertexData )
			continue;
		pDstVertexData->Resolve();

		// Remove all skinning information
		// This seems a little odd, creating data to remove it but if it already exists this is
		// the only way of setting the joint count to 0 without cheating (i.e. referring to the field by name)
		FieldIndex_t nJointWeightField;
		FieldIndex_t nJointIndexField;
		pDstVertexData->CreateJointWeightsAndIndices( 0, &nJointWeightField, &nJointIndexField );

		pDstVertexData->RemoveAttribute( pDstVertexData->FieldName( nJointWeightField ) );
		pDstVertexData->RemoveAttribute( pDstVertexData->FieldName( nJointIndexField ) );

		// Convert data to world space if required
		{
			Vector vTmp;
			static const int nZero = 0;
			static const float flOne = 1;

			FieldIndex_t nPositionField = pDstVertexData->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
			if ( nPositionField >= 0 )
			{
				CDmAttribute *pPosAttr = pDstVertexData->GetVertexData( nPositionField );
				if ( pPosAttr )
				{
					CDmrArray< Vector > posData = pPosAttr;

					// Set everything up to be skinned to joint 0
					pDstVertexData->CreateJointWeightsAndIndices( 1, &nJointWeightField, &nJointIndexField );
					pDstVertexData->AddVertexData( nJointIndexField, posData.Count() );
					pDstVertexData->AddVertexData( nJointWeightField, posData.Count() );

					if ( bConvertToWorldSpace )
					{
						for ( int iPos = 0; iPos < posData.Count(); ++iPos )
						{
							pDstVertexData->SetVertexData( nJointIndexField, iPos, 1, AT_INT, &nZero );
							pDstVertexData->SetVertexData( nJointWeightField, iPos, 1, AT_FLOAT, &flOne );
						}
					}
					else
					{
						for ( int iPos = 0; iPos < posData.Count(); ++iPos )
						{
							VectorTransform( posData[iPos], mSrcAbs, vTmp );
							posData.Set( iPos, vTmp );
							pDstVertexData->SetVertexData( nJointIndexField, i, 1, AT_INT, &nZero );
							pDstVertexData->SetVertexData( nJointWeightField, i, 1, AT_FLOAT, &flOne );
						}
					}
				}
			}

			FieldIndex_t nNormalField = pDstVertexData->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
			if ( nNormalField >= 0 )
			{
				CDmAttribute *pNormalAttr = pDstVertexData->GetVertexData( nNormalField );
				if ( pNormalAttr )
				{
					CDmrArray< Vector > normalData = pNormalAttr;
					for ( int iData = 0; iData < normalData.Count(); ++iData )
					{
						VectorRotate( normalData[iData], mNormal, vTmp );
						vTmp.NormalizeInPlace();
						normalData.Set( iData, vTmp );
					}
				}
			}
		}

		pDstVertexData->Resolve();
	}

	Vector vMin;
	Vector vMax;
	pDstMesh->GetBoundingBox( vMin, vMax );

	const Vector vCenter = ( vMax - vMin ) / 2.0 + vMin;
	const float flSqDistOrigin = vCenter.DistToSqr( Vector( 0, 0, 0 ) );
	const float flSqDistBipHead = vCenter.DistToSqr( vBipHead );

	// See where the model was modelled... around the bip_head bone or at the origin?
	// There are probably other problems which aren't accounted for this is a really naive
	// algorithm

	if ( flSqDistBipHead < flSqDistOrigin  )
	{
		// Model centered around bip head, subtract vBipHead from position data

		Vector vTmp;

		for ( int i = 0; i < pDstMesh->BaseStateCount(); ++i )
		{
			CDmeVertexData *pDstVertexData = pDstMesh->GetBaseState( i );
			if ( !pDstVertexData )
				continue;

			pDstVertexData->Resolve();

			FieldIndex_t nPositionField = pDstVertexData->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
			if ( nPositionField >= 0 )
			{
				CDmAttribute *pPosAttr = pDstVertexData->GetVertexData( nPositionField );
				if ( pPosAttr )
				{
					CDmrArray< Vector > posData = pPosAttr;

					for ( int iPos = 0; iPos < posData.Count(); ++iPos )
					{
						VectorSubtract( posData[iPos], vBipHead, vTmp );
						posData.Set( iPos, vTmp );
					}
				}
			}

			pDstVertexData->Resolve();
		}
	}
}


//=============================================================================
//
//=============================================================================
bool CTargetDMX::OutputQCIFile()
{
	// Should we output QCI?
	if ( !m_strQCITemplate.IsEmpty() )
	{
		CUtlString sDMXName;
		if ( !GetOutputPath( sDMXName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
			return false;
			
		CUtlString sName;
		if ( !GetOutputPath( sName, 1, PATH_FLAG_FILE ) )
			return false;

		CUtlString sAbsPath;
		if ( !GetOutputPath( sAbsPath, 1 ) )
			return false;

		float flFrameRate;
		int nFrameCount;
		if ( !GetAnimationFrameInfo( flFrameRate, nFrameCount ) )
		{
			Warning( "Failed to get animation's frame info for %s\n", sAbsPath.String() );
			return false;
		}

		const int nBlendFrameOffset = 5;
		const int nLastFrameIndex = nFrameCount - 1;

		CUtlString strLayerName = "layer_";
		strLayerName += sName;

		CUtlString strBuf = m_strQCITemplate.Replace( "<ITEMTEST_SEQUENCE_LAYER_NAME>", strLayerName.String() );
		strBuf = strBuf.Replace( "<ITEMTEST_SEQUENCE_NAME>", sName.String() );
		strBuf = strBuf.Replace( "<ITEMTEST_DMX_FILE>", sName.String() );
		strBuf = strBuf.Replace( "<FPS>", CFmtStr( "fps %d", (int)flFrameRate ) );
		strBuf = strBuf.Replace( "<BLEND_LAYER_RANGE>", CFmtStr( "0 %d %d <LAST_FRAME_INDEX>", nBlendFrameOffset, nLastFrameIndex - nBlendFrameOffset ) );
		strBuf = strBuf.Replace( "<FRAME_COUNT>", CFmtStr( "%d", nFrameCount ) );
		strBuf = strBuf.Replace( "<LAST_FRAME_INDEX>", CFmtStr( "%d", nLastFrameIndex ) );
		

		CUtlBuffer bufQCFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
		bufQCFile.PutString( strBuf.String() );

		AddOrEditP4File( sAbsPath.String() );

		g_pFullFileSystem->WriteFile( sAbsPath.String(), "MOD", bufQCFile );
	}

	return true;
}


void LeftRightToValueBalance( float flLeft, float flRight, float &flOutValue, float &flOutBalance )
{
	if ( flLeft > flRight )
	{
		flOutValue = flLeft;
		flOutBalance = 0.5f * flRight / flLeft;
	}
	else
	{
		flOutValue = flRight;
		flOutBalance = 1.0f - 0.5f * flRight / flLeft;
	}
}

void BufPrintf( CUtlBuffer& buf, int nLevel, const char *fmt, ... )
{
	va_list argptr;
	va_start( argptr, fmt );

	while ( nLevel-- > 0 )
	{
		buf.Printf( "  " );
	}

	buf.VaPrintf( fmt, argptr );
	va_end( argptr );
}

void BufBeginBlock( CUtlBuffer& buf, int &nLevel )
{
	BufPrintf( buf, nLevel, "{\n" );
	++nLevel;
}

void BufEndBlock( CUtlBuffer& buf, int &nLevel )
{
	--nLevel;
	BufPrintf( buf, nLevel, "}\n" );
}

template< class T >
CDmeTypedLogLayer< T > *GetTopmostLogLayer( CDmeChannel *pChannel )
{
	if ( !pChannel )
		return NULL;

	CDmeTypedLog< T > *pLog = CastElement< CDmeTypedLog< T > >( pChannel->GetLog() );
	if ( !pLog )
		return NULL;

	return pLog->GetLayer( pLog->GetTopmostLayer() );
}

void CTargetDMX::OutputSounds( CUtlBuffer &buf, int nIndentLevel, CDmElement *pExportedSounds )
{
	if ( !pExportedSounds )
		return;

	CDmrElementArray< CDmElement > soundsArray( pExportedSounds, "sounds" );
	if ( !soundsArray.IsValid() )
		return;

	int nSounds = soundsArray.Count();
	if ( nSounds == 0 )
		return;

	CUtlBuffer soundScriptBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !m_strSoundScriptFile.IsEmpty() )
	{
		if ( !g_pFullFileSystem->ReadFile( m_strSoundScriptFile.String(), "MOD", soundScriptBuffer ) )
		{
			Warning( "Failed to load sound script file\n" );
			return;
		}
	}
	else
	{
		Warning( "Sound script file not specified\n" );
		return;
	}

	BufPrintf( buf, nIndentLevel, "channel \"sounds\"\n" );
	BufBeginBlock( buf, nIndentLevel );
	{
		for ( int i = 0; i < nSounds; ++i )
		{
			CDmElement *pSound = soundsArray[ i ];
			if ( !pSound )
				continue;

			float flStartTime = pSound->GetValue< float >( "startTime" );
			CUtlString strWaveName = pSound->GetValueString( "fileName" );
			V_FixSlashes( strWaveName.GetForModify(), '/' );
			if ( !g_pFullFileSystem->FileExists( CFmtStr( "sound/%s", strWaveName.String() ), "GAME" ) )
			{
				CUtlString strSoundName = strWaveName.StripExtension();
				if ( !g_pFullFileSystem->FileExists( CFmtStr( "sound/%s.mp3", strSoundName.String() ), "GAME" ) )
				{
					Warning( "OutputSounds to VCD missing sound file: %s.wav/mp3\n", strSoundName.String() );
					continue;
				}
			}

			bool bIsVO = V_strstr( strWaveName.String(), "vo/" ) != NULL;
			const char *pszSoundScriptName = CFmtStr( "<CLASS_NAME>_<ITEMTEST_SEQUENCE_NAME>_%s", pSound->GetName() );

			// write sound script entry to sound script file
			int nSoundIndentLevel = 0;
			BufPrintf( soundScriptBuffer, nSoundIndentLevel, "\"%s\"\n", pszSoundScriptName );
			BufBeginBlock( soundScriptBuffer, nSoundIndentLevel );
			{
				BufPrintf( soundScriptBuffer, nSoundIndentLevel, "\"channel\"			\"%s\"\n", bIsVO ? "CHAN_VOICE" : "CHAN_STATIC" );
				BufPrintf( soundScriptBuffer, nSoundIndentLevel, "\"volume\"			\"VOL_NORM\"\n" );
				BufPrintf( soundScriptBuffer, nSoundIndentLevel, "\"pitch\"				\"PITCH_NORM\"\n" );
				BufPrintf( soundScriptBuffer, nSoundIndentLevel, "\"soundlevel\"		\"SNDLVL_95dB\"\n" );
				BufPrintf( soundScriptBuffer, nSoundIndentLevel, "\"wave\"				\"%s\"\n", strWaveName.String() );
			}
			BufEndBlock( soundScriptBuffer, nSoundIndentLevel );

			// write vcd speak event
			BufPrintf( buf, nIndentLevel, "event speak \"%s\"\n", pszSoundScriptName );
			BufBeginBlock( buf, nIndentLevel );
			{
				BufPrintf( buf, nIndentLevel, "time %f <MAX_SEQUENCE_LENGTH>\n", flStartTime ); 
				BufPrintf( buf, nIndentLevel, "param \"%s\"", pszSoundScriptName );
				BufPrintf( buf, nIndentLevel, "fixedlength\n" );
				BufPrintf( buf, nIndentLevel, "cctpe \"cc_master\"\n" );
				BufPrintf( buf, nIndentLevel, "cctoken \"\"\n" );
			}
			BufEndBlock( buf, nIndentLevel );
		}
	}
	BufEndBlock( buf, nIndentLevel );

	CUtlString strBuf = soundScriptBuffer.String();
	FOR_EACH_SUBKEY( GetCustomKeyValues(), pSubKey )
	{
		strBuf = strBuf.Replace( pSubKey->GetName(), pSubKey->GetString() );
	}
	soundScriptBuffer.Clear();
	soundScriptBuffer.PutString( strBuf.String() );

	// write to sound script file

	CUtlString sWorkingDir;
	CItemUpload::GetVProjectDir( sWorkingDir );
	
	char szSoundScriptFullPath[MAX_PATH];
	V_MakeAbsolutePath( szSoundScriptFullPath, sizeof( szSoundScriptFullPath ), m_strSoundScriptFile.String(), sWorkingDir.String() );
	if ( !g_pFullFileSystem->WriteFile( szSoundScriptFullPath, NULL, soundScriptBuffer ) )
	{
		Warning( "Failed to output soundscript: %s\n", szSoundScriptFullPath );
	}

	if ( GetCustomModPath() )
	{
		char szCustomPath[MAX_PATH];
		V_MakeAbsolutePath( szCustomPath, sizeof( szCustomPath ), CFmtStr( "%s/%s", GetCustomModPath(), m_strSoundScriptFile.String() ), sWorkingDir.String() );
		V_FixSlashes( szCustomPath );
		if ( DoFileCopy( szSoundScriptFullPath, szCustomPath ) )
		{
			Asset()->AddModOutput( szCustomPath );
		}
	}
}


// pChannelsClip is assumed to be an exported channelsclip (vs a live channelsclip)
bool CTargetDMX::WriteVCD( CUtlBuffer &vcdBuf )
{
	CDmeAnimationList *pDmeAnimationList = m_pDmRoot->GetValueElement< CDmeAnimationList >( "animationList" );
	CDmeChannelsClip *pDmeChannelsClip = NULL;
 
	if ( pDmeAnimationList )
	{
		if ( pDmeAnimationList->GetAnimationCount() < 0 )
		{
			Warning( "No DmeChannelsClips found in root.animationList of DMX file: \"%s\"\n", m_sInputFile.String() );
			return false;
		}
 
		pDmeChannelsClip = pDmeAnimationList->GetAnimation( 0 );
		if ( !pDmeChannelsClip )
		{
			Warning( "No DmeChannelsClip found in root.animationList[0] of DMX file: \"%s\"\n", m_sInputFile.String() );
			return false;
		}
	}

	DmeFramerate_t framerate( pDmeChannelsClip->GetValue< int >( "frameRate", 30 ) );
	
	int nLevel = 0;
	BufPrintf( vcdBuf, nLevel, "// Choreo version 1\n" );

	// loop animation?
	if ( m_flAnimationLoopStartTime >= 0.f )
	{
		BufPrintf( vcdBuf, nLevel, "event loop \"<ITEMTEST_SEQUENCE_NAME>_loop\"\n" );
		BufBeginBlock( vcdBuf, nLevel );
		{
			BufPrintf( vcdBuf, nLevel, "time <MAX_SEQUENCE_LENGTH> -1.000000\n" );
			BufPrintf( vcdBuf, nLevel, "param \"%f\"\n", m_flAnimationLoopStartTime );
			BufPrintf( vcdBuf, nLevel, "loopcount \"-1\"\n" );
		}
		BufEndBlock( vcdBuf, nLevel );
	}

	BufPrintf( vcdBuf, nLevel, "actor \"<CLASS_NAME>\"\n" );
	BufBeginBlock( vcdBuf, nLevel );
	{

		// sequence channel
		BufPrintf( vcdBuf, nLevel, "channel \"<CLASS_NAME>\"\n" );
		BufBeginBlock( vcdBuf, nLevel );
		{
			BufPrintf( vcdBuf, nLevel, "event sequence <ITEMTEST_SEQUENCE_NAME>\n" );
			BufBeginBlock( vcdBuf, nLevel );
			{
				BufPrintf( vcdBuf, nLevel, "time 0.000000 <MAX_SEQUENCE_LENGTH>\n" );
				BufPrintf( vcdBuf, nLevel, "param <ITEMTEST_SEQUENCE_NAME>\n" );
			}
			BufEndBlock( vcdBuf, nLevel );
		}
		BufEndBlock( vcdBuf, nLevel );

		// Flex Anim channel
		BufPrintf( vcdBuf, nLevel, "channel \"Face\"\n" );
		BufBeginBlock( vcdBuf, nLevel );
		{
			int nChannelCount = pDmeChannelsClip->m_Channels.Count();
			for ( int ci = 0; ci < nChannelCount; ++ci )
			{
				CDmeChannel *pChannel = pDmeChannelsClip->m_Channels[ ci ];
				// skip all transform
				if ( !pChannel || !pChannel->GetToAttribute() || pChannel->GetToAttribute()->GetType() != AT_FLOAT )
					continue;

				CDmElement *pToElement = pChannel->GetToElement();
				const char *pszToElementName = pToElement->GetName();

				// figure out IsStereoControl by checking if prefix is left_ and right_
				// in pDmeChannelsClip->m_Channels
				CDmeChannel *pLeftChannel  = NULL; //pChannel->GetValueElement< CDmeChannel >( "leftvaluechannel" );
				CDmeChannel *pRightChannel = NULL; //pChannel->GetValueElement< CDmeChannel >( "rightvaluechannel" );
				for ( int iSearch = 0; iSearch < nChannelCount; ++iSearch )
				{
					CDmeChannel *pSearchChannel = pDmeChannelsClip->m_Channels[ iSearch ];
					if ( !pSearchChannel )
						continue;

					// search for left and right channels
					const char *pszSearchChannelName = pSearchChannel->GetName();
					if ( V_strstr( pszSearchChannelName, pszToElementName ) )
					{
						if ( V_strstr( pszSearchChannelName, "left_" ) )
						{
							Assert( pLeftChannel == NULL );
							pLeftChannel = pSearchChannel;
						}
						else if ( V_strstr( pszSearchChannelName, "right_" ) )
						{
							Assert( pRightChannel == NULL );
							pRightChannel = pSearchChannel;
						}
					}
				}

				bool bIsStereo = pLeftChannel && pRightChannel;
				if ( bIsStereo )
				{
					// should we do anything for stereo (eye animation)?
					AssertMsg( 0, "Stereo channel is not supported." );
					continue;

					CDmeFloatLogLayer *pLeftLogLayer  = GetTopmostLogLayer< float >( pLeftChannel );
					CDmeFloatLogLayer *pRightLogLayer = GetTopmostLogLayer< float >( pRightChannel );
					if ( !pLeftLogLayer || !pRightLogLayer )
						continue;

					CUtlVectorFixedGrowable< DmeTime_t, 1024 > times;
					CUtlVectorFixedGrowable< float, 1024 > values;
					CUtlVectorFixedGrowable< float, 1024 > balances;

					// we should have some key count on both left and right key
					if ( pLeftLogLayer->GetKeyCount() == 0 || pRightLogLayer->GetKeyCount() == 0 )
					{
						// something is wrong here.
						Assert( 0 );
						continue;
					}

					DmeTime_t tStartOnFrame = MIN( pLeftLogLayer ->GetKeyTime( 0 ), pRightLogLayer->GetKeyTime( 0 ) );
					DmeTime_t tEndOnFrame = MAX( pLeftLogLayer ->GetKeyTime( pLeftLogLayer->GetKeyCount() - 1 ), pRightLogLayer->GetKeyTime( pRightLogLayer->GetKeyCount() - 1 ) );
#if 1 // resample time
					for ( DmeTime_t tCurr = tStartOnFrame; tCurr <= tEndOnFrame; tCurr = tCurr.TimeAtNextFrame( framerate ) )
					{
						float flLeftValue  = pLeftLogLayer->GetValue( pDmeChannelsClip->FromChildMediaTime( tCurr ) );
						float flRightValue = pLeftLogLayer->GetValue( pDmeChannelsClip->FromChildMediaTime( tCurr ) );

						float flValue, flBalance;
						LeftRightToValueBalance( flLeftValue, flRightValue, flValue, flBalance );

						times.AddToTail( tCurr );
						values.AddToTail( flValue );
						balances.AddToTail( flBalance );
					}
#else // merge time list from left and right channels
					int nLeft = pLeftLogLayer->GetKeyCount();
					int nRight = pRightLogLayer->GetKeyCount();
					int iLeft = pLeftLogLayer->FindKey( pDmeChannelsClip->ToChildMediaTime( tStart ) );
					int iRight = pLeftLogLayer->FindKey( pDmeChannelsClip->ToChildMediaTime( tStart ) );
					while ( iLeft < nLeft || iRight < nRight )
					{
						DmeTime_t tLeft  = iLeft  < nLeft  ? pDmeChannelsClip->FromChildMediaTime( pLeftLogLayer ->GetKeyTime( iLeft  ) ) : DMETIME_MAXTIME;
						DmeTime_t tRight = iRight < nRight ? pDmeChannelsClip->FromChildMediaTime( pRightLogLayer->GetKeyTime( iRight ) ) : DMETIME_MAXTIME;

						DmeTime_t t;
						float flLeftVal, flRightVal;
						if ( tLeft == tRight )
						{
							t = tLeft;
							flLeftVal = pLeftLogLayer->GetKeyValue( iLeft );
							flRightVal = pRightLogLayer->GetKeyValue( iRight );
							iLeft++;
							iRight++;
						}
						else if ( tLeft < tRight )
						{
							t = tLeft;
							flLeftVal = pLeftLogLayer->GetKeyValue( iLeft );
							flRightVal = pRightLogLayer->GetValue( t );
							iLeft++;
						}
						else
						{
							t = tRight;
							flLeftVal = pLeftLogLayer->GetValue( t );
							flRightVal = pRightLogLayer->GetKeyValue( iRight );
							iRight++;
						}

						float flValue, flBalance;
						LeftRightToValueBalance( flLeftVal, flRightVal, flValue, flBalance );

						times.AddToTail( t );
						values.AddToTail( flValue );
						balances.AddToTail( flBalance );
					}
#endif
					BufPrintf( vcdBuf, nLevel, "\"%s\" combo\n", pChannel->GetName() );
					BufBeginBlock( vcdBuf, nLevel );

					int nSamples = times.Count();
					for ( int si = 0; si < nSamples; ++si )
					{
						BufPrintf( vcdBuf, nLevel, "%.4f %.4f\n", times[ si ].GetSeconds(), values[ si ] );
					}

					BufEndBlock( vcdBuf, nLevel );

					BufBeginBlock( vcdBuf, nLevel );

					for ( int si = 0; si < nSamples; ++si )
					{
						BufPrintf( vcdBuf, nLevel, "%.4f %.4f\n", times[ si ].GetSeconds(), balances[ si ] );
					}

					BufEndBlock( vcdBuf, nLevel );
				}
				else
				{
					CDmeFloatLogLayer *pLogLayer = GetTopmostLogLayer< float >( pChannel );
					if ( !pLogLayer || pLogLayer->GetKeyCount() == 0 )
						continue;

					BufPrintf( vcdBuf, nLevel, "event expression \"%s\"\n", pToElement->GetName() );
					BufBeginBlock( vcdBuf, nLevel );
					{
						BufPrintf( vcdBuf, nLevel, "time %.4f %.4f\n", pDmeChannelsClip->GetStartTime().GetSeconds(), pDmeChannelsClip->GetEndTime().GetSeconds() );
						BufPrintf( vcdBuf, nLevel, "param \"player\\<CLASS_NAME>\\emotion\\emotion\"\n" );
						BufPrintf( vcdBuf, nLevel, "param2 \"%s\"\n", pToElement->GetName() );
						BufPrintf( vcdBuf, nLevel, "event_ramp\n" );
						BufBeginBlock( vcdBuf, nLevel );
						{
#if 0 // resampling
							if ( tStart < tStartOnFrame )
							{
								float flValue = pLogLayer->GetValue( pDmeChannelsClip->FromChildMediaTime( tStart ) );
								BufPrintf( vcdBuf, nLevel, "%.4f %.4f\n", tStart.GetSeconds(), flValue );
							}

							for ( DmeTime_t tCurr = tStartOnFrame; tCurr <= tEndOnFrame; tCurr = tCurr.TimeAtNextFrame( framerate ) )
							{
								float flValue = pLogLayer->GetValue( pDmeChannelsClip->FromChildMediaTime( tCurr ) );
								BufPrintf( vcdBuf, nLevel, "%.4f %.4f\n", tCurr.GetSeconds(), flValue );
							}

							if ( tEnd > tEndOnFrame )
							{
								float flValue = pLogLayer->GetValue( pDmeChannelsClip->FromChildMediaTime( tEnd ) );
								BufPrintf( vcdBuf, nLevel, "%.4f %.4f\n", tEnd.GetSeconds(), flValue );
							}
#else // use sample time that's already in the animation

							int nKeys = pLogLayer->GetKeyCount();
							for ( int k = 0; k < nKeys; ++k )
							{
								DmeTime_t t = pDmeChannelsClip->FromChildMediaTime( pLogLayer->GetKeyTime( k ) );
								float flValue = pLogLayer->GetKeyValue( k );
								BufPrintf( vcdBuf, nLevel, "%.4f %.4f\n", t.GetSeconds(), flValue );
							}
#endif
						}
						BufEndBlock( vcdBuf, nLevel );
					}
					BufEndBlock( vcdBuf, nLevel );
				}
			}

			// channel sounds
			OutputSounds( vcdBuf, nLevel, m_pDmRoot->GetValueElement< CDmElement >( "exportedSounds" ) );
		}
		BufEndBlock( vcdBuf, nLevel );
	}
	BufEndBlock( vcdBuf, nLevel );
	BufPrintf( vcdBuf, nLevel, "fps %d\n", RoundFloatToInt( framerate.GetFramesPerSecond() ) );
	BufPrintf( vcdBuf, nLevel, "snap off\n" );

	CUtlString strBuf = vcdBuf.String();
	FOR_EACH_SUBKEY( GetCustomKeyValues(), pSubKey )
	{
		strBuf = strBuf.Replace( pSubKey->GetName(), pSubKey->GetString() );
	}

	vcdBuf.Clear();
	vcdBuf.PutString( strBuf );

	return true;
}


//=============================================================================
//
//=============================================================================
CTargetVCD::CTargetVCD( CAsset *pAsset, const CTargetQC *pTargetQC )
	: CTargetBase( pAsset, pTargetQC ), m_pTargetQC( pTargetQC )
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetVCD::Compile()
{
	if ( !CTargetBase::Compile() )
		return false;

	CUtlString sName;
	if ( !GetOutputPath( sName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
		return false;

	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( "CTarget%s::Compile( %s ) - Not Valid: %s\n", GetTypeString(), sName.String(), sTmp.String() );
		return false;
	}

	CUtlString sBinDir;
	if ( !CItemUpload::GetBinDirectory( sBinDir ) )
	{
		Warning( "CTarget%s::Compile( %s ) - GetBinDirectory Failed\n", GetTypeString(), sName.String() );
		return false;
	}

	CUtlString sAbsPath;
	GetOutputPath( sAbsPath, 0 );

	CUtlString sRelPath;
	GetOutputPath( sRelPath, 0, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE );

	if ( sAbsPath.IsEmpty() || sRelPath.IsEmpty() )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPath failed\n", GetTypeString(), sName.String() );
		return false;
	}

	Msg( "Compiling %s: %s\n", GetTypeString(), sRelPath.String() );

	CUtlBuffer bufVCDFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( m_strVCDPath.IsEmpty() )
	{
		if ( !m_pTargetQC->GetTargetDMX( 0 )->WriteVCD( bufVCDFile ) )
		{
			Warning( "CTarget%s::Compile( %s ) - failed to create VCD buffer\n", GetTypeString(), m_strVCDPath.String() );
			return false;
		}
	}
	else if ( !g_pFullFileSystem->ReadFile( m_strVCDPath, NULL, bufVCDFile ) )
	{
		Warning( "CTarget%s::Compile( %s ) - failed to read input file\n", GetTypeString(), m_strVCDPath.String() );
		return false;
	}

	// output to the correct file path
	AddOrEditP4File( sAbsPath.String() );
	g_pFullFileSystem->WriteFile( sAbsPath.String(), "MOD", bufVCDFile );

	Msg( "CTarget%s::Compile OK! - %s\n", GetTypeString(), sRelPath );

	if ( GetCustomModPath() )
	{
		CUtlString sWorkingDir;
		if ( CItemUpload::IgnoreEnvironmentVariables() )
		{
			sWorkingDir = sAbsPath;
			sWorkingDir.SetLength( sWorkingDir.Length() - sRelPath.Length() );
			V_StripTrailingSlash( sWorkingDir.GetForModify() );
		}
		else
		{
			CItemUpload::GetVProjectDir( sWorkingDir );
		}

		char szCustomPath[MAX_PATH];
		V_MakeAbsolutePath( szCustomPath, sizeof( szCustomPath ), CFmtStr( "%s/%s", GetCustomModPath(), sRelPath.String() ), sWorkingDir.String() );
		V_FixSlashes( szCustomPath );
		if ( DoFileCopy( sAbsPath.String(), szCustomPath ) )
		{
			Asset()->AddModOutput( szCustomPath );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const ExtensionList *CTargetVCD::GetExtensionsAndCount( void ) const
{
	static ExtensionList vecExtensions;
	if ( !vecExtensions.Count() )
	{
		vecExtensions.AddToTail( ".vcd" );
	}

	return &vecExtensions;
}


//=============================================================================
//
//=============================================================================
CTargetQC::CTargetQC( CAsset *pAsset, const CTargetMDL *pTargetMDL )
	: CTargetBase( pAsset, pTargetMDL )
	, m_QCTemplate( 0, 0, CUtlBuffer::TEXT_BUFFER )
	, m_TargetVCD( NULL )
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetQC::IsOk( CUtlString &sMsg ) const
{
	if ( m_TargetDMXs.Count() <= 0 )
	{
		sMsg = "No DMX input files specified";
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetQC::Compile()
{
	if ( !CTargetBase::Compile() )
		return false;

	CUtlString sName;
	if ( !GetOutputPath( sName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
		return false;

	CUtlString sAbsPath;
	GetOutputPath( sAbsPath, 0 );

	CUtlString sRelPath;
	GetOutputPath( sRelPath, 0, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE );

	if ( sAbsPath.IsEmpty() || sRelPath.IsEmpty() )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPath failed\n", GetTypeString(), sName.String() );
		return false;
	}

	CUtlString sLOD0;
	GetOutputPath( sLOD0, 0, PATH_FLAG_PATH | PATH_FLAG_FILE | PATH_FLAG_PREFIX | PATH_FLAG_MODELS );

	sLOD0.FixSlashes( '/' );
	const char *pszPostModel = V_strstr( sLOD0.String(), "models/" );
	CUtlString sModelName = pszPostModel ? pszPostModel + 7 : sLOD0;
	sModelName += ".mdl";

	Msg( "Compiling %s: %s\n", GetTypeString(), sRelPath.String() );

	AddOrEditP4File( sAbsPath.String() );

	CUtlString strBuf = GetQCTemplate();
	if ( strBuf.IsEmpty() )
	{
		return false;
	}

	strBuf = strBuf.Replace( "<ITEMTEST_REPLACE_MDLABSPATH>", sModelName.String() );

	for ( int nLOD = 1; nLOD < m_TargetDMXs.Count(); ++nLOD )
	{
		strBuf = strBuf.Replace( CFmtStr( "<ITEMTEST_REPLACE_LOD%d_HEADER>", nLOD ), CFmtStr( "$lod %d", CItemUpload::Manifest()->GetQCLODDistance( nLOD ) ) );
	}

	// Remove REPLACE_LOD block if it's not necessary
	RemoveTextBlock( strBuf.String(), "<ITEMTEST_REPLACE_LOD1_HEADER>", strBuf.GetForModify(), strBuf.Length() );
	RemoveTextBlock( strBuf.String(), "<ITEMTEST_REPLACE_LOD2_HEADER>", strBuf.GetForModify(), strBuf.Length() );

	CUtlString sSearch = "<ITEMTEST_REPLACE_SKIN_OPTIONALBLOCK>";
	CUtlString sReplace = "$cdmaterials \"<ITEMTEST_REPLACE_MATERIALS_PATH>\"\n$texturegroup skinfamilies\n{\n";
	CUtlString sMaterialPath;
	for ( int nSkin = 0; nSkin < CItemUpload::Manifest()->GetNumMaterialSkins(); ++nSkin )
	{
		int nVMTCount = 0;
		CUtlString sSkinMaterials = "    { ";
		for ( int nVMT = 0; nVMT < Asset()->GetTargetVMTCount(); ++nVMT )
		{
			CTargetVMT *pTargetVMT = Asset()->GetTargetVMT( nVMT );

			if ( nSkin >= pTargetVMT->GetOutputCount() )
				continue;

			// We grab the extension which includes the skin variant, and then trim the actual file extension
			CUtlString sVMTName;
			if ( !pTargetVMT->GetOutputPath( sVMTName, nSkin, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
			{
				Warning( "CTarget%s::Compile( %s ) - VMT %d skin %d GetOutputPath failed\n", GetTypeString(), sName.String(), nVMT, nSkin );
				continue;
			}
			V_StripExtension( sVMTName.String(), sVMTName.GetForModify(), sVMTName.Length() );

			if ( sMaterialPath.IsEmpty() )
			{
				pTargetVMT->GetOutputPath( sMaterialPath, 0, PATH_FLAG_PATH | PATH_FLAG_MODELS );
			}

			sSkinMaterials += CFmtStr( "\"%s\" ", sVMTName.String() );
			++nVMTCount;
		}
		sSkinMaterials += "}\n";

		if ( nVMTCount > 0 	)
		{
			sReplace += sSkinMaterials;
		}
	}
	sReplace += "}\n";
	strBuf = strBuf.Replace( sSearch.String(), sReplace.String() );
	strBuf = strBuf.Replace( "<ITEMTEST_REPLACE_MATERIALS_PATH>", sMaterialPath.String() );

	for ( int nLOD = 0; nLOD < m_TargetDMXs.Count(); ++nLOD )
	{
		CUtlString sLOD;
		m_TargetDMXs[ nLOD ]->GetOutputPath( sLOD, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION );

		strBuf = strBuf.Replace( CFmtStr( "<ITEMTEST_REPLACE_LOD%d>", nLOD ), sLOD.String() );
	}

	// Should we include QCI?
	if ( !m_strQCITemplate.IsEmpty() )
	{
		CUtlString strQCIDir;
		m_TargetDMXs[ 0 ]->GetOutputPath( strQCIDir, 1, PATH_FLAG_PATH & ~PATH_FLAG_ABSOLUTE );
		strQCIDir = strQCIDir.Replace( '\\', '/' );
		strBuf = strBuf.Replace( "<QCI_RELATIVE_DIR>", strQCIDir.String() );

		CUtlString strQCIRel;
		m_TargetDMXs[ 0 ]->GetOutputPath( strQCIRel, 1, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE & ~PATH_FLAG_MODELS );
		strQCIRel = strQCIRel.Replace( '\\', '/' );
		strBuf = strBuf.Replace( "<QCI_RELATIVE_PATH>", strQCIRel.String() );
	}

	FOR_EACH_SUBKEY( GetCustomKeyValues(), pSubKey )
	{
		strBuf = strBuf.Replace( pSubKey->GetName(), pSubKey->GetString() );
	}

	CUtlBuffer bufQCFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
	bufQCFile.PutString( strBuf.String() );

	g_pFullFileSystem->WriteFile( sAbsPath.String(), "MOD", bufQCFile );

	if ( !CheckFile( sAbsPath.String() ) )
	{
		Warning( "CTarget%s::Compile( %s ) - File Check Failed - \"%s\"\n", GetTypeString(), sName.String(), sAbsPath.String() );
		return false;
	}

	if ( CItemUpload::Manifest()->UseTerseMessages() )
	{
		Msg( " - Compilation successful.\n" );
	}
	else
	{
		Msg( "CTarget%s::Compile OK! - %s\n", GetTypeString(), sRelPath.String() );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const ExtensionList *CTargetQC::GetExtensionsAndCount( void ) const
{
	static ExtensionList vecExtensions;
	if ( !vecExtensions.Count() )
	{
		vecExtensions.AddToTail( ".qc" );
	}

	return &vecExtensions;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetQC::GetInputs( CUtlVector< CTargetBase * > &sInputs ) const
{
	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	int nOkCount = 0;

	for ( int i = 0; i < m_TargetDMXs.Count(); ++i )
	{
		CTargetBase *pTargetBase = m_TargetDMXs.Element( i ).GetObject();
		if ( !pTargetBase )
			continue;

		nOkCount += ( sInputs.AddToTail( pTargetBase ) >= 0 ? 1 : 0 );
	}

	if ( m_TargetVCD.IsValid() )
	{
		CTargetBase *pTargetBase = m_TargetVCD.GetObject();
		if ( pTargetBase )
		{
			nOkCount += ( sInputs.AddToTail( pTargetBase ) >= 0 ? 1 : 0 );
		}
	}

	return nOkCount > 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CTargetQC::TargetDMXCount() const
{
	return m_TargetDMXs.Count();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTargetQC::SetQCTemplate( const char *pszQCTemplate )
{
	m_QCTemplate.Clear();
	m_QCTemplate.PutString( pszQCTemplate );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CTargetQC::GetQCTemplate()
{
	if ( m_QCTemplate.TellMaxPut() == 0 )
	{
		const char *pszQCTemplate = CItemUpload::Manifest()->GetQCTemplate();
		if ( pszQCTemplate )
		{
			if ( !g_pFullFileSystem->ReadFile( pszQCTemplate, "MOD", m_QCTemplate ) )
			{
				Warning( "Failed to load specified QC template '%s'.\n", pszQCTemplate );
			}
		}
	}
	return (char*)m_QCTemplate.Base();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CTargetQC::AddTargetDMX( const char *pszGeometryFile )
{
	CSmartPtr< CTargetDMX > pTargetDMX = Asset()->NewTarget< CTargetDMX >( this );
	if ( !pTargetDMX )
		return -1;

	const int nIndex = m_TargetDMXs.AddToTail( pTargetDMX );

	if ( nIndex < 0 )
	{
		// Failed to add
		return -1;
	}

	if ( !SetTargetDMX( nIndex, pszGeometryFile ) )
	{
		m_TargetDMXs.RemoveMultipleFromTail( 1 );
		return -1;
	}

	return nIndex;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetQC::SetTargetDMX( int nLOD, const char *pszGeometryFile )
{
	if ( nLOD >= TargetDMXCount() )
		return false;

	CSmartPtr< CTargetDMX > pTargetDMX = m_TargetDMXs.Element( nLOD );
	if ( !pTargetDMX )
		return false;

	pTargetDMX->SetNameSuffix( GetNameSuffix() );
	pTargetDMX->SetLod( nLOD );
	pTargetDMX->SetQCITemplate( m_strQCITemplate.String() );

	return pTargetDMX->SetInputFile( pszGeometryFile );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetQC::RemoveTargetDMX( int nLOD )
{
	if ( nLOD != m_TargetDMXs.Count() - 1 )
		return false;

	m_TargetDMXs.RemoveMultipleFromTail( 1 );

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSmartPtr< CTargetDMX > CTargetQC::GetTargetDMX( int nLOD ) const
{
	if ( nLOD >= TargetDMXCount() )
		return NULL;

	return m_TargetDMXs.Element( nLOD );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSmartPtr< CTargetVCD > CTargetQC::GetTargetVCD()
{
	if ( m_TargetVCD == NULL )
	{
		m_TargetVCD = Asset()->NewTarget< CTargetVCD >( this );
	}

	return m_TargetVCD;
}


//=============================================================================
//
//=============================================================================
CTargetMDL::CTargetMDL( CAsset *pAsset, const CTargetBase *pTargetParent )
	: CTargetBase( pAsset, pTargetParent )
	, m_pTargetQC( NULL )
{
	m_pTargetQC = Asset()->NewTarget< CTargetQC >( this );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetMDL::IsOk( CUtlString &sMsg ) const
{
	CUtlString sTmp;
	if ( !m_pTargetQC || !m_pTargetQC->IsOk( sTmp ) )
	{
		sMsg = "Invalid QC: ";
		sMsg += sTmp;
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetMDL::Compile()
{
	if ( !CTargetBase::Compile() )
		return false;

	CUtlString sName;
	if ( !GetOutputPath( sName, 0, PATH_FLAG_FILE | PATH_FLAG_EXTENSION ) )
		return false;

	CUtlString sBinDir;
	if ( !CItemUpload::GetBinDirectory( sBinDir ) )
	{
		Warning( "CTarget%s::Compile( %s ) - GetBinDirectory Failed\n", GetTypeString(), sName.String() );
		return false;
	}

	CUtlVector< CUtlString > sAbsPaths;
	GetOutputPaths( sAbsPaths );
	if ( sAbsPaths.Count() <= 0 )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPaths failed\n", GetTypeString(), sName.String() );
		return false;
	}

	CUtlString sAbsPath = sAbsPaths.Element( 0 );

	CUtlString sRelPath;
	GetOutputPath( sRelPath, 0, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE );

	if ( sAbsPath.IsEmpty() || sRelPath.IsEmpty() )
	{
		Warning( "CTarget%s::Compile( %s ) - GetOutputPath failed\n", GetTypeString(), sName.String() );
		return false;
	}

	CUtlVector< CUtlString > sAbsInputPaths;
	if ( !GetInputPaths( sAbsInputPaths, false, false ) )
	{
		Warning( "CTarget%s::Compile( %s ) - GetInputPaths failed\n", GetTypeString(), sName.String() );
		return false;
	}

	if ( sAbsInputPaths.Count() != 1 )
	{
		Warning( "CTarget%s::Compile( %s ) - GetPaths returned %d paths, expected 1\n", GetTypeString(), sName.String(), sAbsInputPaths.Count() );
		return false;
	}

	if ( !Asset()->Mkdir( NULL, this ) )
	{
		Warning( "CTarget%s::Compile - Mkdir failed\n", GetTypeString() );
		return false;
	}

	Msg( "Compiling %s: %s\n", GetTypeString(), sRelPath.String() );

	if ( CItemUpload::GetP4() )
	{
		for ( int i = 0; i < sAbsPaths.Count(); ++i )
		{
			AddOrEditP4File( sAbsPaths.Element( i ).String() );
		}
	}

	CUtlString sWorkingDir;
	CFmtStrN< k64KB > sCmd;
	if ( CItemUpload::IgnoreEnvironmentVariables() )
	{
		sWorkingDir = sAbsPath;
		sWorkingDir.SetLength( sWorkingDir.Length() - sRelPath.Length() );
		V_StripTrailingSlash( sWorkingDir.GetForModify() );

		sCmd.sprintf( "\"%s\\studiomdl.exe\" -nop4 -game \"%s\" \"%s\"", sBinDir.String(), sWorkingDir.String(), sAbsInputPaths.Element( 0 ).String() );
	}
	else
	{
		CItemUpload::GetVProjectDir( sWorkingDir );
		sCmd.sprintf( "\"%s\\studiomdl.exe\" -nop4 -vproject \"%s\" \"%s\"", sBinDir.String(), sWorkingDir.String(), sAbsInputPaths.Element( 0 ).String() );
	}

	bool bOk = true;

	if ( CItemUpload::RunCommandLine( sCmd.Access(), sBinDir.String(), this ) )
	{
		for ( int i = 0; i < sAbsPaths.Count(); ++i )
		{
			if ( !CheckFile( sAbsPaths.Element( i ).String() ) )
			{
				Warning( "CTarget%s::Compile( %s ) - CheckFile failed - \"%s\"\n", GetTypeString(), sName.String(), sAbsPaths.Element( i ).String() );
				bOk = false;
				break;
			}
		}
	}
	else
	{
		bOk = false;
	}

	if ( bOk )
	{
		if ( CItemUpload::Manifest()->UseTerseMessages() )
		{
			Msg( " - Compilation successful.\n" );
		}
		else
		{
			Msg( "CTarget%s::Compile OK! - %s\n", GetTypeString(), sRelPath.String() );
		}
	}
	else
	{
		Warning( "CTarget%s::Compile Failed - %s\n", GetTypeString(), sAbsPath.String() );
		return false;
	}

	if ( GetCustomModPath() )
	{
		char szCustomPath[MAX_PATH];
		V_MakeAbsolutePath( szCustomPath, sizeof( szCustomPath ), CFmtStr( "%s/%s", GetCustomModPath(), sRelPath.String() ), sWorkingDir.String() );
		V_FixSlashes( szCustomPath );
		DoFileCopy( sAbsPath.String(), szCustomPath );
		Asset()->AddModOutput( szCustomPath );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const ExtensionList *CTargetMDL::GetExtensionsAndCount( void ) const
{
	// Only output .mdl for animation
	CSmartPtr< CTargetQC > pTargetQC = GetTargetQC();
	if ( pTargetQC.IsValid() && pTargetQC->GetQCITemplate() )
	{
		return CItemUpload::Manifest()->GetAnimationMDLExtentions();
	}

	return CItemUpload::Manifest()->GetMDLExtensions();
	
/*
	static const char *s_szExtensionsDX8[] = {
		".mdl",
		".dx80.vtx",
		".dx90.vtx",
		".sw.vtx",
		".phy",
		".vvd"
	};

	static const char *s_szExtensions[] = {
		".mdl",
		".dx90.vtx",
		".phy",
		".vvd"
	};

	static const char **s_ppszExtensions = s_szExtensionsDX8;
	static bool s_bGameInfoParsed = false;

	if ( !s_bGameInfoParsed )
	{
		KeyValues *pKeyValues = new KeyValues( "gameinfo.txt" );
		if ( pKeyValues != NULL )
		{
			if ( g_pFullFileSystem && pKeyValues->LoadFromFile( g_pFullFileSystem, "gameinfo.txt" ) )
			{
				if ( pKeyValues->GetInt( "SupportsDX8" ) != 0 )
				{
					s_ppszExtensions = s_szExtensionsDX8;
				}
				else
				{
					s_ppszExtensions = s_szExtensions;
				}

				s_bGameInfoParsed = true;
			}
			pKeyValues->deleteThis();
		}
	}

	if ( s_ppszExtensions == s_szExtensions  )
	{
		nExtensionCount = ARRAYSIZE( s_szExtensions );
	}
	else
	{
		nExtensionCount = ARRAYSIZE( s_szExtensionsDX8 );
	}

	return s_ppszExtensions;
*/
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CTargetMDL::GetInputs( CUtlVector< CTargetBase * > &inputs ) const
{
	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	inputs.RemoveAll();

	inputs.AddToTail( m_pTargetQC.GetObject() );

	return true;
}


//=============================================================================
//
//=============================================================================
CAsset::CAsset()
: CTargetBase( NULL, NULL )
, m_bSkinToBipHead( false )
, m_nCurrentModel( -1 )
, m_vmtMap( UtlStringLessThan )
, m_bShouldBuildScenesImage( false )
{
	m_pAsset = this;

	CItemUpload::GetSteamId( m_sSteamId );

	AddModel();

	m_vecTargetIcons.SetCount( CItemUpload::Manifest()->GetNumIconTypes() );

	m_sExcludeFileExtensions.AddToTail( "zip" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CAsset::CAsset( const char *pszName, bool *pbOk /* = NULL */ )
: CTargetBase( NULL, NULL )
, m_nCurrentModel( -1 )
, m_bShouldBuildScenesImage( false )
{
	m_pAsset = this;

	CItemUpload::GetSteamId( m_sSteamId );

	m_sName = pszName;

	AddModel();

	m_vecTargetIcons.SetCount( CItemUpload::Manifest()->GetNumIconTypes() );

	if ( pbOk )
	{
		CUtlString sTmp;
		*pbOk = IsOk( sTmp );

		if ( !( *pbOk ) )
		{
			Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		}
	}

	m_sExcludeFileExtensions.AddToTail( "zip" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CAsset::~CAsset()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::IsOk( CUtlString &sMsg ) const
{
	if ( !IsNameValid() )
	{
		sMsg = "Invalid asset name";
		return false;
	}

	if ( !IsSteamIdValid() )
	{
		sMsg = "Invalid steam id";
		return false;
	}

	if ( !GetTargetMDL().IsValid() )
	{
		sMsg = "No target MDL";
		return false;
	}

	CUtlString sTmp;
	if ( !CItemUpload::GetContentDir( sTmp ) || sTmp.IsEmpty() )
	{
		sMsg = "Cannot figure out content directory, have you installed the Source SDK and restarted Steam?";
		return false;
	}

	return true;
}


bool CAsset::BuildScenesImage()
{
	CUtlString sBinDir;
	if ( !CItemUpload::GetBinDirectory( sBinDir ) )
	{
		Warning( "CTarget%s::Compile( %s ) - GetBinDirectory Failed\n", GetTypeString(), GetAssetName() );
		return false;
	}

	CUtlString sAbsPath;
	GetOutputPath( sAbsPath );

	CUtlString sRelPath;
	GetOutputPath( sRelPath, 0, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE );

	CUtlString sWorkingDir;
	if ( CItemUpload::IgnoreEnvironmentVariables() )
	{
		sWorkingDir = sAbsPath;
		sWorkingDir.SetLength( sWorkingDir.Length() - sRelPath.Length() );
		V_StripTrailingSlash( sWorkingDir.GetForModify() );
	}
	else
	{
		CItemUpload::GetVProjectDir( sWorkingDir );
	}

	char szScenesImage[MAX_PATH];
	V_MakeAbsolutePath( szScenesImage, sizeof( szScenesImage ), "scenes\\scenes.image", sWorkingDir.String() );
	AddOrEditP4File( szScenesImage );

	CFmtStrN< k64KB > sCmd;
	sCmd.sprintf( "%s\\makescenesimage.exe", sBinDir.String() );

	if ( !CItemUpload::RunCommandLine( sCmd.Access(), sWorkingDir.String(), this ) )
	{
		Warning( "Failed to build %s\n", szScenesImage );
		return false;
	}

	Msg( "Build scenes.image OK!\n" );

	if ( GetCustomModPath() )
	{
		char szCustomPath[MAX_PATH];
		V_MakeAbsolutePath( szCustomPath, sizeof( szCustomPath ), CFmtStr( "%s/%s", GetCustomModPath(), "scenes/scenes.image" ), sWorkingDir.String() );
		V_FixSlashes( szCustomPath );
		DoFileCopy( szScenesImage, szCustomPath );
		Asset()->AddModOutput( szCustomPath );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::CompilePreview()
{
	bool bOldP4State = CItemUpload::GetP4();
	CItemUpload::SetP4( false );
	m_CompileOutputFiles.RemoveAll();
	g_bCompilePreview = true;

	// Record the files that are created so they can be removed after preview
	m_sAbsPaths.RemoveAll();
	m_sRelPaths.RemoveAll();
	m_sModOutputs.RemoveAll();
	CUtlVector< CTargetBase * > inputs;
	if ( GetInputs( inputs ) )
	{
		for ( int i = 0; i < inputs.Count(); ++i )
		{
			CTargetBase *pTargetBase = inputs.Element( i );
			if ( !pTargetBase )
				continue;

			pTargetBase->GetOutputPaths( m_sAbsPaths, PATH_FLAG_ALL, true );
			pTargetBase->GetOutputPaths( m_sRelPaths, (PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE) | PATH_FLAG_ZIP, true );
		}
	}

	bool bResult = CTargetBase::Compile();
	g_bCompilePreview = false;
	CItemUpload::SetP4( bOldP4State );

	if ( bResult )
	{
		PostCompilePreview();
	}

	return bResult;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::PostCompilePreview()
{
	Msg( "CAsset::PostCompilePreview Start:\n");

	bool bResult = true;
	if ( m_bShouldBuildScenesImage )
	{
		bResult = BuildScenesImage();
	}

	if ( bResult )
	{
		Msg( "CAsset::PostCompilePreview OK!\n" );
	}
	else
	{
		Msg( "CAsset::PostCompilePreview FAILED!\n" );
	}

	return bResult;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::Compile()
{
	m_CompileOutputFiles.RemoveAll();

	if ( m_sArchivePath.IsEmpty() )
	{
		GetOutputPath( m_sArchivePath, 0 );
	}

	CUtlString sRelPath;
	GetOutputPath( sRelPath, 0, PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE );

	if ( m_sArchivePath.IsEmpty() || sRelPath.IsEmpty() )
	{
		Warning( "CTarget%s::Compile - GetOutputPath failed\n", GetTypeString() );
		return false;
	}

	// this gets filled by Compile()
	m_sModOutputs.RemoveAll();

	if ( !CTargetBase::Compile() )
		return false;

	m_sAbsPaths.RemoveAll();
	GetOutputPaths( m_sAbsPaths, PATH_FLAG_ALL, true );

	m_sRelPaths.RemoveAll();
	GetOutputPaths( m_sRelPaths, (PATH_FLAG_ALL & ~PATH_FLAG_ABSOLUTE) | PATH_FLAG_ZIP, true );

	Assert( m_sAbsPaths.Count() == m_sRelPaths.Count() );

	if ( m_sAbsPaths.Count() <= 0 || m_sAbsPaths.Count() != m_sRelPaths.Count() )
	{
		Warning( "CTarget%s::Compile - GetOutputPaths failed\n", GetTypeString() );
		return false;
	}

	Msg( "Compiling %s: %s\n", GetTypeString(), sRelPath.String() );

	AddOrEditP4File( m_sArchivePath.String() );

	if ( CItemUpload::FileExists( m_sArchivePath.String() ) )
	{
		_unlink( m_sArchivePath.String() );
	}

	HANDLE m_hOutputZipFile = CreateFile( m_sArchivePath.String(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( m_hOutputZipFile == INVALID_HANDLE_VALUE )
	{
		Warning( "CTarget%s::Compile CreateZip Failed - Unable to create ZIP file %s.\n", GetTypeString(), m_sArchivePath.String() );
		return false;
	}

	IZip *pZip = IZip::CreateZip();
	if ( pZip == NULL )
	{
		Warning( "CTarget%s::Compile CreateZip Failed - %s\n", GetTypeString(), sRelPath.String() );
		CloseHandle( m_hOutputZipFile );
		return false;
	}

	CUtlBuffer kvBuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	CreateManifest( kvBuf );

	pZip->AddBufferToZip( "manifest.txt", kvBuf.Base(), kvBuf.TellPut(), true );

	for ( int i = 0; i < m_sAbsPaths.Count(); ++i )
	{
		const char *pszFileExtension = V_GetFileExtension( m_sAbsPaths.Element( i ).String() );

		bool bExcluded = false;
		FOR_EACH_VEC( m_sExcludeFileExtensions, e )
		{
			if ( m_sExcludeFileExtensions[e] == pszFileExtension )
			{
				bExcluded = true;
				break;
			}
		}

		if ( bExcluded )
			continue;

		if ( CItemUpload::Manifest()->UseTerseMessages() )
		{
			Msg( " - added %s\n", m_sRelPaths.Element( i ).String() );
		}
		else
		{
			Msg( " + Zip Add: %s\n", m_sRelPaths.Element( i ).String() );
		}

		pZip->AddFileToZip( m_sRelPaths.Element( i ).String(), m_sAbsPaths.Element( i ).String() );
	}

	pZip->SaveToDisk( m_hOutputZipFile );
	IZip::ReleaseZip( pZip );

	CloseHandle( m_hOutputZipFile );

	if ( !CheckFile( m_sArchivePath.String() ) )
	{
		if ( CItemUpload::Manifest()->UseTerseMessages() )
		{
			Warning( "Failed to write .zip file: \"%s\"\n", m_sArchivePath.String() );
		}
		else
		{
			Warning( "CTarget%s::Compile - File Check Failed - \"%s\"\n", GetTypeString(), m_sArchivePath.String() );
		}
		return false;
	}

	if ( CItemUpload::Manifest()->UseTerseMessages() )
	{
		Msg( " - Compilation successful.\n" );
	}
	else
	{
		Msg( "CTarget%s::Compile OK! - %s\n", GetTypeString(), sRelPath.String() );
	}

	return PostCompile();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::PostCompile()
{
	Msg( "CAsset::PostCompile Start:\n");

	bool bResult = true;
	if ( m_bShouldBuildScenesImage )
	{
		bResult = BuildScenesImage();
	}

	if ( bResult )
	{
		Msg( "CAsset::PostCompile OK!\n" );
	}
	else
	{
		Msg( "CAsset::PostCompile FAILED!\n" );
	}

	return bResult;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::GetInputs( CUtlVector< CTargetBase * > &inputs ) const
{
	CUtlString sTmp;
	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	for ( int nIcon = 0; nIcon < m_vecTargetIcons.Count(); ++nIcon )
	{
		if ( m_vecTargetIcons[ nIcon ].IsValid() )
		{
			inputs.AddToTail( m_vecTargetIcons[ nIcon ].GetObject() );
		}
	}

	for ( int i = 0; i < GetTargetVMTCount(); ++i )
	{
		inputs.AddToTail( GetTargetVMT( i ) );
	}

	FOR_EACH_VEC( m_vecModels, nModelIndex )
	{
		if ( m_vecModels[ nModelIndex ].IsValid() )
		{
			inputs.AddToTail( m_vecModels[ nModelIndex ].GetObject() );
		}
	}

	return inputs.Count() > 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const ExtensionList *CAsset::GetExtensionsAndCount( void ) const
{
	static ExtensionList vecExtensions;
	if ( !vecExtensions.Count() )
	{
		vecExtensions.AddToTail( ".zip" );
	}

	return &vecExtensions;
}


//-----------------------------------------------------------------------------
// Returns items/<steamid>/<name>, false if there's something wrong
//-----------------------------------------------------------------------------
bool CAsset::GetRelativeDir( CUtlString &sRelativeDir, const char *pszPrefix, const CTargetBase *pTarget ) const
{
	CUtlString sTmp;

	if ( !IsOk( sTmp ) )
	{
		Warning( FUNCTION_LINE_STRING "Error! CTarget%s - Not Valid: %s\n", GetTypeString(), sTmp.String() );
		return false;
	}

	if ( pszPrefix )
	{
		sRelativeDir = pszPrefix;
		sRelativeDir += "/";
	}
	else
	{
		sRelativeDir = "";
	}

	if ( pTarget->IsModelPath() )
		sRelativeDir += "models/";

	if ( pTarget->GetCustomRelativeDir() )
	{
		sRelativeDir += pTarget->GetCustomRelativeDir();
	}
	else
	{
		sRelativeDir += pTarget->GetItemDirectory();

		const char *pszClass = GetClass();
		if ( pszClass )
		{
			sRelativeDir += pszClass;
			sRelativeDir += "/";
		}

		if ( CItemUpload::Manifest()->GetItemPathUsesSteamId() )
		{
			if ( !m_sSteamId.IsEmpty() )
			{
				sRelativeDir += m_sSteamId;
				sRelativeDir += "/";
			}
		}

		sRelativeDir += m_sName;
	}

	sRelativeDir.FixSlashes();

	return true;
}


//-----------------------------------------------------------------------------
// Returns GetAbsoluteContentDir()/GetRelativeDir() or GetAbsoluteGameDir()/GetRelativeDir()
//-----------------------------------------------------------------------------
bool CAsset::GetAbsoluteDir( CUtlString &sAbsoluteDir, const char *pszPrefix /* = NULL */, const CTargetBase *pTarget  ) const
{
	CUtlString sDirA;

	if ( pTarget->IsContent() )
	{
		if ( !CItemUpload::GetContentDir( sDirA ) )
			return false;
	}
	else
	{
		if ( !CItemUpload::GetVProjectDir( sDirA ) )
			return false;
	}

	CUtlString sDirB;

	if ( !GetRelativeDir( sDirB, pszPrefix, pTarget ) )
		return false;

	sAbsoluteDir = sDirA;
	sAbsoluteDir += "/";
	sAbsoluteDir += sDirB;

	sAbsoluteDir.FixSlashes();

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::SetName( const char *pszName )
{
	if ( !CItemUpload::SanitizeName( pszName, m_sName ) )
		return false;

	return IsNameValid();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::IsNameValid() const
{
	return m_sName.Length() > 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CAsset::GetSteamId() const
{
	return m_sSteamId.String();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::IsSteamIdValid() const
{
	return CItemUpload::GetDevMode() || m_sSteamId.Length() > 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::SetTargetIcon( int nIcon, const char *pszIconFile )
{
	if ( !m_vecTargetIcons[ nIcon ].IsValid() )
	{
		m_vecTargetIcons[ nIcon ] = new CTargetIcon( this, nIcon );
	}
	return m_vecTargetIcons[ nIcon ]->SetTargetVTF( pszIconFile );
}


//-----------------------------------------------------------------------------
// Add a new model to the output and make it current
//-----------------------------------------------------------------------------
int CAsset::AddModel()
{
	CSmartPtr< CTargetMDL > pModel = NewTarget< CTargetMDL >( this );
	m_nCurrentModel = m_vecModels.AddToTail( pModel );
	return m_nCurrentModel;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::SetCurrentModel( int nModel )
{
	if ( nModel >= 0 && nModel < GetNumModels() )
	{
		m_nCurrentModel = nModel;
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAsset::RemoveModels()
{
	m_vecModels.RemoveAll();
	m_nCurrentModel = -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CAsset::TargetDMXCount() const
{
	CSmartPtr< CTargetQC > pTargetQC = GetTargetQC();
	if ( !pTargetQC )
		return false;

	return pTargetQC->TargetDMXCount();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CAsset::AddTargetDMX( const char *pszGeometryFile )
{
	CSmartPtr< CTargetQC > pTargetQC = GetTargetQC();
	if ( !pTargetQC )
		return -1;

	return pTargetQC->AddTargetDMX( pszGeometryFile );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::SetTargetDMX( int nLOD, const char *pszGeometryFile )
{
	CSmartPtr< CTargetQC > pTargetQC = GetTargetQC();
	if ( !pTargetQC )
		return false;

	return pTargetQC->SetTargetDMX( nLOD, pszGeometryFile );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::RemoveTargetDMX( int nLOD )
{
	CSmartPtr< CTargetQC > pTargetQC = GetTargetQC();
	if ( !pTargetQC )
		return false;

	return pTargetQC->RemoveTargetDMX( nLOD );
}


//-----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
CSmartPtr< CTargetDMX > CAsset::GetTargetDMX( int nLOD )
{
	CSmartPtr< CTargetQC > pTargetQC = GetTargetQC();
	if ( !pTargetQC )
		return false;

	return pTargetQC->GetTargetDMX( nLOD );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSmartPtr< CTargetMDL > CAsset::GetTargetMDL() const
{
	if ( m_nCurrentModel >= 0 )
	{
		return m_vecModels[ m_nCurrentModel ];
	}
	return CSmartPtr< CTargetMDL >();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSmartPtr< CTargetQC > CAsset::GetTargetQC() const
{
	CSmartPtr< CTargetMDL > pTargetMDL = GetTargetMDL();
	if ( !pTargetMDL )
		return NULL;

	return pTargetMDL->GetTargetQC();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CAsset::GetTargetVMTCount() const
{
	return m_vmtMap.Count();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CTargetVMT *CAsset::GetTargetVMT( int nIndex ) const
{
	if ( nIndex < 0 || nIndex >= GetTargetVMTCount() )
		return NULL;

	int nMapIndex = 0;

	FOR_EACH_MAP( m_vmtMap, nMapIt )
	{
		if ( nIndex == nMapIndex )
			return m_vmtMap.Element( nMapIt );

		++nMapIndex;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSmartPtr< CTargetVMT > CAsset::FindOrAddMaterial( const char *pszMaterial, int nMaterialType )
{
	const CUtlString sMaterial( pszMaterial );

	CUtlMap< CUtlString, CTargetVMT * >::IndexType_t nIndex = m_vmtMap.Find( sMaterial );

	if ( !m_vmtMap.IsValidIndex( nIndex ) )
	{
		CSmartPtr< CTargetVMT > pTargetVMT = NewTarget< CTargetVMT >( this );
		if ( !pTargetVMT )
			return NULL;

		pTargetVMT->SetMaterialId( pszMaterial );

		m_vmtMap.Insert( sMaterial, pTargetVMT.GetObject() );

		// See if this is a duplicate of an existing material
		for ( int i = 0; i < GetTargetVMTCount(); ++i )
		{
			CTargetVMT *pTmpTargetVMT = GetTargetVMT( i );
			if ( !pTmpTargetVMT )
				continue;

			if ( pTmpTargetVMT->GetMaterialType() == nMaterialType )
			{
				pTargetVMT->SetDuplicate( nMaterialType );
				break;
			}
		}
		if ( !pTargetVMT->GetDuplicate() )
		{
			pTargetVMT->SetMaterialType( nMaterialType );
		}

		return pTargetVMT;
	}
	else
	{
		return m_vmtMap.Element( nIndex );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSmartPtr< CTargetVMT > CAsset::FindMaterial( const char *pszMaterial )
{
	const CUtlString sMaterial( pszMaterial );

	CUtlMap< CUtlString, CTargetVMT * >::IndexType_t nIndex = m_vmtMap.Find( sMaterial );

	if ( m_vmtMap.IsValidIndex( nIndex ) )
		return m_vmtMap.Element( nIndex );

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::Mkdir( const char *pszPrefix, const CTargetBase *pTarget )
{
	CUtlString sAbsolute;

	if ( !GetAbsoluteDir( sAbsolute, pszPrefix, pTarget ) )
		return false;

	char szBuf[ k64KB ];
	if ( _fullpath( szBuf, sAbsolute.String(), ARRAYSIZE( szBuf ) ) == NULL )
		return false;

	return CItemUpload::CreateDirectory( szBuf );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAsset::CreateManifest( CUtlBuffer &manifestBuf )
{
	KeyValues *pAssetKV = new KeyValues( "asset" );

	UpdateManifest( pAssetKV );

	manifestBuf.Clear();
	manifestBuf.SetBufferType( true, true );
	pAssetKV->RecursiveSaveToFile( manifestBuf, 0 );

	pAssetKV->deleteThis();
}


const char* CAsset::CheckRedundantOutputFilePath( const char* pszInputFilePath, const char* pszVTEXConfig, const char* pszOutputFilePath )
{
	const char* pszLocalVTEXConfig = pszVTEXConfig ? pszVTEXConfig : "";

	// we don't want to output multiple of the same texture file with the same vtex config
	for ( int i=0; i<m_CompileOutputFiles.Count(); ++i )
	{
		const CompileOutputFile_t& tga = m_CompileOutputFiles[i];
		if ( !V_stricmp( tga.m_strInputFilePath.String(), pszInputFilePath ) )
		{
			if ( !V_stricmp( tga.m_strVTEXConfig.String(), pszLocalVTEXConfig ) )
			{
				return tga.m_strOutputFilePath.String();
			}
		}
	}

	int index = m_CompileOutputFiles.AddToTail();
	CompileOutputFile_t& newTGA		= m_CompileOutputFiles[index];
	newTGA.m_strInputFilePath		= pszInputFilePath;
	newTGA.m_strVTEXConfig			= pszLocalVTEXConfig;
	newTGA.m_strOutputFilePath		= pszOutputFilePath;

	return newTGA.m_strOutputFilePath.String();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CAsset::RemoveMaterial( const char *pszMaterial )
{
	const CUtlString sMaterial( pszMaterial );

	return m_vmtMap.Remove( sMaterial );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const CUtlString &CAsset::GetAssetName() const
{
	return m_sName;
}
