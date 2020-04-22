//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_OBJ_RESPAWN_STATION_H
#define C_OBJ_RESPAWN_STATION_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Respawn Station object
//-----------------------------------------------------------------------------
class C_ObjectRespawnStation : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectRespawnStation, C_BaseObject );

public:
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();

	C_ObjectRespawnStation();

	// Status
	virtual void SetDormant( bool bDormant );

protected:
	// A special panel to indicate which one's the respawn station
	CPanelRegistration m_SelectedRespawnPanel;

private:
	C_ObjectRespawnStation( const C_ObjectRespawnStation & ); // not defined, not accessible
};


#endif // C_OBJ_RESPAWN_STATION_H
