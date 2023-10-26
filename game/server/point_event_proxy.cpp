//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: An entity for creating instructor hints entirely with map logic
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "baseentity.h"
#include "world.h"

#ifdef INFESTED_DLL
	#include "asw_marine.h"
	#include "asw_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointEventProxy : public CPointEntity
{
public:
	DECLARE_CLASS( CPointEventProxy, CPointEntity );
	DECLARE_DATADESC();

private:
	void InputGenerateEvent( inputdata_t &inputdata );
	
	string_t	m_iszEventName;
	bool		m_bActivatorAsUserID;
};


LINK_ENTITY_TO_CLASS( point_event_proxy, CPointEventProxy );

BEGIN_DATADESC( CPointEventProxy )

	DEFINE_KEYFIELD( m_iszEventName, FIELD_STRING, "EventName" ),
	DEFINE_KEYFIELD( m_bActivatorAsUserID, FIELD_BOOLEAN, "ActivatorAsUserID" ),
	
	DEFINE_INPUTFUNC( FIELD_VOID, "GenerateEvent", InputGenerateEvent ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the message and/or playing the sound.
//-----------------------------------------------------------------------------
void CPointEventProxy::InputGenerateEvent( inputdata_t &inputdata )
{
	IGameEvent * event = gameeventmanager->CreateEvent( m_iszEventName.ToCStr() );
	if ( event )
	{
		CBasePlayer *pActivator = NULL;

#ifdef INFESTED_DLL
		CASW_Marine *pMarine = dynamic_cast< CASW_Marine* >( inputdata.pActivator );
		if ( pMarine )
		{
			pActivator = pMarine->GetCommander();
		}
#else
		pActivator = dynamic_cast< CBasePlayer* >( inputdata.pActivator );
#endif

		if ( m_bActivatorAsUserID )
		{
			event->SetInt( "userid", ( pActivator ? pActivator->GetUserID() : 0 ) );
		}

		gameeventmanager->FireEvent( event );
	}
}
