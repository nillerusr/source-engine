#ifndef GAMEPADUI_SCROLL_H
#define GAMEPADUI_SCROLL_H
#ifdef _WIN32
#pragma once
#endif

class GamepadUIScrollState
{
public:
    GamepadUIScrollState()
    {
    }

    void UpdateScrollBounds( float flMin, float flMax )
    {
        m_flScrollTarget = clamp( m_flScrollTarget, flMin, flMax );
    }

    void UpdateScrolling( float flScrollSpeed, float flTime )
    {
        float flLerpV = Min( ( flTime - m_flScrollLastScrolledTime ) * flScrollSpeed, 1.0f );
        flLerpV *= 2.0f - flLerpV;
        m_flScrollProgress = Lerp( flLerpV, m_flScrollLastScrolledValue, m_flScrollTarget );
    }

    float GetScrollProgress()
    {
        return m_flScrollProgress;
    }

    float GetScrollTarget() const
    {
        return m_flScrollTarget;
    }

    void SetScrollTarget( float flScrollTarget, float flTime )
    {
        m_flScrollTarget            = flScrollTarget;
        m_flScrollLastScrolledValue = m_flScrollProgress;
        m_flScrollLastScrolledTime  = flTime;
    }

    void OnMouseWheeled( int nDelta, float flTime )
    {
        SetScrollTarget( GetScrollTarget() - nDelta, flTime );
    }

private:
    float m_flScrollTarget            = 0.0f;
    float m_flScrollProgress          = 0.0f;
    float m_flScrollLastScrolledValue = 0.0f;
    float m_flScrollLastScrolledTime  = 0.0f;
};

#endif // GAMEPADUI_SCROLL_H
