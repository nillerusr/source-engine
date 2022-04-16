//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: An entity that creates NPCs in the game. 
//
//=============================================================================//

#include "cbase.h"
#include "entityapi.h"
#include "entityoutput.h"
#include "ai_basenpc.h"
#include "hl1_monstermaker.h"
#include "mapentities.h"


BEGIN_DATADESC( CNPCMaker )

	DEFINE_KEYFIELD( m_iMaxNumNPCs,			FIELD_INTEGER,	"monstercount" ),
	DEFINE_KEYFIELD( m_iMaxLiveChildren,		FIELD_INTEGER,	"MaxLiveChildren" ),
	DEFINE_KEYFIELD( m_flSpawnFrequency,		FIELD_FLOAT,	"delay" ),
	DEFINE_KEYFIELD( m_bDisabled,			FIELD_BOOLEAN,	"StartDisabled" ),
	DEFINE_KEYFIELD( m_iszNPCClassname,		FIELD_STRING,	"monstertype" ),

	DEFINE_FIELD( m_cLiveChildren,			FIELD_INTEGER ),
	DEFINE_FIELD( m_flGround,				FIELD_FLOAT ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Spawn",	InputSpawnNPC ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable",	InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable",	InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle",	InputToggle ),

	// Outputs
	DEFINE_OUTPUT( m_OnSpawnNPC, "OnSpawnNPC" ),

	// Function Pointers
	DEFINE_THINKFUNC( MakerThink ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( monstermaker, CNPCMaker );


//-----------------------------------------------------------------------------
// Purpose: Spawn
//-----------------------------------------------------------------------------
void CNPCMaker::Spawn( void )
{
	SetSolid( SOLID_NONE );
	m_cLiveChildren		= 0;
	Precache();

	// If I can make an infinite number of NPC, force them to fade
	if ( m_spawnflags & SF_NPCMAKER_INF_CHILD )
	{
		m_spawnflags |= SF_NPCMAKER_FADE;
	}

	//Start on?
	if ( m_bDisabled == false )
	{
		SetThink ( &CNPCMaker::MakerThink );
		SetNextThink( gpGlobals->curtime + m_flSpawnFrequency );
	}
	else
	{
		//wait to be activated.
		SetThink ( &CBaseEntity::SUB_DoNothing );
	}
	m_flGround = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether or not it is OK to make an NPC at this instant.
//-----------------------------------------------------------------------------
bool CNPCMaker::CanMakeNPC( void )
{
	if ( m_iMaxLiveChildren > 0 && m_cLiveChildren >= m_iMaxLiveChildren )
	{// not allowed to make a new one yet. Too many live ones out right now.
		return false;
	}

	if ( !m_flGround )
	{
		// set altitude. Now that I'm activated, any breakables, etc should be out from under me. 
		trace_t tr;

		UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() - Vector ( 0, 0, 2048 ), MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
		m_flGround = tr.endpos.z;
	}

	Vector mins = GetAbsOrigin() - Vector( 34, 34, 0 );
	Vector maxs = GetAbsOrigin() + Vector( 34, 34, 0 );
	maxs.z = GetAbsOrigin().z;
	
	//Only adjust for the ground if we want it
	if ( ( m_spawnflags & SF_NPCMAKER_NO_DROP ) == false )
	{
		mins.z = m_flGround;
	}

	CBaseEntity *pList[128];
	
	int count = UTIL_EntitiesInBox( pList, 128, mins, maxs, FL_CLIENT|FL_NPC );
	if ( count )
	{
		//Iterate through the list and check the results
		for ( int i = 0; i < count; i++ )
		{
			//Don't build on top of another entity
			if ( pList[i] == NULL )
				continue;

			//If one of the entities is solid, then we can't spawn now
			if ( ( pList[i]->GetSolidFlags() & FSOLID_NOT_SOLID ) == false )
				return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: If this had a finite number of children, return true if they've all
//			been created.
//-----------------------------------------------------------------------------
bool CNPCMaker::IsDepleted()
{
	if ( (m_spawnflags & SF_NPCMAKER_INF_CHILD) || m_iMaxNumNPCs > 0 )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Toggle the spawner's state
//-----------------------------------------------------------------------------
void CNPCMaker::Toggle( void )
{
	if ( m_bDisabled )
	{
		Enable();
	}
	else
	{
		Disable();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Start the spawner
//-----------------------------------------------------------------------------
void CNPCMaker::Enable( void )
{
	// can't be enabled once depleted
	if ( IsDepleted() )
		return;

	m_bDisabled = false;
	SetThink ( &CNPCMaker::MakerThink );
	SetNextThink( gpGlobals->curtime );
}


//-----------------------------------------------------------------------------
// Purpose: Stop the spawner
//-----------------------------------------------------------------------------
void CNPCMaker::Disable( void )
{
	m_bDisabled = true;
	SetThink ( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that spawns an NPC.
//-----------------------------------------------------------------------------
void CNPCMaker::InputSpawnNPC( inputdata_t &inputdata )
{
	MakeNPC();
}


//-----------------------------------------------------------------------------
// Purpose: Input hander that starts the spawner
//-----------------------------------------------------------------------------
void CNPCMaker::InputEnable( inputdata_t &inputdata )
{
	Enable();
}


//-----------------------------------------------------------------------------
// Purpose: Input hander that stops the spawner
//-----------------------------------------------------------------------------
void CNPCMaker::InputDisable( inputdata_t &inputdata )
{
	Disable();
}


//-----------------------------------------------------------------------------
// Purpose: Input hander that toggles the spawner
//-----------------------------------------------------------------------------
void CNPCMaker::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}


//-----------------------------------------------------------------------------
// Purpose: Precache the target NPC
//-----------------------------------------------------------------------------
void CNPCMaker::Precache( void )
{
	BaseClass::Precache();
	UTIL_PrecacheOther( STRING( m_iszNPCClassname ) );
}


//-----------------------------------------------------------------------------
// Purpose: Creates the NPC.
//-----------------------------------------------------------------------------
void CNPCMaker::MakeNPC( void )
{
	if (!CanMakeNPC())
	{
		return;
	}

	CBaseEntity	*pent = (CBaseEntity*)CreateEntityByName( STRING(m_iszNPCClassname) );

	if ( !pent )
	{
		Warning("NULL Ent in NPCMaker!\n" );
		return;
	}
	
	m_OnSpawnNPC.FireOutput( this, this );

	pent->SetLocalOrigin( GetAbsOrigin() );
	pent->SetLocalAngles( GetAbsAngles() );

	pent->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );

	if ( m_spawnflags & SF_NPCMAKER_FADE )
	{
		pent->AddSpawnFlags( SF_NPC_FADE_CORPSE );
	}


	DispatchSpawn( pent );
	pent->SetOwnerEntity( this );

	m_cLiveChildren++;// count this NPC

	if (!(m_spawnflags & SF_NPCMAKER_INF_CHILD))
	{
		m_iMaxNumNPCs--;

		if ( IsDepleted() )
		{
			// Disable this forever.  Don't kill it because it still gets death notices
			SetThink( NULL );
			SetUse( NULL );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new NPC every so often.
//-----------------------------------------------------------------------------
void CNPCMaker::MakerThink ( void )
{
	SetNextThink( gpGlobals->curtime + m_flSpawnFrequency );

	MakeNPC();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVictim - 
//-----------------------------------------------------------------------------
void CNPCMaker::DeathNotice( CBaseEntity *pVictim )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveChildren--;
}
