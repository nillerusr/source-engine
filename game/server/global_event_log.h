//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// work in progress
//
//===============================================================================

#ifndef __GLOBAL_EVENT_LOG_H
#define __GLOBAL_EVENT_LOG_H
#ifdef _WIN32
#pragma once
#endif


#include "igamesystem.h"


class CGlobalEvent;


class CGlobalEventLog : public CAutoGameSystemPerFrame
{
public:
	typedef enum
	{
		GLOBAL_EVENT_NPCS = 0,

		GLOBAL_EVENT_MAX
	} EGlobalEvent;

public:
	CGlobalEventLog( );

	CGlobalEvent	*GetGlobalEvent( EGlobalEvent GlobalEvent );

	CGlobalEvent	*CreateEvent( const char *pszName, bool bIsHighLevel, CGlobalEvent *pParent = NULL );
	CGlobalEvent	*CreateTempEvent( const char *pszName, CGlobalEvent *pParent = NULL );
	void			RemoveEvent( CGlobalEvent *pEvent );

	void			AddKeyValue( CGlobalEvent *pEvent, bool bVarying, const char *pszKey, const char *pszValueFormat, ... );

	void			SendUpdate( );

protected:
	virtual void PostInit( );
	virtual void FrameUpdatePostEntityThink( );

private:
	unsigned int					m_nNextID;
	CUtlVector< CGlobalEvent * >	m_Events;
	CUtlVector< CGlobalEvent * >	m_TempEvents;
	CUtlVector< CGlobalEvent * >	m_DirtyEvents;
	CGlobalEvent					*m_pGlobalEvents[ GLOBAL_EVENT_MAX ];
};


extern CGlobalEventLog	GlobalEventLog;


#endif // #ifndef __GLOBAL_EVENT_LOG_H

