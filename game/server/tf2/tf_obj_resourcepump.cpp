//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Resource pump
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_obj.h"
#include "tf_obj_resourcepump.h"
#include "tf_func_resource.h"
#include "resource_chunk.h"
#include "techtree.h"
#include "sendproxy.h"
#include "vstdlib/random.h"
#include "tf_stats.h"
#include "VGuiScreen.h"
#include "engine/IEngineSound.h"

BEGIN_DATADESC( CObjectResourcePump )

	DEFINE_THINKFUNC( ResourcePumpThink ),

END_DATADESC()


// Resource pump team-only vars.
BEGIN_SEND_TABLE_NOBASE( CObjectResourcePump, DT_ResourcePumpTeamOnlyVars )
	SendPropInt( SENDINFO(m_iPumpLevel), 4 ),
	SendPropEHandle( SENDINFO(m_hResourceZone) ),
END_SEND_TABLE()


IMPLEMENT_SERVERCLASS_ST(CObjectResourcePump, DT_ResourcePump)
	SendPropDataTable( "teamonly", 0, &REFERENCE_SEND_TABLE( DT_ResourcePumpTeamOnlyVars ), SendProxy_OnlyToTeam ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(obj_resourcepump, CObjectResourcePump);
PRECACHE_REGISTER(obj_resourcepump);

ConVar	obj_resourcepump_health( "obj_resourcepump_health","100", FCVAR_NONE, "Resource pump health" );
ConVar  obj_resourcepump_rate( "obj_resourcepump_rate","0", FCVAR_NONE, "Base rate at which resource pumps give resources to their team." );
ConVar  obj_resourcepump_amount( "obj_resourcepump_amount","0", FCVAR_NONE, "Base amount of resources that resource pumps give to their team." );

#define RESOURCE_PUMP_CONTEXT		"ResourcePumpThink"

#define HUMAN_RESOURCEPUMP_MODEL		"models/objects/obj_resourcepump.mdl"
#define ALIEN_RESOURCEPUMP_MODEL		"models/objects/alien_obj_resourcepump.mdl"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectResourcePump::CObjectResourcePump()
{
	UseClientSideAnimation();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResourcePump::SetupTeamModel( void )
{
	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		SetModel( HUMAN_RESOURCEPUMP_MODEL );
	}
	else
	{
		SetModel( ALIEN_RESOURCEPUMP_MODEL );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResourcePump::Spawn( void )
{
	SetSolid( SOLID_BBOX );
	UTIL_SetSize(this, RESOURCEPUMP_MINS, RESOURCEPUMP_MAXS);

	m_iHealth = obj_resourcepump_health.GetInt();

	m_hResourceZone = NULL;
	m_flPumpSpeed = obj_resourcepump_rate.GetFloat();
	m_fObjectFlags |= OF_DONT_PREVENT_BUILD_NEAR_OBJ | OF_DOESNT_NEED_POWER | OF_MUST_BE_BUILT_IN_RESOURCE_ZONE | OF_MUST_BE_BUILT_ON_ATTACHMENT;
	m_iPumpLevel = 1;

	SetType( OBJ_RESOURCEPUMP );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResourcePump::Activate( void )
{
	BaseClass::Activate();

	SetupPump();
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectResourcePump::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_obj_resourcepump";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResourcePump::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	SetupPump();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResourcePump::SetupPump( void )
{
	// Find the resource zone I've been planted in
	m_hResourceZone = NULL;
	CBaseEntity *pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "trigger_resourcezone" )) != NULL)
	{
		CResourceZone *pZone = (CResourceZone *)pEntity;

		// Are we within this zone?
		if ( pZone->CollisionProp()->IsPointInBounds( GetAbsOrigin() ) )
		{
			m_hResourceZone = pZone;
			break;
		}
	}

	if ( m_hResourceZone == NULL )
	{
		Msg( "Resource Pump (entindex %d) at (%.2f, %.2f, %.2f) can't find it's zone.\n", 
			entindex(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove( this );
		return;
	}

	// Tell the zone
	if ( m_hResourceZone->GetNumBuildPoints() )
	{
		m_hResourceZone->SetObjectOnBuildPoint( 0, this );
	}

	SetContextThink( ResourcePumpThink, gpGlobals->curtime + m_flPumpSpeed, RESOURCE_PUMP_CONTEXT );

	// Start the pump animation
	ResetSequence( SelectWeightedSequence( ACT_IDLE ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectResourcePump::Precache()
{
	PrecacheModel( HUMAN_RESOURCEPUMP_MODEL );
	PrecacheModel( ALIEN_RESOURCEPUMP_MODEL );
	PrecacheVGuiScreen( "screen_obj_resourcepump" );

	PrecacheScriptSound( "ObjectResourcePump.UpgradeFailed" );

}

//-----------------------------------------------------------------------------
// Gets the resource zone (may be NULL!)
//-----------------------------------------------------------------------------
CResourceZone* CObjectResourcePump::GetResourceZone()
{
	return m_hResourceZone;
}


//-----------------------------------------------------------------------------
// Purpose: Pump resources out of the zone I'm sitting on
//-----------------------------------------------------------------------------
void CObjectResourcePump::ResourcePumpThink( void )
{
	Assert( m_hResourceZone != NULL );
	if ( !GetTeam() )
		return;

	float flSpeed = m_hResourceZone->GetResourceRate() ? m_hResourceZone->GetResourceRate() : m_flPumpSpeed;

	SetNextThink( gpGlobals->curtime + flSpeed, RESOURCE_PUMP_CONTEXT );

	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	// If the zone's not active, don't do anything
	if ( !m_hResourceZone->GetActive() )
		return;

	// Suck resources from the zone I'm in
	int iResourcesPerPlayer = 0;
	for ( int i = 0; i < m_iPumpLevel; i++ )
	{
		// Decreasing value for each level
		float flLevelBonus = 1.0 - (i / (float)GetObjectInfo( GetType() )->m_MaxUpgradeLevel);
		iResourcesPerPlayer += (obj_resourcepump_amount.GetFloat() * flLevelBonus);
	}
	int iPumpedResources = MIN( m_hResourceZone->GetResources(), iResourcesPerPlayer );
	if ( iPumpedResources )
	{
		m_hResourceZone->RemoveResources( iPumpedResources );

		// Give out resources to the team
		GetTFTeam()->AddTeamResources( iPumpedResources * GetTFTeam()->GetNumPlayers() );
		TFStats()->IncrementTeamStat( GetTFTeam()->GetTeamNumber(), TF_TEAM_STAT_RESOURCES_HARVESTED, iPumpedResources );
	}

	// If we've just run out of resources in the zone, shut down
	if ( m_hResourceZone->GetResources() <= 0 )
	{
		UTIL_Remove( this );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle commands sent from vgui panels on the client 
//-----------------------------------------------------------------------------
bool CObjectResourcePump::ClientCommand( CBaseTFPlayer *pPlayer, const char *pCmd, ICommandArguments *pArg )
{
	if ( FStrEq( pCmd, "upgrade" ) )
	{
		int iCost = CalculateObjectUpgrade( GetType(), m_iPumpLevel );

		// Do we have enough resources to activate it?
		if ( !iCost || pPlayer->GetBankResources() < iCost )
		{
			// Play a sound indicating it didn't work...
			CSingleUserRecipientFilter filter( pPlayer );
			EmitSound( filter, pPlayer->entindex(), "ObjectResourcePump.UpgradeFailed" );
			return true;
		}

		pPlayer->RemoveBankResources( iCost );

		// Upgrade myself
		m_iPumpLevel += 1;
		return true;
	}

	return BaseClass::ClientCommand( pPlayer, pCmd, pArg );
}
