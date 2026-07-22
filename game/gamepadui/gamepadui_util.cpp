#include "gamepadui_util.h"
#include "gamepadui_interface.h"

#include "tier0/icommandline.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Josh: Unused, but referenced by imageutils.cpp
// SDK2013: not necessary here (Madi)
#ifdef HL2_RETAIL
class IVEngineClient* engine = NULL;
#endif

// Josh: Copied verbatim from basically every other module
// we have on this planet.
const char *COM_GetModDirectory()
{
    static char szModDir[ MAX_PATH ] = {};
    if ( V_strlen( szModDir ) == 0 )
    {
        const char *pszGameDir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
        V_strncpy( szModDir, pszGameDir, sizeof(szModDir) );
        if ( strchr( szModDir, '/' ) || strchr( szModDir, '\\' ) )
        {
            V_StripLastDir( szModDir, sizeof(szModDir) );
            int nDirLen = V_strlen( szModDir );
            V_strncpy( szModDir, pszGameDir + nDirLen, sizeof(szModDir) - nDirLen );
        }
    }

    return szModDir;
}

int DrawPrintWrappedText(vgui::HFont font, int pX, int pY, const wchar_t* pszText, int nLength, int nMaxWidth, bool bDraw)
{
    if ( !pszText || nLength <= 0 || nMaxWidth <= 0 )
        return 0;

    float x = 0.0f;
    int extraY = 0;
    const int nFontTall = vgui::surface()->GetFontTall(font);
    const wchar_t* wszStrStart = pszText;
    const wchar_t* wszLastSpace = NULL;
    const wchar_t* wszEnd = pszText + nLength;

    for (const wchar_t* wsz = pszText; wsz < wszEnd; wsz++)
    {
        wchar_t ch = *wsz;

        if (ch == L' ' || ch == L'\n')
            wszLastSpace = wsz;

        x += vgui::surface()->GetCharacterWidth(font, ch);

        if (x >= nMaxWidth || ch == L'\n')
        {
            const wchar_t* wszBreak;
            const wchar_t* wszNextStart;

            if ( ch == L'\n' )
            {
                wszBreak = wsz;
                wszNextStart = wsz + 1;
            }
            else if ( wszLastSpace )
            {
                wszBreak = wszLastSpace;
                wszNextStart = wszLastSpace + 1;
            }
            else
            {
                // Break mid-word
                if ( wsz == wszStrStart )
                {
                    // Even a single character doesn't fit, just force it
                    wszBreak = wsz + 1;
                    wszNextStart = wsz + 1;
                }
                else
                {
                    wszBreak = wsz;
                    wszNextStart = wsz;
                }
            }

            if ( bDraw )
            {
                vgui::surface()->DrawSetTextPos(pX, pY);
                vgui::surface()->DrawPrintText(wszStrStart, (int)(wszBreak - wszStrStart));
            }

            wszStrStart = wszNextStart;
            wsz = wszNextStart - 1; // loop increment will move it to wszNextStart
            wszLastSpace = NULL;
            x = 0;
            pY += nFontTall;
            extraY += nFontTall;
        }
    }

    if (wszStrStart < wszEnd )
    {
        if ( bDraw )
        {
            vgui::surface()->DrawSetTextPos(pX, pY);
            vgui::surface()->DrawPrintText(wszStrStart, (int)(wszEnd - wszStrStart));
        }
    }

    return extraY;
}

int NextPowerOfTwo( int v )
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return ++v;
}
