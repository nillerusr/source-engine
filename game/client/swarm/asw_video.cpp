#include "cbase.h"
#include "asw_video.h"																																																				    
#include "engine/ienginesound.h"
#include "asw_marine_profile.h"
#include "c_asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define BIK_MEDIA_FOLDER "media/"
#define BIK_EXTENTION ".bik"


const char *( g_szBIKVideoFaceCharacters[ ASW_VOICE_TYPE_TOTAL ] ) =
{
	"kava",	// SARGE
	"kava",	// JAEGER
	"kava",	// WILDCAT
	"kava",	// WOLFE
	"kava",	// FAITH
	"kava",	// BASTILLE
	"kava",	// CRASH
	"kava",	// FLYNN
	"kava",	// VEGAS
};

const char *( g_szBIKVideoFaces[ ASW_VIDEO_FACE_TYPE_TOTAL ] ) =
{
	"static",
	"healthy",
	"healthy_alt00",
	"needHealth",
	"pain",
};


CASW_Video_Face_BIKHandles CASW_Video::s_VideoFaceBIKHandles[ MAX_SPLITSCREEN_PLAYERS ];

CASW_Video_Face_BIKHandles::CASW_Video_Face_BIKHandles( void )
{
	m_bInitialized = false;
	m_nBufferCount = 0;

	for ( int i = 0; i < ASW_VIDEO_FACE_TYPE_TOTAL; ++i )
	{
		m_BIKHandles[ i ] = BIKHANDLE_INVALID;
	}
}

CASW_Video_Face_BIKHandles::~CASW_Video_Face_BIKHandles( void )
{
	Shutdown();
}

void CASW_Video_Face_BIKHandles::Init( int nCharacterVoiceType )
{
	Shutdown();

#if !defined( _X360 ) || defined( BINK_ENABLED_FOR_X360 )
	char szMaterialName[ FILENAME_MAX ];
	char szFileName[ FILENAME_MAX ];

	for ( int i = 0; i < ASW_VIDEO_FACE_TYPE_TOTAL; ++i )
	{
		Q_snprintf( szMaterialName, sizeof( szMaterialName ), "VideoBIKMaterial%i", g_pBIK->GetGlobalMaterialAllocationNumber() );
		Q_snprintf( szFileName, sizeof( szFileName ), BIK_MEDIA_FOLDER "%s_%s" BIK_EXTENTION, g_szBIKVideoFaceCharacters[ nCharacterVoiceType ], g_szBIKVideoFaces[ i ] );

		m_BIKHandles[ i ] = bik->CreateMaterial( szMaterialName, szFileName, "GAME", BIK_NO_AUDIO );
		Assert( m_BIKHandles[ i ] != BIKHANDLE_INVALID );

		bik->Update( m_BIKHandles[ i ] );
	}
#endif

	m_bInitialized = true;
}

void CASW_Video_Face_BIKHandles::Shutdown( void )
{
	if ( !m_bInitialized )
		return;

#if !defined( _X360 ) || defined( BINK_ENABLED_FOR_X360 )
	for ( int i = 0; i < ASW_VIDEO_FACE_TYPE_TOTAL; ++i )
	{
		// Shut down this video
		if ( m_BIKHandles[ i ] != BIKHANDLE_INVALID )
		{
			bik->DestroyMaterial( m_BIKHandles[ i ] );
			m_BIKHandles[ i ] = BIKHANDLE_INVALID;
		}
	}
#endif

	m_bInitialized = false;
	m_nBufferCount = 0;
}


void CASW_Video_Face_BIKHandles::Buffer( void )
{
	if ( m_nBufferCount > 3 || !m_bInitialized )
		return;

#if !defined( _X360 ) || defined( BINK_ENABLED_FOR_X360 )
	for ( int i = 0; i < ASW_VIDEO_FACE_TYPE_TOTAL; ++i )
	{
		if ( m_BIKHandles[ i ] != BIKHANDLE_INVALID )
		{
			bik->Update( m_BIKHandles[ i ] );
		}
	}
#endif

	m_nBufferCount++;
}

BIKMaterial_t CASW_Video_Face_BIKHandles::GetBIKHandle( int nFaceType ) const
{
	if ( !m_bInitialized )
	{
		return BIKHANDLE_INVALID;
	}

	return m_BIKHandles[ nFaceType ];
}


CASW_Video::CASW_Video() :
	m_nPlaybackWidth( 0 ),
	m_nPlaybackHeight( 0 ),
	m_bAllowInterruption( true ),
	m_bStarted( false )
{

	m_bAllowInterruption = false;

	m_nLoopVideo = ASW_VIDEO_FACE_STATIC;
	m_nLastTempVideo = ASW_VIDEO_FACE_STATIC;
	m_nTransitionVideo = ASW_VIDEO_FACE_STATIC;

	m_nNumLoopAlternatives = 0;
	m_fAlternateChance = 1.0f;
	m_bIsLoopVideo = true;
	m_bIsTransition = false;
}

CASW_Video::~CASW_Video()
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		s_VideoFaceBIKHandles[ i ].Shutdown();
	}
}

void CASW_Video::OnVideoOver()
{
	if ( m_bIsTransition )
	{
		m_bIsTransition = false;
		BeginPlayback( m_nLastTempVideo );
	}
	else if ( !m_bIsLoopVideo )
	{
		m_bIsLoopVideo = true;
		BeginPlayback( m_nLoopVideo );
	}
	else if ( m_nNumLoopAlternatives > 0 && RandomFloat() < m_fAlternateChance )
	{
		PlayTempVideo( m_nLoopVideo + RandomInt( 1, m_nNumLoopAlternatives ) );
	}
	else
	{
		bik->SetFrame( GetVideoFaceBIKHandles()->GetBIKHandle( m_nLoopVideo ), 0.0f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Begins playback of a movie
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CASW_Video::BeginPlayback( int nFaceType )
{
#if !defined( _X360 ) || defined( BINK_ENABLED_FOR_X360 )
	// Load and create our BINK video
	CASW_Video_Face_BIKHandles *pHandles = GetVideoFaceBIKHandles();
	if ( !pHandles->IsInitialized() )
	{
		C_ASW_Marine *pMarine = C_ASW_Marine::GetLocalMarine();
		if ( !pMarine )
		{
			return false;
		}

		CASW_Marine_Profile *pProfile = pMarine->GetMarineProfile();
		if ( !pProfile )
		{
			return false;
		}

		pHandles->Init( pProfile->m_VoiceType );
	}

	m_bStarted = true;

	bik->GetTexCoordRange( pHandles->GetBIKHandle( nFaceType ), &m_flU, &m_flV );

	bik->SetFrame( pHandles->GetBIKHandle( nFaceType ), 0.0f );

	return true;
#else
	return false;
#endif
}

void CASW_Video::Update()
{
#if !defined( _X360 ) || defined( BINK_ENABLED_FOR_X360 )
	if ( !m_bStarted )
	{
		BeginPlayback( m_nLoopVideo );
	}

	// Update our frame
	GetVideoFaceBIKHandles()->Buffer();

	if ( bik->Update( GetVideoFaceBIKHandles()->GetBIKHandle( GetCurrentVideo() ) ) == false )
	{
		// Issue a close command
		OnVideoOver();
	}
#endif
}


void CASW_Video::ReturnToLoopVideo( void )
{
	if ( !m_bIsLoopVideo )
	{
		m_bIsLoopVideo = true;
		BeginPlayback( m_nLoopVideo );
	} 
}

void CASW_Video::PlayTempVideo( int nFaceType, int nTransitionFaceType /*= -1*/ )
{
	if ( GetCurrentVideo() == nFaceType )
	{
		return;
	}

	m_nLastTempVideo = nFaceType;
	m_nTransitionVideo = nTransitionFaceType;

	m_bIsLoopVideo = false;

	m_bIsTransition = ( m_nTransitionVideo != -1 );

	BeginPlayback( m_bIsTransition ? m_nTransitionVideo : m_nLastTempVideo );
}

void CASW_Video::SetLoopVideo( int nFaceType, int nNumLoopAlternatives /*= 0*/, float fAlternateChance /*= 1.0f*/ )
{
	m_nNumLoopAlternatives = nNumLoopAlternatives;
	m_fAlternateChance = fAlternateChance;

	if ( m_nLoopVideo == nFaceType )
	{
		return;
	}

	m_nLoopVideo = nFaceType;

	if ( m_bIsLoopVideo && m_bStarted )
	{
		BeginPlayback( m_nLoopVideo );
	}
}

int CASW_Video::GetCurrentVideo( void ) const
{
	if ( m_bIsTransition )
	{
		return GetTransitionVideo();
	}

	return ( m_bIsLoopVideo ? GetLoopVideo() : GetLastTempVideo() );
}

IMaterial* CASW_Video::GetMaterial()
{
#if !defined( _X360 ) || defined( BINK_ENABLED_FOR_X360 )
	return bik->GetMaterial( GetVideoFaceBIKHandles()->GetBIKHandle( GetCurrentVideo() ) );
#else
	return NULL;
#endif
}

CASW_Video_Face_BIKHandles* CASW_Video::GetVideoFaceBIKHandles( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &( s_VideoFaceBIKHandles[ GET_ACTIVE_SPLITSCREEN_SLOT() ] );
}
