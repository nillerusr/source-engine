//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CTFMapContribution object
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFMAPCONTRIBUTION_H
#define TFMAPCONTRIBUTION_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "tf_gcmessages.h"

namespace GCSDK
{
	class CSQLAccess;
};

//---------------------------------------------------------------------------------
// Purpose: All the account-level information that the GC tracks for TF
//---------------------------------------------------------------------------------
class CTFMapContribution : public GCSDK::CProtoBufSharedObject< CSOTFMapContribution, k_EEconTypeMapContribution >
{
#ifdef GC
	DECLARE_CLASS_MEMPOOL( CTFMapContribution );
#endif

public:
	CTFMapContribution() {}

#ifdef GC
	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess );
	virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields );
	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess );

	void WriteToRecord( CSchMapContribution *pMapContribution ) const;
	void ReadFromRecord( const CSchMapContribution & mapContribution );
#endif
};

#endif // TFMAPCONTRIBUTION_H

