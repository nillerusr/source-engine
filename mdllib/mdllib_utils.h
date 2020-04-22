//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MDLLIB_UTILS_H
#define MDLLIB_UTILS_H

#ifdef _WIN32
#pragma once
#endif


#include "utlmap.h"
#include "utlvector.h"
#include "bitvec.h"


//////////////////////////////////////////////////////////////////////////
//
// Helper macros
//
//////////////////////////////////////////////////////////////////////////

// Declare a pointer and automatically do the cast of initial value to the pointer type
#define DECLARE_PTR( type, name, initval ) type *name = ( type * ) ( initval )

// Compute a pointer that is offset given number of bytes from the base pointer
#define BYTE_OFF_PTR( initval, offval ) ( ( ( byte * ) ( initval ) ) + ( offval ) )

// Compute difference in bytes between two pointers
#define BYTE_DIFF_PTR( begin, end ) ( ( ( byte * ) ( end ) ) - ( ( byte * ) ( begin ) ) )


// "for {" to iterate children of a studio container
#define ITERATE_CHILDREN( type, name, parent, accessor, count )					\
	for ( int name##_idx = 0; name##_idx < (parent)->count; ++ name##_idx ) {	\
			type *name = (parent)->accessor( name##_idx );

// "for {" to jointly iterate children of 2 studio containers of same size
#define ITERATE_CHILDREN2( type, type2, name, name2, parent, parent2, accessor, accessor2, count )	\
	for ( int name##_idx = 0; name##_idx < (parent)->count; ++ name##_idx ) {	\
			type *name = (parent)->accessor( name##_idx );					\
			type2 *name2 = (parent2)->accessor2( name##_idx );

// "}" to mark the end of iteration block
#define ITERATE_END }

// Get the child of a container by index
#define CHILD_AT( parent, accessor, idx ) ( (parent)->accessor( idx ) )


//
// CLessSimple< T >
//	Comparison policy to use "t1 < t2" comparison rule.
//
template < typename T >
class CLessSimple
{
public:
	bool Less( const T& src1, const T& src2, void *pCtx )
	{
		pCtx;
		return ( src1 < src2 );
	}
};

//
// CInsertionTracker
//	Class that is tracking insertions that are scheduled to happen at given points.
//	Use policy:
//		InsertBytes / InsertElements	[*]		-- schedule insertions
//		Finalize								-- finalize insertion information
//		ComputePointer / ComputeOffset	[*]		-- compute new pointers/offsets that will happen after insertions
//		MemMove									-- perform memory moves to apply insertions
//
class CInsertionTracker
{
public:
	CInsertionTracker() : m_map( DefLessFunc( byte * ) ) {}

	// Schedules a piece of memory for insertion
public:
	void InsertBytes( void *pos, int length );

	template< typename T >
	void InsertElements( T *ptr, int count = 1 ) { InsertBytes( ( byte * ) ptr, count * sizeof( T ) ); }

	int GetNumBytesInserted() const;

	// Finalizes the insertion information
public:
	void Finalize();

	// Computes where the pointer would point after memory insertion occurs
public:
	void * ComputePointer( void *ptrNothingInserted ) const;
	int ComputeOffset( void *ptrBase, int off ) const;

	// Perform memory moves, the buffer should be large enough to accommodate inserted bytes
public:
	void MemMove( void *ptrBase, int &length ) const;

protected:
	typedef CUtlMap< byte *, int, unsigned int > Map;
	Map m_map; // pos -> length
};


//
// CRemoveTracker
//	Class that is tracking removals that are scheduled to happen at given points.
//	Use policy:
//		RemoveBytes / RemoveElements	[*]		-- schedule removals
//		Finalize								-- finalize removal information
//		ComputePointer / ComputeOffset	[*]		-- compute new pointers/offsets that will happen after removals
//		MemMove									-- perform memory moves to apply removals
//
class CRemoveTracker
{
public:
	CRemoveTracker() : m_map( DefLessFunc( byte * ) ) {}

	// Schedules a piece of memory for removal
public:
	void RemoveBytes( void *pos, int length );
	
	template< typename T >
	void RemoveElements( T *ptr, int count = 1 ) { RemoveBytes( ( byte * ) ptr, count * sizeof( T ) ); }

	int GetNumBytesRemoved() const;

	// Finalizes the removal information
public:
	void Finalize();

	// Computes where the pointer would point after memory removal occurs
public:
	void * ComputePointer( void *ptrNothingRemoved ) const;
	int ComputeOffset( void *ptrBase, int off ) const;

public:
	void MemMove( void *ptrBase, int &length ) const;

protected:
	typedef CUtlMap< byte *, int, unsigned int > Map;
	Map m_map; // pos -> length

	struct Item
	{
		Map::IndexType_t idx;
		byte *ptr;
		int len;
	};
	Item m_hint;
};


//
// CGrowableBitVec
//	Serves bit accumulation.
//	Provides "GrowSetBit" method to automatically grow to the required size
//	and set the given bit.
//	Provides safe "IsBitSet" that would return false for missing bits.
//
class CGrowableBitVec : public CVarBitVec
{
public:
	void GrowSetBit( int iBit )
	{
		if ( iBit >= GetNumBits() )
			Resize( iBit + 1, false );
		Set( iBit );
	}

	bool IsBitSet( int bitNum ) const
	{
		return ( bitNum < GetNumBits() ) && CVarBitVec::IsBitSet( bitNum );
	}
};


//
// CGrowableVector
//	Provides zero-initialization for new elements.
//
template < typename T >
class CGrowableVector : public CUtlVector < T >
{
public:
	T & operator[] ( int idx )
	{
		while ( idx >= Count() )
			AddToTail( T() );
		return CUtlVector < T >::operator []( idx );
	}
};




#endif // #ifndef MDLLIB_UTILS_H
