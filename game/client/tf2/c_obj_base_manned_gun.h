//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_OBJ_BASE_MANNED_GUN_H
#define C_OBJ_BASE_MANNED_GUN_H
#ifdef _WIN32
#pragma once
#endif

#include "basetfvehicle.h"
#include "tf_obj_manned_plasmagun_shared.h"
#include "ObjectControlPanel.h"
#include "tf_obj_base_manned_gun.h"


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CMannedPlasmagunControlPanel : public CRotatingObjectControlPanel
{
	DECLARE_CLASS( CMannedPlasmagunControlPanel, CRotatingObjectControlPanel );

public:
	CMannedPlasmagunControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();
	virtual void OnCommand( const char *command );

	void GetInGun( void );

private:
	vgui::Label		*m_pMannedLabel;
	vgui::Button	*m_pOccupyButton;
};

#endif // C_OBJ_BASE_MANNED_GUN_H
