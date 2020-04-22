//========= Copyright Valve Corporation, All rights reserved. ============//
#include "stdafx.h"
#include "enumutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

ENUMSTRINGS_START( EGCWebApiPrivilege )
{ k_EGCWebApiPriv_None, "None" },	
{ k_EGCWebApiPriv_Account, "Account" },
{ k_EGCWebApiPriv_Approved, "Approved" },
{ k_EGCWebApiPriv_Session, "Session" },
{ k_EGCWebApiPriv_Support, "Support" },
{ k_EGCWebApiPriv_Admin, "Admin" },
{ k_EGCWebApiPriv_EditApp, "EditApp" },
{ k_EGCWebApiPriv_MemberPublisher, "MemberPublisher" },
{ k_EGCWebApiPriv_EditPublisher, "EditPublisher" },
{ k_EGCWebApiPriv_AccountOptional, "AccountOptional" },
ENUMSTRINGS_END( EGCWebApiPrivilege );


//-----------------------------------------------------------------------------
// Purpose: Singleton accessor to registered WG jobs
//-----------------------------------------------------------------------------
CUtlDict< const JobCreationFunc_t* > &GMapGCWGJobCreationFuncs()
{
	static CUtlDict< const JobCreationFunc_t* > s_MapWGJobCreationFuncs;
	return s_MapWGJobCreationFuncs;
}


//-----------------------------------------------------------------------------
// Purpose: Singleton accessor to registered WG jobs
//-----------------------------------------------------------------------------
CUtlDict< const WebApiFunc_t* > &GMapGCWGRequestInfo()
{
	static CUtlDict< const WebApiFunc_t* > s_MapWGRequestInfo;
	return s_MapWGRequestInfo;
}



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGCWGJobMgr::CGCWGJobMgr( )
{

}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCWGJobMgr::~CGCWGJobMgr()
{

}


//-----------------------------------------------------------------------------
// Purpose: Hub has receieved a k_EMsgWGRequest message, find & dispatch WG Job
//-----------------------------------------------------------------------------
bool CGCWGJobMgr::BHandleMsg( IMsgNetPacket *pNetPacket )
{
	CGCMsg<MsgGCWGRequest_t> msg( pNetPacket );
	AssertMsg( msg.GetEMsg() == k_EGCMsgWGRequest, "GCWGJobMgr asked to route a message, but it is not a MsgWGRequest" );

	CUtlString strRequestName;
	msg.BReadStr( &strRequestName );

	int iJobType = GMapGCWGRequestInfo().Find( strRequestName.Get() );

	if ( !GMapGCWGRequestInfo().IsValidIndex( iJobType ) )
	{
		EmitError( SPEW_GC, "Unable to find GC WG handler %s", strRequestName.Get() );
		SendErrorMessage( msg, "Unknown method", k_EResultInvalidParam );
		return false;
	}

	const WebApiFunc_t * pFunc = GMapGCWGRequestInfo()[iJobType];
	if( !BVerifyPrivileges( msg, pFunc ) )
		return false;

	CGCWGJob *job = (CGCWGJob *)(*((GMapGCWGJobCreationFuncs())[iJobType]))( GGCBase(), NULL );
	Assert( job );

	job->SetWebApiFunc( pFunc );

	job->StartJobFromNetworkMsg( pNetPacket, msg.Hdr().m_JobIDSource );
	return true;
}


bool CGCWGJobMgr::BVerifyPrivileges( const CGCMsg<MsgGCWGRequest_t> & msg, const WebApiFunc_t * pFunc )
{
	if(msg.Body().m_unPrivilege != (uint32)pFunc->m_eRequiredPrivilege )
	{
		SendErrorMessage( msg, 
			CFmtStr( "Privilege mismatch in %s gc call. Expected: %s, actual: %s", pFunc->m_pchRequestName, PchNameFromEGCWebApiPrivilege( pFunc->m_eRequiredPrivilege ), PchNameFromEGCWebApiPrivilege( (EGCWebApiPrivilege)msg.Body().m_unPrivilege ) ),
			k_EResultAccessDenied );
		return false;
	}
	else
	{
		return true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sends an error message in response to a WG request
// Inputs:	msg - The message we're responding to. This causes the error to
//				be routed to the right place and eventually back to the right
//				browser.
//			pchErrorMsg - The message to display
//			nResult - The error code to return
//-----------------------------------------------------------------------------
void CGCWGJobMgr::SendErrorMessage( const CGCMsg<MsgGCWGRequest_t> & msg, const char *pchErrorMsg, int32 nResult )
{
	KeyValuesAD pkvErr( "error" );
	pkvErr->SetString( "error", pchErrorMsg );
	pkvErr->SetInt( "success", nResult );

	SendResponse( msg, pkvErr, false );
}


//-----------------------------------------------------------------------------
// Purpose: Sets an error message in a response to a WG request
// Inputs:	pkvResponse - The response KV block to set the error in
//			pchErrorMsg - The message to display
//			nResult - The error code to return
//-----------------------------------------------------------------------------
void CGCWGJobMgr::SetErrorMessage( KeyValues *pkvErr, const char *pchErrorMsg, int32 nResult )
{
	pkvErr->SetString( "error", pchErrorMsg );
	pkvErr->SetInt( "success", nResult );
}


//-----------------------------------------------------------------------------
// Purpose: Sends a message in response to a WG request
// Inputs:	msg - The message we're responding to. This causes the error to
//				be routed to the right place and eventually back to the right
//				browser.
//			pkvResponse - The KeyValues containing the response data
//-----------------------------------------------------------------------------
void CGCWGJobMgr::SendResponse( const CGCMsg<MsgGCWGRequest_t> & msg, KeyValues *pkvResponse, bool bResult )
{
	//prepare response msg
	CGCMsg<MsgGCWGResponse_t> msgResponse( k_EGCMsgWGResponse, msg );
	CUtlBuffer bufResponse;
	KVPacker packer;
	if ( packer.WriteAsBinary( pkvResponse, bufResponse ) )
	{
		msgResponse.AddVariableLenData( bufResponse.Base(), bufResponse.TellPut() );
		msgResponse.Body().m_cubKeyValues = bufResponse.TellPut();
		msgResponse.Body().m_bResult = bResult;
	}
	else
	{
		msgResponse.Body().m_cubKeyValues = 0;
		AssertMsg( false, "Failed to serialize WG response" );
	}
	msgResponse.Hdr().m_JobIDTarget = msg.Hdr().m_JobIDSource;

	GGCBase()->BSendSystemMessage( msgResponse );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a WG job handler to the global list
//-----------------------------------------------------------------------------
void CGCWGJobMgr::RegisterWGJob( const WebApiFunc_t *pWGJobType, const JobType_t *pJobCreationFunc )
{
	const char *pchRequestName = pWGJobType->m_pchRequestName;
	GMapGCWGJobCreationFuncs().Insert( pchRequestName, &(pJobCreationFunc->m_pJobFactory) );
	GMapGCWGRequestInfo().Insert( pchRequestName, pWGJobType );
}


//-----------------------------------------------------------------------------
// Purpose: Get the list of registered WG jobs
//-----------------------------------------------------------------------------
CUtlDict< const WebApiFunc_t* > &CGCWGJobMgr::GetWGRequestMap()
{
	return GMapGCWGRequestInfo();
}


#ifdef DBGFLAG_VALIDATE
void CGCWGJobMgr::Validate( CValidator &validator, const char *pchName )
{
}

void CGCWGJobMgr::ValidateStatics( CValidator &validator )
{
	ValidateObj( GMapGCWGJobCreationFuncs() );
	ValidateObj( GMapGCWGRequestInfo() );
}

#endif // DBGFLAG_VALIDATE



} // namespace GCSDK
