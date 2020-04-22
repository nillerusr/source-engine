//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a factory for entity helpers. When an entity is created,
//			the helpers from its class definition in the FGD are each instantiated
//			and added as children of the entity.
//
//=============================================================================

#include "stdafx.h"
#include "HelperFactory.h"
#include "fgdlib/HelperInfo.h"
#include "MapAlignedBox.h"
#include "MapAnimator.h"
#include "MapAxisHandle.h"
#include "MapDecal.h"
#include "MapFrustum.h"
#include "MapKeyFrame.h"
#include "MapLightCone.h"
#include "MapLine.h"
#include "MapSprite.h"
#include "MapSphere.h"
#include "MapStudioModel.h"
#include "MapOverlay.h"
#include "MapPointHandle.h"
#include "MapQuadBounds.h"
#include "MapLight.h"
#include "MapSideList.h"
#include "MapCylinder.h"
#include "MapInstance.h"
#include "mapsweptplayerhull.h"
#include "DispShore.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

typedef CMapClass *HELPERFACTORY(CHelperInfo *, CMapEntity *);


typedef struct
{
	char *pszName;
	HELPERFACTORY *pfnFactory;
} HelperFactoryMap_t;


static HelperFactoryMap_t HelperFactoryMap[] =
{
	"sprite", CMapSprite::CreateMapSprite,					// Sprite, gets its render mode from the SPR file, has a selection handle.
	"iconsprite", CMapSprite::CreateMapSprite,				// Masked alpha sprite, no selection handle.
	"studio", CMapStudioModel::CreateMapStudioModel,		// Studio model with an axial bounding box.
	"studioprop", CMapStudioModel::CreateMapStudioModel,	// Studio model with an oriented bounding box.
	"lightprop", CMapStudioModel::CreateMapStudioModel,		// Studio model with an oriented bounding box, reverses pitch.
	"quadbounds", CMapQuadBounds::CreateQuadBounds,			// Extracts the verts from a quad face of a brush.
	"animator", CMapAnimator::CreateMapAnimator,			// Previews the motion of a moving entity (keyframe follower).
	"keyframe", CMapKeyFrame::CreateMapKeyFrame,			// Autonames when cloned and draws lines to the next keyframe.
	"decal", CMapDecal::CreateMapDecal,						// Decal that automatically attaches to nearby faces.
	"wirebox", CMapAlignedBox::Create,						// Wireframe box that can extract its mins & maxs.
	"line", CMapLine::Create,								// Line drawn between any two entities.
	"nodelink", CMapLine::Create,							// Line drawn between any two navigation nodes.
	"lightcone", CMapLightCone::Create,						// Faceted cone showing light angles and falloff.
	"frustum", CMapFrustum::Create,							// Wireframe frustum.
	"sphere", CMapSphere::Create,							// Wireframe sphere indicating a radius.
	"overlay", CMapOverlay::CreateMapOverlay,				// A decal with manipulatable vertices.
	"light", CMapLight::CreateMapLight,						// Light source for lighting preview.
	"sidelist", CMapSideList::CreateMapSideList,			// List of CMapFace pointers set by face ID.
	"origin", CMapPointHandle::Create,						// Handle used to manipulate the origin of an entity.
	"vecline", CMapPointHandle::Create,						// Handle used to manipulate another point that draws a line back to its parent entity.
	"axis", CMapAxisHandle::Create,							// Handle used to manipulate the axis of an entity.
	"cylinder", CMapCylinder::Create,						// Wireframe cylinder with separate radii at each end
	"sweptplayerhull", CMapSweptPlayerHull::Create,			// A swept player sized hull between two points (ladders)
	"overlay_transition", CMapOverlayTransition::Create,	// Notes!!	
	"instance", CMapInstance::Create,						// A map instance used for rendering the sub-map
};


//-----------------------------------------------------------------------------
// Purpose: Creates a helper from a helper info object.
// Input  : pHelperInfo - holds information about the helper to be created and
//				arguments to be passed to the helper when it is created.
//			pParent - The entity that will be the helper's parent.
// Output : Returns a pointer to the newly created helper.
//-----------------------------------------------------------------------------
CMapClass *CHelperFactory::CreateHelper(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	//
	// Look up the helper factory function in the factory function table.
	//
	for (int i = 0; i < sizeof(HelperFactoryMap) / sizeof(HelperFactoryMap[0]); i++)
	{
		//
		// If the function was found in the table, create the helper and return it.
		//
		if (!stricmp(HelperFactoryMap[i].pszName, pHelperInfo->GetName()))
		{
			CMapClass *pHelper = HelperFactoryMap[i].pfnFactory(pHelperInfo, pParent);
			return(pHelper);
		}
	}

	return(NULL);
}

