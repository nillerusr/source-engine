//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sapper player class
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CLASS_SAPPER_H
#define TF_CLASS_SAPPER_H
#ifdef _WIN32
#pragma once
#endif

#include "TFClassData_Shared.h"
#include "damagemodifier.h"

class CEMPPulseStatus;
class CBaseObject;


//-----------------------------------------------------------------------------
// Purpose: Sapper
//-----------------------------------------------------------------------------
class CPlayerClassSapper : public CPlayerClass
{
public:
	DECLARE_CLASS( CPlayerClassSapper, CPlayerClass );

	CPlayerClassSapper( CBaseTFPlayer *pPlayer, TFClass iClass  );
	~CPlayerClassSapper();

	// Any objects created/owned by class should be allocated and destroyed here
	virtual void			ClassActivate( void );
	virtual void			ClassDeactivate( void );

	virtual const char*	GetClassModelString( int nTeam );

	// Class Initialization
	virtual void	CreateClass( void );			// Create the class upon initial spawn
	virtual void	RespawnClass( void );			// Called upon all respawns
	virtual bool	ResupplyAmmo( float flPercentage, ResupplyReason_t reason );
	virtual void    SetupMoveData( void );			// Override class specific movement data here.
	virtual void	SetupSizeData( void );			// Override class specific size data here.
	virtual void	ResetViewOffset( void );

	PlayerClassSapperData_t *GetClassData( void ) { return &m_ClassData; }

	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );

	virtual void	ClassThink( void );

	// Energy draining
	void			AddDrainedEnergy( float flEnergy );

	// See if the player has not moved in time_required seconds
	bool			CheckStationaryTime( float time_required );

	// Orders
	virtual void	CreatePersonalOrder( void );

	// Hooks
	virtual void	SetPlayerHull( void );

	float			GetDrainedEnergy( void );
	void			DeductDrainedEnergy( float flEnergy );

public:
	CNetworkVar( float, m_flDrainedEnergy );

private:
	bool				m_bHasMediumRangeEMP;
	bool				m_bHasFasterRechargingEMP;
	bool				m_bHasHugeRadiusEMP;
	bool				m_bHasLongerLastingEMPEffect;

	// Energy drained with the drainbeam
	float				m_flBoostEndTime;
	CDamageModifier		m_DamageModifier;

	static int			m_nEffectIndex;
	CEMPPulseStatus		*m_pEMPPulseStatus;

	Vector			m_vecLastOrigin;
	float			m_flLastMoveTime;

	PlayerClassSapperData_t	m_ClassData;

	// Weapons
	CHandle<CBaseTFCombatWeapon> m_hWpnPlasma;
};

EXTERN_SEND_TABLE( DT_PlayerClassSapperData )


extern char *g_pszEMPPulseStart;

#endif // TF_CLASS_SAPPER_H
