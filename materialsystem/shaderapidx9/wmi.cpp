//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "resource.h"

// Avoid conflicts with MSVC headers and memdbgon.h
#undef PROTECTED_THINGS_ENABLE
#include "basetypes.h"

#define _WIN32_DCOM
#include <comdef.h>
#pragma warning( disable : 4127 ) // VS 2010 warning?
#pragma warning( disable : 4805 ) // VS 2013 warning: warning C4805: '==' : unsafe mix of type 'INT' and type 'bool' in operation
#include <atlcomtime.h>
#pragma warning( default : 4805 )
#pragma warning( default : 4127 )
#include <wbemidl.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


# pragma comment(lib, "wbemuuid.lib")

uint64 GetVidMemBytes( void )
{
	static int bBeenHere = false;
	static uint64 nBytes = 0;

	if( bBeenHere )
	{
		return nBytes;
	}

	bBeenHere = true;

	// Initialize COM
    HRESULT hr =  CoInitialize( NULL ); 
    if ( FAILED( hr ) )
    {
		OutputDebugString ( "GetWMIDeviceStats - Unable to initialize COM library.\n");
        return 0;
    }

    // Obtain the initial locator to WMI
    IWbemLocator *pLoc = NULL;

    hr = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, (LPVOID *) &pLoc);
 
    if ( FAILED( hr ) )
    {
		OutputDebugString ( "GetWMIDeviceStats - Failed to create IWbemLocator object.\n");
        CoUninitialize();
        return 0;
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method

    IWbemServices *pSvc = NULL;
	
    // Connect to the root\cimv2 namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.
    hr = pLoc->ConnectServer(
         _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
         NULL,                    // User name. NULL = current user
         NULL,                    // User password. NULL = current
         0,                       // Locale. NULL indicates current
         NULL,                    // Security flags.
         0,                       // Authority (e.g. Kerberos)
         0,                       // Context object 
         &pSvc                    // pointer to IWbemServices proxy
         );
    
    if ( FAILED( hr ) )
    {
		OutputDebugString ( "GetWMIDeviceStats - Could not connect.\n");
        pLoc->Release();     
        CoUninitialize();
        return 0;
    }

//	OutputDebugString ( L"GetWMIDeviceStats - Connected to ROOT\\CIMV2 WMI namespace\n");


    // Set security levels on the proxy

    hr = CoSetProxyBlanket(
       pSvc,                        // Indicates the proxy to set
       RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
       RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
       NULL,                        // Server principal name 
       RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
       RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
       NULL,                        // client identity
       EOAC_NONE                    // proxy capabilities 
    );

    if ( FAILED( hr ) )
    {
		OutputDebugString ( "GetWMIDeviceStats - Could not set proxy blanket.\n");
        pSvc->Release();
        pLoc->Release();     
        CoUninitialize();
        return 0;
    }


    // Use the IWbemServices pointer to make requests of WMI

	//
	// --- Win32_VideoController --------------------------------------------------
	//

    IEnumWbemClassObject* pEnumerator = NULL;
    hr = pSvc->ExecQuery( bstr_t("WQL"), bstr_t("SELECT * FROM Win32_VideoController"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

    if ( FAILED( hr ) )
    {
		OutputDebugString ( "GetWMIDeviceStats - Query for Win32_VideoController failed.\n");

		pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 0;
    }


    // Get the data from the above query
    IWbemClassObject *pclsObj = NULL;
    ULONG uReturn = 0;
   
    while ( pEnumerator )
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if(0 == uReturn)
        {
            break;
        }

        VARIANT vtProp;
        VariantInit(&vtProp);

		// Pluck a series of properties out of the query from Win32_VideoController

//        hr = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);		// Basically the same as "VideoProcessor"
//		if ( SUCCEEDED( hr ) )
//		{
//			wsprintf( pAdapter->m_szPrimaryAdapterDescription, vtProp.bstrVal );
//		}

        hr = pclsObj->Get(L"AdapterRAM", 0, &vtProp, 0, 0);
		if ( SUCCEEDED( hr ) )
		{
			nBytes = vtProp.ulVal; // Video RAM in bytes, AdatperRam is returned as the I4 type so we read it out as unsigned int, 
								   // see http://msdn.microsoft.com/en-us/library/windows/desktop/aa394512(v=vs.85).aspx
		}

        VariantClear(&vtProp);
    }

    // Cleanup
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
	if ( pclsObj )
	{
		pclsObj->Release();
	}
    CoUninitialize();

	return nBytes;
}
