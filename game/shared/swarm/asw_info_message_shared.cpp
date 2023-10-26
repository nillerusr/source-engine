#include "cbase.h"
#ifdef CLIENT_DLL
#else
#include "asw_player.h"
#include "asw_marine.h"
#endif
#include "asw_info_message_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Info_Message, DT_ASW_Info_Message )

BEGIN_NETWORK_TABLE( CASW_Info_Message, DT_ASW_Info_Message )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropString( RECVINFO( m_WindowTitle ) ),
	RecvPropString( RECVINFO( m_MessageLine1 ) ),
	RecvPropString( RECVINFO( m_MessageLine2 ) ),
	RecvPropString( RECVINFO( m_MessageLine3 ) ),
	RecvPropString( RECVINFO( m_MessageLine4 ) ),
	RecvPropInt( RECVINFO( m_iWindowSize ) ),
	RecvPropString( RECVINFO( m_MessageSound ) ),
	RecvPropString( RECVINFO( m_MessageImage ) ),
#else
	// sendprops
	SendPropString( SENDINFO( m_WindowTitle ) ),
	SendPropString( SENDINFO( m_MessageLine1 ) ),
	SendPropString( SENDINFO( m_MessageLine2 ) ),
	SendPropString( SENDINFO( m_MessageLine3 ) ),
	SendPropString( SENDINFO( m_MessageLine4 ) ),
	SendPropInt( SENDINFO( m_iWindowSize ) ),
	SendPropString( SENDINFO( m_MessageSound ) ),
	SendPropString( SENDINFO( m_MessageImage ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_info_message, CASW_Info_Message );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Info_Message )
	DEFINE_KEYFIELD( m_Key_WindowTitle, FIELD_STRING, "WindowTitle" ),
	DEFINE_KEYFIELD( m_Key_MessageLine1, FIELD_STRING, "MessageLine1" ),
	DEFINE_KEYFIELD( m_Key_MessageLine2, FIELD_STRING, "MessageLine2" ),
	DEFINE_KEYFIELD( m_Key_MessageLine3, FIELD_STRING, "MessageLine3" ),
	DEFINE_KEYFIELD( m_Key_MessageLine4, FIELD_STRING, "MessageLine4" ),
	DEFINE_KEYFIELD( m_iWindowSize, FIELD_INTEGER, "WindowSize" ),
	DEFINE_KEYFIELD( m_Key_MessageSound, FIELD_SOUNDNAME, "MessageSound" ),	
	DEFINE_KEYFIELD( m_Key_MessageImage, FIELD_STRING, "MessageImage" ),

	DEFINE_OUTPUT( m_OnMessageRead,		"OnMessageRead" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ShowMessage", InputShowMessage ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopSound", InputStopSound ),
END_DATADESC()

#endif // not client

CASW_Info_Message::CASW_Info_Message()
{

}


CASW_Info_Message::~CASW_Info_Message()
{

}

#ifndef CLIENT_DLL

void CASW_Info_Message::Spawn()
{
	BaseClass::Spawn();
	memset( m_WindowTitle.GetForModify(), 0, sizeof( m_WindowTitle ) );
	memset( m_MessageLine1.GetForModify(), 0, sizeof( m_MessageLine1 ) );
	memset( m_MessageLine2.GetForModify(), 0, sizeof( m_MessageLine2 ) );
	memset( m_MessageLine3.GetForModify(), 0, sizeof( m_MessageLine3 ) );
	memset( m_MessageLine4.GetForModify(), 0, sizeof( m_MessageLine4 ) );
	memset( m_MessageSound.GetForModify(), 0, sizeof( m_MessageSound ) );
	memset( m_MessageImage.GetForModify(), 0, sizeof( m_MessageImage ) );

	Precache();
}

void CASW_Info_Message::Precache()
{
	BaseClass::Precache();

	char *szSoundFile = (char *)STRING( m_Key_MessageSound );
	if ( m_Key_MessageSound != NULL_STRING && Q_strlen( szSoundFile ) > 1 )
	{
		if (*szSoundFile != '!')
		{
			PrecacheScriptSound(szSoundFile);			
		}
	}
}

// always send this info to players
int CASW_Info_Message::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

void CASW_Info_Message::InputShowMessage( inputdata_t &inputdata )
{
	CBaseEntity *pPlayer = NULL;
	CASW_Player *pASWPlayer = NULL;
	CASW_Marine *pMarine = NULL;

	// find the player, if any
	if ( inputdata.pActivator && inputdata.pActivator->IsPlayer() )
	{
		pPlayer = inputdata.pActivator;
	}
	else
	{
		pPlayer = (gpGlobals->maxClients > 1) ? NULL : UTIL_GetLocalPlayer();
	}

	// check if a marine called this
	if ( inputdata.pActivator)
	{
		pMarine = dynamic_cast<CASW_Marine*>(inputdata.pActivator);
	}
	// if so, find his commander
	if (pMarine && !pPlayer)
	{
		pPlayer = pMarine->GetCommander();
	}

	if (pPlayer)
	{
		pASWPlayer = dynamic_cast<CASW_Player*>(pPlayer);
		if (pASWPlayer)
			pASWPlayer->ShowInfoMessage(this);
	}
}

void CASW_Info_Message::Activate( void )
{
	BaseClass::Activate();
	// copy message details from the keyfields over to the networked strings
	Q_strncpy( m_WindowTitle.GetForModify(), STRING( m_Key_WindowTitle ), 255 );
	Q_strncpy( m_MessageLine1.GetForModify(), STRING( m_Key_MessageLine1 ), 255 );
	Q_strncpy( m_MessageLine2.GetForModify(), STRING( m_Key_MessageLine2 ), 255 );
	Q_strncpy( m_MessageLine3.GetForModify(), STRING( m_Key_MessageLine3 ), 255 );
	Q_strncpy( m_MessageLine4.GetForModify(), STRING( m_Key_MessageLine4 ), 255 );
	Q_strncpy( m_MessageSound.GetForModify(), STRING( m_Key_MessageSound ), 255 );
	Q_strncpy( m_MessageImage.GetForModify(), STRING( m_Key_MessageImage ), 255 );
}

// player has read this message
void CASW_Info_Message::OnMessageRead(CASW_Player *pPlayer)
{
	m_OnMessageRead.FireOutput(pPlayer, this);
}

void CASW_Info_Message::InputStopSound( inputdata_t &inputdata )
{
	CReliableBroadcastRecipientFilter filter;
	UserMessageBegin( filter, "ASWStopInfoMessageSound" );
	MessageEnd();
}

#endif // not client