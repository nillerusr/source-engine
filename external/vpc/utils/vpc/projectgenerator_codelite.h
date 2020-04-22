//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PROJECTGENERATOR_CODELITE_H
#define PROJECTGENERATOR_CODELITE_H
#ifdef _WIN32
#pragma once
#endif


#include "baseprojectdatacollector.h"

class CProjectGenerator_CodeLite
{
public:

	void GenerateCodeLiteProject( CBaseProjectDataCollector *pCollector, const char *pOutFilename, const char *pMakefileFilename );

private:

	void Write( const char *pMsg, ... );
	void WriteHeader();
	void WriteFileReferences();
	void WriteProject( const char *pchMakefileName );
	void WriteBuildFiles();
	void WriteBuildConfigurations();
	void WriteLegacyTargets( const char *pchMakefileName );
	void WriteTrailer();
	void WriteConfig( CSpecificConfig *pConfig );
	void WriteTarget_Build( CSpecificConfig *pConfig );
	void WriteTarget_Compile( CSpecificConfig *pConfig );
	void WriteTarget_Rebuild( CSpecificConfig *pConfig );
	void WriteTarget_Link( CSpecificConfig *pConfig );
	void WriteTarget_Debug( CSpecificConfig *pConfig );
	void WriteIncludes( CSpecificConfig *pConfig );
	void WriteFilesFolder( const char *pFolderName, const char *pExtensions );
	void WriteFiles();

private:
	CBaseProjectDataCollector *m_pCollector;
	FILE *m_fp;
	const char *m_pMakefileFilename;
	int m_nIndent;
};



#endif // PROJECTGENERATOR_CODELITE_H
