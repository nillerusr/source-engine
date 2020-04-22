//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/filtercombobox.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFilterComboBox::CFilterComboBox( Panel *parent, const char *panelName, int numLines, bool allowEdit ) :
	BaseClass( parent, panelName, numLines, allowEdit )
{
}

//-----------------------------------------------------------------------------
// Purpose: panel lost focus message
//-----------------------------------------------------------------------------
void CFilterComboBox::OnKillFocus()
{
	int nLength = GetTextLength();
	char *pFilterText = (char*)_alloca( (nLength+1) * sizeof(char) );
	GetText( pFilterText, nLength+1 );

	// Remove the existing version in the list
	char pItemText[512];
	int nItemCount = GetItemCount();
	int i;
	for ( i = 0; i < nItemCount; ++i )
	{
		GetItemText( i, pItemText, sizeof(pItemText) );
		if ( !Q_stricmp( pFilterText, pItemText ) )
			break;
	}

	if ( i != nItemCount )
	{
		// Remove the existing copy
		DeleteItem( i );
	}

	AddItem( pFilterText, NULL );

	BaseClass::OnKillFocus( );
}


	
