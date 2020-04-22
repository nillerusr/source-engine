//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#pragma once

#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include "fgdlib/WCKeyValues.h"
#include "mathlib/vector.h"

class BoundBox;
class CMapEntity;
class Path3D;


class CMapPathNode
{
	public:

		CMapPathNode();
		CMapPathNode(const CMapPathNode& src);

		char szName[128];	// if blank, use default

		Vector pos;
		DWORD dwID;
		BOOL bSelected;

		char szTargets[2][128];	// resolved when saving to map - not used otherwise
		int nTargets;

		// other values
		WCKeyValues kv;

		CMapPathNode& operator=(const CMapPathNode& src);
};


class CMapPath
{
	friend Path3D;

	public:

		CMapPath();
		~CMapPath();

		enum
		{
			ADD_START	= 0xfffffff0L,
			ADD_END		= 0xfffffff1L
		};

		DWORD AddNode(DWORD dwAfterID, const Vector &vecPos);
		void DeleteNode(DWORD dwID);
		void SetNodePosition(DWORD dwID, Vector& pt);
		CMapPathNode * NodeForID(DWORD dwID, int* piIndex = NULL);
		void GetNodeName(int iIndex, int iName, CString& str);

		// set name/class
		void SetName(LPCTSTR pszName) { V_strcpy_safe(m_szName, pszName); }
		LPCTSTR GetName() { return m_szName; }
		void SetClass(LPCTSTR pszClass) { V_strcpy_safe(m_szClass, pszClass); }
		LPCTSTR GetClass() { return m_szClass; }

		void EditInfo();

		// save/load to/from RMF:
		void SerializeRMF(std::fstream&, BOOL fIsStoring);
		// save to map: (no load!!)
		void SerializeMAP(std::fstream&, BOOL fIsStoring, BoundBox *pIntersecting = NULL);

		//void SaveVMF(CChunkFile *pFile, CSaveInfo *pSaveInfo);
		//void LoadVMF(CChunkFile *pFile);

		CMapEntity *CreateEntityForNode(DWORD dwNodeID);
		void CopyNodeFromEntity(DWORD dwNodeID, CMapEntity *pEntity);

		// directions
		enum
		{
			dirOneway,
			dirCircular,
			dirPingpong
		};

		int GetNodeCount() { return m_Nodes.Count(); }

	private:

		// nodes + number of:
		CUtlVector<CMapPathNode> m_Nodes;
		
		DWORD GetNewNodeID();

		// name:
		char m_szName[128];
		char m_szClass[128];
		int m_iDirection;
};
