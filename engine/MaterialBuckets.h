//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This is a helper class for the situation where you want to build lists of 
// things that fall into buckets, and effeciently keep up with which buckets have
// been used since flush.
//
//=============================================================================//

#ifndef MATERIALBUCKETS_H
#define MATERIALBUCKETS_H
#ifdef _WIN32
#pragma once
#endif

// FLASHLIGHTFIXME: Make all of the buckets share the same m_Elements (ie. make m_Elements static)

template <class Element_t>
class CMaterialsBuckets
{
public:
	typedef unsigned short SortIDHandle_t;
	typedef unsigned short ElementHandle_t;
	CMaterialsBuckets()
	{
		m_FlushCount = -1;
	}

	// Set the number of buckets that are needed.  This should get called every time 
	// a level is loaded.
	void SetNumMaterialSortIDs( int n )
	{
		m_MaterialSortInfoArray.Purge();
		m_MaterialSortInfoArray.SetCount( n );
		m_Elements.Purge();

		m_UsedSortIDs.Purge();
	}

	// Clear out all buckets.  This should get called once a frame.
	void Flush( void )
	{
		m_FlushCount++;
		m_Elements.RemoveAll();
		m_UsedSortIDs.RemoveAll();
	}

	//
	// These functions are used to get at the list of used buckets.
	//
	SortIDHandle_t GetFirstUsedSortID()
	{
		return m_UsedSortIDs.Head();
	}

	SortIDHandle_t GetNextUsedSortID( SortIDHandle_t prevSortID )
	{
		return m_UsedSortIDs.Next( prevSortID );
	}

	int GetSortID( SortIDHandle_t handle )
	{
		return m_UsedSortIDs[handle];
	}
	
	SortIDHandle_t InvalidSortIDHandle()
	{
		return m_UsedSortIDs.InvalidIndex();
	}


	//
	// These functions are used to get at the list of elements for each sortID.
	//
	ElementHandle_t GetElementListHead( int sortID )
	{
		return m_MaterialSortInfoArray[sortID].m_Head;
	}

	ElementHandle_t GetElementListNext( ElementHandle_t h )
	{
		return m_Elements.Next( h );
	}

	Element_t GetElement( ElementHandle_t h )
	{
		return m_Elements[h];
	}

	ElementHandle_t InvalidElementHandle()
	{
		return m_Elements.InvalidIndex();
	}


	// Add an element to the the bucket specified by sortID
	void AddElement( int sortID, Element_t elem )
	{
		// Allocate an element to stick this in.
		unsigned short elemID = m_Elements.Alloc( true );
		m_Elements[elemID] = elem;
		
		if( m_MaterialSortInfoArray[sortID].m_FlushCount != m_FlushCount )
		{
			// This is the first element that has used this sort id since flush.
			// FLASHLIGHTFIXME: need to sort these by vertex format when shoving
			// them into this list!

			// Mark this sortID as having been used since the last flush.
			m_MaterialSortInfoArray[sortID].m_FlushCount = m_FlushCount;

			// Add this sortID to the list of sortIDs used since flush.
			m_UsedSortIDs.AddToTail( sortID );

			// Set the head pointer for this sort id to this element.
			m_MaterialSortInfoArray[sortID].m_Head = elemID;
		}
		else
		{
			// We already have an element in this sort id since flush, so chain
			// into thelist of elements for this sort id.
			m_Elements.LinkBefore( m_MaterialSortInfoArray[sortID].m_Head, elemID );
			m_MaterialSortInfoArray[sortID].m_Head = elemID;
		}
	}

private:
	
	struct MaterialSortInfo_t
	{
		MaterialSortInfo_t() :
			m_FlushCount( -1 ),
			m_Head( (unsigned short)-1 )  // i.e., InvalidIndex()
		{
		}
		
		int m_FlushCount;
		unsigned short m_Head;
	};
	
	// This is a list of material sort info ids that have been used since flush.
	CUtlLinkedList<unsigned short> m_UsedSortIDs;

	// This is m_NumMaterialSortIDs big.
	CUtlVector<MaterialSortInfo_t> m_MaterialSortInfoArray;
	
	// This is used in multilist mode to make elements that belong in the multiple lists of 
	CUtlLinkedList<Element_t, unsigned short, true> m_Elements;

	int m_FlushCount;
		
};

#endif // MATERIALBUCKETS_H
