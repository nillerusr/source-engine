#include "gamepadui_button.h"
#include "gamepadui_interface.h"
#include "gamepadui_util.h"

#include "vgui/IVGui.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DEFAULT_BTN_ARMED_SOUND "ui/buttonrollover.wav"
#define DEFAULT_BTN_RELEASED_SOUND "ui/buttonclickrelease.wav"


GamepadUIButton::GamepadUIButton( vgui::Panel *pParent, vgui::Panel* pActionSignalTarget, const char *pSchemeFile, const char *pCommand, const char *pText, const char *pDescription )
    : BaseClass( pParent, "", "", pActionSignalTarget, pCommand )
    , m_strButtonText( pText )
    , m_strButtonDescription( pDescription )
{
    SetScheme(vgui::scheme()->LoadSchemeFromFile( pSchemeFile, "SchemePanel" ) );
}

GamepadUIButton::GamepadUIButton( vgui::Panel *pParent, vgui::Panel* pActionSignalTarget, const char *pSchemeFile, const char *pCommand, const wchar_t *pText, const wchar_t *pDescription )
    : BaseClass( pParent, "", "", pActionSignalTarget, pCommand )
    , m_strButtonText( pText )
    , m_strButtonDescription( pDescription )
{
    SetScheme(vgui::scheme()->LoadSchemeFromFile( pSchemeFile, "SchemePanel" ) );
}

void GamepadUIButton::ApplySchemeSettings(vgui::IScheme* pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
	
    const char *pButtonSound = pScheme->GetResourceString( "Button.Sound.Armed" );
    if (pButtonSound && *pButtonSound)
        SetArmedSound( pButtonSound );
    else
        SetArmedSound( DEFAULT_BTN_ARMED_SOUND );

    pButtonSound = pScheme->GetResourceString( "Button.Sound.Released" );
    if (pButtonSound && *pButtonSound)
        SetReleasedSound( pButtonSound );
    else
        SetReleasedSound( DEFAULT_BTN_RELEASED_SOUND );

    pButtonSound = pScheme->GetResourceString( "Button.Sound.Depressed" );
    if (pButtonSound && *pButtonSound)
        SetDepressedSound( pButtonSound );

    SetPaintBorderEnabled( false );
    SetPaintBackgroundEnabled( false );
    SetConsoleStylePanel( true );

    UpdateSchemeProperties( this, pScheme );

    m_hTextFont        = pScheme->GetFont( "Button.Text.Font", true );
    m_hTextFontOver    = pScheme->GetFont( "Button.Text.Font.Over", true );
    if (m_hTextFontOver == vgui::INVALID_FONT )
        m_hTextFontOver = m_hTextFont;
    m_hDescriptionFont = pScheme->GetFont( "Button.Description.Font", true );

    m_ePreviousState = ButtonStates::Out;

    m_flCachedExtraHeight = 0.0f;
    if (!m_strButtonDescription.IsEmpty())
    {
        if ( m_bDescriptionWrap )
            m_flCachedExtraHeight = DrawPrintWrappedText(m_hDescriptionFont, 0, 0, m_strButtonDescription.String(), m_strButtonDescription.Length(), m_flWidthAnimationValue[ButtonStates::Over] - 2 * (m_flDescriptionOffsetX + m_flTextOffsetX), false);
        else
            m_flCachedExtraHeight = 0.0f;
    }
	
    if (GamepadUI::GetInstance().GetScreenRatio() != 1.0f)
    {
        float flScreenRatio = GamepadUI::GetInstance().GetScreenRatio();

        m_flWidth *= flScreenRatio;
        for (int i = 0; i < ButtonStates::Count; i++)
            m_flWidthAnimationValue[i] *= flScreenRatio;
    }

    SetSize( m_flWidth, m_flHeight + m_flExtraHeight );
    DoAnimations( true );
}

void GamepadUIButton::RunAnimations( ButtonState state )
{
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flWidth, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flHeight, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flTextOffsetX, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flTextOffsetY, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flDescriptionOffsetX, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flDescriptionOffsetY, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_colBackgroundColor, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_colTextColor, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_colDescriptionColor, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flGlyphFade, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_bDescriptionHide, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flTextLeftBorder, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_colLeftBorder, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_flTextBottomBorder, vgui::AnimationController::INTERPOLATOR_LINEAR );
    GAMEPADUI_RUN_ANIMATION_COMMAND( m_colBottomBorder, vgui::AnimationController::INTERPOLATOR_LINEAR );
}

void GamepadUIButton::DoAnimations( bool bForce )
{
    ButtonState state = this->GetCurrentButtonState();
    if (m_ePreviousState != state || bForce)
    {
        this->RunAnimations( state );
        m_ePreviousState = state;
    }

    SetSize(m_flWidth, m_flHeight + m_flExtraHeight);
}

void GamepadUIButton::OnThink()
{
    BaseClass::OnThink();
    DoAnimations();
}

void GamepadUIButton::PaintButton()
{
    vgui::surface()->DrawSetColor(m_colBackgroundColor);
    vgui::surface()->DrawFilledRect(0, 0, m_flWidth, m_flHeight + m_flExtraHeight);

    PaintBorders();
}

void GamepadUIButton::PaintBorders()
{
    if ( m_flTextLeftBorder )
    {
        vgui::surface()->DrawSetColor(m_colLeftBorder);
        vgui::surface()->DrawFilledRect(0, 0, m_flTextLeftBorder, m_flHeight + m_flExtraHeight);
    }

    if ( m_flTextBottomBorder )
    {
        vgui::surface()->DrawSetColor( m_colBottomBorder );
        vgui::surface()->DrawFilledRect( 0, m_flHeight + m_flExtraHeight - m_flTextBottomBorder, m_flWidth, m_flHeight + m_flExtraHeight );
    }
}

int GamepadUIButton::PaintText()
{
    ButtonState state = this->GetCurrentButtonState();
    int nTextPosX = 0, nTextPosY = 0;
    int nTextSizeX = 0, nTextSizeY = 0;

    if (!m_strButtonText.IsEmpty())
    {
        vgui::surface()->DrawSetTextFont(state == ButtonStates::Out ? m_hTextFont : m_hTextFontOver);
        vgui::surface()->GetTextSize(state == ButtonStates::Out ? m_hTextFont : m_hTextFontOver, m_strButtonText.String(), nTextSizeX, nTextSizeY);

        if (m_CenterX)
            nTextPosX = m_flWidth / 2 - nTextSizeX / 2 + m_flTextOffsetX;
        nTextPosX += m_flTextOffsetX;
        nTextPosY = m_flHeight / 2 - nTextSizeY / 2 + m_flTextOffsetY;
    }

#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
    if ( g_pInputSystem->IsSteamControllerActive() )
    {
        const int nGlyphSize = m_flHeight * 0.80f;
        if ( m_glyph.SetupGlyph( nGlyphSize, FooterButtons::GetButtonActionHandleString( m_eFooterButton ) ) )
        {
            int nGlyphPosX = m_flTextOffsetX;
            if (m_CenterX)
            {
                nGlyphPosX = nTextPosX - m_flHeight / 2;
                nTextPosX += m_flHeight / 2;
            }
            else
            {
                nTextPosX += nGlyphSize + (m_flTextOffsetX / 2);
            }
            int nGlyphPosY = m_flHeight / 2 - nGlyphSize / 2;

            int nAlpha = 255 * (1.0f - m_flGlyphFade);

            m_glyph.PaintGlyph( nGlyphPosX, nGlyphPosY, nGlyphSize, nAlpha );
        }
    }
#endif // HL2_RETAIL

    if (!m_strButtonText.IsEmpty())
    {
        vgui::surface()->DrawSetTextColor(m_colTextColor);
        vgui::surface()->DrawSetTextFont(state == ButtonStates::Out ? m_hTextFont : m_hTextFontOver);
        vgui::surface()->DrawSetTextPos(nTextPosX, nTextPosY);
        vgui::surface()->DrawPrintText(m_strButtonText.String(), m_strButtonText.Length());
    }

    float flCurTime = GamepadUI::GetInstance().GetTime();
    const float flHeightTransTime = m_flHeightAnimationDuration;
    float flNewExtraHeight = 0.0f;
    if (!m_strButtonDescription.IsEmpty() && !m_bDescriptionHide)
    {
        vgui::surface()->DrawSetTextColor(m_colDescriptionColor);
        vgui::surface()->DrawSetTextFont(m_hDescriptionFont);
        if ( m_bDescriptionWrap )
        {
            flNewExtraHeight = DrawPrintWrappedText(m_hDescriptionFont, nTextPosX + m_flDescriptionOffsetX, nTextPosY + nTextSizeY + m_flDescriptionOffsetY, m_strButtonDescription.String(), m_strButtonDescription.Length(), m_flWidth - 2 * (m_flDescriptionOffsetX + m_flTextOffsetX), true);
        }
        else
        {
            flNewExtraHeight = 0;
            vgui::surface()->DrawSetTextPos( nTextPosX + m_flDescriptionOffsetX, nTextPosY + nTextSizeY + m_flDescriptionOffsetY );
            vgui::surface()->DrawPrintText( m_strButtonDescription.String(), m_strButtonDescription.Length() );
        }
    }

    if ( m_flTargetExtraHeight != flNewExtraHeight)
    {
        m_flExtraHeightTime = flCurTime;
        m_flLastExtraHeight = m_flExtraHeight;
        m_flTargetExtraHeight = flNewExtraHeight;
    }

    m_flExtraHeight = Lerp( Clamp( ( flCurTime - m_flExtraHeightTime ) / flHeightTransTime, 0.0f, 1.0f ), m_flLastExtraHeight, m_flTargetExtraHeight );

    return nTextSizeX;
}

void GamepadUIButton::Paint()
{
    BaseClass::Paint();

    PaintButton();
    PaintText();

    m_bNavigateTo = false;
}


void GamepadUIButton::OnKeyCodePressed( vgui::KeyCode code )
{
    ButtonCode_t buttonCode = GetBaseButtonCode( code );
    switch ( buttonCode )
    {
#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
    case STEAMCONTROLLER_A:
#endif

    case KEY_XBUTTON_A:
    case KEY_ENTER:
        if ( IsEnabled() )
        {
            ForceDepressed( true );
            m_bControllerPressed = true;
        }
        BaseClass::OnKeyCodePressed( code );
        // Forward back up to parents for our buttons.
        if ( m_bForwardToParent )
            vgui::Panel::OnKeyCodePressed( code );
        break;
    default:
        if ( m_bControllerPressed )
        {
            ForceDepressed( false );
            m_bControllerPressed = false;
        }
        BaseClass::OnKeyCodePressed( code );
        break;
    }
}

void GamepadUIButton::OnKeyCodeReleased( vgui::KeyCode code )
{
    ButtonCode_t buttonCode = GetBaseButtonCode(code);
    switch ( buttonCode )
    {
#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
    case STEAMCONTROLLER_A:
#endif

    case KEY_XBUTTON_A:
    case KEY_ENTER:
        if ( IsEnabled() && IsDepressed() && m_bControllerPressed )
        {
            ForceDepressed( false );
            DoClick();
            m_bControllerPressed = false;
        }
        // Forward back up to parents for our buttons.
        if ( m_bForwardToParent )
            vgui::Panel::OnKeyCodeReleased( code );
        break;
    default:
        BaseClass::OnKeyCodeReleased( code );
        break;
    }
}

void GamepadUIButton::SetFooterButton( FooterButton button )
{
    m_eFooterButton = button;
    m_glyph.Cleanup();
}

FooterButton GamepadUIButton::GetFooterButton() const
{
    return m_eFooterButton;
}

void GamepadUIButton::SetForwardToParent( bool bForwardToParent )
{
    m_bForwardToParent = bForwardToParent;
}

bool GamepadUIButton::GetForwardToParent() const
{
    return m_bForwardToParent;
}

bool GamepadUIButton::IsFooterButton() const
{
    return m_eFooterButton != FooterButtons::None;
}

ButtonState GamepadUIButton::GetCurrentButtonState()
{
    if ( IsDepressed() )
        return ButtonStates::Pressed;
    else if ( HasFocus() && IsEnabled() )
        return ButtonStates::Over;
    else if ( IsArmed() || m_bNavigateTo )
    {
        if ( IsArmed() )
            m_bNavigateTo = false;

        return ButtonStates::Over;
    }
    else
        return ButtonStates::Out;
}

void GamepadUIButton::NavigateTo()
{
    BaseClass::NavigateTo();

    if ( IsFooterButton() )
        return;

    if ( GetVParent() )
    {
        KeyValues* msg = new KeyValues( "OnGamepadUIButtonNavigatedTo" );
        msg->SetInt( "button", ToHandle() );

        vgui::ivgui()->PostMessage( GetVParent(), msg, GetVPanel() );
    }

    m_bNavigateTo = true;
    RequestFocus( 0 );
}


void GamepadUIButton::NavigateFrom()
{
    BaseClass::NavigateFrom();

    m_bNavigateTo = false;
}

void GamepadUIButton::OnCursorEntered()
{
#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
    if ( g_pInputSystem->IsSteamControllerActive() || !IsEnabled() )
#else
    if ( !IsEnabled() )
#endif
        return;

    BaseClass::OnCursorEntered();

    if ( IsFooterButton() || !m_bMouseNavigate )
        return;

    if ( GetParent() )
        GetParent()->NavigateToChild( this );
    else
        NavigateTo();
}

void GamepadUIButton::FireActionSignal()
{
    BaseClass::FireActionSignal();

    PostMessageToAllSiblingsOfType< GamepadUIButton >( new KeyValues( "OnSiblingGamepadUIButtonOpened" ) );
}

void GamepadUIButton::OnSiblingGamepadUIButtonOpened()
{
    m_bNavigateTo = false;
}
