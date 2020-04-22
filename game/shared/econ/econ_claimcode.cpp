//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Code for the CEconClaimCode object
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "econ_item_tools.h"
#include "econ_claimcode.h"

using namespace GCSDK;

#ifdef GC
IMPLEMENT_CLASS_MEMPOOL( CEconClaimCode, 10 * 1000, UTLMEMORYPOOL_GROW_SLOW );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CEconClaimCode::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchAssignedClaimCode schCode;
	WriteToRecord( &schCode );
	return CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, &schCode );
}

void CEconClaimCode::WriteToRecord( CSchAssignedClaimCode *pClaimCode )
{
	pClaimCode->m_unAccountID = Obj().account_id();
	pClaimCode->m_unCodeType = Obj().code_type();
	pClaimCode->m_rtime32TimeAcquired = Obj().time_acquired();
	WRITE_VAR_CHAR_FIELD( *pClaimCode, VarCharCode, Obj().code().c_str() );
}

void CEconClaimCode::ReadFromRecord( const CSchAssignedClaimCode & code )
{
	const char *pchCode = READ_VAR_CHAR_FIELD( code, m_VarCharCode );
	Obj().set_code_type( code.m_unCodeType );
	Obj().set_time_acquired( code.m_rtime32TimeAcquired );
	Obj().set_code( pchCode );
}


bool BBuildRedemptionURL( CEconClaimCode *pClaimCode, CUtlString &redemptionURL )
{
	const CEconItemDefinition *pItemDef = GEconManager()->GetItemSchema()->GetItemDefinition( pClaimCode->Obj().code_type() );
	if ( pItemDef )
	{
		const char *pOriginalURL = pItemDef->GetDefinitionString( "redeem_url" );
		const char *code = pClaimCode->Obj().code().c_str();
		char url[1024];
		if ( Q_StrSubst( pOriginalURL, "CLAIMCODE", code, url, sizeof( url ) ) )
		{
			redemptionURL = url;
			return true;
		}
	}
	return false;
}

// -----------------------------------------------------------------------------
// Purpose: Gets a summary of what's going on with the GC
// -----------------------------------------------------------------------------
class CJobWG_GetPromoCodes : public CGCGameBaseWGJob
{
public:
	CJobWG_GetPromoCodes( CGCGameBase *pGC ) : CGCGameBaseWGJob( pGC ) {}

	virtual bool BYieldingRunJobFromRequest( KeyValues *pkvRequest, KeyValues *pkvResponse );

private:
};


bool CJobWG_GetPromoCodes::BYieldingRunJobFromRequest( KeyValues *pkvRequest, KeyValues *pkvResponse )
{
	CSteamID actorID( pkvRequest->GetUint64( "token/steamid" ) );

	KeyValues *pkvPromoCodes = pkvResponse->FindKey( "promo_codes", true );

	CSharedObjectCache *pSOCache = m_pGCGameBase->YieldingFindOrLoadSOCache( actorID );
	if ( pSOCache == NULL )
		return true;

	CSharedObjectTypeCache *pTypeCache = pSOCache->FindBaseTypeCache( k_EEconTypeClaimCode );
	if ( pTypeCache == NULL )
		return true;
	
	for ( uint32 i = 0; i < pTypeCache->GetCount(); ++i )
	{
		CEconClaimCode *pClaimCode = (CEconClaimCode*)pTypeCache->GetObject( i );
		const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( pClaimCode->Obj().code_type() );
		if ( pItemDef == NULL )
			continue;

		const CEconTool_ClaimCode *pEconClaimCodeTool = pItemDef->GetTypedEconTool<CEconTool_ClaimCode>();
		if ( pEconClaimCodeTool == NULL )
			continue;

		const char *pClaimCodeName = pEconClaimCodeTool->GetClaimType();
		if ( pClaimCodeName == NULL )
			continue;
		
		CUtlString claimURL;
		if ( BBuildRedemptionURL( pClaimCode, claimURL ) == false )
		{
			SetErrorMessage( pkvResponse, CFmtStr( "Unable to construct redemption url for: %s", pClaimCodeName ), k_EResultFail );
			continue;
		}
		const char *code = pClaimCode->Obj().code().c_str();
		// finally populate the key values
		KeyValues *pkvPromoCode = pkvPromoCodes->CreateNewKey();
		pkvPromoCode->SetString( "code_name", pClaimCodeName );
		pkvPromoCode->SetInt( "timestamp", pClaimCode->Obj().time_acquired() );
		pkvPromoCode->SetString( "code", code );
		pkvPromoCode->SetString( "redeem_url", claimURL.Get() );
	}

	return true;
}

DECLARE_GCWG_JOB( CGCEcon, CJobWG_GetPromoCodes, "GetPromoCodes", k_EGCWebApiPriv_Session )
END_DECLARE_GCWG_JOB( CJobWG_GetPromoCodes);

#endif
