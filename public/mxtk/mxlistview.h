//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#ifndef MXLISTVIEW_H
#define MXLISTVIEW_H
#ifdef _WIN32
#pragma once
#endif

#ifndef INCLUDED_MXWIDGET
#include "mxtk/mxWidget.h"
#endif

#include <wchar.h>

class mxWindow;

class mxListView_i;
class mxListView : public mxWidget
{
	mxListView_i *d_this;

public:
	// CREATORS
	mxListView (mxWindow *parent, int x, int y, int w, int h, int id = 0);
	virtual ~mxListView ();

	// MANIPULATORS
	int add( const char *label );
	void remove ( int item );
	void removeAll ();
	void setLabel ( int item, int column, const char *label);
	void setLabel( int item, int column, const wchar_t *label );

	void setUserData (int item, int column, void *userData);
	void setSelected ( int item, bool b);
	void deselectAll();
	void setImageList( void *himagelist );
	void setImage( int item, int column, int imagenormal );

	void insertTextColumn( int column, int width, char const *label );
	void insertImageColumn( int column, int width, int imageindex );

	void scrollToItem( int item );

	// ACCESSORS
	int	getItemCount() const;
	int getNumSelected() const;
	int getNextSelectedItem ( int startitem = 0 ) const;
	const char *getLabel ( int oitem, int column ) const;
	void *getUserData (int item, int column ) const;
	bool isSelected ( int index ) const;

	void setDrawingEnabled( bool draw );


private:
	// NOT IMPLEMENTED
	mxListView (const mxListView&);
	mxListView& operator= (const mxListView&);
};

#endif // MXLISTVIEW_H
