//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_PLAYERSTATUS_WEAPON_H
#define DOD_HUD_PLAYERSTATUS_WEAPON_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Weapon Selection panel
//-----------------------------------------------------------------------------
class CDoDHudCurrentWeapon : public vgui::EditablePanel 
{
	DECLARE_CLASS_SIMPLE( CDoDHudCurrentWeapon, vgui::EditablePanel );

public:
	CDoDHudCurrentWeapon( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name ){}

	virtual void Paint();

};	

#endif // DOD_HUD_PLAYERSTATUS_WEAPON_H