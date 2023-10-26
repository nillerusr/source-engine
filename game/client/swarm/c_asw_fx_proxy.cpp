//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_asw_alien.h"
#include "c_asw_physics_prop_statue.h"
#include "c_asw_mesh_emitter_entity.h"
#include "c_asw_egg.h"
#include "c_asw_buzzer.h"
#include "c_asw_marine.h"
#include "c_asw_clientragdoll.h"

#include "ProxyEntity.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include "materialsystem/IMaterialSystem.h"
#include <KeyValues.h>

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Material proxy for changing the material of aliens
//-----------------------------------------------------------------------------

class CASW_Model_FX_Proxy : public CEntityMaterialProxy
{
public:
	CASW_Model_FX_Proxy( void );
	virtual				~CASW_Model_FX_Proxy( void );
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pEnt );
	void UpdateEffects( bool bShockBig, bool bOnFire, float flFrozen );
	void TextureTransform( float flSpeed = 0, float flScale = 6.0f );
	virtual IMaterial *	GetMaterial();

private:
	ITexture*	m_pFXTexture;

	//	"$detailscale" "5"
	//"$detailblendfactor" 1.0
	//	"$detailblendmode" 6
	IMaterialVar		*m_pDetailMaterial;
	IMaterialVar		*m_pDetailScale;
	IMaterialVar		*m_pDetailBlendFactor;
	IMaterialVar		*m_pDetailBlendMode;
	IMaterialVar		*m_pTextureScrollVar;
	bool m_bOnFire;
	bool m_bFrozen;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Model_FX_Proxy::CASW_Model_FX_Proxy( void )
{
	m_pFXTexture = NULL;
	m_pDetailMaterial = NULL;
	m_pDetailScale = NULL;
	m_pDetailBlendFactor = NULL;
	m_pDetailBlendMode = NULL;
	m_pTextureScrollVar = NULL;
	m_bOnFire = false;
	m_bFrozen = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Model_FX_Proxy::~CASW_Model_FX_Proxy( void )
{
}

bool CASW_Model_FX_Proxy::Init( IMaterial *pMaterial, KeyValues* pKeyValues )
{
	Assert( pMaterial );

	m_bFrozen = false;
	m_bOnFire = false;

	// Need to get the material var
	bool bDetail;
	m_pDetailMaterial = pMaterial->FindVar( "$detail", &bDetail );

	bool bScale;
	m_pDetailScale = pMaterial->FindVar( "$detailscale", &bScale );

	bool bBlendFact;
	m_pDetailBlendFactor = pMaterial->FindVar( "$detailblendfactor", &bBlendFact );

	bool bBlendMode;
	m_pDetailBlendMode = pMaterial->FindVar( "$detailblendmode", &bBlendMode );

	char const* pScrollVarName = pKeyValues->GetString( "texturescrollvar" );
	if( !pScrollVarName )
		return false;

	bool bScrollVar;
	m_pTextureScrollVar = pMaterial->FindVar( "$detailtexturetransform", &bScrollVar );

	return ( bDetail && bScale && bBlendFact && bBlendMode && bScrollVar );
}

void CASW_Model_FX_Proxy::OnBind( C_BaseEntity *pEnt )
{
	// crashing here because pC_BaseEntity is passed as null?
	if ( !pEnt )
		return;

	C_ASW_Mesh_Emitter *pGib = dynamic_cast<C_ASW_Mesh_Emitter*>( pEnt );
	if ( pGib && pGib->m_bFrozen )
	{
		m_pFXTexture = materials->FindTexture( "effects/model_layer_ice_1", TEXTURE_GROUP_MODEL );//
		if ( m_pFXTexture )
		{
			m_pDetailMaterial->SetTextureValue( m_pFXTexture );
		}
		m_pDetailBlendFactor->SetFloatValue( 0.4f );
		TextureTransform( 0 /*speed*/, 5.0f );
		return;
	}

	C_ASWStatueProp *pStatue = dynamic_cast<C_ASWStatueProp*>( pEnt );
	if ( pStatue )
	{
		m_pFXTexture = materials->FindTexture( "effects/model_layer_ice_1", TEXTURE_GROUP_MODEL );//
		if ( m_pFXTexture )
		{
			m_pDetailMaterial->SetTextureValue( m_pFXTexture );
		}
		m_pDetailBlendFactor->SetFloatValue( 0.4f );
		TextureTransform( 0, 5.0f );
		return;
	}

	bool	bShockBig	= false;
	bool	bOnFire		= false;
	float	flFrozen	= 0;

	//C_ASW_ClientRagdoll
	C_ASW_Alien *pAlien = dynamic_cast<C_ASW_Alien*>( pEnt );
	if ( pAlien )
	{
		bShockBig	= pAlien->m_bElectroStunned;
		bOnFire		= pAlien->m_bOnFire;
		flFrozen	= pAlien->GetMoveType() == MOVETYPE_NONE ? 0.0f : pAlien->GetFrozenAmount();
		//Msg( " alien %d shock = %d fire = %d frozen = %f\n", pAlien->entindex(), bShockBig, bOnFire, flFrozen );
		UpdateEffects( bShockBig, bOnFire, flFrozen );
		return;
	}

	C_ASW_Marine *pMarine = C_ASW_Marine::AsMarine( pEnt );
	if ( pMarine )
	{
		//bShockBig	= pMarine->m_bElectroStunned;
		bOnFire		= pMarine->m_bOnFire;
		flFrozen	= pMarine->GetFrozenAmount();
		UpdateEffects( false, bOnFire, flFrozen );
		return;
	}

	C_ASW_Egg *pEgg = dynamic_cast<C_ASW_Egg*>( pEnt );
	if ( pEgg )
	{
		bOnFire		= pEgg->m_bOnFire;
		flFrozen	= pEgg->GetFrozenAmount();
		UpdateEffects( false, bOnFire, flFrozen );
		return;
	}

	C_ASW_Buzzer *pBuzzer = dynamic_cast<C_ASW_Buzzer*>( pEnt );
	if ( pBuzzer )
	{
		bShockBig	= pBuzzer->m_bElectroStunned;
		bOnFire		= pBuzzer->m_bOnFire;
		flFrozen	= pBuzzer->GetMoveType() == MOVETYPE_NONE ? 0.0f : pBuzzer->GetFrozenAmount();
		UpdateEffects( bShockBig, bOnFire, flFrozen );
		return;
	}

	C_ASW_ClientRagdoll *pRagDoll = dynamic_cast<C_ASW_ClientRagdoll*>( pEnt );
	if ( pRagDoll )
	{
		bShockBig	= pRagDoll->m_bElectroShock;
		bOnFire		= !!(pRagDoll->GetFlags() & FL_ONFIRE);
		UpdateEffects( bShockBig, bOnFire, false );
		return;
	}

	/*
	C_BaseAnimating *pBaseAnimating = dynamic_cast<C_BaseAnimating*>( pEnt );
	if ( pBaseAnimating )
	{
		flFrozen	= pBaseAnimating->GetFrozenAmount();
		UpdateEffects( false, false, flFrozen );
		return;
	}
	*/

	m_pDetailBlendFactor->SetFloatValue( 0.0f );
}

void CASW_Model_FX_Proxy::UpdateEffects( bool bShockBig, bool bOnFire, float flFrozen )
{
	if ( bShockBig || bOnFire || flFrozen > 0 )
	{
		if ( bShockBig )
		{
			m_pFXTexture = materials->FindTexture( "effects/model_layer_shock_1", TEXTURE_GROUP_MODEL );//
			if ( m_pFXTexture )
			{
				float flBlend = 0.75f;
				m_pDetailBlendFactor->SetFloatValue( flBlend );
				m_pDetailMaterial->SetTextureValue( m_pFXTexture );
				TextureTransform( 80, 4.0f );
			}
		}
		else if ( flFrozen > 0 )
		{
			m_pFXTexture = materials->FindTexture( "effects/model_layer_ice_1", TEXTURE_GROUP_MODEL );//
			if ( m_pFXTexture )
			{
				m_pDetailBlendFactor->SetFloatValue( MIN( 0.4f, flFrozen/4) );
				m_pDetailMaterial->SetTextureValue( m_pFXTexture );
				TextureTransform( 0, 5.0f );
			}
		}
		else if ( bOnFire )
		{
			m_pFXTexture = materials->FindTexture( "effects/TiledFire/fire_tiled", TEXTURE_GROUP_MODEL );//
			if ( m_pFXTexture )
			{
				m_pDetailBlendFactor->SetFloatValue( 0.3f );
				m_pDetailMaterial->SetTextureValue( m_pFXTexture );
				TextureTransform( 24, 6.0f );
			}
		}
	}
	else
	{
		m_pDetailBlendFactor->SetFloatValue( 0.0f );
	}
}

void CASW_Model_FX_Proxy::TextureTransform( float flSpeed, float flScale )
{
	// scrolling of the detail material
	float flRate			= abs( flSpeed ) / 128.0;
	float flAngle			= (flSpeed >= 0) ? 180 : 0;

	float sOffset = gpGlobals->curtime * cos( flAngle * ( M_PI / 180.0f ) ) * flRate;
	float tOffset = gpGlobals->curtime * sin( flAngle * ( M_PI / 180.0f ) ) * flRate;

	// make sure that we are positive
	if( sOffset < 0.0f )
	{
		sOffset += 1.0f + -( int )sOffset;
	}
	if( tOffset < 0.0f )
	{
		tOffset += 1.0f + -( int )tOffset;
	}

	// make sure that we are in a [0,1] range
	sOffset = sOffset - ( int )sOffset;
	tOffset = tOffset - ( int )tOffset;

	if (m_pTextureScrollVar->GetType() == MATERIAL_VAR_TYPE_MATRIX)
	{
		VMatrix mat;
		MatrixBuildTranslation( mat, sOffset, tOffset, 0.0f );
		m_pTextureScrollVar->SetMatrixValue( mat );
	}
	else
	{
		m_pTextureScrollVar->SetVecValue( sOffset, tOffset, 0.0f );
	}

	m_pDetailScale->SetFloatValue( flScale );
}

IMaterial *CASW_Model_FX_Proxy::GetMaterial()
{
	if ( !m_pDetailMaterial )
		return NULL;

	return m_pDetailMaterial->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CASW_Model_FX_Proxy, AlienSurfaceFX );