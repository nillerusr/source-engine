//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include	"cbase.h"
#include	"ai_default.h"
#include	"ai_task.h"
#include	"ai_schedule.h"
#include	"ai_node.h"
#include	"ai_hull.h"
#include	"ai_hint.h"
#include	"ai_memory.h"
#include	"ai_route.h"
#include	"ai_motor.h"
#include	"soundent.h"
#include	"game.h"
#include	"npcevent.h"
#include	"entitylist.h"
#include	"activitylist.h"
#include	"animation.h"
#include	"basecombatweapon.h"
#include	"IEffects.h"
#include	"vstdlib/random.h"
#include	"engine/IEngineSound.h"
#include	"ammodef.h"
#include	"hl1_ai_basenpc.h"

class CNPC_Bloater : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Bloater, CHL1BaseNPC );
public:

	void Spawn( void );
	void Precache( void );
	/*void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );

	void PainSound( const CTakeDamageInfo &info );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSnd( void );

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );*/
};

LINK_ENTITY_TO_CLASS( monster_bloater, CNPC_Bloater );

//=========================================================
// Spawn
//=========================================================
void CNPC_Bloater::Spawn()
{
	Precache( );

	SetModel( "models/floater.mdl");
//	UTIL_SetSize( this, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_FLY );
	m_spawnflags		|= FL_FLY;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	m_iHealth			= 40;
//	pev->view_ofs		= VEC_VIEW;// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;

	SetRenderColor( 255, 255, 255, 255 );

	NPCInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Bloater::Precache()
{
	PrecacheModel("models/floater.mdl");
}	
