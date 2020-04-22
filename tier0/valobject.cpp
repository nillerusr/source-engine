//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"
#include "vstdlib/pch_vstdlib.h"

#ifdef DBGFLAG_VALIDATE

//-----------------------------------------------------------------------------
// Purpose: Initializer
// Input:	pchType -			Type of the object we represent.
//								WARNING: pchType must be a static (since we keep a copy of it around for a while)
//			pvObj -				Pointer to the object we represent
//			pchName -			Name of the individual object we represent
//								WARNING: pchName must be a static (since we keep a copy of it around for a while)
//			pValObjectparent-	Our parent object (ie, the object that our object is a member of)
//			pValObjectPrev -	Object that precedes us in the linked list (we're
//								always added to the end)
//-----------------------------------------------------------------------------
void CValObject::Init( tchar *pchType, void *pvObj, tchar *pchName, 
					   CValObject *pValObjectParent, CValObject *pValObjectPrev )
{
	m_nUser = 0;

	// Initialize pchType:
	if ( NULL != pchType )
	{
		Q_strncpy( m_rgchType, pchType, (int) ( sizeof(m_rgchType) / sizeof(*m_rgchType) ) );
	}
	else
	{
		m_rgchType[0] = '\0';
	}

	m_pvObj = pvObj;
	
	// Initialize pchName: 
	if ( NULL != pchName )
	{
		Q_strncpy( m_rgchName, pchName, sizeof(m_rgchName) / sizeof(*m_rgchName) );
	}
	else
	{
		m_rgchName[0] = NULL;
	}

	m_pValObjectParent = pValObjectParent;

	if ( NULL == pValObjectParent )
		m_nLevel = 0;
	else
		m_nLevel = pValObjectParent->NLevel( ) + 1;

	m_cpubMemSelf = 0;
	m_cubMemSelf = 0;
	m_cpubMemTree = 0;
	m_cubMemTree = 0;

	// Insert us at the back of the linked list
	if ( NULL != pValObjectPrev )
	{
		Assert( NULL == pValObjectPrev->m_pValObjectNext );
		pValObjectPrev->m_pValObjectNext = this;
	}
	m_pValObjectNext = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CValObject::~CValObject( )
{
}


//-----------------------------------------------------------------------------
// Purpose: The object we represent has claimed direct ownership of a block of
//			memory.  Record that we own it.
// Input:	pvMem -			Address of the memory block
//-----------------------------------------------------------------------------
void CValObject::ClaimMemoryBlock( void *pvMem )
{
	// Get the memory block header
	CMemBlockHdr *pMemBlockHdr = CMemBlockHdr::PMemBlockHdrFromPvUser( pvMem );
	pMemBlockHdr->CheckValid( );

	// Update our counters
	m_cpubMemSelf++;
	m_cubMemSelf+= pMemBlockHdr->CubUser( );
	m_cpubMemTree++;
	m_cubMemTree+= pMemBlockHdr->CubUser( );

	// If we have a parent object, let it know about the memory (it'll recursively call up the tree)
	if ( NULL != m_pValObjectParent )
		m_pValObjectParent->ClaimChildMemoryBlock( pMemBlockHdr->CubUser( ) );
}


//-----------------------------------------------------------------------------
// Purpose: A child of ours has claimed ownership of a memory block.  Make
//			a note of it, and pass the message back up the tree.
// Input:	cubUser -			Size of the memory block
//-----------------------------------------------------------------------------
void CValObject::ClaimChildMemoryBlock( int cubUser )
{
	m_cpubMemTree++;
	m_cubMemTree += cubUser;

	if ( NULL != m_pValObjectParent )
		m_pValObjectParent->ClaimChildMemoryBlock( cubUser );
}



#endif // DBGFLAG_VALIDATE