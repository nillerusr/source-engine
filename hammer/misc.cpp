//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Miscellaneous utility functions.
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include <direct.h>
#include <time.h>
#include "MapSolid.h"
#include "mapworld.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static DWORD holdrand;


void randomize()
{
	holdrand = DWORD(time(NULL));
}


DWORD random()
{
	return(holdrand = holdrand * 214013L + 2531011L);
}


// MapCheckDlg.cpp:
BOOL DoesContainDuplicates(CMapSolid *pSolid);
static BOOL bCheckDupes = FALSE;


void NotifyDuplicates(CMapSolid *pSolid)
{
	if(!bCheckDupes)
		return;	// stop that

	if(DoesContainDuplicates(pSolid))
	{
		if(IDNO == AfxMessageBox("Duplicate Plane! Do you want more messages?", 
			MB_YESNO))
		{
			bCheckDupes = FALSE;
		}
	}
}


void NotifyDuplicates(const CMapObjectList *pList)
{
	if(!bCheckDupes)
		return;	// stop that

	FOR_EACH_OBJ( *pList, pos )
	{
		CMapClass *pobj = pList->Element(pos);
		if(!pobj->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
			continue;	// not a solid
		NotifyDuplicates((CMapSolid*) pobj);
	}
}


int mychdir(LPCTSTR pszDir)
{
	int curdrive = _getdrive();

	// changes to drive/directory
	if(pszDir[1] == ':' && _chdrive(toupper(pszDir[0]) - 'A' + 1) == -1)
		return -1;
	if(_chdir(pszDir) == -1)
	{
		// change back to original disk
		_chdrive(curdrive);
		return -1;
	}

	return 0;
}


void WriteDebug(char *pszStr)
{
#if 0
	static BOOL bFirst = TRUE;
	
	if(bFirst)
		remove("wcdebug.txt");

	bFirst = FALSE;

	FILE *fp = fopen("wcdebug.txt", "ab");
	fprintf(fp, "%s\r\n", pszStr);
	fclose(fp);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Adds the given object to the list if it is a leaf object (no children).
// Input  : pObject - Object to add to the list.
//			pList - List to put the children in.
// Output : Returns TRUE to continue enumerating when called from EnumChildren.
//-----------------------------------------------------------------------------
BOOL AddLeavesToListCallback(CMapClass *pObject, CMapObjectList *pList)
{
	if (pObject->GetChildCount() == 0)
	{
		pList->AddToTail(pObject);
	}

	return(TRUE);
}

bool IsWorldObject(CMapAtom *pObject)
{
	return (dynamic_cast<CMapWorld*>(pObject) != NULL);
}
