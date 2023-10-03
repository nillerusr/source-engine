#include "cbase.h"
#ifdef CLIENT_DLL
	#include "iasw_client_aim_target.h"
	#define CBaseAnimating C_BaseAnimating
	#define CASW_Queen_Grabber C_ASW_Queen_Grabber
#else
	#include "EntityFlame.h"
	#include "asw_queen.h"
	#include "asw_util_shared.h"	
	#include "asw_marine.h"
	#include "antlion_dust.h"
#endif
#include "asw_fx_shared.h"
#include "asw_queen_grabber_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_GRUB_SAC_BURST_DISTANCE 180.0f

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Queen_Grabber, DT_ASW_Queen_Grabber )

BEGIN_NETWORK_TABLE( CASW_Queen_Grabber, DT_ASW_Queen_Grabber )
#ifdef CLIENT_DLL
	
#else
	
#endif
END_NETWORK_TABLE()

#ifdef GAME_DLL
LINK_ENTITY_TO_CLASS( asw_queen_grabber, CASW_Queen_Grabber );
PRECACHE_WEAPON_REGISTER(asw_queen_grabber);
#endif

#define ASW_QUEEN_GRABBER_MODEL "models/swarm/Queen/QueenGrabber.mdl"

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Queen_Grabber )	
	DEFINE_FIELD( m_hQueen, FIELD_EHANDLE ),	
	DEFINE_FIELD( m_hPrimary, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_hChildGrabber, FIELD_EHANDLE ),	
	DEFINE_THINKFUNC( GrabberThink ),
	DEFINE_FIELD( m_fMaxChasingTime, FIELD_FLOAT ),
END_DATADESC()

ConVar asw_queen_grabber_health("asw_queen_grabber_health", "20", FCVAR_CHEAT, "Initial health of the queen grabbers");
ConVar asw_queen_grabber_dmg("asw_queen_grabber_dmg", "1", FCVAR_CHEAT, "Damager per pump per grabber");

#endif // not client

CASW_Queen_Grabber::CASW_Queen_Grabber()
{
#ifndef CLIENT_DLL
	m_bFinishedRetractAnim = false;
	m_bPrimaryGrabber = false;
	m_fGrabbingEndTime = 0;
	m_bWantsRetractAnim = false;
#endif
}


CASW_Queen_Grabber::~CASW_Queen_Grabber()
{
}

#ifndef CLIENT_DLL

void CASW_Queen_Grabber::Spawn()
{
	BaseClass::Spawn();

	Precache();
	SetModel( ASW_QUEEN_GRABBER_MODEL );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	m_takedamage = DAMAGE_YES;
	m_iHealth = asw_queen_grabber_health.GetInt();
	m_iMaxHealth = m_iHealth;
	int iIdleSeq = LookupSequence("idle");
	ResetSequence(iIdleSeq);

	m_fDiverSpeed = ASW_DIVER_SPEED;

	SetThink( &CASW_Queen_Grabber::GrabberThink );
	SetNextThink( gpGlobals->curtime + 0.1f);
}

bool CASW_Queen_Grabber::CreateVPhysics()
{
	if ( !VPhysicsGetObject() )
	{
		SetupVPhysicsHull();
	}
	return true;
}

void CASW_Queen_Grabber::SetupVPhysicsHull()
{
	if ( VPhysicsGetObject() )
	{
		// Disable collisions to get 
		VPhysicsGetObject()->EnableCollisions(false);
		VPhysicsDestroyObject();
	}
	VPhysicsInitShadow( true, false );
}

void CASW_Queen_Grabber::Precache()
{
	PrecacheModel( ASW_QUEEN_GRABBER_MODEL );
	
	//PrecacheScriptSound("ASW_Grabber.DigLoop");
	//PrecacheScriptSound("ASW_Grabber.GrabLoop");

	BaseClass::Precache();
}

CASW_Queen_Grabber *CASW_Queen_Grabber::Create_Queen_Grabber( CASW_Queen *pQueen, const Vector &pos, const QAngle &ang )
{
	if (!pQueen)
		return NULL;

	CASW_Queen_Grabber *pGrabber = (CASW_Queen_Grabber *)CreateEntityByName( "asw_queen_grabber" );	
	pGrabber->Spawn();
	pGrabber->SetOwnerEntity( pQueen );
	pGrabber->m_hQueen = pQueen;
	pGrabber->SetAbsOrigin( pos );
	pGrabber->SetAbsAngles( ang );	
	pGrabber->SetAbsVelocity( vec3_origin );

	return pGrabber;
}

void CASW_Queen_Grabber::StartDigLoopSound()
{	
	//EmitSound( "ASW_Grabber.DigLoop" );
}

void CASW_Queen_Grabber::StopDigLoopSound()
{
	//StopSound( "ASW_Grabber.DigLoop" );	
}

void CASW_Queen_Grabber::StartGrabLoopSound()
{	
	//EmitSound( "ASW_Grabber.GrabLoop" );
}

void CASW_Queen_Grabber::StopGrabLoopSound()
{
	//StopSound( "ASW_Grabber.GrabLoop" );	
}

void CASW_Queen_Grabber::StopLoopingSounds()
{
	BaseClass::StopLoopingSounds();

	//StopSound( "ASW_Grabber.GrabLoop" );
	//StopSound( "ASW_Grabber.DigLoop" );
}

void CASW_Queen_Grabber::GrabberThink( void )
{		
	if (m_hQueen.Get())
	{
		switch (m_hQueen->m_iDiverState)
		{
		case ASW_QUEEN_DIVER_CHASING:
			{
				UpdateChasing();
				break;
			}
		case ASW_QUEEN_DIVER_RETRACTING:
			{
				UpdateRetracting();
				break;
			}
		default: break;
		}
	}
	
	StudioFrameAdvance();
	SetNextThink( gpGlobals->curtime + 0.1f );
}


CBaseEntity* CASW_Queen_Grabber::GetQueenEnemy()
{
	if (m_hQueen.Get())
	{
		if (m_hQueen->m_iDiverState == ASW_QUEEN_DIVER_GRABBING)	// if we're mid grabbing, we can't change enemy
		{
			return m_hQueen->m_hGrabbingEnemy;
		}
		CBaseEntity *pEnt = m_hQueen->GetEnemy();
		if (pEnt && pEnt->GetHealth() > 0)
			return pEnt;
	}
	return NULL;
}

void CASW_Queen_Grabber::UpdateChasing()
{
	if (!GetQueen())
		return;

	if (!GetQueenEnemy() || (m_fMaxChasingTime != 0 && gpGlobals->curtime > m_fMaxChasingTime))
	{
		//Msg("Killing diver because we lost queen enemy\n");
		GetQueen()->NotifyGrabberKilled(this);	// will change us to retracting
		return;
	}

	Vector vecTowardsEnemy = GetQueenEnemy()->GetAbsOrigin() - GetAbsOrigin();
	// if enemy is too high up, abort
	if (vecTowardsEnemy.z > 60 && vecTowardsEnemy.Length2D() < 200)
	{
		//Msg("Aborting chase because marine is too high up: %f\n", vecTowardsEnemy.z);
		GetQueen()->NotifyGrabberKilled(this);	// will change us to retracting
		return;
	}
	vecTowardsEnemy.z = 0;
	float dist = vecTowardsEnemy.Length2D();
	vecTowardsEnemy.NormalizeInPlace();

	CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(GetQueenEnemy());
	if (pMarine)
	{
		if (dist < ASW_GRAB_DISTANCE)
		{
			// we're close enough, grab him!
			GetQueen()->SetDiverState(ASW_QUEEN_DIVER_GRABBING);
			// move ourselves slightly away from him and facing him			
			SetAbsAngles(QAngle(0, random->RandomInt(0,360), 0));
			if (PositionToGrab(GetQueenEnemy()))
			{
				CreatePeers();
				StartGrabbing();
			}
			else
			{
				// kill us
				Msg("Master tentacle couldn't find room to grab the enemy, so we're aborting\n");
				//ResetSequence(LookupSequence("idle"));
				//GetQueen()->NotifyGrabberKilled(this);
				AbortGrab();
			}
		}
		else
		{
			if (dist < ASW_PRE_GRAB_DISTANCE)
			{
				// we're fairly close to the marine now, tell him to stop moving				
				GetQueen()->m_hPreventMovementMarine = pMarine;
				pMarine->m_bPreventMovement = true;
			}
			else	// if we're chasing, make sure we're idling
			{
				if (GetSequence() != LookupSequence("idle"))
				{					
					ResetSequence(LookupSequence("idle"));
				}
			}

			// move nearer
			if (dist > 0)
			{
				m_fDiverSpeed += ASW_DIVER_ACCELERATION * gpGlobals->frametime;
				//Msg("m_fDiverSpeed = %f\n", m_fDiverSpeed);
				Vector vecNewPos = GetAbsOrigin() + vecTowardsEnemy * m_fDiverSpeed * gpGlobals->frametime;
				// check we didn't go past
				Vector vecDiff = pMarine->GetAbsOrigin() - vecNewPos;
				vecDiff.z = 0;
				vecDiff.NormalizeInPlace();
				if (vecDiff.Dot(vecTowardsEnemy) <= 0)
				{
					// we went past him
					vecNewPos.x = pMarine->GetAbsOrigin().x;
					vecNewPos.y = pMarine->GetAbsOrigin().y;
				}
				// make sure its on the ground level
				DropToGround(vecNewPos);

				SetAbsOrigin(vecNewPos);
				UTIL_CreateAntlionDust( vecNewPos + Vector(0,0,5), QAngle(0,0,0) );
			}
		}
	}		
}

void CASW_Queen_Grabber::UpdateRetracting()
{
	if (!GetQueen())
		return;

	if (m_bWantsRetractAnim)
	{
		m_bWantsRetractAnim = false;
		ResetSequence(LookupSequence("retract"));
		return;
	}

	if (!m_bFinishedRetractAnim)	// don't start moving back until we're inside the ground
		return;

	
	Vector vecTowardsQueen = GetQueen()->GetDiverSpot() - GetAbsOrigin();
	vecTowardsQueen.z = 0;
	float dist = vecTowardsQueen.Length2D();
	vecTowardsQueen.NormalizeInPlace();

	if (dist < ASW_RETRACT_DISTANCE)
	{
		// we're close enough, finish
		GetQueen()->SetDiverState(ASW_QUEEN_DIVER_UNPLUNGING);
		// kill us (all peers should be dead already)		
		//Msg("%f: Deleting in UpdateRetracting as we're under the retract distance\n", gpGlobals->curtime);
		UTIL_Remove(this);
		return;
	}
	else
	{			
		// move nearer
		if (dist > 0)
		{
			//m_fDiverSpeed += ASW_DIVER_ACCELERATION * gpGlobals->frametime;
			Vector vecNewPos = GetAbsOrigin() + vecTowardsQueen * m_fDiverSpeed * gpGlobals->frametime;
			
			// check we didn't go past
			Vector vecDiff = GetQueen()->GetAbsOrigin() - vecNewPos;
			vecDiff.z = 0;
			vecDiff.NormalizeInPlace();
			if (vecDiff.Dot(vecTowardsQueen) <= 0)
			{
				// we went past him
				vecNewPos.x = GetQueen()->GetAbsOrigin().x;
				vecNewPos.y = GetQueen()->GetAbsOrigin().y;				

				// we're close enough, finish
				GetQueen()->SetDiverState(ASW_QUEEN_DIVER_UNPLUNGING);
				// kill us (all peers should be dead already)		
				//Msg("Deleting in UpdateRetracting as we went past the queen?\n");
				UTIL_Remove(this);
			}
			else
			{
				// make sure its on the ground level
				DropToGround(vecNewPos);

				SetAbsOrigin(vecNewPos);
			}
			UTIL_CreateAntlionDust( vecNewPos + Vector(0,0,5), QAngle(0,0,0) );
		}
	}
}

void CASW_Queen_Grabber::DropToGround(Vector &vecPos)
{
	trace_t tr;
	UTIL_TraceLine( vecPos + Vector(0, 0, 50), vecPos - Vector(0, 0, 50), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );
	vecPos.z = tr.endpos.z;
}

CASW_Queen* CASW_Queen_Grabber::GetQueen()
{
	return m_hQueen;
}

void CASW_Queen_Grabber::CreatePeers()
{
	if (!GetQueen())
		return;
	const int num_peers = ASW_NUM_CHILD_GRABBERS;
	float angle_offset = 360.0f / (num_peers + 1);
	QAngle angGrabber = GetAbsAngles();
	angGrabber[YAW] += angle_offset;
	for (int i=0;i<num_peers;i++)
	{
		CASW_Queen_Grabber* pGrabber = CASW_Queen_Grabber::Create_Queen_Grabber(GetQueen(), GetAbsOrigin(), angGrabber);
		if (pGrabber)
		{
			GetQueen()->m_iLiveGrabbers++;
			pGrabber->SetPrimary(this);
			if (pGrabber->PositionToGrab(GetQueenEnemy()))
			{
				pGrabber->StartGrabbing();
			}
			else	// if there was no room to go up and grab, then just kill this tentacle
			{
				GetQueen()->NotifyGrabberKilled(this);
				Msg("Peer tentacle %d couldn't find room to grab, so we're deleting it\n", i);
				UTIL_Remove(this);
				return;
			}
		}
		angGrabber[YAW] += angle_offset;
		m_hChildGrabber[i] = pGrabber;
	}
}

void	CASW_Queen_Grabber::ReachedEndOfSequence()
{
	// we've finished a sequence, check what we need to do next
	if (!GetQueen())
		return;

	//if (m_bPrimaryGrabber)
		//Msg("%f CASW_Queen_Grabber::ReachedEndOfSequence %s\n", gpGlobals->curtime, GetSequenceName(GetSequence()));	

	// check for deleting us if we're dead (or have no enemy) and finished our retraction anim
	if ((GetHealth() <= 0 || !GetQueenEnemy()) && GetSequence() == LookupSequence("retract"))
	{
		if (m_bPrimaryGrabber)
		{
		}
		else
		{
			//Msg("deleting in ReachedEndOfSequence as thsi isn't a primary and we just finished retract\n");
			UTIL_Remove(this);
			return;
		}
	}

	switch (GetQueen()->m_iDiverState)
	{
	case ASW_QUEEN_DIVER_GRABBING:
		{
			if (GetHealth() >= 0 && m_takedamage != DAMAGE_NO)
			{
				//if (m_bPrimaryGrabber)
					//Msg("Setting sequence to pump in reached end\n");
				ResetSequence(LookupSequence("pump"));
				if (GetQueenEnemy() && GetQueenEnemy()->GetHealth() > 0)
				{
					// do him some damage
					CTakeDamageInfo damageinfo( this, GetQueen(),
						Vector(1,1,1), GetQueenEnemy()->GetAbsOrigin(), asw_queen_grabber_dmg.GetInt(), DMG_SLASH );

					GetQueenEnemy()->TakeDamage(damageinfo);
					
					// suicide primary grabber after a certain amount of grabbing
					if (m_bPrimaryGrabber && gpGlobals->curtime > m_fGrabbingEndTime)
					{
						AbortGrab();
					}
				}
				else	// if we've lost our enemy, or killed him, then abort the grab
				{
					AbortGrab();
				}
			}
			else
			{
				//if (m_bPrimaryGrabber)
					//Msg("Setting sequence to idle  in reached (ASW_QUEEN_DIVER_GRABBING)\n");
				ResetSequence(LookupSequence("idle"));
			}
			break;
		}
	case ASW_QUEEN_DIVER_RETRACTING:
		{
			m_bFinishedRetractAnim = true;
			//if (m_bPrimaryGrabber)
				//Msg("Setting sequence to idle in reached (ASW_QUEEN_DIVER_RETRACTING) m_bFinishedRetractAnim marked true\n");
			ResetSequence(LookupSequence("idle"));
			break;
		}
	default: break;
	}
}

int CASW_Queen_Grabber::OnTakeDamage( const CTakeDamageInfo &info )
{	
	if (info.GetDamage() > 0 && GetHealth() > 0)
	{
		// do a flinch?
		int iFlinch = LookupSequence("pain");
		if (GetSequence() != iFlinch)	// if we're not flinching already
		{
			//Msg("Making grabber flinch\n");
			//if (m_bPrimaryGrabber)
				//Msg("Setting sequence to flinch in takedamage\n");
			ResetSequence(iFlinch);	// do a flinch
		}
	}

	// pass all damage up to primary grabber
	if (!m_bPrimaryGrabber && m_hPrimary.Get() && info.GetDamage() != 1000)	// 1000 is a special amount of damage designed to kill us, so don't pass it up
	{
		//Msg("%d non primary grabber took damage, passing it up to primary\n", entindex());
		m_hPrimary->OnTakeDamage(info);
		return 0;
	}

	//if (info.GetDamage() == 1000)
		//Msg("%d grabber taking 1000 damage\n", entindex());

	int i = BaseClass::OnTakeDamage(info);
	
	return i;
}

void CASW_Queen_Grabber::Event_Killed( const CTakeDamageInfo &info )
{	
	int iDiverState = m_hQueen->m_iDiverState;
	//Msg("%d grabber killed\n", entindex());
	if (GetQueen())
	{
		GetQueen()->NotifyGrabberKilled(this);
	}
	
	if( info.GetAttacker() )
	{
		info.GetAttacker()->Event_KilledOther(this, info);
	}

	m_takedamage = DAMAGE_NO;
	m_lifeState = LIFE_DEAD;

	// put its collision back below the ground
	SetCollisionBounds(	Vector(-10, -10, -20), Vector(10, 10, -10) );

	// note: we don't delete us here
	StopGrabLoopSound();

	//if (m_bPrimaryGrabber)
		//Msg("Primary grabber killed, queen state is %d\n", m_hQueen.Get() ? iDiverState : -1);
	if (!m_hQueen.Get() ||  iDiverState == ASW_QUEEN_DIVER_CHASING)	// if we were killed while already underground, just idle and finish up
	{
		if (m_bPrimaryGrabber)
		{
			m_bFinishedRetractAnim = true;
			//Msg("Setting sequence to idle in killed\n");
			ResetSequence(LookupSequence("idle"));
		}
		else
		{
			UTIL_Remove(this);
		}
	}
	else
	{
		//if (m_bPrimaryGrabber)
			//Msg("Setting sequence to retract in killed\n");
		ResetSequence(LookupSequence("retract"));
	}

	// if we're primary, make sure all the children die too (send them a bunch of damage)
	if (m_bPrimaryGrabber)
	{
		//Msg("%d  this is primary grabber, so sending 100 damage to child grabbers\n", entindex());
		for (int i=0;i<ASW_NUM_CHILD_GRABBERS;i++)
		{
			if (m_hChildGrabber[i].Get())
			{
				CTakeDamageInfo info2(info);
				info2.SetDamage(1000);
				m_hChildGrabber[i]->OnTakeDamage(info2);
			}
		}
	}
}

void CASW_Queen_Grabber::AbortGrab()
{
	if (!m_bPrimaryGrabber || m_lifeState == LIFE_DEAD)
		return;

	if (GetQueen())
	{
		GetQueen()->NotifyGrabberKilled(this);
	}

	m_takedamage = DAMAGE_NO;
	m_lifeState = LIFE_DEAD;

	// put its collision back below the ground
	SetCollisionBounds(	Vector(-10, -10, -20), Vector(10, 10, -10) );

	// note: we don't delete us here
	StopGrabLoopSound();
	//Msg("%f: Setting sequence to retract\n", gpGlobals->curtime);
	// for some reason, whichever sequence we set here ends immediately!?
	//ResetSequence(LookupSequence("retract"));
	//ResetSequence(LookupSequence("pump"));
	m_bWantsRetractAnim = true;
	SetNextThink(gpGlobals->curtime);

	// make sure all the children die too (send them a bunch of damage)

	//Msg("%d  this is primary grabber, so sending 100 damage to child grabbers\n", entindex());
	for (int i=0;i<ASW_NUM_CHILD_GRABBERS;i++)
	{
		if (m_hChildGrabber[i].Get())
		{
			//CTakeDamageInfo info2(info);
			//info2.SetDamage(1000);
			CTakeDamageInfo info2( this, this,
							Vector(1,1,1), GetAbsOrigin(), 1000, DMG_SLASH );
			m_hChildGrabber[i]->OnTakeDamage(info2);
		}
	}

	// killing the primary grabber will cause a retract
	/*
	if (m_bPrimaryGrabber && GetHealth() > 0)
	{
		CTakeDamageInfo damageinfo2( this, this,
							Vector(1,1,1), GetAbsOrigin(), GetHealth() + 5, DMG_SLASH );
		OnTakeDamage(damageinfo2);
	}
	*/
	/*
	if (GetSequence() != LookupSequence("retract"))
	{
		SetHealth(0);
		SetCollisionBounds(	Vector(-10, -10, -20), Vector(10, 10, -10) );
		StopGrabLoopSound();
		ResetSequence(LookupSequence("retract"));
	}*/
}

void CASW_Queen_Grabber::MakePrimary()
{
	StartDigLoopSound();
	m_bPrimaryGrabber = true;
	m_hPrimary = this;
}

void CASW_Queen_Grabber::SetPrimary(CASW_Queen_Grabber *pPrimary)
{
	m_hPrimary = pPrimary;
}

void CASW_Queen_Grabber::StartGrabbing()
{
	SetCollisionBounds(	Vector(-10, -10, 0), Vector(10, 10, 100) );
	//Msg("Making grabber play grabber seq\n");
	if (m_bPrimaryGrabber)
	{
		m_fGrabbingEndTime = gpGlobals->curtime + 8.0f;
		StopDigLoopSound();
		StartGrabLoopSound();
		ResetSequence(LookupSequence("grab_short"));
		return;
	}

	float fSeq = random->RandomFloat();
	if ( fSeq < 0.4f)
		ResetSequence(LookupSequence("grab_short"));
	else if (fSeq < 0.7f)
		ResetSequence(LookupSequence("grab_1a"));
	else
		ResetSequence(LookupSequence("grab_2"));
}

int CASW_Queen_Grabber::UpdateTransmitState()
{	
	return SetTransmitState( FL_EDICT_ALWAYS );
}

bool CASW_Queen_Grabber::PositionToGrab(CBaseEntity *pEntity)
{
	if (!pEntity)
		return false;

	int k =0;
	while (!CanGrabAtThisAngle(pEntity) && k < 10)
	{
		SetAbsAngles(QAngle(0, random->RandomInt(0,360), 0));
		k++;
		if (k >= 10)
		{
			return false;
		}
	}

	const int iGrabberLength = 50;
	Vector vecForward;
	AngleVectors(GetAbsAngles(), &vecForward);
	vecForward.z = 0;
	Vector vecNewPos = pEntity->GetAbsOrigin() - vecForward * iGrabberLength;	
	vecNewPos.z = GetAbsOrigin().z;
	
	SetAbsOrigin(vecNewPos);
	return true;
}

bool CASW_Queen_Grabber::CanGrabAtThisAngle(CBaseEntity *pEntity)
{	
	const int iGrabberLength = 50;
	Vector vecForward;
	AngleVectors(GetAbsAngles(), &vecForward);
	vecForward.z = 0;
	Vector vecNewPos = pEntity->GetAbsOrigin() - vecForward * iGrabberLength;	
	vecNewPos.z = GetAbsOrigin().z;

	// check this spot is clear	
	CASWTraceFilterWorldAndPropsOnly filter;
	trace_t tr;
	//NDebugOverlay::Line(vecNewPos + Vector(0,0,100), vecNewPos - Vector(0,0,30), 255, 0, 0, true, 20.0f);
	UTIL_TraceLine(vecNewPos + Vector(0,0,100), vecNewPos - Vector(0,0,30), MASK_SOLID, &filter, &tr);
	
	if (tr.startsolid)
		return false;

	if (tr.fraction < 0.6f)
		return false;

	Vector vecMarineHeadPos = pEntity->GetAbsOrigin();
	vecMarineHeadPos.z = vecNewPos.z + 100;
	UTIL_TraceLine(vecNewPos + Vector(0,0,100), vecMarineHeadPos, MASK_SOLID, &filter, &tr);

	if (tr.startsolid)
		return false;

	if (tr.fraction < 0.8f)
		return false;

	return true;
}


void CASW_Queen_Grabber::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( m_takedamage == DAMAGE_NO )
		return;

	CTakeDamageInfo subInfo = info;

	//SetLastHitGroup( ptr->hitgroup );
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK )
		 && !(subInfo.GetDamageType() & DMG_BURN ))
	{

		if( !IsPlayer() || ( IsPlayer() && gpGlobals->maxClients > 1 ) )
		{
			// NPC's always bleed. Players only bleed in multiplayer.
			//UTIL_ASW_BloodDrips( ptr->endpos, vecDir, BloodColor(), 4 );
			UTIL_ASW_DroneBleed( ptr->endpos, vecDir, 4 );
		}
		
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

void CASW_Queen_Grabber::ASWTraceBleed( float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType )
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
bool CASW_Queen_Grabber::PassesDamageFilter( const CTakeDamageInfo &info )
{
	if (info.GetAttacker())
	{
		if ( IsAlienClass( info.GetAttacker()->Classify() ) )
			return false;
	}
	return BaseClass::PassesDamageFilter(info);
}

#endif	// not clientdll

int CASW_Queen_Grabber::BloodColor()
{
	return BLOOD_COLOR_BRIGHTGREEN; 
}