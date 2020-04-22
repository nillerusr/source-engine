//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// valvelibaw.cpp : implementation file
//

#include "stdafx.h"
#include "valvelib.h"
#include "valvelibaw.h"
#include "chooser.h"

#include <assert.h>

#include<atlbase.h>
extern CComModule _Module;
#include <atlcom.h>
#include <initguid.h>
#include<comdef.h>


#ifdef _PSEUDO_DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define STUPID_MS_BUG 1

enum Config_t
{
	CONFIG_DEBUG = 0,
	CONFIG_RELEASE
};

enum Project_t
{
	PROJECT_LIB = 0,
	PROJECT_DLL,
	PROJECT_EXE
};

struct ProjectInfo_t
{
	DSProjectSystem::IConfigurationPtr	m_pConfig;
	long		m_ConfigIndex;
	Config_t	m_Config;
	Project_t	m_Project;
	CString		m_RelativeTargetPath;
	CString		m_RelativeImplibPath;
	CString		m_RelativeRootPath;
	CString		m_RelativeSrcPath;
	CString		m_TargetPath;
	CString		m_RootName;
	CString		m_BuildName;
	CString		m_Ext;
	bool		m_Tool;
	bool		m_Console;
	bool		m_Public;
	bool		m_PublishImportLib;
};


// This is called immediately after the custom AppWizard is loaded.  Initialize
//  the state of the custom AppWizard here.
void CValvelibAppWiz::InitCustomAppWiz()
{
	// Create a new dialog chooser; CDialogChooser's constructor initializes
	//  its internal array with pointers to the steps.
	m_pChooser = new CDialogChooser;

	// Set the maximum number of steps.
	SetNumberOfSteps(LAST_DLG);

	// TODO: Add any other custom AppWizard-wide initialization here.
}

// This is called just before the custom AppWizard is unloaded.
void CValvelibAppWiz::ExitCustomAppWiz()
{
	// Deallocate memory used for the dialog chooser
	ASSERT(m_pChooser != NULL);
	delete m_pChooser;
	m_pChooser = NULL;

	// TODO: Add code here to deallocate resources used by the custom AppWizard
}

// This is called when the user clicks "Create..." on the New Project dialog
//  or "Next" on one of the custom AppWizard's steps.
CAppWizStepDlg* CValvelibAppWiz::Next(CAppWizStepDlg* pDlg)
{
	// Delegate to the dialog chooser
	return m_pChooser->Next(pDlg);
}

// This is called when the user clicks "Back" on one of the custom
//  AppWizard's steps.
CAppWizStepDlg* CValvelibAppWiz::Back(CAppWizStepDlg* pDlg)
{
	// Delegate to the dialog chooser
	return m_pChooser->Back(pDlg);
}


//-----------------------------------------------------------------------------
// Deals with compiler settings
//-----------------------------------------------------------------------------

static void SetupCompilerSettings(ProjectInfo_t& info)
{
	_bstr_t varTool;
	_bstr_t varSwitch;
	_variant_t varj = info.m_ConfigIndex;

	// We're going to modify settings associated with cl.exe
	varTool   = "cl.exe";

	// Standard include directories
	CString includePath;
	if (info.m_Public)
	{
		includePath.Format("/I%spublic", info.m_RelativeSrcPath, 
			info.m_RelativeSrcPath );

		info.m_pConfig->AddToolSettings(varTool, (char const*)includePath, varj);
	}

	// Control which version of the debug libraries to use (static single thread
	// for normal projects, multithread for tools)
	if (info.m_Tool)
	{
		if (info.m_Config == CONFIG_DEBUG)
			varSwitch = "/MTd";
		else
			varSwitch = "/MT";
	}
	else
	{
		// Suppress use of non-filesystem stuff in new non-tool projects...
		if (info.m_Config == CONFIG_DEBUG)
			varSwitch = "/D \"fopen=dont_use_fopen\" /MTd";
		else
			varSwitch = "/D \"fopen=dont_use_fopen\" /MT";
	}
	info.m_pConfig->AddToolSettings(varTool, varSwitch, varj);

	// If it's a DLL, define a standard exports macro...
	if (info.m_Project == PROJECT_DLL)
	{
		CString dllExport;
		dllExport.Format("/D \"%s_DLL_EXPORT\"", info.m_RootName ); 
		dllExport.MakeUpper();
		info.m_pConfig->AddToolSettings(varTool, (char const*)dllExport, varj);
	}

	// /W4 - Warning level 4
	// /FD - Generates file dependencies
	// /G6 - optimize for pentium pro
	// Add _WIN32, as that's what all our other code uses
	varSwitch = "/W4 /FD /G6 /D \"_WIN32\"";
	info.m_pConfig->AddToolSettings(varTool, varSwitch, varj);

	// Remove Preprocessor def for MFC DLL specifier, _AFXDLL
	// /GX - No exceptions, 
	// /YX - no precompiled headers
	// Remove WIN32, it's put there by default
	varSwitch = "/D \"_AFXDLL\" /GX /YX /D \"WIN32\"";
	info.m_pConfig->RemoveToolSettings(varTool, varSwitch, varj);

	switch (info.m_Config)
	{
	case CONFIG_DEBUG:
		// Remove the following switches (avoid duplicates, in addition to other things)
		varSwitch = "/D \"NDEBUG\" /Zi /ZI /GZ";
		info.m_pConfig->RemoveToolSettings(varTool, varSwitch, varj);

		// Add the following switches:
		// /ZI - Includes debug information in a program database compatible with Edit and Continue. 
		// /Zi - Includes debug information in a program database (no Edit and Continue) 
		// /Od - Disable optimization
		// /GZ - Catch release build problems (uninitialized variables, fp stack problems)
		// /Op - Improves floating-point consistency
		// /Gm - Minimal rebuild
		// /FR - Generate Browse Info
		varSwitch = "/D \"_DEBUG\" /Od /GZ /Op /Gm /FR\"Debug/\"";
		info.m_pConfig->AddToolSettings(varTool, varSwitch, varj);

		if (info.m_Project == PROJECT_LIB)
		{
			// Static libraries cannot use edit-and-continue, why compile it in?
			varSwitch = "/Zi";
			info.m_pConfig->AddToolSettings(varTool, varSwitch, varj);
		}
		else
		{
			varSwitch = "/ZI";
			info.m_pConfig->AddToolSettings(varTool, varSwitch, varj);
		}
		break;

	case CONFIG_RELEASE:
		// Remove the following switches:
		// /Zi - Generates complete debugging information 
		// /ZI - Includes debug information in a program database compatible with Edit and Continue. 
		// /Gm - Minimal rebuild
		// /GZ - Catch release build problems (uninitialized variables, fp stack problems)
		varSwitch = "/D \"_DEBUG\" /Gm /Zi /ZI /GZ";
		info.m_pConfig->RemoveToolSettings(varTool, varSwitch, varj);

		// Add the following switches:
		// /Ox - Uses maximum optimization (/Ob1gity /Gs)
		// /Ow - Assumes aliasing across function calls 
		// /Op - Improves floating-point consistency 
		// /Gf - String pooling
		// /Oi - Generates intrinsic functions
		// /Ot - Favors fast code 
		// /Og - Uses global optimizations
		// /Gy - Enable function level linking
		varSwitch = "/D \"NDEBUG\" /Ox /Ow /Op /Oi /Ot /Og /Gf /Gy";
		info.m_pConfig->AddToolSettings(varTool, varSwitch, varj);
		break;
	}
}

//-----------------------------------------------------------------------------
// Deals with resource compiler
//-----------------------------------------------------------------------------

static void SetupResourceCompilerSettings(ProjectInfo_t& info)
{
	_bstr_t varTool;
	_bstr_t varSwitch;
	_variant_t varj = info.m_ConfigIndex;

	// Remove Preprocessor def for MFC DLL specifier, _AFXDLL
	varTool   = "rc.exe";
	varSwitch = "/d \"_AFXDLL\"";
	info.m_pConfig->RemoveToolSettings(varTool, varSwitch, varj);
}


//-----------------------------------------------------------------------------
// Deals with linker settings
//-----------------------------------------------------------------------------

static void SetupLinkerSettings(ProjectInfo_t& info)
{
	_bstr_t varTool;
	_bstr_t varSwitch;
	_variant_t varj = info.m_ConfigIndex;

	// We're going to modify settings associated with cl.exe
	varTool   = "link.exe";

	// Remove windows libraries by default... hopefully we don't have windows dependencies
	varSwitch = "kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib";
	info.m_pConfig->RemoveToolSettings(varTool, varSwitch, varj);

	// Add libraries we always want to use
	varSwitch = "tier0.lib";
	info.m_pConfig->AddToolSettings(varTool, varSwitch, varj);
	
	// Hook in the static library path
	CString libPath;

	// FIXME: Still haven't decided on build-specific publish dir

	if (info.m_Public)
	{
		libPath.Format( "/libpath:\"%slib\\common\\\" /libpath:\"%slib\\public\\\"",
			info.m_RelativeSrcPath, info.m_RelativeSrcPath );
	}
	else
	{
		libPath.Format( "/libpath:\"%slib\\common\\\"", info.m_RelativeSrcPath );
	}

	info.m_pConfig->AddToolSettings(varTool, (char const*)libPath, varj);
}

//-----------------------------------------------------------------------------
// Deals with lib settings
//-----------------------------------------------------------------------------

static void SetupLibSettings(ProjectInfo_t& info)
{
}

//-----------------------------------------------------------------------------
// Deals with custom build steps
//-----------------------------------------------------------------------------

static void SetupCustomBuildSteps(ProjectInfo_t& info)
{
	// Create the custom build steps
	CString copySteps;
	CString targets;

	if (info.m_Project == PROJECT_LIB)
	{
		CString targetPath;

		targetPath.Format( "%s%s.%s", info.m_RelativeTargetPath, info.m_RootName, info.m_Ext );

		// NOTE: The second attrib is commented out because I'm fairly certain it causes
		// a bug in VSS to make it overwrite the target on a get
		copySteps.Format( 
			"if exist %s attrib -r %s\n"
			"copy $(TargetPath) %s\n"
			"if exist $(TargetDir)\\%s.map copy $(TargetDir)\\%s.map %s%s.map", 
			targetPath, targetPath,
			targetPath,
			info.m_RootName, info.m_RootName, info.m_RelativeTargetPath, info.m_RootName );

		targets.Format( "%s\n", targetPath );
	}
	else
	{
		CString targetPath;
		targetPath.Format( "%s%s.%s", info.m_RelativeTargetPath, info.m_RootName, info.m_Ext );

		// NOTE: The second attrib is commented out because I'm fairly certain it causes
		// a bug in VSS to make it overwrite the target on a get
		copySteps.Format( 
			"if exist %s attrib -r %s\n"
			"copy $(TargetPath) %s\n"
			"if exist $(TargetDir)\\%s.map copy $(TargetDir)\\%s.map %s%s.map", 
			targetPath, targetPath,
			targetPath,
			info.m_RootName, info.m_RootName, info.m_RelativeTargetPath, info.m_RootName );

		targets.Format( "%s", targetPath );

		if ((info.m_Project == PROJECT_DLL) && info.m_PublishImportLib)
		{
			CString targetPath;

 			targetPath.Format( "%s%s.lib", info.m_RelativeImplibPath, info.m_RootName );

			CString impLibCopy;
			impLibCopy.Format( 
				"\nif exist %s attrib -r %s\n"
				"if exist $(TargetDir)\\%s.lib copy $(TargetDir)\\%s.lib %s\n",
				targetPath, targetPath,
				info.m_RootName, info.m_RootName, targetPath
				);
			copySteps += impLibCopy;

			CString implibTarget;
			implibTarget.Format( "\n%s", targetPath );
			targets += implibTarget;
		}
	}

	CString publishDir;
	publishDir.Format( "Publishing to target directory (%s)...", info.m_RelativeTargetPath );
	info.m_pConfig->AddCustomBuildStep( (char const*)copySteps, (char const*)targets, (char const*)publishDir );
}

//-----------------------------------------------------------------------------
// Deals with MIDL build steps
//-----------------------------------------------------------------------------

static void SetupMIDLSettings(ProjectInfo_t& info)
{
}

//-----------------------------------------------------------------------------
// Deals with browse build steps
//-----------------------------------------------------------------------------

static void SetupBrowseSettings(ProjectInfo_t& info)
{
}

void CValvelibAppWiz::CustomizeProject(IBuildProject* pProject)
{
	// This is called immediately after the default Debug and Release
	//  configurations have been created for each platform.  You may customize
	//  existing configurations on this project by using the methods
	//  of IBuildProject and IConfiguration such as AddToolSettings,
	//  RemoveToolSettings, and AddCustomBuildStep. These are documented in
	//  the Developer Studio object model documentation.

	// WARNING!!  IBuildProject and all interfaces you can get from it are OLE
	//  COM interfaces.  You must be careful to release all new interfaces
	//  you acquire.  In accordance with the standard rules of COM, you must
	//  NOT release pProject, unless you explicitly AddRef it, since pProject
	//  is passed as an "in" parameter to this function.  See the documentation
	//  on CCustomAppWiz::CustomizeProject for more information.

	using namespace DSProjectSystem;

	long lNumConfigs;
	IConfigurationsPtr pConfigs;
	IBuildProjectPtr pProj;

	// Needed to convert IBuildProject to the DSProjectSystem namespace
	pProj.Attach((DSProjectSystem::IBuildProject*)pProject, true);

	// Add a release with symbols configuration
	pProj->get_Configurations(&pConfigs);
	pConfigs->get_Count(&lNumConfigs);

	// Needed for OLE2T below
	USES_CONVERSION;
	CComBSTR configName;

	ProjectInfo_t info;
	if (!Valvelibaw.m_Dictionary.Lookup("VALVE_RELATIVE_PATH", info.m_RelativeTargetPath))
		return;
	if (!Valvelibaw.m_Dictionary.Lookup("root", info.m_RootName))
		return;
	if (!Valvelibaw.m_Dictionary.Lookup("VALVE_TARGET_TYPE", info.m_Ext))
		return;
	if (!Valvelibaw.m_Dictionary.Lookup("VALVE_ROOT_RELATIVE_PATH", info.m_RelativeRootPath))
		return;
	if (!Valvelibaw.m_Dictionary.Lookup("VALVE_SRC_RELATIVE_PATH", info.m_RelativeSrcPath))
		return;
	if (!Valvelibaw.m_Dictionary.Lookup("VALVE_TARGET_PATH", info.m_TargetPath))
		return;

	CString tmp;
	info.m_Tool = (Valvelibaw.m_Dictionary.Lookup("VALVE_TOOL", tmp) != 0);
	info.m_Console = (Valvelibaw.m_Dictionary.Lookup("PROJTYPE_CON", tmp) != 0);
	info.m_Public = (Valvelibaw.m_Dictionary.Lookup("VALVE_PUBLIC_PROJECT", tmp) != 0);
	info.m_PublishImportLib = (Valvelibaw.m_Dictionary.Lookup("VALVE_PUBLISH_IMPORT_LIB", tmp) != 0);
	info.m_Project = PROJECT_LIB;
	if ( strstr( info.m_Ext, "dll" ) != 0 )
	{
		info.m_Project = PROJECT_DLL;
		if (!Valvelibaw.m_Dictionary.Lookup("VALVE_IMPLIB_RELATIVE_PATH", info.m_RelativeImplibPath))
			return;
	}
	else if ( strstr( info.m_Ext, "exe" ) != 0 )
	{
		info.m_Project = PROJECT_EXE;
	}

	//Get each individual configuration
	for (long j = 1 ; j <= lNumConfigs ; j++)
	{
		_bstr_t varTool;
		_bstr_t varSwitch;
		_variant_t varj = j;
		info.m_ConfigIndex = j;

		info.m_pConfig = pConfigs->Item(varj);
		
		// Figure if we're debug or release
		info.m_pConfig->get_Name( &configName );
		info.m_Config = CONFIG_RELEASE;
		info.m_BuildName = "Release";
		if ( strstr( OLE2T(configName), "Debug" ) != 0 )
		{
			info.m_Config = CONFIG_DEBUG;
			info.m_BuildName = "Debug";
		}

		// Not using MFC...
		info.m_pConfig->AddToolSettings("mfc", "0", varj);

		SetupCompilerSettings(info);
		SetupResourceCompilerSettings(info);
		if (info.m_Project != PROJECT_LIB)
		{
			SetupLinkerSettings(info);

			if (!info.m_Console)
				SetupMIDLSettings(info);
		}
		else
		{
			SetupLibSettings(info);
		}
		SetupCustomBuildSteps(info);
		SetupBrowseSettings(info);

		info.m_pConfig->MakeCurrentSettingsDefault();
	}
}
	

// Here we define one instance of the CValvelibAppWiz class.  You can access
//  m_Dictionary and any other public members of this class through the
//  global Valvelibaw.
CValvelibAppWiz Valvelibaw;

