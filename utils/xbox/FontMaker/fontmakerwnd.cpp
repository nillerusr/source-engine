//-----------------------------------------------------------------------------
// Name: FontMakerWnd.cpp
//
// Desc: The window and scroll view class for the fontmaker app
//
// Hist: 09.06.02 - Revised Fontmaker sample
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "FontMaker.h"
#include "Glyphs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CTextureFont g_Font;
BOOL        g_bIsGlyphSelected     = FALSE;
int         g_iSelectedGlyphNum    = 0;
WCHAR       g_cSelectedGlyph       = '\0';
GLYPH_ATTR* g_pSelectedGylph       = NULL;

// Colors
#define COLOR_WHITE   ( RGB(255,255,255) )
#define COLOR_BLACK   ( RGB(  0,  0,  0) )
#define COLOR_BLUE    ( RGB(  0,  0,255) )
#define COLOR_RED     ( RGB(255,  0,  0) )
#define COLOR_DARKRED ( RGB(128,  0,  0) )

//-----------------------------------------------------------------------------
// CFontMakerFrameWnd
//-----------------------------------------------------------------------------

IMPLEMENT_DYNCREATE(CFontMakerFrameWnd, CFrameWnd)

BEGIN_MESSAGE_MAP(CFontMakerFrameWnd, CFrameWnd)
    //{{AFX_MSG_MAP(CFontMakerFrameWnd)
    ON_WM_CREATE()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CFontMakerFrameWnd::OnCreate( LPCREATESTRUCT pCreateStruct )
{
    if( CFrameWnd::OnCreate( pCreateStruct ) == -1 )
        return -1;

    // Create a docked dialog bar
    EnableDocking( CBRS_ALIGN_ANY );
    if( !m_wndDialogBar.Create( this, IDD_DIALOGBAR,
                                CBRS_LEFT, IDD_DIALOGBAR ) )
    {
        TRACE0("Failed to create DlgBar\n");
        return -1;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// CFontMakerView
//-----------------------------------------------------------------------------

IMPLEMENT_DYNCREATE(CFontMakerView, CScrollView)

BEGIN_MESSAGE_MAP(CFontMakerView, CScrollView)
    //{{AFX_MSG_MAP(CFontMakerView)
    ON_WM_LBUTTONDOWN()
    ON_WM_ERASEBKGND()
    ON_WM_KEYDOWN()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Name: ~CFontMakerView()
// Desc: Destructor with special cleanup
//-----------------------------------------------------------------------------
CFontMakerView::~CFontMakerView()
{
    // Cleanup before exitting
    m_memDC.DeleteDC();
}

//-----------------------------------------------------------------------------
// Name: OnInitialUpdate()
// Desc: Called when the view is created.
//-----------------------------------------------------------------------------
void CFontMakerView::OnInitialUpdate()
{
    // Set the scroll bars
    CScrollView::OnInitialUpdate();
    CSize szTotal( g_Font.m_dwTextureWidth, g_Font.m_dwTextureHeight );
    SetScrollSizes( MM_TEXT, szTotal );
    CPoint ptCenter( 0, 0 );
    CenterOnPoint( ptCenter );

    // Create a secondary DC, used for drawing
    m_memDC.CreateCompatibleDC( GetDC() );
}

//-----------------------------------------------------------------------------
// Name: OnEraseBkgnd()
// Desc: Overridden function to prevent the view from flickering during resizes
//       and redraws
//-----------------------------------------------------------------------------
BOOL CFontMakerView::OnEraseBkgnd( CDC* pDC ) 
{
    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: RenderSelectedGlyph() 
// Desc: Highlight the selected glyph
//-----------------------------------------------------------------------------
VOID RenderSelectedGlyph( CDC* pDC )
{
    // If no glyph is selected, return
    if( FALSE == g_bIsGlyphSelected )
        return;

    HRGN rgn = CreateRectRgn( 0, 0, g_Font.m_dwTextureWidth, g_Font.m_dwTextureHeight );
    SelectClipRgn( pDC->m_hDC, rgn );

    // Draw a higlighted background for the selected glyph
    {
        int x = g_pSelectedGylph->x;
        int y = g_pSelectedGylph->y;
        int w = g_pSelectedGylph->w;
        int h = g_pSelectedGylph->h;
        pDC->FillSolidRect( x, y, w, h, COLOR_DARKRED );
    }

    // Setup the DC for drawing text
    pDC->SetTextColor( COLOR_WHITE );
    pDC->SelectObject( g_Font.m_hFont );
    pDC->SetTextAlign( TA_LEFT|TA_TOP );
    pDC->SetMapMode( MM_TEXT );
    pDC->SetBkMode( TRANSPARENT );
    pDC->SetBkColor( COLOR_BLUE );

    CPen pen( PS_SOLID, 1, COLOR_RED );
    pDC->SelectObject( &pen );

    // Render the selected glyph
    GLYPH_ATTR* pGlyph = g_pSelectedGylph;
    {
        WCHAR c = g_cSelectedGlyph;
        WCHAR str[2] = L"A";
        str[0] = c ? c : 0xffff;

        if ( g_Font.m_ValidGlyphs[c] == 2 )
        {
            // Draw a square box for a placeholder for custom glyph graphics
            pDC->FillSolidRect( pGlyph->x, pGlyph->y, pGlyph->w, pGlyph->h, COLOR_BLACK );
        }
        else
        {
            int sx = pGlyph->x;
            int sy = pGlyph->y;

            // Adjust ccordinates to account for the leading edge
            if ( str[0] > 0x1000 )
            {
            }
            else
            {
                sx -= pGlyph->a;

                if ( g_Font.m_bOutlineEffect )
                    sx -= 1;
            }

            if ( g_Font.m_bOutlineEffect )
            {
                sx++;
                sy++;

                pDC->SetTextColor( COLOR_BLACK );
                ExtTextOutW( pDC->m_hDC, sx-1, sy-1, ETO_OPAQUE, NULL, str, 1, NULL );
                ExtTextOutW( pDC->m_hDC, sx+0, sy-1, ETO_OPAQUE, NULL, str, 1, NULL );
                ExtTextOutW( pDC->m_hDC, sx+1, sy-1, ETO_OPAQUE, NULL, str, 1, NULL );
                ExtTextOutW( pDC->m_hDC, sx-1, sy+0, ETO_OPAQUE, NULL, str, 1, NULL );
                ExtTextOutW( pDC->m_hDC, sx+1, sy+0, ETO_OPAQUE, NULL, str, 1, NULL );
                ExtTextOutW( pDC->m_hDC, sx-1, sy+1, ETO_OPAQUE, NULL, str, 1, NULL );
                ExtTextOutW( pDC->m_hDC, sx+0, sy+1, ETO_OPAQUE, NULL, str, 1, NULL );
                ExtTextOutW( pDC->m_hDC, sx+1, sy+1, ETO_OPAQUE, NULL, str, 1, NULL );
            }

            if ( g_Font.m_bShadowEffect )
            {
                pDC->SetTextColor( COLOR_BLACK );
                ExtTextOutW( pDC->m_hDC, sx+2, sy+2, ETO_OPAQUE, NULL, str, 1, NULL );
            }

            // Output the letter
            pDC->SetTextColor( COLOR_WHITE );
            ExtTextOutW( pDC->m_hDC, sx, sy, ETO_OPAQUE, NULL, str, 1, NULL );
        }
    }

    // Draw a red outline around the selected glyph
    {
        // Create a red pen
        CPen pen( PS_SOLID, 1, COLOR_RED );
        pDC->SelectObject( &pen );

        // Draw the outline
        int x1 = pGlyph->x - 1;
        int y1 = pGlyph->y - 1;
        int x2 = pGlyph->x + pGlyph->w;
        int y2 = pGlyph->y + pGlyph->h;

        pDC->MoveTo( x1, y1 );
        pDC->LineTo( x2, y1 );
        pDC->LineTo( x2, y2 );
        pDC->LineTo( x1, y2 );
        pDC->LineTo( x1, y1 );
    }

    SelectClipRgn( pDC->m_hDC, NULL );
    DeleteObject( rgn );
}

//-----------------------------------------------------------------------------
// Name: OnDraw()
// Desc: Overridden draw function. Draws the font glyphs if a font is loaded,
//       else just a black background.
//-----------------------------------------------------------------------------
VOID CFontMakerView::OnNewFontGlyphs()
{
    // Select the resulting bits into our memory DC
    static CBitmap Bitmap;
    Bitmap.DeleteObject();
    Bitmap.CreateBitmap( g_Font.m_dwTextureWidth, g_Font.m_dwTextureHeight, 1, 32, g_Font.m_pBits );
    m_memDC.SelectObject( &Bitmap );

    // Trigger the view to be re-drawn
    OnUpdate(0,0,0);
}

//-----------------------------------------------------------------------------
// Name: OnDraw()
// Desc: Overridden draw function. Draws the font glyphs if a font is loaded,
//       else just a black background.
//-----------------------------------------------------------------------------
VOID CFontMakerView::OnDraw( CDC* pDC )
{
    RECT rc;
    GetClientRect( &rc );

    // Draw the view
    if ( g_Font.m_hFont == NULL && !g_Font.m_pCustomFilename )
    {
        // Don't display any scroll bars
        CSize sizeTotal( 0, 0 );
        SetScrollSizes( MM_TEXT, sizeTotal );

        // Draw a black background
        pDC->FillSolidRect( 0, 0, rc.right, rc.bottom, COLOR_BLACK );
    }
    else
    {
        // Set the scroll sizes for the view
        CSize sizeTotal( g_Font.m_dwTextureWidth, g_Font.m_dwTextureHeight );
        SetScrollSizes( MM_TEXT, sizeTotal );

        // Draw the view's black areas
        pDC->FillSolidRect( g_Font.m_dwTextureWidth, 0, max( (int)g_Font.m_dwTextureWidth, rc.right), g_Font.m_dwTextureHeight, COLOR_BLACK );
        pDC->FillSolidRect( 0, g_Font.m_dwTextureHeight, max( (int)g_Font.m_dwTextureWidth, rc.right), max( (int)g_Font.m_dwTextureHeight, rc.bottom), COLOR_BLACK );

        // Display the bitmap of all the rendered glyphs
        pDC->BitBlt( 0, 0, g_Font.m_dwTextureWidth, g_Font.m_dwTextureHeight, &m_memDC, 0, 0, SRCCOPY );
    
        // Draw the selected glyph, if any
        RenderSelectedGlyph( pDC );
    }
}

//-----------------------------------------------------------------------------
// Name: OnLButtonDown()
// Desc: Overridden function to select a glyph when the user clicks the mouse.
//-----------------------------------------------------------------------------
void CFontMakerView::OnLButtonDown( UINT nFlags, CPoint point ) 
{
	if ( !g_Font.m_hFont )
		return;

    // Correct the mouseclick point to account for the scroll position
    point += GetScrollPosition();

    // Find out which glyph, if any, is selected by the mouse click
    theApp.UpdateSelectedGlyph( FALSE );
    
    for( DWORD i=0; i<g_Font.m_dwNumGlyphs && g_Font.m_pGlyphs; i++ )
    {
        GLYPH_ATTR* pGlyph = &g_Font.m_pGlyphs[i];
    
        if ( point.x >= pGlyph->x-2 && point.x <= pGlyph->x + pGlyph->w )
        {
            if ( point.y >= pGlyph->y-2 && point.y <= pGlyph->y + pGlyph->h )
            {
                theApp.UpdateSelectedGlyph( TRUE, i );
                break;
            }
        }
    }

    // Redraw the view to show the newly selected glyph
    Invalidate( TRUE );

    // Call the base class' handler
    CScrollView::OnLButtonDown( nFlags, point );
}

//-----------------------------------------------------------------------------
// Name: OnKeyDown()
// Desc: Overridden function to select a glyph with the keyboard.
//-----------------------------------------------------------------------------
void CFontMakerView::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags ) 
{
	if ( !g_Font.m_hFont )
		return;
	
	if( g_Font.m_dwNumGlyphs > 0 )
    {
        if( FALSE == g_bIsGlyphSelected )
        {
            if( nChar == 37 || nChar == 38 || nChar == 39 || nChar == 40 ) // Left, right, up or down
            {
                // Select the first glyph
                theApp.UpdateSelectedGlyph( TRUE, 0 );
            }
        }
        else
        {
            if( nChar == 37 ) // Left
                theApp.UpdateSelectedGlyph( TRUE, max( 0, g_iSelectedGlyphNum-1 ) );
            else if( nChar == 39 ) // Right
                theApp.UpdateSelectedGlyph( TRUE, min( (int)g_Font.m_dwNumGlyphs-1, g_iSelectedGlyphNum+1 ) );
            else if( nChar == 38 || nChar == 40 ) // Up or down
            {
                // Find the closest glyph above or below to move to
                int x = g_pSelectedGylph->x + g_pSelectedGylph->w/2;
                int y = g_pSelectedGylph->y + g_pSelectedGylph->h/2;

                if( nChar == 38 ) y -= g_pSelectedGylph->h;
                if( nChar == 40 ) y += g_pSelectedGylph->h;

                for( DWORD i=0; i<g_Font.m_dwNumGlyphs; i++ )
                {
                    GLYPH_ATTR* pGlyph = &g_Font.m_pGlyphs[i];
                
                    if( x >= pGlyph->x-2 && x <= pGlyph->x + pGlyph->w )
                    {
                        if( y >= pGlyph->y-2 && y <= pGlyph->y + pGlyph->h )
                        {
                            theApp.UpdateSelectedGlyph( TRUE, i );
                            break;
                        }
                    }
                }
            }
			else if ( nChar == 46 )
			{
				// delete a glyph
				theApp.OnGlyphsCustom();

				// mark glyph as invalid
				g_Font.DeleteGlyph( g_cSelectedGlyph );

				// reset to the first glyph
				theApp.UpdateSelectedGlyph( FALSE );
			}
			else if ( nChar == 45 )
			{
				// insert a glyph
				theApp.OnGlyphsCustom();

				// reset to the first glyph
				theApp.UpdateSelectedGlyph( FALSE );
			
				theApp.InsertGlyph();
			}
        }

        // Redraw the view to show the newly selected glyph
        Invalidate( TRUE );
    }

    CScrollView::OnKeyDown( nChar, nRepCnt, nFlags );
}



