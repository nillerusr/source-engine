//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The implementation of ISourceVirtualReality, which provides utility
//			functions for VR including head tracking, window/viewport information,
//			rendering information, and distortion
//
//=============================================================================

#ifndef SOURCEVIRTUALREALITY_H
#define SOURCEVIRTUALREALITY_H
#if defined( _WIN32 )
#pragma once
#endif

#include "tier3/tier3.h"
#include "sourcevr/isourcevirtualreality.h"
#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "openvr/openvr.h"


// this is a callback so we can regenerate the distortion texture whenever we need to
class CDistortionTextureRegen : public ITextureRegenerator
{
public:
	CDistortionTextureRegen( vr::Hmd_Eye eEye ) : m_eEye( eEye ) {}
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect ) OVERRIDE;
	virtual void Release()  OVERRIDE {}

private:
	vr::Hmd_Eye m_eEye;
};

//-----------------------------------------------------------------------------
// The implementation
//-----------------------------------------------------------------------------


class CSourceVirtualReality: public CTier3AppSystem< ISourceVirtualReality >
{
	typedef CTier3AppSystem< ISourceVirtualReality > BaseClass;

public:

	CSourceVirtualReality();
	~CSourceVirtualReality();

	//---------------------------------------------------------
	// Initialization and shutdown
	//---------------------------------------------------------

	//
	// IAppSystem
	//
	virtual bool							Connect( CreateInterfaceFn factory );
	virtual void							Disconnect();
	virtual void *							QueryInterface( const char *pInterfaceName );

	// these will come from the engine
	virtual InitReturnVal_t					Init();
	virtual void							Shutdown();


	//---------------------------------------------------------
	// ISourceVirtualReality implementation
	//---------------------------------------------------------
	virtual bool ShouldRunInVR() OVERRIDE;
	virtual bool IsHmdConnected() OVERRIDE;
	virtual void GetViewportBounds( VREye eEye, int *pnX, int *pnY, int *pnWidth, int *pnHeight ) OVERRIDE;
	virtual bool DoDistortionProcessing ( VREye eEye ) OVERRIDE;
	virtual bool CompositeHud ( VREye eEye, float ndcHudBounds[4], bool bDoUndistort, bool bBlackout, bool bTranslucent ) OVERRIDE;
	virtual VMatrix GetMideyePose() OVERRIDE;
	virtual bool SampleTrackingState ( float PlayerGameFov, float fPredictionSeconds ) OVERRIDE;
	virtual bool GetEyeProjectionMatrix ( VMatrix *pResult, VREye, float zNear, float zFar, float fovScale ) OVERRIDE;
	virtual bool WillDriftInYaw() OVERRIDE;
	virtual bool GetDisplayBounds( VRRect_t *pRect ) OVERRIDE;
	virtual VMatrix GetMidEyeFromEye( VREye eEye ) OVERRIDE;
	virtual int GetVRModeAdapter() OVERRIDE;
	virtual void CreateRenderTargets( IMaterialSystem *pMaterialSystem ) OVERRIDE;
	virtual void ShutdownRenderTargets() OVERRIDE;
	virtual ITexture *GetRenderTarget( VREye eEye, EWhichRenderTarget eWhich ) OVERRIDE;
	virtual void GetRenderTargetFrameBufferDimensions( int & nWidth, int & nHeight ) OVERRIDE;
	virtual bool Activate() OVERRIDE;
	virtual void Deactivate() OVERRIDE;
	virtual bool ShouldForceVRMode( ) OVERRIDE;
	virtual void SetShouldForceVRMode( ) OVERRIDE;

	void RefreshDistortionTexture();
	void AcquireNewZeroPose();

	bool StartTracker();
	void StopTracker();	
	bool ResetTracking();	// Called to reset tracking

	// makes sure we've initialized OpenVR so we can use
	// m_pHmd
	bool EnsureOpenVRInited();

	// Prefer this to the convar so that convar will stick for the entire 
	// VR activation. We can't lazy-crate render targets and don't
	// want to create the "just in case" somebody turns on this experimental 
	// mode
	bool UsingOffscreenRenderTarget() const { return m_bUsingOffscreenRenderTarget; }

	vr::IVRSystem * GetHmd() { return m_pHmd; }
private:
	bool m_bActive;
	bool m_bShouldForceVRMode;
	bool m_bUsingOffscreenRenderTarget; 
	CDistortionTextureRegen m_textureGeneratorLeft;
	CDistortionTextureRegen m_textureGeneratorRight;
	CTextureReference g_StereoGuiTexture;
	CTextureReference m_pDistortionTextureLeft;
	CTextureReference m_pDistortionTextureRight;
	CTextureReference m_pPredistortRT;
	CTextureReference m_pPredistortRTDepth;
	CMaterialReference m_warpMaterial;
	CMaterialReference m_DistortLeftMaterial;
	CMaterialReference m_DistortRightMaterial;
	CMaterialReference m_DistortHUDLeftMaterial;
	CMaterialReference m_DistortHUDRightMaterial;
	CMaterialReference m_InWorldUIMaterial;
	CMaterialReference m_InWorldUIOpaqueMaterial;
	CMaterialReference m_blackMaterial;

	vr::IVRSystem				*m_pHmd;

	bool	  m_bHaveValidPose;
	VMatrix   m_ZeroFromHeadPose;

};


#endif // SOURCEVIRTUALREALITY_H
