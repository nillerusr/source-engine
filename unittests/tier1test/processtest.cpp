//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for processes
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "vstdlib/iprocessutils.h"
#include "tier1/strtools.h"
#include "tier1/tier1.h"
#include "tier0/dbg.h"


DEFINE_TESTSUITE( ProcessTestSuite )

DEFINE_TESTCASE( ProcessTestSimple, ProcessTestSuite )
{
	Msg( "Simple process test...\n" );

	ProcessHandle_t hProcess = g_pProcessUtils->StartProcess( "unittests\\testprocess.exe -delay 1.0", true ); 
	g_pProcessUtils->WaitUntilProcessCompletes( hProcess );
	int nLen = g_pProcessUtils->GetProcessOutputSize( hProcess );
	char *pBuf = (char*)_alloca( nLen );
	g_pProcessUtils->GetProcessOutput( hProcess, pBuf, nLen );
	g_pProcessUtils->CloseProcess( hProcess );
	Shipping_Assert( !Q_stricmp( pBuf, "Test Finished!\n" ) );
}


DEFINE_TESTCASE( ProcessTestBufferOverflow, ProcessTestSuite )
{
	Msg( "Buffer overflow process test...\n" );

	ProcessHandle_t hProcess = g_pProcessUtils->StartProcess( "unittests\\testprocess.exe -delay 1.0 -extrabytes 32768", true );
	g_pProcessUtils->WaitUntilProcessCompletes( hProcess );
	int nLen = g_pProcessUtils->GetProcessOutputSize( hProcess );
	Shipping_Assert( nLen == 32768 + 16 );
	char *pBuf = (char*)_alloca( nLen );
	g_pProcessUtils->GetProcessOutput( hProcess, pBuf, nLen );
	g_pProcessUtils->CloseProcess( hProcess );
	Shipping_Assert( !Q_strnicmp( pBuf, "Test Finished!\n", 15 ) );

	int nEndExtraBytes = 32768;
	char *pTest = pBuf + 15;
	while( --nEndExtraBytes >= 0 )
	{
		Shipping_Assert( *pTest == (( nEndExtraBytes % 10 ) + '0') );
		++pTest;
	}
}

