//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_TF_PLAYERCLASS_H
#define C_TF_PLAYERCLASS_H
#ifdef _WIN32
#pragma once
#endif

class C_BaseTFPlayer;
class CUserCmd;

class C_PlayerClass
{
public:

	DECLARE_CLASS_NOBASE( C_PlayerClass );

	C_PlayerClass( C_BaseTFPlayer *pPlayer );
	~C_PlayerClass();

	static C_PlayerClass   *Create( C_BaseTFPlayer *pPlayer, int iClassType );
	static void				Destroy( C_PlayerClass *pPlayerClass );

	virtual void			PreClassThink( void ) {};
	virtual void			ClassThink( void ) {};
	virtual void			PostClassThink( void ) {};

	virtual void			ClassPreDataUpdate( void ) {};
	virtual void			ClassOnDataChanged( void ) {};

	virtual void			CreateMove( float flInputSampleTime, CUserCmd *pCmd ) {};

	// Vehicles
	virtual bool			CanGetInVehicle( void ) { return true; }

protected:

	C_BaseTFPlayer		*m_pPlayer;		// reference to player (peer)
};


#include "TFClassData_Shared.h"


#endif // C_TF_PLAYERCLASS_H