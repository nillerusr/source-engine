//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the UI 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "inputmanager.h"
#include "legion.h"
#include "uimanager.h"
#include "inputsystem/iinputsystem.h"
#include "tier2/tier2.h"
#include "tier1/convar.h"


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CInputManager s_InputManager;
extern CInputManager *g_pInputManager = &s_InputManager;

	
//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CInputManager::Init()
{
	// FIXME: Read keybindings from a file
	m_KeyBindings.SetBinding( "w", "+forward" );
	m_KeyBindings.SetBinding( "s", "+back" );
	m_KeyBindings.SetBinding( "`", "toggleconsole" );
	m_ButtonUpToEngine.ClearAll();
	return true;
}


//-----------------------------------------------------------------------------
// Add a command into the command queue
//-----------------------------------------------------------------------------
void CInputManager::AddCommand( const char *pCommand )
{
	m_CommandBuffer.AddText( pCommand );
}


//-----------------------------------------------------------------------------
// FIXME! This is necessary only because of an artifact of how ConCommands used to work
//-----------------------------------------------------------------------------
static ConCommand *FindNamedCommand( char const *name )
{
	// look through the command list for all matches
	ConCommandBase const *cmd = (ConCommandBase const *)vgui::icvar()->GetCommands();
	while (cmd)
	{
		if (!Q_strcmp( name, cmd->GetName() ) )
		{
			if ( cmd->IsCommand() )
			{
				return ( ConCommand * )cmd;
			}
		}
		cmd = cmd->GetNext();
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputManager::PrintConCommandBaseDescription( const ConCommandBase *pVar )
{
	bool bMin, bMax;
	float fMin, fMax;
	const char *pStr;

	Assert( pVar );

	Color clr;
	clr.SetColor( 255, 100, 100, 255 );

	if ( !pVar->IsCommand() )
	{
		ConVar *var = ( ConVar * )pVar;

		bMin = var->GetMin( fMin );
		bMax = var->GetMax( fMax );

		char const *value = NULL;
		char tempVal[ 32 ];

		if ( var->IsBitSet( FCVAR_NEVER_AS_STRING ) )
		{
			value = tempVal;

			if ( fabs( (float)var->GetInt() - var->GetFloat() ) < 0.000001 )
			{
				Q_snprintf( tempVal, sizeof( tempVal ), "%d", var->GetInt() );
			}
			else
			{
				Q_snprintf( tempVal, sizeof( tempVal ), "%f", var->GetFloat() );
			}
		}
		else
		{
			value = var->GetString();
		}

		if ( value )
		{
			Msg( "\"%s\" = \"%s\"", var->GetName(), value );

			if ( Q_stricmp( value, var->GetDefault() ) )
			{
				Msg( " ( def. \"%s\" )", var->GetDefault() );
			}
		}

		if ( bMin )
		{
			Msg( " min. %f", fMin );
		}
		if ( bMax )
		{
			Msg( " max. %f", fMax );
		}

		Msg( "\n" );
	}
	else
	{
		ConCommand *var = ( ConCommand * )pVar;

		Msg( "\"%s\"\n", var->GetName() );
	}

//	PrintConCommandBaseFlags( pVar );

	pStr = pVar->GetHelpText();
	if ( pStr && pStr[0] )
	{
		Msg( " - %s\n", pStr );
	}
}


//-----------------------------------------------------------------------------
// Per-frame update
//-----------------------------------------------------------------------------
void CInputManager::ProcessCommands( )
{
	m_CommandBuffer.BeginProcessingCommands( 1 );
	while ( m_CommandBuffer.DequeueNextCommand() )
	{
		const CCommand& args = m_CommandBuffer.GetCommand();
		const ConCommandBase *pCommand = FindNamedCommand( args[ 0 ] );
		if ( pCommand && pCommand->IsCommand() )
		{
			// FIXME: Um... yuck!?!
			ConCommand *pConCommand = const_cast<ConCommand*>( static_cast<const ConCommand*>( pCommand ) );
			pConCommand->Dispatch( args );
			continue;
		}

		ConVar *pConVar = g_pCVar->FindVar( args[0] );
		if ( !pConVar )
			continue;

		// perform a variable print or set
		if ( args.ArgC() == 1 )
		{
			PrintConCommandBaseDescription( pConVar );
			continue;
		}

		// Note that we don't want the tokenized list, send down the entire string
		// except for surrounding quotes
		char remaining[1024];
		const char *pArgS = args.ArgS();
		int nLen = Q_strlen( pArgS );
		bool bIsQuoted = pArgS[0] == '\"';
		if ( !bIsQuoted )
		{
			Q_strncpy( remaining, args.ArgS(), sizeof(remaining) );
		}
		else
		{
			--nLen;
			Q_strncpy( remaining, &pArgS[1], sizeof(remaining) );
		}

		// Now strip off any trailing spaces
		char *p = remaining + nLen - 1;
		while ( p >= remaining )
		{
			if ( *p > ' ' )
				break;

			*p-- = 0;
		}

		// Strip off ending quote
		if ( bIsQuoted && p >= remaining )
		{
			if ( *p == '\"' )
			{
				*p = 0;
			}
		}

		if ( pConVar->IsBitSet( FCVAR_NEVER_AS_STRING ) )
		{
			pConVar->SetValue( (float)atof( remaining ) );
		}
		else
		{
			pConVar->SetValue( remaining );
		}

	}
	m_CommandBuffer.EndProcessingCommands();
}


//-----------------------------------------------------------------------------
// Per-frame update
//-----------------------------------------------------------------------------
void CInputManager::Update( )
{
	char cmd[1024];

	g_pInputSystem->PollInputState();
	int nEventCount = g_pInputSystem->GetEventCount();
	const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
	for ( int i = 0; i < nEventCount; ++i )
	{
		if ( pEvents[i].m_nType == IE_Quit )
		{
			IGameManager::Stop();
			break;
		}

		bool bBypassVGui = false;
		switch( pEvents[i].m_nType )
		{
		case IE_AppActivated:
			if ( pEvents[i].m_nData == 0 )
			{
				m_ButtonUpToEngine.ClearAll();
			}
			break;

		case IE_ButtonReleased:
			{
				// This logic is necessary to deal with switching back + forth
				// between vgui + the engine. If we downclick in the engine,
				// the engine must get the upclick.
				ButtonCode_t code = (ButtonCode_t)pEvents[i].m_nData;
				if ( m_ButtonUpToEngine[ code ] )
				{
					m_ButtonUpToEngine.Clear( code );
					bBypassVGui = true;
				}
			}
			break;

		default:
			break;
		}


		if ( !bBypassVGui )
		{
			if ( g_pUIManager->ProcessInputEvent( pEvents[i] ) )
				continue;
		}

		// FIXME: Add game keybinding system here
		bool bButtonDown = ( pEvents[i].m_nType == IE_ButtonPressed );
		bool bButtonUp = ( pEvents[i].m_nType == IE_ButtonReleased );
		if ( bButtonDown || bButtonUp )
		{
			ButtonCode_t code = (ButtonCode_t)pEvents[i].m_nData;
			if ( bButtonDown )
			{
				m_ButtonUpToEngine.Set( code );
			}
			const char *pBinding = m_KeyBindings.GetBindingForButton( code );
			if ( !pBinding )
				continue;

			if ( pBinding[0] != '+' )
			{
				if ( bButtonDown )
				{
					m_CommandBuffer.AddText( pBinding );
				}
				continue;
			}

			Q_snprintf( cmd, sizeof(cmd), "%c%s %i\n", bButtonUp ? '-' : '+', &pBinding[1], code );
			m_CommandBuffer.AddText( cmd );
			continue;
		}
	}

	ProcessCommands();
}

