//-----------------------------------------------------------------------------
// Name: FontMaker.h
//
// Desc: Defines the class behaviors for the application.
//
// Hist: 09.06.02 - Revised Fontmaker sample
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef FONTMAKER_H
#define FONTMAKER_H

#include "resource.h"
#include "BitmapFontFile.h"
#include <math.h>
#include "..\toollib\toollib.h"
#include "..\toollib\scriplib.h"
#include "..\toollib\piclib.h"

//-----------------------------------------------------------------------------
// Name: class CFontMakerView
// Desc: The scroll view class for viewing the font texture image
//-----------------------------------------------------------------------------
class CFontMakerView : public CScrollView
{
protected:
    CFontMakerView() {}
    DECLARE_DYNCREATE(CFontMakerView)

    CDC     m_memDC;

public:
    VOID OnNewFontGlyphs();

    virtual ~CFontMakerView();

public:

    // Overridden functions
    //{{AFX_VIRTUAL(CFontMakerView)
    public:
    virtual void OnDraw(CDC* pDC);
    virtual void OnInitialUpdate();
    protected:
    //}}AFX_VIRTUAL

protected:
    // Message map functions
    //{{AFX_MSG(CFontMakerView)
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};




//-----------------------------------------------------------------------------
// Name: class CFontMakerFrameWnd
// Desc: The main frame window class for the app, which contains the dialog bar
//       full of controls and the scroll view to view the font texture image.
//-----------------------------------------------------------------------------
class CFontMakerFrameWnd : public CFrameWnd
{
public:
    CFontMakerFrameWnd() {}
    virtual ~CFontMakerFrameWnd() {}

    CDialogBar  m_wndDialogBar;
    CDialogBar* GetDialogBar() { return &m_wndDialogBar; }

protected:
    DECLARE_DYNCREATE(CFontMakerFrameWnd)

    // Message map functions
    //{{AFX_MSG(CFontMakerFrameWnd)
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};




//-----------------------------------------------------------------------------
// Name: class CFontMakerApp
// Desc: The main app class
//-----------------------------------------------------------------------------
class CFontMakerApp : public CWinApp
{
    CDialogBar*     m_pDialogBar;
    CFontMakerView* m_pView;
    HCURSOR         m_hWaitCursor;

public:
    CFontMakerApp() {}
    ~CFontMakerApp() {}

    VOID    UpdateSelectedGlyph( BOOL bGlyphSelected, int iSelectedGlyph = 0 );
    HRESULT CalculateAndRenderGlyphs();
	VOID	InsertGlyph();

	void	SetTextureSize( int width, int height );

    // Overrides
    //{{AFX_VIRTUAL(CFontMakerApp)
    public:
    virtual BOOL InitInstance();
    virtual int ExitInstance();
    //}}AFX_VIRTUAL

    // Implementation
    //{{AFX_MSG(CFontMakerApp)
    afx_msg void OnNewFontButton();
    afx_msg void OnEffectsCheck();
    afx_msg void OnGlyphsFromRangeRadio();
    afx_msg void OnChangeGlpyhsRangeEdit();
    afx_msg void OnGlyphsFromFileRadio();
    afx_msg void OnChangeGlyphsFileEdit();
    afx_msg void OnGlyphsFileSelectorButton();
    afx_msg void OnTextureSizeButton();
    afx_msg void OnMagnifyButton();
    afx_msg void OnGlyphSpecial();
    afx_msg void OnUpdateButton( CCmdUI* pCmdUI );
    afx_msg void OnSaveButton();
    afx_msg void OnExit();
    afx_msg void OnAbout();
    afx_msg void OnHelp();
	afx_msg void OnGlyphsCustom();
	afx_msg void OnLoadButton();
 	afx_msg void OnLoadCustomFontButton();
   //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};



// External reference to the unique application instance
extern CFontMakerApp theApp;



#endif // FONTMAKER_H
