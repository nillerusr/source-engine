//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_CLASS_SNIPER_H
#define TF_CLASS_SNIPER_H
#pragma once

#include "TFClassData_Shared.h"


//=====================================================================
// Sniper
class CPlayerClassSniper : public CPlayerClass
{
public:
	DECLARE_CLASS( CPlayerClassSniper, CPlayerClass );

	CPlayerClassSniper( CBaseTFPlayer *pPlayer, TFClass iClass  );
	~CPlayerClassSniper();

	virtual void	ClassActivate( void );
	virtual void	ClassDeactivate( void );

	virtual const char*	GetClassModelString( int nTeam );

	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );

	// Class Initialization
	virtual void	RespawnClass( void );			// Called upon all respawns
	virtual bool	ResupplyAmmo( float flPercentage, ResupplyReason_t reason );
	virtual void    SetupMoveData( void );			// Override class specific movement data here.
	virtual void	SetupSizeData( void );			// Override class specific size data here.
	virtual void	ResetViewOffset( void );

	PlayerClassSniperData_t *GetClassData( void ) { return &m_ClassData; }

	// Deployment
	virtual float	GetDeployTime( void );

	// Class abilities
	virtual void	ClassThink();
	// Hiding
	void			CheckHiding( void );

	// Orders
	virtual void		CreatePersonalOrder( void );

	// Hooks
	virtual void	SetPlayerHull( void );

protected:
	// Hiding
	bool	m_bHiding;
	float	m_flHideTransparency;
	float	m_flLastHideUpdate;

	PlayerClassSniperData_t	m_ClassData;

private:
	bool	m_bCanHide;
};

EXTERN_SEND_TABLE( DT_PlayerClassSniperData )

#endif // TF_CLASS_SNIPER_H
