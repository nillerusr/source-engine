// 🐸
#include "gamepadui_frame.h"
#include "gamepadui_button.h"
#include "gamepadui_interface.h"

#include "inputsystem/iinputsystem.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

GamepadUIFrame::GamepadUIFrame( vgui::Panel *pParent, const char *pszPanelName, bool bShowTaskbarIcon, bool bPopup )
    : BaseClass( pParent, pszPanelName, bShowTaskbarIcon, bPopup )
{
    SetConsoleStylePanel( true );
    // bodge to disable the frames title image and display our own
    // (the frames title has an invalid zpos and does not draw over the garnish)
    Frame::SetTitle( "", false );

	memset( &m_pFooterButtons, 0, sizeof( m_pFooterButtons ) );
}

void GamepadUIFrame::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    // Josh: Big load of common default state for us
    MakeReadyForUse();
    SetMoveable( false );
    SetCloseButtonVisible( false );
    SetMinimizeButtonVisible( false );
    SetMaximizeButtonVisible( false );
    SetMenuButtonResponsive( false );
    SetMinimizeToSysTrayButtonVisible( false );
    SetSizeable( false );
    SetDeleteSelfOnClose( true );
    SetPaintBackgroundEnabled( false );
    SetPaintBorderEnabled( false );
    SetTitleBarVisible( false );

	int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );
	SetBounds( 0, 0, nParentW, nParentH );
    UpdateSchemeProperties( this, pScheme );

    m_hTitleFont = pScheme->GetFont( "Title.Font", true );
    m_hGenericFont = pScheme->GetFont( "Generic.Text.Font", true );
}

void GamepadUIFrame::OnThink()
{
    BaseClass::OnThink();

    LayoutFooterButtons();
}

void GamepadUIFrame::Paint()
{
    BaseClass::Paint();

    PaintBackgroundGradients();
    PaintTitle();
}

void GamepadUIFrame::UpdateGradients()
{
}

void GamepadUIFrame::PaintBackgroundGradients()
{
    vgui::surface()->DrawSetColor( Color( 0, 0, 0, 255 ) );

    GradientHelper* pGradients = GamepadUI::GetInstance().GetGradientHelper();
    const float flTime = GamepadUI::GetInstance().GetTime();
    GradientInfo gradients[ GradientSides::Count ] =
    {
        pGradients->GetGradient( GradientSides::Left,  flTime ),
        pGradients->GetGradient( GradientSides::Right, flTime ),
        pGradients->GetGradient( GradientSides::Up,    flTime ),
        pGradients->GetGradient( GradientSides::Down,  flTime ),
    };

    if ( gradients[ GradientSides::Left ].flExtent &&
         gradients[ GradientSides::Left ].flAmount )
    {
        vgui::surface()->DrawFilledRectFade(
            0,
            0,
            GetWide() * gradients[ GradientSides::Left ].flExtent,
            GetTall(),
            255.0f * gradients[ GradientSides::Left ].flAmount, 0,
            true);
    }

    if ( gradients[ GradientSides::Right ].flExtent &&
         gradients[ GradientSides::Right ].flAmount )
    {
        vgui::surface()->DrawFilledRectFade(
            0,
            0,
            GetWide() * gradients[ GradientSides::Right ].flExtent,
            GetTall(),
            0, 255.0f * gradients[ GradientSides::Right ].flAmount,
            true);
    }

    if ( gradients[ GradientSides::Down ].flExtent &&
         gradients[ GradientSides::Down ].flAmount )
    {
        vgui::surface()->DrawFilledRectFade(
            0,
            0,
            GetWide(),
            GetTall() * gradients[ GradientSides::Down ].flExtent,
            255.0f * gradients[ GradientSides::Down ].flAmount, 0,
            false);
    }

    if ( gradients[ GradientSides::Up ].flExtent &&
         gradients[ GradientSides::Up ].flAmount )
    {
        vgui::surface()->DrawFilledRectFade(
            0,
            0,
            GetWide(),
            GetTall() * gradients[ GradientSides::Up ].flExtent,
            0, 255.0f * gradients[ GradientSides::Up ].flAmount,
            false);
    }
}

void GamepadUIFrame::PaintTitle()
{
    if ( m_strFrameTitle.IsEmpty() )
        return;

    vgui::surface()->DrawSetTextColor( m_colTitleColor );
    vgui::surface()->DrawSetTextFont( m_hTitleFont );
    vgui::surface()->DrawSetTextPos( m_flTitleOffsetX, m_flTitleOffsetY );
    vgui::surface()->DrawPrintText( m_strFrameTitle.String(), m_strFrameTitle.Length() );
}

void GamepadUIFrame::SetFooterButtons( FooterButtonMask mask, FooterButtonMask controllerOnlyMask )
{
    for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
    {
        if ( m_pFooterButtons[i] )
        {
            delete m_pFooterButtons[i];
            m_pFooterButtons[i] = NULL;
        }

        FooterButton button = FooterButtons::GetButtonByIdx( i );
        if ( mask & button )
        {
            m_pFooterButtons[i] = new GamepadUIButton(
                this, this,
                GAMEPADUI_DEFAULT_PANEL_SCHEME, FooterButtons::GetButtonAction( button ),
                FooterButtons::GetButtonName( button ), "" );
            m_pFooterButtons[i]->SetFooterButton( button );
            // Make footer buttons render over everything.
            m_pFooterButtons[i]->SetZPos( 100 );
        }
    }

    m_ControllerOnlyFooterMask = controllerOnlyMask;
}

void GamepadUIFrame::LayoutFooterButtons()
{
    int nParentW, nParentH;
	GetParent()->GetSize( nParentW, nParentH );

    int nLeftOffset = 0, nRightOffset = 0;
    for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
    {
        if ( !m_pFooterButtons[i] || !m_pFooterButtons[i]->IsVisible() )
            continue;

        m_nFooterButtonWidth = m_pFooterButtons[i]->GetWide();
        m_nFooterButtonHeight = m_pFooterButtons[i]->GetTall();
        FooterButton button = FooterButtons::GetButtonByIdx( i );
        if ( button & FooterButtons::LeftMask )
        {
            m_pFooterButtons[i]->SetPos( m_flFooterButtonsOffsetX + nLeftOffset, nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight );
            nLeftOffset += m_flFooterButtonsSpacing + m_pFooterButtons[i]->GetWide();
        }
        else
        {
            m_pFooterButtons[i]->SetPos( nParentW - m_pFooterButtons[i]->GetWide() - nRightOffset - m_flFooterButtonsOffsetX, nParentH - m_flFooterButtonsOffsetY - m_nFooterButtonHeight );
            nRightOffset += m_flFooterButtonsSpacing + m_pFooterButtons[i]->GetWide();
        }

#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
        const bool bController = g_pInputSystem->IsSteamControllerActive();
#else
        const bool bController = ( g_pInputSystem->GetJoystickCount() >= 1 );
#endif

        const bool bVisible = bController || !( m_ControllerOnlyFooterMask & button );
        m_pFooterButtons[i]->SetVisible( bVisible );
    }
}

void GamepadUIFrame::OnClose()
{
    BaseClass::OnClose();

    GamepadUIFrame *pFrame = dynamic_cast<GamepadUIFrame *>( GetParent() );
    if ( pFrame )
        pFrame->UpdateGradients();
    else
        GamepadUI::GetInstance().ResetToMainMenuGradients();
}

void GamepadUIFrame::OnKeyCodePressed( vgui::KeyCode code )
{
    ButtonCode_t buttonCode = GetBaseButtonCode( code );
    switch (buttonCode)
    {
#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
    case STEAMCONTROLLER_A:
#endif

    case KEY_XBUTTON_A:
    case KEY_ENTER:
        for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
        {
            if ( FooterButtons::GetButtonByIdx(i) & FooterButtons::ConfirmMask )
            {
                if ( m_pFooterButtons[i] )
                    m_pFooterButtons[i]->ForceDepressed( true );
            }
        }
        break;

#ifdef HL2_RETAIL
    case STEAMCONTROLLER_Y:
#endif

    case KEY_XBUTTON_Y:
        for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
        {
            if ( FooterButtons::GetButtonByIdx(i) & ( FooterButtons::Apply | FooterButtons::Commentary | FooterButtons::Challenge ) )
            {
                if ( m_pFooterButtons[i] )
                    m_pFooterButtons[i]->ForceDepressed( true );
            }
        }
        break;

#ifdef HL2_RETAIL
    case STEAMCONTROLLER_X:
#endif

    case KEY_XBUTTON_X:
        for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
        {
            if ( FooterButtons::GetButtonByIdx(i) & ( FooterButtons::BonusMaps | FooterButtons::UseDefaults ) )
            {
                if ( m_pFooterButtons[i] )
                    m_pFooterButtons[i]->ForceDepressed( true );
            }
        }
        break;

#ifdef HL2_RETAIL
    case STEAMCONTROLLER_B:
#endif

    case KEY_XBUTTON_B:
    case KEY_ESCAPE:
        for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
        {
            if ( FooterButtons::GetButtonByIdx(i) & FooterButtons::DeclineMask )
            {
                if ( m_pFooterButtons[i] )
                    m_pFooterButtons[i]->ForceDepressed( true );
            }
        }
        break;
    default:
        BaseClass::OnKeyCodePressed( code );
        break;
    }
}
void GamepadUIFrame::OnKeyCodeReleased( vgui::KeyCode code )
{
    ButtonCode_t buttonCode = GetBaseButtonCode( code );
    switch (buttonCode)
    {

#ifdef HL2_RETAIL
    case STEAMCONTROLLER_A:
#endif

    case KEY_XBUTTON_A:
    case KEY_ENTER:
        for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
        {
            if ( FooterButtons::GetButtonByIdx(i) & FooterButtons::ConfirmMask )
            {
                if ( m_pFooterButtons[i] )
                {
                    if ( m_pFooterButtons[i]->IsDepressed() )
                    {
                        m_pFooterButtons[i]->ForceDepressed( false );
                        m_pFooterButtons[i]->DoClick();
                    }
                }
            }
        }
        break;

#ifdef HL2_RETAIL
    case STEAMCONTROLLER_Y:
#endif

    case KEY_XBUTTON_Y:
        for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
        {
            if ( FooterButtons::GetButtonByIdx(i) & ( FooterButtons::Apply | FooterButtons::Commentary | FooterButtons::Challenge ) )
            {
                if ( m_pFooterButtons[i] )
                {
                    if ( m_pFooterButtons[i]->IsDepressed() )
                    {
                        m_pFooterButtons[i]->ForceDepressed( false );
                        m_pFooterButtons[i]->DoClick();
                    }
                }
            }
        }
        break;

#ifdef HL2_RETAIL
    case STEAMCONTROLLER_X:
#endif

    case KEY_XBUTTON_X:
        for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
        {
            if ( FooterButtons::GetButtonByIdx(i) & ( FooterButtons::BonusMaps | FooterButtons::UseDefaults ) )
            {
                if ( m_pFooterButtons[i] )
                {
                    if ( m_pFooterButtons[i]->IsDepressed() )
                    {
                        m_pFooterButtons[i]->ForceDepressed( false );
                        m_pFooterButtons[i]->DoClick();
                    }
                }
            }
        }
        break;

#ifdef HL2_RETAIL
    case STEAMCONTROLLER_B:
#endif

    case KEY_XBUTTON_B:
    case KEY_ESCAPE:
        for ( int i = 0; i < FooterButtons::MaxFooterButtons; i++ )
        {
            if ( FooterButtons::GetButtonByIdx(i) & FooterButtons::DeclineMask )
            {
                if ( m_pFooterButtons[i] )
                {
                    if ( m_pFooterButtons[i]->IsDepressed() )
                    {
                        m_pFooterButtons[i]->ForceDepressed( false );
                        m_pFooterButtons[i]->DoClick();
                    }
                }
            }
        }
        break;
    default:
        BaseClass::OnKeyCodeReleased( code );
        break;
    }
}

