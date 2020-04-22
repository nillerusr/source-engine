//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_CLASS_COMMANDO_H
#define C_TF_CLASS_COMMANDO_H
#ifdef _WIN32
#pragma once
#endif

#include "c_tf_playerclass.h"
#include "dt_recv.h"

class C_PlayerClassCommando : public C_PlayerClass
{
public:
	
	DECLARE_CLASS( C_PlayerClassCommando, C_PlayerClass );

	DECLARE_PREDICTABLE();

								C_PlayerClassCommando( C_BaseTFPlayer *pPlayer );
	virtual						~C_PlayerClassCommando();

	PlayerClassCommandoData_t	*GetClassData( void ) { return &m_ClassData; } 

	void						ClassThink( void );
	void						PostClassThink( void );
	void						ClassPreDataUpdate( void );
	void						ClassOnDataChanged( void );

	void						CreateMove( float flInputSampleTime, CUserCmd *pCmd );

	// Vehicles
	bool						CanGetInVehicle( void );

	PlayerClassCommandoData_t	m_ClassData;

private:

	void						CheckBullRushState( void );
	void						InterpolateBullRushViewAngles( void );
};

EXTERN_RECV_TABLE( DT_PlayerClassCommandoData )

#endif // C_TF_CLASS_COMMANDO_H