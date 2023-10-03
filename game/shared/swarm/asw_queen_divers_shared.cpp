#include "cbase.h"
#ifdef CLIENT_DLL
	#include "iasw_client_aim_target.h"
	#define CBaseAnimating C_BaseAnimating
	#define CASW_Queen_Divers C_ASW_Queen_Divers
#else
	#include "EntityFlame.h"	
	#include "asw_util_shared.h"	
#endif
#include "asw_fx_shared.h"
#include "asw_queen_divers_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_GRUB_SAC_BURST_DISTANCE 180.0f

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Queen_Divers, DT_ASW_Queen_Divers )

BEGIN_NETWORK_TABLE( CASW_Queen_Divers, DT_ASW_Queen_Divers )
#ifdef CLIENT_DLL
	
#else
	
#endif
END_NETWORK_TABLE()

#ifdef GAME_DLL
LINK_ENTITY_TO_CLASS( asw_queen_divers, CASW_Queen_Divers );
PRECACHE_WEAPON_REGISTER(asw_queen_divers);
#endif

#define ASW_QUEEN_DIVER_MODEL "models/swarm/Queen/QueenDivers.mdl"

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Queen_Divers )	
	DEFINE_FIELD( m_hQueen, FIELD_EHANDLE ),	
	DEFINE_THINKFUNC( AnimThink ),
END_DATADESC()

#endif // not client

CASW_Queen_Divers::CASW_Queen_Divers()
{

}


CASW_Queen_Divers::~CASW_Queen_Divers()
{

}

#ifndef CLIENT_DLL

void CASW_Queen_Divers::Spawn()
{
	BaseClass::Spawn();

	Precache();
	SetModel( ASW_QUEEN_DIVER_MODEL );
	SetMoveType( MOVETYPE_NONE );
	//SetSolid(SOLID_VPHYSICS);
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	m_takedamage = DAMAGE_YES;
	m_iHealth = 100;
	m_iMaxHealth = m_iHealth;
	int iIdleSeq = LookupSequence("idle");
	SetSequence(iIdleSeq);
	AddEffects(EF_NOSHADOW);
	SetVisible(false);	// stays hidden until we start a diver attack
	//VPhysicsInitStatic();	

	SetCollisionBounds(	Vector(-32, -100, -5), Vector(32, 100, 72) );

	SetThink( &CASW_Queen_Divers::AnimThink );
	SetNextThink( gpGlobals->curtime + 0.1f);
}

void CASW_Queen_Divers::Precache()
{
	PrecacheModel( ASW_QUEEN_DIVER_MODEL );
	
	BaseClass::Precache();
}

CASW_Queen_Divers *CASW_Queen_Divers::Create_Queen_Divers( CASW_Queen *pQueen )
{
	if (!pQueen)
		return NULL;

	CASW_Queen_Divers *pDivers = (CASW_Queen_Divers *)CreateEntityByName( "asw_queen_divers" );	
	pDivers->Spawn();
	pDivers->SetOwnerEntity( pQueen );
	pDivers->m_hQueen = pQueen;
	Vector vecQueen = pQueen->GetAbsOrigin();
	vecQueen.z -= 20.0f;	// put the divers slightly lower so they don't poke up from the ground when curling
	pDivers->SetAbsOrigin(vecQueen);
	pDivers->SetAbsAngles(pQueen->GetAbsAngles());
	pDivers->SetAbsVelocity( vec3_origin );
	pDivers->SetParent(pQueen);

	return pDivers;
}

void CASW_Queen_Divers::SetBurrowing(bool bBurrowing)
{	
	if (bBurrowing)
	{
		SetSolid(SOLID_BBOX);
		SetVisible(true);
		int iBurrowSeq = LookupSequence("divers_in");
		if (GetSequence() != iBurrowSeq)
			ResetSequence(iBurrowSeq);
	}	
	else
	{
		SetSolid(SOLID_NONE);		
		int iUnburrowSeq = LookupSequence("divers_out");
		if (GetSequence() != iUnburrowSeq)
			ResetSequence(iUnburrowSeq);
	}
}

void CASW_Queen_Divers::SetVisible(bool bVisible)
{
	if (bVisible)
		RemoveEffects( EF_NODRAW );
	else
		AddEffects( EF_NODRAW );
}

int CASW_Queen_Divers::OnTakeDamage( const CTakeDamageInfo &info )
{
	// remove damage passing up, queen can be damaged just by shooting her
	// pass the damage up to our queen
	//  todo: make it so the queen can tell the damage is from us?
	//Msg("DIVERS HIT!\n");
	//if (m_hQueen.Get())
	//{
		//CTakeDamageInfo newInfo(info);
		//newInfo.SetInflictor(this);
		//if (m_hQueen->m_iDiverState >= ASW_QUEEN_DIVER_PLUNGING && m_hQueen->m_iDiverState <=ASW_QUEEN_DIVER_RETRACTING)
			//m_hQueen->OnTakeDamage(newInfo);
		//SpawnBlood( info.GetDamagePosition(), vecDir, BloodColor(), subInfo.GetDamage() );// a little surface blood.
		
		//TraceBleed( subInfo.GetDamage(), vecDir, ptr, subInfo.GetDamageType() );
	//}
	return 0;
}

void CASW_Queen_Divers::AnimThink( void )
{	
	SetNextThink( gpGlobals->curtime + 0.1f );
	
	StudioFrameAdvance();
}

void CASW_Queen_Divers::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( m_takedamage == DAMAGE_NO )
		return;

	CTakeDamageInfo subInfo = info;

	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK )
		 && !(subInfo.GetDamageType() & DMG_BURN ))
	{
		
		UTIL_ASW_DroneBleed( ptr->endpos, vecDir, 4 );
				
		ASWTraceBleed( subInfo.GetDamage(), vecDir, ptr, subInfo.GetDamageType() );
	}

	if( info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetInflictor() );
	}
	else
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

	AddMultiDamage( subInfo, this );
}

void CASW_Queen_Divers::ASWTraceBleed( float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType )
{
	if ((BloodColor() == DONT_BLEED) || (BloodColor() == BLOOD_COLOR_MECH))
	{
		return;
	}

	if (flDamage == 0)
		return;

	if (! (bitsDamageType & (DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_AIRBOAT)))
		return;

	// make blood decal on the wall!
	trace_t Bloodtr;
	Vector vecTraceDir;
	float flNoise;
	int cCount;
	int i;

#ifdef GAME_DLL
	if ( !IsAlive() )
	{
		// dealing with a dead npc.
		if ( GetMaxHealth() <= 0 )
		{
			// no blood decal for a npc that has already decalled its limit.
			return;
		}
		else
		{
			m_iMaxHealth -= 1;
		}
	}
#endif

	if (flDamage < 10)
	{
		flNoise = 0.1;
		cCount = 1;
	}
	else if (flDamage < 25)
	{
		flNoise = 0.2;
		cCount = 2;
	}
	else
	{
		flNoise = 0.3;
		cCount = 4;
	}

	float flTraceDist = (bitsDamageType & DMG_AIRBOAT) ? 384 : 172;
	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir * -1;// trace in the opposite direction the shot came from (the direction the shot is going)

		vecTraceDir.x += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.y += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.z += random->RandomFloat( -flNoise, flNoise );

		// Don't bleed on grates.
		Vector vecEndPos = ptr->endpos;
		AI_TraceLine( vecEndPos, vecEndPos + vecTraceDir * -flTraceDist, MASK_SOLID_BRUSHONLY & ~CONTENTS_GRATE, this, COLLISION_GROUP_NONE, &Bloodtr);

		if ( Bloodtr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}

// don't allow us to be hurt by allies
bool CASW_Queen_Divers::PassesDamageFilter( const CTakeDamageInfo &info )
{
	if (info.GetAttacker())
	{
		if ( IsAlienClass( info.GetAttacker()->Classify() ) )
			return false;
	}
	return BaseClass::PassesDamageFilter(info);
}

#endif

int CASW_Queen_Divers::BloodColor()
{
	return BLOOD_COLOR_BRIGHTGREEN; 
}