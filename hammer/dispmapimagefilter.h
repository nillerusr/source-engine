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

#ifndef DISPMAPIMAGEFILTER_H
#define DISPMAPIMAGEFILTER_H
#pragma once

class CChunkFile;
class CMapDisp;
enum ChunkFileResult_t;

//#define PAINTAXIS_AXIAL_X		0
//#define PAINTAXIS_AXIAL_Y		1
//#define PAINTAXIS_AXIAL_Z		2
//#define PAINTAXIS_SUBDIV		3
//#define PAINTAXIS_FACE			4

//=============================================================================
//
// height,0-------height,width
//   |              |
//   |              |             "Orientation of Filter"
//   |              |
//   |              |
//   |              |
//   |              |
//   |              |
//   0,0---------0,width
//

//#define IMAGEFILTERTYPE_ADD			0
//#define IMAGEFILTERTYPE_MULT		1
//#define IMAGEFILTERTYPE_CONV		2
//#define IMAGEFILTERTYPE_CONVATTEN	3
//#define IMAGEFILTERTYPE_EQUAL		4

#define IMAGEFILTER_EQUALMASK		-99999.0f

//#define IMAGEFILTERDATA_DISTANCE	0
//#define IMAGEFILTERDATA_ALPHA		1

//-----------------------------------------------------------------------------
//
//   2--3--4
//   |     |
//	 1  8  5
//   |     |
//   0--7--6
//  
//-----------------------------------------------------------------------------
#define IMAGEFILTER_WEST		1
#define IMAGEFILTER_NORTH		3
#define IMAGEFILTER_EAST		5
#define IMAGEFILTER_SOUTH		7
#define IMAGEFILTER_SOUTHWEST	0
#define IMAGEFILTER_NORTHWEST	2
#define IMAGEFILTER_NORTHEAST	4
#define IMAGEFILTER_SOUTHEAST	6
#define IMAGEFILTER_MAIN		8

#define IMAGEFILTER_ORIENT_WEST			0
#define IMAGEFILTER_ORIENT_NORTH		1
#define IMAGEFILTER_ORIENT_EAST			2
#define IMAGEFILTER_ORIENT_SOUTH		3
#define IMAGEFILTER_ORIENT_SOUTHWEST	0
#define IMAGEFILTER_ORIENT_SOUTHEAST	1
#define IMAGEFILTER_ORIENT_NORTHWEST	2
#define IMAGEFILTER_ORIENT_NORTHEAST	3

//=============================================================================
//
// Displacement Image Filter
//
class CDispMapImageFilter
{
public:

	unsigned int	m_Type;				// type of filter -- add, multiply, conv, etc.
	int				m_DataType;			// type of data - distance, alpha (all 1-d)
	float			*m_pImage;			// filter "image" - matrix defining the "brush"
	int				m_Height;			// filter size (height)
	int				m_Width;			// filter size (width)

	// set at run-time
	float		m_Scale;			// scale
	int			m_AreaHeight;		// convolution area of effect (height)
	int			m_AreaWidth;		// convolution area of effect (width)

	// icon name
	CString		m_Name;				// name of filter

	CDispMapImageFilter();
	~CDispMapImageFilter();

	ChunkFileResult_t LoadFilter( CChunkFile *pFile ); 

private:

	static int GetFilterType( CString type );
	static ChunkFileResult_t LoadFilterKeyCallback( const char *szKey, const char *szValue, CDispMapImageFilter *pFilter );
	static ChunkFileResult_t LoadImageCallback( CChunkFile *pFile, CDispMapImageFilter *pFilter );
	static ChunkFileResult_t LoadImageKeyCallback( const char *szKey, const char *szValue, CDispMapImageFilter *pFilter );
	static void ValidHeight( int height );
	static void ValidWidth( int width );
};



//=============================================================================
//
// Displacement Image Filter Manager
//

#define FILTERLIST_SIZE		64

class CDispMapImageFilterManager
{
public:
    
	//=========================================================================
	//
	// Filter Creation/Destruction Functions
	//
	CDispMapImageFilterManager();

	CDispMapImageFilter *Create( void );
	void Destroy( void );

	void Add( CDispMapImageFilter *pFilter );

	//=========================================================================
	//
	// Filter List Functions
	//
	inline int GetFilterCount( void );
	inline CDispMapImageFilter *GetFilter( int ndx );

	void AddFilterToList( CDispMapImageFilter *pFilter );

	inline void SetActiveFilter( int ndx );
	inline CDispMapImageFilter *GetActiveFilter( void );

	//=========================================================================
	//
	// Operation Functions
	//
	bool Apply( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int paintDirType, Vector const &vPaintDir, bool bSew );

private:

	int						m_ActiveFilter;
	int						m_FilterCount;
	CDispMapImageFilter		*m_pFilterList[FILTERLIST_SIZE];

	int						m_PaintType;
	Vector					m_PaintDir;

	//========================================================================
	//
	// Filter Application Operation Functions
	//
	bool PreApply( CDispMapImageFilter *pFilter, int nPaintDirType, const Vector &vecPaintDir );
	bool PostApply( bool bSew );

	void ApplyAt( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxVert );
	void ApplyAddFilter( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxVert );
	void ApplyMultFilter( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxVert );	
	void ApplyEqualFilter( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxVert );
	void ApplySmoothFilter( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxVert );
	void Apply3x3SmoothFilter( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxVert, Vector &vPos );

	//========================================================================
	//
	// Hit Data
	//
	struct PosHitData_t
	{
		int		m_CornerCount;
		int		m_ndxCorners[2];
		int		m_EdgeCount;
		int		m_ndxEdges[2];
		bool	m_bMain;
	};

	void HitData_Init( PosHitData_t &hitData );
	void HitData_Setup( PosHitData_t &hitData, int ndxHgt, int ndxWid, int imgHgt, int imgWid );

	//=========================================================================
	//
	// Image Data Functions
	//
	void SetImageValues( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxHgt, int ndxWid, int ndxImg, int imgCount, Vector &vPaintValue );
	bool GetImageValues( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxHgt, int ndxWid, int ndxImg, int imgCount, Vector &vPaintValue );
	bool GetImageFlatSubdivValues( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxHgt, int ndxWid, int ndxImg, int imgCount, Vector &value );
	bool GetImageFieldValues( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxHgt, int ndxWid,
						      int ndxImg, int imgCount, Vector &vNormal, float &dist );

	inline void SetImageValue( CMapDisp *pDisp, CDispMapImageFilter *pFilter, int ndx, Vector &vPaintValue );
	inline void GetImageValue( CMapDisp *pDisp, CDispMapImageFilter *pFilter, int ndx, Vector &vPaintValue );
	inline void GetImageFlatSubdivValue( CMapDisp *pDisp, CDispMapImageFilter *pFilter, int ndxDisp, Vector &vPaintValue );
	inline void GetImageFieldData( CMapDisp *pDisp, CDispMapImageFilter *pFilter, int ndxDisp, Vector &vNormal, float &dist );

	int GetSWImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid );
	int GetWImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid );
	int GetNWImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid );
	int GetNImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid );
	int GetNEImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid );
	int GetEImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid );
	int GetSEImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid );
	int GetSImageIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid );

	void MainImageValue( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxHgt, int ndxWid, bool bSet, Vector &value );
	void SWImageValue( CDispMapImageFilter *pFilter, CMapDisp *pNeighborDisp, int neighborOrient, int ndxHgt, int ndxWid, bool bSet, Vector &value );
	void WImageValue( CDispMapImageFilter *pFilter, CMapDisp *pNeighborDisp, int neighborOrient, int ndxHgt, int ndxWid, bool bSet, Vector &value );
	void NWImageValue( CDispMapImageFilter *pFilter, CMapDisp *pNeighborDisp, int neighborOrient, int ndxHgt, int ndxWid, bool bSet, Vector &value );
	void NImageValue( CDispMapImageFilter *pFilter, CMapDisp *pNeighborDisp, int neighborOrient, int ndxHgt, int ndxWid, bool bSet, Vector &value );
	void NEImageValue( CDispMapImageFilter *pFilter, CMapDisp *pNeighborDisp, int neighborOrient, int ndxHgt, int ndxWid, bool bSet, Vector &value );
	void EImageValue( CDispMapImageFilter *pFilter, CMapDisp *pNeighborDisp, int neighborOrient, int ndxHgt, int ndxWid, bool bSet, Vector &value );
	void SEImageValue( CDispMapImageFilter *pFilter, CMapDisp *pNeighborDisp, int neighborOrient, int ndxHgt, int ndxWid, bool bSet, Vector &value );
	void SImageValue( CDispMapImageFilter *pFilter, CMapDisp *pNeighborDisp, int neighborOrient, int ndxHgt, int ndxWid, bool bSet, Vector &value );

	CMapDisp *GetSurfaceAtIndex( CMapDisp *pDisp, int ndxHgt, int ndxWid, int &newIndex );
	CMapDisp *GetImage( CDispMapImageFilter *pFilter, CMapDisp *pDisp, int ndxHgt, int ndxWid,
						int ndxImg, int imgCount, int &orient );

	bool IsNeighborInSelectionSet( CMapDisp *pNeighborDisp );

	//=========================================================================
	//
	// Filter Data Functions
	//
	bool GetFilterVector( CDispMapImageFilter *pFilter,CMapDisp *pDisp, int ndxHgt, int ndxWid,
  			              int ndxImg, int imgCount, int ndxFilter, Vector &vFilterDir );

	//=========================================================================
	//
	// Utility Functions
	//
	void ClampValues( CDispMapImageFilter *pFilter, Vector &v );
	int GetAdjustedIndex( CMapDisp *pDisp, int orient, int ndxHgt, int ndxWid, int ndxImg );
	bool IsEqualMask( CDispMapImageFilter *pFilter, int ndxFilter );
	int GetCornerImageCount( CMapDisp *pDisp, int ndxCorner );
	int GetImageCountAtPoint( CMapDisp *pDisp, int ndxHgt, int ndxWid );
	void CalcEdgeCornerFlags( int ndxHgt, int ndxWid, int imgHgt, int imgWid, bool &bOnEdge, bool &bOnCorner );
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CDispMapImageFilterManager::GetFilterCount( void )
{
	return m_FilterCount;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CDispMapImageFilter *CDispMapImageFilterManager::GetFilter( int ndx )
{
	if( ndx < 0 ) { return NULL; }
	if( ndx >= m_FilterCount ) { return NULL; }

	return m_pFilterList[ndx];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispMapImageFilterManager::SetActiveFilter( int ndx )
{
	if( ndx < 0 ) { return; }
	if( ndx >= m_FilterCount ) { return; }

	m_ActiveFilter = ndx;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CDispMapImageFilter *CDispMapImageFilterManager::GetActiveFilter( void )
{
	return m_pFilterList[m_ActiveFilter];
}


#endif // DISPMAPIMAGEFILTER_H