//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef NPC_GARGANTUA_H
#define NPC_GARGANTUA_H

#include "hl1_ai_basenpc.h"

class CNPC_Gargantua : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Gargantua, CHL1BaseNPC );
public:

	void Spawn( void );
	void Precache( void );
	Class_T  Classify ( void );
	
	float MaxYawSpeed ( void );

	int MeleeAttack1Conditions( float flDot, float flDist );
	int MeleeAttack2Conditions( float flDot, float flDist );
	int RangeAttack1Conditions( float flDot, float flDist );

	void HandleAnimEvent( animevent_t *pEvent );
	int  TranslateSchedule( int scheduleType );

	void StartTask( const Task_t *pTask );
	void RunTask ( const Task_t *pTask );

	bool CanBecomeRagdoll() { return false; }

	void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	int		OnTakeDamage_Alive( const CTakeDamageInfo &info );
		
/*	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	
	Schedule_t *GetScheduleOfType( int Type );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
*/
	void PrescheduleThink( void );
	void Event_Killed( const CTakeDamageInfo &info );
	void DeathEffect( void );

	bool ShouldGib( const CTakeDamageInfo &info );

	void EyeOff( void );
	void EyeOn( int level );
	void EyeUpdate( void );
//	void Leap( void );
	void StompAttack( void );
	void FlameCreate( void );
	void FlameUpdate( void );
	void FlameControls( float angleX, float angleY );
	void FlameDestroy( void );
	inline BOOL FlameIsOn( void ) { return m_pFlame[0] != NULL; }

	void FlameDamage( Vector vecStart, Vector vecEnd, CBaseEntity *pevInflictor, CBaseEntity *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType );


	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

private:
	CBaseEntity* GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType);

	CSprite		*m_pEyeGlow;		// Glow around the eyes
	CBeam		*m_pFlame[4];		// Flame beams

	int			m_eyeBrightness;	// Brightness target
	float		m_seeTime;			// Time to attack (when I see the enemy, I set this)
	float		m_flameTime;		// Time of next flame attack
	float		m_painSoundTime;	// Time of next pain sound
	float		m_streakTime;		// streak timer (don't send too many)
	float		m_flameX;			// Flame thrower aim
	float		m_flameY;
	
	float		m_flDmgTime;
};

#endif //NPC_GARGANTUA_H