//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"

#include "KeyValues.h"
#include "cdll_client_int.h"
#include "view_scene.h"
#include "viewrender.h"
#include "tier0/icommandline.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialvar.h"

#include "ScreenSpaceEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static float g_flDX7NoiseScale = 4.0f; 

//------------------------------------------------------------------------------
// Film grain post-processing effect
//------------------------------------------------------------------------------
struct FilmDustParticle_t
{
	int	  m_nChannel;

	float m_flPositionX;
	float m_flPositionY;

	float m_flWidth;
	float m_flHeight;

	int	  m_nSplotchIndex;
	int	  m_nOrientation;
};

class CFilmGrainEffect : public IScreenSpaceEffect
{
public:													

	CFilmGrainEffect( );
   ~CFilmGrainEffect( );

	void Init( );
	void Shutdown( );

	void SetParameters( KeyValues *params );

	void Render( int x, int y, int w, int h );

	void Enable( bool bEnable );
	bool IsEnabled( );

private:

	void DrawSplotch( float x, float y, float width, float height, float u, float v, float uWidth, float vHeight, int orientation, int alpha=255 );

	bool				m_bEnable;

	CMaterialReference	m_GrainMaterial;
	CMaterialReference	m_DustMaterial;

	Vector4D			m_NoiseScale;

	int					m_nMaxDustParticles;

	float				m_flMinDustSize;
	float				m_flMaxDustSize;

	float				m_flChanceOfDust;

	float				m_flUpdateRate;

	bool				m_bEnableFlicker;
	int					m_nFlickerAlpha;

	bool				m_bSplitScreen;

	int					m_nCachedParticleTime;
	CUtlVector< FilmDustParticle_t > m_CachedParticles;

	float				m_flEffectAlpha;

	IMaterial *			m_pFlickerMaterial; 
};

ADD_SCREENSPACE_EFFECT( CFilmGrainEffect, filmgrain );

//------------------------------------------------------------------------------
// CFilmGrainEffect constructor
//------------------------------------------------------------------------------
CFilmGrainEffect::CFilmGrainEffect( )
{
	m_NoiseScale = Vector4D( 0.14f, 0.14f, 0.14f, 0.78f );

	m_nMaxDustParticles = 3;
	
	m_flMinDustSize = 0.03f;
	m_flMaxDustSize = 0.15f;

	m_flChanceOfDust = 0.75f;

	m_flUpdateRate = 24.0f;
	
	m_nCachedParticleTime = -1;

	m_bSplitScreen = false;

	m_bEnableFlicker = true;
	m_nFlickerAlpha = 90;

	m_flEffectAlpha = 1.0;

	m_pFlickerMaterial = NULL;
}


//------------------------------------------------------------------------------
// CFilmGrainEffect destructor
//------------------------------------------------------------------------------
CFilmGrainEffect::~CFilmGrainEffect( )
{
}


//------------------------------------------------------------------------------
// CFilmGrainEffect init
//------------------------------------------------------------------------------
void CFilmGrainEffect::Init( )
{
	KeyValues *pVMTKeyValues = new KeyValues( "filmgrain" );
	pVMTKeyValues->SetString( "$GRAIN_TEXTURE", "Effects/FilmScan256" );
	pVMTKeyValues->SetString( "$SCALEBIAS", "[0.0 0.0 0.0 0.0]" );
	pVMTKeyValues->SetString( "$NOISESCALE", "[0.0 1.0 0.5 1.0]" );

	m_GrainMaterial.Init( "engine/filmgrain", pVMTKeyValues );
	m_GrainMaterial->Refresh( );

	pVMTKeyValues = new KeyValues( "filmdust" );
	pVMTKeyValues->SetString( "$DUST_TEXTURE", "Effects/Splotches256" );
	pVMTKeyValues->SetString( "$SCALEBIAS", "[0.0 0.0 0.0 0.0]" );
	pVMTKeyValues->SetString( "$CHANNEL_SELECT", "[1.0 0.0 0.0 0.0]" );

	m_DustMaterial.Init( "engine/filmdust", pVMTKeyValues );
	m_DustMaterial->Refresh( );

	m_pFlickerMaterial = materials->FindMaterial( "effects/flicker_128", TEXTURE_GROUP_OTHER, false );
	if ( m_pFlickerMaterial )
	{
		m_pFlickerMaterial->AddRef();
	}
}


//------------------------------------------------------------------------------
// CFilmGrainEffect shutdown
//------------------------------------------------------------------------------
void CFilmGrainEffect::Shutdown( )
{
	m_DustMaterial.Shutdown();
	m_GrainMaterial.Shutdown();
	
	if ( m_pFlickerMaterial )
	{
		m_pFlickerMaterial->Release();
	}
}

 
//------------------------------------------------------------------------------
// CFilmGrainEffect enable
//------------------------------------------------------------------------------
void CFilmGrainEffect::Enable( bool bEnable )
{
	m_bEnable = bEnable;
}

bool CFilmGrainEffect::IsEnabled( )
{
	return m_bEnable;
}

//------------------------------------------------------------------------------
// CFilmGrainEffect SetParameters
//------------------------------------------------------------------------------
void CFilmGrainEffect::SetParameters( KeyValues *params )
{
	if( params->FindKey( "split_screen" ) )
	{
		int ival = params->GetInt( "split_screen" );
		m_bSplitScreen = ival?true:false;

		extern void EnableHUDFilmDemo( bool bEnable, const char *left_string_id, const char *right_string_id );
		EnableHUDFilmDemo( m_bSplitScreen, "#Valve_FilmDemo_FilmGrain_LeftTitle", "#Valve_FilmDemo_FilmGrain_RightTitle" );
	}

	if( params->FindKey( "noise_scale" ) )
	{
		Color noise_color = params->GetColor( "noise_scale" );
		int r, g, b, a;
		noise_color.GetColor( r, g, b, a );
		m_NoiseScale.x = r/255.0f;;
		m_NoiseScale.y = g/255.0f;;
		m_NoiseScale.z = b/255.0f;;
		m_NoiseScale.w = a/255.0f;;
	}

	if( params->FindKey( "max_dust_particles" ) )
	{
		m_nMaxDustParticles = params->GetInt( "max_dust_particles" );
	}

	if( params->FindKey( "min_dust_size" ) )
	{
		m_flMinDustSize = params->GetFloat( "min_dust_size" );
	}

	if( params->FindKey( "max_dust_size" ) )
	{
		m_flMaxDustSize = params->GetFloat( "max_dust_size" );
	}

	if( params->FindKey( "dust_prob" ) )
	{
		m_flChanceOfDust = params->GetFloat( "dust_prob" );
	}

	if( params->FindKey( "update_rate" ) )
	{
		m_flUpdateRate = params->GetFloat( "update_rate" );
	}

	if( params->FindKey( "enable_flicker" ) )
	{
		m_bEnableFlicker = params->GetInt( "enable_flicker" );
	}

	if( params->FindKey( "flicker_alpha" ) )
	{
		m_nFlickerAlpha = params->GetInt( "flicker_alpha" );
	}

	if( params->FindKey( "effect_alpha" ) )
	{
		m_flEffectAlpha = params->GetFloat( "effect_alpha" );
	}
}


//-----------------------------------------------------------------------------
// Draws a quad to resolve accumulation buffer samples as needed
//-----------------------------------------------------------------------------
void CFilmGrainEffect::DrawSplotch( float x, float y, float flWidth, float flHeight, float u, float v, float uWidth, float vWidth, int orientation, int alpha )
{
	float flAlpha = ( alpha / 255.0f ) * m_flEffectAlpha;

	float tempU[4] = { u, u, u+uWidth, u+uWidth };
	float tempV[4] = { v, v+vWidth, v+vWidth, v };

	float _u[4], _v[4];
	for( int i=0;i<4;i++ )
	{
		_u[ (i + orientation)%4 ] = tempU[ i ];
		_v[ (i + orientation)%4 ] = tempV[ i ];
	}

	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( x, y, 0.0f );	// Upper left
	meshBuilder.TexCoord2f( 0, _u[0], _v[0] );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x, y+flHeight, 0.0f );	// Lower left
	meshBuilder.TexCoord2f( 0, _u[1], _v[1] );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x+flWidth, y+flHeight, 0.0 );		// Lower right
	meshBuilder.TexCoord2f( 0, _u[2], _v[2] );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x+flWidth, y, 0.0 );		// Upper right
	meshBuilder.TexCoord2f( 0, _u[3], _v[3] );
	meshBuilder.Color4f( 1.0f, 1.0f, 1.0f, flAlpha );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}


//------------------------------------------------------------------------------
// CFilmGrainEffect render
//------------------------------------------------------------------------------
void CFilmGrainEffect::Render( int x, int y, int w, int h )
{
	if( !m_bEnable )
		return;

	int nTime = (int)(gpGlobals->curtime * m_flUpdateRate);

	// Set up random offsets for grain texture
	float flBiasU  = RandomFloat();
	float flBiasV  = RandomFloat();
	float flScaleU = w / 256.0f;
	float flScaleV = h / 256.0f;

	flBiasU *= w;
	flBiasV *= h;

	int paramCount = m_GrainMaterial->ShaderParamCount();
	IMaterialVar **pParams = m_GrainMaterial->GetShaderParams();
	for( int i=0;i<paramCount;i++ )
	{
		IMaterialVar *pVar = pParams[i];

		// alpha operates from 1.0 -> m_NoiseScale.w
		float alpha = 1.0 - ( 1.0 - m_NoiseScale.w ) * m_flEffectAlpha;
		
		if( !Q_stricmp( pVar->GetName(), "$noisescale" ) )
		{
			if( g_pMaterialSystemHardwareConfig->GetDXSupportLevel()<=70 )
			{
			 	pVar->SetVecValue( m_NoiseScale.x*g_flDX7NoiseScale * m_flEffectAlpha,
									m_NoiseScale.y*g_flDX7NoiseScale * m_flEffectAlpha,
									m_NoiseScale.z*g_flDX7NoiseScale * m_flEffectAlpha,
									alpha );
			}
			else
			{
			 	pVar->SetVecValue( m_NoiseScale.x * m_flEffectAlpha,
									m_NoiseScale.y * m_flEffectAlpha,
									m_NoiseScale.z * m_flEffectAlpha,
									alpha );
			}
		}
	}

	// Render Effect
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( m_GrainMaterial );
	if( m_bSplitScreen )
	{
		DrawSplotch(  0.0f, -1.0f, 2.0f, 2.0f, flBiasU, flBiasV, flScaleU, flScaleV, 0 );
	}
	else
	{
		DrawSplotch( -1.0f, -1.0f, 2.0f, 2.0f, flBiasU, flBiasV, flScaleU, flScaleV, 0 );
	}

	int screenWidth, screenHeight;
 	pRenderContext->GetRenderTargetDimensions( screenWidth, screenHeight );

	// Now let's do the flicker
	if( m_bEnableFlicker )
	{
		if( !IsErrorMaterial( m_pFlickerMaterial ) )
		{
			m_pFlickerMaterial->Refresh();
			m_pFlickerMaterial->SetMaterialVarFlag( MATERIAL_VAR_VERTEXALPHA, true );
			pRenderContext->Bind( m_pFlickerMaterial );

			int nFlickerAlpha = (int)( m_nFlickerAlpha * m_flEffectAlpha );

			if( !m_bSplitScreen )
			{
				DrawSplotch( -1.0f, -1.0f, 2.0f, 2.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0, nFlickerAlpha );
			}
			else
			{
				DrawSplotch(  0.0f, -1.0f, 2.0f, 2.0f, 0.5f, 1.0f, 0.5f, -1.0f, 0, nFlickerAlpha );
			}
		}
	}

	// Now for some dust particles
	if( nTime!=m_nCachedParticleTime )
	{
		// Need to regenerate our dust particles
		m_CachedParticles.RemoveAll();
		m_nCachedParticleTime = nTime;

		float flDustProb = RandomFloat( 0.0f, 1.0f );
		if( flDustProb < m_flChanceOfDust )
		{
			int numDustParticles = RandomInt( 0, m_nMaxDustParticles );
			for( int i=0;i<numDustParticles;i++ )
			{
				FilmDustParticle_t particle;

				particle.m_nChannel = RandomInt( 0, 2 );				

				particle.m_flPositionX = RandomFloat( -1.0f, 1.0f );
				particle.m_flPositionY = RandomFloat( -1.0f, 1.0f );

				particle.m_nOrientation = RandomInt( 0, 3 );

				particle.m_nSplotchIndex = RandomInt( 0, 15 );
				clamp( particle.m_nSplotchIndex, 0, 15 );
                                      
				particle.m_flHeight = RandomFloat( m_flMinDustSize, m_flMaxDustSize );
				particle.m_flWidth = particle.m_flHeight * (float)screenHeight / (float)screenWidth;

				if( (!m_bSplitScreen) || (particle.m_flPositionX-particle.m_flWidth > 0.0f) )
				{
					m_CachedParticles.AddToTail( particle );
				}
			}
		}
	}

	for( int i=0;i<m_CachedParticles.Count();i++ )
	{
		FilmDustParticle_t *particle = &m_CachedParticles[i];

		float channelSelect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
 		channelSelect[ particle->m_nChannel ] = 1.0f;

		paramCount = m_DustMaterial->ShaderParamCount();
		pParams = m_DustMaterial->GetShaderParams();
		for( int i=0;i<paramCount;i++ )
		{
			IMaterialVar *pVar = pParams[i];
			
			if( !Q_stricmp( pVar->GetName(), "$channel_select" ) )
			{
				pVar->SetVecValue( channelSelect[0], channelSelect[1], channelSelect[2], channelSelect[3] );
			}
		}

        float flUOffset = (particle->m_nSplotchIndex % 4) / 4.0f;
		float flVOffset = (particle->m_nSplotchIndex / 4) / 4.0f;

		pRenderContext->Bind( m_DustMaterial );

		DrawSplotch( particle->m_flPositionX, particle->m_flPositionY, particle->m_flWidth * m_flEffectAlpha, particle->m_flHeight * m_flEffectAlpha,
						flUOffset, flVOffset, 0.25f, 0.25f, particle->m_nOrientation );
	}
}


//------------------------------------------------------------------------------
// Console Interface
//------------------------------------------------------------------------------
static void EnableFilmGrain( IConVar *pConVar, char const *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if( var.GetBool() )
	{
		g_pScreenSpaceEffects->EnableScreenSpaceEffect( "filmgrain" );
	}
	else
	{
		g_pScreenSpaceEffects->DisableScreenSpaceEffect( "filmgrain" );
	}
}

static ConVar mat_filmgrain( "mat_filmgrain", "0", FCVAR_CLIENTDLL, "Enable/disable film grain post effect", EnableFilmGrain );

