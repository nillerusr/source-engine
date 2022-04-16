//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MONSTERMAKER_H
#define MONSTERMAKER_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"


//-----------------------------------------------------------------------------
// Spawnflags
//-----------------------------------------------------------------------------
#define	SF_NPCMAKER_START_ON	1	// start active ( if has targetname )
#define SF_NPCMAKER_NPCCLIP		8	// Children are blocked by NPCclip
#define SF_NPCMAKER_FADE		16	// Children's corpses fade
#define SF_NPCMAKER_INF_CHILD	32	// Infinite number of children
#define	SF_NPCMAKER_NO_DROP		64	// Do not adjust for the ground's position when checking for spawn


class CNPCMaker : public CBaseEntity
{
public:
	DECLARE_CLASS( CNPCMaker, CBaseEntity );

	CNPCMaker(void) {}

	void Spawn( void );
	void Precache( void );

	void MakerThink( void );
	bool CanMakeNPC( void );

	void DeathNotice( CBaseEntity *pChild );// NPC maker children use this to tell the NPC maker that they have died.
	void MakeNPC( void );

	// Input handlers
	void InputSpawnNPC( inputdata_t &inputdata );
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );
	
	// State changers
	void Toggle( void );
	void Enable( void );
	void Disable( void );

	bool IsDepleted();

	DECLARE_DATADESC();
	
	int			m_iMaxNumNPCs;			// max number of NPCs this ent can create
	float		m_flSpawnFrequency;		// delay (in secs) between spawns 
	int			m_iMaxLiveChildren;		// max number of NPCs that this maker may have out at one time.
	string_t	m_iszNPCClassname;		// classname of the NPC(s) that will be created.

	COutputEvent m_OnSpawnNPC;
	
	int		m_cLiveChildren;// how many NPCs made by this NPC maker that are currently alive

	float	m_flGround; // z coord of the ground under me, used to make sure no NPCs are under the maker when it drops a new child
	bool	m_bDisabled;
};


#endif // MONSTERMAKER_H
