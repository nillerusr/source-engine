//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PROJECTGENERATOR_XBOX360_H
#define PROJECTGENERATOR_XBOX360_H
#ifdef _WIN32
#pragma once
#endif

#define PROPERTYNAME( X, Y ) X##_##Y,
enum Xbox360Properties_e
{
	#include "projectgenerator_xbox360.inc"
};

class CProjectGenerator_Xbox360 : public IVCProjWriter
{
public:
	CProjectGenerator_Xbox360();
	IBaseProjectGenerator *GetProjectGenerator() { return m_pVCProjGenerator; }
	
	virtual bool Save( const char *pOutputFilename );

private:
	bool		WriteToXML();

	bool		WriteFolder( CProjectFolder *pFolder );
	bool		WriteFile( CProjectFile *pFile );
	bool		WriteConfiguration( CProjectConfiguration *pConfig );
	bool		WriteProperty( const PropertyState_t *pPropertyState, const char *pOutputName = NULL, const char *pValue = NULL );
	bool		WriteTool( const char *pToolName, const CProjectTool *pProjectTool );
	bool		WriteNULLTool( const char *pToolName, const CProjectConfiguration *pConfig );

	CXMLWriter			m_XMLWriter;
	CVCProjGenerator	*m_pVCProjGenerator;
};

#endif // PROJECTGENERATOR_XBOX360_H
