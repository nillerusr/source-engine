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
#include "filesystem_init.h"
#include "icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "mathlib/mathlib.h"
#include "vstdlib/vstdlib.h"
#include "vstdlib/iprocessutils.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstringmap.h"
#include "sfmobjects/sfmsession.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmetrack.h"
#include "movieobjects/dmetrackgroup.h"
#include "movieobjects/dmegamemodel.h"
#include "movieobjects/dmesound.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeanimationset.h"
#include "studio.h"
#include "sfmobjects/sfmanimationsetutils.h"
#include "sfmobjects/flexcontrolbuilder.h"
#include "sfmobjects/sfmphonemeextractor.h"
#include "sfmobjects/exportfacialanimation.h"
#include "soundemittersystem/isoundemittersystembase.h"
#include "phonemeconverter.h"
#include "tier2/riff.h"
#include "tier2/soundutils.h"
#include "soundchars.h"
#include <ctype.h>

#ifdef _DEBUG
#include <windows.h>
#undef GetCurrentDirectory
#endif

class StdIOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName )
	{
		return (int)g_pFullFileSystem->Open( pFileName, "rb" );
	}

	int read( void *pOutput, int size, int file )
	{
		if ( !file )
			return 0;

		return g_pFullFileSystem->Read( pOutput, size, (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		if ( !file )
			return;

		g_pFullFileSystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		if ( !file )
			return 0;

		return g_pFullFileSystem->Tell( (FileHandle_t)file );
	}

	unsigned int size( int file )
	{
		if ( !file )
			return 0;

		return g_pFullFileSystem->Size( (FileHandle_t)file );
	}

	void close( int file )
	{
		if ( !file )
			return;

		g_pFullFileSystem->Close( (FileHandle_t)file );
	}
};

class StdIOWriteBinary : public IFileWriteBinary
{
public:
	int create( const char *pFileName )
	{
		return (int)g_pFullFileSystem->Open( pFileName, "wb" );
	}

	int write( void *pData, int size, int file )
	{
		return g_pFullFileSystem->Write( pData, size, (FileHandle_t)file );
	}

	void close( int file )
	{
		g_pFullFileSystem->Close( (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		g_pFullFileSystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		return g_pFullFileSystem->Tell( (FileHandle_t)file );
	}
};

static StdIOReadBinary io_in;
static StdIOWriteBinary io_out;

#define RIFF_WAVE			MAKEID('W','A','V','E')
#define WAVE_FMT			MAKEID('f','m','t',' ')
#define WAVE_DATA			MAKEID('d','a','t','a')
#define WAVE_FACT			MAKEID('f','a','c','t')
#define WAVE_CUE			MAKEID('c','u','e',' ')


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &walk - 
//-----------------------------------------------------------------------------
static void SceneManager_ParseSentence( CSentence& sentence, IterateRIFF &walk )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.EnsureCapacity( walk.ChunkSize() );
	walk.ChunkRead( buf.Base() );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, walk.ChunkSize() );

	sentence.InitFromDataChunk( buf.Base(), buf.TellPut() );
}

bool SceneManager_LoadSentenceFromWavFileUsingIO( char const *wavfile, CSentence& sentence, IFileReadBinary& io )
{
	sentence.Reset();

	InFileRIFF riff( wavfile, io );

	// UNDONE: Don't use printf to handle errors
	if ( riff.RIFFName() != RIFF_WAVE )
	{
		return false;
	}

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	// This chunk must be first as it contains the wave's format
	// break out when we've parsed it
	bool found = false;
	while ( walk.ChunkAvailable() && !found )
	{
		switch ( walk.ChunkName() )
		{
		case WAVE_VALVEDATA:
		{
			found = true;
			SceneManager_ParseSentence( sentence, walk );
		}
			break;
		}
		walk.ChunkNext();
	}

	return found;
}

bool SceneManager_LoadSentenceFromWavFile( char const *wavfile, CSentence& sentence )
{
	return SceneManager_LoadSentenceFromWavFileUsingIO( wavfile, sentence, io_in );
}


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
class CSFMGenApp : public CTier3DmSteamApp
{
	typedef CTier3DmSteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void Destroy() {}

	void PrintHelp( );

private:
	struct SFMInfo_t
	{
		CUtlString m_GameSound;
		CUtlString m_Text;
		CUtlString m_DMXFileName;
	};

	struct SFMGenInfo_t
	{
		const char *m_pCSVFile;
		const char *m_pModelName;
		const char *m_pOutputDirectory;
		const char *m_pExportFacDirectory;
		bool m_bWritePhonemesInWavs;
		bool m_bUsePhonemesInWavs;
		float m_flSampleRateHz;
		float m_flSampleFilterSize;
		bool m_bGenerateSFMFiles;
		bool m_bExtractPhonemeFromWavsForMp3;
	};

	enum TokenRetVal_t
	{
		TOKEN_COMMA = 0,
		TOKEN_RETURN,
		TOKEN_COMMENT,
		TOKEN_EOF,
	};

	// Parses the excel .csv file
	TokenRetVal_t ParseToken( CUtlBuffer &buf, char *pToken, int nMaxTokenLen );

	// Parses the excel file
	void ParseCSVFile( CUtlBuffer &buf, CUtlVector< SFMInfo_t > &infoList, int nSkipLines );

	// The application object
	void GenerateSFMFiles( SFMGenInfo_t &info );

	// Makes the names unique
	void UniqueifyNames( CUtlVector< SFMInfo_t > &infoList );

	// Creates a single sfm file
	void GenerateSFMFile( const SFMGenInfo_t &sfmGenInfo, const SFMInfo_t &info, studiohdr_t *pStudioHdr, const char *pOutputDirectory, const char *pExportFacPath );

	// Saves an SFM file
	void SaveSFMFile( CDmElement *pRoot, const char *pRelativeScenePath, const char *pFileName );

	// Generates a sound clip for the game sound
	CDmeSoundClip *CreateSoundClip( CDmeFilmClip *pShot, const char *pAnimationSetName, const char *pGameSound, studiohdr_t *pStudioHdr, CDmeGameSound **ppGameSound );

	// Builds the list of all controls in the animation set contributing to facial animation
	void BuildFacialControlList( CDmeFilmClip *pShot, CDmeAnimationSet *pAnimationSet, CUtlVector< LogPreview_t > &list );
};


DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CSFMGenApp );

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CSFMGenApp::Create()
{
	SpewOutputFunc( SpewStdout );

	AppSystemInfo_t appSystems[] = 
	{
		{ "vstdlib.dll",			PROCESS_UTILS_INTERFACE_VERSION },
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
	SetupDefaultFlexController();
	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CSFMGenApp::PreInit( )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pDataModel )
	{
		Error( "// ERROR: sfmgen is missing a required interface!\n" );
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
void CSFMGenApp::PrintHelp( )
{
	Msg( "Usage: sfmgen -i <in .csv file> -m <.mdl relative path> -o <output dir>\n" );
	Msg( "				[-f <fac output dir>] [-p] [-w] [-r <sample rate in hz>] [-s <sample filter size>]\n" );
	Msg( "				[-nop4] [-vproject <path to gameinfo.txt>]\n" );
	Msg( "\t-i\t: Source .CSV file indicating game sound names, text for phoneme extraction, output sfm file names.\n" );
	Msg( "\t-m\t: Indicates the path of the .mdl file under game/mod/models/ to use in the sfm files.\n" );
	Msg( "\t-o\t: Indicates output directory to place generated files in.  Required if Generating SFM files.\n" );
	Msg( "\t-f\t: [Optional] Indicates that facial files should be created in the specified fac output dir.\n" );
	Msg( "\t-p\t: [Optional] Indicates the extracted phonemes should be written into the wav file.\n" );
	Msg( "\t-w\t: [Optional] Indicates the phonemes should be read from the wav file. Cannot also have -p specified.\n" );
	Msg( "\t-r\t: [Optional] Specifies the phoneme extraction sample rate (default = 20)\n" );
	Msg( "\t-s\t: [Optional] Specifies the phoneme extraction sample filter size (default = 0.08)\n" );
	Msg( "\t-nop4\t: Disables auto perforce checkout/add.\n" );
	Msg( "\t-nosfm\t: Disables generating of SFM files (dmx).\n" );
	Msg( "\t-vproject\t: Specifies path to a gameinfo.txt file (which mod to build for).\n" );
}


//-----------------------------------------------------------------------------
// Parses a token from the excel .csv file
//-----------------------------------------------------------------------------
CSFMGenApp::TokenRetVal_t CSFMGenApp::ParseToken( CUtlBuffer &buf, char *pToken, int nMaxTokenLen )
{
	*pToken = 0;

	if ( !buf.IsValid() )
		return TOKEN_EOF;

	int nLen = 0;
	char c = buf.GetChar();
	bool bIsQuoted = false;
	bool bIsComment = false;
	while ( true )
	{
		if ( c == '#' )
		{
			bIsComment = true;
		}

		if ( c == '"' )
		{
			bIsQuoted = !bIsQuoted;
		}
		else if ( ( c == ',' || c == '\n' ) && !bIsQuoted )
		{
			pToken[nLen] = 0;
			if ( bIsComment )
				return TOKEN_COMMENT;

			return ( c == '\n' ) ? TOKEN_RETURN : TOKEN_COMMA;
		}

		if ( nLen < nMaxTokenLen - 1 )
		{
			if ( c != '"' )
			{
				pToken[nLen++] = c;
			}
		}
		if ( !buf.IsValid() ) 
		{
			pToken[nLen] = 0;
			return TOKEN_EOF;
		}
		c = buf.GetChar();
	}

	// Should never get here
	return TOKEN_EOF;
}


//-----------------------------------------------------------------------------
// Parses the excel file
//-----------------------------------------------------------------------------
void CSFMGenApp::ParseCSVFile( CUtlBuffer &buf, CUtlVector< SFMInfo_t > &infoList, int nSkipLines )
{
	char pToken[512];
	for( int nLine = 0; buf.IsValid(); ++nLine )
	{
		SFMInfo_t info;
		TokenRetVal_t nTokenRetVal = ParseToken( buf, pToken, sizeof(pToken) );
		if ( nTokenRetVal == TOKEN_EOF )
			return;
		
		if ( nTokenRetVal == TOKEN_COMMENT )
		{
			continue;
		}

		if ( nTokenRetVal != TOKEN_COMMA )
		{
			Warning( "sfmgen: Missing Column at line %d\n", nLine );
			continue;
		}

		info.m_DMXFileName = pToken;

		if ( ParseToken( buf, pToken, sizeof(pToken) ) != TOKEN_COMMA )
		{
			Warning( "sfmgen: Missing Column at line %d\n", nLine );
			continue;
		}

		if ( ParseToken( buf, pToken, sizeof(pToken) ) != TOKEN_COMMA )
		{
			Warning( "sfmgen: Missing Column at line %d\n", nLine );
			continue;
		}

		info.m_GameSound = pToken;

		nTokenRetVal = ParseToken( buf, pToken, sizeof(pToken) );
		info.m_Text = pToken;
		if ( nTokenRetVal == TOKEN_COMMENT )
		{
			Warning( "sfmgen: No VO Transcript on line %d - Skipping \n", nLine );
			continue;
		}

		// extract the VO Transcript
		while ( nTokenRetVal == TOKEN_COMMA )
		{
			nTokenRetVal = ParseToken( buf, pToken, sizeof(pToken) );
		}

		if ( nSkipLines > nLine )
			continue;

		infoList.AddToTail( info );
	}
}


//-----------------------------------------------------------------------------
// Makes the names unique
//-----------------------------------------------------------------------------
void CSFMGenApp::UniqueifyNames( CUtlVector< SFMInfo_t > &infoList )
{
	CUtlStringMap<int> foundNames;
	int nCount = infoList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pName = infoList[i].m_DMXFileName;

		int nFoundCount; 
		if ( !foundNames.Defined( pName ) )
		{
			nFoundCount = foundNames[ pName ] = 1;
		}
		else
		{
			nFoundCount = ++foundNames[ pName ];
		}

		// Remove whitespace, change to lowercase, uniqueify.
		int nLen = Q_strlen(pName);
		int nDigits = (int)log10( (float)nFoundCount ) + 1;
		char *pTemp = (char*)_alloca( nLen + nDigits + 1 );
		for ( int j = 0; j < nLen; ++j )
		{
			if ( isspace( pName[j] ) )
			{
				pTemp[j] = '_';
			}
			else
			{
				pTemp[j] = tolower( pName[j] );
			}
		}
		if ( nFoundCount > 1 )
		{
			Q_snprintf( &pTemp[nLen], nDigits + 1, "%d", nFoundCount );
			nLen += nDigits;
		}
		pTemp[nLen] = 0;
		infoList[i].m_DMXFileName = pTemp;
	}
}


//-----------------------------------------------------------------------------
// Saves an SFM file
//-----------------------------------------------------------------------------
void CSFMGenApp::SaveSFMFile( CDmElement *pRoot, const char *pOutputDir, const char *pFileName )
{
	// Construct actual file name from the model name + dmx file name
	char pFullPathBuf[MAX_PATH];
	Q_snprintf( pFullPathBuf, sizeof(pFullPathBuf), "%s/%s.dmx", pOutputDir, pFileName );
	const char *pFullPath = pFullPathBuf;

	CP4AutoEditAddFile checkout( pFullPath );

	if ( !g_pDataModel->SaveToFile( pFullPath, NULL, g_pDataModel->GetDefaultEncoding( "sfm_session" ), "sfm_session", pRoot ) )
	{
		Warning( "sfmgen: Unable to write file %s\n", pFullPath );
		return;
	}

	Msg( "sfmgen: Wrote file %s\n", pFullPath );
}


//-----------------------------------------------------------------------------
// Generates a sound clip for the game sound
//-----------------------------------------------------------------------------
CDmeSoundClip *CSFMGenApp::CreateSoundClip( CDmeFilmClip *pShot, const char *pAnimationSetName, const char *pGameSound, studiohdr_t *pStudioHdr, CDmeGameSound **ppGameSound )
{
	*ppGameSound = NULL;

	CDmeTrackGroup *pTrackGroup = pShot->FindOrAddTrackGroup( "audio" );
	CDmeTrack *pTrack = pTrackGroup->FindOrAddTrack( pAnimationSetName, DMECLIP_SOUND );

	// Get the gender for the model
	gender_t actorGender = g_pSoundEmitterSystem->GetActorGender( pStudioHdr->pszName() );

	// Get the wav file for the gamesound.
	CSoundParameters params;
	if ( !g_pSoundEmitterSystem->GetParametersForSound( pGameSound, params, actorGender ) )
	{
		Warning( "Unable to determine .wav file for gamesound %s!\n", pGameSound );
		return NULL;
	}

	// Get the sound duration
	char pFullPath[MAX_PATH];
	char pRelativePath[MAX_PATH];
	const char *pWavFile = PSkipSoundChars( params.soundname );
	Q_ComposeFileName( "sound", pWavFile, pRelativePath, sizeof(pRelativePath) );
	g_pFullFileSystem->RelativePathToFullPath( pRelativePath, "GAME", pFullPath, sizeof(pFullPath) );
	DmeTime_t duration( GetWavSoundDuration( pFullPath ) );

	CDmeGameSound *pDmeSound = CreateElement< CDmeGameSound >( pGameSound, pTrack->GetFileId() );
	Assert( pDmeSound );
	pDmeSound->m_GameSoundName = pGameSound;
	pDmeSound->m_SoundName	= params.soundname;
	pDmeSound->m_Volume		= params.volume;
	pDmeSound->m_Level		= params.soundlevel;
	pDmeSound->m_Pitch		= params.pitch;
	pDmeSound->m_IsStatic	= true;
	pDmeSound->m_Channel	= CHAN_STATIC;
	pDmeSound->m_Flags		= 0;
	pDmeSound->m_Origin		= vec3_origin;
	pDmeSound->m_Direction	= vec3_origin;

	CDmeSoundClip *pSubClip = CreateElement< CDmeSoundClip >( pGameSound, pTrack->GetFileId() );
	pSubClip->m_Sound = pDmeSound;
	pSubClip->SetStartTime( DMETIME_ZERO );
	pSubClip->SetTimeOffset( DmeTime_t( -params.delay_msec / 1000.0f ) );
	pSubClip->SetDuration( duration );

	pTrack->AddClip( pSubClip );
	*ppGameSound = pDmeSound;

	return pSubClip;
}


//-----------------------------------------------------------------------------
// Builds the list of all controls in the animation set contributing to facial animation
//-----------------------------------------------------------------------------
void CSFMGenApp::BuildFacialControlList( CDmeFilmClip *pShot, CDmeAnimationSet *pAnimationSet, CUtlVector< LogPreview_t > &list )
{
	const CDmaElementArray< CDmElement > &controls = pAnimationSet->GetControls();
	int nCount = controls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pControl = controls[i];
		if ( !pControl || pControl->GetValue<bool>( "transform" ) )
			continue;

		LogPreview_t preview;
		preview.m_hControl = pControl;
		preview.m_hShot	= pShot;
		preview.m_bActiveLog = preview.m_bSelected = false;

		if ( pControl->GetValue< bool >( "combo" ) )
		{
			preview.m_hChannels[ LOG_PREVIEW_VALUE ] = pControl->GetValueElement< CDmeChannel >( "valuechannel" );
			preview.m_hChannels[ LOG_PREVIEW_BALANCE ] = pControl->GetValueElement< CDmeChannel >( "balancechannel" );
		}
		else
		{
			preview.m_hChannels[ LOG_PREVIEW_VALUE ] = pControl->GetValueElement< CDmeChannel >( "channel" );
			preview.m_hChannels[ LOG_PREVIEW_BALANCE ] = NULL;
		}

		if ( pControl->GetValue< bool >( "multi" ) )
		{
			preview.m_hChannels[ LOG_PREVIEW_MULTILEVEL ] = pControl->GetValueElement< CDmeChannel >( "multilevelchannel" );
		}
		else
		{
			preview.m_hChannels[ LOG_PREVIEW_MULTILEVEL ] = NULL;
		}

		preview.m_hOwner = preview.m_hChannels[ LOG_PREVIEW_VALUE ]->FindOwnerClipForChannel( pShot );

		list.AddToTail( preview );
	}
}


//-----------------------------------------------------------------------------
// Creates a single sfm file
//-----------------------------------------------------------------------------
void CSFMGenApp::GenerateSFMFile( const SFMGenInfo_t& sfmGenInfo, const SFMInfo_t &info, 
	studiohdr_t *pStudioHdr, const char *pOutputDirectory, const char *pExportFacPath )
{
	CSFMSession session;
	session.Init();

	char pAnimationSetName[256];
	Q_FileBase( pStudioHdr->pszName(), pAnimationSetName, sizeof(pAnimationSetName) );

	// Set the file id(	necessary for phoneme extraction)
	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( pAnimationSetName );
	session.Root()->SetFileId( fileid, TD_DEEP );
	g_pModelPresetGroupMgr->AssociatePresetsWithFile( fileid );

	// Get the shot.
	CDmeFilmClip *pMovie = session.Root()->GetValueElement<CDmeFilmClip>( "activeClip" );
	CDmeFilmClip* pShot = CastElement< CDmeFilmClip >( pMovie->GetFilmTrack()->GetClip( 0 ) );

	// Create a camera for the shot
	DmeCameraParams_t cameraParams( pAnimationSetName, vec3_origin, QAngle( 0, 180, 0 ) );
	cameraParams.origin.x = pStudioHdr->hull_max.x + 20;
	cameraParams.origin.z = Lerp( 0.95f, pStudioHdr->hull_min.z, pStudioHdr->hull_max.z );
	cameraParams.fov = 75.0f;
	CDmeCamera* pCamera = session.CreateCamera( cameraParams );
	pShot->SetCamera( pCamera );

	// Create a game model for the studio hdr
	CDmeGameModel *pGameModel = session.CreateEditorGameModel( pStudioHdr, vec3_origin, Quaternion( 0, 0, 0, 1 ) );

	// Create a scene for the shot
	CDmeDag *pScene = session.FindOrCreateScene( pShot, pAnimationSetName );
	pScene->AddChild( pGameModel );

	// Create a sound clip
	CDmeGameSound *pGameSound;
	CDmeSoundClip *pSoundClip = CreateSoundClip( pMovie, pAnimationSetName, info.m_GameSound, pStudioHdr, &pGameSound );

	if ( pSoundClip )
	{
		pShot->SetDuration( pSoundClip->GetDuration() );
		pMovie->SetDuration( pSoundClip->GetDuration() );

		// Create an animation set
		CDmeAnimationSet *pAnimationSet = CreateAnimationSet( pMovie, pShot, pGameModel, pAnimationSetName, 0, false );

		// Extract phonemes
		CExtractInfo extractInfo;
		extractInfo.m_pClip = pSoundClip;
		extractInfo.m_pSound = pGameSound;
		extractInfo.m_sHintText = info.m_Text;
		extractInfo.m_bUseSentence = sfmGenInfo.m_bUsePhonemesInWavs;

		ExtractDesc_t extractDesc;
		extractDesc.m_nExtractType = EXTRACT_WIPE_CLIP;
		extractDesc.m_pMovie = pMovie;
		extractDesc.m_pShot = pShot;
		extractDesc.m_pSet = pAnimationSet;
		extractDesc.m_flSampleRateHz = sfmGenInfo.m_flSampleRateHz;
		extractDesc.m_flSampleFilterSize = sfmGenInfo.m_flSampleFilterSize;
		extractDesc.m_WorkList.AddToTail( extractInfo );
		BuildFacialControlList( pShot, pAnimationSet, extractDesc.m_ControlList );
		sfm_phonemeextractor->Extract( SPEECH_API_LIPSINC, extractDesc, sfmGenInfo.m_bWritePhonemesInWavs );

		CExtractInfo &results = extractDesc.m_WorkList[ 0 ];
		CDmElement *pExtractionSettings = pGameSound->FindOrAddPhonemeExtractionSettings();
		pExtractionSettings->SetValue< float >( "duration", results.m_flDuration );
		// Store off phonemes
		if ( !pExtractionSettings->HasAttribute( "results" ) )
		{
			pExtractionSettings->AddAttribute( "results", AT_ELEMENT_ARRAY );
		}

		CDmrElementArray< CDmElement > resultsArray( pExtractionSettings, "results" );
		resultsArray.RemoveAll();
		for ( int i = 0; i < results.m_ApplyTags.Count(); ++i )
		{
			CBasePhonemeTag *tag = results.m_ApplyTags[ i ];
			CDmElement *tagElement = CreateElement< CDmElement >( ConvertPhoneme( tag->GetPhonemeCode() ), pGameSound->GetFileId() );
			tagElement->SetValue< float >( "start", tag->GetStartTime() );
			tagElement->SetValue< float >( "end", tag->GetEndTime() );
			resultsArray.AddToTail( tagElement );
		}

		if ( sfmGenInfo.m_bGenerateSFMFiles && pExportFacPath )
		{
			char pFACFileName[MAX_PATH];
			Q_ComposeFileName( pExportFacPath, info.m_DMXFileName, pFACFileName, sizeof(pFACFileName) );
			Q_SetExtension( pFACFileName, ".dmx", sizeof(pFACFileName) ); 
			ExportFacialAnimation( pFACFileName, pMovie, pShot, pAnimationSet );
		}
	}
	
	if ( sfmGenInfo.m_bGenerateSFMFiles )
	{
		SaveSFMFile( session.Root(), pOutputDirectory, info.m_DMXFileName );
	}

	session.Shutdown();
}


//-----------------------------------------------------------------------------
// Computes a full directory
//-----------------------------------------------------------------------------
static void ComputeFullPath( const char *pRelativeDir, char *pFullPath, int nBufLen )
{
	if ( !Q_IsAbsolutePath( pRelativeDir ) )
	{
		char pDir[MAX_PATH];
		if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof(pDir) ) )
		{
			Q_ComposeFileName( pDir, pRelativeDir, pFullPath, nBufLen );
		}
	}
	else
	{
		Q_strncpy( pFullPath, pRelativeDir, nBufLen );
	}

	Q_StripTrailingSlash( pFullPath );

	// Ensure the output directory exists
	g_pFullFileSystem->CreateDirHierarchy( pFullPath );
}



//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
void CSFMGenApp::GenerateSFMFiles( SFMGenInfo_t& info )
{
	char pRelativeModelPath[MAX_PATH];
	Q_ComposeFileName( "models", info.m_pModelName, pRelativeModelPath, sizeof(pRelativeModelPath) );
	Q_SetExtension( pRelativeModelPath, ".mdl", sizeof(pRelativeModelPath) );
	MDLHandle_t hMDL = g_pMDLCache->FindMDL( pRelativeModelPath );
	if ( hMDL == MDLHANDLE_INVALID )
	{
		Warning( "sfmgen: Model %s doesn't exist!\n", pRelativeModelPath );
		return;
	}

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( hMDL );
	if ( !pStudioHdr || g_pMDLCache->IsErrorModel( hMDL ) )
	{
		Warning( "sfmgen: Model %s doesn't exist!\n", pRelativeModelPath );
		return;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( info.m_pCSVFile, NULL, buf ) )
	{
		Warning( "sfmgen: Unable to load file %s\n", info.m_pCSVFile );
		return;
	}

	CUtlVector< SFMInfo_t > infoList;
	ParseCSVFile( buf, infoList, 1 );

	int nCount = infoList.Count();
	if ( nCount == 0 )
	{
		Warning( "sfmgen: no files to create!\n" );
		return;
	}

	UniqueifyNames( infoList );

	// Construct full path to the output directories
	char pFullPath[MAX_PATH];
	char pFullFacPathBuf[MAX_PATH];
	const char *pExportFacPath = NULL;
	if ( info.m_pExportFacDirectory )
	{
		ComputeFullPath( info.m_pExportFacDirectory, pFullFacPathBuf, sizeof( pFullFacPathBuf ) );
		pExportFacPath = pFullFacPathBuf;
	}

	if ( info.m_pOutputDirectory )
	{
		ComputeFullPath( info.m_pOutputDirectory, pFullPath, sizeof( pFullPath ) );
	}
	else
	{
		pFullPath[0] = '\0';
	}


	if (!info.m_bExtractPhonemeFromWavsForMp3)
	{
		for (int i = 0; i < nCount; ++i)
		{
			GenerateSFMFile(info, infoList[i], pStudioHdr, pFullPath, pExportFacPath);
		}
	}
	else
	{
		// Extra Phoneme Data from .wav files for .mp3
		CUtlBuffer bufOutput(0, 0, CUtlBuffer::TEXT_BUFFER);		// Phoneme outout
		CUtlBuffer bufFilenames(0, 0, CUtlBuffer::TEXT_BUFFER);	// Affected Files for reference (files with Phoneme data)
		for (int i = 0; i < nCount; ++i)
		{
			CSentence sSentence;
			if (SceneManager_LoadSentenceFromWavFile(infoList[i].m_GameSound, sSentence))
			{
				// Save Phoneme Data
				CUtlString strTemp(infoList[i].m_DMXFileName);
				//	//bufOutput.Printf( "\n" );
				bufOutput.Printf(strTemp.Replace(".wav", ".mp3").Get());
				bufOutput.Printf("\n{\n");
				sSentence.SaveToBuffer(bufOutput);
				bufOutput.Printf("}\n\n");

				// Save file name
				bufFilenames.Printf(infoList[i].m_GameSound);
				bufFilenames.Printf("\n");

				Msg("%s\n", infoList[i].m_GameSound.Get());
			}
		}

		// output
		//if ( sfm_phonemeextractor->GetSentence( pGameSound, sSentence ) )
		{
			// write this to a text file
			FileHandle_t fh = g_pFullFileSystem->Open("tf_PhonemeData.txt", "wb");
			if (fh)
			{
				g_pFullFileSystem->Write(bufOutput.Base(), bufOutput.TellPut(), fh);
				g_pFullFileSystem->Close(fh);
			}
		}

		{
			// write this to a text file
			FileHandle_t fh = g_pFullFileSystem->Open("tf_PhonemeFiles.txt", "wb");
			if (fh)
			{
				g_pFullFileSystem->Write(bufFilenames.Base(), bufFilenames.TellPut(), fh);
				g_pFullFileSystem->Close(fh);
			}
		}
	}
}
//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CSFMGenApp::Main()
{
	g_pDataModel->SetUndoEnabled( false );

	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	if ( CommandLine()->CheckParm( "-h" ) || CommandLine()->CheckParm( "-help" ) )
	{
		PrintHelp();
		return 0;
	}

	// Do Perforce Stuff
	if ( CommandLine()->FindParm( "-nop4" ) )
	{
		g_p4factory->SetDummyMode( true );
	}

	g_p4factory->SetOpenFileChangeList( "Automatically Generated SFM files" );

	SFMGenInfo_t info;
	info.m_pCSVFile = CommandLine()->ParmValue( "-i" );
	info.m_pModelName = CommandLine()->ParmValue( "-m" );
	info.m_pOutputDirectory = CommandLine()->ParmValue( "-o" );
	info.m_pExportFacDirectory = CommandLine()->ParmValue( "-f" );
	info.m_bWritePhonemesInWavs = CommandLine()->FindParm( "-p" ) != 0;
	info.m_bUsePhonemesInWavs = CommandLine()->FindParm( "-w" ) != 0;
	info.m_flSampleRateHz = CommandLine()->ParmValue( "-r", 20.0f );
	info.m_flSampleFilterSize = CommandLine()->ParmValue( "-s", 0.08f );
	info.m_bGenerateSFMFiles = CommandLine()->FindParm( "-nosfm" ) == 0;
	info.m_bExtractPhonemeFromWavsForMp3 = CommandLine()->FindParm("-mp3");

	if ( !info.m_pCSVFile || !info.m_pModelName )
	{
		PrintHelp();
		return 0;
	}

	if ( !info.m_pOutputDirectory && info.m_bGenerateSFMFiles )
	{
		PrintHelp();
		return 0;
	}

	if ( info.m_bUsePhonemesInWavs && info.m_bWritePhonemesInWavs )
	{
		Warning( "Cannot simultaneously read the phones from wavs and also write them into the wavs!\n" );
		return 0;
	}

	if ( info.m_pExportFacDirectory && !Q_stricmp( info.m_pExportFacDirectory, info.m_pOutputDirectory ) )
	{
		Warning( "Must specify different directories for output + facial export!\n" );
		return 0;
	}

	g_pSoundEmitterSystem->ModInit();
	sfm_phonemeextractor->Init();
	GenerateSFMFiles( info );
	sfm_phonemeextractor->Shutdown();
	g_pSoundEmitterSystem->ModShutdown();
	return -1;
}