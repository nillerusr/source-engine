//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//
#include <tier0/platform.h>
#ifdef IS_WINDOWS_PC
#include "windows.h"
#endif
#include "VAddonAssociation.h"
#include "VGenericConfirmation.h"

#include "EngineInterface.h"

#include "ConfigManager.h"

#include "vgui_controls/Label.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
ConVar cl_support_vpk_assocation( "cl_support_vpk_assocation", "1", FCVAR_ARCHIVE, "Whether vpk associations are enabled for this mod" );
ConVar cl_ignore_vpk_assocation( "cl_ignore_vpk_assocation", "0", FCVAR_ARCHIVE, "Do not ask to set vpk assocation" );

static bool s_checkedNoShow = false;
static bool s_checkedAssociation = false;
static AddonAssociation::EAssociation s_association = AddonAssociation::kAssociation_None;

//=============================================================================
AddonAssociation::AddonAssociation( Panel *parent, const char *panelName )
 : BaseClass( parent, panelName, true, false, false )
 , m_pDoNotAskForAssociation( 0 )
{
	SetProportional( true );

	m_pDoNotAskForAssociation = new CvarToggleCheckButton<CGameUIConVarRef>( 
		this, 
		"CheckButtonAddonAssociation", 
		"#L4D360UI_Cloud_KeepInSync_Tip", 
		"cl_ignore_vpk_assocation",
		true );

	SetTitle( "", false );
	SetDeleteSelfOnClose( true );
	SetLowerGarnishEnabled( false );
	SetMoveable( false );
}

//=============================================================================
AddonAssociation::~AddonAssociation()
{
}

//=============================================================================
void AddonAssociation::OnThink()
{
}

//=============================================================================
static void GetAddonInstallerUtilityPath( char path[MAX_PATH] )
{
	char p[MAX_PATH];
	GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), p, sizeof( p ) );
	Q_StripLastDir( p, sizeof( p ) );	// Get rid of the filename.
	Q_StripTrailingSlash( p );
	Q_strncat( p, "\\bin\\addoninstaller.exe", sizeof( p ) );

	Q_strncpy( path, p, MAX_PATH );
}

//=============================================================================
static void RegisterAssocation( bool showFailure )
{
#ifdef IS_WINDOWS_PC
	char addonInstallPath[MAX_PATH];
	GetAddonInstallerUtilityPath( addonInstallPath );

	char commandLine[MAX_PATH];
	Q_snprintf( commandLine, sizeof( commandLine ), "\"%s\" /register", addonInstallPath );

	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );

	DWORD dwFlags = 0;
	if ( !CreateProcess( 
		0, 
		commandLine, 
		NULL,                                                                                                    // security
		NULL,
		false,
		dwFlags,                                                                                              // flags
		NULL,                                                                                                    // environment
		NULL,                                                                                                    // current directory
		&si,
		&pi ) )
	{
#ifndef ERROR_ELEVATION_REQUIRED
#define ERROR_ELEVATION_REQUIRED 740L
#endif
		DWORD err = GetLastError();
		if ( err == ERROR_ELEVATION_REQUIRED )
		{
			// Try again with RunAs
			SHELLEXECUTEINFOA shExecInfo;

			shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
			
			shExecInfo.fMask = NULL;
			shExecInfo.hwnd = NULL;
			shExecInfo.lpVerb = "runas";
			shExecInfo.lpFile = addonInstallPath;
			shExecInfo.lpParameters = "/register";
			shExecInfo.lpDirectory = NULL;
			shExecInfo.nShow = SW_MAXIMIZE;
			shExecInfo.hInstApp = NULL;
			
			if ( !ShellExecuteEx(&shExecInfo) )
			{	
				err = GetLastError();
			}
			else
			{
				err = ERROR_SUCCESS;
			}
		}

		if ( err != ERROR_SUCCESS )
		{
			char *pszError;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&pszError, 0, NULL);
			Msg("Failed to execute addoninstaller(%d): %s\n", err, pszError );
			LocalFree(pszError);

			if ( showFailure )
			{
				GenericConfirmation* confirmation = 
					static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, CBaseModPanel::GetSingleton().OpenWindow( WT_MAINMENU, NULL ), false ) );
				GenericConfirmation::Data_t data;
				data.pWindowTitle = "#L4D360UI_AddonAssociation_Failure_Title";
				data.pMessageText = "#L4D360UI_AddonAssociation_Failure_Description";
				data.bOkButtonEnabled = true;
				data.bCheckBoxEnabled = true;
				data.pCheckBoxCvarName = "cl_ignore_vpk_assocation";
				data.pCheckBoxLabelText = "#L4D360UI_AddonAssociation_Failure_DoNotAsk";
				confirmation->SetUsageData(data);
			}
			return;
		}
	}

	// If here we assume it was successful
	s_association = AddonAssociation::kAssociation_Ok;
#endif
}

//=============================================================================
AddonAssociation::EAssociation AddonAssociation::VPKAssociation()
{
#ifdef IS_WINDOWS_PC
	if ( cl_ignore_vpk_assocation.GetBool() )
	{
		return kAssociation_Ok;
	}

	// If we cached a check, do it
	if ( s_checkedAssociation )
	{
		return s_association;
	}

	HKEY hKey = NULL;
	LONG vpkKeyError = RegOpenKeyEx( HKEY_CLASSES_ROOT, ".vpk", 0, KEY_READ, &hKey);

	if ( vpkKeyError != ERROR_SUCCESS )
	{
		s_association = kAssociation_None;
	}
	else
	{
		// Key exists, check if its set to us
		char assocation[MAX_PATH];
		DWORD len = sizeof( assocation );
		LONG vpkAssociationError = RegQueryValueEx( hKey, NULL, NULL, NULL, (LPBYTE)assocation, &len );

		if ( vpkAssociationError != ERROR_SUCCESS )
		{
			// no value
			s_association = kAssociation_None;
		}
		else
		{
			if ( Q_stricmp( assocation, "sourceaddonfile" ) != 0 )
			{
				s_association = kAssociation_Other; // assigned to someone else
			}
			else
			{
				s_association = kAssociation_Ok; // Assigned to us
			}
		}
	}

	s_checkedAssociation = true;
	return s_association;
#else
	return kAssociation_Ok;
#endif
}

//=============================================================================
bool AddonAssociation::CheckAndSeeIfShouldShow()
{
	if ( !cl_support_vpk_assocation.GetBool() )
		// VPK associations unsupported
		return false;

	if ( s_checkedNoShow )
		return false;

	s_checkedNoShow = true;

	switch ( VPKAssociation() )
	{
		// Not found, assume it
		case kAssociation_None :
			Msg("Executing addoninstaller to register with vpk files\n");
			RegisterAssocation( true );
			break;

		// Set to someone else, ask
		case kAssociation_Other:
			return true;

		default:
		case kAssociation_Ok:
			break;
	}

	return false;
}

//=============================================================================
void AddonAssociation::OnCommand(const char *command)
{
	if ( Q_stricmp( command, "Yes" ) == 0 || Q_stricmp( command, "No" ) == 0 )
	{
		if ( m_pDoNotAskForAssociation && m_pDoNotAskForAssociation->IsSelected() )
		{
			cl_ignore_vpk_assocation.SetValue( true );
		}

		Close();

		if ( Q_stricmp( command, "Yes" ) == 0 )
		{
			RegisterAssocation( true );
		}

	}
	else
	{
		BaseClass::OnCommand( command );
 	}
}

