//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPOVERLAY_H
#define MAPOVERLAY_H
#pragma once

#include <afxwin.h>
#include "utlvector.h"
#include "MapSideList.h"

class CHelperInfo;
class CMapFace;
class CRender3D;
class CMapView;
class IEditorTexture;

#define OVERLAY_HANDLES_COUNT	4

#define NUM_CLIPFACE_TEXCOORDS	2

#define OVERLAY_TYPE_GENERIC	0x01
#define OVERLAY_TYPE_SHORE		0x02

//=============================================================================
//
// Class Map Overlay
//
class CMapOverlay : public CMapSideList
{
public:

	DECLARE_MAPCLASS( CMapOverlay, CMapSideList );

	// Construction/Deconstruction.
	CMapOverlay();
	~CMapOverlay();

	// Factory for building from a list of string parameters.
	static CMapClass *CreateMapOverlay( CHelperInfo *pInfo, CMapEntity *pParent );

	// Virtual/Interface Implementation.
	virtual void PostloadWorld( CMapWorld *pWorld );

	virtual CMapClass *Copy( bool bUpdateDependencies );
	virtual CMapClass *CopyFrom( CMapClass *pObject, bool bUpdateDependencies );

	void CalcBounds( BOOL bFullUpdate = FALSE );

	virtual void OnParentKeyChanged( const char* szKey, const char* szValue );
	virtual void OnNotifyDependent( CMapClass *pObject, Notify_Dependent_t eNotifyType );

	void DoTransform( const VMatrix &matrix );
	
	void OnPaste( CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, 
				  const CMapObjectList &OriginalList, CMapObjectList &NewList);
	void OnClone( CMapClass *pClone, CMapWorld *pWorld, 
				  const CMapObjectList &OriginalList, CMapObjectList &NewList );
	void OnUndoRedo( void );

	void Render3D( CRender3D *pRender );

	// Overlay.
	void HandlesReset( void );
	bool HandlesHitTest( CMapView *pView, const Vector2D &vPoint );
	void HandlesDragTo( Vector &vecImpact, CMapFace *pFace );
	void HandleMoveTo( int iHandle, Vector &vecPoint, CMapFace *pFace );
	void SetTexCoords( Vector2D vecTexCoords[4] );
	void GetHandlePos( int iHandle, Vector &vecPos );
	bool IsSelected( void )									{ return ( GetSelectionState() == SELECT_NORMAL ); }

	void DoClip( void );
	void CenterEntity( void );

	void GetPlane( cplane_t &plane );

	int	GetFaceCount( void ) { return m_Faces.Count(); }
	CMapFace *GetFace( int iFace ) { return m_Faces.Element( iFace ); }

	void SetMaterial( const char *szMaterialName );
	void SetMaterial( IEditorTexture *pTexture )		{ m_Material.m_pTexture = pTexture; }
	IEditorTexture* GetMaterial()		{ return m_Material.m_pTexture; }

	// Creation.
	void Basis_Init( CMapFace *pFace );
	void Handles_Init( CMapFace *pFace );
	void SideList_Init( CMapFace *pFace );
	void SideList_AddFace( CMapFace *pFace );
	void SetLoaded( bool bLoaded )				{ m_bLoaded = bLoaded; }

	// Attributes.
	inline virtual bool IsVisualElement( void ) { return true; }
	inline virtual bool ShouldRenderLast( void ) { return true; }
	inline const char* GetDescription() { return ( "Overlay" ); }
	void SetOverlayType( unsigned short uiType )			{ m_uiFlags |= uiType; }
	void ResetOverlayType( unsigned short uiType )			{ m_uiFlags &= ~uiType; }
	unsigned short GetOverlayType( void )					{ return m_uiFlags; }

	ChunkFileResult_t SaveDataToVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo );

private:

	//=========================================================================
	//
	// Basis Data
	//
	struct Basis_t
	{
		CMapFace	*m_pFace;					// Index to the face the basis were derived from	
		Vector		m_vecOrigin;				// Origin of basis vectors (in plane)
		Vector		m_vecAxes[3];				// Basis vectors
		int			m_nAxesFlip[3];				// u, v, n flip values (3 bits x,y,z) - saved off the z components of the first 3 uv points
	};

	void Basis_Clear( void );
	void Basis_SetFace( CMapFace *pFace );
	void Basis_UpdateOrigin( void );
	void Basis_BuildAxes( void );
	void Basis_SetInitialUAxis( Vector const &vecNormal );
	bool Basis_IsValid( void );
	void Basis_Copy( Basis_t *pSrc, Basis_t *pDst );
	void Basis_UpdateParentKey( void );

	// Legacy support.
	void Basis_BuildFromSideList( void );
	void Basis_ToggleAxesFlip( int iAxis, int iComponent );
	bool Basis_IsFlipped( int iAxis, int iComponent );

	//=========================================================================
	//
	// Handle Data
	//		
	struct Handles_t
	{
		int			m_iHit;									 // Index of the selected handle
		Vector2D	m_vecBasisCoords[OVERLAY_HANDLES_COUNT]; // U,V coordinates of the 4 corners in the editable plane (use basis)
		Vector		m_vec3D[OVERLAY_HANDLES_COUNT];			 // World space handles for snap testing
	};
		
	void Handles_Clear( void );
	void Handles_Build3D();
	void Handles_Render3D( CRender3D *pRender );
	void Handles_SurfToOverlayPlane( CMapFace *pFace, Vector const &vSurf, Vector &vPoint );
	void Handles_Copy( Handles_t *pSrc, Handles_t *pDst );
	void Handles_UpdateParentKey( void );
	void Handles_FixOrder();

	//=========================================================================
	//
	// ClipFace Data
	//		
	struct BlendData_t
	{
		void Init()
		{
			m_nType = 0;
			memset(m_iPoints, 0, sizeof(m_iPoints));
			memset(m_flBlends, 0, sizeof(m_flBlends));
		}
		
		int		m_nType;			// type of blend (point, edge, barycentric)
		short	m_iPoints[3];		// displacement point indices
		float	m_flBlends[3];		// blending values
	};

	struct ClipFace_t
	{
		CMapFace					*m_pBuildFace;
		int							m_nPointCount;
		CUtlVector<Vector>			m_aPoints;
		CUtlVector<Vector>			m_aDispPointUVs;		// z is always 0 (need to be this way to share functions!)
		CUtlVector<Vector2D>		m_aTexCoords[NUM_CLIPFACE_TEXCOORDS];
		CUtlVector<BlendData_t>		m_aBlends;
		
		ClipFace_t()
		{
			m_pBuildFace = NULL;
			m_nPointCount = 0;
		}

		~ClipFace_t()				
		{ 
			m_aPoints.Purge(); 
			m_aDispPointUVs.Purge(); 
			m_aBlends.Purge();

			for ( int iCoord = 0; iCoord < NUM_CLIPFACE_TEXCOORDS; ++iCoord )
			{
				m_aTexCoords[iCoord].Purge();
			}
		}
	};

	typedef CUtlVector<ClipFace_t*> ClipFaces_t;

	ClipFace_t *ClipFace_Create( int nSize );
	void		ClipFace_Destroy( ClipFace_t **ppClipFace );
	ClipFace_t *ClipFace_Copy( ClipFace_t *pSrc );

	void		ClipFace_GetBounds( ClipFace_t *pClipFace, Vector &vecMin, Vector &vecMax );

	void		ClipFace_Clip( ClipFace_t *pClipFace, cplane_t *pClipPlane, float flEpsilon, ClipFace_t **ppFront, ClipFace_t **ppBack );
	void		ClipFace_ClipBarycentric( ClipFace_t *pClipFace, cplane_t *pClipPlane, float flEpsilon, int iClip, CMapDisp *pDisp, ClipFace_t **ppFront, ClipFace_t **ppBack );
	void		ClipFace_PreClipDisp( ClipFace_t *pClipFace, CMapDisp *pDisp );
	void		ClipFace_PostClipDisp( void );
	void		ClipFace_ResolveBarycentricClip( CMapDisp *pDisp, ClipFace_t *pClipFace, int iClipFacePoint, const Vector2D &vecPointUV, float *pCoefs, int *pTris, Vector2D *pVertsUV );
	bool		ClipFace_CalcBarycentricCooefs( CMapDisp *pDisp, Vector2D *pVertsUV, const Vector2D &vecPointUV, float *pCoefs );

	int			ClipFace_GetAxisType( cplane_t *pClipPlane );
	void		ClipFace_BuildBlend( ClipFace_t *pClipFace, CMapDisp *pDisp, cplane_t *pClipPlane, int iClip, const Vector &vecUV, const Vector &vecPoint );
	void		ClipFace_CopyBlendFrom( ClipFace_t *pClipFace, BlendData_t *pBlendFrom );
	void		ClipFace_BuildFacesFromBlendedData( ClipFace_t *pClipFace );

	//=========================================================================
	//
	// Material Functions
	//
	struct Material_t
	{
		IEditorTexture				*m_pTexture;		// material
		Vector2D					m_vecTextureU;		// material starting and ending U
		Vector2D					m_vecTextureV;		// material starting and ending V
	};

	void Material_Clear( void );
	void Material_Copy( Material_t *pSrc, Material_t *pDst );
	void Material_TexCoordInit( void );
	void Material_UpdateParentKey( void );

	//=========================================================================
	//
	// Clipping
	//
	void PreClip( void );
	void PostClip( void );
	void DoClipFace( CMapFace *pFace );
	void DoClipDisp( CMapFace *pFace, ClipFace_t *pClippedFace );
	void DoClipDispInV( CMapDisp *pDisp, ClipFaces_t &aCurrentFaces );
	void DoClipDispInU( CMapDisp *pDisp, ClipFaces_t &aCurrentFaces );
	void DoClipDispInUVFromTLToBR( CMapDisp *pDisp, ClipFaces_t &aCurrentFaces );
	void DoClipDispInUVFromBLToTR( CMapDisp *pDisp, ClipFaces_t &aCurrentFaces );

	void Disp_ClipFragments( CMapDisp *pDisp, ClipFaces_t &aDispFragments );
	void Disp_DoClip( CMapDisp *pDisp, ClipFaces_t &aDispFragments, cplane_t &clipPlane, float clipDistStart, int nInterval, int nLoopStart, int nLoopEnd, int nLoopInc );

	//==========================================================================
	//
	// Transform
	//
		
	// Utility
	void OverlayUVToOverlayPlane( const Vector2D &vecUV, Vector &vecPoint );
	void OverlayPlaneToOverlayUV( const Vector &vecPoint, Vector2D &vecUV );
	void WorldToOverlayPlane( const Vector &vecWorld, Vector &vecPoint );
	void OverlayPlaneToWorld( CMapFace *pFace, const Vector &vecPlane, Vector &vecWorld );
	void OverlayPlaneToSurfFromList( const Vector &vecOverlayPoint, Vector &vecSurfPoint );
	bool EntityOnSurfFromListToBaseFacePlane( const Vector &vecWorldPoint, Vector &vecBasePoint );

	bool BuildEdgePlanes( Vector const *pPoints, int pointCount, cplane_t *pEdgePlanes, int edgePlaneCount );
	void UpdateDispBarycentric( void );
	void PostModified( void );
	void GetTriVerts( CMapDisp *pDisp, const Vector2D &vecSurfUV, int *pTris, Vector2D *pVertsUV );

private:

	Basis_t			m_Basis;			// Overlay Basis Data
	Handles_t		m_Handles;			// Overlay Handle Data
	Material_t		m_Material;			// Overlay Material

	ClipFace_t		*m_pOverlayFace;	// Primary Overlay
	ClipFaces_t		m_aRenderFaces;		// Clipped Face Cache (Render Faces)

	unsigned short	m_uiFlags;			//
	bool			m_bLoaded;
};


#endif // MAPOVERLAY_H
