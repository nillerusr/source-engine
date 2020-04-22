//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "dod_player.h"
#include "dod_bombtarget.h"
#include "triggers.h"

class CDODBombDispenserMapIcon;

class CDODBombDispenser : public CBaseTrigger
{
public:
	DECLARE_CLASS( CDODBombDispenser, CBaseTrigger );
	DECLARE_DATADESC();

	virtual void Spawn( void );
	void EXPORT Touch( CBaseEntity *pOther );

	bool IsActive( void ) { return !m_bDisabled; }

private:

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	// Which team to give bombs to. TEAM_UNASSIGNED gives to both
	int m_iDispenseToTeam;

	// Is this area giving out bombs?
	bool m_bActive;
};

BEGIN_DATADESC(CDODBombDispenser)

	// Touch functions
	DEFINE_FUNCTION( Touch ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),

	DEFINE_KEYFIELD( m_iDispenseToTeam,			FIELD_INTEGER,	"dispense_team" ),
	DEFINE_KEYFIELD( m_bDisabled,	FIELD_BOOLEAN,	"StartDisabled" ),

END_DATADESC();

LINK_ENTITY_TO_CLASS( dod_bomb_dispenser, CDODBombDispenser );


void CDODBombDispenser::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	SetTouch( &CDODBombDispenser::Touch );

	m_bDisabled = false;

	// make our map icon entity
#ifdef DBGFLAG_ASSERT
    CBaseEntity *pIcon = 
#endif
		CBaseEntity::Create( "dod_bomb_dispenser_icon", WorldSpaceCenter(), GetAbsAngles(), this );

	Assert( pIcon );
}

void CDODBombDispenser::Touch( CBaseEntity *pOther )
{
	if ( m_bDisabled )
		return;

	if( !pOther->IsPlayer() )
		return;

	if( !pOther->IsAlive() )
		return;

	if ( m_iDispenseToTeam != TEAM_UNASSIGNED && pOther->GetTeamNumber() != m_iDispenseToTeam )
		return;

	CDODPlayer *pPlayer = ToDODPlayer( pOther );

	pPlayer->HintMessage( HINT_BOMB_PICKUP );

	switch( pPlayer->GetTeamNumber() )
	{
	case TEAM_ALLIES:
	case TEAM_AXIS:
		{
			if ( pPlayer->Weapon_OwnsThisType( "weapon_basebomb" ) == NULL )
			{
				pPlayer->GiveNamedItem( "weapon_basebomb" );

				CPASFilter filter( pPlayer->WorldSpaceCenter() );
				pPlayer->EmitSound( filter, pPlayer->entindex(), "Weapon_C4.PickUp" );
			}
		}
		break;
	default:
		break;
	}
	
}

void CDODBombDispenser::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

void CDODBombDispenser::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

class CDODBombDispenserMapIcon : public CBaseEntity
{
public:
	DECLARE_CLASS( CDODBombDispenserMapIcon, CBaseEntity );

	DECLARE_NETWORKCLASS();

	virtual int UpdateTransmitState( void )
	{
		if ( (( CDODBombDispenser * )GetOwnerEntity())->IsActive() )
		{
			return SetTransmitState( FL_EDICT_ALWAYS );
		}
		else
		{
			return SetTransmitState( FL_EDICT_DONTSEND );
		}
	}
};

IMPLEMENT_SERVERCLASS_ST(CDODBombDispenserMapIcon, DT_DODBombDispenserMapIcon)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( dod_bomb_dispenser_icon, CDODBombDispenserMapIcon );
