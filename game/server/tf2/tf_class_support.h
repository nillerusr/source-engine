//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The support/suppression player class
//
// $NoKeywords: $
//=============================================================================//
#ifndef TF_CLASS_SUPPORT_H
#define TF_CLASS_SUPPORT_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_basecombatweapon.h"
#include "TFClassData_Shared.h"

//=====================================================================
// Support 
class CPlayerClassSupport : public CPlayerClass
{
public:
	DECLARE_CLASS( CPlayerClassSupport, CPlayerClass );

	CPlayerClassSupport( CBaseTFPlayer *pPlayer, TFClass iClass  );
	~CPlayerClassSupport();

	virtual void	ClassActivate( void );
	virtual void	ClassDeactivate( void );

	virtual const char*	GetClassModelString( int nTeam );

	// Class Initialization
	virtual void	CreateClass( void );			// Create the class upon initial spawn
	virtual bool	ResupplyAmmo( float flPercentage, ResupplyReason_t reason );
	virtual void    SetupMoveData( void );			// Override class specific movement data here.
	virtual void	SetupSizeData( void );			// Override class specific size data here.
	virtual void	ResetViewOffset( void );

	PlayerClassSupportData_t *GetClassData( void ) { return &m_ClassData; }

	// Hooks
	virtual void	SetPlayerHull( void );

protected:
	PlayerClassSupportData_t	m_ClassData;
};

EXTERN_SEND_TABLE( DT_PlayerClassSupportData )

#endif // TF_CLASS_SUPPORT_H
