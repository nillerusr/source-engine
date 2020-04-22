//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPSOLID_H
#define MAPSOLID_H
#pragma once


#include "BlockArray.h"
#include "MapClass.h"
#include "MapFace.h"


enum TextureAlignment_t;
struct ExportDXFInfo_s;


//
// Flags for CreateFromPlanes:
//
#define CREATE_BUILD_PLANE_POINTS		0x0001		// Whether to regenerate the 3 plane points from the generated face points.
#define CREATE_FROM_PLANES_CLIPPING		0x0002

#define MAPSOLID_MAX_FACES				512         // Maximum number of faces a solid can have.


enum HL1_SolidType_t
{
	btSolid,
	btWater,
	btSlime,
	btLava
};


typedef BlockArray <CMapFace, 6, (MAPSOLID_MAX_FACES / 6) + 1> CSolidFaces;


class CMapSolid : public CMapClass
{
	friend CSSolid;

public:

	//
	// construction/deconstruction
	//
	DECLARE_MAPCLASS( CMapSolid, CMapClass );
	CMapSolid( CMapClass *Parent0 = NULL );
	~CMapSolid();

	//
	// Serialization.
	//
	static void PreloadWorld( void );
	static int GetBadSolidCount( void );
	virtual void PostloadWorld(CMapWorld *pWorld);
	ChunkFileResult_t LoadVMF( CChunkFile *pFile, bool &bValid );
	ChunkFileResult_t SaveVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo );
	int SerializeRMF( std::fstream &, BOOL );
	int SerializeMAP( std::fstream &, BOOL );

	//
	// Selection/Hit testing.
	//
	bool HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData);
	CMapClass *PrepareSelection(SelectMode_t eSelectMode);
		bool SaveDXF(ExportDXFInfo_s *pInfo);

	//
	// creation/copy/editing
	//
	int CreateFromPlanes(DWORD dwFlags = 0);
	void InitializeTextureAxes( TextureAlignment_t eAlignment, DWORD dwFlags );
	void CalcBounds( BOOL bFullUpdate = FALSE );
	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);
	int Split(PLANE *pPlane, CMapSolid **pFront = NULL, CMapSolid **pBack = NULL);
	bool Subtract(CMapObjectList *pInside, CMapObjectList *pOutside, CMapClass *pSubtractWith);

	virtual bool ShouldAppearInLightingPreview(void);
	virtual bool ShouldAppearInRaytracedLightingPreview(void);

	//
	// rendering
	//
	bool ShouldRenderLast();
	void Render3D(CRender3D *pRender);
	void Render2D(CRender2D *pRender);

	//
	// solid info
	//
	size_t GetSize();
	const char* GetDescription();
	inline bool IsValid( void ) { return( m_bValid ); }
	inline void SetValid( bool bValid ) { m_bValid = bValid; }
	void SetTexture( LPCTSTR pszTex, int iFace = -1 );
	LPCTSTR GetTexture( int iFace = -1 );
	bool HasDisp( void );
	virtual bool IsSolid( ) { return true; }

	//
	// Half-Life 1 solid types.
	//
	inline HL1_SolidType_t GetHL1SolidType(void) { return(m_eSolidType); }
	inline void SetHL1SolidType(HL1_SolidType_t eSolidType) { m_eSolidType = eSolidType; }
	HL1_SolidType_t HL1SolidTypeFromTextureName(const char *pszTexture);

	virtual bool IsScaleable(void) { return(true); }
	virtual bool IsVisualElement(void) { return(true); }

	// Overridden to set the render color of each of our faces.
	virtual void SetRenderColor(unsigned char uchRed, unsigned char uchGreen, unsigned char uchBlue);
	virtual void SetRenderColor(color32 rgbColor);

	//
	// face info
	//
	inline int GetFaceCount( void ) { return( Faces.GetCount() ); }
	inline void SetFaceCount( int nFaceCount ) { Faces.SetCount( nFaceCount ); }
	inline CMapFace *GetFace( int nFace ) { return( &Faces[nFace] ); }		
	int GetFaceIndex( CMapFace *pFace );	// Returns the index (you could use it with GetFace) or -1 if the face doesn't exist in this solid.
	void AddFace( CMapFace *pFace );
	void DeleteFace( int iIndex );
	CMapFace *FindFaceID(int nFaceID);

	//
	// Notifications.
	//
	virtual void OnAddToWorld(CMapWorld *pWorld);
	virtual void OnPreClone(CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
	virtual void OnPrePaste(CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList);
	virtual void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren);
	virtual void OnUndoRedo();

	inline bool IsCordonBrush() const;
	void SetCordonBrush(bool bSet);

	virtual void AddShadowingTriangles( CUtlVector<Vector> &tri_list );

#ifdef _DEBUG
	void DebugSolid(void);
#endif // _DEBUG

protected:

	void GenerateNewFaceIDs(CMapWorld *pWorld);

	void PickRandomColor();
	color32 GetLineColor( CRender2D *pRender );

	//
	// Implements CMapAtom transformation functions.
	//
	void DoTransform(const VMatrix &matrix);
		
	// dvs: brought in from old carve code; should be reconciled with AddFace, Split, Subtract
	bool AddPlane(const CMapFace *p);
	bool Carve(CMapObjectList *pInside, CMapObjectList *pOutside, CMapSolid *pCarver);
	void ClipByFace(const CMapFace *fa, CMapSolid **f, CMapSolid **b);
	void RemoveEmptyFaces(void);
	
	//
	// Serialization.
	//
	static ChunkFileResult_t LoadSideCallback(CChunkFile *pFile, CMapSolid *pSolid);
	ChunkFileResult_t SaveEditorData(CChunkFile *pFile);
	static int g_nBadSolidCount;

	CSolidFaces Faces;					// The list of faces on this solid.	

	bool m_bValid : 1;						// Is it a proper convex solid?
	bool m_bIsCordonBrush : 1;				// Whether this brush was added by the cordon tool.

	HL1_SolidType_t m_eSolidType;		// Used for HalfLife 1 maps only - solid, water, slime, lava.
};


inline bool CMapSolid::IsCordonBrush() const
{
	return m_bIsCordonBrush;
}

#endif // MAPSOLID_H
