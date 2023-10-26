#include "cbase.h"
#include "asw_bloodhound.h"
#include "beam_shared.h"
#include "rope.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_broadcast_camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define BLOODHOUND_MODEL "models/swarmprops/Vehicles/BloodhoundMesh.mdl"

LINK_ENTITY_TO_CLASS( asw_bloodhound, CASW_Bloodhound );
PRECACHE_REGISTER( asw_bloodhound );

BEGIN_DATADESC( CASW_Bloodhound )
	DEFINE_THINKFUNC( BloodhoundThink ),
	DEFINE_THINKFUNC( SleepThink ),

	DEFINE_FIELD(m_hDestination, FIELD_EHANDLE),
	DEFINE_FIELD(m_fLastThinkTime, FIELD_TIME),
	DEFINE_FIELD(m_iState, FIELD_INTEGER),
	DEFINE_FIELD(m_vecCurrentVelocity, FIELD_VECTOR),
	DEFINE_FIELD(m_vecDestination, FIELD_VECTOR),
	DEFINE_FIELD(m_vecRandomDestinationOffset, FIELD_VECTOR),
	DEFINE_FIELD(m_vecStartingPosition, FIELD_VECTOR),
	DEFINE_FIELD(m_vecCurrentSway, FIELD_VECTOR),
	DEFINE_FIELD(m_fNextRandomDestTime, FIELD_FLOAT),
	DEFINE_FIELD(m_hDropshipRope, FIELD_EHANDLE),
	//DEFINE_SOUNDPATCH( m_pEngineSound ),
END_DATADESC()

ConVar asw_bloodhound_max_speed("asw_bloodhound_max_speed", "600", FCVAR_CHEAT, "Max speed of dropship");
ConVar asw_bloodhound_thrust("asw_bloodhound_thrust", "1200", FCVAR_CHEAT, "Change in velocity per second");
ConVar asw_bloodhound_tolerance("asw_bloodhound_tolerance", "50", FCVAR_CHEAT, "Distance dropship needs to be to reach destination");
ConVar asw_bloodhound_near_distance("asw_bloodhound_near_distance", "500", FCVAR_CHEAT, "How near dropship needs to be to dest to start reducing thrust");
ConVar asw_bloodhound_drag("asw_bloodhound_drag", "0.9", FCVAR_CHEAT, "Scalar applied to velocity every 0.1 seconds");
ConVar asw_bloodhound_randomness("asw_bloodhound_randomness", "10", FCVAR_CHEAT, "Randomness applied to hovering destinations");
ConVar asw_bloodhound_random_interval("asw_bloodhound_random_interval", "1", FCVAR_CHEAT, "Time between changing the random destination");
ConVar asw_bloodhound_sway_speed("asw_bloodhound_sway_speed", "1.0", FCVAR_CHEAT, "How quickly the dropship sways side to side");
ConVar asw_bloodhound_drop_interval("asw_bloodhound_drop_interval", "3.0", FCVAR_CHEAT, "Interval between dropping off marines");
extern ConVar sv_gravity;

CASW_Bloodhound::CASW_Bloodhound()
{
	m_iState = ASW_BLOODHOUND_NONE;
	m_fSwayTime[0] = 0;
	m_fSwayTime[1] = 0;
	m_fSwayTime[2] = 0;
	m_fNextMarineDropTime = 0;
	m_iMarinesDropped = 0;
	m_bPlayedInPositionLine = false;
	for (int i=0;i<ASW_MAX_DROP_MARINES;i++)
	{
		m_hMarines[i] = NULL;
	}
	//m_pEngineSound = NULL;
}

CASW_Bloodhound::~CASW_Bloodhound()
{
}

void CASW_Bloodhound::Spawn()
{
	BaseClass::Spawn();
	Precache();
	SetModel( BLOODHOUND_MODEL );

	SetMoveType( MOVETYPE_STEP );
	m_vecStartingPosition = GetAbsOrigin();

	SetState(ASW_BLOODHOUND_NONE);

	m_hDestination = gEntList.FindEntityByName( NULL, "BloodhoundDestination" );

	StartEngineSound();
}

void CASW_Bloodhound::Precache()
{
	BaseClass::Precache();

	PrecacheModel( BLOODHOUND_MODEL );
	//PrecacheScriptSound( "wildcat.FT1MoveIt" );
	//PrecacheScriptSound( "wildcat.FT1Clear" );
	//PrecacheScriptSound( "Bloodhound.EngineLoop" );
}

void CASW_Bloodhound::SleepThink()
{
	// just sleeping, waiting for mission to start
	SetNextThink( gpGlobals->curtime + 2.0f);
}

void CASW_Bloodhound::SetState(int iNewState)
{
	m_iState = iNewState;

	if (iNewState == ASW_BLOODHOUND_NONE)
	{
		SetThink( &CASW_Bloodhound::SleepThink );
		SetNextThink( gpGlobals->curtime + 2.0f );
	}
	else if (iNewState == ASW_BLOODHOUND_DESCENDING || iNewState == ASW_BLOODHOUND_MOVING_TO_DROP_POSITION
			|| iNewState == ASW_BLOODHOUND_UNLOADING)
	{
		SetThink( &CASW_Bloodhound::BloodhoundThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}

	if (iNewState == ASW_BLOODHOUND_UNLOADING)
	{
		//EmitSound( "wildcat.FT1MoveIt" );
		m_fNextMarineDropTime = gpGlobals->curtime + 3.0f;
		/*
		CBaseEntity *pAnchor = CreateEntityByName( "asw_dropship_rope" );
		pAnchor->SetOwnerEntity( this );
		pAnchor->SetAbsOrigin( GetAbsOrigin() );
		pAnchor->Spawn();
		m_hDropshipRope = dynamic_cast<CDropshipRope*>(pAnchor);
		*/
	}
}

void CASW_Bloodhound::DropMarine(int i)
{
	Msg("DropMarine %d\n", i);
	m_fNextMarineDropTime = 0;
	if (i < 0 || i >= ASW_MAX_DROP_MARINES)	
		return;
	CASW_Marine *pMarine = m_hMarines[i].Get();
	if (pMarine)
	{
		// detach him from his commander
		if (pMarine->GetCommander())
		{
			pMarine->GetCommander()->LeaveMarines();
		}
		// put him underneath us
		QAngle angFacing(0,180,0);
		pMarine->Teleport(&GetAbsOrigin(), &angFacing, &vec3_origin);
		pMarine->AddFlag(FL_FLY);
		pMarine->BeginRappel();

		m_fNextMarineDropTime = gpGlobals->curtime + asw_bloodhound_drop_interval.GetFloat();

		m_iMarinesDropped++;

		if (m_iMarinesDropped == 2)	// add a bit more time after wildcat's dropped
		{
			//m_fNextMarineDropTime += 2;
		}
	}
}

void CASW_Bloodhound::MarineLanded(CASW_Marine *pMarine)
{
	if (!pMarine)
		return;

	// find which of our marines this is
	int iMarineIndex = -1;
	for (int i=0;i<ASW_MAX_DROP_MARINES;i++)
	{
		if (m_hMarines[i].Get() == pMarine)
		{
			iMarineIndex = i;
			break;
		}
	}
	if (iMarineIndex == -1)
		return;

	// find this marine's orders spot
	char buffer[24];
	Q_snprintf(buffer, sizeof(buffer), "marine_deploy_%d", iMarineIndex+1);

	CBaseEntity *pDest = gEntList.FindEntityByName( NULL, buffer );
	if (!pDest)
	{
		Msg("Error, couldn't find deploy spot for marine %d\n", iMarineIndex);
		return;
	}

	Vector vecPos = pDest->GetAbsOrigin();
	pMarine->OrdersFromPlayer(NULL, ASW_ORDER_MOVE_TO, NULL, false, pDest->GetAbsAngles().y, &vecPos);
}

void CASW_Bloodhound::StartDropshipSequence()
{
	// find marines
	CASW_Marine *pMarine = NULL;
	for (int i=0;i<ASW_MAX_DROP_MARINES;i++)
	{
		pMarine = dynamic_cast<CASW_Marine*>(gEntList.FindEntityByClassname( pMarine, "asw_marine" ));
		m_hMarines[i] = pMarine;
		Msg("Set m_hMarines[i] to entindex=%d\n", m_hMarines[i].Get() ? m_hMarines[i]->entindex() : 0);
	}

	SetState(ASW_BLOODHOUND_DESCENDING);
}

void CASW_Bloodhound::BloodhoundThink()
{
	float delta = 0.1f;	// fixed interval for easy physics //gpGlobals->curtime - m_fLastThinkTime;
	
	PerformMovement(delta);	
	SetNextThink( gpGlobals->curtime + 0.1f );	
	CheckReachedDestination();

	if (gpGlobals->curtime > m_fNextMarineDropTime && m_fNextMarineDropTime != 0)
	{
		DropMarine(m_iMarinesDropped);
	}

	if (!m_bPlayedInPositionLine && m_iMarinesDropped >= 2)
	{
		if (m_hMarines[1].Get() && m_hMarines[1]->GetASWOrders() == ASW_ORDER_HOLD_POSITION
			&& m_hMarines[1]->IsCurSchedule(CASW_Marine::SCHED_ASW_HOLD_POSITION) && m_hMarines[1]->m_bOnGround)
		{
			//m_hMarines[1]->EmitSound("wildcat.FT1Clear");
			m_bPlayedInPositionLine = true;
		}
	}

	m_fLastThinkTime = gpGlobals->curtime;
}

void CASW_Bloodhound::SetDestinationPoint()
{
	if (!m_hDestination.Get())
	{
		m_vecDestination = GetAbsOrigin();
		Msg("Error, Bloodhound doesn't have a destination marker.  Place an info_target with name BloodhoundDestination" );
	}
	switch (GetState())
	{
	case ASW_BLOODHOUND_DESCENDING:
		{
			m_vecDestination = GetAbsOrigin();
			m_vecDestination.z = m_hDestination->GetAbsOrigin().z;
		}
		break;
	case ASW_BLOODHOUND_MOVING_TO_DROP_POSITION:
	case ASW_BLOODHOUND_UNLOADING:
		{
			m_vecDestination = m_hDestination->GetAbsOrigin();
		}
		break;
	default:
		{
			Msg("Error: Bloodhound not in a valid destination state\n");
		}
		break;
	}
}

Vector& CASW_Bloodhound::GetRandomDestinationOffset()
{
	if (gpGlobals->curtime > m_fNextRandomDestTime)
	{
		m_vecRandomDestinationOffset = RandomVector(-asw_bloodhound_randomness.GetFloat(), asw_bloodhound_randomness.GetFloat());
		m_fNextRandomDestTime = gpGlobals->curtime + asw_bloodhound_random_interval.GetFloat();
	}

	return m_vecRandomDestinationOffset;
}

void CASW_Bloodhound::UpdateSwayOffset(float delta)
{
	// we cycle 3 timers to provide offset for each axis
	m_fSwayTime[0] += delta * 0.7f * asw_bloodhound_sway_speed.GetFloat();
	m_fSwayTime[1] += delta * 1.0f * asw_bloodhound_sway_speed.GetFloat();
	m_fSwayTime[2] += delta * 1.3f * asw_bloodhound_sway_speed.GetFloat();

	float fSwayDist = asw_bloodhound_randomness.GetFloat();

	m_vecCurrentSway[0] = sin(m_fSwayTime[0]) * fSwayDist;
	m_vecCurrentSway[1] = sin(m_fSwayTime[1]) * fSwayDist;
	m_vecCurrentSway[2] = sin(m_fSwayTime[2]) * fSwayDist;
}

void CASW_Bloodhound::CheckReachedDestination()
{
	Vector diff = m_vecDestination - GetAbsOrigin();
	if (diff.Length() < asw_bloodhound_tolerance.GetFloat() && m_vecCurrentVelocity.Length() < 50.0f)
	{
		// reached destination
		if (GetState() == ASW_BLOODHOUND_DESCENDING)
		{
			SetState(ASW_BLOODHOUND_MOVING_TO_DROP_POSITION);
		}
		else if (GetState() == ASW_BLOODHOUND_MOVING_TO_DROP_POSITION)
		{
			SetState(ASW_BLOODHOUND_UNLOADING);
		}
	}
}

bool CASW_Bloodhound::PerformMovement(float deltatime)
{
	Vector vecNewPos = GetAbsOrigin();

	// remove sway, this is a sine wave added to our position seperate from movement	
	vecNewPos -= m_vecCurrentSway;

	// find where we want to move to
	SetDestinationPoint();
	Vector diff = m_vecDestination - vecNewPos;

	// accelerate our velocity in that direction
	Vector diff_norm = diff;
	diff_norm.NormalizeInPlace();

	Vector vecThrust = diff_norm * deltatime * asw_bloodhound_thrust.GetFloat();
	// apply less thrust as we get nearer
	float dist = diff.Length();
	if (dist < asw_bloodhound_near_distance.GetFloat())
	{
		vecThrust *= (dist / asw_bloodhound_near_distance.GetFloat());
	}
	//m_vecCurrentVelocity.z -= sv_gravity.GetFloat() * deltatime;		// apply gravity
	m_vecCurrentVelocity += vecThrust;

	// apply drag
	m_vecCurrentVelocity *= asw_bloodhound_drag.GetFloat();

	// cap velocity to our max
	float speed = m_vecCurrentVelocity.Length();
	if (speed > asw_bloodhound_max_speed.GetFloat() && speed > 0)
	{
		m_vecCurrentVelocity *= (asw_bloodhound_max_speed.GetFloat() / speed);
	}

	// calculate our new position
	vecNewPos += m_vecCurrentVelocity * deltatime;

	// update our sway
	UpdateSwayOffset(deltatime);
	vecNewPos += m_vecCurrentSway;
	
	// move us there (Bloodhound doesn't do any collision detection while moving)
	UTIL_SetOrigin(this, vecNewPos);

	return true;
}

void CASW_Bloodhound::StartEngineSound()
{
	//CPASAttenuationFilter filter( this );
	//m_pEngineSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), CHAN_STATIC, "Bloodhound.EngineLoop", ATTN_NORM );

	//CSoundEnvelopeController::GetController().Play( m_pEngineSound, 0.1, 100 );
	//if (m_pEngineSound)
	//{
		//CSoundEnvelopeController::GetController().SoundChangePitch( m_pDownloadingSound, 120, 1.0 );
		//CSoundEnvelopeController::GetController().SoundChangeVolume( m_pEngineSound, 1, 1.5 );
	//}
}

void CASW_Bloodhound::StopLoopingSounds(void)
{
	BaseClass::StopLoopingSounds();
	
	//CSoundEnvelopeController::GetController().SoundDestroy( m_pEngineSound );
	//m_pEngineSound = NULL;
}

void CASW_Bloodhound::ResetDropship()
{
	QAngle angFacing(0,0,0);
	Teleport(&m_vecStartingPosition, &angFacing, &vec3_origin);
	m_vecCurrentVelocity = vec3_origin;
	SetState(ASW_BLOODHOUND_NONE);
	m_iMarinesDropped = 0;
	StartDropshipSequence();
}

void asw_bloodhound_reset_f()
{
	CASW_Bloodhound *pDropship = dynamic_cast<CASW_Bloodhound*>(gEntList.FindEntityByClassname( NULL, "asw_bloodhound" ));
	if (pDropship)
		pDropship->ResetDropship();	
	CASW_Broadcast_Camera *pCamera = dynamic_cast<CASW_Broadcast_Camera*>(gEntList.FindEntityByClassname( NULL, "asw_broadcast_camera" ));
	if (pCamera)
	{
		CBaseEntity *pPath = gEntList.FindEntityByClassname( NULL, "Dropship_pc_01" );
		if (pPath)
		{
			pCamera->Teleport(&pPath->GetAbsOrigin(), &pPath->GetAbsAngles(), &vec3_origin);
		}

		pCamera->Enable();

	}
}

ConCommand asw_bloodhound_reset( "asw_bloodhound_reset", asw_bloodhound_reset_f, "Resets dropship sequence", FCVAR_CHEAT );

//=========================================================
//=========================================================

BEGIN_DATADESC( CASWRopeAnchor )
	DEFINE_FIELD( m_hRope, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( FallThink ),
	DEFINE_THINKFUNC( RemoveThink ),
END_DATADESC();

LINK_ENTITY_TO_CLASS( asw_rope_anchor, CASWRopeAnchor );

#define RAPPEL_ROPE_WIDTH 1
void CASWRopeAnchor::Spawn()
{
	BaseClass::Spawn();

	// Decent enough default in case something happens to our owner!
	float flDist = 384;

	if( GetOwnerEntity() )
	{
		flDist = fabs( GetOwnerEntity()->GetAbsOrigin().z - GetAbsOrigin().z );
	}

	m_hRope = CRopeKeyframe::CreateWithSecondPointDetached( this, -1, flDist, RAPPEL_ROPE_WIDTH, "cable/cable.vmt", 5, true );

	ASSERT( m_hRope != NULL );

	SetThink( &CASWRopeAnchor::FallThink );
	SetNextThink( gpGlobals->curtime + 0.2 );
}

void CASWRopeAnchor::FallThink()
{
	SetMoveType( MOVETYPE_FLYGRAVITY );

	Vector vecVelocity = GetAbsVelocity();

	vecVelocity.x = random->RandomFloat( -30.0f, 30.0f );
	vecVelocity.y = random->RandomFloat( -30.0f, 30.0f );

	SetAbsVelocity( vecVelocity );

	SetThink( &CASWRopeAnchor::RemoveThink );
	SetNextThink( gpGlobals->curtime + 3.0 );
}

void CASWRopeAnchor::RemoveThink()
{
	UTIL_Remove( m_hRope );	
	SetThink( &CASWRopeAnchor::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

//=========================================================
//=========================================================