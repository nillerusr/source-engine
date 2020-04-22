//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_EMP_H
#define GRENADE_EMP_H
#ifdef _WIN32
#pragma once
#endif

class CSprite;

#include "grenade_base_empable.h"

#if defined( CLIENT_DLL )

#define CGrenadeEMP C_GrenadeEMP

#endif

//-----------------------------------------------------------------------------
// Purpose: EMP grenade
//-----------------------------------------------------------------------------
class CGrenadeEMP : public CBaseEMPableGrenade
{
	DECLARE_CLASS( CGrenadeEMP, CBaseEMPableGrenade );
public:
	CGrenadeEMP();

	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	UpdateOnRemove( void );
	virtual void	Explode( trace_t *pTrace, int bitsDamageType );
	virtual void	BounceTouch( CBaseEntity *pOther );
	virtual void	BounceSound( void );
	virtual float	GetShakeAmplitude( void );
	virtual int		GetDamageType() const { return DMG_BLAST; }

	void	ApplyRadiusEMPEffect( CBaseEntity *pOwner, const Vector& vecCenter );
	
	static CGrenadeEMP *CGrenadeEMP::Create( const Vector &vecOrigin, const Vector &vecAngles, CBasePlayer *pOwner );

	// A derived class should return true here so that weapon sounds, etc, can
	//  apply the proper filter
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetThrower() &&
			GetThrower() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

	TimedEvent		m_ParticleEvent;

	virtual int			DrawModel( int flags );

#endif

private:
	CNetworkHandle( CSprite, m_hLiveSprite );

private:
	CGrenadeEMP( const CGrenadeEMP & );
};

#endif // GRENADE_EMP_H
