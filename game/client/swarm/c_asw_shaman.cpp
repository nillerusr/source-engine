#include "cbase.h"
#include "c_baseentity.h"
#include "c_asw_shaman.h"
#include "c_asw_clientragdoll.h"
#include "asw_fx_shared.h"
#include "functionproxy.h"
#include "imaterialproxydict.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Shaman, DT_ASW_Shaman, CASW_Shaman)
	RecvPropEHandle		( RECVINFO( m_hHealingTarget ) ),
END_RECV_TABLE()

C_ASW_Shaman::C_ASW_Shaman()
{
 
}


C_ASW_Shaman::~C_ASW_Shaman()
{
	
}

void C_ASW_Shaman::SpawnClientSideEffects()
{
	//ParticleProp()->Create( "meatbug_death", PATTACH_POINT, "attach_explosion" );
}

C_BaseAnimating * C_ASW_Shaman::BecomeRagdollOnClient( void )
{
	if ( m_pHealEffect )
	{
		m_pHealEffect->StopEmission( false, true, false );
		m_pHealEffect = NULL;
	}

	SpawnClientSideEffects();	
	return BaseClass::BecomeRagdollOnClient();

}

C_ClientRagdoll *C_ASW_Shaman::CreateClientRagdoll( bool bRestoring )
{
	return new C_ASW_ClientRagdoll( bRestoring );
}

/*
//-----------------------------------------------------------------------------
// Material proxy for Shaman's glow
//-----------------------------------------------------------------------------

class CASW_Shaman_Proxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput m_IllumTint1;
	CFloatInput m_IllumTint2;
	CFloatInput m_IllumTint3;
};


bool CASW_Shaman_Proxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;
	SetFloatResult( 1 );
	return true;
}

void CASW_Shaman_Proxy::OnBind( void *pC_BaseEntity )
{
	Assert( m_pResult );

	C_ASW_Shaman *pShaman = static_cast<C_ASW_Shaman*>( BindArgToEntity( pC_BaseEntity ) );

	if ( pShaman->m_bChampion )
		SetFloatResult( 4 );

}

EXPOSE_MATERIAL_PROXY( CASW_Shaman_Proxy, ChampionGlow );
*/

void C_ASW_Shaman::ClientThink()
{
	BaseClass::ClientThink();

	UpdateEffects();
}

void C_ASW_Shaman::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	if ( m_pHealEffect )
	{
		m_pHealEffect->StopEmission();
		m_pHealEffect = NULL;
	}
}

void C_ASW_Shaman::UpdateEffects()
{
	if ( !m_hHealingTarget.Get() || GetHealth() <= 0 )
	{
		if ( m_pHealEffect )
		{
			m_pHealEffect->StopEmission();
			m_pHealEffect = NULL;
		}
		return;
	}

	if ( m_pHealEffect )
	{
		if ( m_pHealEffect->GetControlPointEntity( 1 ) != m_hHealingTarget.Get() )
		{
			m_pHealEffect->StopEmission();
			m_pHealEffect = NULL;
		}
	}
			
	if ( !m_pHealEffect )
	{				
		m_pHealEffect = ParticleProp()->Create( "shaman_heal_attach", PATTACH_POINT_FOLLOW, "nozzle" );	// "heal_receiver"
	}

	Assert( m_pHealEffect );

	if ( m_pHealEffect->GetControlPointEntity( 1 ) == NULL )
	{
		C_BaseEntity *pTarget = m_hHealingTarget.Get();
		Vector vOffset( 0.0f, 0.0f, pTarget->WorldSpaceCenter().z - pTarget->GetAbsOrigin().z );

		ParticleProp()->AddControlPoint( m_pHealEffect, 1, pTarget, PATTACH_ABSORIGIN_FOLLOW, NULL, vOffset );
		m_pHealEffect->SetControlPointOrientation( 0, pTarget->Forward(), -pTarget->Left(), pTarget->Up() );
	}
}