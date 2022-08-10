//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "func_respawnroom.h"
#include "func_no_build.h"
#include "tf_team.h"
#include "ndebugoverlay.h"
#include "tf_gamerules.h"
#include "entity_tfstart.h"
#include "modelentities.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Visualizes a respawn room to the enemy team
//-----------------------------------------------------------------------------
class CFuncRespawnRoomVisualizer : public CFuncBrush
{
	DECLARE_CLASS( CFuncRespawnRoomVisualizer, CFuncBrush );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void Spawn( void );
	void	InputRoundActivate( inputdata_t &inputdata );
	int		DrawDebugTextOverlays( void );
	CFuncRespawnRoom *GetRespawnRoom( void ) { return m_hRespawnRoom; }

	virtual int		UpdateTransmitState( void );
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual bool	ShouldCollide( int collisionGroup, int contentsMask ) const;

	void SetActive( bool bActive );

protected:
	string_t					m_iszRespawnRoomName;
	CHandle<CFuncRespawnRoom>	m_hRespawnRoom;
};

LINK_ENTITY_TO_CLASS( func_respawnroom, CFuncRespawnRoom);

BEGIN_DATADESC( CFuncRespawnRoom )
	DEFINE_FUNCTION( RespawnRoomTouch ),
	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "SetActive", InputSetActive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetInactive", InputSetInactive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ToggleActive", InputToggleActive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RoundActivate", InputRoundActivate ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CFuncRespawnRoom, DT_FuncRespawnRoom )
END_SEND_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncRespawnRoom::CFuncRespawnRoom()
{
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the resource zone
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::Spawn( void )
{
	AddSpawnFlags(SF_TRIGGER_ALLOW_CLIENTS);

	BaseClass::Spawn();
	InitTrigger();

	SetCollisionGroup( TFCOLLISION_GROUP_RESPAWNROOMS );

	m_bActive = true;
	SetTouch( &CFuncRespawnRoom::RespawnRoomTouch );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::Activate( void )
{
	BaseClass::Activate();
	m_iOriginalTeam = GetTeamNumber();
	SetActive( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pOther - The thing that touched us.
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::RespawnRoomTouch(CBaseEntity *pOther)
{
	if ( PassesTriggerFilters(pOther) )
	{
		if ( pOther->IsPlayer() && InSameTeam( pOther ) )
		{
			// Players carrying the flag drop it if they try to run into a respawn room
			CTFPlayer *pPlayer = ToTFPlayer(pOther);
			if ( pPlayer->HasTheFlag() )
			{
				pPlayer->DropFlag();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::InputSetActive( inputdata_t &inputdata )
{
	SetActive( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::InputSetInactive( inputdata_t &inputdata )
{
	SetActive( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::InputToggleActive( inputdata_t &inputdata )
{
	if ( m_bActive )
	{
		SetActive( false );
	}
	else
	{
		SetActive( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::InputRoundActivate( inputdata_t &input )
{
	if ( m_iOriginalTeam == TEAM_UNASSIGNED )
	{
		ChangeTeam( TEAM_UNASSIGNED );

		// If we don't have a team, find a respawn point inside us that we can derive a team from.
		CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, "info_player_teamspawn" );
		while( pSpot )
		{
			if ( PointIsWithin( pSpot->GetAbsOrigin() ) )
			{
				CTFTeamSpawn *pTFSpawn = assert_cast<CTFTeamSpawn*>(pSpot);
				if ( !pTFSpawn->IsDisabled() && pTFSpawn->GetTeamNumber() > LAST_SHARED_TEAM )
				{
					ChangeTeam( pTFSpawn->GetTeamNumber() );
					break;
				}
			}

			pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_teamspawn" );
		}

		if ( GetTeamNumber() == TEAM_UNASSIGNED )
		{
			DevMsg( "Unassigned %s(%s) failed to find an info_player_teamspawn within it to use.\n", GetClassname(), GetDebugName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::ChangeTeam( int iTeamNum )
{
	BaseClass::ChangeTeam( iTeamNum );

	for ( int i = m_hVisualizers.Count()-1; i >= 0; i-- )
	{
		if ( m_hVisualizers[i] )
		{
			Assert( m_hVisualizers[i]->GetRespawnRoom() == this );
			m_hVisualizers[i]->ChangeTeam( iTeamNum );
		}
		else
		{
			m_hVisualizers.Remove(i);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::SetActive( bool bActive )
{
	m_bActive = bActive;
	if ( m_bActive )
	{
		Enable();
	}
	else
	{
		Disable();
	}

	for ( int i = m_hVisualizers.Count()-1; i >= 0; i-- )
	{
		if ( m_hVisualizers[i] )
		{
			Assert( m_hVisualizers[i]->GetRespawnRoom() == this );
			m_hVisualizers[i]->SetActive( m_bActive );
		}
		else
		{
			m_hVisualizers.Remove(i);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFuncRespawnRoom::GetActive() const
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified point is within this zone
//-----------------------------------------------------------------------------
bool CFuncRespawnRoom::PointIsWithin( const Vector &vecPoint )
{
	Ray_t ray;
	trace_t tr;
	ICollideable *pCollide = CollisionProp();
	ray.Init( vecPoint, vecPoint );
	enginetrace->ClipRayToCollideable( ray, MASK_ALL, pCollide, &tr );
	return ( tr.startsolid );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoom::AddVisualizer( CFuncRespawnRoomVisualizer *pViz )
{
	if ( m_hVisualizers.Find(pViz) == m_hVisualizers.InvalidIndex() )
	{
		m_hVisualizers.AddToTail( pViz );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Is a given point contained within a respawn room?
//-----------------------------------------------------------------------------
bool PointInRespawnRoom( CBaseEntity *pTarget, const Vector &vecOrigin )
{
	// Find out whether we're in a respawn room or not
	CBaseEntity *pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "func_respawnroom" )) != NULL)
	{
		CFuncRespawnRoom *pRespawnRoom = (CFuncRespawnRoom *)pEntity;

		// Are we within this respawn room?
		if ( pRespawnRoom->GetActive() )
		{
			if ( pRespawnRoom->PointIsWithin( vecOrigin ) )
			{
				if ( !pTarget || pRespawnRoom->GetTeamNumber() == TEAM_UNASSIGNED || pRespawnRoom->InSameTeam( pTarget ) )
					return true;
			}
			else 
			{
				if ( pTarget && pRespawnRoom->IsTouching(pTarget) )
					return true;
			}
		}
	}

	return false;
}

//===========================================================================================================

LINK_ENTITY_TO_CLASS( func_respawnroomvisualizer, CFuncRespawnRoomVisualizer);

BEGIN_DATADESC( CFuncRespawnRoomVisualizer )
	DEFINE_KEYFIELD( m_iszRespawnRoomName, FIELD_STRING, "respawnroomname" ),
	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "RoundActivate", InputRoundActivate ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CFuncRespawnRoomVisualizer, DT_FuncRespawnRoomVisualizer )
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoomVisualizer::Spawn( void )
{
	BaseClass::Spawn();

	SetActive( true );

	SetCollisionGroup( TFCOLLISION_GROUP_RESPAWNROOMS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoomVisualizer::InputRoundActivate( inputdata_t &inputdata )
{
	if ( m_iszRespawnRoomName != NULL_STRING )
	{
		m_hRespawnRoom = dynamic_cast<CFuncRespawnRoom*>(gEntList.FindEntityByName( NULL, m_iszRespawnRoomName ));
		if ( m_hRespawnRoom )
		{
			m_hRespawnRoom->AddVisualizer( this );
			ChangeTeam( m_hRespawnRoom->GetTeamNumber() );
		}
		else
		{
			Warning("%s(%s) was unable to find func_respawnroomvisualizer named '%s'\n", GetClassname(), GetDebugName(), STRING(m_iszRespawnRoomName) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CFuncRespawnRoomVisualizer::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		Q_snprintf(tempstr,sizeof(tempstr),"TeamNumber: %d", GetTeamNumber() );
		EntityText(text_offset,tempstr,0);
		text_offset++;

		color32 teamcolor = g_aTeamColors[ GetTeamNumber() ];
		teamcolor.a = 0;

		if ( m_hRespawnRoom )
		{
			NDebugOverlay::Line( GetAbsOrigin(), m_hRespawnRoom->WorldSpaceCenter(), teamcolor.r, teamcolor.g, teamcolor.b, false, 0.1 );
		}
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CFuncRespawnRoomVisualizer::UpdateTransmitState()
{
	//return SetTransmitState( FL_EDICT_FULLCHECK );

	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Only transmit this entity to clients that aren't in our team
//-----------------------------------------------------------------------------
int CFuncRespawnRoomVisualizer::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	if ( !m_hRespawnRoom || m_hRespawnRoom->GetActive() )
	{
		// Respawn rooms are open in win state
		if ( TFGameRules()->State_Get() != GR_STATE_TEAM_WIN && GetTeamNumber() != TEAM_UNASSIGNED )
		{
			// Only transmit to enemy players
			CBaseEntity *pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
			if ( pRecipientEntity->GetTeamNumber() > LAST_SHARED_TEAM && !InSameTeam(pRecipientEntity) )
				return FL_EDICT_ALWAYS;
		}
	}

	return FL_EDICT_DONTSEND;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFuncRespawnRoomVisualizer::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	// Respawn rooms are open in win state
	if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN )
		return false;

	if ( GetTeamNumber() == TEAM_UNASSIGNED )
		return false;

	if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		switch( GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
			if ( !(contentsMask & CONTENTS_BLUETEAM) )
				return false;
			break;

		case TF_TEAM_RED:
			if ( !(contentsMask & CONTENTS_REDTEAM) )
				return false;
			break;
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncRespawnRoomVisualizer::SetActive( bool bActive )
{
	if ( bActive )
	{
		// We're a trigger, but we want to be solid. Out ShouldCollide() will make
		// us non-solid to members of the team that spawns here.
		RemoveSolidFlags( FSOLID_TRIGGER );
		RemoveSolidFlags( FSOLID_NOT_SOLID );	
	}
	else
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
		AddSolidFlags( FSOLID_TRIGGER );	
	}
}
