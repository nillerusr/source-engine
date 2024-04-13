#include "gamepadui_basepanel.h"
#include "gamepadui_mainmenu.h"

#ifdef _WIN32
#ifdef INVALID_HANDLE_VALUE
#undef INVALID_HANDLE_VALUE
#endif
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "icommandline.h"
#include "filesystem.h"
#include "gamepadui_interface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar gamepadui_background_music_duck( "gamepadui_background_music_duck", "0.35", FCVAR_ARCHIVE );
ConVar gamepadui_sizing_panel_width( "gamepadui_sizing_panel_width", "1280", FCVAR_ARCHIVE );
ConVar gamepadui_sizing_panel_height( "gamepadui_sizing_panel_height", "800", FCVAR_ARCHIVE );

GamepadUIBasePanel::GamepadUIBasePanel( vgui::VPANEL parent ) : BaseClass( NULL, "GamepadUIBasePanel" )
{
    SetParent( parent );
    MakePopup( false );

    m_nBackgroundMusicGUID = 0;
    m_bBackgroundMusicEnabled = !CommandLine()->FindParm( "-nostartupsound" );

    m_pSizingPanel = new GamepadUISizingPanel( this );

    m_pMainMenu = new GamepadUIMainMenu( this );
    OnMenuStateChanged();
}

void GamepadUIBasePanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    // Josh: Need to use GetVParent because this is across
    // a DLL boundary.
	int nVParentW, nVParentH;
    vgui::ipanel()->GetSize( GetVParent(), nVParentW, nVParentH );
	SetBounds( 0, 0, nVParentW, nVParentH );

    // Josh:
    // Force the main menu to invalidate itself.
    // There is a weird ordering bug in VGUI we need to workaround.
    m_pMainMenu->InvalidateLayout( false, true );

    m_pSizingPanel->InvalidateLayout( false, true );
}

GamepadUISizingPanel *GamepadUIBasePanel::GetSizingPanel() const
{
    return m_pSizingPanel;
}

GamepadUIMainMenu* GamepadUIBasePanel::GetMainMenuPanel() const
{
    return m_pMainMenu;
}

GamepadUIFrame *GamepadUIBasePanel::GetCurrentFrame() const
{
    return m_pCurrentFrame;
}

void GamepadUIBasePanel::SetCurrentFrame( GamepadUIFrame *pFrame )
{
    if (pFrame != NULL && m_pCurrentFrame != NULL)
    {
        // If there's already a frame, close it
        m_pCurrentFrame->Close();
    }

    m_pCurrentFrame = pFrame;
}


void GamepadUIBasePanel::OnMenuStateChanged()
{
    if ( m_bBackgroundMusicEnabled && GamepadUI::GetInstance().IsGamepadUIVisible() )
    {
        if ( !IsBackgroundMusicPlaying() )
            ActivateBackgroundEffects();
    }
    else
        ReleaseBackgroundMusic();

    if (m_pCurrentFrame && m_pCurrentFrame != m_pMainMenu)
    {
        m_pCurrentFrame->Close();
        m_pCurrentFrame = NULL;
    }
}

void GamepadUIBasePanel::ActivateBackgroundEffects()
{
    StartBackgroundMusic( 1.0f );
}

bool GamepadUIBasePanel::IsBackgroundMusicPlaying()
{
    if ( !m_nBackgroundMusicGUID )
        return false;

    return GamepadUI::GetInstance().GetEngineSound()->IsSoundStillPlaying( m_nBackgroundMusicGUID );
}

bool GamepadUIBasePanel::StartBackgroundMusic( float flVolume )
{
    if ( IsBackgroundMusicPlaying() )
        return true;

    /* mostly from GameUI */
    char path[ 512 ];
    Q_snprintf( path, sizeof( path ), "sound/ui/gamestartup*.mp3" );
    Q_FixSlashes( path );
    CUtlVector<char*> fileNames;
    FileFindHandle_t fh;

    char const *fn = g_pFullFileSystem->FindFirstEx( path, "MOD", &fh );
    if ( fn )
    {
        do
        {
            char ext[ 10 ];
            Q_ExtractFileExtension( fn, ext, sizeof( ext ) );

            if ( !Q_stricmp( ext, "mp3" ) )
            {
                char temp[ 512 ];
                {
                    Q_snprintf( temp, sizeof( temp ), "ui/%s", fn );
                }

                char *found = new char[ strlen( temp ) + 1 ];
                Q_strncpy( found, temp, strlen( temp ) + 1 );

                Q_FixSlashes( found );
                fileNames.AddToTail( found );
            }

            fn = g_pFullFileSystem->FindNext( fh );

        } while ( fn );

        g_pFullFileSystem->FindClose( fh );
    }

    if ( !fileNames.Count() )
        return false;

#ifdef WIN32
    SYSTEMTIME SystemTime;
    GetSystemTime( &SystemTime );
    int index = SystemTime.wMilliseconds % fileNames.Count();
#else
    struct timeval tm;
    gettimeofday( &tm, NULL );
    int index = tm.tv_usec/1000 % fileNames.Count();
#endif

    const char* pSoundFile = NULL;

    if ( fileNames.IsValidIndex(index) && fileNames[index] )
        pSoundFile = fileNames[ index ];

    if ( !pSoundFile )
        return false;
    
    // check and see if we have a background map loaded.
    // if not, this code path won't properly play the music.
    const bool bInGame = GamepadUI::GetInstance().GetEngineClient()->IsLevelMainMenuBackground();
    if ( bInGame )
    {
        // mixes too loud against soft ui sounds
        GamepadUI::GetInstance().GetEngineSound()->EmitAmbientSound( pSoundFile, gamepadui_background_music_duck.GetFloat() * flVolume );
        m_nBackgroundMusicGUID = GamepadUI::GetInstance().GetEngineSound()->GetGuidForLastSoundEmitted();
    }
    else
    {
        // old way, failsafe in case we don't have a background level.
        char found[ 512 ];
        Q_snprintf( found, sizeof( found ), "play *#%s\n", pSoundFile );
        GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( found );
    }

    fileNames.PurgeAndDeleteElements();

    return m_nBackgroundMusicGUID != 0;
}

void GamepadUIBasePanel::ReleaseBackgroundMusic()
{
    if ( !m_nBackgroundMusicGUID )
        return;

    // need to stop the sound now, do not queue the stop
    // we must release the 2-5MB held by this resource
    GamepadUI::GetInstance().GetEngineSound()->StopSoundByGuid( m_nBackgroundMusicGUID );
    m_nBackgroundMusicGUID = 0;
}

GamepadUISizingPanel::GamepadUISizingPanel( vgui::Panel *pParent ) : BaseClass( pParent, "GamepadUISizingPanel" )
{
    SetVisible( false );
}

void GamepadUISizingPanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    int w = GetParent()->GetWide();
    int h = GetParent()->GetTall();

    float flX, flY;
    GamepadUI::GetInstance().GetScreenRatio( flX, flY );

    float targetW = gamepadui_sizing_panel_width.GetFloat() * flX;
    float targetH = gamepadui_sizing_panel_height.GetFloat() * flY;

    w -= targetW;
    h -= targetH;
    if (w <= 0 || h <= 0)
    {
        GamepadUI_Log( "Setting sizing panel bounds to 0, 0, %i, %i (proportional)\n", GetParent()->GetWide(), GetParent()->GetTall() );
        SetBounds( 0, 0, GetParent()->GetWide(), GetParent()->GetTall() );

        m_flScaleX = m_flScaleY = 1.0f;
    }
    else
    {
        GamepadUI_Log( "Setting sizing panel bounds to %i, %i, %i, %i\n", w/2, h/2, (int)targetW, (int)targetH );
        SetBounds( w/2, h/2, targetW, targetH );

        m_flScaleX = ((float)GetParent()->GetWide()) / targetW;
        m_flScaleY = ((float)GetParent()->GetTall()) / targetH;
    }
}
