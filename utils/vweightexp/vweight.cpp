//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "MAX.H"
#include "DECOMP.H"
#include "STDMAT.H"
#include "ANIMTBL.H"
#include "istdplug.h"
#include "phyexp.h"
#include "BonesPro.h"

#include "vweightexprc.h"
#include "vweightexp.h"
#include "vweightimp.h"


// Save for use with dialogs
static HINSTANCE hInstance;

// We just need one of these to hand off to 3DSMAX.
static VWeightExportClassDesc VWeightExportCD;
static VWeightImportClassDesc VWeightImportCD;


//===================================================================
// Required plug-in export functions
//
BOOL WINAPI DllMain( HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved) 
{	
	static int fFirstTimeHere = TRUE;
	if (fFirstTimeHere)
	{
		fFirstTimeHere = FALSE;
		hInstance = hinstDLL;
	}
	return TRUE;
}

	
EXPORT_THIS int LibNumberClasses(void)
{
	return 2;
}

	
EXPORT_THIS ClassDesc *LibClassDesc(int iWhichClass)
{
	switch(iWhichClass)
	{
		case 0: return &VWeightExportCD;
		case 1: return &VWeightImportCD;
		default: return 0;
	}
}

	
EXPORT_THIS const TCHAR *LibDescription()
{
	return _T("Valve VVW Plug-in.");
}

	
EXPORT_THIS ULONG LibVersion()
{
	return VERSION_3DSMAX;
}



//===================================================================
// Utility functions
//

int AssertFailedFunc(char *sz)
{
	MessageBox(GetActiveWindow(), sz, "Assert failure", MB_OK);
	int Set_Your_Breakpoint_Here = 1;
	return 1;
}


//=================================================================
// Methods for CollectModelTEP
//
Modifier *FindPhysiqueModifier (INode *nodePtr)
{
	// Get object from node. Abort if no object.
	Object *ObjectPtr = nodePtr->GetObjectRef();
	if (!ObjectPtr) return NULL;

	// Is derived object ?
	if (ObjectPtr->SuperClassID() == GEN_DERIVOB_CLASS_ID)
	{
		// Yes -> Cast.
		IDerivedObject *DerivedObjectPtr = static_cast<IDerivedObject*>(ObjectPtr);

		// Iterate over all entries of the modifier stack.
		int ModStackIndex = 0;
		while (ModStackIndex < DerivedObjectPtr->NumModifiers())
		{
			// Get current modifier.
			Modifier *ModifierPtr = DerivedObjectPtr->GetModifier(ModStackIndex);

			// Is this Physique ?
			if (ModifierPtr->ClassID() == Class_ID(	PHYSIQUE_CLASS_ID_A, PHYSIQUE_CLASS_ID_B) )
			{
				// Yes -> Exit.
				return ModifierPtr;
			}
			// Next modifier stack entry.
			ModStackIndex++;
		}
	}
	// Not found.
	return NULL;
}

Modifier *FindBonesProModifier (INode *nodePtr)
{
	// Get object from node. Abort if no object.
	Object *ObjectPtr = nodePtr->GetObjectRef();
	if (!ObjectPtr) return NULL;

	// Is derived object ?
	if (ObjectPtr->SuperClassID() == GEN_DERIVOB_CLASS_ID)
	{
		// Yes -> Cast.
		IDerivedObject *DerivedObjectPtr = static_cast<IDerivedObject*>(ObjectPtr);

		// Iterate over all entries of the modifier stack.
		int ModStackIndex = 0;
		while (ModStackIndex < DerivedObjectPtr->NumModifiers())
		{
			// Get current modifier.
			Modifier *ModifierPtr = DerivedObjectPtr->GetModifier(ModStackIndex);

			// Is this Bones Pro OSM?
			if (ModifierPtr->ClassID() == BP_CLASS_ID_OSM )
			{
				// Yes -> Exit.
				return ModifierPtr;
			}
			// Is this Bones Pro WSM?
			if (ModifierPtr->ClassID() == BP_CLASS_ID_WSM )
			{
				// Yes -> Exit.
				return ModifierPtr;
			}
			// Next modifier stack entry.
			ModStackIndex++;
		}
	}
	// Not found.
	return NULL;
}



//========================================================================
// Utility functions for getting/setting the personal "node index" property.
// NOTE: I'm storing a string-property because I hit a 3DSMax bug in v1.2 when I
// NOTE: tried using an integer property.
// FURTHER NOTE: those properties seem to change randomly sometimes, so I'm
// implementing my own.

typedef struct
{
	char	szNodeName[MAX_NAME_CHARS];
} NAMEMAP;
const int MAX_NAMEMAP = 512;
static NAMEMAP g_NameMap[MAX_NAMEMAP];
static int g_cNameMap = 0;

void ResetINodeMap( void )
{
	g_cNameMap = 0;
}

int BuildINodeMap(INode *pnode)
{
	if (!FUndesirableNode(pnode))
	{
		AddINode(pnode);
	}

	// For each child of this node, we recurse into ourselves
	// until no more children are found.
	for (int c = 0; c < pnode->NumberOfChildren(); c++)
	{
		BuildINodeMap(pnode->GetChildNode(c));
	}
	
	return g_cNameMap;
}


int GetIndexOfNodeName(char *szNodeName, BOOL fAssertPropExists)
{
	for (int inm = 0; inm < g_cNameMap; inm++)
	{
		if (FStrEq(g_NameMap[inm].szNodeName, szNodeName))
		{
			return inm;
		}
	}

	return -1;
}

int GetIndexOfINode( INode *pnode, BOOL fAssertPropExists )
{
	return GetIndexOfNodeName( pnode->GetName(), fAssertPropExists );
}
	
void AddINode( INode *pnode )
{
	TSTR strNodeName(pnode->GetName());
	for (int inm = 0; inm < g_cNameMap; inm++)
	{
		if (FStrEq(g_NameMap[inm].szNodeName, (char*)strNodeName))
		{
			return;
		}
	}

	ASSERT_MBOX(g_cNameMap < MAX_NAMEMAP, "NAMEMAP is full");
	strcpy(g_NameMap[g_cNameMap++].szNodeName, (char*)strNodeName);
}


//=============================================================
// Returns TRUE if a node should be ignored during tree traversal.
//
BOOL FUndesirableNode(INode *pnode)
{
	// Get Node's underlying object, and object class name
	Object *pobj = pnode->GetObjectRef();

	if (!pobj)
		return TRUE;
	// Don't care about lights, dummies, and cameras
	if (pobj->SuperClassID() == CAMERA_CLASS_ID)
		return TRUE;
	if (pobj->SuperClassID() == LIGHT_CLASS_ID)
		return TRUE;
	if (!strstr(pnode->GetName(), "Bip01" ))
		return TRUE;

	return FALSE;

	// Actually, if it's not selected, pretend it doesn't exist!
	//if (!pnode->Selected())
	//	return TRUE;
	//return FALSE;
}


//=============================================================
// Returns TRUE if a node has been marked as skippable
//
BOOL FNodeMarkedToSkip(INode *pnode)
{
	return (::GetIndexOfINode(pnode) == UNDESIRABLE_NODE_MARKER);
}

	

//=============================================================
// gets a weighted value for the current system of nodes
//
void GetUnifiedCoord( Point3 &p1, MaxVertWeight pweight[], MaxNode maxNode[], int cMaxNode )
{
	float flMin = 1E20f;
	for (int iNode = 0; iNode < cMaxNode; iNode++)
	{
		float f = (p1 - maxNode[iNode].mat3NodeTM.GetTrans()).LengthSquared();
		if (f > 0 && f < flMin)
			flMin = f;
		pweight[iNode].flDist = f;
	}

	float flTotal = 0;
	float flInvMin = 1.0 / flMin;
	for (iNode = 0; iNode < cMaxNode; iNode++)
	{
		if (pweight[iNode].flDist > 0)
		{
			pweight[iNode].flDist = flInvMin / pweight[iNode].flDist;
		}
		else
		{
			pweight[iNode].flDist = 10.0;
		}
		flTotal += pweight[iNode].flDist;
	}

	float flInvTotal;
	if (flTotal > 0)
	{
		flInvTotal = 1.0 / flTotal;
	}
	else
	{
		flInvTotal = 1.0;
	}

	for (iNode = 0; iNode < cMaxNode; iNode++)
	{
		pweight[iNode].flDist = pweight[iNode].flDist * flInvTotal;
		// fprintf(m_pfile, "%8.4f ", pweight[iNode].flDist );
	}
}


int GetBoneWeights( IPhyContextExport *mcExport, int iVertex, MaxVertWeight *pweight)
{
	float fTotal = 0;
	IPhyVertexExport *vtxExport = mcExport->GetVertexInterface(iVertex);

	if (vtxExport)
	{
		if (vtxExport->GetVertexType() & BLENDED_TYPE)
		{
			IPhyBlendedRigidVertex *pBlend = ((IPhyBlendedRigidVertex *)vtxExport);

			for (int i = 0; i < pBlend->GetNumberNodes(); i++)
			{
				int index = GetIndexOfINode( pBlend->GetNode( i ) );
				if (index >= 0)
				{
					pweight[index].flWeight = pBlend->GetWeight( i );
					fTotal += pweight[index].flWeight;
				}
			}
		}
		else 
		{
			INode *Bone = ((IPhyRigidVertex *)vtxExport)->GetNode();
			int index = GetIndexOfINode(Bone);
			if (index >= 0)
			{
				pweight[index].flWeight = 100.0;
			}
		}
		mcExport->ReleaseVertexInterface(vtxExport);
	}
	return (fTotal > 0);
}


int GetBoneWeights( Modifier * bonesProMod, int iVertex, MaxVertWeight *pweight)
{
	int iTotal = 0;
	int nb = bonesProMod->SetProperty( BP_PROPID_GET_N_BONES, NULL );

	for ( int iBone = 0; iBone < nb; iBone++)
	{
		BonesPro_BoneVertex bv;
		bv.bindex = iBone;
		bv.vindex = iVertex;
		bonesProMod->SetProperty( BP_PROPID_GET_BV, &bv );

		if (bv.included > 0 && bv.forced_weight >= 0)
		{
			BonesPro_Bone bone;
			bone.t = BP_TIME_ATTACHED;
			bone.index = iBone;
			bonesProMod->SetProperty( BP_PROPID_GET_BONE_STAT, &bone );

			if (bone.node != NULL)
			{
				int index = GetIndexOfINode( bone.node );

				if (index >= 0)
				{
					pweight[index].flWeight = bv.forced_weight;
					iTotal++;
				}
			}
		}
	}
	return iTotal;
}


void SetBoneWeights( Modifier * bonesProMod, int iVertex, MaxVertWeight *pweight)
{
	int nb = bonesProMod->SetProperty( BP_PROPID_GET_N_BONES, NULL );

	// FILE *fp = fopen("bone2.txt", "w");

	for ( int iBone = 0; iBone < nb; iBone++)
	{
		BonesPro_Bone bone;
		bone.t = BP_TIME_ATTACHED;
		bone.index = iBone;
		bonesProMod->SetProperty( BP_PROPID_GET_BONE_STAT, &bone );

		/*
		if (GetIndexOfINode( bone.node ) >= 0)
		{
			fprintf( fp, "\"%s\" %d\n", bone.name, GetIndexOfINode( bone.node ) );
		}
		else
		{
			fprintf( fp, "\"%s\"\n", bone.name );
		}
		*/

		BonesPro_BoneVertex bv;
		bv.bindex = iBone;
		bv.vindex = iVertex;
		bv.included = 0;
		bv.forced_weight = -1;

		if (bone.node != NULL)
		{
			int index = GetIndexOfINode( bone.node );

			if (index >= 0 && pweight[index].flWeight >= 0)
			{
				bv.included = 1;
				bv.forced_weight = pweight[index].flWeight;
			}
		}
		bonesProMod->SetProperty( BP_PROPID_SET_BV, &bv );
	}
	//fclose(fp);
	//exit(1);
}

