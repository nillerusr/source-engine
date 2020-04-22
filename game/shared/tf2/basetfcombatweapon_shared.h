//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASETFCOMBATWEAPON_SHARED_H
#define BASETFCOMBATWEAPON_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "baseplayer_shared.h"
#include "basetfplayer_shared.h"
#include "basecombatweapon_shared.h"

#if defined( CLIENT_DLL )
#define CBaseTFCombatWeapon C_BaseTFCombatWeapon
#endif
//-----------------------------------------------------------------------------
// Purpose: Client side rep of CBaseTFCombatWeapon 
//-----------------------------------------------------------------------------
class CBaseTFCombatWeapon : public CBaseCombatWeapon
{
	DECLARE_CLASS( CBaseTFCombatWeapon, CBaseCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBaseTFCombatWeapon ();

	virtual void	Precache( void );

	bool			IsCamouflaged( void );

	virtual Vector	GetTracerSrc( Vector &vecSrc, Vector &vecFireDir );

	// Check if the owner is being EMP'd and if we can't fire, play an appropriate
	//  failure sound
	// Default is to allow firing no matter what
	virtual bool	ComputeEMPFireState( void ) { return true; }

	virtual void	CheckRemoveDisguise( void );

	virtual int		GetImpactScale( void ) { return 1; };

	// FIXME: why are these virtual?
	virtual float	SequenceDuration( int iSequence );
	virtual float	SequenceDuration( void ) { return SequenceDuration( GetSequence() ); }

	virtual void	WeaponSound( WeaponSound_t sound_type, float soundtime = 0.0f );

	virtual void	PlayAttackAnimation( int activity ); 

	virtual bool	SendWeaponAnim( int iActivity );
	virtual void	SetReflectViewModelAnimations( bool reflect );
	virtual bool	IsReflectingAnimations( void ) const;
	virtual int		GetLastReflectedActivity( void ) { return m_iLastReflectedActivity; };
	virtual int		GetOtherWeaponsActivity( int iActivity ) { return iActivity; }
	virtual int		ReplaceOtherWeaponsActivity( int iActivity ) { return iActivity; }
	virtual bool	SupportsTwoHanded( void ) { return false; };

	virtual void	CleanupOnActStart( void ) { return; }

	bool			IsOwnerEMPed();

	virtual void	BulletWasFired( const Vector &vecStart, const Vector &vecEnd ) { return; };

	// Technology handling
	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology ) { return; };

	/*
	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}
	*/

	virtual int		GetPrimaryAmmo( void );

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual float	CalcViewmodelBob( void );


#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() &&
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	// Camo
	virtual int		GetFxBlend( void );
	virtual bool	IsTransparent( void );

	virtual int		GetSecondaryAmmo( void );
	virtual int		DrawModel( int flags );
	virtual void	DrawAmmo( void );
	virtual void	DrawMiniAmmo( void );
	virtual bool	ShouldDrawPickup( void );

	virtual void	OnDataChanged( DataUpdateType_t updateType );

	virtual const char *GetPrintName( void );
	virtual bool		ShouldShowUsageHint( void );

	static void			CreateCrosshairPanels( void );
	static void			DestroyCrosshairPanels( void );

	virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );

protected:
 	static vgui::Label	*m_pCrosshairAmmo;

private:
	// Share crosshair stuff among all weapons
	static bool			m_bCrosshairInitialized;
	// Create/destroy shared crosshair object
	static void			InitializeCrosshairPanels( void );

private:
	CBaseTFCombatWeapon ( const CBaseTFCombatWeapon  & );

#else
	virtual void	AddAssociatedObject( CBaseObject *pObject ) { }
	virtual void	RemoveAssociatedObject( CBaseObject *pObject ) { }
protected:

	// CVars that contain my damage details
	const ConVar	*m_pDamageCVar;
	const ConVar	*m_pRangeCVar;

#endif

protected:
	// If true, reflect and weapon animations to all view models
	CNetworkVar( bool, m_bReflectViewModelAnimations );
	int				m_iLastReflectedActivity;

	float			bobtime;
	float			bob;
};

#endif // BASETFCOMBATWEAPON_SHARED_H



