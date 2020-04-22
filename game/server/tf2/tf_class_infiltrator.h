//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Infiltrator
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CLASS_INFILTRATOR_H
#define TF_CLASS_INFILTRATOR_H

#ifdef _WIN32
#pragma once
#endif

#define INFILTRATOR_EAVESDROP_RADIUS	256.0f
#define INFILTRATOR_DISGUISE_TIME		3.0f

// Time after losing camo before the infiltrator can re-camo
#define INFILTRATOR_RECAMO_TIME				5.0
// Time after spawning that the infilitrator's camo kicks in
#define INFILTRATOR_CAMOTIME_AFTER_SPAWN	3.0

#include "TFClassData_Shared.h"

class CLootableCorpse;

//=====================================================================
// Infiltrator
class CPlayerClassInfiltrator : public CPlayerClass
{
	DECLARE_CLASS( CPlayerClassInfiltrator, CPlayerClass );
public:
	CPlayerClassInfiltrator( CBaseTFPlayer *pPlayer, TFClass iClass  );
	~CPlayerClassInfiltrator();

	virtual void	ClassActivate( void );

	virtual const char*	GetClassModelString( int nTeam );

	// Class Initialization
	virtual void	RespawnClass( void );			// Called upon all respawns
	virtual bool	ResupplyAmmo( float flPercentage, ResupplyReason_t reason );
	virtual void    SetupMoveData( void );			// Override class specific movement data here.
	virtual void	SetupSizeData( void );			// Override class specific size data here.
	virtual void	ResetViewOffset( void );

	PlayerClassInfiltratorData_t *GetClassData( void ) { return &m_ClassData; }

	virtual void	ClassThink( void );
	virtual void	ClearCamouflage( void );

	virtual int		CanBuild( int iObjectType );
	virtual bool	ClientCommand( const CCommand &args );
	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );

	void			CheckForAssassination( void );

	// Disguise
	virtual void	FinishedDisguising( void );
	virtual void	StopDisguising( void );

	// Hooks
	virtual void	SetPlayerHull( void );

protected:
	bool				m_bCanConsumeCorpses;
	float				m_flStartCamoAt;

	// Assassination weapon
	//CHandle<CWeaponInfiltrator>	m_hAssassinationWeapon;
	CHandle<CBaseCombatWeapon>	m_hSwappedWeapon;

	PlayerClassInfiltratorData_t	m_ClassData;
};

EXTERN_SEND_TABLE( DT_PlayerClassInfiltratorData )

#endif // TF_CLASS_INFILTRATOR_H
