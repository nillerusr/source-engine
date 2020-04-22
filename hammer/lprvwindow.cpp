//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include "hammer.h"
#include "lprvwindow.h"
#include "TextureBrowser.h"
#include "CustomMessages.h"
#include "IEditorTexture.h"
#include "GameConfig.h"
#include "GlobalFunctions.h"
#include "TextureSystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "lpreview_thread.h"
#include "MainFrm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>



BEGIN_MESSAGE_MAP(CLightingPreviewResultsWindow, CWnd)
	//{{AFX_MSG_MAP(CTextureWindow)
	ON_WM_PAINT()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CLightingPreviewResultsWindow::CLightingPreviewResultsWindow(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CLightingPreviewResultsWindow::~CLightingPreviewResultsWindow(void)
{
} 


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParentWnd - 
//			rect - 
//-----------------------------------------------------------------------------
void CLightingPreviewResultsWindow::Create(CWnd *pParentWnd )
{
	static CString LPreviewWndClassName;

	if(LPreviewWndClassName.IsEmpty())
	{
		// create class
		LPreviewWndClassName = AfxRegisterWndClass(
			CS_DBLCLKS | CS_HREDRAW | 
			CS_VREDRAW, LoadCursor(NULL, IDC_ARROW), 
			(HBRUSH) GetStockObject(BLACK_BRUSH), NULL);
	}

	RECT rect;
	rect.left = 500; rect.right = 600;
	rect.top = 500; rect.bottom = 600;

	CWnd::CreateEx(0,LPreviewWndClassName, "LightingPreviewWindow",
				   WS_OVERLAPPEDWINDOW|WS_SIZEBOX,
				   rect, NULL, NULL,NULL);

}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightingPreviewResultsWindow::OnClose()
{
	GetMainWnd()->GlobalNotify(LPRV_WINDOWCLOSED);
	CWnd::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLightingPreviewResultsWindow::OnPaint(void)
{
	CPaintDC dc(this); // device context for painting

	CRect clientrect;
	GetClientRect(clientrect);
	if ( g_pLPreviewOutputBitmap)
	{
		// blit it
		BITMAPINFOHEADER mybmh;
		mybmh.biHeight=-g_pLPreviewOutputBitmap->Height();
		mybmh.biSize=sizeof(BITMAPINFOHEADER);
		// now, set up bitmapheader struct for StretchDIB
		mybmh.biWidth=g_pLPreviewOutputBitmap->Width();
		mybmh.biPlanes=1;
		mybmh.biBitCount=32;
		mybmh.biCompression=BI_RGB;
		mybmh.biSizeImage=g_pLPreviewOutputBitmap->Width()*g_pLPreviewOutputBitmap->Height();

  
		StretchDIBits(
			dc.GetSafeHdc(),clientrect.left,clientrect.top,1+(clientrect.right-clientrect.left),
			1+(clientrect.bottom-clientrect.top),
			0,0,g_pLPreviewOutputBitmap->Width(), g_pLPreviewOutputBitmap->Height(),
			g_pLPreviewOutputBitmap->GetBits(), (BITMAPINFO *) &mybmh,
			DIB_RGB_COLORS, SRCCOPY);
	}
}


