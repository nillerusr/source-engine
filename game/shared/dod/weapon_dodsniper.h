//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_DODSNIPER_H
#define WEAPON_DODSNIPER_H
#ifdef _WIN32
#pragma once
#endif


#include "cbase.h" 
#include "shake.h" 
#include "weapon_dodsemiauto.h"
#include "dod_shareddefs.h"

#if defined( CLIENT_DLL )
	#define CDODSniperWeapon C_DODSniperWeapon
#endif

#define DOD_SNIPER_ZOOM_CHANGE_TIME 0.3

class CDODSniperWeapon : public CDODSemiAutoWeapon
{
public:
	DECLARE_CLASS( CDODSniperWeapon, CDODSemiAutoWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

	CDODSniperWeapon();

	virtual void Spawn( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual bool Reload( void );
	virtual void FinishReload( void );
	virtual void Drop( const Vector &vecVelocity );
	virtual void ItemPostFrame( void );

	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );

	void ResetTimers( void );	// reset all the flags, timers that would cause us to re-zoom or unzoom

#ifdef CLIENT_DLL
	virtual Vector GetDesiredViewModelOffset( C_DODPlayer *pOwner );
	virtual float GetViewModelSwayScale( void );

	float GetZoomedPercentage( void );
#endif

	// Is the weapon completely zoomed, finished the raising animation
	bool IsFullyZoomed( void );
	
	virtual bool ShouldDrawCrosshair( void );

	virtual bool HideViewModelWhenZoomed( void ) { return true; }

	virtual float GetWeaponAccuracy( float flPlayerSpeed );

	virtual float GetZoomedFOV( void ) { return 20; }

	bool IsZoomed( void );

	virtual bool ShouldZoomOutBetweenShots( void ) { return true; }
	virtual bool ShouldRezoomAfterReload( void ) { return false; }

	void ToggleZoom( void );

	void ZoomIn( void );
	void ZoomOut( void );

	void ZoomOutIn( void );

	void SetRezoom( bool bRezoom, float flDelay );

	bool IsZoomingIn( void );

	CNetworkVar( bool, m_bDoViewAnim );



#ifdef CLIENT_DLL
	bool m_bDoViewAnimCache;
	float m_flViewAnimTimer;
	float m_flZoomPercent;
#endif

protected:
	bool m_bShouldRezoomAfterShot;	// if the gun zooms out after a shot, does it zoom back in automatically?

private:
	CDODSniperWeapon( const CDODSniperWeapon & );

	float m_flUnzoomTime;
	float m_flRezoomTime;

	float m_flZoomInTime;
	float m_flZoomOutTime;	 

	bool m_bRezoomAfterReload;
	float m_flZoomChangeTime;
	bool m_bRezoomAfterShot;
};

#endif // WEAPON_DODSNIPER_H