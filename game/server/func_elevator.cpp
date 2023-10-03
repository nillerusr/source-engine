// func_elevator.cpp
// Copyright 2007 Turtle Rock Studios, Inc.

#include "cbase.h"
#include "func_elevator.h"
#include "nav.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


ConVar ZombieAirborneElevator( "z_elevator_in_air", "0" );

//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_elevator_floor, CInfoElevatorFloor );


//--------------------------------------------------------------------------------------------------------
BEGIN_DATADESC( CInfoElevatorFloor )

	// Outputs
	DEFINE_OUTPUT( m_OnReachedFloor, "OnReachedFloor" ),

END_DATADESC()


//--------------------------------------------------------------------------------------------------------
void CInfoElevatorFloor::OnReachedFloor( CBaseEntity *elevator )
{
	m_OnReachedFloor.FireOutput( elevator, elevator );
}


//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( func_elevator, CFuncElevator );


//--------------------------------------------------------------------------------------------------------
BEGIN_DATADESC( CFuncElevator )

	DEFINE_KEYFIELD( m_topFloorPosition,	FIELD_POSITION_VECTOR, "top" ),
	DEFINE_KEYFIELD( m_bottomFloorPosition,	FIELD_POSITION_VECTOR, "bottom" ),

	DEFINE_KEYFIELD( m_maxSpeed,  FIELD_FLOAT,	"speed"),
	DEFINE_KEYFIELD( m_acceleration,  FIELD_FLOAT,	"acceleration"),

	DEFINE_KEYFIELD( m_soundStart,		 FIELD_SOUNDNAME, "StartSound" ),
	DEFINE_KEYFIELD( m_soundStop,		 FIELD_SOUNDNAME, "StopSound" ),
	DEFINE_KEYFIELD( m_soundDisable,	 FIELD_SOUNDNAME, "DisableSound" ),

	DEFINE_FIELD( m_currentSound, FIELD_SOUNDNAME ),
	DEFINE_KEYFIELD( m_flBlockDamage,	 FIELD_FLOAT,	"BlockDamage"),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING,  "MoveToFloor", InputMoveToFloor ),
	DEFINE_INPUTFUNC( FIELD_STRING,  "Disable", InputDisable ),

	// Outputs
	DEFINE_OUTPUT( m_OnReachedTop, "OnReachedTop" ),
	DEFINE_OUTPUT( m_OnReachedBottom, "OnReachedBottom" ),

	// Functions
	DEFINE_FUNCTION( StopMoveSoundThink ),
	// FIXMEL4DTOMAINMERGE
	//DEFINE_FUNCTION( AccelerationThink ),

END_DATADESC()


void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
//--------------------------------------------------------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST(CFuncElevator, DT_FuncElevator)
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
	SendPropVector	(SENDINFO(m_vecOrigin), -1,  SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
	SendPropFloat( SENDINFO( m_acceleration ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_currentSpeed ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_movementStartTime ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_movementStartSpeed ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_movementStartZ ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_destinationFloorPosition ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_maxSpeed ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_isMoving ) ),
END_SEND_TABLE()


//--------------------------------------------------------------------------------------------------------
CFuncElevator::CFuncElevator()
{
}


//--------------------------------------------------------------------------------------------------------
static int FloorHeightSort( const FloorInfo *a, const FloorInfo *b )
{
	return a->height > b->height;
}


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::Spawn( void )
{
	SetMoveType( MOVETYPE_CUSTOM );
	SetModel( STRING( GetModelName() ) );

	SetTouch( NULL );

	Precache();

	AddFlag( FL_CONVEYOR );

	// It is solid?
	SetSolid( SOLID_BSP );

	if ( m_acceleration == 0.0f )
	{
		m_acceleration = m_maxSpeed;	// 1 second acceleration period
	}

	m_enabled = true;

	// Construct a list of data for each floor
	m_floors.RemoveAll();
	FloorInfo floor;

	floor.button = NULL;	// filled in later
	floor.height = m_bottomFloorPosition.z;
	floor.name = AllocPooledString( "bottom" );
	m_floors.AddToHead( floor );

	// Floors are specified by CInfoElevatorFloor entities
	floor.button = NULL;	// filled in later
	floor.height = m_topFloorPosition.z;
	floor.name = AllocPooledString( "top" );
	m_floors.AddToTail( floor );


	// Trawl through the list of entities that have map-specified outputs that target this elevator.  Grab any that
	// are func_buttons that send a MoveToFloor output to us, and save off which floor they go to.
	CBaseEntity *inputSource = NULL;
	inputSource = gEntList.FindEntityByOutputTarget( NULL, GetEntityName() );
	while ( inputSource )
	{
		datamap_t *dmap = inputSource->GetDataDescMap();
		bool found = false;
		while ( dmap && !found )
		{
			int fields = dmap->dataNumFields;
			for ( int i = 0; i < fields; i++ )
			{
				typedescription_t *dataDesc = &dmap->dataDesc[i];
				if ( ( dataDesc->fieldType == FIELD_CUSTOM ) && ( dataDesc->flags & FTYPEDESC_OUTPUT ) )
				{
					CBaseEntityOutput *pOutput = (CBaseEntityOutput *)((int)inputSource + (int)dataDesc->fieldOffset);
					const CEventAction *action = pOutput->GetActionForTarget( GetEntityName() );
					if ( action )
					{
						if ( FStrEq( STRING( action->m_iTargetInput ), "MoveToFloor" ) && action->m_iParameter != NULL_STRING )
						{
							bool isButton = inputSource->ClassMatches( "func_button*" );
							bool existingFloor = false;
							for ( int n=0; n<m_floors.Count(); ++n )
							{
								FloorInfo &floor = m_floors[n];
								if ( floor.name == action->m_iParameter )
								{
									if ( isButton )
									{
										floor.button = inputSource;
									}
									existingFloor = true;
									break;
								}
							}

							if ( !existingFloor )
							{
								// See if it's a real floor
								CBaseEntity *floorEntity = gEntList.FindEntityByName( NULL, action->m_iParameter );
								if ( floorEntity )
								{
									floor.button = (isButton) ? inputSource : NULL;
									floor.height = floorEntity->GetAbsOrigin().z;
									floor.name = action->m_iParameter;
									m_floors.AddToTail( floor );
								}
							}

							found = true;
							break;
						}
					}
				}
			}
			dmap = dmap->baseMap;
		}

		inputSource = gEntList.FindEntityByOutputTarget( inputSource, GetEntityName() );
	}

	m_floors.Sort( FloorHeightSort );

	m_movementStartTime = gpGlobals->curtime;
	m_movementStartSpeed = m_currentSpeed;
	m_movementStartZ = GetAbsOrigin().z;
	m_destinationFloorPosition = GetAbsOrigin().z;
	SetThink(NULL);
	m_targetFloor = NULL;
	m_accelerationTimer.Start();
	m_isMoving = false;
	CreateVPhysics();
}


//--------------------------------------------------------------------------------------------------------
bool CFuncElevator::CreateVPhysics( void )
{
	VPhysicsInitShadow(false, false);
	return true;
}


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::Precache( void )
{
	if ( m_soundStart != NULL_STRING )
	{
		PrecacheScriptSound( STRING( m_soundStart ) );
	}

	if ( m_soundStop != NULL_STRING )
	{
		PrecacheScriptSound( STRING( m_soundStop ) );
	}

	if ( m_soundDisable != NULL_STRING )
	{
		PrecacheScriptSound( STRING( m_soundDisable ) );
	}

	m_currentSound = NULL_STRING;
}


//--------------------------------------------------------------------------------------------------------
int CFuncElevator::GetFloorForHeight( float height ) const
{
	int bestFloor = -1;
	float bestHeightDelta = 100.0f;

	for ( int i=0; i<GetNumFloors(); ++i )
	{
		const FloorInfo *floor = GetFloor( i );

		float heightDelta = fabs( floor->height - height );
		if ( heightDelta < bestHeightDelta )
		{
			bestFloor = i;
			bestHeightDelta = heightDelta;
		}
	}

	return bestFloor;
}


//--------------------------------------------------------------------------------------------------------
EHANDLE CFuncElevator::GetButtonForHeight( float height ) const
{
	int targetFloorIndex = GetFloorForHeight( height );
	if ( targetFloorIndex < 0 || targetFloorIndex >= GetNumFloors() )
		return NULL;

	const FloorInfo *targetFloor = GetFloor( targetFloorIndex );

	if ( !targetFloor )
		return NULL;

	return targetFloor->button;
}


//--------------------------------------------------------------------------------------------------------
EHANDLE CFuncElevator::GetButtonAtCurrentHeight( void ) const
{
	int currentFloorIndex = GetCurrentFloor();
	if ( currentFloorIndex < 0 || currentFloorIndex >= GetNumFloors() )
		return NULL;

	const FloorInfo *currentFloor = GetFloor( currentFloorIndex );

	CBaseEntity *bestButton = NULL;
	float bestHeightDelta = 100.0f;

	for ( int i=0; i<GetNumFloors(); ++i )
	{
		const FloorInfo *floor = GetFloor( i );
		if ( floor == currentFloor )
			continue;

		CBaseEntity *button = floor->button;
		if ( !button )
			continue;

		float heightDelta = fabs( button->WorldSpaceCenter().z - currentFloor->height );
		if ( heightDelta < bestHeightDelta )
		{
			bestHeightDelta = heightDelta;
			bestButton = button;
		}

	}

	return bestButton;
}


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::MoveTo( float destinationZ )
{
	if ( !m_enabled )
		return;

	m_isMoving = true;

	m_accelerationTimer.Start();

	m_movementStartTime = gpGlobals->curtime;
	m_movementStartSpeed = m_currentSpeed;
	m_movementStartZ = GetAbsOrigin().z;
	m_destinationFloorPosition = destinationZ;

	if ( m_soundStart != NULL_STRING )
	{
		if (m_currentSound == m_soundStart)
		{
			StopSound(entindex(), CHAN_BODY, (char*)STRING(m_soundStop));
		}
		else
		{
			m_currentSound = m_soundStart;
			CPASAttenuationFilter filter( this );

			EmitSound_t ep;
			ep.m_nChannel = CHAN_BODY;
			ep.m_pSoundName = (char*)STRING(m_soundStart);
			ep.m_flVolume = 1;
			ep.m_SoundLevel = SNDLVL_NORM;

			EmitSound( filter, entindex(), ep );	
		}
	}

	// Find any physics objects on the elevator and destroy them.
	// This is to address a problem where players and bots fall through the elevator
	// if they stand on a physics object while the elevator is in motion.
	Vector lo, hi;
	GetCollideable()->WorldSpaceSurroundingBounds( &lo, &hi );
	hi.z += HumanHeight;
	lo.z -= HumanHeight;
	CBaseEntity *entitiesList[ 128 ];
	CFlaggedEntitiesEnum enumerator( entitiesList, 128, 0 );
	UTIL_EntitiesInBox( lo, hi, &enumerator );
	for ( int i=0; i<enumerator.GetCount(); ++i )
	{
		CBaseEntity *pEnt = entitiesList[i];
		// We're only concerned about physics props.
		if ( FClassnameIs( pEnt, "prop_physics" ) )
		{
			// Since it may not be a breakable prop, we can't do TakeDamage.
			// Just remove it.
			UTIL_Remove( pEnt );
		}
	}

	// Clear think (that stops sounds)
	SetThink(NULL);
}


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::InputDisable( inputdata_t &inputdata )
{
	if ( !m_enabled )
		return;

	if ( m_currentSpeed != 0.0f )
	{
		// Move to our current location, to cancel any current movement...
		MoveTo( GetAbsOrigin().z );
	}

	SetThink(&CFuncElevator::StopMoveSoundThink);
	SetNextThink( gpGlobals->curtime + 0.1f );

	// ... and disable ourselves to prevent any future movement.
	m_enabled = false;
}


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::InputMoveToFloor( inputdata_t &inputdata )
{
	if ( !m_enabled )
		return;

	const char *floorName = inputdata.value.String();
	m_targetFloor = NULL;

	if ( FStrEq( floorName, "top" ) )
	{
		MoveTo( m_topFloorPosition.z );
	}
	else if ( FStrEq( floorName, "bottom" ) )
	{
		MoveTo( m_bottomFloorPosition.z );
	}
	else
	{
		CBaseEntity *target = gEntList.FindEntityByName( NULL, floorName );
		if ( target )
		{
			m_targetFloor = target;
			MoveTo( target->GetAbsOrigin().z );
		}
		else
		{
			Warning( "Elevator tried to move to bad floor '%s'\n", floorName );
			return;
		}
	}
}


//--------------------------------------------------------------------------------------------------------
/**
 * Returns the current floor, or -1 if the elevator is in-between floors
 */
int CFuncElevator::GetCurrentFloor( void ) const
{
	float currentHeight = GetAbsOrigin().z;
	const float Tolerance = 0.5f;
	for ( int i=0; i<m_floors.Count(); ++i )
	{
		const FloorInfo *floor = GetFloor( i );
		if ( fabs( currentHeight - floor->height ) < Tolerance )
		{
			return i;
		}
	}

	return -1; // in-between floors
}


//--------------------------------------------------------------------------------------------------------
/**
 * Returns the floor to which the elevator is moving, or the current floor number if the elevator is stopped
 */
int CFuncElevator::GetDestinationFloor( void ) const
{
	if ( m_currentSpeed != 0.0f )
	{
		float targetHeight = m_destinationFloorPosition;
		const float Tolerance = 0.5f;
		for ( int i=0; i<m_floors.Count(); ++i )
		{
			const FloorInfo *floor = GetFloor( i );
			if ( fabs( targetHeight - floor->height ) < Tolerance )
			{
				return i;
			}
		}
	}

	return GetCurrentFloor();
}


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::MoveDone( void )
{
	SetContextThink( NULL, TICK_NEVER_THINK, "AccelerationContext" );
	m_currentSpeed = 0.0f;
	m_isMoving = false;

	// Stop sounds at the next think, rather than here as another
	// SetPosition call might immediately follow the end of this move
	SetThink(&CFuncElevator::StopMoveSoundThink);
	SetNextThink( gpGlobals->curtime + 0.1f );
	BaseClass::MoveDone();

	// Sets a floor string and fires the output
	float currentPosition = GetAbsOrigin().z;
	if ( currentPosition >= m_topFloorPosition.z )
	{
		m_OnReachedTop.FireOutput( this, this );
	}
	else if ( currentPosition <= m_bottomFloorPosition.z )
	{
		m_OnReachedBottom.FireOutput( this, this );
	}
	else if ( m_targetFloor.Get() != NULL )
	{
		CInfoElevatorFloor *floor = dynamic_cast< CInfoElevatorFloor * >(m_targetFloor.Get());
		if ( floor )
		{
			floor->OnReachedFloor( this );
		}
	}
}


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::Blocked( CBaseEntity *pOther )
{
	// Hurt the blocker 
	if ( m_flBlockDamage )
	{
		/*
		if ( pOther->GetTeamNumber() == TEAM_SURVIVOR )
		{
			// realistically, we still want to kill them if they're blocked against something not in our move hierarchy.
			const trace_t &touchTrace = GetTouchTrace();
			if ( touchTrace.DidHitNonWorldEntity() )
			{
				return;
			}
		}
		*/
		pOther->TakeDamage( CTakeDamageInfo( this, this, m_flBlockDamage, DMG_CRUSH ) );
	}
}


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::StopMoveSoundThink( void )
{
	string_t targetSound = ( m_enabled ) ? m_soundStop : m_soundDisable;

	if ( m_currentSound != NULL_STRING && ( m_currentSound != targetSound ) )
	{
		StopSound( entindex(), CHAN_BODY, STRING( m_currentSound ) );
	}

	if ( targetSound != NULL_STRING && ( m_currentSound != targetSound ) )
	{
		m_currentSound = targetSound;
		CPASAttenuationFilter filter( this );

		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = STRING( targetSound );
		ep.m_flVolume = 1;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound( filter, entindex(), ep );
	}

	SetThink(NULL);
}


//--------------------------------------------------------------------------------------------------------
bool CFuncElevator::IsPlayerOnElevator( CBasePlayer *player )
{
	if ( !player->IsAlive() )
		return false;

	CBaseEntity *ground = player->GetGroundEntity();
	if ( ground )
	{
		CBaseEntity *groundParent = ground->GetParent();
		while ( groundParent && ground->GetParent() && ground->GetParent() != groundParent )
		{
			groundParent = ground->GetParent();
		}

		CBaseEntity *groundMoveParent = ground->GetMoveParent();
		while ( groundMoveParent && ground->GetMoveParent() && ground->GetMoveParent() != groundMoveParent )
		{
			groundMoveParent = ground->GetMoveParent();
		}

		if ( ground == this || groundMoveParent == this || groundParent == this )
		{
			return true;
		}
	}

	if ( ZombieAirborneElevator.GetBool() )
	{
		Extent extent;
		GetCollideable()->WorldSpaceSurroundingBounds( &extent.lo, &extent.hi );
		extent.hi.z += HumanHeight;
		if ( extent.Contains( player->GetAbsOrigin() ) )
		{
			return true;
		}
	}

	return false;
}



//--------------------------------------------------------------------------------------------------------
class ElevatorPlayerCollector
{
public:
	ElevatorPlayerCollector( CBaseEntity *elevator, int team )
	{
		m_elevator = elevator;
		m_team = team;
		elevator->GetCollideable()->WorldSpaceSurroundingBounds( &m_extent.lo, &m_extent.hi );
		m_extent.hi.z += HumanHeight;
	}

	bool operator()( CBasePlayer *player )
	{
		if ( !player->IsAlive() )
			return true;

		if ( player->GetTeamNumber() != m_team && m_team != TEAM_UNASSIGNED )
			return true;

		CBaseEntity *ground = player->GetGroundEntity();
		if ( ground )
		{
			CBaseEntity *groundParent = ground->GetParent();
			while ( groundParent && ground->GetParent() && ground->GetParent() != groundParent )
			{
				groundParent = ground->GetParent();
			}

			CBaseEntity *groundMoveParent = ground->GetMoveParent();
			while ( groundMoveParent && ground->GetMoveParent() && ground->GetMoveParent() != groundMoveParent )
			{
				groundMoveParent = ground->GetMoveParent();
			}

			if ( ground == m_elevator || groundMoveParent == m_elevator || groundParent == m_elevator )
			{
				m_players.AddToTail( player );
				return true;
			}
		}

		if ( m_extent.Contains( player->GetAbsOrigin() ) )
		{
			m_players.AddToTail( player );
			return true;
		}

		return true;
	}

	CUtlVector< CBasePlayer * > m_players;
	CBaseEntity *m_elevator;
	int m_team;
	Extent m_extent;
};


//--------------------------------------------------------------------------------------------------------
void CFuncElevator::FindPlayersOnElevator( CUtlVector< CBasePlayer * > *players, int teamNumber )
{
	ElevatorPlayerCollector playerCollector( this, teamNumber );
	ForEachPlayer( playerCollector );

	*players = playerCollector.m_players;
}


//--------------------------------------------------------------------------------------------------------
/**
 * Draw any debug text overlays, and return the text offset from the top
 */
int CFuncElevator::DrawDebugTextOverlays( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if ( m_debugOverlays & OVERLAY_TEXT_BIT ) 
	{
		char tempstr[512];

		if ( GetCurrentSpeed() != 0.0f )
		{
			int destinationFloor = GetDestinationFloor();
			const char *floorName = "unknown";
			const FloorInfo *floor = GetFloor( destinationFloor );
			if ( floor )
			{
				floorName = STRING( floor->name );
			}

			Q_snprintf( tempstr, sizeof(tempstr), "Moving at speed %f to floor %d(%s)",
				GetCurrentSpeed(), destinationFloor, floorName );
			EntityText( text_offset, tempstr, 0 );
			++text_offset;
		}
		else
		{
			int currentFloor = GetCurrentFloor();
			const char *floorName = "unknown";
			const FloorInfo *floor = GetFloor( currentFloor );
			if ( floor )
			{
				floorName = STRING( floor->name );
			}

			Q_snprintf( tempstr, sizeof(tempstr), "Currently at floor %d(%s)",
				currentFloor, floorName );
			EntityText( text_offset, tempstr, 0 );
			++text_offset;
		}

		CBaseEntity *nearbyButton = GetButtonAtCurrentHeight();
		if ( nearbyButton )
		{
			Q_snprintf( tempstr, sizeof(tempstr), "Nearby button is %s",
				STRING( nearbyButton->GetEntityName() ) );
			EntityText( text_offset++, tempstr, 0 );
		}
		else
		{
			EntityText( text_offset++, "No nearby buttons", 0 );
		}

		for ( int i=0; i<m_floors.Count(); ++i )
		{
			const FloorInfo &floor = m_floors[i];
			const char *floorName = STRING(floor.name);
			float floorHeight = floor.height;
			const char *buttonName = "<none>";
			CBaseEntity *buttonEntity = GetButtonForHeight( floorHeight );
			if ( buttonEntity != NULL )
			{
				buttonName = STRING(buttonEntity->GetEntityName());
			}

			Q_snprintf( tempstr, sizeof(tempstr), "Floor %s is at %f, triggered by %s",
				floorName, floorHeight, buttonName );
			EntityText( text_offset++, tempstr, 0 );
		}

		CUtlVector< CBasePlayer * > players;
		FindPlayersOnElevator( &players );
		for ( int i=0; i<players.Count(); ++i )
		{
			Q_snprintf( tempstr, sizeof(tempstr), "Occupant %d: %s", i, players[i]->GetPlayerName() );
			EntityText( text_offset, tempstr, 0 );
			++text_offset;
		}
	}
	return text_offset;
}


//--------------------------------------------------------------------------------------------------------
