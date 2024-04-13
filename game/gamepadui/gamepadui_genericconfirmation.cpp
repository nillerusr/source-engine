#include "gamepadui_interface.h"
#include "gamepadui_genericconfirmation.h"
#include "gamepadui_util.h"

#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include "tier0/memdbgon.h"

ConVar gamepadui_center_confirmation_panels( "gamepadui_center_confirmation_panels", "1", FCVAR_NONE, "Centers confirmation panels" );

GamepadUIGenericConfirmationPanel::GamepadUIGenericConfirmationPanel( vgui::Panel *pParent, const char* pPanelName, const char* pTitle, const char* pText, std::function<void()> pCommand, bool bSmallFont, bool bShowCancel )
    : BaseClass( pParent, pPanelName )
    , m_pCommand( std::move( pCommand ) )
    , m_pszGenericConfirmationFontName( bSmallFont ? "Generic.Text.Font" : "GenericConfirmation.Font" )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFileEx( GamepadUI::GetInstance().GetSizingVPanel(), GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
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
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFileEx( GamepadUI::GetInstance().GetSizingVPanel(), GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
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

    if ( !m_strText.IsEmpty() && gamepadui_center_confirmation_panels.GetBool() )
    {
        int nTall = DrawPrintWrappedText( m_hGenericConfirmationFont, m_flGenericConfirmationOffsetX, m_flGenericConfirmationOffsetY, m_strText.String(), m_strText.Length(), GetWide() - 2 * m_flGenericConfirmationOffsetX, false );

        int nParentW, nParentH;
        GetParent()->GetSize( nParentW, nParentH );

        int nYOffset = (nParentH / 2 - nTall / 2) - (m_flTitleOffsetY * 0.5f);

        m_flFooterButtonsOffsetY = (nYOffset - m_flTitleOffsetY);
        m_flFooterButtonsOffsetX *= 3;

        m_flTitleOffsetY = nYOffset - (m_flGenericConfirmationOffsetY - m_flTitleOffsetY);
        m_flGenericConfirmationOffsetY = nYOffset;
    }
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

CON_COMMAND( gamepadui_opengenerictextdialog, "Opens a generic text dialog.\nFormat: gamepadui_opengenerictextdialog <title> <text> <small font (0 or 1)>" )
{
    if (args.ArgC() < 4)
    {
        Msg("Format: gamepadui_opengenerictextdialog <title> <text> <small font (0 or 1)>\n");
        return;
    }

    vgui::Panel *pParent = GamepadUI::GetInstance().GetCurrentFrame();
    if (!pParent)
        pParent = GamepadUI::GetInstance().GetBasePanel();

    new GamepadUIGenericConfirmationPanel( pParent, "GenericConfirmationPanel", args.Arg(1), args.Arg(2),
    [](){}, args.Arg(3)[0] != '0', false );
}

CON_COMMAND( gamepadui_opengenericconfirmdialog, "Opens a generic confirmation dialog which executes a command.\nFormat: gamepadui_opengenericconfirmdialog <title> <text> <small font (0 or 1)> <command>\n<command> supports quotation marks as double apostrophes." )
{
    if (args.ArgC() < 5)
    {
        Msg("Format: gamepadui_opengenericconfirmdialog <title> <text> <small font (0 or 1)> <command>\n");
        return;
    }

    vgui::Panel *pParent = GamepadUI::GetInstance().GetCurrentFrame();
    if (!pParent)
        pParent = GamepadUI::GetInstance().GetBasePanel();

    // To get the command, we just use the remaining string after the small font parameter
    // This method is fairly dirty and relies a bit on guesswork, but it allows spaces and quotes to be used
    // without having to worry about how the initial dialog command handles it
    const char *pCmd = args.GetCommandString();
    char *pSmallFont = V_strstr( pCmd, args.Arg( 3 )[0] != '0' ? " 1 " : " 0 " );
    if (!pSmallFont)
    {
        // Look for quotes instead
        pSmallFont = V_strstr( pCmd, args.Arg( 3 )[0] != '0' ? " \"1\" " : " \"0\" " );
        if (pSmallFont)
            pCmd += (pSmallFont - pCmd) + 5;
        else
        {
            // Give up and use the 4th argument
            pCmd = args.Arg( 4 );
        }
    }
    else
    {
        pCmd += (pSmallFont - pCmd) + 3;
    }

    new GamepadUIGenericConfirmationPanel( pParent, "GenericConfirmationPanel", args.Arg(1), args.Arg(2),
    [pCmd]()
    {
        GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( pCmd );
    }, args.Arg(3)[0] != '0', true );
}
