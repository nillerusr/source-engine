//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "basegrenade_shared.h"
#include "IEffects.h"
#include "c_baseplayer.h"

extern ConVar	lod_effect_distance;

//-----------------------------------------------------------------------------
// Purpose: Client side entity for the ferry target items
//-----------------------------------------------------------------------------
class C_GrenadeObjectSapper : public C_BaseGrenade
{
	DECLARE_CLASS( C_GrenadeObjectSapper, C_BaseGrenade );
public:
	DECLARE_CLIENTCLASS();

	C_GrenadeObjectSapper();

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

public:
	C_GrenadeObjectSapper( const C_GrenadeObjectSapper & );

	bool		m_bSapping;
	float		m_flNextEffectTime;
};

IMPLEMENT_CLIENTCLASS_DT(C_GrenadeObjectSapper, DT_GrenadeObjectSapper, CGrenadeObjectSapper)
	RecvPropInt(RECVINFO(m_bSapping)),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_GrenadeObjectSapper::C_GrenadeObjectSapper( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_GrenadeObjectSapper::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Only think when sapping
	if ( m_bSapping )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		m_flNextEffectTime = gpGlobals->curtime;
	}
	else
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spawn effects if I'm sapping
//-----------------------------------------------------------------------------
void C_GrenadeObjectSapper::ClientThink( void )
{
	if ( m_flNextEffectTime < gpGlobals->curtime )
	{
		// Haxory LOD
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() < lod_effect_distance.GetFloat() )
		{
			g_pEffects->Sparks( GetAbsOrigin() );
		}

		m_flNextEffectTime = gpGlobals->curtime + 0.3;
	}
}
