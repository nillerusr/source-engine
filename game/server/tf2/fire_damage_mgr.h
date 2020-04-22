//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FIRE_DAMAGE_MGR_H
#define FIRE_DAMAGE_MGR_H
#ifdef _WIN32
#pragma once
#endif


#include "igamesystem.h"
#include "utllinkedlist.h"
#include "ehandle.h"


class CEntityBurnEffect;


// ------------------------------------------------------------------------------------------ //
// CFireDamageMgr.
//
// This class manages fire damage being applied to entities. It uses velocity, acceleration,
// and decay to model fire damage building up, and it puts a cap on the maximum amount of damage
// an entity can take from fire during a given frame.
// ------------------------------------------------------------------------------------------ //

class CFireDamageMgr : public CAutoGameSystem
{
// Overrides.
public:

	virtual bool	Init();
	virtual void	FrameUpdatePostEntityThink();


public:
	// Apply fire damage to an entity. flDamageAccel is in units per second, so in the absence of
	// decay, it equals velocity increase per second.
	//
	// NOTE: the damage acceleration should always be greater than the decay per second, or no damage
	//       will be applied since it will decay faster than 
	void		AddDamage( CBaseEntity *pTarget, CBaseEntity *pAttacker, float flDamageAccel, bool bMakeBurnEffect );


private:
	class CDamageAttacker
	{
	public:
		void	Init( CBaseEntity *pAttacker, float flVelocity )
		{
			m_hAttacker = pAttacker;
			m_flVelocity = flVelocity;
			m_flDamageSum = 0;
		}		
		
		EHANDLE	m_hAttacker;
		float	m_flVelocity;	// Current damage velocity.
		
		float	m_flDamageSum;	// Damage is summed up and applied a couple times per second instead of 
								// each frame since fractional damage is rounded to 1.
	};

	class CDamageEnt
	{
	public:
		enum
		{
			MAX_ATTACKERS = 4
		};

		bool				m_bWasAlive;

		EHANDLE				m_hEnt;
		CHandle<CEntityBurnEffect>	m_pBurnEffect;
		
		// Each attacker gets credit for a portion of 
		CDamageAttacker	m_Attackers[MAX_ATTACKERS];
		int				m_nAttackers;
	};


private:

	void		ApplyCollectedDamage( CFireDamageMgr::CDamageEnt *pEnt, int iAttacker );
	void		RemoveDamageEnt( int iEnt );


private:
	
	CUtlLinkedList<CDamageEnt,int>	m_DamageEnts;

	float	m_flMaxDamagePerSecond;
	float 	m_flDecayConstant;

	// This counts down to zero so we only apply fire damage every so often.
	float	m_flApplyDamageCountdown;
};


// Returns true if the entity is burnable by the specified team.
bool IsBurnableEnt( CBaseEntity *pEntity, int iTeam );


// This is used by the flamethrower and the burning gasoline blobs to find entities to burn.
int FindBurnableEntsInSphere(
	CBaseEntity **ents,
	float *dists,
	int nMaxEnts,
	const Vector &vecCenter,
	float flSearchRadius,
	CBaseEntity *pOwner );


CFireDamageMgr* GetFireDamageMgr();


#endif // FIRE_DAMAGE_MGR_H
