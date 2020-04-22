//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef VACBANNEDCONNREFUSED_H
#define VACBANNEDCONNREFUSED_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Purpose: Displays information about new VAC bans
//-----------------------------------------------------------------------------
class CVACBannedConnRefusedDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CVACBannedConnRefusedDialog, vgui::Frame );

public:
	CVACBannedConnRefusedDialog( vgui::VPANEL hVParent, const char *name );

};




#endif // VACBANNEDCONNREFUSED_H