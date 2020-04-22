//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_FUNC_LIQUIDPORTAL_H
#define C_FUNC_LIQUIDPORTAL_H

#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"
#include "PortalRender.h"
#include "Portal_DynamicMeshRenderingUtils.h"
#include "ScreenSpaceEffects.h"

class CPortalRenderable_Func_LiquidPortal : public CPortalRenderable
{
public:
	virtual void	DrawPreStencilMask( void );
	virtual void	DrawStencilMask( void );
	virtual void	DrawPostStencilFixes( void );
	
	virtual void	RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView );
	virtual void	RenderPortalViewToTexture( CViewRender *pViewRender, const CViewSetup &cameraView );
	
	void			AddToVisAsExitPortal( ViewCustomVisibility_t *pCustomVisibility );
	virtual bool	DoesExitViewIntersectWaterPlane( float waterZ, int leafWaterDataID ) const;
	virtual SkyboxVisibility_t	SkyBoxVisibleFromPortal( void );

	bool			CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum, const VPlane *pInputFrustum = NULL, int iInputFrustumPlaneCount = 0 );

	virtual const Vector&	GetFogOrigin( void ) const;
	virtual void			ShiftFogForExitPortalView() const;

	virtual bool	ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, Frustum currentFrustum ); //portal is both visible, and will display at least some portion of a remote view
	virtual CPortalRenderable* GetLinkedPortal() const;
	virtual bool	ShouldUpdateDepthDoublerTexture( const CViewSetup &viewSetup );
	virtual void	DrawPortal( void ); //sort of like what you'd expect to happen in C_BaseAnimating::DrawModel() if portals were fully compatible with models

	virtual void	GetToolRecordingState( bool bActive, KeyValues *msg );
	virtual void	HandlePortalPlaybackMessage( KeyValues *pKeyValues );

	void			DrawOutwardBox( const IMaterial *pMaterial = NULL );
	void			DrawInwardBox( const IMaterial *pMaterial = NULL );
	void			DrawInnerLiquid( bool bClipToBounds = true, float fOpacity = 1.0f, const IMaterial *pMaterial = NULL ); //quads in front of camera clipped to box dimensions

	float			GetFillInterpolationAmount( void );
	bool			IsFillingNow( void );

	void			UpdateBoundingPlanes( void );

	float m_fFillStartTime;
	float m_fFillEndTime;

	Vector m_vAABBMins;
	Vector m_vAABBMaxs;

	float m_fBoundingPlanes[5][4]; //top is highly variable and therefore not actually stored

	CPortalRenderable_Func_LiquidPortal *m_pLinkedPortal;
};

inline float CPortalRenderable_Func_LiquidPortal::GetFillInterpolationAmount( void )
{
	if( m_fFillEndTime < gpGlobals->curtime )
		return 0.0f;

	return ((gpGlobals->curtime - m_fFillStartTime) / (m_fFillEndTime - m_fFillStartTime));
}

inline bool CPortalRenderable_Func_LiquidPortal::IsFillingNow( void )
{
	return ((m_fFillEndTime >= gpGlobals->curtime) && (m_fFillStartTime <= gpGlobals->curtime));
}



class C_Func_LiquidPortal : public C_BaseEntity, public CPortalRenderable_Func_LiquidPortal
{
public:
	DECLARE_CLASS( C_Func_LiquidPortal, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_Func_LiquidPortal( void );
	~C_Func_LiquidPortal( void );

	virtual bool IsTransparent( void ) { return true; };
	virtual bool UsesPowerOfTwoFrameBufferTexture() { return true; };
	virtual int DrawModel( int flags );

	void ComputeLinkMatrix( void );

	virtual void OnDataChanged( DataUpdateType_t updateType );

	CHandle<C_Func_LiquidPortal> m_hLinkedPortal;
};





class CLiquidPortal_InnerLiquidEffect : public IScreenSpaceEffect
{
public:
	CLiquidPortal_InnerLiquidEffect( void );

	void Init( void ) { };
	void Shutdown( void ) { };

	void SetParameters( KeyValues *params );

	void Render( int x, int y, int w, int h );

	void Enable( bool bEnable ) { m_bEnable = bEnable; };
	bool IsEnabled( void ) { return m_bEnable; };

private:
	bool				m_bEnable;

public:
	C_Func_LiquidPortal* m_pImmersionPortal; //portal that did the filling + teleporting
	bool				m_bFadeBackToReality;
	float				m_fFadeBackTimeLeft;
	static const float	s_fFadeBackEffectTime; //how long the effect should take
};

#endif //#ifndef C_FUNC_LIQUIDPORTAL_H
