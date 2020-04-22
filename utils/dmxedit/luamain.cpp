//=============================================================================
//
//========= Copyright Valve Corporation, All rights reserved. ============//
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================


// Standard includes
#include <ctype.h>
#include <io.h>


// Valve includes
#include "appframework/AppFramework.h"
#include "datamodel/idatamodel.h"
#include "filesystem.h"
#include "icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "mathlib/mathlib.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier2/tier2dm.h"
#include "tier3/tier3.h"
#include "tier1/UtlStringMap.h"
#include "vstdlib/vstdlib.h"
#include "vstdlib/iprocessutils.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"


// Lua includes
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


// Local includes
#include "dmxedit.h"


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CDmxEditApp : public CDefaultAppSystemGroup< CSteamAppSystemGroup >
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown();

	void PrintHelp( bool bWiki = false );
};


DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CDmxEditApp );

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CDmxEditApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "vstdlib.dll",			PROCESS_UTILS_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "p4lib.dll",				P4_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	AddSystems( appSystems );

	IMaterialSystem *pMaterialSystem = reinterpret_cast< IMaterialSystem * >( FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION ) );
	if ( !pMaterialSystem )
	{
		Error( "// ERROR: Unable to connect to material system interface!\n" );
		return false;
	}

	pMaterialSystem->SetShaderAPI( "shaderapiempty.dll" );
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmxEditApp::PreInit( )
{
	CreateInterfaceFn factory = GetFactory();

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	ConnectTier3Libraries( &factory, 1 );

	if ( !ConnectDataModel( factory ) )
		return false;

	if ( InitDataModel( ) != INIT_OK )
		return false;

	if ( !g_pFullFileSystem || !g_pDataModel )
	{
		Error( "// ERROR: dmxedit is missing a required interface!\n" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEditApp::PostShutdown()
{
	ShutdownDataModel();
	DisconnectDataModel();

	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CDmxEditApp::Main()
{
	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	if ( CommandLine()->CheckParm( "-h" ) || CommandLine()->CheckParm( "-help" ) )
	{
		PrintHelp();
		return 0;
	}

	if ( CommandLine()->CheckParm( "-wiki" ) )
	{
		PrintHelp( true );
		return 0;
	}

	CUtlStringMap< CUtlString > setVars;
	CUtlString sGame;

	const int nParamCount = CommandLine()->ParmCount();
	for ( int i = 0; i < nParamCount - 1; ++i )
	{
		const char *pCmd = CommandLine()->GetParm( i );
		const char *pArg = CommandLine()->GetParm( i + 1 );

		if ( StringHasPrefix( pCmd, "-s" ) )
		{
			const char *const pEquals = strchr( pArg, '=' );
			if ( !pEquals )
			{
				Warning( "Warning: Invalid command line args, no ='s in -set argument: %s %s\n", pCmd, pArg );
			}

			char buf[ BUFSIZ ];
			Q_strncpy( buf, pArg, pEquals - pArg + 1 );

			const CUtlString sKey( buf );
			CUtlString sVal( pEquals + 1 );

			if ( !V_isdigit( *sVal.Get() ) && *sVal.Get() != '-' && *sVal.Get() != '"' )
			{
				CUtlString sqVal( "\"" );
				sqVal += sVal;
				sqVal += "\"";
				sVal = sqVal;
			}

			setVars[ sKey ] = sVal;

			if ( !Q_stricmp( sKey.Get(), "game" ) && sGame.IsEmpty() )
			{
				sGame = sKey;
			}

			++i;
		}
		else if ( StringHasPrefix( pCmd, "-g" ) )
		{
			if ( *pArg == '"' )
			{
				sGame = pArg;
			}
			else
			{
				sGame = ( "\"" );
				sGame += pArg;
				sGame += "\"";
			}
		}
		else if ( StringHasPrefix( pCmd, "-nop4" ) )
		{
			// Don't issue warning on -nop4
		}
		else if ( StringHasPrefix( pCmd, "-coe" ) || StringHasPrefix( pCmd, "-ContinueOnError" ) )
		{
			// Don't issue warning on -nop4
		}
		else if ( StringHasPrefix( pCmd, "-" ) )
		{
			Warning( "Warning: Unknown command line argument: %s\n", pCmd );
		}
	}

	// Do Perforce Stuff
	if ( CommandLine()->FindParm( "-nop4" ) )
		g_p4factory->SetDummyMode( true );

	g_p4factory->SetOpenFileChangeList( "dmxedit" );

	for ( int i = CommandLine()->ParmCount() - 1; i >= 1; --i )
	{
		const char *pParam = CommandLine()->GetParm( i );
		if ( _access( pParam, 04 ) == 0 )
		{
			CDmxEditLua dmxEditLua;
			for ( int i = 0; i < setVars.GetNumStrings(); ++i )
			{
				dmxEditLua.SetVar( setVars.String( i ), setVars[ i ] );
			}

			if ( !sGame.IsEmpty() )
			{
				dmxEditLua.SetGame( sGame );
			}

			return dmxEditLua.DoIt( pParam );
		}
	}

	Error( "Cannot find any file to execute from passed command line arguments\n\n" );
	PrintHelp();

	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CUtlString Wikize( lua_State *pLuaState, const char *pWikiString )
{
	CUtlString retVal( pWikiString );

	lua_pushstring( pLuaState, "string");
	lua_gettable( pLuaState, LUA_GLOBALSINDEX );
	lua_pushstring( pLuaState, "gsub");
	lua_gettable( pLuaState, -2);
	lua_pushstring( pLuaState, pWikiString );
	lua_pushstring( pLuaState, "([$#]%w+)" );
	lua_pushstring( pLuaState, "'''%0'''" );

	if ( lua_pcall( pLuaState, 3, 1, 0 ) == 0 )
	{
		if ( lua_isstring( pLuaState, -1 ) )
		{
			retVal = lua_tostring( pLuaState, -1 );
		}
		lua_pop( pLuaState, 1 );
	}

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
int Sort_LuaFunc_s( const T_t *pA, const T_t *pB )
{
	return Q_stricmp( (*pA)->m_pFuncName, (*pB)->m_pFuncName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmxEditApp::PrintHelp( bool bWiki /* = false */ )
{
	CUtlVector< LuaFunc_s * > luaFuncs;

	for ( LuaFunc_s *pFunc = LuaFunc_s::s_pFirstFunc; pFunc; pFunc = pFunc->m_pNextFunc )
	{
		luaFuncs.AddToTail( pFunc );
	}
	luaFuncs.Sort( Sort_LuaFunc_s );

	if ( bWiki && LuaFunc_s::s_pFirstFunc )
	{
		lua_State *pLuaState = lua_open();
		if ( pLuaState )
		{
			luaL_openlibs( pLuaState );

			CUtlString wikiString;

			for ( int i = 0; i < luaFuncs.Count(); ++i )
			{
				const LuaFunc_s *pFunc = luaFuncs[ i ];
				if ( pFunc != LuaFunc_s::s_pFirstFunc )
				{
					Msg( "\n" );
				}
				Msg( ";%s( %s );\n", pFunc->m_pFuncName, pFunc->m_pFuncPrototype );
				Msg( ":%s\n", Wikize( pLuaState, pFunc->m_pFuncDesc ).Get() );
			}

			return;
		}
	}

	Msg( "\n" );
	Msg( "NAME\n" );
	Msg( "    dmxedit - Edit dmx files\n" );
	Msg( "\n" );
	Msg( "SYNOPSIS\n" );
	Msg( "    dmxedit [ -h | -help ] [ -game <$game> ] [ -set $var=val ] [ script.lua ]\n" );
	Msg( "\n" );
	Msg( "    -h | -help :           Prints this information\n" );
	Msg( "    -g | -game <$game> :   Sets the VPROJECT environment variable to the specified game.\n" );
	Msg( "    -s | -set <$var=val> : Sets the lua variable var to the specified val before the script is run.\n" );
	Msg( "\n" );
	Msg( "DESCRIPTION\n" );
	Msg( "    Edits dmx files by executing a lua script of dmx editing functions\n" );
	Msg( "\n" );

	if ( !LuaFunc_s::s_pFirstFunc )
		return;

	Msg( "FUNCTIONS\n" );

	const char *pWhitespace = " \t";

	for ( int i = 0; i < luaFuncs.Count(); ++i )
	{
		const LuaFunc_s *pFunc = luaFuncs[ i ];

		Msg( "    %s( %s );\n", pFunc->m_pFuncName, pFunc->m_pFuncPrototype );
		Msg( "      * " );

		CUtlString tmpStr;

		const char *pWordBegin = pFunc->m_pFuncDesc + strspn( pFunc->m_pFuncDesc, pWhitespace );
		const char *pWhiteSpaceBegin = pWordBegin;
		const char *pWordEnd = pWordBegin + strcspn( pWordBegin, pWhitespace );

		bool bNewline = false;
		while ( *pWordBegin )
		{
			if ( pWordEnd - pWhiteSpaceBegin + tmpStr.Length() > 70 )
			{
				if ( bNewline )
				{
					Msg( "        " );
				}
				else
				{
					bNewline = true;
				}
				Msg( "%s\n", tmpStr.Get() );
				tmpStr.Set( "" );
			}

			if ( tmpStr.Length() )
			{
				tmpStr += CUtlString( pWhiteSpaceBegin, pWordEnd - pWhiteSpaceBegin + 1);
			}
			else
			{
				tmpStr += CUtlString( pWordBegin, pWordEnd - pWordBegin + 1 );
			}

			pWhiteSpaceBegin = pWordEnd;
			pWordBegin = pWhiteSpaceBegin + strspn( pWhiteSpaceBegin, pWhitespace );
			pWordEnd = pWordBegin + strcspn( pWordBegin, pWhitespace );
		}

		if ( tmpStr.Length() )
		{
			if ( bNewline )
			{
				Msg( "        " );
			}
			Msg( "%s\n", tmpStr.Get() );
		}
		Msg( "\n" );
	}

	Msg( "CREDITS\n" );
	Msg( "    Lua Copyright © 1994-2006 Lua.org, PUC-Rio.\n ");

	Msg( "\n" );
}