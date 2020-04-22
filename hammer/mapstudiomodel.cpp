//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "GlobalFunctions.h"
#include "MapDefs.h"		// dvs: For COORD_NOTINIT
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapStudioModel.h"
#include "Render2D.h"
#include "Render3D.h"
#include "ViewerSettings.h"
#include "hammer.h"
#include "materialsystem/imesh.h"
#include "TextureSystem.h"
#include "Material.h"
#include "Options.h"
#include "camera.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#define STUDIO_RENDER_DISTANCE		400


IMPLEMENT_MAPCLASS(CMapStudioModel)


float CMapStudioModel::m_fRenderDistance = STUDIO_RENDER_DISTANCE;
BOOL CMapStudioModel::m_bAnimateModels = TRUE;


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapStudioModel from a set
//			of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapStudioModel::CreateMapStudioModel(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	const char *pszModel = pHelperInfo->GetParameter(0);

	//
	// If we weren't passed a model name as an argument, get it from our parent
	// entity's "model" key.
	//
	if (pszModel == NULL)
	{
		pszModel = pParent->GetKeyValue("model");
	}

	//
	// If we have a model name, create a studio model object.
	//
	if (pszModel != NULL)
	{
		bool bLightProp = !stricmp(pHelperInfo->GetName(), "lightprop");
		bool bOrientedBounds = (bLightProp | !stricmp(pHelperInfo->GetName(), "studioprop"));
		return CreateMapStudioModel(pszModel, bOrientedBounds, bLightProp);
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Factory function. Creates a CMapStudioModel object from a relative
//			path to an MDL file.
// Input  : pszModelPath - Relative path to the .MDL file. The path is appended
//				to each path in the	application search path until the model is found.
//			bOrientedBounds - Whether the bounding box should consider the orientation of the model.
// Output : Returns a pointer to the newly created CMapStudioModel object.
//-----------------------------------------------------------------------------
CMapStudioModel *CMapStudioModel::CreateMapStudioModel(const char *pszModelPath, bool bOrientedBounds, bool bReversePitch)
{
	CMapStudioModel *pModel = new CMapStudioModel;
	pModel->m_pStudioModel = CStudioModelCache::CreateModel(pszModelPath);
	if ( pModel->m_pStudioModel )
	{
		pModel->SetOrientedBounds(bOrientedBounds);
		pModel->ReversePitch(bReversePitch);

		pModel->CalcBounds();
	}
	else
	{
		delete pModel;
		pModel = NULL;
	}
	return(pModel);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapStudioModel::CMapStudioModel(void)
{
	Initialize();
	InitViewerSettings();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Releases the studio model cache reference.
//-----------------------------------------------------------------------------
CMapStudioModel::~CMapStudioModel(void)
{
	if (m_pStudioModel != NULL)
	{
		CStudioModelCache::Release(m_pStudioModel);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called by the renderer before every frame to animate the models.
//-----------------------------------------------------------------------------
void CMapStudioModel::AdvanceAnimation(float flInterval)
{
	if (m_bAnimateModels)
	{
		CStudioModelCache::AdvanceAnimation(flInterval);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapStudioModel::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	Vector Mins(0, 0, 0);
	Vector Maxs(0, 0, 0);

	if (m_pStudioModel != NULL)
	{
		//
		// The 3D bounds are the bounds of the oriented model's first sequence, so that
		// frustum culling works properly in the 3D view.
		//
		QAngle angles;
		GetRenderAngles(angles);

		m_pStudioModel->SetAngles(angles);
		m_pStudioModel->ExtractBbox(m_CullBox.bmins, m_CullBox.bmaxs);

		if (m_bOrientedBounds)
		{
			//
			// Oriented bounds - the 2D bounds are the same as the 3D bounds.
			//
			Mins = m_CullBox.bmins;
			Maxs = m_CullBox.bmaxs;
		}
		else
		{
			//
			// The 2D bounds are the movement bounding box of the model, which is not affected
			// by the entity's orientation. This is used for character models for which we want
			// to render a meaningful collision box in the editor.
			//
			m_pStudioModel->ExtractMovementBbox(Mins, Maxs);
		}

		Mins += m_Origin;
		Maxs += m_Origin;

		m_CullBox.bmins += m_Origin;
		m_CullBox.bmaxs += m_Origin;
	}

	//
	// If we do not yet have a valid bounding box, use a default box.
	//
	if ((Maxs - Mins) == Vector(0, 0, 0))
	{
		Mins = m_CullBox.bmins = m_Origin - Vector(10, 10, 10);
		Maxs = m_CullBox.bmaxs = m_Origin + Vector(10, 10, 10);
	}

	m_BoundingBox = m_CullBox;
	m_Render2DBox.UpdateBounds(Mins, Maxs);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapStudioModel::Copy(bool bUpdateDependencies)
{
	CMapStudioModel *pCopy = new CMapStudioModel;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: Makes this an exact duplicate of pObject.
// Input  : pObject - Object to copy.
// Output : Returns this.
//-----------------------------------------------------------------------------
CMapClass *CMapStudioModel::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapStudioModel)));
	CMapStudioModel *pFrom = (CMapStudioModel *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_pStudioModel = pFrom->m_pStudioModel;
	if (m_pStudioModel != NULL)
	{
		CStudioModelCache::AddRef(m_pStudioModel);
	}

	m_Angles = pFrom->m_Angles;
	m_Skin = pFrom->m_Skin;
	m_bOrientedBounds = pFrom->m_bOrientedBounds;
	m_bReversePitch = pFrom->m_bReversePitch;
	m_bPitchSet = pFrom->m_bPitchSet;
	m_flPitch = pFrom->m_flPitch;

	m_bScreenSpaceFade = pFrom->m_bScreenSpaceFade;
	m_flFadeScale = pFrom->m_flFadeScale;
	m_flFadeMinDist = pFrom->m_flFadeMinDist;
	m_flFadeMaxDist = pFrom->m_flFadeMaxDist;
	m_iSolid = pFrom->m_iSolid;

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEnable - 
//-----------------------------------------------------------------------------
void CMapStudioModel::EnableAnimation(BOOL bEnable)
{
	m_bAnimateModels = bEnable;
}


//-----------------------------------------------------------------------------
// Purpose: Returns this object's pitch, yaw, and roll.
//-----------------------------------------------------------------------------
void CMapStudioModel::GetAngles(QAngle &Angles)
{
	Angles = m_Angles;

	if (m_bPitchSet)
	{
		Angles[PITCH] = m_flPitch;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns this object's pitch, yaw, and roll for rendering.
//-----------------------------------------------------------------------------
void CMapStudioModel::GetRenderAngles(QAngle &Angles)
{
	GetAngles(Angles);

	if (m_bReversePitch)
	{
		Angles[PITCH] *= -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapStudioModel::Initialize(void)
{
	m_Angles.Init();
	m_bPitchSet = false;
	m_flPitch = 0;
	m_bReversePitch = false;
	m_pStudioModel = NULL;
	m_Skin = 0;

	m_bScreenSpaceFade = false;
	m_flFadeScale = 1.0f;
	m_flFadeMinDist = 0.0f;
	m_flFadeMaxDist = 0.0f;
	m_iSolid = -1;
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that this object's parent entity has had a key value change.
// Input  : szKey - The key that changed.
//			szValue - The new value of the key.
//-----------------------------------------------------------------------------
void CMapStudioModel::OnParentKeyChanged(const char* szKey, const char* szValue)
{
	if (!stricmp(szKey, "angles"))
	{
		sscanf(szValue, "%f %f %f", &m_Angles[PITCH], &m_Angles[YAW], &m_Angles[ROLL]);
		PostUpdate(Notify_Changed);
	}
	else if (!stricmp(szKey, "pitch"))
	{
		m_flPitch = atof(szValue);
		m_bPitchSet = true;

		PostUpdate(Notify_Changed);
	}
	else if (!stricmp(szKey, "skin"))
	{
		m_Skin = atoi(szValue);
		PostUpdate(Notify_Changed);
	}
	else if (!stricmp(szKey, "fademindist"))
	{
		m_flFadeMinDist = atoi(szValue);
	}
	else if (!stricmp(szKey, "fademaxdist"))
	{
		m_flFadeMaxDist = atoi(szValue);
	}
	else if (!stricmp(szKey, "screenspacefade"))
	{
		m_bScreenSpaceFade = (atoi(szValue) != 0);
	}
	else if (!stricmp(szKey, "fadescale"))
	{
		m_flFadeScale = atof(szValue);
	}
	else if ( !stricmp( szKey, "solid") )
	{
		m_iSolid = atof( szValue );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
bool CMapStudioModel::RenderPreload(CRender3D *pRender, bool bNewContext)
{
	return(m_pStudioModel != NULL);
}


//-----------------------------------------------------------------------------
// Draws basis vectors
//-----------------------------------------------------------------------------
static void DrawBasisVectors( CRender3D* pRender, const Vector &origin, const QAngle &angles)
{
	matrix3x4_t fCurrentMatrix;
	AngleMatrix(angles, fCurrentMatrix);

	pRender->PushRenderMode( RENDER_MODE_WIREFRAME );

	CMeshBuilder meshBuilder;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 );

	meshBuilder.Color3ub(255, 0, 0);
	meshBuilder.Position3f(origin[0], origin[1], origin[2]);
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3ub(255, 0, 0);
	meshBuilder.Position3f(origin[0] + (100 * fCurrentMatrix[0][0]), 
		origin[1] + (100 * fCurrentMatrix[1][0]), origin[2] + (100 * fCurrentMatrix[2][0]));
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3ub(0, 255, 0);
	meshBuilder.Position3f(origin[0], origin[1], origin[2]);
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3ub(0, 255, 0);
	meshBuilder.Position3f(origin[0] + (100 * fCurrentMatrix[0][1]), 
		origin[1] + (100 * fCurrentMatrix[1][1]), origin[2] + (100 * fCurrentMatrix[2][1]));
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3ub(0, 0, 255);
	meshBuilder.Position3f(origin[0], origin[1], origin[2]);
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3ub(0, 0, 255);
	meshBuilder.Position3f(origin[0] + (100 * fCurrentMatrix[0][2]), 
		origin[1] + (100 * fCurrentMatrix[1][2]), origin[2] + (100 * fCurrentMatrix[2][2]));
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// It should render last if any of its materials are translucent, or if
// we are previewing model fades.
//-----------------------------------------------------------------------------
bool CMapStudioModel::ShouldRenderLast()
{
	return m_pStudioModel->IsTranslucent() || Options.view3d.bPreviewModelFade;
}


//-----------------------------------------------------------------------------
// Purpose: Renders the studio model in the 2D views.
// Input  : pRender - Interface to the 2D renderer.
//-----------------------------------------------------------------------------
void CMapStudioModel::Render2D(CRender2D *pRender)
{
	Vector vecMins;
	Vector vecMaxs;
	GetRender2DBox(vecMins, vecMaxs);

	Vector2D pt,pt2;
	pRender->TransformPoint(pt, vecMins);
	pRender->TransformPoint(pt2, vecMaxs);

	color32 rgbColor = GetRenderColor();
	bool	bIsEditable = IsEditable();

	if (GetSelectionState() != SELECT_NONE)
	{
		pRender->SetDrawColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
		pRender->SetHandleColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
	}
	else
	{
		pRender->SetDrawColor( rgbColor.r, rgbColor.g, rgbColor.b );
		pRender->SetHandleColor( rgbColor.r, rgbColor.g, rgbColor.b );
	}

	int sizeX = abs(pt2.x-pt.x);
	int sizeY = abs(pt2.y-pt.y);

	//
	// Don't draw the center handle if the model is smaller than the handle cross 	
	//
	if ( bIsEditable && sizeX >= 8 && sizeY >= 8 && pRender->IsActiveView() )
	{
		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CROSS );

		pRender->DrawHandle( (vecMins+vecMaxs)/2 );
	}
	
	QAngle vecAngles;
	GetRenderAngles(vecAngles);

	bool bDrawAsModel = (Options.view2d.bDrawModels && ((sizeX+sizeY) > 50)) ||	
						IsSelected() ||	( pRender->IsInLocalTransformMode() && !pRender->GetInstanceRendering() );
						
	if ( !bDrawAsModel || IsSelected() )
	{
		// Draw the bounding box.
		pRender->DrawBox( vecMins, vecMaxs );
	}

	if ( bDrawAsModel )
	{
		//
		// Draw the model as wireframe.
		//

		m_pStudioModel->SetAngles(vecAngles);
		m_pStudioModel->SetOrigin(m_Origin[0], m_Origin[1], m_Origin[2]);
		m_pStudioModel->SetSkin(m_Skin);

		if ( GetSelectionState() == SELECT_NORMAL || ( pRender->IsInLocalTransformMode() && pRender->GetInstanceRendering() == false ) )
		{
 			// draw textured model half translucent
			m_pStudioModel->DrawModel2D(pRender, 0.6 , false );
		}
		else
		{
			// just draw the wireframe 
			m_pStudioModel->DrawModel2D(pRender, 1.0 , true );
		}
	}

	if ( IsSelected() )
	{
		//
		// Render the forward vector if the object is selected.
		//
		
		Vector Forward;
		AngleVectors(vecAngles, &Forward, NULL, NULL);

		pRender->SetDrawColor( 255, 255, 0 );
		pRender->DrawLine(m_Origin, m_Origin + Forward * 24);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline float CMapStudioModel::ComputeDistanceFade( CRender3D *pRender ) const
{
	Vector vecViewPos;
	pRender->GetCamera()->GetViewPoint( vecViewPos );

	Vector vecDelta;		
	vecDelta = m_Origin - vecViewPos;

	float flMin = min(m_flFadeMinDist, m_flFadeMaxDist);
	float flMax = max(m_flFadeMinDist, m_flFadeMaxDist);

	if (flMin < 0)
	{
		flMin = 0;
	}

	float alpha = 1.0f;
	if (flMax > 0)
	{
		float flDist = vecDelta.Length();
		if ( flDist > flMax )
		{
			alpha = 0.0f;
		}
		else if ( flDist > flMin )
		{
			alpha = RemapValClamped( flDist, flMin, flMax, 1.0f, 0 );
		}
	}
		
	return alpha;
}


//-----------------------------------------------------------------------------
// Computes fade alpha based on distance fade + screen fade
//-----------------------------------------------------------------------------
inline float CMapStudioModel::ComputeScreenFade( CRender3D *pRender ) const
{
	return 1.0;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline float CMapStudioModel::ComputeFade( CRender3D *pRender ) const
{
	if ( m_bScreenSpaceFade )
	{
		return ComputeScreenFade( pRender );
	}
	else
	{
		return ComputeDistanceFade( pRender );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders the studio model in the 3D views.
// Input  : pRender - Interface to the 3D renderer.
//-----------------------------------------------------------------------------
void CMapStudioModel::Render3D(CRender3D *pRender)
{
	Color CurrentColor;
	CurrentColor.SetColor( r, g, b );

	//
	// Set to the default rendering mode, unless we're in lightmap mode
	//
	if (pRender->GetCurrentRenderMode() == RENDER_MODE_LIGHTMAP_GRID)
		pRender->PushRenderMode(RENDER_MODE_TEXTURED);
	else
		pRender->PushRenderMode(RENDER_MODE_CURRENT);

	//
	// Set up our angles for rendering.
	//
	QAngle vecAngles;
	GetRenderAngles(vecAngles);

	//
	// If we have a model, render it if it is close enough to the camera.
	//
	if (m_pStudioModel != NULL)
	{
		Vector ViewPoint;
		pRender->GetCamera()->GetViewPoint(ViewPoint);

		Vector	Origin( m_Origin );
		if ( pRender->GetInstanceRendering() )
		{
			pRender->TransformInstanceVector( m_Origin, Origin );
		}

		if ((fabs(ViewPoint[0] - Origin[0]) < m_fRenderDistance) &&
			(fabs(ViewPoint[1] - Origin[1]) < m_fRenderDistance) &&
			(fabs(ViewPoint[2] - Origin[2]) < m_fRenderDistance))
		{
			color32 rgbColor = GetRenderColor();

			if (GetSelectionState() != SELECT_NONE)
			{
				pRender->SetDrawColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
			}
			else
			{
				// If the user disabled collisions on this instance of the model, color the wireframe differently
				if ( m_iSolid != -1 )
				{
					if ( m_iSolid == 0 )
					{
						rgbColor.r = GetRValue( Options.colors.clrModelCollisionWireframeDisabled );
						rgbColor.g = GetGValue( Options.colors.clrModelCollisionWireframeDisabled );
						rgbColor.b = GetBValue( Options.colors.clrModelCollisionWireframeDisabled );
						rgbColor.a = 255;
					}
					else
					{
						rgbColor.r = GetRValue( Options.colors.clrModelCollisionWireframe );
						rgbColor.g = GetGValue( Options.colors.clrModelCollisionWireframe );
						rgbColor.b = GetBValue( Options.colors.clrModelCollisionWireframe );
						rgbColor.a = 255;
					}
				}
				pRender->SetDrawColor( rgbColor.r, rgbColor.g, rgbColor.b );
			}

			//
			// Move the model to the proper place and orient it.
			//
			m_pStudioModel->SetAngles(vecAngles);
			m_pStudioModel->SetOrigin(m_Origin[0], m_Origin[1], m_Origin[2]);
			m_pStudioModel->SetSkin(m_Skin);

			float flAlpha = 1.0;
			if ( Options.view3d.bPreviewModelFade )
			{
				flAlpha = ComputeFade( pRender );
			}

			bool bWireframe = pRender->GetCurrentRenderMode() == RENDER_MODE_WIREFRAME;
 
			if ( GetSelectionState() == SELECT_MODIFY )
				bWireframe = true;

			pRender->BeginRenderHitTarget(this);
			m_pStudioModel->DrawModel3D(pRender, flAlpha, bWireframe );
			pRender->EndRenderHitTarget();

			if (IsSelected())
			{
				pRender->RenderWireframeBox(m_Render2DBox.bmins, m_Render2DBox.bmaxs, 255, 255, 0);
			}
		}
		else
		{
			pRender->BeginRenderHitTarget(this);
			pRender->RenderBox(m_Render2DBox.bmins, m_Render2DBox.bmaxs, CurrentColor.r(), CurrentColor.g(), CurrentColor.b(), GetSelectionState());
			pRender->EndRenderHitTarget();
		}
	}
	//
	// Else no model, render as a bounding box.
	//
	else
	{
		pRender->BeginRenderHitTarget(this);
		pRender->RenderBox(m_Render2DBox.bmins, m_Render2DBox.bmaxs, CurrentColor.r(), CurrentColor.g(), CurrentColor.b(), GetSelectionState());
		pRender->EndRenderHitTarget();
	}

	//
	// Draw our basis vectors.
	//
	if (IsSelected())
	{
		DrawBasisVectors( pRender, m_Origin, vecAngles );
	}

	pRender->PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapStudioModel::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapStudioModel::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Angles - 
//-----------------------------------------------------------------------------
void CMapStudioModel::SetAngles(QAngle &Angles)
{
	m_Angles = Angles;

	//
	// Round very small angles to zero.
	//
	for (int nDim = 0; nDim < 3; nDim++)
	{
		if (fabs(m_Angles[nDim]) < 0.001)
		{
			m_Angles[nDim] = 0;
		}
	}

	while (m_Angles[YAW] < 0)
	{
		m_Angles[YAW] += 360;
	}

	if (m_bPitchSet)
	{
		m_flPitch = m_Angles[PITCH];
	}

	//
	// Update the angles of our parent entity.
	//
	CMapEntity *pEntity = dynamic_cast<CMapEntity *>(m_pParent);
	if (pEntity != NULL)
	{
		char szValue[80];
		sprintf(szValue, "%g %g %g", m_Angles[0], m_Angles[1], m_Angles[2]);
		pEntity->NotifyChildKeyChanged(this, "angles", szValue);

		if (m_bPitchSet)
		{
			sprintf(szValue, "%g", m_flPitch);
			pEntity->NotifyChildKeyChanged(this, "pitch", szValue);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the distance at which studio models become rendered as bounding
//			boxes. If this is set to zero, studio models are never rendered.
// Input  : fRenderDistance - Distance in world units.
//-----------------------------------------------------------------------------
void CMapStudioModel::SetRenderDistance(float fRenderDistance)
{
	m_fRenderDistance = fRenderDistance;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapStudioModel::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);

	// rotate model angles

	matrix3x4_t fRotateMatrix, fCurrentMatrix, fMatrixNew;
	fRotateMatrix = matrix.As3x4();

	// Light entities negate pitch again!
	if ( m_bReversePitch )
	{
		QAngle rotAngles;
		MatrixAngles(fRotateMatrix, rotAngles);
		rotAngles[PITCH] *= -1;
		rotAngles[ROLL] *= -1;
		AngleMatrix(rotAngles, fRotateMatrix);
	}

	QAngle angles;
	GetAngles( angles );
	
	AngleMatrix( angles, fCurrentMatrix);
	ConcatTransforms(fRotateMatrix, fCurrentMatrix, fMatrixNew);
	MatrixAngles( fMatrixNew, angles );

	SetAngles( angles );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMapStudioModel::GetFrame(void)
{
	// TODO:
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFrame - 
//-----------------------------------------------------------------------------
void CMapStudioModel::SetFrame(int nFrame)
{
	// TODO:
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current sequence being used for rendering.
//-----------------------------------------------------------------------------
int CMapStudioModel::GetSequence(void)
{
	if (!m_pStudioModel)
	{
		return 0;
	}
	return m_pStudioModel->GetSequence();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMapStudioModel::GetSequenceCount(void)
{
	if (!m_pStudioModel)
	{
		return 0;
	}
	return m_pStudioModel->GetSequenceCount();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//			szName - 
//-----------------------------------------------------------------------------
void CMapStudioModel::GetSequenceName(int nIndex, char *szName)
{
	if (m_pStudioModel)
	{
		m_pStudioModel->GetSequenceName(nIndex, szName);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//-----------------------------------------------------------------------------
void CMapStudioModel::SetSequence(int nIndex)
{
	if (m_pStudioModel)
	{
		m_pStudioModel->SetSequence(nIndex);
	}
}


int CMapStudioModel::GetSequenceIndex( const char *pSequenceName ) const
{
	if ( m_pStudioModel )
	{
		int cnt = m_pStudioModel->GetSequenceCount();
		for ( int i=0; i < cnt; i++ )
		{
			char name[2048];
			m_pStudioModel->GetSequenceName( i, name );
			if ( Q_stricmp( pSequenceName, name ) == 0 )
				return i;
		}
	}

	return -1;
}
