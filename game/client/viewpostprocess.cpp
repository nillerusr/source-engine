//========== Copyright © 2008, Valve Corporation, All rights reserved. ==========
//
// Purpose:
//
//==============================================================================

#include "cbase.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "tier1/callqueue.h"
#include "colorcorrectionmgr.h"
#include "view_scene.h"
#include "c_world.h"
#include "renderparm.h"
#include "shaderapi/ishaderapi.h"
#include "ProxyEntity.h"
#include "imaterialproxydict.h"
#include "model_types.h"



// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// mapmaker controlled autoexposure
bool g_bUseCustomAutoExposureMin = false;
bool g_bUseCustomAutoExposureMax = false;
bool g_bUseCustomBloomScale = false;
float g_flCustomAutoExposureMin = 0;
float g_flCustomAutoExposureMax = 0;
float g_flCustomBloomScale = 0.0f;
float g_flCustomBloomScaleMinimum = 0.0f;

extern void GetTonemapSettingsFromEnvTonemapController( void );

// mapmaker controlled depth of field
bool  g_bDOFEnabled = false;
float g_flDOFNearBlurDepth = 50.0f;
float g_flDOFNearFocusDepth = 200.0f;
float g_flDOFFarFocusDepth = 250.0f;
float g_flDOFFarBlurDepth = 1000.0f;
float g_flDOFNearBlurRadius = 0.0f;
float g_flDOFFarBlurRadius = 5.0f;

bool g_bFlashlightIsOn = false;

// hdr parameters
ConVar mat_bloomscale( "mat_bloomscale", "1" );

ConVar mat_hdr_level( "mat_hdr_level", "2" );
ConVar mat_bloomamount_rate( "mat_bloomamount_rate", "0.05f", FCVAR_CHEAT );
static ConVar debug_postproc( "mat_debug_postprocessing_effects", "0", FCVAR_NONE, "0 = off, 1 = show post-processing passes in quadrants of the screen, 2 = only apply post-processing to the centre of the screen" );
static ConVar mat_dynamic_tonemapping( "mat_dynamic_tonemapping", "1", FCVAR_CHEAT );
static ConVar mat_tonemapping_occlusion_use_stencil( "mat_tonemapping_occlusion_use_stencil", "0" );

static ConVar mat_autoexposure_max( "mat_autoexposure_max", "2" );
static ConVar mat_autoexposure_min( "mat_autoexposure_min", "0.5" );
static ConVar mat_show_histogram( "mat_show_histogram", "0" );
ConVar mat_hdr_uncapexposure( "mat_hdr_uncapexposure", "0", FCVAR_CHEAT );
ConVar mat_force_bloom("mat_force_bloom","0", FCVAR_CHEAT);

ConVar mat_disable_bloom("mat_disable_bloom","0");
ConVar mat_debug_bloom("mat_debug_bloom","0", FCVAR_CHEAT);
ConVar mat_colorcorrection( "mat_colorcorrection", "1", FCVAR_DEVELOPMENTONLY );

ConVar mat_accelerate_adjust_exposure_down( "mat_accelerate_adjust_exposure_down", "3.0", FCVAR_CHEAT );
ConVar mat_hdr_manual_tonemap_rate( "mat_hdr_manual_tonemap_rate", "1.0" );

// fudge factor to make non-hdr bloom more closely match hdr bloom. Because of auto-exposure, high
// bloomscales don't blow out as much in hdr. this factor was derived by comparing images in a
// reference scene.
ConVar mat_non_hdr_bloom_scalefactor("mat_non_hdr_bloom_scalefactor",".3");

// Apply addition scale to the final bloom scale
static ConVar mat_bloom_scalefactor_scalar( "mat_bloom_scalefactor_scalar", "1.0", FCVAR_RELEASE );

//ConVar mat_exposure_center_region_x( "mat_exposure_center_region_x","0.75", FCVAR_CHEAT );
//ConVar mat_exposure_center_region_y( "mat_exposure_center_region_y","0.80", FCVAR_CHEAT );

ConVar mat_exposure_center_region_x( "mat_exposure_center_region_x","0.9", FCVAR_CHEAT );
ConVar mat_exposure_center_region_y( "mat_exposure_center_region_y","0.85", FCVAR_CHEAT );

ConVar mat_tonemap_algorithm( "mat_tonemap_algorithm", "1", FCVAR_CHEAT, "0 = Original Algorithm 1 = New Algorithm" );
ConVar mat_tonemap_percent_target( "mat_tonemap_percent_target", "60.0", FCVAR_CHEAT );
ConVar mat_tonemap_percent_bright_pixels( "mat_tonemap_percent_bright_pixels", "2.0", FCVAR_CHEAT );
ConVar mat_tonemap_min_avglum( "mat_tonemap_min_avglum", "3.0", FCVAR_CHEAT );
ConVar mat_force_tonemap_scale( "mat_force_tonemap_scale", "0.0", FCVAR_CHEAT );
ConVar mat_fullbright( "mat_fullbright", "0", FCVAR_CHEAT );

ConVar mat_grain_enable( "mat_grain_enable", "0" );
ConVar mat_vignette_enable( "mat_vignette_enable", "0" );
ConVar mat_local_contrast_enable( "mat_local_contrast_enable", "0" );

static void SetRenderTargetAndViewPort(ITexture *rt)
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetRenderTarget(rt);
	if ( rt )
	{
		pRenderContext->Viewport(0,0,rt->GetActualWidth(),rt->GetActualHeight());
	}
}

enum HistogramEntryState_t
{
	HESTATE_INITIAL = 0,
	HESTATE_FIRST_QUERY_IN_FLIGHT,
	HESTATE_QUERY_IN_FLIGHT,
	HESTATE_QUERY_DONE,
};

#define NUM_HISTOGRAM_BUCKETS 31
#define NUM_HISTOGRAM_BUCKETS_NEW 17
#define MAX_QUERIES_PER_FRAME 1

class CHistogramBucket
{
public:
	HistogramEntryState_t m_state;
	OcclusionQueryObjectHandle_t m_hOcclusionQueryHandle;
	int m_nFrameQueued;									// when this query was last queued
	int m_nPixels;										// # of pixels this histogram represents
	int m_nPixelsInRange;
	float m_flMinLuminance, m_flMaxLuminance;			// the luminance range this entry was queried with
	float m_flScreenMinX, m_flScreenMinY, m_flScreenMaxX, m_flScreenMaxY; // range is 0..1 in fractions of the screen

	bool ContainsValidData( void )
	{
		return ( m_state == HESTATE_QUERY_DONE ) || ( m_state == HESTATE_QUERY_IN_FLIGHT );
	}

	void IssueQuery( int nFrameNum );
};

void CHistogramBucket::IssueQuery( int nFrameNum )
{
	CMatRenderContextPtr pRenderContext( materials );
	if ( !m_hOcclusionQueryHandle )
	{
		m_hOcclusionQueryHandle = pRenderContext->CreateOcclusionQueryObject();
	}

	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

	// Find min and max gamma-space text range
	float flTestRangeMin = ( m_flMinLuminance == 0.0f ) ? -1e20f : m_flMinLuminance; // Count all pixels < 0.0 as 0.0 (for float HDR buffers)
	float flTestRangeMax = ( m_flMaxLuminance == 1.0f ) ? 1e20f : m_flMaxLuminance; // Count all pixels >1.0 as 1.0

	// Set stencil bits where the colors match
	IMaterial *pLumCompareMaterial = materials->FindMaterial( "dev/lumcompare", TEXTURE_GROUP_OTHER, true );

	IMaterialVar *pMinVar = pLumCompareMaterial->FindVar( "$C0_X", NULL );
	pMinVar->SetFloatValue( flTestRangeMin );

	IMaterialVar *pMaxVar = pLumCompareMaterial->FindVar( "$C0_Y", NULL );
	pMaxVar->SetFloatValue( flTestRangeMax );

	int nScreenMinX = FLerp( nViewportX, ( nViewportX + nViewportWidth - 1 ), 0, 1, m_flScreenMinX );
	int nScreenMaxX = FLerp( nViewportX, ( nViewportX + nViewportWidth - 1 ), 0, 1, m_flScreenMaxX );
	int nScreenMinY = FLerp( nViewportY, ( nViewportY + nViewportHeight - 1 ), 0, 1, m_flScreenMinY );
	int nScreenMaxY = FLerp( nViewportY, ( nViewportY + nViewportHeight - 1 ), 0, 1, m_flScreenMaxY );

	float flExposureWidthScale, flExposureHeightScale;

	// Shrink region of interest if the flashlight is on
	flExposureWidthScale = ( 0.5f * ( 1.0f - mat_exposure_center_region_x.GetFloat() ) );
	flExposureHeightScale = ( 0.5f * ( 1.0f - mat_exposure_center_region_y.GetFloat() ) );

	int nBorderWidth = ( nScreenMaxX - nScreenMinX + 1 ) * flExposureWidthScale;
	int nBorderHeight = ( nScreenMaxY - nScreenMinY + 1 ) * flExposureHeightScale;

	// Do luminance compare
	m_nPixels = ( 1 + nScreenMaxX - nScreenMinX ) * ( 1 + nScreenMaxY - nScreenMinY );

	ShaderStencilState_t state;
	if ( mat_tonemapping_occlusion_use_stencil.GetInt() )
	{
		state.m_nWriteMask = 1;
		state.m_bEnable = true;
		state.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
		state.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
		state.m_FailOp = SHADER_STENCILOP_KEEP;
		state.m_ZFailOp = SHADER_STENCILOP_KEEP;
		state.m_nReferenceValue = 1;
		pRenderContext->SetStencilState( state );
	}
	else
	{
		pRenderContext->BeginOcclusionQueryDrawing( m_hOcclusionQueryHandle );
	}

	int nWindowWidth = 0;
	int nWindowHeight = 0;
	pRenderContext->GetWindowSize( nWindowWidth, nWindowHeight );

	nScreenMinX += nBorderWidth;
	nScreenMinY += nBorderHeight;
	nScreenMaxX -= nBorderWidth;
	nScreenMaxY -= nBorderHeight;
	pRenderContext->DrawScreenSpaceRectangle( pLumCompareMaterial,
											  nScreenMinX - nViewportX, nScreenMinY - nViewportY,
											  1 + nScreenMaxX - nScreenMinX,
											  1 + nScreenMaxY - nScreenMinY,
											  nScreenMinX, nScreenMinY,
											  nScreenMaxX, nScreenMaxY,
											  nWindowWidth, nWindowHeight );

	if ( mat_tonemapping_occlusion_use_stencil.GetInt() )
	{
		// Start counting how many pixels had their stencil bit set via an occlusion query
		pRenderContext->BeginOcclusionQueryDrawing( m_hOcclusionQueryHandle );

		// Issue an occlusion query using stencil as the mask
		state.m_bEnable = true;
		state.m_nTestMask = 1;
		state.m_PassOp = SHADER_STENCILOP_KEEP;
		state.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
		state.m_FailOp = SHADER_STENCILOP_KEEP;
		state.m_ZFailOp = SHADER_STENCILOP_KEEP;
		state.m_nReferenceValue = 1;
		pRenderContext->SetStencilState( state );

		IMaterial *pLumCompareStencilMaterial = materials->FindMaterial( "dev/no_pixel_write", TEXTURE_GROUP_OTHER, true);
		pRenderContext->DrawScreenSpaceRectangle( pLumCompareStencilMaterial,
												  nScreenMinX, nScreenMinY,
												  1 + nScreenMaxX - nScreenMinX,
												  1 + nScreenMaxY - nScreenMinY,
												  nScreenMinX, nScreenMinY,
												  nScreenMaxX, nScreenMaxY,
												  nWindowWidth, nWindowHeight );

		ShaderStencilState_t stateDisable;
		stateDisable.m_bEnable = false;
		pRenderContext->SetStencilState( stateDisable );
	}

	pRenderContext->EndOcclusionQueryDrawing( m_hOcclusionQueryHandle );
	if ( m_state == HESTATE_INITIAL )
		m_state = HESTATE_FIRST_QUERY_IN_FLIGHT;
	else
		m_state = HESTATE_QUERY_IN_FLIGHT;
	m_nFrameQueued = nFrameNum;
}

#define HISTOGRAM_BAR_SIZE 200

class CTonemapSystem
{
	CHistogramBucket m_histogramBucketArray[NUM_HISTOGRAM_BUCKETS];
	int m_nCurrentQueryFrame;
	int m_nCurrentAlgorithm;

	float m_flTargetTonemapScale;
	float m_flCurrentTonemapScale;

	int m_nNumMovingAverageValid;
	float m_movingAverageTonemapScale[10];

	bool m_bOverrideTonemapScaleEnabled;
	float m_flOverrideTonemapScale;

public:
	void IssueAndReceiveBucketQueries();
	void UpdateBucketRanges();
	float FindLocationOfPercentBrightPixels( float flPercentBrightPixels, float flPercentTarget );
	float ComputeTargetTonemapScalar( bool bGetIdealTargetForDebugMode );

	void UpdateMaterialSystemTonemapScalar();
	void SetTargetTonemappingScale( float flTonemapScale );
	void ResetTonemappingScale( float flTonemapScale );
	void SetTonemapScale( IMatRenderContext *pRenderContext, float newvalue, float minvalue, float maxvalue );

	float GetTargetTonemappingScale() { return m_flTargetTonemapScale; }
	float GetCurrentTonemappingScale() { return m_flCurrentTonemapScale; }

	void SetOverrideTonemapScale( bool bEnableOverride, float flTonemapScale );

	// Dev functions
	void DisplayHistogram();

	// Constructor
	CTonemapSystem()
	{
		m_nCurrentQueryFrame = 0;
		m_nCurrentAlgorithm = -1;
		m_flTargetTonemapScale = 1.0f;
		m_flCurrentTonemapScale = 1.0f;

		m_nNumMovingAverageValid = 0;
		for ( int i = 0; i < ARRAYSIZE( m_movingAverageTonemapScale ) - 1; i++ )
		{
			m_movingAverageTonemapScale[i] = 1.0f;
		}

		m_bOverrideTonemapScaleEnabled = false;
		m_flOverrideTonemapScale = 1.0f;

		UpdateBucketRanges();
	}
};

CTonemapSystem * GetCurrentTonemappingSystem()
{
	static CTonemapSystem s_HDR_HistogramSystem[ MAX_SPLITSCREEN_PLAYERS ];
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();
	return &( s_HDR_HistogramSystem[ slot ] );
}

void CTonemapSystem::IssueAndReceiveBucketQueries()
{
	UpdateBucketRanges();

	// Find which histogram entries should have something done this frame
	int nQueriesIssuedThisFrame = 0;
	m_nCurrentQueryFrame++;

	int nNumHistogramBuckets = NUM_HISTOGRAM_BUCKETS;
	if ( mat_tonemap_algorithm.GetInt() == 1 )
		nNumHistogramBuckets = NUM_HISTOGRAM_BUCKETS_NEW;

	for ( int i = 0; i < nNumHistogramBuckets; i++ )
	{
		switch ( m_histogramBucketArray[i].m_state )
		{
			case HESTATE_INITIAL:
				if ( nQueriesIssuedThisFrame<MAX_QUERIES_PER_FRAME )
				{
					m_histogramBucketArray[i].IssueQuery(m_nCurrentQueryFrame);
					nQueriesIssuedThisFrame++;
				}
				break;

			case HESTATE_FIRST_QUERY_IN_FLIGHT:
			case HESTATE_QUERY_IN_FLIGHT:
				if ( m_nCurrentQueryFrame > m_histogramBucketArray[i].m_nFrameQueued + 2 )
				{
					CMatRenderContextPtr pRenderContext( materials );
					int np = pRenderContext->OcclusionQuery_GetNumPixelsRendered(
						m_histogramBucketArray[i].m_hOcclusionQueryHandle );
					if ( np != -1 ) // -1 = Query not finished...wait until next time
					{
						m_histogramBucketArray[i].m_nPixelsInRange = np;
						m_histogramBucketArray[i].m_state = HESTATE_QUERY_DONE;
					}
				}
				break;
		}
	}

	// Now, issue queries for the oldest finished queries we have
	while ( nQueriesIssuedThisFrame < MAX_QUERIES_PER_FRAME )
	{
		int nNumHistogramBuckets = NUM_HISTOGRAM_BUCKETS;
		if ( mat_tonemap_algorithm.GetInt() == 1 )
			nNumHistogramBuckets = NUM_HISTOGRAM_BUCKETS_NEW;

		int nOldestSoFar = -1;
		for ( int i = 0; i < nNumHistogramBuckets; i++ )
		{
			if ( ( m_histogramBucketArray[i].m_state == HESTATE_QUERY_DONE ) &&
				 ( ( nOldestSoFar == -1 ) || ( m_histogramBucketArray[i].m_nFrameQueued < m_histogramBucketArray[nOldestSoFar].m_nFrameQueued ) ) )
			{
				nOldestSoFar = i;
			}
		}

		if ( nOldestSoFar == -1 ) // Nothing to do
			break;

		m_histogramBucketArray[nOldestSoFar].IssueQuery( m_nCurrentQueryFrame );
		nQueriesIssuedThisFrame++;
	}
}

float CTonemapSystem::FindLocationOfPercentBrightPixels( float flPercentBrightPixels, float flPercentTargetToSnapToIfInSameBin = -1.0f )
{
	if ( mat_tonemap_algorithm.GetInt() == 1 ) // New algorithm
	{
		int nTotalValidPixels = 0;
		for ( int i = 0; i < ( NUM_HISTOGRAM_BUCKETS_NEW - 1 ); i++ )
		{
			if ( m_histogramBucketArray[i].ContainsValidData() )
			{
				nTotalValidPixels += m_histogramBucketArray[i].m_nPixelsInRange;
			}
		}

		if ( nTotalValidPixels == 0 )
		{
			return -1.0f;
		}

		// Find where percent range border is
		float flTotalPercentRangeTested = 0.0f;
		float flTotalPercentPixelsTested = 0.0f;
		for ( int i = ( NUM_HISTOGRAM_BUCKETS_NEW - 2 ); i >= 0; i-- ) // Start at the bright end
		{
			if ( !m_histogramBucketArray[i].ContainsValidData() )
				return -1.0f;

			float flPixelPercentNeeded = ( flPercentBrightPixels / 100.0f ) - flTotalPercentPixelsTested;
			float flThisBinPercentOfTotalPixels = float( m_histogramBucketArray[i].m_nPixelsInRange ) / float( nTotalValidPixels );
			float flThisBinLuminanceRange = m_histogramBucketArray[i].m_flMaxLuminance - m_histogramBucketArray[i].m_flMinLuminance;
			if ( flThisBinPercentOfTotalPixels >= flPixelPercentNeeded ) // We found the bin needed
			{
				if ( flPercentTargetToSnapToIfInSameBin >= 0.0f )
				{
					if ( ( m_histogramBucketArray[i].m_flMinLuminance <= ( flPercentTargetToSnapToIfInSameBin / 100.0f ) ) && ( m_histogramBucketArray[i].m_flMaxLuminance >= ( flPercentTargetToSnapToIfInSameBin / 100.0f ) ) )
					{
						// Sticky bin...We're in the same bin as the target so keep the tonemap scale where it is
						return ( flPercentTargetToSnapToIfInSameBin / 100.0f );
					}
				}

				float flPercentOfThesePixelsNeeded = flPixelPercentNeeded / flThisBinPercentOfTotalPixels;
				float flPercentLocationOfBorder = 1.0f - ( flTotalPercentRangeTested + ( flThisBinLuminanceRange * flPercentOfThesePixelsNeeded ) );
				flPercentLocationOfBorder = MAX( m_histogramBucketArray[i].m_flMinLuminance, MIN( m_histogramBucketArray[i].m_flMaxLuminance, flPercentLocationOfBorder ) ); // Clamp to this bin just in case
				return flPercentLocationOfBorder;
			}

			flTotalPercentPixelsTested += flThisBinPercentOfTotalPixels;
			flTotalPercentRangeTested += flThisBinLuminanceRange;
		}

		return -1.0f;
	}
	else
	{
		// Don't know what to do for other algorithms yet
		return -1.0f;
	}
}

float CTonemapSystem::ComputeTargetTonemapScalar( bool bGetIdealTargetForDebugMode = false )
{
	if ( mat_tonemap_algorithm.GetInt() == 1 ) // New algorithm
	{
		float flPercentLocationOfTarget;
		if ( bGetIdealTargetForDebugMode == true)
			flPercentLocationOfTarget = FindLocationOfPercentBrightPixels( mat_tonemap_percent_bright_pixels.GetFloat() ); // Don't pass in the second arg so the scalar doesn't snap to a bin
		else
			flPercentLocationOfTarget = FindLocationOfPercentBrightPixels( mat_tonemap_percent_bright_pixels.GetFloat(), mat_tonemap_percent_target.GetFloat() );
		if ( flPercentLocationOfTarget < 0.0f ) // This is the return error code
		{
			flPercentLocationOfTarget = mat_tonemap_percent_target.GetFloat() / 100.0f; // Pretend we're at the target
		}

		// Make sure this is > 0.0f
		flPercentLocationOfTarget = MAX( 0.0001f, flPercentLocationOfTarget );

		// Compute target scalar
		float flTargetScalar = ( mat_tonemap_percent_target.GetFloat() / 100.0f ) / flPercentLocationOfTarget;

		// Compute secondary target scalar
		float flAverageLuminanceLocation = FindLocationOfPercentBrightPixels( 50.0f );
		if ( flAverageLuminanceLocation > 0.0f )
		{
			float flTargetScalar2 = ( mat_tonemap_min_avglum.GetFloat() / 100.0f ) / flAverageLuminanceLocation;

			// Only override it if it's trying to brighten the image more than the primary algorithm
			if ( flTargetScalar2 > flTargetScalar )
			{
				flTargetScalar = flTargetScalar2;
			}
		}

		// Apply this against last frames scalar
		CMatRenderContextPtr pRenderContext( materials );
		float flLastScale = m_flCurrentTonemapScale;
		flTargetScalar *= flLastScale;

		flTargetScalar = MAX( 0.001f, flTargetScalar );
		return flTargetScalar;
	}
	else // Original tonemapping algorithm
	{
		float flScaleValue = 1.0f;
		if ( m_histogramBucketArray[NUM_HISTOGRAM_BUCKETS-1].ContainsValidData() )
		{
			flScaleValue = m_histogramBucketArray[NUM_HISTOGRAM_BUCKETS-1].m_nPixels * ( 1.0f / m_histogramBucketArray[NUM_HISTOGRAM_BUCKETS-1].m_nPixelsInRange );
		}

		if ( !IsFinite( flScaleValue ) )
		{
			flScaleValue = 1.0f;
		}

		float flTotal = 0.0f;
		int nTotalPixels = 0;
		for ( int i=0; i<NUM_HISTOGRAM_BUCKETS-1; i++ )
		{
			if ( m_histogramBucketArray[i].ContainsValidData() )
			{
				flTotal += flScaleValue * m_histogramBucketArray[i].m_nPixelsInRange * AVG( m_histogramBucketArray[i].m_flMinLuminance, m_histogramBucketArray[i].m_flMaxLuminance );
				nTotalPixels += m_histogramBucketArray[i].m_nPixels;
			}
		}

		float flAverageLuminance = 0.5f;
		if ( nTotalPixels > 0 )
			flAverageLuminance = flTotal * ( 1.0f / nTotalPixels );
		else
			flAverageLuminance = 0.5f;

		// Make sure this is > 0.0f
		flAverageLuminance = MAX( 0.0001f, flAverageLuminance );

		// Compute target scalar
		float flTargetScalar = 0.005f / flAverageLuminance;

		return flTargetScalar;
	}
}

static float GetCurrentBloomScale( void )
{
	// Use the appropriate bloom scale settings.  Mapmakers's overrides the convar settings.
	float flCurrentBloomScale = 1.0f;
	if ( g_bUseCustomBloomScale )
	{
		flCurrentBloomScale = g_flCustomBloomScale;
	}
	else
	{
		flCurrentBloomScale = mat_bloomscale.GetFloat();
	}
	return flCurrentBloomScale;
}

static void GetExposureRange( float *pflAutoExposureMin, float *pflAutoExposureMax )
{
	// Get min
	if ( ( g_bUseCustomAutoExposureMin ) && ( g_flCustomAutoExposureMin > 0.0f ) )
	{
		*pflAutoExposureMin = g_flCustomAutoExposureMin;
	}
	else
	{
		*pflAutoExposureMin = mat_autoexposure_min.GetFloat();
	}

	// Get max
	if ( ( g_bUseCustomAutoExposureMax ) && ( g_flCustomAutoExposureMax > 0.0f ) )
	{
		*pflAutoExposureMax = g_flCustomAutoExposureMax;
	}
	else
	{
		*pflAutoExposureMax = mat_autoexposure_max.GetFloat();
	}

	// Override
	if ( mat_hdr_uncapexposure.GetInt() )
	{
		*pflAutoExposureMax = 100.0f;
		*pflAutoExposureMin = 0.0f;
	}

	// Make sure min <= max
	if ( *pflAutoExposureMin > *pflAutoExposureMax )
	{
		*pflAutoExposureMax = *pflAutoExposureMin;
	}
}

void CTonemapSystem::UpdateBucketRanges()
{
	// Only update if our mode changed
	if ( m_nCurrentAlgorithm == mat_tonemap_algorithm.GetInt() )
		return;
	m_nCurrentAlgorithm = mat_tonemap_algorithm.GetInt();

	//==================================================================//
	// Force fallback to original tone mapping algorithm for these mods //
	//==================================================================//
	static bool s_bFirstTime = true;
	if ( engine == NULL )
	{
		// Force this code to get hit again so we can change algorithm based on the client
		m_nCurrentAlgorithm = -1;
	}
	else if ( s_bFirstTime == true )
	{
		s_bFirstTime = false;

		// This seems like a bad idea but it's fine for now
		const char *sModsForOriginalAlgorithm[] = { "dod", "cstrike", "lostcoast" };
		for ( int i=0; i<3; i++ )
		{
			if ( strlen( engine->GetGameDirectory() ) >= strlen( sModsForOriginalAlgorithm[i] ) )
			{
				if ( stricmp( &( engine->GetGameDirectory()[strlen( engine->GetGameDirectory() ) - strlen( sModsForOriginalAlgorithm[i] )] ), sModsForOriginalAlgorithm[i] ) == 0 )
				{
					mat_tonemap_algorithm.SetValue( 0 ); // Original algorithm
					m_nCurrentAlgorithm = mat_tonemap_algorithm.GetInt();
					break;
				}
			}
		}
	}

	// Get num buckets
	int nNumHistogramBuckets = NUM_HISTOGRAM_BUCKETS;
	if ( mat_tonemap_algorithm.GetInt() == 1 )
		nNumHistogramBuckets = NUM_HISTOGRAM_BUCKETS_NEW;

	m_nCurrentQueryFrame = 0;
	for ( int nBucket = 0; nBucket < nNumHistogramBuckets; nBucket++ )
	{
		CHistogramBucket *pBucket = &( m_histogramBucketArray[ nBucket ] );
		pBucket->m_state = HESTATE_INITIAL;
		pBucket->m_flScreenMinX = 0.0f;
		pBucket->m_flScreenMaxX = 1.0f;
		pBucket->m_flScreenMinY = 0.0f;
		pBucket->m_flScreenMaxY = 1.0f;
		if ( nBucket != ( nNumHistogramBuckets - 1 ) ) // Last bucket is special
		{
			if ( mat_tonemap_algorithm.GetInt() == 0 ) // Original algorithm
			{
				// Use a logarithmic ramp for high range in the low range
				pBucket->m_flMinLuminance = -0.01f + exp( FLerp( log( 0.01f ), log( 0.01f + 1.0f ), 0.0f, nNumHistogramBuckets - 1.0f, nBucket ) );
				pBucket->m_flMaxLuminance = -0.01f + exp( FLerp( log( 0.01f ), log( 0.01f + 1.0f ), 0.0f, nNumHistogramBuckets - 1.0f, nBucket + 1.0f ) );
			}
			else
			{
				// Use even distribution
				pBucket->m_flMinLuminance = float( nBucket ) / float( nNumHistogramBuckets - 1 );
				pBucket->m_flMaxLuminance = float( nBucket + 1 ) / float( nNumHistogramBuckets - 1 );

				// Use a distribution with slightly more bins in the low range
				pBucket->m_flMinLuminance = pBucket->m_flMinLuminance > 0.0f ? powf( pBucket->m_flMinLuminance, 2.5f ) : pBucket->m_flMinLuminance;
				pBucket->m_flMaxLuminance = pBucket->m_flMaxLuminance > 0.0f ? powf( pBucket->m_flMaxLuminance, 2.5f ) : pBucket->m_flMaxLuminance;
			}
		}
		else
		{
			// The last bucket is used as a test to determine the return range for occlusion
			// queries to use as a scale factor. some boards (nvidia) have their occlusion
			// query return values larger when using AA.
			pBucket->m_flMinLuminance = 0.0f;
			pBucket->m_flMaxLuminance = 100000.0f;
		}

		//Warning( "Bucket %d: min/max %f / %f ", nBucket, pBucket->m_flMinLuminance, pBucket->m_flMaxLuminance );
	}
}


void CTonemapSystem::SetOverrideTonemapScale( bool bEnableOverride, float flTonemapScale )
{
	m_bOverrideTonemapScaleEnabled = bEnableOverride;
	m_flOverrideTonemapScale = flTonemapScale;
}


void CTonemapSystem::DisplayHistogram()
{
	if ( !mat_show_histogram.GetInt() || !mat_dynamic_tonemapping.GetInt() || ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE ) )
		return;

	// Get render context
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport();

	// Prep variables for drawing histogram
	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

	// Get num bins
	int nNumHistogramBuckets = NUM_HISTOGRAM_BUCKETS-1;
	if ( mat_tonemap_algorithm.GetInt() == 1 )
		nNumHistogramBuckets = NUM_HISTOGRAM_BUCKETS_NEW-1;

	// Count total pixels in current bins
	int nMaxValidPixels = 0;
	int nTotalValidPixels = 0;
	int nTotalGraphPixelsWide = 0;
	for ( int nBucket = 0; nBucket < nNumHistogramBuckets; nBucket++ )
	{
		CHistogramBucket *pBucket = &( m_histogramBucketArray[ nBucket ] );
		if ( pBucket->ContainsValidData() )
		{
			nTotalValidPixels += pBucket->m_nPixelsInRange;
			if ( pBucket->m_nPixelsInRange > nMaxValidPixels )
			{
				nMaxValidPixels = pBucket->m_nPixelsInRange;
			}
		}

		int nWidth = MAX( 1, 500 * ( pBucket->m_flMaxLuminance - pBucket->m_flMinLuminance ) );
		nTotalGraphPixelsWide += nWidth + 2;
	}

	// Clear background to gray for screenshots
	//int nBoxWidth = ( nTotalGraphPixelsWide + 20 );
	//pRenderContext->ClearColor3ub( 150, 150, 150 );
	//pRenderContext->Viewport( nViewportWidth - nBoxWidth, 0, nBoxWidth, 245 );
	//pRenderContext->ClearBuffers( true, true );

	// Output some text data
	if ( !IsX360() )
	{
		engine->Con_NPrintf( 23 + ( nViewportY / 10 ), "(Histogram luminance is in linear space)" );

		engine->Con_NPrintf( 27 + ( nViewportY / 10 ), "AvgLum @ %4.2f%%  mat_tonemap_min_avglum = %4.2f%%  Using %d pixels  Override(%s): %4.2f", 
			MAX( 0.0f, FindLocationOfPercentBrightPixels( 50.0f ) ) * 100.0f, mat_tonemap_min_avglum.GetFloat(), nTotalValidPixels, m_bOverrideTonemapScaleEnabled ? "On" : "Off", m_flOverrideTonemapScale );
		engine->Con_NPrintf( 29 + ( nViewportY / 10 ), "BloomScale = %4.2f  mat_hdr_manual_tonemap_rate = %4.2f  mat_accelerate_adjust_exposure_down = %4.2f", 
			GetCurrentBloomScale(), mat_hdr_manual_tonemap_rate.GetFloat(), mat_accelerate_adjust_exposure_down.GetFloat() );
	}

	int xpStart = nViewportX + nViewportWidth - nTotalGraphPixelsWide - 10;
	if ( IsX360() )
	{
		xpStart -= 50;
	}

	int yOffset = 4 + nViewportY;

	int xp = xpStart;
	for ( int nBucket = 0; nBucket < nNumHistogramBuckets; nBucket++ )
	{
		int np = 0;
		CHistogramBucket &e = m_histogramBucketArray[ nBucket ];
		if ( e.ContainsValidData() )
			np += e.m_nPixelsInRange;
		int width = MAX( 1, 500 * ( e.m_flMaxLuminance - e.m_flMinLuminance ) );

		//Warning( "Bucket %d: min/max %f / %f.  m_nPixelsInRange=%d   m_nPixels=%d\n", nBucket, e.m_flMinLuminance, e.m_flMaxLuminance, e.m_nPixelsInRange, e.m_nPixels );

		if ( np )
		{
			int height = MAX( 1, MIN( HISTOGRAM_BAR_SIZE, ( (float)np / (float)nMaxValidPixels ) * HISTOGRAM_BAR_SIZE ) );

			pRenderContext->ClearColor3ub( 255, 0, 0 );
			pRenderContext->Viewport( xp, yOffset + HISTOGRAM_BAR_SIZE - height, width, height );
			pRenderContext->ClearBuffers( true, true );
		}
		else
		{
			int height = 1;
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->Viewport( xp, yOffset + HISTOGRAM_BAR_SIZE - height, width, height );
			pRenderContext->ClearBuffers( true, true );
		}
		xp += width + 2;
	}

	if ( mat_tonemap_algorithm.GetInt() == 1 ) // New algorithm only
	{
		float flYellowTargetPixelStart = ( xpStart + ( float( nTotalGraphPixelsWide ) * mat_tonemap_percent_target.GetFloat() / 100.0f ) );
		float flYellowAveragePixelStart = ( xpStart + ( float( nTotalGraphPixelsWide ) * mat_tonemap_min_avglum.GetFloat() / 100.0f ) );

		float flTargetPixelStart = ( xpStart + ( float( nTotalGraphPixelsWide ) * FindLocationOfPercentBrightPixels( mat_tonemap_percent_bright_pixels.GetFloat(), mat_tonemap_percent_target.GetFloat() ) ) );
		float flAveragePixelStart = ( xpStart + ( float( nTotalGraphPixelsWide ) * FindLocationOfPercentBrightPixels( 50.0f ) ) );

		// Draw target yellow border bar
		int nHeight = HISTOGRAM_BAR_SIZE * 3 / 4;
		int nHeightOffset = -( HISTOGRAM_BAR_SIZE - nHeight ) / 2;

		// Green is current percent target location
		pRenderContext->Viewport( flYellowTargetPixelStart-1, yOffset + nHeightOffset + HISTOGRAM_BAR_SIZE - nHeight - 2, 8, nHeight + 4 );
		pRenderContext->ClearColor3ub( 0, 127, 0 );
		pRenderContext->ClearBuffers( true, true );
		
		pRenderContext->Viewport( flYellowTargetPixelStart+1, yOffset + nHeightOffset + HISTOGRAM_BAR_SIZE - nHeight, 4, nHeight );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, true );

		pRenderContext->Viewport( flTargetPixelStart+1, yOffset + nHeightOffset + HISTOGRAM_BAR_SIZE - nHeight, 4, nHeight );
		pRenderContext->ClearColor3ub( 0, 255, 0 );
		pRenderContext->ClearBuffers( true, true );
		
		// Blue is average luminance location
		pRenderContext->Viewport( flYellowAveragePixelStart-1, yOffset + nHeightOffset + HISTOGRAM_BAR_SIZE - nHeight - 2, 8, nHeight + 4 );
		pRenderContext->ClearColor3ub( 0, 114, 188 );
		pRenderContext->ClearBuffers( true, true );
		
		pRenderContext->Viewport( flYellowAveragePixelStart+1, yOffset + nHeightOffset + HISTOGRAM_BAR_SIZE - nHeight, 4, nHeight );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, true );

		pRenderContext->Viewport( flAveragePixelStart+1, yOffset + nHeightOffset + HISTOGRAM_BAR_SIZE - nHeight, 4, nHeight );
		pRenderContext->ClearColor3ub( 0, 191, 243 );
		pRenderContext->ClearBuffers( true, true );
	}

	// Show actual tonemap value
	if ( 1 )
	{
		float flAutoExposureMin;
		float flAutoExposureMax;
		GetExposureRange( &flAutoExposureMin, &flAutoExposureMax );

		float flBarWidth = nTotalGraphPixelsWide;
		float flBarStart = xpStart;

		pRenderContext->Viewport( flBarStart, yOffset + HISTOGRAM_BAR_SIZE - 4 + 20, flBarWidth, 4 );
		pRenderContext->ClearColor3ub( 200, 200, 200 );
		pRenderContext->ClearBuffers( true, true );

		pRenderContext->Viewport( flBarStart, yOffset + HISTOGRAM_BAR_SIZE - 4 + 20 + 1, flBarWidth, 2 );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, true );

		pRenderContext->Viewport( flBarStart + ( flBarWidth * ( ( m_flCurrentTonemapScale - flAutoExposureMin ) / ( flAutoExposureMax - flAutoExposureMin ) ) ) - 1,
								  yOffset + HISTOGRAM_BAR_SIZE - 4 + 20 - 6 - 1, 4 + 2, 16 + 2 );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, true );

		pRenderContext->Viewport( flBarStart + ( flBarWidth * ( ( m_flCurrentTonemapScale - flAutoExposureMin ) / ( flAutoExposureMax - flAutoExposureMin ) ) ),
								  yOffset + HISTOGRAM_BAR_SIZE - 4 + 20 - 6, 4, 16 );
		pRenderContext->ClearColor3ub( 255, 255, 0 );
		pRenderContext->ClearBuffers( true, true );

		if ( !IsX360() )
			engine->Con_NPrintf( 21 + ( nViewportY / 10 ), "%.2f                                                                             %.2f                                                                           %.2f", flAutoExposureMin, ( flAutoExposureMax + flAutoExposureMin ) / 2.0f, flAutoExposureMax );
	}

	// Last bar doesn't clear properly so draw an extra pixel
	pRenderContext->Viewport( 0, 0, 1, 1 );
	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, true );

	pRenderContext->PopRenderTargetAndViewport();
}

// Global postprocessing disable switch
static bool s_bOverridePostProcessingDisable = false;

void UpdateMaterialSystemTonemapScalar()
{
	GetCurrentTonemappingSystem()->UpdateMaterialSystemTonemapScalar();
}

void CTonemapSystem::UpdateMaterialSystemTonemapScalar()
{
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
	{
		// Deal with forced tone map scalar
		float flForcedTonemapScale = mat_force_tonemap_scale.GetFloat();

		if ( mat_fullbright.GetInt() == 1 )
		{
			flForcedTonemapScale = 1.0f;
		}

		if ( flForcedTonemapScale > 0.0f )
		{
			ResetTonemappingScale( flForcedTonemapScale );

			// Send this value to the material system
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->SetToneMappingScaleLinear( Vector( m_flCurrentTonemapScale, m_flCurrentTonemapScale, m_flCurrentTonemapScale ) );
			return;
		}

		// Override tone map scalar
		if ( m_bOverrideTonemapScaleEnabled )
		{
			float flAutoExposureMin;
			float flAutoExposureMax;
			GetExposureRange( &flAutoExposureMin, &flAutoExposureMax );

			float fScale = clamp( m_flOverrideTonemapScale, flAutoExposureMin, flAutoExposureMax );
			ResetTonemappingScale( fScale );

			// Send this value to the material system
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->SetToneMappingScaleLinear( Vector( m_flCurrentTonemapScale, m_flCurrentTonemapScale, m_flCurrentTonemapScale ) );
			return;
		}

		// Send this value to the material system
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetToneMappingScaleLinear( Vector( m_flCurrentTonemapScale, m_flCurrentTonemapScale, m_flCurrentTonemapScale ) );
	}
	else
	{
		// Send 1.0 to the material system since HDR is disabled
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetToneMappingScaleLinear( Vector( 1.0f, 1.0f, 1.0f ) );
	}
}

void CTonemapSystem::ResetTonemappingScale( float flTonemapScale )
{
	if ( flTonemapScale <= 0.0f )
	{
		// L4D Hack to reset the tonemapping scale to the average of min and max since we have such dark lighting
		// compared to our other games. 1.0 is no longer a good value when changing spectator targets.
		float flAutoExposureMin = 0.0f;
		float flAutoExposureMax = 0.0f;
		GetExposureRange( &flAutoExposureMin, &flAutoExposureMax );
		flTonemapScale = ( flAutoExposureMin + flAutoExposureMax ) * 0.5f;
		flTonemapScale = clamp( flTonemapScale, 1.0f, 10.0f ); // Restrict this to the 1-10 range
	}

	// Force current and target tonemap scalar
	m_flCurrentTonemapScale = flTonemapScale;
	m_flTargetTonemapScale = flTonemapScale;

	// Clear averaging history
	m_nNumMovingAverageValid = 0;
}

void CTonemapSystem::SetTargetTonemappingScale( float flTonemapScale )
{
	Assert( IsFinite( flTonemapScale ) );
	if ( IsFinite( flTonemapScale ) )
	{
		m_flTargetTonemapScale = flTonemapScale;
	}
}

// Local contrast setting
PostProcessParameters_t s_LocalPostProcessParameters[ MAX_SPLITSCREEN_PLAYERS ];

// view fade param settings
static Vector4D s_viewFadeColor[ MAX_SPLITSCREEN_PLAYERS ];
static bool  s_bViewFadeModulate[ MAX_SPLITSCREEN_PLAYERS ];

class PPInit
{
public:
	PPInit()
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			s_viewFadeColor[ i ].Init( 0.0f, 0.0f, 0.0f, 0.0f );
			s_bViewFadeModulate[ i ] = false;
		}
	}
};

static PPInit g_PPInit;

void ResetToneMapping( float flTonemappingScale )
{
	GetCurrentTonemappingSystem()->ResetTonemappingScale( flTonemappingScale );

	// Send this value to the material system
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetToneMappingScaleLinear( Vector( flTonemappingScale, flTonemappingScale, flTonemappingScale ) );
}

void CTonemapSystem::SetTonemapScale( IMatRenderContext *pRenderContext, float flTargetTonemapScalar, float flMinValue, float flMaxValue )
{
	Assert( IsFinite( flTargetTonemapScalar ) );
	if ( !IsFinite( flTargetTonemapScalar ) )
		return;

	//=========================================================================//
	// Save off new target tonemap scalar so we can compute a weighted average //
	//=========================================================================//
	if ( m_nNumMovingAverageValid < ARRAYSIZE( m_movingAverageTonemapScale ))
	{
		m_movingAverageTonemapScale[ m_nNumMovingAverageValid++ ] = flTargetTonemapScalar;
	}
	else
	{
		// Scroll, losing oldest
		for ( int i = 0; i < ARRAYSIZE( m_movingAverageTonemapScale ) - 1; i++ )
			m_movingAverageTonemapScale[ i ] = m_movingAverageTonemapScale[ i + 1 ];
		m_movingAverageTonemapScale[ ARRAYSIZE( m_movingAverageTonemapScale ) - 1 ] = flTargetTonemapScalar;
	}

	//==================================================================//
	// Compute a weighted average of the last 10 target tonemap scalars //
	//==================================================================//
	if ( m_nNumMovingAverageValid == ARRAYSIZE( m_movingAverageTonemapScale ) ) // If we have a full buffer
	{
		float flWeightedAverage = 0.0f;
		float flSumWeights = 0.0f;
		int iMidPoint = ARRAYSIZE( m_movingAverageTonemapScale ) / 2;
		for ( int i = 0; i < ARRAYSIZE( m_movingAverageTonemapScale ); i++ )
		{
			float flWeight = abs( i - iMidPoint ) * ( 1.0f / ( ARRAYSIZE( m_movingAverageTonemapScale ) / 2 ) );
			flSumWeights += flWeight;
			flWeightedAverage += flWeight * m_movingAverageTonemapScale[i];
		}
		flWeightedAverage *= ( 1.0f / flSumWeights );
		flWeightedAverage = clamp( flWeightedAverage, flMinValue, flMaxValue );

		SetTargetTonemappingScale( flWeightedAverage );
	}
	else
	{
		SetTargetTonemappingScale( flTargetTonemapScalar );
	}

	//=======================================//
	// Smoothly lerp to the target over time //
	//=======================================//
	float flElapsedTime = MAX( gpGlobals->frametime, 0.0f ); // Clamp to positive
	float flRate = mat_hdr_manual_tonemap_rate.GetFloat();

	if ( mat_tonemap_algorithm.GetInt() == 1 )
	{
		flRate *= 2.0f; // Default 2x for the new tone mapping algorithm so it feels the same as the original
	}

	if ( flRate == 0.0f ) // Zero indicates instantaneous tonemap scaling
	{
		m_flCurrentTonemapScale = m_flTargetTonemapScale;
	}
	else
	{
		if ( m_flTargetTonemapScale < m_flCurrentTonemapScale )
		{
			float acc_exposure_adjust = mat_accelerate_adjust_exposure_down.GetFloat();

			// Adjust at up to 4x rate when over-exposed.
			flRate = MIN( ( acc_exposure_adjust * flRate ), FLerp( flRate, ( acc_exposure_adjust * flRate ), 0.0f, 1.5f, ( m_flCurrentTonemapScale - m_flTargetTonemapScale ) ) );
		}

		float flRateTimesTime = flRate * flElapsedTime;
		if ( mat_tonemap_algorithm.GetInt() == 1 )
		{
			// For the new tone mapping algorithm, limit the rate based on the number of bins to 
			// help reduce the tone map scalar "riding the wave" of the histogram re-building

			//Warning( "flRateTimesTime = %.4f", flRateTimesTime );
			flRateTimesTime = MIN( flRateTimesTime, ( 1.0f / ( float )( NUM_HISTOGRAM_BUCKETS_NEW - 1 ) ) * 0.25f );
			//Warning( " --> %.4f\n", flRateTimesTime );
		}

		float flAlpha = clamp( flRateTimesTime, 0.0f, 1.0f );
		m_flCurrentTonemapScale = ( m_flTargetTonemapScale * flAlpha ) + ( m_flCurrentTonemapScale * ( 1.0f - flAlpha ) );
		//m_flCurrentTonemapScale = FLerp( m_flCurrentTonemapScale, m_flTargetTonemapScale, flAlpha );

		if ( !IsFinite( m_flCurrentTonemapScale ) )
		{
			Assert( 0 );
			m_flCurrentTonemapScale = m_flTargetTonemapScale;
		}
	}

	//==========================================//
	// Step on values if we're forcing a scalar //
	//==========================================//
	float flForcedTonemapScale = mat_force_tonemap_scale.GetFloat();
	if ( flForcedTonemapScale > 0.0f )
	{
		ResetTonemappingScale( flForcedTonemapScale );
	}
}

//=====================================================================================================================
// Public functions for messing with tone mapping
//=====================================================================================================================

float GetCurrentTonemapScale()
{
	return GetCurrentTonemappingSystem()->GetCurrentTonemappingScale();
}

void SetOverrideTonemapScale( bool bEnableOverride, float flTonemapScale )
{
	GetCurrentTonemappingSystem()->SetOverrideTonemapScale( bEnableOverride, flTonemapScale );
}

void SetOverridePostProcessingDisable( bool bForceOff )
{
	s_bOverridePostProcessingDisable = bForceOff;
}

void SetPostProcessParams( const PostProcessParameters_t *pPostProcessParameters )
{
	int nSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	s_LocalPostProcessParameters[ nSplitScreenSlot ] = *pPostProcessParameters;
}

void SetViewFadeParams( byte r, byte g, byte b, byte a, bool bModulate )
{
	int nSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	s_viewFadeColor[ nSplitScreenSlot ].Init( float(r)/255.0f, float(g)/255.0f, float(b)/255.0f, float(a)/255.0f );
	s_bViewFadeModulate[ nSplitScreenSlot ] = bModulate;
}

//=====================================================================================================================
// BloomAdd material proxy ============================================================================================
//=====================================================================================================================

class CBloomAddMaterialProxy : public CEntityMaterialProxy
{
public:
	CBloomAddMaterialProxy();
	virtual ~CBloomAddMaterialProxy() {}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pEntity );
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_pMaterialParam_BloomAmount;

public:
	static void SetBloomAmount( float flBloomAmount ) { s_flBloomAmount = flBloomAmount; }

private:
	static float s_flBloomAmount;
};

float CBloomAddMaterialProxy::s_flBloomAmount = 1.0f;

CBloomAddMaterialProxy::CBloomAddMaterialProxy()
: m_pMaterialParam_BloomAmount( NULL )
{
}

bool CBloomAddMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool bFoundVar = false;

	m_pMaterialParam_BloomAmount = pMaterial->FindVar( "$c0_x", &bFoundVar, false );

	return true;
}

void CBloomAddMaterialProxy::OnBind( C_BaseEntity *pEnt )
{
	if ( m_pMaterialParam_BloomAmount )
		m_pMaterialParam_BloomAmount->SetFloatValue( s_flBloomAmount );
}

IMaterial *CBloomAddMaterialProxy::GetMaterial()
{
	if ( m_pMaterialParam_BloomAmount == NULL)
		return NULL;

	return m_pMaterialParam_BloomAmount->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CBloomAddMaterialProxy, BloomAdd );

//=====================================================================================================================
// Engine_Post material proxy ============================================================================================
//=====================================================================================================================

static ConVar mat_software_aa_strength( "mat_software_aa_strength", "-1.0", 0, "Software AA - perform a software anti-aliasing post-process (an alternative/supplement to MSAA). This value sets the strength of the effect: (0.0 - off), (1.0 - full)" );
static ConVar mat_software_aa_quality( "mat_software_aa_quality", "0", 0, "Software AA quality mode: (0 - 5-tap filter), (1 - 9-tap filter)" );
static ConVar mat_software_aa_edge_threshold( "mat_software_aa_edge_threshold", "1.0", 0, "Software AA - adjusts the sensitivity of the software AA shader's edge detection (default 1.0 - a lower value will soften more edges, a higher value will soften fewer)" );
static ConVar mat_software_aa_blur_one_pixel_lines( "mat_software_aa_blur_one_pixel_lines", "0.5", 0, "How much software AA should blur one-pixel thick lines: (0.0 - none), (1.0 - lots)" );
static ConVar mat_software_aa_tap_offset( "mat_software_aa_tap_offset", "1.0", 0, "Software AA - adjusts the displacement of the taps used by the software AA shader (default 1.0 - a lower value will make the image sharper, higher will make it blurrier)" );
static ConVar mat_software_aa_debug( "mat_software_aa_debug", "0", 0, "Software AA debug mode: (0 - off), (1 - show number of 'unlike' samples: 0->black, 1->red, 2->green, 3->blue), (2 - show anti-alias blend strength), (3 - show averaged 'unlike' colour)" );
static ConVar mat_software_aa_strength_vgui( "mat_software_aa_strength_vgui", "-1.0", 0, "Same as mat_software_aa_strength, but forced to this value when called by the post vgui AA pass." );

class CEnginePostMaterialProxy : public CEntityMaterialProxy
{
public:
	CEnginePostMaterialProxy();
	virtual ~CEnginePostMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pEntity );
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_pMaterialParam_AAValues;
	IMaterialVar *m_pMaterialParam_AAValues2;
	IMaterialVar *m_pMaterialParam_BloomEnable;
	IMaterialVar *m_pMaterialParam_BloomAmount;
	IMaterialVar *m_pMaterialParam_BloomUVTransform;
	IMaterialVar *m_pMaterialParam_ColCorrectEnable;
	IMaterialVar *m_pMaterialParam_ColCorrectNumLookups;
	IMaterialVar *m_pMaterialParam_ColCorrectDefaultWeight;
	IMaterialVar *m_pMaterialParam_ColCorrectLookupWeights;
	IMaterialVar *m_pMaterialParam_LocalContrastStrength;
	IMaterialVar *m_pMaterialParam_LocalContrastEdgeStrength;
	IMaterialVar *m_pMaterialParam_VignetteStart;
	IMaterialVar *m_pMaterialParam_VignetteEnd;
	IMaterialVar *m_pMaterialParam_VignetteBlurEnable;
	IMaterialVar *m_pMaterialParam_VignetteBlurStrength;
	IMaterialVar *m_pMaterialParam_FadeToBlackStrength;
	IMaterialVar *m_pMaterialParam_DepthBlurFocalDistance;
	IMaterialVar *m_pMaterialParam_DepthBlurStrength;
	IMaterialVar *m_pMaterialParam_ScreenBlurStrength;
	IMaterialVar *m_pMaterialParam_FilmGrainStrength;
	IMaterialVar *m_pMaterialParam_VomitEnable;
	IMaterialVar *m_pMaterialParam_VomitColor1;
	IMaterialVar *m_pMaterialParam_VomitColor2;
	IMaterialVar *m_pMaterialParam_FadeColor;
	IMaterialVar *m_pMaterialParam_FadeType;

public:
	static void SetupEnginePostMaterial( const Vector4D & fullViewportBloomUVs, const Vector4D & fullViewportFBUVs, const Vector2D & destTexSize,
										 bool bPerformSoftwareAA, bool bPerformBloom, bool bPerformColCorrect, float flAAStrength, float flBloomAmount );
	static void SetupEnginePostMaterialAA( bool bPerformSoftwareAA, float flAAStrength );
	static void SetupEnginePostMaterialTextureTransform( const Vector4D & fullViewportBloomUVs, const Vector4D & fullViewportFBUVs, Vector2D destTexSize );

private:
	static float s_vBloomAAValues[4];
	static float s_vBloomAAValues2[4];
	static float s_vBloomUVTransform[4];
	static int   s_PostBloomEnable;
	static float s_PostBloomAmount;
};

float CEnginePostMaterialProxy::s_vBloomAAValues[4]					= { 0.0f, 0.0f, 0.0f, 0.0f };
float CEnginePostMaterialProxy::s_vBloomAAValues2[4]				= { 0.0f, 0.0f, 0.0f, 0.0f };
float CEnginePostMaterialProxy::s_vBloomUVTransform[4]				= { 0.0f, 0.0f, 0.0f, 0.0f };
int   CEnginePostMaterialProxy::s_PostBloomEnable					= 1;
float CEnginePostMaterialProxy::s_PostBloomAmount					= 1.0f;

CEnginePostMaterialProxy::CEnginePostMaterialProxy()
{
	m_pMaterialParam_AAValues					= NULL;
	m_pMaterialParam_AAValues2					= NULL;
	m_pMaterialParam_BloomUVTransform			= NULL;
	m_pMaterialParam_BloomEnable				= NULL;
	m_pMaterialParam_BloomAmount				= NULL;
	m_pMaterialParam_ColCorrectEnable			= NULL;
	m_pMaterialParam_ColCorrectNumLookups		= NULL;
	m_pMaterialParam_ColCorrectDefaultWeight	= NULL;
	m_pMaterialParam_ColCorrectLookupWeights	= NULL;
	m_pMaterialParam_LocalContrastStrength		= NULL;
	m_pMaterialParam_LocalContrastEdgeStrength	= NULL;
	m_pMaterialParam_VignetteStart				= NULL;
	m_pMaterialParam_VignetteEnd				= NULL;
	m_pMaterialParam_VignetteBlurEnable			= NULL;
	m_pMaterialParam_VignetteBlurStrength		= NULL;
	m_pMaterialParam_FadeToBlackStrength		= NULL;
	m_pMaterialParam_DepthBlurFocalDistance		= NULL;
	m_pMaterialParam_DepthBlurStrength			= NULL;
	m_pMaterialParam_ScreenBlurStrength			= NULL;
	m_pMaterialParam_FilmGrainStrength			= NULL;
}

CEnginePostMaterialProxy::~CEnginePostMaterialProxy()
{
	// Do nothing
}

bool CEnginePostMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool bFoundVar = false;

	m_pMaterialParam_AAValues = pMaterial->FindVar( "$AAInternal1", &bFoundVar, false );
	m_pMaterialParam_AAValues2 = pMaterial->FindVar( "$AAInternal3", &bFoundVar, false );
	m_pMaterialParam_BloomUVTransform = pMaterial->FindVar( "$AAInternal2", &bFoundVar, false );
	m_pMaterialParam_BloomEnable = pMaterial->FindVar( "$bloomEnable", &bFoundVar, false );
	m_pMaterialParam_BloomAmount = pMaterial->FindVar( "$bloomAmount", &bFoundVar, false );
	m_pMaterialParam_ColCorrectEnable = pMaterial->FindVar( "$colCorrectEnable", &bFoundVar, false );
	m_pMaterialParam_ColCorrectNumLookups = pMaterial->FindVar( "$colCorrect_NumLookups", &bFoundVar, false );
	m_pMaterialParam_ColCorrectDefaultWeight = pMaterial->FindVar( "$colCorrect_DefaultWeight", &bFoundVar, false );
	m_pMaterialParam_ColCorrectLookupWeights = pMaterial->FindVar( "$colCorrect_LookupWeights", &bFoundVar, false );
	m_pMaterialParam_LocalContrastStrength = pMaterial->FindVar( "$localContrastScale", &bFoundVar, false );
	m_pMaterialParam_LocalContrastEdgeStrength = pMaterial->FindVar( "$localContrastEdgeScale", &bFoundVar, false );
	m_pMaterialParam_VignetteStart = pMaterial->FindVar( "$localContrastVignetteStart", &bFoundVar, false );
	m_pMaterialParam_VignetteEnd = pMaterial->FindVar( "$localContrastVignetteEnd", &bFoundVar, false );
	m_pMaterialParam_VignetteBlurEnable = pMaterial->FindVar( "$blurredVignetteEnable", &bFoundVar, false );
	m_pMaterialParam_VignetteBlurStrength = pMaterial->FindVar( "$blurredVignetteScale", &bFoundVar, false );
	m_pMaterialParam_FadeToBlackStrength = pMaterial->FindVar( "$fadeToBlackScale", &bFoundVar, false );
	m_pMaterialParam_DepthBlurFocalDistance = pMaterial->FindVar( "$depthBlurFocalDistance", &bFoundVar, false );
	m_pMaterialParam_DepthBlurStrength = pMaterial->FindVar( "$depthBlurStrength", &bFoundVar, false );
	m_pMaterialParam_ScreenBlurStrength = pMaterial->FindVar( "$screenBlurStrength", &bFoundVar, false );
	m_pMaterialParam_FilmGrainStrength = pMaterial->FindVar( "$noiseScale", &bFoundVar, false );
	m_pMaterialParam_VomitEnable = pMaterial->FindVar( "$vomitEnable", &bFoundVar, false );
	m_pMaterialParam_VomitColor1 = pMaterial->FindVar( "$vomitColor1", &bFoundVar, false );
	m_pMaterialParam_VomitColor2 = pMaterial->FindVar( "$vomitColor2", &bFoundVar, false );
	m_pMaterialParam_FadeColor = pMaterial->FindVar( "$fadeColor", &bFoundVar, false );
	m_pMaterialParam_FadeType = pMaterial->FindVar( "$fade", &bFoundVar, false );
	return true;
}

void CEnginePostMaterialProxy::OnBind( C_BaseEntity *pEnt )
{
	int nSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( m_pMaterialParam_AAValues )
		m_pMaterialParam_AAValues->SetVecValue( s_vBloomAAValues, 4 );

	if ( m_pMaterialParam_AAValues2 )
		m_pMaterialParam_AAValues2->SetVecValue( s_vBloomAAValues2, 4 );

	if ( m_pMaterialParam_BloomUVTransform )
		m_pMaterialParam_BloomUVTransform->SetVecValue( s_vBloomUVTransform, 4 );

	if ( m_pMaterialParam_BloomEnable )
		m_pMaterialParam_BloomEnable->SetIntValue( s_PostBloomEnable );

	if ( m_pMaterialParam_BloomAmount )
		m_pMaterialParam_BloomAmount->SetFloatValue( s_PostBloomAmount );

	if ( m_pMaterialParam_LocalContrastStrength )
		m_pMaterialParam_LocalContrastStrength->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_LOCAL_CONTRAST_STRENGTH ] );

	if ( m_pMaterialParam_LocalContrastEdgeStrength )
		m_pMaterialParam_LocalContrastEdgeStrength->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_LOCAL_CONTRAST_EDGE_STRENGTH ] );

	if ( m_pMaterialParam_VignetteStart )
		m_pMaterialParam_VignetteStart->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_VIGNETTE_START ] );

	if ( m_pMaterialParam_VignetteEnd )
		m_pMaterialParam_VignetteEnd->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_VIGNETTE_END ] );

	if ( m_pMaterialParam_VignetteBlurEnable )
		m_pMaterialParam_VignetteBlurEnable->SetIntValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_VIGNETTE_BLUR_STRENGTH ] > 0.0f ? 1 : 0 );

	if ( m_pMaterialParam_VignetteBlurStrength )
		m_pMaterialParam_VignetteBlurStrength->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_VIGNETTE_BLUR_STRENGTH ] );

	if ( m_pMaterialParam_FadeToBlackStrength )
		m_pMaterialParam_FadeToBlackStrength->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_FADE_TO_BLACK_STRENGTH ] );

	if ( m_pMaterialParam_DepthBlurFocalDistance )
		m_pMaterialParam_DepthBlurFocalDistance->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_DEPTH_BLUR_FOCAL_DISTANCE ] );

	if ( m_pMaterialParam_DepthBlurStrength )
		m_pMaterialParam_DepthBlurStrength->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_DEPTH_BLUR_STRENGTH ] );

	if ( m_pMaterialParam_ScreenBlurStrength )
		m_pMaterialParam_ScreenBlurStrength->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_SCREEN_BLUR_STRENGTH ] );

	if ( m_pMaterialParam_FilmGrainStrength )
		m_pMaterialParam_FilmGrainStrength->SetFloatValue( s_LocalPostProcessParameters[ nSplitScreenSlot ].m_flParameters[ PPPN_FILM_GRAIN_STRENGTH ] );



	if ( m_pMaterialParam_FadeType )
	{
		int nFadeType = ( s_bViewFadeModulate[nSplitScreenSlot] ) ? 2 : 1;
		nFadeType = ( s_viewFadeColor[nSplitScreenSlot][3] > 0.0f ) ? nFadeType : 0;
		m_pMaterialParam_FadeType->SetIntValue( nFadeType );
	}

	if ( m_pMaterialParam_FadeColor )
	{
		m_pMaterialParam_FadeColor->SetVecValue( s_viewFadeColor[nSplitScreenSlot].Base(), 4 );
	}
}

IMaterial *CEnginePostMaterialProxy::GetMaterial()
{
	if ( m_pMaterialParam_AAValues == NULL)
		return NULL;

	return m_pMaterialParam_AAValues->GetOwningMaterial();
}

void CEnginePostMaterialProxy::SetupEnginePostMaterialAA( bool bPerformSoftwareAA, float flAAStrength )
{
	if ( bPerformSoftwareAA )
	{
		// Pass ConVars to the material by proxy
		//  - the strength of the AA effect (from 0 to 1)
		//  - how much to allow 1-pixel lines to be blurred (from 0 to 1)
		//  - pick one of the two quality modes (5-tap or 9-tap filter)
		//  - optionally enable one of several debug modes (via dynamic combos)
		// NOTE: this order matches pixel shader constants in Engine_Post_ps2x.fxc
		s_vBloomAAValues[0]  = flAAStrength;
		s_vBloomAAValues[1]  = 1.0f - mat_software_aa_blur_one_pixel_lines.GetFloat();
		s_vBloomAAValues[2]  = mat_software_aa_quality.GetInt();
		s_vBloomAAValues[3]  = mat_software_aa_debug.GetInt();
		s_vBloomAAValues2[0] = mat_software_aa_edge_threshold.GetFloat();
		s_vBloomAAValues2[1] = mat_software_aa_tap_offset.GetFloat();
		//s_vBloomAAValues2[2] = unused;
		//s_vBloomAAValues2[3] = unused;
	}
	else
	{
		// Zero-strength AA is interpreted as "AA disabled"
		s_vBloomAAValues[0] = 0.0f;
	}
}

void CEnginePostMaterialProxy::SetupEnginePostMaterialTextureTransform( const Vector4D & fullViewportBloomUVs, const Vector4D & fullViewportFBUVs, Vector2D fbSize )
{
	// Engine_Post uses a UV transform (from (quarter-res) bloom texture coords ('1')
	// to (full-res) framebuffer texture coords ('2')).
	//
	// We compute the UV transform as an offset and a scale, using the texture coordinates
	// of the top-left corner of the screen to compute the offset and the coordinate
	// change from the top-left to the bottom-right of the quad to compute the scale.

	// Take texel coordinates (start = top-left, end = bottom-right):
	Vector2D texelStart1	= Vector2D( fullViewportBloomUVs.x,   fullViewportBloomUVs.y );
	Vector2D texelStart2	= Vector2D( fullViewportFBUVs.x,      fullViewportFBUVs.y );
	Vector2D texelEnd1		= Vector2D( fullViewportBloomUVs.z,   fullViewportBloomUVs.w );
	Vector2D texelEnd2		= Vector2D( fullViewportFBUVs.z,      fullViewportFBUVs.w );
	// ...and transform to UV coordinates:
	Vector2D texRes1		= fbSize / 4;
	Vector2D texRes2		= fbSize;
	Vector2D uvStart1		= ( texelStart1 + Vector2D(0.5,0.5) ) / texRes1;
	Vector2D uvStart2		= ( texelStart2 + Vector2D(0.5,0.5) ) / texRes2;
	Vector2D dUV1			= ( texelEnd1   - texelStart1 )       / texRes1;
	Vector2D dUV2			= ( texelEnd2   - texelStart2 )       / texRes2;

	// We scale about the rect's top-left pixel centre (not the origin) in UV-space:
	//    uv' = ((uv - uvStart1)*uvScale + uvStart1) + uvOffset
	//        = uvScale*uv + uvOffset + uvStart1*(1 - uvScale)
	Vector2D uvOffset		= uvStart2 - uvStart1;
	Vector2D uvScale		= dUV2 / dUV1;
	uvOffset				= uvOffset + uvStart1*(Vector2D(1,1) - uvScale);

	s_vBloomUVTransform[0]	= uvOffset.x;
	s_vBloomUVTransform[1]	= uvOffset.y;
	s_vBloomUVTransform[2]	= uvScale.x;
	s_vBloomUVTransform[3]	= uvScale.y;
}

void CEnginePostMaterialProxy::SetupEnginePostMaterial(	const Vector4D & fullViewportBloomUVs, const Vector4D & fullViewportFBUVs, const Vector2D & destTexSize,
														bool bPerformSoftwareAA, bool bPerformBloom, bool bPerformColCorrect, float flAAStrength, float flBloomAmount )
{
	s_PostBloomEnable = bPerformBloom ? 1 : 0;
	s_PostBloomAmount = flBloomAmount;

	SetupEnginePostMaterialAA( bPerformSoftwareAA, flAAStrength );

	SetupEnginePostMaterialTextureTransform( fullViewportBloomUVs, fullViewportFBUVs, destTexSize );
}

EXPOSE_MATERIAL_PROXY( CEnginePostMaterialProxy, engine_post );

static void DrawBloomDebugBoxes( IMatRenderContext *pRenderContext, int nX, int nY, int nWidth, int nHeight )
{
	// draw inset rects which should have a centered bloom 
	pRenderContext->PushRenderTargetAndViewport();
	pRenderContext->SetRenderTarget(NULL);

	// full screen clear
	pRenderContext->Viewport( nX, nY, nWidth, nHeight );
	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, true );

	// inset for screensafe
	int inset = 64;
	int size = 32;

	// centerish, translating
	static int wx = 0;
	wx = ( wx + 1 ) & 63;

	pRenderContext->Viewport( nWidth / 2 + nX + wx, nY + nHeight / 2, size, size );
	pRenderContext->ClearColor3ub( 255, 255, 255 );
	pRenderContext->ClearBuffers( true, true );

	// upper left
	pRenderContext->Viewport( nX + inset, nY + inset, size, size );
	pRenderContext->ClearBuffers( true, true );

	// upper right
	pRenderContext->Viewport( nX + nWidth - inset - size, nY + inset, size, size );
	pRenderContext->ClearBuffers( true, true );
	
	// lower right
	pRenderContext->Viewport( nX + nWidth - inset - size, nY + nHeight - inset - size, size, size );
	pRenderContext->ClearBuffers( true, true );
	
	// lower left
	pRenderContext->Viewport( nX + inset, nX + nHeight - inset - size, size, size );
	pRenderContext->ClearBuffers( true, true );
	
	// restore
	pRenderContext->PopRenderTargetAndViewport();
}

static float GetBloomAmount( void )
{
	HDRType_t hdrType = g_pMaterialSystemHardwareConfig->GetHDRType();

	bool bBloomEnabled = (mat_hdr_level.GetInt() >= 1);
	
	if ( !engine->MapHasHDRLighting() )
		bBloomEnabled = false;
	if ( mat_force_bloom.GetInt() )
		bBloomEnabled = true;
	if ( mat_disable_bloom.GetInt() )
		bBloomEnabled = false;
	if ( building_cubemaps.GetBool() )
		bBloomEnabled = false;
	if ( mat_fullbright.GetInt() == 1 )
	{
		bBloomEnabled = false;
	}

	float flBloomAmount=0.0;

	if (bBloomEnabled)
	{
		static float currentBloomAmount = 1.0f;
		float rate = mat_bloomamount_rate.GetFloat();

		// Use the appropriate bloom scale settings.  Mapmakers's overrides the convar settings.
		currentBloomAmount = GetCurrentBloomScale() * rate + ( 1.0f - rate ) * currentBloomAmount;
		flBloomAmount = currentBloomAmount;

		if (IsX360())
		{
			//we want to scale the bloom effect down because the effect textures are lower reolution on the 360.
			//target match 1280x1024
			if ( (g_pMaterialSystem->GetCurrentConfigForVideoCard().m_VideoMode.m_Height == 720) )
			{
				flBloomAmount *= (720.0f/1024.0f);
			}
			else //640x480
			{
				flBloomAmount *= (480.0f/1024.0f);
			}
		}
	}

	if ( hdrType == HDR_TYPE_NONE )
	{
		flBloomAmount *= mat_non_hdr_bloom_scalefactor.GetFloat();
	}

	flBloomAmount *= mat_bloom_scalefactor_scalar.GetFloat();

	return flBloomAmount;
}

static bool s_bScreenEffectTextureIsUpdated = false;

// WARNING: This function sets rendertarget and viewport. Save and restore is left to the caller.
static void DownsampleFBQuarterSize( IMatRenderContext *pRenderContext, int nSrcWidth, int nSrcHeight, ITexture* pDest,
									 bool bFloatHDR = false )
{
	Assert( pRenderContext );
	Assert( pDest );

	IMaterial *downsample_mat = materials->FindMaterial( bFloatHDR ? "dev/downsample" : "dev/downsample_non_hdr", TEXTURE_GROUP_OTHER, true );

	// *Everything* in here relies on the small RTs being exactly 1/4 the full FB res
	Assert( pDest->GetActualWidth()  == nSrcWidth  / 4 );
	Assert( pDest->GetActualHeight() == nSrcHeight / 4 );
		
	/*
	bool bFound;
	IMaterialVar *pbloomexpvar = downsample_mat->FindVar( "$bloomexp", &bFound, false );
	if ( bFound )
	{
		pbloomexpvar->SetFloatValue( g_flBloomExponent );
	}

	IMaterialVar *pbloomsaturationvar = downsample_mat->FindVar( "$bloomsaturation", &bFound, false );
	if ( bFound )
	{
		pbloomsaturationvar->SetFloatValue( g_flBloomSaturation );
	}
	*/

	// downsample fb to rt0
	SetRenderTargetAndViewPort( pDest );
	// note the -2's below. Thats because we are downsampling on each axis and the shader
	// accesses pixels on both sides of the source coord
	pRenderContext->DrawScreenSpaceRectangle(	downsample_mat, 0, 0, nSrcWidth/4, nSrcHeight/4,
												0, 0, nSrcWidth-2, nSrcHeight-2,
												nSrcWidth, nSrcHeight );

	if ( IsX360() )
	{
		pRenderContext->CopyRenderTargetToTextureEx( pDest, 0, NULL, NULL );
	}
}

static void Generate8BitBloomTexture( IMatRenderContext *pRenderContext, 
										int x, int y, int w, int h, bool bExtractBloomRange, bool bClearRGB = true )
{
	pRenderContext->PushRenderTargetAndViewport();
	ITexture *pSrc = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
	int nSrcWidth = pSrc->GetActualWidth();
	int nSrcHeight = pSrc->GetActualHeight(); //,nViewportHeight;

	IMaterial *xblur_mat = materials->FindMaterial( "dev/blurfilterx_nohdr", TEXTURE_GROUP_OTHER, true );
	IMaterial *yblur_mat = NULL;
	if ( bClearRGB )
	{
		yblur_mat = materials->FindMaterial( "dev/blurfiltery_nohdr_clear", TEXTURE_GROUP_OTHER, true );
	}
	else
	{
		yblur_mat = materials->FindMaterial( "dev/blurfiltery_nohdr", TEXTURE_GROUP_OTHER, true );
	}
	ITexture *dest_rt0 = materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET );
	ITexture *dest_rt1 = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );

	// *Everything* in here relies on the small RTs being exactly 1/4 the full FB res
	Assert( dest_rt0->GetActualWidth()  == pSrc->GetActualWidth()  / 4 );
	Assert( dest_rt0->GetActualHeight() == pSrc->GetActualHeight() / 4 );
	Assert( dest_rt1->GetActualWidth()  == pSrc->GetActualWidth()  / 4 );
	Assert( dest_rt1->GetActualHeight() == pSrc->GetActualHeight() / 4 );

	// downsample fb to rt0
	if ( bExtractBloomRange )
	{
		DownsampleFBQuarterSize( pRenderContext, nSrcWidth, nSrcHeight, dest_rt0 );
	}
	else
	{
		// just downsample, don't apply bloom extraction math
		DownsampleFBQuarterSize( pRenderContext, nSrcWidth, nSrcHeight, dest_rt0, true );
	}

	// guassian blur x rt0 to rt1
	SetRenderTargetAndViewPort( dest_rt1 );
	pRenderContext->DrawScreenSpaceRectangle(	xblur_mat, 0, 0, nSrcWidth/4, nSrcHeight/4,
												0, 0, nSrcWidth/4-1, nSrcHeight/4-1,
												nSrcWidth/4, nSrcHeight/4 );
	if ( IsX360() )
	{
		pRenderContext->CopyRenderTargetToTextureEx( dest_rt1, 0, NULL, NULL );
	}

	// GR - gaussian blur y rt1 to rt0
	SetRenderTargetAndViewPort( dest_rt0 );
	IMaterialVar *pBloomAmountVar = yblur_mat->FindVar( "$bloomamount", NULL );
	pBloomAmountVar->SetFloatValue( 1.0f );	// the bloom amount is now applied in engine_post or bloomadd materials
	pRenderContext->DrawScreenSpaceRectangle(	yblur_mat, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
												0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
												nSrcWidth / 4, nSrcHeight / 4 );

	if ( IsX360() )
	{
		pRenderContext->CopyRenderTargetToTextureEx( dest_rt0, 0, NULL, NULL );
	}

	pRenderContext->PopRenderTargetAndViewport();
}

static void DoTonemapping( IMatRenderContext *pRenderContext, int nX, int nY, int nWidth, int nHeight, float flAutoExposureMin, float flAutoExposureMax )
{
	// Skip if HDR disabled
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
		return;

	// Update HDR histogram
	if ( mat_dynamic_tonemapping.GetInt() )
	{
		if ( s_bScreenEffectTextureIsUpdated == false )
		{
			// FIXME: nX/nY/nWidth/nHeight are used here, but the equivalent parameters are ignored in Generate8BitBloomTexture
			UpdateScreenEffectTexture( 0, nX, nY, nWidth, nHeight, false );
			s_bScreenEffectTextureIsUpdated = true;
		}

		GetCurrentTonemappingSystem()->IssueAndReceiveBucketQueries();

		float flTargetScalar = GetCurrentTonemappingSystem()->ComputeTargetTonemapScalar();
		float flTargetScalarClamped = MAX( flAutoExposureMin, MIN( flAutoExposureMax, flTargetScalar ) );
		flTargetScalarClamped = MAX( 0.001f, flTargetScalarClamped ); // Don't let this go to 0!
		GetCurrentTonemappingSystem()->SetTonemapScale( pRenderContext, flTargetScalarClamped, flAutoExposureMin, flAutoExposureMax );

		if ( mat_show_histogram.GetInt() )
		{
			bool bDrawTextThisFrame = true;
			if ( IsX360() )
			{
				static float s_flLastTimeUpdate = 0.0f;
				if ( int( gpGlobals->curtime ) - int( s_flLastTimeUpdate ) >= 2 )
				{
					s_flLastTimeUpdate = gpGlobals->curtime;
					bDrawTextThisFrame = true;
				}
				else
				{
					bDrawTextThisFrame = false;
				}
			}

			if ( bDrawTextThisFrame == true )
			{
				if ( mat_tonemap_algorithm.GetInt() == 0 )
				{
					engine->Con_NPrintf( 25 + ( nY / 10 ), "(Original algorithm) Target Scalar = %4.2f  Min/Max( %4.2f, %4.2f )  Current Scalar: %4.2f",
										 flTargetScalar, flAutoExposureMin, flAutoExposureMax, GetCurrentTonemappingSystem()->GetCurrentTonemappingScale() );
				}
				else
				{
					if ( IsX360() )
					{
						engine->Con_NPrintf( 25 + ( nY / 10 ), "[mat_show_histogram]  Target Scalar = %4.2f  Min/Max( %4.2f, %4.2f )  Final Scalar: %4.2f\n",
							GetCurrentTonemappingSystem()->ComputeTargetTonemapScalar( true ), flAutoExposureMin, flAutoExposureMax, GetCurrentTonemappingSystem()->GetCurrentTonemappingScale() );
					}
					else
					{
						engine->Con_NPrintf( 25 + ( nY / 10 ), "%.2f%% of pixels above %d%% target @ %4.2f%%  Target Scalar = %4.2f  Min/Max( %4.2f, %4.2f )  Final Scalar: %4.2f",
											 mat_tonemap_percent_bright_pixels.GetFloat(), mat_tonemap_percent_target.GetInt(),
											 ( GetCurrentTonemappingSystem()->FindLocationOfPercentBrightPixels( mat_tonemap_percent_bright_pixels.GetFloat(), mat_tonemap_percent_target.GetFloat() ) * 100.0f ),
											 GetCurrentTonemappingSystem()->ComputeTargetTonemapScalar( true ), flAutoExposureMin, flAutoExposureMax, GetCurrentTonemappingSystem()->GetCurrentTonemappingScale() );
					}
				}
			}
		}
	}
}

static void CenterScaleQuadUVs( Vector4D & quadUVs, const Vector2D & uvScale )
{
	Vector2D uvMid	= 0.5f*Vector2D( ( quadUVs.z + quadUVs.x ), ( quadUVs.w + quadUVs.y ) );
	Vector2D uvRange= 0.5f*Vector2D( ( quadUVs.z - quadUVs.x ), ( quadUVs.w - quadUVs.y ) );
	quadUVs.x		= uvMid.x - uvScale.x*uvRange.x;
	quadUVs.y		= uvMid.y - uvScale.y*uvRange.y;
	quadUVs.z		= uvMid.x + uvScale.x*uvRange.x;
	quadUVs.w		= uvMid.y + uvScale.y*uvRange.y;
}

static ConVar r_queued_post_processing( "r_queued_post_processing", "0" );

// How much to dice up the screen during post-processing on 360
// This has really marginal effects, but 4x1 does seem vaguely better for post-processing
static ConVar mat_postprocess_x( "mat_postprocess_x", "4" );
static ConVar mat_postprocess_y( "mat_postprocess_y", "1" );
static ConVar mat_postprocess_enable( "mat_postprocess_enable", "1", FCVAR_CHEAT );

void DoEnginePostProcessing( int x, int y, int w, int h, bool bFlashlightIsOn, bool bPostVGui )
{
	// don't do this if disabled or in alt-tab
	if ( s_bOverridePostProcessingDisable || w <=0 || h <= 0 )
	{
		return;
	}

	CMatRenderContextPtr pRenderContext( materials );

	if ( r_queued_post_processing.GetInt() )
	{
		ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
		if ( pCallQueue )
		{
			pCallQueue->QueueCall( DoEnginePostProcessing, x, y, w, h, bFlashlightIsOn, bPostVGui );
			return;
		}
	}

	#if defined( _X360 )
		pRenderContext->PushVertexShaderGPRAllocation( 16 ); //max out pixel shader threads
	#endif

	GetTonemapSettingsFromEnvTonemapController();

	g_bFlashlightIsOn = bFlashlightIsOn;

	// Use the appropriate autoexposure min / max settings.
	// Mapmaker's overrides the convar settings.
	float flAutoExposureMin;
	float flAutoExposureMax;
	GetExposureRange( &flAutoExposureMin, &flAutoExposureMax );

	if ( mat_debug_bloom.GetInt() == 1 )
	{
		DrawBloomDebugBoxes( pRenderContext, x, y, w, h );
	}

	s_bScreenEffectTextureIsUpdated = false; // Force an update in tone mapping code
	DoTonemapping( pRenderContext, x, y, w, h, flAutoExposureMin, flAutoExposureMax );

	if ( mat_postprocess_enable.GetInt() == 0 )
	{
		GetCurrentTonemappingSystem()->DisplayHistogram();

		#if defined( _X360 )
			pRenderContext->PopVertexShaderGPRAllocation();
		#endif

		return;
	}

	// Set software-AA on by default for 360
	if ( mat_software_aa_strength.GetFloat() == -1.0f )
	{
		if ( IsX360() )
		{
			mat_software_aa_strength.SetValue( 1.0f );
			if ( g_pMaterialSystem->GetCurrentConfigForVideoCard().m_VideoMode.m_Height > 480 )
			{
				mat_software_aa_quality.SetValue( 0 );
			}
			else
			{
				// For standard-def, we have fewer pixels so we can afford 'high quality' mode (5->9 taps/pixel)
				mat_software_aa_quality.SetValue( 1 );

				// Disable in 480p for now
				mat_software_aa_strength.SetValue( 0.0f );
			}
		}
		else
		{
			mat_software_aa_strength.SetValue( 0.0f );
		}
	}

	// Same trick for setting up the vgui aa strength
	if ( mat_software_aa_strength_vgui.GetFloat() == -1.0f )
	{
		if ( IsX360() && (g_pMaterialSystem->GetCurrentConfigForVideoCard().m_VideoMode.m_Height == 720) )
		{
			mat_software_aa_strength_vgui.SetValue( 2.0f );
		}
		else
		{
			mat_software_aa_strength_vgui.SetValue( 1.0f );
		}
	}

	float flAAStrength;

	// We do a second AA blur pass over the TF intro menus. use mat_software_aa_strength_vgui there instead
	if ( IsX360() && bPostVGui )
	{
		flAAStrength = mat_software_aa_strength_vgui.GetFloat();
	}
	else
	{
		flAAStrength = mat_software_aa_strength.GetFloat();
	}

	// Bloom, software-AA and color-correction (applied in 1 pass, after generation of the bloom texture)
	float flBloomScale = GetBloomAmount();
	bool  bPerformSoftwareAA	= ( flAAStrength != 0.0f );
	bool  bPerformBloom			= !bPostVGui && ( flBloomScale > 0.0f );
	bool  bPerformColCorrect	= !bPostVGui && 
								  g_pColorCorrectionMgr->HasNonZeroColorCorrectionWeights() &&
								  mat_colorcorrection.GetInt();

	pRenderContext->EnableColorCorrection( bPerformColCorrect );

	bool bPerformLocalContrastEnhancement = false;
	IMaterial* pPostMat;
	if ( engine->IsSplitScreenActive() )
		pPostMat = materials->FindMaterial( "dev/engine_post_splitscreen", TEXTURE_GROUP_OTHER, true );
	else
		pPostMat = materials->FindMaterial( "dev/engine_post", TEXTURE_GROUP_OTHER, true );
	if ( pPostMat )
	{
		IMaterialVar* pMatVar = pPostMat->FindVar( "$localcontrastenable", NULL, false );
		if ( pMatVar )
		{
			bPerformLocalContrastEnhancement = pMatVar->GetIntValue() && mat_local_contrast_enable.GetBool();
		}
	}

	if ( true )
	{
		ITexture *pSrc = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
		int nSrcWidth = pSrc->GetActualWidth();
		int nSrcHeight = pSrc->GetActualHeight();

		ITexture *dest_rt1 = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );

		if ( !s_bScreenEffectTextureIsUpdated )
		{
			UpdateScreenEffectTexture( 0, x, y, w, h, false );
			s_bScreenEffectTextureIsUpdated = true;
		}

		if ( bPerformBloom || bPerformLocalContrastEnhancement )
		{
			Generate8BitBloomTexture( pRenderContext, x, y, w, h, true, false );
		}




		// Now add bloom (dest_rt0) to the framebuffer and perform software anti-aliasing and
		// colour correction, all in one pass (improves performance, reduces quantization errors)
		//
		// First, set up texel coords (in the bloom and fb textures) at the centres of the outer pixel of the viewport:
		float flFbWidth = ( float )pSrc->GetActualWidth();
		float flFbHeight = ( float )pSrc->GetActualHeight();

		Vector4D fullViewportPostSrcCorners(	0.0f,	-0.5f,	nSrcWidth/4-1,	nSrcHeight/4-1 );
		Vector4D fullViewportPostSrcRect( nSrcWidth * ( ( x + 0 ) / flFbWidth ) / 4.0f + 0.0f, nSrcHeight * ( ( y + 0 ) / flFbHeight ) / 4.0f - 0.5f,
										  nSrcWidth * ( ( x + w ) / flFbWidth ) / 4.0f - 1.0f, nSrcHeight * ( ( y + h ) / flFbHeight ) / 4.0f - 1.0f );
		Vector4D fullViewportPostDestCorners(	0.0f,	 0.0f,	nSrcWidth - 1,	nSrcHeight - 1 );
		Rect_t   fullViewportPostDestRect = {	x,		 y,		w,				h };
		Vector2D destTexSize(									nSrcWidth,		nSrcHeight );

		// When the viewport is not fullscreen, the UV-space size of a pixel changes
		// (due to a stretchrect blit being used in UpdateScreenEffectTexture()), so
		// we need to adjust the corner-pixel UVs sent to our drawrect call:
		Vector2D uvScale(	( nSrcWidth  - ( nSrcWidth  / (float)w ) ) / ( nSrcWidth  - 1 ),
							( nSrcHeight - ( nSrcHeight / (float)h ) ) / ( nSrcHeight - 1 ) );
		CenterScaleQuadUVs( fullViewportPostSrcCorners,  uvScale );
		CenterScaleQuadUVs( fullViewportPostDestCorners, uvScale );

		Rect_t   partialViewportPostDestRect   = fullViewportPostDestRect;
		Vector4D partialViewportPostSrcCorners = fullViewportPostSrcCorners;
		if ( debug_postproc.GetInt() == 2 )
		{
			// Restrict the post effects to the centre quarter of the screen
			// (we only use a portion of the bloom texture, so this *does* affect bloom texture UVs)
			partialViewportPostDestRect.x		+= 0.25f*fullViewportPostDestRect.width;
			partialViewportPostDestRect.y		+= 0.25f*fullViewportPostDestRect.height;
			partialViewportPostDestRect.width	-= 0.50f*fullViewportPostDestRect.width;
			partialViewportPostDestRect.height	-= 0.50f*fullViewportPostDestRect.height;

			// This math interprets texel coords as being at corner pixel centers (*not* at corner vertices):
			Vector2D uvScale(	1.0f - ( (w / 2) / (float)(w - 1) ),
								1.0f - ( (h / 2) / (float)(h - 1) ) );
			CenterScaleQuadUVs( partialViewportPostSrcCorners, uvScale );
		}

		// Temporary hack... Color correction was crashing on the first frame 
		// when run outside the debugger for some mods (DoD). This forces it to skip
		// a frame, ensuring we don't get the weird texture crash we otherwise would.
		// FIXME: This will be removed when the true cause is found [added: Main CL 144694]
		static bool bFirstFrame = !IsX360();
		if ( !bFirstFrame || !bPerformColCorrect )
		{
			HDRType_t hdrType = g_pMaterialSystemHardwareConfig->GetHDRType();
			if ( hdrType == HDR_TYPE_FLOAT )
			{
				// reset to render the final combine passes to the "real" display backbuffer
				pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_BACK_BUFFER_INDEX, BACK_BUFFER_INDEX_DEFAULT );
				pRenderContext->SetRenderTarget( NULL );
			}

			Vector4D v4dFullViewportPostDestRect( fullViewportPostDestRect.x, fullViewportPostDestRect.y,
												  fullViewportPostDestRect.x + fullViewportPostDestRect.width - 1,
												  fullViewportPostDestRect.y + fullViewportPostDestRect.height - 1 );

			CEnginePostMaterialProxy::SetupEnginePostMaterial( fullViewportPostSrcRect, v4dFullViewportPostDestRect, destTexSize, bPerformSoftwareAA, bPerformBloom, bPerformColCorrect, flAAStrength, flBloomScale );

			pRenderContext->DrawScreenSpaceRectangle( pPostMat,
													  0, 0,
													  partialViewportPostDestRect.width, partialViewportPostDestRect.height,
													  fullViewportPostSrcRect.x, fullViewportPostSrcRect.y,
													  fullViewportPostSrcRect.z, fullViewportPostSrcRect.w,

													  dest_rt1->GetActualWidth(), dest_rt1->GetActualHeight(),
													  GetClientWorldEntity()->GetClientRenderable(),
													  mat_postprocess_x.GetInt(), mat_postprocess_y.GetInt() );
		}
		bFirstFrame = false;
	}

	GetCurrentTonemappingSystem()->DisplayHistogram();

	#if defined( _X360 )
		pRenderContext->PopVertexShaderGPRAllocation();
	#endif
}

void DoBlurFade( float flStrength, float flDesaturate, int x, int y, int w, int h )
{
	if ( flStrength < 0.0001f )
	{
		return;
	}

	UpdateScreenEffectTexture();

	CMatRenderContextPtr pRenderContext( materials );
	Generate8BitBloomTexture( pRenderContext, x, y, w, h, false, false );

	int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
	pRenderContext->GetViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

	int nRtWidth, nRtHeight;
	pRenderContext->GetRenderTargetDimensions( nRtWidth, nRtHeight );

	IMaterial* pMat = materials->FindMaterial( "dev/fade_blur", TEXTURE_GROUP_OTHER, true );
	bool bFound = false;
	IMaterialVar* pVar = pMat->FindVar( "$c0_x", &bFound );
	if ( pVar )
	{
		pVar->SetFloatValue( flStrength );
	}

	// Desaturate strength
	pVar = pMat->FindVar( "$c1_x", &bFound );
	if ( pVar )
	{
		pVar->SetFloatValue( flDesaturate );
	}

	pRenderContext->DrawScreenSpaceRectangle(	pMat, 0, 0, nViewportWidth, nViewportHeight,
												nViewportX, nViewportY,
												nViewportX + nViewportWidth - 1, nViewportY + nViewportHeight - 1,
												nRtWidth, nRtHeight );
}

// Motion Blur Material Proxy =========================================================================================
static float g_vMotionBlurValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static float g_vMotionBlurViewportValues[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
class CMotionBlurMaterialProxy : public CEntityMaterialProxy
{
public:
	CMotionBlurMaterialProxy();
	virtual ~CMotionBlurMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pEntity );
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_pMaterialParam;
	IMaterialVar *m_pMaterialParamViewport;
};

CMotionBlurMaterialProxy::CMotionBlurMaterialProxy()
{
	m_pMaterialParam = NULL;
}

CMotionBlurMaterialProxy::~CMotionBlurMaterialProxy()
{
	// Do nothing
}

bool CMotionBlurMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool bFoundVar = false;

	m_pMaterialParam = pMaterial->FindVar( "$MotionBlurInternal", &bFoundVar, false );
	if ( bFoundVar == false)
		return false;

	m_pMaterialParamViewport = pMaterial->FindVar( "$MotionBlurViewportInternal", &bFoundVar, false );
	if ( bFoundVar == false)
		return false;

	return true;
}

void CMotionBlurMaterialProxy::OnBind( C_BaseEntity *pEnt )
{
	if ( m_pMaterialParam != NULL )
	{
		m_pMaterialParam->SetVecValue( g_vMotionBlurValues, 4 );
	}

	if ( m_pMaterialParamViewport != NULL )
	{
		m_pMaterialParamViewport->SetVecValue( g_vMotionBlurViewportValues, 4 );
	}
}

IMaterial *CMotionBlurMaterialProxy::GetMaterial()
{
	if ( m_pMaterialParam == NULL)
		return NULL;

	return m_pMaterialParam->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CMotionBlurMaterialProxy, MotionBlur );

//=====================================================================================================================
// Image-space Motion Blur ============================================================================================
//=====================================================================================================================
ConVar mat_motion_blur_enabled( "mat_motion_blur_enabled", "1" );

ConVar mat_motion_blur_forward_enabled( "mat_motion_blur_forward_enabled", "0" );
ConVar mat_motion_blur_falling_min( "mat_motion_blur_falling_min", "10.0" );

ConVar mat_motion_blur_falling_max( "mat_motion_blur_falling_max", "20.0" );
ConVar mat_motion_blur_falling_intensity( "mat_motion_blur_falling_intensity", "1.0" );
//ConVar mat_motion_blur_roll_intensity( "mat_motion_blur_roll_intensity", "1.0" );
ConVar mat_motion_blur_rotation_intensity( "mat_motion_blur_rotation_intensity", "1.0" );
ConVar mat_motion_blur_strength( "mat_motion_blur_strength", "1.0" );

struct MotionBlurHistory_t
{
	MotionBlurHistory_t()
	{
		m_flLastTimeUpdate = 0.0f;
		m_flPreviousPitch = 0.0f;
		m_flPreviousYaw = 0.0f;
		m_vPreviousPositon.Init( 0.0f, 0.0f, 0.0f );
		m_mPreviousFrameBasisVectors;
		m_flNoRotationalMotionBlurUntil = 0.0f;
		SetIdentityMatrix( m_mPreviousFrameBasisVectors );
	}

	float m_flLastTimeUpdate;
	float m_flPreviousPitch;
	float m_flPreviousYaw;
	Vector m_vPreviousPositon;
	matrix3x4_t m_mPreviousFrameBasisVectors;
	float m_flNoRotationalMotionBlurUntil;
};

void DoImageSpaceMotionBlur( const CViewSetup &view )
{


	if ( ( !mat_motion_blur_enabled.GetInt() ) || ( view.m_nMotionBlurMode == MOTION_BLUR_DISABLE ) )
	{
		return;
	}

	int x = view.x;
	int y = view.y;
	int w = view.width;
	int h = view.height;

	bool bSFMBlur = ( view.m_nMotionBlurMode == MOTION_BLUR_SFM );

	//======================================================================================================//
	// Get these convars here to make it easier to remove them later and to default each client differently //
	//======================================================================================================//
	float flMotionBlurRotationIntensity = mat_motion_blur_rotation_intensity.GetFloat() * 0.15f; // The default is to not blur past 15% of the range
	float flMotionBlurRollIntensity = 0.3f; // * mat_motion_blur_roll_intensity.GetFloat(); // The default is to not blur past 30% of the range
	float flMotionBlurFallingIntensity = mat_motion_blur_falling_intensity.GetFloat();
	float flMotionBlurFallingMin = mat_motion_blur_falling_min.GetFloat();
	float flMotionBlurFallingMax = mat_motion_blur_falling_max.GetFloat();
	float flMotionBlurGlobalStrength = mat_motion_blur_strength.GetFloat();

	//===============================================================================//
	// Set global g_vMotionBlurValues[4] values so material proxy can get the values //
	//===============================================================================//
	if ( true )
	{
		//=====================//
		// Previous frame data //
		//=====================//
		static MotionBlurHistory_t s_History[ MAX_SPLITSCREEN_PLAYERS ];

		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		MotionBlurHistory_t &history = s_History[ GET_ACTIVE_SPLITSCREEN_SLOT() ];

		//float vPreviousSideVec[3] = { s_mPreviousFrameBasisVectors[0][1], s_mPreviousFrameBasisVectors[1][1], s_mPreviousFrameBasisVectors[2][1] };
		//float vPreviousForwardVec[3] = { s_mPreviousFrameBasisVectors[0][0], s_mPreviousFrameBasisVectors[1][0], s_mPreviousFrameBasisVectors[2][0] };
		//float vPreviousUpVec[3] = { s_mPreviousFrameBasisVectors[0][2], s_mPreviousFrameBasisVectors[1][2], s_mPreviousFrameBasisVectors[2][2] };

		float flTimeElapsed;

		// Motion blur driven by CViewSetup, not engine time (currently only driven by SFM)
		if ( bSFMBlur )
		{
			history.m_flLastTimeUpdate = 0.0f;											// Don't care about these, but zero them out
			history.m_flNoRotationalMotionBlurUntil = 0.0f;								//

			flTimeElapsed = view.m_flShutterTime;

			history.m_vPreviousPositon[0] = view.m_vShutterOpenPosition.x;				//
			history.m_vPreviousPositon[1] = view.m_vShutterOpenPosition.y;				// Slam "previous" values to shutter open values
			history.m_vPreviousPositon[2] = view.m_vShutterOpenPosition.z;				//
			AngleMatrix( view.m_shutterOpenAngles, history.m_mPreviousFrameBasisVectors );//

			history.m_flPreviousPitch = view.m_shutterOpenAngles[PITCH];					// Get "previous" pitch & wrap to +-180
			while ( history.m_flPreviousPitch > 180.0f )
				history.m_flPreviousPitch -= 360.0f;
			while ( history.m_flPreviousPitch < -180.0f )
				history.m_flPreviousPitch += 360.0f;

			history.m_flPreviousYaw = view.m_shutterOpenAngles[YAW];						// Get "previous" yaw & wrap to +-180
			while ( history.m_flPreviousYaw > 180.0f )
				history.m_flPreviousYaw -= 360.0f;
			while ( history.m_flPreviousYaw < -180.0f )
				history.m_flPreviousYaw += 360.0f;
		}
		else // view.m_nDoMotionBlurMode == MOTION_BLUR_GAME 
		{
			flTimeElapsed = gpGlobals->realtime - history.m_flLastTimeUpdate;
		}


		//===================================//
		// Get current pitch & wrap to +-180 //
		//===================================//
		float flCurrentPitch = view.angles[PITCH];
		if ( bSFMBlur )
			flCurrentPitch = view.m_shutterCloseAngles[PITCH];
		while ( flCurrentPitch > 180.0f )
			flCurrentPitch -= 360.0f;
		while ( flCurrentPitch < -180.0f )
			flCurrentPitch += 360.0f;

		//=================================//
		// Get current yaw & wrap to +-180 //
		//=================================//
		float flCurrentYaw = view.angles[YAW];
		if ( bSFMBlur )
			flCurrentYaw = view.m_shutterCloseAngles[YAW];
		while ( flCurrentYaw > 180.0f )
			flCurrentYaw -= 360.0f;
		while ( flCurrentYaw < -180.0f )
			flCurrentYaw += 360.0f;



		/*engine->Con_NPrintf( 0, "Blur Pitch: %6.2f   Yaw: %6.2f", flCurrentPitch, flCurrentYaw );
		engine->Con_NPrintf( 1, "Blur FOV: %6.2f   Aspect: %6.2f   Ortho: %s", view.fov, view.m_flAspectRatio, view.m_bOrtho ? "Yes" : "No" );
		engine->Con_NPrintf( 2, "View Angles: %6.2f %6.2f %6.2f", XYZ(view.angles) );*/

		//===========================//
		// Get current basis vectors //
		//===========================//
		matrix3x4_t mCurrentBasisVectors;

		if ( bSFMBlur )
		{
			AngleMatrix( view.m_shutterCloseAngles, mCurrentBasisVectors );
		}
		else
		{
			AngleMatrix( view.angles, mCurrentBasisVectors );
		}


		Vector vCurrentSideVec(  mCurrentBasisVectors[0][1], mCurrentBasisVectors[1][1], mCurrentBasisVectors[2][1] );
		Vector vCurrentForwardVec( mCurrentBasisVectors[0][0], mCurrentBasisVectors[1][0], mCurrentBasisVectors[2][0] );
		//Vector vCurrentUpVec( mCurrentBasisVectors[0][2], mCurrentBasisVectors[1][2], mCurrentBasisVectors[2][2] );

		//===========================================================================//
		// Get current position (shutter close time when SFM is driving motion blur) //
		//===========================================================================//
		Vector vCurrentPosition = view.origin;

		if ( bSFMBlur )
		{
			vCurrentPosition[0] = view.m_vShutterClosePosition.x;
			vCurrentPosition[1] = view.m_vShutterClosePosition.y;
			vCurrentPosition[2] = view.m_vShutterClosePosition.z;
		}

		//===============================================================//
		// Evaluate change in position to determine if we need to update //
		//===============================================================//
		Vector vPositionChange( 0.0f, 0.0f, 0.0f );
		VectorSubtract( history.m_vPreviousPositon, vCurrentPosition, vPositionChange );
		if ( ( VectorLength( vPositionChange ) > 30.0f ) && ( flTimeElapsed >= 0.5f ) && !bSFMBlur )
		{
			//=======================================================//
			// If we moved a far distance in one frame and more than //
			// half a second elapsed, disable motion blur this frame //
			//=======================================================//
			//engine->Con_NPrintf( 8, " Pos change && time > 0.5 seconds %f ", gpGlobals->realtime );

			g_vMotionBlurValues[0] = 0.0f;
			g_vMotionBlurValues[1] = 0.0f;
			g_vMotionBlurValues[2] = 0.0f;
			g_vMotionBlurValues[3] = 0.0f;
		}
		else if ( ( flTimeElapsed > ( 1.0f / 15.0f ) ) && !bSFMBlur )
		{
			//==========================================//
			// If slower than 15 fps, don't motion blur //
			//==========================================//
			g_vMotionBlurValues[0] = 0.0f;
			g_vMotionBlurValues[1] = 0.0f;
			g_vMotionBlurValues[2] = 0.0f;
			g_vMotionBlurValues[3] = 0.0f;
		}
		else if ( ( VectorLength( vPositionChange ) > 50.0f ) && !bSFMBlur )
		{
			//================================================================================//
			// We moved a far distance in a frame, use the same motion blur as last frame	  //
			// because I think we just went through a portal (should we ifdef this behavior?) //
			//================================================================================//
			//engine->Con_NPrintf( 8, " Position changed %f units @ %.2f time ", VectorLength( vPositionChange ), gpGlobals->realtime );

			history.m_flNoRotationalMotionBlurUntil = gpGlobals->realtime + 1.0f; // Wait a second until the portal craziness calms down
		}
		else
		{
			//====================//
			// Normal update path //
			//====================//
			// Compute horizontal and vertical fov
			float flHorizontalFov = view.fov;
			float flVerticalFov = ( view.m_flAspectRatio <= 0.0f ) ? ( view.fov ) : ( view.fov  / view.m_flAspectRatio );
			//engine->Con_NPrintf( 2, "Horizontal Fov: %6.2f   Vertical Fov: %6.2f", flHorizontalFov, flVerticalFov );

			//=====================//
			// Forward motion blur //
			//=====================//
			float flViewDotMotion = DotProduct( vCurrentForwardVec, vPositionChange );
			if ( mat_motion_blur_forward_enabled.GetBool() ) // Want forward and falling
				g_vMotionBlurValues[2] = flViewDotMotion;
			else // Falling only
				g_vMotionBlurValues[2] = flViewDotMotion * fabs( vCurrentForwardVec[2] ); // Only want this if we're looking up or down;

			//====================================//
			// Yaw (Compensate for circle strafe) //
			//====================================//
			float flSideDotMotion = DotProduct( vCurrentSideVec, vPositionChange );
			float flYawDiffOriginal = history.m_flPreviousYaw - flCurrentYaw;
			if ( ( ( history.m_flPreviousYaw - flCurrentYaw > 180.0f ) || ( history.m_flPreviousYaw - flCurrentYaw < -180.0f ) ) &&
				 ( ( history.m_flPreviousYaw + flCurrentYaw > -180.0f ) && ( history.m_flPreviousYaw + flCurrentYaw < 180.0f ) ) )
				flYawDiffOriginal = history.m_flPreviousYaw + flCurrentYaw;

			float flYawDiffAdjusted = flYawDiffOriginal + ( flSideDotMotion / 3.0f ); // Yes, 3.0 is a magic number, sue me

			// Make sure the adjustment only lessens the effect, not magnify it or reverse it
			if ( flYawDiffOriginal < 0.0f )
				flYawDiffAdjusted = clamp ( flYawDiffAdjusted, flYawDiffOriginal, 0.0f );
			else
				flYawDiffAdjusted = clamp ( flYawDiffAdjusted, 0.0f, flYawDiffOriginal );

			// Use pitch to dampen yaw
			float flUndampenedYaw = flYawDiffAdjusted / flHorizontalFov;
			g_vMotionBlurValues[0] = flUndampenedYaw * ( 1.0f - ( fabs( flCurrentPitch ) / 90.0f ) ); // Dampen horizontal yaw blur based on pitch

			//engine->Con_NPrintf( 4, "flSideDotMotion: %6.2f   yaw diff: %6.2f  ( %6.2f, %6.2f )", flSideDotMotion, ( s_flPreviousYaw - flCurrentYaw ), flYawDiffOriginal, flYawDiffAdjusted );

			//=======================================//
			// Pitch (Compensate for forward motion) //
			//=======================================//
			float flPitchCompensateMask = 1.0f - ( ( 1.0f - fabs( vCurrentForwardVec[2] ) ) * ( 1.0f - fabs( vCurrentForwardVec[2] ) ) );
			float flPitchDiffOriginal = history.m_flPreviousPitch - flCurrentPitch;
			float flPitchDiffAdjusted = flPitchDiffOriginal;

			if ( flCurrentPitch > 0.0f )
				flPitchDiffAdjusted = flPitchDiffOriginal - ( ( flViewDotMotion / 2.0f ) * flPitchCompensateMask ); // Yes, 2.0 is a magic number, sue me
			else
				flPitchDiffAdjusted = flPitchDiffOriginal + ( ( flViewDotMotion / 2.0f ) * flPitchCompensateMask ); // Yes, 2.0 is a magic number, sue me

			// Make sure the adjustment only lessens the effect, not magnify it or reverse it
			if ( flPitchDiffOriginal < 0.0f )
				flPitchDiffAdjusted = clamp ( flPitchDiffAdjusted, flPitchDiffOriginal, 0.0f );
			else
				flPitchDiffAdjusted = clamp ( flPitchDiffAdjusted, 0.0f, flPitchDiffOriginal );

			g_vMotionBlurValues[1] = flPitchDiffAdjusted / flVerticalFov;

			//engine->Con_NPrintf( 5, "flViewDotMotion %6.2f, flPitchCompensateMask %6.2f, flPitchDiffOriginal %6.2f, flPitchDiffAdjusted %6.2f, g_vMotionBlurValues[1] %6.2f", flViewDotMotion, flPitchCompensateMask, flPitchDiffOriginal, flPitchDiffAdjusted, g_vMotionBlurValues[1]);

			//========================================================//
			// Roll (Enabled when we're looking down and yaw changes) //
			//========================================================//
			g_vMotionBlurValues[3] = flUndampenedYaw; // Roll starts out as undampened yaw intensity and is then scaled by pitch
			g_vMotionBlurValues[3] *= ( fabs( flCurrentPitch ) / 90.0f ) * ( fabs( flCurrentPitch ) / 90.0f ) * ( fabs( flCurrentPitch ) / 90.0f ); // Dampen roll based on pitch^3

			//engine->Con_NPrintf( 4, "[2] before scale and bias: %6.2f", g_vMotionBlurValues[2] );
			//engine->Con_NPrintf( 5, "[3] before scale and bias: %6.2f", g_vMotionBlurValues[3] );

			//==============================================================//
			// Time-adjust falling effect until we can do something smarter //
			//==============================================================//
			if ( flTimeElapsed > 0.0f )
				g_vMotionBlurValues[2] /= flTimeElapsed * 30.0f; // 1/30th of a second?
			else
				g_vMotionBlurValues[2] = 0.0f;

			// Scale and bias values after time adjustment
			g_vMotionBlurValues[2] = clamp( ( fabs( g_vMotionBlurValues[2] ) - flMotionBlurFallingMin ) / ( flMotionBlurFallingMax - flMotionBlurFallingMin ), 0.0f, 1.0f ) * ( g_vMotionBlurValues[2] >= 0.0f ? 1.0f : -1.0f );
			g_vMotionBlurValues[2] /= 30.0f; // To counter-adjust for time adjustment above

			//=================//
			// Apply intensity //
			//=================//
			g_vMotionBlurValues[0] *= flMotionBlurRotationIntensity * flMotionBlurGlobalStrength;
			g_vMotionBlurValues[1] *= flMotionBlurRotationIntensity * flMotionBlurGlobalStrength;
			g_vMotionBlurValues[2] *= flMotionBlurFallingIntensity * flMotionBlurGlobalStrength;
			g_vMotionBlurValues[3] *= flMotionBlurRollIntensity * flMotionBlurGlobalStrength;

			//===============================================================//
			// Dampen motion blur from 100%-0% as fps drops from 50fps-30fps //
			//===============================================================//
			if ( !IsX360() && !bSFMBlur ) // I'm not doing this on the 360 yet since I can't test it.  SFM doesn't need it either
			{
				float flSlowFps = 30.0f;
				float flFastFps = 50.0f;
				float flCurrentFps = ( flTimeElapsed > 0.0f ) ? ( 1.0f / flTimeElapsed ) : 0.0f;
				float flDampenFactor = clamp( ( ( flCurrentFps - flSlowFps ) / ( flFastFps - flSlowFps ) ), 0.0f, 1.0f );

				//engine->Con_NPrintf( 4, "gpGlobals->realtime %.2f  gpGlobals->curtime %.2f", gpGlobals->realtime, gpGlobals->curtime );
				//engine->Con_NPrintf( 5, "flCurrentFps %.2f", flCurrentFps );
				//engine->Con_NPrintf( 7, "flTimeElapsed %.2f", flTimeElapsed );

				g_vMotionBlurValues[0] *= flDampenFactor;
				g_vMotionBlurValues[1] *= flDampenFactor;
				g_vMotionBlurValues[2] *= flDampenFactor;
				g_vMotionBlurValues[3] *= flDampenFactor;

				//engine->Con_NPrintf( 6, "Dampen: %.2f", flDampenFactor );
			}

			//engine->Con_NPrintf( 6, "Final values: { %6.2f%%, %6.2f%%, %6.2f%%, %6.2f%% }", g_vMotionBlurValues[0]*100.0f, g_vMotionBlurValues[1]*100.0f, g_vMotionBlurValues[2]*100.0f, g_vMotionBlurValues[3]*100.0f );
		}

		//============================================//
		// Zero out blur if still in that time window //
		//============================================//
		if ( !bSFMBlur && ( gpGlobals->realtime < history.m_flNoRotationalMotionBlurUntil ) )
		{
			//engine->Con_NPrintf( 9, " No Rotation @ %f ", gpGlobals->realtime );

			// Zero out rotational blur but leave forward/falling blur alone
			g_vMotionBlurValues[0] = 0.0f; // X
			g_vMotionBlurValues[1] = 0.0f; // Y
			g_vMotionBlurValues[3] = 0.0f; // Roll
		}
		else
		{
			history.m_flNoRotationalMotionBlurUntil = 0.0f;
		}

		//================================================================================//
		// Disable roll and forward blur if in split screen and reduce the blur intensity //
		//================================================================================//
		if ( engine->IsSplitScreenActive() )
		{
			g_vMotionBlurValues[0] *= 0.25f; // X
			g_vMotionBlurValues[1] *= 0.25f; // Y
			g_vMotionBlurValues[2] = 0.0f;
			g_vMotionBlurValues[3] = 0.0f;
		}

		//====================================//
		// Store current frame for next frame //
		//====================================//
		VectorCopy( vCurrentPosition, history.m_vPreviousPositon );
		history.m_mPreviousFrameBasisVectors = mCurrentBasisVectors;
		history.m_flPreviousPitch = flCurrentPitch;
		history.m_flPreviousYaw = flCurrentYaw;
		history.m_flLastTimeUpdate = gpGlobals->realtime;
	}

	//engine->Con_NPrintf( 6, "Final values: { %6.2f%%, %6.2f%%, %6.2f%%, %6.2f%% }", g_vMotionBlurValues[0]*100.0f, g_vMotionBlurValues[1]*100.0f, g_vMotionBlurValues[2]*100.0f, g_vMotionBlurValues[3]*100.0f );

	//==========================================//
	// Set global g_vMotionBlurViewportValues[] //
	//==========================================//
	if ( true )
	{
		ITexture *pSrc = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
		float flSrcWidth = ( float )pSrc->GetActualWidth();
		float flSrcHeight = ( float )pSrc->GetActualHeight();

		// NOTE #1: float4 stored as ( minx, miny, maxy, maxx )...z&w have been swapped to save pixel shader instructions
		// NOTE #2: This code should definitely work for 2 players (horizontal or vertical), or 4 players (4 corners), but
		//          it might have to be modified if we ever want to support other split screen configurations

		int nOffset; // Offset by one pixel to land in the correct half

		// Left
		nOffset = ( x > 0 ) ? 1 : 0;
		g_vMotionBlurViewportValues[0] = ( float )( x + nOffset ) / ( flSrcWidth - 1 );

		// Right
		nOffset = ( x < ( flSrcWidth - 1 ) ) ? -1 : 0;
		g_vMotionBlurViewportValues[3] = ( float )( x + w + nOffset ) / ( flSrcWidth - 1 );

		// Top
		nOffset = ( y > 0 ) ? 1 : 0; // Offset by one pixel to land in the correct half
		g_vMotionBlurViewportValues[1] = ( float )( y + nOffset ) / ( flSrcHeight - 1 );

		// Bottom
		nOffset = ( y < ( flSrcHeight - 1 ) ) ? -1 : 0;
		g_vMotionBlurViewportValues[2] = ( float )( y + h + nOffset ) / ( flSrcHeight - 1 );

		// Only allow clamping to happen in the middle of the screen, so nudge the clamp values out if they're on the border of the screen
		for ( int i = 0; i < 4; i++ )
		{
			if ( g_vMotionBlurViewportValues[i] <= 0.0f )
				g_vMotionBlurViewportValues[i] = -1.0f;
			else if ( g_vMotionBlurViewportValues[i] >= 1.0f )
				g_vMotionBlurViewportValues[i] = 2.0f;
		}
	}

	//=============================================================================================//
	// Render quad and let material proxy pick up the g_vMotionBlurValues[4] values just set above //
	//=============================================================================================//
	if ( true )
	{
		CMatRenderContextPtr pRenderContext( materials );
		//pRenderContext->PushRenderTargetAndViewport();
		ITexture *pSrc = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
		int nSrcWidth = pSrc->GetActualWidth();
		int nSrcHeight = pSrc->GetActualHeight();
		int nViewportWidth, nViewportHeight, nDummy;
		pRenderContext->GetViewport( nDummy, nDummy, nViewportWidth, nViewportHeight );

		UpdateScreenEffectTexture( 0, x, y, w, h, false );

		// Get material pointer
		IMaterial *pMatMotionBlur = materials->FindMaterial( "dev/motion_blur", TEXTURE_GROUP_OTHER, true );

		//SetRenderTargetAndViewPort( dest_rt0 );
		//pRenderContext->PopRenderTargetAndViewport();

		if ( pMatMotionBlur != NULL )
		{
			pRenderContext->DrawScreenSpaceRectangle(
				pMatMotionBlur,
				0, 0, nViewportWidth, nViewportHeight,
				x, y, x + w-1, y + h-1,
				nSrcWidth, nSrcHeight, GetClientWorldEntity()->GetClientRenderable() );
		}
	}
}

//=====================================================================================================================
// Depth of field =====================================================================================================
//=====================================================================================================================
ConVar mat_dof_enabled( "mat_dof_enabled", "1" );
ConVar mat_dof_override( "mat_dof_override", "0" );
ConVar mat_dof_near_blur_depth( "mat_dof_near_blur_depth", "20.0" );
ConVar mat_dof_near_focus_depth( "mat_dof_near_focus_depth", "100.0" );
ConVar mat_dof_far_focus_depth( "mat_dof_far_focus_depth", "250.0" );
ConVar mat_dof_far_blur_depth( "mat_dof_far_blur_depth", "1000.0" );
ConVar mat_dof_near_blur_radius( "mat_dof_near_blur_radius", "10.0" );
ConVar mat_dof_far_blur_radius( "mat_dof_far_blur_radius", "5.0" );
ConVar mat_dof_quality( "mat_dof_quality", "0" );

static float GetNearBlurDepth()
{
	return mat_dof_override.GetBool() ? mat_dof_near_blur_depth.GetFloat() : g_flDOFNearBlurDepth;
}

static float GetNearFocusDepth()
{
	return mat_dof_override.GetBool() ? mat_dof_near_focus_depth.GetFloat() : g_flDOFNearFocusDepth;
}

static float GetFarFocusDepth()
{
	return mat_dof_override.GetBool() ? mat_dof_far_focus_depth.GetFloat() : g_flDOFFarFocusDepth;
}

static float GetFarBlurDepth()
{
	return mat_dof_override.GetBool() ? mat_dof_far_blur_depth.GetFloat() : g_flDOFFarBlurDepth;
}

static float GetNearBlurRadius()
{
	return mat_dof_override.GetBool() ? mat_dof_near_blur_radius.GetFloat() : g_flDOFNearBlurRadius;
}

static float GetFarBlurRadius()
{
	return mat_dof_override.GetBool() ? mat_dof_far_blur_radius.GetFloat() : g_flDOFFarBlurRadius;
}

bool IsDepthOfFieldEnabled()
{
	const CViewSetup *pViewSetup = view->GetViewSetup();
	if ( !pViewSetup )
		return false;

	// We need high-precision depth, which we currently only get in float HDR mode
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_FLOAT )
		return false;

	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 92 )
		return false;

	// Only SFM sets this at the moment...it supersedes mat_dof_ convars if true
	if ( pViewSetup->m_bDoDepthOfField )
		return true;

	if ( !mat_dof_enabled.GetBool() )
		return false;

	if ( mat_dof_override.GetBool() == true )
	{
		return mat_dof_enabled.GetBool();
	}
	else
	{
		return g_bDOFEnabled;
	}
}

static inline bool SetMaterialVarFloat( IMaterial* pMat, const char* pVarName, float flValue )
{
	Assert( pMat != NULL );
	Assert( pVarName != NULL );
	if ( pMat == NULL || pVarName == NULL )
	{
		return false;
	}

	bool bFound = false;
	IMaterialVar* pVar = pMat->FindVar( pVarName, &bFound );
	if ( bFound )
	{
		pVar->SetFloatValue( flValue );
	}

	return bFound;
}

static inline bool SetMaterialVarInt( IMaterial* pMat, const char* pVarName, int nValue )
{
	Assert( pMat != NULL );
	Assert( pVarName != NULL );
	if ( pMat == NULL || pVarName == NULL )
	{
		return false;
	}

	bool bFound = false;
	IMaterialVar* pVar = pMat->FindVar( pVarName, &bFound );
	if ( bFound )
	{
		pVar->SetIntValue( nValue );
	}
	
	return bFound;
}

void DoDepthOfField( const CViewSetup &view )
{
	if ( !IsDepthOfFieldEnabled() )
	{
		return;
	}

	// Copy from backbuffer to _rt_FullFrameFB
	UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height, false ); // Do we need to check if we already did this?

	CMatRenderContextPtr pRenderContext( materials );

	ITexture *pSrc = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
	int nSrcWidth = pSrc->GetActualWidth();
	int nSrcHeight = pSrc->GetActualHeight();

	if ( mat_dof_quality.GetInt() < 2 )
	{
		/////////////////////////////////////
		// Downsample backbuffer to 1/4 size
		/////////////////////////////////////

		// Update downsampled framebuffer. TODO: Don't do this again for the bloom if we already did it here...
		pRenderContext->PushRenderTargetAndViewport();
		ITexture *dest_rt0 = materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET );

		// *Everything* in here relies on the small RTs being exactly 1/4 the full FB res
		Assert( dest_rt0->GetActualWidth()  == pSrc->GetActualWidth()  / 4 );
		Assert( dest_rt0->GetActualHeight() == pSrc->GetActualHeight() / 4 );

		// Downsample fb to rt0
		DownsampleFBQuarterSize( pRenderContext, nSrcWidth, nSrcHeight, dest_rt0, true );
		
		//////////////////////////////////////
		// Additional blur using 3x3 gaussian
		//////////////////////////////////////

		IMaterial *pMat = materials->FindMaterial( "dev/blurgaussian_3x3", TEXTURE_GROUP_OTHER, true );

		if ( pMat == NULL )
			return;

		SetMaterialVarFloat( pMat, "$c0_x", 0.5f / (float)dest_rt0->GetActualWidth() );
		SetMaterialVarFloat( pMat, "$c0_y", 0.5f / (float)dest_rt0->GetActualHeight() );
		SetMaterialVarFloat( pMat, "$c1_x", -0.5f / (float)dest_rt0->GetActualWidth() );
		SetMaterialVarFloat( pMat, "$c1_y", 0.5f / (float)dest_rt0->GetActualHeight() );

		ITexture *dest_rt1 = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );
		SetRenderTargetAndViewPort( dest_rt1 );

		pRenderContext->DrawScreenSpaceRectangle(
			pMat, 0, 0, nSrcWidth/4, nSrcHeight/4,
			0, 0, dest_rt0->GetActualWidth()-1, dest_rt0->GetActualHeight()-1,
			dest_rt0->GetActualWidth(), dest_rt0->GetActualHeight() );

		if ( IsX360() )
		{
			pRenderContext->CopyRenderTargetToTextureEx( dest_rt1, 0, NULL, NULL );
		}

		pRenderContext->PopRenderTargetAndViewport();
	}

	// Render depth-of-field quad 

	int nViewportWidth = 0;
	int nViewportHeight = 0;
	int nDummy = 0;
	pRenderContext->GetViewport( nDummy, nDummy, nViewportWidth, nViewportHeight );

	IMaterial *pMatDOF = materials->FindMaterial( "dev/depth_of_field", TEXTURE_GROUP_OTHER, true );

	if ( pMatDOF == NULL )
		return;

	SetMaterialVarFloat( pMatDOF, "$nearPlane", view.zNear );
	SetMaterialVarFloat( pMatDOF, "$farPlane", view.zFar );

	// Only SFM drives this bool at the moment...
	if ( view.m_bDoDepthOfField )
	{
		SetMaterialVarFloat( pMatDOF, "$nearBlurDepth", view.m_flNearBlurDepth );
		SetMaterialVarFloat( pMatDOF, "$nearFocusDepth", view.m_flNearFocusDepth );
		SetMaterialVarFloat( pMatDOF, "$farFocusDepth", view.m_flFarFocusDepth );
		SetMaterialVarFloat( pMatDOF, "$farBlurDepth", view.m_flFarBlurDepth );
		SetMaterialVarFloat( pMatDOF, "$nearBlurRadius", view.m_flNearBlurRadius );
		SetMaterialVarFloat( pMatDOF, "$farBlurRadius", view.m_flFarBlurRadius );
		SetMaterialVarInt( pMatDOF, "$quality", view.m_nDoFQuality );
	}
	else // pull from convars/globals
	{
		SetMaterialVarFloat( pMatDOF, "$nearBlurDepth", GetNearBlurDepth() );
		SetMaterialVarFloat( pMatDOF, "$nearFocusDepth", GetNearFocusDepth() );
		SetMaterialVarFloat( pMatDOF, "$farFocusDepth", GetFarFocusDepth() );
		SetMaterialVarFloat( pMatDOF, "$farBlurDepth", GetFarBlurDepth() );
		SetMaterialVarFloat( pMatDOF, "$nearBlurRadius", GetNearBlurRadius() );
		SetMaterialVarFloat( pMatDOF, "$farBlurRadius", GetFarBlurRadius() );
		SetMaterialVarInt( pMatDOF, "$quality", mat_dof_quality.GetInt() );
	}

	pRenderContext->DrawScreenSpaceRectangle(
		pMatDOF,
		0, 0, nViewportWidth, nViewportHeight,
		0, 0, nSrcWidth-1, nSrcHeight-1,
		nSrcWidth, nSrcHeight, GetClientWorldEntity()->GetClientRenderable() );
}




void DrawModulationQuad( IMaterial *pMaterial, IMatRenderContext *pRenderContext, uint8 r, uint8 g, uint8 b, uint8 a, float fDepth )
{
	pRenderContext->EnableClipping( false );
	pRenderContext->Bind( pMaterial );

	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	int w, h;

	pRenderContext->GetRenderTargetDimensions( w, h );
	if ( ( w == 0 ) || ( h == 0 ) )
		return;

	// This is the size of the back-buffer we're reading from.
	int bw, bh;
	bw = w; bh = h;

	float s0, t0;
	float s1, t1;

	float flOffsetS = (bw != 0.0f) ? 1.0f / bw : 0.0f;
	float flOffsetT = (bh != 0.0f) ? 1.0f / bh : 0.0f;
	s0 = 0.5f * flOffsetS;
	t0 = 0.5f * flOffsetT;
	s1 = (w-0.5f) * flOffsetS;
	t1 = (h-0.5f) * flOffsetT;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( -1.0f, -1.0f, fDepth );
	//meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	//meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	//meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, s0, t1 );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -1.0f, 1, fDepth );
	//meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	//meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	//meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, s0, t0 );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 1, 1, fDepth );
	//meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	//meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	//meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, s1, t0 );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 1, -1.0f, fDepth );
	//meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	//meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	//meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, s1, t1 );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
	pRenderContext->EnableClipping( true );
}

ConVar cl_blurClearAlpha( "cl_blurClearAlpha", "0", 0, "0-255, but 0 has errors at the moment" );
ConVar cl_blurDebug( "cl_blurDebug", "0" );
ConVar cl_blurTapSize( "cl_blurTapSize", "0.5" );
ConVar cl_blurPasses( "cl_blurPasses", "1" );

void BlurEntity( IClientRenderable *pRenderable, bool bPreDraw, int drawFlags, const RenderableInstance_t &instance, const CViewSetup &view, int x, int y, int w, int h )
{
	ITexture *pFullFrameFB = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
	ITexture *dest_rt[2];
	dest_rt[0] = materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET );
	dest_rt[1] = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );

	IMaterial *pBlurPass[2];
	pBlurPass[0] = materials->FindMaterial( "dev/blurentity_blurpass0", TEXTURE_GROUP_OTHER );
	pBlurPass[1] = materials->FindMaterial( "dev/blurentity_blurpass1", TEXTURE_GROUP_OTHER );
	IMaterial *pEntBlurCopyBack[2];
	pEntBlurCopyBack[0] = materials->FindMaterial( "dev/blurentity_copyback0", TEXTURE_GROUP_OTHER );
	pEntBlurCopyBack[1] = materials->FindMaterial( "dev/blurentity_copyback1", TEXTURE_GROUP_OTHER );
	IMaterial *pEntBlurAlphaSilhoutte = materials->FindMaterial( "dev/blurentity_alphasilhoutte", TEXTURE_GROUP_OTHER );

	if( !pFullFrameFB || 
		!dest_rt[0] || !dest_rt[1] || 
		!pBlurPass[0] || !pBlurPass[1] || 
		!pEntBlurCopyBack[0] || !pEntBlurCopyBack[1] || 
		!pEntBlurAlphaSilhoutte )
	{
		return; //missing a vital texture/material
	}

	// Copy from backbuffer to _rt_FullFrameFB
	UpdateScreenEffectTexture( 0, x, y, w, h, true ); // Do we need to check if we already did this?

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->PushRenderTargetAndViewport();

	
	int nSrcWidth = pFullFrameFB->GetActualWidth();
	int nSrcHeight = pFullFrameFB->GetActualHeight();

	pRenderContext->OverrideAlphaWriteEnable( true, true ); //ensure we're always copying alpha values in every shader since we're using alpha as a mask when drawing to the back buffer
	
	//replace the alpha channel with a silhoutte of the desired entity. We'll use the blurred alpha when rendering back to the back buffer
	{
		SetRenderTargetAndViewPort( pFullFrameFB );
		pRenderContext->ClearColor4ub( 255, 255, 255, cl_blurClearAlpha.GetInt() );
		pRenderContext->ClearBuffersObeyStencilEx( cl_blurDebug.GetBool(), true, true ); //clear out the existing alpha and depth

		if( bPreDraw ) //in pre-draw mode, this renderable hasn't drawn it's colors anywhere yet, add them to _rt_FullFrameFB
			pRenderable->DrawModel( drawFlags, instance );

		//just write 1.0 to alpha, don't alter color information
		if( !cl_blurDebug.GetBool() )
			pRenderContext->OverrideColorWriteEnable( true, false );
		modelrender->ForcedMaterialOverride( pEntBlurAlphaSilhoutte );
		
		pRenderable->DrawModel( drawFlags, instance );
		modelrender->ForcedMaterialOverride( NULL );
		if( !cl_blurDebug.GetBool() )
			pRenderContext->OverrideColorWriteEnable( false, false );
	}

	IMaterial *pEntBlurCopyBackFinal = NULL; //the material to use when copying the blur back to the backbuffer
	//generate blur texture
	{
		/////////////////////////////////////
		// Downsample backbuffer to 1/4 size
		/////////////////////////////////////	

		// *Everything* in here relies on the small RTs being exactly 1/4 the full FB res
		Assert( dest_rt[0]->GetActualWidth()  == pFullFrameFB->GetActualWidth()  / 4 );
		Assert( dest_rt[0]->GetActualHeight() == pFullFrameFB->GetActualHeight() / 4 );

		// Downsample fb to rt0
		DownsampleFBQuarterSize( pRenderContext, nSrcWidth, nSrcHeight, dest_rt[0], true );

		//////////////////////////////////////
		// Additional blur
		//////////////////////////////////////
		float flBlurTapSize = cl_blurTapSize.GetFloat();
		for( int i = 0; i != 2; ++i )
		{
			SetMaterialVarFloat( pBlurPass[i], "$c0_x", flBlurTapSize / (float)dest_rt[i]->GetActualWidth() );
			SetMaterialVarFloat( pBlurPass[i], "$c0_y", flBlurTapSize / (float)dest_rt[i]->GetActualHeight() );
		}

		int iBlurPasses = cl_blurPasses.GetInt();
	
		for( int i = 0; i < iBlurPasses; ++i )
		{
			int iSrc = i & 1;
			int iDest = 1 - iSrc;
			SetRenderTargetAndViewPort( dest_rt[iDest] );

			pRenderContext->DrawScreenSpaceRectangle(
				pBlurPass[iSrc], 0, 0, nSrcWidth/4, nSrcHeight/4,
				0, 0, dest_rt[iSrc]->GetActualWidth()-1, dest_rt[iSrc]->GetActualHeight()-1,
				dest_rt[iSrc]->GetActualWidth(), dest_rt[iSrc]->GetActualHeight() );

			if ( IsX360() )
			{
				pRenderContext->CopyRenderTargetToTextureEx( dest_rt[iDest], 0, NULL, NULL );
			}
		}
		pEntBlurCopyBackFinal = pEntBlurCopyBack[iBlurPasses & 1];
	}
	pRenderContext->OverrideAlphaWriteEnable( false, true );

	pRenderContext->PopRenderTargetAndViewport();

	//render back to the screen. We use the depth of the closest bbox point as our quad depth
	{
		const Vector &vRenderOrigin = pRenderable->GetRenderOrigin();
		const QAngle &qRenderAngles = pRenderable->GetRenderAngles();
		Vector vMins, vMaxs;
		pRenderable->GetRenderBounds( vMins, vMaxs );

		VMatrix matWorld, matView, matProj, matWorldView, matWorldViewProj;
		//since the model matrix isn't necessarily set for this renderable, construct it manually
		matWorld.SetupMatrixOrgAngles( vRenderOrigin, qRenderAngles );
		pRenderContext->GetMatrix( MATERIAL_VIEW, &matView );
		pRenderContext->GetMatrix( MATERIAL_PROJECTION, &matProj );
		MatrixMultiply( matView, matWorld, matWorldView );
		MatrixMultiply( matProj, matWorldView, matWorldViewProj );

		float fClosestBBoxDepth = 1.0f;
		Vector4D vTest;
		vTest.w = 1.0f;
		for( int i = 0; i != 8; ++i )
		{
			vTest.x = (i & (1 << 0)) ? vMaxs.x : vMins.x;
			vTest.y = (i & (1 << 1)) ? vMaxs.y : vMins.y;
			vTest.z = (i & (1 << 2)) ? vMaxs.z : vMins.z;
			Vector4D vOut;
			matWorldViewProj.V4Mul( vTest, vOut );
			float fDepth = vOut.z/vOut.w;
			if( fDepth < fClosestBBoxDepth )
				fClosestBBoxDepth = fDepth;
		}

		if( fClosestBBoxDepth < 0.0f )
			fClosestBBoxDepth = 0.0f;
		
		DrawModulationQuad( pEntBlurCopyBackFinal, pRenderContext, 255, 255, 255, 255, fClosestBBoxDepth );
	}

	if( bPreDraw && ( instance.m_nAlpha == 255 ) && ( ( drawFlags & STUDIO_TRANSPARENCY ) == 0 ) ) //write depth out to the depth buffer
	{
		modelrender->ForcedMaterialOverride( NULL, OVERRIDE_DEPTH_WRITE );
		pRenderable->DrawModel( drawFlags, instance );
		modelrender->ForcedMaterialOverride( NULL );
	}
}
