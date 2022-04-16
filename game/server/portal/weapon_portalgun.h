//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_PORTALGUN_H
#define WEAPON_PORTALGUN_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_portalbasecombatweapon.h"

#include "prop_portal.h"


class CWeaponPortalgun : public CBasePortalCombatWeapon
{
	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CWeaponPortalgun, CBasePortalCombatWeapon );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

private:
	CNetworkVar( bool,	m_bCanFirePortal1 );	// Is able to use primary fire
	CNetworkVar( bool,	m_bCanFirePortal2 );	// Is able to use secondary fire
	CNetworkVar( int,	m_iLastFiredPortal );	// Which portal was placed last
	CNetworkVar( bool,	m_bOpenProngs );		// Which portal was placed last
	CNetworkVar( float,	m_fCanPlacePortal1OnThisSurface );	// Tells the gun if it can place on the surface it's pointing at
	CNetworkVar( float,	m_fCanPlacePortal2OnThisSurface );	// Tells the gun if it can place on the surface it's pointing at

public:
	unsigned char m_iPortalLinkageGroupID; //which portal linkage group this gun is tied to, usually set by mapper, or inherited from owning player's index
	
	// HACK HACK! Used to make the gun visually change when going through a cleanser!
	CNetworkVar( float,	m_fEffectsMaxSize1 );
	CNetworkVar( float,	m_fEffectsMaxSize2 );

public:
	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_10DEGREES;
		return cone;
	}
	
	virtual void Precache ( void );

	virtual void CreateSounds( void );
	virtual void StopLoopingSounds( void );

	virtual void OnRestore( void );
	virtual void UpdateOnRemove( void );
	void Spawn( void );
	virtual void Activate();
	void DoEffectBlast( bool bPortal2, int iPlacedBy, const Vector &ptStart, const Vector &ptFinalPos, const QAngle &qStartAngles, float fDelay );
	virtual void OnPickedUp( CBaseCombatCharacter *pNewOwner );

	virtual bool ShouldDrawCrosshair( void );
	float GetPortal1Placablity( void ) { return m_fCanPlacePortal1OnThisSurface; }
	float GetPortal2Placablity( void ) { return m_fCanPlacePortal2OnThisSurface; }
	void SetLastFiredPortal( int iLastFiredPortal ) { m_iLastFiredPortal = iLastFiredPortal; }
	int GetLastFiredPortal( void ) { return m_iLastFiredPortal; }

	bool Reload( void );
	void FillClip( void );
	void CheckHolsterReload( void );
	void ItemHolsterFrame( void );
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	bool Deploy( void );

	void SetCanFirePortal1( bool bCanFire = true );
	void SetCanFirePortal2( bool bCanFire = true );
	float CanFirePortal1( void ) { return m_bCanFirePortal1; }
	float CanFirePortal2( void ) { return m_bCanFirePortal2; }

	void PrimaryAttack( void );
	void SecondaryAttack( void );

	void DelayAttack( float fDelay );

	virtual bool PreThink( void );
	virtual void Think( void );

	void OpenProngs( bool bOpenProngs );

	void InputChargePortal1( inputdata_t &inputdata );
	void InputChargePortal2( inputdata_t &inputdata );
	void FirePortal1( inputdata_t &inputdata );
	void FirePortal2( inputdata_t &inputdata );
	void FirePortalDirection1( inputdata_t &inputdata );
	void FirePortalDirection2( inputdata_t &inputdata );

	float TraceFirePortal( bool bPortal2, const Vector &vTraceStart, const Vector &vDirection, trace_t &tr, Vector &vFinalPosition, QAngle &qFinalAngles, int iPlacedBy, bool bTest = false );
	float FirePortal( bool bPortal2, Vector *pVector = 0, bool bTest = false );

	CSoundPatch		*m_pMiniGravHoldSound;

	// Outputs for portalgun
	COutputEvent m_OnFiredPortal1;		// Fires when the gun's first (blue) portal is fired
	COutputEvent m_OnFiredPortal2;		// Fires when the gun's second (red) portal is fired

	void DryFire( void );
	virtual float GetFireRate( void ) { return 0.7; };
	void WeaponIdle( void );

	PortalWeaponID GetWeaponID( void ) const { return WEAPON_PORTALGUN; }

protected:

	void	StartEffects( void );	// Initialize all sprites and beams
	void	StopEffects( bool stopSound = true );	// Hide all effects temporarily
	void	DestroyEffects( void );	// Destroy all sprites and beams

	// Portalgun effects
	void	DoEffect( int effectType, Vector *pos = NULL );

	void	DoEffectClosed( void );
	void	DoEffectReady( void );
	void	DoEffectHolding( void );
	void	DoEffectNone( void );

	CNetworkVar( int,	m_EffectState );		// Current state of the effects on the gun

public:

	DECLARE_ACTTABLE();

	CWeaponPortalgun(void);

private:
	CWeaponPortalgun( const CWeaponPortalgun & );

};


#endif // WEAPON_PORTALGUN_H
