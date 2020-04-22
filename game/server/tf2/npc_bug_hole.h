//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPC_BUG_HOLE_H
#define NPC_BUG_HOLE_H
#ifdef _WIN32
#pragma once
#endif

class CNPC_Bug_Warrior;
class CNPC_Bug_Builder;

//-----------------------------------------------------------------------------
// Purpose: BUG HOLE
//-----------------------------------------------------------------------------
class CMaker_BugHole : public CNPCMaker
{
	DECLARE_CLASS( CMaker_BugHole, CNPCMaker );
public:
	CMaker_BugHole( void );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	MakeNPC( void );
	virtual	void	ChildPreSpawn( CAI_BaseNPC *pChild );
	virtual	void	ChildPostSpawn( CAI_BaseNPC *pChild );
	virtual void	DeathNotice( CBaseEntity *pVictim );
	virtual void	Event_Killed( const CTakeDamageInfo &info );

	// Bug interactions
	void	BugHoleThink( void );
	void	SpawnBug( float flTime );
	void	SpawnWarrior( float flTime );
	void	SpawnBuilder( float flTime );
	void	BugHoleUnderAttack( void );
	void	StartPatrol( void );
	void	CheckBuilder( void );
	void	IncomingFleeingBug( CAI_BaseNPC *pBug );
	void	BugReturned( void );

private:
	string_t	m_iszNPCClassname_Warrior;
	string_t	m_iszNPCClassname_Builder;

	// Bug pool
	int			m_iPool;
	int			m_iMaxPool;
	float		m_flPoolRegenTime;

	float		m_flNextSpawnTime;
	float		m_flNextRegenTime;

	// Patrols
	int			m_iMaxNumberOfPatrollers;
	float		m_flPatrolTime;
	float		m_flNextPatrolTime;
	string_t	m_iszPatrolPathName;

	// Builders
	int			m_iMaxNumberOfBuilders;

	// List of bugs I have out there
	typedef	CHandle<CNPC_Bug_Warrior> WarriorHandle_t;
	CUtlVector<WarriorHandle_t>	m_aWarriorBugs;
	typedef	CHandle<CNPC_Bug_Builder> BuilderHandle_t;
	CUtlVector<BuilderHandle_t>	m_aBuilderBugs;
};

#endif // NPC_BUG_HOLE_H
