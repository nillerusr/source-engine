//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#if defined( USE_SDL )
#undef PROTECTED_THINGS_ENABLE
#include "SDL.h"
#include "SDL_syswm.h"
#endif

#if defined( _WIN32 ) && !defined( _X360 )
#include "winlite.h"
#elif defined(POSIX)
typedef void *HDC;
#endif

#include "appframework/ilaunchermgr.h"

#include "basetypes.h"
#include "sysexternal.h"
#include "cmd.h"
#include "modelloader.h"
#include "gl_matsysiface.h"
#include "vmodes.h"
#include "modes.h"
#include "ivideomode.h"
#include "igame.h"
#include "iengine.h"
#include "engine_launcher_api.h"
#include "iregistry.h"
#include "common.h"
#include "tier0/icommandline.h"
#include "cl_main.h"
#include "filesystem_engine.h"
#include "host.h"
#include "gl_model_private.h"
#include "bitmap/tgawriter.h"
#include "vtf/vtf.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "jpeglib/jpeglib.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "gl_shader.h"
#include "sys_dll.h"
#include "materialsystem/imaterial.h"
#include "IHammer.h"
#include "sourcevr/isourcevirtualreality.h"
#include "tier2/tier2.h"
#include "tier2/renderutils.h"
#include "tier0/etwprof.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#else
#include "xbox/xboxstubs.h"
#endif
#include "video/ivideoservices.h"
#if !defined(NO_STEAM)
#include "cl_steamauth.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

void CL_GetBackgroundLevelName(char *pszBackgroundName, int bufSize, bool bMapName);
void ClientDLL_HudVidInit( void );

ConVar cl_savescreenshotstosteam( "cl_savescreenshotstosteam", "0", FCVAR_HIDDEN, "Saves screenshots to the Steam's screenshot library" );
ConVar cl_screenshotusertag( "cl_screenshotusertag", "", FCVAR_HIDDEN, "User to tag in the screenshot" );
ConVar cl_screenshotlocation( "cl_screenshotlocation", "", FCVAR_HIDDEN, "Location to tag the screenshot with" );

//-----------------------------------------------------------------------------
// HDRFIXME: move this somewhere else.
//-----------------------------------------------------------------------------
static void PFMWrite( float *pFloatImage, const char *pFilename, int width, int height )
{
    FileHandle_t fp;
    fp = g_pFileSystem->Open( pFilename, "wb" );
    g_pFileSystem->FPrintf( fp, "PF\n%d %d\n-1.000000\n", width, height );
    int i;
    for( i = height-1; i >= 0; i-- )
    {
        float *pRow = &pFloatImage[3 * width * i];
        g_pFileSystem->Write( pRow, width * sizeof( float ) * 3, fp );
    }
    g_pFileSystem->Close( fp );
}

//-----------------------------------------------------------------------------
// Purpose: Functionality shared by all video modes
//-----------------------------------------------------------------------------
class CVideoMode_Common : public IVideoMode
{
public:
                        CVideoMode_Common( void );
    virtual             ~CVideoMode_Common( void );

    // Methods of IVideoMode
    virtual bool        Init( );
    virtual void        Shutdown( void );
    virtual vmode_t     *GetMode( int num );
    virtual int         GetModeCount( void );
    virtual bool        IsWindowedMode( void ) const;
    virtual void        UpdateWindowPosition( void );
    virtual void        RestoreVideo( void );
    virtual void        ReleaseVideo( void );
    virtual void        DrawNullBackground( void *hdc, int w, int h );
    virtual void        InvalidateWindow();
    virtual void        DrawStartupGraphic();
    virtual bool        CreateGameWindow( int nWidth, int nHeight, bool bWindowed );
    virtual int         GetModeWidth( void ) const;
    virtual int         GetModeHeight( void ) const;
	virtual int			GetModeStereoWidth() const;
	virtual int			GetModeStereoHeight() const;
	virtual int			GetModeUIWidth() const  OVERRIDE;
	virtual int			GetModeUIHeight() const OVERRIDE;
    virtual const vrect_t &GetClientViewRect( ) const;
    virtual void        SetClientViewRect( const vrect_t &viewRect );
    virtual void        MarkClientViewRectDirty();
    virtual void        TakeSnapshotTGA( const char *pFileName );
    virtual void        TakeSnapshotTGARect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, bool bPFM, CubeMapFaceIndex_t faceIndex );
    virtual void        WriteMovieFrame( const MovieInfo_t& info );
    virtual void        TakeSnapshotJPEG( const char *pFileName, int quality );
    virtual bool        TakeSnapshotJPEGToBuffer( CUtlBuffer& buf, int quality );
protected:
    bool                GetInitialized( ) const;
    void                SetInitialized( bool init );
    void                AdjustWindow( int nWidth, int nHeight, int nBPP, bool bWindowed );
    void                ResetCurrentModeForNewResolution( int width, int height, bool bWindowed );
    int                 GetModeBPP( ) const { return 32; }
    void                DrawStartupVideo();
    void                ComputeStartupGraphicName( char *pBuf, int nBufLen );
	void				WriteScreenshotToSteam( uint8 *pImage, int cubImage, int width, int height );
	void				AddScreenshotToSteam( const char *pchFilenameJpeg, int width, int height );
#if !defined(NO_STEAM)
	void				ApplySteamScreenshotTags( ScreenshotHandle hScreenshot );
#endif

    // Finds the video mode in the list of video modes 
    int                 FindVideoMode( int nDesiredWidth, int nDesiredHeight, bool bWindowed );

    // Purpose: Returns the optimal refresh rate for the specified mode
    int                 GetRefreshRateForMode( const vmode_t *pMode );

    // Inline accessors
    vmode_t&            DefaultVideoMode();
    vmode_t&            RequestedWindowVideoMode();

private:
    // Purpose: Loads the startup graphic
    void                SetupStartupGraphic();
    void                CenterEngineWindow(void *hWndCenter, int width, int height);
    void                DrawStartupGraphic( HWND window );
    void                BlitGraphicToHDC(HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1);
    void                BlitGraphicToHDCWithAlpha(HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1);
    IVTFTexture         *LoadVTF( CUtlBuffer &temp, const char *szFileName );
    void                RecomputeClientViewRect();

    // Overridden by derived classes
    virtual void        ReleaseFullScreen( void );
    virtual void        ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP );
    virtual void        ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format );

    // PFM screenshot methods
    ITexture *GetBuildCubemaps16BitTexture( void );
    ITexture *GetFullFrameFB0( void );

    void BlitHiLoScreenBuffersTo16Bit( void );
    void TakeSnapshotPFMRect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, CubeMapFaceIndex_t faceIndex );

protected:
    enum
    {
#if !defined( _X360 )
        MAX_MODE_LIST = 512
#else
        MAX_MODE_LIST = 2
#endif
    };

    enum
    {
        VIDEO_MODE_DEFAULT = -1,
        VIDEO_MODE_REQUESTED_WINDOW_SIZE = -2,
        CUSTOM_VIDEO_MODES = 2
    };

    // Master mode list
    int                 m_nNumModes;
    vmode_t             m_rgModeList[MAX_MODE_LIST];
    vmode_t             m_nCustomModeList[CUSTOM_VIDEO_MODES];
    bool                m_bInitialized;
    bool                m_bPlayedStartupVideo;

    // Renderable surface information
    int                 m_nModeWidth;
    int                 m_nModeHeight;
    int                 m_nStereoWidth;
    int                 m_nStereoHeight;
    int                 m_nUIWidth;
    int                 m_nUIHeight;
	int					m_nVROverrideX;
	int					m_nVROverrideY;
#if defined( USE_SDL )
	int					m_nRenderWidth;
	int					m_nRenderHeight;
#endif
    bool                m_bWindowed;
    bool                m_bSetModeOnce;
	bool				m_bVROverride;

    // Client view rectangle
    vrect_t             m_ClientViewRect;
    bool                m_bClientViewRectDirty;

    // loading image
    IVTFTexture         *m_pBackgroundTexture;
    IVTFTexture         *m_pLoadingTexture;
};


//-----------------------------------------------------------------------------
// Inline accessors
//-----------------------------------------------------------------------------
inline vmode_t& CVideoMode_Common::DefaultVideoMode()
{
    return m_nCustomModeList[ - VIDEO_MODE_DEFAULT - 1 ];
}

inline vmode_t& CVideoMode_Common::RequestedWindowVideoMode()
{
    return m_nCustomModeList[ - VIDEO_MODE_REQUESTED_WINDOW_SIZE - 1 ];
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVideoMode_Common::CVideoMode_Common( void )
{
    m_nNumModes    = 0;
    m_bInitialized = false;

    DefaultVideoMode().width  = 640;
    DefaultVideoMode().height = 480;
    DefaultVideoMode().bpp    = 32;
    DefaultVideoMode().refreshRate = 0;

    RequestedWindowVideoMode().width  = -1;
    RequestedWindowVideoMode().height = -1;
    RequestedWindowVideoMode().bpp    = 32;
    RequestedWindowVideoMode().refreshRate = 0;
    
    m_bClientViewRectDirty = false;
    m_pBackgroundTexture   = NULL;
    m_pLoadingTexture      = NULL;
    m_bWindowed            = false;
    m_nModeWidth           = IsPC() ? 1024 : 640;
    m_nModeHeight          = IsPC() ? 768 : 480;
	m_bVROverride = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVideoMode_Common::~CVideoMode_Common( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVideoMode_Common::GetInitialized( void ) const
{
    return m_bInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : init - 
//-----------------------------------------------------------------------------
void CVideoMode_Common::SetInitialized( bool init )
{
    m_bInitialized = init;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVideoMode_Common::IsWindowedMode( void ) const
{
    return m_bWindowed;
}


//-----------------------------------------------------------------------------
// Returns the video mode width + height.
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetModeWidth( void ) const
{
    return m_nModeWidth;
}

int CVideoMode_Common::GetModeHeight( void ) const
{
    return m_nModeHeight;
}


//-----------------------------------------------------------------------------
// Returns the video mode width + height for one stereo view.
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetModeStereoWidth( void ) const
{
	return m_nStereoWidth;
}

int CVideoMode_Common::GetModeStereoHeight( void ) const
{
	return m_nStereoHeight;
}


//-----------------------------------------------------------------------------
// Returns the video mode full screen UI width + height.
//-----------------------------------------------------------------------------

int CVideoMode_Common::GetModeUIWidth( void ) const
{
	return m_nUIWidth;
}

int CVideoMode_Common::GetModeUIHeight( void ) const
{
    return m_nUIHeight;
}


//-----------------------------------------------------------------------------
// Returns the enumerated video mode
//-----------------------------------------------------------------------------
vmode_t *CVideoMode_Common::GetMode( int num )
{
    if ( num < 0 )
        return &m_nCustomModeList[-num - 1];

    if ( num >= m_nNumModes )
        return &DefaultVideoMode();

    return &m_rgModeList[num];
}


//-----------------------------------------------------------------------------
// Returns the number of fullscreen video modes 
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetModeCount( void )
{
    return m_nNumModes;
}


//-----------------------------------------------------------------------------
// Purpose: Compares video modes so we can sort the list
// Input  : *arg1 - 
//          *arg2 - 
// Output : static int
//-----------------------------------------------------------------------------
static int __cdecl VideoModeCompare( const void *arg1, const void *arg2 )
{
    vmode_t *m1, *m2;

    m1 = (vmode_t *)arg1;
    m2 = (vmode_t *)arg2;

    if ( m1->width < m2->width )
    {
        return -1;
    }

    if ( m1->width == m2->width )
    {
        if ( m1->height < m2->height )
        {
            return -1;
        }

        if ( m1->height > m2->height )
        {
            return 1;
        }

        return 0;
    }

    return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CVideoMode_Common::Init( )
{   
    return true;
}


//-----------------------------------------------------------------------------
// Finds the video mode in the list of video modes 
//-----------------------------------------------------------------------------
int CVideoMode_Common::FindVideoMode( int nDesiredWidth, int nDesiredHeight, bool bWindowed )
{
#if defined( USE_SDL )

	// If we want to scale the 3D portion of the game and leave the UI at the same res, then
	//	re-enable this code. Not that on retina displays the UI will be super small and that
	//	should probably be fixed.
#if 0
	static ConVarRef mat_viewportscale( "mat_viewportscale" );

	if ( !bWindowed )
	{
		m_nRenderWidth = nDesiredWidth;
		m_nRenderHeight = nDesiredHeight;

		uint nWidth, nHeight, nRefreshHz;

		g_pLauncherMgr->GetNativeDisplayInfo( -1, nWidth, nHeight, nRefreshHz );

		for ( int i = 0; i < m_nNumModes; i++)
		{
			if ( m_rgModeList[i].width != ( int )nWidth )
			{
				continue;
			}

			if ( m_rgModeList[i].height != ( int )nHeight )
			{
				continue;
			}

			if ( m_rgModeList[i].refreshRate != ( int )nRefreshHz )
			{
				continue;
			}

			mat_viewportscale.SetValue( ( float )nDesiredWidth / ( float )nWidth );
			return i;
		}

		Assert( 0 );	// we should have found our native resolution, why not???
	}
	else
	{
		mat_viewportscale.SetValue( 1.0f );
	}
#endif // 0

#endif // USE_SDL

    // Check the default window size..
    if ( ( nDesiredWidth == DefaultVideoMode().width) && (nDesiredHeight == DefaultVideoMode().height) )
        return VIDEO_MODE_DEFAULT;

    // Check the requested window size, but only if we're running windowed
    if ( bWindowed )
    {
        if ( ( nDesiredWidth == RequestedWindowVideoMode().width) && (nDesiredHeight == RequestedWindowVideoMode().height) )
            return VIDEO_MODE_REQUESTED_WINDOW_SIZE;
    }

    int i;
    int iOK = VIDEO_MODE_DEFAULT;
    for ( i = 0; i < m_nNumModes; i++)
    {
        // Match width first
        if ( m_rgModeList[i].width != nDesiredWidth )
            continue;
        
        iOK = i;

        if ( m_rgModeList[i].height != nDesiredHeight )
            continue;

        // Found a decent match
        break;
    }

    // No match, use mode 0
    if ( i >= m_nNumModes )
    {
        if ( iOK != VIDEO_MODE_DEFAULT )
        {
            i = iOK;
        }
        else
        {
            i = 0;
        }
    }

    return i;
}


//-----------------------------------------------------------------------------
// Choose the actual video mode based on the available modes
//-----------------------------------------------------------------------------
void CVideoMode_Common::ResetCurrentModeForNewResolution( int nWidth, int nHeight, bool bWindowed )
{
    // Fill in vid structure for the mode
    int nGameMode = FindVideoMode( nWidth, nHeight, bWindowed );
    vmode_t *pMode = GetMode( nGameMode );

	// default to non-VR values
	m_bWindowed = bWindowed;
	m_nModeWidth = pMode->width;
	m_nModeHeight = pMode->height;
	m_nUIWidth = pMode->width;
	m_nUIHeight = pMode->height;
	m_nStereoWidth = pMode->width;
	m_nStereoHeight = pMode->height;

	// assume we won't be overriding the position
	m_bVROverride = false;

	if ( UseVR() || ShouldForceVRActive() )
	{
		g_pSourceVR->GetViewportBounds( ISourceVirtualReality::VREye_Left, NULL, NULL, &m_nStereoWidth, &m_nStereoHeight );
		VRRect_t vrBounds;
		if( g_pSourceVR->GetDisplayBounds( &vrBounds ) )
		{
			ConVarRef vr_force_windowed( "vr_force_windowed" );

			RequestedWindowVideoMode().width = m_nModeWidth = vrBounds.nWidth;
			RequestedWindowVideoMode().height = m_nModeHeight = vrBounds.nHeight;
			m_bVROverride = true;
			m_bWindowed = vr_force_windowed.GetBool();


			// This is the smallest size the the UI in source games can handle.
			m_nUIWidth =	640;
			m_nUIHeight =	480;

#if defined( WIN32 ) && !defined( USE_SDL )
			m_nVROverrideX = vrBounds.nX;
			m_nVROverrideY = vrBounds.nY;
#elif defined( USE_SDL )
			for ( int i = 0; i < SDL_GetNumVideoDisplays(); i++ )
			{
				SDL_Rect sdlRect;
				SDL_GetDisplayBounds( i, &sdlRect );

				if( sdlRect.x == vrBounds.nX && sdlRect.y == vrBounds.nY 
					&& sdlRect.w == vrBounds.nWidth && sdlRect.h == vrBounds.nHeight )
				{
					static ConVarRef sdl_displayindex( "sdl_displayindex" );
					sdl_displayindex.SetValue( i );
					break;
				}
			}
#endif
		}
	}
	else if( materials->GetCurrentConfigForVideoCard().m_nVRModeAdapter == materials->GetCurrentAdapter() )
	{
		// if we aren't in VR mode but we do have a VR mode adapter set, we must not be full
		// screen because that would show up on the HMD
		m_bWindowed = true;
	}
}


//-----------------------------------------------------------------------------
// Creates the game window, plays the startup movie
//-----------------------------------------------------------------------------
bool CVideoMode_Common::CreateGameWindow( int nWidth, int nHeight, bool bWindowed )
{
    COM_TimestampedLog( "CVideoMode_Common::Init  CreateGameWindow" );

	if ( ShouldForceVRActive() )
	{
		// First make sure we're running a compatible version of DirectX
		ConVarRef mat_dxlevel( "mat_dxlevel" );
		if ( mat_dxlevel.IsValid() )
		{
			if ( mat_dxlevel.GetInt() < 90 )
			{
				Msg( "VR mode does not work with DirectX8.\nPlease use at least \"-dxlevel 90\" or higher.\n" );
				return false;
			}
		}

		VRRect_t bounds;
		g_pSourceVR->GetDisplayBounds( &bounds );

		nWidth = bounds.nWidth;
		nHeight = bounds.nHeight;

		m_nVROverrideX = bounds.nX;
		m_nVROverrideY = bounds.nY;
	}

	// This allows you to have a window of any size.
    // Requires you to set both width and height for the window and
    // that you start in windowed mode
    if ( bWindowed && nWidth && nHeight )
    {
        // FIXME: There's some ordering issues related to the config record
        // and reading the command-line. Would be nice for just one place where this is done.
        RequestedWindowVideoMode().width = nWidth;
        RequestedWindowVideoMode().height = nHeight;
    }
    
    if ( !InEditMode() )
    {
        // Fill in vid structure for the mode.
        // Note: ModeWidth/Height may *not* match requested nWidth/nHeight
        ResetCurrentModeForNewResolution( nWidth, nHeight, bWindowed );

		COM_TimestampedLog( "CreateGameWindow - Start" );
        // When running in stand-alone mode, create your own window 
        if ( !game->CreateGameWindow() )
            return false;
		COM_TimestampedLog( "CreateGameWindow - Finish" );

        // Re-size and re-center the window
        AdjustWindow( GetModeWidth(), GetModeHeight(), GetModeBPP(), IsWindowedMode() );

		COM_TimestampedLog( "SetMode - Start" );
        // Set the mode and let the materialsystem take over
        if ( !SetMode( GetModeWidth(), GetModeHeight(), IsWindowedMode() ) )
            return false;

#if defined( USE_SDL ) && 0
		static ConVarRef mat_viewportscale( "mat_viewportscale" );

		if ( !bWindowed )
		{
			m_nRenderWidth = nWidth;
			m_nRenderHeight = nHeight;

			mat_viewportscale.SetValue(  ( float )nWidth / ( float )GetModeWidth() );
		}
#endif

		COM_TimestampedLog( "SetMode - Finish" );

        // Play our videos for the background after the render device has been initialized
        DrawStartupVideo();

		COM_TimestampedLog( "DrawStartupGraphic - Start" );
        // Play our videos or display our temp image for the background
        DrawStartupGraphic();

		COM_TimestampedLog( "DrawStartupGraphic - Finish" );
    }

    return true;
}

//-----------------------------------------------------------------------------
// Purpose: loads a vtf, through the temporary buffer
//-----------------------------------------------------------------------------
IVTFTexture *CVideoMode_Common::LoadVTF( CUtlBuffer &temp, const char *szFileName )
{
    if ( !g_pFileSystem->ReadFile( szFileName, NULL, temp ) )
        return NULL;

    IVTFTexture *texture = CreateVTFTexture();
    if ( !texture->Unserialize( temp ) )
    {
        Error( "Invalid or corrupt background texture %s\n", szFileName );
        return NULL;
    }
    texture->ConvertImageFormat( IMAGE_FORMAT_RGBA8888, false );
    return texture;
}

//-----------------------------------------------------------------------------
// Computes the startup graphic name
//-----------------------------------------------------------------------------
void CVideoMode_Common::ComputeStartupGraphicName( char *pBuf, int nBufLen )
{
    char szBackgroundName[_MAX_PATH];
    CL_GetBackgroundLevelName( szBackgroundName, sizeof(szBackgroundName), false );

    float aspectRatio = (float)GetModeStereoWidth() / GetModeStereoHeight();
    if ( aspectRatio >= 1.6f )
    {
        // use the widescreen version
        Q_snprintf( pBuf, nBufLen, "materials/console/%s_widescreen.vtf", szBackgroundName );
    }
    else
    {
        Q_snprintf( pBuf, nBufLen, "materials/console/%s.vtf", szBackgroundName );
    }

    if ( !g_pFileSystem->FileExists( pBuf, "GAME" ) )
    {
        Q_strncpy( pBuf, ( aspectRatio >= 1.6f ) ? "materials/console/background01_widescreen.vtf" : "materials/console/background01.vtf", nBufLen );
    }
}


//-----------------------------------------------------------------------------
// Writes a screenshot to Steam screenshot library given an RGB buffer
// and applies any tags that have been set for it
//-----------------------------------------------------------------------------
void CVideoMode_Common::WriteScreenshotToSteam( uint8 *pImage, int cubImage, int width, int height )
{
#if !defined(NO_STEAM)
	if ( cl_savescreenshotstosteam.GetBool() )
	{
		if ( Steam3Client().SteamScreenshots() )
		{
			ScreenshotHandle hScreenshot = Steam3Client().SteamScreenshots()->WriteScreenshot( pImage, cubImage, width, height );
			ApplySteamScreenshotTags( hScreenshot );
		}
	}
	cl_screenshotusertag.SetValue(0);
	cl_screenshotlocation.SetValue("");
#endif
}


//-----------------------------------------------------------------------------
// Adds a screenshot to the Steam screenshot library from disk
// and applies any tags that have been set for it
//-----------------------------------------------------------------------------
void CVideoMode_Common::AddScreenshotToSteam( const char *pchFilename, int width, int height )
{
#if !defined(NO_STEAM)
	if ( cl_savescreenshotstosteam.GetBool() )
	{
		if ( Steam3Client().SteamScreenshots() )
		{
			ScreenshotHandle hScreenshot = Steam3Client().SteamScreenshots()->AddScreenshotToLibrary( pchFilename, NULL, width, height );
			ApplySteamScreenshotTags( hScreenshot );
		}
	}
	cl_screenshotusertag.SetValue(0);
	cl_screenshotlocation.SetValue("");
#endif
}


//-----------------------------------------------------------------------------
// Applies tags to a screenshot for the Steam screenshot library, which are
// passed in through convars
//-----------------------------------------------------------------------------
#if !defined(NO_STEAM)
void CVideoMode_Common::ApplySteamScreenshotTags( ScreenshotHandle hScreenshot )
{
	if ( hScreenshot != INVALID_SCREENSHOT_HANDLE )
	{
		if ( cl_screenshotusertag.GetBool() )
		{
			if ( Steam3Client().SteamUtils() )
			{
				CSteamID steamID( cl_screenshotusertag.GetInt(), Steam3Client().SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
				Steam3Client().SteamScreenshots()->TagUser( hScreenshot, steamID );
			}
		}
		const char *pchLocation = cl_screenshotlocation.GetString();
		if ( pchLocation && pchLocation[0] )
		{
			Steam3Client().SteamScreenshots()->SetLocation( hScreenshot, pchLocation );
		}
	}	
}
#endif

void CVideoMode_Common::SetupStartupGraphic()
{
    COM_TimestampedLog( "CVideoMode_Common::Init  SetupStartupGraphic" );

    char szBackgroundName[_MAX_PATH];
    CL_GetBackgroundLevelName( szBackgroundName, sizeof(szBackgroundName), false );

    // get the image to load
    char material[_MAX_PATH];
    CUtlBuffer buf;

    float aspectRatio = (float)GetModeWidth() / GetModeHeight();
    if ( aspectRatio >= 1.6f )
    {
        // use the widescreen version
        Q_snprintf( material, sizeof(material), 
            "materials/console/%s_widescreen.vtf", szBackgroundName );
    }
    else
    {
        Q_snprintf( material, sizeof(material), 
            "materials/console/%s.vtf", szBackgroundName );
    }

    // load in the background vtf
	buf.Clear();
    m_pBackgroundTexture = LoadVTF( buf, material );
    if ( !m_pBackgroundTexture )
    {
        // fallback to opening just the default background
        m_pBackgroundTexture = LoadVTF( buf, ( aspectRatio >= 1.6f ) ? "materials/console/background01_widescreen.vtf" : "materials/console/background01.vtf" );
        if ( !m_pBackgroundTexture )
        {
            Error( "Can't find background image '%s'\n", material );
            return;
        }
    }

    // loading.vtf
	buf.Clear();	// added this Clear() because we saw cases where LoadVTF was not emptying the buf fully in the above section
    m_pLoadingTexture = LoadVTF( buf, "materials/console/startup_loading.vtf" );
    if ( !m_pLoadingTexture )
    {
        Error( "Can't find background image materials/console/startup_loading.vtf\n" );
        return;
    }
}


//-----------------------------------------------------------------------------
// Purpose: Renders the startup video into the HWND
//-----------------------------------------------------------------------------
void CVideoMode_Common::DrawStartupVideo()
{
    if ( IsX360() )
        return;

	CETWScope timer( "CVideoMode_Common::DrawStartupGraphic" );

    // render an avi, if we have one
	if ( !m_bPlayedStartupVideo && !InEditMode() && !ShouldForceVRActive() )
    {
        game->PlayStartupVideos();
        m_bPlayedStartupVideo = true;
    }
}


//-----------------------------------------------------------------------------
// Purpose: Renders the startup graphic into the HWND
//-----------------------------------------------------------------------------
void CVideoMode_Common::DrawStartupGraphic()
{
    if ( IsX360() )
        return;

	char debugstartup = CommandLine()->FindParm("-debugstartupscreen");

    SetupStartupGraphic();

    if ( !m_pBackgroundTexture || !m_pLoadingTexture )
        return;

    CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

    char pStartupGraphicName[MAX_PATH];
    ComputeStartupGraphicName( pStartupGraphicName, sizeof(pStartupGraphicName) );

	if(debugstartup)
	{
		// slam the startup graphic name for sanity - take your pick
		strcpy( pStartupGraphicName, "materials/console/background01.vtf");
		//strcpy( pStartupGraphicName, "materials/console/testramp.vtf");
	}
	
    // Allocate a white material
    KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
    pVMTKeyValues->SetString( "$basetexture", pStartupGraphicName + 10 );
    pVMTKeyValues->SetInt( "$ignorez", 1 );
    pVMTKeyValues->SetInt( "$nofog", 1 );
    pVMTKeyValues->SetInt( "$no_fullbright", 1 );
    pVMTKeyValues->SetInt( "$nocull", 1 );
    IMaterial *pMaterial = g_pMaterialSystem->CreateMaterial( "__background", pVMTKeyValues );

    pVMTKeyValues = new KeyValues( "UnlitGeneric" );
    pVMTKeyValues->SetString( "$basetexture", "Console/startup_loading.vtf" );
    pVMTKeyValues->SetInt( "$translucent", 1 );
    pVMTKeyValues->SetInt( "$ignorez", 1 );
    pVMTKeyValues->SetInt( "$nofog", 1 );
    pVMTKeyValues->SetInt( "$no_fullbright", 1 );
    pVMTKeyValues->SetInt( "$nocull", 1 );
    IMaterial *pLoadingMaterial = g_pMaterialSystem->CreateMaterial( "__loading", pVMTKeyValues );

    int w = GetModeStereoWidth();
    int h = GetModeStereoHeight();
    int tw = m_pBackgroundTexture->Width();
    int th = m_pBackgroundTexture->Height();
    int lw = m_pLoadingTexture->Width();
    int lh = m_pLoadingTexture->Height();

	if (debugstartup)
	{
		for ( int repeat = 0; repeat<100000; repeat++)
		{
			pRenderContext->Viewport( 0, 0, w, h );
			pRenderContext->DepthRange( 0, 1 );
			pRenderContext->ClearColor3ub( 0, (repeat & 0x7) << 3, 0 );
			pRenderContext->ClearBuffers( true, true, true );
			pRenderContext->SetToneMappingScaleLinear( Vector(1,1,1) );

			if(1)	// draw normal BK
			{
				float depth = 0.55f;
				int slide = (repeat) % 200;	// 100 down and 100 up
				if (slide > 100)
				{
					slide = 200-slide;		// aka 100-(slide-100).
				}
				
				// stop sliding about
				slide = 0;
				
				DrawScreenSpaceRectangle( pMaterial, 0, 0+slide, w, h-50, 0, 0, tw-1, th-1, tw, th, NULL,1,1,depth );
				DrawScreenSpaceRectangle( pLoadingMaterial, w-lw, h-lh+slide/2, lw, lh, 0, 0, lw-1, lh-1, lw, lh, NULL,1,1,depth-0.1 );
			}

			if(0)
			{
					// draw a grid too
				int grid_size = 8;
				float depthacc = 0.0;
				float depthinc = 1.0 / (float)((grid_size * grid_size)+1);
				
				for( int x = 0; x<grid_size; x++)
				{
					float cornerx = ((float)x) * 20.0f;
					
					for( int y=0; y<grid_size; y++)
					{
						float cornery = ((float)y) * 20.0f;

						//if (! ((x^y) & 1) )
						{
							DrawScreenSpaceRectangle( pMaterial, 10.0f+cornerx,10.0f+ cornery, 15, 15, 0, 0, tw-1, th-1, tw, th, NULL,1,1, depthacc );
						}
						
						depthacc += depthinc;
					}
				}
			}

			g_pMaterialSystem->SwapBuffers();			
		}
	}
	else
	{
		pRenderContext->Viewport( 0, 0, w, h );
		pRenderContext->DepthRange( 0, 1 );
		pRenderContext->SetToneMappingScaleLinear( Vector(1,1,1) );
		
		float depth = 0.5f;

		// Make sure we clear both front & back buffer.
		for (int i = 0; i < 2; ++i)
		{
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, true, true );
			DrawScreenSpaceRectangle( pMaterial, 0, 0, w, h, 0, 0, tw-1, th-1, tw, th, NULL,1,1,depth );
			DrawScreenSpaceRectangle( pLoadingMaterial, w-lw, h-lh, lw, lh, 0, 0, lw-1, lh-1, lw, lh, NULL,1,1,depth );
			g_pMaterialSystem->SwapBuffers();
		}
	}

#ifdef DX_TO_GL_ABSTRACTION
	g_pMaterialSystem->DoStartupShaderPreloading();
#endif

    pMaterial->Release();
    pLoadingMaterial->Release();

    // release graphics
    DestroyVTFTexture( m_pBackgroundTexture );
    m_pBackgroundTexture = NULL;
    DestroyVTFTexture( m_pLoadingTexture );
    m_pLoadingTexture = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Blits an image to the loading window hdc
//-----------------------------------------------------------------------------
void CVideoMode_Common::BlitGraphicToHDCWithAlpha(HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1)
{
#ifdef WIN32
    if ( IsX360() )
        return;

    int x = x0;
    int y = y0;
    int wide = x1 - x0;
    int tall = y1 - y0;

    Assert(imageWidth == wide && imageHeight == tall);

    int texwby4 = imageWidth << 2;

    for ( int v = 0; v < tall; v++ )
    {
        int *src = (int *)(rgba + (v * texwby4));
        int xaccum = 0;

        for ( int u = 0; u < wide; u++ )
        {
            byte *xsrc = (byte *)(src + xaccum);
            if (xsrc[3])
            {
                ::SetPixel(hdc, x + u, y + v, RGB(xsrc[0], xsrc[1], xsrc[2]));
            }
            xaccum += 1;
        }
    }
#else
    Assert( !"Impl me" );
#endif
}

void CVideoMode_Common::InvalidateWindow()
{
    if ( CommandLine()->FindParm( "-noshaderapi" ) )
    {
#if defined( USE_SDL )
		SDL_Event fake;
		memset(&fake, '\0', sizeof (SDL_Event));
		fake.type = SDL_WINDOWEVENT;
		fake.window.windowID = SDL_GetWindowID( (SDL_Window *) g_pLauncherMgr->GetWindowRef() );
		fake.window.event = SDL_WINDOWEVENT_EXPOSED;
		SDL_PushEvent(&fake);
#else
		InvalidateRect( (HWND)game->GetMainWindow(), NULL, FALSE );
#endif
    }
}

void CVideoMode_Common::DrawNullBackground( void *hHDC, int w, int h )
{
#ifdef WIN32
    if ( IsX360() )
        return;

    HDC hdc = (HDC)hHDC;

    // Show a message if running without renderer..
    if ( CommandLine()->FindParm( "-noshaderapi" ) )
    {
        HFONT fnt = CreateFontA( -18, 
         0,
         0,
         0,
         FW_NORMAL,
         FALSE,
         FALSE,
         FALSE,
         ANSI_CHARSET,
         OUT_TT_PRECIS,
         CLIP_DEFAULT_PRECIS,
         ANTIALIASED_QUALITY,
         DEFAULT_PITCH,
         "Arial" );

        HFONT oldFont = (HFONT)SelectObject( hdc, fnt );
        int oldBkMode = SetBkMode( hdc, TRANSPARENT );
        COLORREF oldFgColor = SetTextColor( hdc, RGB( 255, 255, 255 ) );

        HBRUSH br = CreateSolidBrush( RGB( 0, 0, 0  ) );
        HBRUSH oldBr = (HBRUSH)SelectObject( hdc, br );
        Rectangle( hdc, 0, 0, w, h );
        
        RECT rc;
        rc.left = 0;
        rc.top = 0;
        rc.right = w;
        rc.bottom = h;

        DrawText( hdc, "Running with -noshaderapi", -1, &rc, DT_NOPREFIX | DT_VCENTER | DT_CENTER | DT_SINGLELINE  );

        rc.top = rc.bottom - 30;

        if ( host_state.worldmodel != NULL )
        {
            rc.left += 10;
            DrawText( hdc, modelloader->GetName( host_state.worldmodel ), -1, &rc, DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE  );
        }

        SetTextColor( hdc, oldFgColor );

        SelectObject( hdc, oldBr );
        SetBkMode( hdc, oldBkMode );
        SelectObject( hdc, oldFont );

        DeleteObject( br );
        DeleteObject( fnt );
    }
#else
    Assert( !"Impl me" );
#endif

}

#ifndef _WIN32

typedef unsigned char BYTE;
typedef signed long LONG;
typedef unsigned long ULONG;

typedef char * LPSTR;

typedef struct tagBITMAPINFOHEADER{
    DWORD      biSize;
    LONG       biWidth;
    LONG       biHeight;
    WORD       biPlanes;
    WORD       biBitCount;
    DWORD      biCompression;
    DWORD      biSizeImage;
    LONG       biXPelsPerMeter;
    LONG       biYPelsPerMeter;
    DWORD      biClrUsed;
    DWORD      biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagBITMAPFILEHEADER {
    WORD    bfType;
    DWORD   bfSize;
    WORD    bfReserved1;
    WORD    bfReserved2;
    DWORD   bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagRGBQUAD {
    BYTE    rgbBlue;
    BYTE    rgbGreen;
    BYTE    rgbRed;
    BYTE    rgbReserved;
} RGBQUAD;

/* constants for the biCompression field */
#define BI_RGB        0L
#define BI_RLE8       1L
#define BI_RLE4       2L
#define BI_BITFIELDS  3L

#if 0
typedef struct _GUID
{
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID;

#endif
typedef GUID UUID;

#endif //WIN32
//-----------------------------------------------------------------------------
// Purpose: Blits an image to the loading window hdc
//-----------------------------------------------------------------------------
void CVideoMode_Common::BlitGraphicToHDC(HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1)
{
    if ( IsX360() )
        return;

#ifdef WIN32
    int x = x0;
    int y = y0;
    int wide = x1 - x0;
    int tall = y1 - y0;

    // Needs to be a multiple of 4
    int dibwide = ( wide + 3 ) & ~3;

    Assert(rgba);
    int texwby4 = imageWidth << 2;

    double st = Plat_FloatTime();

    void *destBits = NULL;

    HBITMAP bm;
    BITMAPINFO bmi;
    Q_memset( &bmi, 0, sizeof( bmi ) );

    BITMAPINFOHEADER *hdr = &bmi.bmiHeader;

    hdr->biSize = sizeof( *hdr );
    hdr->biWidth = dibwide;
    hdr->biHeight = -tall;  // top down bitmap
    hdr->biBitCount = 24;
    hdr->biPlanes = 1;
    hdr->biCompression = BI_RGB;
    hdr->biSizeImage = dibwide * tall * 3;
    hdr->biXPelsPerMeter = 3780;
    hdr->biYPelsPerMeter = 3780;

    // Create a "source" DC
    HDC tempDC = CreateCompatibleDC( hdc );

    // Create the dibsection bitmap
    bm = CreateDIBSection
    (
        tempDC,                     // handle to DC
        &bmi,                       // bitmap data
        DIB_RGB_COLORS,             // data type indicator
        &destBits,                  // bit values
        NULL,                       // handle to file mapping object
        0                           // offset to bitmap bit values
    );
    
    // Select it into the source DC
    HBITMAP oldBitmap = (HBITMAP)SelectObject( tempDC, bm );

    // Setup for bilinaer filtering. If we don't do this filter here, there will be a big
    // annoying pop when it switches to the vguimatsurface version of the background.
    // We leave room for 14 bits of integer precision, so the image can be up to 16k x 16k.
    const int BILINEAR_FIX_SHIFT = 17;
    const int BILINEAR_FIX_MUL = (1 << BILINEAR_FIX_SHIFT);

    #define FIXED_BLEND( a, b, out, frac ) \
        out[0] = (a[0]*frac + b[0]*(BILINEAR_FIX_MUL-frac)) >> BILINEAR_FIX_SHIFT; \
        out[1] = (a[1]*frac + b[1]*(BILINEAR_FIX_MUL-frac)) >> BILINEAR_FIX_SHIFT; \
        out[2] = (a[2]*frac + b[2]*(BILINEAR_FIX_MUL-frac)) >> BILINEAR_FIX_SHIFT;

    float eps = 0.001f;
    float uMax = imageWidth - 1 - eps;
    float vMax = imageHeight - 1 - eps;

    int fixedBilinearV = 0;
    int bilinearUInc = (int)( (uMax / (dibwide-1)) * BILINEAR_FIX_MUL );
    int bilinearVInc = (int)( (vMax / (tall-1)) * BILINEAR_FIX_MUL );

    for ( int v = 0; v < tall; v++ )
    {
        int iBilinearV = fixedBilinearV >> BILINEAR_FIX_SHIFT;
        int fixedFractionV = fixedBilinearV & (BILINEAR_FIX_MUL-1);
        fixedBilinearV += bilinearVInc;

        int fixedBilinearU = 0;
        byte *dest = (byte *)destBits + ( ( y + v ) * dibwide + x ) * 3;

        for ( int u = 0; u < dibwide; u++, dest+=3 )
        {
            int iBilinearU = fixedBilinearU >> BILINEAR_FIX_SHIFT;
            int fixedFractionU = fixedBilinearU & (BILINEAR_FIX_MUL-1);
            fixedBilinearU += bilinearUInc;
        
            Assert( iBilinearU >= 0 && iBilinearU+1 < imageWidth );
            Assert( iBilinearV >= 0 && iBilinearV+1 < imageHeight );

            byte *srcTopLine    = rgba + iBilinearV * texwby4;
            byte *srcBottomLine = rgba + (iBilinearV+1) * texwby4;

            byte *xsrc[4] = {
                srcTopLine + (iBilinearU+0)*4,    srcTopLine + (iBilinearU+1)*4,
                srcBottomLine + (iBilinearU+0)*4, srcBottomLine + (iBilinearU+1)*4  };

            int topColor[3], bottomColor[3], finalColor[3];
            FIXED_BLEND( xsrc[1], xsrc[0], topColor, fixedFractionU );
            FIXED_BLEND( xsrc[3], xsrc[2], bottomColor, fixedFractionU );
            FIXED_BLEND( bottomColor, topColor, finalColor, fixedFractionV );

            // Windows wants the colors in reverse order.
            dest[0] = finalColor[2];
            dest[1] = finalColor[1];
            dest[2] = finalColor[0];
        }
    }
    
    // Now do the Blt
    BitBlt( hdc, 0, 0, dibwide, tall, tempDC, 0, 0, SRCCOPY );

    // This only draws if running -noshaderapi
    DrawNullBackground( hdc, dibwide, tall );

    // Restore the old Bitmap
    SelectObject( tempDC, oldBitmap );

    // Destroy the temporary DC
    DeleteDC( tempDC );

    // Destroy the DIBSection bitmap
    DeleteObject( bm );

    double elapsed = Plat_FloatTime() - st;

    COM_TimestampedLog( "BlitGraphicToHDC: new ver took %.4f", elapsed );
#else
    Assert( !"Impl me" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: This is called in response to a WM_MOVE message
//-----------------------------------------------------------------------------
void CVideoMode_Common::UpdateWindowPosition( void )
{
    int x, y, w, h;

    // Get the window from the game ( right place for it? )
    game->GetWindowRect( &x, &y, &w, &h );

#ifdef WIN32
    RECT window_rect;
    window_rect.left = x;
    window_rect.right = x + w;
    window_rect.top = y;
    window_rect.bottom = y + h;
#endif
    // NOTE: We need to feed this back into the video mode stuff
    // esp. in Resizing window mode.
}

void CVideoMode_Common::ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP )
{
}

void CVideoMode_Common::ReleaseFullScreen( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns the optimal refresh rate for the specified mode
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetRefreshRateForMode( const vmode_t *pMode )
{
    int nRefreshRate = pMode->refreshRate;

    // FIXME: We should only read this once, at the beginning
    // override the refresh rate from the command-line maybe
    nRefreshRate = CommandLine()->ParmValue( "-freq", nRefreshRate );
    nRefreshRate = CommandLine()->ParmValue( "-refresh", nRefreshRate );
    nRefreshRate = CommandLine()->ParmValue( "-refreshrate", nRefreshRate );

    return nRefreshRate;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mode - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVideoMode_Common::AdjustWindow( int nWidth, int nHeight, int nBPP, bool bWindowed )
{
	if ( g_bTextMode )
		return;

	// Use Change Display Settings to go full screen
	ChangeDisplaySettingsToFullscreen( nWidth, nHeight, nBPP );

	RECT WindowRect;
	WindowRect.top      = 0;
	WindowRect.left     = 0;
	WindowRect.right    = nWidth;
	WindowRect.bottom   = nHeight;

#ifndef USE_SDL
#ifndef _X360
	// Get window style
	DWORD style = GetWindowLong( (HWND)game->GetMainWindow(), GWL_STYLE );
	DWORD exStyle = GetWindowLong( (HWND)game->GetMainWindow(), GWL_EXSTYLE );

	if ( bWindowed )
	{
		// Give it a frame (pretty much WS_OVERLAPPEDWINDOW except for we do not modify the
		// flags corresponding to resizing-frame and maximize-box)
		if( !CommandLine()->FindParm( "-noborder" ) && !m_bVROverride )
		{
			style |= WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
		}
		else
		{
			style &= ~WS_OVERLAPPEDWINDOW;
		}

		// remove topmost flag
		exStyle &= ~WS_EX_TOPMOST;
		SetWindowLong( (HWND)game->GetMainWindow(), GWL_EXSTYLE, exStyle );
	}
	else
	{
		// Remove window border going into fullscreen mode to avoid Vista sizing issues when DWM is enabled
		style &= ~WS_OVERLAPPEDWINDOW;
	}

	SetWindowLong( (HWND)game->GetMainWindow(), GWL_STYLE, style );

	// Compute rect needed for that size client area based on window style
	AdjustWindowRectEx( &WindowRect, style, FALSE, exStyle );
#endif

	// Prepare to set window pos, which is required when toggling between topmost and not window flags
	HWND hWndAfter = NULL;
	DWORD dwSwpFlags = 0;
#ifndef _X360
	{
		if ( bWindowed )
		{
			hWndAfter = HWND_NOTOPMOST;
		}
		else
		{
			hWndAfter = HWND_TOPMOST;
		}
		dwSwpFlags = SWP_FRAMECHANGED;
	}
#else
	{
		dwSwpFlags = SWP_NOZORDER;
	}
#endif

	// Move the window to 0, 0 and the new true size
	SetWindowPos( (HWND)game->GetMainWindow(),
		hWndAfter,
		0, 0, WindowRect.right - WindowRect.left,
		WindowRect.bottom - WindowRect.top,
		SWP_NOREDRAW | dwSwpFlags );
#endif // !USE_SDL

	// Now center
	CenterEngineWindow( game->GetMainWindow(),
		WindowRect.right - WindowRect.left,
		WindowRect.bottom - WindowRect.top );
#if defined( USE_SDL )
	g_pLauncherMgr->SetWindowFullScreen( !bWindowed, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top );

	CenterEngineWindow( game->GetMainWindow(),
		WindowRect.right - WindowRect.left,
		WindowRect.bottom - WindowRect.top );

	g_pLauncherMgr->SizeWindow( WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top );

	if( bWindowed )
	{
		SDL_Window* win = (SDL_Window*)g_pLauncherMgr->GetWindowRef();
		if ( m_bVROverride || CommandLine()->FindParm( "-noborder" ) )
			SDL_SetWindowBordered( win, SDL_FALSE );
		else
			SDL_SetWindowBordered( win, SDL_TRUE );
			
	}
#endif

	game->SetWindowSize( nWidth, nHeight );

	// Make sure we have updated window information
	UpdateWindowPosition();
	MarkClientViewRectDirty();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_Common::Shutdown( void )
{
    ReleaseFullScreen();
    game->DestroyGameWindow();

    if ( !GetInitialized() )
        return;

    SetInitialized( false );
}


//-----------------------------------------------------------------------------
// Gets/sets the client view rectangle
//-----------------------------------------------------------------------------
const vrect_t &CVideoMode_Common::GetClientViewRect( ) const
{
    const_cast<CVideoMode_Common*>(this)->RecomputeClientViewRect();
    return m_ClientViewRect;
}

void CVideoMode_Common::SetClientViewRect( const vrect_t &viewRect )
{
    m_ClientViewRect = viewRect;
}


//-----------------------------------------------------------------------------
// Marks the client view rect dirty
//-----------------------------------------------------------------------------
void CVideoMode_Common::MarkClientViewRectDirty()
{
    m_bClientViewRectDirty = true;
}

void CVideoMode_Common::RecomputeClientViewRect()
{
    if ( !InEditMode() )
    {
        if ( !m_bClientViewRectDirty )
            return;
    }

    m_bClientViewRectDirty = false;

    int nWidth, nHeight;
    CMatRenderContextPtr pRenderContext( materials );

    pRenderContext->GetRenderTargetDimensions( nWidth, nHeight );
    m_ClientViewRect.width  = nWidth;
    m_ClientViewRect.height = nHeight;
    m_ClientViewRect.x      = 0;
    m_ClientViewRect.y      = 0;

    if (!nWidth || !nHeight)
    {
        // didn't successfully get the screen size, try again next frame
        // window is probably minimized
        m_bClientViewRectDirty = true;
    }
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hWndCenter - 
//          width - 
//          height - 
// Output : static void
//-----------------------------------------------------------------------------
void CVideoMode_Common::CenterEngineWindow( void *hWndCenter, int width, int height)
{
    int     CenterX, CenterY;

#if defined(USE_SDL)
	// Get the displayindex, and center our window on that display.
	static ConVarRef sdl_displayindex( "sdl_displayindex" );
	int displayindex = sdl_displayindex.IsValid() ? sdl_displayindex.GetInt() : 0;

	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode( displayindex, &mode );

	const int wide = mode.w;
	const int tall = mode.h;

	CenterX = (wide - width) / 2;
	CenterY = (tall - height) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// tweak the x and w positions if the user species them on the command-line
	CenterX = CommandLine()->ParmValue( "-x", CenterX );
	CenterY = CommandLine()->ParmValue( "-y", CenterY );

	// also check for the negated form (since it is hard to say "-x -1000")
	int negx = CommandLine()->ParmValue( "-negx", 0 ); 
	if (negx > 0)
	{
		CenterX = -negx;
	}
	int negy = CommandLine()->ParmValue( "-negy", 0 ); 
	if (negy > 0)
	{
		CenterY = -negy;
	}

	SDL_Rect rect = { 0, 0, 0, 0 };
	SDL_GetDisplayBounds( displayindex, &rect );

	CenterX += rect.x;
	CenterY += rect.y;

	game->SetWindowXY( CenterX, CenterY );
	g_pLauncherMgr->MoveWindow( CenterX, CenterY );
#else
   if ( IsPC() )
    {
        // In windowed mode go through game->GetDesktopInfo because system metrics change
        // when going fullscreen vs windowed.
        // Use system metrics for fullscreen or when game didn't have a chance to initialize.

        int cxScreen = 0, cyScreen = 0, refreshRate = 0;

        if ( !( WS_EX_TOPMOST & ::GetWindowLong( (HWND)hWndCenter, GWL_EXSTYLE ) ) && m_bWindowed )
        {
            game->GetDesktopInfo( cxScreen, cyScreen, refreshRate );
        }
        
        if ( !cxScreen || !cyScreen )
        {
            cxScreen = GetSystemMetrics(SM_CXSCREEN);
            cyScreen = GetSystemMetrics(SM_CYSCREEN);
        }

        // Compute top-left corner offset
        CenterX = (cxScreen - width) / 2;
        CenterY = (cyScreen - height) / 2;
        CenterX = (CenterX < 0) ? 0: CenterX;
        CenterY = (CenterY < 0) ? 0: CenterY;
    }
    else
    {
        CenterX = 0;
        CenterY = 0;
    }

	if( m_bVROverride )
	{
		CenterX = m_nVROverrideX;
		CenterY = m_nVROverrideY;
	}

    // tweak the x and w positions if the user species them on the command-line
    CenterX = CommandLine()->ParmValue( "-x", CenterX );
    CenterY = CommandLine()->ParmValue( "-y", CenterY );

    game->SetWindowXY( CenterX, CenterY );

    SetWindowPos ( (HWND)hWndCenter, NULL, CenterX, CenterY, 0, 0,
                  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
#endif

}


//-----------------------------------------------------------------------------
// Handle alt-tab
//-----------------------------------------------------------------------------
void CVideoMode_Common::RestoreVideo( void )
{
}

void CVideoMode_Common::ReleaseVideo( void )
{
}


//-----------------------------------------------------------------------------
// Read screen pixels
//-----------------------------------------------------------------------------
void CVideoMode_Common::ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format )
{
    int nBytes = ImageLoader::GetMemRequired( w, h, 1, format, false );
    memset( pBuffer, 0, nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: Write vid.buffer out as a .tga file
//-----------------------------------------------------------------------------
void CVideoMode_Common::TakeSnapshotTGA( const char *pFilename )
{
    // bitmap bits
    uint8 *pImage = new uint8[ GetModeStereoWidth() * 3 * GetModeStereoHeight() ];

    // Get Bits from the material system
    ReadScreenPixels( 0, 0, GetModeStereoWidth(), GetModeStereoHeight(), pImage, IMAGE_FORMAT_RGB888 );

    CUtlBuffer outBuf;
    if ( TGAWriter::WriteToBuffer( pImage, outBuf, GetModeStereoWidth(), GetModeStereoHeight(), IMAGE_FORMAT_RGB888,
        IMAGE_FORMAT_RGB888 ) )
    {
        if ( !g_pFileSystem->WriteFile( pFilename, NULL, outBuf ) )
        {
            Warning( "Couldn't write bitmap data snapshot to file %s.\n", pFilename );
        }
		else
		{
			char szPath[MAX_PATH];
			szPath[0] = 0;
			if ( g_pFileSystem->GetLocalPath( pFilename, szPath, sizeof(szPath) ) )
			{
				AddScreenshotToSteam( szPath, GetModeStereoWidth(), GetModeStereoHeight() );
			}
		}
    }

    delete[] pImage;
}

//-----------------------------------------------------------------------------
// PFM screenshot helpers
//-----------------------------------------------------------------------------
ITexture *CVideoMode_Common::GetBuildCubemaps16BitTexture( void )
{
    return materials->FindTexture( "_rt_BuildCubemaps16bit", TEXTURE_GROUP_RENDER_TARGET );
}

ITexture *CVideoMode_Common::GetFullFrameFB0( void )
{
    return materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
}

void CVideoMode_Common::BlitHiLoScreenBuffersTo16Bit( void )
{
    if ( IsX360() )
    {
        // FIXME: this breaks in 480p due to (at least) the multisampled depth buffer (need to cache, clear and restore the depth target)
        Assert( 0 );
        return;
    }
    
    IMaterial *pHDRCombineMaterial = materials->FindMaterial( "dev/hdrcombineto16bit", TEXTURE_GROUP_OTHER, true );
//  if( IsErrorMaterial( pHDRCombineMaterial ) )
//  {
//      Assert( 0 );
//      return;
//  }

    CMatRenderContextPtr pRenderContext( materials );
    ITexture *pSaveRenderTarget;
    pSaveRenderTarget = pRenderContext->GetRenderTarget();

    int oldX, oldY, oldW, oldH;
    pRenderContext->GetViewport( oldX, oldY, oldW, oldH );

    pRenderContext->SetRenderTarget( GetBuildCubemaps16BitTexture() );
    int width, height;
    pRenderContext->GetRenderTargetDimensions( width, height );
    pRenderContext->Viewport( 0, 0, width, height );
    pRenderContext->DrawScreenSpaceQuad( pHDRCombineMaterial );

    pRenderContext->SetRenderTarget( pSaveRenderTarget );
    pRenderContext->Viewport( oldX, oldY, oldW, oldH );
}

void GetCubemapOffset( CubeMapFaceIndex_t faceIndex, int &x, int &y, int &faceDim )
{
    int fbWidth, fbHeight;
    materials->GetBackBufferDimensions( fbWidth, fbHeight );

    if( fbWidth * 4 > fbHeight * 3 )
    {
        faceDim = fbHeight / 3;
    }
    else
    {
        faceDim = fbWidth / 4;
    }

    switch( faceIndex )
    {
    case CUBEMAP_FACE_RIGHT:
        x = 2;
        y = 1;
        break;
    case CUBEMAP_FACE_LEFT:
        x = 0;
        y = 1;
        break;
    case CUBEMAP_FACE_BACK:
        x = 1;
        y = 1;
        break;
    case CUBEMAP_FACE_FRONT:
        x = 3;
        y = 1;
        break;
    case CUBEMAP_FACE_UP:
        x = 2;
        y = 0;
        break;
    case CUBEMAP_FACE_DOWN:
        x = 2;
        y = 2;
        break;
    NO_DEFAULT
    }
    x *= faceDim;
    y *= faceDim;
}

//-----------------------------------------------------------------------------
// Takes a snapshot to PFM
//-----------------------------------------------------------------------------
void CVideoMode_Common::TakeSnapshotPFMRect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, CubeMapFaceIndex_t faceIndex )
{
    if ( IsX360() )
    {
        // FIXME: this breaks in 480p due to (at least) the multisampled depth buffer (need to cache, clear and restore the depth target)
        Assert( 0 );
        return;
    }

    if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
    {
        Warning( "Unable to take PFM screenshots if HDR isn't enabled!\n" );
        return;
    }

    // hack
//  resampleWidth = w;
//  resampleHeight = h;
    // bitmap bits
    float16 *pImage = ( float16 * )malloc( w * h * ImageLoader::SizeInBytes( IMAGE_FORMAT_RGBA16161616F ) );
    float *pImage1 = ( float * )malloc( w * h * ImageLoader::SizeInBytes( IMAGE_FORMAT_RGB323232F ) );

    CMatRenderContextPtr pRenderContext( materials );

    // Save the current render target.
    ITexture *pSaveRenderTarget = pRenderContext->GetRenderTarget();

    // Set this as the render target so that we can read it.
    pRenderContext->SetRenderTarget( GetFullFrameFB0() );

    // Get Bits from the material system
    ReadScreenPixels( x, y, w, h, pImage, IMAGE_FORMAT_RGBA16161616F );

    // Draw what we just grabbed to the screen
    pRenderContext->SetRenderTarget( NULL);

    int scrw, scrh;
    pRenderContext->GetRenderTargetDimensions( scrw, scrh );
    pRenderContext->Viewport( 0, 0, scrw,scrh );

    int offsetX, offsetY, faceDim;
    GetCubemapOffset( faceIndex, offsetX, offsetY, faceDim );
    pRenderContext->DrawScreenSpaceRectangle( materials->FindMaterial( "dev/copyfullframefb", "" ),
        offsetX, offsetY, faceDim, faceDim, 0, 0, w-1, h-1, scrw, scrh );

    // Restore the render target.
    pRenderContext->SetRenderTarget( pSaveRenderTarget );

    // convert from float16 to float32
    ImageLoader::ConvertImageFormat( ( unsigned char * )pImage, IMAGE_FORMAT_RGBA16161616F, 
        ( unsigned char * )pImage1, IMAGE_FORMAT_RGB323232F, 
        w, h );

    Assert( w == h ); // garymcthack - this only works for square images

    float *pFloatImage = ( float * )malloc( resampleWidth * resampleHeight * ImageLoader::SizeInBytes( IMAGE_FORMAT_RGB323232F ) );

    ImageLoader::ResampleInfo_t info;
    info.m_pSrc = ( unsigned char * )pImage1;
    info.m_pDest = ( unsigned char * )pFloatImage;
    info.m_nSrcWidth = w;
    info.m_nSrcHeight = h;
    info.m_nDestWidth = resampleWidth;
    info.m_nDestHeight = resampleHeight;
    info.m_flSrcGamma = 1.0f;
    info.m_flDestGamma = 1.0f;

    if( !ImageLoader::ResampleRGB323232F( info ) )
    {
        Sys_Error( "Can't resample\n" );
    }

    PFMWrite( pFloatImage, pFilename, resampleWidth, resampleHeight );

    free( pImage1 );
    free( pImage );
    free( pFloatImage );
}


//-----------------------------------------------------------------------------
// Takes a snapshot
//-----------------------------------------------------------------------------
void CVideoMode_Common::TakeSnapshotTGARect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, bool bPFM, CubeMapFaceIndex_t faceIndex )
{
    if ( IsX360() )
    {
        Assert( 0 );
        return;
    }

    if ( bPFM )
    {
        TakeSnapshotPFMRect( pFilename, x, y, w, h, resampleWidth, resampleHeight, faceIndex );
        return;
    }

    // bitmap bits
    uint8 *pImage = new uint8[ w * h * 4 ];
    uint8 *pImage1 = new uint8[ resampleWidth * resampleHeight * 4 ];

    // Get Bits from the material system
    ReadScreenPixels( x, y, w, h, pImage, IMAGE_FORMAT_RGBA8888 );

    Assert( w == h ); // garymcthack - this only works for square images

    ImageLoader::ResampleInfo_t info;
    info.m_pSrc = pImage;
    info.m_pDest = pImage1;
    info.m_nSrcWidth = w;
    info.m_nSrcHeight = h;
    info.m_nDestWidth = resampleWidth;
    info.m_nDestHeight = resampleHeight;
    info.m_flSrcGamma = 1.0f;
    info.m_flDestGamma = 1.0f;

    if( !ImageLoader::ResampleRGBA8888( info ) )
    {
        Sys_Error( "Can't resample\n" );
    }
    
    CUtlBuffer outBuf;
    if ( TGAWriter::WriteToBuffer( pImage1, outBuf, resampleWidth, resampleHeight, IMAGE_FORMAT_RGBA8888, IMAGE_FORMAT_RGBA8888 ) )
    {
        if ( !g_pFileSystem->WriteFile( pFilename, NULL, outBuf ) )
        {
            Error( "Couldn't write bitmap data snapshot to file %s.\n", pFilename );
        }
		else
		{
			DevMsg( "Screenshot: %dx%d saved to '%s'.\n", w, h, pFilename );
		}
    }

    delete[] pImage1;
    delete[] pImage;
    materials->SwapBuffers();
}


//-----------------------------------------------------------------------------
// Purpose: Writes the data in *data to the sequentially number .bmp file filename
// Input  : *filename - 
//          width - 
//          height - 
//          depth - 
//          *data - 
// Output : static void
//-----------------------------------------------------------------------------
static void VID_ProcessMovieFrame( const MovieInfo_t& info, bool jpeg, const char *filename, int width, int height, byte *data )
{
    CUtlBuffer outBuf;
    bool bSuccess = false;
    if ( jpeg )
    {
        bSuccess = videomode->TakeSnapshotJPEGToBuffer( outBuf, info.jpeg_quality );
    }
    else
    {
        bSuccess = TGAWriter::WriteToBuffer( data, outBuf, width, height, IMAGE_FORMAT_BGR888, IMAGE_FORMAT_RGB888 );
    }

    if ( bSuccess )
    {
        if ( !g_pFileSystem->WriteFile( filename, NULL, outBuf ) )
        {
            Warning( "Couldn't write movie snapshot to file %s.\n", filename );
            Cbuf_AddText( "endmovie\n" );
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Store current frame to numbered .bmp file
// Input  : *pFilename - 
//-----------------------------------------------------------------------------
extern IVideoRecorder *g_pVideoRecorder;

void CVideoMode_Common::WriteMovieFrame( const MovieInfo_t& info )
{
    char const *pMovieName = info.moviename;
    int nMovieFrame = info.movieframe;

    if ( g_LostVideoMemory )
        return;

    if ( !pMovieName[0] )
    {
        Cbuf_AddText( "endmovie\n" );
        ConMsg( "Tried to write movie buffer with no filename set!\n" );
        return;
    }

    int imagesize = GetModeStereoWidth() * GetModeStereoHeight();
    BGR888_t *hp = new BGR888_t[ imagesize ];
    if ( hp == NULL )
    {
        Sys_Error( "Couldn't allocate bitmap header to snapshot.\n" );
    }

    // Get Bits from material system
    ReadScreenPixels( 0, 0, GetModeStereoWidth(), GetModeStereoHeight(), hp, IMAGE_FORMAT_BGR888 );

    // Store frame to disk
    if ( info.DoTga() )
    {
        VID_ProcessMovieFrame( info, false, va( "%s%04d.tga", pMovieName, nMovieFrame ), 
            GetModeStereoWidth(), GetModeStereoHeight(), (unsigned char*)hp );
    }

    if ( info.DoJpg() )
    {
        VID_ProcessMovieFrame( info, true, va( "%s%04d.jpg", pMovieName, nMovieFrame ), 
            GetModeStereoWidth(), GetModeStereoHeight(), (unsigned char*)hp );
    }

    if ( info.DoVideo() )
    {
		g_pVideoRecorder->AppendVideoFrame( hp );
    }

    delete[] hp;
}

//-----------------------------------------------------------------------------
// Purpose: Expanded data destination object for CUtlBuffer output
//-----------------------------------------------------------------------------
struct JPEGDestinationManager_t
{
    struct jpeg_destination_mgr pub; // public fields
    
    CUtlBuffer  *pBuffer;       // target/final buffer
    byte        *buffer;        // start of temp buffer
};

// choose an efficiently bufferaable size
#define OUTPUT_BUF_SIZE  4096   

//-----------------------------------------------------------------------------
// Purpose:  Initialize destination --- called by jpeg_start_compress
//  before any data is actually written.
//-----------------------------------------------------------------------------
METHODDEF(void) init_destination (j_compress_ptr cinfo)
{
    JPEGDestinationManager_t *dest = ( JPEGDestinationManager_t *) cinfo->dest;
    
    // Allocate the output buffer --- it will be released when done with image
    dest->buffer = (byte *)
        (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
        OUTPUT_BUF_SIZE * sizeof(byte));
    
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

//-----------------------------------------------------------------------------
// Purpose: Empty the output buffer --- called whenever buffer fills up.
// Input  : boolean - 
//-----------------------------------------------------------------------------
METHODDEF(boolean) empty_output_buffer (j_compress_ptr cinfo)
{
    JPEGDestinationManager_t *dest = ( JPEGDestinationManager_t * ) cinfo->dest;
    
    CUtlBuffer *buf = dest->pBuffer;

    // Add some data
    buf->Put( dest->buffer, OUTPUT_BUF_SIZE );
    
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
    
    return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Terminate destination --- called by jpeg_finish_compress
// after all data has been written.  Usually needs to flush buffer.
//
// NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
// application must deal with any cleanup that should happen even
// for error exit.
//-----------------------------------------------------------------------------
METHODDEF(void) term_destination (j_compress_ptr cinfo)
{
    JPEGDestinationManager_t *dest = (JPEGDestinationManager_t *) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;
    
    CUtlBuffer *buf = dest->pBuffer;

    /* Write any data remaining in the buffer */
    if (datacount > 0) 
    {
        buf->Put( dest->buffer, datacount );
    }
}

//-----------------------------------------------------------------------------
// Purpose: Set up functions for writing data to a CUtlBuffer instead of FILE *
//-----------------------------------------------------------------------------
GLOBAL(void) jpeg_UtlBuffer_dest (j_compress_ptr cinfo, CUtlBuffer *pBuffer )
{
    JPEGDestinationManager_t *dest;
    
    /* The destination object is made permanent so that multiple JPEG images
    * can be written to the same file without re-executing jpeg_stdio_dest.
    * This makes it dangerous to use this manager and a different destination
    * manager serially with the same JPEG object, because their private object
    * sizes may be different.  Caveat programmer.
    */
    if (cinfo->dest == NULL) {  /* first time for this JPEG object? */
        cinfo->dest = (struct jpeg_destination_mgr *)
            (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
            sizeof(JPEGDestinationManager_t));
    }
    
    dest = ( JPEGDestinationManager_t * ) cinfo->dest;

    dest->pub.init_destination      = init_destination;
    dest->pub.empty_output_buffer   = empty_output_buffer;
    dest->pub.term_destination      = term_destination;
    dest->pBuffer                   = pBuffer;
}

bool CVideoMode_Common::TakeSnapshotJPEGToBuffer( CUtlBuffer& buf, int quality )
{
#if !defined( _X360 )
    if ( g_LostVideoMemory )
        return false;

    // Validate quality level
    quality = clamp( quality, 1, 100 );

    // Allocate space for bits
    uint8 *pImage = new uint8[ GetModeStereoWidth() * 3 * GetModeStereoHeight() ];
    if ( !pImage )
    {
        Msg( "Unable to allocate %i bytes for image\n", GetModeStereoWidth() * 3 * GetModeStereoHeight() );
        return false;
    }

    // Get Bits from the material system
    ReadScreenPixels( 0, 0, GetModeStereoWidth(), GetModeStereoHeight(), pImage, IMAGE_FORMAT_RGB888 );

    JSAMPROW row_pointer[1];     // pointer to JSAMPLE row[s]
    int row_stride;              // physical row width in image buffer

    // stderr handler
    struct jpeg_error_mgr jerr;

    // compression data structure
    struct jpeg_compress_struct cinfo;

    row_stride = GetModeStereoWidth() * 3; // JSAMPLEs per row in image_buffer

    // point at stderr
    cinfo.err = jpeg_std_error(&jerr);

    // create compressor
    jpeg_create_compress(&cinfo);

    // Hook CUtlBuffer to compression
    jpeg_UtlBuffer_dest(&cinfo, &buf );

    // image width and height, in pixels
    cinfo.image_width = GetModeStereoWidth();
    cinfo.image_height = GetModeStereoHeight();
    // RGB is 3 componnent
    cinfo.input_components = 3;
    // # of color components per pixel
    cinfo.in_color_space = JCS_RGB;

    // Apply settings
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE );

    // Start compressor
    jpeg_start_compress(&cinfo, TRUE);
    
    // Write scanlines
    while ( cinfo.next_scanline < cinfo.image_height ) 
    {
        row_pointer[ 0 ] = &pImage[ cinfo.next_scanline * row_stride ];
        jpeg_write_scanlines( &cinfo, row_pointer, 1 );
    }

    // Finalize image
    jpeg_finish_compress(&cinfo);

    // Cleanup
    jpeg_destroy_compress(&cinfo);
    
    delete[] pImage;

#else
    // not supporting
    Assert( 0 );
#endif
    return true;
}

//-----------------------------------------------------------------------------
// Purpose: Write vid.buffer out as a .jpg file
// Input  : *pFilename - 
//-----------------------------------------------------------------------------
void CVideoMode_Common::TakeSnapshotJPEG( const char *pFilename, int quality )
{
#if !defined( _X360 )
    Assert( pFilename );

    // Output buffer
    CUtlBuffer buf( 0, 0 );
    TakeSnapshotJPEGToBuffer( buf, quality );

    int finalSize = 0;
    FileHandle_t fh = g_pFileSystem->Open( pFilename, "wb" );
    if ( FILESYSTEM_INVALID_HANDLE != fh )
    {
        g_pFileSystem->Write( buf.Base(), buf.TellPut(), fh );
        finalSize = g_pFileSystem->Tell( fh );
        g_pFileSystem->Close( fh );
    }

// Show info to console.
    char orig[ 64 ];
    char final[ 64 ];
    Q_strncpy( orig, Q_pretifymem( GetModeStereoWidth() * 3 * GetModeStereoHeight(), 2 ), sizeof( orig ) );
    Q_strncpy( final, Q_pretifymem( finalSize, 2 ), sizeof( final ) );

    Msg( "Wrote '%s':  %s (%dx%d) compresssed (quality %i) to %s\n",
        pFilename, orig, GetModeStereoWidth(), GetModeStereoHeight(), quality, final );

	if ( finalSize > 0 )
	{
		char szPath[MAX_PATH];
		szPath[0] = 0;
		if ( g_pFileSystem->GetLocalPath( pFilename, szPath, sizeof(szPath) ) )
		{
			AddScreenshotToSteam( szPath, GetModeStereoWidth(), GetModeStereoHeight() );
		}
	}

#else
    Assert( 0 );
#endif
}

//-----------------------------------------------------------------------------
// The version of the VideoMode class for the material system 
//-----------------------------------------------------------------------------
class CVideoMode_MaterialSystem: public CVideoMode_Common
{
public:
    typedef CVideoMode_Common BaseClass;
    
    CVideoMode_MaterialSystem( );

    virtual bool        Init( );
    virtual void        Shutdown( void );
    virtual void        SetGameWindow( void *hWnd );
    virtual bool        SetMode( int nWidth, int nHeight, bool bWindowed );
    virtual void        ReleaseVideo( void );
    virtual void        RestoreVideo( void );
    virtual void        AdjustForModeChange( void );
    virtual void        ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format );

private:
    virtual void        ReleaseFullScreen( void );
    virtual void        ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP );

#ifdef WIN32
	int m_nLastCDSWidth;
	int m_nLastCDSHeight;
	int m_nLastCDSBPP;
	int m_nLastCDSFreq;
#endif
};

static void VideoMode_AdjustForModeChange( void )
{
    ( ( CVideoMode_MaterialSystem * )videomode )->AdjustForModeChange();
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CVideoMode_MaterialSystem::CVideoMode_MaterialSystem( )
{
#ifdef WIN32
	m_nLastCDSWidth = 0;
	m_nLastCDSHeight = 0;
	m_nLastCDSBPP = 0;
	m_nLastCDSFreq = 0;
#endif
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
bool CVideoMode_MaterialSystem::Init( )
{
    m_bSetModeOnce = false;
    m_bPlayedStartupVideo = false;

    // we only support 32-bit rendering.
    int bitsperpixel = 32;

    bool bAllowSmallModes = false;
    if ( CommandLine()->FindParm( "-small" ) )
    {
        bAllowSmallModes = true;
    }

    int nAdapter = materials->GetCurrentAdapter();
    int nModeCount = materials->GetModeCount( nAdapter );

    int nDesktopWidth, nDesktopHeight, nDesktopRefresh;
    game->GetDesktopInfo( nDesktopWidth, nDesktopHeight, nDesktopRefresh );

    for ( int i = 0; i < nModeCount; i++ )
    {
        MaterialVideoMode_t info;
        materials->GetModeInfo( nAdapter, i, info );

        if ( info.m_Width < 640 || info.m_Height < 480 )
        {
            if ( !bAllowSmallModes )
                continue;
        }

        // make sure we don't already have this mode listed
        bool bAlreadyInList = false;
        for ( int j = 0; j < m_nNumModes; j++ )
        {
            if ( info.m_Width == m_rgModeList[ j ].width && info.m_Height == m_rgModeList[ j ].height )
            {

				// in VR mode we want the highest refresh rate, without regard for the desktop refresh rate
				if ( UseVR() || ShouldForceVRActive() )
				{
					if ( info.m_RefreshRate > m_rgModeList[j].refreshRate )
					{
						m_rgModeList[j].refreshRate = info.m_RefreshRate;
					}
				}
				else
				{
					// choose the highest refresh rate available for each mode up to the desktop rate

					// if the new mode is valid and current is invalid or not as high, choose the new one
					if ( info.m_RefreshRate <= nDesktopRefresh && (m_rgModeList[j].refreshRate > nDesktopRefresh || m_rgModeList[j].refreshRate < info.m_RefreshRate) )
					{
						m_rgModeList[j].refreshRate = info.m_RefreshRate;
					}
				}

                bAlreadyInList = true;
                break;
            }
        }

        if ( bAlreadyInList )
            continue;

        m_rgModeList[ m_nNumModes ].width = info.m_Width;
        m_rgModeList[ m_nNumModes ].height = info.m_Height;
        m_rgModeList[ m_nNumModes ].bpp = bitsperpixel;
        // NOTE: Don't clamp this to the desktop rate because we want to be sure we've only added
        // modes that the adapter can do and maybe the desktop rate isn't available in this mode
        m_rgModeList[ m_nNumModes ].refreshRate = info.m_RefreshRate;

        if ( ++m_nNumModes >= MAX_MODE_LIST )
            break;
    }

    // Sort modes for easy searching later
    if ( m_nNumModes > 1 )
    {
        qsort( (void *)&m_rgModeList[0], m_nNumModes, sizeof(vmode_t), VideoModeCompare );
    }

    materials->AddModeChangeCallBack( &VideoMode_AdjustForModeChange );
    SetInitialized( true );
    return true;
}


void CVideoMode_MaterialSystem::Shutdown()
{
    materials->RemoveModeChangeCallBack( &VideoMode_AdjustForModeChange );
    BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
bool CVideoMode_MaterialSystem::SetMode( int nWidth, int nHeight, bool bWindowed )
{
    // Necessary for mode selection to work
    int nFoundMode = FindVideoMode( nWidth, nHeight, bWindowed );
    vmode_t *pMode = GetMode( nFoundMode );

    // update current video state
    MaterialSystem_Config_t config = *g_pMaterialSystemConfig;
    config.m_VideoMode.m_Width = pMode->width;
    config.m_VideoMode.m_Height = pMode->height;

	// make sure VR mode is up to date
	config.SetFlag( MATSYS_VIDCFG_FLAGS_VR_MODE, UseVR() || ShouldForceVRActive() );

	if ( ShouldForceVRActive() )
	{
		config.m_nVRModeAdapter = materials->GetCurrentAdapter();
	}

#ifdef SWDS
    config.m_VideoMode.m_RefreshRate = 60;
#else
    config.m_VideoMode.m_RefreshRate = GetRefreshRateForMode( pMode );
#endif
    
    config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, bWindowed );

#if defined( _X360 )
    XVIDEO_MODE videoMode;
    XGetVideoMode( &videoMode );
    if ( videoMode.fIsWideScreen )
    {
        extern ConVar r_aspectratio;
        r_aspectratio.SetValue( 16.0f/9.0f );
    }
    config.SetFlag( MATSYS_VIDCFG_FLAGS_SCALE_TO_OUTPUT_RESOLUTION, (DWORD)nWidth != videoMode.dwDisplayWidth || (DWORD)nHeight != videoMode.dwDisplayHeight );
    if ( nHeight == 480 || nWidth == 576 )
    {
        // Use 2xMSAA for standard def (see mat_software_aa_strength for fake hi-def aa)
        // FIXME: shuffle the EDRAM surfaces to allow 4xMSAA for standard def
        //        (they would overlap & trash each other with the current arrangement)
        // NOTE: This should affect 640x480 and 848x480 (which is also used for 640x480 widescreen), and PAL 640x576
        config.m_nAASamples = 2;
    }
#endif

    // FIXME: This is trash. We have to do *different* things depending on how we're setting the mode!
    if ( !m_bSetModeOnce )
    {
		//Debugger();
		
        if ( !materials->SetMode( (void*)game->GetMainDeviceWindow(), config ) )
            return false;

        m_bSetModeOnce = true;

        InitStartupScreen();
        return true;
    }

    // update the config 
    OverrideMaterialSystemConfig( config );
    return true;
}


//-----------------------------------------------------------------------------
// Called by the material system when mode changes after a call to OverrideConfig
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::AdjustForModeChange( void )
{
    if ( InEditMode() )
        return;

    // get previous size
	int nOldUIWidth = GetModeUIWidth();
	int nOldUIHeight = GetModeUIHeight();

    // Get the new mode info from the config record
    int nNewWidth = g_pMaterialSystemConfig->m_VideoMode.m_Width;
    int nNewHeight = g_pMaterialSystemConfig->m_VideoMode.m_Height;
    bool bWindowed = g_pMaterialSystemConfig->Windowed();

    // reset the window size
    CMatRenderContextPtr pRenderContext( materials );

    ResetCurrentModeForNewResolution( nNewWidth, nNewHeight, bWindowed );
    AdjustWindow( GetModeWidth(), GetModeHeight(), GetModeBPP(), IsWindowedMode() );
    MarkClientViewRectDirty();
    pRenderContext->Viewport( 0, 0, GetModeStereoWidth(), GetModeStereoHeight() );

    // fixup vgui
    vgui::surface()->OnScreenSizeChanged( nOldUIWidth, nOldUIHeight );
    
    // Re-init the HUD
    ClientDLL_HudVidInit();
}


//-----------------------------------------------------------------------------
// Sets the game window in editor mode
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::SetGameWindow( void *hWnd )
{
    if ( hWnd == NULL )
    {
        // No longer confine rendering into this view
        materials->SetView( NULL );
        return;
    }

    // When running in edit mode, just use hammer's window
    game->SetGameWindow( (HWND)hWnd );

    // FIXME: Move this code into the _MaterialSystem version of CVideoMode
    // In editor mode, the mode width + height is equal to the desktop width + height
    MaterialVideoMode_t mode;
    materials->GetDisplayMode( mode );
    m_bWindowed = true;
    m_nModeWidth = mode.m_Width;
    m_nModeHeight = mode.m_Height;

    materials->SetView( game->GetMainDeviceWindow() );
}


//-----------------------------------------------------------------------------
// Called when we lose the video buffer (alt-tab)
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::ReleaseVideo( void )
{
    if ( IsX360() )
        return;

    if ( IsWindowedMode() )
        return;

	ReleaseFullScreen();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::RestoreVideo( void )
{
    if ( IsX360() )
        return;

    if ( IsWindowedMode() )
        return;

#if defined( USE_SDL )
	SDL_ShowWindow( (SDL_Window*)game->GetMainWindow() );
#else
	ShowWindow( (HWND)game->GetMainWindow(), SW_SHOWNORMAL );
#endif
    AdjustWindow( GetModeWidth(), GetModeHeight(), GetModeBPP(), IsWindowedMode() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::ReleaseFullScreen( void )
{
    if ( IsX360() )
        return;

    if ( IsWindowedMode() )
        return;

#if !defined( USE_SDL )
    // Hide the main window
	if ( m_nLastCDSWidth != 0 )
		ChangeDisplaySettings( NULL, 0 );
	ShowWindow( (HWND)game->GetMainWindow(), SW_MINIMIZE );
	m_nLastCDSWidth = 0;
	m_nLastCDSHeight = 0;
	m_nLastCDSBPP = 0;
	m_nLastCDSFreq = 0;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Use Change Display Settings to go Full Screen
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP )
{
    if ( IsX360() )
        return;

    if ( IsWindowedMode() )
        return;

#if defined( WIN32 ) && !defined( USE_SDL )
	DEVMODE dm;
    memset(&dm, 0, sizeof(dm));

    dm.dmSize       = sizeof( dm );
    dm.dmPelsWidth  = nWidth;
    dm.dmPelsHeight = nHeight;
    dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    dm.dmBitsPerPel = nBPP;

    // FIXME: Fix direct reference of refresh rate from config record
    int freq = g_pMaterialSystemConfig->m_VideoMode.m_RefreshRate;
    if ( freq >= 60 )
    {
        dm.dmDisplayFrequency = freq;
        dm.dmFields |= DM_DISPLAYFREQUENCY;
    }

#if defined(IS_WINDOWS_PC)
	DEVMODE dmCurrent;
	if ( EnumDisplaySettings( materials->GetDisplayDeviceName(), ENUM_CURRENT_SETTINGS, &dmCurrent ) &&
		 dmCurrent.dmBitsPerPel == dm.dmBitsPerPel &&
		 dmCurrent.dmPelsWidth == dm.dmPelsWidth &&
		 dmCurrent.dmPelsHeight == dm.dmPelsHeight &&
		 ( (dm.dmFields & DM_DISPLAYFREQUENCY) == 0 || dmCurrent.dmDisplayFrequency == dm.dmDisplayFrequency ) )
	{
		return;
	}
#endif

	if ( nWidth == m_nLastCDSWidth && nHeight == m_nLastCDSHeight && nBPP == m_nLastCDSBPP && freq == m_nLastCDSFreq )
		return;

	m_nLastCDSWidth = nWidth;
	m_nLastCDSHeight = nHeight;
	m_nLastCDSBPP = nBPP;
	m_nLastCDSFreq = freq;

    ChangeDisplaySettingsEx( materials->GetDisplayDeviceName(),  &dm, NULL, CDS_FULLSCREEN, NULL );
#elif defined( USE_SDL )
	g_pLauncherMgr->SetWindowFullScreen( true, nWidth, nHeight );
#else
	if ( !HushAsserts() )
	{
		Assert( !"Impl me" );
	}
#endif
}

void CVideoMode_MaterialSystem::ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format )
{
    if ( !g_LostVideoMemory )
    {
        bool bReadPixelsFromFrontBuffer = g_pMaterialSystemHardwareConfig->ReadPixelsFromFrontBuffer();
        if( bReadPixelsFromFrontBuffer )
        {
            Shader_SwapBuffers();
        }

        CMatRenderContextPtr pRenderContext( materials );

        Rect_t rect;
        rect.x = x;
        rect.y = y;
        rect.width = w;
        rect.height = h;

        pRenderContext->ReadPixelsAndStretch( &rect, &rect, (unsigned char*)pBuffer, format, w * ImageLoader::SizeInBytes( format ) );

        if( bReadPixelsFromFrontBuffer )
        {
            Shader_SwapBuffers();
        }
    }
    else
    {
        int nBytes = ImageLoader::GetMemRequired( w, h, 1, format, false );
        memset( pBuffer, 0, nBytes );
    }
}


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------

IVideoMode *videomode = ( IVideoMode * )NULL;

void VideoMode_Create( )
{
    videomode = new CVideoMode_MaterialSystem;
    Assert( videomode );
}

void VideoMode_Destroy()
{
    if ( videomode )
    {
        CVideoMode_MaterialSystem *pVideoMode_MS = static_cast<CVideoMode_MaterialSystem*>(videomode);
        delete pVideoMode_MS;
        videomode = NULL;
    }
}
