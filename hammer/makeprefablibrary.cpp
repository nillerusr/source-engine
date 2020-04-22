//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Utility function for creating HalfLife1-style prefab libraries from
//			a directory full of RMF and MAP files.
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "Prefabs.h"
#include "Prefab3d.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: One function - gets all the .rmf and .map files in the current dir,
//			merges them with same-name .txt files, and creates a prefab library
//			as specified in pszName.
// Input  : pszName - 
//-----------------------------------------------------------------------------
void MakePrefabLibrary(LPCTSTR pszName)
{
	CPrefabLibrary *pLibrary = new CPrefabLibraryRMF;
	int nPrefabs = 0;

	printf("Making prefab library %s.ol\n", pszName);
	
	pLibrary->SetName(pszName);

	// disable caching of prefabs
	CPrefab::EnableCaching(FALSE);

	// get files
	static BOOL bFirst = TRUE;
Again:
	WIN32_FIND_DATA fd;
	HANDLE hnd = FindFirstFile(bFirst ? "*.rmf" : "*.map", &fd);

	if(hnd != INVALID_HANDLE_VALUE)	do
	{
		// check file type
		CPrefab *pPrefab = NULL;
		int iLoadResult = -1;

		switch (CPrefab::CheckFileType(fd.cFileName))
		{
			case CPrefab::pftUnknown:
			{
				continue;
			}

			case CPrefab::pftRMF:
			{
				CPrefabRMF *pNew = new CPrefabRMF;
				iLoadResult = pNew->Init(fd.cFileName, TRUE, CPrefab::lsRMF);
				pPrefab = (CPrefab *)pNew;
				break;
			}

			case CPrefab::pftMAP:
			{
				CPrefabRMF *pNew = new CPrefabRMF;
				iLoadResult = pNew->Init(fd.cFileName, TRUE, CPrefab::lsMAP);
				pPrefab = (CPrefab *)pNew;
				break;
			}

			case CPrefab::pftScript:
			{
				Assert(0);	// not supported yet
				break;
			}
		}

		if(iLoadResult == -1)
		{
			// pPrefab might be null but delete doesn't care
			delete pPrefab;
			pPrefab = NULL;
		}

		if(!pPrefab)
			continue;

		printf("  including %s\n", fd.cFileName);
		++nPrefabs;

		// find text file - set info with it
		CString strTextFile = fd.cFileName;
		int iPos = strTextFile.Find('.');
		strTextFile.GetBuffer(0)[iPos] = 0;
		strTextFile.ReleaseBuffer();
		strTextFile += ".txt";

		if(GetFileAttributes(strTextFile) != 0xFFFFFFFF)
		{
			std::ifstream tfile(strTextFile);
			char szBuffer[1024], szBuffer2[1024];
			memset(szBuffer, 0, sizeof szBuffer);
			// read file
			tfile.read(szBuffer, 1023);
			// get rid of \r and \n chars
			char *p1 = szBuffer, *p2 = szBuffer2;
			while(p1[0])
			{
				if(p1[0] != '\n' && p1[0] != '\r')
				{
					p2[0] = p1[0];
					++p2;
				}
				++p1;
			}
			p2[0] = 0;
			// set the prefab's info
			pPrefab->SetNotes(szBuffer2);
		}

		// add to new library
		pLibrary->Add(pPrefab);

	} while(FindNextFile(hnd, &fd));

	if(bFirst)
	{
		bFirst = FALSE;
		goto Again;
	}

	// now rewrite library
	pLibrary->Save();

	CPrefab::FreeAllData();	// free memory
	// re-enable prefab caching
	CPrefab::EnableCaching(TRUE);

	printf("%d prefabs in library.\n", nPrefabs);
}

