//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
//
//=============================================================================

#ifndef BASEPROJECTDATACOLLECTOR_H
#define BASEPROJECTDATACOLLECTOR_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/keyvalues.h"
#include "tier1/utlstack.h"

class CSpecificConfig
{
public:
	CSpecificConfig( CSpecificConfig *pParentConfig );
	~CSpecificConfig();

	const char	*GetConfigName();
	const char	*GetOption( const char *pOptionName );

public:
	CSpecificConfig *m_pParentConfig;
	KeyValues		*m_pKV;
	bool			m_bFileExcluded;	// Is the file that holds this config excluded from the build?
	bool			m_bIsSchema;		// Is this a schema file?
	bool			m_bIsDynamic;	    // Is this a schema file?
};

class CFileConfig
{
public:
	~CFileConfig();

	void			Term();
	const char		*GetName();
	CSpecificConfig	*GetConfig( const char *pConfigName );
	CSpecificConfig	*GetOrCreateConfig( const char *pConfigName, CSpecificConfig *pParentConfig );
	bool			IsExcludedFrom( const char *pConfigName );
	bool			IsDynamicFile( const char *pConfigName );

public:
	CUtlDict< CSpecificConfig*, int >	m_Configurations;
	CUtlString							m_Filename;	// "" if this is the config data for the whole project.
	int									m_nInsertOrder;
};

// This just holds the list of property names that we're supposed to scan for.
class CRelevantPropertyNames
{
public:
	const char **m_pNames;
	int			m_nNames;
};

//
// This class is shared by the makefile and SlickEdit project file generator.
// It just collects interesting file properties into KeyValues and then the project file generator
// is responsible for using that data to write out a project file.
//
class CBaseProjectDataCollector : public IBaseProjectGenerator
{
// IBaseProjectGenerator implementation.
public:

	CBaseProjectDataCollector( CRelevantPropertyNames *pNames );
	~CBaseProjectDataCollector();

	// Called before doing anything in a project (in g_pVPC->GetOutputFilename()).
	virtual void StartProject();
	virtual void EndProject();

	// Access the project name.
	virtual CUtlString GetProjectName();
	virtual void SetProjectName( const char *pProjectName );

	// Get a list of all configurations.
	virtual void GetAllConfigurationNames( CUtlVector< CUtlString > &configurationNames );

	// Configuration data is specified in between these calls and inside BeginPropertySection/EndPropertySection.
	// If bFileSpecific is set, then the configuration data only applies to the last file added.
	virtual void StartConfigurationBlock( const char *pConfigName, bool bFileSpecific );
	virtual void EndConfigurationBlock();

	// These functions are called when it enters a section like $Compiler, $Linker, etc.
	// In between the BeginPropertySection/EndPropertySection, it'll call HandleProperty for any properties inside that section.
	//
	// If you pass pCustomScriptData to HandleProperty, it won't touch the global parsing state -
	// it'll parse the platform filters and property value from pCustomScriptData instead.
	virtual bool StartPropertySection( configKeyword_e keyword, bool *pbShouldSkip = NULL );
	virtual void HandleProperty( const char *pProperty, const char *pCustomScriptData = NULL );
	virtual void EndPropertySection( configKeyword_e keyword );

	// Files go in folders. The generator should maintain a stack of folders as they're added.
	virtual void StartFolder( const char *pFolderName );
	virtual void EndFolder();

	// Add files. Any config blocks/properties between StartFile/EndFile apply to this file only.
	// It will only ever have one active file.
	virtual bool StartFile( const char *pFilename, bool bWarnIfAlreadyExists );
	virtual void EndFile();

	// This is actually just per-file configuration data.
	virtual void FileExcludedFromBuild( bool bExcluded );
	virtual void FileIsSchema( bool bIsSchema );
	virtual void FileIsDynamic( bool bIsDynamic );

	// Remove the specified file.
	virtual bool RemoveFile( const char *pFilename );		// returns ture if a file was removed

public:
	// This is called in EndProject if bAutoCleanupAfterProject is set.
	void Term();
	static void DoStandardVisualStudioReplacements( const char *pStartString, const char *pFullInputFilename, char *pOut, int outLen );

public:
	CUtlString							m_ProjectName;

	CUtlDict< CFileConfig *, int >		m_Files;
	CFileConfig							m_BaseConfigData;

	CUtlStack< CFileConfig* >			m_CurFileConfig;			// Either m_BaseConfigData or one of the files.
	CUtlStack< CSpecificConfig* >		m_CurSpecificConfig;		// Debug, release?
    CUtlStack< configKeyword_e >		m_CurPropertySection;
	CRelevantPropertyNames				m_RelevantPropertyNames;
};

#endif // BASEPROJECTDATACOLLECTOR_H
