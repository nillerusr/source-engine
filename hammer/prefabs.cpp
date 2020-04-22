//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a system for managing prefabs. There are two types
//			of prefabs implemented here: Half-Life style prefabs, and Half-Life 2
//			style prefabs.
//
//			For Half-Life, prefab libraries are stored as binary .OL files, each of
//			which contains multiple .RMF files that are the prefabs.
//
//			For Half-Life 2, prefabs are stored in a tree of folders, each folder
//			representing a library, and the each .VMF file in the folder containing
//			a single prefab.
//
//=============================================================================//


#include "stdafx.h"
#include "Prefabs.h"
#include "Prefab3D.h"
#include "hammer.h"
#include <io.h>
#include <fcntl.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


BOOL CPrefab::bCacheEnabled = TRUE;
CPrefabList CPrefab::PrefabList;
CPrefabList CPrefab::MRU;
CPrefabLibraryList CPrefabLibrary::PrefabLibraryList;


static char *pLibHeader = "Worldcraft Prefab Library\r\n\x1a";
static float fLibVersion = 0.1f;


typedef struct
{
	DWORD dwOffset;
	DWORD dwSize;
	char szName[31];
	char szNotes[MAX_NOTES];
	int iType;
} PrefabHeader;


typedef struct
{
	float fVersion;
	DWORD dwDirOffset;
	DWORD dwNumEntries;
	char szNotes[MAX_NOTES];
} PrefabLibraryHeader;


//-----------------------------------------------------------------------------
// Purpose: Creates a prefab library from a given path.
// Input  : szFile - 
// Output : 
//-----------------------------------------------------------------------------
CPrefabLibrary *CreatePrefabLibrary(const char *szFile)
{
	CPrefabLibrary *pLibrary;

	if (stricmp(&szFile[strlen(szFile) - 2], ".ol") != 0)
	{
		pLibrary = new CPrefabLibraryVMF;
	}
	else
	{
		pLibrary = new CPrefabLibraryRMF;
	}

	if (pLibrary->Load(szFile) == -1)
	{
		delete pLibrary;
		return(NULL);
	}

	return(pLibrary);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefab::CPrefab()
{
	static DWORD dwRunningID = 1;
	// assign running ID
	dwID = dwRunningID++;
	PrefabList.AddTail(this);

	// assign blank name/notes
	szName[0] = szNotes[0] = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefab::~CPrefab()
{
	POSITION p = PrefabList.Find(this);
	if(p)
		PrefabList.RemoveAt(p);
	p = MRU.Find(this);
	if(p)
		MRU.RemoveAt(p);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwID - 
// Output : CPrefab *
//-----------------------------------------------------------------------------
CPrefab * CPrefab::FindID(DWORD dwID)
{
	POSITION p = PrefabList.GetHeadPosition();
	while(p)
	{
		CPrefab *pPrefab = PrefabList.GetNext(p);
		if(pPrefab->dwID == dwID)
			return pPrefab;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : b - 
//-----------------------------------------------------------------------------
void CPrefab::EnableCaching(BOOL b)
{
	bCacheEnabled = b;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPrefab - 
//-----------------------------------------------------------------------------
void CPrefab::AddMRU(CPrefab *pPrefab)
{
	if(!bCacheEnabled)
		return;

	POSITION p = MRU.Find(pPrefab);
	if(p)
	{
		// remove there and add to head
		MRU.RemoveAt(p);
	}
	else if(MRU.GetCount() == 5)
	{
		// uncache tail object
		p = MRU.GetTailPosition();
		if(p)	// might not be any yet
		{
			CPrefab *pUncache = MRU.GetAt(p);
			pUncache->FreeData();
			MRU.RemoveAt(p);
		}
	}

	// add to head
	MRU.AddHead(pPrefab);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefab::FreeAllData()
{
	// free all prefab data memory
	POSITION p = PrefabList.GetHeadPosition();
	while(p)
	{
		CPrefab *pPrefab = PrefabList.GetNext(p);
		pPrefab->FreeData();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
// Output : CPrefab::pfiletype_t
//-----------------------------------------------------------------------------
CPrefab::pfiletype_t CPrefab::CheckFileType(LPCTSTR pszFilename)
{
	// first check extensions
	const char *p = strrchr(pszFilename, '.');
	if(p)
	{
		if(!strcmpi(p, ".rmf"))
			return pftRMF;
		else if(!strcmpi(p, ".map"))
			return pftMAP;
		else if(!strcmpi(p, ".os"))
			return pftScript;
	}

	std::fstream file(pszFilename, std::ios::in | std::ios::binary);

	// read first 16 bytes of file
	char szBuf[255];
	file.read(szBuf, 16);

	// check 1: RMF
	float f = ((float*) szBuf)[0];

	// 0.8 was version at which RMF tag was started
	if(f <= 0.7f || !strncmp(szBuf+sizeof(float), "RMF", 3))
	{
		return pftRMF;
	}

	// check 2: script
	if(!strnicmp(szBuf, "[Script", 7))
	{
		return pftScript;
	}

	// check 3: MAP
	int i = 500;
	while(i--)
	{
		file >> std::ws;
		file.getline(szBuf, 255);
		if(szBuf[0] == '{')
			return pftMAP;
		if(file.eof())
			break;
	}

	return pftUnknown;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabLibrary::CPrefabLibrary()
{
	static DWORD dwRunningID = 1;
	// assign running ID
	dwID = dwRunningID++;
	m_szName[0] = '\0';
	szNotes[0] = '\0';
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabLibrary::~CPrefabLibrary()
{
	FreePrefabs();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefabLibrary::FreePrefabs()
{
	// nuke prefabs
	POSITION p = Prefabs.GetHeadPosition();
	while (p != NULL)
	{
		CPrefab *pPrefab = Prefabs.GetNext(p);
		delete pPrefab;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *a - 
//			*b - 
// Output : static int
//-----------------------------------------------------------------------------
static int __cdecl SortPrefabs(CPrefab *a, CPrefab *b)
{
	return(strcmpi(a->GetName(), b->GetName()));
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPrefabLibrary::Sort(void)
{
	int nPrefabs = Prefabs.GetCount();
	if (nPrefabs < 2)
	{
		return;
	}

	CPrefab **TmpPrefabArray = new CPrefab *[nPrefabs];

	//
	// Make an array we can pass to qsort.
	//
	POSITION p = ENUM_START;
	CPrefab *pPrefab = EnumPrefabs(p);
	int iPrefab = 0;
	while (pPrefab != NULL)
	{
		TmpPrefabArray[iPrefab++] = pPrefab;
		pPrefab = EnumPrefabs(p);
	}

	//
	// Sort the prefabs array by name.
	//
	qsort(TmpPrefabArray, nPrefabs, sizeof(CPrefab *), (int (__cdecl *)(const void *, const void *))SortPrefabs);

	//
	// Store back in list in sorted order.
	//
	Prefabs.RemoveAll();
	for (int i = 0; i < nPrefabs; i++)
	{
		Prefabs.AddTail(TmpPrefabArray[i]);
	}

	delete[] TmpPrefabArray;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
//-----------------------------------------------------------------------------
void CPrefabLibrary::SetNameFromFilename(LPCTSTR pszFilename)
{
	const char *cp = strrchr(pszFilename, '\\');
	strcpy(m_szName, cp ? (cp + 1) : pszFilename);
	char *p = strchr(m_szName, '.');
	if (p != NULL)
	{
		p[0] = '\0';
	}
}


//-----------------------------------------------------------------------------
// Purpose: Frees all the libraries in the prefab library list.
//-----------------------------------------------------------------------------
void CPrefabLibrary::FreeAllLibraries(void)
{
	POSITION pos = PrefabLibraryList.GetHeadPosition();
	while (pos != NULL)
	{
		CPrefabLibrary *pPrefabLibrary = PrefabLibraryList.GetNext(pos);
		if (pPrefabLibrary != NULL)
		{
			delete pPrefabLibrary;
		}
	}

	PrefabLibraryList.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Load all libraries in the prefabs directory.
//-----------------------------------------------------------------------------
void CPrefabLibrary::LoadAllLibraries()
{
	char szDir[MAX_PATH];
	char szFile[MAX_PATH];
	((CHammer *)AfxGetApp())->GetDirectory(DIR_PREFABS, szDir);

	//
	// Add one prefab library for the root prefabs folder in case they put something there.
	//
	CPrefabLibrary *pLibrary = FindOpenLibrary(szDir);
	if (pLibrary == NULL)
	{
		pLibrary = CreatePrefabLibrary(szDir);
		if (pLibrary != NULL)
		{
			PrefabLibraryList.AddTail(pLibrary);
		}
	}
	else
	{
		pLibrary->Load(szDir);
	}

	strcat(szDir, "\\*.*");

	WIN32_FIND_DATA fd;
	HANDLE hnd = FindFirstFile(szDir, &fd);
	strrchr(szDir, '\\')[0] = 0;	// truncate that

	if (hnd == INVALID_HANDLE_VALUE)
	{
		return;	// no libraries
	}

	do
	{
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (fd.cFileName[0] != '.'))
		{
			sprintf(szFile, "%s\\%s", szDir, fd.cFileName);

			pLibrary = FindOpenLibrary(szFile);
			if (pLibrary == NULL)
			{
				pLibrary = CreatePrefabLibrary(szFile);
				if (pLibrary != NULL)
				{
					PrefabLibraryList.AddTail(pLibrary);
				}
			}
			else
			{
				pLibrary->Load(szDir);
			}
		}
	} while (FindNextFile(hnd, &fd));

	FindClose(hnd);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPrefab - 
//-----------------------------------------------------------------------------
void CPrefabLibrary::Add(CPrefab *pPrefab)
{
	if(!Prefabs.Find(pPrefab))
		Prefabs.AddTail(pPrefab);
	pPrefab->dwLibID = dwID;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPrefab - 
//-----------------------------------------------------------------------------
void CPrefabLibrary::Remove(CPrefab *pPrefab)
{
	POSITION p = Prefabs.Find(pPrefab);
	if(p)
		Prefabs.RemoveAt(p);
	if(pPrefab->dwLibID == dwID)	// make sure it doesn't reference this
		pPrefab->dwLibID = 0xffff;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &p - 
// Output : CPrefab *
//-----------------------------------------------------------------------------
CPrefab * CPrefabLibrary::EnumPrefabs(POSITION &p)
{
	if(p == ENUM_START)
		p = Prefabs.GetHeadPosition();
	if(!p)
		return NULL;
	return Prefabs.GetNext(p);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwID - 
// Output : CPrefabLibrary *
//-----------------------------------------------------------------------------
CPrefabLibrary * CPrefabLibrary::FindID(DWORD dwID)
{
	POSITION p = PrefabLibraryList.GetHeadPosition();
	while(p)
	{
		CPrefabLibrary *pPrefabLibrary = PrefabLibraryList.GetNext(p);
		if(pPrefabLibrary->dwID == dwID)
			return pPrefabLibrary;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
// Output : 
//-----------------------------------------------------------------------------
CPrefabLibrary *CPrefabLibrary::FindOpenLibrary(LPCTSTR pszFilename)
{
	// checks to see if a library is open under that filename
	POSITION p = ENUM_START;
	CPrefabLibrary *pLibrary = EnumLibraries(p);
	while (pLibrary != NULL)
	{
		if (pLibrary->IsFile(pszFilename))
		{
			return(pLibrary);
		}
		pLibrary = EnumLibraries(p);
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Enumerates the prefab libraries of a given type.
// Input  : p - Iterator.
//			eType - Type of library to return, LibType_None returns all
//				library types.
// Output : Returns the next library of the given type.
//-----------------------------------------------------------------------------
CPrefabLibrary *CPrefabLibrary::EnumLibraries(POSITION &p, LibraryType_t eType)
{
	if (p == ENUM_START)
	{
		p = PrefabLibraryList.GetHeadPosition();
	}

	while (p != NULL)
	{
		CPrefabLibrary *pLibrary = PrefabLibraryList.GetNext(p);
		if ((eType == LibType_None) || pLibrary->IsType(eType))
		{
			return(pLibrary);
		}
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabLibraryRMF::CPrefabLibraryRMF()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabLibraryRMF::~CPrefabLibraryRMF()
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this prefab represents the given filename, false if not.
// Input  : szFilename - Path of a prefab library or folder.
//-----------------------------------------------------------------------------
bool CPrefabLibraryRMF::IsFile(const char *szFilename)
{
	return(strcmpi(m_strOpenFileName, szFilename) == 0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabLibraryRMF::Load(LPCTSTR pszFilename)
{
	FreePrefabs();

	m_eType = LibType_HalfLife;

	// open file
	m_file.open(pszFilename, std::ios::in | std::ios::binary);
	m_strOpenFileName = pszFilename;
	
	if(!m_file.is_open())
		return -1;

	char szBuf[128];

	// read string header
	m_file.read(szBuf, strlen(pLibHeader));
	if(strncmp(szBuf, pLibHeader, strlen(pLibHeader)))
	{
		// return
		return -1;
	}

	// read binary header
	PrefabLibraryHeader plh;
	m_file.read((char*)&plh, sizeof(plh));
	strcpy(szNotes, plh.szNotes);

	// set name from filename
	SetNameFromFilename(pszFilename);

	// read directory
	PrefabHeader *ph = new PrefabHeader[plh.dwNumEntries];
	m_dwDirOffset = plh.dwDirOffset;
	m_file.seekg(plh.dwDirOffset);
	m_file.read((char*)ph, plh.dwNumEntries * sizeof(PrefabHeader));

	//
	// Read each prefab.
	//
	for(DWORD i = 0; i < plh.dwNumEntries; i++)
	{
		Assert(ph[i].iType == pt3D);
		CPrefabRMF *pPrefab = new CPrefabRMF;

		// seek to prefab
		m_file.seekg(ph[i].dwOffset);
		pPrefab->Init(m_file);

		// set its other info frm the dir entry
		pPrefab->SetName(ph[i].szName);
		pPrefab->SetNotes(ph[i].szNotes);
		pPrefab->dwFileSize = ph[i].dwSize;
		pPrefab->dwFileOffset = ph[i].dwOffset;
		
		Add(pPrefab);
	}

	// delete directory
	delete[] ph;

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: Removes this prefab library from disk.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPrefabLibraryRMF::DeleteFile(void)
{
	return(remove(m_strOpenFileName) == 0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
//			bIndexOnly - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabLibraryRMF::Save(LPCTSTR pszFilename, BOOL bIndexOnly)
{
	// temp storage
	static char szFile[MAX_PATH];

	// if only saving index, special code -
	if(bIndexOnly && m_file.is_open())
	{
		// close file, reopen in binary write
		m_file.close();
		if(Prefabs.GetCount())
		{
			// change size of file first
			int iHandle = _open(m_strOpenFileName, _O_BINARY | _O_WRONLY);
			_chsize(iHandle, m_dwDirOffset);
			_close(iHandle);
		}

		std::fstream file(m_strOpenFileName, std::ios::binary | std::ios::out);

		// write string header
		file << pLibHeader;
		// write binary header (in case notes have changed)
		PrefabLibraryHeader plh;
		plh.dwNumEntries = Prefabs.GetCount();
		plh.fVersion = fLibVersion;
		plh.dwDirOffset = m_dwDirOffset;
		strcpy(plh.szNotes, szNotes);
		file.write((char*)&plh, sizeof plh);

		// recreate a directory and write it
		PrefabHeader *ph = new PrefabHeader[Prefabs.GetCount()];
		int iCur = 0;

		POSITION p = Prefabs.GetHeadPosition();
		while(p)
		{
			CPrefab *pPrefab = Prefabs.GetNext(p);

			// setup this dir entry
			ph[iCur].dwOffset = pPrefab->dwFileOffset;
			ph[iCur].dwSize = pPrefab->dwFileSize;
			V_strcpy_safe(ph[iCur].szName, pPrefab->GetName());
			V_strcpy_safe(ph[iCur].szNotes, pPrefab->GetNotes());
			ph[iCur].iType = pPrefab->GetType();

			++iCur;	// increase current directory entry
		}

		// write directory
		file.seekp(m_dwDirOffset);
		file.write((char*)ph, sizeof(*ph) * Prefabs.GetCount());
		file.close();

		// re-open
		m_file.open(m_strOpenFileName, std::ios::in | std::ios::binary);
		return 1;
	}

	if(pszFilename == NULL)
	{
		pszFilename = szFile;

		if(m_strOpenFileName.IsEmpty())
		{
			char szNewFilename[MAX_PATH];
			CHammer *pApp = (CHammer*) AfxGetApp();
			pApp->GetDirectory(DIR_PREFABS, szNewFilename);

			sprintf(szNewFilename + strlen(szNewFilename), "\\%s.ol", m_szName);

			// make a name
			m_strOpenFileName = szNewFilename;
		}

		strcpy(szFile, m_strOpenFileName);
	}
	else
	{
		strcpy(szFile, pszFilename);
		SetNameFromFilename(pszFilename);
	}

	// open temp file to save to.. then delete & rename old one.
	CString strTempFileName = "Temporary Prefab Library.$$$";
	std::fstream file;
	file.open(strTempFileName, std::ios::binary | std::ios::out);

	// write string header
	file << pLibHeader;

	// write binary header
	// save current position so we can seek back and rewrite it
	DWORD dwBinaryHeaderOffset = file.tellp();
	PrefabLibraryHeader plh;
	plh.dwNumEntries = Prefabs.GetCount();
	plh.fVersion = fLibVersion;
	strcpy(plh.szNotes, szNotes);
	file.write((char*)&plh, sizeof plh);

	// allocate memory for directory
	PrefabHeader *ph = new PrefabHeader[plh.dwNumEntries];
	int iCur = 0;

	char *pCopyBuf = new char[64000];

	// write each prefab
	POSITION p = Prefabs.GetHeadPosition();
	while (p)
	{
		CPrefabRMF *pPrefab = (CPrefabRMF *)Prefabs.GetNext(p);

		// setup this dir entry
		ph[iCur].dwOffset = file.tellp();
		V_strcpy_safe( ph[iCur].szName, pPrefab->GetName() );
		V_strcpy_safe( ph[iCur].szNotes, pPrefab->GetNotes() );
		ph[iCur].iType = pPrefab->GetType();

		if(pPrefab->IsLoaded())
		{
			// it's loaded - save in native method
			pPrefab->Save(file, CPrefab::lsUpdateFilePos);
		}
		else
		{
			// it's not loaded - save with quick method by copying
			// bytes directly from the existing file
			Assert(m_file.is_open());
			m_file.seekg(pPrefab->dwFileOffset);
			DWORD dwToRead = 64000, dwCopied = 0;
			while(dwToRead == 64000)
			{
				if(dwCopied + dwToRead > pPrefab->dwFileSize)
					dwToRead = pPrefab->dwFileSize - dwCopied;
				m_file.read(pCopyBuf, dwToRead);
				file.write(pCopyBuf, dwToRead);
				dwCopied += dwToRead;
			}
		}

		// set offset info HERE because we might use it above
		pPrefab->dwFileOffset = ph[iCur].dwOffset;

		// set size info
		ph[iCur].dwSize = pPrefab->dwFileSize = 
			file.tellp() - (std::streamoff)ph[iCur].dwOffset;

		++iCur;	// increase current directory entry
	}

	// delete copy buf
	delete[] pCopyBuf;

	// rewrite binary header
	plh.dwDirOffset = m_dwDirOffset = file.tellp();
	file.seekp(dwBinaryHeaderOffset);
	file.write((char*)&plh, sizeof(plh));
	file.seekp(0, std::ios::end);

	// write directory
	file.write((char*)ph, sizeof(*ph) * plh.dwNumEntries);
	file.close();	// close temp file
	
	// delete original and rename
	m_file.close();	// might already be open.. might not.
	remove(m_strOpenFileName);

	m_strOpenFileName = szFile;
	rename(strTempFileName, m_strOpenFileName);
	
	// reopen original
	m_file.open(m_strOpenFileName, std::ios::in | std::ios::binary);

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: A library's name is based on its filename. We set the name here
//			and rename the file if it exists, then re-open it.
// Input  : pszName - 
// Output : Returns zero on error, nonzero on success.
//-----------------------------------------------------------------------------
int CPrefabLibraryRMF::SetName(LPCTSTR pszName)
{
	// set szName
	strcpy(m_szName, pszName);

	char szNewFilename[MAX_PATH];
	CHammer *pApp = (CHammer*) AfxGetApp();
	pApp->GetDirectory(DIR_PREFABS, szNewFilename);

	sprintf(szNewFilename + strlen(szNewFilename), "\\%s.ol", pszName);

	if(m_file.is_open())
	{
		// close it - 
		m_file.close();
	}
	else
	{
		// ensure destination name doesn't exist already - 
		if(GetFileAttributes(szNewFilename) != 0xFFFFFFFF)
			return 0;	// exists.
	}

	// rename and reopen
	rename(m_strOpenFileName, szNewFilename);
	m_strOpenFileName = szNewFilename;
	m_file.open(m_strOpenFileName, std::ios::in | std::ios::binary);

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabLibraryVMF::CPrefabLibraryVMF()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPrefabLibraryVMF::~CPrefabLibraryVMF()
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this prefab represents the given filename, false if not.
// Input  : szFilename - Path of a prefab library or folder.
//-----------------------------------------------------------------------------
bool CPrefabLibraryVMF::IsFile(const char *szFilename)
{
	return(strcmpi(m_szFolderName, szFilename) == 0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabLibraryVMF::Load(LPCTSTR pszFilename)
{
	FreePrefabs();

	SetNameFromFilename(pszFilename);
	strcpy(m_szFolderName, pszFilename);

	m_eType = LibType_HalfLife2;

	// dvs: new prefab libs have no notes! who cares?

	//
	// Read the prefabs - they are stored as individual VMF files.
	//
	char szDir[MAX_PATH];
	strcpy(szDir, pszFilename);
	strcat(szDir, "\\*.vmf");

	WIN32_FIND_DATA fd;
	HANDLE hnd = FindFirstFile(szDir, &fd);
	if (hnd == INVALID_HANDLE_VALUE)
	{
		// No prefabs in this folder.
		return(1);
	}

	*strrchr(szDir, '*') = '\0';

	do
	{
		if (fd.cFileName[0] != '.')
		{
			//
			// Build the full path to the prefab file.
			//
			char szFile[MAX_PATH];
			strcpy(szFile, szDir);
			strcat(szFile, fd.cFileName);

			CPrefabVMF *pPrefab = new CPrefabVMF;
			pPrefab->SetFilename(szFile);

			Add(pPrefab);
		}
	} while (FindNextFile(hnd, &fd));

	FindClose(hnd);

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: Removes this prefab library from disk.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPrefabLibraryVMF::DeleteFile(void)
{
	// dvs: can't remove the prefab folder yet
	return(false);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszFilename - 
//			bIndexOnly - 
// Output : int
//-----------------------------------------------------------------------------
int CPrefabLibraryVMF::Save(LPCTSTR pszFilename, BOOL bIndexOnly)
{
	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: Set's the library's name by renaming the folder.
// Input  : pszName - 
// Output : Returns zero on error, nonzero on success.
//-----------------------------------------------------------------------------
int CPrefabLibraryVMF::SetName(LPCTSTR pszName)
{
	// dvs: rename the folder - or maybe don't implement for VMF prefabs
	strcpy(m_szName, pszName);
	return 1;
}


