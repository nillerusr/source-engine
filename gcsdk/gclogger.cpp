//========= Copyright (c), Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"
#include "steamextra/gamecoordinator/igamecoordinatorhost.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Maximum length of a sprintf'ed logging message.
//-----------------------------------------------------------------------------
const int MAX_LOGGING_MESSAGE_LENGTH = 2048;

namespace GCSDK
{

int g_nMaxSpewLevel = 4;
int g_nMaxLogLevel = 4;

//-----------------------------------------------------------------------------
// Purpose: Sends an event back to the GC host for logging
// Input:	pchGroupName - group to log to
//			spewType - the type of the message
//			iLevel - level of spew ( 0..4 )
//			iLevelLog - level for logging
//			pchMsg - printf format
//-----------------------------------------------------------------------------
void EmitBaseMessageV( const char *pchGroupName, SpewType_t spewType, int iSpewLevel, int iLevelLog, const char *pchMsg, va_list vaArgs )
{
	VPROF_BUDGET( "GCHost", VPROF_BUDGETGROUP_STEAM );

	char pchBuf[ MAX_LOGGING_MESSAGE_LENGTH ];
	Q_vsnprintf( pchBuf, MAX_LOGGING_MESSAGE_LENGTH, pchMsg, vaArgs );

#ifdef GC
// !FIXME! DOTAMERGE
//	// If this is coming from the context of a job, then allow that job to
//	// override our emit via a specified handler.
//	if ( g_pJobCur )
//	{
//		IJobEmitSpewHandler *pHandler = g_pJobCur->GetEmitSpewHandler();
//		if ( pHandler )
//		{
//			bool bOutputSpew = pHandler->OnEmitSpew( pchGroupName, spewType, iSpewLevel, iLevelLog, pchBuf );
//			if ( !bOutputSpew )
//			{
//				return;
//			}
//		}
//	}

	{
		VPROF_BUDGET( "GCHost - EmitMessage", VPROF_BUDGETGROUP_STEAM );
		// !FIXME! DOTAMERGE
		//GGCInterface()->EmitSpew( pchGroupName, spewType, iSpewLevel, iLevelLog, pchBuf );
		GGCHost()->EmitMessage( pchGroupName, spewType, iSpewLevel, iLevelLog, pchBuf );
	}
#else
	// TODO: Actually log it
	DevMsg( "%s", pchBuf );
#endif
}

//similar to the above, but takes in a group, and handles filtering messages out that are disabled, and extracting other info from the group
void EmitBaseMessageV( const CGCEmitGroup& Group, SpewType_t spewType, int iConsoleLevel, int iLogLevel, const char *pchMsg, va_list vaArgs )
{
	int iClampConsoleLevel	= ( iConsoleLevel <= MIN( Group.GetConsoleLevel(), g_nMaxSpewLevel ) ) ? SPEW_ALWAYS : SPEW_NEVER;
	int iClampLogLevel		= ( iLogLevel <= MIN( Group.GetLogLevel(), g_nMaxLogLevel ) ) ? LOG_ALWAYS : LOG_NEVER;
	if( ( iClampConsoleLevel != SPEW_NEVER ) || ( iClampLogLevel != LOG_NEVER ) )
		EmitBaseMessageV( Group.GetName(), spewType, iClampConsoleLevel, iClampLogLevel, pchMsg, vaArgs );
}

//------------------------------
// AssertError

void CGCEmitGroup::Internal_AssertError( const char *pchMsg, ... ) const
{
	va_list args;
	va_start( args, pchMsg );
	AssertErrorV( pchMsg, args );
	va_end( args );
}

void CGCEmitGroup::AssertErrorV( const char *pchMsg, va_list vaArgs ) const
{
	EmitBaseMessageV( *this, SPEW_ASSERT, 1, 1, pchMsg, vaArgs );
}

//------------------------------
// Error

void CGCEmitGroup::Internal_Error( const char *pchMsg, ... ) const
{
	va_list args;
	va_start( args, pchMsg );
	ErrorV( pchMsg, args );
	va_end( args );
}

void CGCEmitGroup::ErrorV( const char *pchMsg, va_list vaArgs ) const
{
	EmitBaseMessageV( *this, SPEW_ERROR, 1, 1, pchMsg, vaArgs );
}

//------------------------------
// Warning

void CGCEmitGroup::Internal_Warning( const char *pchMsg, ... ) const
{
	va_list args;
	va_start( args, pchMsg );
	WarningV( pchMsg, args );
	va_end( args );
}

void CGCEmitGroup::WarningV( const char *pchMsg, va_list vaArgs ) const
{
	EmitBaseMessageV( *this, SPEW_WARNING, 2, 2, pchMsg, vaArgs );
}

//------------------------------
// Msg

void CGCEmitGroup::Internal_Msg( const char *pchMsg, ... ) const
{
	va_list args;
	va_start( args, pchMsg );
	MsgV( pchMsg, args );
	va_end( args );
}

void CGCEmitGroup::MsgV( const char *pchMsg, va_list vaArgs ) const
{
	EmitBaseMessageV( *this, SPEW_MESSAGE, 3, 3, pchMsg, vaArgs );
}

//------------------------------
// Verbose

void CGCEmitGroup::Internal_Verbose( const char *pchMsg, ... ) const
{
	va_list args;
	va_start( args, pchMsg );
	VerboseV( pchMsg, args );
	va_end( args );
}

void CGCEmitGroup::VerboseV( const char *pchMsg, va_list vaArgs ) const
{
	EmitBaseMessageV( *this, SPEW_MESSAGE, 4, 4, pchMsg, vaArgs );
}


//------------------------------
// Verbose

void CGCEmitGroup::Internal_BoldMsg( const char *pchMsg, ... ) const
{
	va_list args;
	va_start( args, pchMsg );
	BoldMsgV( pchMsg, args );
	va_end( args );
}

void CGCEmitGroup::BoldMsgV( const char *pchMsg, va_list vaArgs ) const
{
	// !FIXME! DOTAMERGE
	//EmitBaseMessageV( *this, SPEW_BOLD_MESSAGE, 1, 1, pchMsg, vaArgs );
	EmitBaseMessageV( *this, SPEW_MESSAGE, 1, 1, pchMsg, vaArgs );
}


//------------------------------
// General Emit


void CGCEmitGroup::Internal_Emit( EMsgLevel eLvl, PRINTF_FORMAT_STRING const char *pchMsg, ... ) const
{
	va_list args;
	va_start( args, pchMsg );
	EmitV( eLvl, pchMsg, args );
	va_end( args );
}

void CGCEmitGroup::EmitV( EMsgLevel eLvl, PRINTF_FORMAT_STRING const char *pchMsg, va_list vaArgs ) const
{
	switch( eLvl )
	{
	case kMsg_Error:	ErrorV( pchMsg, vaArgs );		break;
	case kMsg_Warning:	WarningV( pchMsg, vaArgs );		break;
	case kMsg_Msg:		MsgV( pchMsg, vaArgs );			break;
	case kMsg_Verbose:	VerboseV( pchMsg, vaArgs );		break;
	default:
		AssertMsg1( false, "Unexpected error level of %d provided to GCEmitGroup::Emit", eLvl );
		break;
	}
}



//---------------------------------------------------------------------
// Legacy Interface
//---------------------------------------------------------------------

void EGInternal_EmitInfo( const CGCEmitGroup& Group, int iLevel, int iLevelLog, const char *pchMsg, ... )
{
	va_list args;
	va_start( args, pchMsg );
	EmitBaseMessageV( Group, SPEW_MESSAGE, iLevel, iLevelLog, pchMsg, args );
	va_end( args );
}

void EmitInfoV( const CGCEmitGroup& Group, int iLevel, int iLevelLog, const char *pchMsg, va_list vaArgs )
{
	EmitBaseMessageV( Group, SPEW_MESSAGE, iLevel, iLevelLog, pchMsg, vaArgs );
}

void EmitWarning( const CGCEmitGroup& Group, int iLevel, const char *pchMsg, ... )
{
	va_list args;
	va_start( args, pchMsg );
	EmitBaseMessageV( Group, SPEW_WARNING, iLevel, iLevel, pchMsg, args );
	va_end( args );
}

void EmitError( const CGCEmitGroup& Group, const char *pchMsg, ... )
{
	va_list args;
	va_start( args, pchMsg );
	EmitBaseMessageV( Group, SPEW_ERROR, 1, 1, pchMsg, args );
	va_end( args );
}

// Emit an assert-like error, generating a minidump
void EmitAssertError( const CGCEmitGroup& Group, const char *pchMsg, ... )
{
	va_list args;
	va_start( args, pchMsg );
	EmitBaseMessageV( Group, SPEW_ASSERT, 1, 1, pchMsg, args );
	va_end( args );
}

//legacy group types
DECLARE_GC_EMIT_GROUP( SPEW_SYSTEM_MISC, system );
DECLARE_GC_EMIT_GROUP( SPEW_JOB, job );
DECLARE_GC_EMIT_GROUP( SPEW_CONSOLE, console );
DECLARE_GC_EMIT_GROUP( SPEW_GC, gc );
DECLARE_GC_EMIT_GROUP( SPEW_SQL, sql );
DECLARE_GC_EMIT_GROUP( SPEW_NETWORK, network );
DECLARE_GC_EMIT_GROUP( SPEW_SHAREDOBJ, sharedobj );
DECLARE_GC_EMIT_GROUP( SPEW_MICROTXN, microtxn );
DECLARE_GC_EMIT_GROUP( SPEW_PROMO, promo );
DECLARE_GC_EMIT_GROUP( SPEW_PKGITEM, pkgitem );
DECLARE_GC_EMIT_GROUP( SPEW_ECONOMY, econ );
DECLARE_GC_EMIT_GROUP( SPEW_THREADS, threads );

} // namespace GCSDK
