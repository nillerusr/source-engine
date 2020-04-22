//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( ITHREAD_H )
#define ITHREAD_H
#ifdef _WIN32
#pragma once
#endif

// Typedef to point at member functions of CCDAudio
typedef void ( CCDAudio::*vfunc )( int param1, int param2 );

//-----------------------------------------------------------------------------
// Purpose: CD Audio thread processing
//-----------------------------------------------------------------------------
abstract_class IThread
{
public:
	virtual			~IThread( void ) { }

	virtual bool	Init( void ) = 0;
	virtual bool	Shutdown( void ) = 0;

	// Add specified item to thread for processing
	virtual bool	AddThreadItem( vfunc pfn, int param1, int param2 ) = 0;
};

extern IThread *thread;

#endif // ITHREAD_H