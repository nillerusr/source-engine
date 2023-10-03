#include "cbase.h"
#include "asw_shareddefs.h"
#include "asw_use_area.h"
#include "asw_player.h"
#include "asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


LINK_ENTITY_TO_CLASS( trigger_asw_trigger_area, CASW_Use_Area );


BEGIN_DATADESC( CASW_Use_Area )
	DEFINE_KEYFIELD( m_iUseTargetName, FIELD_STRING, "usetargetname" ),
	DEFINE_FIELD( m_hUseTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bUseAreaEnabled, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_nPlayersRequired, FIELD_INTEGER, "playersrequired" ),
	DEFINE_KEYFIELD(m_szPanelPropName, FIELD_STRING, "panelpropname" ),
	DEFINE_FIELD(m_hPanelProp, FIELD_EHANDLE),

	DEFINE_OUTPUT( m_OnRequirementFailed, "OnRequirementFailed" ),
END_DATADESC()


IMPLEMENT_SERVERCLASS_ST( CASW_Use_Area, DT_ASW_Use_Area )	
	SendPropEHandle( SENDINFO(m_hUseTarget) ),
	SendPropBool( SENDINFO(m_bUseAreaEnabled) ),
	SendPropEHandle		(SENDINFO( m_hPanelProp ) ),
END_SEND_TABLE()

IMPLEMENT_AUTO_LIST( IASW_Use_Area_List );

CASW_Use_Area::CASW_Use_Area()
{
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	m_bUseAreaEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CASW_Use_Area::Spawn( void )
{
	m_bUseAreaEnabled = !m_bDisabled;
	m_bDisabled = false;	// don't allow normal triggerish disabling
	BaseClass::Spawn();

	InitTrigger();

	if (m_flWait <= 0)
	{		
		m_flWait = 0.2;
	}	

	ASSERTSZ(m_iHealth == 0, "trigger_multiple with health");
		
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	
	// find the target this use area is attached to
	m_hUseTarget = gEntList.FindEntityByName(NULL, m_iUseTargetName, NULL);
	//Assert(!m_hUseTarget);

	m_hPanelProp = gEntList.FindEntityByName( NULL, m_szPanelPropName );	
	m_bMultiplePanelProps = gEntList.FindEntityByName( m_hPanelProp.Get(), m_szPanelPropName ) != NULL;
}

int CASW_Use_Area::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

int CASW_Use_Area::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );	
}

bool CASW_Use_Area::IsUsable(CBaseEntity *pUser)
{
	return m_bUseAreaEnabled.Get() && CollisionProp()->IsPointInBounds(pUser->WorldSpaceCenter());
}

bool CASW_Use_Area::RequirementsMet( CBaseEntity *pUser )
{
	int nPlayersRequired = MIN( m_nPlayersRequired, ASWGameResource()->CountAllAliveMarines() );

	if ( nPlayersRequired <= 1 )
	{
		return true;
	}

	int nTouchingMarines = 0;

	for ( int nEnt = 0; nEnt < m_hTouchingEntities.Count(); ++nEnt )
	{
		CASW_Marine *pMarine = dynamic_cast<CASW_Marine *>( m_hTouchingEntities[ nEnt ].Get() );

		if ( pMarine && pMarine->IsAlive() && !pMarine->m_bKnockedOut )
		{
			nTouchingMarines++;
		}
	}

	if ( nTouchingMarines >= nPlayersRequired )
	{
		return true;
	}

	m_OnRequirementFailed.FireOutput( pUser, this );

	return false;
}

void CASW_Use_Area::ActivateMultiTrigger( CBaseEntity *pActivator )
{
	if ( GetNextThink() > gpGlobals->curtime )
		return;         // still waiting for reset time

	if ( !RequirementsMet( pActivator ) )
	{
		if ( m_flWait > 0 )
		{
			SetThink( &CTriggerMultiple::MultiWaitOver );
			SetNextThink( gpGlobals->curtime + m_flWait );
		}

		return;
	}

	m_hActivator = pActivator;

	m_OnTrigger.FireOutput( m_hActivator, this );

	if ( m_flWait > 0 )
	{
		SetThink( &CTriggerMultiple::MultiWaitOver );
		SetNextThink( gpGlobals->curtime + m_flWait );
	}
}

void CASW_Use_Area::InputEnable( inputdata_t &inputdata )
{
	m_bUseAreaEnabled = true;
}

void CASW_Use_Area::InputDisable( inputdata_t &inputdata )
{
	m_bUseAreaEnabled = false;
}

void CASW_Use_Area::InputToggle( inputdata_t &inputdata )
{	
	m_bUseAreaEnabled = !(m_bUseAreaEnabled.Get());
}