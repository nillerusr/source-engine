//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef IVPROFEXPORT_H
#define IVPROFEXPORT_H
#ifdef _WIN32
#pragma once
#endif


#include "tier0/vprof.h"


#define VPROF_EXPORT_INTERFACE_VERSION "VProfExport001"
#include <Color.h>

abstract_class IVProfExport
{
public:
	enum
	{
		MAX_BUDGETGROUP_TIMES = 512
	};

	class CExportedBudgetGroupInfo
	{
	public:
		const char *m_pName;
		int m_BudgetFlags;	// Combination of BUDGETFLAG_ defines (in vprof.h)
		Color m_Color;
	};

public:

	// Use this to register and unregister for the vprof data (if you don't, GetBudgetGroupTimes may return no data).
	virtual void AddListener() = 0;
	virtual void RemoveListener() = 0;

	// Pause/resume profiling so it doesn't capture data we don't want.
	virtual void PauseProfile() = 0;
	virtual void ResumeProfile() = 0;

	// Set a combination of BUDGETFLAG_ defines to define what data you're get back.
	// Note: this defines which budget groups to REJECT (ones that have flags that are in filter won't be returned).
	virtual void SetBudgetFlagsFilter( int filter ) = 0;

	// Use budgetFlags to filter out which budget groups you're interested in.
	virtual int GetNumBudgetGroups() = 0;
	
	// pInfos must have space to hold the return value from GetNumBudgetGroups().
	virtual void GetBudgetGroupInfos( CExportedBudgetGroupInfo *pInfos ) = 0;

	virtual void GetBudgetGroupTimes( float times[MAX_BUDGETGROUP_TIMES] ) = 0;
};


#endif // IVPROFEXPORT_H
