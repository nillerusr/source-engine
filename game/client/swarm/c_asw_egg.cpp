#include "cbase.h"
#include "c_asw_egg.h"
#include "baseparticleentity.h"
#include "c_asw_generic_emitter_entity.h"
#include "c_asw_player.h"
#include "asw_util_shared.h"
#include "functionproxy.h"
#include "asw_fx_shared.h"
#include "takedamageinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Egg, DT_ASW_Egg, CASW_Egg)
	RecvPropBool( RECVINFO( m_bOnFire ) ),
	RecvPropFloat( RECVINFO( m_fEggAwake ) ),
END_RECV_TABLE()


C_ASW_Egg::C_ASW_Egg()
	: m_GlowObject( this )
{
	m_bClientOnFire = false;
	m_pBurningEffect = NULL;
	m_fEggAwake = 0;

	m_GlowObject.SetColor( Vector( 0.3f, 0.6f, 0.1f ) );
	m_GlowObject.SetAlpha( 0.55f );
	m_GlowObject.SetRenderFlags( false, false );
	m_GlowObject.SetFullBloomRender( true );
}

C_ASW_Egg::~C_ASW_Egg()
{
	m_bClientOnFire = false;
	UpdateFireEmitters();
}

void C_ASW_Egg::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	m_bOnFire = false;
	UpdateFireEmitters();
}

void C_ASW_Egg::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	UpdateFireEmitters();

	if ( type == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void C_ASW_Egg::UpdateFireEmitters()
{
	bool bOnFire = (m_bOnFire && !IsEffectActive(EF_NODRAW));
	if (bOnFire != m_bClientOnFire)
	{
		m_bClientOnFire = bOnFire;
		if (m_bClientOnFire)
		{
			if ( !m_pBurningEffect )
			{
				m_pBurningEffect = UTIL_ASW_CreateFireEffect( this );
			}
			EmitSound( "ASWFire.BurningFlesh" );
		}
		else
		{
			if ( m_pBurningEffect )
			{
				ParticleProp()->StopEmission( m_pBurningEffect );
				m_pBurningEffect = NULL;
			}
			StopSound("ASWFire.BurningFlesh");
			if ( C_BaseEntity::IsAbsQueriesValid() )
				EmitSound("ASWFire.StopBurning");
		}		
	}
}

void C_ASW_Egg::ClientThink()
{
	BaseClass::ClientThink();

	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer && pPlayer->IsSniperScopeActive() )
	{
		m_GlowObject.SetRenderFlags( true, true );
	}
	else
	{
		m_GlowObject.SetRenderFlags( false, false );
	}
}


//-----------------------------------------------------------------------------
// Material proxy for egg line glow
//-----------------------------------------------------------------------------

// sinePeriod: time that it takes to go through whole sine wave in seconds (default: 1.0f)
// sineMax : the max value for the sine wave (default: 1.0f )
// sineMin: the min value for the sine wave  (default: 0.0f )
class CASW_Egg_Proxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput m_SinePeriod;
	CFloatInput m_SineMax;
	CFloatInput m_SineMin;
	CFloatInput m_SineTimeOffset;
};


bool CASW_Egg_Proxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_SinePeriod.Init( pMaterial, pKeyValues, "sinePeriod", 1.0f ))
		return false;
	if (!m_SineMax.Init( pMaterial, pKeyValues, "sineMax", 1.0f ))
		return false;
	if (!m_SineMin.Init( pMaterial, pKeyValues, "sineMin", 0.0f ))
		return false;
	if (!m_SineTimeOffset.Init( pMaterial, pKeyValues, "timeOffset", 0.0f ))
		return false;

	return true;
}

void CASW_Egg_Proxy::OnBind( void *pC_BaseEntity )
{
	Assert( m_pResult );

	C_ASW_Egg *pEgg = static_cast<C_ASW_Egg*>( BindArgToEntity( pC_BaseEntity ) );

	float flValue;
	float flSineTimeOffset = m_SineTimeOffset.GetFloat();
	float flSineMax = m_SineMax.GetFloat();
	float flSineMin = m_SineMin.GetFloat();
	float flSinePeriod = m_SinePeriod.GetFloat();
	if (flSinePeriod == 0)
		flSinePeriod = 1;

	// get a value in [0,1]
	flValue = ( sin( 2.0f * M_PI * (gpGlobals->curtime - flSineTimeOffset) / flSinePeriod ) * 0.5f ) + 0.5f;
	// get a value in [min,max]	
	flValue = ( flSineMax - flSineMin ) * flValue + flSineMin;	

	flValue *= pEgg->m_fEggAwake;

	SetFloatResult( flValue );
}

EXPOSE_INTERFACE( CASW_Egg_Proxy, IMaterialProxy, "EggAwakeSine" IMATERIAL_PROXY_INTERFACE_VERSION );

void C_ASW_Egg::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	CTakeDamageInfo subInfo = info;

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );	

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK )
		&& !( subInfo.GetDamageType() & DMG_BURN ) )
	{
		Bleed( subInfo, ptr->endpos, vecDir, ptr );
	}

	if( !info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

	AddMultiDamage( subInfo, this );
	UTIL_ASW_ClientFloatingDamageNumber( subInfo );
}

void C_ASW_Egg::Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr )
{
	UTIL_ASW_DroneBleed( vecPos, -vecDir, 4 );
}