//========== Copyright © 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#define WIN32_LEAN_AND_MEAN 1
#pragma warning( disable:4201)
#pragma comment(lib, "winmm.lib" )
#include <windows.h>  
#include <mmsystem.h> // multimedia timer (may need winmm.lib)
#include <stdio.h>
#include <io.h>
#include <conio.h>

#include "platform.h"

#include "datamap.h"
#include "tier1/functors.h"
#include "tier1/utlvector.h"
#include "tier1/utlhash.h"
#include "tier1/fmtstr.h"
#include "vscript//ivscript.h"

#include "gmThread.h" // game monkey script
#include "gmArrayLib.h"
#include "gmCall.h"
#include "gmGCRoot.h"
#include "gmGCRootUtil.h"
#include "gmHelpers.h"
#include "gmMathLib.h"
#include "gmStringLib.h"
#include "gmVector3Lib.h"

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CGCDisabler
{
public:
	CGCDisabler( gmMachine *pMachine )
	{
		m_pMachine = pMachine;
		m_bWasEnabled = m_pMachine->IsGCEnabled();
		m_pMachine->EnableGC( false );
	}
	~CGCDisabler()
	{
		m_pMachine->EnableGC( m_bWasEnabled );
	}

	gmMachine *m_pMachine;
	bool m_bWasEnabled;
};

#define DISABLE_GC() CGCDisabler gcDisabler( this )

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CGameMonkeyVM : public IScriptVM,
					  public gmMachine
{
public:
	CGameMonkeyVM() 
	{
	}

	bool Init()
	{
		if ( !m_global )
		{
			// Must be a re-init
			gmMachine::Init();
		}
		s_printCallback = ScriptPrintCallback;
		SetDebugMode( true );
		AddCPPOwnedGMObject( GetGlobals() );

		gmBindArrayLib( this );
		gmBindMathLib( this );
		gmBindStringLib( this );
		gmBindVector3Lib( this );

		m_TypeMap.Init( 256 );
		return true;
	}

	void Shutdown()
	{
		// Dump run time errors to output
		if ( GetDebugMode() )
		{
			FlushErrorLog();
		}
		RemoveAllCPPOwnedGMObjects();
		ResetAndFreeMemory();
		m_TypeMap.Purge();
	}

	ScriptLanguage_t GetLanguage()
	{
		return SL_GAMEMONKEY;
	}

	virtual const char *GetLanguageName()
	{
		return "GameMonkey";
	}

	virtual void AddSearchPath( const char *pszSearchPath )
	{

	}

	bool ConnectDebugger() { return true; }
	void DisconnectDebugger() {}

	bool Frame( float simTime )
	{
		return false;
	}

	HSCRIPT CompileScript( const char *pszScript, const char *pszId )
	{
		DISABLE_GC();
		gmFunctionObject *pFunction = CompileStringToFunction( pszScript );
		if ( !pFunction )
		{
			FlushErrorLog();
			return NULL;
		}

		ObjectHandle_t *pObjectHandle = new ObjectHandle_t;
		pObjectHandle->pObject = pFunction;
		AddCPPOwnedGMObject( pFunction );
		if ( pszId )
		{
			pObjectHandle->pString = AllocStringObject( pszId );
			AddCPPOwnedGMObject( pObjectHandle->pString );
			gmTableObject *pGlobals = GetGlobals();
			pGlobals->Set( this, gmVariable(pObjectHandle->pString), gmVariable(pFunction) );
		}
		else
		{
			pObjectHandle->pString = NULL;
		}
		return (HSCRIPT)pObjectHandle;
	}

	void ReleaseScript( HSCRIPT hScript )
	{
		ReleaseHandle( hScript );
	}

	ScriptStatus_t Run( HSCRIPT hScript, HSCRIPT hScope = NULL, bool bWait = true )
	{
		return CGameMonkeyVM::ExecuteFunction( hScript, NULL, 0, NULL, hScope, bWait );
	}

	ScriptStatus_t Run( const char *pszScript, bool bWait = true )
	{
		Assert( bWait );
		int errors = ExecuteString( pszScript, NULL, bWait );
		// Dump compile time errors to output
		if(errors)
		{
			bool first = true;
			const char * message;

			while((message = GetLog().GetEntry(first))) 
			{
				Msg( "%s\n", message );
			}
			GetLog().Reset();
			return SCRIPT_ERROR;
		}
		return SCRIPT_DONE;
	}

	ScriptStatus_t Run( HSCRIPT hScript, bool bWait )
	{
		Assert( bWait );
		return CGameMonkeyVM::Run( hScript, (HSCRIPT)NULL, bWait );
	}

	HSCRIPT CreateScope( const char *pszScope, HSCRIPT hParent = NULL )
	{
		Assert( pszScope );
		Assert( !hParent );

		DISABLE_GC();

		gmTableObject *pGlobals = GetGlobals();
		gmTableObject *pNewTable = AllocTableObject();
		ObjectHandle_t *pObjectHandle = new ObjectHandle_t;

		AddCPPOwnedGMObject( pNewTable );
		pObjectHandle->pObject = pNewTable;
		pObjectHandle->pString = AllocStringObject( pszScope );
		AddCPPOwnedGMObject( pObjectHandle->pString );

		pGlobals->Set( this, gmVariable(pObjectHandle->pString), gmVariable(pNewTable) );

		return (HSCRIPT)pObjectHandle;
	}

	void ReleaseScope( HSCRIPT hScript )
	{
		ReleaseHandle( hScript );
	}

	HSCRIPT LookupFunction( const char *pszFunction, HSCRIPT hScope = NULL )
	{
		gmTableObject *pScope;

		if ( !hScope )
		{
			pScope = GetGlobals();
		}
		else
		{
			ObjectHandle_t *pScopeHandle = (ObjectHandle_t *)hScope;
			pScope = assert_cast<gmTableObject *>(pScopeHandle->pObject);
		}
		gmVariable varFunction = pScope->Get( this, pszFunction );
		gmFunctionObject *pFunction = varFunction.GetFunctionObjectSafe();
		if ( pFunction )
		{
			ObjectHandle_t *pFunctionHandle = new ObjectHandle_t;
			pFunctionHandle->pObject = pFunction;
			pFunctionHandle->pString = NULL;
			AddCPPOwnedGMObject( pFunction );
			return (HSCRIPT)pFunctionHandle;
		}
		return NULL;
	}

	void ReleaseFunction( HSCRIPT hScript )
	{	
		ReleaseHandle( hScript );
	}

	ScriptStatus_t ExecuteFunction( HSCRIPT hFunction, ScriptVariant_t *pArgs, int nArgs, ScriptVariant_t *pReturn, HSCRIPT hScope, bool bWait )
	{
		Assert( hFunction );
		Assert( bWait );
		ScriptStatus_t result = SCRIPT_ERROR;
		if ( pReturn )
		{
			pReturn->m_type = FIELD_VOID;
		}

		if ( hFunction )
		{
			ObjectHandle_t *pObjectHandle= (ObjectHandle_t *)hFunction;
			gmTableObject *pGlobals = NULL;
			gmTableObject *pScope;
			if ( hScope )
			{
				ObjectHandle_t *pScopeHandle= (ObjectHandle_t *)hScope;
				pScope = assert_cast<gmTableObject *>(pScopeHandle->pObject);
				pGlobals = GetGlobals();
				SetGlobals( pScope );
			}

			gmCall functionCaller;
			gmFunctionObject *pFunction = assert_cast<gmFunctionObject *>(pObjectHandle->pObject);
			if ( functionCaller.BeginFunction( this, pFunction, gmVariable::s_null, !bWait ) )
			{
				for ( int i = 0; i < nArgs; i++ )
				{
					switch ( pArgs[i].m_type )
					{
					case FIELD_FLOAT:		functionCaller.AddParamFloat( pArgs[i] ); break;
					case FIELD_CSTRING:		functionCaller.AddParamString(  pArgs[i], strlen( pArgs[i].m_pszString ) ); break;
					case FIELD_VECTOR:		Assert( 0 ); functionCaller.AddParamNull(); break;
					case FIELD_INTEGER:		functionCaller.AddParamInt( pArgs[i] ); break;
					case FIELD_BOOLEAN:		functionCaller.AddParamInt( pArgs[i].m_bool ); break;
					case FIELD_CHARACTER:	{ char sz[2]; sz[0] = pArgs[i].m_char; sz[1] = 0; functionCaller.AddParamString(  sz, 1 ); break; }
					}
				}

				functionCaller.End();

				if ( pReturn && ( bWait || !functionCaller.GetThread() ) && functionCaller.DidReturnVariable() )
				{
					const gmVariable &ret = functionCaller.GetReturnedVariable();
					switch ( ret.m_type )
					{
					case GM_NULL:		break;
					case GM_INT:		*pReturn = ret.m_value.m_int; break;
					case GM_FLOAT:		*pReturn = ret.m_value.m_float; break;
					case GM_STRING:		
						{ 
							gmStringObject *pString = (gmStringObject *)ret.m_value.m_ref;
							int size = pString->GetLength() + 1; 
							pReturn->m_type = FIELD_CSTRING;
							pReturn->m_pszString = new char[size]; 
							memcpy( (void *)pReturn->m_pszString, pString->GetString(), size ); 
						}
						break;
					default:
						DevMsg( "Script function returned unsupported type\n" );
						pReturn->m_type = FIELD_VOID;
					}
				}
			}

			if ( !FlushErrorLog() )
			{
				result = SCRIPT_DONE;
			}

			if ( hScope )
			{
				SetGlobals( pGlobals );
			}
		}
		return result;
	}

	gmMachine *GetMachine()
	{
		return this;
	}

	void RegisterFunction( ScriptFunctionBinding_t *pScriptFunction )
	{
		RegisterLibraryFunction( pScriptFunction->m_desc.m_pszScriptName, &TranslateCall, NULL, pScriptFunction );
	}

	bool RegisterClass( ScriptClassDesc_t *pClassDesc )
	{
		COMPILE_TIME_ASSERT( sizeof(pClassDesc) == sizeof(int) );
		if ( m_TypeMap.Find( (int)pClassDesc ) != m_TypeMap.InvalidHandle() )
		{
			return true;
		}

		gmType type = CreateUserType( pClassDesc->m_pszScriptName );
		m_TypeMap.Insert( (int)pClassDesc, type );

		ScriptClassDesc_t *pCurDesc = pClassDesc;
		CUtlVectorFixed<gmFunctionEntry, 512> functionEntries;
		while ( pCurDesc )
		{
			int nFunctions = pClassDesc->m_FunctionBindings.Count();
			functionEntries.SetSize( nFunctions );
			int i;
			for ( i = 0; i < nFunctions; i++)
			{
				functionEntries[i].m_function = &TranslateCall;
				functionEntries[i].m_name = pClassDesc->m_FunctionBindings[i].m_desc.m_pszFunction;
				functionEntries[i].m_userData = &pClassDesc->m_FunctionBindings[i];
			}
			RegisterTypeLibrary( type, functionEntries.Base(), nFunctions );
			pCurDesc = pCurDesc->m_pBaseDesc;
		}

		if ( pClassDesc->m_pfnConstruct )
		{
			RegisterLibraryFunction( pClassDesc->m_pszScriptName, &Construct, NULL, pClassDesc );
			RegisterUserCallbacks( type, NULL, &Destruct );
		}

		return true;
	}

	bool RegisterInstance( ScriptClassDesc_t *pDesc, void *pInstance, const char *pszInstance, HSCRIPT hScope = NULL )
	{
		if ( !RegisterClass( pDesc ) )
		{
			return false;
		}

		DISABLE_GC();

 		bool bResult = true;

		InstanceContext_t *pInstanceContext = new InstanceContext_t;
		pInstanceContext->pInstance = pInstance;
		pInstanceContext->pClassDesc = NULL; // i.e., no destruct

		// @TODO: This extra hash lookup could be eliminated (as it is also done in RegisterClass above) [2/13/2008 tom]
		gmUserObject *pObject = AllocUserObject( pInstanceContext, m_TypeMap[m_TypeMap.Find((int)pDesc)] );
		AddCPPOwnedGMObject( pObject );

		gmTableObject *pScope;

		if ( hScope )
		{
			ObjectHandle_t *pScopeHandle= (ObjectHandle_t *)hScope;
			pScope = (gmTableObject *)pScopeHandle->pObject;
		}
		else
		{
			pScope = GetGlobals();
		}

		if ( pScope )
		{
			pScope->Set( this, pszInstance, gmVariable( pObject ) );
		}
		else
		{
			DevMsg( "Undefined script scope!\n" );
			bResult = false;
		}

		return bResult;
	}

	void RemoveInstance( ScriptClassDesc_t *pDesc, void *pInstance, const char *pszInstance, HSCRIPT hScope = NULL )
	{
		DISABLE_GC();
		gmTableObject *pScope;

		if ( hScope )
		{
			ObjectHandle_t *pScopeHandle= (ObjectHandle_t *)hScope;
			pScope = (gmTableObject *)pScopeHandle->pObject;
		}
		else
		{
			pScope = GetGlobals();
		}

		if ( pScope )
		{
			gmVariable scriptVar = pScope->Get( this, pszInstance );
			gmUserObject *pObject = ( !scriptVar.IsNull() ) ? scriptVar.GetUserObjectSafe( m_TypeMap[m_TypeMap.Find((int)pDesc)] ) : NULL;
			if ( pObject )
			{
				delete pObject->m_user;
				pObject->m_user = NULL;
				pScope->Set( this, pszInstance, gmVariable::s_null );
				RemoveCPPOwnedGMObject( pObject );
			}
			else
			{
				DevMsg( "Unknown instance\n" );
			}
		}
		else
		{
			DevMsg( "Undefined script scope!\n" );
		}
	}


private:
	struct InstanceContext_t
	{
		void *pInstance;
		ScriptClassDesc_t *pClassDesc;
	};

	struct ObjectHandle_t
	{
		gmStringObject *pString;
		gmObject *pObject;
	};

	void ReleaseHandle( HSCRIPT hScript )
	{
		if ( hScript )
		{
			ObjectHandle_t *pObjectHandle = (ObjectHandle_t *)hScript;
			RemoveCPPOwnedGMObject( pObjectHandle->pObject );

			if ( pObjectHandle->pString )
			{
				gmTableObject *pGlobals = GetGlobals();
				pGlobals->Set( this, gmVariable(pObjectHandle->pString), gmVariable::s_null );
				RemoveCPPOwnedGMObject( pObjectHandle->pString );
			}
			delete pObjectHandle;
		}
	}

	static void ScriptPrintCallback(gmMachine *pMachine, const char *pString)
	{
		Msg( "%s\n", pString );
	}

	static int GM_CDECL TranslateCall( gmThread *a_thread )
	{
		const gmFunctionObject *fn = a_thread->GetFunctionObject();
		ScriptFunctionBinding_t *pVMScriptFunction = (ScriptFunctionBinding_t *)fn->m_cUserData;

		int nActualParams = a_thread->GetNumParams();
		int nFormalParams = pVMScriptFunction->m_desc.m_Parameters.Count();

		GM_CHECK_NUM_PARAMS( nFormalParams );

		CUtlVectorFixed<ScriptVariant_t, 14> params;
		ScriptVariant_t returnValue;

		params.SetSize( nFormalParams );

		int i = 0;

		if ( nActualParams )
		{
			int iLimit = min( nActualParams, nFormalParams );
			ScriptDataType_t *pCurParamType = pVMScriptFunction->m_desc.m_Parameters.Base();
			for ( i = 0; i < iLimit; i++, pCurParamType++ )
			{
				switch ( *pCurParamType )
				{
				case FIELD_FLOAT:		params[i] = a_thread->ParamFloatOrInt( i ); break;
				case FIELD_CSTRING:		params[i] = a_thread->ParamString( i ); break;
				case FIELD_VECTOR:		Assert( 0 ); params[i] = (Vector *)NULL; break;
				case FIELD_INTEGER:		params[i] = (int)a_thread->ParamFloatOrInt( i ); break;
				case FIELD_BOOLEAN:
					{
						int type = a_thread->ParamType(i);
						if ( type == GM_INT )
							params[i] = (bool)(a_thread->Param(i).m_value.m_int != 0 );
						else if ( type  == GM_FLOAT )
							params[i] = (bool)(a_thread->Param(i).m_value.m_float != 0.0 );
						else
							params[i] = true;
						break;
					}
				case FIELD_CHARACTER:	params[i] = a_thread->ParamString( i )[0]; break;
				}
			}
		}

		for ( ; i < nFormalParams; i++ )
		{
			COMPILE_TIME_ASSERT( sizeof(Vector *) >= sizeof(int) );
			params[i] = (Vector *)NULL;
		}

		InstanceContext_t *pContext;
		void *pObject;

		if ( pVMScriptFunction->m_flags & SF_MEMBER_FUNC )
		{
			pContext = (InstanceContext_t *)a_thread->ThisUser();

			if ( !pContext )
			{
				return GM_EXCEPTION;
			}

			pObject = pContext->pInstance;

			if ( !pObject )
			{
				return GM_EXCEPTION;
			}
		}
		else
		{
			pObject = NULL;
		}

		(*pVMScriptFunction->m_pfnBinding)( pVMScriptFunction->m_pFunction, pObject, params.Base(), params.Count(), ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID ) ? &returnValue : NULL );

		if ( pVMScriptFunction->m_desc.m_ReturnType != FIELD_VOID )
		{
			switch ( pVMScriptFunction->m_desc.m_ReturnType )
			{
			case FIELD_FLOAT:		a_thread->PushFloat( returnValue ); break;
			case FIELD_CSTRING:		Assert( 0 ); a_thread->PushNull(); break;
			case FIELD_VECTOR:		Assert( 0 ); a_thread->PushNull(); break;
			case FIELD_INTEGER:		a_thread->PushInt( returnValue ); break;
			case FIELD_BOOLEAN:		a_thread->PushInt( (bool)returnValue ); break;
			case FIELD_CHARACTER:	Assert( 0 ); a_thread->PushNull(); break;
			}
		}

		return GM_OK;
	}

	static int GM_CDECL Construct( gmThread *a_thread )
	{
		CGameMonkeyVM *pThis = assert_cast<CGameMonkeyVM *>(a_thread->GetMachine());
		const gmFunctionObject *fn = a_thread->GetFunctionObject();
		ScriptClassDesc_t *pClassDesc = (ScriptClassDesc_t *)fn->m_cUserData;

		GM_CHECK_NUM_PARAMS( 0 );

		InstanceContext_t *pInstanceContext = new InstanceContext_t;
		pInstanceContext->pInstance = pClassDesc->m_pfnConstruct();
		pInstanceContext->pClassDesc = pClassDesc;

		// @TODO: put the type in the userdata? [2/12/2008 tom]
		a_thread->PushNewUser( pInstanceContext, pThis->m_TypeMap[pThis->m_TypeMap.Find((int)pClassDesc)] );

		return GM_OK;
	}

	static void Destruct( gmMachine *pMachine, gmUserObject* pObject )
	{
		InstanceContext_t *pInstanceContext = ( InstanceContext_t *)pObject->m_user;
		if ( pInstanceContext && pInstanceContext->pClassDesc )
		{
			pInstanceContext->pClassDesc->m_pfnDestruct( pInstanceContext->pInstance );
			delete pInstanceContext;
		}
	}

	bool FlushErrorLog()
	{
		bool first = true;
		bool errors = false;
		const char * message;

		while((message = GetLog().GetEntry(first))) 
		{
			Msg( "%s\n", message );
			errors = true;
		}
		GetLog().Reset();

		return errors;
	}

	CUtlHashFast<gmType, CUtlHashFastGenericHash> m_TypeMap;
};

IScriptVM *ScriptCreateGameMonkeyVM()
{
	return new CGameMonkeyVM;
}

void ScriptDestroyGameMonkeyVM( IScriptVM *pVM )
{
	CGameMonkeyVM *pGameMonkeyVM = assert_cast<CGameMonkeyVM *>(pVM);
	delete pGameMonkeyVM;
}


#ifdef VGM_TEST
CGameMonkeyVM g_GMScriptVM;
IScriptVM *pScriptVM = &g_GMScriptVM;

struct Test
{
	void Foo() { Msg( "Foo!\n");}
};

BEGIN_SCRIPTDESC_ROOT( Test )
	DEFINE_SCRIPTFUNC( Foo )
END_SCRIPTDESC();

Test test;

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

int main( int argc, const char **argv)
{
	if ( argc < 2 )
	{
		printf( "No script specified" );
		return 1;
	}

	int key;
	do 
	{
		pScriptVM->Init();

		const char *pszScript = argv[1]; 
		FILE *hFile = fopen( pszScript, "rb" );
		if ( !hFile )
		{
			printf( "\"%s\" not found.\n", pszScript );
			return 1;
		}

		int nFileLen = _filelength( _fileno( hFile ) );
		char *pBuf = new char[nFileLen + 1];
		fread( pBuf, 1, nFileLen, hFile );
		pBuf[nFileLen] = 0;
		fclose( hFile );

		printf( "Executing script \"%s\"\n----------------------------------------\n", pszScript );

		HSCRIPT	hScript = pScriptVM->CompileScript( pBuf, "test" );
		if ( hScript )
		{
			HSCRIPT hScope = pScriptVM->CreateScope( "testScope" );
			pScriptVM->Run( hScript, hScope );
			HSCRIPT hFunction = pScriptVM->LookupFunction( "DoIt" );
 			Assert( !hFunction );
 			hFunction = pScriptVM->LookupFunction( "DoIt", hScope );
			Assert( hFunction );
			ScriptVariant_t ret;
			pScriptVM->RegisterInstance( &test, "test" );
			pScriptVM->Call( hFunction, hScope, true, &ret, "Har", 6.0, 99 );

			ret.Free();

			pScriptVM->ReleaseFunction( hFunction );
			pScriptVM->ReleaseScript( hScript );
			pScriptVM->ReleaseScope( hScope );
		}

		printf("Script complete.  Press q to exit, enter to run again.\n");
		key = _getch(); // Keypress before exit
		pScriptVM->Shutdown();
	} while ( key != 'q' );

	return 0;
}

#endif