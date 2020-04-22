//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxListView.cpp
// implementation: Win32 API
// last modified:  May 03 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mxListView.h"
#include <windows.h>
#include <commctrl.h>



class mxListView_i
{
public:
	HWND d_hwnd;
};



mxListView::mxListView (mxWindow *parent, int x, int y, int w, int h, int id)
: mxWidget (parent, x, y, w, h)
{
	if (!parent)
		return;

	d_this = new mxListView_i;

	DWORD dwStyle = LVS_NOSORTHEADER | LVS_REPORT | LVS_SHOWSELALWAYS | WS_VISIBLE | WS_CHILD;
	HWND hwndParent = (HWND) ((mxWidget *) parent)->getHandle ();

	d_this->d_hwnd = CreateWindowEx (WS_EX_CLIENTEDGE, WC_LISTVIEW, "", dwStyle,
				x, y, w, h, hwndParent,
				(HMENU) id, (HINSTANCE) GetModuleHandle (NULL), NULL);
	
	SendMessage (d_this->d_hwnd, WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_VAR_FONT), MAKELPARAM (TRUE, 0));
	SetWindowLong (d_this->d_hwnd, GWL_USERDATA, (LONG) this);

	setHandle ((void *) d_this->d_hwnd);
	setType (MX_LISTVIEW);
	setParent (parent);
	setId (id);
}



mxListView::~mxListView ()
{
	remove (0);
	delete d_this;
}
int mxListView::add ( const char *item )
{
	if (!d_this)
		return 0;

	LVITEM lvItem;
	memset( &lvItem, 0, sizeof( lvItem ) );

	lvItem.iItem = getItemCount();

	lvItem.mask = LVIF_TEXT;
	lvItem.pszText = (LPSTR) item;
	lvItem.cchTextMax = 256;

	return ListView_InsertItem( d_this->d_hwnd, &lvItem );
}

void
mxListView::remove ( int index )
{
	if (!d_this)
		return;

	ListView_DeleteItem (d_this->d_hwnd, index );
}

void
mxListView::removeAll ()
{
	ListView_DeleteAllItems(d_this->d_hwnd);
}

void
mxListView::setLabel ( int item , int column, const char *label)
{
	if (!d_this)
		return;

	LVITEM lvItem;
	memset( &lvItem, 0, sizeof( lvItem ) );
	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = item;
	lvItem.iSubItem = column;

	lvItem.pszText = (LPSTR) label;
	lvItem.cchTextMax = 256;

	ListView_SetItem (d_this->d_hwnd, &lvItem);
}

void mxListView::setLabel( int item, int column, const wchar_t *label )
{
	if (!d_this)
		return;

	LV_ITEMW lvItem;
	memset( &lvItem, 0, sizeof( lvItem ) );
	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = item;
	lvItem.iSubItem = column;

	lvItem.pszText = (wchar_t *)label;
	lvItem.cchTextMax = 256;

   SendMessage(d_this->d_hwnd, LVM_SETITEMW, 0, (LPARAM)(const LV_ITEMW FAR*)(&lvItem));
}

void
mxListView::setUserData ( int item, int column, void *userData)
{
	if (!d_this)
		return;

	LVITEM lvItem;
	memset( &lvItem, 0, sizeof( lvItem ) );
	lvItem.mask = LVIF_PARAM;
	lvItem.iItem = item;
	lvItem.iSubItem = column;
	lvItem.lParam = (LPARAM) userData;

	ListView_SetItem (d_this->d_hwnd, &lvItem);
}


void
mxListView::setSelected ( int item, bool b)
{
	if (!d_this)
		return;

	ListView_SetItemState (d_this->d_hwnd, item, b ? ( LVIS_SELECTED | LVIS_FOCUSED ): 0 , LVIS_SELECTED | LVIS_FOCUSED );
}

int mxListView::getItemCount() const
{
	if (!d_this)
		return 0;

	return ListView_GetItemCount( d_this->d_hwnd );
}

int mxListView::getNextSelectedItem( int startitem /*= 0*/ ) const
{
	if (!d_this)
		return -1;

	if ( ListView_GetSelectedCount( d_this->d_hwnd ) == 0 )
		return -1;

	int c = getItemCount();
	int start = startitem + 1;

	while ( start < c )
	{
		if ( isSelected( start ) )
			return start;
		start++;
	}

	return -1;
}

int mxListView::getNumSelected() const
{
	if (!d_this)
		return 0;

	return ListView_GetSelectedCount( d_this->d_hwnd );
}

const char*
mxListView::getLabel ( int item, int column ) const
{
	static char label[256];
	strcpy (label, "");

	if (!d_this)
		return label;

	LVITEM lvItem;
	memset( &lvItem, 0, sizeof( lvItem ) );
	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = item;
	lvItem.iSubItem = column;
	lvItem.pszText = (LPSTR) label;
	lvItem.cchTextMax = 256;
	ListView_GetItem (d_this->d_hwnd, &lvItem);

	return lvItem.pszText;
}



void*
mxListView::getUserData ( int item, int column ) const
{
	if (!d_this)
		return 0;

	LVITEM lvItem;
	memset( &lvItem, 0, sizeof( lvItem ) );
	lvItem.mask = LVIF_PARAM;
	lvItem.iItem = item;
	lvItem.iSubItem = column;

	ListView_GetItem (d_this->d_hwnd, &lvItem);

	return (void *) lvItem.lParam;
}



bool
mxListView::isSelected ( int index ) const
{
	if (!d_this)
		return false;

	int state = ListView_GetItemState( d_this->d_hwnd, index, LVIS_SELECTED );
	if ( state & LVIS_SELECTED )
		return true;

	return false;
}

void mxListView::setImageList( void *himagelist )
{
	ListView_SetImageList(d_this->d_hwnd, (HIMAGELIST)himagelist, LVSIL_SMALL );
}

void mxListView::setImage( int item, int column, int imagenormal )
{
	if (!d_this)
		return;

	LVITEM lvItem;
	memset( &lvItem, 0, sizeof( lvItem ) );
	lvItem.mask = LVIF_IMAGE;
	lvItem.iItem = item;
	lvItem.iSubItem = column;
	lvItem.iImage = imagenormal;
	//lvItem.state = INDEXTOSTATEIMAGEMASK( imagenormal );
	//lvItem.stateMask = -1;

	ListView_SetItem (d_this->d_hwnd, &lvItem);
}

void mxListView::insertTextColumn( int column, int width, char const *label )
{
	if (!d_this)
		return;

	LVCOLUMN col;
	memset( &col, 0, sizeof( col ) );

	col.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH | LVCF_ORDER;
	col.iOrder = column;
	col.pszText = (char *)label;
	col.cchTextMax = 256;
	col.iSubItem = column;
	col.cx = width;

	ListView_InsertColumn( d_this->d_hwnd, column, &col );
}

void mxListView::insertImageColumn( int column, int width, int imageindex )
{
	if (!d_this)
		return;

	LVCOLUMN col;
	memset( &col, 0, sizeof( col ) );

	col.mask = LVCF_IMAGE | LVCF_SUBITEM | LVCF_WIDTH | LVCF_ORDER | LVCF_FMT;
	col.fmt = LVCFMT_IMAGE;
	col.iOrder = column;
	col.iSubItem = column;
	col.cx = width;
	col.iImage = imageindex;

	ListView_InsertColumn( d_this->d_hwnd, column, &col );
}

void mxListView::setDrawingEnabled( bool draw )
{
	if (!d_this)
		return;

	SendMessage( d_this->d_hwnd, WM_SETREDRAW, (WPARAM)draw ? TRUE : FALSE, (LPARAM)0 );
}

void mxListView::deselectAll()
{
	if ( !d_this )
		return;

	setDrawingEnabled( false );
	int c = getItemCount();
	for ( int i = 0; i < c; i++ )
	{
		if ( isSelected( i ) )
		{
			setSelected( i, false );
		}
	}

	setDrawingEnabled( true );
}

void mxListView::scrollToItem( int item )
{
	if ( !d_this )
		return;

	ListView_EnsureVisible( d_this->d_hwnd, item, FALSE );
}
