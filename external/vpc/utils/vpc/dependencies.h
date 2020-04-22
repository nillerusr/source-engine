//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DEPENDENCIES_H
#define DEPENDENCIES_H
#ifdef _WIN32
#pragma once
#endif

enum EDependencyType
{
	k_eDependencyType_SourceFile,		// .cpp, .cxx, .h, .hxx
	k_eDependencyType_Project,			// this is a project file WITHOUT the target-specific extension (.mak, .vpj, .vcproj).
	k_eDependencyType_Library,			// this is a library file
	k_eDependencyType_Unknown			// Unrecognized file extension (probably .ico or .rc2 or somesuch).
};

class CProjectDependencyGraph;
enum k_EDependsOnFlags
{ 
	k_EDependsOnFlagCheckNormalDependencies		= 0x01,
	k_EDependsOnFlagCheckAdditionalDependencies	= 0x02,
	k_EDependsOnFlagRecurse						= 0x04,
	k_EDependsOnFlagTraversePastLibs			= 0x08
};

// Flags to CProjectDependencyGraph::BuildProjectDependencies.
#define BUILDPROJDEPS_FULL_DEPENDENCY_SET		0x01		// This tells it to build a graph of all projects in the source tree _including_ all games.
#define BUILDPROJDEPS_CHECK_ALL_PROJECTS		0x02		// If this is set, then it reads all .vpc files.
															// If this is not set, then it only includes the files from the command line with the "vpc +tier0 *bitmap +client /tf" syntax

class CDependency
{
friend class CProjectDependencyGraph;
friend class CSingleProjectScanner;

public:
	CDependency( CProjectDependencyGraph *pDependencyGraph );
	virtual ~CDependency();

	// Flags are a combination of k_EDependsOnFlags.
	bool DependsOn( CDependency *pTest, int flags=k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse );
	const char* GetName() const;

	// Returns true if the absolute filename of this thing (CDependency::m_Filename) matches the absolute path specified.
	bool CompareAbsoluteFilename( const char *pAbsPath  ) const;


private:
	bool FindDependency_Internal( CUtlVector<CUtlBuffer> &callTreeOutputStack, CDependency *pTest, int flags, int depth );
	void Mark();
	bool HasBeenMarked();

public:
	CUtlString		m_Filename;		// Full paths (slashes are platform dependent).
									// This is the VPC filename for a project (use CDependency_Project::m_ProjectFilename for the VCPROJ/VPJ filename).
	EDependencyType	m_Type;

	// Files that this guy depends on.
	CUtlVector<CDependency*> m_Dependencies;

	// Files added by $AdditionalProjectDependencies. This is in a separate list because we don't
	// always want DependsOn() to check this.
	CUtlVector<CDependency*> m_AdditionalDependencies;

private:
	CProjectDependencyGraph *m_pDependencyGraph;
	unsigned int m_iDependencyMark;
	bool m_bCheckedIncludes;	// Set to true when we have checked all the includes for this.

	// Cache info.
	int64 m_nCacheFileSize;
	int64 m_nCacheModificationTime;

	// Used by the cache.
	bool m_bCacheDirty; // File size or modification time don't match.
};


// This represents a project (.vcproj) file, NOT a project like a projectIndex_t.
// There can be separate .vcproj files (and thus separate CDependency_Project) for each game and platform of a projectIndex_t.
// If m_Type == k_eDependencyType_Project, then you can cast to this.
class CDependency_Project : public CDependency
{
public:
	typedef CDependency BaseClass;

	CDependency_Project( CProjectDependencyGraph *pDependencyGraph );

	// These functions read/write g_pVPC->GetOutputFilename() and such (all the m_xxStoredXXXX vars below).
	void StoreProjectParameters( const char *szScriptName );
	void ExportProjectParameters();

	// Does a case-insensitive string compare against m_ProjectName.
	// Returns -1 if not found or the index into projects.
	static int FindByProjectName( CUtlVector<CDependency_Project*> &projects, const char *pTestName );


public:
	// Include directories for the project.
	CUtlVector<CUtlString> m_IncludeDirectories;

	// Straight out of the $AdditionalProjectDependencies key (split on semicolons).
	CUtlVector<CUtlString> m_AdditionalProjectDependencies;

	// Straight out of the $AdditionalOutputFiles key (split on semicolons).
	CUtlVector<CUtlString> m_AdditionalOutputFiles;

	CUtlString	m_ProjectName;		// This comes from the $Project key in the .vpc file.
	CUtlString	m_ProjectFilename;	// Absolute path to the VCPROJ file (g_pVPC->GetOutputFilename() - see CDependency::m_Filename for the VPC filename).
	CUtlString	m_ImportLibrary;

	// Note that there can be multiple CDependency_Projects with the same m_iProjectIndex.
	projectIndex_t m_iProjectIndex;

	// This is used by /p4sln. It uses this to call into VPC_ParseProjectScript. These are the values of g_pVPC->GetOutputFilename(), szScriptName, 
	// and the defines at the time of building this project.
	CUtlString m_StoredOutputFilename;
	char m_szStoredScriptName[MAX_PATH];
	char m_szStoredCurrentDirectory[MAX_PATH];
	CUtlVector<bool> m_StoredConditionalsActive;
};


// This class builds a graph of all dependencies, starting at the projects.
class CProjectDependencyGraph : public IProjectIterator
{
friend class CDependency;

public:
	CProjectDependencyGraph();

	// This is the main function to generate dependencies.
	// nBuildProjectDepsFlags is a combination of BUILDPROJDEPS_ flags.
	void BuildProjectDependencies( int nBuildProjectDepsFlags, CUtlVector< CDependency_Project *> *pPhase1Projects = NULL );

	bool HasGeneratedDependencies() const;

	CDependency* FindDependency( const char *pFilename );
	CDependency* FindOrCreateDependency( const char *pFilename );

	// Look for all projects (that we've scanned during BuildProjectDependencies) that depend on the specified project.
	// If bDownwards is true,  then it adds iProject and all projects that _it depends on_.
	// If bDownwards is false, then it adds iProject and all projects that _depend on it_.
	void GetProjectDependencyTree( projectIndex_t iProject, CUtlVector<projectIndex_t> &dependentProjects, bool bDownwards );

	// This solves the central mismatch between the way VPC references projects and the way the CDependency stuff does.
	//
	// - VPC uses projectIndex_t, but a single projectIndex_t can turn into multiple games (server_tf, server_episodic, etc) in VPC_IterateTargetProjects.
	// - The dependency code has a separate CDependency_Project for each game.
	// 
	// This takes a bunch of project indices (usually m_targetProjects, which comes from the command line's "+this -that *theother" syntax), 
	// which are game-agnostic, and based on what games were specified on the command line, it builds the list of CDependency_Project*s.
	void TranslateProjectIndicesToDependencyProjects( CUtlVector<projectIndex_t> &projectList, CUtlVector<CDependency_Project*> &out );

// IProjectIterator overrides.
protected:
	virtual bool VisitProject( projectIndex_t iProject, const char *szProjectName );


private:
	void ClearAllDependencyMarks();

	// Functions for the vpc.cache file management.
	bool LoadCache( const char *pFilename );
	bool SaveCache( const char *pFilename );
	void WriteString( FILE *fp, CUtlString &utlString );
	CUtlString ReadString( FILE *fp );

	void CheckCacheEntries();
	void RemoveDirtyCacheEntries();
	void MarkAllCacheEntriesValid();

	void ResolveAdditionalProjectDependencies( CUtlVector< CDependency_Project *> *pPhase1Projects = NULL );

public:
	// Projects and everything they depend on.
	CUtlVector<CDependency_Project*> m_Projects;
	CUtlDict<CDependency*,int> m_AllFiles;	// All files go in here. They should never be duplicated. These are indexed by the full filename (except .lib files, which have that stripped off).
	bool m_bFullDependencySet;				// See bFullDepedencySet passed into BuildProjectDependencies.
	int m_nFilesParsedForIncludes;

private:
	// Used when sweeping the dependency graph to prevent looping around forever.
	unsigned int		m_iDependencyMark;
	bool m_bHasGeneratedDependencies;	// Set to true after finishing BuildProjectDependencies.
};


bool IsLibraryFile( const char *pFilename );
bool IsSharedLibraryFile( const char *pFilename );


#endif // DEPENDENCIES_H
