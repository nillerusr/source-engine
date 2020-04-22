//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"

#include "sourcevirtualreality.h"
#include "icommandline.h"
#include "filesystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "renderparm.h"
#include "openvr/openvr.h"

using namespace vr;

CSourceVirtualReality g_SourceVirtualReality;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSourceVirtualReality, ISourceVirtualReality,
	SOURCE_VIRTUAL_REALITY_INTERFACE_VERSION, g_SourceVirtualReality );

static VMatrix VMatrixFrom44(const float v[4][4]);
static VMatrix VMatrixFrom34(const float v[3][4]);
static VMatrix OpenVRToSourceCoordinateSystem(const VMatrix& vortex);

// --------------------------------------------------------------------
// Purpose: Set the current HMD pose as the zero pose
// --------------------------------------------------------------------
void CC_VR_Reset_Home_Pos( const CCommand& args )
{
	g_SourceVirtualReality.AcquireNewZeroPose();
}
static ConCommand vr_reset_home_pos("vr_reset_home_pos", CC_VR_Reset_Home_Pos, "Sets the current HMD position as the zero point" );


// --------------------------------------------------------------------
// Purpose: Reinitialize the IHeadtrack object
// --------------------------------------------------------------------
void CC_VR_Track_Reinit( const CCommand& args )
{
	if( g_SourceVirtualReality.ResetTracking() )
	{
		// Tracker can't be restarted: show a message, but don't quit.
		Warning("Can't reset HMD tracker");
	}
}
static ConCommand vr_track_reinit("vr_track_reinit", CC_VR_Track_Reinit, "Reinitializes HMD tracking" );


// Disable distortion processing altogether.
ConVar vr_distortion_enable ( "vr_distortion_enable", "1" );

// Disable distortion by changing the distortion texture itself, so that the rendering path is otherwise identical.
// This won't take effect until the texture is refresed.
ConVar vr_debug_nodistortion ( "vr_debug_nodistortion", "0" );

// Disable just the chromatic aberration correction in the distortion texture, to make undistort quality/artifacts
// easier to see and debug.  As above, won't take effect until the texture is refreshed.
ConVar vr_debug_nochromatic ( "vr_debug_nochromatic", "0" );

// Resolution of the undistort map.
static const int distortionTextureSize = 128;


void CC_vr_refresh_distortion_texture( const CCommand& args )
{
	g_SourceVirtualReality.RefreshDistortionTexture();
}
ConCommand vr_refresh_distortion_texture( "vr_refresh_distortion_texture", CC_vr_refresh_distortion_texture );

ConVar vr_use_offscreen_render_target( "vr_use_offscreen_render_target", "0", 0, "Experimental: Use larger offscreen render target for pre-distorted scene in VR" );

// --------------------------------------------------------------------
// construction/destruction
// --------------------------------------------------------------------
CSourceVirtualReality::CSourceVirtualReality()
	: m_textureGeneratorLeft( vr::Eye_Left ),
	m_textureGeneratorRight( vr::Eye_Right )
{
	m_bActive = false;
	m_bUsingOffscreenRenderTarget = false;
	m_pHmd = NULL;
}

CSourceVirtualReality::~CSourceVirtualReality()
{
}


// --------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------
bool			CSourceVirtualReality::Connect( CreateInterfaceFn factory )
{
	if ( !factory )
		return false;

	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !g_pFullFileSystem )
	{
		Warning( "The head tracker requires the filesystem to run!\n" );
		return false;
	}

	return true;
}


// --------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------
void			CSourceVirtualReality::Disconnect()
{
	BaseClass::Disconnect();
}


// --------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------
void *			CSourceVirtualReality::QueryInterface( const char *pInterfaceName )
{
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


// --------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------
InitReturnVal_t	CSourceVirtualReality::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// if our tracker expects to use the texture base distortion shader,
	// make the procedural textures for that shader now
	m_pDistortionTextureLeft.Init( materials->CreateProceduralTexture( "vr_distort_map_left", TEXTURE_GROUP_PIXEL_SHADERS,
		distortionTextureSize, distortionTextureSize, IMAGE_FORMAT_RGBA16161616,
		TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NODEBUGOVERRIDE |
		TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT ) );
	m_pDistortionTextureRight.Init( materials->CreateProceduralTexture( "vr_distort_map_right", TEXTURE_GROUP_PIXEL_SHADERS,
		distortionTextureSize, distortionTextureSize, IMAGE_FORMAT_RGBA16161616,
		TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NODEBUGOVERRIDE |
		TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT ) );
	m_pDistortionTextureLeft->SetTextureRegenerator( &m_textureGeneratorLeft );
	m_pDistortionTextureRight->SetTextureRegenerator( &m_textureGeneratorRight );

	return INIT_OK;
}

void CSourceVirtualReality::RefreshDistortionTexture()
{
	m_pDistortionTextureLeft->Download();
	m_pDistortionTextureRight->Download();
}


void CDistortionTextureRegen::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
{
	// only do this if we have an HMD
	if( !g_SourceVirtualReality.GetHmd() )
		return;

	unsigned short *imageData = (unsigned short*) pVTFTexture->ImageData( 0, 0, 0 );
	enum ImageFormat imageFormat = pVTFTexture->Format();
	if( imageFormat != IMAGE_FORMAT_RGBA16161616 )
	{
		return;
	}


	// we use different UVs for the full FB source texture
	float fUScale;
	float fUOffset;
	if( g_SourceVirtualReality.UsingOffscreenRenderTarget() )
	{
		fUScale = 1.f;
		fUOffset = 0.f;
	}
	else
	{
		fUScale = 0.5f;
		fUOffset = m_eEye == Eye_Left ? 0.f : 0.5f;
	}

	// optimize
	int width = pVTFTexture->Width();
	int height = pVTFTexture->Height();
	float fHeight = height;
	float fWidth = width;
	int x, y;
	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			int offset = 4 * ( x + y * width );
			assert( offset < width * height * 4 );

			float u = ( (float)x + 0.5f) / fWidth;
			float v = ( (float)y + 0.5f) / fHeight;

			DistortionCoordinates_t coords = g_SourceVirtualReality.GetHmd()->ComputeDistortion( m_eEye, u, v );

			coords.rfRed[0] = Clamp( coords.rfRed[0], 0.f, 1.f ) * fUScale + fUOffset;
			coords.rfGreen[0] = Clamp( coords.rfGreen[0], 0.f, 1.f ) * fUScale + fUOffset;
			coords.rfBlue[0] = Clamp( coords.rfBlue[0], 0.f, 1.f ) * fUScale + fUOffset;

			if ( vr_debug_nodistortion.GetBool() )
			{
				coords.rfRed[0] = coords.rfGreen[0] = coords.rfBlue[0] = u * fUScale + fUOffset;
				coords.rfRed[1] = coords.rfGreen[1] = coords.rfBlue[1] = v;
			}

			if ( vr_debug_nochromatic.GetBool() )
			{
				coords.rfRed[0] = coords.rfBlue[0] = coords.rfGreen[0];
				coords.rfRed[1] = coords.rfBlue[1] = coords.rfGreen[1];
			}

			imageData[offset + 0] = (unsigned short)(Clamp( coords.rfRed[0], 0.f, 1.f ) * 65535.f );
			imageData[offset + 1] = (unsigned short)(Clamp( coords.rfRed[1], 0.f, 1.f ) * 65535.f );
			imageData[offset + 2] = (unsigned short)(Clamp( coords.rfBlue[0], 0.f, 1.f ) * 65535.f  );
			imageData[offset + 3] = (unsigned short)(Clamp( coords.rfBlue[1], 0.f, 1.f ) * 65535.f );
		}
	}
}


// --------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------
void			CSourceVirtualReality::Shutdown()
{
	BaseClass::Shutdown();

	if( m_pHmd )
		VR_Shutdown();

	m_pDistortionTextureLeft.Shutdown();
	m_pDistortionTextureRight.Shutdown();
}


// --------------------------------------------------------------------
// Purpose: Let the caller know if we're in VR mode
// --------------------------------------------------------------------
bool CSourceVirtualReality::ShouldRunInVR()
{
	return m_bActive && m_pHmd;
}


// --------------------------------------------------------------------
// Purpose: Returns true if there's an Hmd connected and everything
//			started up.
// --------------------------------------------------------------------
bool CSourceVirtualReality::IsHmdConnected()
{
	// we really just care if OpenVR init was successful
	return EnsureOpenVRInited();
}


// --------------------------------------------------------------------
// Purpose: Let the caller know how big to make the window and where
//			to put it.
// --------------------------------------------------------------------
bool CSourceVirtualReality::GetDisplayBounds( VRRect_t *pRect )
{
	if( m_pHmd )
	{
		int32_t x, y;
		uint32_t width, height;
		m_pHmd->GetWindowBounds( &x, &y, &width, &height );
		pRect->nX = x;
		pRect->nY = y;
		pRect->nWidth = width;
		pRect->nHeight = height;
		return true;
	}
	else
	{
		return false;
	}
}

// --------------------------------------------------------------------
// Purpose: Allocates the pre-distortion render targets.
// --------------------------------------------------------------------
void CSourceVirtualReality::CreateRenderTargets( IMaterialSystem *pMaterialSystem )
{
	if( !m_pHmd || !m_bActive )
		return;

	g_StereoGuiTexture.Init( materials->CreateNamedRenderTargetTextureEx2(
		"_rt_gui",
		640, 480, RT_SIZE_OFFSCREEN,
		materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED,
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR )
		);

	if( UsingOffscreenRenderTarget() )
	{
		uint32_t nWidth, nHeight;
		m_pHmd->GetRecommendedRenderTargetSize( &nWidth, &nHeight );

		m_pPredistortRT.Init( pMaterialSystem->CreateNamedRenderTargetTextureEx2(
			"_rt_vr_predistort",
			nWidth, nHeight, RT_SIZE_LITERAL,
			IMAGE_FORMAT_RGBA8888,
			MATERIAL_RT_DEPTH_SEPARATE,
			TEXTUREFLAGS_RENDERTARGET |TEXTUREFLAGS_NOMIP/*TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT */,
			0 ) );

		//TODO: Figure out what I really want for the depth texture format
		m_pPredistortRTDepth.Init( pMaterialSystem->CreateNamedRenderTargetTextureEx2( "_rt_vr_predistort_depth", nWidth, nHeight,
			RT_SIZE_LITERAL, IMAGE_FORMAT_NV_DST24, MATERIAL_RT_DEPTH_NONE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT |TEXTUREFLAGS_NOMIP,
			0 ) );
	}

}

void CSourceVirtualReality::ShutdownRenderTargets()
{
	g_StereoGuiTexture.Shutdown();
	m_pPredistortRT.Shutdown();
	m_pPredistortRTDepth.Shutdown();
}


// Returns the (possibly overridden) framebuffer size for render target sizing.
void CSourceVirtualReality::GetRenderTargetFrameBufferDimensions( int & nWidth, int & nHeight )
{
	if( m_pHmd && UsingOffscreenRenderTarget() )
	{

		uint32_t w, h;
		m_pHmd->GetRecommendedRenderTargetSize( &w, &h );
		nWidth = w;
		nHeight = h;
	}
	else
	{
		// this will cause material system to fall back to the
		// actual size of the frame buffer
		nWidth = nHeight = 0;
	}
}


// --------------------------------------------------------------------
// Purpose: fetches the render target for the specified eye
// --------------------------------------------------------------------
ITexture *CSourceVirtualReality::GetRenderTarget( ISourceVirtualReality::VREye eEye, ISourceVirtualReality::EWhichRenderTarget eWhich )
{
	// we don't use any render targets if distortion is disabled
	// Just let the game render to the frame buffer.
	if( !vr_distortion_enable.GetBool() )
		return NULL;

	if( !m_bActive || !m_pHmd )
		return NULL;

	if( !UsingOffscreenRenderTarget() )
		return NULL;

	switch( eWhich )
	{
	case ISourceVirtualReality::RT_Color:
		return m_pPredistortRT;

	case ISourceVirtualReality::RT_Depth:
		return m_pPredistortRTDepth;
	}
	return NULL;
}

vr::Hmd_Eye SourceEyeToHmdEye( ISourceVirtualReality::VREye eEye )
{
	if( eEye == ISourceVirtualReality::VREye_Left )
		return vr::Eye_Left;
	else
		return vr::Eye_Right;
}


// --------------------------------------------------------------------
// Purpose: Let the caller know if we're in VR mode
// --------------------------------------------------------------------
void CSourceVirtualReality::GetViewportBounds( VREye eEye, int *pnX, int *pnY, int *pnWidth, int *pnHeight )
{
	if( !m_pHmd || !m_bActive )
	{
		*pnWidth = 0;
		*pnHeight = 0;
		return;
	}

	// if there are textures, use those
	if( m_pPredistortRT && vr_distortion_enable.GetBool() )
	{
		if( pnX && pnY )
		{
			*pnX = 0;
			*pnY = 0;
		}
		*pnWidth = m_pPredistortRT->GetActualWidth();
		*pnHeight = m_pPredistortRT->GetActualHeight();
	}
	else
	{
		uint32_t x, y, w, h;
		m_pHmd->GetEyeOutputViewport( SourceEyeToHmdEye( eEye ), &x, &y, &w, &h );
		if( pnX && pnY )
		{
			*pnX = x;
			*pnY = y;
		}
		*pnWidth = w;
		*pnHeight = h;
	}
}


// --------------------------------------------------------------------
// Purpose: Returns the current pose
// --------------------------------------------------------------------
VMatrix CSourceVirtualReality::GetMideyePose()
{
	return m_ZeroFromHeadPose;
}

// ----------------------------------------------------------------------
// Purpose: Create a 4x4 projection transform from eye projection and distortion parameters
// ----------------------------------------------------------------------
inline static void ComposeProjectionTransform(float fLeft, float fRight, float fTop, float fBottom, float zNear, float zFar, float fovScale, VMatrix *pmProj )
{
	if( fovScale != 1.0f  && fovScale > 0.f )
	{
		float fFovScaleAdjusted = tan( atan( fTop  ) / fovScale ) / fTop;
		fRight *= fFovScaleAdjusted;
		fLeft *= fFovScaleAdjusted;
		fTop *= fFovScaleAdjusted;
		fBottom *= fFovScaleAdjusted;
	}

	float idx = 1.0f / (fRight - fLeft);
	float idy = 1.0f / (fBottom - fTop);
	float idz = 1.0f / (zFar - zNear);
	float sx =  fRight + fLeft;
	float sy =  fBottom + fTop;

	float (*p)[4] = pmProj->m;
	p[0][0] = 2*idx; p[0][1] = 0;     p[0][2] = sx*idx;    p[0][3] = 0;
	p[1][0] = 0;     p[1][1] = 2*idy; p[1][2] = sy*idy;    p[1][3] = 0;
	p[2][0] = 0;     p[2][1] = 0;     p[2][2] = -zFar*idz; p[2][3] = -zFar*zNear*idz;
	p[3][0] = 0;     p[3][1] = 0;     p[3][2] = -1.0f;     p[3][3] = 0;
}


// ----------------------------------------------------------------------
// Purpose: Computes and returns the projection matrix for the eye
// ----------------------------------------------------------------------
bool CSourceVirtualReality::GetEyeProjectionMatrix ( VMatrix *pResult, VREye eEye, float zNear, float zFar, float fovScale )
{
	Assert ( pResult != NULL );
	if( !pResult || !m_pHmd || !m_bActive )
		return false;

	float fLeft, fRight, fTop, fBottom;
	m_pHmd->GetProjectionRaw( SourceEyeToHmdEye( eEye ), &fLeft, &fRight, &fTop, &fBottom );

	ComposeProjectionTransform( fLeft, fRight, fTop, fBottom, zNear, zFar, fovScale, pResult );
	return true;
}


// ----------------------------------------------------------------------
// Purpose: Returns the mid eye from left/right eye part of the view
// matrix transform chain.
// ----------------------------------------------------------------------
VMatrix CSourceVirtualReality::GetMidEyeFromEye( VREye eEye )
{
	if( m_pHmd )
	{
		vr::HmdMatrix34_t matMidEyeFromEye = m_pHmd->GetEyeToHeadTransform( SourceEyeToHmdEye( eEye ) );
		return OpenVRToSourceCoordinateSystem( VMatrixFrom34( matMidEyeFromEye.m ) );
	}
	else
	{
		VMatrix mat;
		mat.Identity();
		return mat;
	}
}

// returns the adapter index to use for VR mode
int CSourceVirtualReality::GetVRModeAdapter()
{
	if( EnsureOpenVRInited() )
	{
		Assert( m_pHmd );
		return m_pHmd->GetD3D9AdapterIndex();
	}
	else
	{
		return -1;
	}
}

bool CSourceVirtualReality::WillDriftInYaw()
{
	if( m_pHmd )
		return m_pHmd->GetBoolTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, Prop_WillDriftInYaw_Bool );
	else
		return false;
}

void CSourceVirtualReality::AcquireNewZeroPose()
{
	// just let the next tracker update re-zero us
	if( m_pHmd )
		m_pHmd->ResetSeatedZeroPose();
}

bool CSourceVirtualReality::SampleTrackingState ( float PlayerGameFov, float fPredictionSeconds )
{
	if( !m_pHmd || !m_bActive )
		return false;

	// If tracker can't return a pose (it's possibly recalibrating itself)
	// then we will freeze tracking at its current state, rather than
	// snapping it back to the zero position
	vr::TrackedDevicePose_t pose;

	if ( m_pHmd->IsTrackedDeviceConnected( k_unTrackedDeviceIndex_Hmd ) )
	{
		float fSecondsSinceLastVsync;
		m_pHmd->GetTimeSinceLastVsync( &fSecondsSinceLastVsync, NULL );

		float fFrameDuration = 1.f / m_pHmd->GetFloatTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd,
		                                                                    vr::Prop_DisplayFrequency_Float );
		float fPredictedSecondsFromNow = fFrameDuration - fSecondsSinceLastVsync \
		                                 + m_pHmd->GetFloatTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd,
		                                                                          vr::Prop_SecondsFromVsyncToPhotons_Float );

		// Use Seated here because everything using this interface or older is expecting a seated experience
		m_pHmd->GetDeviceToAbsoluteTrackingPose( vr::TrackingUniverseSeated, fPredictedSecondsFromNow, &pose, 1 );

		m_bHaveValidPose = pose.bPoseIsValid;
	}
	else
	{
		m_bHaveValidPose = false;
	}

	if( !m_bHaveValidPose )
		return false;

	m_ZeroFromHeadPose = OpenVRToSourceCoordinateSystem( VMatrixFrom34( pose.mDeviceToAbsoluteTracking.m ) );

	return true;
}


// ----------------------------------------------------------------------
// Purpose: Performs the distortion required for the HMD display
// ----------------------------------------------------------------------
bool CSourceVirtualReality::DoDistortionProcessing ( VREye eEye )
{
	if( !ShouldRunInVR() )
		return false;
	if ( !vr_distortion_enable.GetBool() )
	{
		return false;
	}

	CMatRenderContextPtr pRenderContext( materials );

	IMaterial	*pDistortMaterial;
	if( eEye == VREye_Left )
		pDistortMaterial = m_DistortLeftMaterial;
	else
		pDistortMaterial = m_DistortRightMaterial;

	if( !UsingOffscreenRenderTarget() )
	{
		// copy the frame buffer to the source texture
		ITexture	*pFullFrameFB1 = materials->FindTexture( "_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );
		if( !pFullFrameFB1 )
			return false;

		Rect_t	r;
		this->GetViewportBounds( eEye, &r.x, &r.y, &r.width, &r.height );
		pRenderContext->CopyRenderTargetToTextureEx( pFullFrameFB1, 0, &r, &r );
	}

	// This is where we are rendering to
	uint32_t x, y, w, h;
	m_pHmd->GetEyeOutputViewport( SourceEyeToHmdEye( eEye ), &x, &y, &w, &h );

	pRenderContext->DrawScreenSpaceRectangle (	pDistortMaterial,
		x, y, w, h,
		0, 0, distortionTextureSize-1,distortionTextureSize-1,distortionTextureSize,distortionTextureSize);

	return true;
}


// --------------------------------------------------------------------
// Pastes the HUD directly onto the backbuffer / render target, including
// applying the undistort.
// --------------------------------------------------------------------
bool CSourceVirtualReality::CompositeHud ( VREye eEye, float ndcHudBounds[4], bool bDoUndistort, bool bBlackout, bool bTranslucent )
{
	// run away if we're not doing VR at all
	if ( ! ShouldRunInVR() )
		return false;

	bDoUndistort = bDoUndistort && vr_distortion_enable.GetBool();

	IMaterial *pDistortHUDMaterial = ( eEye == VREye_Left ) ? m_DistortHUDLeftMaterial : m_DistortHUDRightMaterial;

	// The translucency flag will enable/disable both blending and alpha test.  The only case where we don't want them enabled
	// is when we're blacking out the entire screen (we use blending to smooth the edges of the HUD, and we use alpha test to kill
	// the pixels outside the HUD).  Note that right now I'm not expecting to see a mode with bTranslucent and bBlackout
	// both true (maybe happens in sniper mode?).
	pDistortHUDMaterial->SetMaterialVarFlag( MATERIAL_VAR_TRANSLUCENT, ! bBlackout );

	// The ndcHudBounds are the min x, min y, max x, max y of where we want to paste the HUD texture in NDC coordinates
	// of the main 3D view.  We conver to UV (0->1) space here for the shader.
	float huduvs[4];
	huduvs[0] = ndcHudBounds[0] * 0.5 + 0.5;
	huduvs[1] = ndcHudBounds[1] * 0.5 + 0.5;
	huduvs[2] = ndcHudBounds[2] * 0.5 + 0.5;
	huduvs[3] = ndcHudBounds[3] * 0.5 + 0.5;

	// Fix up coordinates depending on whether we're rendering to a buffer sized for one eye or two.
	// (note that disabling distortion also disables use of the offscreen render target)
	if ( vr_distortion_enable.GetBool() && ! UsingOffscreenRenderTarget() )
	{
		huduvs[0] *= 0.5;
		huduvs[2] *= 0.5;
		if ( eEye == VREye_Right )
		{
			huduvs[0] += 0.5;
			huduvs[2] += 0.5;
		}
	}

	IMaterialVar *pVar;

	pVar = pDistortHUDMaterial->FindVar( "$distortbounds", NULL );
	if ( pVar )
	{
		pVar->SetVecValue( huduvs, 4 );
	}

	pVar = pDistortHUDMaterial->FindVar( "$hudtranslucent", NULL );
	if ( pVar )
	{
		pVar->SetIntValue( bTranslucent );
	}

	pVar = pDistortHUDMaterial->FindVar( "$hudundistort", NULL );
	if ( pVar )
	{
		pVar->SetIntValue( bDoUndistort );
	}

	CMatRenderContextPtr pRenderContext( materials );

	uint32_t x, y, w, h;
	m_pHmd->GetEyeOutputViewport( SourceEyeToHmdEye( eEye ), &x, &y, &w, &h );

	pRenderContext->DrawScreenSpaceRectangle (	pDistortHUDMaterial,
		x, y, w, h,
		0, 0, distortionTextureSize-1,distortionTextureSize-1,distortionTextureSize,distortionTextureSize);

	return true;
}


bool CSourceVirtualReality::EnsureOpenVRInited()
{
	if( m_pHmd )
		return true;

	return StartTracker();
}

bool CSourceVirtualReality::StartTracker()
{
	Assert( m_pHmd == NULL );

	// Initialize SteamVR
	vr::HmdError err;
	m_pHmd = vr::VR_Init(  &err );
	if( err != HmdError_None )
	{
		Msg( "Unable to initialize HMD tracker. Error code %d\n", err );
		return false;
	}

	m_pHmd->ResetSeatedZeroPose();

	m_bHaveValidPose = false;
	m_ZeroFromHeadPose.Identity();

	return true;
}

void CSourceVirtualReality::StopTracker()
{
	if ( m_pHmd )
	{
		VR_Shutdown();
		m_pHmd = NULL;
	}
}

bool CSourceVirtualReality::ResetTracking()
{
	StopTracker();
	return StartTracker();
}


bool CSourceVirtualReality::Activate()
{
	// init the HMD itself
	if( !ResetTracking() )
		return false;

	m_bActive = true;
	m_bUsingOffscreenRenderTarget = vr_use_offscreen_render_target.GetBool();

	m_warpMaterial.Init( "dev/warp", "Other" );
	if( UsingOffscreenRenderTarget() )
	{
		m_DistortLeftMaterial.Init( "vr/vr_distort_texture_left", "Other" );
		m_DistortRightMaterial.Init( "vr/vr_distort_texture_right", "Other" );
	}
	else
	{
		m_DistortLeftMaterial.Init( "vr/vr_distort_texture_left_nort", "Other" );
		m_DistortRightMaterial.Init( "vr/vr_distort_texture_right_nort", "Other" );
	}
	m_InWorldUIMaterial.Init( "vgui/inworldui", "Other" );
	m_InWorldUIOpaqueMaterial.Init( "vgui/inworldui_opaque", "Other" );
	m_blackMaterial.Init( "vgui/black", "Other" );

	m_DistortHUDLeftMaterial.Init( "vr/vr_distort_hud_left", "Other" );
	m_DistortHUDRightMaterial.Init( "vr/vr_distort_hud_right", "Other" );

	RefreshDistortionTexture();
	return true;
}


void CSourceVirtualReality::Deactivate()
{
	m_bActive = false;
	m_bShouldForceVRMode = false;

	m_warpMaterial.Shutdown();
	m_DistortLeftMaterial.Shutdown();
	m_DistortRightMaterial.Shutdown();
	m_DistortHUDLeftMaterial.Shutdown();
	m_DistortHUDRightMaterial.Shutdown();
	m_InWorldUIMaterial.Shutdown();
	m_InWorldUIOpaqueMaterial.Shutdown();
	m_blackMaterial.Shutdown();
}

bool CSourceVirtualReality::ShouldForceVRMode()
{
	return m_bShouldForceVRMode;
}

void CSourceVirtualReality::SetShouldForceVRMode()
{
	m_bShouldForceVRMode = true;
}

static VMatrix OpenVRToSourceCoordinateSystem(const VMatrix& vortex)
{
	const float inchesPerMeter = (float)(39.3700787);

	// From Vortex: X=right, Y=up, Z=backwards, scale is meters.
	// To Source: X=forwards, Y=left, Z=up, scale is inches.
	//
	// s_from_v = [ 0 0 -1 0
	//             -1 0 0 0
	//              0 1 0 0
	//              0 0 0 1];
	//
	// We want to compute vmatrix = s_from_v * vortex * v_from_s; v_from_s = s_from_v'
	// Given vortex =
	// [00    01    02    03
	//  10    11    12    13
	//  20    21    22    23
	//  30    31    32    33]
	//
	// s_from_v * vortex * s_from_v' =
	//  22    20   -21   -23
	//  02    00   -01   -03
	// -12   -10    11    13
	// -32   -30    31    33
	//
	const vec_t (*v)[4] = vortex.m;
	VMatrix result(
		v[2][2],  v[2][0], -v[2][1], -v[2][3] * inchesPerMeter,
		v[0][2],  v[0][0], -v[0][1], -v[0][3] * inchesPerMeter,
		-v[1][2], -v[1][0],  v[1][1],  v[1][3] * inchesPerMeter,
		-v[3][2], -v[3][0],  v[3][1],  v[3][3]);

	return result;
}

static VMatrix VMatrixFrom44(const float v[4][4])
{
	return VMatrix(
		v[0][0], v[0][1], v[0][2], v[0][3],
		v[1][0], v[1][1], v[1][2], v[1][3],
		v[2][0], v[2][1], v[2][2], v[2][3],
		v[3][0], v[3][1], v[3][2], v[3][3]);
}

static VMatrix VMatrixFrom34(const float v[3][4])
{
	return VMatrix(
		v[0][0], v[0][1], v[0][2], v[0][3],
		v[1][0], v[1][1], v[1][2], v[1][3],
		v[2][0], v[2][1], v[2][2], v[2][3],
		0,       0,       0,       1       );
}

static VMatrix VMatrixFrom33(const float v[3][3])
{
	return VMatrix(
		v[0][0], v[0][1], v[0][2], 0,
		v[1][0], v[1][1], v[1][2], 0,
		v[2][0], v[2][1], v[2][2], 0,
		0,       0,       0,       1);
}
