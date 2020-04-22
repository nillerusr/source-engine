//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef MANAGERTEST_H
#define MANAGERTEST_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "genericpersistentmanager.h"
#include "replay/replayhandle.h"
#include "replay/irecordingsessionblockmanager.h"
#include "utlstring.h"
#include "baserecordingsession.h"
#include "replay/basereplayserializeable.h"
#include "baserecordingsessionblock.h"

//----------------------------------------------------------------------------------------

class CTestObj : public CBaseReplaySerializeable
{
	typedef CBaseReplaySerializeable BaseClass;
public:
	CTestObj();
	~CTestObj();

	virtual const char	*GetSubKeyTitle() const;
	virtual const char	*GetPath() const;
	virtual void		OnDelete();
	virtual bool		Read( KeyValues *pIn );
	virtual void		Write( KeyValues *pOut );

	CUtlString		m_strTest;
	int				m_nTest;

	int				*m_pTest;
};

//----------------------------------------------------------------------------------------

class ITestManager : public IBaseInterface
{
public:
	virtual void	SomeTest() = 0;
};

//----------------------------------------------------------------------------------------

class CTestManager : public CGenericPersistentManager< CTestObj >,
					 public ITestManager
{
	typedef CGenericPersistentManager< CTestObj > BaseClass;

public:
	CTestManager();

	static void Test();

	//
	// CGenericPersistentManager
	//
	virtual CTestObj	*Create();
	virtual bool		ShouldSerializeToIndividualFiles() const { return true; }
	virtual const char	*GetIndexPath() const;
	virtual const char	*GetDebugName() const			{ return "test manager"; }
	virtual int			GetVersion() const;
	virtual const char	*GetIndexFilename() const	{ return "test_index." GENERIC_FILE_EXTENSION; }

	//
	// ITestManager
	//
	virtual void		SomeTest() {}

};

//----------------------------------------------------------------------------------------

#endif // MANAGERTEST_H