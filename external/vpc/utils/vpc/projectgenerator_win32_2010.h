//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PROJECTGENERATOR_WIN32_2010_H
#define PROJECTGENERATOR_WIN32_2010_H
#ifdef _WIN32
#pragma once
#endif

#define PROPERTYNAME( X, Y ) X##_##Y,
enum Win32_2010_Properties_e
{
	#include "projectgenerator_win32_2010.inc"
};

class CProjectGenerator_Win32_2010 : public IVCProjWriter
{
public:
	CProjectGenerator_Win32_2010();
	IBaseProjectGenerator *GetProjectGenerator() { return m_pVCProjGenerator; }
	
	virtual bool Save( const char *pOutputFilename );

private:
	// primary XML - foo.vcxproj
	bool		WritePrimaryXML( const char *pOutputFilename );
	bool		WriteFolder( CProjectFolder *pFolder, const char *pFileTypeName, int nDepth );
	bool		WriteFile( CProjectFile *pFile, const char *pFileTypeName );
	bool		WriteConfiguration( CProjectConfiguration *pConfig );
	bool		WriteTools( CProjectConfiguration *pConfig );
	bool		WriteProperty( const PropertyState_t *pPropertyState, bool bEmitConfiguration = false, const char *pConfigurationName = NULL, const char *pOutputName = NULL, const char *pValue = NULL );
	bool		WriteTool( const char *pToolName, const CProjectTool *pProjectTool, CProjectConfiguration *pConfig );
	bool		WriteNULLTool( const char *pToolName, const CProjectConfiguration *pConfig );
	bool		WritePropertyGroupTool( CProjectTool *pProjectTool, CProjectConfiguration *pConfiguration );
	bool		WritePropertyGroup();

	// secondary XML - foo.vcxproj.filters
	bool		WriteSecondaryXML( const char *pOutputFilename );
	bool		WriteFolderToSecondaryXML( CProjectFolder *pFolder, const char *pParentPath );
	bool		WriteFolderContentsToSecondaryXML( CProjectFolder *pFolder, const char *pParentPath, const char *pFileTypeName, int nDepth );
	bool		WriteFileToSecondaryXML( CProjectFile *pFile, const char *pParentPath, const char *pFileTypeName );

	const char	*GetKeyNameForFile( CProjectFile *pFile );

	CXMLWriter			m_XMLWriter;
	CXMLWriter			m_XMLFilterWriter;

	CVCProjGenerator	*m_pVCProjGenerator;
};

#endif // PROJECTGENERATOR_WIN32_2010_H
