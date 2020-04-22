//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include <KeyValues.h>
#include "materialsystem/imaterialvar.h"
#include "C_BaseTFPlayer.h"
#include "functionproxy.h"
#include "C_PlayerResource.h"


//-----------------------------------------------------------------------------
// Returns the player health (from 0 to 1)
//-----------------------------------------------------------------------------
class CPlayerHealthProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pEnt );

private:
	CFloatInput	m_Factor;
};

bool CPlayerHealthProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CPlayerHealthProxy::OnBind( void *pArg )
{
	// NOTE: Player health max is not available on the server...
	C_BaseEntity *pEntity = BindArgToEntity( pArg );
	C_BaseTFPlayer* pPlayer = dynamic_cast<C_BaseTFPlayer*>(pEntity);
	if (!pPlayer)
		return;

	Assert( m_pResult );
	SetFloatResult( pPlayer->HealthFraction() * m_Factor.GetFloat() );

	/*
	// Should we draw their health?
	// If he's not on our team we can't see it unless we're a command with "targetinginfo".
	if ( GetLocalTeam() != GetTeam() && !(local->HasNamedTechnology("targetinginfo") && IsLocalPlayerClass(TFCLASS_COMMANDO) ))
		return drawn;
	// Don't draw health bars above myself
	if ( local == this )
		return drawn;
	// Don't draw health bars over dead/dying player
	if ( GetHealth() <= 0 )
		return drawn;

	return drawn;
	*/
}

EXPOSE_INTERFACE( CPlayerHealthProxy, IMaterialProxy, "PlayerHealth" IMATERIAL_PROXY_INTERFACE_VERSION );


//-----------------------------------------------------------------------------
// A function that returns the time since last being damaged
//-----------------------------------------------------------------------------
class CPlayerDamageTimeProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pEnt );

private:
	CFloatInput	m_Factor;
};

bool CPlayerDamageTimeProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1.0f ))
		return false;

	return true;
}

void CPlayerDamageTimeProxy::OnBind( void *pArg )
{
	C_BaseEntity *pEntity = BindArgToEntity( pArg );

	// NOTE: Player health max is not available on the server...
	C_BaseTFPlayer* pPlayer = dynamic_cast<C_BaseTFPlayer*>(pEntity);
	if (!pPlayer)
	{
		SetFloatResult( 10000 * m_Factor.GetFloat() );
		return;
	}

	Assert( m_pResult );
	float dt = gpGlobals->curtime - pPlayer->GetLastDamageTime();
	SetFloatResult( dt * m_Factor.GetFloat() );
}

EXPOSE_INTERFACE( CPlayerDamageTimeProxy, IMaterialProxy, "PlayerDamageTime" IMATERIAL_PROXY_INTERFACE_VERSION );


//-----------------------------------------------------------------------------
// A function that returns the time since last being healed
//-----------------------------------------------------------------------------
class CPlayerHealTimeProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pEnt );

private:
	CFloatInput	m_Factor;
};

bool CPlayerHealTimeProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1.0f ))
		return false;

	return true;
}

void CPlayerHealTimeProxy::OnBind( void *pArg  )
{
	// NOTE: Player health max is not available on the server...
	C_BaseEntity *pEntity = BindArgToEntity( pArg );
	C_BaseTFPlayer* pPlayer = dynamic_cast<C_BaseTFPlayer*>(pEntity);
	if (!pPlayer)
		return;

	Assert( m_pResult );
	float dt = gpGlobals->curtime - pPlayer->GetLastGainHealthTime();
	SetFloatResult( dt * m_Factor.GetFloat() );
}

EXPOSE_INTERFACE( CPlayerHealTimeProxy, IMaterialProxy, "PlayerHealTime" IMATERIAL_PROXY_INTERFACE_VERSION );


//-----------------------------------------------------------------------------
// Returns the player score
//-----------------------------------------------------------------------------
class CPlayerScoreProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pEntity );

private:
	CFloatInput	m_Factor;
};

bool CPlayerScoreProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CPlayerScoreProxy::OnBind( void *pArg )
{
	// Find the view angle between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pArg );
	C_BaseTFPlayer* pPlayer = dynamic_cast<C_BaseTFPlayer*>(pEntity);
	if (!pPlayer)
		return;

	if ( !g_PR )
		return;

	int score = g_PR->GetPlayerScore(pPlayer->index);

	Assert( m_pResult );
	SetFloatResult( score * m_Factor.GetFloat() );
}

EXPOSE_INTERFACE( CPlayerScoreProxy, IMaterialProxy, "PlayerScore" IMATERIAL_PROXY_INTERFACE_VERSION );


