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


//===================================================================
// Global variable definitions
//

// For OutputDebugString and misc sprintf's
static char st_szDBG[300];



//=====================================================================
// Methods for VWeightExportClass
//

CONSTRUCTOR VWeightExportClass::VWeightExportClass(void)
{
	m_cMaxNode	= 0;
	m_cMaxVertex	= 0;
}


DESTRUCTOR VWeightExportClass::~VWeightExportClass(void)
{
	for (int i = 0; i < m_cMaxVertex; i++)
	{
		delete[] m_MaxVertex[i];
	}
}


int VWeightExportClass::DoExport(const TCHAR *name, ExpInterface *ei, Interface *pi, BOOL suppressPrompts, DWORD options) 
{
	ExpInterface	*pexpiface = ei;	// Hungarian
	Interface		*piface = pi;		// Hungarian
	
	// Reset the name-map property manager
	ResetINodeMap();

	// Break up filename, re-assemble longer versions
	TSTR strPath, strFile, strExt;
	TCHAR szFile[MAX_PATH];
	SplitFilename(TSTR(name), &strPath, &strFile, &strExt);
		sprintf(szFile,  "%s\\%s.%s",  (char*)strPath, (char*)strFile, DEFAULT_EXT);


	// Get animation metrics
	m_intervalOfAnimation = piface->GetAnimRange();
	m_tvStart = m_intervalOfAnimation.Start();
	m_tvEnd = m_intervalOfAnimation.End();
	m_tpf = ::GetTicksPerFrame();


	Interface *ip = GetCOREInterface();

	ResetINodeMap( );
	m_cMaxNode = BuildINodeMap( ip->GetRootNode() );

	// Count nodes, label them, collect into array
	CollectNodes( ip->GetRootNode() );

	CollectModel( pexpiface );

#if 1
	FILE *pFile;
	if ((pFile = fopen(szFile, "wb")) == NULL)
		return FALSE/*failure*/;

	int version = 1;
	fwrite( &version, 1, sizeof( int ), pFile );

	int i, j;

	fwrite( &m_cMaxNode, 1, sizeof( int ), pFile );
	fwrite( &m_cMaxVertex, 1, sizeof( int ), pFile );

	for (i = 0; i < m_cMaxNode; i++)
	{
		fwrite( &m_MaxNode[i], 1, sizeof(m_MaxNode[i]), pFile );
	}

	for (j = 0; j < m_cMaxVertex; j++)
	{
		fwrite( m_MaxVertex[j], m_cMaxNode, sizeof(MaxVertWeight), pFile );
	}

	fclose( pFile );
#else
	FILE *pFile;
	if ((pFile = fopen(szFile, "w")) == NULL)
		return FALSE/*failure*/;

	fprintf( pFile, "version %d\n", 1 );

	int i, j;

	fprintf(pFile, "%d\n", m_cMaxNode );
	fprintf(pFile, "%d\n", m_cMaxVertex );

	for (i = 0; i < m_cMaxNode; i++)
	{
		fprintf(pFile, "%5d \"%s\" %3d\n", 
			i, m_MaxNode[i].szNodeName, m_MaxNode[i].imaxnodeParent );
	}

	for (j = 0; j < m_cMaxVertex; j++)
	{
		fprintf( pFile, "%d ", j );

		for (int i = 0; i < m_cMaxNode; i++)
		{
			// if (strstr(m_MaxNode[i].szNodeName, "Bip01 R Finger"))
			// if (m_MaxNode[i].szNodeName[0] == 'D')
			{
				fprintf(pFile, " %5.3f", m_MaxVertex[j][i].flDist );
				fprintf(pFile, " %3.0f", m_MaxVertex[j][i].flWeight );
			}
		}
		fprintf(pFile, "\n" );
	}

	fclose( pFile );
#endif

	// Tell user that exporting is finished (it can take a while with no feedback)
	char szExportComplete[300];
	sprintf(szExportComplete, "Exported %s.", szFile);
	MessageBox(GetActiveWindow(), szExportComplete, "Status", MB_OK);


	return 1/*success*/;
}
	
	
void VWeightExportClass::CollectNodes( INode *pnode )
{
	// Get pre-stored "index"
	int index = ::GetIndexOfINode(pnode);

	if (index >= 0)
	{
		// Get name, store name in array
		TSTR strNodeName(pnode->GetName());
		strcpy(m_MaxNode[index].szNodeName, (char*)strNodeName);

		// Get Node's time-zero Transformation Matrices
		m_MaxNode[index].mat3NodeTM		= pnode->GetNodeTM(0);
		m_MaxNode[index].mat3ObjectTM	= pnode->GetObjectTM(0);
	}

	for (int c = 0; c < pnode->NumberOfChildren(); c++)
	{
		CollectNodes(pnode->GetChildNode(c));
	}

	return;
}

	
	
BOOL VWeightExportClass::CollectModel( ExpInterface *pexpiface)
{
	// Dump mesh info: vertices, normals, UV texture map coords, bone assignments
	CollectModelTEP procCollectModel;

	// init data
	m_cMaxVertex = 0;

	procCollectModel.m_phec	= this;
	//fprintf(pFile, "triangles\n" );
	procCollectModel.m_tvToDump = m_tvStart;
	(void) pexpiface->theScene->EnumTree(&procCollectModel);
	//fprintf(pFile, "end\n" );
	return TRUE;
}	


// #define DEBUG_MESH_DUMP

//=================================================================
// Methods for CollectModelTEP
//
int CollectModelTEP::callback(INode *pnode)
{
	if (::FNodeMarkedToSkip(pnode))
		return TREE_CONTINUE;

	if ( !pnode->Selected())
		return TREE_CONTINUE;

	// clear physique export parameters
	m_mcExport = NULL;
	m_phyExport = NULL;
    m_phyMod = NULL;
    m_bonesProMod = NULL;

	ASSERT_MBOX(!(pnode)->IsRootNode(), "Encountered a root node!");

	int iNode = ::GetIndexOfINode(pnode);
	TSTR strNodeName(pnode->GetName());
	
	// The Footsteps node apparently MUST have a dummy mesh attached!  Ignore it explicitly.
	if (FStrEq((char*)strNodeName, "Bip01 Footsteps"))
		return TREE_CONTINUE;

	// Helper nodes don't have meshes
	Object *pobj = pnode->GetObjectRef();
	if (pobj->SuperClassID() == HELPER_CLASS_ID)
		return TREE_CONTINUE;

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

	// We want the vertex coordinates in World-space, not object-space
	Matrix3 mat3ObjectTM = pnode->GetObjectTM(m_tvToDump);

	// initialize physique export parameters
    m_phyMod = FindPhysiqueModifier(pnode);
    if (m_phyMod)
	{
		// Physique Modifier exists for given Node
	    m_phyExport = (IPhysiqueExport *)m_phyMod->GetInterface(I_PHYINTERFACE);

        if (m_phyExport)
        {
            // create a ModContext Export Interface for the specific node of the Physique Modifier
           m_mcExport = (IPhyContextExport *)m_phyExport->GetContextInterface(pnode);

		   if (m_mcExport)
		   {
		       // convert all vertices to Rigid 
                m_mcExport->ConvertToRigid(TRUE);
		   }
		}
	}

	// initialize bones pro export parameters
	m_wa  = NULL;
	m_bonesProMod = FindBonesProModifier(pnode);
	if (m_bonesProMod)
	{
		m_bonesProMod->SetProperty( BP_PROPID_GET_WEIGHTS, &m_wa );
	}


	int cVerts = pmesh->getNumVerts();

	// Dump the triangle face info
	int cFaces = pmesh->getNumFaces();

	int *iUsed = new int[cVerts];

	for (int iVert = 0; iVert < cVerts; iVert++)
	{
		iUsed[iVert] = 0;
	}

	for (int iFace = 0; iFace < cFaces; iFace++)
	{
		if (pmesh->faces[iFace].flags & HAS_TVERTS)
		{
			iUsed[pmesh->faces[iFace].getVert(0)] = 1;
			iUsed[pmesh->faces[iFace].getVert(1)] = 1;
			iUsed[pmesh->faces[iFace].getVert(2)] = 1;
		}
	}

	for (iVert = 0; iVert < cVerts; iVert++)
	{
		MaxVertWeight *pweight = m_phec->m_MaxVertex[m_phec->m_cMaxVertex] = new MaxVertWeight [m_phec->m_cMaxNode];

		Point3 pt3Vertex1 = pmesh->getVert(iVert);
		Point3 v1 = pt3Vertex1 * mat3ObjectTM;

		GetUnifiedCoord( v1, pweight, m_phec->m_MaxNode, m_phec->m_cMaxNode );

		if (CollectWeights( iVert, pweight ))
		{
			m_phec->m_cMaxVertex++;
		}
	}	

	// fflush( m_pfile );

	return TREE_CONTINUE;
}


int CollectModelTEP::CollectWeights(int iVertex, MaxVertWeight *pweight)
{
	for (int index = 0; index < m_phec->m_cMaxNode; index++)
	{
		pweight[index].flWeight = -1;
	}

	if (m_mcExport)
	{
		return GetBoneWeights( m_mcExport, iVertex, pweight );
	}
	else
	{
		return GetBoneWeights( m_bonesProMod, iVertex, pweight );
	}
}

void CollectModelTEP::cleanup(void)
{
	if (m_phyMod && m_phyExport)
	{
		if (m_mcExport)
		{
			m_phyExport->ReleaseContextInterface(m_mcExport);
			m_mcExport = NULL;
        }
        m_phyMod->ReleaseInterface(I_PHYINTERFACE, m_phyExport);
		m_phyExport = NULL;
		m_phyMod = NULL;
	}
}

