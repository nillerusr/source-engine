//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "resource.h"
#include "ProgressDialog.h"
#include "mxtk/mx.h"
#include "mdlviewer.h"
#include "tier1/utlstring.h"
#include "tier1/strtools.h"
#include "tier1/fmtstr.h"

#include <CommCtrl.h>

class CProgressDialog : public IProgressDialog
{
public:
	CProgressDialog();

	void Start( char const *pchTitle, char const *pchText, bool bShowCancel );
	void Update( float flZeroToOneFraction );
	void UpdateText( char const *pchFmt, ... );
	bool IsCancelled();
	void Finish();

	static BOOL CALLBACK ProgressDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );

private:

	BOOL ProgressDialogProcImpl( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );

	CUtlString	m_sTitle;
	CUtlString	m_sStatus;
	float		m_flFraction;

	bool		m_bShowCancel;
	bool		m_bWantsCancel;

	HWND		m_hwndDlg;

	double		m_flStartTime;
};

static CProgressDialog g_ProgressDialog;
IProgressDialog *g_pProgressDialog = &g_ProgressDialog;

CProgressDialog::CProgressDialog() : 
	m_flFraction( 0.0f ), m_hwndDlg( 0 ), m_bShowCancel( false ), m_bWantsCancel( false ), m_flStartTime( 0.0f )
{
}

bool CProgressDialog::IsCancelled()
{
	return m_bShowCancel && m_bWantsCancel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
BOOL CALLBACK CProgressDialog::ProgressDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	return g_ProgressDialog.ProgressDialogProcImpl( hwndDlg, uMsg, wParam, lParam );	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
void CProgressDialog::Start( char const *pchTitle, char const *pchText, bool bShowCancel )
{
	if ( m_hwndDlg )
	{
		Finish();
	}
	Assert( NULL == m_hwndDlg );

	m_sTitle = pchTitle;
	m_sStatus = pchText;
	m_flFraction = 0.0f;
	m_bShowCancel = bShowCancel;
	m_bWantsCancel = false;
	m_flStartTime = Plat_FloatTime();

	m_hwndDlg = CreateDialog( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_PROGRESS ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)ProgressDialogProc );
}

void CProgressDialog::Update( float flZeroToOneFraction )
{
	m_flFraction = clamp( flZeroToOneFraction, 0.0f, 1.0f );

	// Update the progress bar
	HWND pb = GetDlgItem( m_hwndDlg, IDC_FP_PROGRESS );
	int pos = clamp( (int)( 1000.0f * flZeroToOneFraction ), 0, 1000 );

	SendMessage( pb, (UINT) PBM_SETPOS, pos, 0 );

	HWND btn = GetDlgItem( m_hwndDlg, IDCANCEL );
	LRESULT lr = SendMessage( btn, BM_GETSTATE, 0, 0 );
	if ( lr & BST_PUSHED )
	{
		m_bWantsCancel = true;
	}

	if ( GetAsyncKeyState( VK_ESCAPE ) )
	{
		m_bWantsCancel = true;
	}

	mx::check();
}

void CProgressDialog::UpdateText( char const *pchFmt, ... )
{
	char buf[ 2048 ];
	va_list argptr;
	va_start( argptr, pchFmt );
	Q_vsnprintf( buf, sizeof( buf ), pchFmt, argptr );
	va_end( argptr );
	m_sStatus = buf;

	SetDlgItemText( m_hwndDlg, IDC_FP_PROGRESS_TEXT, CFmtStr( "%s", m_sStatus.String() ) );
	SetDlgItemText( m_hwndDlg, IDC_FP_PROGRESS_PERCENT, CFmtStr( "%.2f %%", m_flFraction * 100.0f ) );

	double elapsed = Plat_FloatTime() - m_flStartTime;
	double flPercentagePerSecond = 0.0f;
	if ( m_flFraction > 0.0f )
	{
		flPercentagePerSecond = elapsed / m_flFraction;
	}

	double flSecondsRemaining = flPercentagePerSecond * ( 1.0f - m_flFraction );

	int seconds = (int)flSecondsRemaining;

	CFmtStr string;

	int hours = 0;
	int minutes = seconds / 60;

	if ( minutes > 0 )
	{
		seconds -= (minutes * 60);
		hours = minutes / 60;

		if ( hours > 0 )
		{
			minutes -= (hours * 60);
		}
	}

	if ( hours > 0 )
	{
		string.sprintf( "Time Remaining:  %2i:%02i:%02i", hours, minutes, seconds );
	}
	else
	{
		string.sprintf( "Time Remaining:  %02i:%02i", minutes, seconds );
	}

	SetDlgItemText( m_hwndDlg, IDC_FP_PROGRESS_ETA, string.Access() );
}

void CProgressDialog::Finish()
{
	if ( !m_hwndDlg )
		return;
	DestroyWindow( m_hwndDlg );
	m_hwndDlg = NULL;
}

BOOL CProgressDialog::ProgressDialogProcImpl( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			RECT rcDlg;
			GetWindowRect( hwndDlg, &rcDlg );

			// Get relative to primary monitor instead of actual window parent
			RECT rcParent;
			rcParent.left = 0;
			rcParent.right = rcParent.left + GetSystemMetrics( SM_CXFULLSCREEN );
			rcParent.top = 0;
			rcParent.bottom = rcParent.top + GetSystemMetrics( SM_CYFULLSCREEN );

			int dialogw, dialogh;
			int parentw, parenth;

			parentw = rcParent.right - rcParent.left;
			parenth = rcParent.bottom - rcParent.top;
			dialogw = rcDlg.right - rcDlg.left;
			dialogh = rcDlg.bottom - rcDlg.top;

			int dlgleft, dlgtop;
			dlgleft = ( parentw - dialogw ) / 2;
			dlgtop = ( parenth - dialogh ) / 2;

			MoveWindow( hwndDlg, 
				dlgleft,
				dlgtop,
				dialogw,
				dialogh,
				TRUE
				);

			SetDlgItemText( hwndDlg, IDC_FP_PROGRESS_TITLE, m_sTitle.String() );
			SetDlgItemText( hwndDlg, IDC_FP_PROGRESS_TEXT, m_sStatus.String() );

			HWND pb = GetDlgItem( hwndDlg, IDC_FP_PROGRESS );
			SendMessage( pb, (UINT) PBM_SETRANGE, 0, MAKELPARAM( 0, 1000 ) );

			ShowWindow( GetDlgItem( hwndDlg, IDCANCEL ), m_bShowCancel ? SW_SHOW : SW_HIDE );

			Update( 0.0f );
		}
		return FALSE;  
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
			m_bWantsCancel = true;
			break;
		}
		return TRUE;
	}
	return FALSE;
}