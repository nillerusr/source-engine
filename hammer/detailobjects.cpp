//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Places "detail" objects which are client-only renderable things
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"

#include "collisionutils.h"
#include "const.h"
#include "interface.h"

#include "KeyValues.h"
#include "utlsymbol.h"
#include "utlvector.h"
#include "utilmatlib.h"
#include "mathlib/VMatrix.h"
#include "vstdlib/random.h"
#include "builddisp.h"
#include "tier1/utlbuffer.h"
#include "IEditorTexture.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"
#include "mapface.h"
#include "camera.h"
#include "options.h"

#include "hammer.h"

// Actually, this is the max per map, but for now this is better than no limit at all.
#define MAX_DETAIL_SPRITES_PER_FACE 65535

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//IMPLEMENT_MAPCLASS(DetailObjects)

bool DetailObjects::s_bBuildDetailObjects = true;

// Defaults to match the parsing defaults in ParseDetailGroup -- code path defaults may/may not execute
DetailObjects::~DetailObjects()
{ 
	m_DetailModels.PurgeAndDeleteElements();
	m_DetailSprites.PurgeAndDeleteElements(); 
}

DetailObjects::DetailModel_t::DetailModel_t() : m_ModelName()
{
	m_Amount = 0.0;
	m_MinCosAngle = -1.0;
	m_MaxCosAngle = -1.0;
	m_Flags = 0;
	m_Orientation = 0;
	m_Type = DETAIL_PROP_TYPE_SPRITE;
	m_Pos[0] = Vector2D(-10, 20);
	m_Pos[1] = Vector2D(10, 0);
	m_Tex[0] = Vector2D(0.5/512, 0.5/512);
	m_Tex[1] = Vector2D(63.5/512, 63.5/512);
	m_flRandomScaleStdDev = 0.0;
	m_ShapeSize = 0;
	m_ShapeAngle = 0;
	m_SwayAmount = 0;
}

CUtlVector<DetailObjects::DetailObject_t>	DetailObjects::s_DetailObjectDict;		// static members?

//-----------------------------------------------------------------------------
// Parses the key-value pairs in the detail.rad file
//-----------------------------------------------------------------------------
void DetailObjects::ParseDetailGroup( int detailId, KeyValues* pGroupKeyValues )
{
	// Sort the group by alpha
	float alpha = pGroupKeyValues->GetFloat( "alpha", 1.0f );
	
	int iGroup = s_DetailObjectDict[detailId].m_Groups.Count();
	while ( --iGroup >= 0 )
	{
		if (alpha > s_DetailObjectDict[detailId].m_Groups[iGroup].m_Alpha)
			break;
	}

	// Insert after the first guy who's more transparent that we are!
	iGroup = s_DetailObjectDict[detailId].m_Groups.InsertAfter( iGroup );
	DetailObjectGroup_t& group = s_DetailObjectDict[detailId].m_Groups[iGroup];

	group.m_Alpha = alpha;

	// Add in all the model groups
	KeyValues* pIter = pGroupKeyValues->GetFirstSubKey();
	float totalAmount = 0.0f;
	while( pIter )
	{
		if (pIter->GetFirstSubKey())
		{
			int i = group.m_Models.AddToTail();

			DetailModel_t &model = group.m_Models[i];

			model.m_ModelName = pIter->GetString( "model", 0 );
			if (model.m_ModelName != UTL_INVAL_SYMBOL)
			{
				model.m_Type = DETAIL_PROP_TYPE_MODEL;
			}
			else
			{
				const char *pSpriteData = pIter->GetString( "sprite", 0 );
				if (pSpriteData)
				{
					const char *pProcModelType = pIter->GetString( "sprite_shape", 0 );

					if ( pProcModelType )
					{
						if ( !Q_stricmp( pProcModelType, "cross" ) )
						{
							model.m_Type = DETAIL_PROP_TYPE_SHAPE_CROSS;
						}
						else if ( !Q_stricmp( pProcModelType, "tri" ) )
						{
							model.m_Type = DETAIL_PROP_TYPE_SHAPE_TRI;
						}
						else
							model.m_Type = DETAIL_PROP_TYPE_SPRITE;
					}					
					else
					{
						// card sprite
                        model.m_Type = DETAIL_PROP_TYPE_SPRITE;
					}

					model.m_Tex[0].Init();
					model.m_Tex[1].Init();

					float x = 0, y = 0, flWidth = 64, flHeight = 64, flTextureSize = 512;
					int nValid = sscanf( pSpriteData, "%f %f %f %f %f", &x, &y, &flWidth, &flHeight, &flTextureSize ); 
					if ( (nValid != 5) || (flTextureSize == 0) )
					{
						Error( "Invalid arguments to \"sprite\" in detail.vbsp (model %s)!\n", model.m_ModelName.String() );
					}

					model.m_Tex[0].x = ( x + 0.5f ) / flTextureSize;
					model.m_Tex[0].y = ( y + 0.5f ) / flTextureSize;
					model.m_Tex[1].x = ( x + flWidth - 0.5f ) / flTextureSize;
					model.m_Tex[1].y = ( y + flHeight - 0.5f ) / flTextureSize;

					model.m_Pos[0].Init( -10, 20 );
					model.m_Pos[1].Init( 10, 0 );

					pSpriteData = pIter->GetString( "spritesize", 0 );
					if (pSpriteData)
					{
						sscanf( pSpriteData, "%f %f %f %f", &x, &y, &flWidth, &flHeight );

						float ox = flWidth * x;
						float oy = flHeight * y;

						model.m_Pos[0].x = -ox;
						model.m_Pos[0].y = flHeight - oy;
						model.m_Pos[1].x = flWidth - ox;
						model.m_Pos[1].y = -oy;
					}

					model.m_flRandomScaleStdDev = pIter->GetFloat( "spriterandomscale", 0.0f );

					// sway is a percent of max sway, cl_detail_max_sway
					float flSway = clamp( pIter->GetFloat( "sway", 0.0f ), 0.0, 1.0 );
					model.m_SwayAmount = (unsigned char)( 255.0 * flSway );

					// shape angle
					// for the tri shape, this is the angle each side is fanned out
					model.m_ShapeAngle = pIter->GetInt( "shape_angle", 0 );

					// shape size
					// for the tri shape, this is the distance from the origin to the center of a side
					float flShapeSize = clamp( pIter->GetFloat( "shape_size", 0.0f ), 0.0, 1.0 );
					model.m_ShapeSize = (unsigned char)( 255.0 * flShapeSize );
				}
			}

			model.m_Amount = pIter->GetFloat( "amount", 1.0 ) + totalAmount;
			totalAmount = model.m_Amount;

			model.m_Flags = 0;
			if (pIter->GetInt( "upright", 0 ))
			{
				model.m_Flags |= MODELFLAG_UPRIGHT;
			}

			// These are used to prevent emission on steep surfaces
			float minAngle = pIter->GetFloat( "minAngle", 180 );
			float maxAngle = pIter->GetFloat( "maxAngle", 180 );
			model.m_MinCosAngle = cos(minAngle * M_PI / 180.f);
			model.m_MaxCosAngle = cos(maxAngle * M_PI / 180.f);
			model.m_Orientation = pIter->GetInt( "detailOrientation", 0 );

			// Make sure minAngle < maxAngle
			if ( model.m_MinCosAngle < model.m_MaxCosAngle)
			{
				model.m_MinCosAngle = model.m_MaxCosAngle;
			}
		}
		pIter = pIter->GetNextKey();
	}

	// renormalize the amount if the total > 1
	if (totalAmount > 1.0f)
	{
		for (int i = 0; i < group.m_Models.Count(); ++i)
		{
			group.m_Models[i].m_Amount /= totalAmount;
		}
	}
}


//-----------------------------------------------------------------------------
// Parses the key-value pairs in the detail.vbsp file
//-----------------------------------------------------------------------------
void DetailObjects::ParseDetailObjectFile( KeyValues& keyValues )
{
	// Iterate over all detail object groups...
	KeyValues* pIter;
	for( pIter = keyValues.GetFirstSubKey(); pIter; pIter = pIter->GetNextKey() )
	{
		if (!pIter->GetFirstSubKey())
			continue;

		int i = s_DetailObjectDict.AddToTail( );
		s_DetailObjectDict[i].m_Name = pIter->GetName() ;
		s_DetailObjectDict[i].m_Density = pIter->GetFloat( "density", 0.0f );

		// Iterate over all detail object groups...
		KeyValues* pIterGroups = pIter->GetFirstSubKey();
		while( pIterGroups )
		{
			if (pIterGroups->GetFirstSubKey())
			{
				ParseDetailGroup( i, pIterGroups );
			}
			pIterGroups = pIterGroups->GetNextKey();
		}
	}
}


//-----------------------------------------------------------------------------
// Finds the name of the detail.vbsp file to use
//-----------------------------------------------------------------------------
const char *DetailObjects::FindDetailVBSPName( void )
{
#if 0
	for( int i = 0; i < num_entities; i++ )
	{
		char* pEntity = ValueForKey( &entities[i], "classname" );
		if ( !strcmp( pEntity, "worldspawn" ) )
		{
			const char *pDetailVBSP = ValueForKey( &entities[i], "detailvbsp" );
			if ( !pDetailVBSP || !pDetailVBSP[0] ) 
			{
				pDetailVBSP = "detail.vbsp";
			}
			return pDetailVBSP;
		}
	}
#endif
	return "detail.vbsp";
}

#include "tier0\memdbgoff.h"

//-----------------------------------------------------------------------------
// Loads up the detail object dictionary
//-----------------------------------------------------------------------------
void DetailObjects::LoadEmitDetailObjectDictionary( const char* pGameDir )
{
	// Set the required global lights filename and try looking in qproject
	const char *pDetailVBSP = FindDetailVBSPName();
	KeyValues * values = new KeyValues( pDetailVBSP );
	if ( values->LoadFromFile( g_pFileSystem, pDetailVBSP ) )
	{
		ParseDetailObjectFile( *values );
	}
	values->deleteThis();
}


//-----------------------------------------------------------------------------
// Selects a detail group
//-----------------------------------------------------------------------------
int DetailObjects::SelectGroup( const DetailObject_t& detail, float alpha )
{
	// Find the two groups whose alpha we're between...
	int start, end;
	for ( start = 0; start < detail.m_Groups.Count() - 1; ++start )
	{
		if (alpha < detail.m_Groups[start+1].m_Alpha)
			break;
	}

	end = start + 1;
	if (end >= detail.m_Groups.Count())
		--end;

	if (start == end)
		return start;

	// Figure out how far we are between start and end...
	float dist = 0.0f;
	float dAlpha = (detail.m_Groups[end].m_Alpha - detail.m_Groups[start].m_Alpha);
	if (dAlpha != 0.0f)
	{
		dist = (alpha - detail.m_Groups[start].m_Alpha) / dAlpha;
	}

	// Pick a number, any number...
	float flR = rand() / (float)VALVE_RAND_MAX;

	// When dist == 0, we *always* want start.
	// When dist == 1, we *always* want end
	// That's why this logic looks a little reversed
	return (flR > dist) ? start : end;
}


//-----------------------------------------------------------------------------
// Selects a detail object
//-----------------------------------------------------------------------------
int DetailObjects::SelectDetail( DetailObjectGroup_t const& group )
{
	// Pick a number, any number...
	float flR = rand() / (float)VALVE_RAND_MAX;

	// Look through the list of models + pick the one associated with this number
	for ( int i = 0; i < group.m_Models.Count(); ++i )
	{
		if ( flR <= group.m_Models[i].m_Amount)
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Add a detail to the lump.
//-----------------------------------------------------------------------------

void DetailObjects::AddDetailModelToFace( const char* pModelName, const Vector& pt, const QAngle& angles, int nOrientation )
{
	StudioModel	*pStudioModel = new StudioModel();
	m_DetailModels.AddToTail( pStudioModel );
	pStudioModel->LoadModel( pModelName );
	pStudioModel->SetOrigin( pt.x, pt.y, pt.z );
	QAngle modelangle = angles;
	pStudioModel->SetAngles( modelangle );
}

//-----------------------------------------------------------------------------
// Add a detail sprite to the lump.
//-----------------------------------------------------------------------------

void DetailObjects::AddDetailSpriteToFace( const Vector &vecOrigin, const QAngle &vecAngles, DetailModel_t const& model, float flScale )
{	
	CSpriteModel	*pSpriteModel = new CSpriteModel;
	m_DetailSprites.AddToTail(pSpriteModel);

	const char	szSpriteName[_MAX_PATH] = "detail/detailsprites";

	pSpriteModel->LoadSprite( szSpriteName );

	pSpriteModel->SetRenderMode( kRenderNormal );
	pSpriteModel->SetMaterialPrimitiveType( MATERIAL_POLYGON );

	pSpriteModel->SetOrigin( vecOrigin );
	pSpriteModel->SetAngles( vecAngles );
	pSpriteModel->SetScale( flScale );
	pSpriteModel->SetInvert( true );

	pSpriteModel->SetExtent( model.m_Pos[0], model.m_Pos[1] );
	pSpriteModel->SetTextureExtent( model.m_Tex[0], model.m_Tex[1] );
}

//-----------------------------------------------------------------------------
// Got a detail! Place it on the surface...
//-----------------------------------------------------------------------------
// BUGBUG: When the global optimizer is on, "normal" gets trashed in this function
// (only when not in the debugger?)
// Printing the values of normal at the bottom of the function fixes it as does
// disabling global optimizations.
void DetailObjects::PlaceDetail( DetailModel_t const& model, const Vector& pt, const Vector& normal )
{
	// But only place it on the surface if it meets the angle constraints...
	float cosAngle = normal.z;

	// Never emit if the angle's too steep
	if (cosAngle < model.m_MaxCosAngle)
		return;

	// If it's between min + max, flip a coin...
	if (cosAngle < model.m_MinCosAngle)
	{
		float probability = (cosAngle - model.m_MaxCosAngle) / 
			(model.m_MinCosAngle - model.m_MaxCosAngle);

		float t = rand() / (float)VALVE_RAND_MAX;
		if (t > probability)
			return;
	}

	// Compute the orientation of the detail
	QAngle angles;
	if (model.m_Flags & MODELFLAG_UPRIGHT)
	{
		// If it's upright, we just select a random yaw
		angles.Init( 0, 360.0f * rand() / (float)VALVE_RAND_MAX, 0.0f );
	}
	else
	{
		// It's not upright, so it must conform to the ground. Choose
		// a random orientation based on the surface normal

		Vector zaxis;
		VectorCopy( normal, zaxis );
		VectorNormalize( zaxis );

		// Choose any two arbitrary axes which are perpendicular to the normal
		Vector xaxis( 1, 0, 0 );
		if (fabs(xaxis.Dot(zaxis)) - 1.0 > -1e-3)
			xaxis.Init( 0, 1, 0 );
		Vector yaxis;
		CrossProduct( zaxis, xaxis, yaxis );
		VectorNormalize( yaxis );
		CrossProduct( yaxis, zaxis, xaxis );
		VectorNormalize( xaxis );
		VMatrix matrix;
		matrix.SetBasisVectors( xaxis, yaxis, zaxis );
		matrix.SetTranslation( vec3_origin );

		float rotAngle = 360.0f * rand() / (float)VALVE_RAND_MAX;
		VMatrix rot = SetupMatrixAxisRot( Vector( 0, 0, 1 ), rotAngle );
		matrix = matrix * rot;

		MatrixToAngles( matrix, angles );
	}

	// FIXME: We may also want a purely random rotation too

	// Insert an element into the object dictionary if it aint there...
	switch ( model.m_Type )
	{
	case DETAIL_PROP_TYPE_MODEL:
		AddDetailModelToFace( model.m_ModelName.String(), pt, angles, model.m_Orientation );
		break;

	// Sprites and procedural models made from sprites
	case DETAIL_PROP_TYPE_SPRITE:
	default:
		{
			float flScale = 1.0f;
			if ( model.m_flRandomScaleStdDev != 0.0f ) 
			{
				flScale = fabs( RandomGaussianFloat( 1.0f, model.m_flRandomScaleStdDev ) );
			}

			AddDetailSpriteToFace( pt, angles, model, flScale );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Places Detail Objects on a face
//-----------------------------------------------------------------------------
void DetailObjects::EmitDetailObjectsOnFace( CMapFace *pMapFace, DetailObject_t& detail )
{
	// See how many points define this particular face
	int	nPoints = pMapFace->GetPointCount();

	// Faces with detail props need at least 3 point to form a plane 
	if (nPoints < 3)
		return;

	// Get the first point of the face
	Vector	p0;
	pMapFace->GetPoint(p0,0);

	// Get next points on the face in pairs -- ie get points necessary to tesselate the face into triangles
	for (int i = 1; i < nPoints-1; i++ )
	{
		// Get the next two points of the face
		Vector	p1, p2;
		pMapFace->GetPoint(p1,i);
		pMapFace->GetPoint(p2,i+1);

		// For the edges of the current triangle tesselating a portion of the face
		Vector	e1, e2;
		VectorSubtract( p1, p0, e1 );
		VectorSubtract( p2, p0, e2 );

		// Calculate the area of the tesselated triange using half the crossproduct of the edges
		Vector	areaVec;
		CrossProduct( e1, e2, areaVec );
		float	normalLength = areaVec.Length();
		float	area = 0.5 * normalLength;

		// Calculate the detail prop density based on the expected density and the tesselated triangle area
		int numSamples = clamp( area * detail.m_Density * 0.000001, 0, MAX_DETAIL_SPRITES_PER_FACE );
		
		// For each possible sample, attempt to randomly place a detail object there
		for (int j = 0; j < numSamples; ++j )
		{
			// Create a random sample location...
			float u = rand() / (float)VALVE_RAND_MAX;
			float v = rand() / (float)VALVE_RAND_MAX;

			// Make sure the u,v coordinate stay within the triangle boundaries (ie they NOT in the far half of the parallelogram)
			if (v > 1.0f - u)
			{
				// Triangle is out of bounds, flip the coordinates so they are in the near half of the parallelogram
				u = 1.0f - u;
				v = 1.0f - v;
				assert( u + v <= 1.0f );
			}

			// Compute alpha - assumed to be 1.0 across entire face for non-displacement map faces, since there is no alpha channel
			float alpha = 1.0f;

			// Select a group based on the alpha value
			int group = SelectGroup( detail, alpha );

			// Now that we've got a group, choose a detail
			int model = SelectDetail( detail.m_Groups[group] );
			if (model < 0)
				continue;

			// Got a detail! Place it on the surface...
			Vector pt, normal;
			VectorMA( p0, u, e1, pt );
			VectorMA( pt, v, e2, pt );
			VectorDivide( areaVec, -normalLength, normal );

			PlaceDetail( detail.m_Groups[group].m_Models[model], pt, normal );
		}
	}
}


//-----------------------------------------------------------------------------
// Places Detail Objects on a face
//-----------------------------------------------------------------------------
float DetailObjects::ComputeDisplacementFaceArea( CMapFace *pMapFace )
{
	float area = 0.0f;

	// Compute the area of the base face
	// Displacement base faces must be quads.
	Vector	edge[4];
	for( int i=0; i<4; i++ )
	{
		Vector	p0, p1;
		pMapFace->GetPoint( p0, i );
		pMapFace->GetPoint( p1, (i+1)%4 );
		VectorSubtract( p1, p0, edge[i] );
	}
	Vector	area_01, area_23;
	CrossProduct( edge[0], edge[1], area_01 );
	CrossProduct( edge[2], edge[3], area_23 );
	area = ( area_01.Length() + area_23.Length() ) * 0.5f;

	return area;
}


//-----------------------------------------------------------------------------
// Places Detail Objects on a face
//-----------------------------------------------------------------------------
void DetailObjects::EmitDetailObjectsOnDisplacementFace( CMapFace *pMapFace, 
						DetailObject_t& detail )
{
	assert(pMapFace->GetPointCount() == 4);

	// We're going to pick a bunch of random points, and then probabilistically
	// decide whether or not to plant a detail object there.

	// Compute the area of the base face
	float area = ComputeDisplacementFaceArea( pMapFace );

	// Compute the number of samples to take
	int numSamples = area * detail.m_Density * 0.000001;

	EditDispHandle_t	editdisphandle = pMapFace->GetDisp();
	CMapDisp			*pMapDisp = EditDispMgr()->GetDisp(editdisphandle);
	CCoreDispInfo		*pCoreDispInfo = pMapDisp->GetCoreDispInfo();

	// Now take a sample, and randomly place an object there
	for (int i = 0; i < numSamples; ++i )
	{
		// Create a random sample...
		float u = rand() / (float)VALVE_RAND_MAX;
		float v = rand() / (float)VALVE_RAND_MAX;

		// Compute alpha
		float alpha;
		Vector pt, normal;
		pCoreDispInfo->GetPositionOnSurface( u, v, pt, &normal, &alpha );
		alpha /= 255.0f;

		// Select a group based on the alpha value
		int group = SelectGroup( detail, alpha );

		// Now that we've got a group, choose a detail
		int model = SelectDetail( detail.m_Groups[group] );
		if (model < 0)
			continue;

		// Got a detail! Place it on the surface...
		PlaceDetail( detail.m_Groups[group].m_Models[model], pt, normal );
	}
}

//-----------------------------------------------------------------------------
// Builds Detail Objects for a particular face
//-----------------------------------------------------------------------------

bool	DetailObjects::ShouldRenderLast(void)
{
	return true;
}

//-----------------------------------------------------------------------------
// Builds Detail Objects for a particular face
//-----------------------------------------------------------------------------
void	DetailObjects::BuildAnyDetailObjects(CMapFace *pMapFace)
{
	// Ignore this call while loading the VMF or else we'll generate a lot of redundant ones.
	if ( !s_bBuildDetailObjects )
		return;
		
	if ( pMapFace->IsCordonFace() )
		return;

	// Try to get at the material
	bool found;

	IEditorTexture *pEditorTexture = pMapFace->GetTexture();
	if ( !pEditorTexture )
		return;

	IMaterial *pMaterial = pEditorTexture->GetMaterial();
	if ( !pMaterial )
		return;

	IMaterialVar *pMaterialVar = pMaterial->FindVar("%detailtype", &found, false );
	if ( !found || !pMaterialVar )
		return;

	const char* pDetailType = pMaterialVar->GetStringValue();
	if ( !pDetailType )
		return;

	// Get the detail type...
	DetailObject_t search;
	search.m_Name = pDetailType;

	DetailObjects	*pDetails = pMapFace->m_pDetailObjects;
	if ( pMapFace->m_pDetailObjects )
	{
		pDetails->m_DetailModels.PurgeAndDeleteElements();
		pDetails->m_DetailSprites.PurgeAndDeleteElements(); 
	}
	else
	{
		pMapFace->m_pDetailObjects = pDetails = new DetailObjects;
	}

	if ( pDetails )
	{
		// Set the center the "detailobjects" to be the average of the face points
		int	nPoints = pMapFace->GetPointCount();
		Vector	faceCenter, faceCorner;
		faceCenter.Init();
		for ( int point=0; point < nPoints; point++ )
		{
			pMapFace->GetPoint(faceCorner,point);
			faceCenter += faceCorner;
		}
		faceCenter /= nPoints;

		pDetails->SetOrigin( faceCenter );

		int objectType = s_DetailObjectDict.Find(search);
		if (objectType < 0)
		{
			char	szTextureName[MAX_PATH];
			pMapFace->GetTextureName(szTextureName);
			Warning("Material %s uses unknown detail object type %s!\n", szTextureName, pDetailType);
			return;
		}

		// Emit objects on a particular face
		DetailObject_t& detail = s_DetailObjectDict[objectType];

		// Initialize the Random Number generators for detail prop placement based on the origFace num.
		int	detailpropseed = pMapFace->GetFaceID();
#ifdef WARNSEEDNUMBER
		Warning("[%d]\n",detailpropseed);
#endif
		srand( detailpropseed );
		RandomSeed( detailpropseed );

		if ( pMapFace->HasDisp() )
		{
			pDetails->EmitDetailObjectsOnDisplacementFace( pMapFace, detail );
		}
		else
		{
			pDetails->EmitDetailObjectsOnFace( pMapFace, detail );
		}
	}
	else
	{
		Warning("Could not allocate DetailObject for CMapFace!\n");
	}
}

void DetailObjects::EnableBuildDetailObjects( bool bEnable )
{
	s_bBuildDetailObjects = bEnable;
}

void	DetailObjects::Render3D(CRender3D *pRender)
{
	Vector Mins, Maxs;
	float fDetailDistance = Options.view3d.nDetailDistance;
	Vector viewPoint; pRender->GetCamera()->GetViewPoint( viewPoint );

	int models = m_DetailModels.Count();
	if ( models )
	{
		pRender->PushRenderMode( RENDER_MODE_DEFAULT );
		for ( int i = 0; i < models; i++ )
		{
			StudioModel	*pModel = m_DetailModels[i];
			pModel->GetOrigin(Mins);
			pModel->GetOrigin(Maxs);
			for( int j=0; j<3; j++ )
			{
				Mins[j] -= fDetailDistance;
				Maxs[j] += fDetailDistance;
			}
			if ( IsPointInBox( viewPoint, Mins, Maxs ) )
				pModel->DrawModel3D( pRender, 1, false  );
		}
		pRender->PopRenderMode();

	}

	int sprites = m_DetailSprites.Count();
	if ( sprites )
	{
		unsigned char	color[3] = { 255, 255, 255 };
		pRender->PushRenderMode( RENDER_MODE_DEFAULT );
		for ( int i = 0; i < sprites; i++ )
		{
			CSpriteModel	*pSprite = m_DetailSprites[i];
			pSprite->GetOrigin(Mins);
			pSprite->GetOrigin(Maxs);
			for( int j=0; j<3; j++ )
			{
				Mins[j] -= fDetailDistance;
				Maxs[j] += fDetailDistance;
			}
			if ( IsPointInBox( viewPoint, Mins, Maxs ) )
				pSprite->DrawSprite3D( pRender, color  );
		}
		pRender->PopRenderMode();
	}
}

// EOF