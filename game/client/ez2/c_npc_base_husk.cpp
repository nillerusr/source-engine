//=============================================================================//
//
// Purpose:		Combine husks
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "particle_parse.h"
#include "functionproxy.h"
#include "toolframework_client.h"
#include "ez2/npc_husk_base_shared.h"

//ConVar cl_husk_eye_health_flicker( "cl_husk_eye_health_flicker", "1" );
//ConVar cl_husk_eye_show_aggression( "cl_husk_eye_show_aggression", "0" );

class C_NPC_BaseHusk : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_BaseHusk, C_AI_BaseNPC );

	C_NPC_BaseHusk();
	~C_NPC_BaseHusk();

	void	OnDataChanged( DataUpdateType_t type );

	int		m_nHuskAggressionLevel;
	int		m_nHuskCognitionFlags;
};

//-----------------------------------------------------------------------------

class C_NPC_HuskSoldier : public C_NPC_BaseHusk
{
public:
	DECLARE_CLASS( C_NPC_HuskSoldier, C_NPC_BaseHusk );
	DECLARE_CLIENTCLASS();
};

LINK_ENTITY_TO_CLASS( npc_husk_soldier, C_NPC_HuskSoldier );

IMPLEMENT_CLIENTCLASS_DT( C_NPC_HuskSoldier, DT_NPC_HuskSoldier, CNPC_HuskSoldier )
	RecvPropInt( RECVINFO( m_nHuskAggressionLevel ) ),
	RecvPropInt( RECVINFO( m_nHuskCognitionFlags ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------

class C_NPC_HuskPolice : public C_NPC_BaseHusk
{
public:
	DECLARE_CLASS( C_NPC_HuskPolice, C_NPC_BaseHusk );
	DECLARE_CLIENTCLASS();
};

LINK_ENTITY_TO_CLASS( npc_husk_police, C_NPC_HuskPolice );

IMPLEMENT_CLIENTCLASS_DT( C_NPC_HuskPolice, DT_NPC_HuskPolice, CNPC_HuskPolice )
	RecvPropInt( RECVINFO( m_nHuskAggressionLevel ) ),
	RecvPropInt( RECVINFO( m_nHuskCognitionFlags ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------

C_NPC_BaseHusk::C_NPC_BaseHusk()
{
}

C_NPC_BaseHusk::~C_NPC_BaseHusk()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_NPC_BaseHusk::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
}

//-----------------------------------------------------------------------------
// Controls the husk's helmet
//-----------------------------------------------------------------------------
class CProxyHuskHead : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CProxyHuskHead::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	return true;
}

void CProxyHuskHead::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		if ( pEntity->IsNPC() )
		{
			// TODO: Something more efficient?
			C_NPC_BaseHusk *pHusk = dynamic_cast<C_NPC_BaseHusk *>( pEntity );
			if ( pHusk && (pHusk->m_nHuskCognitionFlags & bits_HUSK_COGNITION_BLIND) )
			{
				// Blind husks have no visible eyes
				SetFloatResult( -1.0f );
			}
			else
			{
				//if (pEntity->GetHealth() < pEntity->GetMaxHealth() && cl_husk_eye_health_flicker.GetBool())
				//{
				//	SetFloatResult( RandomFloat( ((float)pEntity->GetHealth()) / ((float)pEntity->GetMaxHealth()), 1.0f ) );
				//}
				//else
				{
					SetFloatResult( 1.0f );
				}
			}
		}
		else
		{
			SetFloatResult( -1.0f );
		}
	}
	else
	{
		SetFloatResult( -1.0f );
	}

	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

EXPOSE_INTERFACE( CProxyHuskHead, IMaterialProxy, "HuskHead" IMATERIAL_PROXY_INTERFACE_VERSION );
