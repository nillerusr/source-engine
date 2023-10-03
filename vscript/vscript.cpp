//========== Copyright � 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "interface.h"
#include "vscript/ivscript.h"
#include "languages/gm/vgm/vgm.h"
#include "languages/squirrel/vsquirrel/vsquirrel.h"
#include "languages/lua/vlua/vlua.h" 
#include "languages/python/vpython/vpython.h"
#include "vstdlib/random.h"
#include "tier1/tier1.h"


// lwss - HACCCKKKKK for --wrap on fopen() not working with my gcc
FILE *__wrap_fopen(const char *pathname, const char *mode)
{
    return fopen( pathname, mode );
}
// lwss end
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CScriptManager : public CTier1AppSystem<IScriptManager>
{
public:
	CScriptManager()
	{
	}

	IScriptVM *CreateVM( ScriptLanguage_t language )
	{
		IScriptVM *pVM = NULL;

		if ( language == SL_SQUIRREL )
		{
			pVM = ScriptCreateSquirrelVM();
		}
#if !defined( _GAMECONSOLE ) && !defined( LINUX )
		else if ( language == SL_LUA )
		{
			pVM = ScriptCreateLuaVM();
		}
#endif
#if VPYTHON_ENABLED
		else if ( language == SL_PYTHON )
		{
			pVM = ScriptCreatePythonVM();
		}
#endif
#if VGM_ENABLED
		else if ( language == SL_GAMEMONKEY )
		{
			pVM = ScriptCreateGameMonkeyVM();
		}
#endif

		AssertMsg( pVM, "Unknown script language\n" );
		if ( pVM )
		{
			pVM->Init();
			ScriptRegisterFunction( pVM, RandomFloat, "Generate a random floating point number within a range, inclusive" );
			ScriptRegisterFunction( pVM, RandomInt, "Generate a random integer within a range, inclusive" );
		}
		return pVM;
	}

	void DestroyVM( IScriptVM *p )
	{
		if ( p )
		{
			p->Shutdown();
			if ( p->GetLanguage() == SL_SQUIRREL )
			{
				ScriptDestroySquirrelVM( p );
			}
#if !defined( _GAMECONSOLE ) && !defined( LINUX )
			else if ( p->GetLanguage() == SL_LUA )
			{
				ScriptDestroyLuaVM( p );
			}
#endif
#if VPYTHON_ENABLED
			else if ( p->GetLanguage() == SL_PYTHON )
			{
				ScriptDestroyPythonVM( p );
			}
#endif
#if VGM_ENABLED
			else if ( p->GetLanguage() == SL_GAMEMONKEY )
			{
				ScriptDestroyGameMonkeyVM( p );
			}
#endif
			else
				AssertMsg( 0, "Unknown script language\n" );
		}
	}
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CScriptManager g_ScriptManager;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CScriptManager, IScriptManager, VSCRIPT_INTERFACE_VERSION, g_ScriptManager );


