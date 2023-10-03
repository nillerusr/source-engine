//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ====
//
// Purpose:
//
//=============================================================================

#ifndef LOGIC_PLAYERPROXY_H
#define LOGIC_PLAYERPROXY_H
#pragma once

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the player to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicPlayerProxy : public CLogicalEntity
{
	DECLARE_CLASS( CLogicPlayerProxy, CLogicalEntity );
	DECLARE_DATADESC();

public:
	// FIXME: Subclass


#if defined( HL2_DLL )
	COutputEvent m_OnFlashlightOn;
	COutputEvent m_OnFlashlightOff;
	COutputEvent m_PlayerMissedAR2AltFire; // Player fired a combine ball which did not dissolve any enemies. 
#endif // HL2_DLL

	COutputEvent m_PlayerHasAmmo;
	COutputEvent m_PlayerHasNoAmmo;
	COutputEvent m_PlayerDied;

	COutputInt m_RequestedPlayerHealth;

#if defined HL2_EPISODIC
	void InputSetFlashlightSlowDrain( inputdata_t &inputdata );
	void InputSetFlashlightNormalDrain( inputdata_t &inputdata );
	void InputLowerWeapon( inputdata_t &inputdata );
	void InputSetLocatorTargetEntity( inputdata_t &inputdata );
#endif // HL2_EPISODIC

	void InputRequestPlayerHealth( inputdata_t &inputdata );
	void InputSetPlayerHealth( inputdata_t &inputdata );
	void InputRequestAmmoState( inputdata_t &inputdata );
	void InputEnableCappedPhysicsDamage( inputdata_t &inputdata );
	void InputDisableCappedPhysicsDamage( inputdata_t &inputdata );

	void Activate( void );

	bool PassesDamageFilter( const CTakeDamageInfo &info );

	EHANDLE m_hPlayer;
};


#endif	// LOGIC_PLAYERPROXY_H