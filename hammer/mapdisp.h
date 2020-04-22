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

#ifndef MAPDISP_H
#define MAPDISP_H
#pragma once

//=============================================================================

#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include <utlvector.h>
#include "MapAtom.h"
#include "Render3D.h"
#include "mathlib/VMatrix.h"
#include "DispMapImageFilter.h"
#include "builddisp.h"
#include "DispManager.h"

class CChunkFile;
class CMapClass;
class CMapFace;
class CSaveInfo;
class IWorldEditDispMgr;
class CToolDisplace;
class Color;
class CSelection;

struct Shoreline_t;
struct ExportDXFInfo_s;

enum ChunkFileResult_t;

// Painting Defines
#define DISPPAINT_CHANNEL_POSITION      0
#define DISPPAINT_CHANNEL_ALPHA         1

#define WALKABLE_NORMAL_VALUE           0.7f
#define BUILDABLE_NORMAL_VALUE          0.8f

//=============================================================================
//
// Displacement Map Class
//
class CMapDisp : public CMapAtom
{
private:

    typedef struct
    {
        Vector min;
        Vector max;
    } BBox_t;

    typedef struct
    {
        Vector normal;
        float  dist;
    } Plane_t;

    typedef struct
    {
        Vector v[3];
    } Tri_t;

public:

    enum { MAPDISP_MAX_VERTS      = 289 };          // 17x17
    enum { MAPDISP_MAX_FACES      = 512 };          // ( 16x16 ) x 2
    enum { MAPDISP_MAX_NEIGHBORS  = 8 };            // 4 edges + 4 corners -- always four-sided

    //=========================================================================
    //
    // Constructor/Deconstructor (Initialization)
    //
    CMapDisp();
    ~CMapDisp();

    inline void SetEditHandle( EditDispHandle_t handle ) { m_EditHandle = handle; }
    inline EditDispHandle_t GetEditHandle( void ) { return m_EditHandle; }

    bool InitDispSurfaceData( CMapFace *pFace, bool bGenerateStartPoint );
    void ResetFieldData( void );
    void InitData( int power );
//  void InitData( int power, int minTess, float smoothingAngle, Vector **dispVectorField, Vector **dispVectorOffset, float *dispDistances );

    //=========================================================================
    //
    // Creation, Copy
    //
    bool Create( void );
    CMapDisp *CopyFrom( CMapDisp *pMapDisp, bool bUpdateDependencies );

    //=========================================================================
    //
    // Update/Modification/Editing Functions
    //
    void UpdateSurfData( CMapFace *pFace );
    void UpdateSurfDataAndVectorField( CMapFace *pFace );
    void UpdateData( void );
    void UpdateDataAndNeighborData( void );

    void InvertAlpha( void );

    void Resample( int power );
    void Elevate( float elevation );

    bool TraceLine( Vector &HitPos, Vector &HitNormal, Vector const &RayStart, Vector const &RayEnd );
    bool TraceLineSnapTo( Vector &HitPos, Vector &HitNormal, Vector const &RayStart, Vector const &RayEnd );

    void DoTransform(const VMatrix &matrix);

    void ApplyNoise( float min, float max, float rockiness );

    bool PointSurfIntersection( Vector const &ptCenter, float radius, float &distMin, Vector &ptMin );

    void Split( EditDispHandle_t hBuilderDisp );

    void UpdateWalkable( void );
    void UpdateBuildable( void );
    void UpdateTriRemove( void );

    void CreateShoreOverlays( CMapFace *pFace, Shoreline_t *pShoreline );

    //=========================================================================
    //
    // Attributes
    //
    inline void SetPower( int power );
    inline int GetPower( void );
    inline int CalcPower( int width );

    inline int GetSize( void );
    inline int GetWidth( void );
    inline int GetHeight( void );

    inline int TriangleCount() { return 2 * (GetWidth() - 1) * (GetHeight() - 1); }

    inline void SetElevation( float elevation );
    inline float GetElevation( void );

    void Scale( float scale );
    inline float GetScale( void );

    inline void GetBoundingBox( Vector& boxMin, Vector& boxMax );

    inline size_t GetDataSize( void );

    // flags
    inline bool IsTouched( void );
    inline void SetTouched( void );
    inline void ResetTouched( void );

    inline void SetHasMappingAxes( bool value );

    inline void SetSubdivided( bool bSubdiv );
    inline bool IsSubdivided( void );
    inline void SetReSubdivision( bool bReSubdiv );
    inline bool NeedsReSubdivision( void );

    //=========================================================================
    //
    // Base Surface Data
    //
    inline void GetSurfPoint( int index, Vector& pt );
    inline void GetSurfNormal( Vector& normal );
    inline int GetSurfPointStartIndex( void );
    inline void SetSurfPointStartIndex( int index );
    inline void GetSurfTexCoord( int ndx, Vector2D &texCoord );
    inline void SetSurfTexCoord( int ndx, Vector2D const &texCoord );

    inline int  GetFlags( void )                { return m_CoreDispInfo.GetSurface()->GetFlags(); }
    inline void SetFlags( int nFlags )          { m_CoreDispInfo.GetSurface()->SetFlags( nFlags ); }
    inline bool CheckFlags( int nFlags )        { return ( ( nFlags & GetFlags() ) != 0 ) ? true : false; }

    //=========================================================================
    //
    // Surface Data
    //
    inline void SetVert( int index, Vector const &v );
    inline void GetVert( int index, Vector& v );
    inline void SetAlpha( int index, float alpha );
    inline float GetAlpha( int index );
    inline void GetFlatVert( int index, Vector& v );
    inline void SetFlatVert( int index, const Vector &v );

    inline void ResetFieldVectors( void );
    inline void SetFieldVector( int index, Vector const &v );
    inline void GetFieldVector( int index, Vector& v );
    inline void ResetFieldDistances( void );
    inline void SetFieldDistance( int index, float distance );
    inline float GetFieldDistance( int index );

    inline void ResetSubdivPositions( void );
    inline void SetSubdivPosition( int ndx, Vector const &v );
    inline void GetSubdivPosition( int ndx, Vector& v );
    inline void ResetSubdivNormals( void );
    inline void SetSubdivNormal( int ndx, Vector const &v );
    inline void GetSubdivNormal( int ndx, Vector &v );

    inline int GetTriCount( void )                                                                    { return m_CoreDispInfo.GetTriCount(); }
    inline void GetTriIndices( int iTri, unsigned short &v1, unsigned short &v2, unsigned short &v3 ) { m_CoreDispInfo.GetTriIndices( iTri, v1, v2, v3 ); }
    inline void GetTriPos( int iTri, Vector &v1, Vector &v2, Vector &v3 )                             { m_CoreDispInfo.GetTriPos( iTri, v1, v2, v3 ); }
    inline void SetTriTag( int iTri, unsigned short nTag )                                            { m_CoreDispInfo.SetTriTag( iTri, nTag ); }
    inline void ResetTriTag( int iTri, unsigned short nTag )                                          { m_CoreDispInfo.ResetTriTag( iTri, nTag ); }
    inline void ToggleTriTag( int iTri, unsigned short nTag )                                         { m_CoreDispInfo.ToggleTriTag( iTri, nTag ); }
    inline bool IsTriTag( int iTri, unsigned short nTag )                                             { return m_CoreDispInfo.IsTriTag( iTri, nTag ); }
    inline bool IsTriWalkable( int iTri )                                                             { return m_CoreDispInfo.IsTriWalkable( iTri ); }
    inline bool IsTriBuildable( int iTri )                                                            { return m_CoreDispInfo.IsTriBuildable( iTri ); }

	// this is gone in m_CoreDispInfo.
    inline bool IsTriRemove( int iTri )                                                               { return false; } //m_CoreDispInfo.IsTriRemove( iTri ); }

	int CollideWithDispTri( const Vector &rayStart, const Vector &rayEnd, float &flFraction, bool OneSided = false );

    //=========================================================================
    //
    // Neighbors
    //
    void UpdateNeighborDependencies( bool bDestroy );
    
    static void UpdateNeighborsOfDispsIntersectingBox( const Vector &bbMin, const Vector &bbMax, float flPadding );

    inline void SetEdgeNeighbor( int direction, EditDispHandle_t handle, int orient );
    inline void GetEdgeNeighbor( int direction, EditDispHandle_t &handle, int &orient );
    inline EditDispHandle_t GetEdgeNeighbor( int direction );

    inline int GetCornerNeighborCount( int direction );
    inline void AddCornerNeighbor( int direction, EditDispHandle_t handle, int orient );
    inline void GetCornerNeighbor( int direction, int cornerIndex, EditDispHandle_t &handle, int &orient );
    inline EditDispHandle_t GetCornerNeighbor( int direction, int cornerIndex );

    // for lighting preview
    void AddShadowingTriangles( CUtlVector<Vector> &tri_list );

    //=========================================================================
    //
    // Rendering
    //
    void Render3D( CRender3D *pRender, bool bIsSelected, SelectionState_t faceSelectionState );
    void Render2D( CRender2D *pRender, bool bIsSelected, SelectionState_t faceSelectionState );

    static void SetSelectMask( bool bSelectMask );
    static bool HasSelectMask( void );
    static void SetGridMask( bool bGridMask );
    static bool HasGridMask( void );

    //=========================================================================
    //
    // Selection
    //
    inline void SetTexelHitIndex( int index );
    inline int GetTexelHitIndex( void );
    inline void ResetTexelHitIndex( void );

    inline void SetDispMapHitIndex( int index );
    inline void ResetDispMapHitIndex( void );
    EditDispHandle_t GetHitDispMap( void );

    //=========================================================================
    //
    // Paint Functions
    //
    void Paint_Init( int nType );
    void Paint_InitSelfAndNeighbors( int nType );
    void Paint_SetValue( int iVert, Vector const &vPaint );
    void Paint_Update( bool bSplit );
    void Paint_UpdateSelfAndNeighbors( bool bSplit );
    inline bool Paint_IsDirty( void );

    //=========================================================================
    //
    // Undo Functions (friends)
    //
    friend void EditDisp_ForUndo( EditDispHandle_t editHandle, char *pszPositionName, bool bNeighborsUndo );

    //=========================================================================
    //
    // Utility Functions
    //
    void DispUVToSurf( Vector2D const &dispUV, Vector &surfPt, Vector *pNormal, float *pAlpha )     { m_CoreDispInfo.DispUVToSurf( dispUV, surfPt, pNormal, pAlpha ); }
    void BaseFacePlaneToDispUV( Vector const &planePt, Vector2D &dispUV )                           { m_CoreDispInfo.BaseFacePlaneToDispUV( planePt, dispUV ); }
    bool SurfToBaseFacePlane( Vector const &surfPt, Vector &planePt )                               { return m_CoreDispInfo.SurfToBaseFacePlane( surfPt, planePt ); }

    CCoreDispInfo *GetCoreDispInfo( void )                                      { return &m_CoreDispInfo; }

    //=========================================================================
    //
    // Load/Save Functions
    //
    ChunkFileResult_t LoadVMF(CChunkFile *pFile);
    ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
    bool SerializedLoadMAP( std::fstream &file, CMapFace *pFace, UINT version );
    bool SerializedLoadRMF( std::fstream &file, CMapFace *pFace, float version );
    bool SaveDXF(ExportDXFInfo_s *pInfo);

    void PostLoad( void );

    void UpdateVertPositionForSubdiv( int iVert, const Vector &vecNewSubdivPos );


private:

    enum { NUM_EDGES_CORNERS = 4 };
    enum { MAX_CORNER_NEIGHBORS = 4 };

    EditDispHandle_t    m_EditHandle;                                                       // id of displacement in global manager's list

    CCoreDispInfo   m_CoreDispInfo;                                                         // core displacement info

    int             m_HitTexelIndex;                                                        // the displacement map texel that was "hit"
    int             m_HitDispIndex;                                                         // the displacement map that was hit (this or one of its neighbors)

    Vector          m_LightPosition;
    float           m_LightColor[3];

    EditDispHandle_t    m_EdgeNeighbors[NUM_EDGES_CORNERS];                                     // four possible edge neighbors (W, N, E, S)
    int             m_EdgeNeighborOrientations[NUM_EDGES_CORNERS];                          // neighbor edge orientations
    int             m_CornerNeighborCounts[NUM_EDGES_CORNERS];                              // number of corner neighbors (not counting edge neighbors)
    EditDispHandle_t    m_CornerNeighbors[NUM_EDGES_CORNERS][MAX_CORNER_NEIGHBORS];             // four corners/multiple corner neighbors possible (SW, SE, NW, NE)
    int             m_CornerNeighborOrientations[NUM_EDGES_CORNERS][MAX_CORNER_NEIGHBORS];  // neighbor corner orientations

    bool            m_bHasMappingAxes;
    Vector          m_MapAxes[2];                                                           // for older files (.map, .rmf)

    Vector          m_BBox[2];                                                              // axial-aligned bounding box

    float           m_Scale;

    static bool     m_bSelectMask;                                                          // masks for the Displacement Tool (FaceEditSheet)
    static bool     m_bGridMask;

    bool            m_bSubdiv;
    bool            m_bReSubdiv;

    CUtlVector<CoreDispVert_t*>     m_aWalkableVerts;
    CUtlVector<unsigned short>      m_aWalkableIndices;
    CUtlVector<unsigned short>      m_aForcedWalkableIndices;
    CUtlVector<CoreDispVert_t*>     m_aBuildableVerts;
    CUtlVector<unsigned short>      m_aBuildableIndices;
    CUtlVector<unsigned short>      m_aForcedBuildableIndices;
    CUtlVector<CoreDispVert_t*>     m_aRemoveVerts;
    CUtlVector<unsigned short>      m_aRemoveIndices;

    // Painting Data.
    struct PaintCanvas_t
    {
        enum { CANVAS_SIZE = MAPDISP_MAX_VERTS };

        int     m_nType;                            // what does the canvas hold - position, alpha, etc.
        Vector  m_Values[CANVAS_SIZE];
        bool    m_bValuesDirty[CANVAS_SIZE];
        bool    m_bDirty;
    };

    PaintCanvas_t   m_Canvas;

    //=========================================================================
    //
    // Painting Functions
    //
    void PaintPosition_Update( int iVert );
    void PaintAlpha_Update( int iVert );
    
    //=========================================================================
    //
    // Update/Modification/Editing Functions
    //
    void UpSample( int oldPower );
    void DownSample( int oldPower );
    void GetValidSamplePoints( int index, int width, int height, bool *pValidPoints );
    void SamplePoints( int index, int width, int height, bool *pValidPoints, float *pValue, float *pAlpha, 
                       Vector& newDispVector, Vector& newSubdivPos, Vector &newSubdivNormal );

    void PostCreate( void );
    void UpdateBoundingBox( void );
    void UpdateLightmapExtents( void );
    bool ValidLightmapSize( void );
    void CheckAndUpdateOverlays( bool bFull );
    bool EntityInBoundingBox( Vector const &vOrigin );

    enum { FLIP_HORIZONTAL = 0,
           FLIP_VERTICAL,
           FLIP_TRANSPOSE };

    void Flip( int flipType );
    int GetAxisTypeBasedOnView( int majorAxis, int vertAxis, int horzAxis );
    int GetMajorAxis( Vector &v );

    //=========================================================================
    //
    // Collision Testing
    //
    void CreateBoundingBoxes( BBox_t *pBBox, int count, float bloat );
    void CreatePlanesFromBoundingBox( Plane_t *planes, const Vector& bbMin, const Vector& bbMax );
    void CollideWithBoundingBoxes( const Vector& rayStart, const Vector& rayEnd, BBox_t *pBBox, int bboxCount, Tri_t *pTris, int *triCount );
    float CollideWithTriangles( const Vector& RayStart, const Vector& RayEnd, Tri_t *pTris, int triCount, Vector& surfNormal );

    //=========================================================================
    //
    // Rendering
    //
    void RenderHitBox( CRender3D *pRender, bool bNudge );
    void RenderPaintSphere( CRender3D *pRender, CToolDisplace *pTool );
    void CalcColor( CRender3D *pRender, bool bIsSelected, SelectionState_t faceSelectionState, Color &pColor );

    void RenderSurface( CRender3D *pRender, bool bIsSelected, SelectionState_t faceSelectionState );
    void RenderOverlaySurface( CRender3D *pRender, bool bIsSelected, SelectionState_t faceSelectionState );
    void RenderWalkableSurface( CRender3D *pRender, bool bIsSelected, SelectionState_t faceSelectionState );
    void RenderRemoveSurface( CRender3D *pRender, bool bIsSelected, SelectionState_t faceSelectionState );
    void RenderBuildableSurface( CRender3D *pRender, bool bIsSelected, SelectionState_t faceSelectionState );
    void RenderWireframeSurface( CRender3D *pRender, bool bIsSelected, SelectionState_t faceSelectionState );

    void RenderDisAllowedVerts( CRender3D *pRender );

    void Render3DDebug( CRender3D *pRender, bool isSelected );

    //=========================================================================
    //
    // Neighboring Functions
    //
    inline void ResetNeighbors( void );
    void FindNeighbors( void );

    //=========================================================================
    //
    // Load/Save Functions
    //
    static ChunkFileResult_t LoadDispDistancesCallback(CChunkFile *pFile, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispDistancesKeyCallback(const char *szKey, const char *szValue, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispOffsetsCallback(CChunkFile *pFile, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispOffsetsKeyCallback(const char *szKey, const char *szValue, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispOffsetNormalsCallback(CChunkFile *pFile, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispOffsetNormalsKeyCallback(const char *szKey, const char *szValue, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispKeyCallback(const char *szKey, const char *szValue, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispNormalsCallback(CChunkFile *pFile, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispNormalsKeyCallback(const char *szKey, const char *szValue, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispAlphasCallback(CChunkFile *pFile, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispAlphasKeyCallback(const char *szKey, const char *szValue, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispTriangleTagsCallback(CChunkFile *pFile, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispTriangleTagsKeyCallback(const char *szKey, const char *szValue, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispAllowedVertsCallback(CChunkFile *pFile, CMapDisp *pDisp);
    static ChunkFileResult_t LoadDispAllowedVertsKeyCallback(const char *szKey, const char *szValue, CMapDisp *pDisp);

    //=========================================================================
    //
    // Utility
    //
    bool ComparePoints( const Vector& pt1, const Vector& pt2, const float tolerance );
    int GetStartIndexFromLevel( int levelIndex );
    int GetEndIndexFromLevel( int levelIndex );
    void SnapPointToPlane( Vector const &vNormal, float dist, Vector &pt );
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetPower( int power )
{
    m_CoreDispInfo.SetPower( power );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CMapDisp::GetPower( void )
{
    return m_CoreDispInfo.GetPower();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CMapDisp::CalcPower( int width )
{
    switch( width )
    {
    case 5:
        return 2;
    case 9:
        return 3;
    case 17:
        return 4;
    default:
        return -1;
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CMapDisp::GetWidth( void )
{
    return m_CoreDispInfo.GetWidth();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CMapDisp::GetHeight( void ) 
{ 
    return m_CoreDispInfo.GetHeight();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CMapDisp::GetSize( void ) 
{ 
    return m_CoreDispInfo.GetSize();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetElevation( float elevation )
{
    m_CoreDispInfo.SetElevation( elevation );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline float CMapDisp::GetElevation( void )
{
    return m_CoreDispInfo.GetElevation();
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline float CMapDisp::GetScale( void )
{
    return m_Scale;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetBoundingBox( Vector& boxMin, Vector& boxMax )
{
    boxMin = m_BBox[0];
    boxMax = m_BBox[1];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline size_t CMapDisp::GetDataSize( void )
{
    return ( sizeof( CMapDisp ) );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CMapDisp::IsTouched( void )
{
    return m_CoreDispInfo.IsTouched();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetTouched( void )
{
    m_CoreDispInfo.SetTouched( true );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::ResetTouched( void )
{
    m_CoreDispInfo.SetTouched( false );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetHasMappingAxes( bool value )
{
    m_bHasMappingAxes = value;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetSubdivided( bool bSubdiv )
{
    m_bSubdiv = bSubdiv;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CMapDisp::IsSubdivided( void )
{
    return m_bSubdiv;
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetReSubdivision( bool bReSubdiv )
{
    m_bReSubdiv = bReSubdiv;
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CMapDisp::NeedsReSubdivision( void )
{
    return m_bReSubdiv;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetSurfPoint( int index, Vector& pt )
{
    CCoreDispSurface *pSurf = m_CoreDispInfo.GetSurface();
    pSurf->GetPoint( index, pt );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetSurfNormal( Vector& normal )
{
    CCoreDispSurface *pSurf = m_CoreDispInfo.GetSurface();
    pSurf->GetNormal( normal );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CMapDisp::GetSurfPointStartIndex( void )
{
    CCoreDispSurface *pSurf = m_CoreDispInfo.GetSurface();
    return pSurf->GetPointStartIndex();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetSurfPointStartIndex( int index )
{
    CCoreDispSurface *pSurf = m_CoreDispInfo.GetSurface();
    pSurf->SetPointStartIndex( index );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetSurfTexCoord( int ndx, Vector2D &texCoord )
{
    CCoreDispSurface *pSurf = m_CoreDispInfo.GetSurface();
    pSurf->GetTexCoord( ndx, texCoord );
}

    
//-----------------------------------------------------------------------------
//----------------------------------------------------------------------------- 
inline void CMapDisp::SetSurfTexCoord( int ndx, Vector2D const &texCoord )
{
    CCoreDispSurface *pSurf = m_CoreDispInfo.GetSurface();
    pSurf->SetTexCoord( ndx, texCoord );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetVert( int index, Vector const &v )
{
    m_CoreDispInfo.SetVert( index, v );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetVert( int index, Vector& v )
{
    m_CoreDispInfo.GetVert( index, v );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetAlpha( int index, float alpha )
{
    m_CoreDispInfo.SetAlpha( index, alpha );
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline float CMapDisp::GetAlpha( int index )
{
    return m_CoreDispInfo.GetAlpha( index );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetFlatVert( int index, Vector& v )
{
    m_CoreDispInfo.GetFlatVert( index, v );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetFlatVert( int index, const Vector &v )
{
    m_CoreDispInfo.SetFlatVert( index, v );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::ResetFieldVectors( void )
{
    m_CoreDispInfo.ResetFieldVectors();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetFieldVector( int index, Vector const &v )
{
    m_CoreDispInfo.SetFieldVector( index, v );
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetFieldVector( int index, Vector& v )
{
    m_CoreDispInfo.GetFieldVector( index, v );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::ResetSubdivPositions( void )
{
    m_CoreDispInfo.ResetSubdivPositions();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetSubdivPosition( int ndx, Vector const &v )
{
    m_CoreDispInfo.SetSubdivPosition( ndx, v );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetSubdivPosition( int ndx, Vector& v )
{
    m_CoreDispInfo.GetSubdivPosition( ndx, v );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::ResetSubdivNormals( void )
{
    m_CoreDispInfo.ResetSubdivNormals();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetSubdivNormal( int ndx, Vector const &v )
{
    m_CoreDispInfo.SetSubdivNormal( ndx, v );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetSubdivNormal( int ndx, Vector &v )
{
    m_CoreDispInfo.GetSubdivNormal( ndx, v );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::ResetFieldDistances( void )
{
    m_CoreDispInfo.ResetFieldDistances();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetFieldDistance( int index, float dist )
{
    m_CoreDispInfo.SetFieldDistance( index, dist );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline float CMapDisp::GetFieldDistance( int index )
{
    return m_CoreDispInfo.GetFieldDistance( index );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::ResetNeighbors( void )
{
    for( int i = 0; i < NUM_EDGES_CORNERS; i++ )
    {
        m_EdgeNeighbors[i] = EDITDISPHANDLE_INVALID;
        m_EdgeNeighborOrientations[i] = -1;

        m_CornerNeighborCounts[i] = 0;
        for( int j = 0; j < MAX_CORNER_NEIGHBORS; j++ )
        {
            m_CornerNeighbors[i][j] = EDITDISPHANDLE_INVALID;
            m_CornerNeighborOrientations[i][j] = -1;
        }
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::SetEdgeNeighbor( int direction, EditDispHandle_t handle, int orient )
{
    Assert( direction >= 0 );
    Assert( direction < NUM_EDGES_CORNERS );
    m_EdgeNeighbors[direction] = handle;
    m_EdgeNeighborOrientations[direction] = orient;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetEdgeNeighbor( int direction, EditDispHandle_t &handle, int &orient )
{
    Assert( direction >= 0 );
    Assert( direction < NUM_EDGES_CORNERS );
    handle = m_EdgeNeighbors[direction];
    orient = m_EdgeNeighborOrientations[direction];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline EditDispHandle_t CMapDisp::GetEdgeNeighbor( int direction )
{
    Assert( direction >= 0 );
    Assert( direction < NUM_EDGES_CORNERS );
    return m_EdgeNeighbors[direction];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::AddCornerNeighbor( int direction, EditDispHandle_t handle, int orient )
{
    Assert( direction >= 0 );
    Assert( direction < NUM_EDGES_CORNERS );
    if( m_CornerNeighborCounts[direction] >= MAX_CORNER_NEIGHBORS )
        return;

    m_CornerNeighbors[direction][m_CornerNeighborCounts[direction]] = handle;
    m_CornerNeighborOrientations[direction][m_CornerNeighborCounts[direction]] = orient;
    m_CornerNeighborCounts[direction]++;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CMapDisp::GetCornerNeighborCount( int direction )
{
    Assert( direction >= 0 );
    Assert( direction < NUM_EDGES_CORNERS );
    return m_CornerNeighborCounts[direction];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapDisp::GetCornerNeighbor( int direction, int cornerIndex, EditDispHandle_t &handle, int &orient )
{
    Assert( direction >= 0 );
    Assert( direction < NUM_EDGES_CORNERS );
    Assert( cornerIndex >= 0 );
    Assert( cornerIndex < MAX_CORNER_NEIGHBORS );

    handle = EDITDISPHANDLE_INVALID;
    orient = 0;
    
    if( cornerIndex >= m_CornerNeighborCounts[direction] )
        return;

    handle = m_CornerNeighbors[direction][cornerIndex];
    orient = m_CornerNeighborOrientations[direction][cornerIndex];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline EditDispHandle_t CMapDisp::GetCornerNeighbor( int direction, int cornerIndex )
{
    Assert( direction >= 0 );
    Assert( direction < NUM_EDGES_CORNERS );

    Assert( cornerIndex >= 0 );
    Assert( cornerIndex < MAX_CORNER_NEIGHBORS );

    if( cornerIndex >= m_CornerNeighborCounts[direction] )
        return NULL;

    return m_CornerNeighbors[direction][cornerIndex];
}

inline void CMapDisp::ResetTexelHitIndex( void ) { m_HitTexelIndex = -1; }
inline void CMapDisp::SetTexelHitIndex( int index ) { m_HitTexelIndex = index; }
inline int  CMapDisp::GetTexelHitIndex( void ) { return m_HitTexelIndex; }
inline void CMapDisp::SetDispMapHitIndex( int index ) { m_HitDispIndex = index; }
inline void CMapDisp::ResetDispMapHitIndex( void ) { m_HitDispIndex = -1; }

inline bool CMapDisp::Paint_IsDirty( void ) { return m_Canvas.m_bDirty; }

#endif // MAPDISP_H
