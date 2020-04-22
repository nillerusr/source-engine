//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_mg42.h"
#include "engine/ivdebugoverlay.h"

#if defined( CLIENT_DLL )

	#include "tier1/KeyValues.h"
	#include "particles_simple.h"
	#include "particles_localspace.h"
	#include "fx.h"
	#include "c_dod_player.h"

#else
	
	#include "dod_player.h"

#endif


#ifdef CLIENT_DLL	
void ToolFramework_PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );
#endif


IMPLEMENT_NETWORKCLASS_ALIASED( WeaponMG42, DT_WeaponMG42 )

#ifdef GAME_DLL

BEGIN_DATADESC( CWeaponMG42 )
	DEFINE_THINKFUNC( CoolThink ),
END_DATADESC()

#endif

BEGIN_NETWORK_TABLE( CWeaponMG42, DT_WeaponMG42 )
#ifdef CLIENT_DLL	
	RecvPropInt			( RECVINFO( m_iWeaponHeat ) ),
	RecvPropTime		( RECVINFO( m_flNextCoolTime ) ),
	RecvPropBool		( RECVINFO( m_bOverheated ) ),
#else
	SendPropInt			( SENDINFO( m_iWeaponHeat ), 7, SPROP_UNSIGNED ),
	SendPropFloat		( SENDINFO( m_flNextCoolTime ) ),
	SendPropBool		( SENDINFO( m_bOverheated ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponMG42 )
	DEFINE_PRED_FIELD( m_iWeaponHeat, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flNextCoolTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_bOverheated, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_mg42, CWeaponMG42 );
PRECACHE_WEAPON_REGISTER( weapon_mg42 );

acttable_t CWeaponMG42::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_MG,				false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_MG,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_MG,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_MG,				false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_MG,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_MG,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_MG,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_MG,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_MG,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_MG,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_MG,				false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_MG,				false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_MG,				false },

	// Deployed Aim
	{ ACT_DOD_DEPLOYED,						ACT_DOD_DEPLOY_MG,						false },
	{ ACT_DOD_PRONE_DEPLOYED,				ACT_DOD_PRONE_DEPLOY_MG,				false },

	// Attack ( prone? deployed? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_MG,				false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_MG,				false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_MG,			false },
	{ ACT_DOD_PRIMARYATTACK_DEPLOYED,		ACT_DOD_PRIMARYATTACK_DEPLOYED_MG,		false },
	{ ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED,	ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED_MG,false },

	// Reload ( prone? deployed? )	
	{ ACT_DOD_RELOAD_DEPLOYED,				ACT_DOD_RELOAD_DEPLOYED_MG,				false },
	{ ACT_DOD_RELOAD_PRONE_DEPLOYED,		ACT_DOD_RELOAD_PRONE_DEPLOYED_MG,		false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_MG42,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_MG42,					false },
};

IMPLEMENT_ACTTABLE( CWeaponMG42 );

void CWeaponMG42::Spawn( void )
{
	m_iWeaponHeat = 0;
	m_flNextCoolTime = 0;

	m_bOverheated = false;

#ifdef CLIENT_DLL
	m_pEmitter = NULL;
	m_flParticleAccumulator = 0.0;
	m_hParticleMaterial = ParticleMgr()->GetPMaterial( "sprites/effects/bazookapuff" );
#endif	

	BaseClass::Spawn();
}

#ifdef CLIENT_DLL

	CWeaponMG42::~CWeaponMG42()
	{
		if ( clienttools->IsInRecordingMode() && m_pEmitter.IsValid() && m_pEmitter->GetToolParticleEffectId() != TOOLPARTICLESYSTEMID_INVALID )
		{
			KeyValues *msg = new KeyValues( "ParticleSystem_ActivateEmitter" );
			msg->SetInt( "id", m_pEmitter->GetToolParticleEffectId() );
			msg->SetFloat( "time", gpGlobals->curtime );
			msg->SetInt( "active", 0 );

			msg->SetInt( "emitter", 0 );
			ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );

			msg->SetInt( "emitter", 1 );
			ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );

			msg->SetInt( "emitter", 2 );
			ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );

			msg->SetInt( "emitter", 3 );
			ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );

			msg->deleteThis();
		}
	}

	void CWeaponMG42::OnDataChanged( DataUpdateType_t updateType )
	{
		BaseClass::OnDataChanged( updateType );

		// BUG! This can happen more than once!
		if ( updateType == DATA_UPDATE_CREATED )
		{
			if ( !m_pEmitter.IsValid() )
			{
				m_pEmitter = CSimpleEmitter::Create( "MGOverheat" );
			}

			Assert( m_pEmitter.IsValid() );
		}

		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_ALWAYS );
	}

	// Client Think emits smoke particles based on heat
	// ( except if we are holstered )
	void CWeaponMG42::ClientThink( void )
	{
		m_pEmitter->SetSortOrigin( GetAbsOrigin() );

		float flEmitRate = 0.0;	//particles per second

		// Only smoke if we are dropped ( no owner ) or if we have an owner and are active
		if ( GetOwner() == NULL || GetOwner()->GetActiveWeapon() == this )
		{
			if ( m_iWeaponHeat > 85 )
			{	
				flEmitRate = 30;
			}
			else if ( m_iWeaponHeat > 80 )
			{
				flEmitRate = 20;
			}
			else if ( m_iWeaponHeat > 65 )
			{
				flEmitRate = 10;
			}
			else if ( m_iWeaponHeat > 50 )
			{
				flEmitRate = 5;
			}
		}

		m_flParticleAccumulator += ( gpGlobals->frametime * flEmitRate );

		while( m_flParticleAccumulator > 0.0 )
		{
			EmitSmokeParticle();

			m_flParticleAccumulator -= 1.0;
		}
	}

	void CWeaponMG42::EmitSmokeParticle( void )
	{
		Vector vFront, vBack;	
		QAngle angles;

		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		bool bViewModel = false;

		// if this is locally owned
		if ( GetPlayerOwner() == pLocalPlayer )
		{
			C_BaseViewModel *vm = pLocalPlayer->GetViewModel( 0 );

			if ( !vm )
				return;

			vm->GetAttachment( 1, vFront, angles );
			vm->GetAttachment( 2, vBack, angles );

			bViewModel = true;
		}
		else
		{
			// could be dropped, or held by another player
			GetAttachment( 1, vFront, angles );
			GetAttachment( 2, vBack, angles );
		}

		// Get a position somewhere on the barrel
		Vector vPos = vBack + random->RandomFloat(0.0, 1.0 ) * ( vFront - vBack );

		SimpleParticle *pParticle = m_pEmitter->AddSimpleParticle( m_hParticleMaterial, vPos );
		if ( pParticle )
		{
			pParticle->m_vecVelocity = Vector( 0,0,12 );
			pParticle->m_flRoll = random->RandomFloat( 0, 0.5 );
			pParticle->m_flRollDelta = ( random->RandomInt(0,1) == 0 ? 1 : -1 ) * random->RandomFloat( 0.5, 1.0 );
			pParticle->m_flDieTime = 1.8f;
			pParticle->m_flLifetime = 0;
			pParticle->m_uchColor[0] = 200;
			pParticle->m_uchColor[1] = 200; 
			pParticle->m_uchColor[2] = 200;
			pParticle->m_uchStartAlpha = 60;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = 4;
			pParticle->m_uchEndSize = 25;
			pParticle->m_iFlags = 0;	//bViewModel ? FLE_VIEWMODEL : 0;
		}
	}

#else

	// This function does the cooling when the weapon is dropped or holstered
	// regular, predicted cooling is done in ItemPostFrame
	void CWeaponMG42::CoolThink( void )
	{
		if ( m_iWeaponHeat > 0 )
			m_iWeaponHeat--;
		
		SetContextThink( &CWeaponMG42::CoolThink, gpGlobals->curtime + 0.1, COOL_CONTEXT );
	}

#endif	

void CWeaponMG42::Precache()
{
	PrecacheMaterial( "sprites/effects/bazookapuff" );

	PrecacheScriptSound( "Weapon_Mg42.OverHeat" );

	BaseClass::Precache();
}

bool CWeaponMG42::Reload( void )
{
	if( !IsDeployed() )
	{
#ifdef CLIENT_DLL
		CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

		if ( pPlayer )
			pPlayer->HintMessage( HINT_MG_DEPLOY_TO_RELOAD );
#endif
		return false;
	}

	return BaseClass::Reload();
}

void CWeaponMG42::FinishReload( void )
{
	BaseClass::FinishReload();

	//Reset the heat when you complete a reload
	m_iWeaponHeat = 0;
}

void CWeaponMG42::PrimaryAttack( void )
{
	if( m_bOverheated )
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		if (m_bFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		}

		return;
	}


	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
	Assert( pPlayer );

	if( m_iWeaponHeat >= 99 )
	{
		//can't fire anymore, wait until heat is below 80
#ifdef CLIENT_DLL 
		pPlayer->HintMessage( HINT_WEAPON_OVERHEAT );
#endif
		m_bOverheated = true;
		m_bInAttack = true;

		EmitSound( "Weapon_Mg42.OverHeat" );
		return;
	}

	m_iWeaponHeat += 1; //2;
	m_flNextCoolTime = gpGlobals->curtime + 0.16f;
	
	if( !IsDeployed() )
	{
#ifdef CLIENT_DLL 
		pPlayer->HintMessage( HINT_MG_FIRE_UNDEPLOYED );
#endif
		pPlayer->m_Shared.SetSlowedTime( 0.2 );

		float flStamina = pPlayer->m_Shared.GetStamina();

		pPlayer->m_Shared.SetStamina( flStamina - 15 );
	}

	BaseClass::PrimaryAttack();
}

void CWeaponMG42::ItemPostFrame( void )
{
	ItemFrameCool();

	if( m_iWeaponHeat < 80 )
		m_bOverheated = false;	

	BaseClass::ItemPostFrame();
}

void CWeaponMG42::ItemBusyFrame( void )
{
	ItemFrameCool();

	BaseClass::ItemBusyFrame();
}

void CWeaponMG42::ItemFrameCool( void )
{
	if( gpGlobals->curtime > m_flNextCoolTime )
	{
		if ( m_iWeaponHeat > 0 )
			m_iWeaponHeat--;

		m_flNextCoolTime = gpGlobals->curtime + 0.16f;
	}
}

bool CWeaponMG42::Deploy( void )
{
	// stop the fake cooling when we deploy the weapon
	SetContextThink( NULL, 0.0, COOL_CONTEXT );

	return BaseClass::Deploy();
}

bool CWeaponMG42::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	SetContextThink( &CWeaponMG42::CoolThink, gpGlobals->curtime + 0.1, COOL_CONTEXT );
#endif

	return BaseClass::Holster(pSwitchingTo);
}

void CWeaponMG42::Drop( const Vector &vecVelocity )
{
#ifndef CLIENT_DLL
	SetContextThink( &CWeaponMG42::CoolThink, gpGlobals->curtime + 0.1, COOL_CONTEXT );
#endif

	BaseClass::Drop( vecVelocity );
}

Activity CWeaponMG42::GetReloadActivity( void )
{
	return ACT_VM_RELOAD;
}

Activity CWeaponMG42::GetDrawActivity( void )
{
	Activity actDraw;

	if( 0 && m_iClip1 <= 0 )
		actDraw = ACT_VM_DRAW_EMPTY;	
	else
		actDraw = ACT_VM_DRAW;

	return actDraw;
}

Activity CWeaponMG42::GetDeployActivity( void )
{
	Activity actDeploy;

	switch ( m_iClip1 )
	{
	case 8:
		actDeploy = ACT_VM_DEPLOY_8;
		break;
	case 7:
		actDeploy = ACT_VM_DEPLOY_7;
		break;
	case 6:
		actDeploy = ACT_VM_DEPLOY_6;
		break;
	case 5:
		actDeploy = ACT_VM_DEPLOY_5;
		break;
	case 4:
		actDeploy = ACT_VM_DEPLOY_4;
		break;
	case 3:
		actDeploy = ACT_VM_DEPLOY_3;
		break;
	case 2:
		actDeploy = ACT_VM_DEPLOY_2;
		break;
	case 1:
		actDeploy = ACT_VM_DEPLOY_1;
		break;
	case 0:
		actDeploy = ACT_VM_DEPLOY_EMPTY;
		break;
	default:
		actDeploy = ACT_VM_DEPLOY;
		break;
	}

	return actDeploy;
}

Activity CWeaponMG42::GetUndeployActivity( void )
{
	Activity actUndeploy;

	switch ( m_iClip1 )
	{
	case 8:
		actUndeploy = ACT_VM_UNDEPLOY_8;
		break;
	case 7:
		actUndeploy = ACT_VM_UNDEPLOY_7;
		break;
	case 6:
		actUndeploy = ACT_VM_UNDEPLOY_6;
		break;
	case 5:
		actUndeploy = ACT_VM_UNDEPLOY_5;
		break;
	case 4:
		actUndeploy = ACT_VM_UNDEPLOY_4;
		break;
	case 3:
		actUndeploy = ACT_VM_UNDEPLOY_3;
		break;
	case 2:
		actUndeploy = ACT_VM_UNDEPLOY_2;
		break;
	case 1:
		actUndeploy = ACT_VM_UNDEPLOY_1;
		break;
	case 0:
		actUndeploy = ACT_VM_UNDEPLOY_EMPTY;
		break;
	default:
		actUndeploy = ACT_VM_UNDEPLOY;
		break;
	}

	return actUndeploy;
}

Activity CWeaponMG42::GetIdleActivity( void )
{
	Activity actIdle;

	if( IsDeployed() )
	{
		switch ( m_iClip1 )
		{
		case 8:
			actIdle = ACT_VM_IDLE_DEPLOYED_8;
			break;
		case 7:
			actIdle = ACT_VM_IDLE_DEPLOYED_7;
			break;
		case 6:
			actIdle = ACT_VM_IDLE_DEPLOYED_6;
			break;
		case 5:
			actIdle = ACT_VM_IDLE_DEPLOYED_5;
			break;
		case 4:
			actIdle = ACT_VM_IDLE_DEPLOYED_4;
			break;
		case 3:
			actIdle = ACT_VM_IDLE_DEPLOYED_3;
			break;
		case 2:
			actIdle = ACT_VM_IDLE_DEPLOYED_2;
			break;
		case 1:
			actIdle = ACT_VM_IDLE_DEPLOYED_1;
			break;
		case 0:
			actIdle = ACT_VM_IDLE_DEPLOYED_EMPTY;
			break;
		default:
			actIdle = ACT_VM_IDLE_DEPLOYED;
			break;
		}
	}
	else
	{
		switch ( m_iClip1 )
		{
		case 8:
			actIdle = ACT_VM_IDLE_8;
			break;
		case 7:
			actIdle = ACT_VM_IDLE_7;
			break;
		case 6:
			actIdle = ACT_VM_IDLE_6;
			break;
		case 5:
			actIdle = ACT_VM_IDLE_5;
			break;
		case 4:
			actIdle = ACT_VM_IDLE_4;
			break;
		case 3:
			actIdle = ACT_VM_IDLE_3;
			break;
		case 2:
			actIdle = ACT_VM_IDLE_2;
			break;
		case 1:
			actIdle = ACT_VM_IDLE_1;
			break;
		case 0:
			actIdle = ACT_VM_IDLE_EMPTY;
			break;
		default:
			actIdle = ACT_VM_IDLE;
			break;
		}
	}

	return actIdle;
}

Activity CWeaponMG42::GetPrimaryAttackActivity( void )
{
	Activity actPrim;

	if( IsDeployed() )
	{
		switch ( m_iClip1 )
		{
		case 8:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_8;
			break;
		case 7:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_7;
			break;
		case 6:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_6;
			break;
		case 5:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_5;
			break;
		case 4:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_4;
			break;
		case 3:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_3;
			break;
		case 2:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_2;
			break;
		case 1:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_1;
			break;
		case 0:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_EMPTY;
			break;
		default:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED;
			break;
		}
	}
	else
	{
		switch ( m_iClip1 )
		{
		case 8:
			actPrim = ACT_VM_PRIMARYATTACK_8;
			break;
		case 7:
			actPrim = ACT_VM_PRIMARYATTACK_7;
			break;
		case 6:
			actPrim = ACT_VM_PRIMARYATTACK_6;
			break;
		case 5:
			actPrim = ACT_VM_PRIMARYATTACK_5;
			break;
		case 4:
			actPrim = ACT_VM_PRIMARYATTACK_4;
			break;
		case 3:
			actPrim = ACT_VM_PRIMARYATTACK_3;
			break;
		case 2:
			actPrim = ACT_VM_PRIMARYATTACK_2;
			break;
		case 1:
			actPrim = ACT_VM_PRIMARYATTACK_1;
			break;
		case 0:
			actPrim = ACT_VM_PRIMARYATTACK_EMPTY;
			break;
		default:
			actPrim = ACT_VM_PRIMARYATTACK;
			break;
		}
	}

	return actPrim;
}

float CWeaponMG42::GetRecoil( void )
{
	CDODPlayer *p = ToDODPlayer( GetPlayerOwner() );

	if( p && p->m_Shared.IsInMGDeploy() )
	{
		return 0.0f;
	}

	return 20;
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// This is called after sending this entity's recording state
//-----------------------------------------------------------------------------
void CWeaponMG42::CleanupToolRecordingState( KeyValues *msg )
{
	BaseClass::CleanupToolRecordingState( msg );

	// Generally, this is used to allow the entity to clean up
	// allocated state it put into the message, but here we're going
	// to use it to send particle system messages because we
	// know the smoke has been recorded at this point
	if ( !clienttools->IsInRecordingMode() || !m_pEmitter.IsValid() )
		return;

	// NOTE: Particle system destruction message will be sent by the particle effect itself.
	if ( m_pEmitter->GetToolParticleEffectId() == TOOLPARTICLESYSTEMID_INVALID )
	{
		int nId = m_pEmitter->AllocateToolParticleEffectId();

		KeyValues *msg = new KeyValues( "ParticleSystem_Create" );
		msg->SetString( "name", "CWeaponMG42 smoke" );
		msg->SetInt( "id", nId );
		msg->SetFloat( "time", gpGlobals->curtime );

		KeyValues *pEmitter0 = msg->FindKey( "DmeSpriteEmitter", true );
		pEmitter0->SetInt( "count", 5 );	// particles per second, when duration is < 0
		pEmitter0->SetFloat( "duration", -1 );
		pEmitter0->SetString( "material", "sprites/effects/bazookapuff" );
		pEmitter0->SetInt( "active", 0 );

		KeyValues *pInitializers = pEmitter0->FindKey( "initializers", true );

		KeyValues *pPosition = pInitializers->FindKey( "DmeRandomAttachmentPositionEntityInitializer", true );
		pPosition->SetPtr( "entindex", (void*)entindex() );
 		pPosition->SetInt( "attachmentIndex0", 1 );
 		pPosition->SetInt( "attachmentIndex1", 2 );

		KeyValues *pLifetime = pInitializers->FindKey( "DmeRandomLifetimeInitializer", true );
		pLifetime->SetFloat( "minLifetime", 1.8f );
 		pLifetime->SetFloat( "maxLifetime", 1.8f );

		KeyValues *pVelocity = pInitializers->FindKey( "DmeConstantVelocityInitializer", true );
		pVelocity->SetFloat( "velocityX", 0.0f );
		pVelocity->SetFloat( "velocityY", 0.0f );
		pVelocity->SetFloat( "velocityZ", 12.0f );

		KeyValues *pRoll = pInitializers->FindKey( "DmeRandomRollInitializer", true );
		pRoll->SetFloat( "minRoll", 0.0f );
 		pRoll->SetFloat( "maxRoll", 0.5f );

		KeyValues *pRollSpeed = pInitializers->FindKey( "DmeSplitRandomRollSpeedInitializer", true );
		pRollSpeed->SetFloat( "minRollSpeed", 0.5f );
 		pRollSpeed->SetFloat( "maxRollSpeed", 1.0f );

		KeyValues *pColor = pInitializers->FindKey( "DmeRandomInterpolatedColorInitializer", true );
 		pColor->SetColor( "color1", Color( 200, 200, 200, 255 ) );
 		pColor->SetColor( "color2", Color( 200, 200, 200, 255 ) );

		KeyValues *pAlpha = pInitializers->FindKey( "DmeRandomAlphaInitializer", true );
		pAlpha->SetInt( "minStartAlpha", 60 );
		pAlpha->SetInt( "maxStartAlpha", 60 );
		pAlpha->SetInt( "minEndAlpha", 0 );
		pAlpha->SetInt( "maxEndAlpha", 0 );

		KeyValues *pSize = pInitializers->FindKey( "DmeRandomSizeInitializer", true );
		pSize->SetFloat( "minStartSize", 4 );
		pSize->SetFloat( "maxStartSize", 4 );
		pSize->SetFloat( "minEndSize", 25 );
		pSize->SetFloat( "maxEndSize", 25 );

		KeyValues *pUpdaters = pEmitter0->FindKey( "updaters", true );

		pUpdaters->FindKey( "DmePositionVelocityUpdater", true );
		pUpdaters->FindKey( "DmeRollUpdater", true );
		pUpdaters->FindKey( "DmeAlphaLinearUpdater", true );
		pUpdaters->FindKey( "DmeSizeUpdater", true );

		// create emitters for each emission rate: 5,10,20,30
		KeyValues *pEmitter1 = pEmitter0->MakeCopy();
		pEmitter1->SetInt( "count", 10 );
		msg->AddSubKey( pEmitter1 );

		KeyValues *pEmitter2 = pEmitter0->MakeCopy();
		pEmitter2->SetInt( "count", 20 );
		msg->AddSubKey( pEmitter2 );

		KeyValues *pEmitter3 = pEmitter0->MakeCopy();
		pEmitter3->SetInt( "count", 30 );
		msg->AddSubKey( pEmitter3 );

		// mark only the appropriate emitter active
		bool bHolstered = GetOwner() && GetOwner()->GetActiveWeapon() != this;
		if ( !bHolstered )
		{
			if ( m_iWeaponHeat > 85 )
			{
				pEmitter3->SetInt( "active", 1 );
			}
			else if ( m_iWeaponHeat > 80 )
			{
				pEmitter2->SetInt( "active", 1 );
			}
			else if ( m_iWeaponHeat > 65 )
			{
				pEmitter1->SetInt( "active", 1 );
			}
			else if ( m_iWeaponHeat > 50 )
			{
				pEmitter0->SetInt( "active", 1 );
			}
		}

		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
		msg->deleteThis();
	}
	else
	{
		int nEmitterIndex = -1;
		bool bHolstered = GetOwner() && GetOwner()->GetActiveWeapon() != this;
		if ( !bHolstered )
		{
			if ( m_iWeaponHeat > 85 )
			{
				nEmitterIndex = 3;
			}
			else if ( m_iWeaponHeat > 80 )
			{
				nEmitterIndex = 2;
			}
			else if ( m_iWeaponHeat > 65 )
			{
				nEmitterIndex = 1;
			}
			else if ( m_iWeaponHeat > 50 )
			{
				nEmitterIndex = 0;
			}
		}

		KeyValues *msg = new KeyValues( "ParticleSystem_ActivateEmitter" );
		msg->SetInt( "id", m_pEmitter->GetToolParticleEffectId() );
		msg->SetFloat( "time", gpGlobals->curtime );

		msg->SetInt( "emitter", 0 );
		msg->SetInt( "active", nEmitterIndex == 0 );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );

		msg->SetInt( "emitter", 1 );
		msg->SetInt( "active", nEmitterIndex == 1 );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );

		msg->SetInt( "emitter", 2 );
		msg->SetInt( "active", nEmitterIndex == 2 );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );

		msg->SetInt( "emitter", 3 );
		msg->SetInt( "active", nEmitterIndex == 3 );
		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );

		msg->deleteThis();
	}
}

#endif
