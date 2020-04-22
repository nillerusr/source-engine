//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAPFACE_H
#define MAPFACE_H

#ifdef _WIN32
#pragma once
#endif


#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include "hammer_mathlib.h"
#include "MapAtom.h"
#include "DispManager.h"
#include "mathlib/Vector4d.h"
#include "utlvector.h"
#include "Color.h"
#include "smoothinggroupmgr.h"
#include "detailobjects.h"

class CCheckFaceInfo;
class IEditorTexture;
class CRender;
class CRender3D;
class CChunkFile;
class CSaveInfo;
class IMaterial;
class CMapWorld;
struct MapFaceRender_t;
class CMeshBuilder;
class IMesh;

struct LoadFace_t;

enum EditorRenderMode_t;
enum ChunkFileResult_t;

#define DEFAULT_TEXTURE_SCALE			0.25
#define DEFAULT_LIGHTMAP_SCALE			16

#define SMOOTHING_GROUP_MAX_COUNT		32
#define SMOOTHING_GROUP_DEFAULT			0

//
// Flags for CMapFace::CopyFrom.
//
#define COPY_FACE_PLANE			0x00000001			// Copies only the face's plane. Used for carving.
#define COPY_FACE_POINTS		0x00000002			// Copies the face's points and plane.


//
// Used for storing the extrema of a face. Each element of the Extents_t array
// contains a point that represents a local extreme along a particular dimension.
//
enum
{
	EXTENTS_XMIN = 0,
	EXTENTS_XMAX,
	EXTENTS_YMIN,
	EXTENTS_YMAX,
	EXTENTS_ZMIN,
	EXTENTS_ZMAX,
	NUM_EXTENTS_DIMS,
};

typedef Vector Extents_t[NUM_EXTENTS_DIMS];


struct PLANE
{
	Vector		normal;
	float		dist;
	Vector		planepts[3];
};


typedef struct
{
	int		numpoints;
	Vector	*p;			// variable sized
} winding_t;


enum FaceOrientation_t
{
	FACE_ORIENTATION_FLOOR = 0,
	FACE_ORIENTATION_CEILING,
	FACE_ORIENTATION_NORTH_WALL,
	FACE_ORIENTATION_SOUTH_WALL,
	FACE_ORIENTATION_EAST_WALL,
	FACE_ORIENTATION_WEST_WALL,
	FACE_ORIENTATION_INVALID
};


//
// Both an enumeration and bitflags. Used as bitflags when querying a face for its texture
// alignment because it could be world aligned and face aligned at the same time.
//
enum TextureAlignment_t
{
	TEXTURE_ALIGN_NONE	= 0x0000,
	TEXTURE_ALIGN_WORLD = 0x0001,
	TEXTURE_ALIGN_FACE	= 0x0002,
	TEXTURE_ALIGN_QUAKE = 0x0004
};


enum TextureJustification_t
{
	TEXTURE_JUSTIFY_NONE = 0,
	TEXTURE_JUSTIFY_TOP,
	TEXTURE_JUSTIFY_BOTTOM,
	TEXTURE_JUSTIFY_LEFT,
	TEXTURE_JUSTIFY_CENTER,
	TEXTURE_JUSTIFY_RIGHT,
	TEXTURE_JUSTIFY_FIT,
	TEXTURE_JUSTIFY_MAX
};


#define INIT_TEXTURE_FORCE			0x0001
#define INIT_TEXTURE_AXES			0x0002
#define INIT_TEXTURE_ROTATION		0x0004
#define INIT_TEXTURE_SHIFT			0x0008
#define INIT_TEXTURE_SCALE			0x0010
#define INIT_TEXTURE_ALL			(INIT_TEXTURE_AXES | INIT_TEXTURE_ROTATION | INIT_TEXTURE_SHIFT | INIT_TEXTURE_SCALE)


//
// Flags for CreateFace.
//
#define CREATE_FACE_PRESERVE_PLANE	0x0001		// Hack to prevent plane from being recalculated while building a solid from its planes.
#define CREATE_FACE_CLIPPING		0x0002

//
// Serialized data structure. Do not modify!
//
struct TEXTURE_21
{
	char	texture[MAX_PATH];
	float	rotate;
	float	shift[2];
	float	scale[2];
	BYTE	smooth;
	BYTE	material;
	DWORD	q2surface;
	DWORD	q2contents;
	DWORD	q2value;
};


//
// Post 2.1 explicit texture U/V axes were added.
//
// Serialized data structure. Do not modify!
//
struct TEXTURE_33
{
	char texture[MAX_PATH];
	float UAxis[4];				// Must remain float[4] for RMF serialization.
	float VAxis[4];				// Must remain float[4] for RMF serialization.
	float rotate;
	float scale[2];
	BYTE smooth;
	BYTE material;
	DWORD q2surface;
	DWORD q2contents;
	int nLightmapScale;
};


struct TEXTURE
{
	char texture[MAX_PATH];
	Vector4D UAxis;
	Vector4D VAxis;
	float rotate;
	float scale[2];
	BYTE smooth;
	BYTE material;
	DWORD q2surface;
	DWORD q2contents;
	int nLightmapScale;

	TEXTURE& operator=( TEXTURE const& src )
	{
		// necessary since operator= is private for UAxis
		memcpy( this, &src, sizeof(TEXTURE) );
		return *this;
	}
};


#define FACE_FLAGS_NOSHADOW 1
#define FACE_FLAGS_NODRAW_IN_LPREVIEW 2



class CMapFace : public CMapAtom
{
public:

	CMapFace(void);
	~CMapFace(void);

	// If bRescaleTextureCoordinates is true, then it will rescale and reoffset the texture coordinates
	// so that the texture is in the same apparent spot as the old texture (if they are different sizes).
	void SetTexture(const char *pszNewTex, bool bRescaleTextureCoordinates = false);
	void SetTexture(IEditorTexture *pTexture, bool bRescaleTextureCoordinates = false);
	void GetTextureName(char *pszName) const;

	inline IEditorTexture *GetTexture(void) const;
		
	// Renders opaque faces
	static void			AddFaceToQueue( CMapFace* pMapFace, IEditorTexture* pTexture, EditorRenderMode_t renderMode, bool selected, SelectionState_t faceSelectionState );
	static void			PushFaceQueue( void );
	static void			PopFaceQueue( void );
	static void			RenderOpaqueFaces( CRender3D* pRender );

	//
	// Serialization.
	//
	ChunkFileResult_t LoadVMF(CChunkFile *pFile);
	ChunkFileResult_t SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
	int SerializeRMF(std::fstream&, BOOL);
	int SerializeMAP(std::fstream&, BOOL);

	BOOL CheckFace(CCheckFaceInfo* = NULL);
	BOOL Fix(void);

	float GetNormalDistance(Vector& fPoint);

	inline int GetPointCount(void);
	inline void GetPoint(Vector& Point, int nPoint);

	inline void GetLightmapCoord( Vector2D & LightmapCoord, int nIndex );
	inline void SetLightmapCoord( const Vector2D &LightmapCoord, int nIndex );
	inline void GetTexCoord( Vector2D & TexCoord, int nTexCoord );

	// Texture alignment.
	void GetCenter(Vector& Center);
	FaceOrientation_t GetOrientation(void) const;
	void RotateTextureAxes(float fDegrees);
	void InitializeTextureAxes(TextureAlignment_t eAlignment, DWORD dwFlags);
	void JustifyTexture(TextureJustification_t eJustification);
	void JustifyTextureUsingExtents(TextureJustification_t eJustification, Extents_t Extents);
	void GetFaceBounds(Vector& pfMins, Vector& pfMaxs) const;
	void GetFaceExtents(Extents_t Extents) const;
	void GetTextureExtents(Extents_t Extents, Vector2D & TopLeft, Vector2D & BottomRight) const;
	int GetTextureAlignment(void) const;
	void GetFaceTextureExtents(Vector2D & TopLeft, Vector2D & BottomRight) const;

	void CalcTextureCoordAtPoint( const Vector& pt, Vector2D & texCoord );
	void CalcLightmapCoordAtPoint( const Vector& pt, Vector2D & lightCoord );

	// Returns the max lightmap size for this face
	int MaxLightmapSize() const;

	void NormalizeTextureShifts(void);
	BOOL IsTextureAxisValid(void) const;

	inline void SetCordonFace( bool bCordonFace );
	inline bool IsCordonFace() const;

	// Old code for setting up texture axes. Needed for backwards compatibility.
	void InitializeQuakeStyleTextureAxes(Vector4D& UAxis, Vector4D& VAxis);

	void CreateFace(Vector *pPoints, int nPoints, bool bIsCordonFace = false);
	void CreateFace(winding_t *w, int nFlags = 0);
	CMapFace *CopyFrom(const CMapFace *pFrom, DWORD dwFlags = COPY_FACE_POINTS, bool bUpdateDependencies = true );
	size_t AllocatePoints(int nPoints);

	void OnUndoRedo();

	void CalcPlane(void);
	void CalcPlaneFromFacePoints(void);

	void CalcTextureCoords();
	void OffsetTexture(const Vector &Delta);
	void SetTextureCoords(int nPoint, float u, float v);

	struct TangentSpaceAxes_t
	{
		Vector	tangent;
		Vector	binormal;
	};
		
	void CalcTangentSpaceAxes( void );
	bool AllocTangentSpaceAxes( int count );
	void FreeTangentSpaceAxes( void );

	void Render2D(CRender2D *pRender);
	void Render3D(CRender3D *pRender);
	void Render3DGrid(CRender3D *pRender);
	void RenderVertices(CRender *pRender);

	void OnAddToWorld(CMapWorld *pWorld);
	void OnRemoveFromWorld(void);

	static void SetShowSelection(bool bShowSelection);

	inline void SetRenderAlpha(unsigned char uchAlpha) { m_uchAlpha = uchAlpha; } // HACK: should be in CMapAtom

	inline void GetFaceNormal( Vector& normal );
	bool TraceLine(Vector &HitPos, Vector &HitNormal, Vector const &Start, Vector const &End);
	bool TraceLineInside( Vector &HitPos, Vector &HitNormal, Vector const &Start, Vector const &End, bool bNoDisp = false );

	inline void SetDisp( EditDispHandle_t handle, bool bDestroyPrevious = true );
	inline EditDispHandle_t GetDisp( void );
	inline bool HasDisp( void ) const;

	bool ShouldRenderLast();
	void GetDownVector( int index, Vector& downVect );

	bool GetRender2DBox( Vector& boundMin, Vector& boundMax );
	bool GetCullBox( Vector& boundMin, Vector& boundMax );
	size_t GetDataSize( void );

	inline int GetFaceID(void);
	inline void SetFaceID(int nFaceID);

	// Smoothing group.
	int SmoothingGroupCount( void );
	void AddSmoothingGroup( int iGroup );
	void RemoveSmoothingGroup( int iGroup );
	bool InSmoothingGroup( int iGroup );

	// Indicates this guy should be unlit
	void RenderUnlit( bool enable );

	// (begin serialized information
	TEXTURE texture;					// Texture info.
	Vector *Points;						// Array of face points, dynamically allocated.
	int	nPoints;						// The number of points in the array.
	// end serialized information)

	PLANE plane;

	int m_nFaceFlags;										// FACE_FLAGS_xx

	void DoTransform(const VMatrix &matrix);

	virtual void AddShadowingTriangles( CUtlVector<Vector> &tri_list );

	DetailObjects		*m_pDetailObjects;
	
protected:

	void ComputeColor( CRender3D* pRender, bool bRenderAsSelected, SelectionState_t faceSelectionState,
					   bool ignoreLighting, Color &pColor );

	void DrawFace( Color &pColor, EditorRenderMode_t mode );
	void RenderGridIfCloseEnough( CRender3D* pRender );
	void RenderTextureAxes( CRender3D* pRender );

	// Adds a face's vertices to the meshbuilder
	void AddFaceVertices( CMeshBuilder &builder, CRender3D* pRender, bool bRenderSelected, SelectionState_t faceSelectionState );

	// render texture axes
	static void RenderTextureAxes( CRender3D* pRender, int nCount, CMapFace **ppFaces );
	static void RenderGridsIfCloseEnough( CRender3D* pRender, int nCount, CMapFace **ppFaces );
	static void Render3DGrids( CRender3D* pRender, int nCount, CMapFace **ppFaces );
	static void RenderWireframeFaces( CRender3D* pRender, int nCount, MapFaceRender_t **ppFaces );
	static void RenderFacesBatch( CMeshBuilder &MeshBuilder, IMesh* pMesh, CRender3D* pRender, MapFaceRender_t **ppFaces, int nFaceCount, int nVertexCount, int nIndexCount, bool bWireframe );
	static void RenderFaces( CRender3D* pRender, int nCount, MapFaceRender_t **ppFaces );

	void RenderFace3D( CRender3D* pRender, EditorRenderMode_t renderMode, bool renderSelected, SelectionState_t faceSelectionState );

	//
	// Serialization (chunk handlers).
	//
	static ChunkFileResult_t LoadDispInfoCallback(CChunkFile *pFile, CMapFace *pFace);
	static ChunkFileResult_t LoadKeyCallback(const char *szKey, const char *szValue, LoadFace_t *pLoadFace);

	unsigned char m_uchAlpha;			// HACK: should be in CMapAtom

	int m_nFaceID;						// The unique ID of this face in the world.

	IEditorTexture *m_pTexture;				// Texture that is applied to this face.
	static IEditorTexture *m_pLightmapGrid;	// Lightmap grid texture for use in viewing lightmap scales.
	EditDispHandle_t	m_DispHandle;			// Displacement map applied to this face, NULL if none.

	static bool m_bShowFaceSelection;	// Whether to render faces with a special color when they are selected.

	Vector2D *m_pTextureCoords;			// An array of texture coordinates, one per face point.
	Vector2D *m_pLightmapCoords;			// An array of lightmap coordinates, one per face point.

	bool m_bIsCordonFace : 1;

	// should this be affected by lighting?
	bool m_bIgnoreLighting : 1;

	TangentSpaceAxes_t	*m_pTangentAxes;

	unsigned int		m_fSmoothingGroups;		// 32-bits representing 32 smoothing groups

	void UpdateFaceFlags( void );							// sniff face flags from texture
};


//-----------------------------------------------------------------------------
// Purpose: Returns the unique ID of this face.
//-----------------------------------------------------------------------------
inline int CMapFace::GetFaceID(void)
{
	return(m_nFaceID);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : TexCoord - 
//			nTexCoord - 
//-----------------------------------------------------------------------------
inline void CMapFace::GetLightmapCoord( Vector2D& LightmapCoord, int nIndex )
{
    Assert( nIndex < nPoints );
    LightmapCoord[0] = m_pLightmapCoords[nIndex][0];
    LightmapCoord[1] = m_pLightmapCoords[nIndex][1];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &LightmapCoord - 
//			nIndex - 
// Output : inline void
//-----------------------------------------------------------------------------
inline void CMapFace::SetLightmapCoord( const Vector2D &LightmapCoord, int nIndex )
{
    Assert( nIndex < nPoints );
	m_pLightmapCoords[nIndex][0] = LightmapCoord[0];
	m_pLightmapCoords[nIndex][1] = LightmapCoord[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : TexCoord - 
//			nTexCoord - 
//-----------------------------------------------------------------------------
inline void CMapFace::GetTexCoord( Vector2D& TexCoord, int nTexCoord )
{
    Assert( nTexCoord < nPoints );
    TexCoord[0] = m_pTextureCoords[nTexCoord][0];
    TexCoord[1] = m_pTextureCoords[nTexCoord][1];
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the texture that is applied to this face, NULL if none.
//-----------------------------------------------------------------------------
inline IEditorTexture *CMapFace::GetTexture(void) const
{
	return(m_pTexture);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of vertices that define this face.
//-----------------------------------------------------------------------------
inline int CMapFace::GetPointCount(void)
{
	return(nPoints);
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves a point on this face by its index.
// Input  : Point - Receives point coordinates.
//			nPoint - Index of point to retrieve.
//-----------------------------------------------------------------------------
inline void CMapFace::GetPoint(Vector& Point, int nPoint)
{
	Assert(nPoint < nPoints);
	Point = Points[nPoint];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapFace::GetFaceNormal( Vector& normal )
{
	normal = plane.normal;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the unique ID of this face.
//-----------------------------------------------------------------------------
inline void CMapFace::SetFaceID(int nID)
{
	m_nFaceID = nID;
}


//-----------------------------------------------------------------------------
// Purpose: Attaches a displacement surface to this face.
// Input  : handle - Displacement surface handle of surface attached to this face
//-----------------------------------------------------------------------------
inline void CMapFace::SetDisp( EditDispHandle_t handle, bool bDestroyPrevious )
{
	if ( ( m_DispHandle != EDITDISPHANDLE_INVALID ) && bDestroyPrevious )
	{
		// destroy old handle
		EditDispMgr()->Destroy( m_DispHandle );
	}
		
	Assert( ( handle == EDITDISPHANDLE_INVALID ) || ( EditDispMgr()->GetDisp( handle ) != NULL ) );

	m_DispHandle = handle;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the displacement surface applied to this face, 
//          DISPHANDLE_INVALID if none.
//-----------------------------------------------------------------------------
inline EditDispHandle_t CMapFace::GetDisp( void )
{
    return m_DispHandle;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this face has a displacement surface, false if not.
//-----------------------------------------------------------------------------
inline bool CMapFace::HasDisp( void ) const
{
	return ( m_DispHandle != EDITDISPHANDLE_INVALID );
}


//-----------------------------------------------------------------------------
// Whether this face belongs to a cordon brush.
//-----------------------------------------------------------------------------
inline void CMapFace::SetCordonFace( bool bCordonFace )
{
	m_bIsCordonFace = bCordonFace;
}


inline bool CMapFace::IsCordonFace() const
{
	return m_bIsCordonFace;
}


//-----------------------------------------------------------------------------
// Defines a container class for a list of face pointers.
//-----------------------------------------------------------------------------
class CMapFaceList : public CUtlVector<CMapFace *>
{
public:

	inline CMapFaceList(void) {}
	inline CMapFaceList(CMapFaceList const &other);
	inline CMapFaceList &CMapFaceList::operator =(CMapFaceList const &other);

	inline int FindFaceID(int nFaceID);
	void Intersect(CMapFaceList &IntersectWith, CMapFaceList &In, CMapFaceList &Out);
};


//-----------------------------------------------------------------------------
// Purpose: Copy constructor.
//-----------------------------------------------------------------------------
CMapFaceList::CMapFaceList(CMapFaceList const &other)
{
	*this = other;
}


//-----------------------------------------------------------------------------
// Purpose: Assignment operator for copying face lists.
// Input  : other - 
//-----------------------------------------------------------------------------
CMapFaceList &CMapFaceList::operator =(CMapFaceList const &other)
{
	AddVectorToTail(other);
	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: Searches the list for a face with the given ID.
// Input  : nFaceID - Numeric face ID to search for.
// Output : Index of found element, -1 if none.
//-----------------------------------------------------------------------------
int CMapFaceList::FindFaceID(int nFaceID)
{
	for (int i = 0; i < Count(); i++)
	{
		if ((Element(i) != NULL) && (Element(i)->GetFaceID() == nFaceID))
		{
			return(i);
		}
	}

	return(-1);
}


//-----------------------------------------------------------------------------
// Defines a container class for a list of face IDs.
//-----------------------------------------------------------------------------
class CMapFaceIDList : public CUtlVector<int>
{
public:

	inline CMapFaceIDList(void) {}
	inline CMapFaceIDList(CMapFaceIDList const &other);
	inline CMapFaceIDList &CMapFaceIDList::operator =(CMapFaceIDList const &other);

	void Intersect(CMapFaceIDList &IntersectWith, CMapFaceIDList &In, CMapFaceIDList &Out);
};


//-----------------------------------------------------------------------------
// Purpose: Copy constructor.
//-----------------------------------------------------------------------------
CMapFaceIDList::CMapFaceIDList(CMapFaceIDList const &other)
{
	*this = other;
}


//-----------------------------------------------------------------------------
// Purpose: Assignment operator for copying face ID lists.
// Input  : other - 
//-----------------------------------------------------------------------------
CMapFaceIDList &CMapFaceIDList::operator =(CMapFaceIDList const &other)
{
	AddVectorToTail(other);
	return *this;
}


#endif // MAPFACE_H
