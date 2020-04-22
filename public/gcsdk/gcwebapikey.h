//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: header for Web API key
//
//=============================================================================

#ifndef GCWEBAPIKEY_H
#define GCWEBAPIKEY_H
#ifdef _WIN32
#pragma once
#endif

using GCSDK::CGCMsgBase;
using GCSDK::WebAPIKey_t;
using GCSDK::EWebAPIKeyStatus;

class CMsgWebAPIKey;

class CWebAPIKey
{
public:
	CWebAPIKey() { Clear(); }

	void Clear();
	bool BIsValid() const { return (m_unAccountID != 0 || m_unPublisherGroupID != 0) && m_eStatus == GCSDK::k_EWebAPIKeyValid; }
	bool BIsAccountKey() const { return m_unAccountID != 0; }
	bool BIsPublisherKey() const { return m_unPublisherGroupID != 0; }
	uint32 GetAccountID() const { return m_unAccountID; }
	uint32 GetPublisherGroupID() const { return m_unPublisherGroupID; }
	uint32 GetID() const { return m_unWebAPIKeyID; }
	const char *GetDomain() const { return m_sDomain; }
	EWebAPIKeyStatus GetStatus() const { return m_eStatus; }

	void SerializeIntoProtoBuf( CMsgWebAPIKey & apiKey ) const;
	void DeserializeFromProtoBuf( const CMsgWebAPIKey & apiKey );

private:
	EWebAPIKeyStatus m_eStatus;
	uint32 m_unAccountID;		// set if key is for an account, 0 otherwise
	uint32 m_unPublisherGroupID;	// set if key is for a publisher, 0 otherwise
	uint32 m_unWebAPIKeyID;
	CUtlString m_sDomain;
};

#endif // GCWEBAPIKEY_H
