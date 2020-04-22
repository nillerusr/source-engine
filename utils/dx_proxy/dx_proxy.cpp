//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Proxy for D3DX routines
//
// $NoKeywords: $
//
//=============================================================================//
//

#include <windows.h>
#include <vector>

// Aux function prototype
const char * WINAPI GetDllVersion( void );


#ifdef _DEBUG
#define D3D_DEBUG_INFO 1
#endif


//
// DX9_V00_PC
//
// D3DX static library
// MSFT file version: 5.3.0000001.0904
//
#ifdef DX9_V00_PC

#ifdef DX_PROXY_INC_CONFIG
#	error "DX9_V00_PC: Multiple DX_PROXY configurations disallowed!"
#endif
#define DX_PROXY_INC_CONFIG
#pragma message ( "Compiling DX_PROXY for DX9_V00_PC" )

#if _MSC_VER >= 1900
#pragma comment ( lib, "../../lib/common/win32/2015/release/d3dx9" )
#else
#pragma comment ( lib, "../../dx9sdk/lib/d3dx9" )
#endif
#include "../../dx9sdk/include/d3dx9shader.h"

#endif // #ifdef DX9_V00_PC


//
// DX9_X360
//
// D3DX win32 static library
// MSFT X360 SDK
//
#ifdef DX9_V00_X360

#ifdef DX_PROXY_INC_CONFIG
#	error "DX9_V00_X360: Multiple DX_PROXY configurations disallowed!"
#endif
#define DX_PROXY_INC_CONFIG
#pragma message ( "Compiling DX_PROXY for DX9_V00_X360" )

// Avoid including XBOX math stuff
#define _NO_XBOXMATH
#define __D3DX9MATH_INL__

/*#ifdef _DEBUG
#pragma comment ( lib, "../../x360xdk/lib/win32/vs2005/d3d9d" )
#pragma comment ( lib, "../../x360xdk/lib/win32/vs2005/d3dx9d" )
#pragma comment ( lib, "../../x360xdk/lib/win32/vs2005/xgraphicsd" )
#else*/
#if _MSC_VER >= 1900
#pragma comment ( lib, "../../x360xdk/lib/win32/vs2005/d3d9" )
#pragma comment ( lib, "../../lib/common/win32/2015/release/d3dx9" )
#else
#pragma comment ( lib, "../../x360xdk/lib/win32/vs2005/d3d9" )
#pragma comment ( lib, "../../x360xdk/lib/win32/vs2005/d3dx9" )
#pragma comment ( lib, "../../x360xdk/lib/win32/vs2005/xgraphics" )
#endif

#include "../../x360xdk/include/win32/vs2005/d3dx9shader.h"

#endif // #ifdef DX9_V00_X360


//
// DX9_V30_PC
//
// 1. D3DX static import library
// 2. resource dynamic library d3dx9_33.dll
//
// MSFT file version: 9.16.843.0000
// Distribution: Dec 2006 DirectX SDK
//
// Implementation note: need to delayload d3dx9_33
// because the module should be extracted from resources first.
// Make sure "/DELAYLOAD:d3dx9_33.dll" is passed to linker.
//
#ifdef DX9_V30_PC

#ifdef DX_PROXY_INC_CONFIG
#	error "DX9_V30_PC: Multiple DX_PROXY configurations disallowed!"
#endif
#define DX_PROXY_INC_CONFIG
#pragma message ( "Compiling DX_PROXY for DX9_V30_PC" )

#pragma comment( lib, "delayimp" )

#pragma comment ( lib, "../../dx10sdk/lib/x86/d3dx9" )
#include "../../dx10sdk/include/d3dx9shader.h"

#endif // #ifdef DX9_V30_PC


//
// DX10_V00_PC
//
// 1. D3DX static import library
// 2. resource dynamic library d3dx10.dll
//
// MSFT file version: 9.16.843.0000
// Distribution: Dec 2006 DirectX SDK
//
// Implementation note: need to delayload d3dx10
// because the module should be extracted from resources first.
// Make sure "/DELAYLOAD:d3dx10.dll" is passed to linker.
//
#ifdef DX10_V00_PC

#ifdef DX_PROXY_INC_CONFIG
#	error "DX10_V00_PC: Multiple DX_PROXY configurations disallowed!"
#endif
#define DX_PROXY_INC_CONFIG
#pragma message ( "Compiling DX_PROXY for DX10_V00_PC" )

#pragma comment( lib, "delayimp" )

#pragma comment ( lib, "../../dx10sdk/lib/x86/d3dx10" )
#include "../../dx10sdk/include/d3dx10.h"

typedef D3D10_SHADER_MACRO D3DXMACRO;
typedef LPD3D10INCLUDE LPD3DXINCLUDE;
typedef ID3D10Include ID3DXInclude;
typedef D3D10_INCLUDE_TYPE D3DXINCLUDE_TYPE;
typedef ID3D10Blob* LPD3DXBUFFER;
typedef void* LPD3DXCONSTANTTABLE;

#endif // #ifdef DX10_V00_PC


//
// No DX configuration
#ifndef DX_PROXY_INC_CONFIG
# error "DX9_PC or DX9_X360 must be defined!"
#endif // #ifndef DX_PROXY_INC_CONFIG



//
// ExtractDependencies
//
// Retrieves all the additional required binaries from the resources and
// places them to a temporary location. Then the binaries are mapped into
// the address space of the calling process.
//
static BOOL ExtractDependencies( void )
{
	return TRUE;
/*
	BOOL bResult = TRUE;
	char chSyncName[0x30];
	char const *szDllVersion = GetDllVersion();
	sprintf( chSyncName, "%s_MTX", szDllVersion );

	HANDLE hMutex = ::CreateMutex( NULL, FALSE, chSyncName );
	if ( !hMutex )
		return FALSE;

	DWORD dwWaitResult = ::WaitForSingleObject( hMutex, INFINITE );
	if ( dwWaitResult != WAIT_OBJECT_0 )
		return FALSE;

	// Now we own the mutex
	char chExtractPath[0x100] = { 0 };
	if ( char const *pszTemp = getenv( "TEMP" ) )
		sprintf( chExtractPath, "%s\\", pszTemp );
	else if ( char const *pszTmp = getenv( "TMP" ) )
		sprintf( chExtractPath, "%s\\", pszTmp );
	else
		bResult = FALSE;

	if ( bResult )
	{
		sprintf( chExtractPath + strlen( chExtractPath ), "%s", szDllVersion );
		bResult = ::CreateDirectory( chExtractPath, NULL );
		
		if ( bResult )
		{
			sprintf( chExtractPath + strlen( chExtractPath ), "\\" );

			char const * const arrNames[] = {
#ifdef DX9_V33_PC
				"d3dx9_33.dll", MAKEINTRESOURCE( 1 ),
#else
#endif
				NULL
			};

			// Now loop over the names
			for ( int k = 0; k < sizeof( arrNames ) / ( 2 * sizeof( arrNames[0] ) ); ++ k )
			{
				char const * const &szName = arrNames[ 2 * k ];
				char const * const &szResource = 1[ &szName ];

				char chCreateFileName[0x200];
				sprintf( chCreateFileName, "%s%s", chExtractPath, szName );

				HANDLE hFile = CreateFile( chCreateFileName, FILE_ALL_ACCESS, FILE_SHARE_READ, NULL, CREATE_NEW,
					FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE, NULL );
				#error "This is how you can create temp needed resources"
			}
		}
	}

	::ReleaseMutex( hMutex );
	::CloseHandle( hMutex );

	return bResult;
*/
}


// DLL entry point: DllMain
BOOL WINAPI DllMain(
					HINSTANCE hinstDLL,
					DWORD fdwReason,
					LPVOID lpvReserved
					)
{
	/*UNUSED_ALWAYS*/( hinstDLL );
	/*UNUSED_ALWAYS*/( lpvReserved );

	switch ( fdwReason )
	{
	case DLL_PROCESS_ATTACH:
		// Process is attaching - make sure it can find the dependencies
		return ExtractDependencies();
	}

	return TRUE;
}




// Obtain DLL version
#pragma comment(linker, "/EXPORT:GetDllVersionLong=?GetDllVersionLong@@YGPBDXZ")
const char * WINAPI GetDllVersionLong( void )
{
#if defined( DX9_V00_PC ) && defined( _DEBUG )
	return "{DX_PROXY for DX9_V00_PC DEBUG}";
#endif

#if defined( DX9_V00_PC ) && defined( NDEBUG )
	return "{DX_PROXY for DX9_V00_PC RELEASE}";
#endif

#if defined( DX9_V00_X360 ) && defined( _DEBUG )
	return "{DX_PROXY for DX9_V00_X360 DEBUG}";
#endif

#if defined( DX9_V00_X360 ) && defined( NDEBUG )
	return "{DX_PROXY for DX9_V00_X360 RELEASE}";
#endif

#if defined( DX9_V30_PC ) && defined( _DEBUG )
	return "{DX_PROXY for DX9_V30_PC DEBUG}";
#endif

#if defined( DX9_V30_PC ) && defined( NDEBUG )
	return "{DX_PROXY for DX9_V30_PC RELEASE}";
#endif

#if defined( DX10_V00_PC ) && defined( _DEBUG )
	return "{DX_PROXY for DX10_V00_PC DEBUG}";
#endif

#if defined( DX10_V00_PC ) && defined( NDEBUG )
	return "{DX_PROXY for DX10_V00_PC RELEASE}";
#endif
}


#pragma comment(linker, "/EXPORT:GetDllVersion=?GetDllVersion@@YGPBDXZ")
const char * WINAPI GetDllVersion( void )
{
#if defined( DX9_V00_PC ) && defined( _DEBUG )
	return "DXPRX_DX9_V00_PC_d";
#endif

#if defined( DX9_V00_PC ) && defined( NDEBUG )
	return "DXPRX_DX9_V00_PC_r";
#endif

#if defined( DX9_V00_X360 ) && defined( _DEBUG )
	return "DXPRX_DX9_V00_X360_d";
#endif

#if defined( DX9_V00_X360 ) && defined( NDEBUG )
	return "DXPRX_DX9_V00_X360_r";
#endif

#if defined( DX9_V30_PC ) && defined( _DEBUG )
	return "DXPRX_DX9_V30_PC_d";
#endif

#if defined( DX9_V30_PC ) && defined( NDEBUG )
	return "DXPRX_DX9_V30_PC_r";
#endif

#if defined( DX10_V00_PC ) && defined( _DEBUG )
	return "DXPRX_DX10_V00_PC_d";
#endif

#if defined( DX10_V00_PC ) && defined( NDEBUG )
	return "DXPRX_DX10_V00_PC_r";
#endif
}



#include "filememcache.h"
#include "dxincludeimpl.h"

char s_dummyBuffer[ 512 ];


// Proxied routines
HRESULT WINAPI
Proxy_D3DXCompileShaderFromFile(
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
	if ( !pInclude )
		pInclude = &s_incDxImpl;

	// Open the top-level file via our include interface
	LPCVOID lpcvData;
	UINT numBytes;
	HRESULT hr = pInclude->Open( ( D3DXINCLUDE_TYPE ) 0, pSrcFile, NULL, &lpcvData, &numBytes
#if defined( DX9_V00_X360 )
		, s_dummyBuffer, sizeof( s_dummyBuffer )
#endif
		);
	if ( FAILED( hr ) )
		return hr;

	LPCSTR pShaderData = ( LPCSTR ) lpcvData;

#if defined( DX9_V00_PC ) || defined( DX9_V30_PC ) || defined( DX9_V00_X360 )
	#pragma comment(linker, "/EXPORT:Proxy_D3DXCompileShaderFromFile=?Proxy_D3DXCompileShaderFromFile@@YGJPBDPBU_D3DXMACRO@@PAUID3DXInclude@@00KPAPAUID3DXBuffer@@3PAPAUID3DXConstantTable@@@Z")
	hr = D3DXCompileShader( pShaderData, numBytes, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable );
#endif

#if defined( DX10_V00_PC )
	#pragma comment(linker, "/EXPORT:Proxy_D3DXCompileShaderFromFile=?Proxy_D3DXCompileShaderFromFile@@YGJPBDPBU_D3D10_SHADER_MACRO@@PAUID3D10Include@@00KPAPAUID3D10Blob@@3PAPAX@Z")
	hr = D3DX10CompileFromMemory( pShaderData, numBytes, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, 0, NULL, ppShader, ppErrorMsgs, NULL );
#endif

	// Close the file
	pInclude->Close( lpcvData );
	return hr;
}



