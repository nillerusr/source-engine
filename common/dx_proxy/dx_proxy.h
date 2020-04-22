//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Make dynamic loading of dx_proxy.dll and methods acquisition easier.
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef DX_PROXY_H
#define DX_PROXY_H

#ifdef _WIN32
#pragma once
#endif


/*

class DxProxyModule

Uses a lazy-load technique to load the dx_proxy.dll module and acquire the
function pointers.

The dx_proxy.dll module is automatically unloaded during desctruction.

*/
class DxProxyModule
{
public:
	/// Construction
	DxProxyModule( void );
	/// Destruction
	~DxProxyModule( void );

private: // Prevent copying via copy constructor or assignment
	DxProxyModule( const DxProxyModule & );
	DxProxyModule & operator = ( const DxProxyModule & );

public:
	/// Loads the module and acquires function pointers, returns if the module was
	/// loaded successfully.
	/// If the module was already loaded the call has no effect and returns TRUE.
	BOOL Load( void );
	/// Frees the loaded module.
	void Free( void );

private:
	enum Func {
		fnD3DXCompileShaderFromFile = 0,
		fnTotal
	};
	HMODULE m_hModule;				//!< The handle of the loaded dx_proxy.dll
	FARPROC m_arrFuncs[fnTotal];	//!< The array of loaded function pointers


	///
	/// Interface functions calling into DirectX proxy
	///
public:
	HRESULT D3DXCompileShaderFromFile(
		LPCSTR                          pSrcFile,
		CONST D3DXMACRO*                pDefines,
		LPD3DXINCLUDE                   pInclude,
		LPCSTR                          pFunctionName,
		LPCSTR                          pProfile,
		DWORD                           Flags,
		LPD3DXBUFFER*                   ppShader,
		LPD3DXBUFFER*                   ppErrorMsgs,
		LPD3DXCONSTANTTABLE*            ppConstantTable );
};







//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
//     IMPLEMENTATION
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


inline DxProxyModule::DxProxyModule( void )
{
	m_hModule = NULL;
	ZeroMemory( m_arrFuncs, sizeof( m_arrFuncs ) );
}

inline DxProxyModule::~DxProxyModule( void )
{
	Free();
}

inline BOOL DxProxyModule::Load( void )
{
	if ( (m_hModule == NULL) &&
		( m_hModule = ::LoadLibrary( "dx_proxy.dll" ) ) != NULL )
	{
		// Requested function names array
		LPCSTR const arrFuncNames[fnTotal] = {
			"Proxy_D3DXCompileShaderFromFile"
		};

		// Acquire the functions
		for ( int k = 0; k < fnTotal; ++ k )
		{
			m_arrFuncs[k] = ::GetProcAddress( m_hModule, arrFuncNames[k] );
		}
	}

	return !!m_hModule;
}

inline void DxProxyModule::Free( void )
{
	if ( m_hModule )
	{
		::FreeLibrary( m_hModule );
		m_hModule = NULL;
		ZeroMemory( m_arrFuncs, sizeof( m_arrFuncs ) );
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
//     INTERFACE FUNCTIONS IMPLEMENTATION
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////



inline HRESULT DxProxyModule::D3DXCompileShaderFromFile(
								  LPCSTR                          pSrcFile,
								  CONST D3DXMACRO*                pDefines,
								  LPD3DXINCLUDE                   pInclude,
								  LPCSTR                          pFunctionName,
								  LPCSTR                          pProfile,
								  DWORD                           Flags,
								  LPD3DXBUFFER*                   ppShader,
								  LPD3DXBUFFER*                   ppErrorMsgs,
								  LPD3DXCONSTANTTABLE*            ppConstantTable )
{
	if ( !Load() )
		return MAKE_HRESULT( SEVERITY_ERROR, FACILITY_ITF, 1 );
	if ( !m_arrFuncs[fnD3DXCompileShaderFromFile] )
		return MAKE_HRESULT( SEVERITY_ERROR, FACILITY_ITF, 2 );

	return
		( *
			( HRESULT (WINAPI *)
				( LPCSTR, CONST D3DXMACRO*, LPD3DXINCLUDE,
				  LPCSTR, LPCSTR, DWORD, LPD3DXBUFFER*,
				  LPD3DXBUFFER*, LPD3DXCONSTANTTABLE* )
			)
			m_arrFuncs[fnD3DXCompileShaderFromFile]
		)
		( pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable );
}


#endif // #ifndef DX_PROXY_H
