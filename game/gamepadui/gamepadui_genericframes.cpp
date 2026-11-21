#include "gamepadui_interface.h"
#include "gamepadui_genericframes.h"
#include "gamepadui_util.h"

#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

#include "tier0/memdbgon.h"

GamepadUIGenericBasePanel::GamepadUIGenericBasePanel( vgui::Panel *pParent, const char* pPanelName, const char* pTitle, std::function<void()> pCommand, bool bShowCancel )
    : BaseClass( pParent, pPanelName )
    , m_pCommand( std::move( pCommand ) )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFileEx( GamepadUI::GetInstance().GetSizingVPanel(), GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( hScheme );

    GetFrameTitle() = GamepadUIString( pTitle );
    FooterButtonMask buttons = FooterButtons::Okay;
    if ( bShowCancel )
        buttons |= FooterButtons::Cancel;
    SetFooterButtons( buttons );

    Activate();

    UpdateGradients();
}

GamepadUIGenericBasePanel::GamepadUIGenericBasePanel( vgui::Panel *pParent, const char* pPanelName, const wchar_t* pTitle, std::function<void()> pCommand, bool bShowCancel )
    : BaseClass( pParent, pPanelName )
    , m_pCommand( std::move( pCommand ) )
{
    vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFileEx( GamepadUI::GetInstance().GetSizingVPanel(), GAMEPADUI_DEFAULT_PANEL_SCHEME, "SchemePanel" );
    SetScheme( hScheme );

    GetFrameTitle() = GamepadUIString( pTitle );
    FooterButtonMask buttons = FooterButtons::Okay;
    if ( bShowCancel )
        buttons |= FooterButtons::Cancel;
    SetFooterButtons( buttons );

    Activate();

    UpdateGradients();
}

void GamepadUIGenericBasePanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    if ( CenterPanel() )
    {
        int nWide, nTall;
        ContentSize( nWide, nTall );

        int nParentW, nParentH;
        GetParent()->GetSize( nParentW, nParentH );

        int nYOffset = (nParentH / 2 - nTall / 2) - (m_flTitleOffsetY * 0.5f);

        m_flFooterButtonsOffsetY = (nYOffset - m_flTitleOffsetY);
        m_flFooterButtonsOffsetX *= 3;

        m_flTitleOffsetY = nYOffset - (m_flContentOffsetY - m_flTitleOffsetY);
        m_flContentOffsetY = nYOffset;
    }
}

void GamepadUIGenericBasePanel::UpdateGradients()
{
    const float flTime = GamepadUI::GetInstance().GetTime();
    GamepadUI::GetInstance().GetGradientHelper()->ResetTargets( flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Up, { 1.0f, 1.0f }, flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Left, { 1.0f, 0.6667f }, flTime );
    GamepadUI::GetInstance().GetGradientHelper()->SetTargetGradient( GradientSide::Down, { 1.0f, 1.0f }, flTime );
}

void GamepadUIGenericBasePanel::Paint()
{
    BaseClass::Paint();

    PaintContent();
    // Workaround focus shifting to main menu and getting the wrong gradients
    // causing us to not over paint everything leading to weird left over bits of text
    UpdateGradients();
}

void GamepadUIGenericBasePanel::OnCommand( const char* pCommand )
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

//-----------------------------------------------------------------------------

GamepadUIGenericImagePanel::GamepadUIGenericImagePanel( vgui::Panel *pParent, const char* pPanelName, const char* pTitle, const char *pImage, float flImageWide, float flImageTall, std::function<void()> pCommand, bool bShowCancel )
    : BaseClass( pParent, pPanelName, pTitle, pCommand, bShowCancel )
{
    const char *pszExt = V_GetFileExtension( pImage );
    if (pszExt && V_strncmp( pszExt, "tga", 4 ) == 0)
    {
        m_Image.SetTGAImage( pImage );
    }
    else
    {
        m_Image.SetImage( pImage );
    }

    m_flBaseImageWide = flImageWide;
    m_flBaseImageTall = flImageTall;
}

GamepadUIGenericImagePanel::GamepadUIGenericImagePanel( vgui::Panel *pParent, const char* pPanelName, const wchar_t* pTitle, const char *pImage, float flImageWide, float flImageTall, std::function<void()> pCommand, bool bShowCancel )
    : BaseClass( pParent, pPanelName, pTitle, pCommand, bShowCancel )
{
    const char *pszExt = V_GetFileExtension( pImage );
    if (pszExt && V_strncmp( pszExt, "tga", 4 ) == 0)
    {
        m_Image.SetTGAImage( pImage );
    }
    else
    {
        m_Image.SetImage( pImage );
    }

    m_flBaseImageWide = flImageWide;
    m_flBaseImageTall = flImageTall;
}

void GamepadUIGenericImagePanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    m_flImageWide = float( vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), int( m_flBaseImageWide ) ) );
    m_flImageTall = float( vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), int( m_flBaseImageTall ) ) );

    // For now, always center the image independent of the frame
    int nParentW, nParentH;
    GetParent()->GetSize( nParentW, nParentH );

    m_flContentOffsetX = (nParentW / 2 - m_flImageWide / 2);
    m_flContentOffsetY = (nParentH / 2 - m_flImageTall / 2);
}

void GamepadUIGenericImagePanel::PaintContent()
{
    vgui::surface()->DrawSetTexture( m_Image );
    vgui::surface()->DrawSetColor( m_colContentColor );
    vgui::surface()->DrawTexturedRect( m_flContentOffsetX, m_flContentOffsetY, m_flContentOffsetX + m_flImageWide, m_flContentOffsetY + m_flImageTall );
    vgui::surface()->DrawSetTexture( 0 );
}

CON_COMMAND( gamepadui_opengenericimagedialog, "Opens a generic image dialog.\nFormat: gamepadui_opengenericimagedialog <title> <image> <width> <height> <optional: command>" )
{
    if (args.ArgC() < 5)
    {
        Msg("Format: gamepadui_opengenericimagedialog <title> <image> <width> <height> <optional: command>\n");
        return;
    }

    // Optional command
    const char *pCmd = NULL;
    if (args.ArgC() > 5)
        pCmd = args.Arg( 5 );

    // TODO: Parent to current frame
    new GamepadUIGenericImagePanel( GamepadUI::GetInstance().GetBasePanel(), "GenericPanel", args.Arg(1), args.Arg(2), atof(args.Arg(3)), atof(args.Arg(4)),
    [pCmd]()
    {
        if (pCmd)
        {
            // Replace '' with quotes
            char szCmd[512];
            V_StrSubst( pCmd, "''", "\"", szCmd, sizeof(szCmd) );

            GamepadUI::GetInstance().GetEngineClient()->ClientCmd_Unrestricted( szCmd );
        }
    }, false );
}
