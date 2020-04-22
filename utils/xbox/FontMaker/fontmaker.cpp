//-----------------------------------------------------------------------------
// Name: FontMaker.cpp
//
// Desc: Defines the class behaviors for the application.
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


CTextureFont       g_Font;
extern BOOL        g_bIsGlyphSelected;
extern int         g_iSelectedGlyphNum;
extern GLYPH_ATTR* g_pSelectedGylph;
extern WCHAR       g_cSelectedGlyph;





//-----------------------------------------------------------------------------
// CFontMakerApp
//-----------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CFontMakerApp, CWinApp)
    //{{AFX_MSG_MAP(CFontMakerApp)
    ON_COMMAND(IDM_FILE_NEWFONT, OnNewFontButton)
    ON_BN_CLICKED(IDC_EFFECTSOUTLINED_CHECK, OnEffectsCheck)
    ON_BN_CLICKED(IDC_EFFECTSSHADOWED_CHECK, OnEffectsCheck)
    ON_BN_CLICKED(IDC_EFFECTSBLURRED_CHECK, OnEffectsCheck)
    ON_BN_CLICKED(IDC_EFFECTSSCANLINES_CHECK, OnEffectsCheck)
	ON_BN_CLICKED(IDC_EFFECTSANTIALIAS_CHECK, OnEffectsCheck)
	ON_BN_CLICKED(IDC_GLYPHSFROMRANGE_RADIO, OnGlyphsFromRangeRadio)
    ON_EN_CHANGE(IDC_GLYPHSRANGEFROM_EDIT, OnChangeGlpyhsRangeEdit)
    ON_BN_CLICKED(IDC_GLYPHSFROMFILE_RADIO, OnGlyphsFromFileRadio)
    ON_EN_KILLFOCUS(IDC_GLYPHSFILE_EDIT, OnChangeGlyphsFileEdit)
    ON_BN_CLICKED(IDC_GLYPHSFILESELECTOR_BUTTON, OnGlyphsFileSelectorButton)
    ON_BN_CLICKED(IDC_GLYPHSCUSTOM_RADIO, OnGlyphsCustom)	
	ON_BN_CLICKED(IDC_TEXTURESIZE_BUTTON, OnTextureSizeButton)
    ON_BN_CLICKED(IDC_MAGNIFY_BUTTON, OnMagnifyButton)
    ON_BN_CLICKED(IDC_GLYPH_SPECIAL, OnGlyphSpecial)
    ON_UPDATE_COMMAND_UI(IDC_MAGNIFY_BUTTON, OnUpdateButton)
	ON_COMMAND(IDM_FILE_LOADFONTFILE, OnLoadButton)
	ON_COMMAND(IDM_FILE_SAVEFONTFILES, OnSaveButton)
	ON_COMMAND(IDM_FILE_LOADFONTLAYOUT, OnLoadCustomFontButton)
    ON_COMMAND(IDM_FILE_EXIT, OnExit)
    ON_COMMAND(ID_APP_ABOUT, OnAbout)
    ON_COMMAND(ID_HELP, OnHelp)
    ON_EN_CHANGE(IDC_GLYPHSRANGETO_EDIT, OnChangeGlpyhsRangeEdit)
    ON_UPDATE_COMMAND_UI(IDC_TEXTURESIZE_BUTTON, OnUpdateButton)
    ON_UPDATE_COMMAND_UI(IDC_GLYPHSFILESELECTOR_BUTTON, OnUpdateButton)
    ON_UPDATE_COMMAND_UI(IDM_FILE_NEWFONT, OnUpdateButton)
    ON_UPDATE_COMMAND_UI(IDM_FILE_LOADFONTFILE, OnUpdateButton)
    ON_UPDATE_COMMAND_UI(IDM_FILE_LOADFONTLAYOUT, OnUpdateButton)
    ON_UPDATE_COMMAND_UI(IDM_FILE_SAVEFONTFILES, OnUpdateButton)
    ON_UPDATE_COMMAND_UI(IDM_FILE_EXIT, OnUpdateButton)
    ON_UPDATE_COMMAND_UI(ID_APP_ABOUT, OnUpdateButton)
    ON_UPDATE_COMMAND_UI(ID_HELP, OnUpdateButton)
   //}}AFX_MSG_MAP
END_MESSAGE_MAP()




//-----------------------------------------------------------------------------
// The one and only CFontMakerApp object
//-----------------------------------------------------------------------------
CFontMakerApp theApp;




//-----------------------------------------------------------------------------
// Name: InitInstance()
// Desc: App initialization
//-----------------------------------------------------------------------------
BOOL CFontMakerApp::InitInstance()
{
    // Create the main frame window for the app
    CFontMakerFrameWnd* pFrameWnd = new CFontMakerFrameWnd;
    m_pMainWnd = pFrameWnd;

    // Associate the view with the frame
    CCreateContext context;
    context.m_pCurrentFrame   = NULL;
    context.m_pCurrentDoc     = NULL;
    context.m_pNewViewClass   = RUNTIME_CLASS(CFontMakerView);
    context.m_pNewDocTemplate = NULL;

    // Create the frame and load resources (menu, accelerator, etc.)
    pFrameWnd->LoadFrame( IDR_MAINFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE,
                          NULL, &context );

    // Call OnInitialUpdate() to be called for the view
    pFrameWnd->InitialUpdateFrame( NULL, TRUE );

    // The one and only window has been initialized, so show and update it.
    m_pMainWnd->ShowWindow( SW_SHOW );
    m_pMainWnd->UpdateWindow();

    // Load the hourglass cursor
    m_hWaitCursor = LoadCursor( IDC_WAIT );

    // Get access the the dialog controls and the view
    m_pDialogBar = pFrameWnd->GetDialogBar();
    m_pView      = (CFontMakerView*)pFrameWnd->GetActiveView();

    // Initially, no font is selected
    m_pDialogBar->GetDlgItem( IDC_FONTNAME_STATIC )->SetWindowText( _T("<Choose font>") );
    m_pDialogBar->GetDlgItem( IDC_FONTSTYLE_STATIC )->SetWindowText( _T("") );
    m_pDialogBar->GetDlgItem( IDC_FONTSIZE_STATIC )->SetWindowText( _T("") );

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: OnUpdateButton()
// Desc: This function is needed to override some internal mucking with button
//       states. Without it, button and menu enabling will make you crazy.
//-----------------------------------------------------------------------------
void CFontMakerApp::OnUpdateButton( CCmdUI* pCmdUI )
{
	BOOL bEnable;

    switch( pCmdUI->m_nID )
    {
        // Controls which are active all the time
        case IDM_FILE_NEWFONT:
		case IDM_FILE_LOADFONTLAYOUT:
		case IDM_FILE_LOADFONTFILE:
        case IDM_FILE_EXIT:
        case ID_APP_ABOUT:
        case ID_HELP:
            bEnable = TRUE;
            break; 

		case IDC_TEXTURESIZE_BUTTON:
		case IDM_FILE_SAVEFONTFILES:
		case IDC_MAGNIFY_BUTTON:
			bEnable =  g_Font.m_hFont ? TRUE : FALSE;
			if ( !bEnable )
				bEnable = g_Font.m_pCustomFilename ? TRUE : FALSE;
			break;

        // Controls which are active only when a font is available
        default:
            bEnable = g_Font.m_hFont ? TRUE : FALSE;
            break;
    }

	pCmdUI->Enable( bEnable );
}

BOOL g_bFirstTime = TRUE;

//-----------------------------------------------------------------------------
// Name: OnNewFontButton()
// Desc: Called when the user hits the "New Font" button, this loads the font
//       and enables all the other windows controls.
//-----------------------------------------------------------------------------
void CFontMakerApp::OnNewFontButton() 
{
    // Initialize the LOGFONT structure. It's static so it's state is remembered
    if ( g_Font.m_LogFont.lfHeight == 0 )
	{
		// first time init
		strcpy( g_Font.m_LogFont.lfFaceName, "Arial" );	// Arial font for a default
		g_Font.m_LogFont.lfHeight  = 16;				// 16 height font for a default
		g_Font.m_LogFont.lfWeight  = 400;				// 400 = normal, 700 = bold, etc.
		g_Font.m_LogFont.lfItalic  = 0;					//   0 = normal, 255 = italic
		g_Font.m_LogFont.lfQuality = ANTIALIASED_QUALITY;
	}

	// convert to point size for dialog purposes
	HDC hDC = GetDC( m_pMainWnd->m_hWnd );
	// Current point size unit=1/10 pts
	INT iPointSize = g_Font.m_LogFont.lfHeight * 10;
	g_Font.m_LogFont.lfHeight= -MulDiv( iPointSize, GetDeviceCaps( hDC, LOGPIXELSY ), 720 );
	ReleaseDC( m_pMainWnd->m_hWnd, hDC );

    // Create the CHOOSEFONT structure
    static CHOOSEFONT cf = {0};
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.lpLogFont   = &g_Font.m_LogFont;
    cf.Flags       = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
    cf.nFontType   = SCREEN_FONTTYPE;

    if ( 0 == ChooseFont( &cf ) )
        return;

	g_Font.m_pCustomFilename = NULL;

	// NOT using point sizes, but cell heights
	g_Font.m_LogFont.lfHeight = cf.iPointSize/10;

    // Reset the selected glpyh
    UpdateSelectedGlyph( FALSE );

    if( FAILED( CalculateAndRenderGlyphs() ) )
    {
        // Could not create new font
        MessageBox( m_pMainWnd->m_hWnd, "Could not create the requested font!", "Error", MB_ICONERROR|MB_OK );
        return;
    }

	char tempName[256];
    sprintf( tempName, "%s_%d", g_Font.m_LogFont.lfFaceName, cf.iPointSize/10 );

	// remove any spaces in the font name
	for (unsigned int i=0,j=0; i<strlen( tempName )+1; i++)
	{
		if ( tempName[i] != ' ' )
		{
			g_Font.m_strFontName[j++] = tempName[i];
		}
	}

    if ( g_bFirstTime )
    {
        CString str;

        // Set font properties
        m_pDialogBar->GetDlgItem( IDC_FONT_GROUPBOX )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_FONTNAME_LABEL )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_FONTSTYLE_LABEL )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_FONTSIZE_LABEL )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_FONTNAME_STATIC )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_FONTSTYLE_STATIC )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_FONTSIZE_STATIC )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_EFFECTSOUTLINED_CHECK )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_EFFECTSSHADOWED_CHECK )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_EFFECTSBLURRED_CHECK )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_EFFECTSSCANLINES_CHECK )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_EFFECTSANTIALIAS_CHECK )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_BLUR_EDIT )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_SCANLINES_EDIT )->EnableWindow( TRUE );
		
		if ( g_Font.m_bAntialiasEffect )
		{
			((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSANTIALIAS_CHECK ))->SetCheck( TRUE );
		}
		else
		{
			((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSANTIALIAS_CHECK ))->SetCheck( FALSE );
		}

		str.Format( "%d", g_Font.m_nBlur );
		m_pDialogBar->GetDlgItem( IDC_BLUR_EDIT )->SetWindowText( str );

		str.Format( "%d", g_Font.m_nScanlines );
		m_pDialogBar->GetDlgItem( IDC_SCANLINES_EDIT )->SetWindowText( str );	

        str.Format( "%s", g_Font.m_LogFont.lfFaceName );
        m_pDialogBar->GetDlgItem( IDC_FONTNAME_STATIC )->SetWindowText( str );
        if( g_Font.m_LogFont.lfItalic )
            str.Format( "Italic", g_Font.m_LogFont.lfWeight < 550 ? "" : "Bold " );
        else
            str.Format( "%s", g_Font.m_LogFont.lfWeight < 550 ? "Regular" : "Bold" );
        m_pDialogBar->GetDlgItem( IDC_FONTSTYLE_STATIC )->SetWindowText( str );
        str.Format( "%ld", cf.iPointSize/10 );
        m_pDialogBar->GetDlgItem( IDC_FONTSIZE_STATIC )->SetWindowText( str );

        // Set texture properties
        m_pDialogBar->GetDlgItem( IDC_TEXTURE_GROUPBOX )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_TEXTUREWIDTH_LABEL )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_TEXTUREHEIGHT_LABEL )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_TEXTUREWIDTH_STATIC )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_TEXTUREHEIGHT_STATIC )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_TEXTURESIZE_BUTTON )->EnableWindow( TRUE );

		SetTextureSize( g_Font.m_dwTextureWidth, g_Font.m_dwTextureHeight );

        // Set glyph range properties
        m_pDialogBar->GetDlgItem( IDC_GLYPHS_GROUPBOX )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMRANGE_RADIO )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMFILE_RADIO )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_EDIT )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_EDIT )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_LABEL )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_LABEL )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSFILE_EDIT )->EnableWindow( FALSE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSFILESELECTOR_BUTTON )->EnableWindow( FALSE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSCUSTOM_RADIO )->EnableWindow( TRUE );

        // Set a default range of glyphs to use
        ((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMRANGE_RADIO ))->SetCheck( TRUE );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_EDIT )->SetWindowText( "32" );
        m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_EDIT )->SetWindowText( "127" );
        g_Font.ExtractValidGlyphsFromRange( 32, 127 );

        m_pDialogBar->GetDlgItem( IDC_INSERTGLYPH_LABEL )->EnableWindow( TRUE );
        m_pDialogBar->GetDlgItem( IDC_INSERTGLYPH_EDIT )->EnableWindow( TRUE );
    }
    else
    {
        CString str;

        str.Format( "%s", g_Font.m_LogFont.lfFaceName );
        m_pDialogBar->GetDlgItem( IDC_FONTNAME_STATIC )->SetWindowText( str );
        if ( g_Font.m_LogFont.lfItalic )
            str.Format( "Italic", g_Font.m_LogFont.lfWeight < 550 ? "" : "Bold " );
        else
            str.Format( "%s", g_Font.m_LogFont.lfWeight < 550 ? "Regular" : "Bold" );
        m_pDialogBar->GetDlgItem( IDC_FONTSTYLE_STATIC )->SetWindowText( str );
        str.Format( "%ld", cf.iPointSize/10 );
        m_pDialogBar->GetDlgItem( IDC_FONTSIZE_STATIC )->SetWindowText( str );
    }

	g_bFirstTime = FALSE;
}




//-----------------------------------------------------------------------------
// Name: OnGlyphsFromRangeRadio()
// Desc: User will be specifying a glyph range manually
//-----------------------------------------------------------------------------
void CFontMakerApp::OnGlyphsFromRangeRadio() 
{
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_LABEL )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_LABEL )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_EDIT )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_EDIT )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSFILE_EDIT )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSFILESELECTOR_BUTTON )->EnableWindow( FALSE );

	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMFILE_RADIO ))->SetCheck( false );
	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMRANGE_RADIO ))->SetCheck( true );
	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSCUSTOM_RADIO ))->SetCheck( false );

    OnChangeGlpyhsRangeEdit();
}




//-----------------------------------------------------------------------------
// Name: OnChangeGlpyhsRangeEdit()
// Desc: User changed the range of glpyhs
//-----------------------------------------------------------------------------
void CFontMakerApp::OnChangeGlpyhsRangeEdit() 
{
    if( NULL == g_Font.m_hFont )
        return;

    CEdit* pGlyphRangeFromEdit = (CEdit*)m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_EDIT );
    CEdit* pGlyphRangeToEdit   = (CEdit*)m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_EDIT );

    CString strFrom;
    CString strTo;
    pGlyphRangeFromEdit->GetWindowText( strFrom );
    pGlyphRangeToEdit->GetWindowText( strTo );

    WORD wFrom = (WORD)max( 0, atoi( strFrom ) );
    WORD wTo   = (WORD)min( 65535, atoi( strTo ) );
    g_Font.ExtractValidGlyphsFromRange( wFrom, wTo );

    // Draw the new font glyphs
    CalculateAndRenderGlyphs();
}


void CFontMakerApp::OnGlyphsCustom() 
{
	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMFILE_RADIO ))->SetCheck( false );
	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMRANGE_RADIO ))->SetCheck( false );
	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSCUSTOM_RADIO ))->SetCheck( true );
}


//-----------------------------------------------------------------------------
// Name: OnGlyphsFromFileRadio()
// Desc: User want to extract glyphs that are used in a text file
//-----------------------------------------------------------------------------
void CFontMakerApp::OnGlyphsFromFileRadio() 
{
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_LABEL )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_LABEL )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_EDIT )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_EDIT )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSFILE_EDIT )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSFILESELECTOR_BUTTON )->EnableWindow( TRUE );

	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMFILE_RADIO ))->SetCheck( true );
	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMRANGE_RADIO ))->SetCheck( false );
	((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSCUSTOM_RADIO ))->SetCheck( false );

    OnChangeGlyphsFileEdit();
}




//-----------------------------------------------------------------------------
// Name: OnChangeGlyphsFileEdit()
// Desc: Handle change in name of file to extract glyphs from
//-----------------------------------------------------------------------------
void CFontMakerApp::OnChangeGlyphsFileEdit() 
{
    CEdit* pGlyphFileNameEdit = (CEdit*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFILE_EDIT );

    CString strFileName;
    pGlyphFileNameEdit->GetWindowText( strFileName );

    if( strFileName.IsEmpty() )
        return;

    g_Font.ExtractValidGlyphsFromFile( (const TCHAR*)strFileName );

    // Draw the new font glyphs
    CalculateAndRenderGlyphs();
}




//-----------------------------------------------------------------------------
// Name: OnGlyphsFileSelectorButton()
// Desc: Handle change in name of file to extract glyphs from
//-----------------------------------------------------------------------------
void CFontMakerApp::OnGlyphsFileSelectorButton() 
{
    static TCHAR strFileName[MAX_PATH]   = _T("");
    static TCHAR strFileName2[MAX_PATH]  = _T("");
    static TCHAR strInitialDir[MAX_PATH] = _T("c:\\");

    // Display the OpenFileName dialog. Then, try to load the specified file
    OPENFILENAME ofn = { sizeof(OPENFILENAME), NULL, NULL,
                         _T("Text files (.txt)\0*.txt\0\0"), 
                         NULL, 0, 1, strFileName, MAX_PATH, strFileName2, MAX_PATH, 
                         strInitialDir, _T("Open Text File"), 
                         OFN_FILEMUSTEXIST, 0, 1, NULL, 0, NULL, NULL };

    if( TRUE == GetOpenFileName( &ofn ) )
    {
        m_pDialogBar->GetDlgItem( IDC_GLYPHSFILE_EDIT )->SetWindowText( ofn.lpstrFile);
        OnChangeGlyphsFileEdit();
    }
}




//-----------------------------------------------------------------------------
// Name: OnEffectsCheck()
// Desc: User changed font rendering options
//-----------------------------------------------------------------------------
void CFontMakerApp::OnEffectsCheck() 
{
    g_Font.m_bOutlineEffect   = ((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSOUTLINED_CHECK ))->GetCheck();
    g_Font.m_bShadowEffect    = ((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSSHADOWED_CHECK ))->GetCheck();
	g_Font.m_bAntialiasEffect = ((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSANTIALIAS_CHECK ))->GetCheck();

	bool bValveEffects = false;
    if ( g_Font.m_bOutlineEffect || g_Font.m_bShadowEffect )
	{
		m_pDialogBar->GetDlgItem( IDC_EFFECTSBLURRED_CHECK )->EnableWindow( false );
		m_pDialogBar->GetDlgItem( IDC_EFFECTSSCANLINES_CHECK )->EnableWindow( false );
		((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSBLURRED_CHECK ))->SetCheck( false );
		((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSSCANLINES_CHECK ))->SetCheck( false );
	}
	else
	{
		m_pDialogBar->GetDlgItem( IDC_EFFECTSBLURRED_CHECK )->EnableWindow( true );
		m_pDialogBar->GetDlgItem( IDC_EFFECTSSCANLINES_CHECK )->EnableWindow( true );
		bValveEffects = true;
	}

	if ( bValveEffects && ((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSBLURRED_CHECK ))->GetCheck() )
	{
	    CEdit* pBlurEdit = (CEdit*)m_pDialogBar->GetDlgItem( IDC_BLUR_EDIT );

		CString strBlur;
	    pBlurEdit->GetWindowText( strBlur );

		g_Font.m_nBlur = max( 2, atoi( strBlur ) );

		strBlur.Format( "%d", g_Font.m_nBlur );
		pBlurEdit->SetWindowText( strBlur );
	}
	else
	{
		g_Font.m_nBlur = 0;
	}

    if ( bValveEffects && ((CButton*)m_pDialogBar->GetDlgItem( IDC_EFFECTSSCANLINES_CHECK ))->GetCheck() )
	{
		CEdit* pScanlineEdit = (CEdit*)m_pDialogBar->GetDlgItem( IDC_SCANLINES_EDIT );

		CString strScanlines;
	    pScanlineEdit->GetWindowText( strScanlines );

		g_Font.m_nScanlines = max( 2, atoi( strScanlines ) );

		strScanlines.Format( "%d", g_Font.m_nScanlines );
		pScanlineEdit->SetWindowText( strScanlines );
	}
	else
	{
		g_Font.m_nScanlines = 0;
	}

    // Draw the new font glyphs
    CalculateAndRenderGlyphs();
}




//-----------------------------------------------------------------------------
// Name: OnMagnifyButton() 
// Desc: User wants to run the Windows "magnify" tool
//-----------------------------------------------------------------------------
void CFontMakerApp::OnMagnifyButton() 
{
    // Run the Windows "magnify" tool
    WinExec( "magnify.exe", TRUE );
}




//-----------------------------------------------------------------------------
// Name: class CTextureSizeDlg
// Desc: Simple dialog to change the font texture size
//-----------------------------------------------------------------------------
class CTextureSizeDlg : public CDialog
{
public:
    CTextureSizeDlg();

// Dialog Data
    //{{AFX_DATA(CTextureSizeDlg)
    enum { IDD = IDD_TEXTURESIZE };
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CTextureSizeDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
protected:
    //{{AFX_MSG(CTextureSizeDlg)
        // No message handlers
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

CTextureSizeDlg::CTextureSizeDlg() : CDialog(CTextureSizeDlg::IDD)
{
    //{{AFX_DATA_INIT(CTextureSizeDlg)
    //}}AFX_DATA_INIT
}

void CTextureSizeDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    //{{AFX_DATA_MAP(CTextureSizeDlg)
    DDX_Text( pDX, IDC_WIDTH, g_Font.m_dwTextureWidth );
    DDV_MinMaxInt( pDX, g_Font.m_dwTextureWidth, 16, 2048 ); 

    DDX_Text( pDX, IDC_HEIGHT, g_Font.m_dwTextureHeight );
    DDV_MinMaxInt( pDX, g_Font.m_dwTextureHeight, 16, 2048 ); 
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTextureSizeDlg, CDialog)
    //{{AFX_MSG_MAP(CTextureSizeDlg)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CFontMakerApp::SetTextureSize( int width, int height )
{
	g_Font.m_dwTextureWidth = width;
	g_Font.m_dwTextureHeight = height;

	CString str;
    str.Format( "%ld", g_Font.m_dwTextureWidth );
    m_pDialogBar->GetDlgItem( IDC_TEXTUREWIDTH_STATIC )->SetWindowText( str );
    str.Format( "%ld", g_Font.m_dwTextureHeight );
    m_pDialogBar->GetDlgItem( IDC_TEXTUREHEIGHT_STATIC )->SetWindowText( str );
}

//-----------------------------------------------------------------------------
// Name: OnTextureSizeButton() 
// Desc: User wants to change the font texture size
//-----------------------------------------------------------------------------
void CFontMakerApp::OnTextureSizeButton() 
{
    if ( !g_Font.m_hFont && !g_Font.m_pCustomFilename )
        return;

    CTextureSizeDlg dlgTextureSize;
    dlgTextureSize.DoModal();

	SetTextureSize( g_Font.m_dwTextureWidth, g_Font.m_dwTextureHeight );

    // Draw the new font glyphs
    CalculateAndRenderGlyphs();
}

void CFontMakerApp::InsertGlyph()
{
	CEdit* pGlyphInsert = (CEdit*)m_pDialogBar->GetDlgItem( IDC_INSERTGLYPH_EDIT );

    CString strInsert;
    pGlyphInsert->GetWindowText( strInsert );

	WORD wGlyph = atoi( strInsert );
	if ( wGlyph < 0 )
		wGlyph = 0;
	else if ( wGlyph > 65535 )
		wGlyph = 65535;
	
    g_Font.InsertGlyph( wGlyph );
}

//-----------------------------------------------------------------------------
// Name: UpdateSelectedGlyph()
// Desc: User changed (via mouse or keyboard) which glyph is selected
//-----------------------------------------------------------------------------
void CFontMakerApp::UpdateSelectedGlyph( BOOL bGlyphSelected, int iSelectedGlyph )
{
    // Handle case where no glyph is selected
    g_bIsGlyphSelected  = FALSE;
    g_iSelectedGlyphNum = 0;
    g_pSelectedGylph    = NULL;
    g_cSelectedGlyph    = L'\0';

    if ( bGlyphSelected )
    {
        for ( DWORD i=0; i<=g_Font.m_cMaxGlyph; i++ )
        {
            if ( g_Font.m_TranslatorTable[i] == iSelectedGlyph )
            {
                g_bIsGlyphSelected  = TRUE;
                g_iSelectedGlyphNum = iSelectedGlyph;
                g_pSelectedGylph    = &g_Font.m_pGlyphs[iSelectedGlyph];
                g_cSelectedGlyph    = (WCHAR)i;
                break;
            }
        }
    }

    // Enable/disable/set-text-of the appropriate controls
    if ( g_bIsGlyphSelected )
    {
        CString str;
        str.Format( "%d", g_cSelectedGlyph ); m_pDialogBar->GetDlgItem( IDC_GLYPH_VALUE_STATIC )->SetWindowText( str );
        str.Format( "%d", g_pSelectedGylph->x ); m_pDialogBar->GetDlgItem( IDC_GLYPH_X_STATIC )->SetWindowText( str );
        str.Format( "%d", g_pSelectedGylph->y ); m_pDialogBar->GetDlgItem( IDC_GLYPH_Y_STATIC )->SetWindowText( str );
        str.Format( "%d", g_pSelectedGylph->w ); m_pDialogBar->GetDlgItem( IDC_GLYPH_W_STATIC )->SetWindowText( str );
        str.Format( "%d", g_pSelectedGylph->h ); m_pDialogBar->GetDlgItem( IDC_GLYPH_H_STATIC )->SetWindowText( str );
        str.Format( "%d", g_pSelectedGylph->a ); m_pDialogBar->GetDlgItem( IDC_GLYPH_A_STATIC )->SetWindowText( str );
        str.Format( "%d", g_pSelectedGylph->b ); m_pDialogBar->GetDlgItem( IDC_GLYPH_B_STATIC )->SetWindowText( str );
        str.Format( "%d", g_pSelectedGylph->c ); m_pDialogBar->GetDlgItem( IDC_GLYPH_C_STATIC )->SetWindowText( str );
        ((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPH_SPECIAL ))->SetCheck( g_Font.m_ValidGlyphs[g_cSelectedGlyph] == 2 );
    }
    else
    {
        CString str("");
        m_pDialogBar->GetDlgItem( IDC_GLYPH_VALUE_STATIC )->SetWindowText( str );
        m_pDialogBar->GetDlgItem( IDC_GLYPH_X_STATIC )->SetWindowText( str );
        m_pDialogBar->GetDlgItem( IDC_GLYPH_Y_STATIC )->SetWindowText( str );
        m_pDialogBar->GetDlgItem( IDC_GLYPH_W_STATIC )->SetWindowText( str );
        m_pDialogBar->GetDlgItem( IDC_GLYPH_H_STATIC )->SetWindowText( str );
        m_pDialogBar->GetDlgItem( IDC_GLYPH_A_STATIC )->SetWindowText( str );
        m_pDialogBar->GetDlgItem( IDC_GLYPH_B_STATIC )->SetWindowText( str );
        m_pDialogBar->GetDlgItem( IDC_GLYPH_C_STATIC )->SetWindowText( str );
        ((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPH_SPECIAL ))->SetCheck( FALSE );
    }

    m_pDialogBar->GetDlgItem( IDC_SELECTEDGLYPH_GROUPBOX )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_VALUE_LABEL )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_X_LABEL )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_Y_LABEL )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_W_LABEL )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_H_LABEL )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_A_LABEL )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_B_LABEL )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_C_LABEL )->EnableWindow( g_bIsGlyphSelected );

    m_pDialogBar->GetDlgItem( IDC_GLYPH_VALUE_STATIC )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_X_STATIC )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_Y_STATIC )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_W_STATIC )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_H_STATIC )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_A_STATIC )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_B_STATIC )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_C_STATIC )->EnableWindow( g_bIsGlyphSelected );
    m_pDialogBar->GetDlgItem( IDC_GLYPH_SPECIAL )->EnableWindow( g_bIsGlyphSelected );
}




//-----------------------------------------------------------------------------
// Name: OnGlyphSpecial() 
// Desc: User changed the status of the selected glyph
//-----------------------------------------------------------------------------
void CFontMakerApp::OnGlyphSpecial() 
{
    if( g_bIsGlyphSelected )
    {
        if( ((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPH_SPECIAL ))->GetCheck() )
            g_Font.m_ValidGlyphs[g_cSelectedGlyph] = 2;
        else
            g_Font.m_ValidGlyphs[g_cSelectedGlyph] = 1;

        // Draw the font glyphs, which may have changed layout
        CalculateAndRenderGlyphs();
    }
}

//-----------------------------------------------------------------------------
// Name: OnLoadButton() 
// Desc: User wants to load a font file
//-----------------------------------------------------------------------------
void CFontMakerApp::OnLoadButton() 
{
    CHAR strVBFFileName[MAX_PATH];
    sprintf( strVBFFileName, "%s.vbf", g_Font.m_strFontName );

    OPENFILENAME ofnVBF;       // common dialog box structure
    ZeroMemory( &ofnVBF, sizeof(OPENFILENAME) );
    ofnVBF.lStructSize     = sizeof(OPENFILENAME);
    ofnVBF.hwndOwner       = m_pMainWnd->m_hWnd;
    ofnVBF.lpstrFilter     = "Font files (*.vbf)\0*.vbf\0\0";
    ofnVBF.nFilterIndex    = 1;
    ofnVBF.lpstrFile       = strVBFFileName;
    ofnVBF.nMaxFile        = sizeof(strVBFFileName);
    ofnVBF.lpstrFileTitle  = NULL;
    ofnVBF.nMaxFileTitle   = 0;
    ofnVBF.lpstrInitialDir = NULL;
    ofnVBF.lpstrTitle      = "Load Font (VBF) File...";
    ofnVBF.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_READONLY;

    // Display the Load dialog box for the VBF file
    if ( FALSE == GetOpenFileName( &ofnVBF ) ) 
        return;

    if ( FAILED( g_Font.ReadFontInfoFile( strVBFFileName ) ) )
    {
        m_pMainWnd->MessageBox( "Could not load the Valve bitmap font info file.", 
                                "Error", MB_ICONERROR|MB_OK );
        return;
    }
}

//-----------------------------------------------------------------------------
// OnLoadCustomFontButton
//-----------------------------------------------------------------------------
void CFontMakerApp::OnLoadCustomFontButton() 
{
    CHAR strVCFFileName[MAX_PATH];
    strVCFFileName[0] = '\0';

    OPENFILENAME ofnVCF;       // common dialog box structure
    ZeroMemory( &ofnVCF, sizeof(OPENFILENAME) );
    ofnVCF.lStructSize     = sizeof(OPENFILENAME);
    ofnVCF.hwndOwner       = m_pMainWnd->m_hWnd;
    ofnVCF.lpstrFilter     = "Custom Font files (*.vcf)\0*.vcf\0\0";
    ofnVCF.nFilterIndex    = 1;
    ofnVCF.lpstrFile       = strVCFFileName;
    ofnVCF.nMaxFile        = sizeof(strVCFFileName);
    ofnVCF.lpstrFileTitle  = NULL;
    ofnVCF.nMaxFileTitle   = 0;
    ofnVCF.lpstrInitialDir = NULL;
    ofnVCF.lpstrTitle      = "Load Custom Font (VCF) File...";
    ofnVCF.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_READONLY;

    // Display the Load dialog box for the VBF file
    if ( FALSE == GetOpenFileName( &ofnVCF ) ) 
        return;

    if ( FAILED( g_Font.ReadCustomFontFile( strVCFFileName ) ) )
    {
        m_pMainWnd->MessageBox( "Could not load the Valve bitmap custom font file.", 
                                "Error", MB_ICONERROR|MB_OK );
        return;
    }

    // Reset the selected glpyh
    UpdateSelectedGlyph( FALSE );

    if ( FAILED( CalculateAndRenderGlyphs() ) )
    {
        // Could not create new font
        MessageBox( m_pMainWnd->m_hWnd, "Could not create the requested font!", "Error", MB_ICONERROR|MB_OK );
        return;
    }

    m_pDialogBar->GetDlgItem( IDC_FONT_GROUPBOX )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_FONTNAME_LABEL )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_FONTSTYLE_LABEL )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_FONTSIZE_LABEL )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_FONTNAME_STATIC )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_FONTSTYLE_STATIC )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_FONTSIZE_STATIC )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_EFFECTSOUTLINED_CHECK )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_EFFECTSSHADOWED_CHECK )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_EFFECTSBLURRED_CHECK )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_EFFECTSSCANLINES_CHECK )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_EFFECTSANTIALIAS_CHECK )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_BLUR_EDIT )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_SCANLINES_EDIT )->EnableWindow( FALSE );

	CString str;
	str.Format( "%s", g_Font.m_strFontName );
	m_pDialogBar->GetDlgItem( IDC_FONTNAME_STATIC )->SetWindowText( str );

	str.Format( "%d", g_Font.m_maxCustomCharHeight );
	m_pDialogBar->GetDlgItem( IDC_FONTSIZE_STATIC )->SetWindowText( str );

	// Set texture properties
    m_pDialogBar->GetDlgItem( IDC_TEXTURE_GROUPBOX )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_TEXTUREWIDTH_LABEL )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_TEXTUREHEIGHT_LABEL )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_TEXTUREWIDTH_STATIC )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_TEXTUREHEIGHT_STATIC )->EnableWindow( TRUE );
    m_pDialogBar->GetDlgItem( IDC_TEXTURESIZE_BUTTON )->EnableWindow( TRUE );

	SetTextureSize( g_Font.m_dwTextureWidth, g_Font.m_dwTextureHeight );

    // Set glyph range properties
    m_pDialogBar->GetDlgItem( IDC_GLYPHS_GROUPBOX )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMRANGE_RADIO )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMFILE_RADIO )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_EDIT )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_EDIT )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGEFROM_LABEL )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSRANGETO_LABEL )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSFILE_EDIT )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSFILESELECTOR_BUTTON )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_GLYPHSCUSTOM_RADIO )->EnableWindow( TRUE );

    ((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMRANGE_RADIO ))->SetCheck( FALSE );
    ((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSFROMFILE_RADIO ))->SetCheck( FALSE );
    ((CButton*)m_pDialogBar->GetDlgItem( IDC_GLYPHSCUSTOM_RADIO ))->SetCheck( TRUE );

    m_pDialogBar->GetDlgItem( IDC_INSERTGLYPH_LABEL )->EnableWindow( FALSE );
    m_pDialogBar->GetDlgItem( IDC_INSERTGLYPH_EDIT )->EnableWindow( FALSE );
}

//-----------------------------------------------------------------------------
// Name: OnSaveButton() 
// Desc: User wants to save the font files
//-----------------------------------------------------------------------------
void CFontMakerApp::OnSaveButton() 
{
    CHAR strTGAFileName[MAX_PATH];
    CHAR strVBFFileName[MAX_PATH];

	if ( !g_Font.m_hFont && !g_Font.m_pCustomFilename )
		return;

    sprintf( strTGAFileName, "%s.tga", g_Font.m_strFontName );

    OPENFILENAME ofnTGA;       // common dialog box structure
    ZeroMemory( &ofnTGA, sizeof(OPENFILENAME) );
    ofnTGA.lStructSize     = sizeof(OPENFILENAME);
    ofnTGA.hwndOwner       = m_pMainWnd->m_hWnd;
    ofnTGA.lpstrFilter     = "Targa files (*.tga)\0*.tga\0\0";
    ofnTGA.nFilterIndex    = 1;
    ofnTGA.lpstrFile       = strTGAFileName;
    ofnTGA.nMaxFile        = sizeof(strTGAFileName);
    ofnTGA.lpstrFileTitle  = NULL;
    ofnTGA.nMaxFileTitle   = 0;
    ofnTGA.lpstrInitialDir = NULL;
    ofnTGA.lpstrTitle      = "Save Font Texture Image (TGA) File...";
    ofnTGA.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER;

    // Display the Save As dialog box for the TGA file
    if ( FALSE == GetSaveFileName( &ofnTGA ) ) 
        return;

	// use the tga name, but replace the extension
	CHAR *ptr;
	CHAR temp[MAX_PATH];
	int len;
    strcpy( temp, strTGAFileName );
	len = strlen( temp );
	if ( len > 4 && temp[len-4] == '.' )
	{
		temp[len-3] = 'v';
		temp[len-2] = 'b';
		temp[len-1] = 'f';

		// strip the path
		ptr = strrchr( temp, '\\' );
		if ( ptr )
		{
			strcpy( strVBFFileName, ptr+1 );
		}
		else
		{
			strcpy( strVBFFileName, temp );
		}
	}
	else
	{
	    sprintf( strVBFFileName, "%s.vbf", g_Font.m_strFontName );
	}

	// place the VBF files in the materials directory
	CHAR materialsDir[MAX_PATH];
	strcpy( materialsDir, strTGAFileName );
	strlwr( materialsDir );
	ptr = strstr( materialsDir, "\\content\\hl2x\\materialsrc\\" );
	if ( ptr )
	{	
		// need the final dirs, skip past
		CHAR *ptr2 = ptr + strlen( "\\content\\hl2x\\materialsrc\\" );
		strcpy( temp, ptr2 );

		*ptr = '\0';
		strcat( materialsDir, "\\game\\hl2x\\materials\\" );
		strcat( materialsDir, temp );

		// strip terminal filename
		ptr = materialsDir + strlen( materialsDir ) - 1;
		while ( ptr > materialsDir )
		{
			if ( *ptr == '\\' )
			{
				*ptr = '\0';
				break;
			}
			ptr--;
		}
	}
	else
	{
		materialsDir[0] = '\0';
	}

    // Initialize OPENFILENAME
    OPENFILENAME ofnVBF;       // common dialog box structure
    ZeroMemory( &ofnVBF, sizeof(OPENFILENAME) );
    ofnVBF.lStructSize     = sizeof(OPENFILENAME);
    ofnVBF.hwndOwner       = m_pMainWnd->m_hWnd;
    ofnVBF.lpstrFilter     = "Font files (*.vbf)\0*.vbf\0\0";
    ofnVBF.nFilterIndex    = 1;
    ofnVBF.lpstrFile       = strVBFFileName;
    ofnVBF.nMaxFile        = sizeof(strVBFFileName);
    ofnVBF.lpstrFileTitle  = NULL;
    ofnVBF.nMaxFileTitle   = 0;
	ofnVBF.lpstrInitialDir = materialsDir[0] ? materialsDir : NULL;
    ofnVBF.lpstrTitle      = "Save Valve Bitmap Font (VBF) File...";
    ofnVBF.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER;

    // Display the Save As dialog box for the ABC file
    if ( FALSE == GetSaveFileName( &ofnVBF ) ) 
        return;

    // Make sure the names are valid
    if ( !lstrcmp( strVBFFileName, strTGAFileName ) )
    {
        m_pMainWnd->MessageBox( "Cannot have VBF and TGA filenames be the same!\nFiles not saved.", 
                                "Error", MB_ICONERROR|MB_OK );
        return;
    }

    // Add an extension, if there was not one
    if ( 0 == ofnVBF.nFileExtension )
        lstrcat( strVBFFileName, ".vbf" );
    if ( 0 == ofnTGA.nFileExtension )
        lstrcat( strTGAFileName, ".tga" );

    // Save the valve bitmap font info file (.vbf)
    if ( FAILED( g_Font.WriteFontInfoFile( strVBFFileName ) ) )
    {
        m_pMainWnd->MessageBox( "Could not write the Valve bitmap font info file.", 
                                "Error", MB_ICONERROR|MB_OK );
        return;
    }

	// blur or scanline effects require special processing to ensure
	// they can be used in additive mode
	bool bAdditiveMode = ( g_Font.m_nBlur || g_Font.m_nScanlines );

	// a custom font requires special processing
	bool bCustomFont = g_Font.m_pCustomFilename != NULL;

    // Save the font image file (.tga)
    if ( FAILED( g_Font.WriteFontImageFile( strTGAFileName, bAdditiveMode, bCustomFont ) ) )
    {
        m_pMainWnd->MessageBox( "Could not write the font texture image file.", 
                                "Error", MB_ICONERROR|MB_OK );
    }
}




//-----------------------------------------------------------------------------
// Name: OnAbout() 
// Desc: Display about box
//-----------------------------------------------------------------------------
void CFontMakerApp::OnAbout() 
{
    CDialog dlg(IDD_ABOUT);
    dlg.DoModal();
}




//-----------------------------------------------------------------------------
// Name: OnHelp() 
// Desc: Display app help
//-----------------------------------------------------------------------------
void CFontMakerApp::OnHelp() 
{
    HKEY hRegKey;

    if( ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                       _T("SOFTWARE\\Microsoft\\XboxSDK"),
                                       0, KEY_QUERY_VALUE, &hRegKey ) )
    {
        DWORD dwSize = MAX_PATH;
        CHAR  InstallPath[MAX_PATH];

        if( ERROR_SUCCESS == RegQueryValueEx( hRegKey, _T("InstallPath"), NULL,
                                              NULL, (unsigned char *)InstallPath, &dwSize ) )
        {
            CString path = InstallPath;
            path += _T("\\doc\\xboxsdk.chm::/xbox_jbh_tool_fontmaker.htm");
            
            ::HtmlHelp( m_pMainWnd->GetSafeHwnd(), path, HH_DISPLAY_TOPIC, NULL );
            RegCloseKey( hRegKey );
            return;
        }
        RegCloseKey( hRegKey );
    }
    
    MessageBox( m_pMainWnd->GetSafeHwnd(),
                "Unable to find the Xbox SDK Help file xboxsdk.chm.",
                "Help file error", MB_ICONEXCLAMATION | MB_OK | MB_TASKMODAL );
}




//-----------------------------------------------------------------------------
// Name: OnExit() 
// Desc: User chose to exit the app
//-----------------------------------------------------------------------------
void CFontMakerApp::OnExit() 
{
    // Send a close message to the main window
    m_pMainWnd->SendMessage( WM_CLOSE );
}




//-----------------------------------------------------------------------------
// Name: ExitInstance() 
// Desc: Do some cleanup before exitting the app
//-----------------------------------------------------------------------------
int CFontMakerApp::ExitInstance() 
{
    DestroyCursor( m_hWaitCursor );

    return CWinApp::ExitInstance();
}



//-----------------------------------------------------------------------------
// Name: CalculateAndRenderGlyphs() 
// Desc: User changed the status of the selected glyph
//-----------------------------------------------------------------------------
HRESULT CFontMakerApp::CalculateAndRenderGlyphs() 
{
    HRESULT hr;
    
    // This may take some time, so display a wait cursor
    HCURSOR hOldCursor = GetCursor();
    SetCursor( m_hWaitCursor );

    // Draw the font glyphs, which may have changed layout
    if( FAILED( hr = g_Font.CalculateAndRenderGlyphs() ) )
        return hr;
    
    // Re-select the current glyph since the font data may have changed
    theApp.UpdateSelectedGlyph( g_bIsGlyphSelected, g_iSelectedGlyphNum );

    // Inform the view of the new font glyphs
    m_pView->OnNewFontGlyphs();

    // Restore the cursor
    SetCursor( hOldCursor );

    return S_OK;
}




