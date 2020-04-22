//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defender player class
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CLASS_DEFENDER_H
#define TF_CLASS_DEFENDER_H
#pragma once

#include "TFClassData_Shared.h"

//=====================================================================
// Defender
class CPlayerClassDefender : public CPlayerClass
{
public:
	DECLARE_CLASS( CPlayerClassDefender, CPlayerClass );

	CPlayerClassDefender( CBaseTFPlayer *pPlayer, TFClass iClass  );
	~CPlayerClassDefender();

	virtual void	ClassActivate( void );
	virtual void	ClassDeactivate( void );

	virtual const char*	GetClassModelString( int nTeam );

	// Class Initialization
	virtual void	CreateClass( void );			// Create the class upon initial spawn
	virtual bool	ResupplyAmmo( float flPercentage, ResupplyReason_t reason );
	virtual void    SetupMoveData( void );			// Override class specific movement data here.
	virtual void	SetupSizeData( void );			// Override class specific size data here.
	virtual void	ResetViewOffset( void );

	PlayerClassDefenderData_t *GetClassData( void ) { return &m_ClassData; }

	virtual void	PlayerDied( CBaseEntity *pAttacker );

	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );
	virtual void	UpdateSentrygunTechnology( void );

	// Sentrygun building
	virtual int		CanBuild( int iObjectType );
	int				CanBuildSentryGun();
	virtual void	FinishedObject( CBaseObject *pObject );

	// Orders.
	virtual void	CreatePersonalOrder();

	// Hooks
	virtual void	SetPlayerHull( void );
	virtual void	GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax );

	// Player physics shadow.
	void			InitVCollision( void );

public:
	// Sentrygun building
	int							m_iNumberOfSentriesAllowed;
	bool						m_bHasSmarterSentryguns;
	bool						m_bHasSensorSentryguns;
	// Sentrygun types
	bool						m_bHasMachinegun;
	bool						m_bHasRocketlauncher;
	bool						m_bHasAntiair;

	// Weapons
	CHandle<CBaseTFCombatWeapon> m_hWpnPlasma;

private:
	PlayerClassDefenderData_t	m_ClassData;
};

EXTERN_SEND_TABLE( DT_PlayerClassDefenderData )

#endif // TF_CLASS_DEFENDER_H
