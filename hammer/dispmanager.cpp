//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "UtlLinkedList.h"
//#include "DispManager.h"
#include "MapFace.h"
#include "MapDisp.h"
#include "DispSubdiv.h"
#include "History.h"
#include "tier0/minidump.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//=============================================================================
//
// Global Displacement Manager
//
class CEditDispMgr : public IEditDispMgr
{
public: // functions

	CEditDispMgr();
	virtual ~CEditDispMgr();

	EditDispHandle_t Create( void );
	void Destroy( EditDispHandle_t handle );

	CMapDisp *GetDisp( EditDispHandle_t handle );

private: // variables

	CUtlLinkedList<CMapDisp, EditDispHandle_t>	m_AllocList;
};



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEditDispMgr* EditDispMgr( void )
{
	static CEditDispMgr s_EditDispMgr;
	return &s_EditDispMgr;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditDispMgr::CEditDispMgr()
{
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditDispMgr::~CEditDispMgr()
{
	m_AllocList.Purge();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
EditDispHandle_t CEditDispMgr::Create( void )
{
	EditDispHandle_t handle = m_AllocList.AddToTail();
	if( handle != EDITDISPHANDLE_INVALID )
	{
		CMapDisp *pDisp = &m_AllocList.Element( handle );
		pDisp->SetEditHandle( handle );
	}

	return handle;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispMgr::Destroy( EditDispHandle_t handle )
{
	if ( m_AllocList.IsValidIndex( handle ) )
	{
		m_AllocList.Remove( handle );
	}
	else
	{
		static bool bNoToAll = false;
		if ( !bNoToAll )
		{
			int result = AfxMessageBox( 
				"CEditDispMgr::Destroy - invalid handle.\n"
				"Write minidump?\n",
				MB_YESNO );
			
			if ( result == IDYES )
			{
				// Generate a minidump.
				WriteMiniDump();
			}
			else
			{
				bNoToAll = true;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapDisp *CEditDispMgr::GetDisp( EditDispHandle_t handle )
{
	if( m_AllocList.IsValidIndex( handle ) )
	{
		return &m_AllocList.Element( handle );
	}

	return NULL;
}


//=============================================================================
//
// World Displacement Manager(s)
//
class CWorldEditDispMgr : public IWorldEditDispMgr
{
public: // functions

	// construction/deconstruction
	CWorldEditDispMgr();
	virtual ~CWorldEditDispMgr();

	// world list functionals
	int WorldCount( void );
	CMapDisp *GetFromWorld( int iWorldList );
	CMapDisp *GetFromWorld( EditDispHandle_t handle );

	void AddToWorld( EditDispHandle_t handle );	
	void RemoveFromWorld( EditDispHandle_t handle );
	
	void FindWorldNeighbors( EditDispHandle_t handle );

	// selection list functions
	int SelectCount( void );
	void SelectClear( void );
	CMapDisp *GetFromSelect( int iSelectList );

	void AddToSelect( EditDispHandle_t handle );
	void RemoveFromSelect( EditDispHandle_t handle );
	bool IsInSelect( EditDispHandle_t handle );

	void CatmullClarkSubdivide( void );

	void PreUndo( const char *pszMarkName );
	void Undo( EditDispHandle_t handle, bool bAddNeighbors );
	void PostUndo( void );

	virtual int NumSharedPoints( CMapDisp *pDisp, CMapDisp *pNeighborDisp, int *edge1, int *edge2 );

private: // functions

	void TestNeighbors( CMapDisp *pDisp, CMapDisp *pNeighborDisp );
	int GetCornerIndex( int index );
	int GetEdgeIndex( int *edge );

	bool IsInKeptList( CMapClass *pObject );

private: // variables

	CUtlVector<EditDispHandle_t>	m_WorldList;
	CUtlVector<EditDispHandle_t>	m_SelectList;

	IEditDispSubdivMesh				*m_pSubdivMesh;			// pointer to the subdivision mesh

	CUtlVector<CMapClass*>			m_aKeptList;
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IWorldEditDispMgr *CreateWorldEditDispMgr( void )
{
	return new CWorldEditDispMgr;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void DestroyWorldEditDispMgr( IWorldEditDispMgr **pDispMgr )
{
	if( *pDispMgr )
	{
		delete *pDispMgr;
		*pDispMgr = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CWorldEditDispMgr::CWorldEditDispMgr()
{
	// allocate the subdivision mesh
	m_pSubdivMesh = CreateEditDispSubdivMesh();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CWorldEditDispMgr::~CWorldEditDispMgr()
{
	// clear the displacement manager lists
	m_WorldList.Purge();
	m_SelectList.Purge();

	// de-allocate the subdivision mesh
	DestroyEditDispSubdivMesh( &m_pSubdivMesh );

	m_aKeptList.Purge();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CWorldEditDispMgr::WorldCount( void )
{
	return m_WorldList.Count();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapDisp *CWorldEditDispMgr::GetFromWorld( int iWorldList )
{
	// no assert because the .Element( ) takes care of that!
	EditDispHandle_t handle = m_WorldList.Element( iWorldList );
	return EditDispMgr()->GetDisp( handle );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapDisp *CWorldEditDispMgr::GetFromWorld( EditDispHandle_t handle )
{
	int ndx = m_WorldList.Find( handle );
	if( ndx != -1 )
	{
		return EditDispMgr()->GetDisp( handle );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::AddToWorld( EditDispHandle_t handle )
{
	int ndx = m_WorldList.Find( handle );
	if( ndx == -1 )
	{
		ndx = m_WorldList.AddToTail();
		m_WorldList[ndx] = handle;
	}

	// Update itself when it gets added to the world.
	CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
	if ( pDisp )
	{
		pDisp->UpdateData();
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::RemoveFromWorld( EditDispHandle_t handle )
{
	int ndx = m_WorldList.Find( handle );
	if( ndx != -1 )
	{
		m_WorldList.Remove( ndx );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
// NOTE: this will be in the common code soon!!!!!!!!!
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::FindWorldNeighbors( EditDispHandle_t handle )
{
	// get the current displacement
	CMapDisp *pDisp = GetFromWorld( handle );
	if( !pDisp )
		return;

	//
	// compare against all of the displacements in the world
	//
	int count = WorldCount();
	for( int ndx = 0; ndx < count; ndx++ )
	{
		// get the potential neighbor surface
		CMapDisp *pNeighborDisp = GetFromWorld( ndx );

		// check for valid neighbor and don't compare against self
		if( !pNeighborDisp || ( pNeighborDisp == pDisp ) )
			continue;

		// displacements at different resolutions are not considered neighbors
		// regardless of edge connectivity
		if( pDisp->GetPower() != pNeighborDisp->GetPower() )
			continue;

		// test for neighboring edge/corner properties
		TestNeighbors( pDisp, pNeighborDisp );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::TestNeighbors( CMapDisp *pDisp, CMapDisp *pNeighborDisp )
{
	//
	// find the number of shared points between the two displacements (corners, edges)
	// NOTE: should use only 2, but face may be right on top of one another
	//
	int edge1[4], edge2[4];
	int sharedPointCount = NumSharedPoints( pDisp, pNeighborDisp, edge1, edge2 );

	//
	// set the neighboring info
	//
	if( sharedPointCount == 1 )
	{
		int cornerIndex = GetCornerIndex( edge1[0] );
		int neighborCornerIndex = GetCornerIndex( edge2[0] );

		if ( ( cornerIndex != -1 ) && ( neighborCornerIndex != -1 ) )
		{
			CMapFace *pNeighborFace = ( CMapFace* )pNeighborDisp->GetParent();
			pDisp->AddCornerNeighbor( cornerIndex, pNeighborFace->GetDisp(), neighborCornerIndex );
		}
	}
	else if( sharedPointCount == 2 )
	{
		//
		// get edge indices
		//
		int edgeIndex = GetEdgeIndex( edge1 );
		int neighborEdgeIndex = GetEdgeIndex( edge2 );

		if ( ( edgeIndex != -1 ) && ( neighborEdgeIndex != -1 ) )
		{
			CMapFace *pNeighborFace = ( CMapFace* )pNeighborDisp->GetParent();
			pDisp->SetEdgeNeighbor( edgeIndex, pNeighborFace->GetDisp(), neighborEdgeIndex );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool ComparePoints( const Vector& v1, const Vector& v2, float tolerance )
{
	for( int axis = 0; axis < 3; axis++ )
	{
		if( fabs( v1[axis] - v2[axis] ) > tolerance )
			return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CWorldEditDispMgr::NumSharedPoints( CMapDisp *pDisp, CMapDisp *pNeighborDisp,
								      int *edge1, int *edge2 )
{
	int ptCount = 0;

	for( int i = 0; i < 4; i++ )
	{
		int j;
		for( j = 0; j < 4; j++ )
		{
			Vector pt1, pt2;
			pDisp->GetSurfPoint( i, pt1 );
			pNeighborDisp->GetSurfPoint( j, pt2 );
			if( ComparePoints( pt1, pt2, 0.01f ) )
				break;
		}

		if( j == 4 )
			continue;

		edge1[ptCount] = i;
		edge2[ptCount] = j;
		ptCount++;
	}

	return ptCount;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CWorldEditDispMgr::GetCornerIndex( int index )
{
	switch( index )
	{
	case 0: return 0;
	case 1: return 2;
	case 2: return 3;
	case 3: return 1;
	default: return -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CWorldEditDispMgr::GetEdgeIndex( int *edge )
{
	if( ( edge[0] == 0 && edge[1] == 1 ) || ( edge[0] == 1 && edge[1] == 0 ) )
		return 0;

    if( ( edge[0] == 1 && edge[1] == 2 ) || ( edge[0] == 2 && edge[1] == 1 ) )
        return 1;
    
    if( ( edge[0] == 2 && edge[1] == 3 ) || ( edge[0] == 3 && edge[1] == 2 ) )
        return 2;

    if( ( edge[0] == 3 && edge[1] == 0 ) || ( edge[0] == 0 && edge[1] == 3 ) )
        return 3;

    return -1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CWorldEditDispMgr::SelectCount( void )
{
	return m_SelectList.Count();
}

	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::SelectClear( void )
{
	m_SelectList.RemoveAll();
}

	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapDisp *CWorldEditDispMgr::GetFromSelect( int iSelectList )
{
	// no assert because the .Element( ) takes care of that!
	EditDispHandle_t handle = m_SelectList.Element( iSelectList );
	return EditDispMgr()->GetDisp( handle );	
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::AddToSelect( EditDispHandle_t handle )
{
	int ndx = m_SelectList.Find( handle );
	if( ndx == -1 )
	{
		ndx = m_SelectList.AddToTail();
		m_SelectList[ndx] = handle;
	}
}
	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::RemoveFromSelect( EditDispHandle_t handle )
{
	int ndx = m_SelectList.Find( handle );
	if( ndx != -1 )
	{
		m_SelectList.Remove( handle );
	}
}

	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWorldEditDispMgr::IsInSelect( EditDispHandle_t handle )
{
	int ndx = m_SelectList.Find( handle );
	return ( ndx != -1 );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::CatmullClarkSubdivide( void )
{
	// change the mouse to hourglass, so level designers know something is
	// happening
	HCURSOR oldCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

	//
	// add all of the displacements in the selection list into the UNDO
	// system
	//
	PreUndo( "Subdivision" );

	int selectCount = m_SelectList.Count();
	for( int ndxSelect = 0; ndxSelect < selectCount; ndxSelect++ )
	{
		// get the current displacement surface
		CMapDisp *pDisp = GetFromSelect( ndxSelect );
		if( pDisp )
		{
			Undo( pDisp->GetEditHandle(), false );
		}
	}

	PostUndo();

	// initialize the subdivision mesh
	m_pSubdivMesh->Init();
	
	//
	// add all of the displacements in the selection list into the 
	// subdivision mesh
	//
	for( int ndxSelect = 0; ndxSelect < selectCount; ndxSelect++ )
	{
		// get the current displacement surface
		CMapDisp *pDisp = GetFromSelect( ndxSelect );
		if( pDisp )
		{
			m_pSubdivMesh->AddDispTo( pDisp );
		}
	}

	// subdivision
	m_pSubdivMesh->DoCatmullClarkSubdivision();

	//
	// get back subdivided data for all displacement surfaces in the
	// selection list
	//
	for( int ndxSelect = 0; ndxSelect < selectCount; ndxSelect++ )
	{
		// get the current displacement surface
		CMapDisp *pDisp = GetFromSelect( ndxSelect );
		if( pDisp )
		{
			m_pSubdivMesh->GetDispFrom( pDisp );
		}
	}

	m_pSubdivMesh->Shutdown();

	// set the cursor back
	SetCursor( oldCursor );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWorldEditDispMgr::IsInKeptList( CMapClass *pObject )
{
	if ( m_aKeptList.Find( pObject ) == -1 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::PreUndo( const char *pszMarkName )
{
	GetHistory()->MarkUndoPosition( NULL, pszMarkName );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::Undo( EditDispHandle_t hDisp, bool bAddNeighbors )
{
	// Check the handle.
	Assert( hDisp != EDITDISPHANDLE_INVALID );
	if( hDisp == EDITDISPHANDLE_INVALID )
		return;

	// Get the map class object that contains the displacement surface.
	CMapDisp *pDisp = EditDispMgr()->GetDisp( hDisp );
	if ( !pDisp )
		return;

	CMapFace *pFace = ( CMapFace* )pDisp->GetParent();
	CMapSolid *pSolid = ( CMapSolid* )pFace->GetParent();
	CMapClass *pObject = ( CMapClass* )pSolid;
	if ( !pObject )
		return;

	// Keep the map class object for undo.
	if ( !IsInKeptList( pObject ) )
	{
		m_aKeptList.AddToTail( pObject );
		GetHistory()->Keep( pObject );
	}

	// Keep the map class (displacement parent) neighbor objects for undo.
	if ( bAddNeighbors )
	{
		int					nNeighborOrient;
		EditDispHandle_t	hNeighbor;

		for ( int iNeighbor = 0; iNeighbor < 4; ++iNeighbor )
		{
			pDisp = EditDispMgr()->GetDisp( hDisp );
			if ( pDisp )
			{
				//
				// Edge Neighbors.
				//
				pDisp->GetEdgeNeighbor( iNeighbor, hNeighbor, nNeighborOrient );
				if( hNeighbor != EDITDISPHANDLE_INVALID )
				{
					CMapDisp *pNeighborDisp = EditDispMgr()->GetDisp( hNeighbor );
					CMapFace *pNeighborFace = ( CMapFace* )pNeighborDisp->GetParent();
					CMapSolid *pNeighborSolid = ( CMapSolid* )pNeighborFace->GetParent();
					CMapClass *pNeighborObject = ( CMapClass* )pNeighborSolid;
					if ( !IsInKeptList( pNeighborObject ) )
					{
						m_aKeptList.AddToTail( pNeighborObject );
						GetHistory()->Keep( pNeighborObject );
					}					
				}
			}
			
			pDisp = EditDispMgr()->GetDisp( hDisp );
			if ( pDisp )
			{
				//
				// Corner Neighbors.
				//
				int nCornerCount = pDisp->GetCornerNeighborCount( iNeighbor );
				for( int iCorner = 0; iCorner < nCornerCount; ++iCorner )
				{
					pDisp = EditDispMgr()->GetDisp( hDisp );
					if ( pDisp )
					{
						pDisp->GetCornerNeighbor( iNeighbor, iCorner, hNeighbor, nNeighborOrient );
						
						CMapDisp *pNeighborDisp = EditDispMgr()->GetDisp( hNeighbor );
						if ( pNeighborDisp )
						{
							CMapFace *pNeighborFace = ( CMapFace* )pNeighborDisp->GetParent();
							CMapSolid *pNeighborSolid = ( CMapSolid* )pNeighborFace->GetParent();
							CMapClass *pNeighborObject = ( CMapClass* )pNeighborSolid;	
							if ( !IsInKeptList( pNeighborObject ) )
							{
								m_aKeptList.AddToTail( pNeighborObject );
								GetHistory()->Keep( pNeighborObject );
							}
						}
					}
				}
			}			
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWorldEditDispMgr::PostUndo( void )
{
	// Clear the kept list.
	m_aKeptList.RemoveAll();
}
