//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CEconClaimCode object
//
// $NoKeywords: $
//=============================================================================//

#ifndef ECON_CLAIMCODE_H
#define ECON_CLAIMCODE_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"

//---------------------------------------------------------------------------------
// Purpose: All the account-level information that the GC tracks for TF
//---------------------------------------------------------------------------------
class CEconClaimCode : public GCSDK::CProtoBufSharedObject< CSOEconClaimCode, k_EEconTypeClaimCode >
{
#ifdef GC
	DECLARE_CLASS_MEMPOOL( CEconClaimCode );
#endif

public:

#ifdef GC
	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess );

	void WriteToRecord( CSchAssignedClaimCode *pClaimCode );
	void ReadFromRecord( const CSchAssignedClaimCode & mapContribution );
#endif
};

#ifdef GC
bool BBuildRedemptionURL( CEconClaimCode *pClaimCode, CUtlString &redemptionURL );
#endif

#endif // ECON_CLAIMCODE_H

