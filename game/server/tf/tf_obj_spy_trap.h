//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//=============================================================================

#ifndef TF_OBJ_SPY_TRAP_H
#define TF_OBJ_SPY_TRAP_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj.h"

#ifdef STAGING_ONLY
#define SPY_TRAP_MAX_HEALTH	150

// ------------------------------------------------------------------------ //
// Base trap
// ------------------------------------------------------------------------ //
class CObjectSpyTrap : public CBaseObject
{
	DECLARE_CLASS( CObjectSpyTrap, CBaseObject );
	DECLARE_DATADESC();
public:
	//DECLARE_SERVERCLASS();

	CObjectSpyTrap();

	virtual void Spawn() OVERRIDE;
	virtual void Precache() OVERRIDE;
	virtual void SetObjectMode( int iVal ) OVERRIDE;
	virtual bool IsPlacementPosValid( void ) OVERRIDE;
	virtual void StartTouch( CBaseEntity *pOther ) OVERRIDE;
	virtual void EndTouch( CBaseEntity *pOther ) OVERRIDE;
	virtual bool ShouldCollide( int collisionGroup, int contentsMask ) const OVERRIDE;
	virtual void OnGoActive() OVERRIDE;
	virtual int	GetBaseHealth( void ) OVERRIDE { return SPY_TRAP_MAX_HEALTH; }

	void SetTrapType( int nType ) { m_nTrapMode = nType; }
	int	GetTrapType( void ) { return m_nTrapMode; }
	void SpyTrapThink();

	enum AttributeType
	{
		TRAP_TRIGGER_FRIENDLY			= 1<<0,					// Trap triggered by friendlies (a buff)
		TRAP_TRIGGER_ONBUILD			= 1<<1,					// Trap effect happens on placement
		TRAP_PULSE_EFFECT				= 1<<2,					// Trap's effect pulses on a timer
	};
	void SetAttribute( int attributeFlag );
	bool HasAttribute( int attributeFlag ) const;

private:
	void Activate( CBaseEntity *pTouchEntity );
	void Destroy( void );
	void TriggerTrapEffects( void );

	// Sapper
	void TriggerTrap_RadiusCloak( void );
	bool IsValidRadiusCloakTarget( CTFPlayer *pTarget );

	// Reprogrammer
	void TriggerTrap_Reprogrammer( CBaseEntity *pTouchEntity );

	// Magnet
	void TriggerTrap_Magnet( void );

	int m_attributeFlags;
	bool m_bActive;
	int m_nTrapMode;
	float m_flNextTrapEffectTime;
	float m_flTrapExpireTime;
	float m_flNextPulseTime;
};

inline void CObjectSpyTrap::SetAttribute( int attributeFlag )
{
	m_attributeFlags |= attributeFlag;
}

inline bool CObjectSpyTrap::HasAttribute( int attributeFlag ) const
{
	return m_attributeFlags & attributeFlag ? true : false;
}

#endif // STAGING_ONLY

#endif // TF_OBJ_SPY_TRAP_H
