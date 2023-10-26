#include "cbase.h"
#ifdef CLIENT_DLL
	#define CBaseAnimating C_BaseAnimating
	#define CASW_Remote_Turret C_ASW_Remote_Turret
	#include "c_asw_player.h"
	#include "c_asw_marine.h"
	#include "fx.h"
	#include "soundenvelope.h"
	#include "c_te_effect_dispatch.h"
	#include "c_user_message_register.h"
	#include "c_asw_fx.h"
	#define CASW_Marine C_ASW_Marine
#else
	#include "EntityFlame.h"
	#include "asw_player.h"
	#include "asw_marine.h"
	#include "ai_network.h"
	#include "ndebugoverlay.h"
#endif
#include "asw_gamerules.h"
#include "asw_util_shared.h"
#include "igamemovement.h"
#include "in_buttons.h"
#include "asw_remote_turret_shared.h"
#include "ammodef.h"
#include "fmtstr.h"
#include "effect_dispatch_data.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Remote_Turret, DT_ASW_Remote_Turret )

BEGIN_NETWORK_TABLE( CASW_Remote_Turret, DT_ASW_Remote_Turret )
#ifdef CLIENT_DLL
	RecvPropEHandle( RECVINFO( m_hUser ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),	
	RecvPropBool( RECVINFO( m_bUpsideDown ) ),
	RecvPropQAngles	(RECVINFO(m_angDefault)),
	RecvPropQAngles	(RECVINFO(m_angViewLimit)),
#else
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropEHandle( SENDINFO( m_hUser ) ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11, SPROP_CHANGES_OFTEN ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11, SPROP_CHANGES_OFTEN ),
	SendPropBool( SENDINFO( m_bUpsideDown ) ),
	SendPropQAngles	(SENDINFO(m_angDefault), 10 ),
	SendPropQAngles	(SENDINFO(m_angViewLimit), 10 ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_remote_turret, CASW_Remote_Turret );
PRECACHE_WEAPON_REGISTER(asw_remote_turret);

#define ASW_REMOTE_TURRET_MODEL "models/swarm/SentryGun/remoteturret.mdl"

ConVar asw_turret_fire_rate("asw_turret_fire_rate", "0.1f", FCVAR_CHEAT | FCVAR_REPLICATED, "Firing rate of remote controlled turrets");
ConVar asw_turret_turn_rate("asw_turret_turn_rate", "50.0f", FCVAR_CHEAT | FCVAR_REPLICATED, "Turning rate of remote controlled turrets");
extern ConVar asw_weapon_max_shooting_distance;

#ifndef CLIENT_DLL

BEGIN_DATADESC( CASW_Remote_Turret )
	DEFINE_KEYFIELD(m_bUpsideDown, FIELD_BOOLEAN, "UpsideDown"),
	DEFINE_KEYFIELD( m_angViewLimit, FIELD_VECTOR, "viewlimits" ),
END_DATADESC()

#else
ConVar asw_turret_debug_limits("asw_turret_debug_limits", "0", FCVAR_CHEAT, "Prints debug info about turret turning limits");
#endif // not client

CASW_Remote_Turret::CASW_Remote_Turret()
{
	m_angEyeAngles.Init();
	m_iAmmoType = GetAmmoDef()->Index("ASW_AG");
	m_fNextFireTime = 0;
#ifdef CLIENT_DLL
	m_pLoopingSound = NULL;
	m_bLastPlaySound = false;
#else
	m_hUser = false;
#endif	
}


CASW_Remote_Turret::~CASW_Remote_Turret()
{

}

#ifndef CLIENT_DLL

void CASW_Remote_Turret::Spawn()
{
	BaseClass::Spawn();
	Precache();
	SetModel(ASW_REMOTE_TURRET_MODEL);	
	SetMoveType( MOVETYPE_NONE );
	SetSolid(SOLID_BBOX);
	SetCollisionGroup( COLLISION_GROUP_NONE );
	m_takedamage = DAMAGE_NO;
	m_iHealth = 100;
	m_iMaxHealth = m_iHealth;

	m_angDefault = GetAbsAngles();
	if (m_angViewLimit.Get()[0] == 0)
		m_angViewLimit.GetForModify()[0] = 60;
	if (m_angViewLimit.Get()[1] == 0)
		m_angViewLimit.GetForModify()[1] = 60;
}

void CASW_Remote_Turret::Precache()
{
	PrecacheModel(ASW_REMOTE_TURRET_MODEL);
	PrecacheScriptSound("ASW_Sentry.Fire");
	PrecacheScriptSound("ASW_Sentry.Turn");

	BaseClass::Precache();
}

#endif // not client

void CASW_Remote_Turret::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	// only care about facing and firing
	move->m_vecViewAngles		= ucmd->viewangles;
	// todo: clamp facing angle here?
	move->m_nButtons			= ucmd->buttons;
}

// asw
QAngle g_angTurret = QAngle(0,0,0);
bool g_bLastControllingTurret = false;
void CASW_Remote_Turret::SmoothTurretAngle(QAngle &ang)
{
	float dt = MIN( 0.2, gpGlobals->frametime );
	
	ang[YAW] = ASW_ClampYaw( asw_turret_turn_rate.GetFloat(), m_angEyeAngles[YAW], ang[YAW], dt );
	ang[PITCH] = ASW_ClampYaw( asw_turret_turn_rate.GetFloat()*0.8f, m_angEyeAngles[PITCH], ang[PITCH], dt );	

	m_angEyeAngles = ang;//GetMarine()->EyeAngles()[YAW];
}

void CASW_Remote_Turret::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData )
{
	QAngle angTemp = RealEyeAngles();
	m_angEyeAngles = angTemp;
	SetLocalAngles(m_angEyeAngles);

	// check for firing
	bool bAttack1, bAttack2, bReload;
	GetButtons(bAttack1, bAttack2, bReload);
	if (bAttack1 && gpGlobals->curtime >= m_fNextFireTime)
	{
		FireTurret(pPlayer);
	}
}

#ifdef CLIENT_DLL

const QAngle& CASW_Remote_Turret::RealEyeAngles()
{
	bool bLocallyControlled = false;
	if (m_hUser.Get())
	{
		C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(m_hUser.Get());
		if (pMarine && pMarine->IsInhabited() && pMarine->GetCommander() == C_ASW_Player::GetLocalASWPlayer())
			bLocallyControlled = true;
	}

	if (bLocallyControlled)
	{
		return C_ASW_Player::GetLocalASWPlayer()->EyeAngles();
	}
	else
	{
		return m_angEyeAngles;
	}
}

const QAngle& CASW_Remote_Turret::EyeAngles()
{
	return m_angEyeAngles;
}

const QAngle& CASW_Remote_Turret::GetRenderAngles()
{
	bool bLocallyControlled = false;
	if (m_hUser.Get())
	{
		C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(m_hUser.Get());
		if (pMarine && pMarine->IsInhabited() && pMarine->GetCommander() == C_ASW_Player::GetLocalASWPlayer())
			bLocallyControlled = true;
	}

	static QAngle ang;
	ang = m_angEyeAngles;
	if (bLocallyControlled)
	{
		ang = C_ASW_Player::GetLocalASWPlayer()->EyeAngles();
	}
	if (m_bUpsideDown)
		ang[ROLL] = 180;
	return ang;
}

int CASW_Remote_Turret::GetMuzzleAttachment( void )
{
	return LookupAttachment( "muzzle" );
}

float CASW_Remote_Turret::GetMuzzleFlashScale( void )
{
	return 1.5f;
}

void CASW_Remote_Turret::ProcessMuzzleFlashEvent()
{
	// attach muzzle flash particle system effect
	int iAttachment = GetMuzzleAttachment();		
	
	if ( iAttachment > 0 )
	{		
//#ifndef _DEBUG
		float flScale = GetMuzzleFlashScale();
		FX_MuzzleEffectAttached( flScale, GetRefEHandle(), iAttachment, NULL, false );	
//#endif
	}
	BaseClass::ProcessMuzzleFlashEvent();
}

void CASW_Remote_Turret::OnRestore()
{
	BaseClass::OnRestore();
	SoundInit();
}

void CASW_Remote_Turret::OnDataChanged( DataUpdateType_t updateType )
{
	SoundInit();
	
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink(gpGlobals->curtime);
	}

	BaseClass::OnDataChanged( updateType );
}

void CASW_Remote_Turret::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	SoundShutdown();
}

void CASW_Remote_Turret::SoundInit()
{
	// play an engine start sound!!
	CPASAttenuationFilter filter( this );
	
	m_pLoopingSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Sentry.Turn" );
	CSoundEnvelopeController::GetController().Play( m_pLoopingSound, 0.0, 100 );	
}

void CASW_Remote_Turret::SoundShutdown()
{	
	if ( m_pLoopingSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pLoopingSound );
		m_pLoopingSound = NULL;
	}
}

void CASW_Remote_Turret::ClientThink(void)
{
	float yawdiff = abs(AngleDiff(m_LastAngle[YAW], m_angEyeAngles[YAW]));
	float pitchdiff = abs(AngleDiff(m_LastAngle[PITCH], m_angEyeAngles[PITCH]));
	//bool bPlaySound = !AnglesAreEqual(m_LastAngle[YAW], m_angEyeAngles[YAW],1) ||
						//!AnglesAreEqual(m_LastAngle[PITCH], m_angEyeAngles[PITCH],1);
	bool bPlaySound = yawdiff > 0 || pitchdiff > 0;

	//Msg("[C:%d:%f]CASW_Remote_Turret::ClientThink pos = %f, %f, %f\n", entindex(), gpGlobals->curtime, GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);

	//Msg("Lastyaw = %f current yaw=%f d=%f ", m_LastAngle[YAW], m_angEyeAngles[YAW], yawdiff);
	//Msg("Lastp = %f currentp=%f d=%f\n", m_LastAngle[PITCH], m_angEyeAngles[PITCH], pitchdiff);
	if (m_pLoopingSound && bPlaySound != m_bLastPlaySound)
	{
		if (bPlaySound)
		{
			//Msg("Lastyaw = %f current yaw=%f eq=%d ", m_LastAngle[YAW], m_angEyeAngles[YAW], AnglesAreEqual(m_LastAngle[YAW], m_angEyeAngles[YAW],1));
			//Msg("Lastp = %f currentp=%f eq=%d\n", m_LastAngle[PITCH], m_angEyeAngles[PITCH], AnglesAreEqual(m_LastAngle[PITCH], m_angEyeAngles[PITCH],1));
			float flVolume = CSoundEnvelopeController::GetController().SoundGetVolume( m_pLoopingSound );
			if ( flVolume != 1.0f )
			{
				//Msg("Fading in\n");
				CSoundEnvelopeController::GetController().SoundChangeVolume( m_pLoopingSound, 1.0, 0.01f );
			}
		}
		else
		{
			//Msg("Fading out\n");
			CSoundEnvelopeController::GetController().SoundChangeVolume( m_pLoopingSound, 0.0, 0.01f );
		}
	}
	m_bLastPlaySound = bPlaySound;
	m_LastAngle = m_angEyeAngles;

	if (m_hUser.Get())
	{
		SetNextClientThink(gpGlobals->curtime + 0.1f);
	}
	else
	{
		SetNextClientThink(gpGlobals->curtime + 1.0f);
	}
}

#else

const QAngle& CASW_Remote_Turret::RealEyeAngles()
{
	if (m_hUser.Get())
	{
		CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(m_hUser.Get());
		if (pMarine && pMarine->IsInhabited() && pMarine->GetCommander())
			return pMarine->GetCommander()->EyeAngles();
	}
	return m_angEyeAngles;	
}

const QAngle& CASW_Remote_Turret::EyeAngles()
{

	return m_angEyeAngles;	
}
#endif 

Vector CASW_Remote_Turret::GetTurretCamPosition()
{
	int iAttachment = LookupAttachment( "camera" );		
	if ( iAttachment > 0 )
	{		
		Vector camOrigin;
		QAngle camAngles;

		if ( GetAttachment( iAttachment, camOrigin, camAngles ) )
		{			
			return camOrigin;
		}
	}
	//Msg("not using camera attach pos\n");
	return GetAbsOrigin();
}

Vector CASW_Remote_Turret::GetTurretMuzzlePosition()
{
	Vector vecMuzzleOffset(41.92, 0, 8.0);
	Vector vecMuzzleOffsetTransformed(0,0,0);
	matrix3x4_t matrix;
	QAngle angFacing = EyeAngles();
	if (m_bUpsideDown)
		angFacing[ROLL] += 180;
	AngleMatrix( angFacing, matrix );
	VectorTransform(vecMuzzleOffset, matrix, vecMuzzleOffsetTransformed);
	return vecMuzzleOffsetTransformed + GetAbsOrigin();

	// using the attachment returns different results on the client/server, for some reason
	/*
	int iAttachment = LookupAttachment( "muzzle" );		
	if ( iAttachment > 0 )
	{		
		Vector camOrigin;
		QAngle camAngles;

		if ( GetAttachment( iAttachment, camOrigin, camAngles ) )
		{			
			return camOrigin;
		}
	}
	return GetAbsOrigin();*/
}

void CASW_Remote_Turret::GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload )
{
	if (m_hUser.Get())
	{
		CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(m_hUser.Get());
		if (pMarine && pMarine->IsInhabited() && pMarine->GetCommander())
		{
			bAttack1 = !!(pMarine->GetCommander()->m_nButtons & IN_ATTACK);
			bAttack2 = !!(pMarine->GetCommander()->m_nButtons & IN_ATTACK2);
			bReload = !!(pMarine->GetCommander()->m_nButtons & IN_RELOAD);
			return;
		}
	}

	bAttack1 = false;
	bAttack2 = false;
	bReload = false;
}

void CASW_Remote_Turret::FireTurret(CBasePlayer *pPlayer)
{
	if (!pPlayer)
		return;

	Vector vecShootOrigin, vecShootDir;
	vecShootOrigin = GetTurretMuzzlePosition();
#ifdef CLIENT_DLL
	QAngle angFacing = EyeAngles();
#else
	autoaim_params_t params;
	params.m_fScale = 2.0f;
	params.m_fMaxDist = asw_weapon_max_shooting_distance.GetFloat();
	
	QAngle angFacing = EyeAngles() + AutoaimDeflection(vecShootOrigin, EyeAngles(), params);			
		
#endif
	AngleVectors(angFacing, &vecShootDir);	

	//Msg("%s: Firing bullets from %s ", IsServer() ? "S" : "C", VecToString(vecShootOrigin));
	//Msg(" at angle %s\n", VecToString(angFacing));

	FireBulletsInfo_t info(1, vecShootOrigin, vecShootDir, GetBulletSpread(), asw_weapon_max_shooting_distance.GetFloat(), m_iAmmoType);
	info.m_flDamage = GetSentryDamage();
	info.m_pAttacker = this;
	info.m_iTracerFreq = 1;
	FireBullets(info);
	EmitSound("ASW_Sentry.Fire");

	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();
	CPASFilter filter( data.m_vOrigin );
	DispatchParticleEffect( "muzzle_sentrygun", PATTACH_POINT_FOLLOW, this, "muzzle", false, -1, &filter );

	//DoMuzzleFlash();
#ifdef CLIENT_DLL
	//FX_MicroExplosion(vecShootOrigin, Vector(0,0,1));
#else
	//NDebugOverlay::Box( vecShootOrigin, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), 0, 255, 0, 0, asw_turret_fire_rate.GetFloat() );
#endif

	m_fNextFireTime = gpGlobals->curtime + asw_turret_fire_rate.GetFloat();
}

CBaseEntity* CASW_Remote_Turret::GetUser()
{
	return m_hUser.Get();
}

#ifndef CLIENT_DLL

QAngle CASW_Remote_Turret::AutoaimDeflection( Vector &vecSrc, const QAngle &eyeAngles, autoaim_params_t &params )
{
	float		bestscore;
	float		score;
	Vector		bestdir;
	CBaseEntity	*bestent;
	trace_t		tr;
	Vector		v_forward, v_right, v_up;

	AngleVectors( eyeAngles, &v_forward, &v_right, &v_up );

	// try all possible entities
	bestdir = v_forward;
	bestscore = 0.0f;
	bestent = NULL;

	//Reset this data
	params.m_bOnTargetNatural	= false;
	
	CBaseEntity *pIgnore = NULL;
	CTraceFilterSkipTwoEntities traceFilter( this, pIgnore, COLLISION_GROUP_NONE );

	UTIL_TraceLine( vecSrc, vecSrc + bestdir * MAX_COORD_FLOAT, MASK_SHOT, &traceFilter, &tr );

	CBaseEntity *pEntHit = tr.m_pEnt;

	if ( pEntHit && pEntHit->m_takedamage != DAMAGE_NO && pEntHit->GetHealth() > 0 )
	{
		// don't look through water
		if (!((GetWaterLevel() != 3 && pEntHit->GetWaterLevel() == 3) || (GetWaterLevel() == 3 && pEntHit->GetWaterLevel() == 0)))
		{
			if( pEntHit->GetFlags() & FL_AIMTARGET )
			{
				// Player is already on target naturally, don't autoaim.
				// Fill out the autoaim_params_t struct, though.
				params.m_hAutoAimEntity.Set(pEntHit);
				params.m_vecAutoAimDir = bestdir;
				params.m_vecAutoAimPoint = tr.endpos;
				params.m_bAutoAimAssisting = false;
				params.m_bOnTargetNatural = true;
			}

			return vec3_angle;
		}
	}

	int count = AimTarget_ListCount();
	if ( count )
	{
		CBaseEntity **pList = (CBaseEntity **)stackalloc( sizeof(CBaseEntity *) * count );
		AimTarget_ListCopy( pList, count );

		for ( int i = 0; i < count; i++ )
		{
			Vector center;
			Vector dir;
			CBaseEntity *pEntity = pList[i];

			// Don't shoot yourself
			if ( pEntity == this )
				continue;

			if (!pEntity->IsAlive() || !pEntity->edict() )
				continue;

			//if ( !g_pGameRules->ShouldAutoAim( this, pEntity->edict() ) )
				//continue;

			// don't look through water
			if ((GetWaterLevel() != 3 && pEntity->GetWaterLevel() == 3) || (GetWaterLevel() == 3 && pEntity->GetWaterLevel() == 0))
				continue;

			// don't autoaim at marines
			if( pEntity->Classify() == CLASS_ASW_MARINE )			
				continue;			

			// Don't autoaim at the noisy bodytarget, this makes the autoaim crosshair wobble.
			//center = pEntity->BodyTarget( vecSrc, false );
			center = pEntity->WorldSpaceCenter();

			dir = (center - vecSrc);
			
			float dist = dir.Length2D();
			VectorNormalize( dir );

			// Skip if out of range.
			if( dist > params.m_fMaxDist )
				continue;

			float dot = DotProduct (dir, v_forward );

			// make sure it's in front of the player
			if( dot < 0 )
				continue;

			score = GetAutoaimScore(vecSrc, v_forward, pEntity->GetAutoAimCenter(), pEntity, params.m_fScale);

			if( score <= bestscore )
			{
				continue;
			}

			UTIL_TraceLine( vecSrc, center, MASK_SHOT, &traceFilter, &tr );

			if (tr.fraction != 1.0 && tr.m_pEnt != pEntity )
			{
				// Msg( "hit %s, can't see %s\n", STRING( tr.u.ent->classname ), STRING( pEdict->classname ) );
				continue;
			}

			// This is the best candidate so far.
			bestscore = score;
			bestent = pEntity;
			bestdir = dir;
		}
		if ( bestent )
		{
			QAngle bestang;

			VectorAngles( bestdir, bestang );
			
			bestang -= eyeAngles;

			// Autoaim detected a target for us. Aim automatically at its bodytarget.
			params.m_hAutoAimEntity.Set(bestent);
			params.m_vecAutoAimDir = bestdir;
			params.m_vecAutoAimPoint = bestent->BodyTarget( vecSrc, false );
			params.m_bAutoAimAssisting = true;

			return bestang;
		}
	}

	return QAngle( 0, 0, 0 );
}

float CASW_Remote_Turret::GetAutoaimScore( const Vector &eyePosition, const Vector &viewDir, const Vector &vecTarget, CBaseEntity *pTarget, float fScale )
{
	float radiusSqr;
	float targetRadiusSqr = Square( (pTarget->GetAutoAimRadius() * fScale) );

	Vector vecNearestPoint = PointOnLineNearestPoint( eyePosition, eyePosition + viewDir * 8192, vecTarget );
	Vector vecDiff = vecTarget - vecNearestPoint;

	radiusSqr = vecDiff.LengthSqr();

	if( radiusSqr <= targetRadiusSqr )
	{
		float score;

		score = 1.0f - (radiusSqr / targetRadiusSqr);

		Assert( score >= 0.0f && score <= 1.0f );
		return score;
	}
	
	// 0 means no score- doesn't qualify for autoaim.
	return 0.0f;
}

int CASW_Remote_Turret::UpdateTransmitState()
{
	//if ( GetUser() )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
	//else
	//{
		//return SetTransmitState( FL_EDICT_DONTSEND );
	//}
}

// always send this entity to players (for now...)
int CASW_Remote_Turret::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

void CASW_Remote_Turret::StopUsingTurret()
{
	m_hUser = NULL;
	UpdateTransmitState();
}

void CASW_Remote_Turret::StartedUsingTurret(CBaseEntity* pUser)
{
	m_hUser = pUser;
	UpdateTransmitState();
}
#else
	// client
	// clamp view angles
	void CASW_Remote_Turret::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
	{
		QAngle angMax = m_angDefault.Get() + m_angViewLimit.Get();
		QAngle angMin = m_angDefault.Get() - m_angViewLimit.Get();
		
		// limit all 3 axes of rotation
		if (asw_turret_debug_limits.GetBool()) Msg("Limiting turret:");
		for (int i=0;i<3;i++)
		{
			float fDiff = UTIL_AngleDiff(pCmd->viewangles[i], m_angDefault.Get()[i]);
			float fMinDiff = UTIL_AngleDiff(angMin[i], m_angDefault.Get()[i]);
			float fMaxDiff = UTIL_AngleDiff(angMax[i], m_angDefault.Get()[i]);
			if (asw_turret_debug_limits.GetBool())
				Msg("%d: fdiff=%f mindiff=%f maxdiff=%f min=%f max=%f ang=%f def=%f", i, fDiff, fMinDiff, fMaxDiff,
				  angMin[i], angMax[i], pCmd->viewangles[i], m_angDefault.Get()[i]);

			if (fMinDiff <= -180 || fMinDiff >= 180)	// if 360 rotation is allowed, skip any clamping for this axis
				continue;

			if (fDiff < fMinDiff)
				pCmd->viewangles[i] = angMin[i];
			else if (fDiff > fMaxDiff)
				pCmd->viewangles[i] = angMax[i];
		}
		if (asw_turret_debug_limits.GetBool())  Msg("\n");
	}

#endif

int CASW_Remote_Turret::GetSentryDamage()
{
	if (!ASWGameRules())
		return 10;

	return 25.0f - ((ASWGameRules()->GetMissionDifficulty() - 5.0f) * 0.75f);
}

#ifdef CLIENT_DLL
extern void ASWDoParticleTracer( const char *pTracerEffectName, const Vector &vecStart, const Vector &vecEnd, bool bRedTracer, int iAttributeEffects = 0 );
#endif

void CASW_Remote_Turret::MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType )
{
#ifdef GAME_DLL
	CBroadcastRecipientFilter filter;
	if ( gpGlobals->maxClients <= 1 )
	{
		filter.SetIgnorePredictionCull( true );
	}
	const char* szTracer = "ASWRemoteTurretTracer";
	UserMessageBegin( filter, szTracer );
	WRITE_SHORT( entindex() );
	WRITE_FLOAT( tr.endpos.x );
	WRITE_FLOAT( tr.endpos.y );
	WRITE_FLOAT( tr.endpos.z );
	MessageEnd();
#else
	ASWRemoteTurretTracer( tr.endpos );
#endif
}

#ifdef CLIENT_DLL
void __MsgFunc_ASWRemoteTurretTracer( bf_read &msg )
{
	int iSentry = msg.ReadShort();		
	CASW_Remote_Turret *pSentry = dynamic_cast<CASW_Remote_Turret*>( ClientEntityList().GetEnt( iSentry ) );

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();

	if ( pSentry )
	{
		pSentry->ASWRemoteTurretTracer( vecEnd );
	}
}
USER_MESSAGE_REGISTER( ASWRemoteTurretTracer );

void CASW_Remote_Turret::ASWRemoteTurretTracer( const Vector &vecEnd )
{
	MDLCACHE_CRITICAL_SECTION();
	Vector vecStart;
	QAngle vecAngles;

	if ( IsDormant() )
		return;

	C_BaseAnimating::PushAllowBoneAccess( true, false, "remoteturret" );

	// Get the muzzle origin
	if ( !GetAttachment( GetMuzzleAttachment(), vecStart, vecAngles ) )
	{
		return;
	}

	ASWDoParticleTracer( "tracer_autogun", vecStart, vecEnd, false );

	C_BaseAnimating::PopBoneAccess( "remoteturret" );
}
#endif