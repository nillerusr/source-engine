//========= Copyright Valve Corporation, All rights reserved. ============//
#include <windows.h>
#include "tier1/strtools.h"
#include <conio.h>
#include "utlvector.h"
#include <Dbghelp.h>
#include "vgui_controls/ListPanel.h"
#include "KeyValues.h"

enum moduleType
{
	GOOD,
	BAD,
	UNKNOWN
};

struct version
{
	int v1, v2, v3, v4;
	bool operator == (version version2){ return v1==version2.v1 && v2==version2.v2 && v3==version2.v3 && v4==version2.v4; }
};

struct module
{
	int key;
	char name[1024];
	moduleType myType;
	version versionInfo;
	bool knownVersion;
};

class CMiniDumpObject
{
public:
	//CMiniDumpObject( char *pszFilename, char *pszKnownFilename );
	CMiniDumpObject( const char *pszFilename, CUtlVector<module> *pKnownModuleList );
	CMiniDumpObject( HANDLE pMiniDumpHandle, CUtlVector<module> *pKnownModuleList );

	void AddToBadList( MINIDUMP_MODULE module );
	void AddToGoodList( MINIDUMP_MODULE module );
	void AddToUnknownList( MINIDUMP_MODULE module );
	void PopulateListPanel( vgui::ListPanel *pTokenList, bool bCumulative );
	inline const char *GetName()
	{
		return m_pszMiniDumpFileName;
	}

private:
	void Init( HANDLE pFileMap, CUtlVector<module> *pKnownModuleList );
	void InitFromFilename( const char *pszFilename, CUtlVector<module> *pKnownModuleList );
	void InitFromHandle( HANDLE pMiniDumpHandle, CUtlVector<module> *pKnownModuleList );
	int ModuleListToListPanel( vgui::ListPanel *pTokenList, CUtlVector<module> *pModuleList, bool bCumulative, int startingModule);
	void GetVersionString( char *pszOutput, version *pVersionInfo );
	version GetVersionStruct( VS_FIXEDFILEINFO *pVersionInfo );
	void LoadKnownModules();

	CUtlVector<module>				m_goodModuleList;
	CUtlVector<module>				m_badModuleList;
	CUtlVector<module>				m_unknownModuleList;
	CUtlVector<module>				m_badChecksumList;
	char							m_pszMiniDumpFileName[1024];
		
};