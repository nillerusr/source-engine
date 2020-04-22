//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds XP source data
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_XP_SOURCE_H
#define TF_XP_SOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "tf_gcmessages.h"

#ifdef GC
	#include "tf_gc.h"
#endif

//---------------------------------------------------------------------------------
// Purpose: Contains a delta for a player's XP
//---------------------------------------------------------------------------------
class CXPSource : public GCSDK::CProtoBufSharedObject< CMsgTFXPSource, k_EEconTypeXPSource >
{
public:
	CXPSource();
#ifdef GC
	CXPSource( CMsgTFXPSource msg );
	DECLARE_CLASS_MEMPOOL( CXPSource );

	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess );
	virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields );
	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess );

	void WriteToRecord( CSchPlayerXPSources *pWarData ) const;
	void ReadFromRecord( const CSchPlayerXPSources & warData );
#endif // GC
};

#endif // TF_XP_SOURCE_H
