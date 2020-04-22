//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <commctrl.h>
#include "mxtk/mx.h"
#include "resource.h"
#include "CCLookup.h"
#include "mdlviewer.h"
#include "addsoundentry.h"
#include <vgui/ILocalize.h>

using namespace vgui;

extern vgui::ILocalize *g_pLocalize;

static CCloseCaptionLookupParams g_Params;

static HFONT g_UnicodeFont = NULL;

static void InsertTextColumn( HWND listcontrol, int column, int width, char const *label )
{
	LVCOLUMN col;
	memset( &col, 0, sizeof( col ) );

	col.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH | LVCF_ORDER;
	col.iOrder = column;
	col.pszText = (char *)label;
	col.cchTextMax = 256;
	col.iSubItem = column;
	col.cx = width;

	ListView_InsertColumn( listcontrol, column, &col );
}

static void PopulateCloseCaptionTokenList( HWND wnd, CCloseCaptionLookupParams *params )
{
	HWND control = GetDlgItem( wnd, IDC_CCTOKENLIST );
	if ( !control )
		return;

	InsertTextColumn( control, 0, 200, "CloseCaption Token" );
	InsertTextColumn( control, 1, 800, "Text" );


	ListView_DeleteAllItems( control );

	SendMessage( control, WM_SETFONT, (WPARAM)g_UnicodeFont, MAKELPARAM (TRUE, 0) );

	StringIndex_t i = g_pLocalize->GetFirstStringIndex();
	int saveSelected = -1;

	while ( INVALID_LOCALIZE_STRING_INDEX != i )
	{
		char const *name = g_pLocalize->GetNameByIndex( i );

		LV_ITEMW lvItem;
		memset( &lvItem, 0, sizeof( lvItem ) );
		lvItem.iItem = ListView_GetItemCount( control );
		lvItem.mask = LVIF_TEXT | LVIF_PARAM;
		lvItem.lParam = (LPARAM)i;

		wchar_t label[ 256 ];
		g_pLocalize->ConvertANSIToUnicode( name, label, sizeof( label ) );

		lvItem.pszText = label;
		lvItem.cchTextMax = 256;

		SendMessage( control, LVM_INSERTITEMW, 0, (LPARAM)(const LV_ITEMW FAR*)(&lvItem));

		lvItem.mask = LVIF_TEXT;
		lvItem.iSubItem = 1;

		wchar_t		*value = g_pLocalize->GetValueByIndex( i );

		lvItem.pszText = (wchar_t *)value;
		lvItem.cchTextMax = 1024;

		SendMessage( control, LVM_SETITEMW, 0, (LPARAM)(const LV_ITEMW FAR*)(&lvItem));

		if ( !Q_stricmp( name, params->m_szCCToken ) )
		{
			ListView_SetItemState( control, lvItem.iItem, LVIS_SELECTED, LVIS_STATEIMAGEMASK );
			saveSelected = lvItem.iItem;
		}
		i = g_pLocalize->GetNextStringIndex( i );
	}

	if ( saveSelected != -1 )
	{
		ListView_EnsureVisible(control, saveSelected, FALSE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK CloseCaptionLookupDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			HWND control = GetDlgItem( hwndDlg, IDC_CCTOKENLIST );
			DWORD exStyle = GetWindowLong( control, GWL_EXSTYLE );
			exStyle |= LVS_EX_FULLROWSELECT;
			SetWindowLong( control, GWL_EXSTYLE, exStyle );

			PopulateCloseCaptionTokenList( hwndDlg, &g_Params );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetDlgItemText( hwndDlg, IDC_CCTOKEN, g_Params.m_szCCToken );

			SetFocus( GetDlgItem( hwndDlg, IDC_CCTOKENLIST ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				/*
				//int selindex = SendMessage( GetDlgItem( hwndDlg, IDC_CCTOKENLIST ), LB_GETCURSEL, 0, 0 );
				if ( selindex == LB_ERR )
				{
					mxMessageBox( NULL, "You must select an entry from the list", g_appTitle, MB_OK );
					return TRUE;
				}
				*/

				SendMessage( GetDlgItem( hwndDlg, IDC_CCTOKEN ), WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szCCToken ), (LPARAM)g_Params.m_szCCToken );

				EndDialog( hwndDlg, 1 );
			}
			break;
        case IDCANCEL:
			EndDialog( hwndDlg, 0 );
			break;
		}
		return TRUE;
	case WM_NOTIFY:
		{
			if ( wParam == IDC_CCTOKENLIST )
			{
				NMHDR *hdr = ( NMHDR * )lParam;
				if ( hdr->code == LVN_ITEMCHANGED )
				{
					HWND control = GetDlgItem( hwndDlg, IDC_CCTOKENLIST );

					NM_LISTVIEW *nmlv = ( NM_LISTVIEW * )lParam;

					int item = nmlv->iItem;

					if ( item >= 0 )
					{
						// look up the lparam value
						LVITEM lvi;
						memset( &lvi, 0, sizeof( lvi ) );
						lvi.mask = LVIF_PARAM;
						lvi.iItem = item;

						if ( ListView_GetItem( control, &lvi ) )
						{
							char const *name = g_pLocalize->GetNameByIndex( lvi.lParam );
							if ( name )
							{
								Q_strncpy( g_Params.m_szCCToken, name, sizeof( g_Params.m_szCCToken ) );
								SendMessage( GetDlgItem( hwndDlg, IDC_CCTOKEN ), WM_SETTEXT, (WPARAM)sizeof( g_Params.m_szCCToken ), (LPARAM)g_Params.m_szCCToken );
							}
						}
					}
					return FALSE;
				}
				if ( hdr->code == NM_DBLCLK )
				{
					SendMessage( GetDlgItem( hwndDlg, IDC_CCTOKEN ), WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szCCToken ), (LPARAM)g_Params.m_szCCToken );
					EndDialog( hwndDlg, 1 );
					return FALSE;
				}
			}

		}
		return TRUE;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
int CloseCaptionLookup( CCloseCaptionLookupParams *params )
{
	g_Params = *params;

	g_UnicodeFont = CreateFont(
		 -10, 
		 0,
		 0,
		 0,
		 FW_NORMAL,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 ANTIALIASED_QUALITY,
		 DEFAULT_PITCH,
		 "Tahoma" );

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_CCLOOKUP ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)CloseCaptionLookupDialogProc );

	DeleteObject( g_UnicodeFont );
	g_UnicodeFont = NULL;

	*params = g_Params;

	return retval;
}