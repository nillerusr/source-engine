//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CLASS_ESCORT_H
#define TF_CLASS_ESCORT_H

#ifdef _WIN32
#pragma once
#endif


#include "tf_playerclass.h"
#include "TFClassData_Shared.h"
#include "tf_shieldshared.h"

class CShield;
class CWeaponShield;

//=====================================================================
// Indirect
class CPlayerClassEscort : public CPlayerClass
{
public:
	DECLARE_CLASS( CPlayerClassEscort, CPlayerClass );

	CPlayerClassEscort( CBaseTFPlayer *pPlayer, TFClass iClass  );
	~CPlayerClassEscort();

	virtual void	ClassActivate( void );

	virtual const char*	GetClassModelString( int nTeam );

	// Class Initialization
	virtual void	CreateClass( void );			// Create the class upon initial spawn
	virtual bool	ResupplyAmmo( float flPercentage, ResupplyReason_t reason );
	virtual void    SetupMoveData( void );			// Override class specific movement data here.
	virtual void	SetupSizeData( void );			// Override class specific size data here.
	virtual void	ResetViewOffset( void );

	PlayerClassEscortData_t *GetClassData( void ) { return &m_ClassData; }

	// Hooks
	virtual void	SetPlayerHull( void );

	// Powerups
	virtual void	PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier );
	virtual void	PowerupEnd( int iPowerup );

private:
	// Purpose: 
	CWeaponShield *GetProjectedShield( void );

protected:
	PlayerClassEscortData_t	m_ClassData;
	CHandle<CWeaponShield>	m_hWeaponProjectedShield;
};

EXTERN_SEND_TABLE( DT_PlayerClassEscortData )

#endif // TF_CLASS_ESCORT_H
