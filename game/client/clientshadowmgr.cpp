//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
// Interface to the client system responsible for dealing with shadows
//
// Boy is this complicated. OK, lets talk about how this works at the moment
//
// The ClientShadowMgr contains all of the highest-level state for rendering
// shadows, and it controls the ShadowMgr in the engine which is the central
// clearing house for rendering shadows.
//
// There are two important types of objects with respect to shadows:
// the shadow receiver, and the shadow caster. How is the association made
// between casters + the receivers? Turns out it's done slightly differently 
// depending on whether the receiver is the world, or if it's an entity.
//
// In the case of the world, every time the engine's ProjectShadow() is called, 
// any previous receiver state stored (namely, which world surfaces are
// receiving shadows) are cleared. Then, when ProjectShadow is called, 
// the engine iterates over all nodes + leaves within the shadow volume and 
// marks front-facing surfaces in them as potentially being affected by the 
// shadow. Later on, if those surfaces are actually rendered, the surfaces
// are clipped by the shadow volume + rendered.
// 
// In the case of entities, there are slightly different methods depending
// on whether the receiver is a brush model or a studio model. However, there
// are a couple central things that occur with both.
//
// Every time a shadow caster is moved, the ClientLeafSystem's ProjectShadow
// method is called to tell it to remove the shadow from all leaves + all 
// renderables it's currently associated with. Then it marks each leaf in the
// shadow volume as being affected by that shadow, and it marks every renderable
// in that volume as being potentially affected by the shadow (the function
// AddShadowToRenderable is called for each renderable in leaves affected
// by the shadow volume).
//
// Every time a shadow receiver is moved, the ClientLeafSystem first calls 
// RemoveAllShadowsFromRenderable to have it clear out its state, and then
// the ClientLeafSystem calls AddShadowToRenderable() for all shadows in all
// leaves the renderable has moved into.
//
// Now comes the difference between brush models + studio models. In the case
// of brush models, when a shadow is added to the studio model, it's done in
// the exact same way as for the world. Surfaces on the brush model are marked
// as potentially being affected by the shadow, and if those surfaces are
// rendered, the surfaces are clipped to the shadow volume. When ProjectShadow()
// is called, turns out the same operation that removes the shadow that moved
// from the world surfaces also works to remove the shadow from brush surfaces.
//
// In the case of studio models, we need a separate operation to remove
// the shadow from all studio models
//
// DEFERRED SHADOW RENDERING
//
// When deferred shadow rendering (currently 360 only) is enabled. The
// ClientShadowMgr bypasses most calls to the engine shadow mgr to avoid the
// CPU overhead of clipping world geometry against shadow frustums. Instead,
// We render each shadow frustum and use the depth buffer to back-project each
// covered screen pixel into shadow space and apply the shadow. This causes
// everything that rendered into the depth buffer during the opaque renderables
// pass to be a shadow receiver (shadows on static props are free). Because this
// approach requires a lot of fill-rate, we impose the limitation that shadow
// casters can't receive shadows. Shadow casters are marked in the stencil buffer
// (using stencil mask 0x4) AND in the 360's heirarchical stencil buffer, which
// is most important for controlling fill rate. Initializing the stencil mask
// for shadow casters currently happens in several places: the staticpropmgr,
// c_baseanimating rendering code, and L4D-specific entity classes.
//
//===========================================================================//


#include "cbase.h"
#include "engine/IShadowMgr.h"
#include "model_types.h"
#include "bitmap/imageformat.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/IMaterialVar.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMesh.h"
#include "materialsystem/ITexture.h"
#include "BSPTreeData.h"
#include "utlmultilist.h"
#include "CollisionUtils.h"
#include "iviewrender.h"
#include "IVRenderView.h"
#include "tier0/vprof.h"
#include "engine/ivmodelinfo.h"
#include "view_shared.h"
#include "engine/IVDebugOverlay.h"
#include "engine/IStaticPropMgr.h"
#include "datacache/imdlcache.h"
#include "viewrender.h"
#include "tier0/ICommandLine.h"
#include "vstdlib/jobthread.h"
#include "bonetoworldarray.h"
#include "debugoverlay_shared.h"
#include "shaderapi/ishaderapi.h"
#include "renderparm.h"
#include "rendertexture.h"
#include "clientalphaproperty.h"
#include "flashlighteffect.h"
#include "c_env_projectedtexture.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static ConVar r_flashlightdrawfrustum( "r_flashlightdrawfrustum", "0" );
static ConVar r_flashlightdrawfrustumbbox( "r_flashlightdrawfrustumbbox", "0" );
static ConVar r_flashlightmodels( "r_flashlightmodels", "1" );
static ConVar r_shadowrendertotexture( "r_shadowrendertotexture", "0" );
static ConVar r_shadow_lightpos_lerptime( "r_shadow_lightpos_lerptime", "0.5" );
static ConVar r_shadowfromworldlights_debug( "r_shadowfromworldlights_debug", "0", FCVAR_CHEAT );
static ConVar r_shadowfromanyworldlight( "r_shadowfromanyworldlight", "0", FCVAR_CHEAT );
static ConVar r_shadow_shortenfactor( "r_shadow_shortenfactor", "2" , 0, "Makes shadows cast from local lights shorter" );

static void HalfUpdateRateCallback( IConVar *var, const char *pOldValue, float flOldValue );
static ConVar r_shadow_half_update_rate( "r_shadow_half_update_rate", IsX360() ? "1" : "0", 0, "Updates shadows at half the framerate", HalfUpdateRateCallback );

static void DeferredShadowToggleCallback( IConVar *var, const char *pOldValue, float flOldValue );
static void DeferredShadowDownsampleToggleCallback( IConVar *var, const char *pOldValue, float flOldValue );
ConVar r_shadow_deferred( "r_shadow_deferred", "0", FCVAR_CHEAT, "Toggle deferred shadow rendering", DeferredShadowToggleCallback );
static ConVar r_shadow_deferred_downsample( "r_shadow_deferred_downsample", "0", 0, "Toggle low-res deferred shadow rendering", DeferredShadowDownsampleToggleCallback );
static ConVar r_shadow_deferred_simd( "r_shadow_deferred_simd", "0" );

static ConVar r_shadow_debug_spew( "r_shadow_debug_spew", "0", FCVAR_CHEAT );


ConVar r_flashlightdepthtexture( "r_flashlightdepthtexture", "1" );

#if defined( _X360 )
ConVar r_flashlightdepthreshigh( "r_flashlightdepthreshigh", "512" );
#else
ConVar r_flashlightdepthreshigh( "r_flashlightdepthreshigh", "2048" );
#endif

#if defined( _X360 )
ConVar r_flashlightdepthres( "r_flashlightdepthres", "512" );
#else
ConVar r_flashlightdepthres( "r_flashlightdepthres", "1024" );
#endif

#if defined( _X360 )
#define RTT_TEXTURE_SIZE_640
#endif

#ifdef _WIN32
#pragma warning( disable: 4701 )
#endif


//-----------------------------------------------------------------------------
// A texture allocator used to batch textures together
// At the moment, the implementation simply allocates blocks of max 256x256
// and each block stores an array of uniformly-sized textures
//-----------------------------------------------------------------------------
typedef unsigned short TextureHandle_t;
enum
{
	INVALID_TEXTURE_HANDLE = (TextureHandle_t)~0
};

class CTextureAllocator
{
public:
	// Initialize the allocator with something that knows how to refresh the bits
	void			Init();
	void			Shutdown();

	// Resets the allocator
	void			Reset();

	// Deallocates everything
	void			DeallocateAllTextures();

	// Allocate, deallocate texture
	TextureHandle_t	AllocateTexture( int w, int h );
	void			DeallocateTexture( TextureHandle_t h );

	// Mark texture as being used... (return true if re-render is needed)
	bool			UseTexture( TextureHandle_t h, bool bWillRedraw, float flArea );
	bool			HasValidTexture( TextureHandle_t h );

	// Advance frame...
	void			AdvanceFrame();

	// Get at the location of the texture
	void			GetTextureRect(TextureHandle_t handle, int& x, int& y, int& w, int& h );

	// Get at the texture it's a part of
	ITexture		*GetTexture();
	
	// Get at the total texture size.
	void			GetTotalTextureSize( int& w, int& h );

	void			DebugPrintCache( void );

	void InitRenderTargets( void );

private:
	typedef unsigned short FragmentHandle_t;

	enum
	{
		INVALID_FRAGMENT_HANDLE = (FragmentHandle_t)~0,
#ifdef RTT_TEXTURE_SIZE_640
		TEXTURE_PAGE_SIZE	    = 640,
		MAX_TEXTURE_POWER    	= 7,
#else
		TEXTURE_PAGE_SIZE	    = 1024,
		MAX_TEXTURE_POWER    	= 8,
#endif
#if !defined( _X360 )
		MIN_TEXTURE_POWER	    = 4,
#else
		MIN_TEXTURE_POWER	    = 5,	// per resolve requirements to ensure 32x32 aligned offsets
#endif
		MAX_TEXTURE_SIZE	    = (1 << MAX_TEXTURE_POWER),
		MIN_TEXTURE_SIZE	    = (1 << MIN_TEXTURE_POWER),
		BLOCK_SIZE			    = MAX_TEXTURE_SIZE,
		BLOCKS_PER_ROW		    = (TEXTURE_PAGE_SIZE / MAX_TEXTURE_SIZE),
		BLOCK_COUNT			    = (BLOCKS_PER_ROW * BLOCKS_PER_ROW),
	};

	struct TextureInfo_t
	{
		FragmentHandle_t	m_Fragment;
		unsigned short		m_Size;
		unsigned short		m_Power;
	};

	struct FragmentInfo_t
	{
		unsigned short	m_Block;
		unsigned short	m_Index;
		TextureHandle_t	m_Texture;

		// Makes sure we don't overflow
		unsigned int	m_FrameUsed;
	};

	struct BlockInfo_t
	{
		unsigned short	m_FragmentPower;
	};

	struct Cache_t
	{
		unsigned short	m_List;
	};

	// Adds a block worth of fragments to the LRU
	void AddBlockToLRU( int block );

	// Unlink fragment from cache
	void UnlinkFragmentFromCache( Cache_t& cache, FragmentHandle_t fragment );

	// Mark something as being used (MRU)..
	void MarkUsed( FragmentHandle_t fragment );

	// Mark something as being unused (LRU)..
	void MarkUnused( FragmentHandle_t fragment );

	// Disconnect texture from fragment
	void DisconnectTextureFromFragment( FragmentHandle_t f );

	// Returns the size of a particular fragment
	int	GetFragmentPower( FragmentHandle_t f ) const;

	// Stores the actual texture we're writing into
	CTextureReference	m_TexturePage;

	CUtlLinkedList< TextureInfo_t, TextureHandle_t >	m_Textures;
	CUtlMultiList< FragmentInfo_t, FragmentHandle_t >	m_Fragments;

	Cache_t		m_Cache[MAX_TEXTURE_POWER+1]; 
	BlockInfo_t	m_Blocks[BLOCK_COUNT];
	unsigned int m_CurrentFrame;
};

//-----------------------------------------------------------------------------
// Allocate/deallocate the texture page
//-----------------------------------------------------------------------------
void CTextureAllocator::Init()
{
	for ( int i = 0; i <= MAX_TEXTURE_POWER; ++i )
	{
		m_Cache[i].m_List = m_Fragments.InvalidIndex();
	}

	InitRenderTargets();
}

void CTextureAllocator::InitRenderTargets( void )
{
#if !defined( _X360 )
	// don't need depth buffer for shadows
	m_TexturePage.InitRenderTarget( TEXTURE_PAGE_SIZE, TEXTURE_PAGE_SIZE, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_ARGB8888, MATERIAL_RT_DEPTH_NONE, false, "_rt_Shadows" );
#else
	// unfortunate explicit management required for this render target
	// 32bpp edram is only largest shadow fragment, but resolved to actual shadow atlas
	// because full-res 1024x1024 shadow buffer is too large for EDRAM
	m_TexturePage.InitRenderTargetTexture( TEXTURE_PAGE_SIZE, TEXTURE_PAGE_SIZE, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_ARGB8888, MATERIAL_RT_DEPTH_NONE, false, "_rt_Shadows" );

#ifdef RTT_TEXTURE_SIZE_640
	// use 4x multisampling for smoother shadows
	m_TexturePage.InitRenderTargetSurface( MAX_TEXTURE_SIZE, MAX_TEXTURE_SIZE, IMAGE_FORMAT_ARGB8888, false, RT_MULTISAMPLE_4_SAMPLES );
#else
	// edram footprint is only 256x256x4 = 256K
	m_TexturePage.InitRenderTargetSurface( MAX_TEXTURE_SIZE, MAX_TEXTURE_SIZE, IMAGE_FORMAT_ARGB8888, false );
#endif

	// due to texture/surface size mismatch, ensure texture page is entirely cleared translucent
	// otherwise border artifacts at edge of shadows due to pixel shader averaging of unwanted bits
	m_TexturePage->ClearTexture( 0, 0, 0, 0 );
#endif
	
}


void CTextureAllocator::Shutdown()
{
	m_TexturePage.Shutdown();
}


//-----------------------------------------------------------------------------
// Initialize the allocator with something that knows how to refresh the bits
//-----------------------------------------------------------------------------
void CTextureAllocator::Reset()
{
	DeallocateAllTextures();

	m_Textures.EnsureCapacity(256);
	m_Fragments.EnsureCapacity(256);

	// Set up the block sizes....
#ifdef RTT_TEXTURE_SIZE_640
	// Going to 640x640 gives us roughly the same number of texture slots than the 1024x1024 texture
	// and thus won't change cache thrashing patterns
	m_Blocks[0].m_FragmentPower  = MAX_TEXTURE_POWER-2;
	m_Blocks[1].m_FragmentPower  = MAX_TEXTURE_POWER-2;
	m_Blocks[2].m_FragmentPower  = MAX_TEXTURE_POWER-2;
	m_Blocks[3].m_FragmentPower  = MAX_TEXTURE_POWER-2;		 
	m_Blocks[4].m_FragmentPower  = MAX_TEXTURE_POWER-2;
	m_Blocks[5].m_FragmentPower  = MAX_TEXTURE_POWER-2;
	m_Blocks[6].m_FragmentPower  = MAX_TEXTURE_POWER-2;
	m_Blocks[7].m_FragmentPower  = MAX_TEXTURE_POWER-2;
	m_Blocks[8].m_FragmentPower  = MAX_TEXTURE_POWER-2;
	m_Blocks[9].m_FragmentPower  = MAX_TEXTURE_POWER-2;	// 10 * 16 = 160
	m_Blocks[10].m_FragmentPower = MAX_TEXTURE_POWER-1;
	m_Blocks[11].m_FragmentPower = MAX_TEXTURE_POWER-1;
	m_Blocks[12].m_FragmentPower = MAX_TEXTURE_POWER-1;
	m_Blocks[13].m_FragmentPower = MAX_TEXTURE_POWER-1;
	m_Blocks[14].m_FragmentPower = MAX_TEXTURE_POWER-1;
	m_Blocks[15].m_FragmentPower = MAX_TEXTURE_POWER-1;
	m_Blocks[16].m_FragmentPower = MAX_TEXTURE_POWER-1;
	m_Blocks[17].m_FragmentPower = MAX_TEXTURE_POWER-1;	// 8 * 4 = 32
	m_Blocks[18].m_FragmentPower = MAX_TEXTURE_POWER;	// 7
	m_Blocks[19].m_FragmentPower = MAX_TEXTURE_POWER;
	m_Blocks[20].m_FragmentPower = MAX_TEXTURE_POWER;
	m_Blocks[21].m_FragmentPower = MAX_TEXTURE_POWER;
	m_Blocks[22].m_FragmentPower = MAX_TEXTURE_POWER;
	m_Blocks[23].m_FragmentPower = MAX_TEXTURE_POWER;
	m_Blocks[24].m_FragmentPower = MAX_TEXTURE_POWER;	// 199 slots total
#else
	// FIXME: Improve heuristic?!?
#if !defined( _X360 )
	m_Blocks[0].m_FragmentPower  = MAX_TEXTURE_POWER-4;	// 128 cells at ExE resolution
#else
	m_Blocks[0].m_FragmentPower  = MAX_TEXTURE_POWER-3;	// 64 cells at DxD resolution
#endif
	m_Blocks[1].m_FragmentPower  = MAX_TEXTURE_POWER-3;	// 64 cells at DxD resolution
	m_Blocks[2].m_FragmentPower  = MAX_TEXTURE_POWER-2;	// 32 cells at CxC resolution
	m_Blocks[3].m_FragmentPower  = MAX_TEXTURE_POWER-2;		 
	m_Blocks[4].m_FragmentPower  = MAX_TEXTURE_POWER-1;	// 24 cells at BxB resolution
	m_Blocks[5].m_FragmentPower  = MAX_TEXTURE_POWER-1;
	m_Blocks[6].m_FragmentPower  = MAX_TEXTURE_POWER-1;
	m_Blocks[7].m_FragmentPower  = MAX_TEXTURE_POWER-1;
	m_Blocks[8].m_FragmentPower  = MAX_TEXTURE_POWER-1;
	m_Blocks[9].m_FragmentPower  = MAX_TEXTURE_POWER-1;
	m_Blocks[10].m_FragmentPower = MAX_TEXTURE_POWER;	// 6 cells at AxA resolution
	m_Blocks[11].m_FragmentPower = MAX_TEXTURE_POWER;	 
	m_Blocks[12].m_FragmentPower = MAX_TEXTURE_POWER;
	m_Blocks[13].m_FragmentPower = MAX_TEXTURE_POWER;
	m_Blocks[14].m_FragmentPower = MAX_TEXTURE_POWER;
	m_Blocks[15].m_FragmentPower = MAX_TEXTURE_POWER;	// 190 slots total on 360
#endif


	// Initialize the LRU
	int i;
	for ( i = 0; i <= MAX_TEXTURE_POWER; ++i )
	{
		m_Cache[i].m_List = m_Fragments.CreateList();
	}

	// Now that the block sizes are allocated, create LRUs for the various block sizes
	for ( i = 0; i < BLOCK_COUNT; ++i)
	{
		// Initialize LRU
		AddBlockToLRU( i );
	}

	m_CurrentFrame = 0;
}

void CTextureAllocator::DeallocateAllTextures()
{
	m_Textures.Purge();
	m_Fragments.Purge();
	for ( int i = 0; i <= MAX_TEXTURE_POWER; ++i )
	{
		m_Cache[i].m_List = m_Fragments.InvalidIndex();
	}
}


//-----------------------------------------------------------------------------
// Dump the state of the cache to debug out
//-----------------------------------------------------------------------------
void CTextureAllocator::DebugPrintCache( void )
{
	// For each fragment
	int nNumFragments = m_Fragments.TotalCount();
	int nNumInvalidFragments = 0;

	Warning("Fragments (%d):\n===============\n", nNumFragments);

	for ( int f = 0; f < nNumFragments; f++ )
	{
		if ( ( m_Fragments[f].m_FrameUsed != 0 ) && ( m_Fragments[f].m_Texture != INVALID_TEXTURE_HANDLE ) )
			Warning("Fragment %d, Block: %d, Index: %d, Texture: %d Frame Used: %d\n", f, m_Fragments[f].m_Block, m_Fragments[f].m_Index, m_Fragments[f].m_Texture, m_Fragments[f].m_FrameUsed );
		else
			nNumInvalidFragments++;
	}

	Warning("Invalid Fragments: %d\n", nNumInvalidFragments);

//	for ( int c = 0; c <= MAX_TEXTURE_POWER; ++c )
//	{
//		Warning("Cache Index (%d)\n", m_Cache[c].m_List);
//	}

}


//-----------------------------------------------------------------------------
// Adds a block worth of fragments to the LRU
//-----------------------------------------------------------------------------
void CTextureAllocator::AddBlockToLRU( int block )
{
	int power = m_Blocks[block].m_FragmentPower;
 	int size = (1 << power);

	// Compute the number of fragments in this block
	int fragmentCount = MAX_TEXTURE_SIZE / size;
	fragmentCount *= fragmentCount;

	// For each fragment, indicate which block it's a part of (and the index)
	// and then stick in at the top of the LRU
	while (--fragmentCount >= 0 )
	{
		FragmentHandle_t f = m_Fragments.Alloc( );
		m_Fragments[f].m_Block = block;
		m_Fragments[f].m_Index = fragmentCount;
		m_Fragments[f].m_Texture = INVALID_TEXTURE_HANDLE;
		m_Fragments[f].m_FrameUsed = 0xFFFFFFFF;
		m_Fragments.LinkToHead( m_Cache[power].m_List, f );
	}
}


//-----------------------------------------------------------------------------
// Unlink fragment from cache
//-----------------------------------------------------------------------------
void CTextureAllocator::UnlinkFragmentFromCache( Cache_t& cache, FragmentHandle_t fragment )
{
	m_Fragments.Unlink( cache.m_List, fragment);
}


//-----------------------------------------------------------------------------
// Mark something as being used (MRU)..
//-----------------------------------------------------------------------------
void CTextureAllocator::MarkUsed( FragmentHandle_t fragment )
{
	int block = m_Fragments[fragment].m_Block;
	int power = m_Blocks[block].m_FragmentPower;

	// Hook it at the end of the LRU
	Cache_t& cache = m_Cache[power];
	m_Fragments.LinkToTail( cache.m_List, fragment );
	m_Fragments[fragment].m_FrameUsed = m_CurrentFrame;
}


//-----------------------------------------------------------------------------
// Mark something as being unused (LRU)..
//-----------------------------------------------------------------------------
void CTextureAllocator::MarkUnused( FragmentHandle_t fragment )
{
	int block = m_Fragments[fragment].m_Block;
	int power = m_Blocks[block].m_FragmentPower;

	// Hook it at the end of the LRU
	Cache_t& cache = m_Cache[power];
	m_Fragments.LinkToHead( cache.m_List, fragment );
}


//-----------------------------------------------------------------------------
// Allocate, deallocate texture
//-----------------------------------------------------------------------------
TextureHandle_t	CTextureAllocator::AllocateTexture( int w, int h )
{
	// Implementational detail for now
	Assert( w == h );

	// Clamp texture size
	if (w < MIN_TEXTURE_SIZE)
		w = MIN_TEXTURE_SIZE;
	else if (w > MAX_TEXTURE_SIZE)
		w = MAX_TEXTURE_SIZE;

	TextureHandle_t handle = m_Textures.AddToTail();
	m_Textures[handle].m_Fragment = INVALID_FRAGMENT_HANDLE;
	m_Textures[handle].m_Size = w;

	// Find the power of two
	int power = 0;
	int size = 1;
	while(size < w)
	{
		size <<= 1;
		++power;
	}
	Assert( size == w );

	m_Textures[handle].m_Power = power;

	return handle;
}

void CTextureAllocator::DeallocateTexture( TextureHandle_t h )
{
//	Warning("Beginning of DeallocateTexture\n");
//	DebugPrintCache();

	if (m_Textures[h].m_Fragment != INVALID_FRAGMENT_HANDLE)
	{
		MarkUnused(m_Textures[h].m_Fragment);
		m_Fragments[m_Textures[h].m_Fragment].m_FrameUsed = 0xFFFFFFFF;	// non-zero frame
		DisconnectTextureFromFragment( m_Textures[h].m_Fragment );
	}
	m_Textures.Remove(h);

//	Warning("End of DeallocateTexture\n");
//	DebugPrintCache();
}


//-----------------------------------------------------------------------------
// Disconnect texture from fragment
//-----------------------------------------------------------------------------
void CTextureAllocator::DisconnectTextureFromFragment( FragmentHandle_t f )
{
//	Warning( "Beginning of DisconnectTextureFromFragment\n" );
//	DebugPrintCache();

	FragmentInfo_t& info = m_Fragments[f];
	if (info.m_Texture != INVALID_TEXTURE_HANDLE)
	{
		m_Textures[info.m_Texture].m_Fragment = INVALID_FRAGMENT_HANDLE;
		info.m_Texture = INVALID_TEXTURE_HANDLE;
	}


//	Warning( "End of DisconnectTextureFromFragment\n" );
//	DebugPrintCache();
}


//-----------------------------------------------------------------------------
// Do we have a valid texture assigned?
//-----------------------------------------------------------------------------
bool CTextureAllocator::HasValidTexture( TextureHandle_t h )
{
	TextureInfo_t& info = m_Textures[h];
	FragmentHandle_t currentFragment = info.m_Fragment;
	return (currentFragment != INVALID_FRAGMENT_HANDLE);
}


//-----------------------------------------------------------------------------
// Mark texture as being used...
//-----------------------------------------------------------------------------
bool CTextureAllocator::UseTexture( TextureHandle_t h, bool bWillRedraw, float flArea )
{
//	Warning( "Top of UseTexture\n" );
//	DebugPrintCache();

	TextureInfo_t& info = m_Textures[h];

	// spin up to the best fragment size
	int nDesiredPower = MIN_TEXTURE_POWER;
	int nDesiredWidth = MIN_TEXTURE_SIZE;
	while ( (nDesiredWidth * nDesiredWidth) < flArea )
	{
		if ( nDesiredPower >= info.m_Power )
		{
			nDesiredPower = info.m_Power;
			break;
		}

		++nDesiredPower;
		nDesiredWidth <<= 1;
	}

	// If we've got a valid fragment for this texture, no worries!
	int nCurrentPower = -1;
	FragmentHandle_t currentFragment = info.m_Fragment;
	if (currentFragment != INVALID_FRAGMENT_HANDLE)
	{
		// If the current fragment is at or near the desired power, we're done
		nCurrentPower = GetFragmentPower(info.m_Fragment);
		Assert( nCurrentPower <= info.m_Power );
		bool bShouldKeepTexture = (!bWillRedraw) && (nDesiredPower < 8) && (nDesiredPower - nCurrentPower <= 1);
		if ((nCurrentPower == nDesiredPower) || bShouldKeepTexture)
		{
			// Move to the back of the LRU
			MarkUsed( currentFragment );
			return false;
		}
	}

//	Warning( "\n\nUseTexture B\n" );
//	DebugPrintCache();

	// Grab the LRU fragment from the appropriate cache
	// If that fragment is connected to a texture, disconnect it.
	int power = nDesiredPower;

	FragmentHandle_t f = INVALID_FRAGMENT_HANDLE;
	bool done = false;
	while (!done && power >= 0)
	{
		f = m_Fragments.Head( m_Cache[power].m_List );
	
		// This represents an overflow condition (used too many textures of
		// the same size in a single frame). It that happens, just use a texture
		// of lower res.
		if ( (f != m_Fragments.InvalidIndex()) && (m_Fragments[f].m_FrameUsed != m_CurrentFrame) )
		{
			done = true;
		}
		else
		{
			--power;
		}
	}


//	Warning( "\n\nUseTexture C\n" );
//	DebugPrintCache();

	// Ok, lets see if we're better off than we were...
	if (currentFragment != INVALID_FRAGMENT_HANDLE)
	{
		if (power <= nCurrentPower)
		{
			// Oops... we're not. Let's leave well enough alone
			// Move to the back of the LRU
			MarkUsed( currentFragment );
			return false;
		}
		else
		{
			// Clear out the old fragment
			DisconnectTextureFromFragment(currentFragment);
		}
	}

	if ( f == INVALID_FRAGMENT_HANDLE )
	{
		return false;
	}

	// Disconnect existing texture from this fragment (if necessary)
	DisconnectTextureFromFragment(f);

	// Connnect new texture to this fragment
	info.m_Fragment = f;
	m_Fragments[f].m_Texture = h;

	// Move to the back of the LRU
	MarkUsed( f );

	// Indicate we need a redraw
	return true;
}


//-----------------------------------------------------------------------------
// Returns the size of a particular fragment
//-----------------------------------------------------------------------------
int	CTextureAllocator::GetFragmentPower( FragmentHandle_t f ) const
{
	return m_Blocks[m_Fragments[f].m_Block].m_FragmentPower;
}


//-----------------------------------------------------------------------------
// Advance frame...
//-----------------------------------------------------------------------------
void CTextureAllocator::AdvanceFrame()
{
	// Be sure that this is called as infrequently as possible (i.e. once per frame,
	// NOT once per view) to prevent cache thrash when rendering multiple views in a single frame
	m_CurrentFrame++;
}


//-----------------------------------------------------------------------------
// Prepare to render into texture...
//-----------------------------------------------------------------------------
ITexture* CTextureAllocator::GetTexture()
{
	return m_TexturePage;
}

//-----------------------------------------------------------------------------
// Get at the total texture size.
//-----------------------------------------------------------------------------
void CTextureAllocator::GetTotalTextureSize( int& w, int& h )
{
	w = h = TEXTURE_PAGE_SIZE;
}


//-----------------------------------------------------------------------------
// Returns the rectangle the texture lives in..
//-----------------------------------------------------------------------------
void CTextureAllocator::GetTextureRect(TextureHandle_t handle, int& x, int& y, int& w, int& h )
{
	TextureInfo_t& info = m_Textures[handle];
	Assert( info.m_Fragment != INVALID_FRAGMENT_HANDLE );

	// Compute the position of the fragment in the page
	FragmentInfo_t& fragment = m_Fragments[info.m_Fragment];
	int blockY = fragment.m_Block / BLOCKS_PER_ROW;
	int blockX = fragment.m_Block - blockY * BLOCKS_PER_ROW;

	int fragmentSize = (1 << m_Blocks[fragment.m_Block].m_FragmentPower);
	int fragmentsPerRow = BLOCK_SIZE / fragmentSize;
	int fragmentY = fragment.m_Index / fragmentsPerRow;
	int fragmentX = fragment.m_Index - fragmentY * fragmentsPerRow;

	x = blockX * BLOCK_SIZE + fragmentX * fragmentSize;
	y = blockY * BLOCK_SIZE + fragmentY * fragmentSize;
	w = fragmentSize;
	h = fragmentSize;
}


//-----------------------------------------------------------------------------
// Defines how big of a shadow texture we should be making per caster...
//-----------------------------------------------------------------------------
#define TEXEL_SIZE_PER_CASTER_SIZE	2.0f 
#define MAX_FALLOFF_AMOUNT 240
#define MAX_CLIP_PLANE_COUNT 4
#define SHADOW_CULL_TOLERANCE 0.5f

static ConVar r_shadows( "r_shadows", "1" ); // hook into engine's cvars..
static ConVar r_shadowmaxrendered("r_shadowmaxrendered", "32");
static ConVar r_shadows_gamecontrol( "r_shadows_gamecontrol", "-1", FCVAR_CHEAT );	 // hook into engine's cvars..

//-----------------------------------------------------------------------------
// The class responsible for dealing with shadows on the client side
// Oh, and let's take a moment and notice how happy Robin and John must be 
// owing to the lack of space between this lovely comment and the class name =)
//-----------------------------------------------------------------------------
class CClientShadowMgr : public IClientShadowMgr
{
public:
	CClientShadowMgr();

	virtual char const *Name() { return "CCLientShadowMgr"; }

	// Inherited from IClientShadowMgr
	virtual bool Init();
	virtual void InitRenderTargets();

	virtual void PostInit() {}
	virtual void Shutdown();
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity();

	virtual bool IsPerFrame() { return true; }

	virtual void PreRender() {}
	virtual void Update( float frametime ) { }
	virtual void PostRender() {}

	virtual void OnSave() {}
	virtual void OnRestore() {}
	virtual void SafeRemoveIfDesired() {}

	virtual void ReprojectShadows();

	virtual ClientShadowHandle_t CreateShadow( ClientEntityHandle_t entity, int nEntIndex, int flags, CBitVec< MAX_SPLITSCREEN_PLAYERS > *pSplitScreenBits = NULL );
	virtual void DestroyShadow( ClientShadowHandle_t handle );

	// Create flashlight (projected texture light source)
	virtual ClientShadowHandle_t CreateFlashlight( const FlashlightState_t &lightState );
	virtual void UpdateFlashlightState( ClientShadowHandle_t shadowHandle, const FlashlightState_t &lightState );
	virtual void DestroyFlashlight( ClientShadowHandle_t shadowHandle );

	// Create simple projected texture.  it is not a light or a shadow, but this system does most of the work already for it
	virtual ClientShadowHandle_t CreateProjection( const FlashlightState_t &lightState );
	virtual void UpdateProjectionState( ClientShadowHandle_t shadowHandle, const FlashlightState_t &lightState );
	virtual void DestroyProjection( ClientShadowHandle_t shadowHandle );

	// Update a shadow
	virtual void UpdateProjectedTexture( ClientShadowHandle_t handle, bool force );

	void ComputeBoundingSphere( IClientRenderable* pRenderable, Vector& origin, float& radius );

	virtual void AddToDirtyShadowList( ClientShadowHandle_t handle, bool bForce );
	virtual void AddToDirtyShadowList( IClientRenderable *pRenderable, bool force );

	// Marks the render-to-texture shadow as needing to be re-rendered
	virtual void MarkRenderToTextureShadowDirty( ClientShadowHandle_t handle );

	// deals with shadows being added to shadow receivers
	void AddShadowToReceiver( ClientShadowHandle_t handle,
		IClientRenderable* pRenderable, ShadowReceiver_t type );

	// deals with shadows being added to shadow receivers
	void RemoveAllShadowsFromReceiver( IClientRenderable* pRenderable, ShadowReceiver_t type );

	// Re-renders all shadow textures for shadow casters that lie in the leaf list
	void ComputeShadowTextures( const CViewSetup &view, int leafCount, WorldListLeafData_t* pLeafList );

	// Kicks off rendering into shadow depth maps (if any)
	void ComputeShadowDepthTextures( const CViewSetup &view );

	// Kicks off rendering of volumetrics for the flashlights
	void DrawVolumetrics( const CViewSetup &view );

	void GetFrustumExtents( ClientShadowHandle_t handle, Vector &vecMin, Vector &vecMax );

	// Frees shadow depth textures for use in subsequent view/frame
	void FreeShadowDepthTextures();

	// Returns the shadow texture
	ITexture* GetShadowTexture( unsigned short h );

	// Returns shadow information
	const ShadowInfo_t& GetShadowInfo( ClientShadowHandle_t h );

	// Renders the shadow texture to screen...
	void RenderShadowTexture( int w, int h );

	// Sets the shadow direction
	virtual void SetShadowDirection( const Vector& dir );
	const Vector &GetShadowDirection() const;

	// Sets the shadow color
	virtual void SetShadowColor( unsigned char r, unsigned char g, unsigned char b );
	void GetShadowColor( unsigned char *r, unsigned char *g, unsigned char *b ) const;

	// Sets the shadow distance
	virtual void SetShadowDistance( float flMaxDistance );
	float GetShadowDistance( ) const;

	// Sets the screen area at which blobby shadows are always used
	virtual void SetShadowBlobbyCutoffArea( float flMinArea );
	float GetBlobbyCutoffArea( ) const;

	// Set the darkness falloff bias
	virtual void SetFalloffBias( ClientShadowHandle_t handle, unsigned char ucBias );

	void RestoreRenderState();

	// Computes a rough bounding box encompassing the volume of the shadow
	void ComputeShadowBBox( IClientRenderable *pRenderable, ClientShadowHandle_t shadowHandle, const Vector &vecAbsCenter, float flRadius, Vector *pAbsMins, Vector *pAbsMaxs );

	bool WillParentRenderBlobbyShadow( IClientRenderable *pRenderable );

	// Are we the child of a shadow with render-to-texture?
	bool ShouldUseParentShadow( IClientRenderable *pRenderable );

	void SetShadowsDisabled( bool bDisabled ) 
	{ 
		r_shadows_gamecontrol.SetValue( bDisabled != 1 );
	}

	// Toggle shadow casting from world light sources
	virtual void SetShadowFromWorldLightsEnabled( bool bEnable );
	void SuppressShadowFromWorldLights( bool bSuppress );
	bool IsShadowingFromWorldLights() const { return m_bShadowFromWorldLights && !m_bSuppressShadowFromWorldLights; }

	virtual void DrawDeferredShadows( const CViewSetup &view, int leafCount, WorldListLeafData_t* pLeafList );

	virtual void UpdateSplitscreenLocalPlayerShadowSkip();

private:
	enum
	{
		SHADOW_FLAGS_TEXTURE_DIRTY =	(CLIENT_SHADOW_FLAGS_LAST_FLAG << 1),
		SHADOW_FLAGS_BRUSH_MODEL =		(CLIENT_SHADOW_FLAGS_LAST_FLAG << 2), 
		SHADOW_FLAGS_USING_LOD_SHADOW = (CLIENT_SHADOW_FLAGS_LAST_FLAG << 3),
		SHADOW_FLAGS_LIGHT_WORLD =		(CLIENT_SHADOW_FLAGS_LAST_FLAG << 4),
	};

	struct ClientShadow_t
	{
		ClientEntityHandle_t	m_Entity;
		ShadowHandle_t			m_ShadowHandle;
		ClientLeafShadowHandle_t m_ClientLeafShadowHandle;
		unsigned short			m_Flags;
		VMatrix					m_WorldToShadow;
		Vector2D				m_WorldSize;
		Vector					m_ShadowDir;
		Vector					m_LastOrigin;
		QAngle					m_LastAngles;
		Vector					m_CurrentLightPos;	// When shadowing from local lights, stores the position of the currently shadowing light
		Vector					m_TargetLightPos;	// When shadowing from local lights, stores the position of the new shadowing light
		float					m_LightPosLerp;		// Lerp progress when going from current to target light
		TextureHandle_t			m_ShadowTexture;
		CTextureReference		m_ShadowDepthTexture;
		int						m_nRenderFrame;
		EHANDLE					m_hTargetEntity;

		bool					m_bUseSplitScreenBits;
		CBitVec< MAX_SPLITSCREEN_PLAYERS > m_SplitScreenBits;
		int						m_nLastUpdateFrame;

		// Extra info for deferred shadows.
		// FIXME: This data is also stored in CShadowMgr in the engine.
		int						m_FalloffBias;
		float					m_MaxDist;
		float					m_FalloffStart;
		Vector2D				m_TexCoordOffset;
		Vector2D				m_TexCoordScale;
		VMatrix					m_WorldToTexture;

		int						m_nSplitscreenOwner;
	};

private:
	friend void DeferredShadowToggleCallback( IConVar *var, const char *pOldValue, float flOldValue );
	friend void DeferredShadowDownsampleToggleCallback( IConVar *var, const char *pOldValue, float flOldValue );
	friend void HalfUpdateRateCallback( IConVar *var, const char *pOldValue, float flOldValue );

	// Shadow update functions
	void UpdateStudioShadow( IClientRenderable *pRenderable, ClientShadowHandle_t handle );
	void UpdateBrushShadow( IClientRenderable *pRenderable, ClientShadowHandle_t handle );
	void UpdateShadow( ClientShadowHandle_t handle, bool force );

	// Updates shadow cast direction when shadowing from world lights
	void UpdateShadowDirectionFromLocalLightSource( ClientShadowHandle_t shadowHandle );

	// Gets the entity whose shadow this shadow will render into
	IClientRenderable *GetParentShadowEntity( ClientShadowHandle_t handle );

	// Adds the child bounds to the bounding box
	void AddChildBounds( matrix3x4_t &matWorldToBBox, IClientRenderable* pParent, Vector &vecMins, Vector &vecMaxs );

	// Compute a bounds for the entity + children
	void ComputeHierarchicalBounds( IClientRenderable *pRenderable, Vector &vecMins, Vector &vecMaxs );

	// Builds matrices transforming from world space to shadow space
	void BuildGeneralWorldToShadowMatrix( VMatrix& matWorldToShadow,
		const Vector& origin, const Vector& dir, const Vector& xvec, const Vector& yvec );

	void BuildWorldToShadowMatrix( VMatrix& matWorldToShadow, const Vector& origin, const Quaternion& quatOrientation );

	void BuildPerspectiveWorldToFlashlightMatrix( VMatrix& matWorldToShadow, const FlashlightState_t &flashlightState );

	void BuildOrthoWorldToFlashlightMatrix( VMatrix& matWorldToShadow, const FlashlightState_t &flashlightState );

	// Update a shadow
	void UpdateProjectedTextureInternal( ClientShadowHandle_t handle, bool force );

	// Compute the shadow origin and attenuation start distance
	float ComputeLocalShadowOrigin( IClientRenderable* pRenderable, 
		const Vector& mins, const Vector& maxs, const Vector& localShadowDir, float backupFactor, Vector& origin );

	// Remove a shadow from the dirty list
	void RemoveShadowFromDirtyList( ClientShadowHandle_t handle );

	// NOTE: this will ONLY return SHADOWS_NONE, SHADOWS_SIMPLE, or SHADOW_RENDER_TO_TEXTURE.
	ShadowType_t GetActualShadowCastType( ClientShadowHandle_t handle ) const;
	ShadowType_t GetActualShadowCastType( IClientRenderable *pRenderable ) const;

	// Builds a simple blobby shadow
	void BuildOrthoShadow( IClientRenderable* pRenderable, ClientShadowHandle_t handle, const Vector& mins, const Vector& maxs);

	// Builds a more complex shadow...
	void BuildRenderToTextureShadow( IClientRenderable* pRenderable, 
			ClientShadowHandle_t handle, const Vector& mins, const Vector& maxs );

	// Build a projected-texture flashlight
	void BuildFlashlight( ClientShadowHandle_t handle );

	// Does all the lovely stuff we need to do to have render-to-texture shadows
	void SetupRenderToTextureShadow( ClientShadowHandle_t h );
	void CleanUpRenderToTextureShadow( ClientShadowHandle_t h );

	// Compute the extra shadow planes
	void ComputeExtraClipPlanes( IClientRenderable* pRenderable, 
		ClientShadowHandle_t handle, const Vector* vec, 
		const Vector& mins, const Vector& maxs, const Vector& localShadowDir );

	// Set extra clip planes related to shadows...
	void ClearExtraClipPlanes( ClientShadowHandle_t h );
	void AddExtraClipPlane( ClientShadowHandle_t h, const Vector& normal, float dist );

	// Cull if the origin is on the wrong side of a shadow clip plane....
	bool CullReceiver( ClientShadowHandle_t handle, IClientRenderable* pRenderable, IClientRenderable* pSourceRenderable );

	bool ComputeSeparatingPlane( IClientRenderable* pRend1, IClientRenderable* pRend2, cplane_t* pPlane );

	// Causes all shadows to be re-updated
	void UpdateAllShadows();

	void RemoveAllShadowDecals();

	// One of these gets called with every shadow that potentially will need to re-render
	bool DrawRenderToTextureShadow( int nSlot, unsigned short clientShadowHandle, float flArea );
	void DrawRenderToTextureShadowLOD( int nSlot, unsigned short clientShadowHandle );

	// Draws all children shadows into our own
	bool DrawShadowHierarchy( IClientRenderable *pRenderable, const ClientShadow_t &shadow, bool bChild = false );

	// Setup stage for threading
	bool BuildSetupListForRenderToTextureShadow( unsigned short clientShadowHandle, float flArea );
	bool BuildSetupShadowHierarchy( IClientRenderable *pRenderable, const ClientShadow_t &shadow, bool bChild = false );

	// Computes + sets the render-to-texture texcoords
	void SetRenderToTextureShadowTexCoords( ShadowHandle_t handle, int x, int y, int w, int h );
	void SetRenderToTextureShadowTexCoords( ClientShadow_t& shadow, int x, int y, int w, int h );

	// Visualization....
	void DrawRenderToTextureDebugInfo( IClientRenderable* pRenderable, const Vector& mins, const Vector& maxs );

	// Advance frame
	void AdvanceFrame();

	// Returns renderable-specific shadow info
	float GetShadowDistance( IClientRenderable *pRenderable ) const;
	const Vector &GetShadowDirection( IClientRenderable *pRenderable ) const;

	const Vector &GetShadowDirection( ClientShadowHandle_t shadowHandle ) const;

	// Initialize, shutdown render-to-texture shadows
	void InitDepthTextureShadows();
	void ShutdownDepthTextureShadows();

	// Initialize, shutdown render-to-texture shadows
	void InitRenderToTextureShadows();
	void ShutdownRenderToTextureShadows();

	// Initialize, shutdown deferred render-to-texture shadows
	void InitDeferredShadows();
	void ShutdownDeferredShadows();

	void ShutdownRenderTargets( void );
	static bool ShadowHandleCompareFunc( const ClientShadowHandle_t& lhs, const ClientShadowHandle_t& rhs )
	{
		return lhs < rhs;
	}

	ClientShadowHandle_t CreateProjectedTexture( ClientEntityHandle_t entity, int nEntIndex, int flags, CBitVec< MAX_SPLITSCREEN_PLAYERS > *pSplitScreenBits );

	// Lock down the usage of a shadow depth texture...must be unlocked use on subsequent views / frames
	bool	LockShadowDepthTexture( CTextureReference *shadowDepthTexture, int nStartTexture );
	void	UnlockAllShadowDepthTextures();

	// Set and clear flashlight target renderable
	void	SetFlashlightTarget( ClientShadowHandle_t shadowHandle, EHANDLE targetEntity );

	// Set flashlight light world flag
	void	SetFlashlightLightWorld( ClientShadowHandle_t shadowHandle, bool bLightWorld );

	bool	IsFlashlightTarget( ClientShadowHandle_t shadowHandle, IClientRenderable *pRenderable );

	// Builds a list of active shadows requiring shadow depth renders
	int		BuildActiveShadowDepthList( const CViewSetup &viewSetup, int nMaxDepthShadows, ClientShadowHandle_t *pActiveDepthShadows, int &nNumHighRes );

	// Builds a list of active flashlights
	int		BuildActiveFlashlightList( const CViewSetup &viewSetup, int nMaxFlashlights, ClientShadowHandle_t *pActiveFlashlights );

	// Sets the view's active flashlight render state
	void	SetViewFlashlightState( int nActiveFlashlightCount, ClientShadowHandle_t* pActiveFlashlights );

	// Draw flashlight wireframe using debug overlay
	void	DrawFrustum( const Vector &vOrigin, const VMatrix &matWorldToFlashlight );

	// Draw uberlight rig in wireframe using debug overlay
	void	DrawUberlightRig( const Vector &vOrigin, const VMatrix &matWorldToFlashlight, FlashlightState_t state );

	// Called from PreRender to work through the dirty shadow set
	void	UpdateDirtyShadows();
	void	UpdateDirtyShadowsHalfRate();
	void	UpdateDirtyShadow( ClientShadowHandle_t handle );
	void	FlushLeftOverDirtyShadows();

	void	QueueShadowForDestruction( ClientShadowHandle_t handle );
	void	DestroyQueuedShadows();

	// Deferred RTT shadow rendering support
	static void BuildCubeWithDegenerateEdgeQuads( CMeshBuilder& meshBuilder, const matrix3x4_t& objToWorld, const VMatrix& projToShadow, const CClientShadowMgr::ClientShadow_t& shadow );
	bool SetupDeferredShadow( const ClientShadow_t& shadow, const Vector& camPos, matrix3x4_t* pObjToWorldMat ) const;
	void DownsampleDepthBuffer( IMatRenderContext* pRenderContext, const VMatrix& invViewProjMat );
	void CompositeDeferredShadows( IMatRenderContext* pRenderContext );
	static void ComputeFalloffInfo( const ClientShadow_t& shadow, Vector* pShadowFalloffParams );

private:
	Vector	m_SimpleShadowDir;
	color32	m_AmbientLightColor;
	CMaterialReference m_SimpleShadow;
	CMaterialReference m_RenderShadow;
	CMaterialReference m_RenderModelShadow;
	CMaterialReference m_RenderDeferredShadowMat;
	CMaterialReference m_RenderDeferredSimpleShadowMat;
	CTextureReference m_DummyColorTexture;
	CUtlLinkedList< ClientShadow_t, ClientShadowHandle_t >	m_Shadows;
	CTextureAllocator m_ShadowAllocator;

	bool m_RenderToTextureActive;
	bool m_bRenderTargetNeedsClear;
	bool m_bUpdatingDirtyShadows;
	float m_flShadowCastDist;
	float m_flMinShadowArea;

	typedef CUtlRBTree< ClientShadowHandle_t, unsigned short > ClientShadowHandleSet;
	ClientShadowHandleSet	m_DirtyShadows;
	ClientShadowHandleSet	m_DirtyShadowsLeftOver;	// shadows left over to update from previous frame

	CUtlVector< ClientShadowHandle_t > m_TransparentShadows;
	CUtlVector< ClientShadowHandle_t > m_shadowsToDestroy;

	int m_nPrevFrameCount;

	// These members maintain current state of depth texturing (size and global active state)
	// If either changes in a frame, PreRender() will catch it and do the appropriate allocation, deallocation or reallocation
	bool m_bDepthTextureActive;
	int m_nDepthTextureResolution; // Assume square (height == width)
	int m_nDepthTextureResolutionHigh; // Assume square (height == width)
	int m_nLowResStart; // Place in the shadow render target where the low res shadows start

	bool m_bDepthTexturesAllocated;
	CUtlVector< CTextureReference > m_DepthTextureCache;
	CUtlVector< bool > m_DepthTextureCacheLocks;
	int	m_nMaxDepthTextureShadows;

	bool m_bShadowFromWorldLights;
	bool m_bSuppressShadowFromWorldLights;

	friend class CVisibleShadowList;
	friend class CVisibleShadowFrustumList;

	CTextureReference m_downSampledNormals;
	CTextureReference m_downSampledDepth;

	void CalculateRenderTargetsAndSizes( void );
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CClientShadowMgr s_ClientShadowMgr;
IClientShadowMgr* g_pClientShadowMgr = &s_ClientShadowMgr;


//-----------------------------------------------------------------------------
// Builds a list of potential shadows that lie within our PVS + view frustum
//-----------------------------------------------------------------------------
struct VisibleShadowInfo_t
{
	ClientShadowHandle_t	m_hShadow;
	float					m_flArea;
	Vector					m_vecAbsCenter;
};

class CVisibleShadowList : public IClientLeafShadowEnum
{
public:

	CVisibleShadowList();
	int FindShadows( const CViewSetup *pView, int nLeafCount, WorldListLeafData_t *pLeafList );
	int GetVisibleShadowCount() const;
	int GetVisibleBlobbyShadowCount() const;

	const VisibleShadowInfo_t &GetVisibleShadow( int i ) const;
	const VisibleShadowInfo_t &GetVisibleBlobbyShadow( int i ) const;

private:
	void EnumShadow( unsigned short clientShadowHandle );
	float ComputeScreenArea( const Vector &vecCenter, float r ) const;
	void PrioritySort();

	CUtlVector<VisibleShadowInfo_t> m_ShadowsInView;
	CUtlVector<VisibleShadowInfo_t> m_BlobbyShadowsInView;
	CUtlVector<int>	m_PriorityIndex;
};


//-----------------------------------------------------------------------------
// Singleton instances of shadow and shadow frustum lists
//-----------------------------------------------------------------------------
static CVisibleShadowList			s_VisibleShadowList;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CUtlVector<C_BaseAnimating *> s_NPCShadowBoneSetups;
static CUtlVector<C_BaseAnimating *> s_NonNPCShadowBoneSetups;

//-----------------------------------------------------------------------------
// CVisibleShadowList - Constructor and Accessors
//-----------------------------------------------------------------------------
CVisibleShadowList::CVisibleShadowList() : m_ShadowsInView( 0, 64 ), m_PriorityIndex( 0, 64 ) 
{
}

int CVisibleShadowList::GetVisibleShadowCount() const
{
	return m_ShadowsInView.Count();
}

const VisibleShadowInfo_t &CVisibleShadowList::GetVisibleShadow( int i ) const
{
	return m_ShadowsInView[m_PriorityIndex[i]];
}

int CVisibleShadowList::GetVisibleBlobbyShadowCount() const
{
	return m_BlobbyShadowsInView.Count();
}

const VisibleShadowInfo_t &CVisibleShadowList::GetVisibleBlobbyShadow( int i ) const
{
	return m_BlobbyShadowsInView[i];
}

//-----------------------------------------------------------------------------
// CVisibleShadowList - Computes approximate screen area of the shadow
//-----------------------------------------------------------------------------
float CVisibleShadowList::ComputeScreenArea( const Vector &vecCenter, float r ) const
{
	CMatRenderContextPtr pRenderContext( materials );
	float flScreenDiameter = pRenderContext->ComputePixelDiameterOfSphere( vecCenter, r );
	return flScreenDiameter * flScreenDiameter;
}


//-----------------------------------------------------------------------------
// CVisibleShadowList - Visits every shadow in the list of leaves
//-----------------------------------------------------------------------------
void CVisibleShadowList::EnumShadow( unsigned short clientShadowHandle )
{
	CClientShadowMgr::ClientShadow_t& shadow = s_ClientShadowMgr.m_Shadows[clientShadowHandle];

	// Don't bother if we rendered it this frame, no matter which view it was rendered for
	if ( shadow.m_nRenderFrame == gpGlobals->framecount )
		return;

	// Don't bother with flashlights
	if ( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION )) != 0 )
		return;

	// We don't need to bother with it if it's not render-to-texture
	ShadowType_t shadowType = s_ClientShadowMgr.GetActualShadowCastType( clientShadowHandle );
	if ( shadowType != SHADOWS_RENDER_TO_TEXTURE && shadowType != SHADOWS_SIMPLE )
		return;

	// Don't bother with it if the shadow is totally transparent
	if ( shadow.m_FalloffBias == 255 )
		return;

	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );
	Assert( pRenderable );

	// Don't bother with children of hierarchy; they will be drawn with their parents
	if ( s_ClientShadowMgr.ShouldUseParentShadow( pRenderable ) || s_ClientShadowMgr.WillParentRenderBlobbyShadow( pRenderable ) )
		return;

	// Compute a sphere surrounding the shadow
	// FIXME: This doesn't account for children of hierarchy... too bad!
	Vector vecAbsCenter;
	float flRadius;
	s_ClientShadowMgr.ComputeBoundingSphere( pRenderable, vecAbsCenter, flRadius );

	// Compute a box surrounding the shadow
	Vector vecAbsMins, vecAbsMaxs;
	s_ClientShadowMgr.ComputeShadowBBox( pRenderable, clientShadowHandle, vecAbsCenter, flRadius, &vecAbsMins, &vecAbsMaxs );

	// FIXME: Add distance check here?

	// Make sure it's in the frustum. If it isn't it's not interesting
	if (engine->CullBox( vecAbsMins, vecAbsMaxs ))
		return;

	if ( shadowType == SHADOWS_RENDER_TO_TEXTURE )
	{
		int i = m_ShadowsInView.AddToTail( );
		VisibleShadowInfo_t &info = m_ShadowsInView[i];

		info.m_hShadow = clientShadowHandle;
		info.m_flArea = ComputeScreenArea( vecAbsCenter, flRadius );

		// Har, har. When water is rendering (or any multipass technique), 
		// we may well initially render from a viewpoint which doesn't include this shadow. 
		// That doesn't mean we shouldn't check it again though. Sucks that we need to compute
		// the sphere + bbox multiply times though.
		shadow.m_nRenderFrame = gpGlobals->framecount;
	}
	else
	{
		int i = m_BlobbyShadowsInView.AddToTail( );
		VisibleShadowInfo_t &info = m_BlobbyShadowsInView[i];

		info.m_hShadow = clientShadowHandle;
		info.m_flArea = 0.0f;
	}
}


//-----------------------------------------------------------------------------
// CVisibleShadowList - Sort based on screen area/priority
//-----------------------------------------------------------------------------
void CVisibleShadowList::PrioritySort()
{
	int nCount = m_ShadowsInView.Count();
	m_PriorityIndex.EnsureCapacity( nCount );

	m_PriorityIndex.RemoveAll();

	int i, j;
	for ( i = 0; i < nCount; ++i )
	{
		m_PriorityIndex.AddToTail(i);
	}

	for ( i = 0; i < nCount - 1; ++i )
	{
		int nLargestInd = i;
		float flLargestArea = m_ShadowsInView[m_PriorityIndex[i]].m_flArea;
		for ( j = i + 1; j < nCount; ++j )
		{
			int nIndex = m_PriorityIndex[j];
			if ( flLargestArea < m_ShadowsInView[nIndex].m_flArea )
			{
				nLargestInd = j;
				flLargestArea = m_ShadowsInView[nIndex].m_flArea;
			}
		}
		V_swap( m_PriorityIndex[i], m_PriorityIndex[nLargestInd] );
	}
}


//-----------------------------------------------------------------------------
// CVisibleShadowList - Main entry point for finding shadows in the leaf list
//-----------------------------------------------------------------------------
int CVisibleShadowList::FindShadows( const CViewSetup *pView, int nLeafCount, WorldListLeafData_t *pLeafList )
{
	VPROF_BUDGET( "CVisibleShadowList::FindShadows", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	m_ShadowsInView.RemoveAll();
	m_BlobbyShadowsInView.RemoveAll();
	ClientLeafSystem()->EnumerateShadowsInLeaves( nLeafCount, pLeafList, this );

	int nCount = m_ShadowsInView.Count();
	if (nCount != 0)
	{
		// Sort based on screen area/priority
		PrioritySort();
	}
	return nCount;
}



// sniff the command line parameters, etc. to determine how many shadow rt's and their dimensions
void CClientShadowMgr::CalculateRenderTargetsAndSizes( void )
{
	bool bTools = CommandLine()->CheckParm( "-tools" ) != NULL;
		
	m_nDepthTextureResolution = r_flashlightdepthres.GetInt();
	m_nDepthTextureResolutionHigh = r_flashlightdepthreshigh.GetInt();
	if ( bTools )									// Higher resolution shadow maps in tools mode
	{
		char defaultRes[] = "2048";
		m_nDepthTextureResolution = atoi( CommandLine()->ParmValue( "-sfm_shadowmapres", defaultRes ) );
	}
	m_nMaxDepthTextureShadows = bTools ? MAX_DEPTH_TEXTURE_SHADOWS_TOOLS : MAX_DEPTH_TEXTURE_SHADOWS;	// Just one shadow depth texture in games, more in tools
}
//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CClientShadowMgr::CClientShadowMgr() :
	m_DirtyShadows( 0, 0, ShadowHandleCompareFunc ),
	m_DirtyShadowsLeftOver( 0, 0, ShadowHandleCompareFunc ),
	m_nPrevFrameCount( -1 ),
	m_RenderToTextureActive( false ),
	m_bDepthTextureActive( false ),
	m_bDepthTexturesAllocated( false ),
	m_bShadowFromWorldLights( false ),
	m_bSuppressShadowFromWorldLights( false )
{

	
}


//-----------------------------------------------------------------------------
// Changes the shadow direction...
//-----------------------------------------------------------------------------
CON_COMMAND_F( r_shadowdir, "Set shadow direction", FCVAR_CHEAT )
{
	Vector dir;
	if ( args.ArgC() == 1 )
	{
		Vector dir = s_ClientShadowMgr.GetShadowDirection();
		Msg( "%.2f %.2f %.2f\n", dir.x, dir.y, dir.z );
		return;
	}

	if ( args.ArgC() == 4 )
	{
		dir.x = atof( args[1] );
		dir.y = atof( args[2] );
		dir.z = atof( args[3] );
		s_ClientShadowMgr.SetShadowDirection(dir);
	}
}

CON_COMMAND_F( r_shadowangles, "Set shadow angles", FCVAR_CHEAT )
{
	Vector dir;
	QAngle angles;
	if (args.ArgC() == 1)
	{
		Vector dir = s_ClientShadowMgr.GetShadowDirection();
		QAngle angles;
		VectorAngles( dir, angles );
		Msg( "%.2f %.2f %.2f\n", angles.x, angles.y, angles.z );
		return;
	}

	if (args.ArgC() == 4)
	{
		angles.x = atof( args[1] );
		angles.y = atof( args[2] );
		angles.z = atof( args[3] );
		AngleVectors( angles, &dir );
		s_ClientShadowMgr.SetShadowDirection(dir);
	}
}

CON_COMMAND_F( r_shadowcolor, "Set shadow color", FCVAR_CHEAT )
{
	if (args.ArgC() == 1)
	{
		unsigned char r, g, b;
		s_ClientShadowMgr.GetShadowColor( &r, &g, &b );
		Msg( "Shadow color %d %d %d\n", r, g, b );
		return;
	}

	if (args.ArgC() == 4)
	{
		int r = atoi( args[1] );
		int g = atoi( args[2] );
		int b = atoi( args[3] );
		s_ClientShadowMgr.SetShadowColor(r, g, b);
	}
}

CON_COMMAND_F( r_shadowdist, "Set shadow distance", FCVAR_CHEAT )
{
	if (args.ArgC() == 1)
	{
		float flDist = s_ClientShadowMgr.GetShadowDistance( );
		Msg( "Shadow distance %.2f\n", flDist );
		return;
	}

	if (args.ArgC() == 2)
	{
		float flDistance = atof( args[1] );
		s_ClientShadowMgr.SetShadowDistance( flDistance );
	}
}

CON_COMMAND_F( r_shadowblobbycutoff, "some shadow stuff", FCVAR_CHEAT )
{
	if (args.ArgC() == 1)
	{
		float flArea = s_ClientShadowMgr.GetBlobbyCutoffArea( );
		Msg( "Cutoff area %.2f\n", flArea );
		return;
	}

	if (args.ArgC() == 2)
	{
		float flArea = atof( args[1] );
		s_ClientShadowMgr.SetShadowBlobbyCutoffArea( flArea );
	}
}

void OnShadowFromWorldLights( IConVar *var, const char *pOldValue, float flOldValue );
static ConVar r_shadowfromworldlights( "r_shadowfromworldlights", "1", FCVAR_NONE, "Enable shadowing from world lights", OnShadowFromWorldLights );
void OnShadowFromWorldLights( IConVar *var, const char *pOldValue, float flOldValue )
{
	s_ClientShadowMgr.SuppressShadowFromWorldLights( !r_shadowfromworldlights.GetBool() );
}

static void ShadowRestoreFunc( int nChangeFlags )
{
	s_ClientShadowMgr.RestoreRenderState();
}

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CClientShadowMgr::Init() 
{
	return true;
}

void CClientShadowMgr::InitRenderTargets()
{
	m_bRenderTargetNeedsClear = false;
	m_SimpleShadow.Init( "decals/simpleshadow", TEXTURE_GROUP_DECAL );

	Vector dir( 0.1, 0.1, -1 );
	SetShadowDirection(dir);
	SetShadowDistance( 50 );

	SetShadowBlobbyCutoffArea( 0.005 );


	if ( r_shadowrendertotexture.GetBool() )
	{
		InitRenderToTextureShadows();
	}

	// If someone turned shadow depth mapping on but we can't do it, force it off
	if ( r_flashlightdepthtexture.GetBool() && !g_pMaterialSystemHardwareConfig->SupportsShadowDepthTextures() )
	{
		r_flashlightdepthtexture.SetValue( 0 );
		ShutdownDepthTextureShadows();	
	}

	InitDepthTextureShadows();

	r_flashlightdepthres.SetValue( m_nDepthTextureResolution );
	r_flashlightdepthreshigh.SetValue( m_nDepthTextureResolutionHigh );

	if ( m_DepthTextureCache.Count() )
	{
		bool bTools = CommandLine()->CheckParm( "-tools" ) != NULL;
		int nNumShadows = bTools ? MAX_DEPTH_TEXTURE_SHADOWS_TOOLS : MAX_DEPTH_TEXTURE_SHADOWS;
		m_nLowResStart = bTools ? MAX_DEPTH_TEXTURE_HIGHRES_SHADOWS_TOOLS : MAX_DEPTH_TEXTURE_HIGHRES_SHADOWS;

		if ( m_nLowResStart > nNumShadows )
		{
			// All shadow slots filled with high res
			m_nLowResStart = 0;
		}

		// Shadow may be resized during allocation (due to resolution constraints etc)
		m_nDepthTextureResolution = m_DepthTextureCache[ m_nLowResStart ]->GetActualWidth();
		r_flashlightdepthres.SetValue( m_nDepthTextureResolution );

		m_nDepthTextureResolutionHigh = m_DepthTextureCache[ 0 ]->GetActualWidth();
		r_flashlightdepthreshigh.SetValue( m_nDepthTextureResolutionHigh );
	}
	InitDeferredShadows();

	materials->AddRestoreFunc( ShadowRestoreFunc );
}

void CClientShadowMgr::ShutdownRenderTargets( void )
{
	if ( materials )										// ugh - this gets called during program shutdown, but with no mat system
	{
		materials->RemoveRestoreFunc( ShadowRestoreFunc );
	}
}

void CClientShadowMgr::Shutdown()
{
	ShutdownRenderTargets();
	m_SimpleShadow.Shutdown();
	m_Shadows.RemoveAll();
	ShutdownRenderToTextureShadows();

	ShutdownDepthTextureShadows();

	ShutdownDeferredShadows();

}


//-----------------------------------------------------------------------------
// Initialize, shutdown depth-texture shadows
//-----------------------------------------------------------------------------
void CClientShadowMgr::InitDepthTextureShadows()
{
	VPROF_BUDGET( "CClientShadowMgr::InitDepthTextureShadows", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	if ( m_bDepthTextureActive )
		return;
	
	m_bDepthTextureActive = true;

	if ( !r_flashlightdepthtexture.GetBool() )
		return;

	if( !m_bDepthTexturesAllocated || m_nDepthTextureResolution != r_flashlightdepthres.GetInt() || m_nDepthTextureResolutionHigh != r_flashlightdepthreshigh.GetInt() )
	{
		CalculateRenderTargetsAndSizes();
		m_bDepthTexturesAllocated = true;

		ImageFormat dstFormat  = g_pMaterialSystemHardwareConfig->GetShadowDepthTextureFormat();	// Vendor-dependent depth texture format
#if !defined( _X360 )
		ImageFormat nullFormat = g_pMaterialSystemHardwareConfig->GetNullTextureFormat();			// Vendor-dependent null texture format (takes as little memory as possible)
#endif
		materials->BeginRenderTargetAllocation();

#if defined( _X360 )
		// For the 360, we'll be rendering depth directly into the dummy depth and Resolve()ing to the depth texture.
		// only need the dummy surface, don't care about color results
		m_DummyColorTexture.InitRenderTargetTexture( 1, 1, RT_SIZE_OFFSCREEN, IMAGE_FORMAT_BGR565, MATERIAL_RT_DEPTH_SHARED, false, "_rt_ShadowDummy" );
		m_DummyColorTexture.InitRenderTargetSurface( m_nDepthTextureResolution, m_nDepthTextureResolution, IMAGE_FORMAT_BGR565, false );
#else
		m_DummyColorTexture.InitRenderTarget( m_nDepthTextureResolution, m_nDepthTextureResolution, RT_SIZE_OFFSCREEN, nullFormat, MATERIAL_RT_DEPTH_NONE, false, "_rt_ShadowDummy" );
#endif

		// Create some number of depth-stencil textures
		m_DepthTextureCache.Purge();
		m_DepthTextureCacheLocks.Purge();
		for( int i=0; i < m_nMaxDepthTextureShadows; i++ )
		{
			CTextureReference depthTex;	// Depth-stencil surface
			bool bFalse = false;

			char strRTName[64];
			sprintf( strRTName, "_rt_ShadowDepthTexture_%d", i );

			int nTextureResolution = ( i < MAX_DEPTH_TEXTURE_HIGHRES_SHADOWS ? m_nDepthTextureResolutionHigh : m_nDepthTextureResolution );

#if defined( _X360 )
			// create a render target to use as a resolve target to get the shared depth buffer
			// surface is effectively never used
			depthTex.InitRenderTargetTexture( nTextureResolution, nTextureResolution, RT_SIZE_OFFSCREEN, dstFormat, MATERIAL_RT_DEPTH_NONE, false, strRTName );
			depthTex.InitRenderTargetSurface( 1, 1, dstFormat, false );
#else
			depthTex.InitRenderTarget( nTextureResolution, nTextureResolution, RT_SIZE_OFFSCREEN, dstFormat, MATERIAL_RT_DEPTH_NONE, false, strRTName );
#endif

			m_DepthTextureCache.AddToTail( depthTex );
			m_DepthTextureCacheLocks.AddToTail( bFalse );
		}

		materials->EndRenderTargetAllocation();
	}
}

void CClientShadowMgr::ShutdownDepthTextureShadows() 
{
	if( m_bDepthTexturesAllocated )
	{
		// Shut down the dummy texture
		m_DummyColorTexture.Shutdown();

		while( m_DepthTextureCache.Count() )
		{
			m_DepthTextureCache[ m_DepthTextureCache.Count()-1 ].Shutdown();

			m_DepthTextureCacheLocks.Remove( m_DepthTextureCache.Count()-1 );
			m_DepthTextureCache.Remove( m_DepthTextureCache.Count()-1 );
		}

		m_bDepthTexturesAllocated = false;
	}
	m_bDepthTextureActive = false;
}

//-----------------------------------------------------------------------------
// Initialize, shutdown render-to-texture shadows
//-----------------------------------------------------------------------------
void CClientShadowMgr::InitRenderToTextureShadows()
{
	if ( !m_RenderToTextureActive )
	{
		m_RenderToTextureActive = true;

		g_pMaterialSystem->BeginRenderTargetAllocation();
		m_ShadowAllocator.Init();
		g_pMaterialSystem->EndRenderTargetAllocation();

		m_RenderShadow.Init( "decals/rendershadow", TEXTURE_GROUP_DECAL );
		m_RenderModelShadow.Init( "decals/rendermodelshadow", TEXTURE_GROUP_DECAL );

		m_ShadowAllocator.Reset();
		m_bRenderTargetNeedsClear = true;

		float fr = (float)m_AmbientLightColor.r / 255.0f;
		float fg = (float)m_AmbientLightColor.g / 255.0f;
		float fb = (float)m_AmbientLightColor.b / 255.0f;
		m_RenderShadow->ColorModulate( fr, fg, fb );
		m_RenderModelShadow->ColorModulate( fr, fg, fb );

		// Iterate over all existing textures and allocate shadow textures
		for (ClientShadowHandle_t i = m_Shadows.Head(); i != m_Shadows.InvalidIndex(); i = m_Shadows.Next(i) )
		{
			ClientShadow_t& shadow = m_Shadows[i];
			if ( shadow.m_Flags & SHADOW_FLAGS_USE_RENDER_TO_TEXTURE )
			{
				SetupRenderToTextureShadow( i );
				MarkRenderToTextureShadowDirty( i );

				// Switch the material to use render-to-texture shadows
				shadowmgr->SetShadowMaterial( shadow.m_ShadowHandle, m_RenderShadow, m_RenderModelShadow, (void*)i );
			}
		}
	}
}

void CClientShadowMgr::ShutdownRenderToTextureShadows()
{
	if (m_RenderToTextureActive)
	{
		// Iterate over all existing textures and deallocate shadow textures
		for (ClientShadowHandle_t i = m_Shadows.Head(); i != m_Shadows.InvalidIndex(); i = m_Shadows.Next(i) )
		{
			CleanUpRenderToTextureShadow( i );

			// Switch the material to use blobby shadows
			ClientShadow_t& shadow = m_Shadows[i];

			shadowmgr->SetShadowMaterial( shadow.m_ShadowHandle, m_SimpleShadow, m_SimpleShadow, (void*)CLIENTSHADOW_INVALID_HANDLE );
			shadowmgr->SetShadowTexCoord( shadow.m_ShadowHandle, 0, 0, 1, 1 );
			ClearExtraClipPlanes( i );
		}

		m_RenderShadow.Shutdown();
		m_RenderModelShadow.Shutdown();

		m_ShadowAllocator.DeallocateAllTextures();
		m_ShadowAllocator.Shutdown();

		// Cause the render target to go away
		materials->UncacheUnusedMaterials();

		m_RenderToTextureActive = false;
	}
}

#define DEFERRED_SHADOW_BUFFER_WIDTH 320
#define DEFERRED_SHADOW_BUFFER_HEIGHT 180

void CClientShadowMgr::InitDeferredShadows()
{
	if ( IsX360() )
	{
		m_RenderDeferredShadowMat.Init( "engine/renderdeferredshadow", TEXTURE_GROUP_OTHER );
		m_RenderDeferredSimpleShadowMat.Init( "engine/renderdeferredsimpleshadow", TEXTURE_GROUP_OTHER );
	}

	if ( r_shadow_deferred_downsample.GetBool() )
	{
#if defined( _X360 )
		m_downSampledNormals.InitRenderTargetTexture( DEFERRED_SHADOW_BUFFER_WIDTH, DEFERRED_SHADOW_BUFFER_HEIGHT, RT_SIZE_OFFSCREEN, IMAGE_FORMAT_ARGB8888, MATERIAL_RT_DEPTH_SEPARATE, false, "_rt_DownsampledNormals" );
		m_downSampledNormals.InitRenderTargetSurface( DEFERRED_SHADOW_BUFFER_WIDTH, DEFERRED_SHADOW_BUFFER_HEIGHT, IMAGE_FORMAT_ARGB8888, true );
		m_downSampledDepth.InitRenderTargetTexture( DEFERRED_SHADOW_BUFFER_WIDTH, DEFERRED_SHADOW_BUFFER_HEIGHT, RT_SIZE_OFFSCREEN, IMAGE_FORMAT_D24FS8, MATERIAL_RT_DEPTH_NONE, false, "_rt_DownsampledDepth" );
#endif
	}
}

void CClientShadowMgr::ShutdownDeferredShadows()
{
	m_RenderDeferredShadowMat.Shutdown();
	m_RenderDeferredSimpleShadowMat.Shutdown();

	m_downSampledNormals.Shutdown();
	m_downSampledDepth.Shutdown();
}

//-----------------------------------------------------------------------------
// Sets the shadow color
//-----------------------------------------------------------------------------
void CClientShadowMgr::SetShadowColor( unsigned char r, unsigned char g, unsigned char b )
{
	float fr = (float)r / 255.0f;
	float fg = (float)g / 255.0f;
	float fb = (float)b / 255.0f;

	// Hook the shadow color into the shadow materials
	m_SimpleShadow->ColorModulate( fr, fg, fb );

	if (m_RenderToTextureActive)
	{
		if ( m_RenderShadow )
		{
			m_RenderShadow->ColorModulate( fr, fg, fb );
		}
		if ( m_RenderModelShadow )
		{
			m_RenderModelShadow->ColorModulate( fr, fg, fb );
		}

		if ( IsX360() )
		{
			m_RenderDeferredShadowMat->ColorModulate( fr, fg, fb );
			m_RenderDeferredSimpleShadowMat->ColorModulate( fr, fg, fb );
		}
	}

	m_AmbientLightColor.r = r;
	m_AmbientLightColor.g = g;
	m_AmbientLightColor.b = b;
}

void CClientShadowMgr::GetShadowColor( unsigned char *r, unsigned char *g, unsigned char *b ) const
{
	*r = m_AmbientLightColor.r;
	*g = m_AmbientLightColor.g;
	*b = m_AmbientLightColor.b;
}


//-----------------------------------------------------------------------------
// Level init... get the shadow color
//-----------------------------------------------------------------------------
void CClientShadowMgr::LevelInitPreEntity()
{
	m_bUpdatingDirtyShadows = false;

	// Default setting for this, can be overridden by shadow control entities
	SetShadowFromWorldLightsEnabled( true );

	Vector ambientColor;
	engine->GetAmbientLightColor( ambientColor );
	ambientColor *= 3;
	ambientColor += Vector( 0.3f, 0.3f, 0.3f );

	unsigned char r = ambientColor[0] > 1.0 ? 255 : 255 * ambientColor[0];
	unsigned char g = ambientColor[1] > 1.0 ? 255 : 255 * ambientColor[1];
	unsigned char b = ambientColor[2] > 1.0 ? 255 : 255 * ambientColor[2];

	SetShadowColor(r, g, b);

	// Set up the texture allocator
	if ( m_RenderToTextureActive )
	{
		m_ShadowAllocator.Reset();
		m_bRenderTargetNeedsClear = true;
	}
}


//-----------------------------------------------------------------------------
// Clean up all shadows
//-----------------------------------------------------------------------------
void CClientShadowMgr::LevelShutdownPostEntity()
{
	// Paranoid code to make sure all flashlights are deactivated.
	// This should happen in the C_BasePlayer destructor, but I'm turning everything off to release the
	// flashlight shadows just in case.
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; i++ )
	{
		FlashlightEffectManager( i ).TurnOffFlashlight( true );
	}

	// All shadows *should* have been cleaned up when the entities went away
	// but, just in case....
	Assert( m_Shadows.Count() == 0 );

	ClientShadowHandle_t h = m_Shadows.Head();
	while (h != CLIENTSHADOW_INVALID_HANDLE)
	{
		ClientShadowHandle_t next = m_Shadows.Next(h);
		DestroyShadow( h );
		h = next;
	}

	// Deallocate all textures
	if (m_RenderToTextureActive)
	{
		m_ShadowAllocator.DeallocateAllTextures();
	}

	r_shadows_gamecontrol.SetValue( -1 );
}


//-----------------------------------------------------------------------------
// Deals with alt-tab
//-----------------------------------------------------------------------------
void CClientShadowMgr::RestoreRenderState()
{
	// Mark all shadows dirty; they need to regenerate their state
	ClientShadowHandle_t h;
	for ( h = m_Shadows.Head(); h != m_Shadows.InvalidIndex(); h = m_Shadows.Next(h) )
	{
		m_Shadows[h].m_Flags |= SHADOW_FLAGS_TEXTURE_DIRTY;
	}

	SetShadowColor( m_AmbientLightColor.r, m_AmbientLightColor.g, m_AmbientLightColor.b );
	m_bRenderTargetNeedsClear = true;
}


//-----------------------------------------------------------------------------
// Does all the lovely stuff we need to do to have render-to-texture shadows
//-----------------------------------------------------------------------------
void CClientShadowMgr::SetupRenderToTextureShadow( ClientShadowHandle_t h )
{
	// First, compute how much texture memory we want to use.
	ClientShadow_t& shadow = m_Shadows[h];
	
	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );
	if ( !pRenderable )
		return;

	Vector mins, maxs;
	pRenderable->GetShadowRenderBounds( mins, maxs, GetActualShadowCastType( h ) );

	// Compute the maximum dimension
	Vector size;
	VectorSubtract( maxs, mins, size );
	float maxSize = MAX( size.x, size.y );
	maxSize = MAX( maxSize, size.z );

	// Figure out the texture size
	// For now, we're going to assume a fixed number of shadow texels
	// per shadow-caster size; add in some extra space at the boundary.
	int texelCount = TEXEL_SIZE_PER_CASTER_SIZE * maxSize;
	
	// Pick the first power of 2 larger...
	int textureSize = 1;
	while (textureSize < texelCount)
	{
		textureSize <<= 1;
	}

	shadow.m_ShadowTexture = m_ShadowAllocator.AllocateTexture( textureSize, textureSize );
}


void CClientShadowMgr::CleanUpRenderToTextureShadow( ClientShadowHandle_t h )
{
	ClientShadow_t& shadow = m_Shadows[h];
	if (m_RenderToTextureActive && (shadow.m_Flags & SHADOW_FLAGS_USE_RENDER_TO_TEXTURE))
	{
		m_ShadowAllocator.DeallocateTexture( shadow.m_ShadowTexture );
		shadow.m_ShadowTexture = INVALID_TEXTURE_HANDLE;
	}
}


//-----------------------------------------------------------------------------
// Causes all shadows to be re-updated
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateAllShadows()
{
	for ( ClientShadowHandle_t i = m_Shadows.Head(); i != m_Shadows.InvalidIndex(); i = m_Shadows.Next(i) )
	{
		ClientShadow_t& shadow = m_Shadows[i];

		// Don't bother with flashlights
		if ( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) != 0 )
			continue;

		IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );
		if ( !pRenderable )
			continue;

		Assert( pRenderable->GetShadowHandle() == i );
		AddToDirtyShadowList( pRenderable, true );
	}
}

void CClientShadowMgr::RemoveAllShadowDecals()
{
	for ( ClientShadowHandle_t i = m_Shadows.Head(); i != m_Shadows.InvalidIndex(); i = m_Shadows.Next(i) )
	{
		ClientShadow_t& shadow = m_Shadows[i];

		// Don't bother with flashlights
		if ( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) != 0 )
			continue;

		shadowmgr->RemoveAllDecalsFromShadow( shadow.m_ShadowHandle );
	}
}

//-----------------------------------------------------------------------------
// Sets the shadow direction
//-----------------------------------------------------------------------------
void CClientShadowMgr::SetShadowDirection( const Vector& dir )
{
	VectorCopy( dir, m_SimpleShadowDir );
	VectorNormalize( m_SimpleShadowDir );

	if ( m_RenderToTextureActive )
	{
		UpdateAllShadows();
	}
}

const Vector &CClientShadowMgr::GetShadowDirection() const
{
	// This will cause blobby shadows to always project straight down
	static Vector s_vecDown( 0, 0, -1 );
	if ( !m_RenderToTextureActive )
		return s_vecDown;

	return m_SimpleShadowDir;
}


//-----------------------------------------------------------------------------
// Gets shadow information for a particular renderable
//-----------------------------------------------------------------------------
float CClientShadowMgr::GetShadowDistance( IClientRenderable *pRenderable ) const
{
	float flDist = m_flShadowCastDist;

	// Allow the renderable to override the default
	pRenderable->GetShadowCastDistance( &flDist, GetActualShadowCastType( pRenderable ) );

	return flDist;
}

const Vector &CClientShadowMgr::GetShadowDirection( IClientRenderable *pRenderable ) const
{
	Vector &vecResult = AllocTempVector();
	vecResult = GetShadowDirection();

	// Allow the renderable to override the default
	pRenderable->GetShadowCastDirection( &vecResult, GetActualShadowCastType( pRenderable ) );

	return vecResult;
}

const Vector &CClientShadowMgr::GetShadowDirection( ClientShadowHandle_t shadowHandle ) const
{
	Assert( shadowHandle != CLIENTSHADOW_INVALID_HANDLE );

	IClientRenderable* pRenderable = ClientEntityList().GetClientRenderableFromHandle( m_Shadows[shadowHandle].m_Entity );
	Assert( pRenderable );

	if ( !IsShadowingFromWorldLights() )
	{
		return GetShadowDirection( pRenderable );
	}

	Vector &vecResult = AllocTempVector();
	vecResult = m_Shadows[shadowHandle].m_ShadowDir;
	
	// Allow the renderable to override the default
	pRenderable->GetShadowCastDirection( &vecResult, GetActualShadowCastType( pRenderable ) );

	return vecResult;
}

void CClientShadowMgr::UpdateShadowDirectionFromLocalLightSource( ClientShadowHandle_t shadowHandle )
{
	Assert( shadowHandle != CLIENTSHADOW_INVALID_HANDLE );

	ClientShadow_t& shadow = m_Shadows[shadowHandle];

	IClientRenderable* pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );

	// TODO: Figure out why this still gets hit
	Assert( pRenderable );
	if ( !pRenderable )
	{
		DevWarning( "%s(): Skipping shadow with invalid client renderable (shadow handle %d)\n", __FUNCTION__, shadowHandle );
		return;
	}

	Vector bbMin, bbMax;
	pRenderable->GetRenderBoundsWorldspace( bbMin, bbMax );
	Vector origin( 0.5f * ( bbMin + bbMax ) );
	origin.z = bbMin.z;	// Putting origin at the bottom of the bounding box makes the shadows a little shorter

	Vector lightPos;
	Vector lightBrightness;

	if ( shadow.m_LightPosLerp >= 1.0f )	// skip finding new light source if we're in the middle of a lerp
	{
		if( modelrender->GetBrightestShadowingLightSource( pRenderable->GetRenderOrigin(), lightPos, lightBrightness,
				r_shadowfromanyworldlight.GetBool() ) == false )
		{
			// didn't find a light source at all, use default shadow direction
			// TODO: Could switch to using blobby shadow in this case
			lightPos.Init( FLT_MAX, FLT_MAX, FLT_MAX );
		}
	}

	if ( shadow.m_LightPosLerp == FLT_MAX )	// first light pos ever, just init
	{
		shadow.m_CurrentLightPos = lightPos;
		shadow.m_TargetLightPos = lightPos;
		shadow.m_LightPosLerp = 1.0f;
	}
	else if ( shadow.m_LightPosLerp < 1.0f )
	{
		// We're in the middle of a lerp from current to target light. Finish it.
		shadow.m_LightPosLerp += gpGlobals->frametime * 1.0f/r_shadow_lightpos_lerptime.GetFloat();
		shadow.m_LightPosLerp = clamp( shadow.m_LightPosLerp, 0.0f, 1.0f );

		Vector currLightPos( shadow.m_CurrentLightPos );
		Vector targetLightPos( shadow.m_TargetLightPos );
		if ( currLightPos.x == FLT_MAX )
		{
			currLightPos = origin - 200.0f * GetShadowDirection();
		}
		if ( targetLightPos.x == FLT_MAX )
		{
			targetLightPos = origin - 200.0f * GetShadowDirection();
		}

		// lerp light pos
		Vector v1 = origin - shadow.m_CurrentLightPos;
		v1.NormalizeInPlace();

		Vector v2 = origin - shadow.m_TargetLightPos;
		v2.NormalizeInPlace();

		if ( v1.Dot( v2 ) < 0.0f )
		{
			// if change in shadow angle is more than 90 degrees, lerp over the renderable's top to avoid long sweeping shadows
			Vector fakeOverheadLightPos( origin.x, origin.y, origin.z + 200.0f );
			if( shadow.m_LightPosLerp < 0.5f )
			{
				lightPos = Lerp( 2.0f * shadow.m_LightPosLerp, currLightPos, fakeOverheadLightPos );
			}
			else
			{
				lightPos = Lerp( 2.0f * shadow.m_LightPosLerp - 1.0f, fakeOverheadLightPos, targetLightPos );
			}
		}
		else
		{
			lightPos = Lerp( shadow.m_LightPosLerp, currLightPos, targetLightPos );
		}

		if ( shadow.m_LightPosLerp >= 1.0f )
		{
			shadow.m_CurrentLightPos = shadow.m_TargetLightPos;
		}
	}
	else if ( shadow.m_LightPosLerp >= 1.0f )
	{
		// check if we have a new closest light position and start a new lerp
		float flDistSq = ( lightPos - shadow.m_CurrentLightPos ).LengthSqr();

		if ( flDistSq > 1.0f )
		{
			// light position has changed, which means we got a new light source. Initiate a lerp
			shadow.m_TargetLightPos = lightPos;
			shadow.m_LightPosLerp = 0.0f;
		}

		lightPos = shadow.m_CurrentLightPos;
	}

	if ( lightPos.x == FLT_MAX )
	{
		lightPos = origin - 200.0f * GetShadowDirection();
	}

	Vector vecResult( origin - lightPos );
	vecResult.NormalizeInPlace();

	vecResult.z *= r_shadow_shortenfactor.GetFloat();
	vecResult.NormalizeInPlace();

	shadow.m_ShadowDir = vecResult;

	if ( r_shadowfromworldlights_debug.GetBool() )
	{
		NDebugOverlay::Line( lightPos, origin, 255, 255, 0, false, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Sets the shadow distance
//-----------------------------------------------------------------------------
void CClientShadowMgr::SetShadowDistance( float flMaxDistance )
{
	m_flShadowCastDist = flMaxDistance;
	UpdateAllShadows();
}

float CClientShadowMgr::GetShadowDistance( ) const
{
	return m_flShadowCastDist;
}


//-----------------------------------------------------------------------------
// Sets the screen area at which blobby shadows are always used
//-----------------------------------------------------------------------------
void CClientShadowMgr::SetShadowBlobbyCutoffArea( float flMinArea )
{
	m_flMinShadowArea = flMinArea;
}

float CClientShadowMgr::GetBlobbyCutoffArea( ) const
{
	return m_flMinShadowArea;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientShadowMgr::SetFalloffBias( ClientShadowHandle_t handle, unsigned char ucBias )
{
	shadowmgr->SetFalloffBias( m_Shadows[handle].m_ShadowHandle, ucBias );
	m_Shadows[handle].m_FalloffBias = ucBias;
}

//-----------------------------------------------------------------------------
// Returns the shadow texture
//-----------------------------------------------------------------------------
ITexture* CClientShadowMgr::GetShadowTexture( unsigned short h )
{
	return m_ShadowAllocator.GetTexture();
}


//-----------------------------------------------------------------------------
// Returns information needed by the model proxy
//-----------------------------------------------------------------------------
const ShadowInfo_t& CClientShadowMgr::GetShadowInfo( ClientShadowHandle_t h )
{
	return shadowmgr->GetInfo( m_Shadows[h].m_ShadowHandle );
}


//-----------------------------------------------------------------------------
// Renders the shadow texture to screen...
//-----------------------------------------------------------------------------
void CClientShadowMgr::RenderShadowTexture( int w, int h )
{
	if (m_RenderToTextureActive)
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( m_RenderShadow );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.Color4ub( 0, 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w, 0.0f, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
		meshBuilder.Color4ub( 0, 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( w, h, 0.0f );
		meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
		meshBuilder.Color4ub( 0, 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( 0.0f, h, 0.0f );
		meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
		meshBuilder.Color4ub( 0, 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}


//-----------------------------------------------------------------------------
// Create/destroy a shadow
//-----------------------------------------------------------------------------
ClientShadowHandle_t CClientShadowMgr::CreateProjectedTexture( ClientEntityHandle_t entity, int nEntIndex, int flags, CBitVec< MAX_SPLITSCREEN_PLAYERS > *pSplitScreenBits )
{
	// We need to know if it's a brush model for shadows
	if( ( flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) == 0 )
	{
		IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( entity );
		if ( !pRenderable )
			return m_Shadows.InvalidIndex();

		int modelType = modelinfo->GetModelType( pRenderable->GetModel() );
		if (modelType == mod_brush)
		{
			flags |= SHADOW_FLAGS_BRUSH_MODEL;
		}
	}

	ClientShadowHandle_t h = m_Shadows.AddToTail();
	ClientShadow_t& shadow = m_Shadows[h];
	shadow.m_Entity = entity;
	shadow.m_ClientLeafShadowHandle = ClientLeafSystem()->AddShadow( h, flags );
	shadow.m_Flags = flags;
	shadow.m_nRenderFrame = -1;
	shadow.m_ShadowDir = GetShadowDirection();
	shadow.m_LastOrigin.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	shadow.m_LastAngles.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	shadow.m_CurrentLightPos.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	shadow.m_TargetLightPos.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	shadow.m_LightPosLerp = FLT_MAX;
	Assert( ( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) == 0 ) != 
			( ( shadow.m_Flags & SHADOW_FLAGS_SHADOW ) == 0 ) );

	shadow.m_nLastUpdateFrame = 0;

	shadow.m_nSplitscreenOwner = -1; // No one owns this texture
	if ( ( flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) || ( flags & SHADOW_FLAGS_USE_DEPTH_TEXTURE ) )
	{
		// The local player isn't always resolvable if this projected texture isn't the player's flashlight, so
		// if the local player isn't resolvable, leave the splitscreen owner set to -1 so all splitscreen players render it
		if ( engine->IsLocalPlayerResolvable() )
		{
			// Set ownership to this player
			shadow.m_nSplitscreenOwner = GET_ACTIVE_SPLITSCREEN_SLOT();
		}
	}

	// Set up the flags....
	IMaterial* pShadowMaterial = m_SimpleShadow;
	IMaterial* pShadowModelMaterial = m_SimpleShadow;
	void* pShadowProxyData = (void*)CLIENTSHADOW_INVALID_HANDLE;

	if ( m_RenderToTextureActive && (flags & SHADOW_FLAGS_USE_RENDER_TO_TEXTURE) )
	{
		SetupRenderToTextureShadow(h);

		pShadowMaterial = m_RenderShadow;
		pShadowModelMaterial = m_RenderModelShadow;
		pShadowProxyData = (void*)h;
	}

	if( ( flags & SHADOW_FLAGS_USE_DEPTH_TEXTURE ) || ( flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) )
	{
		pShadowMaterial = NULL;		// these materials aren't used for shadow depth texture shadows.
		pShadowModelMaterial = NULL;
		pShadowProxyData = (void*)h;
	}

	int createShadowFlags;
	if( flags & SHADOW_FLAGS_SIMPLE_PROJECTION )
	{
		createShadowFlags = SHADOW_SIMPLE_PROJECTION;
	}
	else if( flags & SHADOW_FLAGS_FLASHLIGHT )
	{
		// don't use SHADOW_CACHE_VERTS with projective lightsources since we expect that they will change every frame.
		// FIXME: might want to make it cache optionally if it's an entity light that is static.
		createShadowFlags = SHADOW_FLASHLIGHT;
	}
	else
	{
		createShadowFlags = SHADOW_CACHE_VERTS;
	}

	shadow.m_ShadowHandle = shadowmgr->CreateShadowEx( pShadowMaterial, pShadowModelMaterial, pShadowProxyData, createShadowFlags, nEntIndex );

	shadow.m_bUseSplitScreenBits = pSplitScreenBits ? true : false;
	if ( pSplitScreenBits )
	{
		shadow.m_SplitScreenBits.Copy( *pSplitScreenBits );
	}
	return h;
}

ClientShadowHandle_t CClientShadowMgr::CreateFlashlight( const FlashlightState_t &lightState )
{
	// We don't really need a model entity handle for a projective light source, so use an invalid one.
	static ClientEntityHandle_t invalidHandle = INVALID_CLIENTENTITY_HANDLE;

	int shadowFlags = SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_LIGHT_WORLD;
	if( lightState.m_bEnableShadows && r_flashlightdepthtexture.GetBool() )
	{
		shadowFlags |= SHADOW_FLAGS_USE_DEPTH_TEXTURE;
	}

	ClientShadowHandle_t shadowHandle = CreateProjectedTexture( invalidHandle, -1, shadowFlags, NULL );

	UpdateFlashlightState( shadowHandle, lightState );
	UpdateProjectedTexture( shadowHandle, true );
	return shadowHandle;
}

ClientShadowHandle_t CClientShadowMgr::CreateShadow( ClientEntityHandle_t entity, int nEntIndex, int flags, CBitVec< MAX_SPLITSCREEN_PLAYERS > *pSplitScreenBits /*= NULL*/ )
{
	// We don't really need a model entity handle for a projective light source, so use an invalid one.
	flags &= ~SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK;
	flags |= SHADOW_FLAGS_SHADOW | SHADOW_FLAGS_TEXTURE_DIRTY;
	ClientShadowHandle_t shadowHandle = CreateProjectedTexture( entity, nEntIndex, flags, pSplitScreenBits );

	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( entity );
	if ( pRenderable )
	{
		Assert( !pRenderable->IsShadowDirty( ) );
		pRenderable->MarkShadowDirty( true );

		CClientAlphaProperty *pAlphaProperty = static_cast<CClientAlphaProperty*>( pRenderable->GetIClientUnknown()->GetClientAlphaProperty() );
		if ( pAlphaProperty )
		{
			pAlphaProperty->SetShadowHandle( shadowHandle );
		}
	}

	// NOTE: We *have* to call the version that takes a shadow handle
	// even if we have an entity because this entity hasn't set its shadow handle yet
	AddToDirtyShadowList( shadowHandle, true );
	return shadowHandle;
}


//-----------------------------------------------------------------------------
// Updates the flashlight direction and re-computes surfaces it should lie on
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateFlashlightState( ClientShadowHandle_t shadowHandle, const FlashlightState_t &flashlightState )
{
	VPROF_BUDGET( "CClientShadowMgr::UpdateFlashlightState", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	if( flashlightState.m_bEnableShadows && r_flashlightdepthtexture.GetBool() )
	{
		m_Shadows[shadowHandle].m_Flags |= SHADOW_FLAGS_USE_DEPTH_TEXTURE;
	}
	else
	{
		m_Shadows[shadowHandle].m_Flags &= ~SHADOW_FLAGS_USE_DEPTH_TEXTURE;
	}

	if ( flashlightState.m_bOrtho )
	{
		BuildOrthoWorldToFlashlightMatrix( m_Shadows[shadowHandle].m_WorldToShadow, flashlightState );
	}
	else
	{
		BuildPerspectiveWorldToFlashlightMatrix( m_Shadows[shadowHandle].m_WorldToShadow, flashlightState );
	}
											
	shadowmgr->UpdateFlashlightState( m_Shadows[shadowHandle].m_ShadowHandle, flashlightState );
}

void CClientShadowMgr::DestroyFlashlight( ClientShadowHandle_t shadowHandle )
{
	DestroyShadow( shadowHandle );
}


ClientShadowHandle_t CClientShadowMgr::CreateProjection( const FlashlightState_t &lightState )
{
//	return CreateFlashlight(lightState);

	// We don't really need a model entity handle for a projective light source, so use an invalid one.
	static ClientEntityHandle_t invalidHandle = INVALID_CLIENTENTITY_HANDLE;

	int shadowFlags = SHADOW_FLAGS_SIMPLE_PROJECTION;

	ClientShadowHandle_t shadowHandle = CreateProjectedTexture( invalidHandle, -1, shadowFlags, NULL );

	UpdateFlashlightState( shadowHandle, lightState );
	UpdateProjectedTexture( shadowHandle, true );

	return shadowHandle;
}


//-----------------------------------------------------------------------------
// Updates the flashlight direction and re-computes surfaces it should lie on
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateProjectionState( ClientShadowHandle_t shadowHandle, const FlashlightState_t &flashlightState )
{
//	UpdateFlashlightState(shadowHandle, flashlightState );
//	return;

	VPROF_BUDGET( "CClientShadowMgr::UpdateProjectionState", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	if ( flashlightState.m_bOrtho )
	{
		BuildOrthoWorldToFlashlightMatrix( m_Shadows[shadowHandle].m_WorldToShadow, flashlightState );
	}
	else
	{
		BuildPerspectiveWorldToFlashlightMatrix( m_Shadows[shadowHandle].m_WorldToShadow, flashlightState );
	}

	shadowmgr->UpdateFlashlightState( m_Shadows[shadowHandle].m_ShadowHandle, flashlightState );
}

void CClientShadowMgr::DestroyProjection( ClientShadowHandle_t shadowHandle )
{
	DestroyShadow( shadowHandle );
}


//-----------------------------------------------------------------------------
// Remove a shadow from the dirty list
//-----------------------------------------------------------------------------
void CClientShadowMgr::RemoveShadowFromDirtyList( ClientShadowHandle_t handle )
{
	int idx = m_DirtyShadows.Find( handle );
	if ( idx != m_DirtyShadows.InvalidIndex() )
	{
		// Clean up the shadow update bit.
		IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( m_Shadows[handle].m_Entity );
		if ( pRenderable )
		{
			pRenderable->MarkShadowDirty( false );
		}
		m_DirtyShadows.RemoveAt( idx );
	}
	idx = m_DirtyShadowsLeftOver.Find( handle );
	if ( idx != m_DirtyShadowsLeftOver.InvalidIndex() )
	{
		// Clean up the shadow update bit.
		IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( m_Shadows[handle].m_Entity );
		if ( pRenderable )
		{
			pRenderable->MarkShadowDirty( false );
		}
		m_DirtyShadowsLeftOver.RemoveAt( idx );
	}
}


//-----------------------------------------------------------------------------
// Remove a shadow
//-----------------------------------------------------------------------------
void CClientShadowMgr::DestroyShadow( ClientShadowHandle_t handle )
{
	if ( m_bUpdatingDirtyShadows )
	{
		// While we're updating dirty shadows, destroying a shadow can cause an RB-Tree we're currently iterating to be changed.
		// This can cause tree corruption resulting in infinite loops or crashes. Instead, we queue the shadow handle for deletion and
		// service the queue after we're done updating.
		QueueShadowForDestruction( handle );
		return;
	}

	Assert( m_Shadows.IsValidIndex(handle) );
	RemoveShadowFromDirtyList( handle );
	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( m_Shadows[handle].m_Entity );
	if ( pRenderable )
	{
		CClientAlphaProperty *pAlphaProperty = static_cast<CClientAlphaProperty*>( pRenderable->GetIClientUnknown()->GetClientAlphaProperty() );
		if ( pAlphaProperty )
		{
			pAlphaProperty->SetShadowHandle( CLIENTSHADOW_INVALID_HANDLE );
		}
	}
	shadowmgr->DestroyShadow( m_Shadows[handle].m_ShadowHandle );
	if ( m_Shadows[handle].m_ClientLeafShadowHandle != CLIENT_LEAF_SHADOW_INVALID_HANDLE )
	{
		ClientLeafSystem()->RemoveShadow( m_Shadows[handle].m_ClientLeafShadowHandle );
	}
	CleanUpRenderToTextureShadow( handle );
	m_Shadows.Remove(handle);
}


//-----------------------------------------------------------------------------
// Queues a shadow for removal
//-----------------------------------------------------------------------------
void CClientShadowMgr::QueueShadowForDestruction( ClientShadowHandle_t handle )
{
	// this function should be called infrequently (it is a failsafe)
	// so check to make sure it's not queued to delete twice
	if ( m_shadowsToDestroy.IsValidIndex( m_shadowsToDestroy.Find(handle) ) )
	{
		AssertMsg1( false, "Tried to queue shadow %d for deletion twice!\n", (int)(handle) );
	} 
	else
	{
		m_shadowsToDestroy.AddToTail( handle );
	}
}


//-----------------------------------------------------------------------------
// Removes queued shadows
//-----------------------------------------------------------------------------
void CClientShadowMgr::DestroyQueuedShadows()
{
	Assert( !m_bUpdatingDirtyShadows );

	for ( int i = 0; i < m_shadowsToDestroy.Count(); i++ )
	{
		DestroyShadow( m_shadowsToDestroy[i] );
	}
	m_shadowsToDestroy.RemoveAll();
}

//-----------------------------------------------------------------------------
// Build the worldtotexture matrix
//-----------------------------------------------------------------------------
void CClientShadowMgr::BuildGeneralWorldToShadowMatrix( VMatrix& matWorldToShadow,
	const Vector& origin, const Vector& dir, const Vector& xvec, const Vector& yvec )
{
	// We're assuming here that xvec + yvec aren't necessary perpendicular

	// The shadow->world matrix is pretty simple:
	// Just stick the origin in the translation component
	// and the vectors in the columns...
	matWorldToShadow.SetBasisVectors( xvec, yvec, dir );
	matWorldToShadow.SetTranslation( origin );
	matWorldToShadow[3][0] = matWorldToShadow[3][1] = matWorldToShadow[3][2] = 0.0f;
	matWorldToShadow[3][3] = 1.0f;

	// Now do a general inverse to get matWorldToShadow
	MatrixInverseGeneral( matWorldToShadow, matWorldToShadow );
}

void CClientShadowMgr::BuildWorldToShadowMatrix( VMatrix& matWorldToShadow,	const Vector& origin, const Quaternion& quatOrientation )
{
	// The shadow->world matrix is pretty simple:
	// Just stick the origin in the translation component
	// and the vectors in the columns...
	// The inverse of this transposes the rotational component
	// and the translational component =  - (rotation transpose) * origin

	matrix3x4_t matOrientation;											
	QuaternionMatrix( quatOrientation, matOrientation );		// Convert quat to matrix3x4
	PositionMatrix( vec3_origin, matOrientation );				// Zero out translation elements

	VMatrix matBasis( matOrientation );							// Convert matrix3x4 to VMatrix

	Vector vForward, vLeft, vUp;
	matBasis.GetBasisVectors( vForward, vLeft, vUp );
	matBasis.SetForward( vLeft );								// Bizarre vector flip inherited from earlier code, WTF?
	matBasis.SetLeft( vUp );
	matBasis.SetUp( vForward );
	matWorldToShadow = matBasis.Transpose();					// Transpose

	Vector translation;
	Vector3DMultiply( matWorldToShadow, origin, translation );

	translation *= -1.0f;
	matWorldToShadow.SetTranslation( translation );

	// The the bottom row.
	matWorldToShadow[3][0] = matWorldToShadow[3][1] = matWorldToShadow[3][2] = 0.0f;
	matWorldToShadow[3][3] = 1.0f;
}

void CClientShadowMgr::BuildPerspectiveWorldToFlashlightMatrix( VMatrix& matWorldToShadow, const FlashlightState_t &flashlightState )
{
	VPROF_BUDGET( "CClientShadowMgr::BuildPerspectiveWorldToFlashlightMatrix", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	// Buildworld to shadow matrix, then perspective projection and concatenate
	VMatrix matWorldToShadowView, matPerspective;
	BuildWorldToShadowMatrix( matWorldToShadowView, flashlightState.m_vecLightOrigin,
							  flashlightState.m_quatOrientation );

	MatrixBuildPerspective( matPerspective, flashlightState.m_fHorizontalFOVDegrees,
							flashlightState.m_fVerticalFOVDegrees,
							flashlightState.m_NearZ, flashlightState.m_FarZ );

	MatrixMultiply( matPerspective, matWorldToShadowView, matWorldToShadow );
}

void CClientShadowMgr::BuildOrthoWorldToFlashlightMatrix( VMatrix& matWorldToShadow, const FlashlightState_t &flashlightState )
{
	VPROF_BUDGET( "CClientShadowMgr::BuildPerspectiveWorldToFlashlightMatrix", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	// Buildworld to shadow matrix, then perspective projection and concatenate
	VMatrix matWorldToShadowView, matPerspective;
	BuildWorldToShadowMatrix( matWorldToShadowView, flashlightState.m_vecLightOrigin,
		flashlightState.m_quatOrientation );

	MatrixBuildOrtho( matPerspective, 
					  flashlightState.m_fOrthoLeft, flashlightState.m_fOrthoTop, flashlightState.m_fOrthoRight, flashlightState.m_fOrthoBottom,
					  flashlightState.m_NearZ, flashlightState.m_FarZ );

	// Shift it z/y to 0 to -2 space
	VMatrix addW;
	addW.Identity();
	addW[0][3] = -1.0f;
	addW[1][3] = -1.0f;
	addW[2][3] = 0.0f;
	MatrixMultiply( addW, matPerspective, matPerspective );

	// Flip x/y to positive 0 to 1... flip z to negative
	VMatrix scaleHalf;
	scaleHalf.Identity();
	scaleHalf[0][0] = -0.5f;
	scaleHalf[1][1] = -0.5f;
	scaleHalf[2][2] = -1.0f;
	MatrixMultiply( scaleHalf, matPerspective, matPerspective );

	MatrixMultiply( matPerspective, matWorldToShadowView, matWorldToShadow );
}

//-----------------------------------------------------------------------------
// Compute the shadow origin and attenuation start distance
//-----------------------------------------------------------------------------
float CClientShadowMgr::ComputeLocalShadowOrigin( IClientRenderable* pRenderable, 
	const Vector& mins, const Vector& maxs, const Vector& localShadowDir, float backupFactor, Vector& origin )
{
	// Compute the centroid of the object...
	Vector vecCentroid;
	VectorAdd( mins, maxs, vecCentroid );
	vecCentroid *= 0.5f;

	Vector vecSize;
	VectorSubtract( maxs, mins, vecSize );
	float flRadius = vecSize.Length() * 0.5f;

	// NOTE: The *origin* of the shadow cast is a point on a line passing through
	// the centroid of the caster. The direction of this line is the shadow cast direction,
	// and the point on that line corresponds to the endpoint of the box that is
	// furthest *back* along the shadow direction

	// For the first point at which the shadow could possibly start falling off,
	// we need to use the point at which the ray described above leaves the
	// bounding sphere surrounding the entity. This is necessary because otherwise,
	// tall, thin objects would have their shadows appear + disappear as then spun about their origin

	// Figure out the corner corresponding to the min + max projection
	// along the shadow direction

	// We're basically finding the point on the cube that has the largest and smallest
	// dot product with the local shadow dir. Then we're taking the dot product
	// of that with the localShadowDir. lastly, we're subtracting out the
	// centroid projection to give us a distance along the localShadowDir to
	// the front and back of the cube along the direction of the ray.
	float centroidProjection = DotProduct( vecCentroid, localShadowDir );
	float minDist = -centroidProjection;
	for (int i = 0; i < 3; ++i)
	{
		if ( localShadowDir[i] > 0.0f )
		{
			minDist += localShadowDir[i] * mins[i];
		}
		else
		{
			minDist += localShadowDir[i] * maxs[i];
		}
	}

	minDist *= backupFactor;

	VectorMA( vecCentroid, minDist, localShadowDir, origin );

	return flRadius - minDist;
}


//-----------------------------------------------------------------------------
// Sorts the components of a vector
//-----------------------------------------------------------------------------
static inline void SortAbsVectorComponents( const Vector& src, int* pVecIdx )
{
	Vector absVec( fabs(src[0]), fabs(src[1]), fabs(src[2]) );

	int maxIdx = (absVec[0] > absVec[1]) ? 0 : 1;
	if (absVec[2] > absVec[maxIdx])
	{
		maxIdx = 2;
	}

	// always choose something right-handed....
	switch(	maxIdx )
	{
	case 0:
		pVecIdx[0] = 1;
		pVecIdx[1] = 2;
		pVecIdx[2] = 0;
		break;
	case 1:
		pVecIdx[0] = 2;
		pVecIdx[1] = 0;
		pVecIdx[2] = 1;
		break;
	case 2:
		pVecIdx[0] = 0;
		pVecIdx[1] = 1;
		pVecIdx[2] = 2;
		break;
	}
}


//-----------------------------------------------------------------------------
// Build the worldtotexture matrix
//-----------------------------------------------------------------------------
static void BuildWorldToTextureMatrix( const VMatrix& matWorldToShadow, 
							const Vector2D& size, VMatrix& matWorldToTexture )
{
	// Build a matrix that maps from shadow space to (u,v) coordinates
	VMatrix shadowToUnit;
	MatrixBuildScale( shadowToUnit, 1.0f / size.x, 1.0f / size.y, 1.0f );
	shadowToUnit[0][3] = shadowToUnit[1][3] = 0.5f;

	// Store off the world to (u,v) transformation
	MatrixMultiply( shadowToUnit, matWorldToShadow, matWorldToTexture );
}



static void BuildOrthoWorldToShadowMatrix( VMatrix& worldToShadow,
													 const Vector& origin, const Vector& dir, const Vector& xvec, const Vector& yvec )
{
	// This version is faster and assumes dir, xvec, yvec are perpendicular
	AssertFloatEquals( DotProduct( dir, xvec ), 0.0f, 1e-3 );
	AssertFloatEquals( DotProduct( dir, yvec ), 0.0f, 1e-3 );
	AssertFloatEquals( DotProduct( xvec, yvec ), 0.0f, 1e-3 );

	// The shadow->world matrix is pretty simple:
	// Just stick the origin in the translation component
	// and the vectors in the columns...
	// The inverse of this transposes the rotational component
	// and the translational component =  - (rotation transpose) * origin
	worldToShadow.SetBasisVectors( xvec, yvec, dir );
	MatrixTranspose( worldToShadow, worldToShadow );

	Vector translation;
	Vector3DMultiply( worldToShadow, origin, translation );

	translation *= -1.0f;
	worldToShadow.SetTranslation( translation );

	// The the bottom row.
	worldToShadow[3][0] = worldToShadow[3][1] = worldToShadow[3][2] = 0.0f;
	worldToShadow[3][3] = 1.0f;
}


//-----------------------------------------------------------------------------
// Set extra clip planes related to shadows...
//-----------------------------------------------------------------------------
void CClientShadowMgr::ClearExtraClipPlanes( ClientShadowHandle_t h )
{
	if ( !r_shadow_deferred.GetBool() )
		shadowmgr->ClearExtraClipPlanes( m_Shadows[h].m_ShadowHandle );
}

void CClientShadowMgr::AddExtraClipPlane( ClientShadowHandle_t h, const Vector& normal, float dist )
{
	if ( !r_shadow_deferred.GetBool() )
		shadowmgr->AddExtraClipPlane( m_Shadows[h].m_ShadowHandle, normal, dist );
}


//-----------------------------------------------------------------------------
// Compute the extra shadow planes
//-----------------------------------------------------------------------------
void CClientShadowMgr::ComputeExtraClipPlanes( IClientRenderable* pRenderable, 
	ClientShadowHandle_t handle, const Vector* vec, 
	const Vector& mins, const Vector& maxs, const Vector& localShadowDir )
{
	// Compute the world-space position of the corner of the bounding box
	// that's got the highest dotproduct with the local shadow dir...
	Vector origin = pRenderable->GetRenderOrigin( );
	float dir[3];

	int i;
	for ( i = 0; i < 3; ++i )
	{
		if (localShadowDir[i] < 0.0f)
		{
			VectorMA( origin, maxs[i], vec[i], origin );
			dir[i] = 1;
		}
		else
		{
			VectorMA( origin, mins[i], vec[i], origin );
			dir[i] = -1;
		}
	}

	// Now that we have it, create 3 planes...
	Vector normal;
	ClearExtraClipPlanes(handle);
	for ( i = 0; i < 3; ++i )
	{
		VectorMultiply( vec[i], dir[i], normal );
		float dist = DotProduct( normal, origin );
		AddExtraClipPlane( handle, normal, dist );
	}

	ClientShadow_t& shadow = m_Shadows[handle];
	C_BaseEntity *pEntity = ClientEntityList().GetBaseEntityFromHandle( shadow.m_Entity );
	if ( pEntity && pEntity->m_bEnableRenderingClipPlane )
	{
		normal[ 0 ] = -pEntity->m_fRenderingClipPlane[ 0 ];
		normal[ 1 ] = -pEntity->m_fRenderingClipPlane[ 1 ];
		normal[ 2 ] = -pEntity->m_fRenderingClipPlane[ 2 ];
		AddExtraClipPlane( handle, normal, -pEntity->m_fRenderingClipPlane[ 3 ] - 0.5f );
	}
}


inline ShadowType_t CClientShadowMgr::GetActualShadowCastType( ClientShadowHandle_t handle ) const
{
	if ( handle == CLIENTSHADOW_INVALID_HANDLE )
	{
		return SHADOWS_NONE;
	}
	
	if ( m_Shadows[handle].m_Flags & SHADOW_FLAGS_USE_RENDER_TO_TEXTURE )
	{
		return ( m_RenderToTextureActive ? SHADOWS_RENDER_TO_TEXTURE : SHADOWS_SIMPLE );
	}
	else if( m_Shadows[handle].m_Flags & SHADOW_FLAGS_USE_DEPTH_TEXTURE )
	{
		return SHADOWS_RENDER_TO_DEPTH_TEXTURE;
	}
	else
	{
		return SHADOWS_SIMPLE;
	}
}

inline ShadowType_t CClientShadowMgr::GetActualShadowCastType( IClientRenderable *pEnt ) const
{
	return GetActualShadowCastType( pEnt->GetShadowHandle() );
}


//-----------------------------------------------------------------------------
// Adds a shadow to all leaves along a ray
//-----------------------------------------------------------------------------
class CShadowLeafEnum : public ISpatialLeafEnumerator
{
public:
	bool EnumerateLeaf( int leaf, int context )
	{
		m_LeafList.AddToTail( leaf );
		return true;
	}

	void ExtractUnconnectedLeaves( const Vector &vecOrigin )
	{
		VPROF_BUDGET( "ExtractUnconnectedLeaves", "ExtractUnconnectedLeaves" );
		int nCount = m_LeafList.Count();
		bool *pIsConnected = (bool*)stackalloc( nCount * sizeof(bool) );
		engine->ComputeLeavesConnected( vecOrigin, nCount, m_LeafList.Base(), pIsConnected );
		int nCountRemoved = 0;
		for ( int i = nCount; --i >= 0; )
		{
			if ( !pIsConnected[i] )
			{
				++nCountRemoved;
				m_LeafList.FastRemove( i );
			}
		}
	}

	CUtlVectorFixedGrowable< int, 512 > m_LeafList;
};


//-----------------------------------------------------------------------------
// Builds a list of leaves inside the shadow volume
//-----------------------------------------------------------------------------
static void BuildShadowLeafList( CShadowLeafEnum *pEnum, const Vector& origin, 
	const Vector& dir, const Vector2D& size, float maxDist )
{
	Ray_t ray;
	VectorCopy( origin, ray.m_Start );
	VectorMultiply( dir, maxDist, ray.m_Delta );
	ray.m_StartOffset.Init( 0, 0, 0 );

	float flRadius = sqrt( size.x * size.x + size.y * size.y ) * 0.5f;
	ray.m_Extents.Init( flRadius, flRadius, flRadius );
	ray.m_IsRay = false;
	ray.m_IsSwept = true;

	ISpatialQuery* pQuery = engine->GetBSPTreeQuery();
	pQuery->EnumerateLeavesAlongRay( ray, pEnum, 0 );
}


//-----------------------------------------------------------------------------
// Builds a simple blobby shadow
//-----------------------------------------------------------------------------
void CClientShadowMgr::BuildOrthoShadow( IClientRenderable* pRenderable, 
		ClientShadowHandle_t handle, const Vector& mins, const Vector& maxs)
{
	// Get the object's basis
	Vector vec[3];
	AngleVectors( pRenderable->GetRenderAngles(), &vec[0], &vec[1], &vec[2] );
	vec[1] *= -1.0f;

	Vector vecShadowDir = GetShadowDirection( handle );

	// Project the shadow casting direction into the space of the object
	Vector localShadowDir;
	localShadowDir[0] = DotProduct( vec[0], vecShadowDir );
	localShadowDir[1] = DotProduct( vec[1], vecShadowDir );
	localShadowDir[2] = DotProduct( vec[2], vecShadowDir );

	// Figure out which vector has the largest component perpendicular
	// to the shadow handle...
	// Sort by how perpendicular it is
	int vecIdx[3];
	SortAbsVectorComponents( localShadowDir, vecIdx );

	// Here's our shadow basis vectors; namely the ones that are
	// most perpendicular to the shadow casting direction
	Vector xvec = vec[vecIdx[0]];
	Vector yvec = vec[vecIdx[1]];

	// Project them into a plane perpendicular to the shadow direction
	xvec -= vecShadowDir * DotProduct( vecShadowDir, xvec );
	yvec -= vecShadowDir * DotProduct( vecShadowDir, yvec );
	VectorNormalize( xvec );
	VectorNormalize( yvec );

	// Compute the box size
	Vector boxSize;
	VectorSubtract( maxs, mins, boxSize );

	// We project the two longest sides into the vectors perpendicular
	// to the projection direction, then add in the projection of the perp direction
	Vector2D size( boxSize[vecIdx[0]], boxSize[vecIdx[1]] );
	size.x *= fabs( DotProduct( vec[vecIdx[0]], xvec ) );
	size.y *= fabs( DotProduct( vec[vecIdx[1]], yvec ) );

	// Add the third component into x and y
	size.x += boxSize[vecIdx[2]] * fabs( DotProduct( vec[vecIdx[2]], xvec ) );
	size.y += boxSize[vecIdx[2]] * fabs( DotProduct( vec[vecIdx[2]], yvec ) );

	// Bloat a bit, since the shadow wants to extend outside the model a bit
	size.x += 10.0f;
	size.y += 10.0f;

	// Clamp the minimum size
	Vector2DMax( size, Vector2D(10.0f, 10.0f), size );

	// Place the origin at the point with min dot product with shadow dir
	Vector org;
	float falloffStart = ComputeLocalShadowOrigin( pRenderable, mins, maxs, localShadowDir, 2.0f, org );

	// Transform the local origin into world coordinates
	Vector worldOrigin = pRenderable->GetRenderOrigin( );
	VectorMA( worldOrigin, org.x, vec[0], worldOrigin );
	VectorMA( worldOrigin, org.y, vec[1], worldOrigin );
	VectorMA( worldOrigin, org.z, vec[2], worldOrigin );

	// FUNKY: A trick to reduce annoying texelization artifacts!?
	float dx = 1.0f / TEXEL_SIZE_PER_CASTER_SIZE;
	worldOrigin.x = (int)(worldOrigin.x / dx) * dx;
	worldOrigin.y = (int)(worldOrigin.y / dx) * dx;
	worldOrigin.z = (int)(worldOrigin.z / dx) * dx;

	// NOTE: We gotta use the general matrix because xvec and yvec aren't perp
	VMatrix matWorldToShadow, matWorldToTexture;
	// negate xvec so that the matrix will have the same handedness as for an RTT shadow
	BuildGeneralWorldToShadowMatrix( m_Shadows[handle].m_WorldToShadow, worldOrigin, vecShadowDir, -xvec, yvec );
	BuildWorldToTextureMatrix( m_Shadows[handle].m_WorldToShadow, size, matWorldToTexture );
	Vector2DCopy( size, m_Shadows[handle].m_WorldSize );

	MatrixCopy( matWorldToTexture, m_Shadows[handle].m_WorldToTexture );

	// Compute the falloff attenuation
	// Area computation isn't exact since xvec is not perp to yvec, but close enough
//	float shadowArea = size.x * size.y;	

	// The entity may be overriding our shadow cast distance
	float flShadowCastDistance = GetShadowDistance( pRenderable );
	float maxHeight = flShadowCastDistance + falloffStart; //3.0f * sqrt( shadowArea );

	CShadowLeafEnum leafList;
	BuildShadowLeafList( &leafList, worldOrigin, vecShadowDir, size, maxHeight );
	int nCount = leafList.m_LeafList.Count();
	const int *pLeafList = leafList.m_LeafList.Base();

	if ( !r_shadow_deferred.GetBool() )
	{
		shadowmgr->ProjectShadow( m_Shadows[handle].m_ShadowHandle, worldOrigin,
			vecShadowDir, matWorldToTexture, size, nCount, pLeafList, maxHeight, falloffStart, MAX_FALLOFF_AMOUNT, pRenderable->GetRenderOrigin() );
	}

	m_Shadows[handle].m_MaxDist = maxHeight;
	m_Shadows[handle].m_FalloffStart = falloffStart;


	// Compute extra clip planes to prevent poke-thru
// FIXME!!!!!!!!!!!!!!  Removing this for now since it seems to mess up the blobby shadows.
//	ComputeExtraClipPlanes( pEnt, handle, vec, mins, maxs, localShadowDir );

	// Add the shadow to the client leaf system so it correctly marks 
	// leafs as being affected by a particular shadow
	ClientLeafSystem()->ProjectShadow( m_Shadows[handle].m_ClientLeafShadowHandle, nCount, pLeafList );
}


//-----------------------------------------------------------------------------
// Visualization....
//-----------------------------------------------------------------------------
void CClientShadowMgr::DrawRenderToTextureDebugInfo( IClientRenderable* pRenderable, const Vector& mins, const Vector& maxs )
{   
	// Get the object's basis
	Vector vec[3];
	AngleVectors( pRenderable->GetRenderAngles(), &vec[0], &vec[1], &vec[2] );
	vec[1] *= -1.0f;

	Vector vecSize;
	VectorSubtract( maxs, mins, vecSize );

	Vector vecOrigin = pRenderable->GetRenderOrigin();
	Vector start, end, end2;

	VectorMA( vecOrigin, mins.x, vec[0], start );
	VectorMA( start, mins.y, vec[1], start );
	VectorMA( start, mins.z, vec[2], start );

	VectorMA( start, vecSize.x, vec[0], end );
	VectorMA( end, vecSize.z, vec[2], end2 );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 ); 
	debugoverlay->AddLineOverlay( end2, end, 255, 0, 0, true, 0.01 ); 

	VectorMA( start, vecSize.y, vec[1], end );
	VectorMA( end, vecSize.z, vec[2], end2 );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 ); 
	debugoverlay->AddLineOverlay( end2, end, 255, 0, 0, true, 0.01 ); 

	VectorMA( start, vecSize.z, vec[2], end );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 );
	
	start = end;
	VectorMA( start, vecSize.x, vec[0], end );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 ); 

	VectorMA( start, vecSize.y, vec[1], end );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 ); 

	VectorMA( end, vecSize.x, vec[0], start );
	VectorMA( start, -vecSize.x, vec[0], end );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 ); 

	VectorMA( start, -vecSize.y, vec[1], end );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 ); 

	VectorMA( start, -vecSize.z, vec[2], end );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 );

	start = end;
	VectorMA( start, -vecSize.x, vec[0], end );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 ); 

	VectorMA( start, -vecSize.y, vec[1], end );
	debugoverlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.01 ); 

	C_BaseEntity *pEnt = pRenderable->GetIClientUnknown()->GetBaseEntity();
	if ( pEnt )
	{
		debugoverlay->AddTextOverlay( vecOrigin, 0, "%d", pEnt->entindex() );
	}
	else
	{
		debugoverlay->AddTextOverlay( vecOrigin, 0, "%X", (size_t)pRenderable );
	}
}


extern ConVar cl_drawshadowtexture;
extern ConVar cl_shadowtextureoverlaysize;

//-----------------------------------------------------------------------------
// Builds a more complex shadow...
//-----------------------------------------------------------------------------
void CClientShadowMgr::BuildRenderToTextureShadow( IClientRenderable* pRenderable, 
		ClientShadowHandle_t handle, const Vector& mins, const Vector& maxs)
{
	if ( cl_drawshadowtexture.GetInt() )
	{
		// Red wireframe bounding box around objects whose RTT shadows are being updated that frame
		DrawRenderToTextureDebugInfo( pRenderable, mins, maxs );
	}

	// Get the object's basis
	Vector vec[3];
	AngleVectors( pRenderable->GetRenderAngles(), &vec[0], &vec[1], &vec[2] );
	vec[1] *= -1.0f;

	Vector vecShadowDir = GetShadowDirection( handle );

//	Debugging aid
//	const model_t *pModel = pRenderable->GetModel();
//	const char *pDebugName = modelinfo->GetModelName( pModel );

	// Project the shadow casting direction into the space of the object
	Vector localShadowDir;
	localShadowDir[0] = DotProduct( vec[0], vecShadowDir );
	localShadowDir[1] = DotProduct( vec[1], vecShadowDir );
	localShadowDir[2] = DotProduct( vec[2], vecShadowDir );

	// Compute the box size
	Vector boxSize;
	VectorSubtract( maxs, mins, boxSize );
	
	Vector yvec;
	float fProjMax = 0.0f;
	for( int i = 0; i != 3; ++i )
	{
		Vector test = vec[i] - ( vecShadowDir * DotProduct( vecShadowDir, vec[i] ) );
		test *= boxSize[i]; //doing after the projection to simplify projection math
		float fLengthSqr = test.LengthSqr();
		if( fLengthSqr > fProjMax )
		{
			fProjMax = fLengthSqr;
			yvec = test;
		}
	}		

	VectorNormalize( yvec );

	// Compute the x vector
	Vector xvec;
	CrossProduct( yvec, vecShadowDir, xvec );

	// We project the two longest sides into the vectors perpendicular
	// to the projection direction, then add in the projection of the perp direction
	Vector2D size;
	size.x = boxSize.x * fabs( DotProduct( vec[0], xvec ) ) + 
		boxSize.y * fabs( DotProduct( vec[1], xvec ) ) + 
		boxSize.z * fabs( DotProduct( vec[2], xvec ) );
	size.y = boxSize.x * fabs( DotProduct( vec[0], yvec ) ) + 
		boxSize.y * fabs( DotProduct( vec[1], yvec ) ) + 
		boxSize.z * fabs( DotProduct( vec[2], yvec ) );

	size.x += 2.0f * TEXEL_SIZE_PER_CASTER_SIZE;
	size.y += 2.0f * TEXEL_SIZE_PER_CASTER_SIZE;

	// Place the origin at the point with min dot product with shadow dir
	Vector org;
	float falloffStart = ComputeLocalShadowOrigin( pRenderable, mins, maxs, localShadowDir, 1.0f, org );

	// Transform the local origin into world coordinates
	Vector worldOrigin = pRenderable->GetRenderOrigin( );
	VectorMA( worldOrigin, org.x, vec[0], worldOrigin );
	VectorMA( worldOrigin, org.y, vec[1], worldOrigin );
	VectorMA( worldOrigin, org.z, vec[2], worldOrigin );

	VMatrix matWorldToTexture;
	BuildOrthoWorldToShadowMatrix( m_Shadows[handle].m_WorldToShadow, worldOrigin, vecShadowDir, xvec, yvec );
	BuildWorldToTextureMatrix( m_Shadows[handle].m_WorldToShadow, size, matWorldToTexture );
	Vector2DCopy( size, m_Shadows[handle].m_WorldSize );

	MatrixCopy( matWorldToTexture, m_Shadows[handle].m_WorldToTexture );

	// Compute the falloff attenuation
	// Area computation isn't exact since xvec is not perp to yvec, but close enough
	// Extra factor of 4 in the maxHeight due to the size being half as big
//	float shadowArea = size.x * size.y;	

	// The entity may be overriding our shadow cast distance
	float flShadowCastDistance = GetShadowDistance( pRenderable );
	float maxHeight = flShadowCastDistance + falloffStart; //3.0f * sqrt( shadowArea );

	CShadowLeafEnum leafList;
	BuildShadowLeafList( &leafList, worldOrigin, vecShadowDir, size, maxHeight );
	int nCount = leafList.m_LeafList.Count();
	const int *pLeafList = leafList.m_LeafList.Base();

	if ( !r_shadow_deferred.GetBool() )
	{
		shadowmgr->ProjectShadow( m_Shadows[handle].m_ShadowHandle, worldOrigin, 
			vecShadowDir, matWorldToTexture, size, nCount, pLeafList, maxHeight, falloffStart, MAX_FALLOFF_AMOUNT, pRenderable->GetRenderOrigin() );

		// Compute extra clip planes to prevent poke-thru
		ComputeExtraClipPlanes( pRenderable, handle, vec, mins, maxs, localShadowDir );
	}

	m_Shadows[handle].m_MaxDist = maxHeight;
	m_Shadows[handle].m_FalloffStart = falloffStart;

	// Add the shadow to the client leaf system so it correctly marks 
	// leafs as being affected by a particular shadow
	ClientLeafSystem()->ProjectShadow( m_Shadows[handle].m_ClientLeafShadowHandle, nCount, pLeafList );
}

static void LineDrawHelperProjective( const Vector &startShadowSpace, const Vector &endShadowSpace, 
						   const VMatrix &shadowToWorld, unsigned char r = 255, unsigned char g = 255, unsigned char b = 255 )
{
	Vector startWorldSpace, endWorldSpace;
	Vector3DMultiplyPositionProjective( shadowToWorld, startShadowSpace, startWorldSpace );
	Vector3DMultiplyPositionProjective( shadowToWorld, endShadowSpace, endWorldSpace );

	debugoverlay->AddLineOverlay( startWorldSpace, endWorldSpace, r, g, b, false, -1 );
}


static void LineDrawHelper( const Vector &start, const Vector &end, 
						   const VMatrix &viewMatrixInverse, unsigned char r = 255, unsigned char g = 255, unsigned char b = 255 )
{
	Vector startWorldSpace, endWorldSpace;
	Vector3DMultiplyPosition( viewMatrixInverse, start, startWorldSpace );
	Vector3DMultiplyPosition( viewMatrixInverse, end, endWorldSpace );

	debugoverlay->AddLineOverlay( startWorldSpace, endWorldSpace, r, g, b, false, -1 );
}



//   We're going to sweep out an inner and outer superellipse.  Each superellipse has the equation:
//
//           2          2
//           -          -
//      ( x )d     ( y )d
//      (---)   +  (---)    =  1
//      ( a )      ( b )
//
//  where,
//           d is what we're calling m_fRoundness
//
//           The inner superellipse uses a = m_fWidth               and  b = m_fHeight 
//           The outer superellipse uses a = (m_fWidth + m_fWedge)  and  b = (m_fHeight + m_fHedge) 
//

// Controls density of wireframe uberlight
#define NUM_SUPER_ELLIPSE_POINTS      192
#define CONNECTOR_FREQ			       24

void CClientShadowMgr::DrawUberlightRig( const Vector &vOrigin, const VMatrix &matWorldToFlashlight, FlashlightState_t state )
{
	int i;
	float fXNear, fXFar, fYNear, fYFar, fXNearEdge, fXFarEdge, fYNearEdge, fYFarEdge, m;
	UberlightState_t uber = state.m_uberlightState;
	VMatrix viewMatrixInverse, viewMatrix;

	// A set of scratch points on the +x +y quadrants of the near and far ends of the swept superellipse wireframe
	Vector vecNearInnerPoints[(NUM_SUPER_ELLIPSE_POINTS / 4) + 1];        // Inner points for four superellipses
	Vector vecFarInnerPoints[(NUM_SUPER_ELLIPSE_POINTS / 4) + 1];
	Vector vecNearEdgeInnerPoints[(NUM_SUPER_ELLIPSE_POINTS / 4) + 1];
	Vector vecFarEdgeInnerPoints[(NUM_SUPER_ELLIPSE_POINTS / 4) + 1];

	Vector vecNearOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 4) + 1];        // Outer points for four superellipses
	Vector vecFarOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 4) + 1];
	Vector vecNearEdgeOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 4) + 1];
	Vector vecFarEdgeOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 4) + 1];

	// Clock hand which sweeps out a full circle
	float fTheta = 0;
	float fThetaIncrement = (2.0f * 3.14159) / ((float)NUM_SUPER_ELLIPSE_POINTS);

	// precompute the 2/d exponent
	float r = 2.0f / uber.m_fRoundness;         

	// Initialize arrays of points in light's local space defining +x +y quadrants of extruded superellipses (including x==0 and y==0 vertices)
	for ( i = 0; i<(NUM_SUPER_ELLIPSE_POINTS / 4) + 1; i++ )
	{
		if ( i == 0 )												 // If this is the 0th vertex
		{
			fXFar      = uber.m_fWidth * uber.m_fCutOff;             // compute near and far x's
			fXNear     = uber.m_fWidth * uber.m_fCutOn;
			fXFarEdge  = uber.m_fWidth * (uber.m_fCutOff + uber.m_fFarEdge);          
			fXNearEdge = uber.m_fWidth * (uber.m_fCutOn - uber.m_fNearEdge);

			fYFar = fYNear = fYFarEdge = fYNearEdge =0;     // y's are zero
		}
		else if ( i == (NUM_SUPER_ELLIPSE_POINTS / 4) )     // If this is the vertex on the y axis, avoid numerical problems
		{
			fXFar = fXNear = fXFarEdge = fXNearEdge = 0;    // x's are zero

			fYFar      = uber.m_fHeight * uber.m_fCutOff;             // compute near and far y's
			fYNear     = uber.m_fHeight * uber.m_fCutOn;
			fYFarEdge  = uber.m_fHeight * (uber.m_fCutOff + uber.m_fFarEdge);
			fYNearEdge = uber.m_fHeight * (uber.m_fCutOn - uber.m_fNearEdge);
		}
		else
		{
			m = sinf(fTheta) / cosf(fTheta);   // compute slope of line from origin

			// Solve for inner x's (intersect line of slope m with inner superellipses)
			fXFar      = (powf(powf(1.0f/uber.m_fWidth,r) + powf(m/uber.m_fHeight,r), -1.0f/r) * uber.m_fCutOff);
			fXNear     = (powf(powf(1.0f/uber.m_fWidth,r) + powf(m/uber.m_fHeight,r), -1.0f/r) * uber.m_fCutOn);

			fXFarEdge  = (powf(powf(1.0f/uber.m_fWidth,r) + powf(m/uber.m_fHeight,r), -1.0f/r) * (uber.m_fCutOff + uber.m_fFarEdge));
			fXNearEdge = (powf(powf(1.0f/uber.m_fWidth,r) + powf(m/uber.m_fHeight,r), -1.0f/r) * (uber.m_fCutOn - uber.m_fNearEdge));

			// Solve for inner y's using line equations
			fYFar  = m * fXFar;
			fYNear = m * fXNear;
			fYFarEdge  = m * fXFarEdge;
			fYNearEdge = m * fXNearEdge;
		}

		// World to Light's View matrix
		BuildWorldToShadowMatrix( viewMatrix, state.m_vecLightOrigin, state.m_quatOrientation );
		viewMatrixInverse = viewMatrix.InverseTR();

		// Store world space positions in array
		vecFarInnerPoints[i] = Vector( fXFar, fYFar, uber.m_fCutOff );
		vecNearInnerPoints[i] = Vector( fXNear, fYNear, uber.m_fCutOn );
		vecFarEdgeInnerPoints[i] = Vector( fXFarEdge, fYFarEdge, uber.m_fCutOff + uber.m_fFarEdge );
		vecNearEdgeInnerPoints[i] = Vector( fXNearEdge, fYNearEdge, uber.m_fCutOn - uber.m_fNearEdge );

		if ( i == 0 )																// If this is the 0th vertex
		{
			fXFar  = (uber.m_fWidth + uber.m_fWedge) * uber.m_fCutOff;              // compute near and far x's
			fXNear = (uber.m_fWidth + uber.m_fWedge) * uber.m_fCutOn;
			fXFarEdge  = (uber.m_fWidth + uber.m_fWedge) * (uber.m_fCutOff + uber.m_fFarEdge);
			fXNearEdge = (uber.m_fWidth + uber.m_fWedge) * (uber.m_fCutOn - uber.m_fNearEdge);

			fYFar = fYNear = fYFarEdge = fYNearEdge = 0;             // y's are zero
		}
		else if ( i == (NUM_SUPER_ELLIPSE_POINTS / 4) )  // If this is the vertex on the y axis, avoid numerical problems
		{
			fXFar = fXNear = fXFarEdge = fXNearEdge = 0;             // x's are zero

			fYFar  = (uber.m_fHeight + uber.m_fHedge) * uber.m_fCutOff;             // compute near and far y's
			fYNear = (uber.m_fHeight + uber.m_fHedge) * uber.m_fCutOn;
			fYFarEdge  = (uber.m_fHeight + uber.m_fHedge) * (uber.m_fCutOff + uber.m_fFarEdge); 
			fYNearEdge = (uber.m_fHeight + uber.m_fHedge) * (uber.m_fCutOn - uber.m_fNearEdge);
		}
		else
		{
			m = sinf(fTheta) / cosf(fTheta);   // compute slope of line from origin

			// Solve for inner x's (intersect line of slope m with inner superellipses)
			fXFar  = (powf(powf(1.0f/(uber.m_fWidth + uber.m_fWedge),r) + powf(m/(uber.m_fHeight + uber.m_fHedge),r), -1.0f/r) * uber.m_fCutOff);
			fXNear = (powf(powf(1.0f/(uber.m_fWidth + uber.m_fWedge),r) + powf(m/(uber.m_fHeight + uber.m_fHedge),r), -1.0f/r) * uber.m_fCutOn);

			fXFarEdge  = (powf(powf(1.0f/(uber.m_fWidth + uber.m_fWedge),r) + powf(m/(uber.m_fHeight + uber.m_fHedge),r), -1.0f/r) * (uber.m_fCutOff+ uber.m_fFarEdge));
			fXNearEdge = (powf(powf(1.0f/(uber.m_fWidth + uber.m_fWedge),r) + powf(m/(uber.m_fHeight + uber.m_fHedge),r), -1.0f/r) * (uber.m_fCutOn - uber.m_fNearEdge));

			// Solve for inner y's using line equations
			fYFar  = m * fXFar;
			fYNear = m * fXNear;
			fYFarEdge  = m * fXFarEdge;
			fYNearEdge = m * fXNearEdge;
		}

		// Store in array
		vecFarOuterPoints[i] = Vector( fXFar, fYFar, uber.m_fCutOff );
		vecNearOuterPoints[i] = Vector( fXNear, fYNear, uber.m_fCutOn );
		vecFarEdgeOuterPoints[i] = Vector( fXFarEdge, fYFarEdge, uber.m_fCutOff + uber.m_fFarEdge );
		vecNearEdgeOuterPoints[i] = Vector( fXNearEdge, fYNearEdge, uber.m_fCutOn - uber.m_fNearEdge );

		fTheta += fThetaIncrement;
	}

	Vector preVector, curVector;

	// Near inner superellipse
	for ( i=0; i<NUM_SUPER_ELLIPSE_POINTS; i++ )
	{
		if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
		{
			curVector = vecNearInnerPoints[i];
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
		{
			curVector = vecNearInnerPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
			curVector.x *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
		{
			curVector = vecNearInnerPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
			curVector.x *= -1;
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else                                                                 // +x -y quadrant, negate y when copying from scratch array
		{
			curVector = vecNearInnerPoints[NUM_SUPER_ELLIPSE_POINTS-i];
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}

		if ( i != 0 )
		{
			LineDrawHelper( preVector, curVector, viewMatrixInverse, 255, 10, 10 );
		}

		preVector = curVector;
	}

	LineDrawHelper( preVector, vecNearInnerPoints[0], viewMatrixInverse, 255, 10, 10 );

	// Far inner superellipse
	for (i=0; i<NUM_SUPER_ELLIPSE_POINTS; i++)
	{
		if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
		{
			curVector = vecFarInnerPoints[i];
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
		{
			curVector = vecFarInnerPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
			curVector.x *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
		{
			curVector = vecFarInnerPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
			curVector.x *= -1;
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else                                                                 // +x -y quadrant, negate y when copying from scratch array
		{
			curVector = vecFarInnerPoints[NUM_SUPER_ELLIPSE_POINTS-i];
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}

		if ( i != 0 )
		{
			LineDrawHelper( preVector, curVector, viewMatrixInverse, 10, 10, 255 );
		}

		preVector = curVector;
	}

	LineDrawHelper( preVector, vecFarInnerPoints[0], viewMatrixInverse, 10, 10, 255 );


	// Near outer superellipse
	for (i=0; i<NUM_SUPER_ELLIPSE_POINTS; i++)
	{
		if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
		{
			curVector = vecNearOuterPoints[i];
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
		{
			curVector = vecNearOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
			curVector.x *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
		{
			curVector = vecNearOuterPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
			curVector.x *= -1;
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else                                                                 // +x -y quadrant, negate y when copying from scratch array
		{
			curVector = vecNearOuterPoints[NUM_SUPER_ELLIPSE_POINTS-i];
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}

		if ( i != 0 )
		{
			LineDrawHelper( preVector, curVector, viewMatrixInverse, 255, 10, 10);
		}

		preVector = curVector;
	}

	LineDrawHelper( preVector, vecNearOuterPoints[0], viewMatrixInverse, 255, 10, 10 );


	// Far outer superellipse
	for (i=0; i<NUM_SUPER_ELLIPSE_POINTS; i++)
	{
		if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
		{
			curVector = vecFarOuterPoints[i];
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
		{
			curVector = vecFarOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
			curVector.x *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
		{
			curVector = vecFarOuterPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
			curVector.x *= -1;
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else                                                                 // +x -y quadrant, negate y when copying from scratch array
		{
			curVector = vecFarOuterPoints[NUM_SUPER_ELLIPSE_POINTS-i];
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}

		if ( i != 0 )
		{
			LineDrawHelper( preVector, curVector, viewMatrixInverse, 10, 10, 255 );
		}

		preVector = curVector;
	}

	LineDrawHelper( preVector, vecFarOuterPoints[0], viewMatrixInverse, 10, 10, 255 );

	// Near edge inner superellipse
	for (i=0; i<NUM_SUPER_ELLIPSE_POINTS; i++)
	{
		if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
		{
			curVector = vecNearEdgeInnerPoints[i];
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
		{
			curVector = vecNearEdgeInnerPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
			curVector.x *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
		{
			curVector = vecNearEdgeInnerPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
			curVector.x *= -1;
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else                                                                 // +x -y quadrant, negate y when copying from scratch array
		{
			curVector = vecNearEdgeInnerPoints[NUM_SUPER_ELLIPSE_POINTS-i];
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}

		if ( i != 0 )
		{
			LineDrawHelper( preVector, curVector, viewMatrixInverse, 255, 10, 10 );
		}

		preVector = curVector;
	}

	LineDrawHelper( preVector, vecNearEdgeInnerPoints[0], viewMatrixInverse, 255, 10, 10 );


	// Far inner superellipse
	for (i=0; i<NUM_SUPER_ELLIPSE_POINTS; i++)
	{
		if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
		{
			curVector = vecFarEdgeInnerPoints[i];
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
		{
			curVector = vecFarEdgeInnerPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
			curVector.x *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
		{
			curVector = vecFarEdgeInnerPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
			curVector.x *= -1;
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else                                                                 // +x -y quadrant, negate y when copying from scratch array
		{
			curVector = vecFarEdgeInnerPoints[NUM_SUPER_ELLIPSE_POINTS-i];
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}

		if ( i != 0 )
		{
			LineDrawHelper( preVector, curVector, viewMatrixInverse, 10, 10, 255 );
		}

		preVector = curVector;
	}

	LineDrawHelper( preVector, vecFarEdgeInnerPoints[0], viewMatrixInverse, 10, 10, 255 );


	// Near outer superellipse
	for (i=0; i<NUM_SUPER_ELLIPSE_POINTS; i++)
	{
		if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
		{
			curVector = vecNearEdgeOuterPoints[i];
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
		{
			curVector = vecNearEdgeOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
			curVector.x *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
		{
			curVector = vecNearEdgeOuterPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
			curVector.x *= -1;
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else                                                                 // +x -y quadrant, negate y when copying from scratch array
		{
			curVector = vecNearEdgeOuterPoints[NUM_SUPER_ELLIPSE_POINTS-i];
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}

		if ( i != 0 )
		{
			LineDrawHelper( preVector, curVector, viewMatrixInverse, 255, 10, 10 );
		}

		preVector = curVector;
	}

	LineDrawHelper( preVector, vecNearEdgeOuterPoints[0], viewMatrixInverse, 255, 10, 10 );


	// Far outer superellipse
	for ( i=0; i<NUM_SUPER_ELLIPSE_POINTS; i++ )
	{
		if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
		{
			curVector = vecFarEdgeOuterPoints[i];
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
		{
			curVector = vecFarEdgeOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
			curVector.x *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
		{
			curVector = vecFarEdgeOuterPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
			curVector.x *= -1;
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}
		else                                                                 // +x -y quadrant, negate y when copying from scratch array
		{
			curVector = vecFarEdgeOuterPoints[NUM_SUPER_ELLIPSE_POINTS-i];
			curVector.y *= -1;
			curVector.x += uber.m_fShearx * curVector.z;
			curVector.y += uber.m_fSheary * curVector.z;
		}

		if ( i != 0 )
		{
			LineDrawHelper( preVector, curVector, viewMatrixInverse, 10, 10, 255 );
		}

		preVector = curVector;
	}

	LineDrawHelper( preVector, vecFarEdgeOuterPoints[0], viewMatrixInverse, 10, 10, 255 );

	// Connectors
	for ( i=0; i< NUM_SUPER_ELLIPSE_POINTS; i++ )
	{
		if ( ( i % CONNECTOR_FREQ ) == 0 )
		{
			Vector vecNearEdgeOuter, vecNearOuter, vecFarOuter, vecFarEdgeOuter;

			if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 4))                            // +x +y quadrant, copy from scratch array directly
			{
				vecNearEdgeOuter = vecNearEdgeOuterPoints[i];
				vecNearEdgeOuter.x += uber.m_fShearx * vecNearEdgeOuter.z;
				vecNearEdgeOuter.y += uber.m_fSheary * vecNearEdgeOuter.z;

				vecNearOuter = vecNearOuterPoints[i];
				vecNearOuter.x += uber.m_fShearx * vecNearOuter.z;
				vecNearOuter.y += uber.m_fSheary * vecNearOuter.z;

				vecFarEdgeOuter = vecFarEdgeOuterPoints[i];
				vecFarEdgeOuter.x += uber.m_fShearx * vecFarEdgeOuter.z;
				vecFarEdgeOuter.y += uber.m_fSheary * vecFarEdgeOuter.z;

				vecFarOuter = vecFarOuterPoints[i];
				vecFarOuter.x += uber.m_fShearx * vecFarOuter.z;
				vecFarOuter.y += uber.m_fSheary * vecFarOuter.z;
			}
			else if ( i <= (NUM_SUPER_ELLIPSE_POINTS / 2))                        // -x +y quadrant, negate x when copying from scratch array
			{
				vecNearEdgeOuter = vecNearEdgeOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
				vecNearEdgeOuter.x *= -1;
				vecNearEdgeOuter.x += uber.m_fShearx * vecNearEdgeOuter.z;
				vecNearEdgeOuter.y += uber.m_fSheary * vecNearEdgeOuter.z;

				vecNearOuter = vecNearOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
				vecNearOuter.x *= -1;
				vecNearOuter.x += uber.m_fShearx * vecNearOuter.z;
				vecNearOuter.y += uber.m_fSheary * vecNearOuter.z;

				vecFarEdgeOuter = vecFarEdgeOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
				vecFarEdgeOuter.x *= -1;
				vecFarEdgeOuter.x += uber.m_fShearx * vecFarEdgeOuter.z;
				vecFarEdgeOuter.y += uber.m_fSheary * vecFarEdgeOuter.z;

				vecFarOuter = vecFarOuterPoints[(NUM_SUPER_ELLIPSE_POINTS / 2)-i];
				vecFarOuter.x *= -1;
				vecFarOuter.x += uber.m_fShearx * vecFarOuter.z;
				vecFarOuter.y += uber.m_fSheary * vecFarOuter.z;
			}
			else if ( i <= (3*NUM_SUPER_ELLIPSE_POINTS / 4))                     // -x -y quadrant, negate when copying from scratch array
			{
				vecNearEdgeOuter = vecNearEdgeOuterPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
				vecNearEdgeOuter.x *= -1;
				vecNearEdgeOuter.y *= -1;
				vecNearEdgeOuter.x += uber.m_fShearx * vecNearEdgeOuter.z;
				vecNearEdgeOuter.y += uber.m_fSheary * vecNearEdgeOuter.z;

				vecNearOuter = vecNearOuterPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
				vecNearOuter.x *= -1;
				vecNearOuter.y *= -1;
				vecNearOuter.x += uber.m_fShearx * vecNearOuter.z;
				vecNearOuter.y += uber.m_fSheary * vecNearOuter.z;

				vecFarEdgeOuter = vecFarEdgeOuterPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
				vecFarEdgeOuter.x *= -1;
				vecFarEdgeOuter.y *= -1;
				vecFarEdgeOuter.x += uber.m_fShearx * vecFarEdgeOuter.z;
				vecFarEdgeOuter.y += uber.m_fSheary * vecFarEdgeOuter.z;

				vecFarOuter = vecFarOuterPoints[i-(NUM_SUPER_ELLIPSE_POINTS / 2)];
				vecFarOuter.x *= -1;
				vecFarOuter.y *= -1;
				vecFarOuter.x += uber.m_fShearx * vecFarOuter.z;
				vecFarOuter.y += uber.m_fSheary * vecFarOuter.z;
			}
			else                                                                 // +x -y quadrant, negate y when copying from scratch array
			{
				vecNearEdgeOuter = vecNearEdgeOuterPoints[NUM_SUPER_ELLIPSE_POINTS-i];
				vecNearEdgeOuter.y *= -1;
				vecNearEdgeOuter.x += uber.m_fShearx * vecNearEdgeOuter.z;
				vecNearEdgeOuter.y += uber.m_fSheary * vecNearEdgeOuter.z;

				vecNearOuter = vecNearOuterPoints[NUM_SUPER_ELLIPSE_POINTS-i];
				vecNearOuter.y *= -1;
				vecNearOuter.x += uber.m_fShearx * vecNearOuter.z;
				vecNearOuter.y += uber.m_fSheary * vecNearOuter.z;

				vecFarEdgeOuter = vecFarEdgeOuterPoints[NUM_SUPER_ELLIPSE_POINTS-i];
				vecFarEdgeOuter.y *= -1;
				vecFarEdgeOuter.x += uber.m_fShearx * vecFarEdgeOuter.z;
				vecFarEdgeOuter.y += uber.m_fSheary * vecFarEdgeOuter.z;

				vecFarOuter = vecFarOuterPoints[NUM_SUPER_ELLIPSE_POINTS-i];
				vecFarOuter.y *= -1;
				vecFarOuter.x += uber.m_fShearx * vecFarOuter.z;
				vecFarOuter.y += uber.m_fSheary * vecFarOuter.z;
			}

			LineDrawHelper( vecNearOuter,	 vecNearEdgeOuter, viewMatrixInverse, 255, 10, 10 );
			LineDrawHelper( vecNearOuter,	 vecFarOuter,	   viewMatrixInverse, 220, 10, 220 );
			LineDrawHelper( vecFarEdgeOuter, vecFarOuter,	   viewMatrixInverse, 10, 10, 255 );
		}
	}
}

void CClientShadowMgr::DrawFrustum( const Vector &vOrigin, const VMatrix &matWorldToFlashlight )
{
	VMatrix flashlightToWorld;
	MatrixInverseGeneral( matWorldToFlashlight, flashlightToWorld );

	// Draw boundaries of frustum
	LineDrawHelperProjective( Vector( 0.0f, 0.0f, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 0.0f, 0.0f, 1.0f ), Vector( 0.0f, 1.0f, 1.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 0.0f, 1.0f, 1.0f ), Vector( 0.0f, 1.0f, 0.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 0.0f, 1.0f, 0.0f ), Vector( 0.0f, 0.0f, 0.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 1.0f, 0.0f, 0.0f ), Vector( 1.0f, 0.0f, 1.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 1.0f, 0.0f, 1.0f ), Vector( 1.0f, 1.0f, 1.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 0.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 1.0f, 1.0f, 0.0f ), Vector( 1.0f, 0.0f, 0.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 0.0f, 0.0f, 0.0f ), Vector( 1.0f, 0.0f, 0.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 0.0f, 0.0f, 1.0f ), Vector( 1.0f, 0.0f, 1.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 0.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 1.0f ), flashlightToWorld, 255, 255, 255 );
	LineDrawHelperProjective( Vector( 0.0f, 1.0f, 0.0f ), Vector( 1.0f, 1.0f, 0.0f ), flashlightToWorld, 255, 255, 255 );

	// Draw RGB triad at front plane
	LineDrawHelperProjective( Vector( 0.5f, 0.5f, 0.0f ), Vector( 1.0f, 0.5f, 0.0f ),  flashlightToWorld, 255,   0,   0 );
	LineDrawHelperProjective( Vector( 0.5f, 0.5f, 0.0f ), Vector( 0.5f, 1.0f, 0.0f ),  flashlightToWorld,   0, 255,   0 );
	LineDrawHelperProjective( Vector( 0.5f, 0.5f, 0.0f ), Vector( 0.5f, 0.5f, 0.35f ), flashlightToWorld,   0,   0, 255 );
}



//-----------------------------------------------------------------------------
// Builds a list of leaves inside the flashlight volume
//-----------------------------------------------------------------------------
static void BuildFlashlightLeafList( CShadowLeafEnum *pEnum, const Vector &vecOrigin, const VMatrix &worldToShadow )
{
	// Use an AABB around the frustum to enumerate leaves.
	Vector mins, maxs;
	CalculateAABBFromProjectionMatrix( worldToShadow, &mins, &maxs );
	ISpatialQuery* pQuery = engine->GetBSPTreeQuery();
	pQuery->EnumerateLeavesInBox( mins, maxs, pEnum, 0 );
//	pEnum->ExtractUnconnectedLeaves( vecOrigin );
}


void CClientShadowMgr::BuildFlashlight( ClientShadowHandle_t handle )
{
	// For the 360, we just draw flashlights with the main geometry
	// and bypass the entire shadow casting system.
	ClientShadow_t &shadow = m_Shadows[handle];
	if ( shadowmgr->SinglePassFlashlightModeEnabled() )
	{
		// This will update the matrices, but not do work to add the flashlight to surfaces
		shadowmgr->ProjectFlashlight( shadow.m_ShadowHandle, shadow.m_WorldToShadow, 0, NULL );
		return;
	}

	VPROF_BUDGET( "CClientShadowMgr::BuildFlashlight", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	bool bLightModels = r_flashlightmodels.GetBool();
	bool bLightSpecificEntity = shadow.m_hTargetEntity.Get() != NULL;
	bool bLightWorld = ( shadow.m_Flags & SHADOW_FLAGS_LIGHT_WORLD ) != 0;
	int nCount = 0;
	const int *pLeafList = 0;

	CShadowLeafEnum leafList;
	if ( bLightWorld || ( bLightModels && !bLightSpecificEntity ) )
	{
		const FlashlightState_t& flashlightState = shadowmgr->GetFlashlightState( shadow.m_ShadowHandle );
		BuildFlashlightLeafList( &leafList, flashlightState.m_vecLightOrigin, shadow.m_WorldToShadow );
		nCount = leafList.m_LeafList.Count();
		pLeafList = leafList.m_LeafList.Base();
	}

	if( bLightWorld )
	{
		shadowmgr->ProjectFlashlight( shadow.m_ShadowHandle, shadow.m_WorldToShadow, nCount, pLeafList );
	}
	else
	{
		// This should clear all models and surfaces from this shadow
		shadowmgr->EnableShadow( shadow.m_ShadowHandle, false );
		shadowmgr->EnableShadow( shadow.m_ShadowHandle, true );
	}

	if ( !bLightModels )
		return;

	if ( !bLightSpecificEntity )
	{
		// Add the shadow to the client leaf system so it correctly marks 
		// leafs as being affected by a particular shadow
		ClientLeafSystem()->ProjectFlashlight( shadow.m_ClientLeafShadowHandle, nCount, pLeafList );
		return;
	}

	if( ( shadow.m_Flags & ( SHADOW_FLAGS_SIMPLE_PROJECTION ) ) != 0 )
	{
		return;
	}

	// We know what we are focused on, so just add the shadow directly to that receiver
	Assert( shadow.m_hTargetEntity->GetModel() );

	C_BaseEntity *pChild = shadow.m_hTargetEntity->FirstMoveChild();
	while( pChild )
	{
		int modelType = modelinfo->GetModelType( pChild->GetModel() );
		if (modelType == mod_brush)
		{
			AddShadowToReceiver( handle, pChild, SHADOW_RECEIVER_BRUSH_MODEL );
		}
		else if ( modelType == mod_studio )
		{
			AddShadowToReceiver( handle, pChild, SHADOW_RECEIVER_STUDIO_MODEL );
		}

		pChild = pChild->NextMovePeer();
	}

	int modelType = modelinfo->GetModelType( shadow.m_hTargetEntity->GetModel() );
	if (modelType == mod_brush)
	{
		AddShadowToReceiver( handle, shadow.m_hTargetEntity, SHADOW_RECEIVER_BRUSH_MODEL );
	}
	else if ( modelType == mod_studio )
	{
		AddShadowToReceiver( handle, shadow.m_hTargetEntity, SHADOW_RECEIVER_STUDIO_MODEL );
	}
}


//-----------------------------------------------------------------------------
// Adds the child bounds to the bounding box
//-----------------------------------------------------------------------------
void CClientShadowMgr::AddChildBounds( matrix3x4_t &matWorldToBBox, IClientRenderable* pParent, Vector &vecMins, Vector &vecMaxs )
{
	Vector vecChildMins, vecChildMaxs;
	Vector vecNewChildMins, vecNewChildMaxs;
	matrix3x4_t childToBBox;

	IClientRenderable *pChild = pParent->FirstShadowChild();
	while( pChild )
	{
		// Transform the child bbox into the space of the main bbox
		// FIXME: Optimize this?
		if ( GetActualShadowCastType( pChild ) != SHADOWS_NONE)
		{
			pChild->GetShadowRenderBounds( vecChildMins, vecChildMaxs, SHADOWS_RENDER_TO_TEXTURE );
			ConcatTransforms( matWorldToBBox, pChild->RenderableToWorldTransform(), childToBBox );
			TransformAABB( childToBBox, vecChildMins, vecChildMaxs, vecNewChildMins, vecNewChildMaxs );
			VectorMin( vecMins, vecNewChildMins, vecMins );
			VectorMax( vecMaxs, vecNewChildMaxs, vecMaxs );
		}

		AddChildBounds( matWorldToBBox, pChild, vecMins, vecMaxs );
		pChild = pChild->NextShadowPeer();
	}
}


//-----------------------------------------------------------------------------
// Compute a bounds for the entity + children
//-----------------------------------------------------------------------------
void CClientShadowMgr::ComputeHierarchicalBounds( IClientRenderable *pRenderable, Vector &vecMins, Vector &vecMaxs )
{
	ShadowType_t shadowType = GetActualShadowCastType( pRenderable );

	pRenderable->GetShadowRenderBounds( vecMins, vecMaxs, shadowType );

	// We could use a good solution for this in the regular PC build, since
	// it causes lots of extra bone setups for entities you can't see.
	if ( IsPC() )
	{
		IClientRenderable *pChild = pRenderable->FirstShadowChild();

		// Don't recurse down the tree when we hit a blobby shadow
		if ( pChild && shadowType != SHADOWS_SIMPLE )
		{
			matrix3x4_t matWorldToBBox;
			MatrixInvert( pRenderable->RenderableToWorldTransform(), matWorldToBBox );
			AddChildBounds( matWorldToBBox, pRenderable, vecMins, vecMaxs );
		}
	}
}


//-----------------------------------------------------------------------------
// Shadow update functions
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateStudioShadow( IClientRenderable *pRenderable, ClientShadowHandle_t handle )
{
	if( !( m_Shadows[handle].m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) )
	{
		Vector mins, maxs;
		ComputeHierarchicalBounds( pRenderable, mins, maxs );

		ShadowType_t shadowType = GetActualShadowCastType( handle );
		if ( shadowType != SHADOWS_RENDER_TO_TEXTURE )
		{
			BuildOrthoShadow( pRenderable, handle, mins, maxs );
		}
		else
		{
			BuildRenderToTextureShadow( pRenderable, handle, mins, maxs );
		}
	}
	else
	{
		BuildFlashlight( handle );
	}
}

void CClientShadowMgr::UpdateBrushShadow( IClientRenderable *pRenderable, ClientShadowHandle_t handle )
{
	if( !( m_Shadows[handle].m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) )
	{
		// Compute the bounding box in the space of the shadow...
		Vector mins, maxs;
		ComputeHierarchicalBounds( pRenderable, mins, maxs );

		ShadowType_t shadowType = GetActualShadowCastType( handle );
		if ( shadowType != SHADOWS_RENDER_TO_TEXTURE )
		{
			BuildOrthoShadow( pRenderable, handle, mins, maxs );
		}
		else
		{
			BuildRenderToTextureShadow( pRenderable, handle, mins, maxs );
		}
	}
	else
	{
		VPROF_BUDGET( "CClientShadowMgr::UpdateBrushShadow", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

		BuildFlashlight( handle );
	}
}


#ifdef _DEBUG

static bool s_bBreak = false;

void ShadowBreak_f()
{
	s_bBreak = true;
}

static ConCommand r_shadowbreak("r_shadowbreak", ShadowBreak_f);

#endif // _DEBUG


bool CClientShadowMgr::WillParentRenderBlobbyShadow( IClientRenderable *pRenderable )
{
	if ( !pRenderable )
		return false;

	IClientRenderable *pShadowParent = pRenderable->GetShadowParent();
	if ( !pShadowParent )
		return false;

	// If there's *no* shadow casting type, then we want to see if we can render into its parent
 	ShadowType_t shadowType = GetActualShadowCastType( pShadowParent );
	if ( shadowType == SHADOWS_NONE )
		return WillParentRenderBlobbyShadow( pShadowParent );

	return shadowType == SHADOWS_SIMPLE;
}


//-----------------------------------------------------------------------------
// Are we the child of a shadow with render-to-texture?
//-----------------------------------------------------------------------------
bool CClientShadowMgr::ShouldUseParentShadow( IClientRenderable *pRenderable )
{
	if ( !pRenderable )
		return false;

	IClientRenderable *pShadowParent = pRenderable->GetShadowParent();
	if ( !pShadowParent )
		return false;

	// Can't render into the parent if the parent is blobby
	ShadowType_t shadowType = GetActualShadowCastType( pShadowParent );
	if ( shadowType == SHADOWS_SIMPLE )
		return false;

	// If there's *no* shadow casting type, then we want to see if we can render into its parent
 	if ( shadowType == SHADOWS_NONE )
		return ShouldUseParentShadow( pShadowParent );

	// Here, the parent uses a render-to-texture shadow
	return true;
}

//-----------------------------------------------------------------------------
// Before we render any view, make sure all shadows are re-projected vs world
//-----------------------------------------------------------------------------
void CClientShadowMgr::ReprojectShadows()
{
	// only update shadows once per frame
	Assert( gpGlobals->framecount != m_nPrevFrameCount );
	m_nPrevFrameCount = gpGlobals->framecount;

	VPROF_BUDGET( "CClientShadowMgr::ReprojectShadows", VPROF_BUDGETGROUP_SHADOW_RENDERING );
	MDLCACHE_CRITICAL_SECTION();

	//
	// -- Shadow Depth Textures -----------------------
	//

	{
		// VPROF scope
		//VPROF_BUDGET( "CClientShadowMgr::PreRender", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

		// If someone turned shadow depth mapping on but we can't do it, force it off
		if ( r_flashlightdepthtexture.GetBool() && !g_pMaterialSystemHardwareConfig->SupportsShadowDepthTextures() )
		{
			r_flashlightdepthtexture.SetValue( 0 );
			ShutdownDepthTextureShadows();	
		}

		bool bDepthTextureActive     = r_flashlightdepthtexture.GetBool();
		int  nDepthTextureResolution = r_flashlightdepthres.GetInt();
		int  nDepthTextureResolutionHigh = r_flashlightdepthreshigh.GetInt();

		if ( ( bDepthTextureActive == true ) && ( m_bDepthTextureActive == true ) &&
			 ( nDepthTextureResolution != m_nDepthTextureResolution || nDepthTextureResolutionHigh != m_nDepthTextureResolutionHigh ) )
		{
			// If shadow depth texturing remains on, but resolution changed, shut down and reinitialize depth textures
			ShutdownDepthTextureShadows();	
			InitDepthTextureShadows();
		}
		else
		{
			if ( bDepthTextureActive && !m_bDepthTextureActive )
			{
				// If shadow depth texture is now needed but wasn't allocated, allocate it
				// Turning on shadow depth mapping
				materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
				InitDepthTextureShadows();	// only allocates buffers if they don't already exist
				materials->FinishRenderTargetAllocation();
			}
		}
	}

	//
	// -- Render to Texture Shadows -----------------------
	//

	if ( !r_shadows.GetBool() )
	{
		return;
	}

	bool bRenderToTextureActive = r_shadowrendertotexture.GetBool();
	if ( !m_RenderToTextureActive && bRenderToTextureActive )
	{
		materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
		InitRenderToTextureShadows();
		materials->FinishRenderTargetAllocation();
		UpdateAllShadows();
		return;
	}

	if ( m_RenderToTextureActive && !bRenderToTextureActive )
	{
		materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
		ShutdownRenderToTextureShadows();
		materials->FinishRenderTargetAllocation();
		UpdateAllShadows();
		return;
	}

	m_bUpdatingDirtyShadows = true;

	if ( r_shadow_half_update_rate.GetBool() )
	{
		UpdateDirtyShadowsHalfRate();
	}
	else
	{
		UpdateDirtyShadows();
	}

	m_bUpdatingDirtyShadows = false;

	DestroyQueuedShadows();
}

//-----------------------------------------------------------------------------
// Update all shadows in the dirty shadow set
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateDirtyShadows()
{
	if ( r_shadow_debug_spew.GetBool() )
	{
		DevMsg( "dirty shadows: %3d\n", m_DirtyShadows.Count() );
	}

	unsigned short i = m_DirtyShadows.FirstInorder();
	while ( i != m_DirtyShadows.InvalidIndex() )
	{
		MDLCACHE_CRITICAL_SECTION();
		ClientShadowHandle_t& handle = m_DirtyShadows[ i ];
		UpdateDirtyShadow( handle );
		i = m_DirtyShadows.NextInorder(i);
	}
	m_DirtyShadows.RemoveAll();

	// Transparent shadows must remain dirty, since they were not re-projected
	int nCount = m_TransparentShadows.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_DirtyShadows.Insert( m_TransparentShadows[i] );
	}
	m_TransparentShadows.RemoveAll();
}

//-----------------------------------------------------------------------------
// Update half the shadows in the dirty shadow set, leaving the rest for next frame
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateDirtyShadowsHalfRate()
{
	unsigned short i = m_DirtyShadowsLeftOver.FirstInorder();
	while ( i != m_DirtyShadowsLeftOver.InvalidIndex() )
	{
		MDLCACHE_CRITICAL_SECTION();
		ClientShadowHandle_t& handle = m_DirtyShadowsLeftOver[ i ];
		UpdateDirtyShadow( handle );

		// remove from real dirty set, so that we don't touch shadows twice
		unsigned short nIdx = m_DirtyShadows.Find( handle );
		if ( nIdx != m_DirtyShadows.InvalidIndex() )
		{
			m_DirtyShadows.RemoveAt( nIdx );
		}
		i = m_DirtyShadowsLeftOver.NextInorder( i );
	}
	int nNumShadowsProcessed = m_DirtyShadowsLeftOver.Count();

	int nNumTotalDirtyShadows = nNumShadowsProcessed + m_DirtyShadows.Count();
	int nNumShadowsToProcess = nNumTotalDirtyShadows / 2 + 1;
	if ( r_shadow_debug_spew.GetBool() )
	{
		DevMsg( "dirty shadows: %3d\n", nNumShadowsToProcess );
	}

	nNumShadowsToProcess -= nNumShadowsProcessed;
	nNumShadowsToProcess = MAX( 0, nNumShadowsToProcess );
	nNumShadowsToProcess = MIN( m_DirtyShadows.Count(), nNumShadowsToProcess );
	//DevMsg( "dirty: %2d leftover: %2d total: %2d from main list: %2d processed: %d\n", m_DirtyShadows.Count(), m_DirtyShadowsLeftOver.Count(), nNumTotalDirtyShadows, nNumShadowsToProcess, nNumShadowsProcessed + nNumShadowsToProcess );

	m_DirtyShadowsLeftOver.RemoveAll();

	//
	// process any additional dirty shadows
	//

	i = m_DirtyShadows.FirstInorder();
	for ( int j = 0; j < nNumShadowsToProcess; j++ )
	{
		MDLCACHE_CRITICAL_SECTION();
		ClientShadowHandle_t& handle = m_DirtyShadows[ i ];
		UpdateDirtyShadow( handle );
		i = m_DirtyShadows.NextInorder( i );
	}
	// put the leftover shadows into the leftover bucket
	while ( i != m_DirtyShadows.InvalidIndex() )
	{
		ClientShadowHandle_t& handle = m_DirtyShadows[ i ];
		i = m_DirtyShadows.NextInorder( i );
		m_DirtyShadowsLeftOver.Insert( handle );
	}

	m_DirtyShadows.RemoveAll();

	// Transparent shadows must remain dirty, since they were not re-projected
	int nCount = m_TransparentShadows.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_DirtyShadows.Insert( m_TransparentShadows[i] );
	}
	m_TransparentShadows.RemoveAll();
}

//-----------------------------------------------------------------------------
// Updates a single dirty shadow
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateDirtyShadow( ClientShadowHandle_t handle )
{
	Assert( m_Shadows.IsValidIndex( handle ) );
	if ( IsShadowingFromWorldLights() )
	{
		UpdateShadowDirectionFromLocalLightSource( handle );
	}
	UpdateProjectedTextureInternal( handle, false );
}

//-----------------------------------------------------------------------------
// Convar callback
//-----------------------------------------------------------------------------
void HalfUpdateRateCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	s_ClientShadowMgr.FlushLeftOverDirtyShadows();
}

//-----------------------------------------------------------------------------
// Flushes any leftover dirty shadows into the main dirty shadow set
//-----------------------------------------------------------------------------
void CClientShadowMgr::FlushLeftOverDirtyShadows()
{
	unsigned short i = m_DirtyShadowsLeftOver.FirstInorder();
	while ( i != m_DirtyShadowsLeftOver.InvalidIndex() )
	{
		m_DirtyShadows.InsertIfNotFound( m_DirtyShadowsLeftOver[ i ] );
		i = m_DirtyShadowsLeftOver.NextInorder(i);
	}
	m_DirtyShadowsLeftOver.RemoveAll();
}

//-----------------------------------------------------------------------------
// Gets the entity whose shadow this shadow will render into
//-----------------------------------------------------------------------------
IClientRenderable *CClientShadowMgr::GetParentShadowEntity( ClientShadowHandle_t handle )
{
	ClientShadow_t& shadow = m_Shadows[handle];
	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );
	if ( pRenderable )
	{
		if ( ShouldUseParentShadow( pRenderable ) )
		{
			IClientRenderable *pParent = pRenderable->GetShadowParent();
			while ( GetActualShadowCastType( pParent ) == SHADOWS_NONE )
			{
				pParent = pParent->GetShadowParent();
				Assert( pParent );
			}
			return pParent;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Marks a shadow as needing re-projection
//-----------------------------------------------------------------------------
CThreadFastMutex g_DirtyListAddMutex; // This should be made lock free? [3/19/2008 tom]
void CClientShadowMgr::AddToDirtyShadowList( ClientShadowHandle_t handle, bool bForce )
{
	// Don't add to the dirty shadow list while we're iterating over it
	// The only way this can happen is if a child is being rendered into a parent
	// shadow, and we don't need it to be added to the dirty list in that case.
	if ( m_bUpdatingDirtyShadows )
		return;

	if ( handle == CLIENTSHADOW_INVALID_HANDLE )
		return;

	AUTO_LOCK( g_DirtyListAddMutex );

	Assert( m_DirtyShadows.Find( handle ) == m_DirtyShadows.InvalidIndex() );
	m_DirtyShadows.Insert( handle );

	// This pretty much guarantees we'll recompute the shadow
	if ( bForce )
	{
		m_Shadows[handle].m_LastAngles.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	}

	// If we use our parent shadow, then it's dirty too...
	IClientRenderable *pParent = GetParentShadowEntity( handle );
	if ( pParent )
	{
		AddToDirtyShadowList( pParent, bForce );
	}
}


//-----------------------------------------------------------------------------
// Marks a shadow as needing re-projection
//-----------------------------------------------------------------------------
void CClientShadowMgr::AddToDirtyShadowList( IClientRenderable *pRenderable, bool bForce )
{
	// Don't add to the dirty shadow list while we're iterating over it
	// The only way this can happen is if a child is being rendered into a parent
	// shadow, and we don't need it to be added to the dirty list in that case.
	if ( m_bUpdatingDirtyShadows )
		return;

	AUTO_LOCK( g_DirtyListAddMutex );

	// Are we already in the dirty list?
	if ( pRenderable->IsShadowDirty( ) )
		return;

	ClientShadowHandle_t handle = pRenderable->GetShadowHandle();
	if ( handle == CLIENTSHADOW_INVALID_HANDLE )
		return;

#ifdef _DEBUG
	// Make sure everything's consistent
	if ( handle != CLIENTSHADOW_INVALID_HANDLE )
	{
		IClientRenderable *pShadowRenderable = ClientEntityList().GetClientRenderableFromHandle( m_Shadows[handle].m_Entity );
		Assert( pRenderable == pShadowRenderable );
	}
#endif

	pRenderable->MarkShadowDirty( true );
	AddToDirtyShadowList( handle, bForce );
}


//-----------------------------------------------------------------------------
// Marks the render-to-texture shadow as needing to be re-rendered
//-----------------------------------------------------------------------------
void CClientShadowMgr::MarkRenderToTextureShadowDirty( ClientShadowHandle_t handle )
{
	// Don't add bogus handles!
	if (handle != CLIENTSHADOW_INVALID_HANDLE)
	{
		// Mark the shadow has having a dirty renter-to-texture
		ClientShadow_t& shadow = m_Shadows[handle];
		shadow.m_Flags |= SHADOW_FLAGS_TEXTURE_DIRTY;

		// If we use our parent shadow, then it's dirty too...
		IClientRenderable *pParent = GetParentShadowEntity( handle );
		if ( pParent )
		{
			ClientShadowHandle_t parentHandle = pParent->GetShadowHandle();
			if ( parentHandle != CLIENTSHADOW_INVALID_HANDLE )
			{
				m_Shadows[parentHandle].m_Flags |= SHADOW_FLAGS_TEXTURE_DIRTY;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Update a shadow
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateShadow( ClientShadowHandle_t handle, bool force )
{
	ClientShadow_t& shadow = m_Shadows[handle];

	// Get the client entity....
	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );
	if ( !pRenderable )
	{
		// Retire the shadow if the entity is gone
		DestroyShadow( handle );
		return;
	}

	// Don't bother if there's no model on the renderable
	if ( !pRenderable->GetModel() )
	{
		pRenderable->MarkShadowDirty( false );
		return;
	}

	// Mark shadow as dirty if no split screen user needs it
	bool bShouldDraw = false;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		if ( pRenderable->ShouldDrawForSplitScreenUser( i ) )
		{
			bShouldDraw = true;
			break;
		}
	}
	if ( !bShouldDraw )
	{
		pRenderable->MarkShadowDirty( false );
		return;
	}

	// FIXME: NOTE! Because this is called from PreRender, the falloff bias is
	// off by a frame. We could move the code in PreRender to occur after world
	// list building is done to fix this issue.
	// Don't bother with it if the shadow is totally transparent
	if ( shadow.m_FalloffBias == 255 )
	{
		if ( !r_shadow_deferred.GetBool() )
		{
			shadowmgr->EnableShadow( shadow.m_ShadowHandle, false );
		}

		m_TransparentShadows.AddToTail( handle );
		return;
	}

#ifdef _DEBUG
	if (s_bBreak)
	{
		s_bBreak = false;
	}
#endif
	// Hierarchical children shouldn't be projecting shadows...
	// Check to see if it's a child of an entity with a render-to-texture shadow...
	if ( ShouldUseParentShadow( pRenderable ) || WillParentRenderBlobbyShadow( pRenderable ) )
	{
		if ( !r_shadow_deferred.GetBool() )
		{
			shadowmgr->EnableShadow( shadow.m_ShadowHandle, false );
		}
		pRenderable->MarkShadowDirty( false );
		return;
	}

	if ( !r_shadow_deferred.GetBool() )
	{
		shadowmgr->EnableShadow( shadow.m_ShadowHandle, true );
	}

	// Figure out if the shadow moved...
	// Even though we have dirty bits, some entities
	// never clear those dirty bits
	const Vector& origin = pRenderable->GetRenderOrigin();
	const QAngle& angles = pRenderable->GetRenderAngles();

	if (force || (origin != shadow.m_LastOrigin) || (angles != shadow.m_LastAngles))
	{
		// Store off the new pos/orientation
		VectorCopy( origin, shadow.m_LastOrigin );
		VectorCopy( angles, shadow.m_LastAngles );

		CMatRenderContextPtr pRenderContext( materials );
		const model_t *pModel = pRenderable->GetModel();
		MaterialFogMode_t fogMode = pRenderContext->GetFogMode();
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
		switch( modelinfo->GetModelType( pModel ) )
		{
		case mod_brush:
			UpdateBrushShadow( pRenderable, handle );
			break;

		case mod_studio:
			UpdateStudioShadow( pRenderable, handle );
			break;

		default:
			// Shouldn't get here if not a brush or studio
			Assert(0);
			break;
		}
		pRenderContext->FogMode( fogMode );
	}

	// NOTE: We can't do this earlier because pEnt->GetRenderOrigin() can
	// provoke a recomputation of render origin, which, for aiments, can cause everything
	// to be marked as dirty. So don't clear the flag until this point.
	pRenderable->MarkShadowDirty( false );
}


//-----------------------------------------------------------------------------
// Update a shadow
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateProjectedTextureInternal( ClientShadowHandle_t handle, bool force )
{
	ClientShadow_t& shadow = m_Shadows[handle];

	if( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) != 0 )
	{
		VPROF_BUDGET( "CClientShadowMgr::UpdateProjectedTextureInternal", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

		Assert( ( shadow.m_Flags & SHADOW_FLAGS_SHADOW ) == 0 );
		ClientShadow_t& shadow = m_Shadows[handle];

		shadowmgr->EnableShadow( shadow.m_ShadowHandle, true );

		// FIXME: What's the difference between brush and model shadows for light projectors? Answer: nothing.
		UpdateBrushShadow( NULL, handle );
	}
	else
	{
		Assert( shadow.m_Flags & SHADOW_FLAGS_SHADOW );
		Assert( ( shadow.m_Flags & SHADOW_FLAGS_FLASHLIGHT ) == 0 );
		UpdateShadow( handle, force );
	}
}


//-----------------------------------------------------------------------------
// Update a shadow
//-----------------------------------------------------------------------------
void CClientShadowMgr::UpdateProjectedTexture( ClientShadowHandle_t handle, bool force )
{
	VPROF_BUDGET( "CClientShadowMgr::UpdateProjectedTexture", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	if ( handle == CLIENTSHADOW_INVALID_HANDLE )
		return;

	// NOTE: This can only work for flashlights, since UpdateProjectedTextureInternal
	// depends on the falloff offset to cull shadows.
	ClientShadow_t &shadow = m_Shadows[ handle ];
	if( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) == 0 )
	{
		Warning( "CClientShadowMgr::UpdateProjectedTexture can only be used with flashlights!\n" );
		return;
	}

	UpdateProjectedTextureInternal( handle, force );
	RemoveShadowFromDirtyList( handle );
}

	
//-----------------------------------------------------------------------------
// Computes bounding sphere
//-----------------------------------------------------------------------------
void CClientShadowMgr::ComputeBoundingSphere( IClientRenderable* pRenderable, Vector& origin, float& radius )
{
	Assert( pRenderable );
	Vector mins, maxs;
	pRenderable->GetShadowRenderBounds( mins, maxs, GetActualShadowCastType( pRenderable ) );
	Vector size;
	VectorSubtract( maxs, mins, size );
	radius = size.Length() * 0.5f;

	// Compute centroid (local space)
	Vector centroid;
	VectorAdd( mins, maxs, centroid );
	centroid *= 0.5f;

	// Transform centroid into world space
	Vector vec[3];
	AngleVectors( pRenderable->GetRenderAngles(), &vec[0], &vec[1], &vec[2] );
	vec[1] *= -1.0f;

	VectorCopy( pRenderable->GetRenderOrigin(), origin );
	VectorMA( origin, centroid.x, vec[0], origin );
	VectorMA( origin, centroid.y, vec[1], origin );
	VectorMA( origin, centroid.z, vec[2], origin );
}


//-----------------------------------------------------------------------------
// Computes a rough AABB encompassing the volume of the shadow
//-----------------------------------------------------------------------------
void CClientShadowMgr::ComputeShadowBBox( IClientRenderable *pRenderable, ClientShadowHandle_t shadowHandle, const Vector &vecAbsCenter, float flRadius, Vector *pAbsMins, Vector *pAbsMaxs )
{
	// This is *really* rough. Basically we simply determine the
	// maximum shadow casting length and extrude the box by that distance

	Vector vecShadowDir = GetShadowDirection( shadowHandle );
	for (int i = 0; i < 3; ++i)
	{
		float flShadowCastDistance = GetShadowDistance( pRenderable );
		float flDist = flShadowCastDistance * vecShadowDir[i];

		if (vecShadowDir[i] < 0)
		{
			(*pAbsMins)[i] = vecAbsCenter[i] - flRadius + flDist;
			(*pAbsMaxs)[i] = vecAbsCenter[i] + flRadius;
		}
		else
		{
			(*pAbsMins)[i] = vecAbsCenter[i] - flRadius;
			(*pAbsMaxs)[i] = vecAbsCenter[i] + flRadius + flDist;
		}
	}
}


//-----------------------------------------------------------------------------
// Compute a separating axis...
//-----------------------------------------------------------------------------
bool CClientShadowMgr::ComputeSeparatingPlane( IClientRenderable* pRend1, IClientRenderable* pRend2, cplane_t* pPlane )
{
	Vector min1, max1, min2, max2;
	pRend1->GetShadowRenderBounds( min1, max1, GetActualShadowCastType( pRend1 ) );
	pRend2->GetShadowRenderBounds( min2, max2, GetActualShadowCastType( pRend2 ) );
	return ::ComputeSeparatingPlane( 
		pRend1->GetRenderOrigin(), pRend1->GetRenderAngles(), min1, max1,
		pRend2->GetRenderOrigin(), pRend2->GetRenderAngles(), min2, max2,
		3.0f, pPlane );
}


//-----------------------------------------------------------------------------
// Cull shadows based on rough bounding volumes
//-----------------------------------------------------------------------------
bool CClientShadowMgr::CullReceiver( ClientShadowHandle_t handle, IClientRenderable* pRenderable,
									IClientRenderable* pSourceRenderable )
{
	// check flags here instead and assert !pSourceRenderable
	if( m_Shadows[handle].m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) )
	{
		VPROF_BUDGET( "CClientShadowMgr::CullReceiver", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

		Assert( !pSourceRenderable );	
		const Frustum_t &frustum = shadowmgr->GetFlashlightFrustum( m_Shadows[handle].m_ShadowHandle );

		Vector mins, maxs;
		pRenderable->GetRenderBoundsWorldspace( mins, maxs );

		return frustum.CullBox( mins, maxs );
	}

	Assert( pSourceRenderable );	
	// Compute a bounding sphere for the renderable
	Vector origin;
	float radius;
	ComputeBoundingSphere( pRenderable, origin, radius );

	// Transform the sphere center into the space of the shadow
	Vector localOrigin;
	const ClientShadow_t& shadow = m_Shadows[handle];

	Vector3DMultiplyPosition( shadow.m_WorldToShadow, origin, localOrigin );

	// Compute a rough bounding box for the shadow (in shadow space)
	Vector shadowMin, shadowMax;
	shadowMin.Init( -shadow.m_WorldSize.x * 0.5f, -shadow.m_WorldSize.y * 0.5f, 0 );
	shadowMax.Init( shadow.m_WorldSize.x * 0.5f, shadow.m_WorldSize.y * 0.5f, shadow.m_MaxDist );

	// If the bounding sphere doesn't intersect with the shadow volume, cull
	if (!IsBoxIntersectingSphere( shadowMin, shadowMax, localOrigin, radius ))
		return true;

	Vector originSource;
	float radiusSource;
	ComputeBoundingSphere( pSourceRenderable, originSource, radiusSource );

	// Fast check for separating plane...
	bool foundSeparatingPlane = false;
	cplane_t plane;
	if (!IsSphereIntersectingSphere( originSource, radiusSource, origin, radius ))
	{
		foundSeparatingPlane = true;

		// the plane normal doesn't need to be normalized...
		VectorSubtract( origin, originSource, plane.normal );
	}
	else
	{
		foundSeparatingPlane = ComputeSeparatingPlane( pRenderable, pSourceRenderable, &plane );
	}

	if (foundSeparatingPlane)
	{
		// Compute which side of the plane the renderable is on..
		Vector vecShadowDir = GetShadowDirection( handle );
		float shadowDot = DotProduct( vecShadowDir, plane.normal );
		float receiverDot = DotProduct( plane.normal, origin );
		float sourceDot = DotProduct( plane.normal, originSource );

		if (shadowDot > 0.0f)
		{
			if (receiverDot <= sourceDot)
			{
//				Vector dest;
//				VectorMA( pSourceRenderable->GetRenderOrigin(), 50, plane.normal, dest ); 
//				debugoverlay->AddLineOverlay( pSourceRenderable->GetRenderOrigin(), dest, 255, 255, 0, true, 1.0f );
				return true;
			}
			else
			{
//				Vector dest;
//				VectorMA( pSourceRenderable->GetRenderOrigin(), 50, plane.normal, dest ); 
//				debugoverlay->AddLineOverlay( pSourceRenderable->GetRenderOrigin(), dest, 255, 0, 0, true, 1.0f );
			}
		}
		else
		{
			if (receiverDot >= sourceDot)
			{
//				Vector dest;
//				VectorMA( pSourceRenderable->GetRenderOrigin(), -50, plane.normal, dest ); 
//				debugoverlay->AddLineOverlay( pSourceRenderable->GetRenderOrigin(), dest, 255, 255, 0, true, 1.0f );
				return true;
			}
			else
			{
//				Vector dest;
//				VectorMA( pSourceRenderable->GetRenderOrigin(), 50, plane.normal, dest ); 
//				debugoverlay->AddLineOverlay( pSourceRenderable->GetRenderOrigin(), dest, 255, 0, 0, true, 1.0f );
			}
		}
	}

	// No additional clip planes? ok then it's a valid receiver
	/*
	if (shadow.m_ClipPlaneCount == 0)
		return false;

	// Check the additional cull planes
	int i;
	for ( i = 0; i < shadow.m_ClipPlaneCount; ++i)
	{
		// Fast sphere cull
		if (DotProduct( origin, shadow.m_ClipPlane[i] ) - radius > shadow.m_ClipDist[i])
			return true;
	}

	// More expensive box on plane side cull...
	Vector vec[3];
	Vector mins, maxs;
	cplane_t plane;
	AngleVectors( pRenderable->GetRenderAngles(), &vec[0], &vec[1], &vec[2] );
	pRenderable->GetBounds( mins, maxs );

	for ( i = 0; i < shadow.m_ClipPlaneCount; ++i)
	{
		// Transform the plane into the space of the receiver
		plane.normal.x = DotProduct( vec[0], shadow.m_ClipPlane[i] );
		plane.normal.y = DotProduct( vec[1], shadow.m_ClipPlane[i] );
		plane.normal.z = DotProduct( vec[2], shadow.m_ClipPlane[i] );

		plane.dist = shadow.m_ClipDist[i] - DotProduct( shadow.m_ClipPlane[i], pRenderable->GetRenderOrigin() );

		// If the box is on the front side of the plane, we're done.
		if (BoxOnPlaneSide2( mins, maxs, &plane, 3.0f ) == 1)
			return true;
	}
	*/

	return false;
}


//-----------------------------------------------------------------------------
// deals with shadows being added to shadow receivers
//-----------------------------------------------------------------------------
void CClientShadowMgr::AddShadowToReceiver( ClientShadowHandle_t handle,
	IClientRenderable* pRenderable, ShadowReceiver_t type )
{
	ClientShadow_t &shadow = m_Shadows[handle];

	// Don't add a shadow cast by an object to itself...
	IClientRenderable* pSourceRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );

	// NOTE: if pSourceRenderable == NULL, the source is probably a flashlight since there is no entity.
	if (pSourceRenderable == pRenderable)
		return;

	// Don't bother if this renderable doesn't receive shadows or light from flashlights
	if( !pRenderable->ShouldReceiveProjectedTextures( SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK ) )
		return;

	// Cull if the origin is on the wrong side of a shadow clip plane....
	if ( CullReceiver( handle, pRenderable, pSourceRenderable ) )
		return;

	// Do different things depending on the receiver type
	switch( type )
	{
	case SHADOW_RECEIVER_BRUSH_MODEL:

		if( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) )
		{
			VPROF_BUDGET( "CClientShadowMgr::AddShadowToReceiver", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

			if( (!shadow.m_hTargetEntity) || IsFlashlightTarget( handle, pRenderable ) )
			{
				shadowmgr->AddShadowToBrushModel( shadow.m_ShadowHandle, 
					const_cast<model_t*>(pRenderable->GetModel()),
					pRenderable->GetRenderOrigin(), pRenderable->GetRenderAngles() );
			}
		}
		else
		{
			if ( !r_shadow_deferred.GetBool() )
			{
				shadowmgr->AddShadowToBrushModel( shadow.m_ShadowHandle, 
					const_cast<model_t*>(pRenderable->GetModel()),
					pRenderable->GetRenderOrigin(), pRenderable->GetRenderAngles() );

			}
		}
		break;

	case SHADOW_RECEIVER_STATIC_PROP:
		// Don't add shadows to props if we're not using render-to-texture
		if ( GetActualShadowCastType( handle ) == SHADOWS_RENDER_TO_TEXTURE )
		{
			if ( !r_shadow_deferred.GetBool() )
			{
				// Also don't add them unless an NPC or player casts them..
				// They are wickedly expensive!!!
				C_BaseEntity *pEnt = pSourceRenderable->GetIClientUnknown()->GetBaseEntity();
				if ( pEnt && ( pEnt->GetFlags() & (FL_NPC | FL_CLIENT)) )
				{
					staticpropmgr->AddShadowToStaticProp( shadow.m_ShadowHandle, pRenderable );
				}
			}
		}
		else if( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) )
		{
			VPROF_BUDGET( "CClientShadowMgr::AddShadowToReceiver", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

			if( (!shadow.m_hTargetEntity) || IsFlashlightTarget( handle, pRenderable ) )
			{
				staticpropmgr->AddShadowToStaticProp( shadow.m_ShadowHandle, pRenderable );
			}
		}
		break;

	case SHADOW_RECEIVER_STUDIO_MODEL:
		if( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) )
		{
			VPROF_BUDGET( "CClientShadowMgr::AddShadowToReceiver", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

			if( (!shadow.m_hTargetEntity) || IsFlashlightTarget( handle, pRenderable ) )
			{
				pRenderable->CreateModelInstance();
				shadowmgr->AddShadowToModel( shadow.m_ShadowHandle, pRenderable->GetModelInstance() );
			}
		}
		break;
//	default:
	}
}


//-----------------------------------------------------------------------------
// deals with shadows being added to shadow receivers
//-----------------------------------------------------------------------------
void CClientShadowMgr::RemoveAllShadowsFromReceiver( 
					IClientRenderable* pRenderable, ShadowReceiver_t type )
{
	// Don't bother if this renderable doesn't receive shadows
	if ( !pRenderable->ShouldReceiveProjectedTextures( SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK ) )
		return;

	// Do different things depending on the receiver type
	switch( type )
	{
	case SHADOW_RECEIVER_BRUSH_MODEL:
		{
			model_t* pModel = const_cast<model_t*>(pRenderable->GetModel());
			shadowmgr->RemoveAllShadowsFromBrushModel( pModel );
		}
		break;

	case SHADOW_RECEIVER_STATIC_PROP:
		staticpropmgr->RemoveAllShadowsFromStaticProp(pRenderable);
		break;

	case SHADOW_RECEIVER_STUDIO_MODEL:
		if( pRenderable && pRenderable->GetModelInstance() != MODEL_INSTANCE_INVALID )
		{
			Assert(shadowmgr);
			shadowmgr->RemoveAllShadowsFromModel( pRenderable->GetModelInstance() );
		}
		break;

//	default:
//		// FIXME: How do deal with this stuff? Add a method to IClientRenderable?
//		C_BaseEntity* pEnt = static_cast<C_BaseEntity*>(pRenderable);
//		pEnt->RemoveAllShadows();
	}
}


//-----------------------------------------------------------------------------
// Computes + sets the render-to-texture texcoords
//-----------------------------------------------------------------------------
void CClientShadowMgr::SetRenderToTextureShadowTexCoords( ShadowHandle_t handle, int x, int y, int w, int h )
{
	// Let the shadow mgr know about the texture coordinates...
	// That way it'll be able to batch rendering better.
	int textureW, textureH;
	m_ShadowAllocator.GetTotalTextureSize( textureW, textureH );

	// Go in a half-pixel to avoid blending with neighboring textures..
	float u, v, du, dv;

	u  = ((float)x + 0.5f) / (float)textureW;
	v  = ((float)y + 0.5f) / (float)textureH;
	du = ((float)w - 1) / (float)textureW;
	dv = ((float)h - 1) / (float)textureH;

	shadowmgr->SetShadowTexCoord( handle, u, v, du, dv );
}


void CClientShadowMgr::SetRenderToTextureShadowTexCoords( ClientShadow_t& shadow, int x, int y, int w, int h )
{
	// Let the shadow mgr know about the texture coordinates...
	// That way it'll be able to batch rendering better.
	int textureW, textureH;
	m_ShadowAllocator.GetTotalTextureSize( textureW, textureH );

	// Go in a half-pixel to avoid blending with neighboring textures..
	float u, v, du, dv;

	u  = ((float)x + 0.5f) / (float)textureW;
	v  = ((float)y + 0.5f) / (float)textureH;
	du = ((float)w - 1) / (float)textureW;
	dv = ((float)h - 1) / (float)textureH;

	shadow.m_TexCoordOffset.Init( u, v );
	shadow.m_TexCoordScale.Init( du, dv );
}

//-----------------------------------------------------------------------------
// Setup all children shadows
//-----------------------------------------------------------------------------
bool CClientShadowMgr::BuildSetupShadowHierarchy( IClientRenderable *pRenderable, const ClientShadow_t &shadow, bool bChild )
{
	bool bDrewTexture = false;

	// Stop traversing when we hit a blobby shadow
	ShadowType_t shadowType = GetActualShadowCastType( pRenderable );
	if ( pRenderable && shadowType == SHADOWS_SIMPLE )
		return false;

	if ( !pRenderable || shadowType != SHADOWS_NONE )
	{
		bool bDrawModelShadow;
		if ( !bChild )
		{
			bDrawModelShadow = ((shadow.m_Flags & SHADOW_FLAGS_BRUSH_MODEL) == 0);
		}
		else
		{
			int nModelType = modelinfo->GetModelType( pRenderable->GetModel() );
			bDrawModelShadow = nModelType == mod_studio;
		}

		if ( bDrawModelShadow )
		{
			C_BaseEntity *pEntity = pRenderable->GetIClientUnknown()->GetBaseEntity();
			if ( pEntity )
			{
				if ( pEntity->IsNPC() )
				{
					s_NPCShadowBoneSetups.AddToTail( assert_cast<C_BaseAnimating *>( pEntity ) );
				}
				else if ( pEntity->GetBaseAnimating() )
				{
					s_NonNPCShadowBoneSetups.AddToTail( assert_cast<C_BaseAnimating *>( pEntity ) );
				}

			}
			bDrewTexture = true;
		}
	}

	if ( !pRenderable )
		return bDrewTexture;

	IClientRenderable *pChild;
	for ( pChild = pRenderable->FirstShadowChild(); pChild; pChild = pChild->NextShadowPeer() )
	{
		if ( BuildSetupShadowHierarchy( pChild, shadow, true ) )
		{
			bDrewTexture = true;
		}
	}
	return bDrewTexture;
}

//-----------------------------------------------------------------------------
// Draws all children shadows into our own
//-----------------------------------------------------------------------------
bool CClientShadowMgr::DrawShadowHierarchy( IClientRenderable *pRenderable, const ClientShadow_t &shadow, bool bChild )
{
	bool bDrewTexture = false;

	// Stop traversing when we hit a blobby shadow
	ShadowType_t shadowType = GetActualShadowCastType( pRenderable );
	if ( pRenderable && shadowType == SHADOWS_SIMPLE )
		return false;

	if ( !pRenderable || shadowType != SHADOWS_NONE )
	{
		bool bDrawModelShadow;
		bool bDrawBrushShadow;
		if ( !bChild )
		{
			bDrawModelShadow = ((shadow.m_Flags & SHADOW_FLAGS_BRUSH_MODEL) == 0);
			bDrawBrushShadow = !bDrawModelShadow;
		}
		else
		{
			int nModelType = modelinfo->GetModelType( pRenderable->GetModel() );
			bDrawModelShadow = nModelType == mod_studio;
			bDrawBrushShadow = nModelType == mod_brush;
		}

		if ( pRenderable && ( shadow.m_Flags & SHADOW_FLAGS_CUSTOM_DRAW ) )
		{
			RenderableInstance_t instance;
			instance.m_nAlpha = 255;
			pRenderable->DrawModel( STUDIO_SHADOWTEXTURE, instance );
			bDrewTexture = true;
		}
		else if ( bDrawModelShadow )
		{
			CMatRenderContextPtr pRenderContext( materials );
			CMatRenderDataReference lock( pRenderContext );
			DrawModelInfo_t info;
			matrix3x4a_t *pBoneToWorld = modelrender->DrawModelShadowSetup( pRenderable, pRenderable->GetBody(), pRenderable->GetSkin(), &info );
			if ( pBoneToWorld )
			{
				modelrender->DrawModelShadow( pRenderable, info, pBoneToWorld );
			}
			bDrewTexture = true;
		}
		else if ( bDrawBrushShadow )
		{
			render->DrawBrushModelShadow( pRenderable );
			bDrewTexture = true;
		}
	}

	if ( !pRenderable )
		return bDrewTexture;

	IClientRenderable *pChild;
	for ( pChild = pRenderable->FirstShadowChild(); pChild; pChild = pChild->NextShadowPeer() )
	{
		if ( DrawShadowHierarchy( pChild, shadow, true ) )
		{
			bDrewTexture = true;
		}
	}
	return bDrewTexture;
}

//-----------------------------------------------------------------------------
// This gets called with every shadow that potentially will need to re-render
//-----------------------------------------------------------------------------
bool CClientShadowMgr::BuildSetupListForRenderToTextureShadow( unsigned short clientShadowHandle, float flArea )
{
	ClientShadow_t& shadow = m_Shadows[clientShadowHandle];
	bool bDirtyTexture = (shadow.m_Flags & SHADOW_FLAGS_TEXTURE_DIRTY) != 0;
	bool bNeedsRedraw = m_ShadowAllocator.UseTexture( shadow.m_ShadowTexture, bDirtyTexture, flArea );
	if ( bNeedsRedraw || bDirtyTexture )
	{
		shadow.m_Flags |= SHADOW_FLAGS_TEXTURE_DIRTY;

		if ( !m_ShadowAllocator.HasValidTexture( shadow.m_ShadowTexture ) )
			return false;

		// shadow to be redrawn; for now, we'll always do it.
		IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );

		if ( BuildSetupShadowHierarchy( pRenderable, shadow ) )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// This gets called with every shadow that potentially will need to re-render
//-----------------------------------------------------------------------------
bool CClientShadowMgr::DrawRenderToTextureShadow( int nSlot, unsigned short clientShadowHandle, float flArea )
{
	ClientShadow_t& shadow = m_Shadows[clientShadowHandle];

	if ( shadow.m_bUseSplitScreenBits && 
		!shadow.m_SplitScreenBits.IsBitSet( nSlot ) )
	{
		return false;
	}

	// If we were previously using the LOD shadow, set the material
	bool bPreviouslyUsingLODShadow = ( shadow.m_Flags & SHADOW_FLAGS_USING_LOD_SHADOW ) != 0; 
	shadow.m_Flags &= ~SHADOW_FLAGS_USING_LOD_SHADOW;
	if ( bPreviouslyUsingLODShadow )
	{
		shadowmgr->SetShadowMaterial( shadow.m_ShadowHandle, m_RenderShadow, m_RenderModelShadow, (void*)clientShadowHandle );
	}

	// Mark texture as being used...
	bool bDirtyTexture = (shadow.m_Flags & SHADOW_FLAGS_TEXTURE_DIRTY) != 0;
	bool bDrewTexture = false;
	bool bNeedsRedraw = m_ShadowAllocator.UseTexture( shadow.m_ShadowTexture, bDirtyTexture, flArea );

	if ( !m_ShadowAllocator.HasValidTexture( shadow.m_ShadowTexture ) )
	{
		DrawRenderToTextureShadowLOD( nSlot, clientShadowHandle );
		return false;
	}

	if ( bNeedsRedraw || bDirtyTexture )
	{
		// shadow to be redrawn; for now, we'll always do it.
		IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );

		CMatRenderContextPtr pRenderContext( materials );
		
		// Sets the viewport state
		int x, y, w, h;
		m_ShadowAllocator.GetTextureRect( shadow.m_ShadowTexture, x, y, w, h );
		pRenderContext->Viewport( IsX360() ? 0 : x, IsX360() ? 0 : y, w, h ); 

		// Clear the selected viewport only (don't need to clear depth)
		pRenderContext->ClearBuffers( true, false );

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->LoadMatrix( shadow.m_WorldToTexture );

		if ( DrawShadowHierarchy( pRenderable, shadow ) )
		{
			bDrewTexture = true;
			if ( IsX360() )
			{
				// resolve render target to system memory texture
				Rect_t srcRect = { 0, 0, w, h };
				Rect_t dstRect = { x, y, w, h };
				pRenderContext->CopyRenderTargetToTextureEx( m_ShadowAllocator.GetTexture(), 0, &srcRect, &dstRect );
			}
		}
		else
		{
			// NOTE: Think the flags reset + texcoord set should only happen in DrawShadowHierarchy
			// but it's 2 days before 360 ship.. not going to change this now.
			DevMsg( "Didn't draw shadow hierarchy.. bad shadow texcoords probably going to happen..grab Brian!\n" );
		}

		// Only clear the dirty flag if the caster isn't animating
		if ( (shadow.m_Flags & SHADOW_FLAGS_ANIMATING_SOURCE) == 0 )
		{
			shadow.m_Flags &= ~SHADOW_FLAGS_TEXTURE_DIRTY;
		}

		SetRenderToTextureShadowTexCoords( shadow.m_ShadowHandle, x, y, w, h );
		SetRenderToTextureShadowTexCoords( shadow, x, y, w, h );
	}
	else if ( bPreviouslyUsingLODShadow )
	{
		// In this case, we were previously using the LOD shadow, but we didn't
		// have to reconstitute the texture. In this case, we need to reset the texcoord
		int x, y, w, h;
		m_ShadowAllocator.GetTextureRect( shadow.m_ShadowTexture, x, y, w, h );
		SetRenderToTextureShadowTexCoords( shadow.m_ShadowHandle, x, y, w, h );
		SetRenderToTextureShadowTexCoords( shadow, x, y, w, h );
	}

	return bDrewTexture;
}


//-----------------------------------------------------------------------------
// "Draws" the shadow LOD, which really means just set up the blobby shadow
//-----------------------------------------------------------------------------
void CClientShadowMgr::DrawRenderToTextureShadowLOD( int nSlot, unsigned short clientShadowHandle )
{
	if ( r_shadow_deferred.GetBool() )
	{
		return;
	}

	ClientShadow_t &shadow = m_Shadows[clientShadowHandle];
	if ( shadow.m_bUseSplitScreenBits && 
		!shadow.m_SplitScreenBits.IsBitSet( nSlot ) )
	{
		return;
	}

	if ( (shadow.m_Flags & SHADOW_FLAGS_USING_LOD_SHADOW) == 0 )
	{
		shadowmgr->SetShadowMaterial( shadow.m_ShadowHandle, m_SimpleShadow, m_SimpleShadow, (void*)CLIENTSHADOW_INVALID_HANDLE );
		shadowmgr->SetShadowTexCoord( shadow.m_ShadowHandle, 0, 0, 1, 1 );
		ClearExtraClipPlanes( shadow.m_ShadowHandle );
		shadow.m_Flags |= SHADOW_FLAGS_USING_LOD_SHADOW;
	}
}


//-----------------------------------------------------------------------------
// Advances to the next frame, 
//-----------------------------------------------------------------------------
void CClientShadowMgr::AdvanceFrame()
{
	// We're starting the next frame
	m_ShadowAllocator.AdvanceFrame();
}


//-----------------------------------------------------------------------------
// Re-render shadow depth textures that lie in the leaf list
//-----------------------------------------------------------------------------
int CClientShadowMgr::BuildActiveShadowDepthList( const CViewSetup &viewSetup, int nMaxDepthShadows, ClientShadowHandle_t *pActiveDepthShadows, int &nNumHighRes )
{
	float fDots[ 1024 ];

	nNumHighRes = 0;

	Frustum_t viewFrustum;
	GeneratePerspectiveFrustum( viewSetup.origin, viewSetup.angles, viewSetup.zNear, viewSetup.zFar, viewSetup.fov, viewSetup.m_flAspectRatio, viewFrustum );

	// Get a general look position for 
	Vector vViewForward;
	AngleVectors( viewSetup.angles, &vViewForward );

	int nActiveDepthShadowCount = 0;

	for ( ClientShadowHandle_t i = m_Shadows.Head(); i != m_Shadows.InvalidIndex(); i = m_Shadows.Next(i) )
	{
		if ( nActiveDepthShadowCount >= nMaxDepthShadows && nNumHighRes == nActiveDepthShadowCount )
		{
			// Easy out! There's nothing more we can do
			break;
		}

		ClientShadow_t& shadow = m_Shadows[i];

		// If this is not a flashlight which should use a shadow depth texture, skip!
		if ( ( shadow.m_Flags & SHADOW_FLAGS_USE_DEPTH_TEXTURE ) == 0 )
			continue;

		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		if ( ( shadow.m_nSplitscreenOwner >= 0 ) && ( shadow.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() ) )
			continue;

		const FlashlightState_t& flashlightState = shadowmgr->GetFlashlightState( shadow.m_ShadowHandle );

		// Bail if this flashlight doesn't want shadows
		if ( !flashlightState.m_bEnableShadows )
			continue;

		// Calculate an AABB around the shadow frustum
		Vector vecAbsMins, vecAbsMaxs;
		CalculateAABBFromProjectionMatrix( shadow.m_WorldToShadow, &vecAbsMins, &vecAbsMaxs );

		// FIXME: Could do other sorts of culling here, such as frustum-frustum test, distance etc.
		// If it's not in the view frustum, move on
		if ( !flashlightState.m_bOrtho && viewFrustum.CullBox( vecAbsMins, vecAbsMaxs ) )
		{
			shadowmgr->SetFlashlightDepthTexture( shadow.m_ShadowHandle, NULL, 0 );
			continue;
		}

		if ( nActiveDepthShadowCount >= nMaxDepthShadows )
		{
			if ( !flashlightState.m_bShadowHighRes )
			{
				// All active shadows are high res
				static bool s_bOverflowWarning = false;
				if ( !s_bOverflowWarning )
				{
					Warning( "Too many depth textures rendered in a single view!\n" );
					Assert( 0 );
					s_bOverflowWarning = true;
				}
				shadowmgr->SetFlashlightDepthTexture( shadow.m_ShadowHandle, NULL, 0 );
				continue;
			}
			else
			{
				// Lets take the place of other non high res active shadows
				for ( int j = nActiveDepthShadowCount - 1; j >= 0; --j )
				{
					// Find a low res one to replace
					ClientShadow_t& prevShadow = m_Shadows[ pActiveDepthShadows[ j ] ];
					const FlashlightState_t& prevFlashlightState = shadowmgr->GetFlashlightState( prevShadow.m_ShadowHandle );

					if ( !prevFlashlightState.m_bShadowHighRes )
					{
						pActiveDepthShadows[ j ] = i;
						++nNumHighRes;
						break;
					}
				}

				continue;
			}
		}

		if ( flashlightState.m_bShadowHighRes )
		{
			++nNumHighRes;
		}

		// Calculate the approximate distance to the nearest side
		Vector vLightDirection = flashlightState.m_vecLightOrigin - viewSetup.origin;
		VectorNormalize( vLightDirection );
		fDots[ nActiveDepthShadowCount ] = vLightDirection.Dot( vViewForward );

		pActiveDepthShadows[ nActiveDepthShadowCount++ ] = i;
	}

	// sort them
	for ( int i = 0; i < nActiveDepthShadowCount - 1; i++ )
	{
		for ( int j = 0; j < nActiveDepthShadowCount - i - 1; j++ )
		{
			if ( fDots[ j ] < fDots[ j + 1 ] )
			{
				ClientShadowHandle_t nTemp = pActiveDepthShadows[ j ];
				pActiveDepthShadows[ j ] = pActiveDepthShadows[ j + 1 ];
				pActiveDepthShadows[ j + 1 ] = nTemp;
			}
		}
	}

	return nActiveDepthShadowCount;
}


//-----------------------------------------------------------------------------
// Re-render shadow depth textures that lie in the leaf list
//-----------------------------------------------------------------------------
int CClientShadowMgr::BuildActiveFlashlightList( const CViewSetup &viewSetup, int nMaxFlashlights, ClientShadowHandle_t *pActiveFlashlights )
{
	int nActiveFlashlightCount = 0;
	for ( ClientShadowHandle_t i = m_Shadows.Head(); i != m_Shadows.InvalidIndex(); i = m_Shadows.Next(i) )
	{
		ClientShadow_t& shadow = m_Shadows[i];

		if ( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) == 0 )
			continue;
		
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		if ( ( shadow.m_nSplitscreenOwner >= 0 ) && ( shadow.m_nSplitscreenOwner != GET_ACTIVE_SPLITSCREEN_SLOT() ) )
			continue;

		// Calculate an AABB around the shadow frustum
		Vector vecAbsMins, vecAbsMaxs;
		CalculateAABBFromProjectionMatrix( shadow.m_WorldToShadow, &vecAbsMins, &vecAbsMaxs );

		Frustum_t viewFrustum;
		GeneratePerspectiveFrustum( viewSetup.origin, viewSetup.angles, viewSetup.zNear, viewSetup.zFar, viewSetup.fov, viewSetup.m_flAspectRatio, viewFrustum );

		// FIXME: Could do other sorts of culling here, such as frustum-frustum test, distance etc.
		// If it's not in the view frustum, move on
		if ( viewFrustum.CullBox( vecAbsMins, vecAbsMaxs ) )
		{
			continue;
		}

		if ( nActiveFlashlightCount >= nMaxFlashlights )
		{
			static bool s_bOverflowWarning = false;
			if ( !s_bOverflowWarning )
			{
				Warning( "Too many flashlights rendered in a single view!\n" );
				Assert( 0 );
				s_bOverflowWarning = true;
			}
			//shadowmgr->SetFlashlightDepthTexture( shadow.m_ShadowHandle, NULL, 0 );
			continue;
		}

		pActiveFlashlights[nActiveFlashlightCount++] = i;
	}
	return nActiveFlashlightCount;
}

//-----------------------------------------------------------------------------
// Sets the view's active flashlight render state
//-----------------------------------------------------------------------------
void CClientShadowMgr::SetViewFlashlightState( int nActiveFlashlightCount, ClientShadowHandle_t* pActiveFlashlights )
{
	// NOTE: On the 360, we render the entire scene with the flashlight state
	// set and don't render flashlights additively in the shadow mgr at a far later time
	// because the CPU costs are prohibitive

	shadowmgr->PushSinglePassFlashlightStateEnabled( IsX360() );

	if ( m_nMaxDepthTextureShadows > 1 )
	{
		AssertOnce( nActiveFlashlightCount <= m_nMaxDepthTextureShadows ); 
	}

	if ( nActiveFlashlightCount > 0 )
	{
		Assert( ( m_Shadows[ pActiveFlashlights[0] ].m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) != 0 );
		shadowmgr->SetSinglePassFlashlightRenderState( m_Shadows[ pActiveFlashlights[0] ].m_ShadowHandle );
	}
	else
	{
		shadowmgr->SetSinglePassFlashlightRenderState( SHADOW_HANDLE_INVALID );
	}	
}



//-----------------------------------------------------------------------------
// Kicks off rendering of volumetrics for the flashlights
//-----------------------------------------------------------------------------
void CClientShadowMgr::DrawVolumetrics( const CViewSetup &viewSetup )
{
	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "Draw Flashlight Volumetrics" );

	shadowmgr->DrawVolumetrics();
}


void AddPointToExtentsHelper( const VMatrix &flashlightToWorld, const Vector &vecPos, Vector &vecMin, Vector &vecMax )
{
	Vector worldSpacePos;

	Vector3DMultiplyPositionProjective( flashlightToWorld, vecPos, worldSpacePos );
	VectorMin( vecMin, worldSpacePos, vecMin );
	VectorMax( vecMax, worldSpacePos, vecMax );
}


void CClientShadowMgr::GetFrustumExtents( ClientShadowHandle_t handle, Vector &vecMin, Vector &vecMax )
{
	Assert( m_Shadows.IsValidIndex( handle ) );

	CClientShadowMgr::ClientShadow_t &shadow = m_Shadows[ handle ];

	VMatrix flashlightToWorld;
	MatrixInverseGeneral( shadow.m_WorldToShadow, flashlightToWorld );

	vecMin = Vector( FLT_MAX, FLT_MAX, FLT_MAX );
	vecMax = Vector( -FLT_MAX, -FLT_MAX, -FLT_MAX );

	AddPointToExtentsHelper( flashlightToWorld, Vector( 0.0f, 0.0f, 0.0f ), vecMin, vecMax );
	AddPointToExtentsHelper( flashlightToWorld, Vector( 0.0f, 0.0f, 1.0f ), vecMin, vecMax );
	AddPointToExtentsHelper( flashlightToWorld, Vector( 0.0f, 1.0f, 0.0f ), vecMin, vecMax );
	AddPointToExtentsHelper( flashlightToWorld, Vector( 1.0f, 0.0f, 0.0f ), vecMin, vecMax );
	AddPointToExtentsHelper( flashlightToWorld, Vector( 0.0f, 1.0f, 1.0f ), vecMin, vecMax );
	AddPointToExtentsHelper( flashlightToWorld, Vector( 1.0f, 0.0f, 1.0f ), vecMin, vecMax );
	AddPointToExtentsHelper( flashlightToWorld, Vector( 1.0f, 1.0f, 0.0f ), vecMin, vecMax );
	AddPointToExtentsHelper( flashlightToWorld, Vector( 1.0f, 1.0f, 1.0f ), vecMin, vecMax );
}

//-----------------------------------------------------------------------------
// Re-render shadow depth textures that lie in the leaf list
//-----------------------------------------------------------------------------
void CClientShadowMgr::ComputeShadowDepthTextures( const CViewSetup &viewSetup )
{
	if ( !r_flashlightdepthtexture.GetBool() )
	{
		// Build list of active flashlights
		ClientShadowHandle_t pActiveFlashlights[16];
		int nActiveFlashlights = BuildActiveFlashlightList( viewSetup, ARRAYSIZE( pActiveFlashlights ), pActiveFlashlights );
		SetViewFlashlightState( nActiveFlashlights, pActiveFlashlights );
		return;
	}

	VPROF_BUDGET( "CClientShadowMgr::ComputeShadowDepthTextures", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "Shadow Depth Textures" );

	// Build list of active shadow depth textures / flashlights
	int iNumHighRes = 0;
	ClientShadowHandle_t pActiveDepthShadows[1024];
	int nActiveDepthShadowCount = BuildActiveShadowDepthList( viewSetup, ARRAYSIZE( pActiveDepthShadows ), pActiveDepthShadows, iNumHighRes );

	int iLowResStart = ( iNumHighRes >= m_nMaxDepthTextureShadows ? 0 : iNumHighRes );

	// Iterate over all existing textures and allocate shadow textures
	bool bDebugFrustum = r_flashlightdrawfrustum.GetBool();
	bool bDebugFrustumBBox = r_flashlightdrawfrustumbbox.GetBool();
	for ( int j = 0; j < nActiveDepthShadowCount; ++j )
	{
		ClientShadow_t& shadow = m_Shadows[ pActiveDepthShadows[j] ];

		FlashlightState_t& flashlightState = const_cast<FlashlightState_t&>( shadowmgr->GetFlashlightState( shadow.m_ShadowHandle ) );

		CTextureReference shadowDepthTexture;
		bool bGotShadowDepthTexture = LockShadowDepthTexture( &shadowDepthTexture, flashlightState.m_bShadowHighRes ? 0 : iLowResStart );
		if ( !bGotShadowDepthTexture )
		{
			// If we don't get one, that means all the shadow maps allowed for this fram have already been used, no depth texture
			shadowmgr->SetFlashlightDepthTexture( shadow.m_ShadowHandle, NULL, 0 );
			continue;
		}

		CViewSetup shadowView;
		shadowView.m_flAspectRatio = 1.0f;
		shadowView.x = shadowView.y = 0;
		shadowView.width = shadowDepthTexture->GetActualWidth();
		shadowView.height = shadowDepthTexture->GetActualHeight();

		// Copy flashlight parameters
		if ( !flashlightState.m_bOrtho )
		{
			shadowView.m_bOrtho = false;
		}
		else
		{
			shadowView.m_bOrtho = true;
			shadowView.m_OrthoLeft = flashlightState.m_fOrthoLeft;
			shadowView.m_OrthoTop = flashlightState.m_fOrthoTop;
			shadowView.m_OrthoRight = flashlightState.m_fOrthoRight;
			shadowView.m_OrthoBottom = flashlightState.m_fOrthoBottom;
		}

		shadowView.m_bDoBloomAndToneMapping = false;
		shadowView.m_nMotionBlurMode = MOTION_BLUR_DISABLE;

		shadowView.fov = shadowView.fovViewmodel = flashlightState.m_fHorizontalFOVDegrees;
		shadowView.origin = flashlightState.m_vecLightOrigin;
		QuaternionAngles( flashlightState.m_quatOrientation, shadowView.angles ); // Convert from Quaternion to QAngle

		shadowView.zNear = shadowView.zNearViewmodel = flashlightState.m_NearZ;
		shadowView.zFar = shadowView.zFarViewmodel = flashlightState.m_FarZ;

		// Can turn on all light frustum overlays or per light with flashlightState parameter...
		if ( bDebugFrustum || flashlightState.m_bDrawShadowFrustum )
		{
			if ( flashlightState.m_bUberlight )
			{
				DrawUberlightRig( shadowView.origin, shadow.m_WorldToShadow, flashlightState );
			}

			DrawFrustum( shadowView.origin, shadow.m_WorldToShadow );
		}

		if ( bDebugFrustumBBox )
		{
			Vector vecExtentsMin, vecExtentsMax;
			GetFrustumExtents( pActiveDepthShadows[j], vecExtentsMin, vecExtentsMax );

			float flVisibleBBoxMinHeight = MIN( vecExtentsMax.z - 1.0f, C_EnvProjectedTexture::GetVisibleBBoxMinHeight() );
			vecExtentsMin.z = MAX( vecExtentsMin.z, flVisibleBBoxMinHeight );

			NDebugOverlay::Box( Vector( 0.0f, 0.0f, 0.0f ), vecExtentsMin, vecExtentsMax, 0, 0, 255, 100, 0.0f );
		}

		// Set depth bias factors specific to this flashlight
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->SetShadowDepthBiasFactors( flashlightState.m_flShadowSlopeScaleDepthBias, flashlightState.m_flShadowDepthBias );

		shadowView.m_bRenderFlashlightDepthTranslucents = flashlightState.m_bGlobalLight;

		// Render to the shadow depth texture with appropriate view
		view->UpdateShadowDepthTexture( m_DummyColorTexture, shadowDepthTexture, shadowView );

		// Associate the shadow depth texture and stencil bit with the flashlight for use during scene rendering
		shadowmgr->SetFlashlightDepthTexture( shadow.m_ShadowHandle, shadowDepthTexture, 0 );
	}

	SetViewFlashlightState( nActiveDepthShadowCount, pActiveDepthShadows );
}

	
//-----------------------------------------------------------------------------
// Re-renders all shadow textures for shadow casters that lie in the leaf list
//-----------------------------------------------------------------------------
static void SetupBonesOnBaseAnimating( C_BaseAnimating *&pBaseAnimating )
{
	pBaseAnimating->SetupBones( NULL, -1, -1, gpGlobals->curtime );
}


void CClientShadowMgr::ComputeShadowTextures( const CViewSetup &view, int leafCount, WorldListLeafData_t* pLeafList )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	VPROF_BUDGET( "CClientShadowMgr::ComputeShadowTextures", VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( !m_RenderToTextureActive || (r_shadows.GetInt() == 0) || r_shadows_gamecontrol.GetInt() == 0 )
		return;

	MDLCACHE_CRITICAL_SECTION();
	// First grab all shadow textures we may want to render
	int nCount = s_VisibleShadowList.FindShadows( &view, leafCount, pLeafList );
	if ( nCount == 0 )
		return;

	// FIXME: Add heuristics based on distance, etc. to futz with
	// the shadow allocator + to select blobby shadows

	CMatRenderContextPtr pRenderContext( materials );

	PIXEVENT( pRenderContext, "Render-To-Texture Shadows" );

	// Clear to white (color unused), black alpha
	pRenderContext->ClearColor4ub( 255, 255, 255, 0 );

	// No height clip mode please.
	MaterialHeightClipMode_t oldHeightClipMode = pRenderContext->GetHeightClipMode();
	pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );

	// No projection matrix (orthographic mode)
	// FIXME: Make it work for projective shadows?
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Scale( 1, -1, 1 );
	pRenderContext->Ortho( 0, 0, 1, 1, -9999, 0 );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();

	pRenderContext->PushRenderTargetAndViewport( m_ShadowAllocator.GetTexture() );

	if ( !IsX360() && m_bRenderTargetNeedsClear )
	{
		// don't need to clear absent depth buffer
		pRenderContext->ClearBuffers( true, false );
		m_bRenderTargetNeedsClear = false;
	}

	int nMaxShadows = r_shadowmaxrendered.GetInt();
	int nModelsRendered = 0;
	int i;

	for (i = 0; i < nCount; ++i)
	{
		const VisibleShadowInfo_t &info = s_VisibleShadowList.GetVisibleShadow(i);
		if ( nModelsRendered < nMaxShadows )
		{
			if ( DrawRenderToTextureShadow( nSlot, info.m_hShadow, info.m_flArea ) )
			{
				++nModelsRendered;
			}
		}
		else
		{
			DrawRenderToTextureShadowLOD( nSlot, info.m_hShadow );
		}
	}

	// Render to the backbuffer again
	pRenderContext->PopRenderTargetAndViewport();

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->SetHeightClipMode( oldHeightClipMode );

	pRenderContext->SetHeightClipMode( oldHeightClipMode );

	// Restore the clear color
	pRenderContext->ClearColor3ub( 0, 0, 0 );
}

//-------------------------------------------------------------------------------------------------------
// Lock down the usage of a shadow depth texture...must be unlocked for use on subsequent views / frames
//-------------------------------------------------------------------------------------------------------
bool CClientShadowMgr::LockShadowDepthTexture( CTextureReference *shadowDepthTexture, int nStartTexture )
{
	for ( int i = nStartTexture; i < m_DepthTextureCache.Count(); i++ )		// Search for cached shadow depth texture
	{
		if ( m_DepthTextureCacheLocks[i] == false && m_DepthTextureCache[i].IsValid() )				// If a free one is found
		{
			*shadowDepthTexture = m_DepthTextureCache[i];
			m_DepthTextureCacheLocks[i] = true;
			return true;
		}
	}

	return false;												// Didn't find it...
}

//------------------------------------------------------------------
// Unlock shadow depth texture for use on subsequent views / frames
//------------------------------------------------------------------
void CClientShadowMgr::UnlockAllShadowDepthTextures()
{
	for (int i=0; i< m_DepthTextureCache.Count(); i++ )
	{
		m_DepthTextureCacheLocks[i] = false;
	}

	shadowmgr->SetSinglePassFlashlightRenderState( SHADOW_HANDLE_INVALID );
	shadowmgr->PopSinglePassFlashlightStateEnabled();	
}

void CClientShadowMgr::SetFlashlightTarget( ClientShadowHandle_t shadowHandle, EHANDLE targetEntity )
{
	Assert( m_Shadows.IsValidIndex( shadowHandle ) );

	CClientShadowMgr::ClientShadow_t &shadow = m_Shadows[ shadowHandle ];
	if( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) == 0 )
		return;

//	shadow.m_pTargetRenderable = pRenderable;
	shadow.m_hTargetEntity = targetEntity;
}


void CClientShadowMgr::SetFlashlightLightWorld( ClientShadowHandle_t shadowHandle, bool bLightWorld )
{
	Assert( m_Shadows.IsValidIndex( shadowHandle ) );

	ClientShadow_t &shadow = m_Shadows[ shadowHandle ];
	if( ( shadow.m_Flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) ) == 0 )
		return;

	if ( bLightWorld )
	{
		shadow.m_Flags |= SHADOW_FLAGS_LIGHT_WORLD;
	}
	else
	{
		shadow.m_Flags &= ~SHADOW_FLAGS_LIGHT_WORLD;
	}
}


bool CClientShadowMgr::IsFlashlightTarget( ClientShadowHandle_t shadowHandle, IClientRenderable *pRenderable )
{
	ClientShadow_t &shadow = m_Shadows[ shadowHandle ];

	if( shadow.m_hTargetEntity->GetClientRenderable() == pRenderable )
		return true;

	C_BaseEntity *pChild = shadow.m_hTargetEntity->FirstMoveChild();
	while( pChild )
	{
		if( pChild->GetClientRenderable()==pRenderable )
			return true;

		pChild = pChild->NextMovePeer();
	}
							
	return false;
}

void CClientShadowMgr::SetShadowFromWorldLightsEnabled( bool bEnable )
{
	bool bIsShadowingFromWorldLights = IsShadowingFromWorldLights();
	m_bShadowFromWorldLights = bEnable;
	if ( bIsShadowingFromWorldLights != IsShadowingFromWorldLights() )
	{
		UpdateAllShadows();
	}
}

void CClientShadowMgr::SuppressShadowFromWorldLights( bool bSuppress )
{
	bool bIsShadowingFromWorldLights = IsShadowingFromWorldLights();
	m_bSuppressShadowFromWorldLights = bSuppress;
	if ( bIsShadowingFromWorldLights != IsShadowingFromWorldLights() )
	{
		UpdateAllShadows();
	}
}

///////////////////////////////////////////////////////////////////////
// Vertex and index data for cube with degenerate faces on each edge
///////////////////////////////////////////////////////////////////////

ALIGN16 static const float pShadowBoundVerts[] = 
{
	// +X face
	 1.0f, -1.0f, -1.0f, 1.0f, 
	 1.0f,  1.0f, -1.0f, 1.0f, 
	 1.0f,  1.0f,  1.0f, 1.0f, 
	 1.0f, -1.0f,  1.0f, 1.0f, 

	// +Y face
	-1.0f,  1.0f, -1.0f, 1.0f, 
	-1.0f,  1.0f,  1.0f, 1.0f, 
	 1.0f,  1.0f,  1.0f, 1.0f, 
	 1.0f,  1.0f, -1.0f, 1.0f, 

	// -X face
	-1.0f, -1.0f,  1.0f, 1.0f, 
	-1.0f,  1.0f,  1.0f, 1.0f, 
	-1.0f,  1.0f, -1.0f, 1.0f, 
	-1.0f, -1.0f, -1.0f, 1.0f, 

	// -Y face
	-1.0f, -1.0f, -1.0f, 1.0f, 
	 1.0f, -1.0f, -1.0f, 1.0f, 
	 1.0f, -1.0f,  1.0f, 1.0f, 
	-1.0f, -1.0f,  1.0f, 1.0f, 
	
	// +Z face
	-1.0f, -1.0f,  1.0f, 1.0f, 
	 1.0f, -1.0f,  1.0f, 1.0f, 
	 1.0f,  1.0f,  1.0f, 1.0f, 
	-1.0f,  1.0f,  1.0f, 1.0f, 

	// -Z face
	 1.0f, -1.0f, -1.0f, 1.0f, 
	-1.0f, -1.0f, -1.0f, 1.0f, 
	-1.0f,  1.0f, -1.0f, 1.0f, 
	 1.0f,  1.0f, -1.0f, 1.0f
};

ALIGN16 static const float pShadowBoundNormals[] = 
{
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	-1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, -1.0f, 0.0f, 0.0f,

	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, -1.0f, 0.0f,

};

ALIGN16 static const unsigned short pShadowBoundIndices[] = 
{
	// box faces
	0, 2, 1, 0, 3, 2,
	4, 6, 5, 4, 7, 6,
	8, 10, 9, 8, 11, 10,
	12, 14, 13, 12, 15, 14,
	16, 18, 17, 16, 19, 18,
	20, 22, 21, 20, 23, 22,

	// degenerate faces on edges
	2, 7, 1, 2, 6, 7,
	5, 10, 4, 5, 9, 10,
	8, 12, 11, 8, 15, 12,
	14, 0, 13, 14, 3, 0,

	17, 2, 3, 17, 18, 2,
	18, 5, 6, 18, 19, 5,
	8, 19, 16, 8, 9, 19,
	14, 16, 17, 14, 15, 16,

	0, 23, 20, 0, 1, 23,
	7, 22, 23, 7, 4, 22,
	11, 21, 22, 11, 22, 10,
	13, 21, 12, 13, 20, 21
};

// Adds a cube with degenerate edge quads to a mesh builder
void CClientShadowMgr::BuildCubeWithDegenerateEdgeQuads( CMeshBuilder& meshBuilder, const matrix3x4_t& objToWorld, const VMatrix& projToShadow, const CClientShadowMgr::ClientShadow_t& shadow )
{
	static bool bSIMDDataInitialized = false;
	static FourVectors srcPositions[6];
	static FourVectors srcNormals[2];

	if ( !bSIMDDataInitialized )
	{
		// convert vertex data into SIMD data
		const float* RESTRICT pPos = pShadowBoundVerts;
		const float* RESTRICT pNormal = pShadowBoundNormals;
		for( int v = 0; v < 6; ++v )
		{
			srcPositions[v].LoadAndSwizzleAligned( pPos, pPos+4, pPos+8, pPos+12);
			pPos += 16;
		}

		srcNormals[0].LoadAndSwizzleAligned( pNormal, pNormal+4, pNormal+8, pNormal+12);
		pNormal += 16;
		srcNormals[1].LoadAndSwizzleAligned( pNormal, pNormal+4, pNormal+4, pNormal+4);

		bSIMDDataInitialized = true;
	}

	Vector fallOffParams;
	ComputeFalloffInfo( shadow, &fallOffParams );

	int nBaseVertIdx = meshBuilder.GetCurrentVertex();

	float texCoord[4] = { shadow.m_TexCoordOffset.x, shadow.m_TexCoordOffset.y, shadow.m_TexCoordScale.x, shadow.m_TexCoordScale.y };
	Vector shadowDir = shadow.m_ShadowDir * shadow.m_MaxDist;

	if ( r_shadow_deferred_simd.GetBool() )
	{
		// FIXME: Something in here is buggy
		FourVectors positions[6];
		FourVectors normals[2];
		positions[0] = srcPositions[0];
		positions[1] = srcPositions[1];
		positions[2] = srcPositions[2];
		positions[3] = srcPositions[3];
		positions[4] = srcPositions[4];
		positions[5] = srcPositions[5];
		positions[0].TransformBy( objToWorld );
		positions[1].TransformBy( objToWorld );
		positions[2].TransformBy( objToWorld );
		positions[3].TransformBy( objToWorld );
		positions[4].TransformBy( objToWorld );
		positions[5].TransformBy( objToWorld );
		//FourVectors::TransformManyBy( srcPositions, 6, objToWorld, positions );	// this doesn't work but the above does. What gives???
		FourVectors::RotateManyBy( srcNormals, 2, objToWorld, normals );
		normals[0].VectorNormalizeFast();		// optional, will throw asserts in debug if we don't normalize
		normals[1].VectorNormalizeFast();

		for( int v = 0; v < ARRAYSIZE( pShadowBoundVerts ) / 4; ++v )
		{
			meshBuilder.Position3fv( positions[v/4].Vec( v%4 ).Base() );
			meshBuilder.Normal3fv( normals[v/16].Vec( (v/4)%4 ).Base() );
			meshBuilder.TexCoord3fv( 0, shadowDir.Base() );
			meshBuilder.TexCoord4fv( 1, texCoord );
			meshBuilder.TexCoord4fv( 2, projToShadow[0] );
			meshBuilder.TexCoord4fv( 3, projToShadow[1] );
			meshBuilder.TexCoord4fv( 4, projToShadow[2] );
			meshBuilder.TexCoord4fv( 5, projToShadow[3] );
			meshBuilder.TexCoord3fv( 6, fallOffParams.Base() );
			meshBuilder.AdvanceVertex();
		}
	}
	else
	{
		const float* pPos = pShadowBoundVerts;
		const float* pNormal = pShadowBoundNormals;
		for( int v = 0; v < ARRAYSIZE( pShadowBoundVerts ) / 4; ++v )
		{
			Vector pos;
			Vector normal;
			VectorTransform( pPos, objToWorld, (float*)&pos );
			VectorRotate( pNormal, objToWorld, (float*)&normal );

			meshBuilder.Position3fv( pos.Base() );
			meshBuilder.Normal3fv( normal.Base() );
			meshBuilder.TexCoord3fv( 0, shadowDir.Base() );
			meshBuilder.TexCoord4fv( 1, texCoord );
			meshBuilder.TexCoord4fv( 2, projToShadow[0] );
			meshBuilder.TexCoord4fv( 3, projToShadow[1] );
			meshBuilder.TexCoord4fv( 4, projToShadow[2] );
			meshBuilder.TexCoord4fv( 5, projToShadow[3] );
			meshBuilder.TexCoord3fv( 6, fallOffParams.Base() );
			meshBuilder.AdvanceVertex();

			pPos += 4;
			if ( v % 4 == 3)
				pNormal += 4;
		}
	}

	for( int i = 0; i < ARRAYSIZE( pShadowBoundIndices ) / 2; ++i )
	{
		// this causes alignment exception on 360?
		//meshBuilder.FastIndex2( nBaseVertIdx + pShadowBoundIndices[2*i], nBaseVertIdx + pShadowBoundIndices[2*i+1] );
		meshBuilder.FastIndex( nBaseVertIdx + pShadowBoundIndices[2*i] );
		meshBuilder.FastIndex( nBaseVertIdx + pShadowBoundIndices[2*i+1] );
	}
}

struct IntersectingShadowInfo_t
{
	ClientShadowHandle_t h;
	matrix3x4_t objToWorld;
};

// Sets up rendering info for deferred shadows
bool CClientShadowMgr::SetupDeferredShadow( const ClientShadow_t& shadow, const Vector& camPos, matrix3x4_t* pObjToWorldMat ) const
{
	IClientRenderable *pRenderable = ClientEntityList().GetClientRenderableFromHandle( shadow.m_Entity );

	Vector mins;
	Vector maxs;
	pRenderable->GetShadowRenderBounds( mins, maxs, GetActualShadowCastType( pRenderable ) );
	Vector center = mins + maxs;
	center *= 0.5f;
	Vector dims = maxs - mins;
	dims *= 0.5f;

	matrix3x4_t scaleAndOffset ( dims.x, 0.0f, 0.0f, center.x,
		0.0f, dims.y, 0.0f, center.y,
		0.0f, 0.0f, dims.z, center.z );

	matrix3x4_t objToWorld;
	AngleMatrix( pRenderable->GetRenderAngles(), pRenderable->GetRenderOrigin(), objToWorld );
	matrix3x4_t worldToObj;
	MatrixInvert( objToWorld, worldToObj );
	MatrixMultiply( objToWorld, scaleAndOffset, objToWorld );
	
	MatrixCopy( objToWorld, *pObjToWorldMat );

	// test if camera is inside shadow volume
	Vector shadowDirObjSpace;
	Vector camPosObjSpace;
	VectorRotate( shadow.m_ShadowDir, worldToObj, shadowDirObjSpace );
	VectorTransform( camPos, worldToObj, camPosObjSpace );
	BoxTraceInfo_t ti;

	if ( IntersectRayWithBox( camPosObjSpace, -shadow.m_MaxDist*shadowDirObjSpace, mins, maxs, 0.0f, &ti ) )
	{
		return true;
	}

	return false;
}

void CClientShadowMgr::DownsampleDepthBuffer( IMatRenderContext* pRenderContext, const VMatrix& invViewProjMat )
{
	int nScreenWidth, nScreenHeight, nDummy;
	pRenderContext->GetViewport( nDummy, nDummy, nScreenWidth, nScreenHeight );

	int nWidth = m_downSampledNormals->GetActualWidth();
	int nHeight = m_downSampledNormals->GetActualHeight();
	pRenderContext->PushRenderTargetAndViewport( m_downSampledNormals, 0, 0, nWidth, nHeight );
	
	IMaterial* pMat = materials->FindMaterial( "dev/downsampledepth", TEXTURE_GROUP_OTHER );
	
	// yes, this is stupid
	IMaterialVar* pVar = pMat->FindVar( "$c0_x", NULL );
	pVar->SetFloatValue( invViewProjMat[0][0] );
	pVar = pMat->FindVar( "$c0_y", NULL );
	pVar->SetFloatValue( invViewProjMat[0][1] );
	pVar = pMat->FindVar( "$c0_z", NULL );
	pVar->SetFloatValue( invViewProjMat[0][2] );
	pVar = pMat->FindVar( "$c0_w", NULL );
	pVar->SetFloatValue( invViewProjMat[0][3] );
	pVar = pMat->FindVar( "$c1_x", NULL );
	pVar->SetFloatValue( invViewProjMat[1][0] );
	pVar = pMat->FindVar( "$c1_y", NULL );
	pVar->SetFloatValue( invViewProjMat[1][1] );
	pVar = pMat->FindVar( "$c1_z", NULL );
	pVar->SetFloatValue( invViewProjMat[1][2] );
	pVar = pMat->FindVar( "$c1_w", NULL );
	pVar->SetFloatValue( invViewProjMat[1][3] );
	pVar = pMat->FindVar( "$c2_x", NULL );
	pVar->SetFloatValue( invViewProjMat[2][0] );
	pVar = pMat->FindVar( "$c2_y", NULL );
	pVar->SetFloatValue( invViewProjMat[2][1] );
	pVar = pMat->FindVar( "$c2_z", NULL );
	pVar->SetFloatValue( invViewProjMat[2][2] );
	pVar = pMat->FindVar( "$c2_w", NULL );
	pVar->SetFloatValue( invViewProjMat[2][3] );
	pVar = pMat->FindVar( "$c3_x", NULL );
	pVar->SetFloatValue( invViewProjMat[3][0] );
	pVar = pMat->FindVar( "$c3_y", NULL );
	pVar->SetFloatValue( invViewProjMat[3][1] );
	pVar = pMat->FindVar( "$c3_z", NULL );
	pVar->SetFloatValue( invViewProjMat[3][2] );
	pVar = pMat->FindVar( "$c3_w", NULL );
	pVar->SetFloatValue( invViewProjMat[3][3] );

	pVar = pMat->FindVar( "$c4_x", NULL );
	pVar->SetFloatValue( 1.0f / float( nScreenWidth ) );
	pVar = pMat->FindVar( "$c4_y", NULL );
	pVar->SetFloatValue( 1.0f / float( nScreenHeight ) );

	pRenderContext->DrawScreenSpaceRectangle( pMat, 0, 0, nWidth, nHeight,
		0, 0, 2*nWidth-2, 2*nHeight-2,
		2*nWidth, 2*nHeight );

	if ( IsX360() )
	{
		pRenderContext->CopyRenderTargetToTextureEx( m_downSampledNormals, 0, NULL, NULL );
		pRenderContext->CopyRenderTargetToTextureEx( m_downSampledDepth, -1, NULL, NULL );
	}

	pRenderContext->PopRenderTargetAndViewport();
}

void CClientShadowMgr::CompositeDeferredShadows( IMatRenderContext* pRenderContext )
{
	int nWidth, nHeight, nDummy;
	pRenderContext->GetViewport( nDummy, nDummy, nWidth, nHeight );
	IMaterial* pMat = materials->FindMaterial( "dev/compositedeferredshadow", TEXTURE_GROUP_OTHER );

	/*
	IMaterialVar* pMatVar = pMat->FindVar( "$basetexture", NULL, false );
	if ( pMatVar )
	{
		ITexture *pFullScreen = materials->FindTexture( "_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );
		pMatVar->SetTextureValue( pFullScreen );
	}
	*/

	pRenderContext->DrawScreenSpaceRectangle( pMat, 0, 0, nWidth, nHeight,
		0, 0, 2*nWidth-2, 2*nHeight-2,
		2*nWidth, 2*nHeight );
}

void CClientShadowMgr::ComputeFalloffInfo( const ClientShadow_t& shadow, Vector* pShadowFalloffParams )
{
	float flFalloffOffset = 0.7f * shadow.m_FalloffStart;	// pull the offset in a little to hide the shadow darkness discontinuity

	float flFalloffDist = shadow.m_MaxDist - flFalloffOffset;
	float flOOZFalloffDist = ( flFalloffDist > 0.0f ) ? 1.0f / flFalloffDist : 1.0f;

	// for use in the shader
	pShadowFalloffParams->x = -flFalloffOffset * flOOZFalloffDist;
	pShadowFalloffParams->y = flOOZFalloffDist;
	pShadowFalloffParams->z = 1.0f/255.0f * shadow.m_FalloffBias;
}

void CClientShadowMgr::DrawDeferredShadows( const CViewSetup &view, int leafCount, WorldListLeafData_t* pLeafList )
{
	VPROF_BUDGET( __FUNCTION__, VPROF_BUDGETGROUP_SHADOW_RENDERING );

	if ( !IsX360() )
	{
		return;
	}

	// We assume the visible shadow list was updated in ComputeShadowTextures. This is only correct if we're not rendering from multiple view points.

	int nNumShadows = s_VisibleShadowList.GetVisibleShadowCount();
	if ( nNumShadows == 0 )
	{
		return;
	}

	IntersectingShadowInfo_t* pInsideVolume = (IntersectingShadowInfo_t*)stackalloc( nNumShadows * sizeof(IntersectingShadowInfo_t) );
	int nNumInsideVolumes = 0;

	CMatRenderContextPtr pRenderContext( materials );

	PIXEVENT( pRenderContext, "DEFERRED_SHADOWS" );

	matrix3x4_t viewMatrix;
	pRenderContext->GetMatrix( MATERIAL_VIEW, &viewMatrix );

	VMatrix projMatrix;
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &projMatrix );

	VMatrix viewProjMatrix;
	VMatrix invViewProjMatrix;
	MatrixMultiply( projMatrix, VMatrix(viewMatrix), viewProjMatrix );
	MatrixInverseGeneral( viewProjMatrix, invViewProjMatrix );

	ShaderStencilState_t state;
	state.m_bEnable = true;
	state.m_nWriteMask = 0xFF;
	state.m_nTestMask = 0x1 << 2;
	state.m_nReferenceValue = 0x0;
	state.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
	state.m_PassOp = SHADER_STENCILOP_KEEP;
	state.m_FailOp = SHADER_STENCILOP_KEEP;
	state.m_ZFailOp = SHADER_STENCILOP_KEEP;
#if defined( _X360 )
	state.m_bHiStencilEnable = true;
	state.m_bHiStencilWriteEnable = false;
	state.m_HiStencilCompareFunc = SHADER_HI_STENCILFUNC_EQUAL;
	state.m_nHiStencilReferenceValue = 0;
#endif

#ifdef _X360
	pRenderContext->PushVertexShaderGPRAllocation( 16 );
#endif

	if ( r_shadow_deferred_downsample.GetBool() )
	{
		DownsampleDepthBuffer( pRenderContext, invViewProjMatrix );

		int nWidth = m_downSampledNormals->GetActualWidth();
		int nHeight = m_downSampledNormals->GetActualHeight();
		pRenderContext->PushRenderTargetAndViewport( m_downSampledNormals, 0, 0, nWidth, nHeight );
	}
	else
	{
		pRenderContext->SetStencilState( state );
#if defined( _X360 )
		pRenderContext->FlushHiStencil();
#endif
	}

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	
	IMaterialVar* pTextureVar = m_RenderDeferredShadowMat->FindVar( "$basetexture", NULL, false );
	if( pTextureVar )
	{
		pTextureVar->SetTextureValue( s_ClientShadowMgr.GetShadowTexture( CLIENTSHADOW_INVALID_HANDLE ) );
	}
	pTextureVar = m_RenderDeferredShadowMat->FindVar( "$depthtexture", NULL, false );
	if( pTextureVar )
	{
		//pTextureVar->SetTextureValue( s_ClientShadowMgr.GetShadowTexture( CLIENTSHADOW_INVALID_HANDLE ) );
		pTextureVar->SetTextureValue( GetFullFrameDepthTexture() );
	}

	IMaterialVar* pZFailVar = m_RenderDeferredShadowMat->FindVar( "$zfailenable", NULL, false );

	if( pZFailVar )
	{
		pZFailVar->SetIntValue( 0 );
	}
	pRenderContext->Bind( m_RenderDeferredShadowMat );

	Vector camPos;
	pRenderContext->GetWorldSpaceCameraPosition( &camPos );

	IMesh* pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;

	int nMaxVerts, nMaxIndices;
	pRenderContext->GetMaxToRender( pMesh, false, &nMaxVerts, &nMaxIndices );
	int nMaxShadowsPerBatch = MIN( nMaxVerts / ( ARRAYSIZE( pShadowBoundVerts ) / 4 ), nMaxIndices / ARRAYSIZE( pShadowBoundIndices ) );

	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundVerts ) / 4, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundIndices ) );
	int nNumShadowsBatched = 0;
	
	// Z-Pass shadow volumes
	for ( int i = 0; i < nNumShadows; ++i )
	{
		const VisibleShadowInfo_t& vsi = s_VisibleShadowList.GetVisibleShadow( i );
		ClientShadow_t& shadow = m_Shadows[vsi.m_hShadow];

		matrix3x4_t objToWorld;
		if ( SetupDeferredShadow( shadow, camPos, &objToWorld ) )
		{
			pInsideVolume[nNumInsideVolumes].h = vsi.m_hShadow;
			MatrixCopy( objToWorld, pInsideVolume[nNumInsideVolumes].objToWorld );
			nNumInsideVolumes++;
			continue;
		}

		VMatrix projToTextureMatrix;
		MatrixMultiply( shadow.m_WorldToTexture, invViewProjMatrix, projToTextureMatrix );

		// create extruded bounding geometry
		BuildCubeWithDegenerateEdgeQuads( meshBuilder, objToWorld, projToTextureMatrix, shadow );
		nNumShadowsBatched++;
		if ( nNumShadowsBatched == nMaxShadowsPerBatch )
		{
			// flush
			meshBuilder.End();
			pMesh->Draw();
			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundVerts ) / 4, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundIndices ) );
			nNumShadowsBatched = 0;
		}
	}

	// render
	meshBuilder.End();
	if ( nNumShadowsBatched > 0 )
	{
		pMesh->Draw();
		nNumShadowsBatched = 0;
	}
	else
	{
		pMesh->MarkAsDrawn();
	}

	// draw deferred blobby shadow volumes
	if ( s_VisibleShadowList.GetVisibleBlobbyShadowCount() > 0 )
	{
		pRenderContext->Bind( m_RenderDeferredSimpleShadowMat );
		pTextureVar = m_RenderDeferredSimpleShadowMat->FindVar( "$depthtexture", NULL, false );
		if( pTextureVar )
		{
			pTextureVar->SetTextureValue( GetFullFrameDepthTexture() );
		}

		int nNumShadows = s_VisibleShadowList.GetVisibleBlobbyShadowCount();
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundVerts ) / 4, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundIndices ) );

		for ( int i = 0; i < nNumShadows; ++i )
		{
			const VisibleShadowInfo_t& vsi = s_VisibleShadowList.GetVisibleBlobbyShadow( i );
			ClientShadow_t& shadow = m_Shadows[vsi.m_hShadow];

			matrix3x4_t objToWorld;
			if ( SetupDeferredShadow( shadow, camPos, &objToWorld ) )
			{
				//DevWarning( "Blobby shadow needs z-fail rendering. Skipped.\n" );
				continue;
			}

			VMatrix projToTextureMatrix;
			MatrixMultiply( shadow.m_WorldToTexture, invViewProjMatrix, projToTextureMatrix );

			// create extruded bounding geometry
			BuildCubeWithDegenerateEdgeQuads( meshBuilder, objToWorld, projToTextureMatrix, shadow );
			nNumShadowsBatched++;
			if ( nNumShadowsBatched == nMaxShadowsPerBatch )
			{
				// flush
				meshBuilder.End();
				pMesh->Draw();
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundVerts ) / 4, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundIndices ) );
				nNumShadowsBatched = 0;
			}
		}

		// render
		meshBuilder.End();
		if ( nNumShadowsBatched > 0 )
		{
			pMesh->Draw();
			nNumShadowsBatched = 0;
		}
		else
		{
			pMesh->MarkAsDrawn();
		}
	}

	// draw zfail volumes
	if ( nNumInsideVolumes > 0 )
	{
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );
		if( pZFailVar )
		{
			pZFailVar->SetIntValue( 1 );
		}
		pRenderContext->Bind( m_RenderDeferredShadowMat );

		//IMesh* pMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundVerts ) / 4, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundIndices ) );
		for ( int i = 0; i < nNumInsideVolumes; ++i )
		{
			ClientShadow_t& shadow = m_Shadows[pInsideVolume[i].h];

			VMatrix projToTextureMatrix;
			MatrixMultiply( shadow.m_WorldToTexture, invViewProjMatrix, projToTextureMatrix );

			// create extruded bounding geometry
			BuildCubeWithDegenerateEdgeQuads( meshBuilder, pInsideVolume[i].objToWorld, projToTextureMatrix, shadow );
			nNumShadowsBatched++;
			if ( nNumShadowsBatched == nMaxShadowsPerBatch )
			{
				// flush
				meshBuilder.End();
				pMesh->Draw();
				//pMesh = pRenderContext->GetDynamicMesh();
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundVerts ) / 4, nMaxShadowsPerBatch * ARRAYSIZE( pShadowBoundIndices ) );
				nNumShadowsBatched = 0;
			}
		}
		meshBuilder.End();
		if ( nNumShadowsBatched > 0 )
		{
			pMesh->Draw();
			nNumShadowsBatched = 0;
		}
		else
		{
			pMesh->MarkAsDrawn();
		}
	}

	pRenderContext->PopMatrix();
	// reset culling
	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	if ( r_shadow_deferred_downsample.GetBool() )
	{
		pRenderContext->CopyRenderTargetToTextureEx( m_downSampledNormals, 0, NULL, NULL );
		pRenderContext->PopRenderTargetAndViewport();

		pRenderContext->SetStencilState( state );

		CompositeDeferredShadows( pRenderContext );
	}

	ShaderStencilState_t stateDisable;
	stateDisable.m_bEnable = false;
#if defined( _X360 )
	stateDisable.m_bHiStencilEnable = false;
	stateDisable.m_bHiStencilWriteEnable = false;
#endif
	pRenderContext->SetStencilState( stateDisable );

#ifdef _X360
	pRenderContext->PopVertexShaderGPRAllocation();
#endif

	// NOTE: We could use geometry instancing for drawing the extruded bounding boxes
}

void DeferredShadowToggleCallback( IConVar*, const char *, float )
{
	if ( !IsX360() )
	{
		DevMsg( "Deferred shadow rendering only supported on the 360.\n" );
		return;
	}

	s_ClientShadowMgr.UpdateAllShadows();
	s_ClientShadowMgr.RemoveAllShadowDecals();
}

void DeferredShadowDownsampleToggleCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	s_ClientShadowMgr.ShutdownDeferredShadows();
	s_ClientShadowMgr.InitDeferredShadows();
}


void CClientShadowMgr::UpdateSplitscreenLocalPlayerShadowSkip()
{
	// Don't draw the current splitscreen player's own shadow
	if ( engine->IsSplitScreenActive() )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();

		C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
		C_BasePlayer* pFirstPersonEnt = pPlayer;
		if ( pPlayer )
		{
			if ( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
			{
				pFirstPersonEnt = ToBasePlayer( pPlayer->GetObserverTarget() );
			}
			else if ( pPlayer->GetObserverMode() != OBS_MODE_NONE )
			{
				pFirstPersonEnt = NULL;
			}
		}

		// pFirstPersonEnt NULL only if the player is spectating, but not first-person-spectating
		if ( pFirstPersonEnt )
		{
			shadowmgr->SkipShadowForEntity( pFirstPersonEnt->entindex() );
		}
		else
		{
			shadowmgr->SkipShadowForEntity( INT_MIN );	// pick an entindex that is guaranteed not to be used by anything
		}
	}
	else
	{
		shadowmgr->SkipShadowForEntity( INT_MIN );	// pick an entindex that is guaranteed not to be used by anything
	}
}

//-----------------------------------------------------------------------------
// A material proxy that resets the base texture to use the rendered shadow
//-----------------------------------------------------------------------------
class CShadowProxy : public IMaterialProxy
{
public:
	CShadowProxy();
	virtual ~CShadowProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pProxyData );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar* m_BaseTextureVar;
	IMaterialVar* m_MaxFalloffAmountVar;
};

CShadowProxy::CShadowProxy()
: m_BaseTextureVar( NULL ),
  m_MaxFalloffAmountVar( NULL )
{
}

CShadowProxy::~CShadowProxy()
{
}


bool CShadowProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	if ( !foundVar )
		return false;
	m_MaxFalloffAmountVar = pMaterial->FindVar( "$maxfalloffamount", &foundVar, false );
	return foundVar;
}

void CShadowProxy::OnBind( void *pProxyData )
{
	unsigned short clientShadowHandle = ( unsigned short )(int)pProxyData&0xffff;
	ITexture* pTex = s_ClientShadowMgr.GetShadowTexture( clientShadowHandle );
	m_BaseTextureVar->SetTextureValue( pTex );
	m_MaxFalloffAmountVar->SetFloatValue( MAX_FALLOFF_AMOUNT );
}

IMaterial *CShadowProxy::GetMaterial()
{
	return m_BaseTextureVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CShadowProxy, Shadow );



//-----------------------------------------------------------------------------
// A material proxy that resets the base texture to use the rendered shadow
//-----------------------------------------------------------------------------
class CShadowModelProxy : public IMaterialProxy
{
public:
	CShadowModelProxy();
	virtual ~CShadowModelProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pProxyData );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar* m_BaseTextureVar;
	IMaterialVar* m_BaseTextureOffsetVar;
	IMaterialVar* m_BaseTextureScaleVar;
	IMaterialVar* m_BaseTextureMatrixVar;
	IMaterialVar* m_FalloffOffsetVar;
	IMaterialVar* m_FalloffDistanceVar;
	IMaterialVar* m_FalloffAmountVar;
};

CShadowModelProxy::CShadowModelProxy()
{
	m_BaseTextureVar = NULL;
	m_BaseTextureOffsetVar = NULL;
	m_BaseTextureScaleVar = NULL;
	m_BaseTextureMatrixVar = NULL;
	m_FalloffOffsetVar = NULL;
	m_FalloffDistanceVar = NULL;
	m_FalloffAmountVar = NULL;
}

CShadowModelProxy::~CShadowModelProxy()
{
}


bool CShadowModelProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	if (!foundVar)
		return false;
	m_BaseTextureOffsetVar = pMaterial->FindVar( "$basetextureoffset", &foundVar, false );
	if (!foundVar)
		return false;
	m_BaseTextureScaleVar = pMaterial->FindVar( "$basetexturescale", &foundVar, false );
	if (!foundVar)
		return false;
	m_BaseTextureMatrixVar = pMaterial->FindVar( "$basetexturetransform", &foundVar, false );
	if (!foundVar)
		return false;
	m_FalloffOffsetVar = pMaterial->FindVar( "$falloffoffset", &foundVar, false );
	if (!foundVar)
		return false;
	m_FalloffDistanceVar = pMaterial->FindVar( "$falloffdistance", &foundVar, false );
	if (!foundVar)
		return false;
	m_FalloffAmountVar = pMaterial->FindVar( "$falloffamount", &foundVar, false );
	return foundVar;
}

void CShadowModelProxy::OnBind( void *pProxyData )
{
	unsigned short clientShadowHandle = ( unsigned short )((int)pProxyData&0xffff);
	ITexture* pTex = s_ClientShadowMgr.GetShadowTexture( clientShadowHandle );
	m_BaseTextureVar->SetTextureValue( pTex );

	const ShadowInfo_t& info = s_ClientShadowMgr.GetShadowInfo( clientShadowHandle );
	m_BaseTextureMatrixVar->SetMatrixValue( info.m_WorldToShadow );
	m_BaseTextureOffsetVar->SetVecValue( info.m_TexOrigin.Base(), 2 );
	m_BaseTextureScaleVar->SetVecValue( info.m_TexSize.Base(), 2 );
	m_FalloffOffsetVar->SetFloatValue( info.m_FalloffOffset );
	m_FalloffDistanceVar->SetFloatValue( info.m_MaxDist );
	m_FalloffAmountVar->SetFloatValue( info.m_FalloffAmount );
}

IMaterial *CShadowModelProxy::GetMaterial()
{
	return m_BaseTextureVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CShadowModelProxy, ShadowModel );
