//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMELEMENTFRAMEWORK_H
#define DMELEMENTFRAMEWORK_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/idatamodel.h"
#include "tier1/utlvector.h"
#include "dependencygraph.h"


//-----------------------------------------------------------------------------
// element framework implementation
//-----------------------------------------------------------------------------
class CDmElementFramework : public IDmElementFramework
{
public:
	CDmElementFramework();

public:
	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IDmElementFramework
	virtual DmPhase_t GetPhase();
	virtual void SetOperators( const CUtlVector< IDmeOperator* > &operators );
	virtual void BeginEdit(); // ends in edit phase, forces apply/resolve if from edit phase
	virtual void Operate( bool bResolve ); // ends in output phase
	virtual void Resolve();

public:
	// Other public methods
	void AddElementToDirtyList( DmElementHandle_t hElement );
	void RemoveCleanElementsFromDirtyList();

	// Non-virtual methods of identical virtual functions
	DmPhase_t FastGetPhase();


private:
	void EditApply();

	// Invoke the resolve method
	void Resolve( bool clearDirtyFlags );

	CDependencyGraph m_dependencyGraph;
	CUtlVector< DmElementHandle_t > m_dirtyElements;
	DmPhase_t m_phase;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
extern CDmElementFramework *g_pDmElementFrameworkImp;


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline DmPhase_t CDmElementFramework::FastGetPhase()
{
	return m_phase;
}


#endif // DMELEMENTFRAMEWORK_H