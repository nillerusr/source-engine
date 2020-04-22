//========= Copyright (c), Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "gcsystemaccess.h"
#include "gcjob.h"
#include "gcsdk/gcreportprinter.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{


static GCConVar gcaccess_enable( "gcaccess_enable", "1", "Kill switch that disables all gcaccess tracking and systems" );

DECLARE_GC_EMIT_GROUP( g_EGAccess, access );


//-----------------------------------------------------------------------------------------------------------------------------------------
GCAccessSystem_t GenerateUniqueGCAccessID()
{
	static GCAccessSystem_t s_nID = 0;
	return s_nID++;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// CGCAccessSystem
//-----------------------------------------------------------------------------------------------------------------------------------------

//a GCAccess system that has been registered
class CGCAccessSystem
{
public:
	//the name of the system for display
	CUtlString			m_sName;
	//the unique ID for this system
	GCAccessSystem_t	m_nID;
	//which actions should be enabled or disabled for this system in particular
	uint32				m_nActions;
	uint32				m_nSuppressActions;

	struct AccessStats_t
	{
		AccessStats_t();
		void Add( const AccessStats_t& rhs );
		//how many raw accesses have we had?
		uint32		m_nNumValid;
		//how many failed because the context didn't have rights?
		uint32		m_nNumFail;
		//how many failed because the associated ID wasn't valid?
		uint32		m_nNumFailID;
	};

	struct TrackedJob_t
	{
		CUtlString		m_sContext;
		AccessStats_t	m_Stats;
	};

	//which jobs have violated this system access rights
	CUtlHashMapLarge< uintp, TrackedJob_t >	m_Jobs;
};

//-----------------------------------------------------------------------------------------------------------------------------------------
// CGCAccessContext
//-----------------------------------------------------------------------------------------------------------------------------------------

CGCAccessContext::CGCAccessContext() :
	m_nActions( 0 ),
	m_nSuppressActions( 0 )
{
}

void CGCAccessContext::Init( const char* pszName, uint32 nActions, uint32 nSuppressActions )
{
	m_sName = pszName;
	m_nActions = nActions;
	m_nSuppressActions = nSuppressActions;
}

void CGCAccessContext::AddSystem( GCAccessSystem_t nSystem )
{
	m_nSystems.InsertIfNotFound( nSystem );
}

bool CGCAccessContext::HasSystem( GCAccessSystem_t system ) const
{
	return ( m_nSystems.Find( system ) != m_nSystems.InvalidIndex() );
}

bool CGCAccessContext::IsSubsetOf( const CGCAccessContext& context ) const
{
	FOR_EACH_VEC( m_nSystems, nCurrSystem )
	{
		if( !context.HasSystem( m_nSystems[ nCurrSystem ] ) )
			return false;
	}
	return true;
}

void CGCAccessContext::AddSystemsFrom( const CGCAccessContext& context )
{
	FOR_EACH_VEC( context.m_nSystems, nCurrSystem )
	{
		AddSystem( context.m_nSystems[ nCurrSystem ] );
	}
}

void CGCAccessContext::SetAction( EGCAccessAction eAction, bool bSet )
{
	if( bSet )
		m_nActions |= eAction;
	else
		m_nActions &= ~( uint32 )eAction;
}

void CGCAccessContext::SetSuppressAction( EGCAccessAction eAction, bool bSet )
{
	if( bSet )
		m_nSuppressActions |= eAction;
	else
		m_nSuppressActions &= ~( uint32 )eAction;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// CGCAccess
//-----------------------------------------------------------------------------------------------------------------------------------------

static CGCAccess g_GCAccess;
CGCAccess& GGCAccess()
{
	return g_GCAccess;
}

CGCAccessSystem::AccessStats_t::AccessStats_t()
	: m_nNumFail( 0 )
	, m_nNumFailID( 0 )
	, m_nNumValid( 0 )
{}

void CGCAccessSystem::AccessStats_t::Add( const AccessStats_t& rhs )
{
	m_nNumFail += rhs.m_nNumFail;
	m_nNumFailID += rhs.m_nNumFailID;
	m_nNumValid += rhs.m_nNumValid;
}

CGCAccess::CGCAccess() :
	m_nActions( GCAccessAction_TrackSuccess | GCAccessAction_TrackFail| GCAccessAction_ReturnFail ),
	m_nSuppressActions( 0 )
{
	m_GlobalContext.Init( "Global" );
}

void CGCAccess::RegisterSystem( const char* pszName, GCAccessSystem_t nID, uint32 nActions, uint32 nSupressActions )
{
	//make sure that we don't have a conflict
	int nIndex = m_Systems.Find( nID );
	if( nIndex != m_Systems.InvalidIndex() )
	{
		// !FIXME!
		// DOTAMERGE AssertMsg varargs
		AssertMsg3( false, "Multiple systems conflciting in Register System: ID %d, original: %s, new %s", nID, m_Systems[ nIndex ]->m_sName.String(), pszName );
		return;
	}

	CGCAccessSystem* pSystem = new CGCAccessSystem;
	pSystem->m_sName = pszName;
	pSystem->m_nID = nID;
	pSystem->m_nActions = nActions;
	pSystem->m_nSuppressActions = nSupressActions;
	m_Systems.Insert( nID, pSystem );
}

bool CGCAccess::ValidateAccess( GCAccessSystem_t nSystem )
{
	//global kill switch
	if( !gcaccess_enable.GetBool() )
		return true;

	return InternalValidateAccess( nSystem, CSteamID(), CSteamID() );	
}

bool CGCAccess::ValidateSteamIDAccess( GCAccessSystem_t nSystem, CSteamID steamID )
{
	//global kill switch
	if( !gcaccess_enable.GetBool() )
		return true;

	CSteamID expectedID = ( g_pJobCur ) ? g_pJobCur->SOVALIDATE_GetSteamID() : steamID;
	return InternalValidateAccess( nSystem, steamID, expectedID );	
}

bool CGCAccess::InternalValidateAccess( GCAccessSystem_t nSystem, CSteamID steamID, CSteamID expectedID )
{
	//see if tracking for this system is disabled. This list is almost always empty, so just use a linear search for now.
	FOR_EACH_VEC( m_SuppressAccess, nAction )
	{
		if( m_SuppressAccess[ nAction ].m_nSystem == nSystem )
			return true;
	}

	//assume the global context
	const CGCAccessContext* pContext = &m_GlobalContext;
	const char* pszJobName = "[global]";

	//if we have a job, use it's context
	if( g_pJobCur )
	{
		const CGCJob* pJob = static_cast< const CGCJob* >( g_pJobCur );
		pszJobName = pJob->GetName();
		if( pJob->GetContext() )
			pContext = pJob->GetContext();
	}

	//is this a valid system to access
	bool bValidSteamID = ( steamID == expectedID );
	bool bValidSystem = pContext->HasSystem( nSystem );
	bool bValidAccess = ( bValidSystem && bValidSteamID );

	//determine the actions we want to do for tracking
	uint32 nActions = pContext->GetActions() | m_nActions;
	uint32 nSuppressActions = pContext->GetSuppressActions() | m_nSuppressActions;

	//look up the system that we are accessing
	CGCAccessSystem* pSystem = NULL;
	int nSystemIndex = m_Systems.Find( nSystem );
	if( nSystemIndex == m_Systems.InvalidIndex() )
	{
		// !FIXME! DOTAMERGE
		// AssertMsg varargs
		AssertMsg1( false, "Error: Tracking a system that has not been registered. Make sure to register system %u", nSystem );
	}
	else
	{
		//make sure that we have a stat entry for this job
		pSystem = m_Systems[ nSystemIndex ];
		nActions |= pSystem->m_nActions;
		nSuppressActions |= pSystem->m_nSuppressActions;
	}

	//remove any suppressed actions
	nActions &= ~nSuppressActions;

	//see if we need to track this access
	bool bTrackSuccess = ( nActions & GCAccessAction_TrackSuccess ) && bValidAccess;
	bool bTrackFail    = ( nActions & GCAccessAction_TrackFail ) && !bValidAccess;
	if( ( bTrackSuccess || bTrackFail ) && pSystem )
	{
		//make sure that we have a stat entry for this job
		int nJobIndex = pSystem->m_Jobs.Find( ( uintp )pszJobName );
		if( nJobIndex == pSystem->m_Jobs.InvalidIndex() )
		{
			nJobIndex = pSystem->m_Jobs.Insert( ( uintp )pszJobName );
			pSystem->m_Jobs[ nJobIndex ].m_sContext = pContext->GetName();
		}
		
		//update our stats based upon what was valid and what wasn't
		CGCAccessSystem::AccessStats_t& stats = pSystem->m_Jobs[ nJobIndex ].m_Stats; 
		if( bTrackSuccess )
			stats.m_nNumValid++;

		if( bTrackFail )
		{
			if( !bValidSteamID )
				stats.m_nNumFailID++;
			if( !bValidSystem )
				stats.m_nNumFail++;
		}
	}

	//see if this is a single access we want to try and track
	FOR_EACH_VEC( m_SingleAsserts, nCurrAssert )
	{
		SingleAssert_t* pAssert = m_SingleAsserts[ nCurrAssert ];
		if( nSystem == pAssert->m_System )
		{
			if( V_stricmp( ( pAssert->m_bContext ) ? pContext->GetName() : pszJobName, pAssert->m_sContextOrJob ) == 0 )
			{
				//log this assert
				{
					// !FIXME! DOTAMERGE
					//CGCInterface::CDisableAssertRateLimit disableAssertLimit;

					// !FIXME! DOTAMERGE
					// AssertMsg varargs
					AssertMsg3( false, "GCSystemAccess Caught %s (context %s) calling into %s", pszJobName, pContext->GetName(), pSystem ? pSystem->m_sName.String() : "unknown" );
				}

				//and clear it
				delete pAssert;
				m_SingleAsserts.FastRemove( nCurrAssert );
			}
		}
	}

	//at this point, if it is a valid access, just bail
	if( bValidAccess )
		return true;

	//otherwise, handle the failure case
	if( nActions & ( GCAccessAction_Msg | GCAccessAction_Assert ) )
	{
		int nSystemIndex = m_Systems.Find( nSystem );
		const char* pszSystem = ( nSystemIndex != m_Systems.InvalidIndex() ) ? m_Systems[ nSystemIndex ]->m_sName.String() : "<unregistered>";

		//display a message based upon if it was a system or ID violation
		CFmtStr1024 szMsg;
		if( !bValidSystem )
		{
			szMsg.sprintf( "Job %s Accessed invalid system %s (%u) while in context %s\n", pszJobName, pszSystem, nSystem, pContext->GetName() );
		}
		else
		{
			szMsg.sprintf( "Job %s Accessed invalid steam ID %s but expected %s in system %s (%u) while in context %s\n", pszJobName, steamID.RenderLink(), expectedID.RenderLink(), pszSystem, nSystem, pContext->GetName() );
		}

		if( nActions & GCAccessAction_Msg )
			EG_MSG( g_EGAccess, "%s", szMsg.String() );
		if( nActions & GCAccessAction_Assert )
		{
			// !FIXME! DOTAMERGE
			// AssertMsg varargs
			AssertMsg1( false, "%s", szMsg.String() );
		}
	}

	return !( nActions & GCAccessAction_ReturnFail );
}

void CGCAccess::SuppressAccess( GCAccessSystem_t nSystem, bool bEnable )
{
	//see if it is an existing item already suppressed that we need to modify the ref count of
	FOR_EACH_VEC( m_SuppressAccess, nAccess )
	{
		if( m_SuppressAccess[ nAccess ].m_nSystem == nSystem )
		{
			if( bEnable )
			{
				//to enable, we want to decrease the disable count, or remove if we are fully re-enabled
				if( m_SuppressAccess[ nAccess ].m_nCount <= 1 )
					m_SuppressAccess.FastRemove( nAccess );
				else
					m_SuppressAccess[ nAccess ].m_nCount--;
			}
			else
			{
				m_SuppressAccess[ nAccess ].m_nCount++;
			}
			return;
		}
	}

	//only add it to the list if we are disabling
	if( !bEnable )
	{
		SuppressAccess_t access;
		access.m_nSystem = nSystem;
		access.m_nCount = 1;
		m_SuppressAccess.AddToTail( access );
	}
}

void CGCAccess::ClearSystemStats()
{
	FOR_EACH_MAP_FAST( m_Systems, nSystem )
	{
		m_Systems[ nSystem ]->m_Jobs.Purge();
	}
}

bool CGCAccess::CatchSingleAssert( const char* pszSystem, bool bContext, const char* pszContextOrJob )
{
	//find the system we are referencing so we don't add invalid breakpoints if we can avoid it
	GCAccessSystem_t nSystemID = ( GCAccessSystem_t )-1;
	FOR_EACH_MAP_FAST( m_Systems, nSystem )
	{
		if( V_stricmp( m_Systems[ nSystem ]->m_sName, pszSystem ) == 0 )
		{
			nSystemID = m_Systems.Key( nSystem );
			break;
		}
	}

	if( nSystemID == ( GCAccessSystem_t )-1 )
		return false;

	SingleAssert_t* pAssert = new SingleAssert_t;
	pAssert->m_System			= nSystemID;
	pAssert->m_bContext			= bContext;
	pAssert->m_sContextOrJob	= pszContextOrJob;
	m_SingleAsserts.AddToTail( pAssert );
	return true;
}

void CGCAccess::ClearSingleAsserts()
{
	m_SingleAsserts.PurgeAndDeleteElements();
}


//given a display type and a stats object, determines if it meets the criteria
static bool ShouldDisplayStats( CGCAccess::EDisplay eDisplay, const CGCAccessSystem::AccessStats_t& stats )
{
	if( eDisplay == CGCAccess::eDisplay_Referenced )
		return ( stats.m_nNumFail + stats.m_nNumFailID + stats.m_nNumValid ) > 0;
	if( eDisplay == CGCAccess::eDisplay_Violations )
		return ( stats.m_nNumFail + stats.m_nNumFailID ) > 0;
	if( eDisplay == CGCAccess::eDisplay_IDViolations )
		return stats.m_nNumFailID > 0;

	//unknown, so just assume all
	return true;
}


void CGCAccess::ReportJobs( const char* pszContext, EDisplay eDisplay ) const
{
	//collect all of the job stats into a single summary table
	CUtlHashMapLarge< uintp, CGCAccessSystem::TrackedJob_t > jobs;
	FOR_EACH_MAP_FAST( m_Systems, nSystem )
	{
		const CGCAccessSystem* pSystem = m_Systems[ nSystem ];
		FOR_EACH_MAP_FAST( pSystem->m_Jobs, nJob )
		{
			const CGCAccessSystem::TrackedJob_t& job = pSystem->m_Jobs[ nJob ];
			//skip any contexts we don't care about
			if( pszContext && ( V_stricmp( pszContext, job.m_sContext ) != 0 ) )
				continue;
			if( !ShouldDisplayStats( eDisplay, job.m_Stats ) )
				continue;

			int nIndex = jobs.Find( pSystem->m_Jobs.Key( nJob ) );
			if( nIndex == jobs.InvalidIndex() )
			{
				nIndex = jobs.Insert( pSystem->m_Jobs.Key( nJob ) );
				jobs[ nIndex ].m_sContext = job.m_sContext;
			}

			jobs[ nIndex ].m_Stats.Add( job.m_Stats );
		}
	}

	CGCReportPrinter rp;
	rp.AddStringColumn( "Job" );
	rp.AddStringColumn( "Context" );
	rp.AddIntColumn( "Valid", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "Invalid", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "InvalidID", CGCReportPrinter::eSummary_Total );

	FOR_EACH_MAP_FAST( jobs, nJob )
	{
		rp.StrValue( ( const char* )jobs.Key( nJob ), CFmtStr( "gcaccess_dump_job \"%s\" %d", ( const char* )jobs.Key( nJob ), eDisplay ).String() );
		rp.StrValue( jobs[ nJob ].m_sContext, CFmtStr( "gcaccess_dump_context \"%s\" %d", jobs[ nJob ].m_sContext.String(), eDisplay ).String() );
		rp.IntValue( jobs[ nJob ].m_Stats.m_nNumValid );
		rp.IntValue( jobs[ nJob ].m_Stats.m_nNumFail );
		rp.IntValue( jobs[ nJob ].m_Stats.m_nNumFailID );
		rp.CommitRow();
	}

	rp.SortReport( "Job", false );
	rp.PrintReport( SPEW_CONSOLE );
}

void CGCAccess::ReportSystems( const char* pszContext, EDisplay eDisplay ) const
{
	CGCReportPrinter rp;
	rp.AddStringColumn( "System" );
	rp.AddIntColumn( "ID", CGCReportPrinter::eSummary_None );
	rp.AddIntColumn( "Jobs", CGCReportPrinter::eSummary_None );
	rp.AddIntColumn( "Valid", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "Invalid", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "InvalidID", CGCReportPrinter::eSummary_Total );

	FOR_EACH_MAP_FAST( m_Systems, nSystem )
	{
		const CGCAccessSystem* pSystem = m_Systems[ nSystem ];
		CGCAccessSystem::AccessStats_t stats;
		FOR_EACH_MAP_FAST( pSystem->m_Jobs, nJob )
		{
			const CGCAccessSystem::TrackedJob_t& job = pSystem->m_Jobs[ nJob ];
			//skip any contexts we don't care about
			if( pszContext && ( V_stricmp( pszContext, job.m_sContext ) != 0 ) )
				continue;		

			stats.Add( job.m_Stats );
		}

		if( !ShouldDisplayStats( eDisplay, stats ) )
			continue;

		rp.StrValue( pSystem->m_sName, CFmtStr( "gcaccess_dump_system \"%s\" %d", pSystem->m_sName.String(), eDisplay ).String() );
		rp.IntValue( pSystem->m_nID );
		rp.IntValue( pSystem->m_Jobs.Count() );
		rp.IntValue( stats.m_nNumValid );
		rp.IntValue( stats.m_nNumFail );
		rp.IntValue( stats.m_nNumFailID );
		rp.CommitRow();
	}

	rp.SortReport( "System", false );
	rp.PrintReport( SPEW_CONSOLE );
}

void CGCAccess::FullReport( const char* pszSystemFilter, const char* pszContextFilter, const char* pszJobFilter, EDisplay eDisplay ) const
{
	CGCReportPrinter rp;
	rp.AddStringColumn( "Job" );
	rp.AddStringColumn( "Context" );
	rp.AddStringColumn( "System" );
	rp.AddIntColumn( "Valid", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "Invalid", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "InvalidID", CGCReportPrinter::eSummary_Total );

	FOR_EACH_MAP_FAST( m_Systems, nSystem )
	{
		const CGCAccessSystem* pSystem = m_Systems[ nSystem ];
		if( pszSystemFilter && V_stricmp( pszSystemFilter, pSystem->m_sName ) )
			continue;

		FOR_EACH_MAP_FAST( pSystem->m_Jobs, nJob )
		{
			const CGCAccessSystem::AccessStats_t& stats = pSystem->m_Jobs[ nJob ].m_Stats; 
			const char* pszJob = ( const char* )pSystem->m_Jobs.Key( nJob );
			const char* pszContext = ( const char* )pSystem->m_Jobs[ nJob ].m_sContext;

			if( !ShouldDisplayStats( eDisplay, stats ) )
				continue;			
			if( pszJobFilter && V_stricmp( pszJobFilter, pszJob ) )
				continue;
			if( pszContextFilter && V_stricmp( pszContextFilter, pszContext ) )
				continue;

			rp.StrValue( pszJob, CFmtStr( "gcaccess_dump_job \"%s\" %d", pszJob, eDisplay ).String() );
			rp.StrValue( pszContext, CFmtStr( "gcaccess_dump_context \"%s\" %d", pszContext, eDisplay ).String() );
			rp.StrValue( pSystem->m_sName, CFmtStr( "gcaccess_dump_system \"%s\" %d", pSystem->m_sName.String(), eDisplay ).String() );
			rp.IntValue( stats.m_nNumValid );
			rp.IntValue( stats.m_nNumFail );
			rp.IntValue( stats.m_nNumFailID );
			rp.CommitRow();
		}
	}

	rp.SortReport( "Context", false );
	rp.PrintReport( SPEW_CONSOLE );
}

void CGCAccess::DependencyReport( const char* pszSystem, EDisplay eDisplay ) const
{
	//first off, we need to find the system we are scanning for dependencies on
	const CGCAccessSystem* pMatchSystem = NULL;
	FOR_EACH_MAP_FAST( m_Systems, nSystem )
	{
		const CGCAccessSystem* pSystem = m_Systems[ nSystem ];
		if( V_stricmp( pszSystem, pSystem->m_sName ) == 0 )
		{
			pMatchSystem = pSystem;
			break;
		}
	}

	if( !pMatchSystem )
	{
		EG_MSG( SPEW_CONSOLE, "Unable to find system %s for dependency report\n", pszSystem );
		return;
	}

	//now generate our report
	CGCReportPrinter rp;
	rp.AddStringColumn( "Job" );
	rp.AddStringColumn( "Context" );
	rp.AddStringColumn( "System" );
	rp.AddIntColumn( "Valid", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "Invalid", CGCReportPrinter::eSummary_Total );
	rp.AddIntColumn( "InvalidID", CGCReportPrinter::eSummary_Total );

	FOR_EACH_MAP_FAST( m_Systems, nSystem )
	{
		const CGCAccessSystem* pSystem = m_Systems[ nSystem ];
		//skip ourself
		if( pSystem == pMatchSystem )
			continue;

		FOR_EACH_MAP_FAST( pSystem->m_Jobs, nJob )
		{
			const CGCAccessSystem::AccessStats_t& stats = pSystem->m_Jobs[ nJob ].m_Stats; 
			const char* pszJob = ( const char* )pSystem->m_Jobs.Key( nJob );
			const char* pszContext = ( const char* )pSystem->m_Jobs[ nJob ].m_sContext;

			//skip any job that isn't using our match system
			if( !pMatchSystem->m_Jobs.HasElement( ( uintp )pszJob ) )
				continue;
			if( !ShouldDisplayStats( eDisplay, stats ) )
				continue;	

			rp.StrValue( pszJob, CFmtStr( "gcaccess_dump_job \"%s\" %d", pszJob, eDisplay ).String() );
			rp.StrValue( pszContext, CFmtStr( "gcaccess_dump_context \"%s\" %d", pszContext, eDisplay ).String() );
			rp.StrValue( pSystem->m_sName, CFmtStr( "gcaccess_dump_system \"%s\" %d", pSystem->m_sName.String(), eDisplay ).String() );
			rp.IntValue( stats.m_nNumValid );
			rp.IntValue( stats.m_nNumFail );
			rp.IntValue( stats.m_nNumFailID );
			rp.CommitRow();
		}
	}

	rp.SortReport( "System", false );
	rp.PrintReport( SPEW_CONSOLE );


}

GC_CON_COMMAND_PARAMS( gcaccess_dump_job, 1, "<Job Name> Dumps all of the accesses associated with the specified job" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 3 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 2 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().FullReport( NULL, NULL, args[ 1 ], eDisplay );
}

GC_CON_COMMAND_PARAMS( gcaccess_dump_context, 1, "<Job Name> Dumps all of the accesses associated with the specified context" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 3 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 2 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().FullReport( NULL, args[ 1 ], NULL, eDisplay );
}

GC_CON_COMMAND_PARAMS( gcaccess_dump_system, 1, "<Job Name> Dumps all of the accesses associated with the specified system" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 3 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 2 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().FullReport( args[ 1 ], NULL, NULL, eDisplay );
}

GC_CON_COMMAND_PARAMS( gcaccess_dump_context_jobs, 1, "<Context Name> Dumps all of the accesses associated with the specified context" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 3 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 2 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().ReportJobs( args[ 1 ], eDisplay );
}

GC_CON_COMMAND_PARAMS( gcaccess_dump_context_systems, 1, "<Context Name> Dumps all of the accesses associated with the specified context" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 3 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 2 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().ReportSystems( args[ 1 ], eDisplay );
}

GC_CON_COMMAND_PARAMS( gcaccess_dump_system_dependencies, 1, "<System Name> This will dump out for all jobs that reference the specified system, what other systems they reference" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 3 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 2 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().DependencyReport( args[ 1 ], eDisplay );
}

GC_CON_COMMAND( gcaccess_dump_jobs, "Dumps all the jobs that have been tracked with the gcaccess system as well as count of violations" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 2 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 1 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().ReportJobs( NULL, eDisplay);
}

GC_CON_COMMAND( gcaccess_dump_systems, "Dumps all the systems that have been tracked with the gcaccess system as well as count of violations" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 2 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 1 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().ReportSystems( NULL, eDisplay );
}

GC_CON_COMMAND( gcaccess_dump_full, "Dumps all the information tracked by the gcaccess" )
{
	CGCAccess::EDisplay eDisplay = ( args.ArgC() >= 2 ) ? ( CGCAccess::EDisplay )V_atoi( args[ 1 ] ) : CGCAccess::eDisplay_Referenced;
	GGCAccess().FullReport( NULL, NULL, NULL, eDisplay );
}

GC_CON_COMMAND_PARAMS( gcaccess_catch_context, 2, "<system> <context> - Catches and generates an assert when the specified context calls into the provided system. This will only generate a single assert" )
{
	if( !GGCAccess().CatchSingleAssert( args[ 1 ], true, args[ 2 ] ) )
		EG_MSG( SPEW_CONSOLE, "Unable to register catch, likely \'%s\' is not a valid system.", args[ 1 ] );
}

GC_CON_COMMAND_PARAMS( gcaccess_catch_job, 2, "<system> <job> - Catches and generates an assert when the specified job calls into the provided system. This will only generate a single assert" )
{
	if( !GGCAccess().CatchSingleAssert( args[ 1 ], false, args[ 2 ] ) )
		EG_MSG( SPEW_CONSOLE, "Unable to register catch, likely \'%s\' is not a valid system.", args[ 1 ] );
}

GC_CON_COMMAND( gcaccess_catch_clear, "Clears all previously specified catches" )
{
	GGCAccess().ClearSingleAsserts();
}

} //namespace GCSDK
