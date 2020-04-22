//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "MapPath.h"
#include "hammer.h"
#include "EditPathDlg.h"
#include "MapEntity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


float GetFileVersion(void);


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapPath::CMapPath(void)
{
	m_iDirection = dirOneway;
	SetName("");
	SetClass("path_corner");
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapPath::~CMapPath(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapPathNode::CMapPathNode(void)
{
	bSelected = FALSE;
	szName[0] = 0;
}

CMapPathNode::CMapPathNode(const CMapPathNode& src)
{
	*this = src;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
// Output : CMapPathNode
//-----------------------------------------------------------------------------
CMapPathNode &CMapPathNode::operator=(const CMapPathNode &src)
{
	// we don't care.
	Q_strncpy( szName, src.szName, sizeof(szName) );
	bSelected = src.bSelected;
	kv.RemoveAll();
	for ( int i=src.kv.GetFirst(); i != src.kv.GetInvalidIndex(); i=src.kv.GetNext( i ) )
	{
		MDkeyvalue KeyValue = src.kv.GetKeyValue(i);
		kv.SetValue(KeyValue.szKey, KeyValue.szValue);
	}
	pos = src.pos;
	dwID = src.dwID;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwID - 
//			piIndex - 
// Output : CMapPathNode *
//-----------------------------------------------------------------------------
CMapPathNode *CMapPath::NodeForID(DWORD dwID, int* piIndex)
{
	for(int iNode = 0; iNode < m_Nodes.Count(); iNode++)
	{
		if(m_Nodes[iNode].dwID == dwID)
		{
			if(piIndex)
				piIndex[0] = iNode;
			return &m_Nodes[iNode];
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : DWORD
//-----------------------------------------------------------------------------
DWORD CMapPath::GetNewNodeID(void)
{
	DWORD dwNewID = 1;
	while(true)
	{
		int iNode;
		for(iNode = 0; iNode < m_Nodes.Count(); iNode++)
		{
			if(m_Nodes[iNode].dwID == dwNewID)
				break;
		}

		if(iNode == m_Nodes.Count())
			return dwNewID;

		++dwNewID;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwAfterID - 
//			vecPos - 
// Output : 
//-----------------------------------------------------------------------------
DWORD CMapPath::AddNode(DWORD dwAfterID, const Vector &vecPos)
{
	int iPos;

	if(dwAfterID == ADD_START)
		iPos = 0;
	else if(dwAfterID == ADD_END)
		iPos = m_Nodes.Count();
	else if(!NodeForID(dwAfterID, &iPos))
		return 0;	// not found!

	CMapPathNode node;
	node.pos = vecPos;
	node.bSelected = FALSE;
	node.dwID = GetNewNodeID();

	if(iPos == m_Nodes.Count())
	{
		// add at tail
		m_Nodes.AddToTail(node);
	}
	else
	{
		m_Nodes.InsertBefore( iPos, node );
	}

	return node.dwID;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwID - 
//			*pt - 
//-----------------------------------------------------------------------------
void CMapPath::SetNodePosition(DWORD dwID, Vector& pt)
{
	int iIndex;
	NodeForID(dwID, &iIndex);

	m_Nodes[iIndex].pos = pt;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwID - 
//-----------------------------------------------------------------------------
void CMapPath::DeleteNode(DWORD dwID)
{
	int iIndex;
	if ( NodeForID(dwID, &iIndex) )
	{
		m_Nodes.Remove(iIndex);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
//-----------------------------------------------------------------------------
void CMapPath::SerializeRMF(std::fstream& file, BOOL fIsStoring)
{
	int iSize;

	if(fIsStoring)
	{
		// save!!
		file.write(m_szName, 128);
		file.write(m_szClass, 128);
		file.write((char*) &m_iDirection, sizeof(m_iDirection));
	
		iSize = m_Nodes.Count();
		file.write((char*) &iSize, sizeof iSize);
		for(int i = 0; i < m_Nodes.Count(); i++)
		{
			CMapPathNode& node = m_Nodes[i];
			// store each node
			file.write((char*) &node.pos[0], 3 * sizeof(float));
			file.write((char*) &node.dwID, sizeof(node.dwID));
			file.write((char*) &node.szName, sizeof(node.szName));
			
			//
			// Write keyvalue count.
			//
			WCKeyValues &kv = node.kv;
			iSize = 0;
			for ( int z=kv.GetFirst(); z != kv.GetInvalidIndex(); z=kv.GetNext( z ) )
			{
				++iSize;
			}
			file.write((char*) &iSize, sizeof(iSize));

			//
			// Write keyvalues.
			//
			for (int k = kv.GetFirst(); k != kv.GetInvalidIndex(); k=kv.GetNext( k ) )
			{
				MDkeyvalue &KeyValue = kv.GetKeyValue(k);
				if (KeyValue.szKey[0] != '\0')
				{
					KeyValue.SerializeRMF(file, TRUE);
				}
			}
		}
	}
	else
	{
		// load!!
		file.read(m_szName, 128);
		file.read(m_szClass, 128);
		file.read((char*) &m_iDirection, sizeof m_iDirection);

		file.read((char*) &iSize, sizeof iSize);
		int nNodes = iSize;
		m_Nodes.RemoveAll();

		// read nodes
		for(int i = 0; i < nNodes; i++)
		{
			CMapPathNode node;
			// store each node
			file.read((char*) &node.pos[0], 3 * sizeof(float));
			file.read((char*) &node.dwID, sizeof(node.dwID));
			if(GetFileVersion() >= 1.6f)
			{
				file.read((char*) &node.szName, sizeof(node.szName));

				// read keyvalues
				file.read((char*) &iSize, sizeof(iSize));
				WCKeyValues &kv = node.kv;
				for (int k = 0; k < iSize; k++)
				{
					MDkeyvalue KeyValue;
					KeyValue.SerializeRMF(file, FALSE);
					kv.SetValue( KeyValue.szKey, KeyValue.szValue );
				}
			}

			m_Nodes.AddToTail(node);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iIndex - 
//			iName - 
//			str - 
//-----------------------------------------------------------------------------
void CMapPath::GetNodeName(int iIndex, int iName, CString& str)
{
	if(m_Nodes[iIndex].szName[0])
		str = m_Nodes[iIndex].szName;
	else
	{
		if(iName)
			str.Format("%s%02d", m_szName, iName);
		else
			str = m_szName;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			fIsStoring - 
//			*pIntersecting - 
//-----------------------------------------------------------------------------
void CMapPath::SerializeMAP(std::fstream& file, BOOL fIsStoring, BoundBox *pIntersecting)
{
	if( m_Nodes.Count() == 0)
		return;

	// if saving WITHIN a box, check all nodes to see if they all 
	//  fit within that box. if not, don't save ANY of the path.
	if(pIntersecting)
	{
		for(int i = 0; i < m_Nodes.Count(); i++)
		{
			if (!pIntersecting->ContainsPoint(m_Nodes[i].pos))
			{
				return;	// doesn't intersect - don't save path
			}
		}
	}


	Assert(fIsStoring);

	CString strTemp;
	MDkeyvalue kvTemp;

	// initialize nodes for saving
	for(int i = 0; i < m_Nodes.Count(); i++)
	{
		m_Nodes[i].nTargets = 0;
	}

	int iDirec = 1;
	int iCurNode = 0;
	int iMax = m_Nodes.Count()-1;
	int iName = 0;

	// resolve targets 
	int iLastNodeIndex = -1;
	BOOL bFirstPass = TRUE;

ResolveNamesAgain:
	while(1)
	{
		// store targetname
		GetNodeName(iCurNode, iName, strTemp);

		// store our name in the previous node (if not -1)
		if(iLastNodeIndex != -1)
		{
			CMapPathNode &prevNode = m_Nodes[iLastNodeIndex];
			strcpy(prevNode.szTargets[prevNode.nTargets++], strTemp);
		}

		++iName;

		iLastNodeIndex = iCurNode;

		if(iCurNode == iMax)
			break;
		iCurNode += iDirec;
	}

	if(bFirstPass && m_iDirection == dirPingpong && m_Nodes.Count() > 2)
	{
		// redo loop
		bFirstPass = FALSE;
		iDirec = -1;
		iCurNode = m_Nodes.Count()-2;
		iMax = 0;
		goto ResolveNamesAgain;
	}
	else if (m_iDirection == dirCircular)
	{
		//
		// Connect the last node to the first node.
		//
		CMapPathNode &LastNode = m_Nodes[iMax];
		GetNodeName(iCurNode, 0, strTemp);
		strcpy(LastNode.szTargets[LastNode.nTargets], strTemp);
		LastNode.nTargets++;
	}

	iDirec = 1;
	iCurNode = 0;
	iMax = m_Nodes.Count()-1;
	iName = 0;

SaveAgain:
	while(1)
	{
		file << "{" << "\r\n";

		// store name
		kvTemp.Set("classname", m_szClass);
		kvTemp.SerializeMAP(file, TRUE);

		CMapPathNode &node = m_Nodes[iCurNode];

		// store location
		strTemp.Format("%.0f %.0f %.0f", node.pos[0], node.pos[1], 
			node.pos[2]);
		kvTemp.Set("origin", strTemp);
		kvTemp.SerializeMAP(file, TRUE);

		// store targetname
		GetNodeName(iCurNode, iName, strTemp);
		kvTemp.Set("targetname", strTemp);
		kvTemp.SerializeMAP(file, TRUE);

		// store target (if not last)
		BOOL bStoreTarget = TRUE;
		if(iCurNode == iMax && m_iDirection == dirOneway)
			bStoreTarget = FALSE;

		if (bStoreTarget)
		{
			kvTemp.Set("target", (iDirec == 1) ? node.szTargets[0] : node.szTargets[1]);
			kvTemp.SerializeMAP(file, TRUE);
		}

		// other keyvalues
		WCKeyValues &kv = node.kv;
		for (int k = kv.GetFirst(); k != kv.GetInvalidIndex(); k=kv.GetNext( k ) )
		{
			MDkeyvalue &KeyValue = kv.GetKeyValue(k);
			if (KeyValue.szKey[0] != '\0')
			{
				KeyValue.SerializeMAP(file, TRUE);
			}
		}

		file << "}" << "\r\n";

		++iName;
		iLastNodeIndex = iCurNode;

		if(iCurNode == iMax)
			break;
		iCurNode += iDirec;
	}

	if(iDirec == 1 && m_iDirection == dirPingpong && m_Nodes.Count() > 2)
	{
		// redo loop
		iDirec = -1;
		iCurNode = m_Nodes.Count()-2;
		iMax = 1;
		goto SaveAgain;
	}
}

// Edit

void CMapPath::EditInfo()
{
	CEditPathDlg dlg;
	dlg.m_strName = m_szName;
	dlg.m_strClass = m_szClass;
	dlg.m_iDirection = m_iDirection;
	
	if(dlg.DoModal() != IDOK)
		return;

	SetName(dlg.m_strName);
	SetClass(dlg.m_strClass);
	m_iDirection = dlg.m_iDirection;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwNodeID - 
// Output : CMapEntity *
//-----------------------------------------------------------------------------
CMapEntity *CMapPath::CreateEntityForNode(DWORD dwNodeID)
{
	int iIndex;
	CMapPathNode *pNode = NodeForID(dwNodeID, &iIndex);
	if (pNode == NULL)
	{
		return NULL;	// no node, no entity!
	}

	CMapEntity *pEntity = new CMapEntity;

	for (int k = pNode->kv.GetFirst(); k != pNode->kv.GetInvalidIndex(); k=pNode->kv.GetNext( k ) )
	{
		pEntity->SetKeyValue(pNode->kv.GetKey(k), pNode->kv.GetValue(k));
	}
	
	// store target/targetname properties:
	CString str;
	str.Format("%s%02d", m_szName, iIndex);
	pEntity->SetKeyValue("targetname", str);

	int iNext = iIndex + 1;
	if(iNext != -1)
	{
		str.Format("%s%02d", m_szName, iNext);
		pEntity->SetKeyValue("target", str);
	}

	pEntity->SetClass(m_szClass);

	return pEntity;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwNodeID - 
//			*pEntity - 
//-----------------------------------------------------------------------------
void CMapPath::CopyNodeFromEntity(DWORD dwNodeID, CMapEntity *pEntity)
{
	CMapPathNode *pNode = NodeForID(dwNodeID);
	if (!pNode)
	{
		return;	// no node, no copy!
	}

	pNode->kv.RemoveAll();

	//
	// Copy all the keys except target and targetname from the entity to the pathnode.
	//
	for ( int i=pEntity->GetFirstKeyValue(); i != pEntity->GetInvalidKeyValue(); i=pEntity->GetNextKeyValue( i ) )
	{
		if (!strcmp(pEntity->GetKey(i), "target") || !strcmp(pEntity->GetKey(i), "targetname"))
		{
			continue;
		}

		pNode->kv.SetValue(pEntity->GetKey(i), pEntity->GetKeyValue(i));
	}
}


/*
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szKey - 
//			*szValue - 
//			*pNode - 
// Output : CChunkFileResult_t
//-----------------------------------------------------------------------------

UNDONE: Nobody uses the path tool because the user interface is so poor.
		Path support has been pulled until the tool itself can be fixed or replaced.

CChunkFileResult_t CMapPathNode::LoadKeyCallback(const char *szKey, const char *szValue, CMapPathNode *pNode)
{
	if (!stricmp(szKey, "origin"))
	{
		CChunkFile::ReadKeyValueVector3(szValue, pNode->pos);
	}
	else if (!stricmp(szKey, "id"))
	{
		CChunkFile::ReadKeyValueInt(szValue, &pNode->dwID);
	}
	else if (!stricmp(szKey, "name"))
	{
		strcpy(pNode->szName, szValue);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapPathNode::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	ChunkFileResult_t  eResult = pFile->BeginChunk("node");

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueVector3("origin", node.pos);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("id", node.dwID);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValue("name", node.szName);
	}
	
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->BeginChunk("keys");
	}

	//
	// Write keyvalues.
	//
	if (eResult == ChunkFile_Ok)
	{
		iSize = kv.GetCount();
		for (int k = 0; k < iSize; k++)
		{
			MDkeyvalue &KeyValue = kv.GetKeyValue(k);
			if (eResult == ChunkFile_Ok)
			{
				eResult = pFile->WriteKeyValue(KeyValue.GetKey(), KeyValue.GetValue());
			}
		}
	}

	// End the keys chunk.
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	// End the node chunk.
	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}

	return(eResult);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szKey - 
//			*szValue - 
//			*pPath - 
// Output : CChunkFileResult_t
//-----------------------------------------------------------------------------
CChunkFileResult_t CMapPath::LoadKeyCallback(const char *szKey, const char *szValue, CMapPath *pPath)
{
	if (!stricmp(szKey, "name"))
	{
		pPath->SetName(szValue);
	}
	else if (!stricmp(szKey, "classname"))
	{
		pPath->SetClass(szValue);
	}
	else if (!stricmp(szKey, "direction"))
	{
		CChunkFile::ReadKeyValueInt(szValue, &pPath->m_iDirection);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
//-----------------------------------------------------------------------------
void CMapPath::LoadVMF(CChunkFile *pFile)
{
	file.read((char*) &iSize, sizeof iSize);
	m_nNodes = iSize;
	m_Nodes.SetSize(m_nNodes);

	// read nodes
	for (int i = 0; i < m_nNodes; i++)
	{
		CMapPathNode &node = m_Nodes[i];

			// read keyvalues
			file.read((char*) &iSize, sizeof(iSize));
			KeyValues &kv = node.kv;
			kv.SetSize(iSize);
			for (int k = 0; k < iSize; k++)
			{
				MDkeyvalue &KeyValue = kv.GetKeyValue(k);
				KeyValue.SerializeRMF(file, FALSE);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFile - 
//-----------------------------------------------------------------------------
void CMapPath::SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo)
{
	int iSize;

	ChunkFileResult_t eResult = pFile->BeginChunk("path");

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile-WriteKeyValue("name", m_szName);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValue("classname", m_szClass);
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->WriteKeyValueInt("direction", m_iDirection);
	}

	if (eResult == ChunkFile_Ok)
	{
		for (int i = 0; i < m_nNodes; i++)
		{
			CMapPathNode &node = m_Nodes[i];
			eResult = node.SaveVMF(pFile, pSaveInfo);
		}
	}

	if (eResult == ChunkFile_Ok)
	{
		eResult = pFile->EndChunk();
	}
}
*/
