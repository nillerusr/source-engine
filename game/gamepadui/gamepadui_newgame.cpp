#include "gamepadui_interface.h"
#include "gamepadui_image.h"
#include "gamepadui_util.h"
#include "gamepadui_frame.h"
#include "gamepadui_scroll.h"
#include "gamepadui_genericconfirmation.h"

#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include "vgui_controls/ComboBox.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ScrollBar.h"

#include "KeyValues.h"
#include "filesystem.h"

#include "icommandline.h"

#include "tier0/memdbgon.h"

class GamepadUIChapterButton;
struct chapter_t;

#define GAMEPADUI_CHAPTER_SCHEME GAMEPADUI_RESOURCE_FOLDER "schemechapterbutton.res"

// Modders should override this if necessary. (Madi)
// TODO - merge these into scheme config?
bool GameHasCommentary()
{
    const char *pszGameDir = CommandLine()->ParmValue( "-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
    return !V_strcmp( pszGameDir, "episodic" ) ||
           !V_strcmp( pszGameDir, "ep2" ) ||
           !V_strcmp( pszGameDir, "portal" ) ||
           !V_strcmp( pszGameDir, "lostcoast" );
}

bool GameHasBonusMaps()
{
    const char *pszGameDir = CommandLine()->ParmValue( "-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
    return !V_strcmp( pszGameDir, "portal" );
}

class GamepadUINewGamePanel : public GamepadUIFrame
{
    DECLARE_CLASS_SIMPLE( GamepadUINewGamePanel, GamepadUIFrame );

public:
    GamepadUINewGamePanel( vgui::Panel *pParent, const char* pPanelName );

    void UpdateGradients();

    void OnThink() OVERRIDE;
    void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
    void OnCommand( char const* pCommand ) OVERRIDE;

    MESSAGE_FUNC_HANDLE( OnGamepadUIButtonNavigatedTo, "OnGamepadUIButtonNavigatedTo", button );

    void LayoutChapterButtons();

    void OnMouseWheeled( int delta ) OVERRIDE;

    void StartGame( int nChapter );

private:
    CUtlVector< GamepadUIChapterButton* > m_pChapterButtons;
    CUtlVector< chapter_t > m_Chapters;

    GamepadUIScrollState m_ScrollState;

    GAMEPADUI_PANEL_PROPERTY( float, m_ChapterOffsetX, "Chapters.OffsetX", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_ChapterOffsetY, "Chapters.OffsetY", "0", SchemeValueTypes::ProportionalFloat );
    GAMEPADUI_PANEL_PROPERTY( float, m_ChapterSpacing, "Chapters.Spacing", "0", SchemeValueTypes::ProportionalFloat );

    bool m_bCommentaryMode = false;
};

class GamepadUIChapterButton : public GamepadUIButton
{
public:
    DECLARE_CLASS_SIMPLE( GamepadUIChapterButton, GamepadUIButton );

    GamepadUIChapterButton( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char* pSchemeFile, const char* pCommand, const char* pText, const char* pDescription, const char *pChapterImage )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_Image( pChapterImage )
    {
    }

    GamepadUIChapterButton( vgui::Panel* pParent, vgui::Panel* pActionSignalTarget, const char* pSchemeFile, const char* pCommand, const wchar* pText, const wchar* pDescription, const char *pChapterImage )
        : BaseClass( pParent, pActionSignalTarget, pSchemeFile, pCommand, pText, pDescription )
        , m_Image( pChapterImage )
    {
    }

    ~GamepadUIChapterButton()
    {
        if ( s_pLastNewGameButton == this )
            s_pLastNewGameButton = NULL;
    }

    ButtonState GetCurrentButtonState() OVERRIDE
    {
        if ( s_pLastNewGameButton == this )
            return ButtonState::Over;
        return BaseClass::GetCurrentButtonState();
    }

    void Paint() OVERRIDE
    {
        int x, y, w, t;
        GetBounds( x, y, w, t );

        PaintButton();

        vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
        vgui::surface()->DrawSetTexture( m_Image );
        int imgH = ( w * 9 ) / 16;
        //vgui::surface()->DrawTexturedRect( 0, 0, w, );
        float offset = m_flTextOffsetYAnimationValue[ButtonStates::Out] - m_flTextOffsetY;
        vgui::surface()->DrawTexturedSubRect( 0, 0, w, imgH - offset, 0.0f, 0.0f, 1.0f, ( imgH - offset ) / imgH );
        vgui::surface()->DrawSetTexture( 0 );
        if ( !IsEnabled() )
        {
            vgui::surface()->DrawSetColor( Color( 255, 255, 255, 16 ) );
            vgui::surface()->DrawFilledRect( 0, 0, w, imgH - offset );
        }

        PaintText();
    }
	
    void ApplySchemeSettings( vgui::IScheme* pScheme )
    {
        BaseClass::ApplySchemeSettings( pScheme );

        if (GamepadUI::GetInstance().GetScreenRatio() != 1.0f)
        {
            float flScreenRatio = GamepadUI::GetInstance().GetScreenRatio();

            m_flHeight *= flScreenRatio;
            for (int i = 0; i < ButtonStates::Count; i++)
                m_flHeightAnimationValue[i] *= flScreenRatio;

            // Also change the text offset
            m_flTextOffsetY *= flScreenRatio;
            for (int i = 0; i < ButtonStates::Count; i++)
                m_flTextOffsetYAnimationValue[i] *= flScreenRatio;

            SetSize( m_flWidth, m_flHeight + m_flExtraHeight );
            DoAnimations( true );
        }
    }

    void NavigateTo() OVERRIDE
    {
        BaseClass::NavigateTo();
        s_pLastNewGameButton = this;
    }

    static GamepadUIChapterButton* GetLastNewGameButton() { return s_pLastNewGameButton; }

private:
    GamepadUIImage m_Image;

    static GamepadUIChapterButton *s_pLastNewGameButton;
};

GamepadUIChapterButton* GamepadUIChapterButton::s_pLastNewGameButton = NULL;

/* From GameUI: */
// TODO: Modders may need to override this. (Madi)
static const int MAX_CHAPTERS = 32;

struct chapter_t
{
    char filename[32];
};

static int __cdecl ChapterSortFunc( const void *elem1, const void *elem2 )
{
    chapter_t *c1 = ( chapter_t * )elem1;
    chapter_t *c2 = ( chapter_t * )elem2;

    // compare chapter number first
    static int chapterlen = strlen( "chapter" );
    if ( atoi( c1->filename + chapterlen ) > atoi( c2->filename + chapterlen ) )
        return 1;
    else if ( atoi( c1->filename + chapterlen ) < atoi( c2->filename + chapterlen ) )
        return -1;

    // compare length second ( longer string show up later in the list, eg. chapter9 before chapter9a )
    if ( strlen( c1->filename ) > strlen( c2->filename ) )
        return 1;
    else if ( strlen( c1->filename ) < strlen( c2->filename ) )
        return -1;

    // compare strings third
    return strcmp( c1->filename, c2->filename );
}

static int FindChapters( chapter_t *pChapters )
{
    int chapterIndex = 0;
    char szFullFileName[MAX_PATH];

    FileFindHandle_t findHandle = FILESYSTEM_INVALID_FIND_HANDLE;
    const char *fileName = "cfg/chapter*.cfg";
    fileName = g_pFullFileSystem->FindFirst( fileName, &findHandle );
    while ( fileName && chapterIndex < MAX_CHAPTERS )
    {
        if ( fileName[0] )
        {
            // Only load chapter configs from the current mod's cfg dir
            // or else chapters appear that we don't want!
            Q_snprintf( szFullFileName, sizeof( szFullFileName ), "cfg/%s", fileName );
            FileHandle_t f = g_pFullFileSystem->Open( szFullFileName, "rb", "MOD" );
            if ( f )
            {
                // don't load chapter files that are empty, used in the demo
                if ( g_pFullFileSystem->Size( f ) > 0	 )
                {
                    Q_strncpy( pChapters[chapterIndex].filename, fileName, sizeof( pChapters[chapterIndex].filename ) );
                    ++chapterIndex;
                }
                g_pFullFileSystem->Close( f );
            }
        }
        fileName = g_pFullFileSystem->FindNext( findHandle );
    }

    qsort( pChapters, chapterIndex, sizeof( chapter_t ), &ChapterSortFunc );
    return chapterIndex;
}
/* End from GameUI */

static int GetUnlockedChapters()
{
    ConVarRef var( "sv_unlockedchapters" );

    return var.IsValid() ? MAX( var.GetInt(), 1 ) : 1;
}

GamepadUINewGamePanel::GamepadUINewGamePanel( vgui::Panel *pParent, const char* PanelName ) : BaseClass( pParent, PanelName )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFile( GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( hScheme );

    GetFrameTitle() = GamepadUIString( "#GameUI_NewGame" );
    FooterButtonMask buttons = FooterButtons::Back | FooterButtons::Select;
    if ( GameHasCommentary() )
        buttons |= FooterButtons::Commentary;
    if ( GameHasBonusMaps() )
        buttons |= FooterButtons::BonusMaps;
    SetFooterButtons( buttons, FooterButtons::Select );

    Activate();

    chapter_t chapters[MAX_CHAPTERS];
    int nChapterCount = FindChapters( chapters );

    for ( int i = 0; i < nChapterCount; i++ )
    {
        const char *fileName = chapters[i].filename;
        char chapterID[32] = { 0 };
        sscanf( fileName, "chapter%s", chapterID );
        // strip the extension
        char *ext = V_stristr( chapterID, ".cfg" );
        if ( ext )
        {
            *ext = 0;
        }
        const char* pGameDir = COM_GetModDirectory();

        char chapterName[64];
        Q_snprintf( chapterName, sizeof( chapterName ), "#%s_Chapter%s_Title", pGameDir, chapterID );

        char command[32];
        Q_snprintf( command, sizeof( command ), "chapter %d", i );

        wchar_t text[32];
        wchar_t num[32];
        wchar_t* chapter = g_pVGuiLocalize->Find( "#GameUI_Chapter" );
        g_pVGuiLocalize->ConvertANSIToUnicode( chapterID, num, sizeof( num ) );
        _snwprintf( text, ARRAYSIZE( text ), L"%ls %ls", chapter ? chapter : L"CHAPTER", num );

        GamepadUIString strChapterName( chapterName );

        char chapterImage[64];
        Q_snprintf( chapterImage, sizeof( chapterImage ), "gamepadui/chapter%s.vmt", chapterID );
        GamepadUIChapterButton *pChapterButton = new GamepadUIChapterButton(
            this, this,
            GAMEPADUI_CHAPTER_SCHEME, command,
            strChapterName.String(), text, chapterImage);
        pChapterButton->SetEnabled( i < GetUnlockedChapters() );
        pChapterButton->SetPriority( i );
        pChapterButton->SetForwardToParent( true );

        m_pChapterButtons.AddToTail( pChapterButton );
        m_Chapters.AddToTail( chapters[i] );
    }

    if ( m_pChapterButtons.Count() > 0 )
        m_pChapterButtons[0]->NavigateTo();

    for ( int i = 1; i < m_pChapterButtons.Count(); i++ )
    {
        m_pChapterButtons[i]->SetNavLeft( m_pChapterButtons[i - 1] );
        m_pChapterButtons[i - 1]->SetNavRight( m_pChapterButtons[i] );
    }

    UpdateGradients();
}

void GamepadUINewGamePanel::UpdateGradients()
{
    const float flTime = GamepadUI::GetInstance().GetTime();
    GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Up, { 1.0f, 1.0f }, flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Down, { 1.0f, 0.5f }, flTime );
}

void GamepadUINewGamePanel::OnThink()
{
    BaseClass::OnThink();

    LayoutChapterButtons();
}

void GamepadUINewGamePanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    if (GamepadUI::GetInstance().GetScreenRatio() != 1.0f)
    {
        float flScreenRatio = GamepadUI::GetInstance().GetScreenRatio();
        m_ChapterOffsetX *= (flScreenRatio*flScreenRatio);
    }
}

void GamepadUINewGamePanel::OnGamepadUIButtonNavigatedTo( vgui::VPANEL button )
{
    GamepadUIButton *pButton = dynamic_cast< GamepadUIButton * >( vgui::ipanel()->GetPanel( button, GetModuleName() ) );
    if ( pButton )
    {
        int nParentW, nParentH;
	    GetParent()->GetSize( nParentW, nParentH );

        int nX, nY;
        pButton->GetPos( nX, nY );
        if ( nX + pButton->m_flWidth > nParentW || nX < 0 )
        {
            int nTargetX = pButton->GetPriority() * (pButton->m_flWidth + m_ChapterSpacing);

            if ( nX < nParentW / 2 )
            {
                nTargetX += nParentW - m_ChapterOffsetX;
                // Add a bit of spacing to make this more visually appealing :)
                nTargetX -= m_ChapterSpacing;
            }
            else
            {
                nTargetX += pButton->m_flWidth;
                // Add a bit of spacing to make this more visually appealing :)
                nTargetX += (pButton->m_flWidth / 2) + m_ChapterSpacing;
            }


            m_ScrollState.SetScrollTarget( nTargetX - ( nParentW - m_ChapterOffsetX ), GamepadUI::GetInstance().GetTime() );
        }
    }
}

void GamepadUINewGamePanel::LayoutChapterButtons()
{
    int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );

    float flScrollClamp = 0.0f;
    for ( int i = 0; i < m_pChapterButtons.Count(); i++ )
    {
        int nSize = ( m_pChapterButtons[0]->GetWide() + m_ChapterSpacing );

        if ( i < m_pChapterButtons.Count() - 2 )
            flScrollClamp += nSize;
    }

    m_ScrollState.UpdateScrollBounds( 0.0f, flScrollClamp );

    for ( int i = 0; i < m_pChapterButtons.Count(); i++ )
    {
        int size = ( m_pChapterButtons[0]->GetWide() + m_ChapterSpacing );

        m_pChapterButtons[i]->SetPos( m_ChapterOffsetX + i * size - m_ScrollState.GetScrollProgress(), m_ChapterOffsetY );
        m_pChapterButtons[i]->SetVisible( true );
    }

    m_ScrollState.UpdateScrolling( 2.0f, GamepadUI::GetInstance().GetTime() );
}

void GamepadUINewGamePanel::OnCommand( char const* pCommand )
{
    if ( !V_strcmp( pCommand, "action_back" ) )
    {
        Close();
    }
    else if ( !V_strcmp( pCommand, "action_commentary" ) )
    {
        GamepadUIChapterButton *pPanel = GamepadUIChapterButton::GetLastNewGameButton();
        if ( pPanel )
        {
            m_bCommentaryMode = true;
            pPanel->DoClick();
        }
    }
    else if ( !V_strcmp( pCommand, "action_bonus_maps" ) )
    {
        GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( "gamepadui_openbonusmapsdialog\n" );
    }
    else if ( StringHasPrefixCaseSensitive( pCommand, "chapter " ) )
    {
        const char* pszEngineCommand = pCommand + 8;
        if ( *pszEngineCommand )
            StartGame( atoi( pszEngineCommand ) );

        Close();
    }
    else
    {
        BaseClass::OnCommand( pCommand );
    }
}

void GamepadUINewGamePanel::OnMouseWheeled( int nDelta )
{
    m_ScrollState.OnMouseWheeled( nDelta * m_ChapterSpacing * 20.0f, GamepadUI::GetInstance().GetTime() );
}

struct MapCommand_t
{
    char szCommand[512];
};

void GamepadUINewGamePanel::StartGame( int nChapter )
{
    MapCommand_t cmd;
    cmd.szCommand[0] = 0;
    Q_snprintf( cmd.szCommand, sizeof( cmd.szCommand ), "disconnect\ndeathmatch 0\nprogress_enable\nexec %s\n", m_Chapters[nChapter].filename );

    // Set commentary
    ConVarRef commentary( "commentary" );
    commentary.SetValue( m_bCommentaryMode );

    ConVarRef sv_cheats( "sv_cheats" );
    sv_cheats.SetValue( m_bCommentaryMode );

    // If commentary is on, we go to the explanation dialog ( but not for teaser trailers )
    if ( m_bCommentaryMode )//&& !m_ChapterPanels[m_iSelectedChapter]->IsTeaserChapter() )
    {
        // Check our current state and disconnect us from any multiplayer server we're connected to.
        // This fixes an exploit where players would click "start" on the commentary dialog to enable
        // sv_cheats on the client ( via the code above ) and then hit <esc> to get out of the explanation dialog.
        if ( GamepadUI::GetInstance().IsInMultiplayer() )
        {
            GamepadUI::GetInstance().GetEngineClient()->ExecuteClientCmd( "disconnect" );
        }

		new GamepadUIGenericConfirmationPanel( GamepadUI::GetInstance().GetBasePanel(), "SaveOverwriteConfirmationPanel", "#GameUI_LoadCommentary", "#GAMEUI_Commentary_Console_Explanation",
		[cmd]()
		{
			GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( cmd.szCommand );
		}, true);
    }
    else
    {
        GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( cmd.szCommand );
    }
}

CON_COMMAND( gamepadui_opennewgamedialog, "" )
{
    new GamepadUINewGamePanel( GamepadUI::GetInstance().GetBasePanel(), "" );
}
