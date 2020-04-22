//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: implementation of CWebAPIKey
//
//=============================================================================


#include "stdafx.h"
#include "gcsdk/msgprotobuf.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Clears key settings
//-----------------------------------------------------------------------------
void CWebAPIKey::Clear()
{
	m_unAccountID = 0;
	m_unPublisherGroupID = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Serializes the request into a message object (for proxying between
// back-end Steam servers).
//-----------------------------------------------------------------------------
void CWebAPIKey::SerializeIntoProtoBuf( CMsgWebAPIKey & apiKey ) const
{
	apiKey.set_status( m_eStatus );
	apiKey.set_account_id( m_unAccountID );
	apiKey.set_publisher_group_id( m_unPublisherGroupID );
	apiKey.set_key_id( m_unWebAPIKeyID );
	apiKey.set_domain( m_sDomain.Get() );
}


//-----------------------------------------------------------------------------
// Purpose: Deserializes the response from a message object (for proxying between
// back-end Steam servers).
//-----------------------------------------------------------------------------
void CWebAPIKey::DeserializeFromProtoBuf( const CMsgWebAPIKey & apiKey )
{
	m_eStatus = (EWebAPIKeyStatus)apiKey.status();
	m_unAccountID = apiKey.account_id();
	m_unPublisherGroupID = apiKey.publisher_group_id();
	m_unWebAPIKeyID = apiKey.key_id();
	m_sDomain = apiKey.domain().c_str();
}
