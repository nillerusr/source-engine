//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic playerclass
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CLASS_MEDIC_H
#define TF_CLASS_MEDIC_H

#ifdef _WIN32
#pragma once
#endif


#include "TFClassData_Shared.h"

//=====================================================================
// Medic
class CPlayerClassMedic : public CPlayerClass
{
	DECLARE_CLASS( CPlayerClassMedic, CPlayerClass );
public:
	CPlayerClassMedic( CBaseTFPlayer *pPlayer, TFClass iClass  );
	virtual ~CPlayerClassMedic();

	virtual void	ClassActivate( void );
	virtual void	ClassDeactivate( void );

	virtual const char*	GetClassModelString( int nTeam );

	// Class Initialization
	virtual void	CreateClass( void );			// Create the class upon initial spawn
	virtual void	RespawnClass( void );			// Called upon all respawns
	virtual bool	ResupplyAmmo( float flPercentage, ResupplyReason_t reason );
	virtual void    SetupMoveData( void );			// Override class specific movement data here.
	virtual void	SetupSizeData( void );			// Override class specific size data here.
	virtual void	ResetViewOffset( void );

	PlayerClassMedicData_t *GetClassData( void ) { return &m_ClassData; }

	virtual void	ClassThink( void );
	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );

	// Objects
	int		CanBuild( int iObjectType );

	// Orders
	virtual void	CreatePersonalOrder( void );

	// Hooks
	virtual void	SetPlayerHull( void );
	virtual void	GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax );

	virtual float	OnTakeDamage( const CTakeDamageInfo &info );

	// Player physics shadow.
	void			InitVCollision( void );

protected:

	// Weapons
	CHandle<CBaseTFCombatWeapon> m_hWpnPlasma;

	bool	m_bHasAutoRepair;
	float	m_flNextHealTime;

	PlayerClassMedicData_t	m_ClassData;
};

EXTERN_SEND_TABLE( DT_PlayerClassMedicData )

#endif // TF_CLASS_MEDIC_H
