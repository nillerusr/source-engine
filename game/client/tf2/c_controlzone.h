//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#if !defined( C_CONTROLZONE_H )
#define C_CONTROLZONE_H
#ifdef _WIN32
#pragma once
#endif

class ConVar;
//-----------------------------------------------------------------------------
// Purpose: Client side rep of control zone entity ( trigger, so not usually visible )
//-----------------------------------------------------------------------------
class C_ControlZone : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ControlZone, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_ControlZone();
	virtual			~C_ControlZone();

	virtual	bool	ShouldDraw();
	virtual void	OnDataChanged( DataUpdateType_t updateType );

public:
	int				m_nZoneNumber;

private:
	const ConVar	*m_pShowTriggers;
};

#endif // C_CONTROLZONE_H