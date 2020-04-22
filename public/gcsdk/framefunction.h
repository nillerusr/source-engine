//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: Provides a frame function manager that allows for different parts
//  of a GC to hook into per frame updated
//
//=============================================================================

#ifndef FRAMEFUNCTION_H
#define FRAMEFUNCTION_H
#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{

//the generic interface for frame functions. These can be derived from and should execute the work in BRun. It returns a boolean indicating whether or not it has
//more work to complete
class CBaseFrameFunction
{
public:

	//the different tiers of functions that can have functions tied to them
	enum EFrameType
	{
		k_EFrameType_RunOnce,					// called once per frame
		k_EFrameType_RunUntilCompleted,			// run tasks that shouldn't wait a frame between execution (gets called at least once per frame, more if work to do & we have time)		
		k_EFrameType_RunUntilCompletedLowPri,	// as above, but only runs if k_EFrameTypeRunUntilCompleted function return they have no remaining work to do
		//must come last
		k_EFrameType_Count
	};

	CBaseFrameFunction( const char *pchName, EFrameType eFrameType );
	virtual ~CBaseFrameFunction();

	// runs the item
	virtual bool BRun( const CLimitTimer &limitTimer ) = 0;

	//called to handle registering for updates and unregistering. Not registered by default
	void Register();
	void Deregister();
	bool IsRegistered() const				{ return m_bRegistered; }

protected:

	//the frame function manager has access to the internals to avoid exposing them to derived classes
	friend class CFrameFunctionMgr;

	//let the frame function manager access internals for profiling
	CUtlString	m_sName;					// function name (for debugging)
	uint64		m_nTrackedTime;				// how much time we have tracked with this frame function
	uint32		m_nNumCalls;				// the number of ticks that this has been associated with
	EFrameType	m_EFrameType;				// what kind of frame function is this
	bool		m_bRegistered;				// are we currently registered?
};


//a utility class that helps handle registering for frame functions and adapts it to a member function call
template < class T >
class CFrameFunction : 
	public CBaseFrameFunction
{
public:
	typedef bool ( T::*func_t )( const CLimitTimer &limitTimer );

	//construct and register all in one
	CFrameFunction( T *pObj, func_t func, const char *pchName, EFrameType eFrameType ) : 
		CBaseFrameFunction( pchName, eFrameType ), m_pObj( NULL ), m_Func( NULL )
	{
		Register( pObj, func );
	}

	//construct, but don't immediately register for updates
	CFrameFunction( const char *pchName, EFrameType eFrameType ) :
		CBaseFrameFunction( pchName, eFrameType ), m_pObj( NULL ), m_Func( NULL )
	{
	}

	void Register( T *pObj, func_t func )
	{
		m_pObj = pObj;
		m_Func = func;
		CBaseFrameFunction::Register();
	}

	virtual bool BRun( const CLimitTimer &limitTimer )
	{
		return (m_pObj->*m_Func)( limitTimer );
	}

	virtual bool BAllocedSeparately() { return false; }
private:
	T *m_pObj;
	func_t m_Func;
};

//the main manager that handles registration of all the frame functions and tracks performance and dispatches
//appropriately
class CFrameFunctionMgr
{
public:

	CFrameFunctionMgr();

	//called to register a main loop function for the specified tier
	void Register( CBaseFrameFunction* pFrameFunc );
	//handles unregistering a main loop function
	void Deregister( CBaseFrameFunction* pFrameFunc );

	//called to execute the main frame. This should preceded individual frame ticks
	void	RunFrame( const CLimitTimer& limitTimer );
	//called within a frame for each tick
	bool	RunFrameTick( const CLimitTimer& limitTimer );

	//called to dump a listing of the performance timings of all the frame functions that are registered
	void	DumpProfile();
	//resets the profile stats
	void	ClearProfile();

private:

	//sort function
	static bool SortFrameFuncByTime( const CBaseFrameFunction* pLhs, const CBaseFrameFunction* pRhs );

	//called to update the frame functions associated with the specified list
	bool RunFrameList( CBaseFrameFunction::EFrameType eType, const CLimitTimer& limitTimer );

	//the listing of subscribed 
	CUtlVector< CBaseFrameFunction* >	m_MainLoopFrameFuncs[ CBaseFrameFunction::k_EFrameType_Count ];
	//the number of frames that we have profiled
	uint32		m_nNumProfileFrames;
	//have we completed all of the high priority work for this frame?
	bool		m_bCompletedHighPri;
};

//the global frame function manager
CFrameFunctionMgr& GFrameFunctionMgr();

} //namespace GCSDK

#endif
