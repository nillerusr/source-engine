//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include <sys\types.h>
#include <sys\stat.h>
#include "ChunkFile.h"
#include "Prefab3D.h"
#include "Options.h"
#include "History.h"
#include "MapGroup.h"
#include "MapWorld.h"
#include "GlobalFunctions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefab3D::CPrefab3D()
{
	m_pWorld = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefab3D::~CPrefab3D()
{
	FreeData();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefab3D::FreeData()
{
	delete m_pWorld;
	m_pWorld = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapClass *CPrefab3D::Create(void)
{
	if (!IsLoaded() && (Load() == -1))
	{
		return(NULL);
	}

	CMapClass *pCopy;
	CMapClass *pOriginal;

	//
	// Check for just one object - if only one, don't group it.
	//
	if (m_pWorld->GetChildCount() == 1)
	{
		
		pOriginal = m_pWorld->GetChildren()->Element(0);
		pCopy = pOriginal->Copy(false);
	}
	else
	{
		// Original object is world
		pOriginal = m_pWorld;

		// New object is a new group
		pCopy = (CMapClass *)new CMapGroup;
	}

	//
	// Copy children from original (if any).
	//
	pCopy->CopyChildrenFrom(pOriginal, false);

	// HACK: must calculate bounds here due to a hack in CMapClass::CopyChildrenFrom
	pCopy->CalcBounds();

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : point - Where to center the prefab.
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CPrefab3D::CreateAtPoint(const Vector &point)
{
	//
	// Create the prefab object. It will either be a single object
	// or a group containing the prefab objects.
	//
	CMapClass *pObject = Create();

	if (pObject != NULL)
	{
		//
		// Move the prefab center to match the given point.
		//
		Vector move = point;
		Vector center;
		pObject->GetBoundsCenter(center);
		for (int i = 0; i < 3; i++)
		{
			move[i] -= center[i];
		}

		BOOL bOldLock = Options.SetLockingTextures(TRUE);
		pObject->TransMove(move);
		Options.SetLockingTextures(bOldLock);
	}

	return(pObject);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CMapClass *CPrefab3D::CreateAtPointAroundOrigin( Vector const &point )
{
	//
	// Create the prefab object. It will either be a single object
	// or a group containing the prefab objects.
	//
	CMapClass *pObject = Create();

	if( !pObject )
		return NULL;

	Vector move = point;

	BOOL bOldLock = Options.SetLockingTextures( TRUE );
	pObject->TransMove( move );
	Options.SetLockingTextures( bOldLock );

	return ( pObject );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pBox - 
// Output : 
//-----------------------------------------------------------------------------
CMapClass *CPrefab3D::CreateInBox(BoundBox *pBox)
{
	//
	// Create the prefab object. It will either be a single object
	// or a group containing the prefab objects.
	//
	CMapClass *pObject = Create();

	if (pObject != NULL)
	{
		//
		// Scale the prefab to match the box bounds.
		//
		Vector NewSize;
		pBox->GetBoundsSize(NewSize);

		Vector CurSize;
		pObject->GetBoundsSize(CurSize);

		Vector scale;
		for (int i = 0; i < 3; i++)
		{
			scale[i] = NewSize[i] / CurSize[i];
		}

		Vector zero(0, 0, 0);
		pObject->TransScale(zero, scale);

		//
		// Move the prefab center to match the box center.
		//
		Vector move;
		pBox->GetBoundsCenter(move);

		Vector center;
		pObject->GetBoundsCenter(center);
		for (int i = 0; i < 3; i++)
		{
			move[i] -= center[i];
		}

		BOOL bOldLock = Options.SetLockingTextures(TRUE);
		pObject->TransMove(move);
		Options.SetLockingTextures(bOldLock);
	}

	return(pObject);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefab3D::CenterOnZero()
{
	Vector ptCenter;
	m_pWorld->GetBoundsCenter(ptCenter);
	ptCenter[0] = -ptCenter[0];
	ptCenter[1] = -ptCenter[1];
	ptCenter[2] = -ptCenter[2];

	BOOL bOldLock = Options.SetLockingTextures(TRUE);
	m_pWorld->TransMove(ptCenter);
	Options.SetLockingTextures(bOldLock);
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the prefab data has been loaded from disk, false if not.
//-----------------------------------------------------------------------------
bool CPrefab3D::IsLoaded(void)
{
	return (m_pWorld != NULL);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabRMF::CPrefabRMF()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabRMF::~CPrefabRMF()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabRMF::DoLoad(std::fstream& file, DWORD dwFlags)
{
	int iRvl;

	GetHistory()->Pause();

	AddMRU(this);

	if(m_pWorld)
		delete m_pWorld;
	m_pWorld = new CMapWorld( NULL );

	// read data
	if(dwFlags & lsMAP)
		iRvl = m_pWorld->SerializeMAP(file, FALSE);
	else
		iRvl = m_pWorld->SerializeRMF(file, FALSE);

	// error?
	if(iRvl == -1)
	{
		GetHistory()->Resume();
		return iRvl;
	}

	m_pWorld->CalcBounds(TRUE);

	GetHistory()->Resume();

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabRMF::DoSave(std::fstream& file, DWORD dwFlags)
{
	// save world
	if(dwFlags & lsMAP)
		return m_pWorld->SerializeMAP(file, TRUE);

	return m_pWorld->SerializeRMF(file, TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabRMF::Load(DWORD dwFlags)
{
	//
	// Get parent library's file handle.
	//
	CPrefabLibraryRMF *pLibrary = dynamic_cast <CPrefabLibraryRMF *>(CPrefabLibrary::FindID(dwLibID));
	if (!pLibrary)
	{
		return -1;
	}

	std::fstream &file = pLibrary->m_file;
	file.seekg(dwFileOffset);

	return(DoLoad(file, dwFlags));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
//			bLoadNow - 
//			dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabRMF::Init(LPCTSTR pszFilename, BOOL bLoadNow, DWORD dwFlags)
{
	std::fstream file(pszFilename, std::ios::in | std::ios::binary);

	// ensure we're named
	memset(szName, 0, sizeof szName);
	strncpy(szName, pszFilename, sizeof szName - 1);
	return Init(file, bLoadNow, dwFlags);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			bLoadNow - 
//			dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabRMF::Init(std::fstream &file, BOOL bLoadNow, DWORD dwFlags)
{
	int iRvl = 1;	// start off ok
	
	if(bLoadNow)
	{
		// do load now
		iRvl = DoLoad(file, dwFlags);
	}

	if(!szName[0])
	{
		// ensure we're named
		strcpy(szName, "Prefab");
	}

	return iRvl;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
//			dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabRMF::Save(LPCTSTR pszFilename, DWORD dwFlags)
{
	std::fstream file(pszFilename, std::ios::out | std::ios::binary);
	return Save(file, dwFlags);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
//			dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabRMF::Save(std::fstream& file, DWORD dwFlags)
{
	if (!IsLoaded() && (Load() == -1))
	{
		AfxMessageBox("Couldn't Load prefab to Save it.");
		return -1;
	}

	return DoSave(file, dwFlags);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabVMF::CPrefabVMF()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabVMF::~CPrefabVMF()
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the prefab data has been loaded from disk, false if not.
//-----------------------------------------------------------------------------
bool CPrefabVMF::IsLoaded(void)
{
	if (m_pWorld == NULL)
	{
		return false;
	}

	//
	// We have loaded this prefab at least once this session. Check the file date/time
	// against our cached date/time to see if we need to reload it.
	//
	struct _stat info;
	if (_stat(m_szFilename, &info) == 0)
	{
		if (info.st_mtime > m_nFileTime)
		{
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabVMF::Load(DWORD dwFlags)
{
	//
	// Create a new world to hold the loaded objects.
	//
	if (m_pWorld != NULL)
	{
		delete m_pWorld;
	}

	m_pWorld = new CMapWorld( NULL );

	//
	// Open the file.
	//
	CChunkFile File;
	ChunkFileResult_t eResult = File.Open(m_szFilename, ChunkFile_Read);

	//
	// Read the file.
	//
	if (eResult == ChunkFile_Ok)
	{
		//
		// Set up handlers for the subchunks that we are interested in.
		//
		CChunkHandlerMap Handlers;
		Handlers.AddHandler("world", (ChunkHandler_t)CPrefabVMF::LoadWorldCallback, this);
		Handlers.AddHandler("entity", (ChunkHandler_t)CPrefabVMF::LoadEntityCallback, this);
		// dvs: Handlers.SetErrorHandler((ChunkErrorHandler_t)CPrefabVMF::HandleLoadError, this);

		File.PushHandlers(&Handlers);

		//CMapDoc::SetLoadingMapDoc( this );  dvs: fix - without this, no displacements in prefabs

		//
		// Read the sub-chunks. We ignore keys in the root of the file, so we don't pass a
		// key value callback to ReadChunk.
		//
		while (eResult == ChunkFile_Ok)
		{
			eResult = File.ReadChunk();
		}

		if (eResult == ChunkFile_EOF)
		{
			eResult = ChunkFile_Ok;
		}

		//CMapDoc::SetLoadingMapDoc( NULL );

		File.PopHandlers();
	}

	if (eResult == ChunkFile_Ok)
	{
		m_pWorld->PostloadWorld();
		m_pWorld->CalcBounds();

		File.Close();

		//
		// Store the file modification time to use as a cache check.
		//
		struct _stat info;
		if (_stat(m_szFilename, &info) == 0)
		{
			m_nFileTime = info.st_mtime;
		}
	}
	else
	{
		//GetMainWnd()->MessageBox(File.GetErrorText(eResult), "Error loading prefab", MB_OK | MB_ICONEXCLAMATION);
	}

	return(eResult == ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CPrefabVMF::LoadEntityCallback(CChunkFile *pFile, CPrefabVMF *pPrefab)
{
	CMapEntity *pEntity = new CMapEntity;

	ChunkFileResult_t eResult = pEntity->LoadVMF(pFile);

	if (eResult == ChunkFile_Ok)
	{
		CMapWorld *pWorld = pPrefab->GetWorld();
		pWorld->AddChild(pEntity);
	}

	return(ChunkFile_Ok);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pFile - 
//			pData - 
// Output : ChunkFileResult_t
//-----------------------------------------------------------------------------
ChunkFileResult_t CPrefabVMF::LoadWorldCallback(CChunkFile *pFile, CPrefabVMF *pPrefab)
{
	CMapWorld *pWorld = pPrefab->GetWorld();
	ChunkFileResult_t eResult = pWorld->LoadVMF(pFile);
	return(eResult);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
//			dwFlags - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabVMF::Save(LPCTSTR pszFilename, DWORD dwFlags)
{
	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefabVMF::SetFilename(const char *szFilename)
{
	//
	// Extract the file name without the path or extension as the prefab name.
	//
	_splitpath(szFilename, NULL, NULL, szName, NULL);

	strcpy(m_szFilename, szFilename);
}

