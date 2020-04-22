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


//===================================================================
// Global variable definitions
//

// For OutputDebugString and misc sprintf's
static char st_szDBG[8193];



//=====================================================================
// Methods for VWeightImportClass
//

CONSTRUCTOR VWeightImportClass::VWeightImportClass(void)
{
	m_cSrcMaxNodes	= 0;
	m_cSrcMaxVertex	= 0;
	m_cMaxNodes = 0;
}


DESTRUCTOR VWeightImportClass::~VWeightImportClass(void)
{
	for (int i = 0; i < m_cSrcMaxVertex; i++)
	{
		delete[] m_SrcMaxVertex[i];
	}
}


int VWeightImportClass::DoImport(const TCHAR *name, ImpInterface *ii, Interface *pi, BOOL suppressPrompts) 
{
	ImpInterface	*pimpiface = ii;
	Interface		*piface = pi;
	
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
	m_cMaxNodes = BuildINodeMap( ip->GetRootNode() );

	CollectNodes( ip->GetRootNode() );

	// read src data
	FILE *pFile;
	if ((pFile = fopen(szFile, "rb")) == NULL)
		return FALSE/*failure*/;

	int version = 1;
	fread( &version, 1, sizeof( int ), pFile );

	int i, j;

	fread( &m_cSrcMaxNodes, 1, sizeof( int ), pFile );
	fread( &m_cSrcMaxVertex, 1, sizeof( int ), pFile );

	for (i = 0; i < m_cSrcMaxNodes; i++)
	{
		fread( &m_SrcMaxNode[i], 1, sizeof(m_SrcMaxNode[i]), pFile );
	}

	int cNodes = (m_cSrcMaxNodes > m_cMaxNodes) ? m_cSrcMaxNodes : m_cMaxNodes;
	for (j = 0; j < m_cSrcMaxVertex; j++)
	{
		m_SrcMaxVertex[j] = new MaxVertWeight [cNodes];
		fread( m_SrcMaxVertex[j], m_cSrcMaxNodes, sizeof(MaxVertWeight), pFile );
	}

	fclose( pFile );

	RemapSrcWeights( );
	
	NodeEnum( ip->GetSelNode( 0 ) );

	// Tell user that exporting is finished (it can take a while with no feedback)
	char szExportComplete[300];
	sprintf(szExportComplete, "Imported %s.", szFile);
	MessageBox(GetActiveWindow(), szExportComplete, "Status", MB_OK);

	return 1/*success*/;
}
	

void VWeightImportClass::CollectNodes( INode *pnode )
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
	
	
void VWeightImportClass::NodeEnum( INode *node )
{
#if 0
	// For each child of this node, we recurse into ourselves
	// until no more children are found.
	for (int c = 0; c < node->NumberOfChildren(); c++)
	{
		NodeEnum(node->GetChildNode(c));
	}
#endif

	UpdateModel( node );
}

void VWeightImportClass::RemapSrcWeights( )
{
	int index;

	for (index = 0; index < m_cMaxNodes; index++)
	{
		m_iToSrc[index] = -1;
	}

	for (int iSrc = 0; iSrc < m_cSrcMaxNodes; iSrc++)
	{
		index = GetIndexOfNodeName( (char*) m_SrcMaxNode[iSrc].szNodeName );
		if (index >= 0)
		{
			m_iToSrc[index] = iSrc;
		}
		else
		{
			iSrc = iSrc;
		}
	}

	MaxVertWeight flTmp[512];
	for (index = 0; index < m_cMaxNodes; index++)
	{
		flTmp[index].flDist = 0.0;
		flTmp[index].flWeight = 0.0;
	}

	for (int iVert = 0; iVert < m_cSrcMaxVertex; iVert++)
	{
		for (index = 0; index < m_cMaxNodes; index++)
		{
			if (m_iToSrc[index] >= 0)
			{
				flTmp[index] = m_SrcMaxVertex[iVert][m_iToSrc[index]];
			}
		}
		for (index = 0; index < m_cMaxNodes; index++)
		{
			m_SrcMaxVertex[iVert][index] = flTmp[index];
		}
	}
}


int VWeightImportClass::UpdateModel(INode *pnode)
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
	ObjectState os = pnode->EvalWorldState(m_tvStart);
	pobj = os.obj;

	// Shouldn't have gotten this far if it's a helper object
	if (pobj->SuperClassID() == HELPER_CLASS_ID)
	{
		return TREE_CONTINUE;
	}

	// convert mesh to triobject
	if (!pobj->CanConvertToType(triObjectClassID))
		return TREE_CONTINUE;
	TriObject *ptriobj = (TriObject*)pobj->ConvertToType(m_tvStart, triObjectClassID);

	if (ptriobj == NULL)
		return TREE_CONTINUE;

	Mesh *pmesh = &ptriobj->mesh;

	// We want the vertex coordinates in World-space, not object-space
	Matrix3 mat3ObjectTM = pnode->GetObjectTM(m_tvStart);

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

	MaxVertWeight mvTmp[512];

	for (int iVert = 0; iVert < cVerts; iVert++)
	{
		Point3 pt3Vertex1 = pmesh->getVert(iVert);
		Point3 v1 = pt3Vertex1 * mat3ObjectTM;

		if (v1.x < -24.0)
		{
			v1 = v1;
		}

		GetUnifiedCoord( v1, mvTmp, m_MaxNode, m_cMaxNodes );
		GetBoneWeights( m_bonesProMod, iVert, mvTmp );

		int iClosest = -1;
		float flMinDist = 1E30f;
		for (int iVert2 = 0; iVert2 < m_cSrcMaxVertex; iVert2++)
		{
			float flDist = 0;
			for (int index = 0; index < m_cMaxNodes && flDist < flMinDist; index++)
			{
				if (m_iToSrc[index]>=0)
				{
					float tmp = mvTmp[index].flDist - m_SrcMaxVertex[iVert2][index].flDist;
					flDist += (tmp * tmp);
				}
			}
			if (flDist < flMinDist)
			{
				iClosest = iVert2;
				flMinDist = flDist;
			}
		}

		if (iClosest != -1)
		{
			SetBoneWeights( m_bonesProMod, iVert, m_SrcMaxVertex[iClosest] );
		}
	}

	if (m_bonesProMod)
	{
		m_bonesProMod->SetProperty ( BP_PROPID_REFRESH, NULL );
	}
	// fflush( m_pfile );

	return TREE_CONTINUE;
}



void UpdateModelTEP::cleanup(void)
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


