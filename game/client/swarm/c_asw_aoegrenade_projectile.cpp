#include "cbase.h"
#include "precache_register.h"
#include "particles_simple.h"
#include "iefx.h"
#include "dlight.h"
#include "view.h"
#include "fx.h"
#include "clientsideeffects.h"
#include "c_pixel_visibility.h"
#include "c_asw_aoegrenade_projectile.h"
#include "soundenvelope.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_weapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//Precahce the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, ASWPrecacheEffectAOEGrenades )
	PRECACHE( MATERIAL, "swarm/effects/blueflare" )
	PRECACHE( MATERIAL, "effects/yellowflare" )
	PRECACHE( MATERIAL, "effects/yellowflare_noz" )
PRECACHE_REGISTER_END()


//-----------------------------------------------------------------------------
// Purpose: RecvProxy that converts a marine's player UtlVector to entindexes
//-----------------------------------------------------------------------------
void RecvProxy_AOEGrenList(  const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_ASW_AOEGrenade_Projectile *pAOEGren = (C_ASW_AOEGrenade_Projectile*)pStruct;

	CBaseHandle *pHandle = (CBaseHandle*)(&(pAOEGren->m_hAOETargets[pData->m_iElement])); 
	RecvProxy_IntToEHandle( pData, pStruct, pHandle );

	// update the heal beams
	pAOEGren->m_bUpdateAOETargets = true;
}

void RecvProxyArrayLength_AOEGrenArray( void *pStruct, int objectID, int currentArrayLength )
{
	C_ASW_AOEGrenade_Projectile *pAOEGren = (C_ASW_AOEGrenade_Projectile*)pStruct;

	if ( pAOEGren->m_hAOETargets.Count() != currentArrayLength )
		pAOEGren->m_hAOETargets.SetSize( currentArrayLength );

	// update the heal beams
	pAOEGren->m_bUpdateAOETargets = true;
}

IMPLEMENT_CLIENTCLASS_DT( C_ASW_AOEGrenade_Projectile, DT_ASW_AOEGrenade_Projectile, CASW_AOEGrenade_Projectile )
	RecvPropArray2( 
			   RecvProxyArrayLength_AOEGrenArray,
			   RecvPropInt( "aoegren_array_element", 0, SIZEOF_IGNORE, 0, RecvProxy_AOEGrenList ), 
			   MAX_PLAYERS, 
			   0, 
			   "aoetarget_array"
			   ),
	RecvPropFloat( RECVINFO( m_flTimeBurnOut ) ),
	//RecvPropFloat( RECVINFO( m_flTimePulse ) ),
	RecvPropFloat( RECVINFO( m_flScale ) ),
	RecvPropBool( RECVINFO( m_bSettled ) ),
	RecvPropFloat( RECVINFO( m_flRadius ) ),
END_RECV_TABLE()

// aoegrenades maintain a linked list of themselves, for quick checking for autoaim
C_ASW_AOEGrenade_Projectile* g_pHeadAOEGrenade = NULL;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
C_ASW_AOEGrenade_Projectile::C_ASW_AOEGrenade_Projectile()
{
	m_flTimeBurnOut	= 0.0f;
	//m_flTimePulse	= 0.0f;
	m_pDLight = NULL;

	m_bSettled		= false;
	m_bPlayingSound	= false;

	//SetDynamicallyAllocated( false );
	m_queryHandle = 0;
	m_fStartLightTime = 0;
	m_fLightRadius = 0;

	m_bUpdateAOETargets = false;

	m_pPulseEffect = NULL;

	m_hSphereModel = NULL;
	m_flTimeCreated = -1;

	m_fUpdateAttachFXTime = 0;
}

C_ASW_AOEGrenade_Projectile::~C_ASW_AOEGrenade_Projectile( void )
{
	if (m_pDLight)
	{
		m_pDLight->die = gpGlobals->curtime;
		m_pDLight = NULL;
	}

	if (m_hSphereModel.Get())
	{
		UTIL_Remove( m_hSphereModel.Get() );
		m_hSphereModel = NULL;
	}

	StopSound( GetIdleLoopSoundName() );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void C_ASW_AOEGrenade_Projectile::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	if ( state == SHOULDTRANSMIT_END )
	{
		AddEffects( EF_NODRAW );
	}
	else if ( state == SHOULDTRANSMIT_START )
	{
		RemoveEffects( EF_NODRAW );
	}

	BaseClass::NotifyShouldTransmit( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_ASW_AOEGrenade_Projectile::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		//SetSortOrigin( GetAbsOrigin() );
		SoundInit();
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}

	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
	}

	BaseClass::OnDataChanged( updateType );

	if ( m_bUpdateAOETargets )
	{
		UpdateTargetAOEEffects();
		m_bUpdateAOETargets = false;
	}

	UpdatePingEffects();
}

void C_ASW_AOEGrenade_Projectile::UpdateTargetAOEEffects( void )
{
	// Find all the targets we've stopped giving a buff to
	AOEGrenTargetFXList_t::IndexLocalType_t i = m_hAOETargetEffects.Head();
	while ( m_hAOETargetEffects.IsValidIndex(i) )
	{
		AOETargetEffects_t &aoeTargetEffect = m_hAOETargetEffects[i];
		Assert( m_hAOETargetEffects[i].me == &m_hAOETargetEffects[i] );
		bool bStillAOEGren = false;

		// Are we still buffing this target?
		for ( int target = 0; target < m_hAOETargets.Count(); target++ )
		{
			if ( m_hAOETargets[target] && m_hAOETargets[target] == aoeTargetEffect.hTarget.Get() )
			{
				bStillAOEGren = true;
				break;
			}
		}

		// advance before deleting the pointer out from under us
		const AOEGrenTargetFXList_t::IndexLocalType_t oldi = i;
		i = m_hAOETargetEffects.Next( i );

		if ( !bStillAOEGren )
		{
			ParticleProp()->StopEmission( aoeTargetEffect.pEffect );

			// stop the sound on this marine
			C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>( m_hAOETargetEffects[oldi].hTarget.Get() );
			if ( pMarine && pMarine->GetCommander() )
			{
				C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
				if ( pMarine->GetCommander() == pLocalPlayer && pMarine->IsInhabited() && m_hAOETargetEffects[oldi].pBuffLoopSound )
				{
					CSoundEnvelopeController::GetController().SoundDestroy( m_hAOETargetEffects[oldi].pBuffLoopSound );
					m_hAOETargetEffects[oldi].pBuffLoopSound = NULL;
				}
			}

			m_hAOETargetEffects.Remove(oldi);
		}
	}

	// Now add any new targets
	for ( int i = 0; i < m_hAOETargets.Count(); i++ )
	{
		C_BaseEntity *pTarget = m_hAOETargets[i].Get();

		// Loops through the aoe targets, and make sure we have an effect for each of them
		if ( pTarget )
		{
			bool bHaveEffect = false;

			for ( AOEGrenTargetFXList_t::IndexLocalType_t i = m_hAOETargetEffects.Head() ;
				  m_hAOETargetEffects.IsValidIndex(i) ;
				  i = m_hAOETargetEffects.Next(i) )
			{
				if ( m_hAOETargetEffects[i].hTarget.Get() == pTarget )
				{
					bHaveEffect = true;
					break;
				}
			}

			if ( !bHaveEffect )
			{
				CNewParticleEffect *pEffect = ParticleProp()->Create( GetArcEffectName(), PATTACH_ABSORIGIN_FOLLOW );

				AOEGrenTargetFXList_t::IndexLocalType_t iIndex = m_hAOETargetEffects.AddToTail();
				m_hAOETargetEffects[iIndex].hTarget = pTarget;
				m_hAOETargetEffects[iIndex].pEffect = pEffect;
				Assert( m_hAOETargetEffects[iIndex].me == &m_hAOETargetEffects[iIndex] );

				UpdateParticleAttachments( m_hAOETargetEffects[iIndex].pEffect, pTarget );

				// Start the sound over again every time we start a new beam
				//StopSound( GetLoopSoundName() );

				C_ASW_Marine *pMarine = C_ASW_Marine::AsMarine( pTarget );
				if ( pMarine && pMarine->GetCommander() )
				{
					C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
					if ( pMarine->GetCommander() == pLocalPlayer && pMarine->IsInhabited() )
					{
						if ( m_hAOETargetEffects[iIndex].pBuffLoopSound )
						{
							CSoundEnvelopeController::GetController().SoundDestroy( m_hAOETargetEffects[iIndex].pBuffLoopSound );
							m_hAOETargetEffects[iIndex].pBuffLoopSound = NULL;
						}

						CSingleUserRecipientFilter filter( pLocalPlayer );
						EmitSound( filter, pMarine->entindex(), GetStartSoundName() );
						m_hAOETargetEffects[iIndex].pBuffLoopSound = CSoundEnvelopeController::GetController().SoundCreate( filter, pMarine->entindex(), GetLoopSoundName() );
						CSoundEnvelopeController::GetController().Play( m_hAOETargetEffects[iIndex].pBuffLoopSound, 1.0, 100 );
					}
				}
			}
		}
	}
}

void C_ASW_AOEGrenade_Projectile::UpdateParticleAttachments( CNewParticleEffect *pEffect, C_BaseEntity *pTarget )
{
	if ( GetArcAttachmentName() )
	{
		bool bAttachWeapon = false;
		if ( ShouldAttachEffectToWeapon() )
		{
			C_ASW_Marine *pMarine = C_ASW_Marine::AsMarine( pTarget );
			if ( pMarine && pMarine->GetActiveASWWeapon() )
			{
				C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
				int iAttachment = pWeapon->LookupAttachment( "muzzle" );
				if ( pWeapon->IsOffensiveWeapon() && iAttachment > 0 )
				{
					bAttachWeapon = true;
					ParticleProp()->AddControlPoint( pEffect, 1, pWeapon, PATTACH_POINT_FOLLOW, "muzzle" );
				}
			}
		}

		if ( !bAttachWeapon )
			ParticleProp()->AddControlPoint( pEffect, 1, pTarget, PATTACH_POINT_FOLLOW, GetArcAttachmentName() );
	}
	else
	{
		ParticleProp()->AddControlPoint( pEffect, 1, pTarget, PATTACH_ABSORIGIN_FOLLOW );
	}
}

void C_ASW_AOEGrenade_Projectile::UpdatePingEffects( void )
{
	if ( m_bSettled && m_pPulseEffect.GetObject() == NULL )
	{	
		m_pPulseEffect = ParticleProp()->Create( GetPingEffectName(), PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 8 ) );
		if ( m_pPulseEffect )
		{
			m_pPulseEffect->SetControlPoint( 1, Vector( m_flRadius, 0, 0 ) );
		}
	}

	if ( ShouldSpawnSphere() && m_bSettled && m_hSphereModel.Get() == NULL )
	{	
		C_BaseAnimating *pEnt = new C_BaseAnimating;
		if (!pEnt)
		{
			Msg("Error, couldn't create new C_BaseAnimating\n");
			return;
		}
		if (!pEnt->InitializeAsClientEntity( "models/items/shield_bubble/shield_bubble.mdl", false ))
			//if (!pEnt->InitializeAsClientEntity( "models/props_combine/coreball.mdl", false ))
		{
			Msg("Error, couldn't InitializeAsClientEntity\n");
			pEnt->Release();
			return;
		}

		pEnt->SetParent( this );
		pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
		pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );	
		pEnt->SetSolid( SOLID_NONE );
		pEnt->SetSkin( GetSphereSkin() );
		pEnt->RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

		m_hSphereModel = pEnt;
		m_flTimeCreated = gpGlobals->curtime;
	}
}

const Vector& C_ASW_AOEGrenade_Projectile::GetEffectOrigin()
{
	static Vector s_vecEffectPos;
	Vector forward, right, up;
	AngleVectors(GetAbsAngles(), &forward, &right, &up);
	s_vecEffectPos = GetAbsOrigin() + up * 5;
	return s_vecEffectPos;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : timeDelta - 
//-----------------------------------------------------------------------------
void C_ASW_AOEGrenade_Projectile::ClientThink( void )
{
	float	baseScale = m_flScale;
	float	flTimeLeft = m_flTimeBurnOut - gpGlobals->curtime;
	//Account for fading out
	if ( ( m_flTimeBurnOut != -1.0f ) && ( flTimeLeft <= 5.0f ) )
	{
		baseScale *= ( flTimeLeft / 5.0f );

		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pLoopedSound, clamp<float>(0.6f * baseScale, 0.0f, 0.6f), 0 );

		AOEGrenTargetFXList_t::IndexLocalType_t i = m_hAOETargetEffects.Head();
		while ( m_hAOETargetEffects.IsValidIndex(i) )
		{
			// Are we still buffing this target?
			for ( int target = 0; target < m_hAOETargets.Count(); target++ )
			{
				if ( m_hAOETargetEffects[i].pBuffLoopSound )
				{
					CSoundEnvelopeController::GetController().SoundChangeVolume( m_hAOETargetEffects[i].pBuffLoopSound, clamp<float>(0.6f * baseScale, 0.0f, 0.6f), 0 );
				}
			}

			i = m_hAOETargetEffects.Next( i );
		}
	}

	if ( baseScale < 0.01f )
		return;
	//
	// Dynamic light
	//

	if ( m_fStartLightTime == 0.0f )
	{
		m_fStartLightTime = gpGlobals->curtime;
	}
	if ( !m_pDLight )
	{
		m_pDLight = effects->CL_AllocDlight( index );

		Color rgbaGrenadeColor = GetGrenadeColor();

		m_pDLight->color.r = rgbaGrenadeColor.r();
		m_pDLight->color.g = rgbaGrenadeColor.g();
		m_pDLight->color.b = rgbaGrenadeColor.b();
		m_pDLight->color.exponent = 1;
	}

	
	m_pDLight->origin = GetAbsOrigin() + Vector(0, 0, 5);	// make the dlight slightly higher than the aoegrenade, so it doesn't bury the light being so close to the ground

	if ( m_fLightRadius < 32.0f )
	{
		m_fLightRadius += flTimeLeft * (1.0f + random->RandomFloat() * 36.0f);
		if (m_fLightRadius > 32.0f)
			m_fLightRadius = 100.0f;
	}

	m_pDLight->radius = baseScale * 120 * (m_fLightRadius/32.0f);

	if ( flTimeLeft > 4.0f )
	{
		m_pDLight->die = gpGlobals->curtime + 30.0f;
	}


	// spehere bubble models
	if ( !ShouldSpawnSphere() || !m_hSphereModel.Get() )
		return;

	C_BaseAnimating *pSphere = static_cast<C_BaseAnimating*>( m_hSphereModel.Get() );
	if ( pSphere )
	{
		float	flTimeLeft = m_flTimeBurnOut - gpGlobals->curtime;

		float flScale = GetSphereScale();

		if ( m_flTimeCreated > 0 && ( gpGlobals->curtime - m_flTimeCreated ) < 0.25f )
		{
			pSphere->SetModelScale( MIN( ((gpGlobals->curtime - m_flTimeCreated)/0.25f)*flScale, 1.0f ) );
		}
		else
		{
			pSphere->SetModelScale( flScale );
		}

		if ( flTimeLeft < 0.25f )
		{
			pSphere->SetModelScale( MAX( (flTimeLeft/0.25f)*flScale, 0.01f ) );
		}
	}

	if ( m_fUpdateAttachFXTime < gpGlobals->curtime )
	{
		AOEGrenTargetFXList_t::IndexLocalType_t i = m_hAOETargetEffects.Head();
		while ( m_hAOETargetEffects.IsValidIndex(i) )
		{
			// Are we still buffing this target?
			for ( int target = 0; target < m_hAOETargets.Count(); target++ )
			{
				C_BaseEntity *pTarget = m_hAOETargets[target].Get();
				if ( m_hAOETargetEffects[i].hTarget.Get() == pTarget && m_hAOETargetEffects[i].pEffect )
					UpdateParticleAttachments( m_hAOETargetEffects[i].pEffect, pTarget );	
			}

			i = m_hAOETargetEffects.Next( i );
		}

		/*
		for ( int i = 0; i < m_hAOETargets.Count(); i++ )
		{
			C_BaseEntity *pTarget = m_hAOETargets[i].Get();

			// Loops through the aoe targets, and make sure we have an effect for each of them
			if ( pTarget )
			{
				bool bHaveEffect = false;

				for ( AOEGrenTargetFXList_t::IndexLocalType_t j = m_hAOETargetEffects.Head() ;
					m_hAOETargetEffects.IsValidIndex(j) ;
					i = m_hAOETargetEffects.Next(j) )
				{
					if ( m_hAOETargetEffects[j].hTarget.Get() == pTarget && m_hAOETargetEffects[j].pEffect )
					{
						UpdateParticleAttachments( m_hAOETargetEffects[j].pEffect, pTarget );
						break;
					}
				}
			}
		}
		*/

		m_fUpdateAttachFXTime = gpGlobals->curtime + 0.5f;
	}
}

void C_ASW_AOEGrenade_Projectile::OnRestore()
{
	BaseClass::OnRestore();
	SoundInit();	
}

void C_ASW_AOEGrenade_Projectile::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	SoundShutdown();
}

void C_ASW_AOEGrenade_Projectile::SoundInit()
{
	// play aoegrenade start sound!!
	CPASAttenuationFilter filter( this );

	EmitSound( GetActivateSoundName() );

	// Bring up the aoegrenade burning loop sound
	if( !m_pLoopedSound )
	{
		m_pLoopedSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), GetIdleLoopSoundName() );
		CSoundEnvelopeController::GetController().Play( m_pLoopedSound, 0.0, 100 );
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pLoopedSound, 0.6, 2.0 );
	}
}

void C_ASW_AOEGrenade_Projectile::SoundShutdown()
{	
	AOEGrenTargetFXList_t::IndexLocalType_t i = m_hAOETargetEffects.Head();
	while ( m_hAOETargetEffects.IsValidIndex(i) )
	{
		// Are we still buffing this target?
		for ( int target = 0; target < m_hAOETargets.Count(); target++ )
		{
			if ( m_hAOETargetEffects[i].pBuffLoopSound )
			{
				CSoundEnvelopeController::GetController().SoundDestroy( m_hAOETargetEffects[i].pBuffLoopSound );
				m_hAOETargetEffects[i].pBuffLoopSound = NULL;
			}

			/*
			C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>( m_hAOETargets[target].Get() );
			if ( pMarine && pMarine->GetCommander() )
			{
				C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
				if ( pMarine->GetCommander() == pLocalPlayer && pMarine->IsInhabited() && m_hAOETargetEffects[i].pBuffLoopSound )
				{
					CSoundEnvelopeController::GetController().SoundDestroy( m_hAOETargetEffects[i].pBuffLoopSound );
					m_hAOETargetEffects[i].pBuffLoopSound = NULL;
				}
			}
			*/
		}

		m_hAOETargetEffects.Remove(i);
	}

	if ( m_pLoopedSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pLoopedSound );
		m_pLoopedSound = NULL;
	}
}
