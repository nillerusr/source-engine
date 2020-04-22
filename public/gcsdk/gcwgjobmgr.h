//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef GCWGJOB_H
#define GCWGJOB_H
#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{

class CGCWGJobMgr;

// defines a single parameter to a web api func
struct WebApiParam_t
{
	const char *m_pchParam;
	const char *m_pchDescription;
	bool m_bOptional;					// true if optional
};

//-----------------------------------------------------------------------------
// Privilege type for WG requests
// NOTE:  This enum is a copy of EWegApiPrivilege from servercommon.h.
enum EGCWebApiPrivilege
{
	k_EGCWebApiPriv_None		= 0,					// doens't require any privileges
	k_EGCWebApiPriv_Account		= 1,					// user must have a Steam account with password set
	k_EGCWebApiPriv_Approved	= 2,					// user must not be blocked from community activity
	k_EGCWebApiPriv_Session		= 3,					// user must have a current Steam3 session
	k_EGCWebApiPriv_Support		= 4,					// user must have Support flag set
	k_EGCWebApiPriv_Admin		= 5,					// user must have Admin flag set
	//////////////////////////////////////////////////////////////////////////
	//	Steamworks Application Editing - 
	//
	//	This represents a minimal requirement - The user must have some of the 
	//	EAppRights available to his account for a particular application.
	//	This value is stored in the g_WebApiFuncs table.
	//
	//	The functions dispatched to from the g_WebApiFuncs table are responsible 
	//	for doing finer grain permissions checks.
	//	At this time, only the k_EAppRightManageCEG check is performed at the coarser grain.
	//
	//	Some privileges such k_EAppRightEditInfo are implemented entirely within the 
	//	Web Server's .php code, as these rights do not manipulate data through the Web Gateway, 
	//	but manipulate through direct access to file system files and perforce operations !	
	//
	k_EGCWebApiPriv_EditApp		= 6,					// user has some rights onto specific app - is publisher-affiliated and rights match app (or is admin)
	//
	//	End Steamworks Application Editing - 
	//
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//	Steamworks Publisher Editing - 
	//
	//	These represent requests for particular rights involving the manipulation of 
	//	publisher data.  k_EGCWebApiPriv_MemberPublisher only requires that the user be a member of a publisher, 
	//	whereas k_EGCWebApiPriv_EditPublisher specifically requires the user has k_EPubRightManagerUsers permission
	//	within a particular publisher.  That is because at this time k_EPubRightManagerUsers is the ONLY 
	//	right defined so the coarse grain view of k_EGCWebApiPriv_EditPublisher exactly matches the finer grain
	//	view defined by EPubRights.	
	//
	k_EGCWebApiPriv_MemberPublisher = 7,				// user is publisher-affiliated with specific publisher (or is admin)
	k_EGCWebApiPriv_EditPublisher	= 8,				// user can edit specific publisher (or is admin)
	//
	//	End Steamworks Publisher Editing - 
	//
	//////////////////////////////////////////////////////////////////////////
	k_EGCWebApiPriv_AccountOptional = 9,				// validate the token if we get one but also allow public requests through
};


//-----------------------------------------------------------------------------

struct WebApiFunc_t
{
	const char *m_pchRequestName;
	const char *m_pchRequestHandlerJobName;
	EGCWebApiPrivilege m_eRequiredPrivilege;
	WebApiParam_t m_rgParams[20];
};

class CGCWGJobMgr
{
public:
	CGCWGJobMgr( );
	~CGCWGJobMgr();
	bool BHandleMsg( IMsgNetPacket *pNetPacket );
	static CUtlDict< const WebApiFunc_t* > &GetWGRequestMap();

	static void SendErrorMessage( const CGCMsg<MsgGCWGRequest_t> & msg, const char *pchErrorMsg, int32 nResult );
	static void SetErrorMessage( KeyValues *pkvErr, const char *pchErrorMsg, int32 nResult );
	static void SendResponse( const CGCMsg<MsgGCWGRequest_t> & msg, KeyValues *pkvResponse, bool bResult );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
	static void ValidateStatics( CValidator &validator );
#endif // DBGFLAG_VALIDATE

protected:
	static void RegisterWGJob( const WebApiFunc_t *pWGJobType, const JobType_t *pJobCreationFunc );
	friend void GCWGJob_RegisterWGJobType( const WebApiFunc_t *pWGJobType, const JobType_t *pJobCreationFunc );

	bool BVerifyPrivileges( const CGCMsg<MsgGCWGRequest_t> & msg, const WebApiFunc_t * pFunc );
	bool BVerifyParams( const CGCMsg<MsgGCWGRequest_t> & msg, const WebApiFunc_t * pFunc );
};

inline void GCWGJob_RegisterWGJobType( const WebApiFunc_t *pWGJobType, const JobType_t *pJobCreationFunc )
{
	CGCWGJobMgr::RegisterWGJob( pWGJobType, pJobCreationFunc );
}

// declares a job as a wg job, require/optional params should be placed between begin and end declare.
#define DECLARE_GCWG_JOB( gcbaseSubclass, jobclass, requestname, requiredprivilege ) \
	CJob *CreateWGJob_##jobclass( gcbaseSubclass *pvParent, void * pvStartParam ); \
	static const JobType_t g_JobType_##jobclass = { #jobclass, k_EGCMsgInvalid, k_EServerTypeGC, (JobCreationFunc_t)CreateWGJob_##jobclass }; \
	CJob *CreateWGJob_##jobclass( gcbaseSubclass *pvParent, void * pvStartParam ) \
{ \
	CJob *job = CJob::AllocateJob<jobclass>( pvParent ); \
	Job_SetJobType( *job, &g_JobType_##jobclass ); \
	if ( pvStartParam ) job->SetStartParam( pvStartParam ); \
	return job; \
} \
	static const WebApiFunc_t g_WGRequestInfo_##jobclass = { requestname, #jobclass, requiredprivilege, {

#define REQUIRED_GCWG_PARAM( pstrParameter, pstrDescription ) { pstrParameter, pstrDescription, false },

#define OPTIONAL_GCWG_PARAM( pstrParameter, pstrDescription ) { pstrParameter, pstrDescription, true },

#define END_DECLARE_GCWG_JOB( jobclass ) } }; \
	static class CRegWGJob_##jobclass \
{ \
public: CRegWGJob_##jobclass() \
{ \
	GCWGJob_RegisterWGJobType( &g_WGRequestInfo_##jobclass, &g_JobType_##jobclass ); \
} \
} g_RegWGJob_##jobclass;

// quick and dirty - register a job with no required/optional parameters
#define REG_GCWG_JOB( jobclass, requestname, requiredprivilege )  \
	DECLARE_WG_JOB( jobclass, requestname, requiredprivilege ) \
	END_DECLARE_GCWG_JOB( jobclass )

} // namespace GCSDK

#endif