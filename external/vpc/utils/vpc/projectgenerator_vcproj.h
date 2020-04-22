//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef VCPROJGENERATOR_H
#define VCPROJGENERATOR_H
#ifdef _WIN32
#pragma once
#endif

class CProjectConfiguration;
class CVCProjGenerator;
class CProjectTool;

struct PropertyState_t
{
	ToolProperty_t	*m_pToolProperty;
	CUtlString		m_OrdinalString;
	CUtlString		m_StringValue;
};

// ps3 visual studio integration
enum PS3VSIType_e
{
	PS3_VSI_TYPE_UNDEFINED = -1,
	PS3_VSI_TYPE_SNC = 0,
	PS3_VSI_TYPE_GCC = 1,
};

class CProjectFile
{
public:
	CProjectFile( CVCProjGenerator *pGenerator, const char *pFilename );
	~CProjectFile();

	bool GetConfiguration( const char *pConfigName, CProjectConfiguration **ppConfig );
	bool AddConfiguration( const char *pConfigName, CProjectConfiguration **ppConfig );
	bool RemoveConfiguration( CProjectConfiguration *pConfig );

	CUtlString			m_Name;
	CVCProjGenerator	*m_pGenerator;
	CUtlVector< CProjectConfiguration* > m_Configs;
};

class CProjectFolder
{
public:
	CProjectFolder( CVCProjGenerator *pGenerator, const char *pFolderName );
	~CProjectFolder();

	bool GetFolder( const char *pFolderName, CProjectFolder **pFolder );
	bool AddFolder( const char *pFolderName, CProjectFolder **pFolder );
	void AddFile( const char *pFilename, CProjectFile **ppFile );
	bool FindFile( const char *pFilename );
	bool RemoveFile( const char *pFilename );

	CUtlString							m_Name;
	CVCProjGenerator					*m_pGenerator;
	CUtlLinkedList< CProjectFolder* >	m_Folders;
	CUtlLinkedList< CProjectFile* >		m_Files;
};

class CPropertyStateLessFunc
{
public:
	bool Less( const int& lhs, const int& rhs, void *pContext );
};

class CPropertyStates
{
public:
	CPropertyStates();

	bool SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool = NULL );
	bool SetBoolProperty( ToolProperty_t *pToolProperty, bool bEnabled );

	PropertyState_t *GetProperty( int nPropertyId );
	PropertyState_t *GetProperty( const char *pPropertyName );

	CUtlVector< PropertyState_t > m_Properties;
	CUtlSortVector< int, CPropertyStateLessFunc > m_PropertiesInOutputOrder;

private:
	bool SetStringProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool = NULL );
	bool SetListProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool = NULL );
	bool SetBoolProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool = NULL );
	bool SetBoolProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool, bool bEnabled );
	bool SetIntegerProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool = NULL );
};

class CProjectTool
{
public:
	CProjectTool( CVCProjGenerator *pGenerator )
	{
		m_pGenerator = pGenerator;
	}

	CVCProjGenerator *GetGenerator() { return m_pGenerator; }

	// when the property belongs to the root tool (i.e. linker), no root tool is passed in
	// when the property is for the file's specific configuration tool, (i.e. compiler/debug), the root tool must be supplied
	virtual bool SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool = NULL );

	CPropertyStates	m_PropertyStates;

private:
	CVCProjGenerator *m_pGenerator;
};

class CDebuggingTool : public CProjectTool
{
public:
	CDebuggingTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CCompilerTool : public CProjectTool
{
public:
	CCompilerTool( CVCProjGenerator *pGenerator, const char *pConfigName, bool bIsFileConfig ) : CProjectTool( pGenerator )
	{
		m_ConfigName = pConfigName;
		m_bIsFileConfig = bIsFileConfig;
	}

	bool SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool = NULL );

private:
	CUtlString	m_ConfigName;
	bool		m_bIsFileConfig;
};

class CLibrarianTool : public CProjectTool
{
public:
	CLibrarianTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CLinkerTool : public CProjectTool
{
public:
	CLinkerTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CManifestTool : public CProjectTool
{
public:
	CManifestTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CXMLDocGenTool : public CProjectTool
{
public:
	CXMLDocGenTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CBrowseInfoTool : public CProjectTool
{
public:
	CBrowseInfoTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CResourcesTool : public CProjectTool
{
public:
	CResourcesTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CPreBuildEventTool : public CProjectTool
{
public:
	CPreBuildEventTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CPreLinkEventTool : public CProjectTool
{
public:
	CPreLinkEventTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CPostBuildEventTool : public CProjectTool
{
public:
	CPostBuildEventTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CCustomBuildTool : public CProjectTool
{
public:
	CCustomBuildTool( CVCProjGenerator *pGenerator, const char *pConfigName, bool bIsFileConfig ) : CProjectTool( pGenerator )
	{
		m_ConfigName = pConfigName;
		m_bIsFileConfig = bIsFileConfig;
	}

	bool SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool = NULL );

private:
	CUtlString	m_ConfigName;
	bool		m_bIsFileConfig;
};

class CXboxImageTool : public CProjectTool
{
public:
	CXboxImageTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CXboxDeploymentTool : public CProjectTool
{
public:
	CXboxDeploymentTool( CVCProjGenerator *pGenerator ) : CProjectTool( pGenerator ) {}
};

class CProjectConfiguration
{
public:
	CProjectConfiguration( CVCProjGenerator *pGenerator, const char *pConfigName, const char *pFilename );
	~CProjectConfiguration();

	CDebuggingTool *GetDebuggingTool()				{ return m_pDebuggingTool; }
	CCompilerTool *GetCompilerTool()				{ return m_pCompilerTool; }
	CLibrarianTool *GetLibrarianTool()				{ return m_pLibrarianTool; }
	CLinkerTool *GetLinkerTool()					{ return m_pLinkerTool; }
	CManifestTool *GetManifestTool()				{ return m_pManifestTool; }
	CXMLDocGenTool *GetXMLDocGenTool()				{ return m_pXMLDocGenTool; }
	CBrowseInfoTool *GetBrowseInfoTool()			{ return m_pBrowseInfoTool; }
	CResourcesTool *GetResourcesTool()				{ return m_pResourcesTool; }
	CPreBuildEventTool *GetPreBuildEventTool()		{ return m_pPreBuildEventTool; }
	CPreLinkEventTool *GetPreLinkEventTool()		{ return m_pPreLinkEventTool; }
	CPostBuildEventTool *GetPostBuildEventTool()	{ return m_pPostBuildEventTool; }
	CCustomBuildTool *GetCustomBuildTool()			{ return m_pCustomBuildTool; }
	CXboxImageTool *GetXboxImageTool()				{ return m_pXboxImageTool; }
	CXboxDeploymentTool *GetXboxDeploymentTool()	{ return m_pXboxDeploymentTool; }

	bool IsEmpty();

	bool SetProperty( ToolProperty_t *pToolProperty );

	CVCProjGenerator *m_pGenerator;

	// type of config, and config's properties
	bool m_bIsFileConfig;
	CUtlString m_Name;

	CPropertyStates	m_PropertyStates;

private:
	// the config's tools
	CDebuggingTool				*m_pDebuggingTool;
	CCompilerTool				*m_pCompilerTool;
	CLibrarianTool				*m_pLibrarianTool;
	CLinkerTool					*m_pLinkerTool;
	CManifestTool				*m_pManifestTool;
	CXMLDocGenTool				*m_pXMLDocGenTool;
	CBrowseInfoTool				*m_pBrowseInfoTool;
	CResourcesTool				*m_pResourcesTool;
	CPreBuildEventTool			*m_pPreBuildEventTool;
	CPreLinkEventTool			*m_pPreLinkEventTool;
	CPostBuildEventTool			*m_pPostBuildEventTool;
	CCustomBuildTool			*m_pCustomBuildTool;
	CXboxImageTool				*m_pXboxImageTool;
	CXboxDeploymentTool			*m_pXboxDeploymentTool;
};

class IVCProjWriter
{
public:
	virtual bool Save( const char *pOutputFilename ) = 0;
};

class CVCProjGenerator : public CBaseProjectDataCollector
{
public:
	typedef CBaseProjectDataCollector BaseClass;
	CVCProjGenerator();

	virtual const char	*GetProjectFileExtension();
	virtual void		StartProject();
	virtual void		EndProject();
	virtual CUtlString	GetProjectName();
	virtual void		SetProjectName( const char *pProjectName );
	virtual void		GetAllConfigurationNames( CUtlVector< CUtlString > &configurationNames );
	virtual void		StartConfigurationBlock( const char *pConfigName, bool bFileSpecific );
	virtual void		EndConfigurationBlock();
	virtual bool		StartPropertySection( configKeyword_e keyword, bool *pbShouldSkip );
	virtual void		HandleProperty( const char *pProperty, const char *pCustomScriptData );
	virtual void		EndPropertySection( configKeyword_e keyword );
	virtual void		StartFolder( const char *pFolderName );
	virtual void		EndFolder();
	virtual bool		StartFile( const char *pFilename, bool bWarnIfAlreadyExists );
	virtual void		EndFile();
	virtual void		FileExcludedFromBuild( bool bExcluded );
	virtual bool		RemoveFile( const char *pFilename );

	CGeneratorDefinition	*GetGeneratorDefinition()	{ return m_pGeneratorDefinition; }
	void					SetupGeneratorDefinition( IVCProjWriter *pVCProjWriter, const char *pDefinitionName, PropertyName_t *pPropertyNames );

	PS3VSIType_e		GetVSIType() { return m_VSIType; }

	CUtlString			GetGUIDString()	{ return m_GUIDString; }

	bool				GetRootConfiguration( const char *pConfigName, CProjectConfiguration **pConfig );

	CProjectFolder		*GetRootFolder() { return m_pRootFolder; }

private:
	void				Clear();
	bool 				Config_GetConfigurations( const char *pszConfigName );

	// returns true if found, false otherwise
	bool				GetFolder( const char *pFolderName, CProjectFolder *pParentFolder, CProjectFolder **pOutFolder );
	// returns true if added, false otherwise (duplicate)
	bool				AddFolder( const char *pFolderName, CProjectFolder *pParentFolder, CProjectFolder **pOutFolder );

	// returns true if found, false otherwise
	bool				FindFile( const char *pFilename, CProjectFile **pFile );
	void				AddFileToFolder( const char *pFilename, CProjectFolder *pFolder, bool bWarnIfExists, CProjectFile **pFile );

	// returns true if removed, false otherwise (not found)
	bool				RemoveFileFromFolder( const char *pFilename, CProjectFolder *pFolder );

	bool				IsConfigurationNameValid( const char *pConfigName );

	void				SetGUID( const char *pOutputFilename );

	configKeyword_e		SetPS3VisualStudioIntegrationType( configKeyword_e eKeyword );

	void				ApplyInternalPreprocessorDefinitions();

private:
	configKeyword_e			m_nActivePropertySection;
	CGeneratorDefinition	*m_pGeneratorDefinition;

	CDebuggingTool			*m_pDebuggingTool;
	CCompilerTool			*m_pCompilerTool;
	CLibrarianTool			*m_pLibrarianTool;
	CLinkerTool				*m_pLinkerTool;
	CManifestTool			*m_pManifestTool;
	CXMLDocGenTool			*m_pXMLDocGenTool;
	CBrowseInfoTool			*m_pBrowseInfoTool;
	CResourcesTool			*m_pResourcesTool;
	CPreBuildEventTool		*m_pPreBuildEventTool;
	CPreLinkEventTool		*m_pPreLinkEventTool;
	CPostBuildEventTool		*m_pPostBuildEventTool;
	CCustomBuildTool		*m_pCustomBuildTool;
	CXboxImageTool			*m_pXboxImageTool;
	CXboxDeploymentTool		*m_pXboxDeploymentTool;

	CProjectConfiguration	*m_pConfig;
	CProjectConfiguration	*m_pFileConfig;
	CProjectFile			*m_pProjectFile;

	CSimplePointerStack< CProjectFolder*, CProjectFolder*, 128 >		m_spFolderStack;
	CSimplePointerStack< CCompilerTool*, CCompilerTool*, 128 >			m_spCompilerStack;
	CSimplePointerStack< CCustomBuildTool*, CCustomBuildTool*, 128 >	m_spCustomBuildToolStack;

	CUtlString				m_ProjectName;
	CUtlString				m_OutputFilename;

	CProjectFolder			*m_pRootFolder;

	CUtlVector< CProjectConfiguration* >	m_RootConfigurations;

	// primary file dictionary
	CUtlRBTree< CProjectFile*, int >	m_FileDictionary;

	CUtlString				m_GUIDString;

	IVCProjWriter			*m_pVCProjWriter;

	// ps3 visual studio integration
	PS3VSIType_e			m_VSIType;
};

#endif // VCPROJGENERATOR_H

