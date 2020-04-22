//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_SHIELD_H
#define TF_SHIELD_H

#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "utlvector.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class Vector;
class CGameTrace;
typedef CGameTrace trace_t;
struct edict_t;


//-----------------------------------------------------------------------------
// Base class for shield entities
//-----------------------------------------------------------------------------
class CShield : public CBaseEntity
{
public:
	DECLARE_CLASS( CShield, CBaseEntity );
	DECLARE_SERVERCLASS();

	CShield();
	~CShield();

public:
	void Precache();
	void Spawn( void );

	virtual void Think( void );

	// Sets the desired center direction
	virtual void SetCenterAngles( const QAngle &angles ) {}
	virtual void SetAlwaysOrient( bool bOrient ) {}
	virtual bool IsAlwaysOrienting( ) { return false; }

	// Used by the mobile shield.
		
		// Change the angular spring constant. This affects how fast the shield rotates to face the angles
		// given in SetAngles. Higher numbers are more responsive, but if you go too high (around 40), it will
		// jump past the specified angles and wiggle a little bit.
		virtual void SetAngularSpringConstant( float flConstant ) {}

		// Move the shield out a certain amount.
		virtual void SetFrontDistance( float flDistance ) {}

	// Called when we hit something that we deflect...
	void RegisterDeflection(const Vector& vecDir, int bitsDamageType, trace_t *ptr);

	// Called when we hit something that we let through...
	void RegisterPassThru(const Vector& vecDir, int bitsDamageType, trace_t *ptr);

	// Activates/deactivates a shield for collision purposes
	void ActivateCollisions( bool activate );

	// Does this shield protect from a particular damage type?
	float ProtectionAmount( int weaponType ) const;

	// Deactivates all shields of players on a particular team
	// If you don't specify a team, it'll affect all shields
	static void ActivateShields( bool activate, int team = -1 );

	// For collision testing
	bool TestCollision( const Ray_t& ray, unsigned int mask, trace_t& trace );

	// Called when the shield has moved
	virtual void ShieldMoved() {}

	// Called when the shield is EMPed (or de-EMPed)
	virtual void SetEMPed( bool isEmped );

	// Indicates the visual center of the shape, may not be the actual center
	// (best example is the dome: it projects from a point which is not
	// at the center of the hemisphere).
	virtual void SetGeometryOffset( const Vector& vector ) {};

	// Shield power & recharging
	void	SetupRecharge( float flPower, float flDelay, float flAmount, float flTickTime );
	float	GetPower( void ) { return m_flPower; }
	void	SetPower( float flPower );
	// Make the shield recharge it's health
	void	ShieldRechargeThink( void );

	// Is this ray blocked by any shields?
	static bool IsBlockedByShields( const Vector& src, const Vector& end );

	virtual void SetOwnerEntity( CBaseEntity *pOwner );
	
	virtual void SetThetaPhi( float flTheta, float flPhi ) { }
	virtual void SetAttachmentIndex( int nAttachmentIndex ) {}

protected:
	//
	// derived classes must implement these
	//
	virtual int Width() { return 0; }
	virtual int Height() { return 0; }
	virtual bool IsPanelActive( int x, int y ) { return true; }
	virtual const Vector& GetPoint( int x, int y ) { return vec3_origin; }
	bool IsEMPed() const { return m_bIsEMPed; }

	float			m_flPower;
	float			m_flMaxPower;
	CNetworkVar( float, m_flPowerLevel );		// m_flPower mapped to 0->1 range for networking
	float			m_flRechargeDelay;
	float			m_flRechargeAmount;
	float			m_flRechargeTime;
	float			m_flNextRechargeTime;

private:
	int				m_iBuckshotHitsThisFrame;
	float			m_flLastProbeTime;
	CNetworkVar( bool, m_bIsEMPed );
	CNetworkVar( int, m_nOwningPlayerIndex );

	// List of all active shields
	static CUtlVector< CShield* >	s_Shields;
};


//-----------------------------------------------------------------------------
// Class factory methods to create the various versions of the shield
//-----------------------------------------------------------------------------
CShield* CreateMobileShield( CBaseEntity *owner, float flFrontDistance = 0 );


//-----------------------------------------------------------------------------
// Returns true if the entity is a shield
//-----------------------------------------------------------------------------
bool IsShield( CBaseEntity *pEnt );

#endif // TF_SHIELD_H
