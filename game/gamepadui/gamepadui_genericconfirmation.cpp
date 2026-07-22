#include "gamepadui_interface.h"
#include "gamepadui_genericconfirmation.h"
#include "gamepadui_util.h"

#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include "tier0/memdbgon.h"

GamepadUIGenericConfirmationPanel::GamepadUIGenericConfirmationPanel( vgui::Panel *pParent, const char* pPanelName, const char* pTitle, const char* pText, std::function<void()> pCommand, bool bSmallFont, bool bShowCancel )
    : BaseClass( pParent, pPanelName )
    , m_pCommand( std::move( pCommand ) )
    , m_pszGenericConfirmationFontName( bSmallFont ? "Generic.Text.Font" : "GenericConfirmation.Font" )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFile( GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( hScheme );

    GetFrameTitle() = GamepadUIString( pTitle );
    m_strText = GamepadUIString( pText );
    FooterButtonMask buttons = FooterButtons::Okay;
    if ( bShowCancel )
        buttons |= FooterButtons::Cancel;
    SetFooterButtons( buttons );

    Activate();

    UpdateGradients();
}

GamepadUIGenericConfirmationPanel::GamepadUIGenericConfirmationPanel( vgui::Panel *pParent, const char* pPanelName, const wchar_t* pTitle, const wchar_t* pText, std::function<void()> pCommand, bool bSmallFont, bool bShowCancel )
    : BaseClass( pParent, pPanelName )
    , m_pCommand( std::move( pCommand ) )
    , m_pszGenericConfirmationFontName( bSmallFont ? "Generic.Text.Font" : "GenericConfirmation.Font" )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFile( GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( hScheme );

    GetFrameTitle() = GamepadUIString( pTitle );
    m_strText = GamepadUIString( pText );
    FooterButtonMask buttons = FooterButtons::Okay;
    if ( bShowCancel )
        buttons |= FooterButtons::Cancel;
    SetFooterButtons( buttons );

    Activate();

    UpdateGradients();
}

void GamepadUIGenericConfirmationPanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    m_hGenericConfirmationFont = pScheme->GetFont( m_pszGenericConfirmationFontName, true );
}

void GamepadUIGenericConfirmationPanel::PaintText()
{
    if ( m_strText.IsEmpty() )
        return;

    vgui::surface()->DrawSetTextColor( m_colGenericConfirmationColor );
    vgui::surface()->DrawSetTextFont( m_hGenericConfirmationFont );
    vgui::surface()->DrawSetTextPos( m_flGenericConfirmationOffsetX, m_flGenericConfirmationOffsetY );
    DrawPrintWrappedText( m_hGenericConfirmationFont, m_flGenericConfirmationOffsetX, m_flGenericConfirmationOffsetY, m_strText.String(), m_strText.Length(), GetWide() - 2 * m_flGenericConfirmationOffsetX, true );
}

void GamepadUIGenericConfirmationPanel::UpdateGradients()
{
    const float flTime = GamepadUI::GetInstance().GetTime();
    GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Up, { 1.0f, 1.0f }, flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Left, { 1.0f, 0.6667f }, flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Down, { 1.0f, 1.0f }, flTime );
}

void GamepadUIGenericConfirmationPanel::Paint()
{
    BaseClass::Paint();

    PaintText();
    // Workaround focus shifting to main menu and getting the wrong gradients
    // causing us to not over paint everything leading to weird left over bits of text
    UpdateGradients();
}

void GamepadUIGenericConfirmationPanel::OnCommand( const char* pCommand )
{
    if ( !V_strcmp( pCommand, "action_cancel" ) )
    {
        Close();
    }
    else if ( !V_strcmp( pCommand, "action_okay" ) )
    {
        m_pCommand();
        Close();
    }
}

CON_COMMAND( gamepadui_openquitgamedialog, "" )
{
    new GamepadUIGenericConfirmationPanel( GamepadUI::GetInstance().GetBasePanel(), "QuitConfirmationPanel", "#GameUI_QuitConfirmationTitle", "#GameUI_QuitConfirmationText",
    []()
    {
        GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( "quit" );
    } );
}
