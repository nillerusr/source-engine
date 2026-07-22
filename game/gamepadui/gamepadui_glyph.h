#ifndef GAMEPADUI_GLYPH_H
#define GAMEPADUI_GLYPH_H
#ifdef _WIN32
#pragma once
#endif

#include "gamepadui_interface.h"
#include "gamepadui_util.h"

#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
#include "steam/hl2/isteaminput.h"
#include "imageutils.h"
#endif // HL2_RETAIL

#include "inputsystem/iinputsystem.h"
#include "bitmap/bitmap.h"

class GamepadUIGlyph
{
public:
    GamepadUIGlyph()
    {
        m_nOriginTextures[0] = -1;
        m_nOriginTextures[1] = -1;
    }

    ~GamepadUIGlyph()
    {
        Cleanup();
    }

    bool SetupGlyph( int nSize, const char *pszAction, bool bBaseLight = false )
    {
#ifdef HL2_RETAIL // Steam input and Steam Controller are not supported in SDK2013 (Madi)
        uint64 nSteamInputHandles[STEAM_INPUT_MAX_COUNT];
        if ( !GamepadUI::GetInstance().GetSteamAPIContext() || !GamepadUI::GetInstance().GetSteamAPIContext()->SteamInput() )
            return false;

        GamepadUI::GetInstance().GetSteamAPIContext()->SteamInput()->GetConnectedControllers( nSteamInputHandles );

        uint64 nController = g_pInputSystem->GetActiveSteamInputHandle();
        if ( !nController )
            nController = nSteamInputHandles[0];

        if ( !nController )
            return false;

        InputActionSetHandle_t hActionSet = GamepadUI::GetInstance().GetSteamAPIContext()->SteamInput()->GetCurrentActionSet( nController );
        InputDigitalActionHandle_t hDigitalAction = GamepadUI::GetInstance().GetSteamAPIContext()->SteamInput()->GetDigitalActionHandle( pszAction );
        if ( !hDigitalAction )
        {
            Cleanup();
            return false;
        }

        EInputActionOrigin eOrigins[STEAM_INPUT_MAX_ORIGINS] = {};
        int nOriginCount = GamepadUI::GetInstance().GetSteamAPIContext()->SteamInput()->GetDigitalActionOrigins( nController, hActionSet, hDigitalAction, eOrigins );
        EInputActionOrigin eOrigin = eOrigins[0];
        if ( !nOriginCount || eOrigin == k_EInputActionOrigin_None )
        {
            Cleanup();
            return false;
        }

        if ( m_eActionOrigin == eOrigin )
            return IsValid();

        Cleanup();

        m_eActionOrigin = eOrigin;

        if ( !IsPowerOfTwo( nSize ) )
            nSize = NextPowerOfTwo( nSize );

        int nGlyphSize = 256;
        ESteamInputGlyphSize eGlyphSize = k_ESteamInputGlyphSize_Large;
        if (nSize <= 32)
        {
            eGlyphSize = k_ESteamInputGlyphSize_Small;
            nGlyphSize = 32;
        }
        else if (nSize <= 128)
        {
            eGlyphSize = k_ESteamInputGlyphSize_Medium;
            nGlyphSize = 128;
        }
        else
        {
            eGlyphSize = k_ESteamInputGlyphSize_Large;
            nGlyphSize = 256;
        }

        ESteamInputGlyphStyle kStyles[2] =
        {
            bBaseLight ? ESteamInputGlyphStyle_Light : ESteamInputGlyphStyle_Knockout,
            ESteamInputGlyphStyle_Dark,
        };

        for ( int i = 0; i < 2; i++ )
        {
            const char* pszGlyph = GamepadUI::GetInstance().GetSteamAPIContext()->SteamInput()->GetGlyphPNGForActionOrigin( eOrigin, eGlyphSize, kStyles[i] );
            if (!pszGlyph)
            {
                Cleanup();
                return false;
            }

            Bitmap_t bitmap;
            ConversionErrorType error = ImgUtl_LoadBitmap( pszGlyph, bitmap );
            if ( error != CE_SUCCESS )
            {
                Cleanup();
                return false;
            }

            ImgUtl_ResizeBitmap( bitmap, nSize, nSize, &bitmap );

            m_nOriginTextures[i] = vgui::surface()->CreateNewTextureID(true);
            if ( m_nOriginTextures[i] <= 0 )
            {
                Cleanup();
                return false;
            }
            g_pMatSystemSurface->DrawSetTextureRGBAEx2( m_nOriginTextures[i], bitmap.GetBits(), bitmap.Width(), bitmap.Height(), ImageFormat::IMAGE_FORMAT_RGBA8888, true, false );
        }
#else
        return false;

#endif // HL2_RETAIL

        return true;
    }

    void PaintGlyph( int nX, int nY, int nSize, int nBaseAlpha )
    {
        int nPressedAlpha = 255 - nBaseAlpha;

        if ( nBaseAlpha )
        {
            vgui::surface()->DrawSetColor( Color( 255, 255, 255, nBaseAlpha ) );
            vgui::surface()->DrawSetTexture( m_nOriginTextures[0] );
            vgui::surface()->DrawTexturedRect( nX, nY, nX + nSize, nY + nSize );
        }

        if ( nPressedAlpha )
        {
            vgui::surface()->DrawSetColor( Color( 255, 255, 255, nPressedAlpha ) );
            vgui::surface()->DrawSetTexture( m_nOriginTextures[1] );
            vgui::surface()->DrawTexturedRect( nX, nY, nX + nSize, nY + nSize );
        }

        vgui::surface()->DrawSetTexture(0);
    }

    bool IsValid()
    {
        return m_nOriginTextures[0] > 0 && m_nOriginTextures[1] > 0;
    }

    void Cleanup()
    {
        for ( int i = 0; i < 2; i++ )
        {
            if ( m_nOriginTextures[ i ] > 0 )
                vgui::surface()->DestroyTextureID( m_nOriginTextures[ i ] );
            m_nOriginTextures[ i ] = -1;
        }

#ifdef HL2_RETAIL
        m_eActionOrigin = k_EInputActionOrigin_None;
#endif // HL2_RETAIL
    }

private:

#ifdef HL2_RETAIL
    EInputActionOrigin m_eActionOrigin = k_EInputActionOrigin_None;
#endif // HL2_RETAIL

    int m_nOriginTextures[2];
};

#endif
