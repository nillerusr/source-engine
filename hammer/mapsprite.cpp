//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Supports sprite preview and sprite icons for entities.
//
//===========================================================================//

#include "stdafx.h"
#include "hammer_mathlib.h"
#include "Box3D.h"
#include "BSPFile.h"
#include "const.h"
#include "MapDefs.h"		// dvs: For COORD_NOTINIT
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapSprite.h"
#include "Render2D.h"
#include "Render3D.h"
#include "hammer.h"
#include "Texture.h"
#include "TextureSystem.h"
#include "materialsystem/imesh.h"
#include "Material.h"
#include "Options.h"
#include "camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapSprite)


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapSprite from a set
//			of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapSprite::CreateMapSprite(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	const char *pszSprite = pHelperInfo->GetParameter(0);

	//
	// If we weren't passed a sprite name as an argument, get it from our parent
	// entity's "model" key.
	//
	if (pszSprite == NULL)
	{
		pszSprite = pParent->GetKeyValue("model");
	}

	// HACK?
	// When loading sprites, it can be the case that 'materials' is prepended
	// This is because we have to look in the materials directory for sprites
	// Remove the materials prefix...
	if (pszSprite)
	{
		if (!strnicmp(pszSprite, "materials", 9) && ((pszSprite[9] == '/') || (pszSprite[9] == '\\')) )
		{
			pszSprite += 10;
		}
	}

	//
	// If we have a sprite name, create a sprite object.
	//
	CMapSprite *pSprite = NULL;

	if (pszSprite != NULL)
	{
		pSprite = CreateMapSprite(pszSprite);
		if (pSprite != NULL)
		{
			//
			// Icons are alpha tested.
			//
			if (!stricmp(pHelperInfo->GetName(), "iconsprite"))
			{
				pSprite->SetRenderMode( kRenderTransAlpha );
				pSprite->m_bIsIcon = true;
			}
			else
			{
				// FIXME: Gotta do this a little better
				// This initializes the render mode in the sprite
				pSprite->SetRenderMode( pSprite->m_eRenderMode );
			}
		}
	}

	return(pSprite);
}


//-----------------------------------------------------------------------------
// Purpose: Factory. Use this to construct CMapSprite objects, since the
//			constructor is protected.
//-----------------------------------------------------------------------------
CMapSprite *CMapSprite::CreateMapSprite(const char *pszSpritePath)
{
	CMapSprite *pSprite = new CMapSprite;

	if (pSprite != NULL)
	{
		char szPath[MAX_PATH];

		pSprite->Initialize();

		// HACK: Remove the extension, this is for backward compatability
		// It's trying to load a .spr, but we're giving it a .vmt.
		strcpy( szPath, pszSpritePath );
		char* pDot = strrchr( szPath, '.' );
		if (pDot)
			*pDot = 0;

		pSprite->m_pSpriteInfo = CSpriteCache::CreateSprite(szPath);
		if (pSprite->m_pSpriteInfo)
		{
			pSprite->CalcBounds();
		}
	}

	return(pSprite);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapSprite::CMapSprite(void)
{
	Initialize();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapSprite::~CMapSprite(void)
{
	CSpriteCache::Release(m_pSpriteInfo);
}


//-----------------------------------------------------------------------------
// Sets the render mode
//-----------------------------------------------------------------------------

void CMapSprite::SetRenderMode( int eRenderMode )
{
	m_eRenderMode = eRenderMode;
	if (m_pSpriteInfo)
		m_pSpriteInfo->SetRenderMode( m_eRenderMode );
}

//-----------------------------------------------------------------------------
// Purpose: Calculates our bounding box based on the sprite dimensions.
// Input  : bFullUpdate - Whether we should recalculate our childrens' bounds.
//-----------------------------------------------------------------------------
void CMapSprite::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	float fRadius = 8;
	
	if (m_pSpriteInfo)
	{
		fRadius = max(m_pSpriteInfo->GetWidth(), m_pSpriteInfo->GetHeight()) * m_fScale / 2.0;
		if (fRadius == 0)
		{
			fRadius = 8;
		}
	}

	//
	// Build our bounds for frustum culling in the 3D view.
	//
	Vector Mins = m_Origin - Vector(fRadius, fRadius, fRadius);
	Vector Maxs = m_Origin + Vector(fRadius, fRadius, fRadius);
	m_CullBox.UpdateBounds(Mins, Maxs);

	m_BoundingBox = m_CullBox;

	//
	// Build our bounds for 2D rendering. We keep sprites small in the 2D views no
	// matter how large they are scaled.
	//
	if (!m_bIsIcon)
	{
		fRadius = 2;
	}

	Mins = m_Origin - Vector(fRadius, fRadius, fRadius);
	Maxs = m_Origin + Vector(fRadius, fRadius, fRadius);
	m_Render2DBox.UpdateBounds(Mins, Maxs);
}


//-----------------------------------------------------------------------------
// Purpose: Returns a copy of this object.
// Output : Pointer to the new object.
//-----------------------------------------------------------------------------
CMapClass *CMapSprite::Copy(bool bUpdateDependencies)
{
	CMapSprite *pCopy = new CMapSprite;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: Turns this into a duplicate of the given object.
// Input  : pObject - Pointer to the object to copy from.
// Output : Returns a pointer to this object.
//-----------------------------------------------------------------------------
CMapClass *CMapSprite::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	CMapSprite *pFrom = dynamic_cast<CMapSprite *>(pObject);
	Assert(pObject != NULL);

	if (pObject != NULL)
	{
		CMapClass::CopyFrom(pObject, bUpdateDependencies);

		m_Angles = pFrom->m_Angles;

		m_pSpriteInfo = pFrom->m_pSpriteInfo;
		CSpriteCache::AddRef(pFrom->m_pSpriteInfo);

		m_nCurrentFrame = pFrom->m_nCurrentFrame;
		m_fSecondsPerFrame = pFrom->m_fSecondsPerFrame;
		m_fElapsedTimeThisFrame = pFrom->m_fElapsedTimeThisFrame;
		m_fScale = pFrom->m_fScale;
		SetRenderMode( pFrom->m_eRenderMode );
		m_RenderColor = pFrom->m_RenderColor;
		m_bIsIcon = pFrom->m_bIsIcon;
	}

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEnable - 
//-----------------------------------------------------------------------------
void CMapSprite::EnableAnimation(BOOL bEnable)
{
	//m_bAnimateModels = bEnable;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Angles - 
//-----------------------------------------------------------------------------
void CMapSprite::GetAngles(QAngle &Angles)
{
	Angles = m_Angles;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapSprite::Initialize(void)
{
	m_Angles.Init();

	m_eRenderMode = kRenderNormal;

	m_RenderColor.r = 255;
	m_RenderColor.g = 255;
	m_RenderColor.b = 255;

	m_fSecondsPerFrame = 1;
	m_fElapsedTimeThisFrame = 0;
	m_nCurrentFrame = 0;

	m_fScale = 0.25;

	m_bIsIcon = false;
}


//-----------------------------------------------------------------------------
// Updates time and returns the next frame
//-----------------------------------------------------------------------------
int CMapSprite::GetNextSpriteFrame( CRender3D* pRender )
{
	//
	// Determine whether we need to advance to the next frame based on our
	// sprite framerate and the elapsed time.
	//
	int nNumFrames = m_pSpriteInfo->GetFrameCount();
	if (nNumFrames > 1)
	{
		float fElapsedTime = pRender->GetElapsedTime();
		m_fElapsedTimeThisFrame += fElapsedTime;

		while (m_fElapsedTimeThisFrame > m_fSecondsPerFrame)
		{
			m_nCurrentFrame++;
			m_fElapsedTimeThisFrame -= m_fSecondsPerFrame;
		}

		m_nCurrentFrame %= nNumFrames;
	}

	return m_nCurrentFrame;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapSprite::Render3D(CRender3D *pRender)
{
	int nPasses;
	if ((GetSelectionState() != SELECT_NONE) && (!m_bIsIcon))
	{
		if (pRender->NeedsOverlay())
			nPasses = 3;
		else
			nPasses = 2;
	}
	else
	{
		nPasses = 1;
	}

	//
	// If we have a sprite, render it.
	//
	if (m_pSpriteInfo)
	{
		//
		// Only sprite icons can be clicked on, sprite preview objects cannot.
		//
		if (m_bIsIcon)
		{
			pRender->BeginRenderHitTarget(this);
		}

		m_pSpriteInfo->SetOrigin(m_Origin);
		m_pSpriteInfo->SetAngles(m_Angles);

		m_pSpriteInfo->Bind(pRender, GetNextSpriteFrame(pRender));
		for (int nPass = 0; nPass < nPasses; nPass++)
		{
			if (nPass == 0)
			{
				// First pass uses the default rendering mode.
				// unless that mode is texture
				if (pRender->GetCurrentRenderMode() == RENDER_MODE_LIGHTMAP_GRID)
					pRender->PushRenderMode( RENDER_MODE_TEXTURED);
				else
					pRender->PushRenderMode( RENDER_MODE_CURRENT );
			}
			else
			{
				if (nPass == nPasses - 1)
				{
					// last pass uses wireframe rendering mode.
					pRender->PushRenderMode( RENDER_MODE_WIREFRAME);
				}
				else
				{
					pRender->PushRenderMode( RENDER_MODE_SELECTION_OVERLAY );
				}
			}


			m_pSpriteInfo->SetScale(m_fScale > 0 ? m_fScale : 1.0 );

			float fBlend;
			// dvs: lots of things contribute to blend factor. See r_blend in engine.
			//if (m_eRenderMode == kRenderNormal)
			{
				fBlend = 1.0;
			}

			unsigned char color[4];
			SpriteColor( color, m_eRenderMode, m_RenderColor, fBlend * 255);

			//
			// If selected, render a yellow wireframe box.
			//
			if (GetSelectionState() != SELECT_NONE)
			{
				if (m_bIsIcon)
				{
					pRender->RenderWireframeBox(m_Render2DBox.bmins, m_Render2DBox.bmaxs, 255, 255, 0);
				}
				else
				{
					color[0] = 255;
					color[1] = color[2] = 0;
				}

				//
				// If selected, render the sprite with a yellow wireframe around it.
				//
				if ( nPass > 0 )
				{
					color[0] = color[1] = 255; 
					color[2] = 0;
				}
			}

			MaterialPrimitiveType_t type = (nPass > 0) ? MATERIAL_LINE_LOOP : MATERIAL_POLYGON;
			m_pSpriteInfo->SetMaterialPrimitiveType( type );

			m_pSpriteInfo->DrawSprite3D( pRender, color );

			pRender->PopRenderMode();
		}

		//
		// Only sprite icons can be clicked on, sprite preview objects cannot.
		//
		if (m_bIsIcon)
		{
			pRender->EndRenderHitTarget();
		}
	}
	//
	// Else no sprite, render as a bounding box.
	//
	else if (m_bIsIcon)
	{
		pRender->BeginRenderHitTarget(this);
		pRender->RenderBox(m_Render2DBox.bmins, m_Render2DBox.bmaxs, r, g, b, GetSelectionState());
		pRender->EndRenderHitTarget();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapSprite::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapSprite::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapSprite::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);

	matrix3x4_t fCurrentMatrix,fMatrixNew;
	AngleMatrix(m_Angles, fCurrentMatrix);
	ConcatTransforms(matrix.As3x4(), fCurrentMatrix, fMatrixNew);
	MatrixAngles(fMatrixNew, m_Angles);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pColor - 
//			pEntity - 
//			alpha - 
//-----------------------------------------------------------------------------
void CMapSprite::SpriteColor(unsigned char *pColor, int eRenderMode, colorVec RenderColor, int alpha)
{
	int a;

	if ((eRenderMode == kRenderTransAdd) || (eRenderMode == kRenderGlow) || (eRenderMode == kRenderWorldGlow))
	{
		a = alpha;
	}
	else
	{
		a = 256;
	}
	
	if ((RenderColor.r == 0) && (RenderColor.g == 0) && (RenderColor.b == 0))
	{
		pColor[0] = pColor[1] = pColor[2] = (255 * a) >> 8;
	}
	else
	{
		pColor[0] = ((int)RenderColor.r * a)>>8;
		pColor[1] = ((int)RenderColor.g * a)>>8;
		pColor[2] = ((int)RenderColor.b * a)>>8;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that this object's parent entity has had a key value change.
// Input  : szKey - The key that changed.
//			szValue - The new value of the key.
//-----------------------------------------------------------------------------
void CMapSprite::OnParentKeyChanged(const char* szKey, const char* szValue)
{
	if (!stricmp(szKey, "framerate"))
	{
		float fFramesPerSecond = atof(szValue);
		if (fabs(fFramesPerSecond) > 0.001)
		{
			m_fSecondsPerFrame = 1 / fFramesPerSecond;
		}
	}
	else if (!stricmp(szKey, "scale"))
	{
		m_fScale = atof(szValue);
		if (m_fScale == 0)
		{
			m_fScale = 1;
		}
		m_pSpriteInfo->SetScale(m_fScale);

		PostUpdate(Notify_Changed);
	}
	else if (!stricmp(szKey, "rendermode"))
	{
		switch (atoi(szValue))
		{
			case 0: // "Normal"
			{
				SetRenderMode( kRenderNormal );
				break;
			}

			case 1: // "Color"
			{
				SetRenderMode( kRenderTransColor );
				break;
			}

			case 2: // "Texture"
			{
				SetRenderMode( kRenderNormal );
				break;
			}

			case 3: // "Glow"
			{
				SetRenderMode( kRenderGlow );
				break;
			}

			case 4: // "Solid"
			{
				SetRenderMode( kRenderNormal );
				break;
			}

			case 5: // "Additive"
			{
				SetRenderMode( kRenderTransAdd );
				break;
			}

			case 7: // "Additive Fractional Frame"
			{
				SetRenderMode( kRenderTransAddFrameBlend );
				break;
			}

			case 9: // "World Space Glow"
			{
				SetRenderMode( kRenderWorldGlow );
				break;
			}
		}
	}
	//
	// If we are the child of a light entity and its color is changing, change our render color.
	//
	else if (!stricmp(szKey, "_light"))
	{
		sscanf(szValue, "%d %d %d", &m_RenderColor.r, &m_RenderColor.g, &m_RenderColor.b);
	}
	else if (!stricmp(szKey, "angles"))
	{
		sscanf(szValue, "%f %f %f", &m_Angles[PITCH], &m_Angles[YAW], &m_Angles[ROLL]);
		PostUpdate(Notify_Changed);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMapSprite::ShouldRenderLast(void)
{
	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapSprite::Render2D(CRender2D *pRender)
{
	Vector vecMins;
	Vector vecMaxs;
	GetRender2DBox(vecMins, vecMaxs);

	Vector2D pt,pt2;
	pRender->TransformPoint(pt, vecMins);
	pRender->TransformPoint(pt2, vecMaxs);

	if ( !IsSelected() )
	{
	    pRender->SetDrawColor( r, g, b );
		pRender->SetHandleColor( r, g, b );
	}
	else
	{
	    pRender->SetDrawColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
		pRender->SetHandleColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
	}

	// Draw the bounding box.
		
	pRender->DrawBox( vecMins, vecMaxs );

	//
	// Draw center handle.
	//

	if ( pRender->IsActiveView() )
	{
		int sizex = abs(pt.x - pt2.x)+1;
		int sizey = abs(pt.y - pt2.y)+1;

		// dont draw handle if object is too small
		if ( sizex > 6 && sizey > 6 )
		{
			pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CROSS );
			pRender->DrawHandle( (vecMins+vecMaxs)/2 );
		}
	}
}


//-----------------------------------------------------------------------------
// Called by entity code to render sprites
//-----------------------------------------------------------------------------
void CMapSprite::RenderLogicalAt(CRender2D *pRender, const Vector2D &vecMins, const Vector2D &vecMaxs )
{
	// If we have a sprite, render it.
	if (!m_pSpriteInfo)
		return;

	m_pSpriteInfo->Bind( pRender, 0 );
	pRender->PushRenderMode( RENDER_MODE_TEXTURED);

	unsigned char color[4] = { 255, 255, 255, 255 };

	SpriteColor( color, m_eRenderMode, m_RenderColor, 255);

	// If selected, render a yellow wireframe box.
	if ( GetSelectionState() != SELECT_NONE )
	{
		color[0] = 255;
		color[1] = color[2] = 0;
	}

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_POLYGON, 4 );

	meshBuilder.Position3f( vecMins.x, vecMins.y, 0.0f );
	meshBuilder.TexCoord2f(0, 0, 1);
	meshBuilder.Color3ub( color[0], color[1], color[2] );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( vecMins.x, vecMaxs.y, 0.0f );
	meshBuilder.TexCoord2f(0, 0, 0);
	meshBuilder.Color3ub( color[0], color[1], color[2] );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( vecMaxs.x, vecMaxs.y, 0.0f );
	meshBuilder.TexCoord2f(0, 1, 0);
	meshBuilder.Color3ub( color[0], color[1], color[2] );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( vecMaxs.x, vecMins.y, 0.0f );
	meshBuilder.TexCoord2f(0, 1, 1);
	meshBuilder.Color3ub( color[0], color[1], color[2] );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRender->PopRenderMode();
}