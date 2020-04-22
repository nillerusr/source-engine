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
#include "vtaexprc.h"
#include "vtadefs.h"



//===================================================================
// Prototype declarations
//
int GetIndexOfINode(INode *pnode,BOOL fAssertPropExists = TRUE);
void SetIndexOfINode(INode *pnode, int inode);
BOOL FUndesirableNode(INode *pnode);
BOOL FNodeMarkedToSkip(INode *pnode);
float FlReduceRotation(float fl);


//===================================================================
// Global variable definitions
//

// Save for use with dialogs
static HINSTANCE hInstance;

// We just need one of these to hand off to 3DSMAX.
static VtaExportClassDesc VtaExportCD;

// For OutputDebugString and misc sprintf's
static char st_szDBG[300];

// INode mapping table
static int g_inmMac = 0;

//===================================================================
// Utility functions
//

static int AssertFailedFunc(char *sz)
{
	MessageBox(GetActiveWindow(), sz, "Assert failure", MB_OK);
	int Set_Your_Breakpoint_Here = 1;
	return 1;
}
#define ASSERT_MBOX(f, sz) ((f) ? 1 : AssertFailedFunc(sz))


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
	return 1;
}

	
EXPORT_THIS ClassDesc *LibClassDesc(int iWhichClass)
{
	switch(iWhichClass)
	{
		case 0: return &VtaExportCD;
		default: return 0;
	}
}

	
EXPORT_THIS const TCHAR *LibDescription()
{
	return _T("Valve VTA Plug-in.");
}

	
EXPORT_THIS ULONG LibVersion()
{
	return VERSION_3DSMAX;
}


//=====================================================================
// Methods for VtaExportClass
//

CONSTRUCTOR VtaExportClass::VtaExportClass(void)
{
	m_rgmaxnode		= NULL;
}


DESTRUCTOR VtaExportClass::~VtaExportClass(void)
{
	if (m_rgmaxnode)
		delete[] m_rgmaxnode;
}


int VtaExportClass::DoExport(const TCHAR *name,ExpInterface *ei,Interface *i, BOOL suppressPrompts, DWORD options) 
{
	ExpInterface	*pexpiface = ei;	// Hungarian
	Interface		*piface = i;		// Hungarian
	
	// Reset the name-map property manager
	g_inmMac = 0;

	// Break up filename, re-assemble longer versions
	TSTR strPath, strFile, strExt;
	TCHAR szFile[MAX_PATH];
	SplitFilename(TSTR(name), &strPath, &strFile, &strExt);
		sprintf(szFile,  "%s\\%s.%s",  (char*)strPath, (char*)strFile, DEFAULT_EXT);

	FILE *pFile;
	if ((pFile = fopen(szFile, "w")) == NULL)
		return FALSE/*failure*/;

	fprintf( pFile, "version %d\n", 1 );

	// Get animation metrics
	m_intervalOfAnimation = piface->GetAnimRange();
	m_tvStart = m_intervalOfAnimation.Start();
	m_tvEnd = m_intervalOfAnimation.End();
	m_tpf = ::GetTicksPerFrame();

	// Count nodes, label them, collect into array
	if (!CollectNodes(pexpiface))
		return 0;	/*fail*/
	
	// Output nodes
	if (!DumpBones(pFile, pexpiface))
	{
		fclose( pFile );
		return 0;	/*fail*/
	}

	// Output bone rotations, for each frame. Do only first frame if this is the reference frame MAX file
	DumpRotations(pFile, pexpiface);

	// Output triangle meshes (first frame/all frames), if this is the reference frame MAX file
	DumpModel(pFile, pexpiface);

	// Tell user that exporting is finished (it can take a while with no feedback)
	char szExportComplete[300];
	sprintf(szExportComplete, "Exported %s.", szFile);
	MessageBox(GetActiveWindow(), szExportComplete, "Status", MB_OK);

	fclose( pFile );

	return 1/*success*/;
}
	
	
BOOL VtaExportClass::CollectNodes( ExpInterface *pexpiface)
{
	// Count total nodes in the model, so I can alloc array
	// Also "brands" each node with node index, or with "skip me" marker.
	CountNodesTEP procCountNodes;
	procCountNodes.m_cNodes = 0;
	(void) pexpiface->theScene->EnumTree(&procCountNodes);
	ASSERT_MBOX(procCountNodes.m_cNodes > 0, "No nodes!");

	// Alloc and fill array
	m_imaxnodeMac = procCountNodes.m_cNodes;
	m_rgmaxnode = new MaxNode[m_imaxnodeMac];
	ASSERT_MBOX(m_rgmaxnode != NULL, "new failed");


	CollectNodesTEP procCollectNodes;
	procCollectNodes.m_phec = this;
	(void) pexpiface->theScene->EnumTree(&procCollectNodes);
	
	return TRUE;
}


BOOL VtaExportClass::DumpBones(FILE *pFile, ExpInterface *pexpiface)
{
	// Dump bone names
	DumpNodesTEP procDumpNodes;
	procDumpNodes.m_pfile = pFile;
	procDumpNodes.m_phec = this;
	fprintf(pFile, "nodes\n" );
	(void) pexpiface->theScene->EnumTree(&procDumpNodes);
	fprintf(pFile, "end\n" );

	return TRUE;
}

	
BOOL VtaExportClass::DumpRotations(FILE *pFile, ExpInterface *pexpiface)
{
	// Dump bone-rotation info, for each frame
	// Also dumps root-node translation info (the model's world-position at each frame)
	DumpFrameRotationsTEP procDumpFrameRotations;
	procDumpFrameRotations.m_pfile	= pFile;
	procDumpFrameRotations.m_phec	= this;

	fprintf(pFile, "skeleton\n" );
	for (TimeValue tv = m_tvStart; tv <= m_tvEnd; tv += m_tpf)
	{
		fprintf(pFile, "time %d\n", tv / GetTicksPerFrame() );
		procDumpFrameRotations.m_tvToDump = tv;
		(void) pexpiface->theScene->EnumTree(&procDumpFrameRotations);
	}
	fprintf(pFile, "end\n" );

	return TRUE;
}
	
	
BOOL VtaExportClass::DumpModel( FILE *pFile, ExpInterface *pexpiface)
{
	// Dump mesh info: vertices, normals, UV texture map coords, bone assignments
	DumpModelTEP procDumpModel;
	procDumpModel.m_pfile	= pFile;
	procDumpModel.m_phec	= this;

	procDumpModel.m_tvBase = m_tvStart;

	fprintf(pFile, "vertexanimation\n" );
	procDumpModel.m_baseVert = NULL;
	for (TimeValue tv = m_tvStart; tv <= m_tvEnd; tv += m_tpf)
	{
		fprintf(pFile, "time %d\n", tv / GetTicksPerFrame() );
		procDumpModel.m_tvToDump = tv;
		procDumpModel.m_baseVertCount = 0;
		(void) pexpiface->theScene->EnumTree(&procDumpModel);
	}
	fprintf(pFile, "end\n" );

	return TRUE;
}	




//=============================================================================
//							TREE-ENUMERATION PROCEDURES
//=============================================================================

#define ASSERT_AND_ABORT(f, sz)							\
	if (!(f))											\
	{													\
		ASSERT_MBOX(FALSE, sz);							\
		/* cleanup( ); */								\
		return TREE_ABORT;								\
	}


//=================================================================
// Methods for CountNodesTEP
//
int CountNodesTEP::callback( INode *node)
{
	INode *pnode = node; // Hungarian

	ASSERT_MBOX(!(pnode)->IsRootNode(), "Encountered a root node!");

	if (::FUndesirableNode(pnode))
	{
		// Mark as skippable
		::SetIndexOfINode(pnode, VtaExportClass::UNDESIRABLE_NODE_MARKER);
		return TREE_CONTINUE;
	}
	
	// Establish "node index"--just ascending ints
	::SetIndexOfINode(pnode, m_cNodes);

	m_cNodes++;
	
	return TREE_CONTINUE;
}


//=================================================================
// Methods for CollectNodesTEP
//
int CollectNodesTEP::callback(INode *node)
{
	INode *pnode = node; // Hungarian

	ASSERT_MBOX(!(pnode)->IsRootNode(), "Encountered a root node!");

	if (::FNodeMarkedToSkip(pnode))
		return TREE_CONTINUE;

	// Get pre-stored "index"
	int iNode = ::GetIndexOfINode(pnode);
	ASSERT_MBOX(iNode >= 0 && iNode <= m_phec->m_imaxnodeMac-1, "Bogus iNode");
	
	// Get name, store name in array
	TSTR strNodeName(pnode->GetName());
	strcpy(m_phec->m_rgmaxnode[iNode].szNodeName, (char*)strNodeName);

	// Get Node's time-zero Transformation Matrices
	m_phec->m_rgmaxnode[iNode].mat3NodeTM		= pnode->GetNodeTM(0/*TimeValue*/);
	m_phec->m_rgmaxnode[iNode].mat3ObjectTM		= pnode->GetObjectTM(0/*TimeValue*/);

	// I'll calculate this later
	m_phec->m_rgmaxnode[iNode].imaxnodeParent	= VtaExportClass::UNDESIRABLE_NODE_MARKER;

	return TREE_CONTINUE;
}






//=================================================================
// Methods for DumpNodesTEP
//
int DumpNodesTEP::callback(INode *pnode)
{
	ASSERT_MBOX(!(pnode)->IsRootNode(), "Encountered a root node!");

	if (::FNodeMarkedToSkip(pnode))
		return TREE_CONTINUE;

	// Get node's parent
	INode *pnodeParent;
	pnodeParent = pnode->GetParentNode();
	
	// The model's root is a child of the real "scene root"
	TSTR strNodeName(pnode->GetName());
	BOOL fNodeIsRoot = pnodeParent->IsRootNode( );
	
	int iNode = ::GetIndexOfINode(pnode);
	int iNodeParent = ::GetIndexOfINode(pnodeParent, !fNodeIsRoot/*fAssertPropExists*/);

	// Convenient time to cache this
	m_phec->m_rgmaxnode[iNode].imaxnodeParent = fNodeIsRoot ? VtaExportClass::UNDESIRABLE_NODE_MARKER : iNodeParent;

	// Root node has no parent, thus no translation
	if (fNodeIsRoot)
		iNodeParent = -1;
		
	// Dump node description
	fprintf(m_pfile, "%3d \"%s\" %3d\n", 
		iNode, 
		strNodeName, 
		iNodeParent );

	return TREE_CONTINUE;
}



//=================================================================
// Methods for DumpFrameRotationsTEP
//
int DumpFrameRotationsTEP::callback(INode *pnode)
{
	ASSERT_MBOX(!(pnode)->IsRootNode(), "Encountered a root node!");

	if (::FNodeMarkedToSkip(pnode))
		return TREE_CONTINUE;

	int iNode = ::GetIndexOfINode(pnode);

	TSTR strNodeName(pnode->GetName());

	// The model's root is a child of the real "scene root"
	INode *pnodeParent = pnode->GetParentNode();
	BOOL fNodeIsRoot = pnodeParent->IsRootNode( );

	// Get Node's "Local" Transformation Matrix
	Matrix3 mat3NodeTM		= pnode->GetNodeTM(m_tvToDump);
	Matrix3 mat3ParentTM	= pnodeParent->GetNodeTM(m_tvToDump);
	mat3NodeTM.NoScale();		// Clear these out because they apparently
	mat3ParentTM.NoScale();		// screw up the following calculation.
	Matrix3 mat3NodeLocalTM	= mat3NodeTM * Inverse(mat3ParentTM);
	Point3 rowTrans = mat3NodeLocalTM.GetTrans();

	// Get the rotation (via decomposition into "affine parts", then quaternion-to-Euler)
	// Apparently the order of rotations returned by QuatToEuler() is X, then Y, then Z.
	AffineParts affparts;
	float rgflXYZRotations[3];

	decomp_affine(mat3NodeLocalTM, &affparts);
	QuatToEuler(affparts.q, rgflXYZRotations);

	float xRot = rgflXYZRotations[0];		// in radians
	float yRot = rgflXYZRotations[1];		// in radians
	float zRot = rgflXYZRotations[2];		// in radians

	// Get rotations in the -2pi...2pi range
	xRot = ::FlReduceRotation(xRot);
	yRot = ::FlReduceRotation(yRot);
	zRot = ::FlReduceRotation(zRot);
	
	// Print rotations
	//fprintf(m_pfile, "%3d %8.4f %8.4f %8.4f %8.4f %8.4f %8.4f\n", 
	fprintf(m_pfile, "%3d %f %f %f %f %f %f\n", 
			// Node:%-15s Rotation (x,y,z)\n",
			iNode, rowTrans.x, rowTrans.y, rowTrans.z, xRot, yRot, zRot);

	return TREE_CONTINUE;
}



//=================================================================
// Methods for DumpModelTEP
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


// #define DEBUG_MESH_DUMP

//=================================================================
// Methods for DumpModelTEP
//
int DumpModelTEP::callback(INode *pnode)
{
	ASSERT_MBOX(!(pnode)->IsRootNode(), "Encountered a root node!");

	if (::FNodeMarkedToSkip(pnode))
		return TREE_CONTINUE;
	
	if ( !pnode->Selected())
		return TREE_CONTINUE;

	int iNode = ::GetIndexOfINode(pnode);
	TSTR strNodeName(pnode->GetName());
	
	// The Footsteps node apparently MUST have a dummy mesh attached!  Ignore it explicitly.
	if (FStrEq((char*)strNodeName, "Bip01 Footsteps"))
		return TREE_CONTINUE;

	// Helper nodes don't have meshes
	Object *pobj = pnode->GetObjectRef();
	if (pobj->SuperClassID() == HELPER_CLASS_ID)
		return TREE_CONTINUE;


   // Get the object's parameter block if it has one 

    IParamBlock *pb = NULL;
    IParamArray *pa = pobj->GetParamBlock(); 

	if (!pa) 
	{
		int i = pobj->NumRefs();
		// Search the references looking for a parameter block 
		for (i = 0; i < pobj->NumRefs(); i++) 
		{
			RefTargetHandle r = pobj->GetReference(i); 
			if (r)
			{
				Class_ID x = r->ClassID();

				if (r && r->ClassID() == Class_ID(PARAMETER_BLOCK_CLASS_ID,0)) 
				{
					pb = (IParamBlock *) r;
				}
			}
		}
	}
	else 
	{
		// pb = pa->GetParamBlock2();
	}

	if (pb)
	{
	    // Find out how many _animatable_ parameters there are 
        int count = pb->NumSubs();

        // Display data about each one
        for (int i = 0; i < count; i++)
		{
			TSTR name = pb->SubAnimName(i);

			int pbIndex = pb->AnimNumToParamNum(i);

			TSTR cname;

			SClass_ID sc = pb->GetAnimParamControlType(i); 

			if (sc == CTRL_FLOAT_CLASS_ID) 

			cname = TSTR(_T("Float"));

			else if (sc == CTRL_POINT3_CLASS_ID) 

			cname = TSTR(_T("Point3"));
		}
	}



	// Get Node's object, convert to a triangle-mesh object, so I can access the Faces
	ObjectState os = pnode->EvalWorldState(m_tvToDump);
	pobj = os.obj;

	// Shouldn't have gotten this far if it's a helper object
	if (pobj->SuperClassID() == HELPER_CLASS_ID)
	{
		sprintf(st_szDBG, "ERROR--Helper node %s has an attached mesh, and it shouldn't.", (char*)strNodeName);
		ASSERT_AND_ABORT(FALSE, st_szDBG);
	}

	// convert mesh to triobject
	if (!pobj->CanConvertToType(triObjectClassID))
		return TREE_CONTINUE;
	TriObject *ptriobj = (TriObject*)pobj->ConvertToType(m_tvToDump, triObjectClassID);

	if (ptriobj == NULL)
		return TREE_CONTINUE;

	Mesh *pmesh = &ptriobj->mesh;
	BOOL deleteMesh = (ptriobj != pobj);

	// Ensure that the vertex normals are up-to-date
	pmesh->buildNormals();

	// We want the vertex coordinates in World-space, not object-space
	Matrix3 mat3ObjectTM = pnode->GetObjectTM(m_tvToDump);
	Matrix3 mat3ObjectNTM = mat3ObjectTM;
	mat3ObjectNTM.NoScale( );

	int cVerts = pmesh->getNumVerts();

	// it would be nice to just evaluate the object for both time periods, but MAX
	// may do EvalWorldState in place, so the vertex animations get stomped
	if (m_tvToDump == m_tvBase)
	{
		if (m_baseVert)
			m_baseVert = (Point3 *)realloc( m_baseVert, (m_baseVertCount + cVerts) * sizeof( Point3 ) );
		else
			m_baseVert = (Point3 *)malloc( (m_baseVertCount + cVerts) * sizeof( Point3 ) );

		for (int iVert = 0; iVert < cVerts; iVert++)
		{
			Point3 pt3Vertex1 = pmesh->getVert(iVert);
			int iAdjVert = m_baseVertCount + iVert;

			m_baseVert[iAdjVert] = pt3Vertex1 * mat3ObjectTM;

			Point3 pt3Normal1 = pmesh->getNormal(iVert);
			pt3Normal1 = VectorTransform( mat3ObjectNTM, pt3Normal1 );

			fprintf(m_pfile, "%5d %8.4f %8.4f %8.4f  %9.6f %9.6f %9.6f\n",
					iAdjVert, m_baseVert[iAdjVert].x, m_baseVert[iAdjVert].y, m_baseVert[iAdjVert].z,
					pt3Normal1.x, pt3Normal1.y, pt3Normal1.z );

		}
	}
	else
	{
		for (int iVert = 0; iVert < cVerts; iVert++)
		{
			Point3 pt3Vertex1 = pmesh->getVert(iVert);
			Point3 v1 = pt3Vertex1 * mat3ObjectTM;

			int iAdjVert = m_baseVertCount + iVert;

			if (Length( m_baseVert[iAdjVert] - v1) > 0.01)
			{
				Point3 pt3Vertex2 = pt3Vertex1;
				pt3Vertex2.z = pt3Vertex2.z + 10.0;
				pmesh->setVert( iVert, pt3Vertex2 );
				
				Point3 pt3Normal1 = pmesh->getNormal(iVert);
				pt3Normal1 = VectorTransform( mat3ObjectNTM, pt3Normal1 );

				fprintf(m_pfile, "%5d %8.4f %8.4f %8.4f  %9.6f %9.6f %9.6f\n",
						iAdjVert, v1.x, v1.y, v1.z, 
						pt3Normal1.x, pt3Normal1.y, pt3Normal1.z );
			}
		}
	}
	m_baseVertCount += cVerts;

	fflush( m_pfile );

	/*
	if (deleteMesh)
		delete pmesh;
	*/

	return TREE_CONTINUE;
}



Point3 DumpModelTEP::Pt3GetRVertexNormal(RVertex *prvertex, DWORD smGroupFace)
{
	// Lookup the appropriate vertex normal, based on smoothing group.
	int cNormals = prvertex->rFlags & NORCT_MASK;

	ASSERT_MBOX((cNormals == 1 && prvertex->ern == NULL) ||
				(cNormals > 1 && prvertex->ern != NULL), "BOGUS RVERTEX");
	
	if (cNormals == 1)
		return prvertex->rn.getNormal();
	else
	{
		for (int irn = 0; irn < cNormals; irn++)
			if (prvertex->ern[irn].getSmGroup() & smGroupFace)
				break;

		if (irn >= cNormals) 
		{
			irn = 0;
			// ASSERT_MBOX(irn < cNormals, "unknown smoothing group\n");
		}
		return prvertex->ern[irn].getNormal();
	}
}


DumpModelTEP::~DumpModelTEP()
{
	if (m_baseVert)
	{
		delete m_baseVert;
	}
}


//========================================================================
// Utility functions for getting/setting the personal "node index" property.
// NOTE: I'm storing a string-property because I hit a 3DSMax bug in v1.2 when I
// NOTE: tried using an integer property.
// FURTHER NOTE: those properties seem to change randomly sometimes, so I'm
// implementing my own.

typedef struct
{
	char	szNodeName[VtaExportClass::MAX_NAME_CHARS];
	int		iNode;
} NAMEMAP;
const int MAX_NAMEMAP = 512;
static NAMEMAP g_rgnm[MAX_NAMEMAP];

int GetIndexOfINode(INode *pnode, BOOL fAssertPropExists)
{
	TSTR strNodeName(pnode->GetName());
	for (int inm = 0; inm < g_inmMac; inm++)
		if (FStrEq(g_rgnm[inm].szNodeName, (char*)strNodeName))
			return g_rgnm[inm].iNode;
	if (fAssertPropExists)
		ASSERT_MBOX(FALSE, "No NODEINDEXSTR property");
	return -7777;
}

	
void SetIndexOfINode(INode *pnode, int inode)
{
	TSTR strNodeName(pnode->GetName());
	NAMEMAP *pnm;
	for (int inm = 0; inm < g_inmMac; inm++)
		if (FStrEq(g_rgnm[inm].szNodeName, (char*)strNodeName))
			break;
	if (inm < g_inmMac)
		pnm = &g_rgnm[inm];
	else
	{
		ASSERT_MBOX(g_inmMac < MAX_NAMEMAP, "NAMEMAP is full");
		pnm = &g_rgnm[g_inmMac++];
		strcpy(pnm->szNodeName, (char*)strNodeName);
	}
	pnm->iNode = inode;
}


//=============================================================
// Returns TRUE if a node should be ignored during tree traversal.
//
BOOL FUndesirableNode(INode *pnode)
{
	// Get Node's underlying object, and object class name
	Object *pobj = pnode->GetObjectRef();

	// Don't care about lights, dummies, and cameras
	if (pobj->SuperClassID() == CAMERA_CLASS_ID)
		return TRUE;
	if (pobj->SuperClassID() == LIGHT_CLASS_ID)
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
	return (::GetIndexOfINode(pnode) == VtaExportClass::UNDESIRABLE_NODE_MARKER);
}

	
//=============================================================
// Reduces a rotation to within the -2PI..2PI range.
//
static float FlReduceRotation(float fl)
{
	while (fl >= TWOPI)
		fl -= TWOPI;
	while (fl <= -TWOPI)
		fl += TWOPI;
	return fl;
}
