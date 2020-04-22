//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#include "pch_tier0.h"

#include "tier0/memblockhdr.h"


#ifdef DBGFLAG_VALIDATE

// we use malloc & free internally in our validation code; turn off the deprecation #defines
#undef malloc
#undef free

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CValidator::CValidator( )
{
	m_pValObjectFirst = NULL;
	m_pValObjectLast = NULL;
	m_pValObjectCur = NULL;
	m_cpvOwned = 0;
	m_bMemLeaks = false;

	// Mark all memory blocks as unclaimed, prior to starting the validation process
	CMemBlockHdr *pMemBlockHdr = CMemBlockHdr::PMemBlockHdrFirst( );
	pMemBlockHdr = pMemBlockHdr->PMemBlockHdrNext( );	// Head is just a placeholder
	while ( NULL != pMemBlockHdr )
	{
		pMemBlockHdr->SetBClaimed( false );

		pMemBlockHdr = pMemBlockHdr->PMemBlockHdrNext( );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CValidator::~CValidator( )
{
	CValObject *pValObject = m_pValObjectFirst;
	CValObject *pValObjectNext;
	while ( NULL != pValObject )
	{
		pValObjectNext = pValObject->PValObjectNext( );
		Destruct<CValObject> (pValObject);
		free( pValObject );
		pValObject = pValObjectNext;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Call this each time you start a new Validate() function.  It creates
//			a new CValObject to track the caller.
// Input:	pchType -		The caller's type (typically a class name)
//			pvObj -			The caller (typically an object pointer)
//			pchName -		The caller's individual name (typically a member var of another class)
//-----------------------------------------------------------------------------
void CValidator::Push( tchar *pchType, void *pvObj, tchar *pchName )
{
	// Create a new ValObject and add it to the linked list

	CValObject *pValObjectNew = (CValObject *) malloc( sizeof (CValObject ) );
	Construct<CValObject> (pValObjectNew);
	pValObjectNew->Init( pchType, pvObj, pchName, m_pValObjectCur, m_pValObjectLast );
	m_pValObjectLast = pValObjectNew;
	if ( NULL == m_pValObjectFirst )
		m_pValObjectFirst = pValObjectNew;

	// Make this the current object
	m_pValObjectCur = pValObjectNew;
}


//-----------------------------------------------------------------------------
// Purpose: Call this each time you end a Validate() function.  It decrements 
//			our current structure depth.
//-----------------------------------------------------------------------------
void CValidator::Pop( )
{
	Assert( NULL != m_pValObjectCur );
	m_pValObjectCur = m_pValObjectCur->PValObjectParent( );
}


//-----------------------------------------------------------------------------
// Purpose: Call this to register each memory block you own.
// Input:	pvMem -		Memory block you own
//-----------------------------------------------------------------------------
void CValidator::ClaimMemory( void *pvMem )
{
	if ( NULL == pvMem )
		return;

	// Mark the block as owned
	CMemBlockHdr *pMemBlockHdr = CMemBlockHdr::PMemBlockHdrFromPvUser( pvMem );
	pMemBlockHdr->CheckValid( );
	Assert( !pMemBlockHdr->BClaimed( ) );
	pMemBlockHdr->SetBClaimed( true );

	// Let the current object know about it
	Assert( NULL != m_pValObjectCur );
	m_pValObjectCur->ClaimMemoryBlock( pvMem );

	// Update our counter
	m_cpvOwned++;
}


//-----------------------------------------------------------------------------
// Purpose: We're done enumerating our objects.  Perform any final calculations.
//-----------------------------------------------------------------------------
void CValidator::Finalize( void )
{
	// Count our memory leaks
	CMemBlockHdr *pMemBlockHdr = CMemBlockHdr::PMemBlockHdrFirst( );
	pMemBlockHdr = pMemBlockHdr->PMemBlockHdrNext( );
	m_cpubLeaked = 0;
	m_cubLeaked = 0;
	while ( NULL != pMemBlockHdr )
	{
		if ( !pMemBlockHdr->BClaimed( ) )
		{
			m_cpubLeaked++;
			m_cubLeaked += pMemBlockHdr->CubUser( );
			m_bMemLeaks = true;
		}

		pMemBlockHdr = pMemBlockHdr->PMemBlockHdrNext( );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Render all reported objects to the console
// Input:	cubThreshold -		Only render object whose children have at least
//								cubThreshold bytes allocated
//-----------------------------------------------------------------------------
void CValidator::RenderObjects( int cubThreshold )
{
	// Walk our object list and render them all to the console
	CValObject *pValObject = m_pValObjectFirst;
	while ( NULL != pValObject )
	{
		if ( pValObject->CubMemTree( ) >= cubThreshold )
		{
			for ( int ich = 0; ich < pValObject->NLevel( ); ich++ )
				ConMsg( 2, _T(" ") );

			ConMsg( 2, _T("%s at 0x%x--> %d blocks = %d bytes\n"),
				pValObject->PchType( ), pValObject->PvObj( ), pValObject->CpubMemTree( ),
				pValObject->CubMemTree( ) );
		}

		pValObject = pValObject->PValObjectNext( );
	}


	// Dump a summary to the console
	ConMsg( 2, _T("Allocated:\t%d blocks\t%d bytes\n"), CpubAllocated( ), CubAllocated( ) );
}


//-----------------------------------------------------------------------------
// Purpose: Render any discovered memory leaks to the console
//-----------------------------------------------------------------------------
void CValidator::RenderLeaks( void )
{
	if ( m_bMemLeaks )
		ConMsg( 1, _T("\n") );

	// Render any leaked blocks to the console
	CMemBlockHdr *pMemBlockHdr = CMemBlockHdr::PMemBlockHdrFirst( );
	pMemBlockHdr = pMemBlockHdr->PMemBlockHdrNext( );
	while ( NULL != pMemBlockHdr )
	{
		if ( !pMemBlockHdr->BClaimed( ) )
		{
			ConMsg( 1, _T("Leaked mem block: Addr = 0x%x\tSize = %d\n"), 
				pMemBlockHdr->PvUser( ), pMemBlockHdr->CubUser( ) );
			ConMsg( 1, _T("\tAlloc = %s, line %d\n"),
				pMemBlockHdr->PchFile( ), pMemBlockHdr->NLine( ) );
		}

		pMemBlockHdr = pMemBlockHdr->PMemBlockHdrNext( );
	}

	// Dump a summary to the console
	if ( 0 != m_cpubLeaked )
		ConMsg( 1, _T("!!!Leaked:\t%d blocks\t%d bytes\n"), m_cpubLeaked, m_cubLeaked );
}

//-----------------------------------------------------------------------------
// Purpose: Find the validator object associated with the given real object.
//-----------------------------------------------------------------------------
CValObject *CValidator::FindObject( void * pvObj )
{
	CValObject *pValObject = m_pValObjectFirst;
	CValObject *pValObjectNext;
	while ( NULL != pValObject )
	{
		pValObjectNext = pValObject->PValObjectNext( );
		if( pvObj == pValObject->PvObj() )
			return pValObject;

		pValObject = pValObjectNext;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Diff one CValidator against another.  Each Validator object is 
// tagged with whether it is new since the last snapshot or not.
//-----------------------------------------------------------------------------
void CValidator::DiffAgainst( CValidator *pOtherValidator )	// Removes any entries from this validator that are also present in the other.
{
	// Render any leaked blocks to the console
	CValObject *pValObject = m_pValObjectFirst;
	CValObject *pValObjectNext;
	while ( NULL != pValObject )
	{
		pValObjectNext = pValObject->PValObjectNext( );
		pValObject->SetBNewSinceSnapshot( pOtherValidator->FindObject( pValObject->PvObj() ) == NULL );
		
		if( pValObject->BNewSinceSnapshot() && pValObject->CubMemTree( ) )
		{
			for ( int ich = 0; ich < pValObject->NLevel( ); ich++ )
				ConMsg( 2, _T(" ") );

			ConMsg( 2, _T("%s at 0x%x--> %d blocks = %d bytes\n"),
				pValObject->PchType( ), pValObject->PvObj( ), pValObject->CpubMemTree( ),
				pValObject->CubMemTree( ) );
		}

		pValObject = pValObjectNext;
	}

}

void CValidator::Validate( CValidator &validator, tchar *pchName )
{
	validator.Push( _T("CValidator"), this, pchName );

	validator.ClaimMemory( this );

	// Render any leaked blocks to the console
	CValObject *pValObject = m_pValObjectFirst;
	CValObject *pValObjectNext;
	while ( NULL != pValObject )
	{
		pValObjectNext = pValObject->PValObjectNext( );
		validator.ClaimMemory( pValObject );
		pValObject = pValObjectNext;
	}
	
	validator.Pop();
}

#endif // DBGFLAG_VALIDATE