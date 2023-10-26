//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//
#include <tier0/platform.h>
#ifdef IS_WINDOWS_PC
#include "windows.h"
#endif
#include "vgetlegacydata.h"
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
GetLegacyData::GetLegacyData( Panel *parent, const char *panelName )
 : BaseClass( parent, panelName, true, false, false )
{
	GameUI().PreventEngineHideGameUI();
	SetProportional( true );

	m_LblDesc = new Label( this, "LblGetLegacyDataDescription", "" );

	SetTitle( "", false );
	SetDeleteSelfOnClose( true );
	SetLowerGarnishEnabled( false );
	SetMoveable( false );
	LoadControlSettings( "Resource/UI/BaseModUI/getlegacydata.res" );
}

//=============================================================================
GetLegacyData::~GetLegacyData()
{
	GameUI().AllowEngineHideGameUI();
}

//=============================================================================
void GetLegacyData::OnThink()
{
}

//=============================================================================
void GetLegacyData::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	// If we are still installing then change from the default button state and message defined in the res file
	if ( IsInstalled() && IsInstalling() )
	{
		m_LblDesc->SetText( "#L4D360UI_GetLegacyData_Installing" );

		FindChildByName( "BtnOK" )->SetVisible( true );
		FindChildByName( "BtnNo" )->SetVisible( false );
		FindChildByName( "BtnYes" )->SetVisible( false );

		FindChildByName( "BtnOK" )->SetEnabled( true );
		FindChildByName( "BtnNo" )->SetEnabled( false );
		FindChildByName( "BtnYes" )->SetEnabled( false );
	}
}


//=============================================================================
bool GetLegacyData::IsInstalling()
{
	// legacy support is only needed for L4D2
	return false;

#ifdef IS_WINDOWS_PC
	HKEY hKey = NULL;
	LONG lKeyError = RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\Steam\\Apps\\564", 0, KEY_READ, &hKey);

	if ( lKeyError == ERROR_SUCCESS )
	{
		// Key exists, check if its set to us
		DWORD dwHasAllLocalContent;
		DWORD len = sizeof( dwHasAllLocalContent );
		RegQueryValueEx( hKey, "HasAllLocalContent", NULL, NULL, (LPBYTE)&dwHasAllLocalContent, &len );

		RegCloseKey( hKey );

		return ( dwHasAllLocalContent != 1 );
	}
#endif
	
	return false;
}

//=============================================================================
bool GetLegacyData::IsInstalled()
{
	// legacy support is only needed for L4D2
	return true;


#ifdef IS_WINDOWS_PC
	HKEY hKey = NULL;
	LONG lKeyError = RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\Steam\\Apps\\564", 0, KEY_READ, &hKey);

	if ( lKeyError == ERROR_SUCCESS )
	{
		// Key exists, check if its set to us
		DWORD dwInstalled;
		DWORD len = sizeof( dwInstalled );
		RegQueryValueEx( hKey, "Installed", NULL, NULL, (LPBYTE)&dwInstalled, &len );

		RegCloseKey( hKey );

		return ( dwInstalled != 0 );
	}
#endif

	return false;
}

//=============================================================================
bool GetLegacyData::CheckAndSeeIfShouldShow()
{
	return( !IsInstalled() || IsInstalling() );
}

//=============================================================================
void GetLegacyData::OnCommand(const char *command)
{
	if ( Q_stricmp( command, "Yes" ) == 0 || Q_stricmp( command, "No" ) == 0 )
	{
		Close();

#ifdef IS_WINDOWS_PC
		if ( Q_stricmp( command, "Yes" ) == 0 )
		{
			// App ID for the legacy addon data is 564
			ShellExecute ( 0, "open", "steam://install/564", NULL, 0, SW_SHOW );
		}
#endif

	}
	else
	{
		BaseClass::OnCommand( command );
 	}
}

