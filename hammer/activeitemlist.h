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

#ifndef ACTIVEITEMLIST_H
#define ACTIVEITEMLIST_H
#pragma once

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//=============================================================================

typedef int ITEM_HANDLE;

//=============================================================================

template <class T> class ActiveItemList
{
public:
    
    ActiveItemList();
    ActiveItemList( int size );

    void SetSize( int size );
    int GetSize( void );

    int GetNumberOfItems( void );
	T* GetFirstItem( void );
	T* GetNextItem( void );

    ITEM_HANDLE GetEmptyItemHandle( void );

    T* GetItem( ITEM_HANDLE handle );
    void RemoveItem( ITEM_HANDLE handle );

    void SetActiveItem( ITEM_HANDLE handle );
    T* GetActiveItem( void );

    void Free( void );

protected:

    int			m_NumItems;         // the number of items in the list
    int			m_ActiveItem;       // the active item index
	int			m_CurrentItem;
    int			m_ListSize;         // size of the list
    T			*m_pList;           // the active item list
    bool		*m_pEmptyList;		// keep an empty list
};

//=============================================================================


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> ActiveItemList<T>::ActiveItemList()
{
    m_NumItems = 0;
    m_ActiveItem = -1;
	m_CurrentItem = -1;
    m_ListSize = 0;
    m_pList = NULL;
    m_pEmptyList = NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> ActiveItemList<T>::ActiveItemList( int size )
{
    int     i;      // loop counter

    // set the size of the list
    m_ListSize = size;

    //
    // allocate memory for the list
    //
    if( !( m_pList = new T[size] ) )
        return;

    if( !( m_pEmptyList = new bool[size] ) )
        return;

    //
    // initialize the active item list
    //
    m_NumItems = 0;
    m_ActiveItem = -1;
	m_CurrentItem = -1;

    for( i = 0; i < size; i++ )
        m_pEmptyList[i] = true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> void ActiveItemList<T>::SetSize( int size )
{
    int     i;      // loop counter
    
    // set the size of the list
    m_ListSize = size;

    //
    // allocate memory for the list
    //
    if( !( m_pList = new T[size] ) )
        return;

    if( !( m_pEmptyList = new bool[size] ) )
        return;
    
    //
    // initialize the active item list
    //
    m_NumItems = 0;
    m_ActiveItem = -1;
	m_CurrentItem = -1;

    for( i = 0; i < size; i++ )
        m_pEmptyList[i] = true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> int ActiveItemList<T>::GetSize( void )
{
    return m_ListSize;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> int ActiveItemList<T>::GetNumberOfItems( void )
{
    return m_NumItems;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> T* ActiveItemList<T>::GetFirstItem( void )
{
	int		i;		// loop counter

	// reset current item index
	m_CurrentItem = -1;

	//
	// find the first item in the list
	//
	for( i = 0; i < m_ListSize; i++ )
	{
		if( !m_pEmptyList[i] )
		{
			m_CurrentItem = i;
			return &m_pList[i];
		}
	}

	// no items found
	return NULL;
}

	
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> T* ActiveItemList<T>::GetNextItem( void )
{
	int		i;		// loop counter

	//
	// find the next item in the list
	//
	for( i = m_CurrentItem + 1; i < m_ListSize; i++ )
	{
		if( !m_pEmptyList[i] )
		{
			m_CurrentItem = i;
			return &m_pList[i];
		}
	}

	// no more items found
	return NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> ITEM_HANDLE ActiveItemList<T>::GetEmptyItemHandle( void )
{
    int     i;          // loop counter

    //
    // find an empty item slot and return the handle
    //
    for( i = 0; i < m_ListSize; i++ )
    {
        if( m_pEmptyList[i] )
        {
            m_pEmptyList[i] = false;
            m_NumItems++;
            return i;
        }
    }

    // no empty item slot
    return -1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> T* ActiveItemList<T>::GetItem( ITEM_HANDLE handle )
{
    return &m_pList[handle];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> void ActiveItemList<T>::RemoveItem( ITEM_HANDLE handle )
{
    //
    // set the item to empty and decrement the number of items in list
    //
    m_pEmptyList[handle] = true;
    m_NumItems--;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> void ActiveItemList<T>::SetActiveItem( ITEM_HANDLE handle )
{
    // set the active item
    m_ActiveItem = handle;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> T* ActiveItemList<T>::GetActiveItem( void )
{
    if( m_ActiveItem == -1 )
        return NULL;

    return &m_pList[m_ActiveItem];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class T> void ActiveItemList<T>::Free( void )
{
    //
    // clean up lists
    //
    if( m_pList )
        delete [] m_pList;

    if( m_pEmptyList )
        delete [] m_pEmptyList;
}


#endif // ACTIVEITEMLIST_H