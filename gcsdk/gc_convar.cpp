//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "gcsdk/gclogger.h"

#include "memdbgon.h" // needs to be the last include in the file

using namespace GCSDK;

const char *GCConVar::GetName( void ) const
{
	if ( m_strGCName.Length() == 0 )
	{
		m_pchBaseName = ConVar::GetName();
		if( GCSDK::GGCBase() )
		{
			m_strGCName.Format( "%s_%u", m_pchBaseName, GCSDK::GGCBase()->GetAppID() );
		}
		else
		{
			m_strGCName.Format( "%s_gc", m_pchBaseName );
		}
	}

	return m_strGCName.String();
}


const char *GCConCommand::GetName( void ) const
{
	if ( m_strGCName.Length() == 0 )
	{
		m_pchBaseName = ConCommand::GetName();
		if( GCSDK::GGCBase() )
		{
			m_strGCName.Format( "%s_%u", m_pchBaseName, GCSDK::GGCBase()->GetAppID() );
		}
		else
		{
			m_strGCName.Format( "%s_gc", m_pchBaseName );
		}
	}

	return m_strGCName.String();
}

//-----------------------------------------------------------------------------
// Purpose: Checks a command to see if it got enough arguments
// Input  : nArgs - The minimum required args on the command
//			args - The arguments passed to the command
//			command - The command being executed
//-----------------------------------------------------------------------------
bool BCheckArgs( int nArgs, const CCommand &args, const ConCommandBase &command )
{
	if ( args.ArgC() <= nArgs )
	{
		EG_ERROR( SPEW_CONSOLE, "Incorrect number of arguments. %d arguments required, %d were given.\n", nArgs, args.ArgC() - 1 );
		EG_ERROR( SPEW_CONSOLE, "\t%s\n", command.GetHelpText() );
		return false;
	}

	return true;
}
