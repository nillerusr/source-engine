//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTALRENDERABLE_FLATBASIC_H
#define PORTALRENDERABLE_FLATBASIC_H

#ifdef _WIN32
#pragma once
#endif

#include "PortalRender.h"

struct PortalMeshPoint_t;
#define PORTALRENDERFIXMESH_OUTERBOUNDPLANES 12


struct FlatBasicPortalRenderingMaterials_t
{
	CMaterialReference	m_PortalMaterials[2];
	CMaterialReference	m_PortalRenderFixMaterials[2];
	CMaterialReference	m_PortalDepthDoubler;
	CMaterialReference	m_PortalStaticOverlay[2];
	CMaterialReference	m_Portal_Stencil_Hole;
	CMaterialReference	m_Portal_Refract[2];
	//CTextureReference	m_PortalLightTransfer_ShadowTexture; //light transfers disabled indefinitely

	IMaterialVar		*m_pDepthDoubleViewMatrixVar;
};

//As seen in "Portal"
class CPortalRenderable_FlatBasic : public C_BaseAnimating, public CPortalRenderable
{
	DECLARE_CLASS( CPortalRenderable_FlatBasic, C_BaseAnimating );

public:
	CPortalRenderable_FlatBasic( void );

	//generates a 8x6 tiled set of quads, each clipped to the view frustum. Helps vs11/ps11 portal shaders interpolate correctly. Not necessary for vs20/ps20 portal shaders or stencil mode.
	virtual void	DrawComplexPortalMesh( const IMaterial *pMaterialOverride = NULL, float fForwardOffsetModifier = 0.25f ); 
	
	//generates a single quad
	virtual void	DrawSimplePortalMesh( const IMaterial *pMaterialOverride = NULL, float fForwardOffsetModifier = 0.25f ); 
	
	//draws a screenspace mesh to replace missing pixels caused by the camera near plane intersecting the portal mesh
	virtual void	DrawRenderFixMesh( const IMaterial *pMaterialOverride = NULL, float fFrontClipDistance = 0.3f ); 

	//When we're in a configuration that sees through recursive portal views to a depth of 2, we should be able to cheaply approximate even further depth using pixels from previous frames
	virtual void	DrawDepthDoublerMesh( float fForwardOffsetModifier = 0.25f ); 

	virtual void	DrawPortal( void );

	virtual void	DrawPreStencilMask( void ); //Draw to wherever you can see through the portal. The mask will later be filled with the portal view.
	virtual void	DrawStencilMask( void ); //Draw to wherever you can see through the portal. The mask will later be filled with the portal view.
	virtual void	DrawPostStencilFixes( void ); //After done drawing to the portal mask, we need to fix the depth buffer as well as fog. So draw your mesh again, writing to z and with the fog color alpha'd in by distance

	virtual void	RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView );
	virtual void	RenderPortalViewToTexture( CViewRender *pViewRender, const CViewSetup &cameraView );

	void			AddToVisAsExitPortal( ViewCustomVisibility_t *pCustomVisibility );

	virtual bool	ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, CUtlVector<VPlane> &currentComplexFrustum ); //portal is both visible, and will display at least some portion of a remote view
	virtual bool	ShouldUpdatePortalView_BasedOnPixelVisibility( float fScreenFilledByStencilMaskLastFrame_Normalized );
	virtual bool	ShouldUpdateDepthDoublerTexture( const CViewSetup &viewSetup );

	virtual void	GetToolRecordingState( bool bActive, KeyValues *msg );
	virtual void	HandlePortalPlaybackMessage( KeyValues *pKeyValues );

	virtual const Vector &GetFogOrigin( void ) const { return m_ptOrigin; };
	virtual SkyboxVisibility_t	SkyBoxVisibleFromPortal( void ) { return m_InternallyMaintainedData.m_nSkyboxVisibleFromCorners; };
	virtual bool	DoesExitViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const;

	bool			WillUseDepthDoublerThisDraw( void ) const; //returns true if the DrawPortal() would draw a depth doubler mesh if you were to call it right now

	virtual CPortalRenderable *GetLinkedPortal() const { return m_pLinkedPortal; };
	bool			CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum );

protected:
	void			ClipFixToBoundingAreaAndDraw( PortalMeshPoint_t *pVerts, const IMaterial *pMaterial );
	void			Internal_DrawRenderFixMesh( const IMaterial *pMaterial );

	// renders a quad that simulates fog as an overlay for something else (most notably the hole we create for stencil mode portals)
	void			RenderFogQuad( void );

	void			PortalMoved( void ); // call this if you've moved the portal around so we can update internally maintained data


	static const FlatBasicPortalRenderingMaterials_t& m_Materials;
	struct FlatBasicPortal_InternalData_t
	{
		VPlane				m_BoundingPlanes[PORTALRENDERFIXMESH_OUTERBOUNDPLANES + 2]; // +2 for front and back

		VisOverrideData_t	m_VisData; // a data to use for visibility calculations (to override area portal culling)
		int					m_iViewLeaf; // leaf to start in for area portal flowing through calculations

		VMatrix				m_DepthDoublerTextureView; //cached version of view matrix at depth 1 for use when drawing the depth doubler mesh
		bool				m_bUsableDepthDoublerConfiguration; //every time a portal moves we re-evaluate whether the depth doubler will reasonably approximate more views
		SkyboxVisibility_t	m_nSkyboxVisibleFromCorners;

		Vector				m_ptForwardOrigin;
		Vector				m_ptCorners[4];

		float				m_fPlaneDist; //combines with m_vForward to make a plane
	};

	FlatBasicPortal_InternalData_t m_InternallyMaintainedData;

public:
	CPortalRenderable_FlatBasic	*m_pLinkedPortal;
	Vector			m_ptOrigin;
	Vector			m_vForward, m_vUp, m_vRight;
	float			m_fStaticAmount;
	float			m_fSecondaryStaticAmount; // used to help kludge the end of our recursive rendering chain
	float			m_fOpenAmount;	
	bool			m_bIsPortal2; //for any set of portals, one must be portal 1, and the other portal 2. Uses different render targets
};

#endif //#ifndef PORTALRENDERABLE_FLATBASIC_H