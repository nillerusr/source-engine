//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef UTLQUEUE_H
#define UTLQUEUE_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

// T is the type stored in the queue
template< class T, class M = CUtlMemory< T > > 
class CUtlQueue
{
public:

	// constructor: lessfunc is required, but may be set after the constructor with
	// SetLessFunc
	CUtlQueue( int growSize = 0, int initSize = 0 );
	CUtlQueue( T *pMemory, int numElements );

	// element access
	T& operator[]( int i );
	T const& operator[]( int i ) const;
	T& Element( int i );
	T const& Element( int i ) const;

	// return the item from the front of the queue and delete it
	T const& RemoveAtHead();
	// return the item from the end of the queue and delete it
	T const& RemoveAtTail();

	// return item at the front of the queue
	T const& Head();
	// return item at the end of the queue
	T const& Tail();

	// put a new item on the queue to the tail.
	void	Insert( T const &element );

	// checks if an element of this value already exists on the queue, returns true if it does
	bool		Check( T const element );

	// Returns the count of elements in the queue
	int			Count() const { return m_heap.Count(); }
	
	// Is element index valid?
	bool IsIdxValid( int i ) const;

	// doesn't deallocate memory
	void		RemoveAll() { m_heap.RemoveAll(); }

	// Memory deallocation
	void		Purge() { m_heap.Purge(); }

protected:
	CUtlVector<T, M>	m_heap;
	T					m_current;
};

//-----------------------------------------------------------------------------
// The CUtlQueueFixed class:
// A queue class with a fixed allocation scheme
//-----------------------------------------------------------------------------
template< class T, size_t MAX_SIZE >
class CUtlQueueFixed : public CUtlQueue< T, CUtlMemoryFixed<T, MAX_SIZE > >
{
	typedef CUtlQueue< T, CUtlMemoryFixed<T, MAX_SIZE > > BaseClass;
public:

	// constructor, destructor
	CUtlQueueFixed( int growSize = 0, int initSize = 0 ) : BaseClass( growSize, initSize ) {}
	CUtlQueueFixed( T* pMemory, int numElements ) : BaseClass( pMemory, numElements ) {}
};

template< class T, class M >
inline CUtlQueue<T, M>::CUtlQueue( int growSize, int initSize ) :
	m_heap(growSize, initSize)
{
}

template< class T, class M >
inline CUtlQueue<T, M>::CUtlQueue( T *pMemory, int numElements )	: 
	m_heap(pMemory, numElements)
{
}

//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------

template< class T, class M >
inline T& CUtlQueue<T,M>::operator[]( int i )
{
	return m_heap[i];
}

template< class T, class M >
inline T const& CUtlQueue<T,M>::operator[]( int i ) const
{
	return m_heap[i];
}

template< class T, class M >
inline T& CUtlQueue<T,M>::Element( int i )
{
	return m_heap[i];
}

template< class T, class M >
inline T const& CUtlQueue<T,M>::Element( int i ) const
{
	return m_heap[i];
}

//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------

template< class T, class M >
inline bool CUtlQueue<T,M>::IsIdxValid( int i ) const
{
	return (i >= 0) && (i < m_heap.Count());
}

template <class T, class M>
inline T const& CUtlQueue<T, M>::RemoveAtHead()
{
	m_current = m_heap[0];
	m_heap.Remove((int)0);
	return m_current; 
}

template <class T, class M>
inline T const& CUtlQueue<T, M>::RemoveAtTail()
{
	m_current = m_heap[ m_heap.Count() - 1 ];
	m_heap.Remove((int)(m_heap.Count() - 1));
	return m_current; 
}

template <class T, class M>
inline T const& CUtlQueue<T, M>::Head()
{
	m_current = m_heap[0];
	return m_current; 
}

template <class T, class M>
inline T const& CUtlQueue<T, M>::Tail()
{
	m_current = m_heap[ m_heap.Count() - 1 ];
	return m_current; 
}

template <class T, class M>
void CUtlQueue<T, M>::Insert( T const &element )
{
	int index = m_heap.AddToTail();
	m_heap[index] = element;
}

template <class T, class M>
bool CUtlQueue<T, M>::Check( T const element )
{
	int index = m_heap.Find(element);
	return ( index != -1 );
}


#endif // UTLQUEUE_H
