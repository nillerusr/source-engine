//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
	   
#include "unitlib/unitlib.h"
#include "tier0/dbg.h"
#include <string.h>



//-----------------------------------------------------------------------------
//
// Base class for test cases
//
//-----------------------------------------------------------------------------
CTestCase::CTestCase( char const* pName, ITestSuite* pParent )
{
	Assert( pName );
	m_pName = new char[strlen(pName) + 1];
	strcpy( m_pName, pName );
	
	// Only install the test case if it has no parent
	if (pParent)
	{
		pParent->AddTest( this );
	}
	else
	{
		UnitTestInstallTestCase( this );
	}
}

CTestCase::~CTestCase()
{
	if (m_pName)
		delete[] m_pName;
}

char const* CTestCase::GetName()
{
	return m_pName;
}


//-----------------------------------------------------------------------------
//
// Test suite class
//
//-----------------------------------------------------------------------------

CTestSuite::CTestSuite( char const* pName, ITestSuite* pParent )
{
	m_TestCount = 0;
	m_ppTestCases = 0;

	m_pName = new char[strlen(pName) + 1];
	strcpy( m_pName, pName );

	// Only install the test case if it has no parent
	if (pParent)
	{
		pParent->AddTest( this );
	}
	else
	{
		UnitTestInstallTestCase( this );
	}
}

CTestSuite::~CTestSuite()
{
	if (m_ppTestCases)
		free(m_ppTestCases);
	if (m_pName)
		delete[] m_pName;
}

char const* CTestSuite::GetName()
{
	return m_pName;
}

void CTestSuite::AddTest( ITestCase* pTest )
{
	Assert( pTest );
	if (!m_ppTestCases)
	{
		m_ppTestCases = (ITestCase**)malloc( sizeof(ITestCase**) );
	}
	else
	{
		m_ppTestCases = (ITestCase**)realloc( m_ppTestCases, (m_TestCount+1) * sizeof(ITestCase**) );
	}							  

	m_ppTestCases[m_TestCount++] = pTest;
}

void CTestSuite::RunTest()
{
	for ( int i = 0; i < m_TestCount; ++i )
	{
		m_ppTestCases[i]->RunTest();
	}
}



//-----------------------------------------------------------------------------
// This is the main function exported by the unit test library used by
// unit test DLLs to install their test cases into a list to be run
//-----------------------------------------------------------------------------

static int s_TestCount = 0;
static int s_TestAllocated = 0;
static ITestCase** s_ppTestCases = 0;

void UnitTestInstallTestCase( ITestCase* pTest )
{
	Assert( pTest );
	if (s_TestCount == s_TestAllocated)
	{
		if (!s_ppTestCases)
		{
			s_ppTestCases = (ITestCase**)malloc( 16 * sizeof(ITestCase**) );
			s_TestAllocated = 16;
		}
		else
		{
			s_ppTestCases = (ITestCase**)realloc( s_ppTestCases, s_TestAllocated * 2 * sizeof(ITestCase**) );
			s_TestAllocated *= 2;
		}
	}
	s_ppTestCases[s_TestCount++] = pTest;
}


//-----------------------------------------------------------------------------
// These are the methods used by the unit test running program to run all tests
//-----------------------------------------------------------------------------

int UnitTestCount()
{
	return s_TestCount;
}

ITestCase* GetUnitTest( int i )
{
	Assert( i < s_TestCount );
	return s_ppTestCases[i];
}
