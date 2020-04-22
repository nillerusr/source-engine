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

//=============================================================================
//
// NOTE: the painting code in here needs to be cleaned up and a new algorithm
//       is needed for handling valence greater than 4 cases (I am not too happy
//       with the current one)
//

#include <stdafx.h>
#include "MapDisp.h"
#include "DispSew.h"
#include "ChunkFile.h"
#include "GlobalFunctions.h"
#include "ToolDisplace.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define NULL_VALUE   -99999.0f

//=============================================================================
//
// Displacement Image Filter Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CDispMapImageFilter::CDispMapImageFilter()
{
	m_Type = (unsigned int)-1;
	m_DataType = -1;

	m_Height = 0;
	m_Width = 0;
	m_pImage = NULL;

	m_Scale = 1.0f;
	m_AreaHeight = 0;
	m_AreaWidth = 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CDispMapImageFilter::~CDispMapImageFilter()
{
	// de-allocate displacement image memory
	if( m_pImage )
	{
		delete [] m_pImage;
		m_pImage = NULL;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilter::GetFilterType( CString type )
{
	if( type == "FILTER_ADD" ) { return DISPPAINT_EFFECT_RAISELOWER; }
	if( type == "FILTER_MULT" ) { return DISPPAINT_EFFECT_MODULATE; }
	if( type == "FILTER_CONVATTEN" ) { return DISPPAINT_EFFECT_SMOOTH; }
	if( type == "FILTER_EQUAL" ) { return DISPPAINT_EFFECT_RAISETO; }

	return -1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CDispMapImageFilter::LoadImageCallback( CChunkFile *pFile, 
														  CDispMapImageFilter *pFilter )
{
	return( pFile->ReadChunk( ( KeyHandler_t )LoadImageKeyCallback, pFilter ) );
}

static bool bInitMemory = true;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CDispMapImageFilter::LoadImageKeyCallback( const char *szKey, const char *szValue,
															 CDispMapImageFilter *pFilter )
{
	//
	// allocate filter image memory
	//
	if( bInitMemory )
	{
		int size = pFilter->m_Height * pFilter->m_Width;
		pFilter->m_pImage = new float[size+1];
		if( !pFilter->m_pImage )
			return( ChunkFile_Fail );

		bInitMemory = false;
	}

	if( !strnicmp( szKey, "row", 3 ) )
	{
		char szBuf[MAX_KEYVALUE_LEN];
		strcpy( szBuf, szValue );

		int row = atoi( &szKey[3] );

		char *pszNext = strtok( szBuf, " " );

		int ndx = row * pFilter->m_Height;
		while( pszNext != NULL )
		{
			float imageValue = ( float )atof( pszNext );
			pFilter->m_pImage[ndx] = imageValue;
			pszNext = strtok( NULL, " " );
			ndx++;
		}
	}

	return( ChunkFile_Ok );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilter::ValidHeight( int height )
{
	if( ( height < 1 ) || ( height > 9 ) )
	{
		Msg( mwError, "Filter height is out of range - %d\n", height );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilter::ValidWidth( int width )
{
	if( ( width < 1 ) || ( width > 9 ) )
	{
		Msg( mwError, "Filter width is out of range - %d\n", width );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CDispMapImageFilter::LoadFilterKeyCallback( const char *szKey, const char *szValue,
															  CDispMapImageFilter *pFilter )
{
	if( !stricmp( szKey, "Height" ) )
	{
		CChunkFile::ReadKeyValueInt( szValue, pFilter->m_Height );
		ValidHeight( pFilter->m_Height );
	}
	else if( !stricmp( szKey, "Width" ) )
	{
		CChunkFile::ReadKeyValueInt( szValue, pFilter->m_Width );
		ValidWidth( pFilter->m_Width );
	}
	else if( !stricmp( szKey, "FilterType" ) )
	{
		CString strFilterType = szValue;
		pFilter->m_Type = GetFilterType( strFilterType );
	}
	else if( !stricmp( szKey, "IconName" ) )
	{
		pFilter->m_Name = szValue;
	}

	return( ChunkFile_Ok );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ChunkFileResult_t CDispMapImageFilter::LoadFilter( CChunkFile *pFile )
{
	bInitMemory = true;

	CChunkHandlerMap Handlers;
	Handlers.AddHandler( "Image", ( ChunkHandler_t )LoadImageCallback, this );

	pFile->PushHandlers( &Handlers );
	ChunkFileResult_t eResult = pFile->ReadChunk( ( KeyHandler_t )LoadFilterKeyCallback, this );
	pFile->PopHandlers();

	return( eResult );
}


//=============================================================================
//
// Displacement Filter Manager Functions
//


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CDispMapImageFilterManager::CDispMapImageFilterManager()
{
	m_FilterCount = 0;
	m_ActiveFilter = 0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::AddFilterToList( CDispMapImageFilter *pFilter )
{
	// don't allow overflow! -- should be an error message here!!!
	if( m_FilterCount >= FILTERLIST_SIZE )
		return;

	m_pFilterList[m_FilterCount] = pFilter;
	m_FilterCount++;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CDispMapImageFilter *CDispMapImageFilterManager::Create( void )
{
	// allocate filter
	CDispMapImageFilter *pFilter = new CDispMapImageFilter;
	if( !pFilter )
		return NULL;

	// add filter to list
	AddFilterToList( pFilter );

	return pFilter;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::Add( CDispMapImageFilter *pFilter )
{
	AddFilterToList( pFilter );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::Destroy( void )
{
	for( int i = 0; i < m_FilterCount; i++ )
	{
		// get the current filter
		CDispMapImageFilter *pFilter = GetFilter( i );
		if( !pFilter )
			continue;

		delete pFilter;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispMapImageFilterManager::SetImageValue( CMapDisp *pDisp, CDispMapImageFilter *pFilter,
													   int ndxDisp, Vector &vPaintValue )
{
	pDisp->Paint_SetValue( ndxDisp, vPaintValue );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispMapImageFilterManager::GetImageValue( CMapDisp *pDisp, CDispMapImageFilter *pFilter,
													   int ndxDisp, Vector &vPaintValue )
{
	if( pFilter->m_DataType == DISPPAINT_CHANNEL_POSITION )
	{
		pDisp->GetVert( ndxDisp, vPaintValue );
	}
	else if( pFilter->m_DataType == DISPPAINT_CHANNEL_ALPHA )
	{
		float alpha = pDisp->GetAlpha( ndxDisp );
		vPaintValue.Init( alpha, 0.0f, 0.0f );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispMapImageFilterManager::GetImageFlatSubdivValue( CMapDisp *pDisp, CDispMapImageFilter *pFilter,
													             int ndxDisp, Vector &vPaintValue )
{
	if( pFilter->m_DataType == DISPPAINT_CHANNEL_POSITION )
	{
		Vector vSPos;
		pDisp->GetFlatVert( ndxDisp, vPaintValue );
		pDisp->GetSubdivPosition( ndxDisp, vSPos );
		vPaintValue += vSPos;
	}
	else if( pFilter->m_DataType == DISPPAINT_CHANNEL_ALPHA )
	{
		vPaintValue.Init( 0.0f, 0.0f, 0.0f );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispMapImageFilterManager::GetImageFieldData( CMapDisp *pDisp, CDispMapImageFilter *pFilter,
														   int ndxDisp, Vector &vNormal, float &dist )
{
	if( pFilter->m_DataType == DISPPAINT_CHANNEL_POSITION )
	{
		pDisp->GetFieldVector( ndxDisp, vNormal );
		dist = pDisp->GetFieldDistance( ndxDisp );
	}
	else if( pFilter->m_DataType == DISPPAINT_CHANNEL_ALPHA )
	{
		vNormal.Init( 0.0f, 0.0f, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::PreApply( CDispMapImageFilter *pFilter,
										   int nPaintDirType, const Vector &vecPaintDir )
{
	// Save the paint type and direction locally.
	m_PaintType = nPaintDirType;
	m_PaintDir = vecPaintDir;

	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return false;

	// Get the displacements in the selection set.
	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			pDisp->Paint_Init( pFilter->m_DataType );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::PostApply( bool bSew )
{
	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return false;

	// Get the displacements in the selection set.
	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp )
		{
			pDisp->Paint_Update( false );
		}
	}

	// Sew all surfaces post painting.
	if( bSew )
	{
		FaceListSewEdges();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::Apply( CDispMapImageFilter *pFilter, CMapDisp *pDisp, 
									    int paintDirType, Vector const &vPaintDir, bool bSew )
{
	// Get the index of the vertex on the displacement surface "hit."
	int iVert = pDisp->GetTexelHitIndex();
	if( iVert == -1 )
		return false;

	if ( !PreApply( pFilter, paintDirType, vPaintDir ) )
		return false;

	// Apply the filter to the given displacement at the impacted vertex index.
	ApplyAt( pFilter, pDisp, iVert );

	if ( !PostApply( bSew ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::ApplyAt( CDispMapImageFilter *pFilter, CMapDisp *pDisp,
								          int ndxVert )
{
    // Apply filter appropriately type.
    switch( pFilter->m_Type )
    {
    case DISPPAINT_EFFECT_RAISELOWER:
		{
			ApplyAddFilter( pFilter, pDisp, ndxVert );
			return;
		}
    case DISPPAINT_EFFECT_MODULATE:
		{
			ApplyMultFilter( pFilter, pDisp, ndxVert );
		    return;
		}
    case DISPPAINT_EFFECT_SMOOTH:
		{
			ApplySmoothFilter( pFilter, pDisp, ndxVert );
			return;
		}
	case DISPPAINT_EFFECT_RAISETO:
		{
			ApplyEqualFilter( pFilter, pDisp, ndxVert );
			return;
		}
    default:
		{
	        return;
		}
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::IsNeighborInSelectionSet( CMapDisp *pNeighborDisp )
{
	// Get the displacement manager from the active map document.
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return false;

	// Get the displacements in the selection set.
	int nDispCount = pDispMgr->SelectCount();
	for ( int iDisp = 0; iDisp < nDispCount; iDisp++ )
	{
		CMapDisp *pDisp = pDispMgr->GetFromSelect( iDisp );
		if ( pDisp == pNeighborDisp )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::HitData_Init( PosHitData_t &hitData )
{
	hitData.m_CornerCount = 0;
	hitData.m_ndxCorners[0] = hitData.m_ndxCorners[1] = -1;
	hitData.m_EdgeCount = 0;
	hitData.m_ndxEdges[0] = hitData.m_ndxEdges[1] = -1;
	hitData.m_bMain = false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::HitData_Setup( PosHitData_t &hitData, 
											    int ndxHgt, int ndxWid, 
												int imgHgt, int imgWid )
{
	// reset the hit data
	HitData_Init( hitData );

	//
	// corners
	//

	// southwest
	if( ( ndxHgt <= 0.0f ) && ( ndxWid <= 0.0f ) )
	{
		hitData.m_ndxCorners[hitData.m_CornerCount] = IMAGEFILTER_SOUTHWEST;
		hitData.m_CornerCount++;
	}

	// northwest
	if( ( ndxHgt >= ( imgHgt - 1 ) ) && ( ndxWid <= 0.0f ) )
	{
		hitData.m_ndxCorners[hitData.m_CornerCount] = IMAGEFILTER_NORTHWEST;
		hitData.m_CornerCount++;
	}

	// northeast
	if( ( ndxHgt >= ( imgHgt - 1 ) ) && ( ndxWid >= ( imgWid - 1 ) ) )
	{
		hitData.m_ndxCorners[hitData.m_CornerCount] = IMAGEFILTER_NORTHEAST;
		hitData.m_CornerCount++;
	}

	// southeast
	if( ( ndxHgt <= 0.0f ) && ( ndxWid >= ( imgWid - 1 ) ) )
	{
		hitData.m_ndxCorners[hitData.m_CornerCount] = IMAGEFILTER_SOUTHEAST;
		hitData.m_CornerCount++;
	}

	//
	// edges "images"
	//

	// west
	if( ( ndxHgt >= 0.0f ) && ( ndxHgt <= ( imgHgt - 1 ) ) && ( ndxWid <= 0.0f ) )
	{
		hitData.m_ndxEdges[hitData.m_EdgeCount] = IMAGEFILTER_WEST;
		hitData.m_EdgeCount++;
	}

	// north
	if( ( ndxHgt >= ( imgHgt - 1 ) ) && ( ndxWid >= 0.0f ) && ( ndxWid <= ( imgWid - 1 ) ) )
	{
		hitData.m_ndxEdges[hitData.m_EdgeCount] = IMAGEFILTER_NORTH;
		hitData.m_EdgeCount++;
	}

	// east
	if( ( ndxHgt >= 0.0f ) && ( ndxHgt <= ( imgHgt - 1 ) ) && ( ndxWid >= ( imgWid - 1 ) ) )
	{
		hitData.m_ndxEdges[hitData.m_EdgeCount] = IMAGEFILTER_EAST;
		hitData.m_EdgeCount++;
	}

	// south
	if( ( ndxHgt <= 0.0f ) && ( ndxWid >= 0.0f ) && ( ndxWid <= ( imgWid - 1 ) ) )
	{
		hitData.m_ndxEdges[hitData.m_EdgeCount] = IMAGEFILTER_SOUTH;
		hitData.m_EdgeCount++;
	}

	//
	// main "image"
	//
	if( ( ndxHgt >= 0.0f ) && ( ndxHgt < imgHgt ) && ( ndxWid >= 0.0f ) && ( ndxWid < imgWid ) )
	{
		hitData.m_bMain = true;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetCornerImageCount( CMapDisp *pDisp, int ndxCorner )
{
	switch( ndxCorner )
	{
	case IMAGEFILTER_SOUTHWEST: { return pDisp->GetCornerNeighborCount( 0 ); }
	case IMAGEFILTER_NORTHWEST: { return pDisp->GetCornerNeighborCount( 2 ); }
	case IMAGEFILTER_NORTHEAST: { return pDisp->GetCornerNeighborCount( 3 ); }
	case IMAGEFILTER_SOUTHEAST: { return pDisp->GetCornerNeighborCount( 1 ); }
	default: { return -1; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMapDisp *CDispMapImageFilterManager::GetImage( CDispMapImageFilter *pFilter,
											    CMapDisp *pDisp, int ndxHgt, int ndxWid,
												int ndxImg, int imgCount, int &orient )
{
	CMapDisp *pNeighborDisp = NULL;
	EditDispHandle_t neighborHandle;

	switch( ndxImg )
	{
	case IMAGEFILTER_SOUTHWEST: 
		{
			int count = pDisp->GetCornerNeighborCount( 0 );
			if( count != 0 )
			{
				for( int i = 0; i < count; i++ )
				{
					if( i != imgCount )
						continue;

					pDisp->GetCornerNeighbor( 0, i, neighborHandle, orient );
					if( neighborHandle != EDITDISPHANDLE_INVALID )
					{
						pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
					}
					return pNeighborDisp;
				}			
			}

			return NULL;
		}
	case IMAGEFILTER_WEST: 
		{
			pDisp->GetEdgeNeighbor( 0, neighborHandle, orient );
			if( neighborHandle != EDITDISPHANDLE_INVALID )
			{
				pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
			}
			return pNeighborDisp;
		}
	case IMAGEFILTER_NORTHWEST: 
		{ 
			int count = pDisp->GetCornerNeighborCount( 2 );
			if( count != 0 )
			{
				for( int i = 0; i < count; i++ )
				{
					if( i != imgCount )
						continue;

					pDisp->GetCornerNeighbor( 2, i, neighborHandle, orient );
					if( neighborHandle != EDITDISPHANDLE_INVALID )
					{
						pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
					}
					return pNeighborDisp;
				}			
			}

			return NULL;
		}
	case IMAGEFILTER_NORTH: 
		{
			pDisp->GetEdgeNeighbor( 1, neighborHandle, orient );
			if( neighborHandle != EDITDISPHANDLE_INVALID )
			{
				pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
			}
			return pNeighborDisp;
		}
	case IMAGEFILTER_NORTHEAST: 
		{ 
			int count = pDisp->GetCornerNeighborCount( 3 );
			if( count != 0 )
			{
				for( int i = 0; i < count; i++ )
				{
					if( i != imgCount )
						continue;

					pDisp->GetCornerNeighbor( 3, i, neighborHandle, orient );
					if( neighborHandle != EDITDISPHANDLE_INVALID )
					{
						pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
					}
					return pNeighborDisp;
				}			
			}

			return NULL;
		}
	case IMAGEFILTER_EAST: 
		{ 
			pDisp->GetEdgeNeighbor( 2, neighborHandle, orient );
			if( neighborHandle != EDITDISPHANDLE_INVALID )
			{
				pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
			}
			return pNeighborDisp;
		}
	case IMAGEFILTER_SOUTHEAST: 
		{ 
			int count = pDisp->GetCornerNeighborCount( 1 );
			if( count != 0 )
			{
				for( int i = 0; i < count; i++ )
				{
					if( i != imgCount )
						continue;

					pDisp->GetCornerNeighbor( 1, i, neighborHandle, orient );
					if( neighborHandle != EDITDISPHANDLE_INVALID )
					{
						pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
					}
					return pNeighborDisp;
				}			
			}

			return NULL;
		}
	case IMAGEFILTER_SOUTH: 
		{ 
			pDisp->GetEdgeNeighbor( 3, neighborHandle, orient );
			if( neighborHandle != EDITDISPHANDLE_INVALID )
			{
				pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
			}
			return pNeighborDisp;
		}
	case IMAGEFILTER_MAIN: 
		{ 
			return pDisp;
		}
	default: 
		{ 
			return NULL; 
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::GetImageFieldValues( CDispMapImageFilter *pFilter,
									                  CMapDisp *pDisp, int ndxHgt, int ndxWid,
										              int ndxImg, int imgCount,
													  Vector &vNormal, float &dist )
{
	//
	// get the image (displacement) given a position
	//
	int orient;
	CMapDisp *pNeighborDisp = GetImage( pFilter, pDisp, ndxHgt, ndxWid, ndxImg, imgCount, orient );
	if( !pNeighborDisp )
		return false;

	switch( ndxImg )
	{
	case IMAGEFILTER_SOUTHWEST:
		{
			int ndx = GetSWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFieldData( pNeighborDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	case IMAGEFILTER_WEST:
		{
			int ndx = GetWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFieldData( pNeighborDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	case IMAGEFILTER_NORTHWEST:
		{
			int ndx = GetNWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFieldData( pNeighborDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	case IMAGEFILTER_NORTH:
		{
			int ndx = GetNImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFieldData( pNeighborDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	case IMAGEFILTER_NORTHEAST:
		{
			int ndx = GetNEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFieldData( pNeighborDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	case IMAGEFILTER_EAST:
		{
			int ndx = GetEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFieldData( pNeighborDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	case IMAGEFILTER_SOUTHEAST:
		{
			int ndx = GetSEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFieldData( pNeighborDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	case IMAGEFILTER_SOUTH:
		{
			int ndx = GetSImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFieldData( pNeighborDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	case IMAGEFILTER_MAIN:
		{
			int ndx = ndxHgt * pDisp->GetWidth() + ndxWid;
			GetImageFieldData( pDisp, pFilter, ndx, vNormal, dist );
			return true;
		}
	default: { return false; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::GetImageFlatSubdivValues( CDispMapImageFilter *pFilter,
									                       CMapDisp *pDisp, int ndxHgt, int ndxWid,
										                   int ndxImg, int imgCount, Vector &value )
{
	//
	// get the image (displacement) given a position
	//
	int orient;
	CMapDisp *pNeighborDisp = GetImage( pFilter, pDisp, ndxHgt, ndxWid, ndxImg, imgCount, orient );
	if( !pNeighborDisp )
		return false;

	switch( ndxImg )
	{
	case IMAGEFILTER_SOUTHWEST:
		{
			int ndx = GetSWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFlatSubdivValue( pNeighborDisp, pFilter, ndx, value );
			return true;
		}
	case IMAGEFILTER_WEST:
		{
			int ndx = GetWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFlatSubdivValue( pNeighborDisp, pFilter, ndx, value );
			return true;
		}
	case IMAGEFILTER_NORTHWEST:
		{
			int ndx = GetNWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFlatSubdivValue( pNeighborDisp, pFilter, ndx, value );
			return true;
		}
	case IMAGEFILTER_NORTH:
		{
			int ndx = GetNImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFlatSubdivValue( pNeighborDisp, pFilter, ndx, value );
			return true;
		}
	case IMAGEFILTER_NORTHEAST:
		{
			int ndx = GetNEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFlatSubdivValue( pNeighborDisp, pFilter, ndx, value );
			return true;
		}
	case IMAGEFILTER_EAST:
		{
			int ndx = GetEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFlatSubdivValue( pNeighborDisp, pFilter, ndx, value );
			return true;
		}
	case IMAGEFILTER_SOUTHEAST:
		{
			int ndx = GetSEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFlatSubdivValue( pNeighborDisp, pFilter, ndx, value );
			return true;
		}
	case IMAGEFILTER_SOUTH:
		{
			int ndx = GetSImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
			GetImageFlatSubdivValue( pNeighborDisp, pFilter, ndx, value );
			return true;
		}
	case IMAGEFILTER_MAIN:
		{
			int ndx = ndxHgt * pDisp->GetWidth() + ndxWid;
			GetImageFlatSubdivValue( pDisp, pFilter, ndx, value );
			return true;
		}
	default: { return false; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::GetImageValues( CDispMapImageFilter *pFilter,
									             CMapDisp *pDisp, int ndxHgt, int ndxWid,
										         int ndxImg, int imgCount, Vector &value )
{
	// Get the image (displacement) given a position
	int orient;
	CMapDisp *pNeighborDisp = GetImage( pFilter, pDisp, ndxHgt, ndxWid, ndxImg, imgCount, orient );
	if( !pNeighborDisp || !IsNeighborInSelectionSet( pNeighborDisp ) )
		return false;

	switch( ndxImg )
	{
	case IMAGEFILTER_SOUTHWEST: { SWImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, false, value ); return true; }
	case IMAGEFILTER_WEST: { WImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, false, value ); return true; }
	case IMAGEFILTER_NORTHWEST: { NWImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, false, value ); return true; }
	case IMAGEFILTER_NORTH: { NImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, false, value ); return true; }
	case IMAGEFILTER_NORTHEAST: { NEImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, false, value ); return true; }
	case IMAGEFILTER_EAST: { EImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, false, value ); return true; }
	case IMAGEFILTER_SOUTHEAST: { SEImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, false, value ); return true; }
	case IMAGEFILTER_SOUTH: { SImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, false, value ); return true; }
	case IMAGEFILTER_MAIN: { MainImageValue( pFilter, pDisp, ndxHgt, ndxWid, false, value ); return true; }
	default: { return false; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetAdjustedIndex( CMapDisp *pDisp, int orient, 
												  int ndxHgt, int ndxWid, int ndxImg )
{
	switch( ndxImg )
	{
	case IMAGEFILTER_SOUTHWEST: 
		{
			return GetSWImageIndex( pDisp, orient, ndxHgt, ndxWid );
		}
	case IMAGEFILTER_WEST: 
		{
			return GetWImageIndex( pDisp, orient, ndxHgt, ndxWid );
		}
	case IMAGEFILTER_NORTHWEST: 
		{ 
			return GetNWImageIndex( pDisp, orient, ndxHgt, ndxWid );
		}
	case IMAGEFILTER_NORTH: 
		{
			return GetNImageIndex( pDisp, orient, ndxHgt, ndxWid );
		}
	case IMAGEFILTER_NORTHEAST: 
		{ 
			return GetNEImageIndex( pDisp, orient, ndxHgt, ndxWid );
		}
	case IMAGEFILTER_EAST: 
		{ 
			return GetEImageIndex( pDisp, orient, ndxHgt, ndxWid );
		}
	case IMAGEFILTER_SOUTHEAST: 
		{ 
			return GetSEImageIndex( pDisp, orient, ndxHgt, ndxWid );
		}
	case IMAGEFILTER_SOUTH: 
		{ 
			return GetSImageIndex( pDisp, orient, ndxHgt, ndxWid );
		}
	case IMAGEFILTER_MAIN: 
		{ 
			int imgWid = pDisp->GetWidth();
			return( ndxHgt * imgWid + ndxWid );
		}
	default: 
		{ 
			return -1; 
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::SetImageValues( CDispMapImageFilter *pFilter,
												 CMapDisp *pDisp, int ndxHgt, int ndxWid, 
												 int ndxImg, int imgCount, Vector &value )
{
	CMapDisp *pNeighborDisp = NULL;
	int		 orient;
	EditDispHandle_t neighborHandle;

	switch( ndxImg )
	{
	case IMAGEFILTER_SOUTHWEST: 
		{
			int count = pDisp->GetCornerNeighborCount( 0 );
			if( count != 0 )
			{
				for( int i = 0; i < count; i++ )
				{
					if( i != imgCount )
						continue;

					pDisp->GetCornerNeighbor( 0, i, neighborHandle, orient );
					if( neighborHandle != EDITDISPHANDLE_INVALID )
					{
						pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
						SWImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, true, value );
					}
					return;
				}			
			}

			return;
		}
	case IMAGEFILTER_WEST: 
		{
			pDisp->GetEdgeNeighbor( 0, neighborHandle, orient );
			if( neighborHandle != EDITDISPHANDLE_INVALID )
			{
				pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
				WImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, true, value );
				return;
			}

			return;
		}
	case IMAGEFILTER_NORTHWEST: 
		{ 
			int count = pDisp->GetCornerNeighborCount( 2 );
			if( count != 0 )
			{
				for( int i = 0; i < count; i++ )
				{
					if( i != imgCount )
						continue;

					pDisp->GetCornerNeighbor( 2, i, neighborHandle, orient );
					if( neighborHandle != EDITDISPHANDLE_INVALID )
					{
						pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
						NWImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, true, value );
					}
					return;
				}			
			}

			return;
		}
	case IMAGEFILTER_NORTH: 
		{
			pDisp->GetEdgeNeighbor( 1, neighborHandle, orient );
			if( neighborHandle != EDITDISPHANDLE_INVALID )
			{
				pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
				NImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, true, value );
				return;
			}

			// shouldn't be here!!
			return;
		}
	case IMAGEFILTER_NORTHEAST: 
		{ 
			int count = pDisp->GetCornerNeighborCount( 3 );
			if( count != 0 )
			{
				for( int i = 0; i < count; i++ )
				{
					if( i != imgCount )
						continue;

					pDisp->GetCornerNeighbor( 3, i, neighborHandle, orient );
					if( neighborHandle != EDITDISPHANDLE_INVALID )
					{
						pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
						NEImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, true, value );
					}
					return;
				}			
			}

			return;
		}
	case IMAGEFILTER_EAST: 
		{ 
			pDisp->GetEdgeNeighbor( 2, neighborHandle, orient );
			if( neighborHandle != EDITDISPHANDLE_INVALID )
			{
				pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
				EImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, true, value );
				return;
			}

			return;
		}
	case IMAGEFILTER_SOUTHEAST: 
		{ 
			int count = pDisp->GetCornerNeighborCount( 1 );
			if( count != 0 )
			{
				for( int i = 0; i < count; i++ )
				{
					if( i != imgCount )
						continue;

					pDisp->GetCornerNeighbor( 1, i, neighborHandle, orient );
					if( neighborHandle != EDITDISPHANDLE_INVALID )
					{
						pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
						SEImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, true, value );
					}
					return;
				}			
			}

			return;
		}
	case IMAGEFILTER_SOUTH: 
		{ 
			pDisp->GetEdgeNeighbor( 3, neighborHandle, orient );
			if( neighborHandle != EDITDISPHANDLE_INVALID )
			{
				pNeighborDisp = EditDispMgr()->GetDisp( neighborHandle );
				SImageValue( pFilter, pNeighborDisp, orient, ndxHgt, ndxWid, true, value );
				return;
			}

			return;
		}
	case IMAGEFILTER_MAIN: 
		{ 
			MainImageValue( pFilter, pDisp, ndxHgt, ndxWid, true, value );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::MainImageValue( CDispMapImageFilter *pFilter,
										         CMapDisp *pDisp, int ndxHgt, int ndxWid, 
										         bool bSet, Vector &value )
{
	// get the image height and width
	int height = pDisp->GetHeight();

	// calc index value
	int index = ndxHgt * height + ndxWid;
	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetSWImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid )
{
	//
	// get the image height and width
	//
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	//
	//
	//
	int hIndex, wIndex;
	switch( orient )
	{
	case IMAGEFILTER_ORIENT_SOUTHWEST:
		{
			hIndex = -ndxHgt;
			wIndex = -ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_SOUTHEAST:
		{
			hIndex = -ndxWid;
			wIndex = ( width - 1 ) + ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_NORTHWEST:
		{
			hIndex = ( height - 1 ) + ndxWid;
			wIndex = -ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_NORTHEAST:
		{
			hIndex = ( height - 1 ) + ndxHgt;
			wIndex = ( width - 1 ) + ndxWid;
			break;
		}
	default:
		{
			hIndex = 0;
			wIndex = 0;
		}
	}

	//
	// clamp height and width index values
	//
	if( hIndex < 0 ) { hIndex = 0; }
	if( wIndex < 0 ) { wIndex = 0; }
	if( hIndex > ( height - 1 ) ) { hIndex = ( height - 1 ); }
	if( wIndex > ( width - 1 ) ) { wIndex = ( width - 1 ); }

	// calc index value
	return( hIndex * height + wIndex );

}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::SWImageValue( CDispMapImageFilter *pFilter,
										       CMapDisp *pDisp, int orient,
										       int ndxHgt, int ndxWid, bool bSet, 
											   Vector &value )
{
	// get the index
	int index = GetSWImageIndex( pDisp, orient, ndxHgt, ndxWid );

	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetWImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid )
{
	//
	// get the image height and width
	//
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	//
	//
	//
	int hIndex, wIndex;
	switch( orient )
	{
	case IMAGEFILTER_ORIENT_WEST:
		{
			hIndex = ( height - 1 ) - ndxHgt;
			wIndex = -ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_NORTH:
		{
			hIndex = ( height - 1 ) + ndxWid;
			wIndex = ( width - 1 ) - ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_EAST:
		{
			hIndex = ndxHgt;
			wIndex = ( width - 1 ) + ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_SOUTH:
		{
			hIndex = -ndxWid;
			wIndex = ndxHgt;
			break;
		}
	default: 
		{
			hIndex = 0;
			wIndex = 0;
		}
	}

	//
	// clamp height and width index values
	//
	if( hIndex < 0 ) { hIndex = 0; }
	if( wIndex < 0 ) { wIndex = 0; }
	if( hIndex > ( height - 1 ) ) { hIndex = ( height - 1 ); }
	if( wIndex > ( width - 1 ) ) { wIndex = ( width - 1 ); }

	// calc index value
	return( hIndex * height + wIndex );

}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::WImageValue( CDispMapImageFilter *pFilter,
									          CMapDisp *pDisp, int orient,
									          int ndxHgt, int ndxWid, bool bSet, Vector &value )
{
	// get image index
	int index = GetWImageIndex( pDisp, orient, ndxHgt, ndxWid );

	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetNWImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid )
{
	//
	// get the image height and width
	//
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	//
	//
	//
	int hIndex, wIndex;
	switch( orient )
	{
	case IMAGEFILTER_ORIENT_SOUTHWEST:
		{
			hIndex = ndxHgt - ( height - 1 );
			wIndex = -ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_SOUTHEAST:
		{
			hIndex = ndxHgt - ( height - 1 );
			wIndex = ( width - 1 ) + ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_NORTHWEST:
		{
			hIndex = ( 2 * ( height - 1 ) ) - ndxHgt;
			wIndex = -ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_NORTHEAST:
		{
			hIndex = ( height - 1 ) + ndxWid;
			wIndex = ( 2 * ( width - 1 ) ) - ndxHgt;
			break;
		}
	default: 
		{
			hIndex = 0;
			wIndex = 0;
		 }
	}

	//
	// clamp height and width index values
	//
	if( hIndex < 0 ) { hIndex = 0; }
	if( wIndex < 0 ) { wIndex = 0; }
	if( hIndex > ( height - 1 ) ) { hIndex = ( height - 1 ); }
	if( wIndex > ( width - 1 ) ) { wIndex = ( width - 1 ); }

	// calc index value
	return( hIndex * height + wIndex );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::NWImageValue( CDispMapImageFilter *pFilter,
										       CMapDisp *pDisp, int orient,
										       int ndxHgt, int ndxWid, bool bSet, Vector &value )
{
	// get image index
	int index = GetNWImageIndex( pDisp, orient, ndxHgt, ndxWid );

	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetNImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid )
{
	//
	// get the image height and width
	//
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	//
	//
	//
	int hIndex, wIndex;
	switch( orient )
	{
	case IMAGEFILTER_ORIENT_WEST:
		{
			hIndex = ( height - 1 ) - ndxWid;
			wIndex = ndxHgt - ( width - 1 );
			break;
		}
	case IMAGEFILTER_ORIENT_NORTH:
		{
			hIndex = ( 2 * ( height - 1 ) ) - ndxHgt;
			wIndex = ( width - 1 ) - ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_EAST:
		{
			hIndex = ndxWid;
			wIndex = ( 2 * ( width - 1 ) ) - ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_SOUTH:
		{
			hIndex = ndxHgt - ( height - 1 );
			wIndex = ndxWid;
			break;
		}
	default: 
		{
			hIndex = 0;
			wIndex = 0;
		}
	}

	//
	// clamp height and width index values
	//
	if( hIndex < 0 ) { hIndex = 0; }
	if( wIndex < 0 ) { wIndex = 0; }
	if( hIndex > ( height - 1 ) ) { hIndex = ( height - 1 ); }
	if( wIndex > ( width - 1 ) ) { wIndex = ( width - 1 ); }

	// calc index value
	return( hIndex * height + wIndex );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::NImageValue( CDispMapImageFilter *pFilter,
									          CMapDisp *pDisp, int orient,
									          int ndxHgt, int ndxWid, bool bSet, Vector &value )
{
	// get image index
	int index = GetNImageIndex( pDisp, orient, ndxHgt, ndxWid );

	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetNEImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid )
{
	//
	// get the image height and width
	//
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	//
	//
	//
	int hIndex, wIndex;
	switch( orient )
	{
	case IMAGEFILTER_ORIENT_SOUTHWEST:
		{
			hIndex = ndxHgt - ( height - 1 );
			wIndex = ndxWid - ( width - 1 );
			break;
		}
	case IMAGEFILTER_ORIENT_SOUTHEAST:
		{
			hIndex = ndxWid - ( height - 1 );
			wIndex = ( 2 * ( width - 1 ) ) - ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_NORTHWEST:
		{
			hIndex = ( 2 * ( height - 1 ) ) - ndxWid;
			wIndex = ndxHgt - ( width - 1 );
			break;
		}
	case IMAGEFILTER_ORIENT_NORTHEAST:
		{
			hIndex = ( 2 * ( height - 1 ) ) - ndxHgt;
			wIndex = ( 2 * ( width - 1 ) ) - ndxWid;
			break;
		}
	default: 
		{
			hIndex = 0;
			wIndex = 0;
		}
	}

	//
	// clamp height and width index values
	//
	if( hIndex < 0 ) { hIndex = 0; }
	if( wIndex < 0 ) { wIndex = 0; }
	if( hIndex > ( height - 1 ) ) { hIndex = ( height - 1 ); }
	if( wIndex > ( width - 1 ) ) { wIndex = ( width - 1 ); }

	// calc index value
	return( hIndex * height + wIndex );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::NEImageValue( CDispMapImageFilter *pFilter,
										       CMapDisp *pDisp, int orient,
										       int ndxHgt, int ndxWid, bool bSet, Vector &value )
{
	// get image index
	int index = GetNEImageIndex( pDisp, orient, ndxHgt, ndxWid );

	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetEImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid )
{
	//
	// get the image height and width
	//
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	//
	//
	//
	int hIndex, wIndex;
	switch( orient )
	{
	case IMAGEFILTER_ORIENT_WEST:
		{
			hIndex = ndxHgt;
			wIndex = ndxWid - ( width - 1 );
			break;
		}
	case IMAGEFILTER_ORIENT_NORTH:
		{
			hIndex = ( 2 * ( height - 1 ) ) - ndxWid;
			wIndex = ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_EAST:
		{
			hIndex = ( height - 1 ) - ndxHgt;
			wIndex = ( 2 * ( width - 1 ) ) - ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_SOUTH:
		{
			hIndex = ndxWid - ( height - 1 );
			wIndex = ( width - 1 ) - ndxHgt;
			break;
		}
	default: 
		{
			hIndex = 0;
			wIndex = 0;
		}
	}

	//
	// clamp height and width index values
	//
	if( hIndex < 0 ) { hIndex = 0; }
	if( wIndex < 0 ) { wIndex = 0; }
	if( hIndex > ( height - 1 ) ) { hIndex = ( height - 1 ); }
	if( wIndex > ( width - 1 ) ) { wIndex = ( width - 1 ); }

	// calc index value
	return( hIndex * height + wIndex );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::EImageValue( CDispMapImageFilter *pFilter,
									          CMapDisp *pDisp, int orient,
									          int ndxHgt, int ndxWid, bool bSet, Vector &value )
{
	// get image index
	int index = GetEImageIndex( pDisp, orient, ndxHgt, ndxWid );

	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetSEImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid )
{
	//
	// get the image height and width
	//
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	//
	//
	//
	int hIndex, wIndex;
	switch( orient )
	{
	case IMAGEFILTER_ORIENT_SOUTHWEST:
		{
			hIndex = ndxWid - ( height - 1 );
			wIndex = -ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_SOUTHEAST:
		{
			hIndex = -ndxHgt;
			wIndex = ( 2 * ( width - 1 ) ) - ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_NORTHWEST:
		{
			hIndex = ( height - 1 ) + ndxHgt;
			wIndex = ndxWid - ( width - 1 );
			break;
		}
	case IMAGEFILTER_ORIENT_NORTHEAST:
		{
			hIndex = ( 2 * ( height - 1 ) ) - ndxWid;
			wIndex = ( width - 1 ) + ndxHgt;
			break;
		}
	default: 
		{
			hIndex = 0;
			wIndex = 0;
		}
	}

	//
	// clamp height and width index values
	//
	if( hIndex < 0 ) { hIndex = 0; }
	if( wIndex < 0 ) { wIndex = 0; }
	if( hIndex > ( height - 1 ) ) { hIndex = ( height - 1 ); }
	if( wIndex > ( width - 1 ) ) { wIndex = ( width - 1 ); }

	// calc index value
	return( hIndex * height + wIndex );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::SEImageValue( CDispMapImageFilter *pFilter,
										       CMapDisp *pDisp, int orient,
										       int ndxHgt, int ndxWid, bool bSet, Vector &value )
{
	// get image index
	int index = GetSEImageIndex( pDisp, orient, ndxHgt, ndxWid );
	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDispMapImageFilterManager::GetSImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid )
{
	//
	// get the image height and width
	//
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	//
	//
	//
	int hIndex, wIndex;
	switch( orient )
	{
	case IMAGEFILTER_ORIENT_WEST:
		{
			hIndex = ndxWid;
			wIndex = -ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_NORTH:
		{
			hIndex = ( height - 1 ) + ndxHgt;
			wIndex = ndxWid;
			break;
		}
	case IMAGEFILTER_ORIENT_EAST:
		{
			hIndex = ( height - 1 ) - ndxWid;
			wIndex = ( width - 1 ) + ndxHgt;
			break;
		}
	case IMAGEFILTER_ORIENT_SOUTH:
		{
			hIndex = -ndxHgt;
			wIndex = ( width - 1 ) - ndxWid;
			break;
		}
	default: 
		{
			hIndex = 0;
			wIndex = 0;
		}
	}

	//
	// clamp height and width index values
	//
	if( hIndex < 0 ) { hIndex = 0; }
	if( wIndex < 0 ) { wIndex = 0; }
	if( hIndex > ( height - 1 ) ) { hIndex = ( height - 1 ); }
	if( wIndex > ( width - 1 ) ) { wIndex = ( width - 1 ); }

	// calc index value
	return( hIndex * height + wIndex );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::SImageValue( CDispMapImageFilter *pFilter,
									          CMapDisp *pDisp, int orient,
									          int ndxHgt, int ndxWid, bool bSet, Vector &value )
{
	// get image index
	int index = GetSImageIndex( pDisp, orient, ndxHgt, ndxWid );

	//
	// return the value at this position
	//
	if( bSet )
	{
		SetImageValue( pDisp, pFilter, index, value );
	}
	else
	{
		GetImageValue( pDisp, pFilter, index, value );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::ClampValues( CDispMapImageFilter *pFilter, Vector &v )
{
	if( pFilter->m_DataType == DISPPAINT_CHANNEL_ALPHA )
	{
		if( v.x < 0.0f ) { v.x = 0.0f; }
		if( v.x > 255.0f ) { v.x = 255.0f; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::GetFilterVector( CDispMapImageFilter *pFilter,
									              CMapDisp *pDisp, int ndxHgt, int ndxWid,
										          int ndxImg, int imgCount, int ndxFilter, 
												  Vector &vFilterDir )
{
	// 
	// handle the alpha case
	//
	if( pFilter->m_DataType == DISPPAINT_CHANNEL_ALPHA )
	{
		vFilterDir.Init( ( pFilter->m_pImage[ndxFilter] * pFilter->m_Scale ), 0.0f, 0.0f );
		return true;
	}

	//
	// get the image (displacement) given a position
	//
	int orient;
	CMapDisp *pNeighborDisp = GetImage( pFilter, pDisp, ndxHgt, ndxWid, ndxImg, imgCount, orient );
	if( !pNeighborDisp )
		return false;

	Vector normal;
	normal = m_PaintDir;

	if( m_PaintType == DISPPAINT_AXIS_SUBDIV )
	{
		switch( ndxImg )
		{
		case IMAGEFILTER_SOUTHWEST:
			{
				int ndx = GetSWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
				pNeighborDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		case IMAGEFILTER_WEST: 
			{
				int ndx = GetWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
				pNeighborDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		case IMAGEFILTER_NORTHWEST: 
			{ 
				int ndx = GetNWImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
				pNeighborDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		case IMAGEFILTER_NORTH: 
			{
				int ndx = GetNImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
				pNeighborDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		case IMAGEFILTER_NORTHEAST: 
			{
				int ndx = GetNEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
				pNeighborDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		case IMAGEFILTER_EAST: 
			{
				int ndx = GetEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
				pNeighborDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		case IMAGEFILTER_SOUTHEAST: 
			{
				int ndx = GetSEImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
				pNeighborDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		case IMAGEFILTER_SOUTH: 
			{ 
				int ndx = GetSImageIndex( pNeighborDisp, orient, ndxHgt, ndxWid );
				pNeighborDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		case IMAGEFILTER_MAIN: 
			{ 
				int ndx = ndxHgt * pDisp->GetWidth() + ndxWid;
				pDisp->GetSubdivNormal( ndx, normal );
				break;
			}
		default: 
			{ 
				return false; 
			}
		}
	}

	vFilterDir = normal * ( pFilter->m_pImage[ndxFilter] * pFilter->m_Scale );
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::ApplyAddFilter( CDispMapImageFilter *pFilter,
										         CMapDisp *pDisp, int iVert )
{
	// Get displacement image and filter height and width data
	int nImageHeight = pDisp->GetHeight();
	int nImageWidth = pDisp->GetWidth();

	int nFilterHeight = pFilter->m_Height;
	int nFilterWidth = pFilter->m_Width;

	// Calculate filter mid point.
	int nFilterMidHeight = ( nFilterHeight - 1 ) / 2;
	int nFilterMidWidth = ( nFilterWidth - 1 ) / 2;

	// Componentize image index.
	int iVertHeight = iVert / nImageHeight;
	int iVertWidth = iVert % nImageWidth;

	// For all positions in filter
	Vector vecImage;
	for( int iHgt = 0; iHgt < nFilterHeight; ++iHgt )
	{
		for( int iWid = 0; iWid < nFilterWidth; ++iWid )
		{
			// Position relative to the center of the filter.
			int nHeight = iHgt - nFilterMidHeight;
			int nWidth = iWid - nFilterMidWidth;
			
			// Adjusted height and width.
			int nAdjHeight = iVertHeight + nHeight;
			int nAdjWidth = iVertWidth + nWidth;

			// Setup the hit data.
			PosHitData_t hitData;
			HitData_Setup( hitData, nAdjHeight, nAdjWidth, nImageHeight, nImageWidth );

			// Update corners.
			if( hitData.m_CornerCount != 0 )
			{
				for( int iCorner = 0; iCorner < hitData.m_CornerCount; ++iCorner )
				{
					int nCount = GetCornerImageCount( pDisp, hitData.m_ndxCorners[iCorner] );
					for( int iCurCorner = 0; iCurCorner < nCount; ++iCurCorner )
					{
						if( GetImageValues( pFilter, pDisp, nAdjHeight, nAdjWidth, hitData.m_ndxCorners[iCorner], iCurCorner, vecImage ) )
						{
							Vector vecFilter;
							GetFilterVector( pFilter, pDisp, nAdjHeight, nAdjWidth, hitData.m_ndxCorners[iCorner], iCurCorner, ( iHgt * nFilterHeight + iWid ), vecFilter );
							vecImage += vecFilter;
							
							// Clamp values (for alpha).
							ClampValues( pFilter, vecImage );
						
							SetImageValues( pFilter, pDisp, nAdjHeight, nAdjWidth, hitData.m_ndxCorners[iCorner], iCurCorner, vecImage );
						}
					}
				}
			}

			// Update edges.
			if( hitData.m_EdgeCount != 0 )
			{
				for( int iEdge = 0; iEdge < hitData.m_EdgeCount; ++iEdge )
				{
					if( GetImageValues( pFilter, pDisp, nAdjHeight, nAdjWidth, hitData.m_ndxEdges[iEdge], 0, vecImage ) )
					{
						Vector vecFilter;
						GetFilterVector( pFilter, pDisp, nAdjHeight, nAdjWidth, hitData.m_ndxEdges[iEdge], 0, ( iHgt * nFilterHeight + iWid ), vecFilter );
						vecImage += vecFilter;
						
						// Clamp values (for alpha).
						ClampValues( pFilter, vecImage );
						
						SetImageValues( pFilter, pDisp, nAdjHeight, nAdjWidth, hitData.m_ndxEdges[iEdge], 0, vecImage );
					}
				}
			}

			// Update main.
			if( hitData.m_bMain )
			{
				if( GetImageValues( pFilter, pDisp, nAdjHeight, nAdjWidth, IMAGEFILTER_MAIN, 0, vecImage ) )
				{
					Vector vecFilter;
					GetFilterVector( pFilter, pDisp, nAdjHeight, nAdjWidth, IMAGEFILTER_MAIN, 0, ( iHgt * nFilterHeight + iWid ), vecFilter );
					vecImage += vecFilter;
					
					// Clamp values (for alpha).
					ClampValues( pFilter, vecImage );
					
					SetImageValues( pFilter, pDisp, nAdjHeight, nAdjWidth, IMAGEFILTER_MAIN, 0, vecImage );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::ApplyMultFilter( CDispMapImageFilter *pFilter,
								  		          CMapDisp *pDisp, int ndxVert )
{
	//
	// get displacement image and filter height and width data
	//
	int imgHgt = pDisp->GetHeight();
	int imgWid = pDisp->GetWidth();

	int filterHgt = pFilter->m_Height;
	int filterWid = pFilter->m_Width;

	//
	// get filter mid point
	//
	int filterMidHgt = ( filterHgt - 1 ) / 2;
	int filterMidWid = ( filterWid - 1 ) / 2;

	//
	// componentize image index
	//
	int ndxVertHgt = ndxVert / imgHgt;
	int ndxVertWid = ndxVert % imgWid;

	//
	// for all positions in filter
	//
	Vector vImg;
	for( int ndxHgt = 0; ndxHgt < filterHgt; ndxHgt++ )
	{
		for( int ndxWid = 0; ndxWid < filterWid; ndxWid++ )
		{
			// position relative to the center of the filter
			int height = ndxHgt - filterMidHgt;
			int width = ndxWid - filterMidWid;
			
			// adjusted height and width
			int adjHgt = ndxVertHgt + height;
			int adjWid = ndxVertWid + width;

			// setup the hit data
			PosHitData_t hitData;
			HitData_Setup( hitData, adjHgt, adjWid, imgHgt, imgWid );

			// update corners
			if( hitData.m_CornerCount != 0 )
			{
				for( int i = 0; i < hitData.m_CornerCount; i++ )
				{
					int count = GetCornerImageCount( pDisp, hitData.m_ndxCorners[i] );
					for( int j = 0; j < count; j++ )
					{
						if( GetImageValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxCorners[i], j, vImg ) )
						{
							Vector vFilter;
							GetFilterVector( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxCorners[i], j, (ndxHgt*filterHgt+ndxWid), vFilter );
							vImg *= vFilter;
							
							// clamp values (for alpha)
							ClampValues( pFilter, vImg );
							
							SetImageValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxCorners[i], j, vImg );
						}
					}
				}
			}

			// update edges
			if( hitData.m_EdgeCount != 0 )
			{
				for( int i = 0; i < hitData.m_EdgeCount; i++ )
				{
					if( GetImageValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxEdges[i], 0, vImg ) )
					{
						Vector vFilter;
						GetFilterVector( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxEdges[i], 0, (ndxHgt*filterHgt+ndxWid), vFilter );
						vImg += vFilter;
						
						// clamp values (for alpha)
						ClampValues( pFilter, vImg );
						
						SetImageValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxEdges[i], 0, vImg );
					}
				}
			}

			// update main
			if( hitData.m_bMain )
			{
				if( GetImageValues( pFilter, pDisp, adjHgt, adjWid, IMAGEFILTER_MAIN, 0, vImg ) )
				{
					Vector vFilter;
					GetFilterVector( pFilter, pDisp, adjHgt, adjWid, IMAGEFILTER_MAIN, 0, (ndxHgt*filterHgt+ndxWid), vFilter );
					vImg += vFilter;
					
					// clamp values (for alpha)
					ClampValues( pFilter, vImg );
					
					SetImageValues( pFilter, pDisp, adjHgt, adjWid, IMAGEFILTER_MAIN, 0, vImg );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::Apply3x3SmoothFilter( CDispMapImageFilter *pFilter,
													   CMapDisp *pDisp, int ndxVert,
													   Vector &vPos )
{
	//
	// get displacement image and filter height and width data
	//
	int imgHgt = pDisp->GetHeight();
	int imgWid = pDisp->GetWidth();

	int filterHgt = pFilter->m_Height;
	int filterWid = pFilter->m_Width;

	int filterMidHgt = ( pFilter->m_Height - 1 ) / 2;
	int filterMidWid = ( pFilter->m_Width - 1 ) / 2;

	//
	// componentize image index
	//
	int ndxVertHgt = ndxVert / imgHgt;
	int ndxVertWid = ndxVert % imgWid;

	Vector vNormal;
	Vector vNormals( 0.0f, 0.0f, 0.0f );
	float  dist;
	float  dists = 0.0f;
	float totalFrac = 0.0f;

	for( int ndxHgt = 0; ndxHgt < filterHgt; ndxHgt++ )
	{
		for( int ndxWid = 0; ndxWid < filterWid; ndxWid++ )
		{
			// position relative to the center of the filter
			int height = ndxHgt - filterMidHgt;
			int width = ndxWid - filterMidWid;
			
			// adjusted height and width
			int adjHgt = ndxVertHgt + height;
			int adjWid = ndxVertWid + width;

			// setup the hit data
			PosHitData_t hitData;
			HitData_Setup( hitData, adjHgt, adjWid, imgHgt, imgWid );

			// update corners
			if( hitData.m_CornerCount != 0 )
			{
				for( int i = 0; i < hitData.m_CornerCount; i++ )
				{
					int count = GetCornerImageCount( pDisp, hitData.m_ndxCorners[i] );
					for( int j = 0; j < count; j++ )
					{
						if( GetImageFieldValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxCorners[i], j, vNormal, dist ) )
						{
							vNormals += ( vNormal * pFilter->m_pImage[ndxHgt*filterHgt+ndxWid] );
							dists += ( dist * pFilter->m_pImage[ndxHgt*filterHgt+ndxWid] );
							totalFrac += pFilter->m_pImage[ndxHgt*filterHgt+ndxWid];
						}
					}
				}
			}

			// update edges
			if( hitData.m_EdgeCount != 0 )
			{
				for( int i = 0; i < hitData.m_EdgeCount; i++ )
				{
					if( GetImageFieldValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxEdges[i], 0, vNormal, dist ) )
					{
						vNormals += ( vNormal * pFilter->m_pImage[ndxHgt*filterHgt+ndxWid] );
						dists += ( dist * pFilter->m_pImage[ndxHgt*filterHgt+ndxWid] );
						totalFrac += pFilter->m_pImage[ndxHgt*filterHgt+ndxWid];
					}
				}
			}

			// update main
			if( hitData.m_bMain )
			{
				if( GetImageFieldValues( pFilter, pDisp, adjHgt, adjWid, IMAGEFILTER_MAIN, 0, vNormal, dist ) )
				{
					vNormals += ( vNormal * pFilter->m_pImage[ndxHgt*filterHgt+ndxWid] );
					dists += ( dist * pFilter->m_pImage[ndxHgt*filterHgt+ndxWid] );
					totalFrac += pFilter->m_pImage[ndxHgt*filterHgt+ndxWid];
				}
			}
		}
	}

	VectorNormalize( vNormals );
	dists /= totalFrac;

	vPos = vNormals * dists;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::ApplySmoothFilter( CDispMapImageFilter *pFilter,
										            CMapDisp *pDisp, int ndxVert )
{
	//
	// get displacement image and filter height and width data
	//
	int imgHgt = pDisp->GetHeight();
	int imgWid = pDisp->GetWidth();

	int areaHgt = pFilter->m_AreaHeight;
	int areaWid = pFilter->m_AreaWidth;

	int areaMidHgt = ( areaHgt - 1 ) / 2;
	int areaMidWid = ( areaWid - 1 ) / 2;

	//
	// componentize image index
	//
	int ndxVertHgt = ndxVert / imgHgt;
	int ndxVertWid = ndxVert % imgWid;

	//
	// for all positions in filter
	//
	Vector vPos;
	for( int ndxHgt = 0; ndxHgt < areaHgt; ndxHgt++ )
	{
		for( int ndxWid = 0; ndxWid < areaWid; ndxWid++ )
		{
			// position relative to the center of the area of effect
			int height = ndxHgt - areaMidHgt;
			int width = ndxWid - areaMidWid;

			// adjusted height and width
			int adjHgt = ndxVertHgt + height;
			int adjWid = ndxVertWid + width;

			// setup the hit data
			PosHitData_t hitData;
			HitData_Setup( hitData, adjHgt, adjWid, imgHgt, imgWid );

			// update corners
			if( hitData.m_CornerCount != 0 )
			{
				for( int i = 0; i < hitData.m_CornerCount; i++ )
				{
					int count = GetCornerImageCount( pDisp, hitData.m_ndxCorners[i] );
					for( int j = 0; j < count; j++ )
					{
						//
						// get the current corner
						//
						int orient;
						CMapDisp *pAdjDisp = GetImage( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxCorners[i], j, orient );
						if( pAdjDisp )
						{
							int adjIndex = GetAdjustedIndex( pAdjDisp, orient, adjHgt, adjWid, hitData.m_ndxCorners[i] );
							
							// apply a 3x3 box filter at each position
							Apply3x3SmoothFilter( pFilter, pAdjDisp, adjIndex, vPos );
						
							// get the flat/subdivision position
							Vector vFlat, vSubPos;
							pAdjDisp->GetFlatVert( adjIndex, vFlat );
							pAdjDisp->GetSubdivPosition( adjIndex, vSubPos );

							vPos += vFlat;
							vPos += vSubPos;

							SetImageValue( pAdjDisp, pFilter, adjIndex, vPos );
						}
					}
				}
			}

			// update edges
			if( hitData.m_EdgeCount != 0 )
			{
				for( int i = 0; i < hitData.m_EdgeCount; i++ )
				{
						//
						// get the current corner
						//
						int orient;
						CMapDisp *pAdjDisp = GetImage( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxEdges[i], 0, orient );
						if( pAdjDisp )
						{
							int adjIndex = GetAdjustedIndex( pAdjDisp, orient, adjHgt, adjWid, hitData.m_ndxEdges[i] );
							
							// apply a 3x3 box filter at each position
							Apply3x3SmoothFilter( pFilter, pAdjDisp, adjIndex, vPos );

							// get the flat/subdivision position
							Vector vFlat, vSubPos;
							pAdjDisp->GetFlatVert( adjIndex, vFlat );
							pAdjDisp->GetSubdivPosition( adjIndex, vSubPos );

							vPos += vFlat;
							vPos += vSubPos;
							
							SetImageValue( pAdjDisp, pFilter, adjIndex, vPos );
						}
				}
			}

			// update main
			if( hitData.m_bMain )
			{
				//
				// get the current corner
				//
				int orient;
				CMapDisp *pAdjDisp = GetImage( pFilter, pDisp, adjHgt, adjWid, IMAGEFILTER_MAIN, 0, orient );
				if( pAdjDisp )
				{
					int adjIndex = GetAdjustedIndex( pAdjDisp, orient, adjHgt, adjWid, IMAGEFILTER_MAIN );
					
					// apply a 3x3 box filter at each position
					Apply3x3SmoothFilter( pFilter, pAdjDisp, adjIndex, vPos );

					// get the flat/subdivision position
					Vector vFlat, vSubPos;
					pAdjDisp->GetFlatVert( adjIndex, vFlat );
					pAdjDisp->GetSubdivPosition( adjIndex, vSubPos );
					
					vPos += vFlat;
					vPos += vSubPos;
	
					SetImageValue( pAdjDisp, pFilter, adjIndex, vPos );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDispMapImageFilterManager::IsEqualMask( CDispMapImageFilter *pFilter, int ndxFilter )
{
	if( pFilter->m_pImage[ndxFilter] == IMAGEFILTER_EQUALMASK )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispMapImageFilterManager::ApplyEqualFilter( CDispMapImageFilter *pFilter,
								  		           CMapDisp *pDisp, int ndxVert )
{
	//
	// get displacement image and filter height and width data
	//
	int imgHgt = pDisp->GetHeight();
	int imgWid = pDisp->GetWidth();

	int filterHgt = pFilter->m_Height;
	int filterWid = pFilter->m_Width;

	//
	// get filter mid point
	//
	int filterMidHgt = ( filterHgt - 1 ) / 2;
	int filterMidWid = ( filterWid - 1 ) / 2;

	//
	// componentize image index
	//
	int ndxVertHgt = ndxVert / imgHgt;
	int ndxVertWid = ndxVert % imgWid;

	//
	// for all positions in filter
	//
	Vector vImg;
	for( int ndxHgt = 0; ndxHgt < filterHgt; ndxHgt++ )
	{
		for( int ndxWid = 0; ndxWid < filterWid; ndxWid++ )
		{
			// position relative to the center of the filter
			int height = ndxHgt - filterMidHgt;
			int width = ndxWid - filterMidWid;
			
			// adjusted height and width
			int adjHgt = ndxVertHgt + height;
			int adjWid = ndxVertWid + width;

			// setup the hit data
			PosHitData_t hitData;
			HitData_Setup( hitData, adjHgt, adjWid, imgHgt, imgWid );

			// update corners
			if( hitData.m_CornerCount != 0 )
			{
				for( int i = 0; i < hitData.m_CornerCount; i++ )
				{
					int count = GetCornerImageCount( pDisp, hitData.m_ndxCorners[i] );
					for( int j = 0; j < count; j++ )
					{
						if( GetImageFlatSubdivValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxCorners[i], j, vImg ) )
						{
							if( !IsEqualMask( pFilter, (ndxHgt*filterHgt+ndxWid) ) )
							{
								Vector vFilter;
								GetFilterVector( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxCorners[i], j, (ndxHgt*filterHgt+ndxWid), vFilter );
								vImg += vFilter;

								ClampValues( pFilter, vImg );

								SetImageValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxCorners[i], j, vImg );
							}
						}
					}
				}
			}

			// update edges
			if( hitData.m_EdgeCount != 0 )
			{
				for( int i = 0; i < hitData.m_EdgeCount; i++ )
				{
					if( GetImageFlatSubdivValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxEdges[i], 0, vImg ) )
					{
						if( !IsEqualMask( pFilter, (ndxHgt*filterHgt+ndxWid) ) )
						{
							Vector vFilter;
							GetFilterVector( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxEdges[i], 0, (ndxHgt*filterHgt+ndxWid), vFilter );
							vImg += vFilter;
							
							ClampValues( pFilter, vImg );

							SetImageValues( pFilter, pDisp, adjHgt, adjWid, hitData.m_ndxEdges[i], 0, vImg );
						}
					}
				}
			}

			// update main
			if( hitData.m_bMain )
			{
				if( GetImageFlatSubdivValues( pFilter, pDisp, adjHgt, adjWid, IMAGEFILTER_MAIN, 0, vImg ) )
				{
					if( !IsEqualMask( pFilter, (ndxHgt*filterHgt+ndxWid) ) )
					{
						Vector vFilter;
						GetFilterVector( pFilter, pDisp, adjHgt, adjWid, IMAGEFILTER_MAIN, 0, (ndxHgt*filterHgt+ndxWid), vFilter );
						vImg += vFilter;
						
						ClampValues( pFilter, vImg );

						SetImageValues( pFilter, pDisp, adjHgt, adjWid, IMAGEFILTER_MAIN, 0, vImg );
					}
				}
			}
		}
	}
}


