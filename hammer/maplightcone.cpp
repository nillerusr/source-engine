//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Renders a cone for spotlight entities. Only renders when the parent
//			entity is selected.
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "fgdlib/HelperInfo.h"
#include "MapDefs.h"		// dvs: For COORD_NOTINIT
#include "MapEntity.h"
#include "MapLightCone.h"
#include "Render3D.h"
#include "Material.h"
#include "materialsystem/imaterialsystem.h"
#include "TextureSystem.h"
#include "hammer.h"
#include "Options.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define NUM_LIGHTCONE_ZONES		5


IMPLEMENT_MAPCLASS(CMapLightCone)


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapLightCone helper from a
//			set of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the helper.
// Output : Returns a pointer to the helper, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapLightCone::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapLightCone *new1=new CMapLightCone;
	if( new1 != NULL )
	{
		//
		// The first parameter should be the inner fov key name. If it isn't
		// there we assume "_inner_cone".
		//
		const char *pszKeyName = pHelperInfo->GetParameter(0);
		if (pszKeyName != NULL)
		{
			strcpy(new1->m_szInnerConeKeyName, pszKeyName);
		}
		else
		{
			strcpy(new1->m_szInnerConeKeyName, "_inner_cone");
		}

		//
		// The second parameter should be the outer fov key name. If it isn't
		// there we assume "_cone".
		//
		pszKeyName = pHelperInfo->GetParameter(1);
		if (pszKeyName != NULL)
		{
			strcpy(new1->m_szOuterConeKeyName, pszKeyName);
		}
		else
		{
			strcpy(new1->m_szOuterConeKeyName, "_cone");
		}

		//
		// The third parameter should be the color of the light. If it isn't
		// there we assume "_light".
		//
		pszKeyName = pHelperInfo->GetParameter(2);
		if (pszKeyName != NULL)
		{
			strcpy(new1->m_szColorKeyName, pszKeyName);
		}
		else
		{
			strcpy(new1->m_szColorKeyName, "_light");
		}

		pszKeyName = pHelperInfo->GetParameter(3);
		if (pszKeyName != NULL)
		{
			new1->m_flPitchScale = Q_atof( pszKeyName );
		}
		else
		{
			new1->m_flPitchScale = 1.0f;
		}
	}
	return new1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapLightCone::CMapLightCone(void)
{
	m_fQuadraticAttn = 1;
	m_fLinearAttn = 0;
	m_fConstantAttn = 0;
	m_bPitchSet = false;
	m_fPitch = 0;
	m_fFocus = 1;
	m_flPitchScale = 1;

	m_fBrightness = 100;
	m_fInnerConeAngle = 0;
	m_fOuterConeAngle = 45;

	m_fFiftyPercentDistance = -1;							// disabled - use attenuation
	m_Angles.Init();
	SignalUpdate( EVTYPE_LIGHTING_CHANGED );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Deletes faces allocated by BuildCone.
//-----------------------------------------------------------------------------
CMapLightCone::~CMapLightCone(void)
{
	for (int i = 0; i < m_Faces.Count(); i++)
	{
		CMapFace *pFace = m_Faces.Element(i);
		delete pFace;
	}
	SignalUpdate( EVTYPE_LIGHTING_CHANGED );
}


//-----------------------------------------------------------------------------
// Purpose: Builds the light cone faces in local space. Does NOT call CalcBounds,
//			because that CalcBounds updates the parent, which causes problems
//			in the undo system.
//-----------------------------------------------------------------------------
void CMapLightCone::BuildCone(void)
{
	//
	// Delete the current face list.
	//
	for (int i = 0; i < m_Faces.Count(); i++)
	{
		CMapFace *pFace = m_Faces.Element(i);
		delete pFace;
	}
	m_Faces.RemoveAll();

	//
	// Make sure at least one of the lighting coefficients is nonzero.
	//
	if ((m_fQuadraticAttn == 0) && (m_fLinearAttn == 0) && (m_fConstantAttn == 0))
	{
		m_fConstantAttn = 1;
	}

	//
	// Solve for the lighting scale factor by which the brightness will be multiplied.
	//
	float fScaleFactor = m_fQuadraticAttn * 10000 + m_fLinearAttn * 100 + m_fConstantAttn;
	if (fScaleFactor == 0)
	{
		return;
	}

	//
	// Calculate the distances from the light origin to the various zones.
	//
	float fOffsetDist = 0;
	// Constant attenuation factor doesn't actually offset the cone yet. If it does, uncomment this:
	//SolveQuadratic(fOffsetDist, 0, m_fQuadraticAttn, m_fLinearAttn, -m_fConstantAttn);

	float fZoneDist[NUM_LIGHTCONE_ZONES];
	memset( fZoneDist, 0, sizeof( fZoneDist ) );
	fZoneDist[0] = 0;
	SolveQuadratic(fZoneDist[1], 0.25 * fScaleFactor, m_fQuadraticAttn, m_fLinearAttn, m_fConstantAttn);
	SolveQuadratic(fZoneDist[2], fScaleFactor, m_fQuadraticAttn, m_fLinearAttn, m_fConstantAttn);
	SolveQuadratic(fZoneDist[3], 4 * fScaleFactor, m_fQuadraticAttn, m_fLinearAttn, m_fConstantAttn);
	SolveQuadratic(fZoneDist[4], Options.view3d.fLightConeLength * fScaleFactor, m_fQuadraticAttn, m_fLinearAttn, m_fConstantAttn);

	//
	// there's no cone if it's greater then 90 degrees
	//
	if (m_fOuterConeAngle < 90)
	{

		//
		// Calculate the cone radius at each zone.
		//
		float fZoneRadius[NUM_LIGHTCONE_ZONES];
		for (int i = 0; i < NUM_LIGHTCONE_ZONES; i++)
		{
			fZoneRadius[i] = (fOffsetDist + fZoneDist[i]) * tan(DEG2RAD(m_fOuterConeAngle));
		}

		//
		// Build the new face list using the new parameters.
		//
		float fStepSize = 360.0 / 15.0;
		for (int nZone = 0; nZone < NUM_LIGHTCONE_ZONES - 1; nZone++)
		{
			float fSin0 = 0;
			float fCos0 = 1;

			float fTopDist = fZoneDist[nZone];
			float fBottomDist = fZoneDist[nZone + 1];

			float fTopRadius = fZoneRadius[nZone];
			float fBottomRadius = fZoneRadius[nZone + 1];

			for (int fAngle = fStepSize; fAngle <= 361; fAngle += fStepSize)
			{
				float fSin1 = sin(DEG2RAD(fAngle));
				float fCos1 = cos(DEG2RAD(fAngle));

				Vector Points[4];

				Points[0][2] = fBottomRadius * fCos1;
				Points[0][1] = fBottomRadius * fSin1;
				Points[0][0] = fBottomDist;

				Points[1][2] = fBottomRadius * fCos0;
				Points[1][1] = fBottomRadius * fSin0;
				Points[1][0] = fBottomDist;

				Points[2][2] = fTopRadius * fCos0;
				Points[2][1] = fTopRadius * fSin0;
				Points[2][0] = fTopDist;

				int nPoints = 3;
				if (fTopRadius != 0)
				{
					Points[3][2] = fTopRadius * fCos1;
					Points[3][1] = fTopRadius * fSin1;
					Points[3][0] = fTopDist;
					nPoints = 4;
				}

				CMapFace *pFace = new CMapFace;
				pFace->SetRenderColor(r * (1 - nZone / (float)NUM_LIGHTCONE_ZONES), g * (1 - nZone / (float)NUM_LIGHTCONE_ZONES), b * (1 - nZone / (float)NUM_LIGHTCONE_ZONES));
				pFace->SetRenderAlpha(180);
				pFace->CreateFace(Points, nPoints);
				pFace->RenderUnlit(true);
				m_Faces.AddToTail(pFace);

				fSin0 = fSin1;
				fCos0 = fCos1;
			}
		}
	}

	//
	// Lobe's aren't defined for > 90
	//
	if (m_fOuterConeAngle > 90)
		return;

	//
	// Build the a face list that shows light-angle falloff
	//
	float fStepSize = 360.0 / 15.0;
	float fPitchStepSize = 90.0 / 15.0;
	float fFocusRadius0 = 0;
	float fFocusDist0 = fZoneDist[1];
	float fInnerDot = cos(DEG2RAD(m_fInnerConeAngle));
	float fOuterDot = cos(DEG2RAD(m_fOuterConeAngle));

	for (float fPitch = fPitchStepSize; fPitch < m_fOuterConeAngle + fPitchStepSize; fPitch += fPitchStepSize)
	{
		float fSin0 = 0;
		float fCos0 = 1;

		// clamp to edge of cone
		if (fPitch > m_fOuterConeAngle)
			fPitch = m_fOuterConeAngle;

		float fIllumination = 0;
		if (fPitch <= m_fInnerConeAngle)
		{
			fIllumination = 1.0;
		}
		else
		{
			float fPitchDot = cos(DEG2RAD(fPitch));

			fIllumination = (fPitchDot - fOuterDot) / (fInnerDot - fOuterDot);

			if ((m_fFocus != 1) && (m_fFocus != 0))
			{
				fIllumination = pow( fIllumination, m_fFocus );
			}
		}

		// cosine falloff ^ exponent

		// draw as lobe
		float fFocusDist1 = cos(DEG2RAD(fPitch)) * fIllumination * fZoneDist[1];
		float fFocusRadius1 = sin(DEG2RAD(fPitch)) * fIllumination * fZoneDist[1];

		// draw as disk
		// float fFocusDist1 = fZoneDist[1];
		// float fFocusRadius1 = sin(DEG2RAD(fPitch)) * fZoneRadius[1] / sin(DEG_RAD * m_fConeAngle);

		for (int fAngle = fStepSize; fAngle <= 361; fAngle += fStepSize)
		{
			float fSin1 = sin(DEG2RAD(fAngle));
			float fCos1 = cos(DEG2RAD(fAngle));

			Vector Points[4];

			Points[0][2] = fFocusRadius1 * fCos0;
			Points[0][1] = fFocusRadius1 * fSin0;
			Points[0][0] = fFocusDist1;

			Points[1][2] = fFocusRadius1 * fCos1;

			Points[1][1] = fFocusRadius1 * fSin1;
			Points[1][0] = fFocusDist1;

			Points[2][2] = fFocusRadius0 * fCos1;
			Points[2][1] = fFocusRadius0 * fSin1;
			Points[2][0] = fFocusDist0;

			int nPoints = 3;
			if (fFocusRadius0 != 0)
			{
				Points[3][2] = fFocusRadius0 * fCos0;
				Points[3][1] = fFocusRadius0 * fSin0;
				Points[3][0] = fFocusDist0;
				nPoints = 4;
			}

			CMapFace *pFace = new CMapFace;
			pFace->SetRenderColor(r * fIllumination, g * fIllumination, b * fIllumination);
			pFace->SetRenderAlpha(180);
			pFace->CreateFace(Points, nPoints);
			pFace->RenderUnlit(true);
			m_Faces.AddToTail(pFace);

			fSin0 = fSin1;
			fCos0 = fCos1;
		}
		fFocusRadius0 = fFocusRadius1;
		fFocusDist0 = fFocusDist1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapLightCone::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	//
	// HACK: Update our origin to stick to our parent.
	//
	if (m_pParent != NULL)
	{
		GetParent()->GetOrigin(m_Origin);
	}

	//
	// Pretend to be very small for the 2D view. Won't be necessary when 2D
	// rendering is done in the map classes.
	//
	m_Render2DBox.ResetBounds();
	m_Render2DBox.UpdateBounds(m_Origin);

	SetCullBoxFromFaceList( &m_Faces );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Angles - 
//-----------------------------------------------------------------------------
void CMapLightCone::GetAngles(QAngle &Angles)
{
	Angles = m_Angles;

	if (m_bPitchSet)
	{
		Angles[PITCH] = m_fPitch;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapLightCone::Copy(bool bUpdateDependencies)
{
	CMapLightCone *pCopy = new CMapLightCone;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapLightCone::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapLightCone)));
	CMapLightCone *pFrom = (CMapLightCone *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_fBrightness = pFrom->m_fBrightness;

	m_fQuadraticAttn = pFrom->m_fQuadraticAttn;
	m_fLinearAttn = pFrom->m_fLinearAttn;
	m_fConstantAttn = pFrom->m_fConstantAttn;
	m_flPitchScale = pFrom->m_flPitchScale;

	m_fInnerConeAngle = pFrom->m_fInnerConeAngle;
	m_fOuterConeAngle = pFrom->m_fOuterConeAngle;

	m_Angles = pFrom->m_Angles;

	m_bPitchSet = pFrom->m_bPitchSet;
	m_fPitch = pFrom->m_fPitch;

	m_fFocus = pFrom->m_fFocus;

	m_fFiftyPercentDistance = pFrom->m_fFiftyPercentDistance;
	m_fZeroPercentDistance = pFrom->m_fZeroPercentDistance;
	m_LightColor = pFrom->m_LightColor;
	
	Q_strncpy( m_szColorKeyName, pFrom->m_szColorKeyName, sizeof( m_szColorKeyName ) );
	Q_strncpy( m_szInnerConeKeyName, pFrom->m_szInnerConeKeyName, sizeof( m_szInnerConeKeyName ) );
	Q_strncpy( m_szOuterConeKeyName, pFrom->m_szOuterConeKeyName, sizeof( m_szOuterConeKeyName ) );

	BuildCone();

	SignalUpdate( EVTYPE_LIGHTING_CHANGED );
	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that this object's parent entity has had a key value change.
// Input  : szKey - The key that changed.
//			szValue - The new value of the key.
//-----------------------------------------------------------------------------
void CMapLightCone::OnParentKeyChanged(const char *szKey, const char *szValue)
{
	bool bRebuild = true;

	if (!stricmp(szKey, "angles"))
	{
		sscanf(szValue, "%f %f %f", &m_Angles[PITCH], &m_Angles[YAW], &m_Angles[ROLL]);
	}
	else if (!stricmp(szKey, m_szColorKeyName))
	{
		int nRed;
		int nGreen;
		int nBlue;
		int nBrightness;
		sscanf(szValue, "%d %d %d %d", &nRed, &nGreen, &nBlue, &nBrightness);

		r = m_LightColor.x = nRed;
		g = m_LightColor.y = nGreen;
		b = m_LightColor.z = nBlue;
		m_fBrightness = nBrightness;
	}
	else if (!stricmp(szKey, "pitch"))
	{
		// Pitch
		m_bPitchSet = true;
		m_fPitch = atof(szValue);
	}
	else if (!stricmp(szKey, "_constant_attn"))
	{
		// Constant attenuation
		m_fConstantAttn = atof(szValue);
	}
	else if (!stricmp(szKey, "_linear_attn"))
	{
		// Linear attenuation
		m_fLinearAttn = atof(szValue);
	}
	else if (!stricmp(szKey, "_quadratic_attn"))
	{
		// Quadratic attenuation
		m_fQuadraticAttn = atof(szValue);
	}
	else if (!stricmp(szKey, "_exponent"))
	{
		// Focus
		m_fFocus = atof(szValue);
	}
	else if (!stricmp(szKey, "_fifty_percent_distance"))
	{
		// Focus
		m_fFiftyPercentDistance = atof(szValue);
	}
	else if (!stricmp(szKey, "_zero_percent_distance"))
	{
		// Focus
		m_fZeroPercentDistance = atof(szValue);
	}
	else if (!stricmp(szKey, m_szInnerConeKeyName) || !stricmp(szKey, m_szOuterConeKeyName))
	{
		// check both of these together since they might be the same key.
		if( !stricmp(szKey, m_szInnerConeKeyName ))
		{
			// Inner Cone angle
			m_fInnerConeAngle = atof(szValue);
		}
		if( !stricmp(szKey, m_szOuterConeKeyName ))
		{
			// Outer Cone angle
			m_fOuterConeAngle = atof(szValue);
		}
	}
	else
	{
		bRebuild = false;
	}

	if (bRebuild)
	{
		SignalUpdate( EVTYPE_LIGHTING_CHANGED );
		BuildCone();
		PostUpdate(Notify_Changed);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
// Input  : pWorld - The world that we are in.
//-----------------------------------------------------------------------------
void CMapLightCone::PostloadWorld(CMapWorld *pWorld)
{
	CMapClass::PostloadWorld(pWorld);

	BuildCone();
	SignalUpdate( EVTYPE_LIGHTING_CHANGED );
	CalcBounds();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapLightCone::Render3D(CRender3D *pRender)
{
	if (m_pParent->IsSelected())
	{
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		pRenderContext->MatrixMode(MATERIAL_MODEL);
		pRenderContext->PushMatrix();

		pRenderContext->Translate(m_Origin[0],  m_Origin[1],  m_Origin[2]);

		QAngle Angles;
		GetAngles(Angles);

		pRenderContext->Rotate(Angles[YAW], 0, 0, 1);
		pRenderContext->Rotate(m_flPitchScale * Angles[PITCH], 0, -1, 0);
		pRenderContext->Rotate(Angles[ROLL], 1, 0, 0);

		if (
			(pRender->GetCurrentRenderMode() != RENDER_MODE_LIGHT_PREVIEW2) &&
			(pRender->GetCurrentRenderMode() != RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) &&
			(GetSelectionState() != SELECT_MODIFY )
			)
		{
			// Render the cone faces flatshaded.
			pRender->PushRenderMode( RENDER_MODE_TRANSLUCENT_FLAT );
			
			for (int i = 0; i < m_Faces.Count(); i++)
			{
				CMapFace *pFace = m_Faces.Element(i);
				pFace->Render3D(pRender);
			}

			pRender->PopRenderMode();
		}

		//
		// Render the cone faces in yellow wireframe (on top)
		//
		pRender->PushRenderMode( RENDER_MODE_WIREFRAME );

		for (int i = 0; i < m_Faces.Count(); i++)
		{
			CMapFace *pFace = m_Faces.Element(i);
			pFace->Render3D(pRender);
		}

		//
		// Restore the default rendering mode.
		//
		pRender->PopRenderMode();

		pRenderContext->PopMatrix();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapLightCone::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapLightCone::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Solves a quadratic equation with the given coefficients.
// Input  : x - Receives solution.
//			y - Root to solve for.
//			A, B, C - Quadratic, linear, and constant coefficients.
// Output : Returns true if a real solution was found, false if not.
//-----------------------------------------------------------------------------
bool CMapLightCone::SolveQuadratic(float &x, float y, float A, float B, float C)
{
	C -= y;

	if (A == 0)
	{
		if (B != 0)
		{
			x = -C / B;
			return(true);
		}
	}
	else
	{
		float fDeterminant = B * B - 4 * A * C;
		if (fDeterminant > 0)
		{
			x = (-B + sqrt(fDeterminant)) / (2 * A);
			return(true);
		}
	}

	return(false);
}


//-----------------------------------------------------------------------------
// Purpose: Never select anything because of this helper.
//-----------------------------------------------------------------------------
CMapClass *CMapLightCone::PrepareSelection(SelectMode_t eSelectMode)
{
	return NULL;
}
