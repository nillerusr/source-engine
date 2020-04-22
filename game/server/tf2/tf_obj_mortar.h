//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_MORTAR_H
#define TF_OBJ_MORTAR_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj.h"
#include "utllinkedlist.h"

#define MAX_DEPLOYED_MORTARS		1

class CWeaponMortar;

// ------------------------------------------------------------------------ //
// Mortar object that's built by the player
// ------------------------------------------------------------------------ //
class CObjectMortar : public CBaseObject
{
	DECLARE_CLASS( CObjectMortar, CBaseObject );
public:
	static CObjectMortar* Create(const Vector &vOrigin, const QAngle &vAngles, int team);

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CObjectMortar();

	virtual int		ObjectCaps( void ) { return FCAP_IMPULSE_USE; };
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	Spawn();
	virtual void	Precache();
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );

	// Firing called by the mortar "weapon"
	bool			FireMortar( float flFiringPower, float flFiringAccuracy, bool bRangeUpgraded, bool bAccuracyUpgraded );

	// Salvo reloading
	void			StartReloading( void );
	void			ReloadingThink( void );
	bool			IsReloading( void ) { return (m_iSalvoLeft <= 0); };

	float			LastBlastTime() { return m_flLastBlastTime; }
	void			SetBlastTime( float time ) { m_flLastBlastTime = time; }

	const Vector	&LastBlastPosition() { return m_vLastBlastPos; }
	void			SetBlastPosition( const Vector &pos ) { m_vLastBlastPos = pos; }

private:
	CNetworkVar( int, m_iRoundType );
	CNetworkArray( int, m_iMortarRounds, MA_LASTAMMOTYPE );

	int				m_iSalvoLeft;	// Decremented every shot. Once depleted, the mortar must reload.

	// Stored for the global mortar list for anti-mortar orders.
	unsigned short	m_MortarListIndex;
	Vector			m_vLastBlastPos;
	double			m_flLastBlastTime;			// -1 if no shots have hit anything yet.
};

#endif // TF_OBJ_MORTAR_H
