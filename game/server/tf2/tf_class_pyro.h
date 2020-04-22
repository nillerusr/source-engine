//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CLASS_PYRO_H
#define TF_CLASS_PYRO_H
#ifdef _WIN32
#pragma once
#endif


#include "TFClassData_Shared.h"


class CWeaponFlameThrower;
class CWeaponGasCan;


//=====================================================================
// Medic
class CPlayerClassPyro : public CPlayerClass
{
	DECLARE_CLASS( CPlayerClassPyro, CPlayerClass );
public:
	CPlayerClassPyro( CBaseTFPlayer *pPlayer, TFClass iClass  );
	virtual ~CPlayerClassPyro();


// Overrides.
public:
	
	virtual void	SetupSizeData();
	virtual void	CreateClass();
	virtual void	SetPlayerHull();
	virtual void	GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax );
	virtual void	ResetViewOffset();
	virtual void	InitVCollision();
	virtual bool	ResupplyAmmo( float flFraction, ResupplyReason_t reason );
	virtual void	ClassActivate();

	virtual const char*	GetClassModelString( int nTeam );


private:

	PlayerClassPyroData_t	m_ClassData;

	CHandle<CWeaponFlameThrower>	m_hWpnFlameThrower;
	CHandle<CWeaponGasCan>			m_hWpnGasCan;
};



#endif // TF_CLASS_PYRO_H
