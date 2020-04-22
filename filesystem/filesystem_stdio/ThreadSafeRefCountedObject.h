//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef THREADSAFEREFCOUNTEDOBJECT_H
#define THREADSAFEREFCOUNTEDOBJECT_H
#ifdef _WIN32
#pragma once
#endif


// This class can be used for fast access to an object from multiple threads,
// and the main thread can wait until the threads are done using the object before it frees the object.
template< class T >
class CThreadSafeRefCountedObject
{
public:
	CThreadSafeRefCountedObject( T initVal )
	{
		m_RefCount = 0;
		m_pObject = initVal;
		m_RefCount = 0;
	}
	
	void Init( T pObj )
	{
		Assert( ThreadInMainThread() );
		Assert( !m_pObject );
		m_RefCount = 0;
		m_pObject = pObj;
		m_RefCount = 1;
	}
	
	// Threads that access the object need to use AddRef/Release to access it.
	T AddRef()
	{
		if ( ++m_RefCount > 1 )
		{
			return m_pObject;
		}
		else
		{
			// If the refcount was 0 when we called this, then the whitelist is about to be freed.
			--m_RefCount;
			return NULL;
		}
	}
	void ReleaseRef( T pObj )
	{
		if ( --m_RefCount >= 1 )
		{
			Assert( m_pObject == pObj );
		}
	}

	// The main thread can use this to access the object, since only it can Init() and Free() the object.
	T GetInMainThread()
	{
		Assert( ThreadInMainThread() );
		return m_pObject;
	}

	// The main thread calls this after it has released its last reference to the object.
	void ResetWhenNoRemainingReferences( T newValue )
	{
		Assert( ThreadInMainThread() );
		
		// Wait until we can free it.
		while ( m_RefCount > 0 )
		{
			CThread::Sleep( 20 );
		}
		
		m_pObject = newValue;
	}

private:	
	CInterlockedIntT<long>	m_RefCount;
	T						m_pObject;
};


#endif // THREADSAFEREFCOUNTEDOBJECT_H
