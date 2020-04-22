//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVACBannedConnRefusedDialog::CVACBannedConnRefusedDialog( VPANEL hVParent, const char *name ) : BaseClass( NULL, name )
{
	SetParent( hVParent );
	SetSize( 480, 220 );
	SetSizeable( false );

	LoadControlSettings( "servers/VACBannedConnRefusedDialog.res" );
	MoveToCenterOfScreen();
}
