//========= Copyright 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#pragma once

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include "utlstring.h"
#include "utlrbtree.h"
#include "utlvector.h"
#include "utlbuffer.h"
#include "utlstack.h"
#include "utldict.h"
#include "utlsortvector.h"
#include "checksum_crc.h"
#include "checksum_md5.h"
#include "fmtstr.h"
#include "exprevaluator.h"
#include "tier1/interface.h"
#include "p4lib/ip4.h"
#include "scriptsource.h"
#include "logging.h"
#ifdef STEAM
#include "vstdlib/strtools.h"
#else
#include "tier1/strtools.h"
#endif
#include "sys_utils.h"
#include "keyvalues.h"
#include "generatordefinition.h"

DECLARE_LOGGING_CHANNEL( LOG_VPC );

#if defined( WIN32 )
#include <atlbase.h>
#include <io.h>
#endif // WIN32

struct KeywordName_t
{
	const char			*m_pName;
	configKeyword_e		m_Keyword;
};

typedef bool (*procptr_t)( const char *pPropertyName );
typedef bool (*GetSymbolProc_t)( const char *pKey );

#define INVALID_INDEX -1

struct property_t
{
	const char	*pName;
	procptr_t	handler;
	int			platformMask;
};

enum conditionalType_e
{
	CONDITIONAL_NULL,
	CONDITIONAL_PLATFORM,
	CONDITIONAL_GAME,
	CONDITIONAL_CUSTOM
};

struct conditional_t
{
	conditional_t()
	{
		type = CONDITIONAL_NULL;
		m_bDefined = false;
		m_bGameConditionActive = false;
	}

	CUtlString			name;
	CUtlString			upperCaseName;
	conditionalType_e	type;

	// a conditional can be present in the table but not defined
	// e.g. default conditionals that get set by command line args
	bool				m_bDefined;

	// only used during multiple game iterations for game conditionals as each 'defined' game becomes active
	bool				m_bGameConditionActive;
};

struct macro_t
{
	macro_t()
	{
		m_bSetupDefineInProjectFile = false;
		m_bInternalCreatedMacro = false;
	}

	CUtlString	name;
	CUtlString	value;
	
	// If set to true, then VPC will add this as a -Dname=value parameter to the compiler's command line.
	bool		m_bSetupDefineInProjectFile;
	
	// VPC created this macro itself rather than the macro being created from a script file.
	bool		m_bInternalCreatedMacro;
};

typedef int scriptIndex_t;
struct script_t
{
	CUtlString		name;
	CUtlString		m_condition;
};

typedef int	projectIndex_t;
struct project_t
{
	CUtlString				name;
	CUtlVector< script_t >	scripts;
};

typedef int groupIndex_t;
struct group_t
{
	CUtlVector< projectIndex_t >	projects;
};

typedef int groupTagIndex_t;
struct groupTag_t
{
	groupTag_t()
	{
		bSameAsProject = false;
	}

	CUtlString					name;
	CUtlVector< groupIndex_t >	groups;

	// this tag is an implicit definition of the project
	bool						bSameAsProject;
};

struct scriptList_t
{
	scriptList_t()
	{
		m_crc = 0;
	}

	CUtlString	m_scriptName;
	CRC32_t		m_crc;
};

class IProjectIterator
{
public:
	// iProject indexes g_projectList.
	virtual bool VisitProject( projectIndex_t iProject, const char *szScriptPath ) = 0;
};

#include "ibasesolutiongenerator.h"
#include "ibaseprojectgenerator.h"
#if defined( WIN32 )
#include "baseprojectdatacollector.h"
#include "projectgenerator_vcproj.h"
#include "projectgenerator_win32.h"
#include "projectgenerator_win32_2010.h"
#include "projectgenerator_xbox360.h"
#include "projectgenerator_xbox360_2010.h"
#include "projectgenerator_ps3.h"
#endif

enum EVSVersion
{
	k_EVSVersion_Invalid,
	k_EVSVersion_2005,
	k_EVSVersion_2008,
	k_EVSVersion_2010,
	k_EVSVersion_2012,
	k_EVSVersion_2013,
	k_EVSVersion_2015,
};

class CVPC
{
public:
	CVPC();
	~CVPC();

	bool		Init( int argc, char **argv );
	void		Shutdown( bool bHasError = false );

	void		VPCError( PRINTF_FORMAT_STRING const char *pFormat, ... );
	void		VPCWarning( PRINTF_FORMAT_STRING const char *pFormat, ... );
	void		VPCStatus( bool bAlwaysSpew, PRINTF_FORMAT_STRING const char *pFormat, ... );
	void		VPCSyntaxError( PRINTF_FORMAT_STRING const char *pFormat = NULL, ... );

	bool		IsProjectCurrent( const char *pVCProjFilename, bool bSpewStatus );

	bool		HasCommandLineParameter( const char *pParamName );
	bool		HasP4SLNCommand();

	CScript		&GetScript()			{ return m_Script; }

	bool		IsVerbose()				{ return m_bVerbose; }
	bool		IsQuiet()				{ return m_bQuiet; }
	bool		IsShowDependencies()	{ return m_bShowDeps; }
	bool		IsForceGenerate()		{ return m_bForceGenerate; }
	bool		IsPosixPCHDisabled()	{ return m_bNoPosixPCH; }
	bool		IsForceIterate()		{ return m_bForceIterate; }
	bool		IsDecorateProject()		{ return m_bDecorateProject; }
	const char *GetDecorateString()		{ return m_strDecorate.String(); }
	bool		IsCheckFiles()			{ return m_bCheckFiles; }
	bool		Is2010()				{ return m_bUseVS2010FileFormat || m_eVSVersion == k_EVSVersion_2010; }
	bool		Is2012()				{ return m_eVSVersion == k_EVSVersion_2012; }	// When this returns true so does Is2010() because of the file format similarities
	bool		Is2013()				{ return m_eVSVersion == k_EVSVersion_2013; }	// When this returns true so does Is2010() because of the file format similarities
	bool		Is2015()				{ return m_eVSVersion == k_EVSVersion_2015; }	// When this returns true so does Is2010() because of the file format similarities
	bool		BUse2008()				{ return m_eVSVersion == k_EVSVersion_2008; }
	bool		IsDedicatedBuild()		{ return m_bDedicatedBuild;	}
	bool		IsUnity()				{ return m_bUseUnity; }
	bool		IsShowCaseIssues()		{ return m_bShowCaseIssues; }
	bool		UseValveBinDir()		{ return m_bUseValveBinDir; }
	bool		IsVerboseMakefile()		{ return m_bVerboseMakefile; }
	bool		BUseP4SCC()				{ return m_bP4SCC; }
	bool		BUse32BitTools()		{ return m_b32BitTools; }

	void		DecorateProjectName( char *pchProjectName );

	int			GetMissingFilesCount() const	{ return m_FilesMissing; }
	void		IncrementFileMissing()	{ ++m_FilesMissing; }
	void		ResetMissingFilesCount()		{ m_FilesMissing = 0; }

	bool		IsIgnoreRedundancyWarning()					{ return m_bIgnoreRedundancyWarning; }
	void		SetIgnoreRedundancyWarning( bool bSet )		{ m_bIgnoreRedundancyWarning = bSet; }

	const char *GetStartDirectory()			{ return m_StartDirectory.Get(); }
	const char *GetSourcePath()				{ return m_SourcePath.Get(); }
	const char *GetProjectPath()			{ return m_ProjectPath.Get(); }
	const char *GetCRCString()				{ return m_SupplementalCRCString.Get(); }
	const char *GetSolutionItemsFilename()	{ return m_SolutionItemsFilename.Get(); }

	const char *GetOutputFilename()							{ return m_OutputFilename.Get(); }
	void SetOutputFilename( const char *pOutputFilename )	{ m_OutputFilename = pOutputFilename; }

	const char *GetProjectName()							{ return m_ProjectName.Get(); }
	void SetProjectName( const char *pProjectName )			{ m_ProjectName = pProjectName; }

	const char *GetLoadAddressName()						{ return m_LoadAddressName.Get(); }
	void SetLoadAddressName( const char *pLoadAddressName )	{ m_LoadAddressName = pLoadAddressName; }

	const char *GetOutputMirrorPath()						{ return m_OutputMirrorString.Get(); }

	int			ProcessCommandLine();

	// Returns the mask identifying what platforms whould be built
	bool					IsPlatformDefined( const char *pName );
	const char				*GetTargetPlatformName();

	void GetProjectDependencies( CUtlVector<CDependency_Project *> &referencedProjects );
	void SetPhase1Projects( CUtlVector<CDependency_Project *> *pPhase1Projects ) { m_pPhase1Projects = pPhase1Projects; }

	IBaseProjectGenerator	*GetProjectGenerator()										{ return m_pProjectGenerator; }
	void					SetProjectGenerator( IBaseProjectGenerator *pGenerator )	{ m_pProjectGenerator = pGenerator; }

	IBaseSolutionGenerator	*GetSolutionGenerator()	{ return m_pSolutionGenerator; }

	// Conditionals
	conditional_t			*FindOrCreateConditional( const char *pName, bool bCreate, conditionalType_e type );
	void					ResolveMacrosInConditional( char const *pString, char *pOutBuff, int outBuffSize );
	bool					ResolveConditionalSymbol( const char *pSymbol );
	bool					EvaluateConditionalExpression( const char *pExpression );
	bool					ConditionHasDefinedType( const char* pCondition, conditionalType_e type );
	void					SetConditional( const char *pName, bool bSet = true );

	// Macros
	macro_t					*FindOrCreateMacro( const char *pName, bool bCreate, const char *pValue );
	void					ResolveMacrosInString( char const *pString, char *pOutBuff, int outBuffSize );
	int						GetMacrosMarkedForCompilerDefines( CUtlVector< macro_t* > &macroDefines );
	void					RemoveScriptCreatedMacros();
	const char				*GetMacroValue( const char *pName );
	void					SetMacro( const char *pName, const char *pValue, bool bSetupDefineInProjectFile );

	// Iterates all the projects in the specified list, checks their conditionals, and calls pIterator->VisitProject for
	// each one that passes the conditional tests.
	//
	// If bForce is false, then it does a CRC check before visiting any project to see if the target project file is
	// already up-to-date with its .vpc file.
	void					IterateTargetProjects( CUtlVector<projectIndex_t> &projectList, IProjectIterator *pIterator );

	bool					ParseProjectScript( const char *pScriptName, int depth, bool bQuiet, bool bWriteCRCCheckFile );

	void					AddScriptToCRCCheck( const char *pScriptName, CRC32_t crc );

	const char				*KeywordToName( configKeyword_e keyword );
	configKeyword_e			NameToKeyword( const char *pKeywordName );

	int						GetProjectsInGroup( CUtlVector< projectIndex_t > &projectList, const char *pGroupHame );

private:
	void					SpewUsage( void );

	bool					LoadPerforceInterface();
	void					UnloadPerforceInterface();

	void					InProcessCRCCheck();
	void					CheckForInstalledXDK();

	void					DetermineSourcePath();
	void					SetDefaultSourcePath();

	void					SetupGenerators();
	void					SetupDefaultConditionals();
	void					SetMacrosAndConditionals();
	void					ResolveMacrosInStringInternal( char const *pString, char *pOutBuff, int outBuffSize, bool bStringIsConditional );

	void					HandleSingleCommandLineArg( const char *pArg );
	void					ParseBuildOptions( int argc, char *argv[] );

	bool					CheckBinPath( char *pOutBinPath, int outBinPathSize );
	bool					RestartFromCorrectLocation( bool *pIsChild );

	void					GenerateOptionsCRCString();
	void					CreateOutputFilename( project_t *pProject, const char *pchPlatform, const char *pchPhase, const char *pGameName, const char *pchExtension );
	void					FindProjectFromVCPROJ( const char *pScriptNameVCProj );
	const char				*BuildTempGroupScript( const char *pScriptName );

	bool					HandleP4SLN( IBaseSolutionGenerator *pSolutionGenerator );
	void					HandleMKSLN( IBaseSolutionGenerator *pSolutionGenerator );

	void					GenerateBuildSet( CProjectDependencyGraph &dependencyGraph );
	bool					BuildTargetProjects();
	bool					BuildTargetProject( IProjectIterator *pIterator, projectIndex_t projectIndex, script_t *pProjectScript, const char *pGameName );

	bool					m_bVerbose;
	bool					m_bQuiet;
	bool					m_bUsageOnly;
	bool					m_bHelp;
	bool					m_bSpewPlatforms;
	bool					m_bSpewGames;
	bool					m_bSpewGroups;
	bool					m_bSpewProjects;
	bool					m_bIgnoreRedundancyWarning;
	bool					m_bSpewProperties;
	bool					m_bTestMode;
	bool					m_bForceGenerate;
	bool					m_bNoPosixPCH;
	bool					m_bForceIterate;
	bool					m_bEnableVpcGameMacro;
	bool    				m_bCheckFiles;
	bool					m_bDecorateProject;
	bool					m_bShowDeps;
	bool					m_bP4AutoAdd;
	bool					m_bP4SlnCheckEverything;
	bool					m_bDedicatedBuild;
	bool					m_bAppendSrvToDedicated; // concat "_srv" to dedicated server .so's.
	bool					m_bUseValveBinDir;		 // On Linux, use gcc toolchain from /valve/bin/
	bool					m_bAnyProjectQualified;
	EVSVersion				m_eVSVersion;
	bool					m_bUseVS2010FileFormat;
	bool					m_bUseUnity;
	bool					m_bShowCaseIssues;
	bool					m_bVerboseMakefile;
	bool					m_bP4SCC;					// VPC_SCC_INTEGRATION define, or "/srcctl" cmd line option, or env var VPC_SRCCTL=1
	bool					m_b32BitTools;				// Normally we prefer the 64-bit toolchain when building a 64-bit target. This turns that off.

	// How many of the files listed in the VPC files are missing?
	int						m_FilesMissing;

	int						m_nArgc;
	char					**m_ppArgv;

	CColorizedLoggingListener	m_LoggingListener;

	CSysModule				*m_pP4Module;
	CSysModule				*m_pFilesystemModule;

	CScript					m_Script;

	// Path where vpc was started from
	CUtlString				m_StartDirectory;

	// Root path to the sources (i.e. the directory where the vpc_scripts directory can be found in).
	CUtlString				m_SourcePath;

	// path to the project being processed (i.e. the directory where this project's .vpc can be found).
	CUtlString				m_ProjectPath;

	// strings derived from command-line commands which is checked alongside project CRCs:
	CUtlString				m_SupplementalCRCString;
	CUtlString				m_ExtraOptionsCRCString;

	CUtlString				m_MKSolutionFilename;

	CUtlString				m_SolutionItemsFilename;	// For /slnitems

	CUtlString				m_P4SolutionFilename;		// For /p4sln
	CUtlVector< int >		m_iP4Changelists;

	CUtlString				m_OutputFilename;
	CUtlString				m_ProjectName;
	CUtlString				m_LoadAddressName;

	CUtlString				m_OutputMirrorString;

	CUtlString				m_TempGroupScriptFilename;

		CUtlString				m_strDecorate;

	// This abstracts the differences between different output methods.
	IBaseProjectGenerator			*m_pProjectGenerator;
	IBaseSolutionGenerator			*m_pSolutionGenerator;

	CUtlVector< CUtlString >		m_BuildCommands;

	CUtlVector<CDependency_Project *>	*m_pPhase1Projects;

public:
	CUtlVector< conditional_t >		m_Conditionals;
	CUtlVector< macro_t >			m_Macros;

	CUtlVector< scriptList_t >		m_ScriptList;

	CUtlVector< project_t >			m_Projects;
	CUtlVector< projectIndex_t >	m_TargetProjects;

	CProjectDependencyGraph			m_dependencyGraph;

	CUtlVector< group_t >			m_Groups;
	CUtlVector< groupTag_t >		m_GroupTags;

	CUtlVector< CUtlString >		m_P4GroupRestrictions;

	CUtlVector< CUtlString >		m_SchemaFiles;

	CUtlDict< CUtlString, int > 	m_CustomBuildSteps;

	bool							m_bGeneratedProject;

	CUtlDict< bool > 				m_UnityFilesSeen;
	CUtlStack< CUtlString > 		m_UnityStack;
	CUtlString						m_sUnityCurrent;
	bool							m_bInMkSlnPass;
};

extern CVPC *g_pVPC;

extern const char			*g_pOption_ImportLibrary;					// "$ImportLibrary";
extern const char			*g_pOption_OutputFile;						// "$OutputFile";
extern const char			*g_pOption_GameOutputFile;					// "$GameOutputFile";
extern const char			*g_pOption_AdditionalIncludeDirectories;	// "$AdditionalIncludeDirectories"
extern const char			*g_pOption_AdditionalProjectDependencies;	// "$AdditionalProjectDependencies"
extern const char			*g_pOption_AdditionalOutputFiles;			// "$AdditionalOutputFiles"
extern const char			*g_pOption_PreprocessorDefinitions;			// "$PreprocessorDefinitions"
extern char					*g_IncludeSeparators[2];

extern 	void				VPC_ParseGroupScript( const char *pScriptName );

extern groupTagIndex_t		VPC_Group_FindOrCreateGroupTag( const char *pName, bool bCreate );
																		
extern void					VPC_Keyword_Configuration();
extern void					VPC_Keyword_FileConfiguration();


struct folderConfig_t
{
	CUtlVector<CUtlString> vecConfigurationNames;
	CScriptSource scriptSource;

	bool BHasConfig() { return vecConfigurationNames.Count() > 0; }
	void Clear() { vecConfigurationNames.RemoveAll(); }
};

void VPC_Keyword_FolderConfiguration( folderConfig_t *pFolderConfig );
void VPC_ApplyFolderConfigurationToFile( const folderConfig_t &folderConfig );

extern void					VPC_Config_SpewProperties( configKeyword_e keyword );
extern bool					VPC_Config_IgnoreOption( const char *pPropertyName );

extern void					VPC_FakeKeyword_SchemaFolder( class CBaseProjectDataCollector *pDataCollector );
