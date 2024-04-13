#include "gamepadui_scrollbar.h"
#include "vgui/IInput.h"

void GamepadUIScrollBar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    GetPos( m_nStartX, m_nStartY );

    if (m_bHorizontal)
    {
        m_flScrollSize = m_flWidth;
    }
    else
    {
        m_flScrollSize = m_flHeight;
    }
}

void GamepadUIScrollBar::OnThink()
{
    BaseClass::OnThink();

    if (!m_pScrollState)
        return;

    if (m_nMouseOffset != -1)
    {
        int nMouseX, nMouseY;
        vgui::input()->GetCursorPos( nMouseX, nMouseY );

        if (m_bHorizontal)
        {
            float flRatio = ((float)((nMouseX - m_nStartX) - m_nMouseOffset)) / m_flScrollSize;

            m_pScrollState->SetScrollTarget( flRatio * m_flMax, GamepadUI::GetInstance().GetTime() );
        }
        else
        {
            float flRatio = ((float)((nMouseY - m_nStartY) - m_nMouseOffset)) / m_flScrollSize;

            m_pScrollState->SetScrollTarget( flRatio * m_flMax, GamepadUI::GetInstance().GetTime() );
        }
    }
    else if (m_flKeyDir != 0)
    {
        m_pScrollState->SetScrollTarget( m_pScrollState->GetScrollProgress() + m_flKeyDir, GamepadUI::GetInstance().GetTime() );
    }

    if (m_flMax > 0.0f)
    {
        if (m_bHorizontal)
        {
            int nX = m_nStartX + (m_flScrollSize * ((m_pScrollState->GetScrollProgress() - m_flMin) / m_flMax));
            SetPos( nX, m_nStartY );
        }
        else
        {
            int nY = m_nStartY + (m_flScrollSize * ((m_pScrollState->GetScrollProgress() - m_flMin) / m_flMax));
            SetPos( m_nStartX, nY );
        }
    }
}

void GamepadUIScrollBar::InitScrollBar( GamepadUIScrollState *pScrollState, int nX, int nY )
{
    m_pScrollState = pScrollState;

    m_nStartX = nX;
    m_nStartY = nY;
}

void GamepadUIScrollBar::UpdateScrollBounds( float flMin, float flMax, float flRegionSize, float flScrollSize )
{
    if (flMin == 0.0f && flMax == 0.0f)
    {
        SetVisible( false );
        m_pScrollState = NULL;
        return;
    }

    SetVisible( true );

    m_flMin = flMin;
    m_flMax = flMax;
    m_flRegionSize = flRegionSize;

    if (m_bHorizontal)
    {
        m_flWidth = flScrollSize * (m_flRegionSize / (m_flMax + m_flRegionSize));
        for (int i = 0; i < ButtonStates::Count; i++)
            m_flWidthAnimationValue[i] = m_flWidth;

        m_flScrollSize = flScrollSize - m_flWidth;
    }
    else
    {
        m_flHeight = flScrollSize * (m_flRegionSize / (m_flMax + m_flRegionSize));
        for (int i = 0; i < ButtonStates::Count; i++)
            m_flHeightAnimationValue[i] = m_flHeight;

        m_flScrollSize = flScrollSize - m_flHeight;
    }
}

void GamepadUIScrollBar::OnMousePressed( vgui::MouseCode code )
{
    BaseClass::OnMousePressed( code );

    int nX, nY;
    GetPos( nX, nY );

    int nMouseX, nMouseY;
    vgui::input()->GetCursorPos( nMouseX, nMouseY );

    m_nMouseOffset = (m_bHorizontal ? nMouseX - nX : nMouseY - nY);
}

void GamepadUIScrollBar::OnMouseReleased( vgui::MouseCode code )
{
    BaseClass::OnMouseReleased( code );

    m_nMouseOffset = -1;
}

void GamepadUIScrollBar::OnKeyCodePressed( vgui::KeyCode code )
{
    ButtonCode_t buttonCode = GetBaseButtonCode( code );

    switch ( buttonCode )
    {
        case KEY_UP:
        case KEY_XBUTTON_UP:
            m_flKeyDir = -m_flScrollSpeed;
            break;
        case KEY_DOWN:
        case KEY_XBUTTON_DOWN:
            m_flKeyDir = m_flScrollSpeed;
            break;
        default:
            return BaseClass::OnKeyCodePressed( code );
    }
}

void GamepadUIScrollBar::OnKeyCodeReleased( vgui::KeyCode code )
{
    ButtonCode_t buttonCode = GetBaseButtonCode( code );

    switch ( buttonCode )
    {
        case KEY_UP:
        case KEY_XBUTTON_UP:
        case KEY_DOWN:
        case KEY_XBUTTON_DOWN:
            m_flKeyDir = 0;
            break;
        default:
            return BaseClass::OnKeyCodeReleased( code );
    }
}
