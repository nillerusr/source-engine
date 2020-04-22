//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef MAKEFILECREATOR_H
#define MAKEFILECREATOR_H
#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"
#include "utlsymbol.h"
#include "utldict.h"
#include "utlmap.h"
#include "vcprojconvert.h"
#include "filesystem.h"

class CMakefileCreator
{
public:

	CMakefileCreator();
	~CMakefileCreator();

	void CreateMakefiles( CVCProjConvert & proj );

private:
	void CleanupFileName( char *name );
	void OutputDirs( FileHandle_t f );
	void OutputBuildTarget( FileHandle_t f );
	void OutputObjLists( CVCProjConvert::CConfiguration & config, FileHandle_t f );
	void OutputIncludes( CVCProjConvert::CConfiguration & config, FileHandle_t f );
	void OutputMainBuilder( FileHandle_t f );

	void CreateBaseDirs( CVCProjConvert::CConfiguration & config );
	void CreateMakefileName( const char *projectName, CVCProjConvert::CConfiguration & config );
	void CreateDirectoryFriendlyName( const char *dirName, char *friendlyDirName, int friendlyDirNameSize );
	void CreateObjDirectoryFriendlyName ( char *name );
	void FileWrite( FileHandle_t f, PRINTF_FORMAT_STRING const char *fmt, ... );


	CUtlDict<CUtlSymbol, int> m_BaseDirs;
	CUtlMap<int, int>	m_FileToBaseDirMapping;

	struct OutputDirMapping_t
	{
		CUtlSymbol m_SrcDir;
		CUtlSymbol m_ObjDir;
		CUtlSymbol m_ObjName;
		CUtlSymbol m_ObjOutputDir;
		int m_iBaseDirIndex;
	};

	CUtlVector<struct OutputDirMapping_t> m_BuildDirectories;
	CUtlSymbol		m_MakefileName;
	CUtlSymbol		m_ProjName;
	CUtlSymbol		m_BaseDir;
};

#endif // MAKEFILECREATOR_H
