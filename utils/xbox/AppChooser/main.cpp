//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: X360 Application Chooser
//
//===========================================================================//

#if !defined( _X360 )
#include <windows.h>
#endif
#include "appframework/iappsystemgroup.h"
#include "appframework/appframework.h"
#include "tier0/dbg.h"
#include "tier1/interface.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"
#include "vstdlib/cvar.h"
#include "filesystem_init.h"
#include "tier1/utlbuffer.h"
#include "icommandline.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "studio.h"
#include "utlbuffer.h"
#include "tier2/utlstreambuffer.h"
#include "tier2/tier2.h"
#include "appframework/tier3app.h"
#include "mathlib/mathlib.h"
#include "inputsystem/iinputsystem.h"
#include "vphysics_interface.h"
#include "istudiorender.h"
#include "studio.h"
#include "vgui/IVGui.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "matsys_controls/matsyscontrols.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/imagepanel.h"
#include "vgui_controls/AnimationController.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/ishaderapi.h"
#include "materialsystem/itexture.h"
#include "filesystem/IQueuedLoader.h"
#include "xwvfile.h"
#if defined( _X360 )
#include "hl2orange.spa.h"
#include "xbox/xbox_console.h"
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_launch.h"
#include <xaudio.h>
#include <xmedia.h>
#include <xmp.h>
#endif
#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif
#include "tier0/memdbgon.h"

#define INACTIVITY_TIMEOUT	120

#define MOVIE_PATH			"d:\\movies"

bool g_bActive = true;

// user background music can force this to be zero
static float g_TargetMovieVolume = 1.0f;

extern SpewOutputFunc_t g_DefaultSpewFunc;

typedef void (*MovieEndCallback_t)();

struct game_t
{
	const wchar_t	*pName;
	const char		*pGameDir;
	bool			bEnabled;
};

game_t g_Games[] = 
{
	{ L"HALF-LIFE 2",					"hl2",		true },
	{ L"HALF-LIFE 2:\nEPISODE ONE",		"episodic",	true },
	{ L" HALF-LIFE 2:\nEPISODE TWO",	"ep2",		true },
	{ L"PORTAL",						"portal",	true },
	{ L"   TEAM\nFORTRESS 2",		    "tf",		true },
};

struct StartupMovie_t
{
	const char *pMovieName;
	bool		bUserCanSkip;
};

// played in order on initial startup
StartupMovie_t g_StartupMovies[] =
{
	{ "valve_legalese.wmv",	false },
};

const char *g_DemoMovies[] =
{
	"demo.wmv",
	"teaser_l4d.wmv",
};

class CImage : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CImage, vgui::Panel );
public:
	CImage( vgui::Panel *pParent, const char *pName, const char *pImageName ) : BaseClass( pParent, pName ) 
	{
		m_imageID = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_imageID, pImageName, false, false );	
		m_Color = Color( 255, 255, 255, 255 );
		SetPaintBackgroundEnabled( true );
		m_AspectRatio = 1.0f;
		m_bLetterbox = false;
	}

	CImage( vgui::Panel *pParent, const char *pName, IMaterial *pMaterial ) : BaseClass( pParent, pName ) 
	{
		m_imageID = vgui::surface()->CreateNewTextureID();
		g_pMatSystemSurface->DrawSetTextureMaterial( m_imageID, pMaterial );	
		m_Color = Color( 255, 255, 255, 255 );
		SetPaintBackgroundEnabled( true );
		m_AspectRatio = 1.0f;
		m_bLetterbox = false;
	}

	void SetAspectRatio( float aspectRatio, bool bLetterbox )
	{
		if ( aspectRatio > 1.0f )
		{
			m_AspectRatio = aspectRatio;
			m_bLetterbox = bLetterbox;
		}
	}

	void SetColor( int r, int g, int b )
	{
		// set rgb only
		m_Color.SetColor( r, g, b, m_Color.a() );
	}

	void SetAlpha( int alpha )
	{
		// set alpha only
		alpha = clamp( alpha, 0, 255 );
		m_Color.SetColor( m_Color.r(), m_Color.g(), m_Color.b(), alpha );
	}

	int GetAlpha() const
	{
		return m_Color.a();
	}

	virtual void PaintBackground( void )
	{
		if ( m_Color.a() != 0 )
		{
			int panelWidth, panelHeight;
			GetSize( panelWidth, panelHeight );
	
			float s0 = 0.0f;
			float s1 = 1.0f;
			int y = 0;
			if ( m_AspectRatio > 1.0f )
			{
				if ( m_bLetterbox )
				{
					// horizontal letterbox
					float adjustedHeight = (float)panelWidth / m_AspectRatio;
					float bandHeight = ( (float)panelHeight - adjustedHeight ) / 2;

					vgui::surface()->DrawSetColor( Color( 0, 0, 0, m_Color.a() ) );
					vgui::surface()->DrawFilledRect( 0, 0, panelWidth, bandHeight );
					vgui::surface()->DrawFilledRect( 0, panelHeight - bandHeight, panelWidth, panelHeight );

					y = bandHeight;
					panelHeight = adjustedHeight;
				}
				else
				{
					// hold the panel's height constant, determine the corresponding aspect corrected image width
					float imageWidth = (float)panelHeight * m_AspectRatio;
					// adjust the image width as a percentage of the panel's width
					// scale and center;
					s1 = (float)panelWidth / imageWidth;
					s0 = ( 1 - s1 ) / 2.0f;
					s1 = s0 + s1;
				}
			}

			vgui::surface()->DrawSetColor( m_Color );
			vgui::surface()->DrawSetTexture( m_imageID );
			vgui::surface()->DrawTexturedSubRect( 0, y, panelWidth, y+panelHeight, s0, 0.0f, s1, 1.0f );
		}
	}

	int		m_imageID;
	Color	m_Color;
	float	m_AspectRatio;
	bool	m_bLetterbox;
};

class CMovieImage : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CMovieImage, vgui::Panel );
public:

	CMovieImage( vgui::Panel *pParent, const char *pName, const char *pUniqueName, int nDecodeWidth, int nDecodeHeight, float fadeDelay = 1.0f ) : BaseClass( pParent, pName ) 
	{
		// decoupled from actual panel bounds, can be decoded at any power of two resolution
		m_nDecodeWidth = nDecodeWidth;
		m_nDecodeHeight = nDecodeHeight;

		char materialName[MAX_PATH];
		V_snprintf( materialName, sizeof( materialName ), "MovieMaterial_%s.vmt", pUniqueName );
		char textureName[MAX_PATH];
		V_snprintf( textureName, sizeof( textureName ), "MovieFrame_%s", pUniqueName );

		// create a texture for use as movie frame grab
		m_pTexture = g_pMaterialSystem->CreateProceduralTexture( 
			textureName, 
			TEXTURE_GROUP_OTHER,
			m_nDecodeWidth,
			m_nDecodeHeight,
			g_pMaterialSystem->GetBackBufferFormat(),
			TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY );

		KeyValues *pVMTKeyValues = new KeyValues( "screenspace_general" );
		pVMTKeyValues->SetInt( "$ignorez", 1 );
		pVMTKeyValues->SetInt( "$linearread_basetexture", 1 );
		pVMTKeyValues->SetInt( "$X360APPCHOOSER", 1 );
		pVMTKeyValues->SetString( "$PIXSHADER", "appchooser360movie_ps20b" );
		pVMTKeyValues->SetString( "$basetexture", textureName );

		m_pMaterial = g_pMaterialSystem->CreateMaterial( materialName, pVMTKeyValues );

		m_imageID = vgui::surface()->CreateNewTextureID();
		g_pMatSystemSurface->DrawSetTextureMaterial( m_imageID, m_pMaterial );

		SetPaintBackgroundEnabled( true );

		// initially invisible
		m_Color = Color( 255, 255, 255, 0 );

		m_FadeDelay = fadeDelay;
		m_pXMVPlayer = NULL;
		m_bStopping = false;
		m_StartTime = 0;
		m_StopTime = 0;
		m_pMovieEndCallback = NULL;
		m_bLooped = false;
		m_AspectRatio = 1.0f;
		m_bLetterbox = false;
	}

	void SetAspectRatio( float aspectRatio, bool bLetterbox )
	{
		if ( aspectRatio > 1.0f )
		{
			m_AspectRatio = aspectRatio;
			m_bLetterbox = bLetterbox;
		}
	}

	void SetColor( int r, int g, int b )
	{
		// set rgb only
		m_Color.SetColor( r, g, b, m_Color.a() );
	}

	// -1 means never have any audio, otherwise set to current target
	void InitUserAudioMix( bool bAudio )
	{
		m_CurrentVolume = bAudio ? g_TargetMovieVolume : -1;
	}

	// fade in/out based on user interaction with dashboard music system
	void UpdateMovieVolume( bool bForce, float frametime = 1.0f )
	{
		// m_CurrentVolume < 0 means this movie never plays audio
		if ( !m_pXMVPlayer || m_CurrentVolume < 0 )
			return;

		// forced update or new volume, ramp & set
		if ( bForce || g_TargetMovieVolume != m_CurrentVolume )
		{
			if ( bForce )
			{
				frametime = 1.0f;
			}

			m_CurrentVolume = Approach( g_TargetMovieVolume, m_CurrentVolume, frametime * 0.5f );
			
			// UNDONE: Under what conditions can this fail?  If it fails it could cause audible pops
			IXAudioSourceVoice *pVoice = NULL;
			HRESULT hr = m_pXMVPlayer->GetSourceVoice( &pVoice );
			if ( !FAILED( hr ) && pVoice )
			{
				pVoice->SetVolume( m_CurrentVolume );
			}
		}
	}

	bool StartMovieFromMemory( const void *pBuffer, int bufferSize, bool bAudio, bool bLoop, MovieEndCallback_t pMovieEndCallback = NULL )
	{
		if ( m_pXMVPlayer || m_bStopping )
		{
			// already started or currently stopping
			return false;
		}

		if ( !pBuffer || !bufferSize )
		{
			return false;
		}

		XMEDIA_XMV_CREATE_PARAMETERS xmvParameters;
		V_memset( &xmvParameters, 0, sizeof( xmvParameters ) );

		xmvParameters.dwFlags = XMEDIA_CREATE_CPU_AFFINITY;
		if ( bLoop )
		{
			xmvParameters.dwFlags |= XMEDIA_CREATE_FOR_LOOP;
			m_bLooped = true;
		}
		if ( !bAudio )
		{
			xmvParameters.dwAudioStreamId = (DWORD)XMEDIA_STREAM_ID_DONT_USE;
		}
		InitUserAudioMix(bAudio);

		xmvParameters.dwVideoDecoderCpu = 2;
		xmvParameters.dwVideoRendererCpu = 2;
		xmvParameters.dwAudioDecoderCpu = 4;
		xmvParameters.dwAudioRendererCpu = 4;
		xmvParameters.createType = XMEDIA_CREATE_FROM_MEMORY;
		xmvParameters.createFromMemory.pvBuffer = (PVOID)pBuffer;
		xmvParameters.createFromMemory.dwBufferSize = bufferSize;

		IDirect3DDevice9 *pD3DDevice = (IDirect3DDevice9 *)g_pMaterialSystem->GetD3DDevice();
		HRESULT hr = XMediaCreateXmvPlayer( pD3DDevice, &xmvParameters, &m_pXMVPlayer );
		if ( FAILED( hr ) )
		{
			return false;
		}

		m_pMovieEndCallback = pMovieEndCallback;

		RECT rect;
		rect.left = 0;
		rect.top = 0;
		rect.right = m_nDecodeWidth;
		rect.bottom = m_nDecodeHeight;
		m_pXMVPlayer->SetRectangle( &rect );
		UpdateMovieVolume( true );

		SetAlpha( 0 );
		m_StartTime = Plat_FloatTime() + m_FadeDelay;
		m_StopTime = 0;

		return true;
	}

	bool StartMovieFromFile( const char *pMovieName, bool bAudio, bool bLoop, MovieEndCallback_t pMovieEndCallback = NULL )
	{
		if ( m_pXMVPlayer || m_bStopping )
		{
			// already started or currently stopping
			return false;
		}

		XMEDIA_XMV_CREATE_PARAMETERS xmvParameters;
		V_memset( &xmvParameters, 0, sizeof( xmvParameters ) );

		xmvParameters.dwFlags = XMEDIA_CREATE_CPU_AFFINITY;
		if ( bLoop )
		{
			xmvParameters.dwFlags |= XMEDIA_CREATE_FOR_LOOP;
			m_bLooped = true;
		}
		if ( !bAudio )
		{
			xmvParameters.dwAudioStreamId = (DWORD)XMEDIA_STREAM_ID_DONT_USE;
		}
		InitUserAudioMix( bAudio );

		char szFilename[MAX_PATH];
		V_ComposeFileName( MOVIE_PATH, pMovieName, szFilename, sizeof( szFilename ) );

		xmvParameters.dwVideoDecoderCpu = 2;
		xmvParameters.dwVideoRendererCpu = 2;
		xmvParameters.dwAudioDecoderCpu = 4;
		xmvParameters.dwAudioRendererCpu = 4;
		xmvParameters.createType = XMEDIA_CREATE_FROM_FILE;
		xmvParameters.createFromFile.szFileName = szFilename;

		IDirect3DDevice9 *pD3DDevice = (IDirect3DDevice9 *)g_pMaterialSystem->GetD3DDevice();
		HRESULT hr = XMediaCreateXmvPlayer( pD3DDevice, &xmvParameters, &m_pXMVPlayer );
		if ( FAILED( hr ) )
		{
			return false;
		}

		m_pMovieEndCallback = pMovieEndCallback;

		RECT rect;
		rect.left = 0;
		rect.top = 0;
		rect.right = m_nDecodeWidth;
		rect.bottom = m_nDecodeHeight;
		m_pXMVPlayer->SetRectangle( &rect );
		UpdateMovieVolume( true );

		SetAlpha( 0 );
		m_StartTime = Plat_FloatTime() + m_FadeDelay;
		m_StopTime = 0;

		return true;
	}

	void StopMovie()
	{
		if ( !m_pXMVPlayer || m_bStopping )
		{
			// already stopped or currently stopping
			return;
		}

		m_bLooped = false;
		m_bStopping = true;
		m_pXMVPlayer->Stop( XMEDIA_STOP_IMMEDIATE );

		SetAlpha( 255 );
		m_StartTime = 0;
		m_StopTime = Plat_FloatTime() + m_FadeDelay;
	}

	virtual void PaintBackground( void )
	{
		if ( m_StartTime )
		{
			// fade up goes from [0..1] and holds on 1
			float t = ( Plat_FloatTime() - m_StartTime ) * 2.0f;
			t = clamp( t, 0.0f, 1.0f );
			SetAlpha( t * 255.0f );
		}

		if ( m_StopTime )
		{
			// fade out goes from [1..0] and holds on 0
			float t = ( Plat_FloatTime() - m_StopTime ) * 2.0f;
			t = 1.0f - clamp( t, 0.0f, 1.0f );
			SetAlpha( t * 255.0f );
			if ( m_Color.a() == 0 )
			{
				if ( m_bStopping && m_pMovieEndCallback )
				{
					m_pMovieEndCallback();
				}
				m_bStopping = false;
			}
		}

		if ( m_Color.a() != 0 )
		{
			int panelWidth, panelHeight;
			GetSize( panelWidth, panelHeight );

			float s0 = 0.0f;
			float s1 = 1.0f;
			int y = 0;
			if ( m_AspectRatio > 1.0f )
			{
				if ( m_bLetterbox )
				{
					// horizontal letterbox
					float adjustedHeight = (float)panelWidth / m_AspectRatio;
					float bandHeight = ( (float)panelHeight - adjustedHeight ) / 2;
					
					vgui::surface()->DrawSetColor( Color( 0, 0, 0, m_Color.a() ) );
					vgui::surface()->DrawFilledRect( 0, 0, panelWidth, bandHeight );
					vgui::surface()->DrawFilledRect( 0, panelHeight - bandHeight, panelWidth, panelHeight );

					y = bandHeight;
					panelHeight = adjustedHeight;
				}
				else
				{
					// hold the panel's height constant, determine the corresponding aspect corrected image width
					float imageWidth = (float)panelHeight * m_AspectRatio;
					// adjust the image width as a percentage of the panel's width
					// scale and center;
					s1 = (float)panelWidth / imageWidth;
					s0 = ( 1 - s1 ) / 2.0f;
					s1 = s0 + s1;
				}
			}

			vgui::surface()->DrawSetColor( m_Color );
			vgui::surface()->DrawSetTexture( m_imageID );
			vgui::surface()->DrawTexturedSubRect( 0, y, panelWidth, y+panelHeight, s0, 0.0f, s1, 1.0f );
		}
	}

	bool IsFullyReleased()
	{
		// fully stopped and released when object no longer exists
		return ( m_pXMVPlayer == NULL ) && ( m_bStopping == false );
	}

	bool RenderVideoFrame()
	{
		if ( !m_pXMVPlayer )
		{
			return false;
		}

		// If RenderNextFrame does not return S_OK then the frame was not
		// rendered (perhaps because it was cancelled) so a regular frame
		// buffer should be rendered before calling present.
		bool bRenderedFrame = true;
		HRESULT hr = m_pXMVPlayer->RenderNextFrame( 0, NULL );
		if ( FAILED( hr ) || hr == XMEDIA_W_EOF )
		{
			bRenderedFrame = false;

			if ( !m_bLooped )
			{
				// Release the movie object
				m_pXMVPlayer->Release();
				m_pXMVPlayer = NULL;

				if ( !m_bStopping )
				{
					m_bStopping = true;
					SetAlpha( 255 );
					m_StartTime = 0;
					m_StopTime = Plat_FloatTime() + m_FadeDelay;
				}
			}
		}

		// UNDONE: Need a frametime here.  Assume it's 30fps.
		// NOTE: This is only used to time audio fades, so if it's wrong by 2X it's not
		// going to be noticeable.  Probably fine to ship this.
		UpdateMovieVolume( false, 1.0f / 30.0f );

		// Reset our cached view of what pixel and vertex shaders are set, because
		// it is no longer accurate, since XMV will have set their own shaders.
		// This avoids problems when the shader cache thinks it knows what shader
		// is set and it is wrong.
		IDirect3DDevice9 *pD3DDevice = (IDirect3DDevice9 *)g_pMaterialSystem->GetD3DDevice();
		pD3DDevice->SetVertexShader( NULL );
		pD3DDevice->SetPixelShader( NULL );
		pD3DDevice->SetVertexDeclaration( NULL );
		pD3DDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

		if ( bRenderedFrame )
		{
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

			Rect_t rect;
			rect.x = 0;
			rect.y = 0;
			rect.width = m_nDecodeWidth;
			rect.height = m_nDecodeHeight;
			pRenderContext->CopyRenderTargetToTextureEx( m_pTexture, 0, &rect );
		}

		return bRenderedFrame;
	}

private:
	void SetAlpha( int alpha )
	{
		// set alpha only
		alpha = clamp( alpha, 0, 255 );
		m_Color.SetColor( m_Color.r(), m_Color.g(), m_Color.b(), alpha );
	}

	int					m_imageID;
	Color				m_Color;
	IXMediaXmvPlayer	*m_pXMVPlayer;
	ITexture			*m_pTexture;
	IMaterial			*m_pMaterial;
	bool				m_bStopping;
	bool				m_bLooped;
	float				m_StartTime;
	float				m_StopTime;
	int					m_nDecodeWidth;
	int					m_nDecodeHeight;
	float				m_FadeDelay;
	float				m_AspectRatio;
	float				m_CurrentVolume;
	bool				m_bLetterbox;
	MovieEndCallback_t	m_pMovieEndCallback;
};

class CShadowLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( CShadowLabel, vgui::Label );
public:
	CShadowLabel( vgui::Panel *pParent, const char *pName, const wchar_t *pText ) : BaseClass( pParent, pName, pText ) 
	{
	}

	virtual void Paint( void )
	{
		BaseClass::Paint();
		BaseClass::Paint();
		BaseClass::Paint();
		BaseClass::Paint();
		BaseClass::Paint();
	}
};

class CGamePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGamePanel, vgui::Panel );
public:
	CGamePanel( vgui::Panel *pParent, const char *pName, game_t *pGame, bool bIsWidescreen, KeyValues *pKVSettings ) : BaseClass( pParent, pName ) 
	{
		m_bEnabled = pGame->bEnabled;

		vgui::HScheme hScheme = vgui::scheme()->GetScheme( "SourceScheme" );
		vgui::IScheme *pSourceScheme = vgui::scheme()->GetIScheme( hScheme );

		KeyValues *pKV = pKVSettings->FindKey( "GamePanel" );
		int panelWidth = pKV->GetInt( "wide" );
		int panelHeight = pKV->GetInt( "tall" );
		SetSize( panelWidth, panelHeight );

		// thumbnail static image
		char szFilename[MAX_PATH];
		V_snprintf( szFilename, sizeof( szFilename ), "vgui/appchooser/%s", pGame->pGameDir );
		pKV = pKVSettings->FindKey( "GameImage" );
		int y = pKV->GetInt( "ypos" );
		int w = pKV->GetInt( "wide" );
		int h = pKV->GetInt( "tall" );
		int x = ( panelWidth - w ) / 2;
		m_pThumbImage = new CImage( this, "GameImage", szFilename );
		SETUP_PANEL( m_pThumbImage );
		m_pThumbImage->SetBounds( x, y, w, h );
		m_pThumbImage->SetAspectRatio( bIsWidescreen ? 1.0f : 1.778f, false );
		m_pThumbImage->SetVisible( true );

		// thumbnail movie
		m_pMovieImage = new CMovieImage( this, "Movie", pGame->pGameDir, 256, 256, 0.2f );
		SETUP_PANEL( m_pMovieImage );
		m_pMovieImage->SetBounds( x, y, w, h );
		m_pMovieImage->SetVisible( true );
		V_snprintf( szFilename, sizeof( szFilename ), "%s\\thumb_%s.wmv", MOVIE_PATH, pGame->pGameDir );
		if ( !g_pFullFileSystem->ReadFile( szFilename, NULL, m_MovieBuffer ) )
		{
			m_MovieBuffer.Purge();
		}

		// game title shadow
		pKV = pKVSettings->FindKey( "GameTitle" );
		y = pKV->GetInt( "ypos" );
		m_pTitleShadow = new CShadowLabel( this, "GameTitle", pGame->pName );
		SETUP_PANEL( m_pTitleShadow );
		m_pTitleShadow->SetVisible( true );
		m_pTitleShadow->SetFont( pSourceScheme->GetFont( "AppChooserGameTitleFontBlur" ) );
		m_pTitleShadow->SizeToContents();
		m_pTitleShadow->SetPos( 0, y );
		m_pTitleShadow->SetWide( panelWidth );
		m_pTitleShadow->SetContentAlignment( vgui::Label::a_center );
		m_pTitleShadow->SetPaintBackgroundEnabled( false );
		m_pTitleShadow->SetFgColor( Color( 0, 0, 0, 255 ) );

		// game title
		m_pTitle = new vgui::Label( this, "GameTitle", pGame->pName );
		SETUP_PANEL( m_pTitle );
		m_pTitle->SetVisible( true );
		m_pTitle->SetFont( pSourceScheme->GetFont( "AppChooserGameTitleFont" ) );
		m_pTitle->SizeToContents();
		m_pTitle->SetPos( 0, y );
		m_pTitle->SetWide( panelWidth );
		m_pTitle->SetContentAlignment( vgui::Label::a_center );
		m_pTitle->SetPaintBackgroundEnabled( false );
		m_pTitle->SetFgColor( Color( 255, 255, 255, 255 ) );

		// button bounds
		vgui::HFont hFont = pSourceScheme->GetFont( "GameUIButtons" );
		pKV = pKVSettings->FindKey( "GameButton" );
		y = panelHeight - vgui::surface()->GetFontTall( hFont ) -  pKV->GetInt( "ypos" );
		m_pButtonText = new vgui::Label( this, "GameButton", g_pVGuiLocalize->Find( "#GameUI_Icons_A_BUTTON" ) );
		SETUP_PANEL( m_pButtonText );
		m_pButtonText->SetVisible( false );
		m_pButtonText->SetFont( hFont );
		m_pButtonText->SizeToContents();
		m_pButtonText->SetPos( 0, y );
		m_pButtonText->SetWide( panelWidth );
		m_pButtonText->SetContentAlignment( vgui::Label::a_center );
		m_pButtonText->SetPaintBackgroundEnabled( false );
		m_pButtonText->SetFgColor( Color( 255, 255, 255, 255 ) );
	}

	~CGamePanel( void )
	{
	}

	void SetSelected( bool bSelected )
	{
		m_pButtonText->SetVisible( m_bEnabled ? bSelected : false );
		if ( bSelected )
		{
			SetBgColor( Color( 190, 115, 0, 128 ) );
			m_pThumbImage->SetColor( 255, 255, 255 );
			m_pMovieImage->SetColor( 255, 255, 255 );
			m_pTitle->SetFgColor( Color( 255, 255, 255, 255 ) );
		}
		else
		{
			SetBgColor( Color( 160, 160, 160, 50 ) );
			m_pThumbImage->SetColor( 100, 100, 100 );
			m_pMovieImage->SetColor( 120, 120, 120 );
			m_pTitle->SetFgColor( Color( 140, 140, 140, 255 ) );
		}
	}

	void StartMovie()
	{
		m_pMovieImage->StartMovieFromMemory( m_MovieBuffer.Base(), m_MovieBuffer.TellMaxPut(), true, true );
	}

	void StopMovie()
	{
		m_pMovieImage->StopMovie();
	}

	CImage				*m_pThumbImage;
	CMovieImage			*m_pMovieImage;
	CShadowLabel		*m_pTitleShadow;
	vgui::Label			*m_pTitle;
	vgui::Label			*m_pButtonText;
	CUtlBuffer			m_MovieBuffer;
	bool				m_bEnabled;
	int					m_imageID;
};

//-----------------------------------------------------------------------------
// Simple XAudio wrapper to instance a sound. Provides lightweight audio feedback for ui.
// Instance this per sound.
//-----------------------------------------------------------------------------
class CXSound
{
public:
	CXSound()
	{
		for ( int i=0; i<ARRAYSIZE( m_pSourceVoices ); i++ )
		{
			m_pSourceVoices[i] = NULL;
		}
		m_pXMAData = NULL;
		m_nXMADataSize = 0;
		m_numVoices = 0;
	}

	~CXSound()
	{
		Stop();
		Release();
	}

	//-----------------------------------------------------------------------------
	// Setup the wave, caller can clamp the number of simultaneous playing instances
	// of this sound. Useful for ui clicks to keep up with rapid input.
	//-----------------------------------------------------------------------------
	bool SetupWave( const char *pFilename, int numSimultaneous = 1 )
	{
		CUtlBuffer buf;
		if ( !g_pFullFileSystem->ReadFile( pFilename, "GAME", buf ) )
		{
			Msg( "SetupWave: File '%s' not found\n", pFilename );
			return false;
		}

		// only supporting xwv format
		xwvHeader_t* pHeader = (xwvHeader_t *)buf.Base();
		if ( pHeader->id != XWV_ID || pHeader->version != XWV_VERSION || pHeader->format != XWV_FORMAT_XMA )
		{
			Msg( "SetupWave: File '%s' has bad format\n", pFilename );
			return false;
		}

		XAUDIOSOURCEVOICEINIT SourceVoiceInit = { 0 };
		SourceVoiceInit.Format.SampleType = XAUDIOSAMPLETYPE_XMA;
		SourceVoiceInit.Format.NumStreams = 1;
		SourceVoiceInit.MaxPacketCount = 1;
		SourceVoiceInit.Format.Stream[0].SampleRate = pHeader->GetSampleRate();
		SourceVoiceInit.Format.Stream[0].ChannelCount = pHeader->channels;

		// create enough source voices to support simultaneous play of this sound
		HRESULT hr;
		numSimultaneous = min( numSimultaneous, ARRAYSIZE( m_pSourceVoices ) );
		for ( int i=0; i<numSimultaneous; i++ )
		{
			// create the voice
			hr = XAudioCreateSourceVoice( &SourceVoiceInit, &m_pSourceVoices[i] );
			if ( FAILED( hr ) )
			{
				return false;
			}
		}
		m_numVoices = numSimultaneous;

		// get the xma data
		m_nXMADataSize = pHeader->dataSize;
		m_pXMAData = (unsigned char *)XPhysicalAlloc( m_nXMADataSize, MAXULONG_PTR, 0, PAGE_READWRITE );
		V_memcpy( m_pXMAData, (unsigned char *)buf.Base() + pHeader->dataOffset, m_nXMADataSize );

		return true;
	}

	bool IsPlaying()
	{
		XAUDIOSOURCESTATE SourceState;
		for ( int i=0; i<m_numVoices; i++ )
		{
			m_pSourceVoices[i]->GetVoiceState( &SourceState );
			if ( SourceState & XAUDIOSOURCESTATE_STARTED )
			{
				return true;
			}
		}
		return false;
	}

	void Play()
	{
		int numPlaying = 0;
		XAUDIOSOURCESTATE SourceState;
		for ( int i=0; i<m_numVoices; i++ )
		{
			m_pSourceVoices[i]->GetVoiceState( &SourceState );
			if ( SourceState & XAUDIOSOURCESTATE_STARTED )
			{
				numPlaying++;
			}
		}
		if ( numPlaying >= m_numVoices )
		{
			return;
		}

		// find a free voice
		IXAudioSourceVoice *pVoice = NULL;
		for ( int i=0; i<m_numVoices; i++ )
		{
			m_pSourceVoices[i]->GetVoiceState( &SourceState );
			if ( !( SourceState & XAUDIOSOURCESTATE_STARTED ) )
			{
				// use the free voice
				pVoice = m_pSourceVoices[i];
				break;
			}
		}
		if ( !pVoice )
		{
			// none free
			return;
		}

		// Set up packet
		XAUDIOPACKET Packet = { 0 };
		Packet.pBuffer = m_pXMAData;
		Packet.BufferSize = m_nXMADataSize;

		// Submit packet
		HRESULT hr = pVoice->SubmitPacket( &Packet, XAUDIOSUBMITPACKET_DISCONTINUITY );
		if ( !FAILED( hr ) )
		{
			pVoice->Start( 0 );
		}
	}

	void Stop()
	{
		for ( int i=0; i<m_numVoices; i++ )
		{
			m_pSourceVoices[i]->Stop( 0 );
		}
	}

	void Release()
	{
		for ( int i=0; i<ARRAYSIZE( m_pSourceVoices ); i++ )
		{
			if ( m_pSourceVoices[i] )
			{
				m_pSourceVoices[i]->Release();
				m_pSourceVoices[i] = NULL;
			}
		}
		if ( m_pXMAData )
		{
			XPhysicalFree( m_pXMAData );
			m_pXMAData = NULL;
		}
		m_nXMADataSize = 0;
		m_numVoices = 0;
	}

private:
	IXAudioSourceVoice	*m_pSourceVoices[4];
	unsigned char		*m_pXMAData;
	int					m_nXMADataSize;
	int					m_numVoices;
};

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CAppChooser : public CVguiSteamApp
{
	typedef CVguiSteamApp BaseClass;

public:
	virtual bool	Create();
	virtual bool	PreInit();
	virtual int		Main();
	virtual void	Destroy();

	void			OnSelectionPrevious();
	void			OnSelectionNext();
	void			OnActivateGame();
	void			OnInactivityTimeout();
	void			OnStopDemoMovie();
	void			OnDemoMovieEnd();
	void			OnStopStartupMovie();
	void			Reset();
	void			ResetTimeout( float timeout );
	bool			IsInputEnabled() { return m_bInputEnabled; }
	bool			IsDemoMoviePlaying() { return m_bPlayingDemoMovie; }
	bool			IsStartupMoviePlaying() { return m_bPlayingStartupMovies; }
	void			HandleInvite( DWORD nUserId  );
 
private:
	const char		*GetAppName() { return "AppChooser"; }

	bool			CreateWindow( int width, int height, bool fullscreen );
	bool			InitMaterialSystem();
	bool			InitVGUI();
	void			ShutdownVGUI();
	bool			InitAudio();
	void			ShutdownAudio();
	void			FrameTick();
	void			RenderScene();
	void			ExitChooser();
	void			EnableInput( bool bEnable );
	void			StartExitingProcess();
	void			SetLoadingIconPosition( bool bIsMultiplayer );

	int				m_nScreenWidth;
	int				m_nScreenHeight;
	HWND			m_hWnd;
	float			m_FadeInTime;
	float			m_FadeOutTime;
	float			m_StartTime;
	float			m_Timeout;
	int				m_Selection;
	int				m_LastSelection;
	bool			m_bInputEnabled;
	int				m_ExitingFrameCount;
	float			m_GameMovieStartTime;
	float			m_BackgroundMovieStartTime;
	int				m_LastBackgroundMovie;
	int				m_StartupMovie;

	// various operating states
	bool			m_bPlayingStartupMovies;
	bool			m_bPlayingDemoMovie;
	bool			m_bExiting;
	
	// Live invite handling
	bool			m_bInviteAccepted;
	XNKID			m_InviteSessionID;
	DWORD			m_nInviteUserID;
	
	int				m_iImageID;
	vgui::Panel		*m_pRootPanel;
	CImage			*m_pPersistedImage;
	CImage			*m_pOverlay;
	CImage			*m_pFullBlack;
	CMovieImage		*m_pStartupMovieImage;
	CMovieImage		*m_pBackgroundMovie;
	CMovieImage		*m_pDemoMovieImage;
	CGamePanel		*m_pGames[ARRAYSIZE( g_Games )];
	CImage			*m_pProductTitle;
	CShadowLabel	*m_pInstructionsShadow;
	vgui::Label		*m_pInstructions;
	vgui::Label		*m_pLoading;
	CImage			*m_pLoadingIcon;
	CImage			*m_pProductBackgrounds[ARRAYSIZE( g_Games )];
	DWORD			m_iStorageDeviceID;
	DWORD			m_iUserIdx;
	KeyValues		*m_pKVSettings;
	int				m_DemoMovie;

	CXSound			m_Click;
	CXSound			m_Clack;
	CXSound			m_Deny;
};

CAppChooser g_AppChooserSystem;

//--------------------------------------------------------------------------------------
// InitMaterialSystem
//
//--------------------------------------------------------------------------------------
bool CAppChooser::InitMaterialSystem()
{
	RECT rect;
	
	MaterialSystem_Config_t config;
	config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, IsPC() ? true : false );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, 0 );

	config.m_VideoMode.m_Width = 0;
	config.m_VideoMode.m_Height = 0;
	config.m_VideoMode.m_Format = IMAGE_FORMAT_BGRX8888;
	config.m_VideoMode.m_RefreshRate = 0;
	config.dxSupportLevel = IsX360() ? 98 : 0;

	g_pMaterialSystem->ModInit();
	bool modeSet = g_pMaterialSystem->SetMode( m_hWnd, config );
	if ( !modeSet )
	{
		Error( "Failed to set mode\n" );
		return false;
	}

	g_pMaterialSystem->OverrideConfig( config, false );

	GetClientRect( m_hWnd, &rect );
	m_nScreenWidth = rect.right;
	m_nScreenHeight = rect.bottom;

	return true;
}

void CAppChooser::SetLoadingIconPosition( bool bIsMultiplayer )
{
	// matches config from matsys_interface.cpp::InitStartupScreen()
	float flNormalizedX;
	float flNormalizedY;
	float flNormalizedSize;
	if ( !bIsMultiplayer )
	{
		flNormalizedX = 0.5f;
		flNormalizedY = 0.86f;
		flNormalizedSize = 0.1f;
	}
	else
	{
		flNormalizedX = 0.5f;
		flNormalizedY = 0.9f;
		flNormalizedSize = 0.1f;
	}

	// matches calcs from CShaderDeviceDx8::RefreshFrontBufferNonInteractive()
	float flXPos = flNormalizedX;
	float flYPos = flNormalizedY;
	float flHeight = flNormalizedSize;
	int nSize = m_nScreenHeight * flHeight;
	int x = m_nScreenWidth * flXPos - nSize * 0.5f;
	int y = m_nScreenHeight * flYPos - nSize * 0.5f;
	int w = nSize;
	int h = nSize;
	m_pLoadingIcon->SetBounds( x, y, w, h );
}

//-----------------------------------------------------------------------------
// Setup all our VGUI info
//-----------------------------------------------------------------------------

bool CAppChooser::InitVGUI( void )
{
	int x, y, w, h, g;
	KeyValues *pKV;

	vgui::surface()->GetScreenSize( w, h );
	float aspectRatio = (float)w/(float)h;
	bool bIsWidescreen = ( aspectRatio >= 1.7f );
	bool bIsHiDef = h > 480;

	const char *pResolutionKey = "";
	if ( bIsWidescreen )
	{
		// 16:9 aspect
		if ( bIsHiDef )
		{
			pResolutionKey = "_hidef";
		}
		else
		{
			pResolutionKey = "_lodef_wide";
		}
	}
	else
	{
		// 4:3 apsect
		if ( bIsHiDef )
		{
			pResolutionKey = "_hidef_norm";
		}
		else
		{
			pResolutionKey = "_lodef";
		}
	}

	// Start vgui
	vgui::ivgui()->Start();
	vgui::ivgui()->SetSleep( false );

	// load the scheme
	vgui::scheme()->LoadSchemeFromFile( "resource/sourcescheme.res", NULL );
	vgui::HScheme hScheme = vgui::scheme()->GetScheme( "SourceScheme" );
	vgui::IScheme *pSourceScheme = vgui::scheme()->GetIScheme( hScheme );

	m_pKVSettings = new KeyValues( "AppChooser.res" );
	if ( m_pKVSettings->LoadFromFile( g_pFullFileSystem, "resource/UI/AppChooser.res", "GAME" ) )
	{
		m_pKVSettings->ProcessResolutionKeys( pResolutionKey );
	}
	else
	{
		return false;
	}

	// localization
	g_pVGuiLocalize->AddFile( "resource/gameui_%language%.txt" ,"GAME", true );

	// Init the root panel
	m_pRootPanel = new vgui::Panel( NULL, "RootPanel" );
	m_pRootPanel->SetBounds( 0, 0, m_nScreenWidth, m_nScreenHeight );
	m_pRootPanel->SetPaintBackgroundEnabled( true );
	m_pRootPanel->SetVisible( true );
	vgui::surface()->SetEmbeddedPanel( m_pRootPanel->GetVPanel() );

	// need a full pure opaque black before all panel drawing
	// this fixes the top of frame xmv video render work
	m_pFullBlack = new CImage( m_pRootPanel, "FullBlack", "vgui/black" );
	SETUP_PANEL( m_pFullBlack );
	m_pFullBlack->SetBounds( 0, 0, m_nScreenWidth, m_nScreenHeight );
	m_pFullBlack->SetVisible( true );

	// all the background movies were authored for widescreen
	m_pBackgroundMovie = new CMovieImage( m_pRootPanel, "Movie", "Background", m_nScreenWidth, m_nScreenHeight, 0.0f );
	SETUP_PANEL( m_pBackgroundMovie );
	m_pBackgroundMovie->SetBounds( 0, 0, m_nScreenWidth, m_nScreenHeight );
	m_pBackgroundMovie->SetAspectRatio( bIsWidescreen ? 1.0f : 1.778f, false );
	m_pBackgroundMovie->SetColor( 255, 255, 255 );
	m_pBackgroundMovie->SetVisible( true );

	pKV = m_pKVSettings->FindKey( "Logo" );
	y = pKV->GetInt( "ypos" );
	w = pKV->GetInt( "wide" );
	h = pKV->GetInt( "tall" );
	m_pProductTitle = new CImage( m_pRootPanel, "Logo", "vgui/appchooser/orangeboxlogo" );
	SETUP_PANEL( m_pProductTitle );
	m_pProductTitle->SetBounds( ( m_nScreenWidth - w )/2, y, w, h );
	m_pProductTitle->SetVisible( true );

	wchar_t *pString = g_pVGuiLocalize->Find( "#GameUI_AppChooser_SelectGame" );
	pKV = m_pKVSettings->FindKey( "SelectGame" );
	y = pKV->GetInt( "ypos" );
	m_pInstructionsShadow = new CShadowLabel( m_pRootPanel, "SelectGame", pString );
	SETUP_PANEL( m_pInstructionsShadow );
	m_pInstructionsShadow->SetFont( pSourceScheme->GetFont( "ChapterTitleBlur" ) );
	m_pInstructionsShadow->SizeToContents();
	m_pInstructionsShadow->SetWide( m_nScreenWidth );
	m_pInstructionsShadow->SetPos( 0, y );
	m_pInstructionsShadow->SetContentAlignment( vgui::Label::a_center );
	m_pInstructionsShadow->SetPaintBackgroundEnabled( false );
	m_pInstructionsShadow->SetFgColor( Color( 0, 0, 0, 255) );

	m_pInstructions = new vgui::Label( m_pRootPanel, "SelectGame", pString );
	SETUP_PANEL( m_pInstructions );
	m_pInstructions->SetFont( pSourceScheme->GetFont( "ChapterTitle" ) );
	m_pInstructions->SizeToContents();
	m_pInstructions->SetWide( m_nScreenWidth );
	m_pInstructions->SetPos( 0, y );
	m_pInstructions->SetContentAlignment( vgui::Label::a_center );
	m_pInstructions->SetPaintBackgroundEnabled( false );
	m_pInstructions->SetFgColor( Color( 255, 255, 255, 255) );

	pKV = m_pKVSettings->FindKey( "GamePanel" );
	w = pKV->GetInt( "wide" );
	g = pKV->GetInt( "gap" );
	x = ( m_nScreenWidth - ( (int)( ARRAYSIZE( g_Games ) ) * ( w + g ) - g ) ) / 2;
	y = pKV->GetInt( "ypos" );
	for ( int i=0; i<ARRAYSIZE( g_Games ); i++ )
	{
		m_pGames[i] = new CGamePanel( m_pRootPanel, "GamePanel", &g_Games[i], bIsWidescreen, m_pKVSettings );
		SETUP_PANEL( m_pGames[i] );
		m_pGames[i]->SetPos( x, y );
		m_pGames[i]->SetPaintBackgroundType( 2 );
		m_pGames[i]->SetSelected( false );
		x += w + g;
	}

	// the product backgrounds are topmost and used as fade out materials
	for ( int i=0; i<ARRAYSIZE( g_Games ); i++ )
	{
		char filename[MAX_PATH];
		V_snprintf( filename, sizeof( filename ), "vgui/appchooser/background_%s%s", g_Games[i].pGameDir, bIsWidescreen ? "_widescreen" : "" );
		m_pProductBackgrounds[i] = new CImage( m_pRootPanel, "Products", filename );
		SETUP_PANEL( m_pProductBackgrounds[i] );
		m_pProductBackgrounds[i]->SetBounds( 0, 0, m_nScreenWidth, m_nScreenHeight );
		m_pProductBackgrounds[i]->SetVisible( true );
		m_pProductBackgrounds[i]->SetAlpha( 0 );
	}

	pString = g_pVGuiLocalize->Find( "#GameUI_Loading" );
	y = m_nScreenHeight / 2;
	m_pLoading = new vgui::Label( m_pRootPanel, "Text", pString );
	SETUP_PANEL( m_pLoading );
	m_pLoading->SetFont( pSourceScheme->GetFont( "ChapterTitle" ) );
	m_pLoading->SizeToContents();
	m_pLoading->SetWide( m_nScreenWidth );
	m_pLoading->SetPos( 0, y );
	m_pLoading->SetContentAlignment( vgui::Label::a_center );
	m_pLoading->SetPaintBackgroundEnabled( false );
	m_pLoading->SetFgColor( Color( 255, 255, 255, 255) );
	m_pLoading->SetVisible( false );

	m_pLoadingIcon = new CImage( m_pRootPanel, "LoadingIcon", "vgui/appchooser/loading_icon" );
	SETUP_PANEL( m_pLoadingIcon );
	SetLoadingIconPosition( false );
	m_pLoadingIcon->SetVisible( false );
	m_pLoadingIcon->SetAlpha( 0 );

	// create back buffer cloned texture
	ITexture *pTexture = NULL;
	if ( XboxLaunch()->GetLaunchFlags() & LF_INTERNALLAUNCH )
	{
		pTexture = g_pMaterialSystem->CreateProceduralTexture( 
			"PersistedTexture", 
			TEXTURE_GROUP_OTHER,
			m_nScreenWidth,
			m_nScreenHeight,
			g_pMaterialSystem->GetBackBufferFormat(),
			TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY );

		// the persisted texture is in the back buffer, get it
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->CopyRenderTargetToTexture( pTexture );
	}

	// create a material to bind the persisted texture
	// the fade-in material is topmost, fully opaque, and fades to transluscent on initial rendering
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$linearwrite", 1 );    // These 2 lines are needed so that we don't lose bits of precision in
	pVMTKeyValues->SetInt( "$gammacolorread", 1 ); // the dark colors due to the 360's lossy sRGB read hardware
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	if ( pTexture )
	{
		pVMTKeyValues->SetString( "$basetexture", "PersistedTexture" );
	}
	else
	{
		// there is no persisted texture, fade in from black
		pVMTKeyValues->SetString( "$basetexture", "vgui/black" );
	}
	IMaterial *pFadeInMaterial = g_pMaterialSystem->CreateMaterial( "__FadeInMaterial.vmt", pVMTKeyValues );

	// the persisted image is either the image from the relaunch or black during first boot
	m_pPersistedImage = new CImage( m_pRootPanel, "FadeInOverlay", pFadeInMaterial );
	SETUP_PANEL( m_pPersistedImage );
	m_pPersistedImage->SetBounds( 0, 0, m_nScreenWidth, m_nScreenHeight );
	m_pPersistedImage->SetVisible( true );
	m_pPersistedImage->SetAlpha( 255 );
	m_pOverlay = m_pPersistedImage;

	// full screen demo movie, use letterboxing
	m_pDemoMovieImage = new CMovieImage( m_pRootPanel, "Movie", "Demo", m_nScreenWidth, m_nScreenHeight, 2.0f );
	SETUP_PANEL( m_pDemoMovieImage );
	m_pDemoMovieImage->SetBounds( 0, 0, m_nScreenWidth, m_nScreenHeight );
	m_pDemoMovieImage->SetAspectRatio( bIsWidescreen ? 1.0f : 1.778f, true );
	m_pDemoMovieImage->SetVisible( true );

	// full screen startup movies, use letterboxing, instant start
	m_pStartupMovieImage = new CMovieImage( m_pRootPanel, "Movie", "Startup", m_nScreenWidth, m_nScreenHeight, 0.0f );
	SETUP_PANEL( m_pStartupMovieImage );
	m_pStartupMovieImage->SetBounds( 0, 0, m_nScreenWidth, m_nScreenHeight );
	m_pStartupMovieImage->SetAspectRatio( bIsWidescreen ? 1.0f : 1.778f, true );
	m_pStartupMovieImage->SetVisible( true );

	return true;
}

//-----------------------------------------------------------------------------
// Stop VGUI
//-----------------------------------------------------------------------------
void CAppChooser::ShutdownVGUI( void )
{
	delete m_pRootPanel;
}

//-----------------------------------------------------------------------------
// Initialize Audio
//-----------------------------------------------------------------------------
bool CAppChooser::InitAudio()
{
	// Set up initialize parameters of XAudio Engine
	// Both threads on core 2
	XAUDIOENGINEINIT EngineInit = { 0 };
	EngineInit.pEffectTable = &XAudioDefaultEffectTable;
	EngineInit.ThreadUsage = XAUDIOTHREADUSAGE_THREAD4 | XAUDIOTHREADUSAGE_THREAD5;

	// Initialize the XAudio Engine
	HRESULT hr = XAudioInitialize( &EngineInit );
	if ( FAILED( hr ) )
	{
		Error( "Error calling XAudioInitialize\n" );
	}

	m_Click.SetupWave( "sound/ui/buttonclick.360.wav", 2 );
	m_Clack.SetupWave( "sound/ui/buttonclickrelease.360.wav", 2 );
	m_Deny.SetupWave( "sound/player/suit_denydevice.360.wav", 2 );

	return true;
}

void CAppChooser::ShutdownAudio()
{
	// Shut down and free XAudio resources
	XAudioShutDown();
}

//--------------------------------------------------------------------------------------
// Reset timeout
//--------------------------------------------------------------------------------------
void CAppChooser::ResetTimeout( float timeout )
{
	if ( timeout > 0 )
	{
		m_Timeout = Plat_FloatTime() + timeout;
	}
	else
	{
		m_Timeout = 0;
	}
}

//--------------------------------------------------------------------------------------
// Intialization Reset. Expected to be called once after inits.
//--------------------------------------------------------------------------------------
void CAppChooser::Reset()
{
	// set selection to previous game or default
	m_Selection = 0;
	m_LastSelection = 0;
	const char *pGameName = CommandLine()->ParmValue( "-game", "ep2" );
	for ( int i=0; i<ARRAYSIZE( g_Games ); i++ )
	{
		if ( V_stristr( pGameName, g_Games[i].pGameDir ) )
		{
			m_Selection = i;
			break;
		}
	}

	// get the storage device
	m_iStorageDeviceID = XboxLaunch()->GetStorageID();
	Msg( "Storage ID: %d", m_iStorageDeviceID );

	// Set the user index
	m_iUserIdx = XboxLaunch()->GetUserID();
	if ( g_pInputSystem )
	{
		g_pInputSystem->SetPrimaryUserId( m_iUserIdx );
	}
	XBX_SetPrimaryUserId( m_iUserIdx );
	Msg( "User ID: %d", m_iUserIdx );

	m_StartTime = Plat_FloatTime();
	m_FadeInTime = 0;
	m_FadeOutTime = 0;

	m_bExiting = false;
	m_ExitingFrameCount = 0;

	m_bPlayingDemoMovie = false;
	m_DemoMovie = 0;

	// background movie and selection startup in sync
	m_LastBackgroundMovie = m_Selection;
	m_BackgroundMovieStartTime = 0;
	m_GameMovieStartTime = 0;

	EnableInput( true );

	if ( !( XboxLaunch()->GetLaunchFlags() & LF_INTERNALLAUNCH ) )
	{
		// first time boot
		// startup movies play first, and never again
		ResetTimeout( 0 );

		m_StartupMovie = -1;	
		m_bPlayingStartupMovies = true;
	}
	else
	{
		// normal background startup
		ResetTimeout( INACTIVITY_TIMEOUT );

		// foreground movie starts a little staggered
		m_BackgroundMovieStartTime = Plat_FloatTime();
		m_GameMovieStartTime = m_BackgroundMovieStartTime + 1.0f;
	}

	// Init our invite data
	m_bInviteAccepted = false;
	m_nInviteUserID = XBX_INVALID_USER_ID;
	memset( (void *)&m_InviteSessionID, 0, sizeof( m_InviteSessionID ) );
}

//--------------------------------------------------------------------------------------
// Handle previous selection
//--------------------------------------------------------------------------------------
void CAppChooser::OnSelectionPrevious()
{
	// backward wraparound
	m_Selection = ( m_Selection + ARRAYSIZE( g_Games ) - 1 ) % ARRAYSIZE( g_Games );
	m_Click.Play();
}

//--------------------------------------------------------------------------------------
// Handle next selection
//--------------------------------------------------------------------------------------
void CAppChooser::OnSelectionNext()
{
	// forward wraparound
	m_Selection = ( m_Selection + ARRAYSIZE( g_Games ) + 1 ) % ARRAYSIZE( g_Games );
	m_Click.Play();
}

void CAppChooser::EnableInput( bool bEnable )
{
	m_bInputEnabled = bEnable;
}

//--------------------------------------------------------------------------------------
// Chooser exits
//--------------------------------------------------------------------------------------
void CAppChooser::ExitChooser()
{
	// reset stale arguments that encode prior game
	// launcher will establish correct arguments based on desired game
	CommandLine()->RemoveParm( "-game" );
	CommandLine()->AppendParm( "-game", g_Games[m_Selection].pGameDir );

	// Special command line parameter for tf. Command line args persist across
	// relaunches, so remove it first in case we came from tf and are going to another game.
	CommandLine()->RemoveParm( "-swapcores" );
	if ( !Q_stricmp( g_Games[m_Selection].pGameDir, "tf" ) )
	{
		CommandLine()->AppendParm( "-swapcores", NULL );
	}

	int fFlags = LF_EXITFROMCHOOSER;

	// allocate the full payload
	int nPayloadSize = XboxLaunch()->MaxPayloadSize();
	byte *pPayload = (byte *)stackalloc( nPayloadSize );
	V_memset( pPayload, 0, nPayloadSize );

	// payload is at least the command line
	// any user data needed must be placed AFTER the command line
	const char *pCmdLine = CommandLine()->GetCmdLine();
	int nCmdLineLength = (int)strlen( pCmdLine ) + 1;
	V_memcpy( pPayload, pCmdLine, min( nPayloadSize, nCmdLineLength ) );

	// add any other data here to payload, after the command line
	// ...

	XboxLaunch()->SetStorageID( m_iStorageDeviceID );
	
	m_iUserIdx = XBX_GetPrimaryUserId();
	if ( m_bInviteAccepted )
	{
		// A potentially different user was invited, so we need to connect them
		m_iUserIdx = m_nInviteUserID;
	}	
	XboxLaunch()->SetUserID( m_iUserIdx );

	if ( m_bInviteAccepted )
	{
		// In the case of an invitation acceptance, we need to pack extra data into the payload
		fFlags |= LF_INVITERESTART;
		XboxLaunch()->SetInviteSessionID( &m_InviteSessionID );
	}

	// Save out the data
	bool bLaunch = XboxLaunch()->SetLaunchData( (void *)pPayload, nPayloadSize, fFlags );
	if ( bLaunch )
	{
		COM_TimestampedLog( "Launching: \"%s\" Flags: 0x%8.8x", pCmdLine, XboxLaunch()->GetLaunchFlags() );

		g_pMaterialSystem->PersistDisplay();
		ShutdownVGUI();
		ShutdownAudio();
		XBX_DisconnectConsoleMonitor();

		XboxLaunch()->Launch();
	}
}

//--------------------------------------------------------------------------------------
// Handle game selection
//--------------------------------------------------------------------------------------
void CAppChooser::OnActivateGame()
{
	if ( !m_FadeInTime || Plat_FloatTime() <= m_FadeInTime + 1.0f )
	{
		// lockout user selection input until screen has stable rendering and completed its fade in
		// prevents button mashing doing an immediate game selection just as the startup movies end
		return;
	}

	if ( !g_Games[m_Selection].bEnabled )
	{
		m_Deny.Play();
		return;
	}

	m_Clack.Play();
	while ( m_Clack.IsPlaying() )
	{
		// let the audio complete
		Sleep( 1 );
	}

	StartExitingProcess();
}

void CAppChooser::OnDemoMovieEnd()
{
	g_AppChooserSystem.ResetTimeout( INACTIVITY_TIMEOUT );
	m_bPlayingDemoMovie = false;
}

void DemoMovieCallback()
{
	g_AppChooserSystem.OnDemoMovieEnd();
}

//--------------------------------------------------------------------------------------
// Handle inactivity event
//--------------------------------------------------------------------------------------
void CAppChooser::OnInactivityTimeout()
{
	// no further inactivity timeouts
	ResetTimeout( 0 );

	const char *pDemoMovieName = g_DemoMovies[m_DemoMovie];
	m_DemoMovie++;
	if ( m_DemoMovie >= ARRAYSIZE( g_DemoMovies ) )
	{
		// reset
		m_DemoMovie = 0;
	}

	m_bPlayingDemoMovie = m_pDemoMovieImage->StartMovieFromFile( pDemoMovieName, true, false, DemoMovieCallback );
	if ( !m_bPlayingDemoMovie )
	{
		// try again, later
		ResetTimeout( INACTIVITY_TIMEOUT );
	}
}

//--------------------------------------------------------------------------------------
// Handle demo stop request
//--------------------------------------------------------------------------------------
void CAppChooser::OnStopDemoMovie()
{
	m_pDemoMovieImage->StopMovie();
}

//--------------------------------------------------------------------------------------
// Handle startup stop request
//--------------------------------------------------------------------------------------
void CAppChooser::OnStopStartupMovie()
{
	if ( m_StartupMovie >= 0 && g_StartupMovies[m_StartupMovie].bUserCanSkip )
	{
		m_pStartupMovieImage->StopMovie();
	}
}

//--------------------------------------------------------------------------------------
// Setup the exiting context. Cannot be aborted.
//--------------------------------------------------------------------------------------
void CAppChooser::StartExitingProcess()
{
	bool bIsMultiplayer = V_stricmp( g_Games[m_Selection].pGameDir, "tf" ) == 0;

	// stable rendering, start the fade out
	m_pOverlay = m_pProductBackgrounds[m_Selection];

	m_pLoading->SetVisible( true );
	m_pLoading->SetAlpha( 0 );
	SetLoadingIconPosition( bIsMultiplayer );
	m_pLoadingIcon->SetVisible( true );
	m_pLoadingIcon->SetAlpha( 0 );

	m_FadeOutTime = Plat_FloatTime();

	m_bExiting = true;
	m_ExitingFrameCount = 0;

	// ensure that nothing can stop the exiting process
	// otherwise the player could rapidly select a game during the fade outs
	ResetTimeout( 0 );
	EnableInput( false );
}

//--------------------------------------------------------------------------------------
// Handle per frame update, prior to render
//--------------------------------------------------------------------------------------
void CAppChooser::FrameTick()
{
	if ( m_ExitingFrameCount )
	{
		return;
	}

	g_TargetMovieVolume = 1.0f;
	BOOL bControl;
	if ( ERROR_SUCCESS == XMPTitleHasPlaybackControl(&bControl) )
	{
		g_TargetMovieVolume = bControl ? 1.0f : 0.0f;
	}

	if ( m_FadeInTime )
	{
		// fade in overlay goes from [1..0] and holds on 0
		float t = ( Plat_FloatTime() - m_FadeInTime ) * 2.0f;
		t = 1.0f - clamp( t, 0.0f, 1.0f );
		m_pOverlay->SetAlpha ( t * 255.0f );
	}

	if ( m_FadeOutTime )
	{
		// fade out overlay goes from [0..1] and holds on 1
		float t = ( Plat_FloatTime() - m_FadeOutTime ) * 2.0f;
		t = clamp( t, 0.0f, 1.0f );
		m_pOverlay->SetAlpha ( t * 255.0f );
	}

	for ( int i=0; i<ARRAYSIZE( g_Games ); i++ )
	{
		m_pGames[i]->SetSelected( false );
	}
	m_pGames[m_Selection]->SetSelected( true );

	if ( m_bExiting )
	{
		m_pLoading->SetAlpha( m_pOverlay->GetAlpha() ); 
		m_pLoadingIcon->SetAlpha( m_pOverlay->GetAlpha() ); 
		if ( m_pOverlay->GetAlpha() == 255 )
		{
			// exiting needs to have fade overlay fully opaque before stopping background movie
			m_pBackgroundMovie->StopMovie();

			// exiting commences after all movies full release and enough frames have swapped to ensure stability
			// strict time (frame rate) cannot be trusted, must allow a non trivial amount of presents
			m_ExitingFrameCount = 30;
		}
	}

	if ( m_bPlayingStartupMovies )
	{
		if ( m_pStartupMovieImage->IsFullyReleased() )
		{
			m_StartupMovie++;
			if ( m_StartupMovie >= ARRAYSIZE( g_StartupMovies ) )
			{
				// end of cycle
				m_StartupMovie = -1;
				m_bPlayingStartupMovies = false;
			}
			else
			{
				m_bPlayingStartupMovies = m_pStartupMovieImage->StartMovieFromFile( g_StartupMovies[m_StartupMovie].pMovieName, true, false );
			}
		}
	
		if ( !m_bPlayingStartupMovies )
		{
			ResetTimeout( INACTIVITY_TIMEOUT );
			m_BackgroundMovieStartTime = Plat_FloatTime();
			m_GameMovieStartTime = m_BackgroundMovieStartTime + 1.0f;
		}
		else
		{
			return;
		}
	}

	if ( m_bInviteAccepted )
	{
		// stop attract mode
		if ( m_bPlayingDemoMovie )
		{
			// hint the demo movie to stop
			OnStopDemoMovie();
		}
	
		// attract movies must finish before allowing invite
		// background movies must finish before allowing invite
		// starts the exiting process ONCE
		if ( !m_bPlayingStartupMovies && !m_bPlayingDemoMovie && !m_bExiting )
		{
			for ( int i = 0; i < ARRAYSIZE( g_Games ); i++ )
			{
				if ( V_stristr( "tf", g_Games[i].pGameDir ) )
				{
					// EVIL! spoof a user slection to invite only TF
					m_Selection = i;
					StartExitingProcess();

					// must fixup state, invite could have happened very early, at any time, before the initial fade-in
					// ensure no movies start
					m_BackgroundMovieStartTime = 0;
					m_GameMovieStartTime = 0;
				
					// hack the fade times to match the expected state
					if ( !m_FadeInTime || m_FadeInTime >= Plat_FloatTime() )
					{
						// can't fade out, invite occured before fade-in
						// can only snap it because there is no background to fade from
						m_pPersistedImage->SetAlpha( 0 );
						m_FadeInTime = 0;
						m_FadeOutTime = 0;
						m_pOverlay->SetAlpha( 255 );
					}
					break;
				}
			}
		}
	}

	if ( m_bExiting || m_bPlayingDemoMovie || m_LastSelection != m_Selection )
	{		
		m_pGames[m_LastSelection]->StopMovie(); 
		m_LastSelection = m_Selection;

		// user can change selection very quickly
		// lag the movie starting until selection settles
		m_GameMovieStartTime = Plat_FloatTime() + 2.0f;
	}

	if ( !m_bExiting && !m_bPlayingDemoMovie && ( m_GameMovieStartTime != 0 ) && ( Plat_FloatTime() >= m_GameMovieStartTime ) )
	{
		// keep trying the current selection, it will eventually start
		m_pGames[m_Selection]->StartMovie(); 
		if ( m_LastBackgroundMovie != m_Selection )
		{
			m_pBackgroundMovie->StopMovie();
			m_LastBackgroundMovie = m_Selection;
			m_BackgroundMovieStartTime = Plat_FloatTime() + 2.0f;
		}
	}

	// update the background movie, lags far behind any user selection
	if ( !m_bExiting && !m_bPlayingDemoMovie && ( m_BackgroundMovieStartTime != 0 ) && ( Plat_FloatTime() >= m_BackgroundMovieStartTime ) )
	{
		char szFilename[MAX_PATH];
		V_snprintf( szFilename, sizeof( szFilename ), "background_%s.wmv", g_Games[m_Selection].pGameDir );

		// keep trying the current selection, it will eventually start
		bool bStarted = m_pBackgroundMovie->StartMovieFromFile( 
			szFilename,
			false,
			true );
		if ( bStarted )
		{
			m_LastBackgroundMovie = m_Selection;
		}
	}
}

//--------------------------------------------------------------------------------------
// Render the scene
//--------------------------------------------------------------------------------------
void CAppChooser::RenderScene()
{
	FrameTick();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	g_pMaterialSystem->BeginFrame( Plat_FloatTime() );

	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, true );
	pRenderContext->Flush( true );

	// render all movies, XMV needs to hijack D3D
	bool bPreviousState = g_pMaterialSystem->OwnGPUResources( false );
	{
		m_pStartupMovieImage->RenderVideoFrame();
		m_pBackgroundMovie->RenderVideoFrame();
		m_pDemoMovieImage->RenderVideoFrame();
		for ( int i=0; i<ARRAYSIZE( m_pGames ); i++ )
		{
			m_pGames[i]->m_pMovieImage->RenderVideoFrame();
		}
	}
	g_pMaterialSystem->OwnGPUResources( bPreviousState );

	pRenderContext->Viewport( 0, 0, m_nScreenWidth, m_nScreenHeight );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();
	pRenderContext->Flush( true );

	vgui::ivgui()->RunFrame();
	vgui::surface()->PaintTraverseEx( vgui::surface()->GetEmbeddedPanel() );

	g_pMaterialSystem->EndFrame();
	g_pMaterialSystem->SwapBuffers();

	if ( !m_FadeInTime && !m_bPlayingStartupMovies && !m_bExiting )
	{
		// stable rendering, start the fade in
		m_FadeInTime = Plat_FloatTime() + 1.0f;
	}

	// check for timeout
	if ( m_Timeout && ( Plat_FloatTime() >= m_Timeout ) )
	{
		m_Timeout = 0;
		OnInactivityTimeout();
	}
	
	if ( m_ExitingFrameCount > 1 && m_pBackgroundMovie->IsFullyReleased() && m_pGames[m_Selection]->m_pMovieImage->IsFullyReleased() )
	{
		// must wait for a few frames to settle to ensure frame buffer is stable
		m_ExitingFrameCount--;
		if ( m_ExitingFrameCount == 1 )
		{
			ExitChooser();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAppChooser::HandleInvite( DWORD nUserId )
{
	// invites are asynchronous and could happen at any time
	if ( m_bInviteAccepted )
	{
		// already accepted
		return;
	}

	// Collect our session data
	XINVITE_INFO inviteInfo;
	DWORD dwError = XInviteGetAcceptedInfo( nUserId, &inviteInfo );
	if ( dwError != ERROR_SUCCESS )
	{
		return;
	}

	// We only care if we're asked to join an Orange Box session
	if ( inviteInfo.dwTitleID != TITLEID_THE_ORANGE_BOX )
	{
		return;
	}

	// Store off the session ID and mark the invite as accepted internally
	m_bInviteAccepted = true;
	m_InviteSessionID = inviteInfo.hostInfo.sessionID;
	m_nInviteUserID = nUserId;
	
	// must wait for the startup movies or attract mode to end
	// FrameTick() will detect the invite and transition
	// the user has accpeted and cannot abort the invite
	EnableInput( false );
	ResetTimeout( 0 );
}

//--------------------------------------------------------------------------------------
// Window Proc
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( iMsg )
	{
		case WM_CLOSE:
			g_bActive = false;
			break;
			
		case WM_DESTROY:
			PostQuitMessage( 0 );
			return 0L;

		case WM_LIVE_INVITE_ACCEPTED:
			g_AppChooserSystem.HandleInvite( LOWORD( lParam ) );
			break;

		case WM_XCONTROLLER_KEY:
			if ( !g_AppChooserSystem.IsInputEnabled() )
			{
				break;
			}

			if ( g_AppChooserSystem.IsStartupMoviePlaying() )
			{
				// some startup movies can be aborted
				switch ( wParam )
				{
				case KEY_XBUTTON_A:
				case KEY_XBUTTON_START:
					if ( LOWORD( lParam ) )
					{
						g_AppChooserSystem.OnStopStartupMovie();
					}
				}
				// no other input is allowed during this state
				break;
			}

			if ( g_AppChooserSystem.IsDemoMoviePlaying() )
			{
				// demo movies can be aborted
				switch ( wParam )
				{
				case KEY_XBUTTON_A:
				case KEY_XBUTTON_START:
					if ( LOWORD( lParam ) )
					{
						g_AppChooserSystem.OnStopDemoMovie();
					}
				}
			}
			else
			{
				if ( LOWORD( lParam ) )
				{
					g_AppChooserSystem.ResetTimeout( INACTIVITY_TIMEOUT );
				}

				switch ( wParam )
				{
				case KEY_XBUTTON_LEFT:
				case KEY_XSTICK1_LEFT:
					if ( LOWORD( lParam ) )
					{
						g_AppChooserSystem.OnSelectionPrevious();
					}
					break;

				case KEY_XBUTTON_RIGHT:
				case KEY_XSTICK1_RIGHT:
					if ( LOWORD( lParam ) )
					{
						g_AppChooserSystem.OnSelectionNext();
					}
					break;

				case KEY_XBUTTON_A:
				case KEY_XBUTTON_START:
					if ( LOWORD( lParam ) )
					{
						g_AppChooserSystem.OnActivateGame();
					}
					break;
				}
			}
			break;
	}
   
   return DefWindowProc( hWnd, iMsg, wParam, lParam );
}

//--------------------------------------------------------------------------------------
// CreateWindow
//
//--------------------------------------------------------------------------------------
bool CAppChooser::CreateWindow( int width, int height, bool bFullScreen )
{
   HWND        hWnd;
   WNDCLASSEX  wndClass;
   DWORD       dwStyle, dwExStyle;
   int         x, y, sx, sy;
   
   if ( ( hWnd = FindWindow( GetAppName(), GetAppName() ) ) != NULL )
   {
	   SetForegroundWindow( hWnd );
	   return true;
   }
   
   wndClass.cbSize = sizeof( wndClass );
   wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
   wndClass.lpfnWndProc = ::WndProc;
   wndClass.cbClsExtra = 0;
   wndClass.cbWndExtra = 0;
   wndClass.hInstance = (HINSTANCE)GetAppInstance();
   wndClass.hIcon = 0;
   wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
   wndClass.hbrBackground = (HBRUSH)COLOR_GRAYTEXT;
   wndClass.lpszMenuName = NULL;
   wndClass.lpszClassName = GetAppName();
   wndClass.hIconSm = 0;
   if ( !RegisterClassEx( &wndClass ) )
   {
	   Error( "Window class registration failed\n" );
	   return false;
   }
   
   if ( bFullScreen )
   {
	   dwExStyle = WS_EX_TOPMOST;
	   dwStyle = WS_POPUP | WS_VISIBLE;
   }
   else
   {
	   dwExStyle = 0;
	   dwStyle = WS_CAPTION | WS_SYSMENU;
   }
	
   x = y = 0;
   sx = width;
   sy = height;

   hWnd = CreateWindowEx(
			dwExStyle,
			GetAppName(),				// window class name
			GetAppName(),				// window caption
			dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, // window style
			x,							// initial x position
			y,							// initial y position
			sx,							// initial x size
			sy,							// initial y size
			NULL,						// parent window handle
			NULL,						// window menu handle
			(HINSTANCE)GetAppInstance(),// program instance handle
			NULL);						// creation parameter
	
	if ( hWnd == NULL )
	{
	   return false;
	}

	m_hWnd = hWnd;
   
	return true;
}

//-----------------------------------------------------------------------------
//	Create
//-----------------------------------------------------------------------------
bool CAppChooser::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "filesystem_stdio.dll",	QUEUEDLOADER_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "vguimatsurface.dll",		VGUI_SURFACE_INTERFACE_VERSION },
		{ "", "" }
	};

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	SpewOutputFunc( g_DefaultSpewFunc );

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );

	// vxconsole - true will block (legacy behavior)
	XBX_InitConsoleMonitor( false );

	if ( !AddSystems( appSystems ) ) 
		return false;

	IMaterialSystem* pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
	{
		Error( "Failed to find %s\n", MATERIAL_SYSTEM_INTERFACE_VERSION );
		return false;
	}

	if ( IsX360() )
	{
		IFileSystem* pFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
		if ( !pFileSystem )
		{
			Error( "Failed to find %s\n", FILESYSTEM_INTERFACE_VERSION );
			return false;
		}
		pFileSystem->LoadModule( "shaderapidx9.dll" );
	}

	pMaterialSystem->SetShaderAPI( "shaderapidx9.dll" );

	return true;
}

//-----------------------------------------------------------------------------
// PreInit
//-----------------------------------------------------------------------------
bool CAppChooser::PreInit()
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem )
	{
		Warning( "Unable to find required interfaces!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
	{
		Error( "Failed to setup search paths\n" );
		return false;
	}

	// Create the main program window and our viewport
	int w = 640;
	int h = 480;
	if ( IsX360() )
	{
		w = GetSystemMetrics( SM_CXSCREEN );
		h = GetSystemMetrics( SM_CYSCREEN );
	}

    if ( !CreateWindow( w, h, false ) )
	{
        ChangeDisplaySettings( 0, 0 );
        Error( "Unable to create main window\n" );
        return false;
	}

	ShowWindow( m_hWnd, SW_SHOWNORMAL );
	UpdateWindow( m_hWnd );
	SetForegroundWindow( m_hWnd );
	SetFocus( m_hWnd );

	XOnlineStartup();

	return true;
}


//-----------------------------------------------------------------------------
// Destroy
//-----------------------------------------------------------------------------
void CAppChooser::Destroy()
{
	XOnlineCleanup();
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------
int CAppChooser::Main()
{   
	if ( !InitMaterialSystem() )
	{
		return 0;
	}

	if ( !InitVGUI() )
	{
		return 0;
	}

	if ( !InitAudio() )
	{
		return 0;
	}

	// post initialization reset
	Reset();

	// Setup a listener for invites
	XBX_NotifyCreateListener( XNOTIFY_LIVE );

	MSG msg;
    while ( g_bActive == TRUE )
	{
		// Pump the XBox notifications
		XBX_ProcessEvents();

		while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
            TranslateMessage( &msg );
            DispatchMessage( &msg );
		}

		g_pInputSystem->PollInputState();
		RenderScene();
	}

	ShutdownVGUI();
	ShutdownAudio();
	Destroy();

	return 0;
}

//-----------------------------------------------------------------------------
// The entry point for the application
//-----------------------------------------------------------------------------
extern "C" __declspec(dllexport) int AppChooserMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	CommandLine()->CreateCmdLine( lpCmdLine );

	SetAppInstance( hInstance );

	CSteamApplication steamApplication( &g_AppChooserSystem );
	steamApplication.Run();

	return 0;
}
