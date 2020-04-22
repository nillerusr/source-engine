//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "managertest.h"
#include "replay/replayutils.h"
#include "cl_replaycontext.h"
#include "KeyValues.h"
#include "replay/shared_defs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if _DEBUG

//----------------------------------------------------------------------------------------

#define TESTMANAGER_VERSION		0
#define SUBDIR_TEST				"test"

//----------------------------------------------------------------------------------------

const char *Test_GetPath()
{
	return Replay_va( "%s%c%s%c%s%c", g_pEngine->GetGameDir(), CORRECT_PATH_SEPARATOR, SUBDIR_REPLAY, CORRECT_PATH_SEPARATOR, SUBDIR_TEST, CORRECT_PATH_SEPARATOR );
}

//----------------------------------------------------------------------------------------

CTestObj::CTestObj()
:	m_nTest( -1 )
{
	m_strTest = "";

	m_pTest = new int;
}

CTestObj::~CTestObj()
{
	delete m_pTest;
}

const char *CTestObj::GetSubKeyTitle() const
{
	return Replay_va( "test_%i", GetHandle() );
}

const char *CTestObj::GetPath() const
{
	return Test_GetPath();
}

void CTestObj::OnDelete()
{
}

bool CTestObj::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_nTest = pIn->GetInt( "int_test", -1 );
	m_strTest = pIn->GetString( "int_test" );

	return true;
}

void CTestObj::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );
	pOut->SetInt( "int_test", m_nTest );
	pOut->SetString( "str_test", m_strTest.Get() );
}

//----------------------------------------------------------------------------------------

/*static*/ void CTestManager::Test()
{
#if 0
	CUtlLinkedList< CTestObj *, int > lstTest;
	CTestObj test;
	lstTest.AddToTail( &test );
	lstTest.RemoveAll();

	CTestManager m;
	m.Init();
	for ( int i = 0; i < 3; ++i )
	{
		CTestObj *pNewObj = m.CreateAndGenerateHandle();
		m.Add( pNewObj );
	}
	m.Shutdown();
#endif
}

CTestManager::CTestManager()
{
}

const char *CTestManager::GetIndexPath() const
{
	return Test_GetPath();
}

int CTestManager::GetVersion() const
{
	return TESTMANAGER_VERSION;
}

CTestObj *CTestManager::Create()
{
	return new CTestObj();
}

#endif
	
//----------------------------------------------------------------------------------------
