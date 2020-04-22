//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef IBASEPROJECTGENERATOR_H
#define IBASEPROJECTGENERATOR_H
#ifdef _WIN32
#pragma once
#endif


//
// Usage:
// 
//	StartProject
//		StartConfigurationBlock	
//			StartPropertySection
//				HandleProperty...
//			EndPropertySection
//		EndConfigurationBlock
//
//		AddFile...
//			[inside each file it can do another configuration block as above]
//			[also, files can be put in folders with StartFolder/AddFolder]
//	EndProject
//
class IBaseProjectGenerator
{
public:
	// What file extension does this use? (vcproj, mak, vpj).
	virtual const char* GetProjectFileExtension() = 0;

	// Called before doing anything in a project (in g_pVPC->GetOutputFilename()).
	virtual void StartProject() = 0;
	virtual void EndProject() = 0;

	// Access the project name.
	virtual CUtlString GetProjectName() = 0;
	virtual void SetProjectName( const char *pProjectName ) = 0;

	// Get a list of all configurations.
	virtual void GetAllConfigurationNames( CUtlVector< CUtlString > &configurationNames ) = 0;

	// Configuration data is specified in between these calls and inside BeginPropertySection/EndPropertySection.
	// If bFileSpecific is set, then the configuration data only applies to the last file added.
	virtual void StartConfigurationBlock( const char *pConfigName, bool bFileSpecific ) = 0;
	virtual void EndConfigurationBlock() = 0;

	// These functions are called when it enters a section like $Compiler, $Linker, etc.
	// In between the BeginPropertySection/EndPropertySection, it'll call HandleProperty for any properties inside that section.
	virtual bool StartPropertySection( configKeyword_e keyword, bool *pbShouldSkip = NULL ) = 0;
	virtual void HandleProperty( const char *pProperty, const char *pCustomScriptData=NULL ) = 0;
	virtual void EndPropertySection( configKeyword_e keyword ) = 0;

	// Files go in folders. The generator should maintain a stack of folders as they're added.
	virtual void StartFolder( const char *pFolderName ) = 0;
	virtual void EndFolder() = 0;
	
	// Add files. Any config blocks/properties between StartFile/EndFile apply to this file only.
	// It will only ever have one active file.
	virtual bool StartFile( const char *pFilename, bool bWarnIfAlreadyExists ) = 0;
	virtual void EndFile() = 0;

	// This is actually just per-file configuration data.
	virtual void FileExcludedFromBuild( bool bExcluded ) = 0;
	virtual void FileIsSchema( bool bIsSchema ) = 0;		// Mark the current file as schema.
	virtual void FileIsDynamic( bool bIsDynamic ) = 0;		// Mark the current file as dynamic.

	// Remove the specified file. return true if success
	virtual bool RemoveFile( const char *pFilename ) = 0;
};

#endif // IBASEPROJECTGENERATOR_H
