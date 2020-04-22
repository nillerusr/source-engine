//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "stdafx.h"
#include "const.h"
#include "Sprite.h"
#include "Material.h"			// FIXME: we need to work only with IEditorTexture!
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "Render3d.h"
#include "camera.h"
#include "tier1/utldict.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


class CSpriteDataCache
{
public:
	CMaterial *m_pMaterial;
	IMaterialVar *m_pFrameVar;
	IMaterialVar *m_pRenderModeVar;
	IMaterialVar *m_pOrientationVar;
	IMaterialVar *m_pOriginVar;
	int m_Width;
	int m_Height;
	bool m_bOriginVarFound;
	bool m_bOrientationVarFound;
};


CUtlDict<CSpriteDataCache*, int> g_SpriteDataCache;
SpriteCache_t CSpriteCache::m_Cache[SPRITE_CACHE_SIZE];
int CSpriteCache::m_nItems = 0;


//-----------------------------------------------------------------------------
// Purpose: Returns an instance of a particular studio model. If the model is
//			in the cache, a pointer to that model is returned. If not, a new one
//			is created and added to the cache.
// Input  : pszModelPath - Full path of the .MDL file.
//-----------------------------------------------------------------------------
CSpriteModel *CSpriteCache::CreateSprite(const char *pszSpritePath)
{
	//
	// First look for the sprite in the cache. If it's there, increment the
	// reference count and return a pointer to the cached sprite.
	//
	for (int i = 0; i < m_nItems; i++)
	{
		if (!stricmp(pszSpritePath, m_Cache[i].pszPath))
		{
			m_Cache[i].nRefCount++;
			return(m_Cache[i].pSprite);
		}
	}

	//
	// If it isn't there, try to create one.
	//
	CSpriteModel *pSprite = new CSpriteModel;

	if (pSprite != NULL)
	{
		if (!pSprite->LoadSprite(pszSpritePath))
		{
			delete pSprite;
			pSprite = NULL;
		}
	}

	//
	// If we successfully created it, add it to the cache.
	//
	if (pSprite != NULL)
	{
		CSpriteCache::AddSprite(pSprite, pszSpritePath);
	}

	return(pSprite);
}


//-----------------------------------------------------------------------------
// Purpose: Adds the model to the cache, setting the reference count to one.
// Input  : pModel - Model to add to the cache.
//			pszSpritePath - The full path of the .MDL file, which is used as a
//				key in the sprite cache.
// Output : Returns TRUE if the sprite was successfully added, FALSE if we ran
//			out of memory trying to add the sprite to the cache.
//-----------------------------------------------------------------------------
bool CSpriteCache::AddSprite(CSpriteModel *pSprite, const char *pszSpritePath)
{
	//
	// Copy the sprite pointer.
	//
	m_Cache[m_nItems].pSprite = pSprite;

	//
	// Allocate space for and copy the model path.
	//
	m_Cache[m_nItems].pszPath = new char [strlen(pszSpritePath) + 1];
	if (m_Cache[m_nItems].pszPath != NULL)
	{
		strcpy(m_Cache[m_nItems].pszPath, pszSpritePath);
	}
	else
	{
		return(false);
	}

	m_Cache[m_nItems].nRefCount = 1;

	m_nItems++;

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Increments the reference count on a sprite in the cache. Called by
//			client code when a pointer to the sprite is copied, making that
//			reference independent.
// Input  : pModel - Sprite for which to increment the reference count.
//-----------------------------------------------------------------------------
void CSpriteCache::AddRef(CSpriteModel *pSprite)
{
	for (int i = 0; i < m_nItems; i++)
	{
		if (m_Cache[i].pSprite == pSprite)
		{
			m_Cache[i].nRefCount++;
			return;
		}
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Called by client code to release an instance of a model. If the
//			model's reference count is zero, the model is freed.
// Input  : pModel - Pointer to the model to release.
//-----------------------------------------------------------------------------
void CSpriteCache::Release(CSpriteModel *pSprite)
{
	for (int i = 0; i < m_nItems; i++)
	{
		if (m_Cache[i].pSprite == pSprite)
		{
			m_Cache[i].nRefCount--;
			Assert(m_Cache[i].nRefCount >= 0);

			//
			// If this model is no longer referenced, free it and remove it
			// from the cache.
			//
			if (m_Cache[i].nRefCount <= 0)
			{
				//
				// Free the path, which was allocated by AddModel.
				//
				delete [] m_Cache[i].pszPath;
				delete m_Cache[i].pSprite;

				//
				// Decrement the item count and copy the last element in the cache over
				// this element.
				//
				m_nItems--;

				m_Cache[i].pSprite = m_Cache[m_nItems].pSprite;
				m_Cache[i].pszPath = m_Cache[m_nItems].pszPath;
				m_Cache[i].nRefCount = m_Cache[m_nItems].nRefCount;
			}

			break;
		}
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CSpriteModel::CSpriteModel(void) : 
	m_pMaterial(0), m_NumFrames(-1), m_fScale(1.0), m_Origin(0,0,0), m_UL(0,0), m_LR(0,0), m_TexUL(0,1), m_TexLR(1,0), m_bInvert(false)
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees the sprite image and descriptor.
//-----------------------------------------------------------------------------
CSpriteModel::~CSpriteModel(void)
{
}


//-----------------------------------------------------------------------------
// Sets the render mode
//-----------------------------------------------------------------------------
void CSpriteModel::SetRenderMode( const int mode )
{
	if (m_pMaterial && m_pRenderModeVar)
	{
		if ( mode != m_pRenderModeVar->GetIntValue() )
		{
			m_pRenderModeVar->SetIntValue( mode );
			m_pMaterial->GetMaterial()->RecomputeStateSnapshots();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pEntity - 
//			type - 
//			forward - 
//			right - 
//			up - 
//-----------------------------------------------------------------------------
void CSpriteModel::GetSpriteAxes(QAngle& Angles, int type, Vector& forward, Vector& right, Vector& up, Vector& ViewUp, Vector& ViewRight, Vector& ViewForward)
{
	int				i;
	float			dot, angle, sr, cr;
	Vector			tvec;

	// Automatically roll parallel sprites if requested
	if (Angles[2] != 0 && type == SPR_VP_PARALLEL )
	{
		type = SPR_VP_PARALLEL_ORIENTED;
	}

	switch (type)
	{
		case SPR_FACING_UPRIGHT:
		{
			// generate the sprite's axes, with vup straight up in worldspace, and
			// r_spritedesc.vright perpendicular to modelorg.
			// This will not work if the view direction is very close to straight up or
			// down, because the cross product will be between two nearly parallel
			// vectors and starts to approach an undefined state, so we don't draw if
			// the two vectors are less than 1 degree apart
			tvec[0] = -m_Origin[0];
			tvec[1] = -m_Origin[1];
			tvec[2] = -m_Origin[2];
			VectorNormalize (tvec);
			dot = tvec[2];	// same as DotProduct (tvec, r_spritedesc.vup) because
							//  r_spritedesc.vup is 0, 0, 1
			if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
				return;
			up[0] = 0;
			up[1] = 0;
			up[2] = 1;
			right[0] = tvec[1];
									// CrossProduct(r_spritedesc.vup, -modelorg,
			right[1] = -tvec[0];
									//              r_spritedesc.vright)
			right[2] = 0;
			VectorNormalize (right);
			forward[0] = -right[1];
			forward[1] = right[0];
			forward[2] = 0;
						// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
						//  r_spritedesc.vpn)
			break;
		}

		case SPR_VP_PARALLEL:
		{
			// generate the sprite's axes, completely parallel to the viewplane. There
			// are no problem situations, because the sprite is always in the same
			// position relative to the viewer
			for (i=0 ; i<3 ; i++)
			{
				up[i] = ViewUp[i];
				right[i] = ViewRight[i];
				forward[i] = ViewForward[i];
			}
			break;
		}
	
		case SPR_VP_PARALLEL_UPRIGHT:
		{
			// generate the sprite's axes, with vup straight up in worldspace, and
			// r_spritedesc.vright parallel to the viewplane.
			// This will not work if the view direction is very close to straight up or
			// down, because the cross product will be between two nearly parallel
			// vectors and starts to approach an undefined state, so we don't draw if
			// the two vectors are less than 1 degree apart
			dot = ViewForward[2];	// same as DotProduct (vpn, r_spritedesc.vup) because
									//  r_spritedesc.vup is 0, 0, 1
			if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
				return;
			
			up[0] = 0;
			up[1] = 0;
			up[2] = 1;
			
			right[0] = ViewForward[1];
			right[1] = -ViewForward[0];
			right[2] = 0;
			VectorNormalize (right);

			forward[0] = -right[1];
			forward[1] = right[0];
			forward[2] = 0;
			break;
		}
	
		case SPR_ORIENTED:
		{
			// generate the sprite's axes, according to the sprite's world orientation
			AngleVectors(Angles, &forward, &right, &up);
			break;
		}

		case SPR_VP_PARALLEL_ORIENTED:
		{
			// generate the sprite's axes, parallel to the viewplane, but rotated in
			// that plane around the center according to the sprite entity's roll
			// angle. So vpn stays the same, but vright and vup rotate
			angle = Angles[ROLL] * (M_PI*2 / 360);
			sr = sin(angle);
			cr = cos(angle);

			for (i=0 ; i<3 ; i++)
			{
				forward[i] = ViewForward[i];
				right[i] = ViewRight[i] * cr + ViewUp[i] * sr;
				up[i] = ViewRight[i] * -sr + ViewUp[i] * cr;
			}
			break;
		}

		default:
		{
			//Sys_Error ("R_DrawSprite: Bad sprite type %d", type);
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Sets the sprite's scale
//-----------------------------------------------------------------------------

void CSpriteModel::SetScale( const float fScale )
{
	m_fScale = fScale;
}

//-----------------------------------------------------------------------------
// Sets the sprite's origin
//-----------------------------------------------------------------------------

void CSpriteModel::SetOrigin( const Vector &v )
{
	m_Origin = v;
}

//-----------------------------------------------------------------------------
// Sets the sprite's origin
//-----------------------------------------------------------------------------

void CSpriteModel::GetOrigin( Vector &v )
{
	v = m_Origin;
}

//-----------------------------------------------------------------------------
// Sets the sprite's vertical inversion
//-----------------------------------------------------------------------------

void CSpriteModel::SetInvert( const bool b )
{
	m_bInvert = b;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the Euler angles for the model.
// Input  : fAngles - A pointer to engine PITCH, YAW, and ROLL angles.
//-----------------------------------------------------------------------------
void CSpriteModel::SetAngles( const QAngle& pfAngles )
{
	m_Angles[PITCH] = pfAngles[PITCH];
	m_Angles[YAW] = pfAngles[YAW];
	m_Angles[ROLL] = pfAngles[ROLL];
}

//-----------------------------------------------------------------------------
// Sets the material's primative type
//-----------------------------------------------------------------------------

void CSpriteModel::SetMaterialPrimitiveType( const MaterialPrimitiveType_t type )
{
	m_MaterialPrimitiveType = type;
}

//-----------------------------------------------------------------------------
// Renders the sprite in 3D mode
//-----------------------------------------------------------------------------

void CSpriteModel::DrawSprite3D( CRender3D *pRender, unsigned char color[3]  )
{
	Vector corner, spritex, spritey, spritez;
	Vector ViewUp;
	Vector ViewRight;
	Vector ViewForward;

	pRender->GetViewUp( ViewUp );
	pRender->GetViewRight( ViewRight );
	pRender->GetViewForward( ViewForward );

	GetSpriteAxes(m_Angles, GetType(), spritez, spritex, spritey, ViewUp, ViewRight, ViewForward);

	Vector2D ul, lr;
	Vector2DMultiply( m_UL, m_fScale, ul );
	Vector2DMultiply( m_LR, m_fScale, lr );

	VectorMA( m_Origin, ul.x, spritex, corner );
	VectorMA( corner, lr.y, spritey, corner );
	spritex *= (lr.x - ul.x);
	spritey *= (ul.y - lr.y);

	Vector2D texul, texlr;
	texul.x = m_TexUL.x;
	texul.y = m_bInvert ? m_TexLR.y : m_TexUL.y;
	texlr.x = m_TexLR.x;
	texlr.y = m_bInvert ? m_TexUL.y : m_TexLR.y;


	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	pRender->BindTexture( m_pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, m_MaterialPrimitiveType, 4 );

	meshBuilder.Position3fv(corner.Base());
	meshBuilder.TexCoord2f(0, texul.x, texul.y);
	meshBuilder.Color3ub( color[0], color[1], color[2] );
	meshBuilder.AdvanceVertex();

	corner += spritey;
	meshBuilder.Position3fv(corner.Base());
	meshBuilder.TexCoord2f(0, texul.x, texlr.y);
	meshBuilder.Color3ub( color[0], color[1], color[2] );
	meshBuilder.AdvanceVertex();

	corner += spritex;
	meshBuilder.Position3fv(corner.Base());
	meshBuilder.TexCoord2f(0, texlr.x, texlr.y);
	meshBuilder.Color3ub( color[0], color[1], color[2] );
	meshBuilder.AdvanceVertex();

	corner -= spritey;
	meshBuilder.Position3fv(corner.Base());
	meshBuilder.TexCoord2f(0, texlr.x, texul.y);
	meshBuilder.Color3ub( color[0], color[1], color[2] );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Binds a sprite
//-----------------------------------------------------------------------------
void CSpriteModel::Bind( CRender* pRender, int frame )
{
	if (m_pMaterial && m_pFrameVar)
	{
		m_pFrameVar->SetIntValue( frame );
		pRender->BindTexture( m_pMaterial );
	}
}


CSpriteDataCache* LookupSpriteDataCache( const char *pSpritePath )
{
	char filename[MAX_PATH];
	V_strncpy( filename, pSpritePath, sizeof( filename ) );
	V_FixSlashes( filename );
	
	CSpriteDataCache *pData;
	int i = g_SpriteDataCache.Find( filename );
	if ( i == g_SpriteDataCache.InvalidIndex() )
	{
		pData = new CSpriteDataCache;
		memset( pData, 0, sizeof( *pData ) );
		g_SpriteDataCache.Insert( filename, pData );

		pData->m_pMaterial = CMaterial::CreateMaterial( filename, true );
		if ( pData->m_pMaterial && pData->m_pMaterial->GetMaterial() )
		{
			pData->m_Width = pData->m_pMaterial->GetWidth();
			pData->m_Height = pData->m_pMaterial->GetHeight();
			pData->m_pFrameVar = pData->m_pMaterial->GetMaterial()->FindVar( "$spriteFrame", 0 );
			pData->m_pRenderModeVar = pData->m_pMaterial->GetMaterial()->FindVar( "$spriterendermode", 0 );

			pData->m_pOrientationVar = pData->m_pMaterial->GetMaterial()->FindVar( "$spriteOrientation", &pData->m_bOrientationVarFound, false );
			pData->m_pOriginVar = pData->m_pMaterial->GetMaterial()->FindVar( "$spriteorigin", &pData->m_bOriginVarFound );
		}
	}
	else
	{
		pData = g_SpriteDataCache[i];
	}
	
	return pData;
}




//-----------------------------------------------------------------------------
// Purpose: Loads a sprite material.
// Input  : pszSpritePath - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

bool CSpriteModel::LoadSprite(const char *pszSpritePath)
{
	CSpriteDataCache *pCache = LookupSpriteDataCache( pszSpritePath );
	
	m_pMaterial = pCache->m_pMaterial;
	if( m_pMaterial && m_pMaterial->GetMaterial() )
	{
		m_Width = pCache->m_Width;
		m_Height = pCache->m_Height;
		// FIXME: m_NumFrames = m_pMaterial->GetMaterial()->GetNumAnimationFrames();
		m_pFrameVar = pCache->m_pFrameVar;
		m_pRenderModeVar = pCache->m_pRenderModeVar;

		IMaterialVar *orientationVar = pCache->m_pOrientationVar;
		bool found = pCache->m_bOrientationVarFound;
		if( found )
		{
			m_Type = orientationVar->GetIntValue();
		}
		else
		{
			m_Type = SPR_VP_PARALLEL_UPRIGHT;
		}

		IMaterialVar *pOriginVar = pCache->m_pOriginVar;
		Vector origin;
		found = pCache->m_bOriginVarFound;
		if( !found || ( pOriginVar->GetType() != MATERIAL_VAR_TYPE_VECTOR ) )
		{
			origin[0] = -m_Width * 0.5f;
			origin[1] = m_Height * 0.5f;
		}
		else
		{
			Vector originVarValue;
			pOriginVar->GetVecValue( originVarValue.Base(), 3);
			origin[0] = -m_Width * originVarValue[0];
			origin[1] = m_Height * originVarValue[1];
		}

		m_UL.y = origin[1];
		m_LR.y = origin[1] - m_Height;
		m_UL.x = origin[0];
		m_LR.x = m_Width + origin[0];
	
		return true;
	}
	else
	{
		return false;
	}
}


//-----------------------------------------------------------------------------
// Kind of a hack...
//-----------------------------------------------------------------------------
int CSpriteModel::GetFrameCount()
{
	// FIXME: Figure out the correct time to cache in this info
	if ((m_NumFrames < 0) && m_pMaterial)
	{
		m_NumFrames = m_pMaterial->GetMaterial()->GetNumAnimationFrames();
	}
	return (m_NumFrames < 0) ? 0 : m_NumFrames;
}