//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PREFABS_H
#define PREFABS_H
#pragma once


#include <afxtempl.h>
#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)


class BoundBox;
class CMapClass;
class CPrefab;
class CPrefabLibrary;


const POSITION ENUM_START = POSITION(1);
const int MAX_NOTES = 501;


enum
{
	pt3D,
};


enum LibraryType_t
{
	LibType_None,
	LibType_HalfLife,
	LibType_HalfLife2,
};


typedef CTypedPtrList<CPtrList, CPrefab*> CPrefabList;
typedef CTypedPtrList<CPtrList, CPrefabLibrary*> CPrefabLibraryList;


class CPrefab
{
public:

	CPrefab(void);
	virtual ~CPrefab(void);

	// load/save flags:
	enum
	{
		lsRMF = 0x00,	// default
		lsMAP = 0x01,
		lsRaw = 0x02,
		lsUpdateFilePos = 0x04
	};

	virtual int Save(LPCTSTR pszFilename, DWORD = 0) = 0;
	virtual int Load(DWORD = 0) = 0;

	// set info:
	void SetName(LPCTSTR pszName)
	{ strcpy(szName, pszName); }
	void SetNotes(LPCTSTR pszNotes)
	{ strcpy(szNotes, pszNotes); }

	// get info:
	LPCTSTR GetName() { return szName; }
	LPCTSTR GetNotes() { return szNotes; }

	// unique id assigned at creation time:
	DWORD GetID() { return dwID; }

	DWORD GetLibraryID() { return dwLibID; }

	// common interface:
	virtual CMapClass *CreateInBox(BoundBox *pBox) = 0;
	virtual int GetType() = 0;
	virtual void FreeData() = 0;
	virtual bool IsLoaded() = 0;

	// filetype determination:
	typedef enum
	{
		pftUnknown,
		pftRMF,
		pftMAP,
		pftScript
	} pfiletype_t;

	// static misc stuff:
	static pfiletype_t CheckFileType(LPCTSTR pszFilename);
	static CPrefab* FindID(DWORD dwID);

	// caching:
	static void AddMRU(CPrefab *pPrefab);
	static void EnableCaching(BOOL = TRUE);
	static void FreeAllData();	// free ALL objects' data

protected:

	char szName[31];
	char szNotes[MAX_NOTES];
	DWORD dwID;
	DWORD dwLibID;	// library id
	
	DWORD dwFileOffset;
	DWORD dwFileSize;	// size in file - for copying purposes

	static CPrefabList PrefabList;
	static CPrefabList MRU;
	static BOOL bCacheEnabled;

friend class CPrefabLibrary;
friend class CPrefabLibraryRMF;
friend class CPrefabLibraryVMF;
};


//
// A collection of prefabs.
//
class CPrefabLibrary
{
public:
	CPrefabLibrary();
	~CPrefabLibrary();

	virtual int Load(LPCTSTR pszFilename) = 0;
	virtual bool DeleteFile(void) = 0;
	virtual int Save(LPCTSTR pszFilename = NULL, BOOL bIndexOnly = FALSE) = 0;
	virtual bool IsFile(const char *szFile) = 0;

	void SetNameFromFilename(LPCTSTR pszFilename);
	virtual int SetName(const char *pszName) = 0;
	void SetNotes(LPCTSTR pszNotes)
	{
		strcpy(szNotes, pszNotes);
	}

	// get info:
	LPCTSTR GetName() { return m_szName; }
	LPCTSTR GetNotes() { return szNotes; }
	inline bool IsType(LibraryType_t eType);

	// unique id assigned at creation time:
	DWORD GetID() { return dwID; }

	CPrefab * EnumPrefabs(POSITION& p);
	void Add(CPrefab *pPrefab);
	void Remove(CPrefab *pPrefab);
	void Sort();

	static CPrefabLibrary *FindID(DWORD dwID);
	static CPrefabLibrary *EnumLibraries(POSITION &p, LibraryType_t eType = LibType_None);
	static void LoadAllLibraries(void);
	static void FreeAllLibraries(void);
	static CPrefabLibrary *FindOpenLibrary(LPCTSTR pszFilename);

protected:

	void FreePrefabs();

	static CPrefabLibraryList PrefabLibraryList;

	CPrefabList Prefabs;
	char m_szName[31];
	char szNotes[MAX_NOTES];
	DWORD dwID;
	LibraryType_t m_eType;			// HalfLife or HalfLife2 library?

friend class CPrefab;
friend class CPrefabRMF;
friend class CPrefabVMF;
};


class CPrefabLibraryRMF : public CPrefabLibrary
{
public:
	CPrefabLibraryRMF();
	~CPrefabLibraryRMF();

	bool IsFile(const char *szFile);
	int Load(LPCTSTR pszFilename);
	bool DeleteFile(void);
	int Save(LPCTSTR pszFilename = NULL, BOOL bIndexOnly = FALSE);
	int SetName(const char *pszName);

	std::fstream m_file;

protected:

	DWORD m_dwDirOffset;			// dir offset in open file
	CString m_strOpenFileName;		// open file name

friend class CPrefab;
};


class CPrefabLibraryVMF : public CPrefabLibrary
{
public:
	CPrefabLibraryVMF();
	~CPrefabLibraryVMF();

	bool IsFile(const char *szFile);
	int Load(LPCTSTR pszFilename);
	bool DeleteFile(void);
	int Save(LPCTSTR pszFilename = NULL, BOOL bIndexOnly = FALSE);
	int SetName(const char *pszName);

protected:

	char m_szFolderName[MAX_PATH];

friend class CPrefab;
};


//-----------------------------------------------------------------------------
// Purpose: Returns whether this library is of a given type. Half-Life used
//			.ol files to represent prefab libraries, Half-Life 2 uses a folder
//			of VMF files.
//-----------------------------------------------------------------------------
bool CPrefabLibrary::IsType(LibraryType_t eType)
{
	return(m_eType == eType);
}


#endif // PREFABS_H
