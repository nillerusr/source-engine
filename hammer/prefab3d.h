//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PREFAB3D_H
#define PREFAB3D_H
#pragma once


#include "Prefabs.h"


class CChunkFile;
class CMapWorld;
class Vector;


enum ChunkFileResult_t;


class CPrefab3D : public CPrefab
{
	public:

		CPrefab3D();
		~CPrefab3D();

		virtual bool IsLoaded(void);
		void FreeData();

		void CenterOnZero();

		CMapClass *Create(void);
		CMapClass *CreateInBox(BoundBox *pBox);
		CMapClass *CreateAtPoint(const Vector &point);
		CMapClass *CreateAtPointAroundOrigin( Vector const &point );

		int GetType(void) { return pt3D; }
		inline CMapWorld *GetWorld(void);
		inline void SetWorld(CMapWorld *pWorld);

	protected:

		// prefab data:
		CMapWorld *m_pWorld;
}; 


//-----------------------------------------------------------------------------
// Purpose: Returns a world containing the objects that make up this prefab.
//-----------------------------------------------------------------------------
CMapWorld *CPrefab3D::GetWorld(void)
{
	return(m_pWorld);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefab3D::SetWorld(CMapWorld *pWorld)
{
	FreeData();
	m_pWorld = pWorld;
}


class CPrefabRMF : public CPrefab3D
{
	public:

		CPrefabRMF();
		~CPrefabRMF();

		int Init(LPCTSTR pszFilename, BOOL bLoadNow = FALSE, DWORD = 0);
		int Load(DWORD dwFlags = 0);
		int Save(LPCTSTR pszFilename, DWORD = 0);

		int Init(std::fstream &file, BOOL bLoadNow = FALSE, DWORD = 0);
		int Save(std::fstream &file, DWORD = 0);

	private:

		int DoLoad(std::fstream&, DWORD = 0);
		int DoSave(std::fstream&, DWORD = 0);
}; 


class CPrefabVMF : public CPrefab3D
{
	public:

		CPrefabVMF();
		~CPrefabVMF();

		int Load(DWORD dwFlags = 0);
		int Save(LPCTSTR pszFilename, DWORD = 0);

		virtual bool IsLoaded(void);

		void SetFilename(const char *szFilename);

	protected:

		static ChunkFileResult_t LoadEntityCallback(CChunkFile *pFile, CPrefabVMF *pPrefab);
		static ChunkFileResult_t LoadWorldCallback(CChunkFile *pFile, CPrefabVMF *pPrefab);

		char m_szFilename[MAX_PATH];	// Full path of the prefab VMF.
		int m_nFileTime;				// File modification time of the last loaded version of the prefab.
}; 

#endif // PREFAB3D_H
