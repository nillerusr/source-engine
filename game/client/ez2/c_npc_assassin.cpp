//=============================================================================//
//
// Purpose:		Combine assassins restored in Source 2013. Created mostly from scratch
//				as a new Combine soldier variant focused on stealth, flanking, and fast
//				movements. Initially inspired by Black Mesa's female assassins.
// 
//				This was originally created for Entropy : Zero 2.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "particle_parse.h"
#include "functionproxy.h"
#include "toolframework_client.h"

class C_NPC_Assassin : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_Assassin, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	C_NPC_Assassin();
	~C_NPC_Assassin();

	void	OnDataChanged( DataUpdateType_t type );

	ShadowType_t	ShadowCastType();

	inline float	GetCloakFactor() const { return m_flCloakFactor; }

	float m_flCloakFactor = 0.0f;
};

LINK_ENTITY_TO_CLASS( npc_assassin, C_NPC_Assassin );

IMPLEMENT_CLIENTCLASS_DT( C_NPC_Assassin, DT_NPC_Assassin, CNPC_Assassin )
	RecvPropFloat( RECVINFO( m_flCloakFactor ) ),
END_RECV_TABLE()

C_NPC_Assassin::C_NPC_Assassin()
{
}

C_NPC_Assassin::~C_NPC_Assassin()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_NPC_Assassin::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	// If we ever have to migrate to particles instead of CSpriteTrail, use this code
	// 
	//	if (type == DATA_UPDATE_CREATED)
	//	{
	//		// Turn on eye glow
	//		DispatchParticleEffect( "npc_assassin_eyeglow", PATTACH_POINT_FOLLOW, this, LookupAttachment( "eyes" ) );
	//	}
	//	else if (m_lifeState != LIFE_ALIVE)
	//	{
	//		StopParticleEffects( this );
	//	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ShadowType_t C_NPC_Assassin::ShadowCastType()
{
	// No shadows when cloaked
	if (m_flCloakFactor > 0.0f)
		return SHADOWS_NONE;

	return BaseClass::ShadowCastType();
}

//-----------------------------------------------------------------------------
// Controls assassin cloak
//-----------------------------------------------------------------------------
class CProxyAssassinCloak : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CProxyAssassinCloak::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	return true;
}

void CProxyAssassinCloak::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		if ( pEntity->IsNPC() /*&& V_strstr( pEntity->GetClassname(), "Assassin" )*/ )
		{
			// TODO: Something more efficient?
			C_NPC_Assassin *pAssassin = dynamic_cast<C_NPC_Assassin*>( pEntity );
			if ( pAssassin && pAssassin->GetCloakFactor() > 0.0f )
			{
				SetFloatResult( pAssassin->GetCloakFactor() + RandomFloat( 0.0f, 0.05f ) );
			}
			else
			{
				SetFloatResult( 0.0f );
			}
		}
		else if (pEntity->GetRenderMode() == kRenderNormal && pEntity->GetRenderColor().a < 255)
		{
			// For entities with normal render modes, set the cloak factor to their alpha value
			// (used by ragdolls)
			SetFloatResult( 1.0f - ((float)pEntity->GetRenderColor().a / 255.0f) );
		}
		else
		{
			SetFloatResult( 0.0f );
		}
	}
	else
	{
		SetFloatResult( 0.0f );
	}
	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

EXPOSE_INTERFACE( CProxyAssassinCloak, IMaterialProxy, "AssassinCloak" IMATERIAL_PROXY_INTERFACE_VERSION );
