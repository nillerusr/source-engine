//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPC_BUG_BUILDER_H
#define NPC_BUG_BUILDER_H
#ifdef _WIN32
#pragma once
#endif

#include "AI_BaseNPC.h"

#define	BUG_BUILDER_MODEL		"models/npcs/bugs/bug_builder.mdl"

class CMaker_BugHole;

//-----------------------------------------------------------------------------
// Purpose: BUILDER BUG
//-----------------------------------------------------------------------------
class CNPC_Bug_Builder : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Bug_Builder, CAI_BaseNPC );
public:
	CNPC_Bug_Builder( void );

	virtual void Spawn( void );
	virtual void Precache( void );

	virtual int SelectSchedule( void );
	virtual void StartTask( const Task_t *pTask );
	virtual void RunTask( const Task_t *pTask );

	virtual float MaxYawSpeed( void );
	virtual void HandleAnimEvent( animevent_t *pEvent );
	virtual void IdleSound( void );
	virtual void PainSound( const CTakeDamageInfo &info );
	virtual void AlertSound( void );
	virtual bool FValidateHintType(CAI_Hint *pHint);

	virtual bool ShouldPlayIdleSound( void );

	virtual Class_T	Classify( void ) { return CLASS_ANTLION; }
	virtual int	GetSoundInterests( void ) { return 0; }

	DECLARE_DATADESC();

	// BugHole handling
	void	SetBugHole( CMaker_BugHole *pBugHole );
	void	ReturnToBugHole( void );

private:
	virtual Disposition_t	IRelationType( CBaseEntity *pTarget );

	void	Event_Killed( const CTakeDamageInfo &info );

	float	m_flIdleDelay;

	float	m_flNextDawdle;

	// BugHole handling
	CHandle< CMaker_BugHole >	m_hMyBugHole;

	DEFINE_CUSTOM_AI;
};

#endif // NPC_BUG_BUILDER_H
