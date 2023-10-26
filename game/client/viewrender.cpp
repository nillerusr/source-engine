//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Responsible for drawing the scene
//
//===========================================================================//

#include "cbase.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "ivieweffects.h"
#include "iinput.h"
#include "model_types.h"
#include "clientsideeffects.h"
#include "particlemgr.h"
#include "viewrender.h"
#include "iclientmode.h"
#include "voice_status.h"
#include "glow_overlay.h"
#include "materialsystem/imesh.h"
#include "materialsystem/ITexture.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include "materialsystem/imaterialsystem.h"
#include "DetailObjectSystem.h"
#include "tier0/vprof.h"
#include "tier1/mempool.h"
#include "vstdlib/jobthread.h"
#include "datacache/imdlcache.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "tier0/icommandline.h"
#include "view_scene.h"
#include "particles_ez.h"
#include "engine/IStaticPropMgr.h"
#include "engine/ivdebugoverlay.h"
#include "c_pixel_visibility.h"
#include "precache_register.h"
#include "c_rope.h"
#include "c_effects.h"
#include "smoke_fog_overlay.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "ScreenSpaceEffects.h"
#include "toolframework_client.h"
#include "c_func_reflective_glass.h"
#include "keyvalues.h"
#include "renderparm.h"
#include "modelrendersystem.h"
#include "vgui/ISurface.h"

#define PARTICLE_USAGE_DEMO									// uncomment to get particle bar thing




#if defined( HL2_CLIENT_DLL ) || defined( INFESTED_DLL )
#define USE_MONITORS
#endif
#include "rendertexture.h"
#include "viewpostprocess.h"
#include "viewdebug.h"

#ifdef USE_MONITORS
#include "c_point_camera.h"
#endif // USE_MONITORS

#ifdef INFESTED_DLL
#include "c_asw_render_targets.h"
#include "clientmode_asw.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static void testfreezeframe_f( void )
{
	view->FreezeFrame( 3.0 );
}
static ConCommand test_freezeframe( "test_freezeframe", testfreezeframe_f, "Test the freeze frame code.", FCVAR_CHEAT );

//-----------------------------------------------------------------------------

static ConVar r_visocclusion( "r_visocclusion", "0", FCVAR_CHEAT );
extern ConVar r_flashlightdepthtexture;
extern ConVar vcollide_wireframe;
extern ConVar mat_motion_blur_enabled;
extern ConVar r_depthoverlay;
extern ConVar r_shadow_deferred;

//-----------------------------------------------------------------------------
// Convars related to controlling rendering
//-----------------------------------------------------------------------------
static ConVar cl_maxrenderable_dist("cl_maxrenderable_dist", "3000", FCVAR_CHEAT, "Max distance from the camera at which things will be rendered" );

ConVar r_entityclips( "r_entityclips", "1" ); //FIXME: Nvidia drivers before 81.94 on cards that support user clip planes will have problems with this, require driver update? Detect and disable?

// Matches the version in the engine
static ConVar r_drawopaqueworld( "r_drawopaqueworld", "1", FCVAR_CHEAT );
static ConVar r_drawtranslucentworld( "r_drawtranslucentworld", "1", FCVAR_CHEAT );
static ConVar r_3dsky( "r_3dsky","1", 0, "Enable the rendering of 3d sky boxes" );
static ConVar r_skybox( "r_skybox","1", FCVAR_CHEAT, "Enable the rendering of sky boxes" );
ConVar r_drawviewmodel( "r_drawviewmodel","1", FCVAR_CHEAT );
static ConVar r_drawtranslucentrenderables( "r_drawtranslucentrenderables", "1", FCVAR_CHEAT );
static ConVar r_drawopaquerenderables( "r_drawopaquerenderables", "1", FCVAR_CHEAT );

static ConVar r_flashlightdepth_drawtranslucents( "r_flashlightdepth_drawtranslucents", "0", FCVAR_NONE );

ConVar r_flashlightvolumetrics( "r_flashlightvolumetrics", "1" );


// FIXME: This is not static because we needed to turn it off for TF2 playtests
ConVar r_DrawDetailProps( "r_DrawDetailProps", "1", FCVAR_NONE, "0=Off, 1=Normal, 2=Wireframe" );

ConVar r_worldlistcache( "r_worldlistcache", "1" );

ConVar mat_ambient_light_r_forced( "mat_ambient_light_r_forced", "-1.0" );
ConVar mat_ambient_light_g_forced( "mat_ambient_light_g_forced", "-1.0" );
ConVar mat_ambient_light_b_forced( "mat_ambient_light_b_forced", "-1.0" );

//-----------------------------------------------------------------------------
// Convars related to fog color
//-----------------------------------------------------------------------------
static void GetFogColor( fogparams_t *pFogParams, float *pColor, bool ignoreOverride = false, bool ignoreHDRColorScale = false );
static float GetFogMaxDensity( fogparams_t *pFogParams, bool ignoreOverride = false );
static bool GetFogEnable( fogparams_t *pFogParams, bool ignoreOverride = false );
static float GetFogStart( fogparams_t *pFogParams, bool ignoreOverride = false );
static float GetFogEnd( fogparams_t *pFogParams, bool ignoreOverride = false );
static float GetSkyboxFogStart( bool ignoreOverride = false );
static float GetSkyboxFogEnd( bool ignoreOverride = false );
static float GetSkyboxFogMaxDensity( bool ignoreOverride = false );
static void GetSkyboxFogColor( float *pColor, bool ignoreOverride = false, bool ignoreHDRColorScale = false );
static void FogOverrideCallback( IConVar *pConVar, char const *pOldString, float flOldValue );
static ConVar fog_override( "fog_override", "0", FCVAR_CHEAT, "Overrides the map's fog settings (-1 populates fog_ vars with map's values)", FogOverrideCallback );
// set any of these to use the maps fog
static ConVar fog_start( "fog_start", "-1", FCVAR_CHEAT );
static ConVar fog_end( "fog_end", "-1", FCVAR_CHEAT );
static ConVar fog_color( "fog_color", "-1 -1 -1", FCVAR_CHEAT );
static ConVar fog_enable( "fog_enable", "1", FCVAR_CHEAT );
static ConVar fog_startskybox( "fog_startskybox", "-1", FCVAR_CHEAT );
static ConVar fog_endskybox( "fog_endskybox", "-1", FCVAR_CHEAT );
static ConVar fog_maxdensityskybox( "fog_maxdensityskybox", "-1", FCVAR_CHEAT );
static ConVar fog_colorskybox( "fog_colorskybox", "-1 -1 -1", FCVAR_CHEAT );
static ConVar fog_enableskybox( "fog_enableskybox", "1", FCVAR_CHEAT );
static ConVar fog_maxdensity( "fog_maxdensity", "-1", FCVAR_CHEAT );
static ConVar fog_hdrcolorscale( "fog_hdrcolorscale", "-1", FCVAR_CHEAT );
static ConVar fog_hdrcolorscaleskybox( "fog_hdrcolorscaleskybox", "-1", FCVAR_CHEAT );
static void FogOverrideCallback( IConVar *pConVar, char const *pOldString, float flOldValue )
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !localPlayer )
		return;

	ConVarRef var( pConVar );
	if ( var.GetInt() == -1 )
	{
		fogparams_t *pFogParams = localPlayer->GetFogParams();

		float fogColor[3];
		fog_start.SetValue( GetFogStart( pFogParams, true ) );
		fog_end.SetValue( GetFogEnd( pFogParams, true ) );
		GetFogColor( pFogParams, fogColor, true, true );
		fog_color.SetValue( VarArgs( "%.1f %.1f %.1f", fogColor[0]*255, fogColor[1]*255, fogColor[2]*255 ) );
		fog_enable.SetValue( GetFogEnable( pFogParams, true ) );
		fog_startskybox.SetValue( GetSkyboxFogStart( true ) );
		fog_endskybox.SetValue( GetSkyboxFogEnd( true ) );
		fog_maxdensityskybox.SetValue( GetSkyboxFogMaxDensity( true ) );
		GetSkyboxFogColor( fogColor, true, true );
		fog_colorskybox.SetValue( VarArgs( "%.1f %.1f %.1f", fogColor[0]*255, fogColor[1]*255, fogColor[2]*255 ) );
		fog_enableskybox.SetValue( localPlayer->m_Local.m_skybox3d.fog.enable.Get() );
		fog_maxdensity.SetValue( GetFogMaxDensity( pFogParams, true ) );
		fog_hdrcolorscale.SetValue( pFogParams->HDRColorScale );
		fog_hdrcolorscaleskybox.SetValue( localPlayer->m_Local.m_skybox3d.fog.HDRColorScale.Get() );
	}
}

//-----------------------------------------------------------------------------
// Water-related convars
//-----------------------------------------------------------------------------
static ConVar r_debugcheapwater( "r_debugcheapwater", "0", FCVAR_CHEAT );
#ifndef _X360
static ConVar r_waterforceexpensive( "r_waterforceexpensive", "0" );
#endif
static ConVar r_waterforcereflectentities( "r_waterforcereflectentities", "0" );
static ConVar r_WaterDrawRefraction( "r_WaterDrawRefraction", "1", 0, "Enable water refraction" );
static ConVar r_WaterDrawReflection( "r_WaterDrawReflection", "1", 0, "Enable water reflection" );
static ConVar r_ForceWaterLeaf( "r_ForceWaterLeaf", "1", 0, "Enable for optimization to water - considers view in leaf under water for purposes of culling" );
static ConVar mat_drawwater( "mat_drawwater", "1", FCVAR_CHEAT );
static ConVar mat_clipz( "mat_clipz", "1" );


//-----------------------------------------------------------------------------
// Other convars
//-----------------------------------------------------------------------------
static ConVar cl_drawmonitors( "cl_drawmonitors", "1" );
static ConVar r_eyewaterepsilon( "r_eyewaterepsilon", "7.0f", FCVAR_CHEAT );

extern ConVar cl_leveloverview;

ConVar r_fastzreject( "r_fastzreject", "0", 0, "Activate/deactivates a fast z-setting algorithm to take advantage of hardware with fast z reject. Use -1 to default to hardware settings" );

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static Vector g_vecCurrentRenderOrigin(0,0,0);
static QAngle g_vecCurrentRenderAngles(0,0,0);
static Vector g_vecCurrentVForward(0,0,0), g_vecCurrentVRight(0,0,0), g_vecCurrentVUp(0,0,0);
static VMatrix g_matCurrentCamInverse;
bool s_bCanAccessCurrentView = false;
IntroData_t *g_pIntroData = NULL;
static bool	g_bRenderingView = false;			// For debugging...
static int g_CurrentViewID = VIEW_NONE;
bool g_bRenderingScreenshot = false;

static FrustumCache_t s_FrustumCache;
FrustumCache_t *FrustumCache( void )
{
	return &s_FrustumCache;
}


#define FREEZECAM_SNAPSHOT_FADE_SPEED 340
float g_flFreezeFlash[ MAX_SPLITSCREEN_PLAYERS ];

//-----------------------------------------------------------------------------

CON_COMMAND( r_cheapwaterstart,  "" )
{
	if( args.ArgC() == 2 )
	{
		float dist = atof( args[ 1 ] );
		view->SetCheapWaterStartDistance( dist );
	}
	else
	{
		float start, end;
		view->GetWaterLODParams( start, end );
		Warning( "r_cheapwaterstart: %f\n", start );
	}
}

CON_COMMAND( r_cheapwaterend,  "" )
{
	if( args.ArgC() == 2 )
	{
		float dist = atof( args[ 1 ] );
		view->SetCheapWaterEndDistance( dist );
	}
	else
	{
		float start, end;
		view->GetWaterLODParams( start, end );
		Warning( "r_cheapwaterend: %f\n", end );
	}
}






//-----------------------------------------------------------------------------
// Describes a pruned set of leaves to be rendered this view. Reference counted
// because potentially shared by a number of views
//-----------------------------------------------------------------------------
struct ClientWorldListInfo_t : public CRefCounted1<WorldListInfo_t>
{
	ClientWorldListInfo_t() 
	{ 
		memset( (WorldListInfo_t *)this, 0, sizeof(WorldListInfo_t) ); 
		m_pOriginalLeafIndex = NULL;
		m_bPooledAlloc = false;
	}

	// Allocate a list intended for pruning
	static ClientWorldListInfo_t *AllocPooled( const ClientWorldListInfo_t &exemplar );

	// Because we remap leaves to eliminate unused leaves, we need a remap
	// when drawing translucent surfaces, which requires the *original* leaf index
	// using m_pOriginalLeafIndex[ remapped leaf index ] == actual leaf index
	uint16 *m_pOriginalLeafIndex;

private:
	virtual bool OnFinalRelease();

	bool m_bPooledAlloc;
	static CObjectPool<ClientWorldListInfo_t> gm_Pool;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class CWorldListCache
{
public:
	CWorldListCache()
	{

	}
	void Flush()
	{
		for ( int i = m_Entries.FirstInorder(); i != m_Entries.InvalidIndex(); i = m_Entries.NextInorder( i ) )
		{
			delete m_Entries[i];
		}
		m_Entries.RemoveAll();
	}

	bool Find( const CViewSetup &viewSetup, IWorldRenderList **ppList, ClientWorldListInfo_t **ppListInfo )
	{
		Entry_t lookup( viewSetup );

		int i = m_Entries.Find( &lookup );

		if ( i != m_Entries.InvalidIndex() )
		{
			Entry_t *pEntry = m_Entries[i];
			*ppList = InlineAddRef( pEntry->pList );
			*ppListInfo = InlineAddRef( pEntry->pListInfo );
			return true;
		}

		return false;
	}

	void Add( const CViewSetup &viewSetup, IWorldRenderList *pList, ClientWorldListInfo_t *pListInfo )
	{
		m_Entries.Insert( new Entry_t( viewSetup, pList, pListInfo ) );
	}

private:
	struct Entry_t
	{
		Entry_t( const CViewSetup &viewSetup, IWorldRenderList *pList = NULL, ClientWorldListInfo_t *pListInfo = NULL ) :
			pList( ( pList ) ? InlineAddRef( pList ) : NULL ),
			pListInfo( ( pListInfo ) ? InlineAddRef( pListInfo ) : NULL )
		{
            // @NOTE (toml 8/18/2006): because doing memcmp, need to fill all of the fields and the padding!
			memset( &m_bOrtho, 0, offsetof(Entry_t, pList ) - offsetof(Entry_t, m_bOrtho ) );
			m_bOrtho = viewSetup.m_bOrtho;			
			m_OrthoLeft = viewSetup.m_OrthoLeft;		
			m_OrthoTop = viewSetup.m_OrthoTop;
			m_OrthoRight = viewSetup.m_OrthoRight;
			m_OrthoBottom = viewSetup.m_OrthoBottom;
			fov = viewSetup.fov;				
			origin = viewSetup.origin;					
			angles = viewSetup.angles;				
			zNear = viewSetup.zNear;			
			zFar = viewSetup.zFar;			
			m_flAspectRatio = viewSetup.m_flAspectRatio;
			m_bOffCenter = viewSetup.m_bOffCenter;
			m_flOffCenterTop = viewSetup.m_flOffCenterTop;
			m_flOffCenterBottom = viewSetup.m_flOffCenterBottom;
			m_flOffCenterLeft = viewSetup.m_flOffCenterLeft;
			m_flOffCenterRight = viewSetup.m_flOffCenterRight;
		}

		~Entry_t()
		{
			if ( pList )
				pList->Release();
			if ( pListInfo )
				pListInfo->Release();
		}

		// The fields from CViewSetup that would actually affect the list
		float	m_OrthoLeft;		
		float	m_OrthoTop;
		float	m_OrthoRight;
		float	m_OrthoBottom;
		float	fov;				
		Vector	origin;					
		QAngle	angles;				
		float	zNear;			
		float	zFar;			
		float	m_flAspectRatio;
		float	m_flOffCenterTop;
		float	m_flOffCenterBottom;
		float	m_flOffCenterLeft;
		float	m_flOffCenterRight;
		bool	m_bOrtho;			
		bool	m_bOffCenter;

		IWorldRenderList *pList;
		ClientWorldListInfo_t *pListInfo;
	};

	class CEntryComparator
	{
	public:
		CEntryComparator( int ) {}
		bool operator!() const { return false; }
		bool operator()( const Entry_t *lhs, const Entry_t *rhs ) const 
		{ 
			return ( memcmp( lhs, rhs, sizeof(Entry_t) - ( sizeof(Entry_t) - offsetof(Entry_t, pList ) ) ) < 0 );
		}
	};

	CUtlRBTree<Entry_t *, unsigned short, CEntryComparator> m_Entries;
};

CWorldListCache g_WorldListCache;

//-----------------------------------------------------------------------------
// Standard 3d skybox view
//-----------------------------------------------------------------------------
class CSkyboxView : public CRendering3dView
{
	DECLARE_CLASS( CSkyboxView, CRendering3dView );
public:
	CSkyboxView(CViewRender *pMainView) : 
		CRendering3dView( pMainView ),
		m_pSky3dParams( NULL )
	  {
	  }

	bool			Setup( const CViewSetup &view, int *pClearFlags, SkyboxVisibility_t *pSkyboxVisible );
	void			Draw();

protected:



	virtual SkyboxVisibility_t	ComputeSkyboxVisibility();

	bool			GetSkyboxFogEnable();

	void			Enable3dSkyboxFog( void );
	void			DrawInternal( view_id_t iSkyBoxViewID = VIEW_3DSKY, bool bInvokePreAndPostRender = true, ITexture *pRenderTarget = NULL );

	sky3dparams_t *	PreRender3dSkyboxWorld( SkyboxVisibility_t nSkyboxVisible );

	sky3dparams_t *m_pSky3dParams;
};




//-----------------------------------------------------------------------------
// Shadow depth texture
//-----------------------------------------------------------------------------
class CShadowDepthView : public CRendering3dView
{
	DECLARE_CLASS( CShadowDepthView, CRendering3dView );
public:
	CShadowDepthView(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	void Setup( const CViewSetup &shadowViewIn, ITexture *pRenderTarget, ITexture *pDepthTexture );
	void Draw();

private:
	ITexture *m_pRenderTarget;
	ITexture *m_pDepthTexture;
};

//-----------------------------------------------------------------------------
// Freeze frame. Redraws the frame at which it was enabled.
//-----------------------------------------------------------------------------
class CFreezeFrameView : public CRendering3dView
{
	DECLARE_CLASS( CFreezeFrameView, CRendering3dView );
public:
	CFreezeFrameView(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	void Setup( const CViewSetup &view );
	void Draw();

private:
	CMaterialReference m_pFreezeFrame;
	CMaterialReference m_TranslucentSingleColor;

	int					m_nSubRect[ 4 ];
	int					m_nScreenSize[ 2 ];
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CBaseWorldView : public CRendering3dView
{
	DECLARE_CLASS( CBaseWorldView, CRendering3dView );
protected:
	CBaseWorldView(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	virtual bool	AdjustView( float waterHeight );

	void			DrawSetup( float waterHeight, int flags, float waterZAdjust, int iForceViewLeaf = -1 );
	void			DrawExecute( float waterHeight, view_id_t viewID, float waterZAdjust );

	virtual void	PushView( float waterHeight );
	virtual void	PopView();

};


//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
class CSimpleWorldView : public CBaseWorldView
{
	DECLARE_CLASS( CSimpleWorldView, CBaseWorldView );
public:
	CSimpleWorldView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

	void			Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& info, ViewCustomVisibility_t *pCustomVisibility = NULL );
	void			Draw();

private: 
	VisibleFogVolumeInfo_t m_fogInfo;

};


//-----------------------------------------------------------------------------
// Base class for scenes with water
//-----------------------------------------------------------------------------
class CBaseWaterView : public CBaseWorldView
{
	DECLARE_CLASS( CBaseWaterView, CBaseWorldView );
public:
	CBaseWaterView(CViewRender *pMainView) : 
		CBaseWorldView( pMainView ),
		m_SoftwareIntersectionView( pMainView )
	{}

	//	void Setup( const CViewSetup &, const WaterRenderInfo_t& info );

protected:
	void			CalcWaterEyeAdjustments( const VisibleFogVolumeInfo_t &fogInfo, float &newWaterHeight, float &waterZAdjust, bool bSoftwareUserClipPlane );

	class CSoftwareIntersectionView : public CBaseWorldView
	{
		DECLARE_CLASS( CSoftwareIntersectionView, CBaseWorldView );
	public:
		CSoftwareIntersectionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup( bool bAboveWater );
		void Draw();

	private: 
		CBaseWaterView *GetOuter() { return GET_OUTER( CBaseWaterView, m_SoftwareIntersectionView ); }
	};

	friend class CSoftwareIntersectionView;

	CSoftwareIntersectionView m_SoftwareIntersectionView;

	WaterRenderInfo_t m_waterInfo;
	float m_waterHeight;
	float m_waterZAdjust;
	bool m_bSoftwareUserClipPlane;
	VisibleFogVolumeInfo_t m_fogInfo;
};


//-----------------------------------------------------------------------------
// Scenes above water
//-----------------------------------------------------------------------------
class CAboveWaterView : public CBaseWaterView
{
	DECLARE_CLASS( CAboveWaterView, CBaseWaterView );
public:
	CAboveWaterView(CViewRender *pMainView) : 
		CBaseWaterView( pMainView ),
		m_ReflectionView( pMainView ),
		m_RefractionView( pMainView ),
		m_IntersectionView( pMainView )
	{}

	void Setup(  const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo );
	void			Draw();

	class CReflectionView : public CBaseWorldView
	{
		DECLARE_CLASS( CReflectionView, CBaseWorldView );
	public:
		CReflectionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup( bool bReflectEntities );
		void Draw();

	private:
		CAboveWaterView *GetOuter() { return GET_OUTER( CAboveWaterView, m_ReflectionView ); }
	};

	class CRefractionView : public CBaseWorldView
	{
		DECLARE_CLASS( CRefractionView, CBaseWorldView );
	public:
		CRefractionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CAboveWaterView *GetOuter() { return GET_OUTER( CAboveWaterView, m_RefractionView ); }
	};

	class CIntersectionView : public CBaseWorldView
	{
		DECLARE_CLASS( CIntersectionView, CBaseWorldView );
	public:
		CIntersectionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CAboveWaterView *GetOuter() { return GET_OUTER( CAboveWaterView, m_IntersectionView ); }
	};


	friend class CRefractionView;
	friend class CReflectionView;
	friend class CIntersectionView;

	bool m_bViewIntersectsWater;

	CReflectionView m_ReflectionView;
	CRefractionView m_RefractionView;
	CIntersectionView m_IntersectionView;
};


//-----------------------------------------------------------------------------
// Scenes below water
//-----------------------------------------------------------------------------
class CUnderWaterView : public CBaseWaterView
{
	DECLARE_CLASS( CUnderWaterView, CBaseWaterView );
public:
	CUnderWaterView(CViewRender *pMainView) : 
		CBaseWaterView( pMainView ),
		m_RefractionView( pMainView )
	{}

	void			Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& info );
	void			Draw();

	class CRefractionView : public CBaseWorldView
	{
		DECLARE_CLASS( CRefractionView, CBaseWorldView );
	public:
		CRefractionView(CViewRender *pMainView) : CBaseWorldView( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CUnderWaterView *GetOuter() { return GET_OUTER( CUnderWaterView, m_RefractionView ); }
	};

	friend class CRefractionView;

	bool m_bDrawSkybox; // @MULTICORE (toml 8/17/2006): remove after setup hoisted

	CRefractionView m_RefractionView;
};


//-----------------------------------------------------------------------------
// Scenes containing reflective glass
//-----------------------------------------------------------------------------
class CReflectiveGlassView : public CSimpleWorldView
{
	DECLARE_CLASS( CReflectiveGlassView, CSimpleWorldView );
public:
	CReflectiveGlassView( CViewRender *pMainView ) : BaseClass( pMainView )
	{
	}

	virtual bool AdjustView( float flWaterHeight );
	virtual void PushView( float waterHeight );
	virtual void PopView( );
	void Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, const cplane_t &reflectionPlane );
	void Draw();

	cplane_t m_ReflectionPlane;
};

class CRefractiveGlassView : public CSimpleWorldView
{
	DECLARE_CLASS( CRefractiveGlassView, CSimpleWorldView );
public:
	CRefractiveGlassView( CViewRender *pMainView ) : BaseClass( pMainView )
	{
	}

	virtual bool AdjustView( float flWaterHeight );
	virtual void PushView( float waterHeight );
	virtual void PopView( );
	void Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, const cplane_t &reflectionPlane );
	void Draw();

	cplane_t m_ReflectionPlane;
};




//-----------------------------------------------------------------------------
// view of a single entity by itself
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// Computes draw flags for the engine to build its world surface lists
//-----------------------------------------------------------------------------
static inline unsigned long BuildEngineDrawWorldListFlags( unsigned nDrawFlags )
{
	unsigned long nEngineFlags = 0;

	if ( ( nDrawFlags & DF_SKIP_WORLD ) == 0 )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_WORLD_GEOMETRY;
	}

	if ( ( nDrawFlags & DF_SKIP_WORLD_DECALS_AND_OVERLAYS ) == 0 )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS;
	}

	if ( nDrawFlags & DF_DRAWSKYBOX )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_SKYBOX;
	}

	if ( nDrawFlags & DF_RENDER_ABOVEWATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_STRICTLYABOVEWATER;
		nEngineFlags |= DRAWWORLDLISTS_DRAW_INTERSECTSWATER;
	}

	if ( nDrawFlags & DF_RENDER_UNDERWATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_STRICTLYUNDERWATER;
		nEngineFlags |= DRAWWORLDLISTS_DRAW_INTERSECTSWATER;
	}

	if ( nDrawFlags & DF_RENDER_WATER )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_WATERSURFACE;
	}

	if( nDrawFlags & DF_CLIP_SKYBOX )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_CLIPSKYBOX;
	}

	if( nDrawFlags & DF_SHADOW_DEPTH_MAP )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_SHADOWDEPTH;
		nEngineFlags &= ~DRAWWORLDLISTS_DRAW_DECALS_AND_OVERLAYS;
	}

	if( nDrawFlags & DF_RENDER_REFRACTION )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_REFRACTION;
	}

	if( nDrawFlags & DF_RENDER_REFLECTION )
	{
		nEngineFlags |= DRAWWORLDLISTS_DRAW_REFLECTION;
	}

	return nEngineFlags;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void SetClearColorToFogColor()
{
	unsigned char ucFogColor[3];
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->GetFogColor( ucFogColor );
	if( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER )
	{
		// @MULTICORE (toml 8/16/2006): Find a way to not do this twice in eye above water case
		float scale = LinearToGammaFullRange( pRenderContext->GetToneMappingScaleLinear().x );
		ucFogColor[0] *= scale;
		ucFogColor[1] *= scale;
		ucFogColor[2] *= scale;
	}
	pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
}

//-----------------------------------------------------------------------------
// Precache of necessary materials
//-----------------------------------------------------------------------------

#ifdef HL2_CLIENT_DLL
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheViewRender )
	PRECACHE( MATERIAL, "scripted/intro_screenspaceeffect" )
PRECACHE_REGISTER_END()
#endif

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePostProcessingEffects )
	PRECACHE( MATERIAL, "dev/blurfiltery_and_add_nohdr" )
	PRECACHE( MATERIAL, "dev/blurfilterx" )
	PRECACHE( MATERIAL, "dev/blurfilterx_nohdr" )
	PRECACHE( MATERIAL, "dev/blurfiltery" )
	PRECACHE( MATERIAL, "dev/blurfiltery_nohdr" )
	PRECACHE( MATERIAL, "dev/blurfiltery_nohdr_clear" )
	PRECACHE( MATERIAL, "dev/bloomadd" )
	PRECACHE( MATERIAL, "dev/downsample" )
	PRECACHE( MATERIAL, "dev/downsample_non_hdr" )
	PRECACHE( MATERIAL, "dev/no_pixel_write" )
	PRECACHE( MATERIAL, "dev/lumcompare" )
	PRECACHE( MATERIAL, "dev/floattoscreen_combine" )
	PRECACHE( MATERIAL, "dev/copyfullframefb_vanilla" )
	PRECACHE( MATERIAL, "dev/copyfullframefb" )
	PRECACHE( MATERIAL, "dev/engine_post" )
	PRECACHE( MATERIAL, "dev/engine_post_splitscreen" )
	PRECACHE( MATERIAL, "dev/motion_blur" )
	PRECACHE( MATERIAL, "dev/depth_of_field" )
	PRECACHE( MATERIAL, "dev/blurgaussian_3x3" )
	PRECACHE( MATERIAL, "dev/fade_blur" )
#if defined( INFESTED_DLL )
	PRECACHE( MATERIAL, "dev/glow_color" )
	PRECACHE( MATERIAL, "dev/glow_downsample" )
	PRECACHE( MATERIAL, "dev/glow_blur_x" )
	PRECACHE( MATERIAL, "dev/glow_blur_y" )
	PRECACHE( MATERIAL, "dev/halo_add_to_screen" )
#endif // INFESTED_DLL
#if defined( INFESTED_DLL )
	PRECACHE( MATERIAL, "engine/writestencil" )
#endif // INFSETED_DLL
PRECACHE_REGISTER_END( )

//-----------------------------------------------------------------------------
// Accessors to return the current view being rendered
//-----------------------------------------------------------------------------
const Vector &CurrentViewOrigin()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentRenderOrigin;
}

const QAngle &CurrentViewAngles()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentRenderAngles;
}

const Vector &CurrentViewForward()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentVForward;
}

const Vector &CurrentViewRight()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentVRight;
}

const Vector &CurrentViewUp()
{
	Assert( s_bCanAccessCurrentView );
	return g_vecCurrentVUp;
}

const VMatrix &CurrentWorldToViewMatrix()
{
	Assert( s_bCanAccessCurrentView );
	return g_matCurrentCamInverse;
}


//-----------------------------------------------------------------------------
// Methods to set the current view/guard access to view parameters
//-----------------------------------------------------------------------------
void AllowCurrentViewAccess( bool allow )
{
	s_bCanAccessCurrentView = allow;
}

bool IsCurrentViewAccessAllowed()
{
	return s_bCanAccessCurrentView;
}

static ConVar mat_lpreview_mode( "mat_lpreview_mode", "-1", FCVAR_CHEAT );
void SetupCurrentView( const Vector &vecOrigin, const QAngle &angles, view_id_t viewID, bool bDrawWorldNormal = false, bool bCullFrontFaces = false )
{
	// Store off view origin and angles
	g_vecCurrentRenderOrigin = vecOrigin;
	g_vecCurrentRenderAngles = angles;

	// Compute the world->main camera transform
	ComputeCameraVariables( vecOrigin, angles, 
		&g_vecCurrentVForward, &g_vecCurrentVRight, &g_vecCurrentVUp, &g_matCurrentCamInverse );

	g_CurrentViewID = viewID;
	AllowCurrentViewAccess( true );

	// Cache off fade distances
	float flScreenFadeMinSize, flScreenFadeMaxSize;
	view->GetScreenFadeDistances( &flScreenFadeMinSize, &flScreenFadeMaxSize );
	modelinfo->SetViewScreenFadeRange( flScreenFadeMinSize, flScreenFadeMaxSize );

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA, ((viewID == VIEW_MAIN) || (viewID == VIEW_3DSKY)) ? 1 : 0 );


	if ( bDrawWorldNormal )
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, ENABLE_FIXED_LIGHTING_OUTPUTNORMAL_AND_DEPTH );

	if ( mat_lpreview_mode.GetInt() != -1 )
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, mat_lpreview_mode.GetInt() );

	if ( bCullFrontFaces )
	{
		pRenderContext->FlipCulling( true );
	}
}

view_id_t CurrentViewID()
{
	Assert( g_CurrentViewID != VIEW_ILLEGAL );
	return ( view_id_t )g_CurrentViewID;
}

//-----------------------------------------------------------------------------
// Purpose: Portal views are considered 'Main' views. This function tests a view id 
//			against all view ids used by portal renderables, as well as the main view.
//-----------------------------------------------------------------------------
bool IsMainView ( view_id_t id )
{

	return (id == VIEW_MAIN);

}

void FinishCurrentView()
{
	AllowCurrentViewAccess( false );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
void CSimpleRenderExecutor::AddView( CRendering3dView *pView )
{
 	CBase3dView *pPrevRenderer = m_pMainView->SetActiveRenderer( pView );
	pView->Draw();
	m_pMainView->SetActiveRenderer( pPrevRenderer );
}


#if !defined( INFESTED_DLL )
static CViewRender g_ViewRender;
IViewRender *GetViewRenderInstance()
{
	return &g_ViewRender;
}
#endif


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CViewRender::CViewRender()
	: m_SimpleExecutor( this )
{
	m_flCheapWaterStartDistance = 0.0f;
	m_flCheapWaterEndDistance = 0.1f;
	m_BaseDrawFlags = 0;
	m_pActiveRenderer = NULL;
	m_pCurrentlyDrawingEntity = NULL;
	m_bAllowViewAccess = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
inline bool CViewRender::ShouldDrawEntities( void )
{
	return ( !m_pDrawEntities || (m_pDrawEntities->GetInt() != 0) );
}


//-----------------------------------------------------------------------------
// Purpose: Check all conditions which would prevent drawing the view model
// Input  : drawViewmodel - 
//			*viewmodel - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CViewRender::ShouldDrawViewModel( bool bDrawViewmodel )
{
	if ( !bDrawViewmodel )
		return false;

	if ( !r_drawviewmodel.GetBool() )
		return false;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	if ( !C_BasePlayer::GetLocalPlayer() )
		return false;

	if ( C_BasePlayer::GetLocalPlayer()->ShouldDrawLocalPlayer() )
		return false;

	if ( !ShouldDrawEntities() )
		return false;

	if ( render->GetViewEntity() > gpGlobals->maxClients )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CViewRender::UpdateRefractIfNeededByList( CViewModelRenderablesList::RenderGroups_t &list )
{
	int nCount = list.Count();
	for( int i=0; i < nCount; ++i )
	{
		IClientRenderable *pRenderable = list[i].m_pRenderable;
		Assert( pRenderable );
		if ( pRenderable->GetRenderFlags() & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
		{
			UpdateRefractTexture();
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewRender::DrawRenderablesInList( CViewModelRenderablesList::RenderGroups_t &renderGroups, int flags )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#if defined( DBGFLAG_ASSERT )
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
#endif
	Assert( m_pCurrentlyDrawingEntity == NULL );
	int nCount = renderGroups.Count();
	for( int i=0; i < nCount; ++i )
	{
		IClientRenderable *pRenderable = renderGroups[i].m_pRenderable;
		Assert( pRenderable );

		// Non-view models wanting to render in view model list...
		if ( pRenderable->ShouldDraw() )
		{
			Assert( !IsSplitScreenSupported() || pRenderable->ShouldDrawForSplitScreenUser( nSlot ) );
			m_pCurrentlyDrawingEntity = pRenderable->GetIClientUnknown()->GetBaseEntity();
			pRenderable->DrawModel( STUDIO_RENDER | flags, renderGroups[i].m_InstanceData );
		}
	}
	m_pCurrentlyDrawingEntity = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Actually draw the view model
// Input  : drawViewModel - 
//-----------------------------------------------------------------------------
void CViewRender::DrawViewModels( const CViewSetup &view, bool drawViewmodel )
{
	VPROF( "CViewRender::DrawViewModel" );



	bool bShouldDrawPlayerViewModel = ShouldDrawViewModel( drawViewmodel );
	bool bShouldDrawToolViewModels = ToolsEnabled();

	if ( !bShouldDrawPlayerViewModel && !bShouldDrawToolViewModels )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	MDLCACHE_CRITICAL_SECTION();

	#if defined( _X360 )
		pRenderContext->PushVertexShaderGPRAllocation( 32 );
	#endif

	PIXEVENT( pRenderContext, "DrawViewModels()" );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();

	CViewSetup viewModelSetup( view );
	viewModelSetup.zNear = view.zNearViewmodel;
	viewModelSetup.zFar = view.zFarViewmodel;
	viewModelSetup.fov = view.fovViewmodel;
	viewModelSetup.m_flAspectRatio = engine->GetScreenAspectRatio( view.width, view.height );

	render->Push3DView( viewModelSetup, 0, NULL, GetFrustum() );


	const bool bUseDepthHack = true;


	// FIXME: Add code to read the current depth range
	float depthmin = 0.0f;
	float depthmax = 1.0f;

	// HACK HACK:  Munge the depth range to prevent view model from poking into walls, etc.
	// Force clipped down range
	if( bUseDepthHack )
		pRenderContext->DepthRange( 0.0f, 0.1f );
	
	CViewModelRenderablesList list;
	ClientLeafSystem()->CollateViewModelRenderables( &list );
	CViewModelRenderablesList::RenderGroups_t &opaqueList = list.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_OPAQUE ];
	CViewModelRenderablesList::RenderGroups_t &translucentList = list.m_RenderGroups[ CViewModelRenderablesList::VM_GROUP_TRANSLUCENT ];

	if ( ToolsEnabled() && ( !bShouldDrawPlayerViewModel || !bShouldDrawToolViewModels ) )
	{
		int nOpaque = opaqueList.Count();
		for ( int i = nOpaque-1; i >= 0; --i )
		{
			IClientRenderable *pRenderable = opaqueList[ i ].m_pRenderable;
			bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity() ? true : false;
			if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
			{
				opaqueList.FastRemove( i );
			}
		}

		int nTranslucent = translucentList.Count();
		for ( int i = nTranslucent-1; i >= 0; --i )
		{
			IClientRenderable *pRenderable = translucentList[ i ].m_pRenderable;
			bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity() ? true : false;
			if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
			{
				translucentList.FastRemove( i );
			}
		}
	}

	// Update refract for opaque models & draw
	bool bUpdatedRefractForOpaque = UpdateRefractIfNeededByList( opaqueList );
	DrawRenderablesInList( opaqueList );

	// Update refract for translucent models (if we didn't already update it above) & draw
	if ( !bUpdatedRefractForOpaque ) // Only do this once for better perf
	{
		UpdateRefractIfNeededByList( translucentList );
	}
	DrawRenderablesInList( translucentList, STUDIO_TRANSPARENCY );

	// Reset the depth range to the original values
	if( bUseDepthHack )
		pRenderContext->DepthRange( depthmin, depthmax );

	render->PopView( GetFrustum() );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	#if defined( _X360 )
		pRenderContext->PopVertexShaderGPRAllocation();
	#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CViewRender::ShouldDrawBrushModels( void )
{
	if ( m_pDrawBrushModels && !m_pDrawBrushModels->GetInt() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Performs screen space effects, if any
//-----------------------------------------------------------------------------
void CViewRender::PerformScreenSpaceEffects( int x, int y, int w, int h )
{
	VPROF("CViewRender::PerformScreenSpaceEffects()");

	// FIXME: Screen-space effects are busted in the editor
	if ( engine->IsHammerRunning() )
		return;

	g_pScreenSpaceEffects->RenderEffects( x, y, w, h );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the screen space effect material (can't be done during rendering)
//-----------------------------------------------------------------------------
void CViewRender::SetScreenOverlayMaterial( IMaterial *pMaterial )
{
	m_ScreenOverlayMaterial.Init( pMaterial );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMaterial *CViewRender::GetScreenOverlayMaterial( )
{
	return m_ScreenOverlayMaterial;
}


//-----------------------------------------------------------------------------
// Purpose: Performs screen space effects, if any
//-----------------------------------------------------------------------------
void CViewRender::PerformScreenOverlay( int x, int y, int w, int h )
{
	VPROF("CViewRender::PerformScreenOverlay()");

	if (m_ScreenOverlayMaterial)
	{
		if ( m_ScreenOverlayMaterial->NeedsFullFrameBufferTexture() )
		{
			DrawScreenEffectMaterial( m_ScreenOverlayMaterial, x, y, w, h );
		}
		else if ( m_ScreenOverlayMaterial->NeedsPowerOfTwoFrameBufferTexture() )
		{
			// First copy the FB off to the offscreen texture
			UpdateRefractTexture( x, y, w, h, true );

			// Now draw the entire screen using the material...
			CMatRenderContextPtr pRenderContext( materials );
			ITexture *pTexture = GetPowerOfTwoFrameBufferTexture( );
			int sw = pTexture->GetActualWidth();
			int sh = pTexture->GetActualHeight();
			pRenderContext->DrawScreenSpaceRectangle( m_ScreenOverlayMaterial, x, y, w, h,
												 0, 0, sw-1, sh-1, sw, sh );
		}
		else
		{
			byte color[4] = { 255, 255, 255, 255 };
			render->ViewDrawFade( color, m_ScreenOverlayMaterial );
		}
	}
}

void CViewRender::DrawUnderwaterOverlay( void )
{
	IMaterial *pOverlayMat = m_UnderWaterOverlayMaterial;

	if ( pOverlayMat )
	{
		CMatRenderContextPtr pRenderContext( materials );

		int x, y, w, h;

		pRenderContext->GetViewport( x, y, w, h );
		if ( pOverlayMat->NeedsFullFrameBufferTexture() )
		{
			DrawScreenEffectMaterial( pOverlayMat, x, y, w, h );
		}
		else if ( pOverlayMat->NeedsPowerOfTwoFrameBufferTexture() )
		{
			// First copy the FB off to the offscreen texture
			UpdateRefractTexture( x, y, w, h, true );

			// Now draw the entire screen using the material...
			CMatRenderContextPtr pRenderContext( materials );
			ITexture *pTexture = GetPowerOfTwoFrameBufferTexture( );
			int sw = pTexture->GetActualWidth();
			int sh = pTexture->GetActualHeight();
			pRenderContext->DrawScreenSpaceRectangle( pOverlayMat, x, y, w, h,
													  0, 0, sw-1, sh-1, sw, sh );
		}
		else
		{
			pRenderContext->DrawScreenSpaceRectangle( pOverlayMat, x, y, w, h,
													  0, 0, 1, 1, 1, 1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the min/max fade distances
//-----------------------------------------------------------------------------
static ConVar r_fade360style( "r_fade360style", "1" );
void CViewRender::GetScreenFadeDistances( float *pMin, float *pMax )
{
	if ( pMin )
	{
		*pMin = m_FadeData.m_flPixelMin;
	}

	if ( pMax )
	{
		*pMax = m_FadeData.m_flPixelMax;
	}

	// A complete, brutal hack, necessitated by our next-week ship date. 
	// On the 360, we use fade distances to deal with splitscreen. 
	// The tuning is such that the numbers used are correct for 720p.
	// We are not doing this optimization to save on fillrate; instead we are doing it
	// to save on CPU. Therefore, specifying the fades in terms of pixels is not correct.
	// If we're not running @ 720p, then we will recompute a new number based on screen res ratio.
	if ( IsX360() || r_fade360style.GetInt() )
	{
		int screenWidth, screenHeight;
		g_pMaterialSystem->GetBackBufferDimensions( screenWidth, screenHeight );
		if ( screenHeight != 720 )
		{
			float flRatio = (float)screenHeight / 720.0f;
			if ( pMin )
			{
				*pMin *= flRatio;
			}
			if ( pMax )
			{
				*pMax *= flRatio;
			}
		}
	}

}

void CViewRender::OnScreenFadeMinSize( const CCommand &args )
{
	if ( args.ArgC() < 2 )
		return;

	m_FadeData.m_flPixelMin = atof( args[1] ) * 1000.0f;
}

void CViewRender::OnScreenFadeMaxSize( const CCommand &args )
{
	if ( args.ArgC() < 2 )
		return;

	m_FadeData.m_flPixelMax = atof( args[1] ) * 1000.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the fade data.
//-----------------------------------------------------------------------------
void CViewRender::InitFadeData( void )
{
	// What system are we running.
	// GetActualCPULevel() returns the cpu_level convar value, even on the 360.
	// L4D knocks down this convar in splitscreen mode to control the fade distances.
	int nSystemLevel = GetActualCPULevel();
	m_FadeData = g_aFadeData[nSystemLevel+1];
}

C_BaseEntity *CViewRender::GetCurrentlyDrawingEntity()
{
	return m_pCurrentlyDrawingEntity;
}

void CViewRender::SetCurrentlyDrawingEntity( C_BaseEntity *pEnt )
{
	m_pCurrentlyDrawingEntity = pEnt;
}

bool CViewRender::UpdateShadowDepthTexture( ITexture *pRenderTarget, ITexture *pDepthTexture, const CViewSetup &shadowViewIn )
{
	VPROF_INCREMENT_COUNTER( "shadow depth textures rendered", 1 );

	CMatRenderContextPtr pRenderContext( materials );

#ifdef PIX_ENABLE
	char szPIXEventName[128];
	sprintf( szPIXEventName, "UpdateShadowDepthTexture (%s)", pDepthTexture->GetName() );
	PIXEVENT( pRenderContext, szPIXEventName );
#endif

	CRefPtr<CShadowDepthView> pShadowDepthView = new CShadowDepthView( this );
	pShadowDepthView->Setup( shadowViewIn, pRenderTarget, pDepthTexture );
	AddViewToScene( pShadowDepthView );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Renders world and all entities, etc.
//-----------------------------------------------------------------------------
void CViewRender::ViewDrawScene( bool bDrew3dSkybox, SkyboxVisibility_t nSkyboxVisible, const CViewSetup &view, 
								int nClearFlags, view_id_t viewID, bool bDrawViewModel, int baseDrawFlags, ViewCustomVisibility_t *pCustomVisibility )
{
	VPROF( "CViewRender::ViewDrawScene" );

	// this allows the refract texture to be updated once per *scene* on 360
	// (e.g. once for a monitor scene and once for the main scene)
	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	g_pClientShadowMgr->PreRender();

	// Shadowed flashlights supported on ps_2_b and up...
	if ( ( viewID == VIEW_MAIN ) && ( !view.m_bDrawWorldNormal ) )
	{
		// On the 360, we call this even when we don't have shadow depth textures enabled, so that
		// the flashlight state gets set up properly
		g_pClientShadowMgr->ComputeShadowDepthTextures( view );
	}

	m_BaseDrawFlags = baseDrawFlags;

	SetupCurrentView( view.origin, view.angles, viewID, view.m_bDrawWorldNormal, view.m_bCullFrontFaces );

	// Invoke pre-render methods
	IGameSystem::PreRenderAllSystems();

	// Start view
	unsigned int visFlags;
	SetupVis( view, visFlags, pCustomVisibility );

	if ( !bDrew3dSkybox && 
		( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) && ( visFlags & IVRenderView::VIEW_SETUP_VIS_EX_RETURN_FLAGS_USES_RADIAL_VIS ) )
	{
		// This covers the case where we don't see a 3dskybox, yet radial vis is clipping
		// the far plane.  Need to clear to fog color in this case.
		nClearFlags |= VIEW_CLEAR_COLOR;
		SetClearColorToFogColor( );
	}

	bool drawSkybox = r_skybox.GetBool();
	if ( bDrew3dSkybox || ( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) )
	{
		drawSkybox = false;
	}

	ParticleMgr()->IncrementFrameCode();

	DrawWorldAndEntities( drawSkybox, view, nClearFlags, pCustomVisibility );

	// Disable fog for the rest of the stuff
	DisableFog();

	// UNDONE: Don't do this with masked brush models, they should probably be in a separate list
	// render->DrawMaskEntities()

	// Here are the overlays...

	if ( !view.m_bDrawWorldNormal )
	{
		CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );
	}

	// issue the pixel visibility tests
	if ( IsMainView( CurrentViewID() ) && !view.m_bDrawWorldNormal )
	{
		PixelVisibility_EndCurrentView();
	}

	// Draw rain..
	DrawPrecipitation();


	// Draw volumetrics for shadowed flashlights
	if ( r_flashlightvolumetrics.GetBool() && (viewID != VIEW_SHADOW_DEPTH_TEXTURE) && !view.m_bDrawWorldNormal )
	{
		g_pClientShadowMgr->DrawVolumetrics( view );
	}


	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	// Debugging info goes over the top
	CDebugViewRender::Draw3DDebuggingInfo( view );

	// Draw client side effects
	// NOTE: These are not sorted against the rest of the frame
	clienteffects->DrawEffects( gpGlobals->frametime );	

	// Mark the frame as locked down for client fx additions
	SetFXCreationAllowed( false );

	// Invoke post-render methods
	IGameSystem::PostRenderAllSystems();

	FinishCurrentView();

	// Free shadow depth textures for use in future view
	if ( ( viewID == VIEW_MAIN ) && ( !view.m_bDrawWorldNormal ) )
	{
		g_pClientShadowMgr->UnlockAllShadowDepthTextures();
	}

	// Set int rendering parameters back to defaults
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, 0 );

	if ( view.m_bCullFrontFaces )
	{
		pRenderContext->FlipCulling( false );
	}
}

void CheckAndTransitionColor( float flPercent, float *pColor, float *pLerpToColor )
{
	if ( pLerpToColor[0] != pColor[0] || pLerpToColor[1] != pColor[1] || pLerpToColor[2] != pColor[2] )
	{
		float flDestColor[3];

		flDestColor[0] = pLerpToColor[0];
		flDestColor[1] = pLerpToColor[1];
		flDestColor[2] = pLerpToColor[2];

		pColor[0] = FLerp( pColor[0], flDestColor[0], flPercent );
		pColor[1] = FLerp( pColor[1], flDestColor[1], flPercent );
		pColor[2] = FLerp( pColor[2], flDestColor[2], flPercent );
	}
	else
	{
		pColor[0] = pLerpToColor[0];
		pColor[1] = pLerpToColor[1];
		pColor[2] = pLerpToColor[2];
	}
}

static void GetFogColorTransition( fogparams_t *pFogParams, float *pColorPrimary, float *pColorSecondary )
{
	if ( !pFogParams )
		return;

	if ( pFogParams->lerptime >= gpGlobals->curtime )
	{
		float flPercent = 1.0f - (( pFogParams->lerptime - gpGlobals->curtime ) / pFogParams->duration );

		float flPrimaryColorLerp[3] = { pFogParams->colorPrimaryLerpTo.GetR(), pFogParams->colorPrimaryLerpTo.GetG(), pFogParams->colorPrimaryLerpTo.GetB() };
		float flSecondaryColorLerp[3] = { pFogParams->colorSecondaryLerpTo.GetR(), pFogParams->colorSecondaryLerpTo.GetG(), pFogParams->colorSecondaryLerpTo.GetB() };

		CheckAndTransitionColor( flPercent, pColorPrimary, flPrimaryColorLerp );
		CheckAndTransitionColor( flPercent, pColorSecondary, flSecondaryColorLerp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the fog color to use in rendering the current frame.
//-----------------------------------------------------------------------------
static void GetFogColor( fogparams_t *pFogParams, float *pColor, bool ignoreOverride, bool ignoreHDRColorScale )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if ( !pbp || !pFogParams )
		return;

	bool bFogOverride = fog_override.GetBool() && !ignoreOverride;
	float HDRColorScale;
	if( bFogOverride && (fog_hdrcolorscale.GetFloat() != -1.0f) )
	{
		HDRColorScale = fog_hdrcolorscale.GetFloat();
	}
	else
	{
		HDRColorScale = pFogParams->HDRColorScale;
	}

	pColor[0] = pColor[1] = pColor[2] = -1.0f;
	const char *fogColorString = fog_color.GetString();
	if( bFogOverride && fogColorString )
	{
		sscanf( fogColorString, "%f %f %f", pColor, pColor+1, pColor+2 );
	}	

	if( (pColor[0] == -1.0f) && (pColor[1] == -1.0f) && (pColor[2] == -1.0f) ) //if not overriding fog, or if we get non-overridden fog color values
	{
		float flPrimaryColor[3] = { pFogParams->colorPrimary.GetR(), pFogParams->colorPrimary.GetG(), pFogParams->colorPrimary.GetB() };
		float flSecondaryColor[3] = { pFogParams->colorSecondary.GetR(), pFogParams->colorSecondary.GetG(), pFogParams->colorSecondary.GetB() };

		GetFogColorTransition( pFogParams, flPrimaryColor, flSecondaryColor );

		if( pFogParams->blend )
		{
			//
			// Blend between two fog colors based on viewing angle.
			// The secondary fog color is at 180 degrees to the primary fog color.
			//
			Vector forward;
			pbp->EyeVectors( &forward, NULL, NULL );
			
			Vector vNormalized = pFogParams->dirPrimary;
			VectorNormalize( vNormalized );
			pFogParams->dirPrimary = vNormalized;

			float flBlendFactor = 0.5 * forward.Dot( pFogParams->dirPrimary ) + 0.5;

			// FIXME: convert to linear colorspace
			pColor[0] = flPrimaryColor[0] * flBlendFactor + flSecondaryColor[0] * ( 1 - flBlendFactor );
			pColor[1] = flPrimaryColor[1] * flBlendFactor + flSecondaryColor[1] * ( 1 - flBlendFactor );
			pColor[2] = flPrimaryColor[2] * flBlendFactor + flSecondaryColor[2] * ( 1 - flBlendFactor );
		}
		else
		{
			pColor[0] = flPrimaryColor[0];
			pColor[1] = flPrimaryColor[1];
			pColor[2] = flPrimaryColor[2];
		}
	}

	if ( !ignoreHDRColorScale && g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
	{
		VectorScale( pColor, HDRColorScale, pColor );
	}

	VectorScale( pColor, 1.0f / 255.0f, pColor );
}


static float GetFogStart( fogparams_t *pFogParams, bool ignoreOverride )
{
	if( !pFogParams )
		return 0.0f;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_start.GetFloat() == -1.0f )
		{
			return pFogParams->start;
		}
		else
		{
			return fog_start.GetFloat();
		}
	}
	else
	{
		if ( pFogParams->lerptime > gpGlobals->curtime )
		{
			if ( pFogParams->start != pFogParams->startLerpTo )
			{
				if ( pFogParams->lerptime > gpGlobals->curtime )
				{
					float flPercent = 1.0f - (( pFogParams->lerptime - gpGlobals->curtime ) / pFogParams->duration );

					return FLerp( pFogParams->start, pFogParams->startLerpTo, flPercent );
				}
				else
				{
					if ( pFogParams->start != pFogParams->startLerpTo )
					{
						pFogParams->start = pFogParams->startLerpTo;
					}
				}
			}
		}

		return pFogParams->start;
	}
}

static float GetFogEnd( fogparams_t *pFogParams, bool ignoreOverride )
{
	if( !pFogParams )
		return 0.0f;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_end.GetFloat() == -1.0f )
		{
			return pFogParams->end;
		}
		else
		{
			return fog_end.GetFloat();
		}
	}
	else
	{
		if ( pFogParams->lerptime > gpGlobals->curtime )
		{
			if ( pFogParams->end != pFogParams->endLerpTo )
			{
				if ( pFogParams->lerptime > gpGlobals->curtime )
				{
					float flPercent = 1.0f - (( pFogParams->lerptime - gpGlobals->curtime ) / pFogParams->duration );

					return FLerp( pFogParams->end, pFogParams->endLerpTo, flPercent );
				}
				else
				{
					if ( pFogParams->end != pFogParams->endLerpTo )
					{
						pFogParams->end = pFogParams->endLerpTo;
					}
				}
			}
		}

		return pFogParams->end;
	}
}

static bool GetFogEnable( fogparams_t *pFogParams, bool ignoreOverride )
{
	if ( cl_leveloverview.GetFloat() > 0 )
		return false;

	// Ask the clientmode
	if ( GetClientMode()->ShouldDrawFog() == false )
		return false;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_enable.GetInt() )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if( pFogParams )
			return pFogParams->enable != false;

		return false;
	}
}


static float GetFogMaxDensity( fogparams_t *pFogParams, bool ignoreOverride )
{
	if( !pFogParams )
		return 1.0f;

	if ( cl_leveloverview.GetFloat() > 0 )
		return 1.0f;

	// Ask the clientmode
	if ( !GetClientMode()->ShouldDrawFog() )
		return 1.0f;

	if ( fog_override.GetInt() && !ignoreOverride )
	{
		if ( fog_maxdensity.GetFloat() == -1.0f )
			return pFogParams->maxdensity;
		else
			return fog_maxdensity.GetFloat();
	}
	else
	{
		if ( pFogParams->lerptime > gpGlobals->curtime )
		{
			if ( pFogParams->maxdensity != pFogParams->maxdensityLerpTo )
			{
				if ( pFogParams->lerptime > gpGlobals->curtime )
				{
					float flPercent = 1.0f - (( pFogParams->lerptime - gpGlobals->curtime ) / pFogParams->duration );

					return FLerp( pFogParams->maxdensity, pFogParams->maxdensityLerpTo, flPercent );
				}
				else
				{
					if ( pFogParams->maxdensity != pFogParams->maxdensityLerpTo )
					{
						pFogParams->maxdensity = pFogParams->maxdensityLerpTo;
					}
				}
			}
		}

		return pFogParams->maxdensity;
	}
}



//-----------------------------------------------------------------------------
// Purpose: Returns the skybox fog color to use in rendering the current frame.
//-----------------------------------------------------------------------------
static void GetSkyboxFogColor( float *pColor, bool ignoreOverride, bool ignoreHDRColorScale )
{			   
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	bool bFogOverride = fog_override.GetBool() && !ignoreOverride;
	float HDRColorScale;
	if( bFogOverride && (fog_hdrcolorscaleskybox.GetFloat() != -1.0f) )
	{
		HDRColorScale = fog_hdrcolorscaleskybox.GetFloat();
	}
	else
	{
		HDRColorScale = local->m_skybox3d.fog.HDRColorScale;
	}

	pColor[0] = pColor[1] = pColor[2] = -1.0f;
	const char *fogColorString = fog_colorskybox.GetString();
	if( bFogOverride && fogColorString )
	{
		sscanf( fogColorString, "%f %f %f", pColor, pColor+1, pColor+2 );
	}	

	
	if( (pColor[0] == -1.0f) && (pColor[1] == -1.0f) && (pColor[2] == -1.0f) ) //if not overriding fog, or if we get non-overridden fog color values
	{
		if( local->m_skybox3d.fog.blend )
		{
			//
			// Blend between two fog colors based on viewing angle.
			// The secondary fog color is at 180 degrees to the primary fog color.
			//
			Vector forward;
			pbp->EyeVectors( &forward, NULL, NULL );

			Vector vNormalized = local->m_skybox3d.fog.dirPrimary;
			VectorNormalize( vNormalized );
			local->m_skybox3d.fog.dirPrimary = vNormalized;

			float flBlendFactor = 0.5 * forward.Dot( local->m_skybox3d.fog.dirPrimary ) + 0.5;
						 
			// FIXME: convert to linear colorspace
			pColor[0] = local->m_skybox3d.fog.colorPrimary.GetR() * flBlendFactor + local->m_skybox3d.fog.colorSecondary.GetR() * ( 1 - flBlendFactor );
			pColor[1] = local->m_skybox3d.fog.colorPrimary.GetG() * flBlendFactor + local->m_skybox3d.fog.colorSecondary.GetG() * ( 1 - flBlendFactor );
			pColor[2] = local->m_skybox3d.fog.colorPrimary.GetB() * flBlendFactor + local->m_skybox3d.fog.colorSecondary.GetB() * ( 1 - flBlendFactor );
		}
		else
		{
			pColor[0] = local->m_skybox3d.fog.colorPrimary.GetR();
			pColor[1] = local->m_skybox3d.fog.colorPrimary.GetG();
			pColor[2] = local->m_skybox3d.fog.colorPrimary.GetB();
		}
	}

	if ( !ignoreHDRColorScale && g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
	{
		VectorScale( pColor, HDRColorScale, pColor );
	}
	VectorScale( pColor, 1.0f / 255.0f, pColor );
}


static float GetSkyboxFogStart( bool ignoreOverride )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return 0.0f;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_startskybox.GetFloat() == -1.0f )
		{
			return local->m_skybox3d.fog.start;
		}
		else
		{
			return fog_startskybox.GetFloat();
		}
	}
	else
	{
		return local->m_skybox3d.fog.start;
	}
}

static float GetSkyboxFogEnd( bool ignoreOverride )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return 0.0f;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	if( fog_override.GetInt() && !ignoreOverride )
	{
		if( fog_endskybox.GetFloat() == -1.0f )
		{
			return local->m_skybox3d.fog.end;
		}
		else
		{
			return fog_endskybox.GetFloat();
		}
	}
	else
	{
		return local->m_skybox3d.fog.end;
	}
}


static float GetSkyboxFogMaxDensity( bool ignoreOverride )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if ( !pbp )
		return 1.0f;

	CPlayerLocalData *local = &pbp->m_Local;

	if ( cl_leveloverview.GetFloat() > 0 )
		return 1.0f;

	// Ask the clientmode
	if ( !GetClientMode()->ShouldDrawFog() )
		return 1.0f;

	if ( fog_override.GetInt() && !ignoreOverride )
	{
		if ( fog_maxdensityskybox.GetFloat() == -1.0f )
			return local->m_skybox3d.fog.maxdensity;
		else
			return fog_maxdensityskybox.GetFloat();
	}
	else
		return local->m_skybox3d.fog.maxdensity;
}




void CViewRender::DisableFog( void )
{
	VPROF("CViewRander::DisableFog()");

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->FogMode( MATERIAL_FOG_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewRender::SetupVis( const CViewSetup& view, unsigned int &visFlags, ViewCustomVisibility_t *pCustomVisibility )
{
	VPROF( "CViewRender::SetupVis" );

	if ( pCustomVisibility && pCustomVisibility->m_nNumVisOrigins )
	{
		// Pass array or vis origins to merge
		render->ViewSetupVisEx( ShouldForceNoVis(), pCustomVisibility->m_nNumVisOrigins, pCustomVisibility->m_rgVisOrigins, visFlags );
	}
	else
	{
		// Use render origin as vis origin by default
		render->ViewSetupVisEx( ShouldForceNoVis(), 1, &view.origin, visFlags );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Renders voice feedback and other sprites attached to players
// Input  : none
//-----------------------------------------------------------------------------
void CViewRender::RenderPlayerSprites()
{
	GetClientVoiceMgr()->DrawHeadLabels();
}

void CViewRender::DrawLetterBoxRectangles( int nSlot, const CUtlVector< vrect_t >& vecLetterBoxRectangles )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( m_WhiteMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, vecLetterBoxRectangles.Count() );

	Color clr( 0, 0, 0, 255 );

	float zpos = -99999;

	for ( int i=0; i<vecLetterBoxRectangles.Count(); ++i )
	{
		const vrect_t &r = vecLetterBoxRectangles[ i ];

		meshBuilder.Position3f( r.x, r.y, zpos );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( r.x + r.width, r.y, zpos );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( r.x + r.width, r.y + r.height, zpos  );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( r.x, r.y + r.height, zpos );
		meshBuilder.Color4ub( clr.r(), clr.g(), clr.b(), clr.a() );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();

	}
	meshBuilder.End();
	pMesh->Draw();

	
}

/*

1, 2, 3 and 4 are the possible letter box rectangles, if any are needed

(x,y)------------------------------------
|		|			2		|			|
|		|(view.x,view.y)----|			|
|		|					|			|
|	1	|					|		4	|
|		|					|			|
|		|___________________|(vright,vbottom)
|		|			3		|			|
|---------------------------------------| (right, bottom)
|										|
|										|
|										|
|										|
-----------------------------------------
*/
void CViewRender::GetLetterBoxRectangles( int nSlot, const CViewSetup &view, CUtlVector< vrect_t >& vecLetterBoxRectangles )
{
	// This uses the full screen size, not the hud or 3d inset size
	int x, y, w, h;
	VGui_GetPanelBounds( nSlot, x, y, w, h );

	int right = x + w;
	int bottom = y + h;

	vrect_t r;

	int vbottom = view.y + view.height;
	int vright = view.x + view.width;

	// HACK:  Adding in one extra pixel of slop at the border with the inset here...

	// Need rect # 1?
	if ( view.x != x )
	{
		r.x = x;
		r.y = y;
		r.width = view.x + 1;
		r.height = h;

		vecLetterBoxRectangles.AddToTail( r );
	}

	// Need rect # 2?
	if ( view.y != y )
	{
		r.x = view.x;
		r.y = y;
		r.width = view.width;
		r.height = view.y + 1;

		vecLetterBoxRectangles.AddToTail( r );
	}

	// Need rect # 3?
	if ( bottom != vbottom )
	{
		r.x = view.x;
		r.y = vbottom - 1;
		r.width = view.width;
		r.height = bottom - vbottom + 1;

		vecLetterBoxRectangles.AddToTail( r );
	}

	// Need rect # 4?
	if ( right != vright )
	{
		r.x = vright - 1;
		r.y = y;
		r.width = right - vright + 1;
		r.height = h;

		vecLetterBoxRectangles.AddToTail( r );
	}
}

//-----------------------------------------------------------------------------
// Sets up, cleans up the main 3D view
//-----------------------------------------------------------------------------
void CViewRender::SetupMain3DView( int nSlot, const CViewSetup &view, const CViewSetup &hudViewSetup, int &nClearFlags, ITexture *pRenderTarget )
{
	// FIXME: I really want these fields removed from CViewSetup 
	// and passed in as independent flags
	// Clear the color here if requested.

	int nDepthStencilFlags = nClearFlags & ( VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL );
	nClearFlags &= ~( nDepthStencilFlags ); // Clear these flags
	if ( nClearFlags & VIEW_CLEAR_COLOR )
	{
		nClearFlags |= nDepthStencilFlags; // Add them back in if we're clearing color
	}

	// See if this view needs borders
	CUtlVector< vrect_t > letterbox;
	GetLetterBoxRectangles( nSlot, view, letterbox );
	if ( letterbox.Count() )
	{
		CViewSetup letterBoxViewSetup;
		letterBoxViewSetup.x = 0;
		letterBoxViewSetup.y = 0;
		VGui_GetTrueScreenSize( letterBoxViewSetup.width, letterBoxViewSetup.height );
		render->Push2DView( letterBoxViewSetup, 0, pRenderTarget, GetFrustum() );
		
		DrawLetterBoxRectangles( nSlot, letterbox );

		render->PopView( GetFrustum() );
	}

	// If we are using HDR, we render to the HDR backbuffer
	// instead of whatever was previously the render target
	if( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
	{
		// Indicates that the render target is already HDR
		if ( view.m_bHDRTarget )
		{
			render->Push3DView( view, nClearFlags, pRenderTarget, GetFrustum() );
		}
		else
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_BACK_BUFFER_INDEX, BACK_BUFFER_INDEX_HDR );
			pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode
			render->Push3DView( view, nClearFlags, NULL, GetFrustum() );
		}
	}
	else
	{
		render->Push3DView( view, nClearFlags, NULL, GetFrustum() );
	}

	// If we didn't clear the depth here, we'll need to clear it later
	nClearFlags ^= nDepthStencilFlags; // Toggle these bits
	if ( nClearFlags & VIEW_CLEAR_COLOR )
	{
		// If we cleared the color here, we don't need to clear it later
		nClearFlags &= ~( VIEW_CLEAR_COLOR | VIEW_CLEAR_FULL_TARGET );
	}
}

void CViewRender::CleanupMain3DView( const CViewSetup &view )
{
	// Make sure we reset from the HDR rendertarget back to the main backbuffer
	if( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_BACK_BUFFER_INDEX, BACK_BUFFER_INDEX_DEFAULT );
		pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode
	}

	render->PopView( GetFrustum() );
}


//-----------------------------------------------------------------------------
// Queues up an overlay rendering
//-----------------------------------------------------------------------------
void CViewRender::QueueOverlayRenderView( const CViewSetup &view, int nClearFlags, int whatToDraw )
{
	// Can't have 2 in a single scene
	Assert( !m_bDrawOverlay );

    m_bDrawOverlay = true;
	m_OverlayViewSetup = view;
	m_OverlayClearFlags = nClearFlags;
	m_OverlayDrawFlags = whatToDraw;
}

//-----------------------------------------------------------------------------
// Purpose: Force the view to freeze on the next frame for the specified time
//-----------------------------------------------------------------------------
void CViewRender::FreezeFrame( float flFreezeTime )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( flFreezeTime == 0 )
	{
		m_FreezeParams[ slot ].m_flFreezeFrameUntil = 0;
		m_FreezeParams[ slot ].m_bTakeFreezeFrame = false;
	}
	else
	{
		if ( m_FreezeParams[ slot ].m_flFreezeFrameUntil > gpGlobals->curtime )
		{
			m_FreezeParams[ slot ].m_flFreezeFrameUntil += flFreezeTime;
		}
		else
		{
			m_FreezeParams[ slot ].m_flFreezeFrameUntil = gpGlobals->curtime + flFreezeTime;
			m_FreezeParams[ slot ].m_bTakeFreezeFrame = true;	
		}
	}
}

const char *COM_GetModDirectory();

void PositionHudPanels( CUtlVector< vgui::VPANEL > &list, const CViewSetup &view )
{
	for ( int i = 0; i < list.Count(); ++i )
	{
		vgui::VPANEL root = list[ i ];
		if ( root != 0 )
		{
			vgui::ipanel()->SetPos( root, view.x, view.y );
			vgui::ipanel()->SetSize( root, view.width, view.height );
		}
	}
}

#ifdef PARTICLE_USAGE_DEMO
static ConVar r_particle_demo( "r_particle_demo", "0", FCVAR_CHEAT );
static CNonDrawingParticleSystem *s_pDemoSystem = NULL;

void ParticleUsageDemo( void )
{
	if ( r_particle_demo.GetInt() )
	{
		if ( ! s_pDemoSystem )
		{
			s_pDemoSystem = ParticleMgr()->CreateNonDrawingEffect( "christest" );
		}
		// draw a bunch of bars
		CParticleCollection *pSystem = s_pDemoSystem->Get();
		for( int i = 0; i < pSystem->m_nActiveParticles; i++ )
		{
			Vector vecColor = pSystem->GetVectorAttributeValue( PARTICLE_ATTRIBUTE_TINT_RGB, i );
			vecColor *= 255.0;
			float flRadius = *( pSystem->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, i ) );
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->ClearColor4ub( vecColor.x, vecColor.y, vecColor.z, 255 );
			pRenderContext->Viewport( 0, i * 20, flRadius, 17 );
			pRenderContext->ClearBuffers( true, true );
		}
	}
	else
	{
		// its off
		if ( s_pDemoSystem )
		{
			delete s_pDemoSystem;
			s_pDemoSystem = NULL;
		}

	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: This renders the entire 3D view and the in-game hud/viewmodel
// Input  : &view - 
//			whatToDraw - 
//-----------------------------------------------------------------------------
// This renders the entire 3D view.
void CViewRender::RenderView( const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw )
{
	m_UnderWaterOverlayMaterial.Shutdown();					// underwater view will set

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();

	m_CurrentView = view;

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );
	VPROF( "CViewRender::RenderView" );

	// Don't want Left4Dead running less than DX 9
	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 90 )
	{
		// We know they were running at least 9.0 when the game started...we check the 
		// value in ClientDLL_Init()...so they must be messing with their DirectX settings.
		if ( Q_stricmp( COM_GetModDirectory(), "left4dead" ) == 0 )
		{
			static bool bFirstTime = true;
			if ( bFirstTime )
			{
				bFirstTime = false;
				Msg( "This game has a minimum requirement of Shader Model 2.0 to run properly.\n" );
			}
			return;
		}
	}

	{
		// HACK: server-side weapons use the viewmodel model, and client-side weapons swap that out for
		// the world model in DrawModel.  This is too late for some bone setup work that happens before
		// DrawModel, so here we just iterate all weapons we know of and fix them up ahead of time.
		MDLCACHE_CRITICAL_SECTION();
		CUtlLinkedList< CBaseCombatWeapon * > &weaponList = C_BaseCombatWeapon::GetWeaponList();
		FOR_EACH_LL( weaponList, it )
		{
			C_BaseCombatWeapon *weapon = weaponList[it];
			if ( !weapon->IsDormant() )
			{
				weapon->EnsureCorrectRenderingModel();
			}
		}
	}

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *saveRenderTarget = pRenderContext->GetRenderTarget();
	pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode

	if ( !m_FreezeParams[ slot ].m_bTakeFreezeFrame && m_FreezeParams[ slot ].m_flFreezeFrameUntil > gpGlobals->curtime )
	{
		CRefPtr<CFreezeFrameView> pFreezeFrameView = new CFreezeFrameView( this );
		pFreezeFrameView->Setup( view );
		AddViewToScene( pFreezeFrameView );

		g_bRenderingView = true;
		AllowCurrentViewAccess( true );
	}
	else
	{
		g_flFreezeFlash[ slot ] = 0.0f;

	#ifdef USE_MONITORS
		if ( cl_drawmonitors.GetBool() && 
			( ( whatToDraw & RENDERVIEW_SUPPRESSMONITORRENDERING ) == 0 ) )
		{
			DrawMonitors( view );	
		}
	#endif



		g_bRenderingView = true;

		RenderPreScene( view );

		// Must be first 
		render->SceneBegin();

		g_pColorCorrectionMgr->UpdateColorCorrection();

		// Send the current tonemap scalar to the material system
		UpdateMaterialSystemTonemapScalar();

		// clear happens here probably
		SetupMain3DView( slot, view, hudViewSetup, nClearFlags, saveRenderTarget );

		g_pClientShadowMgr->UpdateSplitscreenLocalPlayerShadowSkip();

		bool bDrew3dSkybox = false;
		SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

		// Don't bother with the skybox if we're drawing an ND buffer for the SFM
		if ( !view.m_bDrawWorldNormal )
		{
			// if the 3d skybox world is drawn, then don't draw the normal skybox
			if ( true ) // For pix event
			{
				#if PIX_ENABLE
				{
					CMatRenderContextPtr pRenderContext( materials );
					PIXEVENT( pRenderContext, "Skybox Rendering" );
				}
				#endif

				CSkyboxView *pSkyView = new CSkyboxView( this );
				if ( ( bDrew3dSkybox = pSkyView->Setup( view, &nClearFlags, &nSkyboxVisible ) ) != false )
				{
					AddViewToScene( pSkyView );
				}
				SafeRelease( pSkyView );
			}
		}

		// Force it to clear the framebuffer if they're in solid space.
		if ( ( nClearFlags & VIEW_CLEAR_COLOR ) == 0 )
		{
			MDLCACHE_CRITICAL_SECTION();
			if ( enginetrace->GetPointContents( view.origin ) == CONTENTS_SOLID )
			{
				nClearFlags |= VIEW_CLEAR_COLOR;
			}
		}

		PreViewDrawScene( view );

		// Render world and all entities, particles, etc.
		if( !g_pIntroData )
		{
			#if PIX_ENABLE
			{
				CMatRenderContextPtr pRenderContext( materials );
				PIXEVENT( pRenderContext, "ViewDrawScene()" );
			}
			#endif
			ViewDrawScene( bDrew3dSkybox, nSkyboxVisible, view, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );
		}
		else
		{
			#if PIX_ENABLE
			{
				CMatRenderContextPtr pRenderContext( materials );
				PIXEVENT( pRenderContext, "ViewDrawScene_Intro()" );
			}
			#endif
			ViewDrawScene_Intro( view, nClearFlags, *g_pIntroData );
		}

		// We can still use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( true );

		PostViewDrawScene( view );

		engine->DrawPortals();

		DisableFog();

		// Finish scene
		render->SceneEnd();

		// Draw lightsources if enabled
		render->DrawLights();

		RenderPlayerSprites();

		// Image-space motion blur and depth of field
		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushVertexShaderGPRAllocation( 16 ); //Max out pixel shader threads
			pRenderContext.SafeRelease();
		}
		#endif

		if ( !building_cubemaps.GetBool() )
		{
			if ( IsDepthOfFieldEnabled() )
			{
				pRenderContext.GetFrom( materials );
				{
					PIXEVENT( pRenderContext, "DoDepthOfField()" );
					DoDepthOfField( view );
				}
				pRenderContext.SafeRelease();
			}

			if ( ( view.m_nMotionBlurMode != MOTION_BLUR_DISABLE ) && ( mat_motion_blur_enabled.GetInt() ) )
			{
				pRenderContext.GetFrom( materials );
				{
					PIXEVENT( pRenderContext, "DoImageSpaceMotionBlur()" );
					DoImageSpaceMotionBlur( view );
				}
				pRenderContext.SafeRelease();
			}
		}

		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PopVertexShaderGPRAllocation();
			pRenderContext.SafeRelease();
		}
		#endif

		// Now actually draw the viewmodel
		DrawViewModels( view, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

		DrawUnderwaterOverlay();

		PixelVisibility_EndScene();

		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushVertexShaderGPRAllocation( 16 ); //Max out pixel shader threads
			pRenderContext.SafeRelease();
		}
		#endif

		// Draw fade over entire screen if needed
		byte color[4];
		bool blend;
		GetViewEffects()->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );

		// Store off color fade params to be applied in fullscreen postprocess pass
		SetViewFadeParams( color[0], color[1], color[2], color[3], blend );

		// Draw an overlay to make it even harder to see inside smoke particle systems.
		DrawSmokeFogOverlay();

		// Overlay screen fade on entire screen
		PerformScreenOverlay( view.x, view.y, view.width, view.height );

		// Prevent sound stutter if going slow
		engine->Sound_ExtraUpdate();	

		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
		{
			pRenderContext.GetFrom( materials );
			pRenderContext->SetToneMappingScaleLinear(Vector(1,1,1));
			pRenderContext.SafeRelease();
		}

		if ( !building_cubemaps.GetBool() && view.m_bDoBloomAndToneMapping )
		{
			pRenderContext.GetFrom( materials );
			{
				static bool bAlreadyShowedLoadTime = false;
				
				if ( ! bAlreadyShowedLoadTime )
				{
					bAlreadyShowedLoadTime = true;
					if ( CommandLine()->CheckParm( "-timeload" ) )
					{
						Warning( "time to initial render = %f\n", Plat_FloatTime() );
					}
				}

				PIXEVENT( pRenderContext, "DoEnginePostProcessing()" );

				bool bFlashlightIsOn = false;
				C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
				if ( pLocal )
				{
					bFlashlightIsOn = pLocal->IsEffectActive( EF_DIMLIGHT );
				}
				DoEnginePostProcessing( view.x, view.y, view.width, view.height, bFlashlightIsOn );
			}
			pRenderContext.SafeRelease();
		}

		// And here are the screen-space effects

		if ( IsPC() )
		{
			// Grab the pre-color corrected frame for editing purposes
			engine->GrabPreColorCorrectedFrame( view.x, view.y, view.width, view.height );
		}

		PerformScreenSpaceEffects( view.x, view.y, view.width, view.height );


		#if defined( _X360 )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PopVertexShaderGPRAllocation();
			pRenderContext.SafeRelease();
		}
		#endif

		GetClientMode()->DoPostScreenSpaceEffects( &view );

		CleanupMain3DView( view );

		if ( m_FreezeParams[ slot ].m_bTakeFreezeFrame )
		{
			pRenderContext = materials->GetRenderContext();
			if ( IsX360() )
			{
				// 360 doesn't create the Fullscreen texture
				pRenderContext->CopyRenderTargetToTextureEx( GetFullFrameFrameBufferTexture( 1 ), 0, NULL, NULL );
			}
			else
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetFullscreenTexture(), 0, NULL, NULL );
			}
			pRenderContext.SafeRelease();
			m_FreezeParams[ slot ].m_bTakeFreezeFrame = false;
		}

		pRenderContext = materials->GetRenderContext();
		pRenderContext->SetRenderTarget( saveRenderTarget );
		pRenderContext.SafeRelease();

		// Draw the overlay
		if ( m_bDrawOverlay )
		{	   
			// This allows us to be ok if there are nested overlay views
			CViewSetup currentView = m_CurrentView;
			CViewSetup tempView = m_OverlayViewSetup;
			tempView.fov = ScaleFOVByWidthRatio( tempView.fov, tempView.m_flAspectRatio / ( 4.0f / 3.0f ) );
			tempView.m_bDoBloomAndToneMapping = false;				// FIXME: Hack to get Mark up and running
			tempView.m_nMotionBlurMode = MOTION_BLUR_DISABLE;		// FIXME: Hack to get Mark up and running
			m_bDrawOverlay = false;
			RenderView( tempView, hudViewSetup, m_OverlayClearFlags, m_OverlayDrawFlags );
			m_CurrentView = currentView;
		}
	}

	// Clear a row of pixels at the edge of the viewport if it isn't at the edge of the screen
	if ( VGui_IsSplitScreen() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->PushRenderTargetAndViewport();

		int nScreenWidth, nScreenHeight;
		g_pMaterialSystem->GetBackBufferDimensions( nScreenWidth, nScreenHeight );

		// NOTE: view.height is off by 1 on the PC in a release build, but debug is correct! I'm leaving this here to help track this down later.
		// engine->Con_NPrintf( 25 + hh, "view( %d, %d, %d, %d ) GetBackBufferDimensions( %d, %d )\n", view.x, view.y, view.width, view.height, nScreenWidth, nScreenHeight );

		if ( view.x != 0 ) // if left of viewport isn't at 0
		{
			pRenderContext->Viewport( view.x, view.y, 1, view.height );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( ( view.x + view.width ) != nScreenWidth ) // if right of viewport isn't at edge of screen
		{
			pRenderContext->Viewport( view.x + view.width - 1, view.y, 1, view.height );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( view.y != 0 ) // if top of viewport isn't at 0
		{
			pRenderContext->Viewport( view.x, view.y, view.width, 1 );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		if ( ( view.y + view.height ) != nScreenHeight ) // if bottom of viewport isn't at edge of screen
		{
			pRenderContext->Viewport( view.x, view.y + view.height - 1, view.width, 1 );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}

		pRenderContext->PopRenderTargetAndViewport();
		pRenderContext->Release();
	}

	// Draw the 2D graphics
	m_CurrentView = hudViewSetup;
	pRenderContext = materials->GetRenderContext();
	if ( true )
	{
		PIXEVENT( pRenderContext, "2D Client Rendering" );

		render->Push2DView( hudViewSetup, 0, saveRenderTarget, GetFrustum() );

		Render2DEffectsPreHUD( hudViewSetup );

		if ( whatToDraw & RENDERVIEW_DRAWHUD )
		{
			VPROF_BUDGET( "VGui_DrawHud", VPROF_BUDGETGROUP_OTHER_VGUI );
			// paint the vgui screen
			VGui_PreRender();

			CUtlVector< vgui::VPANEL > vecHudPanels;

			vecHudPanels.AddToTail( VGui_GetClientDLLRootPanel() );

			// This block is suspect - why are we resizing fullscreen panels to be the size of the hudViewSetup
			// which is potentially only half the screen
			if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
			{
				vecHudPanels.AddToTail( VGui_GetFullscreenRootVPANEL() );

#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
				vecHudPanels.AddToTail( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
#endif
				vecHudPanels.AddToTail( enginevgui->GetPanel( PANEL_CLIENTDLL_TOOLS ) );
			}

			PositionHudPanels( vecHudPanels, hudViewSetup );

			// The crosshair, etc. needs to get at the current setup stuff
			AllowCurrentViewAccess( true );

			// Draw the in-game stuff based on the actual viewport being used
			render->VGui_Paint( PAINT_INGAMEPANELS );

			AllowCurrentViewAccess( false );

			VGui_PostRender();

			GetClientMode()->PostRenderVGui();
			pRenderContext->Flush();
		}

		CDebugViewRender::Draw2DDebuggingInfo( hudViewSetup );

		Render2DEffectsPostHUD( hudViewSetup );

		g_bRenderingView = false;

		// We can no longer use the 'current view' stuff set up in ViewDrawScene
		AllowCurrentViewAccess( false );

		if ( IsPC() )
		{
			CDebugViewRender::GenerateOverdrawForTesting();
		}

		render->PopView( GetFrustum() );
	}
	pRenderContext.SafeRelease();

	g_WorldListCache.Flush();

	m_CurrentView = view;

#ifdef PARTICLE_USAGE_DEMO
	ParticleUsageDemo();
#endif


}

//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CViewRender::Render2DEffectsPreHUD( const CViewSetup &view )
{
}

//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CViewRender::Render2DEffectsPostHUD( const CViewSetup &view )
{
}



//-----------------------------------------------------------------------------
//
// NOTE: Below here is all of the stuff that needs to be done for water rendering
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Determines what kind of water we're going to use
//-----------------------------------------------------------------------------
void CViewRender::DetermineWaterRenderInfo( const VisibleFogVolumeInfo_t &fogVolumeInfo, WaterRenderInfo_t &info )
{
	// By default, assume cheap water (even if there's no water in the scene!)
	info.m_bCheapWater = true;
	info.m_bRefract = false;
	info.m_bReflect = false;
	info.m_bReflectEntities = false;
	info.m_bDrawWaterSurface = false;
	info.m_bOpaqueWater = true;



	IMaterial *pWaterMaterial = fogVolumeInfo.m_pFogVolumeMaterial;
	if (( fogVolumeInfo.m_nVisibleFogVolume == -1 ) || !pWaterMaterial )
		return;


	// Use cheap water if mat_drawwater is set
	info.m_bDrawWaterSurface = mat_drawwater.GetBool();
	if ( !info.m_bDrawWaterSurface )
	{
		info.m_bOpaqueWater = false;
		return;
	}

#ifdef _X360
	bool bForceExpensive = false;
#else
	bool bForceExpensive = r_waterforceexpensive.GetBool();
#endif
	bool bForceReflectEntities = r_waterforcereflectentities.GetBool();



	// Determine if the water surface is opaque or not
	info.m_bOpaqueWater = !pWaterMaterial->IsTranslucent();

	bool bForceCheap = false;

	// The material can override the default settings though
	IMaterialVar *pForceCheapVar = pWaterMaterial->FindVar( "$forcecheap", NULL, false );
	IMaterialVar *pForceExpensiveVar = pWaterMaterial->FindVar( "$forceexpensive", NULL, false );
	if ( pForceCheapVar && pForceCheapVar->IsDefined() )
	{
		bForceCheap = ( pForceCheapVar->GetIntValueFast() != 0 );
		if ( bForceCheap )
		{
			bForceExpensive = false;
		}
	}
	if ( !bForceCheap && pForceExpensiveVar && pForceExpensiveVar->IsDefined() )
	{
		 bForceExpensive = bForceExpensive || ( pForceExpensiveVar->GetIntValueFast() != 0 );
	}

	bool bDebugCheapWater = r_debugcheapwater.GetBool();
	if( bDebugCheapWater )
	{
		Msg( "Water material: %s dist to water: %f\nforcecheap: %s forceexpensive: %s\n", 
			pWaterMaterial->GetName(), fogVolumeInfo.m_flDistanceToWater, 
			bForceCheap ? "true" : "false", bForceExpensive ? "true" : "false" );
	}

	// Unless expensive water is active, reflections are off.
	bool bLocalReflection;
#ifdef _X360
	if( !r_WaterDrawReflection.GetBool() )
#else
	if( !bForceExpensive || !r_WaterDrawReflection.GetBool() )
#endif
	{
		bLocalReflection = false;
	}
	else
	{
		IMaterialVar *pReflectTextureVar = pWaterMaterial->FindVar( "$reflecttexture", NULL, false );
		bLocalReflection = pReflectTextureVar && (pReflectTextureVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE);
	}

	// Brian says FIXME: I disabled cheap water LOD when local specular is specified.
	// There are very few places that appear to actually
	// take advantage of it (places where water is in the PVS, but outside of LOD range).
	// It was 2 hours before code lock, and I had the choice of either doubling fill-rate everywhere
	// by making cheap water lod actually work (the water LOD wasn't actually rendering!!!)
	// or to just always render the reflection + refraction if there's a local specular specified.
	// Note that water LOD *does* work with refract-only water

	// Gary says: I'm reverting this change so that water LOD works on dx9 for ep2.

	// Check if the water is out of the cheap water LOD range; if so, use cheap water
#ifdef _X360
	if ( !bForceExpensive && ( bForceCheap || ( fogVolumeInfo.m_flDistanceToWater >= m_flCheapWaterEndDistance ) ) )
	{
		return;
	}
#else
	if ( ( (fogVolumeInfo.m_flDistanceToWater >= m_flCheapWaterEndDistance) && !bLocalReflection ) || bForceCheap )
 		return;
#endif
	// Get the material that is for the water surface that is visible and check to see
	// what render targets need to be rendered, if any.
	if ( !r_WaterDrawRefraction.GetBool() )
	{
		info.m_bRefract = false;
	}
	else
	{
		IMaterialVar *pRefractTextureVar = pWaterMaterial->FindVar( "$refracttexture", NULL, false );
		info.m_bRefract = pRefractTextureVar && (pRefractTextureVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE);

		// Refractive water can be seen through
		if ( info.m_bRefract )
		{
			info.m_bOpaqueWater = false;
		}
	}

	info.m_bReflect = bLocalReflection;
	if ( info.m_bReflect )
	{
		if( bForceReflectEntities )
		{
			info.m_bReflectEntities = true;
		}
		else
		{
			IMaterialVar *pReflectEntitiesVar = pWaterMaterial->FindVar( "$reflectentities", NULL, false );
			info.m_bReflectEntities = pReflectEntitiesVar && (pReflectEntitiesVar->GetIntValueFast() != 0);
		}
	}

	info.m_bCheapWater = !info.m_bReflect && !info.m_bRefract;

	if( bDebugCheapWater )
	{
		Warning( "refract: %s reflect: %s\n", info.m_bRefract ? "true" : "false", info.m_bReflect ? "true" : "false" );
	}
}

//-----------------------------------------------------------------------------
// Draws the world and all entities
//-----------------------------------------------------------------------------
void CViewRender::DrawWorldAndEntities( bool bDrawSkybox, const CViewSetup &viewIn, int nClearFlags, ViewCustomVisibility_t *pCustomVisibility )
{
	MDLCACHE_CRITICAL_SECTION();

	VisibleFogVolumeInfo_t fogVolumeInfo;

	render->GetVisibleFogVolume( viewIn.origin, &fogVolumeInfo );


	WaterRenderInfo_t info;
	DetermineWaterRenderInfo( fogVolumeInfo, info );

	if ( info.m_bCheapWater )
	{		     
		cplane_t glassReflectionPlane;
		if ( IsReflectiveGlassInView( viewIn, glassReflectionPlane ) )
		{								    
			CRefPtr<CReflectiveGlassView> pGlassReflectionView = new CReflectiveGlassView( this );
			pGlassReflectionView->Setup( viewIn, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, bDrawSkybox, fogVolumeInfo, info, glassReflectionPlane );
			AddViewToScene( pGlassReflectionView );

			CRefPtr<CRefractiveGlassView> pGlassRefractionView = new CRefractiveGlassView( this );
			pGlassRefractionView->Setup( viewIn, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, bDrawSkybox, fogVolumeInfo, info, glassReflectionPlane );
			AddViewToScene( pGlassRefractionView );
		}

		CRefPtr<CSimpleWorldView> pNoWaterView = new CSimpleWorldView( this );
		pNoWaterView->Setup( viewIn, nClearFlags, bDrawSkybox, fogVolumeInfo, info, pCustomVisibility );
		AddViewToScene( pNoWaterView );
		return;
	}

	Assert( !pCustomVisibility );

	// Blat out the visible fog leaf if we're not going to use it
	if ( !r_ForceWaterLeaf.GetBool() )
	{
		fogVolumeInfo.m_nVisibleFogVolumeLeaf = -1;
	}

	// We can see water of some sort
	if ( !fogVolumeInfo.m_bEyeInFogVolume )
	{
		CRefPtr<CAboveWaterView> pAboveWaterView = new CAboveWaterView( this );
		pAboveWaterView->Setup( viewIn, bDrawSkybox, fogVolumeInfo, info );
		AddViewToScene( pAboveWaterView );
	}
	else
	{
		CRefPtr<CUnderWaterView> pUnderWaterView = new CUnderWaterView( this );
		pUnderWaterView->Setup( viewIn, bDrawSkybox, fogVolumeInfo, info );
		AddViewToScene( pUnderWaterView );
	}
}


//-----------------------------------------------------------------------------
// Pushes a water render target
//-----------------------------------------------------------------------------
static Vector s_vSavedLinearLightMapScale(-1,-1,-1); // x<0 = no saved scale

static void SetLightmapScaleForWater(void)
{
	if (g_pMaterialSystemHardwareConfig->GetHDRType()==HDR_TYPE_INTEGER)
	{
		CMatRenderContextPtr pRenderContext( materials );
		s_vSavedLinearLightMapScale=pRenderContext->GetToneMappingScaleLinear();
		Vector t25=s_vSavedLinearLightMapScale;
		t25*=0.25;
		pRenderContext->SetToneMappingScaleLinear(t25);
	}
}

//-----------------------------------------------------------------------------
// Returns true if the view plane intersects the water
//-----------------------------------------------------------------------------
bool DoesViewPlaneIntersectWater( float waterZ, int leafWaterDataID )
{
	if ( leafWaterDataID == -1 )
		return false;



	CMatRenderContextPtr pRenderContext( materials );
	
	VMatrix viewMatrix, projectionMatrix, viewProjectionMatrix, inverseViewProjectionMatrix;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &viewMatrix );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &projectionMatrix );
	MatrixMultiply( projectionMatrix, viewMatrix, viewProjectionMatrix );
	MatrixInverseGeneral( viewProjectionMatrix, inverseViewProjectionMatrix );

	Vector mins, maxs;
	ClearBounds( mins, maxs );
	Vector testPoint[4];
	testPoint[0].Init( -1.0f, -1.0f, 0.0f );
	testPoint[1].Init( -1.0f, 1.0f, 0.0f );
	testPoint[2].Init( 1.0f, -1.0f, 0.0f );
	testPoint[3].Init( 1.0f, 1.0f, 0.0f );
	int i;
	bool bAbove = false;
	bool bBelow = false;
	float fudge = 7.0f;
	for( i = 0; i < 4; i++ )
	{
		Vector worldPos;
		Vector3DMultiplyPositionProjective( inverseViewProjectionMatrix, testPoint[i], worldPos );
		AddPointToBounds( worldPos, mins, maxs );
//		Warning( "viewplanez: %f waterZ: %f\n", worldPos.z, waterZ );
		if( worldPos.z + fudge > waterZ )
		{
			bAbove = true;
		}
		if( worldPos.z - fudge < waterZ )
		{
			bBelow = true;
		}
	}

	// early out if the near plane doesn't cross the z plane of the water.
	if( !( bAbove && bBelow ) )
		return false;

	Vector vecFudge( fudge, fudge, fudge );
	mins -= vecFudge;
	maxs += vecFudge;
	
	// the near plane does cross the z value for the visible water volume.  Call into
	// the engine to find out if the near plane intersects the water volume.
	return render->DoesBoxIntersectWaterVolume( mins, maxs, leafWaterDataID );
} 







//-----------------------------------------------------------------------------
// Methods related to controlling the cheap water distance
//-----------------------------------------------------------------------------
void CViewRender::SetCheapWaterStartDistance( float flCheapWaterStartDistance )
{
	m_flCheapWaterStartDistance = flCheapWaterStartDistance;
}

void CViewRender::SetCheapWaterEndDistance( float flCheapWaterEndDistance )
{
	m_flCheapWaterEndDistance = flCheapWaterEndDistance;
}

void CViewRender::GetWaterLODParams( float &flCheapWaterStartDistance, float &flCheapWaterEndDistance )
{
	flCheapWaterStartDistance = m_flCheapWaterStartDistance;
	flCheapWaterEndDistance = m_flCheapWaterEndDistance;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &view - 
//			&introData - 
//-----------------------------------------------------------------------------
void CViewRender::ViewDrawScene_Intro( const CViewSetup &view, int nClearFlags, const IntroData_t &introData )
{
	VPROF( "CViewRender::ViewDrawScene" );

	CMatRenderContextPtr pRenderContext( materials );

	// this allows the refract texture to be updated once per *scene* on 360
	// (e.g. once for a monitor scene and once for the main scene)
	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	// -----------------------------------------------------------------------
	// Set the clear color to black since we are going to be adding up things
	// in the frame buffer.
	// -----------------------------------------------------------------------
	// Clear alpha to 255 so that masking with the vortigaunts (0) works properly.
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	
	// -----------------------------------------------------------------------
	// Draw the primary scene and copy it to the first framebuffer texture
	// -----------------------------------------------------------------------	
	unsigned int visFlags;

	// NOTE: We only increment this once since time doesn't move forward.
	ParticleMgr()->IncrementFrameCode();

	if( introData.m_bDrawPrimary  )
	{
		CViewSetup playerView( view );
		playerView.origin = introData.m_vecCameraView;
		playerView.angles = introData.m_vecCameraViewAngles;
		if ( introData.m_playerViewFOV )
		{
			playerView.fov = ScaleFOVByWidthRatio( introData.m_playerViewFOV, engine->GetScreenAspectRatio( view.width, view.height ) / ( 4.0f / 3.0f ) );
		}

		g_pClientShadowMgr->PreRender();

		// Shadowed flashlights supported on ps_2_b and up...
		if ( r_flashlightdepthtexture.GetBool() )
		{
			g_pClientShadowMgr->ComputeShadowDepthTextures( playerView );
		}

		SetupCurrentView( playerView.origin, playerView.angles, VIEW_INTRO_PLAYER );

		// Invoke pre-render methods
		IGameSystem::PreRenderAllSystems();

		// Start view, clear frame/z buffer if necessary
		SetupVis( playerView, visFlags );
		
		render->Push3DView( playerView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, NULL, GetFrustum() );
		DrawWorldAndEntities( true /* drawSkybox */, playerView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH  );
		render->PopView( GetFrustum() );

		// Free shadow depth textures for use in future view
		if ( r_flashlightdepthtexture.GetBool() )
		{
			g_pClientShadowMgr->UnlockAllShadowDepthTextures();
		}
	}
	else
	{
		pRenderContext->ClearBuffers( true, true );
	}
	Rect_t actualRect;
	UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height, false, &actualRect );

	g_pClientShadowMgr->PreRender();

	// Shadowed flashlights supported on ps_2_b and up...
	if ( r_flashlightdepthtexture.GetBool() )
	{
		g_pClientShadowMgr->ComputeShadowDepthTextures( view );
	}

	// -----------------------------------------------------------------------
	// Draw the secondary scene and copy it to the second framebuffer texture
	// -----------------------------------------------------------------------
	SetupCurrentView( view.origin, view.angles, VIEW_INTRO_CAMERA );

	// Invoke pre-render methods
	IGameSystem::PreRenderAllSystems();

	// Start view, clear frame/z buffer if necessary
	SetupVis( view, visFlags );

	// Clear alpha to 255 so that masking with the vortigaunts (0) works properly.
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

	DrawWorldAndEntities( true /* drawSkybox */, view, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH  );

	UpdateScreenEffectTexture( 1, view.x, view.y, view.width, view.height );

	// -----------------------------------------------------------------------
	// Draw quads on the screen for each screenspace pass.
	// -----------------------------------------------------------------------
	// Find the material that we use to render the overlays
	IMaterial *pOverlayMaterial = materials->FindMaterial( "scripted/intro_screenspaceeffect", TEXTURE_GROUP_OTHER );
	IMaterialVar *pModeVar = pOverlayMaterial->FindVar( "$mode", NULL );
	IMaterialVar *pAlphaVar = pOverlayMaterial->FindVar( "$alpha", NULL );

	pRenderContext->ClearBuffers( true, true );
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	
	int passID;
	for( passID = 0; passID < introData.m_Passes.Count(); passID++ )
	{
		const IntroDataBlendPass_t& pass = introData.m_Passes[passID];
		if ( pass.m_Alpha == 0 )
			continue;

		// Pick one of the blend modes for the material.
		if ( pass.m_BlendMode >= 0 && pass.m_BlendMode <= 9 )
		{
			pModeVar->SetIntValue( pass.m_BlendMode );
		}
		else
		{
			Assert(0);
		}
		// Set the alpha value for the material.
		pAlphaVar->SetFloatValue( pass.m_Alpha );
		
		// Draw a quad for this pass.
		ITexture *pTexture = GetFullFrameFrameBufferTexture( 0 );
		pRenderContext->DrawScreenSpaceRectangle( pOverlayMaterial, view.x, view.y, view.width, view.height,
											actualRect.x, actualRect.y, actualRect.x+actualRect.width-1, actualRect.y+actualRect.height-1, 
											pTexture->GetActualWidth(), pTexture->GetActualHeight() );
	}
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
	
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
	
	// Draw the starfield
	// FIXME
	// blur?
	
	// Disable fog for the rest of the stuff
	DisableFog();
	
	// Here are the overlays...
	CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );

	// issue the pixel visibility tests
	PixelVisibility_EndCurrentView();

	// And here are the screen-space effects
	PerformScreenSpaceEffects( view.x, view.y, view.width, view.height );

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	// Debugging info goes over the top
	CDebugViewRender::Draw3DDebuggingInfo( view );

	// Let the particle manager simulate things that haven't been simulated.
	ParticleMgr()->PostRender();

	FinishCurrentView();

	// Free shadow depth textures for use in future view
	if ( r_flashlightdepthtexture.GetBool() )
	{
		g_pClientShadowMgr->UnlockAllShadowDepthTextures();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets up scene and renders camera view
// Input  : cameraNum - 
//			&cameraView
//			*localPlayer - 
//			x - 
//			y - 
//			width - 
//			height - 
//			highend - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CViewRender::DrawOneMonitor( ITexture *pRenderTarget, int cameraNum, C_PointCamera *pCameraEnt, 
	const CViewSetup &cameraView, C_BasePlayer *localPlayer, int x, int y, int width, int height )
{
#ifdef USE_MONITORS
	VPROF_INCREMENT_COUNTER( "cameras rendered", 1 );
	// Setup fog state for the camera.
	fogparams_t oldFogParams;
	float flOldZFar = 0.0f;

	bool bSky = pCameraEnt->IsSkyEnabled();

	if ( pCameraEnt->GetBrightness() != -1.0f )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetAmbientLightColor( pCameraEnt->GetBrightness(), pCameraEnt->GetBrightness(), pCameraEnt->GetBrightness() );
	}

	bool fogEnabled = pCameraEnt->IsFogEnabled();

	CViewSetup monitorView = cameraView;

	fogparams_t *pFogParams = NULL;

	if ( fogEnabled )
	{	
		if ( !localPlayer )
			return false;

		pFogParams = localPlayer->GetFogParams();

		// Save old fog data.
		oldFogParams = *pFogParams;
		flOldZFar = cameraView.zFar;

		pFogParams->enable = true;
		pFogParams->start = pCameraEnt->GetFogStart();
		pFogParams->end = pCameraEnt->GetFogEnd();
		pFogParams->farz = pCameraEnt->GetFogEnd();
		pFogParams->maxdensity = pCameraEnt->GetFogMaxDensity();

		unsigned char r, g, b;
		pCameraEnt->GetFogColor( r, g, b );
		pFogParams->colorPrimary.SetR( r );
		pFogParams->colorPrimary.SetG( g );
		pFogParams->colorPrimary.SetB( b );

		monitorView.zFar = pCameraEnt->GetFogEnd();
	}

	monitorView.width = width;
	monitorView.height = height;
	monitorView.x = x;
	monitorView.y = y;
	monitorView.origin = pCameraEnt->GetAbsOrigin();
	monitorView.angles = pCameraEnt->GetAbsAngles();
	monitorView.fov = pCameraEnt->GetFOV();
	monitorView.m_bOrtho = false;
	monitorView.m_flAspectRatio = pCameraEnt->UseScreenAspectRatio() ? 0.0f : 1.0f;

	// @MULTICORE (toml 8/11/2006): this should be a renderer....
	Frustum frustum;
 	render->Push3DView( monitorView, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, pRenderTarget, (VPlane *)frustum );
	ViewDrawScene( false, bSky ? SKYBOX_2DSKYBOX_VISIBLE : SKYBOX_NOT_VISIBLE, monitorView, 0, VIEW_MONITOR );
 	render->PopView( frustum );

	// Reset brightness
	if ( pCameraEnt->GetBrightness() != 1.0f )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetAmbientLightColor( -1.0f, -1.0f, -1.0f );
	}

	// Reset the world fog parameters.
	if ( fogEnabled )
	{
		if ( pFogParams )
		{
			*pFogParams = oldFogParams;
		}
		monitorView.zFar = flOldZFar;
	}
#endif // USE_MONITORS
	return true;
}

void CViewRender::DrawMonitors( const CViewSetup &cameraView )
{


#ifdef USE_MONITORS

	// Early out if no cameras
	C_PointCamera *pCameraEnt = GetPointCameraList();
	if ( !pCameraEnt )
		return;

#ifdef _DEBUG
	g_bRenderingCameraView = true;
#endif

	// FIXME: this should check for the ability to do a render target maybe instead.
	// FIXME: shouldn't have to truck through all of the visible entities for this!!!!
	ITexture *pCameraTarget = GetCameraTexture();
	int width = pCameraTarget->GetActualWidth();
	int height = pCameraTarget->GetActualHeight();

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	
	int cameraNum;
	for ( cameraNum = 0; pCameraEnt != NULL; pCameraEnt = pCameraEnt->m_pNext )
	{
		if ( !pCameraEnt->IsActive() || pCameraEnt->IsDormant() )
			continue;

		if ( !DrawOneMonitor( pCameraTarget, cameraNum, pCameraEnt, cameraView, player, 0, 0, width, height ) )
			continue;

		++cameraNum;
	}

	if ( IsX360() && cameraNum > 0 )
	{
		// resolve render target to system memory texture
		// resolving *after* all monitors drawn to ensure a single blit using fastest resolve path
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->PushRenderTargetAndViewport( pCameraTarget );
		pRenderContext->CopyRenderTargetToTextureEx( pCameraTarget, 0, NULL, NULL );
		pRenderContext->PopRenderTargetAndViewport();
	}

#ifdef _DEBUG
	g_bRenderingCameraView = false;
#endif

#endif // USE_MONITORS
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

ClientWorldListInfo_t *ClientWorldListInfo_t::AllocPooled( const ClientWorldListInfo_t &exemplar )
{
	ClientWorldListInfo_t *pResult = gm_Pool.GetObject();

	size_t nBytes = AlignValue( ( exemplar.m_LeafCount * (sizeof(WorldListLeafData_t) + sizeof(exemplar.m_pLeafDataList[0]))), 4096 );
	byte *pMemory = (byte *)pResult->m_pLeafDataList;

	if ( pMemory )
	{
		// Previously allocated, add a reference. Otherwise comes out of GetObject as a new object with a refcount of 1
		pResult->AddRef();
	}

	if ( !pMemory || _msize( pMemory ) < nBytes )
	{
		pMemory = (byte *)realloc( pMemory, nBytes );
	}

	pResult->m_pLeafDataList = (WorldListLeafData_t*)pMemory;
	pResult->m_pOriginalLeafIndex = (uint16*)( (byte *)( pResult->m_pLeafDataList ) + exemplar.m_LeafCount * sizeof(exemplar.m_pLeafDataList[0]) );

	pResult->m_bPooledAlloc = true;

	return pResult;
}

bool ClientWorldListInfo_t::OnFinalRelease()
{
	if ( m_bPooledAlloc )
	{
		Assert( m_pLeafDataList );
		gm_Pool.PutObject( this );
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBase3dView::CBase3dView( CViewRender *pMainView ) :
	m_pMainView( pMainView ),
	m_Frustum( pMainView->m_Frustum ),
	m_nSlot( GET_ACTIVE_SPLITSCREEN_SLOT() )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnt - 
// Output : int
//-----------------------------------------------------------------------------
VPlane* CBase3dView::GetFrustum()
{
	// The frustum is only valid while in a RenderView call.
	// @MULTICORE (toml 8/11/2006): reimplement this when ready -- Assert(g_bRenderingView || g_bRenderingCameraView || g_bRenderingScreenshot);	
	return m_Frustum;
}


CObjectPool<ClientWorldListInfo_t> ClientWorldListInfo_t::gm_Pool;


//-----------------------------------------------------------------------------
// Base class for 3d views
//-----------------------------------------------------------------------------
CRendering3dView::CRendering3dView(CViewRender *pMainView) :
	CBase3dView( pMainView ),
	m_pWorldRenderList( NULL ), 
	m_pRenderablesList( NULL ), 
	m_pWorldListInfo( NULL ), 
	m_pCustomVisibility( NULL ),
	m_DrawFlags( 0 ),
	m_ClearFlags( 0 )
{
}


//-----------------------------------------------------------------------------
// Sort entities in a back-to-front ordering
//-----------------------------------------------------------------------------
void CRendering3dView::Setup( const CViewSetup &setup )
{
	// @MULTICORE (toml 8/15/2006): don't reset if parameters don't require it. For now, just reset
	memcpy( static_cast<CViewSetup *>(this), &setup, sizeof( setup ) );
	ReleaseLists();

	m_pRenderablesList = new CClientRenderablesList; 
	m_pCustomVisibility = NULL;
}


//-----------------------------------------------------------------------------
// Sort entities in a back-to-front ordering
//-----------------------------------------------------------------------------
void CRendering3dView::ReleaseLists()
{
	SafeRelease( m_pWorldRenderList );
	SafeRelease( m_pRenderablesList );
	SafeRelease( m_pWorldListInfo );
	m_pCustomVisibility = NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CRendering3dView::SetupRenderablesList( int viewID )
{
//	VPROF( "CViewRender::SetupRenderablesList" );

	VPROF_BUDGET( "SetupRenderablesList", "SetupRenderablesList" );

	// Clear the list.
	int i;
	for( i=0; i < RENDER_GROUP_COUNT; i++ )
	{
		m_pRenderablesList->m_RenderGroupCounts[i] = 0;
	}

	// Now collate the entities in the leaves.
	if( !m_pMainView->ShouldDrawEntities() )
		return;

	m_pMainView->IncRenderablesListsNumber();

	// Precache information used commonly in CollateRenderables
	SetupRenderInfo_t setupInfo;
	setupInfo.m_pWorldListInfo = m_pWorldListInfo;
	setupInfo.m_nRenderFrame = m_pMainView->BuildRenderablesListsNumber();	// only one incremented?
	setupInfo.m_nDetailBuildFrame = m_pMainView->BuildWorldListsNumber();	//
	setupInfo.m_pRenderList = m_pRenderablesList;
	setupInfo.m_bDrawDetailObjects = GetClientMode()->ShouldDrawDetailObjects() && r_DrawDetailProps.GetInt();
	setupInfo.m_bDrawTranslucentObjects = ( r_flashlightdepth_drawtranslucents.GetBool() || (viewID != VIEW_SHADOW_DEPTH_TEXTURE) || m_bRenderFlashlightDepthTranslucents );
	setupInfo.m_nViewID = viewID;
	setupInfo.m_vecRenderOrigin = origin;
	setupInfo.m_vecRenderForward = CurrentViewForward();

	float fMaxDist = cl_maxrenderable_dist.GetFloat();

	// Shadowing light typically has a smaller farz than cl_maxrenderable_dist
	setupInfo.m_flRenderDistSq = (viewID == VIEW_SHADOW_DEPTH_TEXTURE) ? MIN(zFar, fMaxDist) : fMaxDist;
	setupInfo.m_flRenderDistSq *= setupInfo.m_flRenderDistSq;

	ClientLeafSystem()->BuildRenderablesList( setupInfo );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
// Kinda awkward...three optional parameters at the end...
void CRendering3dView::BuildWorldRenderLists( bool bDrawEntities, int iForceViewLeaf /* = -1 */, 
	bool bUseCacheIfEnabled /* = true */, bool bShadowDepth /* = false */, float *pReflectionWaterHeight /*= NULL*/ )
{
	VPROF_BUDGET( "BuildWorldRenderLists", VPROF_BUDGETGROUP_WORLD_RENDERING );

    // @MULTICORE (toml 8/18/2006): to address....
	extern void UpdateClientRenderableInPVSStatus();
	UpdateClientRenderableInPVSStatus();

	Assert( !m_pWorldRenderList && !m_pWorldListInfo);

	m_pMainView->IncWorldListsNumber();
	// Override vis data if specified this render, otherwise use default behavior with NULL
	VisOverrideData_t* pVisData = ( m_pCustomVisibility && m_pCustomVisibility->m_VisData.m_fDistToAreaPortalTolerance != FLT_MAX ) ?  &m_pCustomVisibility->m_VisData : NULL;
	bool bUseCache = ( bUseCacheIfEnabled && r_worldlistcache.GetBool() );
	if ( !bUseCache || pVisData || !g_WorldListCache.Find( *this, &m_pWorldRenderList, &m_pWorldListInfo ) )
	{
        // @MULTICORE (toml 8/18/2006): when make parallel, will have to change caching to be atomic, where follow ons receive a pointer to a list that is not yet built
		m_pWorldRenderList =  render->CreateWorldList();
		m_pWorldListInfo = new ClientWorldListInfo_t;

		render->BuildWorldLists( m_pWorldRenderList, m_pWorldListInfo, 
			( m_pCustomVisibility ) ? m_pCustomVisibility->m_iForceViewLeaf : iForceViewLeaf, 
			pVisData, bShadowDepth, pReflectionWaterHeight );

		if ( bUseCache && !pVisData )
		{
			g_WorldListCache.Add( *this, m_pWorldRenderList, m_pWorldListInfo );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Computes the actual world list info based on the render flags
//-----------------------------------------------------------------------------
void CRendering3dView::PruneWorldListInfo()
{
	// Drawing everything? Just return the world list info as-is 
	int nWaterDrawFlags = m_DrawFlags & (DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER);
	if ( nWaterDrawFlags == (DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER) )
	{
		return;
	}

	if ( nWaterDrawFlags == DF_RENDER_ABOVEWATER && !m_pWorldListInfo->m_bHasWater )
		return;
	ClientWorldListInfo_t *pNewInfo;
	// Only allocate memory if actually will draw something
	if ( m_pWorldListInfo->m_LeafCount > 0 && nWaterDrawFlags )
	{
		pNewInfo = ClientWorldListInfo_t::AllocPooled( *m_pWorldListInfo );
	}
	else
	{
		pNewInfo = new ClientWorldListInfo_t;
	}

	pNewInfo->m_ViewFogVolume = m_pWorldListInfo->m_ViewFogVolume;
	pNewInfo->m_bHasWater = m_pWorldListInfo->m_bHasWater;
	pNewInfo->m_LeafCount = 0;

	if ( nWaterDrawFlags != DF_RENDER_UNDERWATER || m_pWorldListInfo->m_bHasWater )
	{
		// Not drawing anything? Then don't bother with renderable lists
		if ( nWaterDrawFlags != 0 )
		{
			// Create a sub-list based on the actual leaves being rendered
			bool bRenderingUnderwater = (nWaterDrawFlags & DF_RENDER_UNDERWATER) != 0;
			for ( int i = 0; i < m_pWorldListInfo->m_LeafCount; ++i )
			{
				bool bLeafIsUnderwater = ( m_pWorldListInfo->m_pLeafDataList[i].waterData != -1 );
				if ( bRenderingUnderwater == bLeafIsUnderwater )
				{
					pNewInfo->m_pLeafDataList[ pNewInfo->m_LeafCount ] = m_pWorldListInfo->m_pLeafDataList[ i ];
					pNewInfo->m_pOriginalLeafIndex[ pNewInfo->m_LeafCount ] = i;
					++pNewInfo->m_LeafCount;
				}
			}
		}
	}

	m_pWorldListInfo->Release();
	m_pWorldListInfo = pNewInfo;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static inline void UpdateBrushModelLightmap( IClientRenderable *pEnt )
{
	model_t *pModel = ( model_t * )pEnt->GetModel();
	render->UpdateBrushModelLightmap( pModel, pEnt );
}

void CRendering3dView::BuildRenderableRenderLists( int viewID )
{
	MDLCACHE_CRITICAL_SECTION();

	if ( viewID != VIEW_SHADOW_DEPTH_TEXTURE )
	{
		render->BeginUpdateLightmaps();
	}

	SetupRenderablesList( viewID );

	if ( viewID != VIEW_SHADOW_DEPTH_TEXTURE )
	{
		// update lightmap on brush models if necessary
		for ( int i = 0; i < RENDER_GROUP_COUNT; ++i )
		{
			CClientRenderablesList::CEntry *pEntities = m_pRenderablesList->m_RenderGroups[i];
			int nCount = m_pRenderablesList->m_RenderGroupCounts[i];
			for( int j=0; j < nCount; ++j )
			{
				if ( pEntities[j].m_nModelType != RENDERABLE_MODEL_BRUSH )
					continue;
				Assert(pEntities[j].m_TwoPass==0);
				UpdateBrushModelLightmap( pEntities[j].m_pRenderable );
			}
		}

		render->EndUpdateLightmaps();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CRendering3dView::DrawWorld( float waterZAdjust )
{
	VPROF_INCREMENT_COUNTER( "RenderWorld", 1 );
	VPROF_BUDGET( "DrawWorld", VPROF_BUDGETGROUP_WORLD_RENDERING );
	if( !r_drawopaqueworld.GetBool() )
	{
		return;
	}

	unsigned long engineFlags = BuildEngineDrawWorldListFlags( m_DrawFlags );

	render->DrawWorldLists( m_pWorldRenderList, engineFlags, waterZAdjust );
}

//-----------------------------------------------------------------------------
// Sets up automatic z-prepass on the 360. No-op on PC.
//-----------------------------------------------------------------------------
void CRendering3dView::Begin360ZPass()
{
#ifdef _X360

		// set up command buffer-based fast z rejection for 360
		if ( r_fastzreject.GetBool() && !( m_DrawFlags & DF_SHADOW_DEPTH_MAP ) )
		{
			CMatRenderContextPtr pRenderContext( materials );
			int nNumIndices = render->GetNumIndicesForWorldLists( m_pWorldRenderList, BuildEngineDrawWorldListFlags( m_DrawFlags ) );
			pRenderContext->Begin360ZPass( nNumIndices );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Finishes automatic z-prepass on the 360. Will kick of Z render and color
// render passes. No-op on PC.
//-----------------------------------------------------------------------------
void CRendering3dView::End360ZPass()
{
#ifdef _X360
	{


		if ( r_fastzreject.GetBool() && !( m_DrawFlags & DF_SHADOW_DEPTH_MAP ) )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->End360ZPass();
		}
	}
#endif
}


CMaterialReference g_material_WriteZ; //init'ed on by CViewRender::Init()

//-----------------------------------------------------------------------------
// Fakes per-entity clip planes on cards that don't support user clip planes.
//  Achieves the effect by drawing an invisible box that writes to the depth buffer
//  around the clipped area. It's not perfect, but better than nothing.
//-----------------------------------------------------------------------------
static void DrawClippedDepthBox( IClientRenderable *pEnt, float *pClipPlane )
{
//#define DEBUG_DRAWCLIPPEDDEPTHBOX //uncomment to draw the depth box as a colorful box

	static const int iQuads[6][5] = {	{ 0, 4, 6, 2, 0 }, //always an extra copy of first index at end to make some algorithms simpler
										{ 3, 7, 5, 1, 3 },
										{ 1, 5, 4, 0, 1 },
										{ 2, 6, 7, 3, 2 },
										{ 0, 2, 3, 1, 0 },
										{ 5, 7, 6, 4, 5 } };

	static const int iLines[12][2] = {	{ 0, 1 },
										{ 0, 2 },
										{ 0, 4 },
										{ 1, 3 },
										{ 1, 5 },
										{ 2, 3 },
										{ 2, 6 },
										{ 3, 7 },
										{ 4, 6 },
										{ 4, 5 },
										{ 5, 7 },
										{ 6, 7 } };


#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
	static const float fColors[6][3] = {	{ 1.0f, 0.0f, 0.0f },
											{ 0.0f, 1.0f, 1.0f },
											{ 0.0f, 1.0f, 0.0f },
											{ 1.0f, 0.0f, 1.0f },
											{ 0.0f, 0.0f, 1.0f },
											{ 1.0f, 1.0f, 0.0f } };
#endif

	
	

	Vector vNormal = *(Vector *)pClipPlane;
	float fPlaneDist = pClipPlane[3];

	Vector vMins, vMaxs;
	pEnt->GetRenderBounds( vMins, vMaxs );

	Vector vOrigin = pEnt->GetRenderOrigin();
	QAngle qAngles = pEnt->GetRenderAngles();
	
	Vector vForward, vUp, vRight;
	AngleVectors( qAngles, &vForward, &vRight, &vUp );

	Vector vPoints[8];
	vPoints[0] = vOrigin + (vForward * vMins.x) + (vRight * vMins.y) + (vUp * vMins.z);
	vPoints[1] = vOrigin + (vForward * vMaxs.x) + (vRight * vMins.y) + (vUp * vMins.z);
	vPoints[2] = vOrigin + (vForward * vMins.x) + (vRight * vMaxs.y) + (vUp * vMins.z);
	vPoints[3] = vOrigin + (vForward * vMaxs.x) + (vRight * vMaxs.y) + (vUp * vMins.z);
	vPoints[4] = vOrigin + (vForward * vMins.x) + (vRight * vMins.y) + (vUp * vMaxs.z);
	vPoints[5] = vOrigin + (vForward * vMaxs.x) + (vRight * vMins.y) + (vUp * vMaxs.z);
	vPoints[6] = vOrigin + (vForward * vMins.x) + (vRight * vMaxs.y) + (vUp * vMaxs.z);
	vPoints[7] = vOrigin + (vForward * vMaxs.x) + (vRight * vMaxs.y) + (vUp * vMaxs.z);

	int iClipped[8];
	float fDists[8];
	for( int i = 0; i != 8; ++i )
	{
		fDists[i] = vPoints[i].Dot( vNormal ) - fPlaneDist;
		iClipped[i] = (fDists[i] > 0.0f) ? 1 : 0;
	}

	Vector vSplitPoints[8][8]; //obviously there are only 12 lines, not 64 lines or 64 split points, but the indexing is way easier like this
	int iLineStates[8][8]; //0 = unclipped, 2 = wholly clipped, 3 = first point clipped, 4 = second point clipped

	//categorize lines and generate split points where needed
	for( int i = 0; i != 12; ++i )
	{
		const int *pPoints = iLines[i];
		int iLineState = (iClipped[pPoints[0]] + iClipped[pPoints[1]]);
		if( iLineState != 1 ) //either both points are clipped, or neither are clipped
		{
			iLineStates[pPoints[0]][pPoints[1]] = 
				iLineStates[pPoints[1]][pPoints[0]] = 
					iLineState;
		}
		else
		{
			//one point is clipped, the other is not
			if( iClipped[pPoints[0]] == 1 )
			{
				//first point was clipped, index 1 has the negative distance
				float fInvTotalDist = 1.0f / (fDists[pPoints[0]] - fDists[pPoints[1]]);
				vSplitPoints[pPoints[0]][pPoints[1]] = 
					vSplitPoints[pPoints[1]][pPoints[0]] =
						(vPoints[pPoints[1]] * (fDists[pPoints[0]] * fInvTotalDist)) - (vPoints[pPoints[0]] * (fDists[pPoints[1]] * fInvTotalDist));
				
				Assert( fabs( vNormal.Dot( vSplitPoints[pPoints[0]][pPoints[1]] ) - fPlaneDist ) < 0.01f );

				iLineStates[pPoints[0]][pPoints[1]] = 3;
				iLineStates[pPoints[1]][pPoints[0]] = 4;
			}
			else
			{
				//second point was clipped, index 0 has the negative distance
				float fInvTotalDist = 1.0f / (fDists[pPoints[1]] - fDists[pPoints[0]]);
				vSplitPoints[pPoints[0]][pPoints[1]] = 
					vSplitPoints[pPoints[1]][pPoints[0]] =
						(vPoints[pPoints[0]] * (fDists[pPoints[1]] * fInvTotalDist)) - (vPoints[pPoints[1]] * (fDists[pPoints[0]] * fInvTotalDist));

				Assert( fabs( vNormal.Dot( vSplitPoints[pPoints[0]][pPoints[1]] ) - fPlaneDist ) < 0.01f );

				iLineStates[pPoints[0]][pPoints[1]] = 4;
				iLineStates[pPoints[1]][pPoints[0]] = 3;
			}
		}
	}


	CMatRenderContextPtr pRenderContext( materials );
	
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
	pRenderContext->Bind( materials->FindMaterial( "debug/debugvertexcolor", TEXTURE_GROUP_OTHER ), NULL );
#else
	pRenderContext->Bind( g_material_WriteZ, NULL );
#endif

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 18 ); //6 sides, possible one cut per side. Any side is capable of having 3 tri's. Lots of padding for things that aren't possible

	//going to draw as a collection of triangles, arranged as a triangle fan on each side
	for( int i = 0; i != 6; ++i )
	{
		const int *pPoints = iQuads[i];

		//can't start the fan on a wholly clipped line, so seek to one that isn't
		int j = 0;
		do
		{
			if( iLineStates[pPoints[j]][pPoints[j+1]] != 2 ) //at least part of this line will be drawn
				break;

			++j;
		} while( j != 3 );

		if( j == 3 ) //not enough lines to even form a triangle
			continue;

		float *pStartPoint = 0;
		float *pTriangleFanPoints[4]; //at most, one of our fans will have 5 points total, with the first point being stored separately as pStartPoint
		int iTriangleFanPointCount = 1; //the switch below creates the first for sure
		
		//figure out how to start the fan
		switch( iLineStates[pPoints[j]][pPoints[j+1]] )
		{
		case 0: //uncut
			pStartPoint = &vPoints[pPoints[j]].x;
			pTriangleFanPoints[0] = &vPoints[pPoints[j+1]].x;
			break;

		case 4: //second index was clipped
			pStartPoint = &vPoints[pPoints[j]].x;
			pTriangleFanPoints[0] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			break;

		case 3: //first index was clipped
			pStartPoint = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			pTriangleFanPoints[0] = &vPoints[pPoints[j + 1]].x;
			break;

		default:
			Assert( false );
			break;
		};

		for( ++j; j != 3; ++j ) //add end points for the rest of the indices, we're assembling a triangle fan
		{
			switch( iLineStates[pPoints[j]][pPoints[j+1]] )
			{
			case 0: //uncut line, normal endpoint
				pTriangleFanPoints[iTriangleFanPointCount] = &vPoints[pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			case 2: //wholly cut line, no endpoint
				break;

			case 3: //first point is clipped, normal endpoint
				//special case, adds start and end point
				pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
				++iTriangleFanPointCount;

				pTriangleFanPoints[iTriangleFanPointCount] = &vPoints[pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			case 4: //second point is clipped
				pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
				++iTriangleFanPointCount;
				break;

			default:
				Assert( false );
				break;
			};
		}
		
		//special case endpoints, half-clipped lines have a connecting line between them and the next line (first line in this case)
		switch( iLineStates[pPoints[j]][pPoints[j+1]] )
		{
		case 3:
		case 4:
			pTriangleFanPoints[iTriangleFanPointCount] = &vSplitPoints[pPoints[j]][pPoints[j+1]].x;
			++iTriangleFanPointCount;
			break;
		};

		Assert( iTriangleFanPointCount <= 4 );

		//add the fan to the mesh
		int iLoopStop = iTriangleFanPointCount - 1;
		for( int k = 0; k != iLoopStop; ++k )
		{
			meshBuilder.Position3fv( pStartPoint );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			float fHalfColors[3] = { fColors[i][0] * 0.5f, fColors[i][1] * 0.5f, fColors[i][2] * 0.5f };
			meshBuilder.Color3fv( fHalfColors );
#endif
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Position3fv( pTriangleFanPoints[k] );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			meshBuilder.Color3fv( fColors[i] );
#endif
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( pTriangleFanPoints[k+1] );
#ifdef DEBUG_DRAWCLIPPEDDEPTHBOX
			meshBuilder.Color3fv( fColors[i] );
#endif
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();
	pRenderContext->Flush( false );
}

static inline bool BlurTest( IClientRenderable *pRenderable, int drawFlags, bool bPreDraw, const RenderableInstance_t &instance )
{
	if( CurrentViewID() == VIEW_MONITOR )
		return false;

	if( !bPreDraw )
		return false;

	IClientUnknown *pUnknown = pRenderable->GetIClientUnknown();
	if( !pUnknown )
		return false;

	IClientEntity *pClientEntity = pUnknown->GetIClientEntity();
	if( !pClientEntity )
		return false;

	if( pClientEntity->IsBlurred() )
	{
		const CViewSetup *pViewSetup = view->GetViewSetup();
		if( pViewSetup )
		{
			BlurEntity( pRenderable, bPreDraw, drawFlags, instance, *pViewSetup, pViewSetup->x, pViewSetup->y, pViewSetup->width, pViewSetup->height );
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Unified bit of draw code for opaque and translucent renderables
//-----------------------------------------------------------------------------
static inline void DrawRenderable( IClientRenderable *pEnt, int flags, const RenderableInstance_t &instance )
{
	float *pRenderClipPlane = NULL;
	if( r_entityclips.GetBool() )
		pRenderClipPlane = pEnt->GetRenderClipPlane();

	if( pRenderClipPlane )	
	{
		CMatRenderContextPtr pRenderContext( materials );
		if( !materials->UsingFastClipping() ) //do NOT change the fast clip plane mid-scene, depth problems result. Regular user clip planes are fine though
			pRenderContext->PushCustomClipPlane( pRenderClipPlane );
		else
			DrawClippedDepthBox( pEnt, pRenderClipPlane );
		Assert( view->GetCurrentlyDrawingEntity() == NULL );
		view->SetCurrentlyDrawingEntity( pEnt->GetIClientUnknown()->GetBaseEntity() );
		bool bBlockNormalDraw = BlurTest( pEnt, flags, true, instance );
		if( !bBlockNormalDraw )
			pEnt->DrawModel( flags, instance );
		BlurTest( pEnt, flags, false, instance );
		view->SetCurrentlyDrawingEntity( NULL );

		if( !materials->UsingFastClipping() )	
			pRenderContext->PopCustomClipPlane();
	}
	else
	{
		Assert( view->GetCurrentlyDrawingEntity() == NULL );
		view->SetCurrentlyDrawingEntity( pEnt->GetIClientUnknown()->GetBaseEntity() );
		bool bBlockNormalDraw = BlurTest( pEnt, flags, true, instance );
		if( !bBlockNormalDraw )
			pEnt->DrawModel( flags, instance );
		BlurTest( pEnt, flags, false, instance );
		view->SetCurrentlyDrawingEntity( NULL );
	}
}

//-----------------------------------------------------------------------------
// Draws all opaque renderables in leaves that were rendered
//-----------------------------------------------------------------------------
static inline void DrawOpaqueRenderable( IClientRenderable *pEnt, bool bTwoPass, bool bShadowDepth )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	float color[3];

	Assert( !IsSplitScreenSupported() || pEnt->ShouldDrawForSplitScreenUser( GET_ACTIVE_SPLITSCREEN_SLOT() ) );
	Assert( (pEnt->GetIClientUnknown() == NULL) || (pEnt->GetIClientUnknown()->GetIClientEntity() == NULL) || (pEnt->GetIClientUnknown()->GetIClientEntity()->IsBlurred() == false) );
	pEnt->GetColorModulation( color );
	render->SetColorModulation(	color );

	int flags = STUDIO_RENDER;
	if ( bTwoPass )
	{
		flags |= STUDIO_TWOPASS;
	}

	if ( bShadowDepth )
	{
		flags |= STUDIO_SHADOWDEPTHTEXTURE;
	}

	RenderableInstance_t instance;
	instance.m_nAlpha = 255;
	DrawRenderable( pEnt, flags, instance );
}

//-------------------------------------


static void SetupBonesOnBaseAnimating( C_BaseAnimating *&pBaseAnimating )
{
	pBaseAnimating->SetupBones( NULL, -1, -1, gpGlobals->curtime );
}


static void DrawOpaqueRenderables_DrawBrushModels( int nCount, CClientRenderablesList::CEntry **ppEntities, bool bShadowDepth )
{
	for( int i = 0; i < nCount; ++i )
	{
		Assert( !ppEntities[i]->m_TwoPass );
		DrawOpaqueRenderable( ppEntities[i]->m_pRenderable, false, bShadowDepth );
	}
}

static void DrawOpaqueRenderables_DrawStaticProps( int nCount, CClientRenderablesList::CEntry **ppEntities, bool bShadowDepth )
{
	if ( nCount == 0 )
		return;

	float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	render->SetColorModulation(	one );
	render->SetBlend( 1.0f );
	
	const int MAX_STATICS_PER_BATCH = 512;
	IClientRenderable *pStatics[ MAX_STATICS_PER_BATCH ];
	RenderableInstance_t pInstances[ MAX_STATICS_PER_BATCH ];
	
	int numScheduled = 0, numAvailable = MAX_STATICS_PER_BATCH;

	for( int i = 0; i < nCount; ++i )
	{
		CClientRenderablesList::CEntry *itEntity = ppEntities[i];
		if ( itEntity->m_pRenderable )
			NULL;
		else
			continue;

		pInstances[ numScheduled ] = itEntity->m_InstanceData;
		pStatics[ numScheduled ++ ] = itEntity->m_pRenderable;
		if ( -- numAvailable > 0 )
			continue; // place a hint for compiler to predict more common case in the loop
		
		staticpropmgr->DrawStaticProps( pStatics, pInstances, numScheduled, bShadowDepth, vcollide_wireframe.GetBool() );
		numScheduled = 0;
		numAvailable = MAX_STATICS_PER_BATCH;
	}
	
	if ( numScheduled )
		staticpropmgr->DrawStaticProps( pStatics, pInstances, numScheduled, bShadowDepth, vcollide_wireframe.GetBool() );
}

static void DrawOpaqueRenderables_Range( int nCount, CClientRenderablesList::CEntry **ppEntities, bool bShadowDepth )
{
	for ( int i = 0; i < nCount; ++i )
	{
		CClientRenderablesList::CEntry *itEntity = ppEntities[i]; 
		if ( itEntity->m_pRenderable )
		{
			DrawOpaqueRenderable( itEntity->m_pRenderable, ( itEntity->m_TwoPass != 0 ), bShadowDepth );
		}
	}
}

ConVar cl_modelfastpath( "cl_modelfastpath", "1" );
ConVar cl_skipslowpath( "cl_skipslowpath", "0", FCVAR_CHEAT, "Set to 1 to skip any models that don't go through the model fast path" );
extern ConVar r_drawothermodels;
static void	DrawOpaqueRenderables_ModelRenderables( int nCount, ModelRenderSystemData_t* pModelRenderables, bool bShadowDepth )
{
	g_pModelRenderSystem->DrawModels( pModelRenderables, nCount, bShadowDepth ? MODEL_RENDER_MODE_SHADOW_DEPTH : MODEL_RENDER_MODE_NORMAL );
}

static void	DrawOpaqueRenderables_NPCs( int nCount, CClientRenderablesList::CEntry **ppEntities, bool bShadowDepth )
{
	DrawOpaqueRenderables_Range( nCount, ppEntities, bShadowDepth );
}

void CRendering3dView::DrawOpaqueRenderables( bool bShadowDepth )
{
	VPROF("CViewRender::DrawOpaqueRenderables" );

	if( !r_drawopaquerenderables.GetBool() )
		return;

	if( !m_pMainView->ShouldDrawEntities() )
		return;

	render->SetBlend( 1 );

	//
	// Prepare to iterate over all leaves that were visible, and draw opaque things in them.	
	//
	RopeManager()->ResetRenderCache();
	g_pParticleSystemMgr->ResetRenderCache();

	// Categorize models by type
	int nOpaqueRenderableCount = m_pRenderablesList->m_RenderGroupCounts[RENDER_GROUP_OPAQUE];
	CUtlVector< CClientRenderablesList::CEntry* > brushModels( (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) ), nOpaqueRenderableCount );
	CUtlVector< CClientRenderablesList::CEntry* > staticProps( (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) ), nOpaqueRenderableCount );
	CUtlVector< CClientRenderablesList::CEntry* > otherRenderables( (CClientRenderablesList::CEntry **)stackalloc( nOpaqueRenderableCount * sizeof( CClientRenderablesList::CEntry* ) ), nOpaqueRenderableCount );
	CClientRenderablesList::CEntry *pOpaqueList = m_pRenderablesList->m_RenderGroups[RENDER_GROUP_OPAQUE];
	for ( int i = 0; i < nOpaqueRenderableCount; ++i )
	{
		switch( pOpaqueList[i].m_nModelType )
		{
		case RENDERABLE_MODEL_BRUSH:		brushModels.AddToTail( &pOpaqueList[i] ); break; 
		case RENDERABLE_MODEL_STATIC_PROP:	staticProps.AddToTail( &pOpaqueList[i] ); break; 
		default:							otherRenderables.AddToTail( &pOpaqueList[i] ); break; 
		}
	}

	//
	// First do the brush models
	//
	DrawOpaqueRenderables_DrawBrushModels( brushModels.Count(), brushModels.Base(), bShadowDepth );

	// Move all static props to modelrendersystem
	bool bUseFastPath = ( cl_modelfastpath.GetInt() != 0 );

	//
	// Sort everything that's not a static prop
	//
	int nStaticPropCount = staticProps.Count();
	int numOpaqueEnts = otherRenderables.Count();
	CUtlVector< CClientRenderablesList::CEntry* > arrRenderEntsNpcsFirst( (CClientRenderablesList::CEntry **)stackalloc( numOpaqueEnts * sizeof( CClientRenderablesList::CEntry ) ), numOpaqueEnts );
	CUtlVector< ModelRenderSystemData_t > arrModelRenderables( (ModelRenderSystemData_t *)stackalloc( ( numOpaqueEnts + nStaticPropCount ) * sizeof( ModelRenderSystemData_t ) ), numOpaqueEnts + nStaticPropCount );

	// Queue up RENDER_GROUP_OPAQUE_ENTITY entities to be rendered later.
	CClientRenderablesList::CEntry *itEntity;
	if( r_drawothermodels.GetBool() )
	{
		for ( int i = 0; i < numOpaqueEnts; ++i )
		{
			itEntity = otherRenderables[i];
			if ( !itEntity->m_pRenderable )
				continue;

			IClientUnknown *pUnknown = itEntity->m_pRenderable->GetIClientUnknown();
			IClientModelRenderable *pModelRenderable = pUnknown->GetClientModelRenderable();
			C_BaseEntity *pEntity = pUnknown->GetBaseEntity();

			// FIXME: Strangely, some static props are in the non-static prop bucket
			// which is what the last case in this if statement is for
			if ( bUseFastPath && pModelRenderable )
			{
				ModelRenderSystemData_t data;
				data.m_pRenderable = itEntity->m_pRenderable;
				data.m_pModelRenderable = pModelRenderable;
				data.m_InstanceData = itEntity->m_InstanceData;
				arrModelRenderables.AddToTail( data );
				otherRenderables.FastRemove( i );
				--i; --numOpaqueEnts;
				continue;
			}

			if ( !pEntity )
				continue;

			if ( pEntity->IsNPC() )
			{
				arrRenderEntsNpcsFirst.AddToTail( itEntity );
				otherRenderables.FastRemove( i );
				--i; --numOpaqueEnts;
				continue;
			}
		}
	}

	// Queue up the static props to be rendered later.
	for ( int i = 0; i < nStaticPropCount; ++i )
	{
		itEntity = staticProps[i];
		if ( !itEntity->m_pRenderable )
			continue;

		IClientUnknown *pUnknown = itEntity->m_pRenderable->GetIClientUnknown();
		IClientModelRenderable *pModelRenderable = pUnknown->GetClientModelRenderable();
		if ( !bUseFastPath || !pModelRenderable )
			continue;

		ModelRenderSystemData_t data;
		data.m_pRenderable = itEntity->m_pRenderable;
		data.m_pModelRenderable = pModelRenderable;
		data.m_InstanceData = itEntity->m_InstanceData;
		arrModelRenderables.AddToTail( data );

		staticProps.FastRemove( i );
		--i; --nStaticPropCount;
	}

	//
	// Draw model renderables now (ie. models that use the fast path)
	//					 
	DrawOpaqueRenderables_ModelRenderables( arrModelRenderables.Count(), arrModelRenderables.Base(), bShadowDepth );

	// Turn off z pass here. Don't want non-fastpath models with potentially large dynamic VB requirements overwrite
	// stuff in the dynamic VB ringbuffer. We're calling End360ZPass again in DrawExecute, but that's not a problem.
	// Begin360ZPass/End360ZPass don't have to be matched exactly.
	End360ZPass();

	//
	// Draw static props + opaque entities that aren't using the fast path.
	//
	DrawOpaqueRenderables_Range( otherRenderables.Count(), otherRenderables.Base(), bShadowDepth );
	DrawOpaqueRenderables_DrawStaticProps( staticProps.Count(), staticProps.Base(), bShadowDepth );

	//
	// Draw NPCs now
	//
	DrawOpaqueRenderables_NPCs( arrRenderEntsNpcsFirst.Count(), arrRenderEntsNpcsFirst.Base(), bShadowDepth );

	//
	// Ropes and particles
	//
	RopeManager()->DrawRenderCache( bShadowDepth );
	g_pParticleSystemMgr->DrawRenderCache( bShadowDepth );
}


//-----------------------------------------------------------------------------
// Renders all translucent world + detail objects in a particular set of leaves
//-----------------------------------------------------------------------------
void CRendering3dView::DrawTranslucentWorldInLeaves( bool bShadowDepth )
{
	if ( bShadowDepth )
		return;

	VPROF_BUDGET( "CViewRender::DrawTranslucentWorldInLeaves", VPROF_BUDGETGROUP_WORLD_RENDERING );
	const ClientWorldListInfo_t& info = *m_pWorldListInfo;
	CUtlVectorFixedGrowable<int, 32> transSortIndexList;
	for( int iCurLeafIndex = info.m_LeafCount - 1; iCurLeafIndex >= 0; iCurLeafIndex-- )
	{
		if ( info.m_pLeafDataList[iCurLeafIndex].translucentSurfaceCount )
		{
			int nActualLeafIndex = info.m_pOriginalLeafIndex ? info.m_pOriginalLeafIndex[ iCurLeafIndex ] : iCurLeafIndex;
			Assert( nActualLeafIndex != INVALID_LEAF_INDEX );
			transSortIndexList.AddToTail(nActualLeafIndex);
		}
	}
	if ( transSortIndexList.Count() )
	{
		// Now draw the surfaces in this leaf
		render->DrawTranslucentSurfaces( m_pWorldRenderList, transSortIndexList.Base(), transSortIndexList.Count(), m_DrawFlags );
	}
}


//-----------------------------------------------------------------------------
// Renders all translucent world + detail objects in a particular set of leaves
//-----------------------------------------------------------------------------
void CRendering3dView::DrawTranslucentWorldAndDetailPropsInLeaves( int iCurLeafIndex, int iFinalLeafIndex, int nEngineDrawFlags, int &nDetailLeafCount, LeafIndex_t* pDetailLeafList, bool bShadowDepth )
{
	if ( bShadowDepth )
		return;

	CUtlVectorFixedGrowable<int, 32> transSortIndexList;
	VPROF_BUDGET( "CViewRender::DrawTranslucentWorldAndDetailPropsInLeaves", VPROF_BUDGETGROUP_WORLD_RENDERING );
	const ClientWorldListInfo_t& info = *m_pWorldListInfo;
	for( ; iCurLeafIndex >= iFinalLeafIndex; iCurLeafIndex-- )
	{
		if ( info.m_pLeafDataList[iCurLeafIndex].translucentSurfaceCount )
		{
			int nActualLeafIndex = info.m_pOriginalLeafIndex ? info.m_pOriginalLeafIndex[ iCurLeafIndex ] : iCurLeafIndex;
			Assert( nActualLeafIndex != INVALID_LEAF_INDEX );
			// First draw any queued-up detail props from previously visited leaves
			if ( nDetailLeafCount )
			{
				DetailObjectSystem()->RenderTranslucentDetailObjects( m_pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );
				nDetailLeafCount = 0;
			}
			transSortIndexList.AddToTail(nActualLeafIndex);
		}

		// Queue up detail props that existed in this leaf
		if ( ClientLeafSystem()->ShouldDrawDetailObjectsInLeaf( info.m_pLeafDataList[iCurLeafIndex].leafIndex, m_pMainView->BuildWorldListsNumber() ) )
		{
			if ( transSortIndexList.Count() )
			{
				// Now draw the surfaces in this leaf
				render->DrawTranslucentSurfaces( m_pWorldRenderList, transSortIndexList.Base(), transSortIndexList.Count(), nEngineDrawFlags );
			}
			pDetailLeafList[nDetailLeafCount] = info.m_pLeafDataList[iCurLeafIndex].leafIndex;
			++nDetailLeafCount;
		}
	}
	if ( transSortIndexList.Count() )
	{
		// Now draw the surfaces in this leaf
		render->DrawTranslucentSurfaces( m_pWorldRenderList, transSortIndexList.Base(), transSortIndexList.Count(), nEngineDrawFlags );
	}
}

//-----------------------------------------------------------------------------
// Renders all translucent entities in the render list
//-----------------------------------------------------------------------------
static inline void DrawTranslucentRenderable( IClientRenderable *pEnt, const RenderableInstance_t &instance, bool twoPass, bool bShadowDepth )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	Assert( !IsSplitScreenSupported() || pEnt->ShouldDrawForSplitScreenUser( GET_ACTIVE_SPLITSCREEN_SLOT() ) );

	// Renderable list building should already have caught this
	Assert( instance.m_nAlpha > 0 );

	// Determine blending amount and tell engine
	float blend = (float)( instance.m_nAlpha / 255.0f );

	// Tell engine
	render->SetBlend( blend );

	float color[3];
	pEnt->GetColorModulation( color );
	render->SetColorModulation(	color );

	int flags = STUDIO_RENDER | STUDIO_TRANSPARENCY;
	if ( twoPass )
		flags |= STUDIO_TWOPASS;

	if ( bShadowDepth )
		flags |= STUDIO_SHADOWDEPTHTEXTURE;

	DrawRenderable( pEnt, flags, instance );
}


//-----------------------------------------------------------------------------
// Renders all translucent entities in the render list
//-----------------------------------------------------------------------------
void CRendering3dView::DrawTranslucentRenderablesNoWorld( bool bInSkybox )
{
	VPROF( "CViewRender::DrawTranslucentRenderablesNoWorld" );

	if ( !m_pMainView->ShouldDrawEntities() || !r_drawtranslucentrenderables.GetBool() )
		return;

	// Draw the particle singletons.
	DrawParticleSingletons( bInSkybox );

	bool bShadowDepth = (m_DrawFlags & DF_SHADOW_DEPTH_MAP ) != 0;

	CClientRenderablesList::CEntry *pEntities = m_pRenderablesList->m_RenderGroups[RENDER_GROUP_TRANSLUCENT];
	int iCurTranslucentEntity = m_pRenderablesList->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT] - 1;

	while( iCurTranslucentEntity >= 0 )
	{
		IClientRenderable *pRenderable = pEntities[iCurTranslucentEntity].m_pRenderable;
		int nRenderFlags = pRenderable->GetRenderFlags();
		
		if ( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
		{
			UpdateRefractTexture();
		}

		if ( nRenderFlags & ERENDERFLAGS_NEEDS_FULL_FB )
		{
			UpdateScreenEffectTexture();
		}

		DrawTranslucentRenderable( pRenderable, pEntities[iCurTranslucentEntity].m_InstanceData, 
			pEntities[iCurTranslucentEntity].m_TwoPass != 0, bShadowDepth );
		--iCurTranslucentEntity;
	}

	// Reset the blend state.
	render->SetBlend( 1 );
}


//-----------------------------------------------------------------------------
// Renders all translucent entities in the render list that ignore the Z buffer
//-----------------------------------------------------------------------------
void CRendering3dView::DrawNoZBufferTranslucentRenderables( void )
{
	VPROF( "CViewRender::DrawNoZBufferTranslucentRenderables" );

	if ( !m_pMainView->ShouldDrawEntities() || !r_drawtranslucentrenderables.GetBool() )
		return;

	bool bShadowDepth = (m_DrawFlags & DF_SHADOW_DEPTH_MAP ) != 0;

	// FIXME: This ignores Z. We don't need to sort it at all? Not sure about refraction here...
	// Could use fast path
	CClientRenderablesList::CEntry *pEntities = m_pRenderablesList->m_RenderGroups[RENDER_GROUP_TRANSLUCENT_IGNOREZ];
	int iCurTranslucentEntity = m_pRenderablesList->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT_IGNOREZ] - 1;

	while( iCurTranslucentEntity >= 0 )
	{
		IClientRenderable *pRenderable = pEntities[iCurTranslucentEntity].m_pRenderable;
		int nRenderFlags = pRenderable->GetRenderFlags();
		if ( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
		{
			UpdateRefractTexture();
		}

		if ( nRenderFlags & ERENDERFLAGS_NEEDS_FULL_FB )
		{
			UpdateScreenEffectTexture();
		}

		DrawTranslucentRenderable( pRenderable, pEntities[iCurTranslucentEntity].m_InstanceData,
			pEntities[iCurTranslucentEntity].m_TwoPass != 0, bShadowDepth );
		--iCurTranslucentEntity;
	}

	// Reset the blend state.
	render->SetBlend( 1 );
}


static ConVar r_unlimitedrefract( "r_unlimitedrefract", "0" );

static void UpdateNecessaryRenderTargets( int nRenderFlags )
{
	if ( nRenderFlags )
	{
		CMatRenderContextPtr pRenderContext( materials );
		ITexture *rt = pRenderContext->GetRenderTarget();

		// supress refract update
		if (
			( nRenderFlags & ERENDERFLAGS_REFRACT_ONLY_ONCE_PER_FRAME ) &&
			( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB ) &&
			( gpGlobals->framecount == g_viewscene_refractUpdateFrame ) &&
			( r_unlimitedrefract.GetInt() == 0 ) )
		{
			nRenderFlags &= ~( ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB );
		}
		if ( rt )
		{
			if ( nRenderFlags & ERENDERFLAGS_NEEDS_FULL_FB )
			{
				UpdateScreenEffectTexture( 0, 0, 0, rt->GetActualWidth(), rt->GetActualHeight(), true );
			}
			else if ( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
			{
				UpdateRefractTexture(0, 0, rt->GetActualWidth(), rt->GetActualHeight());
			}
		}
		else
		{
			if ( nRenderFlags & ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB )
			{
				UpdateRefractTexture();
			}
		}

		pRenderContext.SafeRelease();
	}
}

//-----------------------------------------------------------------------------
// Renders all translucent world, entities, and detail objects in a particular set of leaves
//-----------------------------------------------------------------------------
ConVar cl_tlucfastpath( "cl_tlucfastpath", "1" );
extern ConVar cl_colorfastpath;

void CRendering3dView::DrawTranslucentRenderables( bool bInSkybox, bool bShadowDepth )
{
	const ClientWorldListInfo_t& info = *m_pWorldListInfo;


	{
		//opaques generally write depth, and translucents generally don't.
		//So immediately after opaques are done is the best time to snap off the depth buffer to a texture.
		switch ( g_CurrentViewID )
		{				 
		case VIEW_MAIN:
#ifdef _X360
		case VIEW_INTRO_CAMERA:
		case VIEW_INTRO_PLAYER:
#endif
			UpdateFullScreenDepthTexture();
			break;

		default:
			materials->GetRenderContext()->SetFullScreenDepthTextureValidityFlag( false );
			break;
		}
	}


	

	// FIXME: Add support for deferred shadows in 3D skybox
	if ( r_shadow_deferred.GetBool() && bInSkybox == false )
	{
		g_pClientShadowMgr->DrawDeferredShadows( (*this), m_pWorldListInfo->m_LeafCount, m_pWorldListInfo->m_pLeafDataList );
	}

	if ( !r_drawtranslucentworld.GetBool() )
	{
		DrawTranslucentRenderablesNoWorld( bInSkybox );
		return;
	}

	VPROF( "CViewRender::DrawTranslucentRenderables" );
	int iPrevLeaf = info.m_LeafCount - 1;
	int nDetailLeafCount = 0;
	LeafIndex_t *pDetailLeafList = (LeafIndex_t*)stackalloc( info.m_LeafCount * sizeof(LeafIndex_t) );

// 	bool bDrawUnderWater = (nFlags & DF_RENDER_UNDERWATER) != 0;
// 	bool bDrawAboveWater = (nFlags & DF_RENDER_ABOVEWATER) != 0;
// 	bool bDrawWater = (nFlags & DF_RENDER_WATER) != 0;
// 	bool bClipSkybox = (nFlags & DF_CLIP_SKYBOX ) != 0;
	unsigned long nEngineDrawFlags = BuildEngineDrawWorldListFlags( m_DrawFlags & ~DF_DRAWSKYBOX );

	DetailObjectSystem()->BeginTranslucentDetailRendering();

	if ( m_pMainView->ShouldDrawEntities() && r_drawtranslucentrenderables.GetBool() )
	{
		MDLCACHE_CRITICAL_SECTION();
		// Draw the particle singletons.
		DrawParticleSingletons( bInSkybox );

		int nTranslucentRenderableCount = m_pRenderablesList->m_RenderGroupCounts[RENDER_GROUP_TRANSLUCENT];
		CClientRenderablesList::CEntry *pEntities = m_pRenderablesList->m_RenderGroups[RENDER_GROUP_TRANSLUCENT];
		int iCurTranslucentEntity = nTranslucentRenderableCount - 1;

		int *pFastPathIndex = (int*)stackalloc( nTranslucentRenderableCount * sizeof( int ) );
		CUtlVector< ModelRenderSystemData_t > fastPathData( (ModelRenderSystemData_t *)stackalloc( nTranslucentRenderableCount * sizeof( ModelRenderSystemData_t ) ), nTranslucentRenderableCount );
		TranslucentInstanceRenderData_t *pRenderData = (TranslucentInstanceRenderData_t*)stackalloc( nTranslucentRenderableCount * sizeof( TranslucentInstanceRenderData_t ) );
		TranslucentTempData_t tempData;
		tempData.m_pColorMeshHandles = ( DataCacheHandle_t * )stackalloc( nTranslucentRenderableCount * sizeof( DataCacheHandle_t ) );
		bool bDoFastPath = cl_tlucfastpath.GetBool();
		bool bColorizeFastPath = cl_colorfastpath.GetBool();
		IMaterial *pFastPathColorMaterial = g_pModelRenderSystem->GetFastPathColorMaterial();
		if ( bDoFastPath )
		{
			for ( int i = 0; i < nTranslucentRenderableCount; ++i )
			{
				Assert( pEntities[i].m_pRenderable );
				IClientRenderable *pRenderable = pEntities[i].m_pRenderable;
				IClientUnknown *pUnknown = pRenderable->GetIClientUnknown();
				IClientModelRenderable *pModelRenderable = pUnknown->GetClientModelRenderable();
				if ( pModelRenderable )
				{
					int j = fastPathData.AddToTail( );
					ModelRenderSystemData_t &data = fastPathData[j];
					data.m_pRenderable = pRenderable;
					data.m_pModelRenderable = pModelRenderable;
					data.m_InstanceData = pEntities[i].m_InstanceData;
					pFastPathIndex[i] = j;
				}
				else
				{
					pFastPathIndex[i] = -1;
				}
			}

			g_pModelRenderSystem->ComputeTranslucentRenderData( fastPathData.Base(), fastPathData.Count(), pRenderData, &tempData );
		}

		bool bRenderingWaterRenderTargets = ( m_DrawFlags & ( DF_RENDER_REFRACTION | DF_RENDER_REFLECTION ) ) ? true : false;

		while( iCurTranslucentEntity >= 0 )
		{
			// Seek the current leaf up to our current translucent-entity leaf.
			int iThisLeaf = pEntities[iCurTranslucentEntity].m_iWorldListInfoLeaf;

			// First draw the translucent parts of the world up to and including those in this leaf
			DrawTranslucentWorldAndDetailPropsInLeaves( iPrevLeaf, iThisLeaf, nEngineDrawFlags, nDetailLeafCount, pDetailLeafList, bShadowDepth );

			// We're traversing the leaf list backwards to get the appropriate sort ordering (back to front)
			iPrevLeaf = iThisLeaf - 1;

			// Draw all the translucent entities with this leaf.
			int nLeaf = info.m_pLeafDataList[iThisLeaf].leafIndex;

			bool bDrawDetailProps = ClientLeafSystem()->ShouldDrawDetailObjectsInLeaf( nLeaf, m_pMainView->BuildWorldListsNumber() );
			if ( bDrawDetailProps )
			{
				// Draw detail props up to but not including this leaf
				Assert( nDetailLeafCount > 0 ); 
				--nDetailLeafCount;
				Assert( pDetailLeafList[nDetailLeafCount] == nLeaf );
				DetailObjectSystem()->RenderTranslucentDetailObjects( m_pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );

				// Draw translucent renderables in the leaf interspersed with detail props
				for( ;pEntities[iCurTranslucentEntity].m_iWorldListInfoLeaf == iThisLeaf && iCurTranslucentEntity >= 0; --iCurTranslucentEntity )
				{
					IClientRenderable *pRenderable = pEntities[iCurTranslucentEntity].m_pRenderable;

					// Draw any detail props in this leaf that's farther than the entity
					const Vector &vecRenderOrigin = pRenderable->GetRenderOrigin();
					DetailObjectSystem()->RenderTranslucentDetailObjectsInLeaf( m_pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nLeaf, &vecRenderOrigin );

					int nRenderFlags = pRenderable->GetRenderFlags();
					
					if ( ( nRenderFlags & ( ERENDERFLAGS_NEEDS_FULL_FB | ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB ) ) && !bShadowDepth )
					{
						if( bRenderingWaterRenderTargets )
						{
							continue;
						}

						UpdateNecessaryRenderTargets( nRenderFlags );
					}

					// Then draw the translucent renderable
					if ( bDoFastPath && pFastPathIndex[iCurTranslucentEntity] >= 0 )
					{
						if ( bColorizeFastPath )
						{
							g_pStudioRender->ForcedMaterialOverride( pFastPathColorMaterial );
						}
						TranslucentInstanceRenderData_t &renderData = pRenderData[ pFastPathIndex[iCurTranslucentEntity] ]; 

						int nDrawFlags = pEntities[iCurTranslucentEntity].m_TwoPass ? STUDIORENDER_DRAW_TRANSLUCENT_ONLY : STUDIORENDER_DRAW_ENTIRE_MODEL;
						g_pStudioRender->DrawModelArray( *renderData.m_pModelInfo, 1, renderData.m_pInstanceData, sizeof(StudioArrayInstanceData_t), nDrawFlags );
						g_pStudioRender->ForcedMaterialOverride( NULL );
					}
					else
					{
						DrawTranslucentRenderable( pRenderable, pEntities[iCurTranslucentEntity].m_InstanceData,
						(pEntities[iCurTranslucentEntity].m_TwoPass != 0), bShadowDepth );
					}
				}

				// Draw all remaining props in this leaf
				DetailObjectSystem()->RenderTranslucentDetailObjectsInLeaf( m_pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nLeaf, NULL );
			}
			else
			{
				// Draw queued up detail props (we know that the list of detail leaves won't include this leaf, since ShouldDrawDetailObjectsInLeaf is false)
				// Therefore no fixup on nDetailLeafCount is required as in the above section
				DetailObjectSystem()->RenderTranslucentDetailObjects( m_pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );

				for( ;pEntities[iCurTranslucentEntity].m_iWorldListInfoLeaf == iThisLeaf && iCurTranslucentEntity >= 0; --iCurTranslucentEntity )
				{
					IClientRenderable *pRenderable = pEntities[iCurTranslucentEntity].m_pRenderable;

					int nRenderFlags = pRenderable->GetRenderFlags();

					if ( ( nRenderFlags & ( ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB | ERENDERFLAGS_NEEDS_FULL_FB ) ) && !bShadowDepth )
					{
						if( bRenderingWaterRenderTargets )
						{
							continue;
						}
						UpdateNecessaryRenderTargets( nRenderFlags );
					}

					if ( bDoFastPath && pFastPathIndex[iCurTranslucentEntity] >= 0 )
					{
						if ( bColorizeFastPath )
						{
							g_pStudioRender->ForcedMaterialOverride( pFastPathColorMaterial );
						}
						TranslucentInstanceRenderData_t &renderData = pRenderData[ pFastPathIndex[iCurTranslucentEntity] ]; 
						int nDrawFlags = pEntities[iCurTranslucentEntity].m_TwoPass ? STUDIORENDER_DRAW_TRANSLUCENT_ONLY : STUDIORENDER_DRAW_ENTIRE_MODEL;
						g_pStudioRender->DrawModelArray( *renderData.m_pModelInfo, 1, renderData.m_pInstanceData, sizeof(StudioArrayInstanceData_t), nDrawFlags );
						g_pStudioRender->ForcedMaterialOverride( NULL );
					}
					else
					{
						DrawTranslucentRenderable( pRenderable, pEntities[iCurTranslucentEntity].m_InstanceData,
						(pEntities[iCurTranslucentEntity].m_TwoPass != 0), bShadowDepth );
					}
				}
			}

			nDetailLeafCount = 0;
		}

		if ( bDoFastPath )
		{
			g_pModelRenderSystem->CleanupTranslucentTempData( &tempData );
		}
	}

	// Draw the rest of the surfaces in world leaves
	DrawTranslucentWorldAndDetailPropsInLeaves( iPrevLeaf, 0, nEngineDrawFlags, nDetailLeafCount, pDetailLeafList, bShadowDepth );

	// Draw any queued-up detail props from previously visited leaves
	DetailObjectSystem()->RenderTranslucentDetailObjects( m_pRenderablesList->m_DetailFade, CurrentViewOrigin(), CurrentViewForward(), CurrentViewRight(), CurrentViewUp(), nDetailLeafCount, pDetailLeafList );

	// Reset the blend state.
	render->SetBlend( 1 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CRendering3dView::EnableWorldFog( void )
{
	VPROF("CViewRender::EnableWorldFog");
	CMatRenderContextPtr pRenderContext( materials );

	fogparams_t *pFogParams = NULL;
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if ( pbp )
	{
		pFogParams = pbp->GetFogParams();
	}

	if( GetFogEnable( pFogParams ) )
	{
		float fogColor[3];
		GetFogColor( pFogParams, fogColor );
		pRenderContext->FogMode( MATERIAL_FOG_LINEAR );
		pRenderContext->FogColor3fv( fogColor );
		pRenderContext->FogStart( GetFogStart( pFogParams ) );
		pRenderContext->FogEnd( GetFogEnd( pFogParams ) );
		pRenderContext->FogMaxDensity( GetFogMaxDensity( pFogParams ) );
	}
	else
	{
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CRendering3dView::GetDrawFlags()
{
	return m_DrawFlags;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CRendering3dView::SetFogVolumeState( const VisibleFogVolumeInfo_t &fogInfo, bool bUseHeightFog )
{
	render->SetFogVolumeState( fogInfo.m_nVisibleFogVolume, bUseHeightFog );


}


//-----------------------------------------------------------------------------
// Standard 3d skybox view
//-----------------------------------------------------------------------------
SkyboxVisibility_t CSkyboxView::ComputeSkyboxVisibility()
{
	return engine->IsSkyboxVisibleFromPoint( origin );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CSkyboxView::GetSkyboxFogEnable()
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return false;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	if( fog_override.GetInt() )
	{
		if( fog_enableskybox.GetInt() )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return !!local->m_skybox3d.fog.enable;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxView::Enable3dSkyboxFog( void )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	CMatRenderContextPtr pRenderContext( materials );

	if( GetSkyboxFogEnable() )
	{
		float fogColor[3];
		GetSkyboxFogColor( fogColor );
		float scale = 1.0f;
		if ( local->m_skybox3d.scale > 0.0f )
		{
			scale = 1.0f / local->m_skybox3d.scale;
		}
		pRenderContext->FogMode( MATERIAL_FOG_LINEAR );
		pRenderContext->FogColor3fv( fogColor );
		pRenderContext->FogStart( GetSkyboxFogStart() * scale );
		pRenderContext->FogEnd( GetSkyboxFogEnd() * scale );
		pRenderContext->FogMaxDensity( GetSkyboxFogMaxDensity() );
	}
	else
	{
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
sky3dparams_t *CSkyboxView::PreRender3dSkyboxWorld( SkyboxVisibility_t nSkyboxVisible )
{
	if ( ( nSkyboxVisible != SKYBOX_3DSKYBOX_VISIBLE ) && r_3dsky.GetInt() != 2 )
		return NULL;

	// render the 3D skybox
	if ( !r_3dsky.GetInt() )
		return NULL;

	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();

	// No local player object yet...
	if ( !pbp )
		return NULL;

	CPlayerLocalData* local = &pbp->m_Local;
	if ( local->m_skybox3d.area == 255 )
		return NULL;

	return &local->m_skybox3d;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxView::DrawInternal( view_id_t iSkyBoxViewID, bool bInvokePreAndPostRender, ITexture *pRenderTarget )
{
	unsigned char **areabits = render->GetAreaBits();
	unsigned char *savebits;
	unsigned char tmpbits[ 32 ];
	savebits = *areabits;
	memset( tmpbits, 0, sizeof(tmpbits) );

	// set the sky area bit
	tmpbits[m_pSky3dParams->area>>3] |= 1 << (m_pSky3dParams->area&7);

	*areabits = tmpbits;

	// if you can get really close to the skybox geometry it's possible that you'll be able to clip into it
	// with this near plane.  If so, move it in a bit.  It's at 2.0 to give us more precision.  That means you 
	// need to keep the eye position at least 2 * scale away from the geometry in the skybox
	zNear = 2.0;
	zFar = MAX_TRACE_LENGTH;

	// scale origin by sky scale and translate to sky origin
	{
		float scale = (m_pSky3dParams->scale > 0) ? (1.0f / m_pSky3dParams->scale) : 1.0f;
		Vector vSkyOrigin = m_pSky3dParams->origin;
		VectorScale( origin, scale, origin );
		VectorAdd( origin, vSkyOrigin, origin );

		if( m_bCustomViewMatrix )
		{
			Vector vTransformedSkyOrigin;
			VectorRotate( vSkyOrigin, m_matCustomViewMatrix, vTransformedSkyOrigin ); //Rotate instead of transform because we haven't scale the existing offset yet

			//scale existing translation, and tack on the skybox offset (subtract because it's a view matrix)
			m_matCustomViewMatrix.m_flMatVal[0][3] = (m_matCustomViewMatrix.m_flMatVal[0][3] * scale) - vTransformedSkyOrigin.x;
			m_matCustomViewMatrix.m_flMatVal[1][3] = (m_matCustomViewMatrix.m_flMatVal[1][3] * scale) - vTransformedSkyOrigin.y;
			m_matCustomViewMatrix.m_flMatVal[2][3] = (m_matCustomViewMatrix.m_flMatVal[2][3] * scale) - vTransformedSkyOrigin.z;
		}
	}

	Enable3dSkyboxFog();

	// BUGBUG: Fix this!!!  We shouldn't need to call setup vis for the sky if we're connecting
	// the areas.  We'd have to mark all the clusters in the skybox area in the PVS of any 
	// cluster with sky.  Then we could just connect the areas to do our vis.
	//m_bOverrideVisOrigin could hose us here, so call direct
	render->ViewSetupVis( false, 1, &m_pSky3dParams->origin.Get() );
	render->Push3DView( (*this), m_ClearFlags, pRenderTarget, GetFrustum() );

	// Store off view origin and angles
	SetupCurrentView( origin, angles, iSkyBoxViewID );

#if defined( _X360 )
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushVertexShaderGPRAllocation( 32 );
	pRenderContext.SafeRelease();
#endif

	// Invoke pre-render methods
	if ( bInvokePreAndPostRender )
	{
		IGameSystem::PreRenderAllSystems();
	}

	render->BeginUpdateLightmaps();
	BuildWorldRenderLists( true, -1, true );
	BuildRenderableRenderLists( iSkyBoxViewID );
	render->EndUpdateLightmaps();

	// FIXME: Don't do deferred shadows on 3D skybox for now.
	if ( !r_shadow_deferred.GetBool() )
	{
		g_pClientShadowMgr->ComputeShadowTextures( (*this), m_pWorldListInfo->m_LeafCount, m_pWorldListInfo->m_pLeafDataList );
	}

	DrawWorld( 0.0f );

	// Iterate over all leaves and render objects in those leaves
	DrawOpaqueRenderables( false );

	// Iterate over all leaves and render objects in those leaves
	DrawTranslucentRenderables( true, false );
	DrawNoZBufferTranslucentRenderables();

	m_pMainView->DisableFog();

	CGlowOverlay::UpdateSkyOverlays( zFar, m_bCacheFullSceneState );

	PixelVisibility_EndCurrentView();

	// restore old area bits
	*areabits = savebits;

	// Invoke post-render methods
	if( bInvokePreAndPostRender )
	{
		IGameSystem::PostRenderAllSystems();
		FinishCurrentView();
	}

	// FIXME: Workaround to 3d skybox not depth-of-fielding properly. The real fix is for the 3d skybox dest alpha depth values
	// to be biased. Currently all I do is clear alpha to 1 after the 3D skybox path. This avoids the skybox being unblurred.
	if( IsDepthOfFieldEnabled() )
	{
		// draw a fullscreen quad setting destalpha to 1

		IMaterial *pMat = materials->FindMaterial( "dev/clearalpha", TEXTURE_GROUP_OTHER, true );
		if ( pMat != NULL )
		{
			int nWidth = 0;
			int nHeight = 0;
			int nDummy = 0;
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->GetViewport( nDummy, nDummy, nWidth, nHeight );

			pRenderContext->DrawScreenSpaceRectangle(
				pMat,
				0, 0, nWidth, nHeight,
				0, 0, nWidth-1, nHeight-1,
				nWidth, nHeight, NULL /*GetClientWorldEntity()->GetClientRenderable()*/ );
		}
	}

	render->PopView( GetFrustum() );

#if defined( _X360 )
	pRenderContext.GetFrom( materials );
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CSkyboxView::Setup( const CViewSetup &view, int *pClearFlags, SkyboxVisibility_t *pSkyboxVisible )
{
	BaseClass::Setup( view );

	// The skybox might not be visible from here
	*pSkyboxVisible = ComputeSkyboxVisibility();
	m_pSky3dParams = PreRender3dSkyboxWorld( *pSkyboxVisible );

	if ( !m_pSky3dParams )
	{
		return false;
	}

	// At this point, we've cleared everything we need to clear
	// The next path will need to clear depth, though.
	m_ClearFlags = *pClearFlags;
	*pClearFlags &= ~( VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL | VIEW_CLEAR_FULL_TARGET );
	*pClearFlags |= VIEW_CLEAR_DEPTH; // Need to clear depth after rednering the skybox

	m_DrawFlags = DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER | DF_RENDER_WATER;
	if( r_skybox.GetBool() )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CSkyboxView::Draw()
{
	VPROF_BUDGET( "CViewRender::Draw3dSkyboxworld", "3D Skybox" );

	DrawInternal();
}









//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CShadowDepthView::Setup( const CViewSetup &shadowViewIn, ITexture *pRenderTarget, ITexture *pDepthTexture )
{
	BaseClass::Setup( shadowViewIn );
	m_pRenderTarget = pRenderTarget;
	m_pDepthTexture = pDepthTexture;
}


bool DrawingShadowDepthView( void ) //for easy externing
{
	return (CurrentViewID() == VIEW_SHADOW_DEPTH_TEXTURE);
}
						  
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CShadowDepthView::Draw()
{
	VPROF_BUDGET( "CShadowDepthView::Draw", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	// Start view
	unsigned int visFlags;
	m_pMainView->SetupVis( (*this), visFlags );  // @MULTICORE (toml 8/9/2006): Portal problem, not sending custom vis down

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->ClearColor3ub(0xFF, 0xFF, 0xFF);

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 112 ); //almost all work is done in vertex shaders for depth rendering, max out their threads
#endif

	pRenderContext.SafeRelease();

	if( IsPC() )
	{
		render->Push3DView( (*this), VIEW_CLEAR_DEPTH, m_pRenderTarget, GetFrustum(), m_pDepthTexture );
	}
	else if( IsX360() )
	{
		//for the 360, the dummy render target has a separate depth buffer which we Resolve() from afterward
		render->Push3DView( (*this), VIEW_CLEAR_DEPTH, m_pRenderTarget, GetFrustum() );
	}

	SetupCurrentView( origin, angles, VIEW_SHADOW_DEPTH_TEXTURE );

	MDLCACHE_CRITICAL_SECTION();

	{
		VPROF_BUDGET( "BuildWorldRenderLists", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
		BuildWorldRenderLists( true, -1, true, true ); // @MULTICORE (toml 8/9/2006): Portal problem, not sending custom vis down
	}

	{
		VPROF_BUDGET( "BuildRenderableRenderLists", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
		BuildRenderableRenderLists( CurrentViewID() );
	}

	engine->Sound_ExtraUpdate();	// Make sure sound doesn't stutter

	m_DrawFlags = m_pMainView->GetBaseDrawFlags() | DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER | DF_SHADOW_DEPTH_MAP;	// Don't draw water surface...

	{
		VPROF_BUDGET( "DrawWorld", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
		DrawWorld( 0.0f );
	}

	// Draw opaque and translucent renderables with appropriate override materials
	// OVERRIDE_DEPTH_WRITE is OK with a NULL material pointer
	modelrender->ForcedMaterialOverride( NULL, OVERRIDE_DEPTH_WRITE );	

	{
		VPROF_BUDGET( "DrawOpaqueRenderables", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
		DrawOpaqueRenderables( true );
	}

	if ( m_bRenderFlashlightDepthTranslucents || r_flashlightdepth_drawtranslucents.GetBool() )
	{
		VPROF_BUDGET( "DrawTranslucentRenderables", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
		DrawTranslucentRenderables( false, true );
	}

	modelrender->ForcedMaterialOverride( 0 );

	m_DrawFlags = 0;

	pRenderContext.GetFrom( materials );

	if( IsX360() )
	{
		//Resolve() the depth texture here. Before the pop so the copy will recognize that the resolutions are the same
		pRenderContext->CopyRenderTargetToTextureEx( m_pDepthTexture, -1, NULL, NULL );
	}

	render->PopView( GetFrustum() );

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CFreezeFrameView::Setup( const CViewSetup &shadowViewIn )
{
	BaseClass::Setup( shadowViewIn );

	VGui_GetTrueScreenSize( m_nScreenSize[ 0 ], m_nScreenSize[ 1 ] );
	VGui_GetPanelBounds( GET_ACTIVE_SPLITSCREEN_SLOT(), m_nSubRect[ 0 ], m_nSubRect[ 1 ], m_nSubRect[ 2 ], m_nSubRect[ 3 ] );

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", IsX360() ? "_rt_FullFrameFB1" : "_rt_FullScreen" );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$nofog", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	m_pFreezeFrame.Init( "FreezeFrame_FullScreen", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pFreezeFrame->Refresh();

	m_TranslucentSingleColor.Init( "debug/debugtranslucentsinglecolor", TEXTURE_GROUP_OTHER );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFreezeFrameView::Draw( void )
{
	CMatRenderContextPtr pRenderContext( materials );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 16 ); //max out pixel shader threads
#endif

	pRenderContext->DrawScreenSpaceRectangle( m_pFreezeFrame, x, y, width, height,
		m_nSubRect[ 0 ], m_nSubRect[ 1 ], m_nSubRect[ 0 ] + m_nSubRect[ 2 ] - 1, m_nSubRect[ 1 ] + m_nSubRect[ 3 ] - 1, m_nScreenSize[ 0 ], m_nScreenSize[ 1 ] );

	//Fake a fade during freezeframe view.
	if ( g_flFreezeFlash[ m_nSlot ] >= gpGlobals->curtime && 
		engine->IsTakingScreenshot() == false )
	{
		// Overlay screen fade on entire screen
		IMaterial* pMaterial = m_TranslucentSingleColor;

		int iFadeAlpha = FREEZECAM_SNAPSHOT_FADE_SPEED * ( g_flFreezeFlash[ m_nSlot ] - gpGlobals->curtime );
		
		iFadeAlpha = MIN( iFadeAlpha, 255 );
		iFadeAlpha = MAX( 0, iFadeAlpha );
		
		pMaterial->AlphaModulate( iFadeAlpha * ( 1.0f / 255.0f ) );
		pMaterial->ColorModulate( 1.0f,	1.0f, 1.0f );
		pMaterial->SetMaterialVarFlag( MATERIAL_VAR_IGNOREZ, true );

		pRenderContext->DrawScreenSpaceRectangle( pMaterial, x, y, width, height, m_nSubRect[ 0 ], m_nSubRect[ 1 ], m_nSubRect[ 0 ] + m_nSubRect[ 2 ] - 1, m_nSubRect[ 1 ] + m_nSubRect[ 3 ] - 1, m_nScreenSize[ 0 ], m_nScreenSize[ 1 ] );
	}

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
bool CBaseWorldView::AdjustView( float waterHeight )
{
	if( m_DrawFlags & DF_RENDER_REFRACTION )
	{
		ITexture *pTexture = GetWaterRefractionTexture();

		// Use the aspect ratio of the main view! So, don't recompute it here
		x = y = 0;
		width = pTexture->GetActualWidth();
		height = pTexture->GetActualHeight();

		return true;
	}

	if( m_DrawFlags & DF_RENDER_REFLECTION )
	{
		ITexture *pTexture = GetWaterReflectionTexture();

		// Use the aspect ratio of the main view! So, don't recompute it here
		x = y = 0;
		width = pTexture->GetActualWidth();
		height = pTexture->GetActualHeight();
		angles[0] = -angles[0];
		angles[2] = -angles[2];
		origin[2] -= 2.0f * ( origin[2] - (waterHeight));
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
void CBaseWorldView::PushView( float waterHeight )
{
	float spread = 2.0f;
	if( m_DrawFlags & DF_FUDGE_UP )
	{
		waterHeight += spread;
	}
	else
	{
		waterHeight -= spread;
	}

	MaterialHeightClipMode_t clipMode = MATERIAL_HEIGHTCLIPMODE_DISABLE;
	if ( ( m_DrawFlags & DF_CLIP_Z ) && mat_clipz.GetBool() )
	{
		if( m_DrawFlags & DF_CLIP_BELOW )
		{
			clipMode = MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT;
		}
		else
		{
			clipMode = MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT;
		}
	}

	CMatRenderContextPtr pRenderContext( materials );

	if( m_DrawFlags & DF_RENDER_REFRACTION )
	{
		pRenderContext->SetFogZ( waterHeight );
		pRenderContext->SetHeightClipZ( waterHeight );
		pRenderContext->SetHeightClipMode( clipMode );

		// Have to re-set up the view since we reset the size
		render->Push3DView( *this, m_ClearFlags, GetWaterRefractionTexture(), GetFrustum() );

		return;
	}

	if( m_DrawFlags & DF_RENDER_REFLECTION )
	{
		ITexture *pTexture = GetWaterReflectionTexture();

		pRenderContext->SetFogZ( waterHeight );

		bool bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();
		if( bSoftwareUserClipPlane && ( origin[2] > waterHeight - r_eyewaterepsilon.GetFloat() ) )
		{
			waterHeight = origin[2] + r_eyewaterepsilon.GetFloat();
		}

		pRenderContext->SetHeightClipZ( waterHeight );
		pRenderContext->SetHeightClipMode( clipMode );

		render->Push3DView( *this, m_ClearFlags, pTexture, GetFrustum() );

		SetLightmapScaleForWater();
		return;
	}

	if ( m_ClearFlags & ( VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR | VIEW_CLEAR_STENCIL ) )
	{
		if ( m_ClearFlags & VIEW_CLEAR_OBEY_STENCIL )
		{
			pRenderContext->ClearBuffersObeyStencil( ( m_ClearFlags & VIEW_CLEAR_COLOR ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_DEPTH ) ? true : false );
		}
		else
		{
			if ( r_shadow_deferred.GetBool() )
			{
				// FIXME: Is there a better place to force a stencil clear for deferred shadows?
				if ( m_ClearFlags & VIEW_CLEAR_DEPTH )
					m_ClearFlags |= VIEW_CLEAR_STENCIL;
			}

			pRenderContext->ClearBuffers( ( m_ClearFlags & VIEW_CLEAR_COLOR ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_DEPTH ) ? true : false, ( m_ClearFlags & VIEW_CLEAR_STENCIL ) ? true : false );
		}
	}

	pRenderContext->SetHeightClipMode( clipMode );
	if ( clipMode != MATERIAL_HEIGHTCLIPMODE_DISABLE )
	{   
		pRenderContext->SetHeightClipZ( waterHeight );
	}
}


//-----------------------------------------------------------------------------
// Pops a water render target
//-----------------------------------------------------------------------------
void CBaseWorldView::PopView()
{
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
	if( m_DrawFlags & (DF_RENDER_REFRACTION | DF_RENDER_REFLECTION) )
	{
		if ( IsX360() )
		{
			// these renders paths used their surfaces, so blit their results
			if ( m_DrawFlags & DF_RENDER_REFRACTION )
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetWaterRefractionTexture(), NULL, NULL );
			}
			if ( m_DrawFlags & DF_RENDER_REFLECTION )
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetWaterReflectionTexture(), NULL, NULL );
			}
		}

		render->PopView( GetFrustum() );
		if (s_vSavedLinearLightMapScale.x>=0)
		{
			pRenderContext->SetToneMappingScaleLinear(s_vSavedLinearLightMapScale);
			s_vSavedLinearLightMapScale.x=-1;
		}
	}
}


//-----------------------------------------------------------------------------
// Draws the world + entities
//-----------------------------------------------------------------------------
void CBaseWorldView::DrawSetup( float waterHeight, int nSetupFlags, float waterZAdjust, int iForceViewLeaf )
{
	int savedViewID = g_CurrentViewID;
	g_CurrentViewID = VIEW_ILLEGAL;

	bool bViewChanged = AdjustView( waterHeight );

	if ( bViewChanged )
	{
		render->Push3DView( *this, 0, NULL, GetFrustum() );
	}

	render->BeginUpdateLightmaps();

	bool bDrawEntities = ( nSetupFlags & DF_DRAW_ENTITITES ) != 0;
	bool bDrawReflection = ( nSetupFlags & DF_RENDER_REFLECTION ) != 0;
	BuildWorldRenderLists( bDrawEntities, iForceViewLeaf, true, false, bDrawReflection ? &waterHeight : NULL );

	PruneWorldListInfo();

	if ( bDrawEntities )
	{
		BuildRenderableRenderLists( savedViewID );
	}

	render->EndUpdateLightmaps();

	if ( bViewChanged )
	{
		render->PopView( GetFrustum() );
	}

	g_CurrentViewID = savedViewID;
}


void CBaseWorldView::DrawExecute( float waterHeight, view_id_t viewID, float waterZAdjust )
{
	// @MULTICORE (toml 8/16/2006): rethink how, where, and when this is done...
	g_pClientShadowMgr->ComputeShadowTextures( *this, m_pWorldListInfo->m_LeafCount, m_pWorldListInfo->m_pLeafDataList );

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	int savedViewID = g_CurrentViewID;
	g_CurrentViewID = viewID;

	// Update our render view flags.
	int iDrawFlagsBackup = m_DrawFlags;
	m_DrawFlags |= m_pMainView->GetBaseDrawFlags();

	PushView( waterHeight );

	CMatRenderContextPtr pRenderContext( materials );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 );
#endif

	ITexture *pSaveFrameBufferCopyTexture = pRenderContext->GetFrameBufferCopyTexture( 0 );
	pRenderContext->SetFrameBufferCopyTexture( GetPowerOfTwoFrameBufferTexture() );
	pRenderContext.SafeRelease();

	Begin360ZPass();
	m_DrawFlags |= DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
	DrawWorld( waterZAdjust );
	m_DrawFlags &= ~DF_SKIP_WORLD_DECALS_AND_OVERLAYS;
	if ( m_DrawFlags & DF_DRAW_ENTITITES )
	{
		DrawOpaqueRenderables( false );
	}
	End360ZPass();		// DrawOpaqueRenderables currently already calls End360ZPass. No harm in calling it again to make sure we're always ending it

	// Only draw decals on opaque surfaces after now. Benefit is two-fold: Early Z benefits on PC, and
	// we're pulling out stuff that uses the dynamic VB from the 360 Z pass
	// (which can lead to rendering corruption if we overflow the dyn. VB ring buffer).
	m_DrawFlags |= DF_SKIP_WORLD;
	DrawWorld( waterZAdjust );
	m_DrawFlags &= ~DF_SKIP_WORLD;
		
	if ( !m_bDrawWorldNormal )
	{
		if ( m_DrawFlags & DF_DRAW_ENTITITES )
		{
			DrawTranslucentRenderables( false, false );
			DrawNoZBufferTranslucentRenderables();
		}
		else
		{
			// Draw translucent world brushes only, no entities
			DrawTranslucentWorldInLeaves( false );
		}
	}

	// issue the pixel visibility tests for sub-views
	if ( !IsMainView( CurrentViewID() ) && CurrentViewID() != VIEW_INTRO_CAMERA )
	{
		PixelVisibility_EndCurrentView();
	}

	pRenderContext.GetFrom( materials );
	pRenderContext->SetFrameBufferCopyTexture( pSaveFrameBufferCopyTexture );
	PopView();

	m_DrawFlags = iDrawFlagsBackup;

	g_CurrentViewID = savedViewID;

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
void CSimpleWorldView::Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, ViewCustomVisibility_t *pCustomVisibility )
{
	BaseClass::Setup( view );

	m_ClearFlags = nClearFlags;
	m_DrawFlags = DF_DRAW_ENTITITES;

	if ( !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
	}
	else
	{
		bool bViewIntersectsWater = DoesViewPlaneIntersectWater( fogInfo.m_flWaterHeight, fogInfo.m_nVisibleFogVolume );
		if( bViewIntersectsWater )
		{
			// have to draw both sides if we can see both.
			m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
		}
		else if ( fogInfo.m_bEyeInFogVolume )
		{
			m_DrawFlags |= DF_RENDER_UNDERWATER;
		}
		else
		{
			m_DrawFlags |= DF_RENDER_ABOVEWATER;
		}
	}
	if ( waterInfo.m_bDrawWaterSurface )
	{
		m_DrawFlags |= DF_RENDER_WATER;
	}

	if ( !fogInfo.m_bEyeInFogVolume && bDrawSkybox )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	m_pCustomVisibility = pCustomVisibility;
	m_fogInfo = fogInfo;
}


//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
void CSimpleWorldView::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_NoWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CSimpleWorldView::Draw" );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 ); //lean toward pixel shader threads
#endif

	pRenderContext.SafeRelease();

	DrawSetup( 0, m_DrawFlags, 0 );

	if ( !m_fogInfo.m_bEyeInFogVolume )
	{
		EnableWorldFog();
	}
	else
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;

		SetFogVolumeState( m_fogInfo, false );

		pRenderContext.GetFrom( materials );

		unsigned char ucFogColor[3];
		pRenderContext->GetFogColor( ucFogColor );
		pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
	}

	pRenderContext.SafeRelease();

	DrawExecute( 0, CurrentViewID(), 0 );

	pRenderContext.GetFrom( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseWaterView::CalcWaterEyeAdjustments( const VisibleFogVolumeInfo_t &fogInfo,
											 float &newWaterHeight, float &waterZAdjust, bool bSoftwareUserClipPlane )
{
	if( !bSoftwareUserClipPlane )
	{
		newWaterHeight = fogInfo.m_flWaterHeight;
		waterZAdjust = 0.0f;
		return;
	}

	newWaterHeight = fogInfo.m_flWaterHeight;
	float eyeToWaterZDelta = origin[2] - fogInfo.m_flWaterHeight;
	float epsilon = r_eyewaterepsilon.GetFloat();
	waterZAdjust = 0.0f;
	if( fabs( eyeToWaterZDelta ) < epsilon )
	{
		if( eyeToWaterZDelta > 0 )
		{
			newWaterHeight = origin[2] - epsilon;
		}
		else
		{
			newWaterHeight = origin[2] + epsilon;
		}
		waterZAdjust = newWaterHeight - fogInfo.m_flWaterHeight;
	}

	//	Warning( "view.origin[2]: %f newWaterHeight: %f fogInfo.m_flWaterHeight: %f waterZAdjust: %f\n", 
	//		( float )view.origin[2], newWaterHeight, fogInfo.m_flWaterHeight, waterZAdjust );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseWaterView::CSoftwareIntersectionView::Setup( bool bAboveWater )
{
	BaseClass::Setup( *GetOuter() );

	m_DrawFlags = 0;
	m_DrawFlags = ( bAboveWater ) ? DF_RENDER_UNDERWATER : DF_RENDER_ABOVEWATER;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CBaseWaterView::CSoftwareIntersectionView::Draw()
{
	DrawSetup( GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );
	DrawExecute( GetOuter()->m_waterHeight, CurrentViewID(), GetOuter()->m_waterZAdjust );
}

//-----------------------------------------------------------------------------
// Draws the scene when the view point is above the level of the water
//-----------------------------------------------------------------------------
void CAboveWaterView::Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo )
{
	BaseClass::Setup( view );

	m_bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();

	CalcWaterEyeAdjustments( fogInfo, m_waterHeight, m_waterZAdjust, m_bSoftwareUserClipPlane );

	// BROKEN STUFF!
	if ( m_waterZAdjust == 0.0f )
	{
		m_bSoftwareUserClipPlane = false;
	}

	m_DrawFlags = DF_RENDER_ABOVEWATER | DF_DRAW_ENTITITES;
	m_ClearFlags = VIEW_CLEAR_DEPTH;



	if ( bDrawSkybox )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	if ( waterInfo.m_bDrawWaterSurface )
	{
		m_DrawFlags |= DF_RENDER_WATER;
	}
	if ( !waterInfo.m_bRefract && !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_UNDERWATER;
	}

	m_fogInfo = fogInfo;
	m_waterInfo = waterInfo;
}

		 
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_EyeAboveWater" );

	// eye is outside of water
	
	CMatRenderContextPtr pRenderContext( materials );
	
	// render the reflection
	if( m_waterInfo.m_bReflect )
	{
		m_ReflectionView.Setup( m_waterInfo.m_bReflectEntities );
		m_pMainView->AddViewToScene( &m_ReflectionView );
	}
	
	bool bViewIntersectsWater = false;

	// render refraction
	if ( m_waterInfo.m_bRefract )
	{
		m_RefractionView.Setup();
		m_pMainView->AddViewToScene( &m_RefractionView );

		if( !m_bSoftwareUserClipPlane )
		{
			bViewIntersectsWater = DoesViewPlaneIntersectWater( m_fogInfo.m_flWaterHeight, m_fogInfo.m_nVisibleFogVolume );
		}
	}
	else if ( !( m_DrawFlags & DF_DRAWSKYBOX ) )
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;
	}



	// NOTE!!!!!  YOU CAN ONLY DO THIS IF YOU HAVE HARDWARE USER CLIP PLANES!!!!!!
	bool bHardwareUserClipPlanes = !g_pMaterialSystemHardwareConfig->UseFastClipping();
	if( bViewIntersectsWater && bHardwareUserClipPlanes )
	{
		// This is necessary to keep the non-water fogged world from drawing underwater in 
		// the case where we want to partially see into the water.
		m_DrawFlags |= DF_CLIP_Z | DF_CLIP_BELOW;
	}

	// render the world
	DrawSetup( m_waterHeight, m_DrawFlags, m_waterZAdjust );
	EnableWorldFog();
	DrawExecute( m_waterHeight, CurrentViewID(), m_waterZAdjust );

	if ( m_waterInfo.m_bRefract )
	{
		if ( m_bSoftwareUserClipPlane )
		{
			m_SoftwareIntersectionView.Setup( true );
			m_SoftwareIntersectionView.Draw( );
		}
		else if ( bViewIntersectsWater )
		{
			m_IntersectionView.Setup();
			m_pMainView->AddViewToScene( &m_IntersectionView );
		}
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CReflectionView::Setup( bool bReflectEntities )
{
	BaseClass::Setup( *GetOuter() );

	m_ClearFlags = VIEW_CLEAR_DEPTH;

	// NOTE: Clearing the color is unnecessary since we're drawing the skybox
	// and dest-alpha is never used in the reflection
	m_DrawFlags = DF_RENDER_REFLECTION | DF_CLIP_Z | DF_CLIP_BELOW | 
		DF_RENDER_ABOVEWATER;

	// NOTE: This will cause us to draw the 2d skybox in the reflection 
	// (which we want to do instead of drawing the 3d skybox)
	m_DrawFlags |= DF_DRAWSKYBOX;

	if( bReflectEntities )
	{
		m_DrawFlags |= DF_DRAW_ENTITITES;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CReflectionView::Draw()
{


	// Store off view origin and angles and set the new view
	int nSaveViewID = CurrentViewID();
	SetupCurrentView( origin, angles, VIEW_REFLECTION );

	// Disable occlusion visualization in reflection
	bool bVisOcclusion = r_visocclusion.GetBool();
	r_visocclusion.SetValue( 0 );

	DrawSetup( GetOuter()->m_fogInfo.m_flWaterHeight, m_DrawFlags, 0.0f, GetOuter()->m_fogInfo.m_nVisibleFogVolumeLeaf );

	EnableWorldFog();
	DrawExecute( GetOuter()->m_fogInfo.m_flWaterHeight, VIEW_REFLECTION, 0.0f );

	r_visocclusion.SetValue( bVisOcclusion );
	


	// finish off the view and restore the previous view.
	SetupCurrentView( origin, angles, ( view_id_t )nSaveViewID );

	// This is here for multithreading
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Flush();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CRefractionView::Setup()
{
	BaseClass::Setup( *GetOuter() );

	m_ClearFlags = VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH;

	m_DrawFlags = DF_RENDER_REFRACTION | DF_CLIP_Z | 
		DF_RENDER_UNDERWATER | DF_FUDGE_UP | 
		DF_DRAW_ENTITITES ;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CRefractionView::Draw()
{


	// Store off view origin and angles and set the new view
	int nSaveViewID = CurrentViewID();
	SetupCurrentView( origin, angles, VIEW_REFRACTION );

	DrawSetup( GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );

	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	SetClearColorToFogColor();
	DrawExecute( GetOuter()->m_waterHeight, VIEW_REFRACTION, GetOuter()->m_waterZAdjust );



	// finish off the view.  restore the previous view.
	SetupCurrentView( origin, angles, ( view_id_t )nSaveViewID );

	// This is here for multithreading
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->Flush();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CIntersectionView::Setup()
{
	BaseClass::Setup( *GetOuter() );
	m_DrawFlags = DF_RENDER_UNDERWATER | DF_CLIP_Z | DF_DRAW_ENTITITES;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CAboveWaterView::CIntersectionView::Draw()
{
	DrawSetup( GetOuter()->m_fogInfo.m_flWaterHeight, m_DrawFlags, 0 );

	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	SetClearColorToFogColor( );
	DrawExecute( GetOuter()->m_fogInfo.m_flWaterHeight, VIEW_NONE, 0 );
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
}


//-----------------------------------------------------------------------------
// Draws the scene when the view point is under the level of the water
//-----------------------------------------------------------------------------
void CUnderWaterView::Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo )
{
	BaseClass::Setup( view );

	m_bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();

	CalcWaterEyeAdjustments( fogInfo, m_waterHeight, m_waterZAdjust, m_bSoftwareUserClipPlane );

	IMaterial *pWaterMaterial = fogInfo.m_pFogVolumeMaterial;
		IMaterialVar *pScreenOverlayVar = pWaterMaterial->FindVar( "$underwateroverlay", NULL, false );
		if ( pScreenOverlayVar && ( pScreenOverlayVar->IsDefined() ) )
		{
			char const *pOverlayName = pScreenOverlayVar->GetStringValue();
			if ( pOverlayName[0] != '0' )						// fixme!!!
			{
				IMaterial *pOverlayMaterial = materials->FindMaterial( pOverlayName,  TEXTURE_GROUP_OTHER );
				m_pMainView->SetWaterOverlayMaterial( pOverlayMaterial );
			}
		}
	// NOTE: We're not drawing the 2d skybox under water since it's assumed to not be visible.

	// render the world underwater
	// Clear the color to get the appropriate underwater fog color
	m_DrawFlags = DF_FUDGE_UP | DF_RENDER_UNDERWATER | DF_DRAW_ENTITITES;
	m_ClearFlags = VIEW_CLEAR_DEPTH;

	if( !m_bSoftwareUserClipPlane )
	{
		m_DrawFlags |= DF_CLIP_Z;
	}
	if ( waterInfo.m_bDrawWaterSurface )
	{
		m_DrawFlags |= DF_RENDER_WATER;
	}
	if ( !waterInfo.m_bRefract && !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_ABOVEWATER;
	}

	m_fogInfo = fogInfo;
	m_waterInfo = waterInfo;
	m_bDrawSkybox = bDrawSkybox;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CUnderWaterView::Draw()
{
	// FIXME: The 3d skybox shouldn't be drawn when the eye is under water

	VPROF( "CViewRender::ViewDrawScene_EyeUnderWater" );

	CMatRenderContextPtr pRenderContext( materials );

	// render refraction (out of water)
	if ( m_waterInfo.m_bRefract )
	{
		m_RefractionView.Setup( );
		m_pMainView->AddViewToScene( &m_RefractionView );
	}

	if ( !m_waterInfo.m_bRefract )
	{
		SetFogVolumeState( m_fogInfo, true );
		unsigned char ucFogColor[3];
		pRenderContext->GetFogColor( ucFogColor );
		pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
	}

	DrawSetup( m_waterHeight, m_DrawFlags, m_waterZAdjust );
	SetFogVolumeState( m_fogInfo, false );
	DrawExecute( m_waterHeight, CurrentViewID(), m_waterZAdjust );
	m_ClearFlags = 0;

	if( m_waterZAdjust != 0.0f && m_bSoftwareUserClipPlane && m_waterInfo.m_bRefract )
	{
		m_SoftwareIntersectionView.Setup( false );
		m_SoftwareIntersectionView.Draw( );
	}
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

}



//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CUnderWaterView::CRefractionView::Setup()
{
	BaseClass::Setup( *GetOuter() );
	// NOTE: Refraction renders into the back buffer, over the top of the 3D skybox
	// It is then blitted out into the refraction target. This is so that
	// we only have to set up 3d sky vis once, and only render it once also!
	m_DrawFlags = DF_CLIP_Z | 
		DF_CLIP_BELOW | DF_RENDER_ABOVEWATER | 
		DF_DRAW_ENTITITES;

	m_ClearFlags = VIEW_CLEAR_DEPTH;
	if ( GetOuter()->m_bDrawSkybox )
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;
		m_DrawFlags |= DF_DRAWSKYBOX | DF_CLIP_SKYBOX;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CUnderWaterView::CRefractionView::Draw()
{
	CMatRenderContextPtr pRenderContext( materials );
	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	unsigned char ucFogColor[3];
	pRenderContext->GetFogColor( ucFogColor );
	pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );

	DrawSetup( GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );

	EnableWorldFog();
	DrawExecute( GetOuter()->m_waterHeight, VIEW_REFRACTION, GetOuter()->m_waterZAdjust );

	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = width;
	srcRect.height = height;

	ITexture *pTexture = GetWaterRefractionTexture();
	pRenderContext->CopyRenderTargetToTextureEx( pTexture, 0, &srcRect, NULL );
}


//-----------------------------------------------------------------------------
//
// Reflective glass view starts here
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Draws the scene when the view contains reflective glass
//-----------------------------------------------------------------------------
void CReflectiveGlassView::Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, 
	const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, const cplane_t &reflectionPlane )
{
	BaseClass::Setup( view, nClearFlags, bDrawSkybox, fogInfo, waterInfo, NULL );
	m_ReflectionPlane = reflectionPlane;
}


bool CReflectiveGlassView::AdjustView( float flWaterHeight )
{
	ITexture *pTexture = GetWaterReflectionTexture();
		   
	// Use the aspect ratio of the main view! So, don't recompute it here
	x = y = 0;
	width = pTexture->GetActualWidth();
	height = pTexture->GetActualHeight();

	// Reflect the camera origin + vectors around the reflection plane 
	float flDist = DotProduct( origin, m_ReflectionPlane.normal ) - m_ReflectionPlane.dist;
	VectorMA( origin, - 2.0f * flDist, m_ReflectionPlane.normal, origin );

	Vector vecForward, vecUp;
	AngleVectors( angles, &vecForward, NULL, &vecUp );

	float flDot = DotProduct( vecForward, m_ReflectionPlane.normal );
	VectorMA( vecForward, - 2.0f * flDot, m_ReflectionPlane.normal, vecForward );

	flDot = DotProduct( vecUp, m_ReflectionPlane.normal );
	VectorMA( vecUp, - 2.0f * flDot, m_ReflectionPlane.normal, vecUp );

	VectorAngles( vecForward, vecUp, angles );
	return true;
}

void CReflectiveGlassView::PushView( float waterHeight )
{
	render->Push3DView( *this, m_ClearFlags, GetWaterReflectionTexture(), GetFrustum() );
	 
	Vector4D plane;
	VectorCopy( m_ReflectionPlane.normal, plane.AsVector3D() );
	plane.w = m_ReflectionPlane.dist + 0.1f;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushCustomClipPlane( plane.Base() );
}

void CReflectiveGlassView::PopView( )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PopCustomClipPlane( );
	render->PopView( GetFrustum() );
}


//-----------------------------------------------------------------------------
// Renders reflective or refractive parts of glass
//-----------------------------------------------------------------------------
void CReflectiveGlassView::Draw()
{
	VPROF( "CReflectiveGlassView::Draw" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CReflectiveGlassView::Draw" );

	// Disable occlusion visualization in reflection
	bool bVisOcclusion = r_visocclusion.GetBool();
	r_visocclusion.SetValue( 0 );
				   
	BaseClass::Draw();

	r_visocclusion.SetValue( bVisOcclusion );

	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->Flush();
}



//-----------------------------------------------------------------------------
// Draws the scene when the view contains reflective glass
//-----------------------------------------------------------------------------
void CRefractiveGlassView::Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, 
	const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, const cplane_t &reflectionPlane )
{
	BaseClass::Setup( view, nClearFlags, bDrawSkybox, fogInfo, waterInfo, NULL );
	m_ReflectionPlane = reflectionPlane;
}


bool CRefractiveGlassView::AdjustView( float flWaterHeight )
{
	ITexture *pTexture = GetWaterRefractionTexture();

	// Use the aspect ratio of the main view! So, don't recompute it here
	x = y = 0;
	width = pTexture->GetActualWidth();
	height = pTexture->GetActualHeight();
	return true;
}


void CRefractiveGlassView::PushView( float waterHeight )
{
	render->Push3DView( *this, m_ClearFlags, GetWaterRefractionTexture(), GetFrustum() );

	Vector4D plane;
	VectorMultiply( m_ReflectionPlane.normal, -1, plane.AsVector3D() );
	plane.w = -m_ReflectionPlane.dist + 0.1f;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushCustomClipPlane( plane.Base() );
}


void CRefractiveGlassView::PopView( )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PopCustomClipPlane( );
	render->PopView( GetFrustum() );
}



//-----------------------------------------------------------------------------
// Renders reflective or refractive parts of glass
//-----------------------------------------------------------------------------
void CRefractiveGlassView::Draw()
{
	VPROF( "CRefractiveGlassView::Draw" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CRefractiveGlassView::Draw" );

	BaseClass::Draw();

	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->Flush();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void FrustumCache_t::Add( const CViewSetup *pView, int iSlot )
{
	// Check for a valid view setup.
	if ( !pView )
		return;

	// Create the perspective frustum.
	GeneratePerspectiveFrustum( pView->origin, pView->angles, pView->zNear, pView->zFar, pView->fov, pView->m_flAspectRatio, m_Frustums[iSlot] );
}



