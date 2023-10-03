#include "cbase.h"

#include "asw_holo_sentry_shared.h"

#ifndef CLIENT_DLL
	#include "cvisibilitymonitor.h"
#else
	#include "c_asw_player.h"
	#include "asw_hud_master.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Holo_Sentry, DT_ASW_Holo_Sentry )

BEGIN_NETWORK_TABLE( CASW_Holo_Sentry, DT_ASW_Holo_Sentry )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropBool(RECVINFO(m_bEnabled)),
#else
	// sendprops
	SendPropBool(SENDINFO(m_bEnabled)),
#endif
END_NETWORK_TABLE()


#ifndef CLIENT_DLL

extern ConVar asw_visrange_generic;


LINK_ENTITY_TO_CLASS( asw_holo_sentry, CASW_Holo_Sentry );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Holo_Sentry )
	DEFINE_KEYFIELD( m_bEnabled,		FIELD_BOOLEAN, "enabled" ),
	DEFINE_KEYFIELD( m_bStartDisabled,	FIELD_BOOLEAN, "StartDisabled" ),

	DEFINE_INPUTFUNC( FIELD_VOID,		"Enable",		InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Disable",		InputDisable ),

	DEFINE_OUTPUT( m_OnSentryPlaced,	"OnSentryPlaced" ),
END_DATADESC()
#endif // not client


CASW_Holo_Sentry::CASW_Holo_Sentry()
{
}


CASW_Holo_Sentry::~CASW_Holo_Sentry()
{
}

void CASW_Holo_Sentry::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

#ifndef CLIENT_DLL
	StopListeningForAllEvents();
#else
	DestroySentryBuildDisplay();
#endif
}


#ifndef CLIENT_DLL

bool CASW_Holo_Sentry::VismonEvaluator( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer )
{
	CASW_Holo_Sentry *pHoloSentry = static_cast< CASW_Holo_Sentry* >( pVisibleEntity );
	return pHoloSentry->m_bEnabled;
}

void CASW_Holo_Sentry::Precache()
{
	BaseClass::Precache();
}

void CASW_Holo_Sentry::Spawn()
{
	BaseClass::Spawn();

	m_bEnabled = !m_bStartDisabled;

	ListenForGameEvent( "sentry_placed" );

	VisibilityMonitor_AddEntity( this, asw_visrange_generic.GetFloat() * 0.9f, NULL, &CASW_Holo_Sentry::VismonEvaluator );
}

void CASW_Holo_Sentry::FireGameEvent( IGameEvent *event )
{
	const char *name = event->GetName();

	if ( Q_strcmp( name, "sentry_placed" ) == 0 )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
		m_OnSentryPlaced.FireOutput( pPlayer, this );
	}
}

void CASW_Holo_Sentry::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CASW_Holo_Sentry::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}


#else	// client

void CASW_Holo_Sentry::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( m_bEnabled )
	{
		CreateSentryBuildDisplay();
	}
	else
	{
		DestroySentryBuildDisplay();
	}
}

void CASW_Holo_Sentry::CreateSentryBuildDisplay()
{
	if ( m_hSentryBuildDisplay )
		return;

	bool bHasSentry = false;

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		bHasSentry = ( pPlayer->Weapon_OwnsThisType( "asw_weapon_sentry" ) != NULL );

		if ( !bHasSentry )
		{
			CASW_Hud_Master *pHUDMaster = GET_HUDELEMENT( CASW_Hud_Master );
			if ( pHUDMaster )
			{
				int nSlot = pHUDMaster->GetHotBarSlot( "asw_weapon_sentry" );
				bHasSentry = pHUDMaster->OwnsHotBarSlot( pPlayer, nSlot );
			}
		}
	}

	if ( !bHasSentry )
	{
		return;
	}

	Vector vForward;
	GetVectors( &vForward, NULL, NULL );

	m_hSentryBuildDisplay = ParticleProp()->Create( "sentry_build_display", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 8 ) - vForward * 50.0f );
	m_hSentryBuildDisplay->SetControlPoint( 5, Vector( 10, 124, 203 ) );
}

void CASW_Holo_Sentry::DestroySentryBuildDisplay()
{
	if ( !m_hSentryBuildDisplay )
		return;

	ParticleProp()->StopEmissionAndDestroyImmediately( m_hSentryBuildDisplay );
	m_hSentryBuildDisplay = NULL;
}

#endif // not client
