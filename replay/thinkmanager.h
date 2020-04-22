//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef FLUSHMANAGER_H
#define FLUSHMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "utllinkedlist.h"
#include "ithinkmanager.h"

//----------------------------------------------------------------------------------------

class IThinker;

//----------------------------------------------------------------------------------------

//
// Generic think manager - only worries about CGenericPersistentManagers for
// now though - not a CGenericPersistentManager's Think() can result in subtle
// bugs where files don't  get saved properly.
//
class CThinkManager : public IThinkManager
{
public:
	virtual void		AddThinker( IThinker *pThinker );
	virtual void		RemoveThinker( IThinker *pThinker );

	void				Think();

	CUtlLinkedList< IThinker *, int > m_lstManagers;
};

//----------------------------------------------------------------------------------------

extern IThinkManager *g_pThinkManager;

//----------------------------------------------------------------------------------------

#endif // FLUSHMANAGER_H
