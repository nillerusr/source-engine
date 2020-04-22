//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Manages the set of application configuration options.
//
//=============================================================================//

#include "stdafx.h"
#include "Options.h"
#include "hammer.h"
#include "MainFrm.h"
#include "mapdoc.h"
#include "KeyValues.h"
#include "ConfigManager.h"
#include "GlobalFunctions.h"
#include "CustomMessages.h"
#include "OptionProperties.h"
#include <process.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


const char GAMECFG_SIG[] = "Game Configurations File\r\n\x1a";
const float GAMECFG_VERSION = 1.4f;

static const char *pszGeneral = "General";
static const char *pszView2D = "2D Views";
static const char *pszView3D = "3D Views";
static const char *g_szColors = "Custom2DColors";

const int iThisVersion = 2;

// So File | Open will be in the right directory.
char *g_pMapDir = NULL;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsConfigs::COptionsConfigs(void)
{
	nConfigs = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
COptionsConfigs::~COptionsConfigs(void)
{
	for (int i = 0; i < nConfigs; i++)
	{
		CGameConfig *pConfig = Configs[i];
		if (!pConfig)
			continue;

		delete pConfig;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CGameConfig *COptionsConfigs::AddConfig(void)
{
	CGameConfig *pConfig = new CGameConfig;
	Configs.SetAtGrow(nConfigs++, pConfig);

	return pConfig;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwID - 
//			piIndex - 
// Output : 
//-----------------------------------------------------------------------------
CGameConfig *COptionsConfigs::FindConfig(DWORD dwID, int *piIndex)
{
	for(int i = 0; i < nConfigs; i++)
	{
		if(Configs[i]->dwID == dwID)
		{
			if(piIndex)
				piIndex[0] = i;
			return Configs[i];
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Looks for a game configuration with a given mod directory.
//-----------------------------------------------------------------------------
CGameConfig *COptionsConfigs::FindConfigForGame(const char *szGame)
{
	for (int i = 0; i < nConfigs; i++)
	{
		char *pszGameDir = Configs[i]->m_szModDir;
		if ( Q_stricmp( pszGameDir, szGame ) == 0 )
			return Configs[i];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns the number of game configurations successfully imported.
//-----------------------------------------------------------------------------
int COptionsConfigs::ImportOldGameConfigs(const char *pszFileName)
{
	int nConfigsRead = 0;

	char szRootDir[MAX_PATH];
	char szFullPath[MAX_PATH];
	APP()->GetDirectory(DIR_PROGRAM, szRootDir);
	Q_MakeAbsolutePath( szFullPath, MAX_PATH, pszFileName, szRootDir );
	std::fstream file( szFullPath, std::ios::in | std::ios::binary );
	if (file.is_open())
	{
		// Read sig.
		char szSig[sizeof(GAMECFG_SIG)];
		file.read(szSig, sizeof szSig);
		if (!memcmp(szSig, GAMECFG_SIG, sizeof szSig))
		{
			// Read version.
			float fThisVersion;
			file.read((char *)&fThisVersion, sizeof(fThisVersion));
			if ((fThisVersion >= 1.0) && (fThisVersion <= GAMECFG_VERSION))
			{
				// Read number of configs.
				int nTotalConfigs;
				file.read((char *)&nTotalConfigs, sizeof(nTotalConfigs));

				// Load each config.
				for (int i = 0; i < nTotalConfigs; i++)
				{
					CGameConfig *pConfig = AddConfig();
					pConfig->Import(file, fThisVersion);
					nConfigsRead++;

					if (!g_pMapDir)
					{
						g_pMapDir = (char *)pConfig->szMapDir;
					}
				}
			}
		}

		file.close();
	}

	return(nConfigsRead);
}

// Our call to "new" will be hosed without this header
#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool COptionsConfigs::ResetGameConfigs( bool bOverwrite )
{
	CGameConfigManager mgr;
	int nNumLoaded = 0;

	mgr.SetBaseDirectory( m_strConfigDir );

	if ( bOverwrite )
	{
		// Reset the configurations on the disk
		mgr.ResetConfigs();
		
		// Load the newly changed game configs
		nNumLoaded = LoadGameConfigs();
	}
	else
	{
		// Simply get the keyvalue block from the manager and parse that
		KeyValues *pDefaultConfigs = new KeyValues( "Defaults" );

		if ( mgr.GetDefaultGameBlock( pDefaultConfigs ) == false )
			return false;
		
		// Load from the blocks
		nNumLoaded = LoadGameConfigsBlock( pDefaultConfigs ); 

		// Clean up
		pDefaultConfigs->deleteThis();
	}

	return ( nNumLoaded > 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pBlock - 
// Output : int
//-----------------------------------------------------------------------------
int COptionsConfigs::LoadGameConfigsBlock( KeyValues *pBlock )
{
	if ( pBlock == NULL )
		return 0;

	int nConfigsRead = 0;

	for ( KeyValues *pKey = pBlock->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey() )
	{
		CGameConfig *pConfig = AddConfig();
		if ( pConfig != NULL )
		{	
			if ( pConfig->Load( pKey ) )
			{
				nConfigsRead++;
			}
		}
	}

	return nConfigsRead;
}

//-----------------------------------------------------------------------------
// Purpose: Loads all the game configs from disk.
// Output : Returns the number of game configurations successfully loaded.
//-----------------------------------------------------------------------------
int COptionsConfigs::LoadGameConfigs()
{
	//
	// Read game configs - this is from an external file so we can distribute it
	// with the editor as a set of defaults.
	//
	// Older versions of the editor used a binary file. Try that first.
	//
	int nConfigsRead = ImportOldGameConfigs("GameCfg.wc");
	if (nConfigsRead > 0)
	{
		// This will cause a double conversion, from .wc to .ini to .txt, but oh well...
		char szRootDir[MAX_PATH];
		char szFullPath[MAX_PATH];
		APP()->GetDirectory(DIR_PROGRAM, szRootDir);
		Q_MakeAbsolutePath( szFullPath, MAX_PATH, "GameCfg.wc", szRootDir );
		remove( szFullPath );
		char szSaveName[MAX_PATH];
		strcpy(szSaveName, m_strConfigDir);
		Q_AppendSlash(szSaveName, sizeof(szSaveName));
		Q_strcat(szSaveName, "GameCfg.ini", sizeof(szSaveName));
		SaveGameConfigs();
		return(nConfigsRead);
	}

	CGameConfigManager mgr;

	if ( !mgr.LoadConfigs( m_strConfigDir ) )
		return 0;

	KeyValues *pGame = mgr.GetGameBlock();
	if (!pGame)
		return 0;

	// Install the message handler for error messages.
	GDSetMessageFunc(Msg);

	// Load from the blocks
	nConfigsRead = LoadGameConfigsBlock( pGame );

	return nConfigsRead;
}


//-----------------------------------------------------------------------------
// Purpose: Saves all the cgame configurations to disk.
// Input  : *pszFileName - 
//-----------------------------------------------------------------------------
void COptionsConfigs::SaveGameConfigs()
{
	// Only do this if we've got configs to save!
	if ( GetGameConfigCount() == 0 )
		return;

	CGameConfigManager mgr;

	if ( mgr.LoadConfigs( m_strConfigDir ) == false )
		return;

	// Get the global configuration data
	KeyValues *pGame = mgr.GetGameBlock();
	
	// For each Hammer known configuation, update the values in the global configs
	for ( int i = 0; i < nConfigs; i++ )
	{
		KeyValues *pConfig = pGame->FindKey(Configs.GetAt(i)->szName);
		
		// Add the configuration if it wasn't found
		if ( pConfig == NULL )
		{
			pConfig = pGame->CreateNewKey();
			if ( pConfig == NULL )
			{
				// FIXME: fatal error
				return;
			}
		}

		// Update the changes
		Configs.GetAt(i)->Save(pConfig);
	}

	// For each global configuration, remove any configs Hammer has deleted
	bool bFoundConfig;
	KeyValues *pConfig = pGame->GetFirstTrueSubKey();
	while ( pConfig != NULL )
	{
		// Search through all the configs Hammer knows of for a matching name
		bFoundConfig = false;
		for ( int i = 0; i < nConfigs; i++ )
		{
			if ( !Q_stricmp( pConfig->GetName(), Configs.GetAt(i)->szName ) )
			{
				bFoundConfig = true;
				break;
			}
		}

		// Move along to the next config
		if ( bFoundConfig )
		{
			pConfig = pConfig->GetNextTrueSubKey();
			continue;
		}

		// Delete the configuration block if we didn't find it
		KeyValues *pNextConfig = pConfig->GetNextTrueSubKey();
		pGame->RemoveSubKey( pConfig );
		pConfig->deleteThis();
		pConfig = pNextConfig;
	}
	
	// Save the resulting changes
	mgr.SaveConfigs( m_strConfigDir );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptions::COptions(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Looks for the Valve Hammer Editor registry settings and returns whether
//			they were found.
//-----------------------------------------------------------------------------
static bool HammerSettingsFound(void)
{
	bool bFound = false;
	HKEY hkeySoftware;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_READ | KEY_WRITE, &hkeySoftware) == ERROR_SUCCESS)
	{
		HKEY hkeyValve;
		if (RegOpenKeyEx(hkeySoftware, "Valve", 0, KEY_READ | KEY_WRITE, &hkeyValve) == ERROR_SUCCESS)
		{
			HKEY hkeyHammer;
			if (RegOpenKeyEx(hkeyValve, "Hammer", 0, KEY_READ | KEY_WRITE, &hkeyHammer) == ERROR_SUCCESS)
			{
				HKEY hkeyConfigured;
				if (RegOpenKeyEx(hkeyHammer, "Configured", 0, KEY_READ | KEY_WRITE, &hkeyConfigured) == ERROR_SUCCESS)
				{
					bFound = true;
					RegCloseKey(hkeyConfigured);
				}
				RegCloseKey(hkeyHammer);
			}
			RegCloseKey(hkeyValve);
		}
		RegCloseKey(hkeySoftware);
	}

	return bFound;
}


//-----------------------------------------------------------------------------
// Purpose: Looks for the Valve Hammer Editor registry settings and returns whether
//			they were found.
//-----------------------------------------------------------------------------
static bool ValveHammerEditorSettingsFound(void)
{
	bool bFound = false;
	HKEY hkeySoftware;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_READ | KEY_WRITE, &hkeySoftware) == ERROR_SUCCESS)
	{
		HKEY hkeyValve;
		if (RegOpenKeyEx(hkeySoftware, "Valve", 0, KEY_READ | KEY_WRITE, &hkeyValve) == ERROR_SUCCESS)
		{
			HKEY hkeyHammer;
			if (RegOpenKeyEx(hkeyValve, "Valve Hammer Editor", 0, KEY_READ | KEY_WRITE, &hkeyHammer) == ERROR_SUCCESS)
			{
				HKEY hkeyConfigured;
				if (RegOpenKeyEx(hkeyHammer, "Configured", 0, KEY_READ | KEY_WRITE, &hkeyConfigured) == ERROR_SUCCESS)
				{
					bFound = true;
					RegCloseKey(hkeyConfigured);
				}
				RegCloseKey(hkeyHammer);
			}
			RegCloseKey(hkeyValve);
		}
		RegCloseKey(hkeySoftware);
	}

	return bFound;
}


//-----------------------------------------------------------------------------
// Purpose: Looks for the Worldcraft registry settings and returns whether they
//			were found.
//-----------------------------------------------------------------------------
static bool WorldcraftSettingsFound(void)
{
	bool bFound = false;
	HKEY hkeySoftware;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_READ | KEY_WRITE, &hkeySoftware) == ERROR_SUCCESS)
	{
		HKEY hkeyValve;
		if (RegOpenKeyEx(hkeySoftware, "Valve", 0, KEY_READ | KEY_WRITE, &hkeyValve) == ERROR_SUCCESS)
		{
			HKEY hkeyWorldcraft;
			if (RegOpenKeyEx(hkeyValve, "Worldcraft", 0, KEY_READ | KEY_WRITE, &hkeyWorldcraft) == ERROR_SUCCESS)
			{
				bFound = true;
				RegCloseKey(hkeyWorldcraft);
			}
			RegCloseKey(hkeyValve);
		}
		RegCloseKey(hkeySoftware);
	}

	return bFound;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool COptions::Init(void)
{
	//
	// If the we have no registry settings and the "Valve Hammer Editor" registry tree exists,
	// import settings from there. If that isn't found, try "Worldcraft".
	//
	bool bWCSettingsFound = false;
	bool bVHESettingsFound = false;

	if (!HammerSettingsFound())
	{
		bVHESettingsFound = ValveHammerEditorSettingsFound();
		if (!bVHESettingsFound)
		{
			bWCSettingsFound = WorldcraftSettingsFound();
		}
	}

	if (bVHESettingsFound)
	{
		APP()->BeginImportVHESettings();
	}
	else if (bWCSettingsFound)
	{
		APP()->BeginImportWCSettings();
	}

	SetDefaults();
	
	if (!Read())
	{
		return false;
	}

	if (bVHESettingsFound || bWCSettingsFound)
	{
		APP()->EndImportSettings();
	}

	//
	// Notify appropriate windows of new settings.
	// dvs: is all this necessary?
	//
	CMainFrame *pMainWnd = GetMainWnd();
	if (pMainWnd != NULL)
	{
		pMainWnd->SetBrightness(textures.fBrightness);

		pMainWnd->UpdateAllDocViews( MAPVIEW_OPTIONS_CHANGED );
		
		// FIXME: can't do this before the filesystem is initialized
		//pMainWnd->GlobalNotify(WM_GAME_CHANGED);
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables texture locking.
// Input  : b - TRUE to enable texture locking, FALSE to disable.
// Output : Returns the previous value of the texture locking flag.
//-----------------------------------------------------------------------------
BOOL COptions::SetLockingTextures(BOOL b)
{
	BOOL bOld = general.bLockingTextures;
	general.bLockingTextures = b;
	return(bOld);
}


//-----------------------------------------------------------------------------
// Purpose: Returns TRUE if texture locking is enabled, FALSE if not.
//-----------------------------------------------------------------------------
BOOL COptions::IsLockingTextures(void)
{
	return(general.bLockingTextures);
}

BOOL COptions::SetScaleLockingTextures(BOOL b)
{
	BOOL bOld = general.bScaleLockingTextures;
	general.bScaleLockingTextures = b;
	return(bOld);
}

BOOL COptions::IsScaleLockingTextures(void)
{
	return general.bScaleLockingTextures;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether new faces should be world aligned or face aligned.
//-----------------------------------------------------------------------------
TextureAlignment_t COptions::GetTextureAlignment(void)
{
	return(general.eTextureAlignment);
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether new faces should be world aligned or face aligned.
// Input  : eTextureAlignment - TEXTURE_ALIGN_WORLD or TEXTURE_ALIGN_FACE.
// Output : Returns the old setting for texture alignment.
//-----------------------------------------------------------------------------
TextureAlignment_t COptions::SetTextureAlignment(TextureAlignment_t eTextureAlignment)
{
	TextureAlignment_t eOld = general.eTextureAlignment;
	general.eTextureAlignment = eTextureAlignment;
	return(eOld);
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether helpers should be hidden or shown.
//-----------------------------------------------------------------------------
bool COptions::GetShowHelpers(void)
{
	return (general.bShowHelpers == TRUE);
}

bool COptions::IsVGUIModelBrowserEnabled()
{
	return (general.bUseVGUIModelBrowser == TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether helpers should be hidden or shown.
//-----------------------------------------------------------------------------
void COptions::SetShowHelpers(bool bShow)
{
	general.bShowHelpers = bShow ? TRUE : FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: Loads the application configuration settings.
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
bool COptions::Read(void)
{
	if (!APP()->GetProfileInt("Configured", "Configured", 0))
	{
		return false;
	}

	DWORD dwTime = APP()->GetProfileInt("Configured", "Installed", time(NULL));
	CTimeSpan ts(time(NULL) - dwTime);
	uDaysSinceInstalled = ts.GetDays();

	int i, iSize;
	CString str;

	// read texture info - it's stored in the general section from
	//  an old version, but this doesn't matter much.
	iSize = APP()->GetProfileInt(pszGeneral, "TextureFileCount", 0);
	if(iSize)
	{
		// make sure default is removed
		textures.nTextureFiles = 0;
		textures.TextureFiles.RemoveAll();
		// read texture file names
		for(i = 0; i < iSize; i++)
		{
			str.Format("TextureFile%d", i);
			str = APP()->GetProfileString(pszGeneral, str);
			if(GetFileAttributes(str) == 0xffffffff)
			{
				// can't find
				continue;
			}
			textures.TextureFiles.Add(str);
			textures.nTextureFiles++;
		}
	}
	else
	{
		// SetDefaults() added 'textures.wad' to the list
	}
	textures.fBrightness = float(APP()->GetProfileInt(pszGeneral, "Brightness", 10)) / 10.0;

	// load general info
	general.nMaxCameras = APP()->GetProfileInt(pszGeneral, "Max Cameras", 100);
	general.iUndoLevels = APP()->GetProfileInt(pszGeneral, "Undo Levels", 50);
	general.bLockingTextures = APP()->GetProfileInt(pszGeneral, "Locking Textures", TRUE);
	general.bScaleLockingTextures = APP()->GetProfileInt(pszGeneral, "Scale Locking Textures", FALSE);
	general.eTextureAlignment = (TextureAlignment_t)APP()->GetProfileInt(pszGeneral, "Texture Alignment", TEXTURE_ALIGN_WORLD);
	general.bLoadwinpos = APP()->GetProfileInt(pszGeneral, "Load Default Positions", TRUE);
	general.bIndependentwin = APP()->GetProfileInt(pszGeneral, "Independent Windows", FALSE);
	general.bGroupWhileIgnore = APP()->GetProfileInt(pszGeneral, "GroupWhileIgnore", FALSE);
	general.bStretchArches = APP()->GetProfileInt(pszGeneral, "StretchArches", TRUE);
	general.bShowHelpers = APP()->GetProfileInt(pszGeneral, "Show Helpers", TRUE);
	general.bCheckVisibleMapErrors = APP()->GetProfileInt(pszGeneral, "Visible Map Errors", FALSE);
	general.iTimeBetweenSaves = APP()->GetProfileInt(pszGeneral, "Time Between Saves", 15);
	general.iMaxAutosaveSpace = APP()->GetProfileInt(pszGeneral, "Max Autosave Space", 100);
	general.iMaxAutosavesPerMap = APP()->GetProfileInt(pszGeneral, "Max Saves Per Map", 5);
	general.bEnableAutosave = APP()->GetProfileInt(pszGeneral, "Autosaves Enabled", 1);
	general.bClosedCorrectly = APP()->GetProfileInt(pszGeneral, "Closed Correctly", TRUE);
	general.bUseVGUIModelBrowser = APP()->GetProfileInt(pszGeneral, "VGUI Model Browser", TRUE);	
	general.bShowHiddenTargetsAsBroken = APP()->GetProfileInt(pszGeneral, "Show Hidden Targets As Broken", TRUE);	
	general.bRadiusCulling = APP()->GetProfileInt(pszGeneral, "Use Radius Culling", FALSE);

	char szDefaultAutosavePath[MAX_PATH];
	V_strcpy_safe( szDefaultAutosavePath, APP()->GetProfileString( pszGeneral, "Directory", "C:" ) );
	V_strcpy_safe( szDefaultAutosavePath, "\\HammerAutosave\\" );
	strcpy( general.szAutosaveDir, APP()->GetProfileString("General", "Autosave Dir", szDefaultAutosavePath));
	if ( Q_strlen( general.szAutosaveDir ) == 0 )
	{
		strcpy( general.szAutosaveDir, szDefaultAutosavePath );
	}
	APP()->SetDirectory( DIR_AUTOSAVE, general.szAutosaveDir );	

	// read view2d
	view2d.bCrosshairs = APP()->GetProfileInt(pszView2D, "Crosshairs", FALSE);
	view2d.bGroupCarve = APP()->GetProfileInt(pszView2D, "GroupCarve", TRUE);
	view2d.bScrollbars = APP()->GetProfileInt(pszView2D, "Scrollbars", TRUE);
	view2d.bRotateConstrain = APP()->GetProfileInt(pszView2D, "RotateConstrain", FALSE);
	view2d.bDrawVertices = APP()->GetProfileInt(pszView2D, "Draw Vertices", TRUE);
	view2d.bDrawModels = APP()->GetProfileInt(pszView2D, "Draw Models", TRUE);
	view2d.bWhiteOnBlack = APP()->GetProfileInt(pszView2D, "WhiteOnBlack", TRUE);
	view2d.bGridHigh1024 = APP()->GetProfileInt(pszView2D, "GridHigh1024", TRUE);
	view2d.bGridHigh10 = APP()->GetProfileInt(pszView2D, "GridHigh10", TRUE);
	view2d.bHideSmallGrid = APP()->GetProfileInt(pszView2D, "HideSmallGrid", TRUE);
	view2d.bNudge = APP()->GetProfileInt(pszView2D, "Nudge", FALSE);
	view2d.bOrientPrimitives = APP()->GetProfileInt(pszView2D, "OrientPrimitives", FALSE);
	view2d.bAutoSelect = APP()->GetProfileInt(pszView2D, "AutoSelect", FALSE);
	view2d.bSelectbyhandles = APP()->GetProfileInt(pszView2D, "SelectByHandles", FALSE);
	view2d.iGridIntensity = APP()->GetProfileInt(pszView2D, "GridIntensity", 30);
	view2d.iDefaultGrid = APP()->GetProfileInt(pszView2D, "Default Grid", 64);
	view2d.iGridHighSpec = APP()->GetProfileInt(pszView2D, "GridHighSpec", 8);
	view2d.bKeepclonegroup = APP()->GetProfileInt(pszView2D, "Keepclonegroup", TRUE);
	view2d.bGridHigh64 = APP()->GetProfileInt(pszView2D, "Gridhigh64", TRUE);
	view2d.bGridDots = APP()->GetProfileInt(pszView2D, "GridDots", FALSE);
	view2d.bCenteroncamera = APP()->GetProfileInt(pszView2D, "Centeroncamera", FALSE);
	view2d.bUsegroupcolors = APP()->GetProfileInt(pszView2D, "Usegroupcolors", TRUE);

	// read view3d
	view3d.bHardware = APP()->GetProfileInt(pszView3D, "Hardware", FALSE);
	view3d.bReverseY = APP()->GetProfileInt(pszView3D, "Reverse Y", TRUE);
	view3d.iBackPlane = APP()->GetProfileInt(pszView3D, "BackPlane", 5000);
	view3d.bUseMouseLook = APP()->GetProfileInt(pszView3D, "UseMouseLook", TRUE);
	view3d.nModelDistance = APP()->GetProfileInt(pszView3D, "ModelDistance", 400);
	view3d.nDetailDistance = APP()->GetProfileInt(pszView3D, "DetailDistance", 1200);
	view3d.bAnimateModels = APP()->GetProfileInt(pszView3D, "AnimateModels", FALSE);
	view3d.nForwardSpeedMax = APP()->GetProfileInt(pszView3D, "ForwardSpeedMax", 1000);
	view3d.nTimeToMaxSpeed = APP()->GetProfileInt(pszView3D, "TimeToMaxSpeed", 500);
	view3d.bFilterTextures = APP()->GetProfileInt(pszView3D, "FilterTextures", TRUE);
	view3d.bReverseSelection = APP()->GetProfileInt(pszView3D, "ReverseSelection", FALSE);
	view3d.fFOV = 90;
	view3d.fLightConeLength = 10;
		
	ReadColorSettings();

	//
	// If we can't load any game configurations, pop up the options screen.
	//
	if (configs.LoadGameConfigs() == 0)
	{
		if (!RunConfigurationDialog())
			return false;
	}

	//
	// By default use the first config.
	//
	if (configs.nConfigs > 0)
	{
		g_pGameConfig = configs.Configs.GetAt(0);
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool COptions::RunConfigurationDialog()
{
	CString strText;
	strText.LoadString(IDS_NO_CONFIGS_AVAILABLE);
	if (MessageBox(NULL, strText, "First Time Setup", MB_ICONQUESTION | MB_YESNO) == IDYES)
	{
		APP()->OpenURL(ID_HELP_FIRST_TIME_SETUP, GetMainWnd()->GetSafeHwnd());
	}

	COptionProperties dlg("Configure Hammer");

	do
	{
		if (dlg.DoModal() != IDOK)
		{
			return false;
		}

		if (configs.nConfigs == 0)
		{
			MessageBox(NULL, "You must create at least one game configuration before using Hammer.", "First Time Setup", MB_ICONEXCLAMATION | MB_OK);
		}

	} while (configs.nConfigs == 0);

	Options.Write( TRUE, TRUE );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptions::ReadColorSettings(void)
{ 
	colors.bUseCustom = (APP()->GetProfileInt(g_szColors, "UseCustom", 0) != 0);
	if (colors.bUseCustom)
	{
		colors.clrAxis = APP()->GetProfileColor(g_szColors, "Grid0", 0 , 100, 100);
		colors.bScaleAxisColor = (APP()->GetProfileInt(g_szColors, "ScaleGrid0", 0) != 0);
		colors.clrGrid = APP()->GetProfileColor(g_szColors, "Grid", 50 , 50, 50);
		colors.bScaleGridColor = (APP()->GetProfileInt(g_szColors, "ScaleGrid", 1) != 0);
		colors.clrGrid10 = APP()->GetProfileColor(g_szColors, "Grid10", 40 , 40, 40);
		colors.bScaleGrid10Color = (APP()->GetProfileInt(g_szColors, "ScaleGrid10", 1) != 0);
		colors.clrGrid1024 = APP()->GetProfileColor(g_szColors, "Grid1024", 40 , 40, 40);
		colors.bScaleGrid1024Color = (APP()->GetProfileInt(g_szColors, "ScaleGrid1024", 1) != 0);
		colors.clrGridDot = APP()->GetProfileColor(g_szColors, "GridDot", 128, 128, 128);
		colors.bScaleGridDotColor = (APP()->GetProfileInt(g_szColors, "ScaleGridDot", 1) != 0);

		colors.clrBrush = APP()->GetProfileColor(g_szColors, "LineColor", 0, 0, 0);
		colors.clrEntity = APP()->GetProfileColor(g_szColors, "Entity", 220, 30, 220);
		colors.clrVertex = APP()->GetProfileColor(g_szColors, "Vertex", 0, 0, 0);
		colors.clrBackground = APP()->GetProfileColor(g_szColors, "Background", 0, 0, 0);
		colors.clrToolHandle = APP()->GetProfileColor(g_szColors, "HandleColor", 0, 0, 0);
		colors.clrToolBlock = APP()->GetProfileColor(g_szColors, "BoxColor", 0, 0, 0);
		colors.clrToolSelection = APP()->GetProfileColor(g_szColors, "ToolSelect", 0, 0, 0);
		colors.clrToolMorph = APP()->GetProfileColor(g_szColors, "Morph", 255, 0, 0);
		colors.clrToolPath = APP()->GetProfileColor(g_szColors, "Path", 255, 0, 0);
		colors.clrSelection = APP()->GetProfileColor(g_szColors, "Selection", 220, 0, 0);
		colors.clrToolDrag = APP()->GetProfileColor(g_szColors, "ToolDrag", 255, 255, 0);
	}
	else
	{
		if (Options.view2d.bWhiteOnBlack)
		{
			// BLACK BACKGROUND
			colors.clrBackground = RGB(0, 0, 0);
			colors.clrGrid = RGB(255, 255, 255);
			colors.clrGridDot = RGB(255, 255, 255);
			colors.clrGrid1024 = RGB(100, 50, 5);
			colors.clrGrid10 = RGB(255, 255, 255);
			colors.clrAxis = RGB(0, 100, 100);
			colors.clrBrush = RGB(255, 255, 255);
			colors.clrVertex = RGB(255, 255, 255);

			colors.clrToolHandle = RGB(255, 255, 255);
			colors.clrToolBlock = RGB(255, 255, 255);
			colors.clrToolDrag = RGB(255, 255, 0);
		}
		else
		{
			// WHITE BACKGROUND
			colors.clrBackground = RGB(255, 255, 255);
			colors.clrGrid = RGB(50, 50, 50);
			colors.clrGridDot = RGB(40, 40, 40);
			colors.clrGrid1024 = RGB(200, 100, 10);
			colors.clrGrid10 = RGB(40, 40, 40);
			colors.clrAxis = RGB(0, 100, 100);
			colors.clrBrush = RGB(0, 0, 0);
			colors.clrVertex = RGB(0, 0, 0);

			colors.clrToolHandle = RGB(0, 0, 0);
			colors.clrToolBlock = RGB(0, 0, 0);
			colors.clrToolDrag = RGB(0, 0, 255);
		}

		colors.bScaleAxisColor = false;
		colors.bScaleGridColor = true;
		colors.bScaleGrid10Color = true;
		colors.bScaleGrid1024Color = false;
		colors.bScaleGridDotColor = true;

		colors.clrToolSelection = RGB(255, 255, 0);
		colors.clrSelection = RGB(255, 0, 0);
		colors.clrToolMorph = RGB(255, 0, 0);
		colors.clrToolPath = RGB(255, 0, 0);
		colors.clrEntity = RGB(220, 30, 220);
		colors.clrModelCollisionWireframe = RGB( 255, 255, 0 );
		colors.clrModelCollisionWireframeDisabled = RGB( 220, 30, 220 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fOverwrite - 
//-----------------------------------------------------------------------------
void COptions::Write( BOOL fOverwrite, BOOL fSaveConfigs )
{
	APP()->WriteProfileInt("Configured", "Configured", iThisVersion);

	int i, iSize;
	CString str;

	// write texture info - remember, it's stored in general
	iSize = textures.nTextureFiles;
	APP()->WriteProfileInt(pszGeneral, "TextureFileCount", iSize);
	for(i = 0; i < iSize; i++)
	{
		str.Format("TextureFile%d", i);
		APP()->WriteProfileString(pszGeneral, str, textures.TextureFiles[i]);
	}
	APP()->WriteProfileInt(pszGeneral, "Brightness", int(textures.fBrightness * 10));

	// write general
	APP()->WriteProfileInt(pszGeneral, "Max Cameras", general.nMaxCameras);
	APP()->WriteProfileInt(pszGeneral, "Undo Levels", general.iUndoLevels);
	APP()->WriteProfileInt(pszGeneral, "Locking Textures", general.bLockingTextures);
	APP()->WriteProfileInt(pszGeneral, "Scale Locking Textures", general.bScaleLockingTextures);
	APP()->WriteProfileInt(pszGeneral, "Texture Alignment", general.eTextureAlignment);
	APP()->WriteProfileInt(pszGeneral, "Independent Windows", general.bIndependentwin);
	APP()->WriteProfileInt(pszGeneral, "Load Default Positions", general.bLoadwinpos);
	APP()->WriteProfileInt(pszGeneral, "GroupWhileIgnore", general.bGroupWhileIgnore);
	APP()->WriteProfileInt(pszGeneral, "StretchArches", general.bStretchArches);
	APP()->WriteProfileInt(pszGeneral, "Show Helpers", general.bShowHelpers);
	APP()->WriteProfileInt(pszGeneral, "Visible Map Errors", general.bCheckVisibleMapErrors);
	APP()->WriteProfileInt(pszGeneral, "Time Between Saves", general.iTimeBetweenSaves);
	APP()->WriteProfileInt(pszGeneral, "Max Autosave Space", general.iMaxAutosaveSpace);
	APP()->WriteProfileInt(pszGeneral, "Max Saves Per Map", general.iMaxAutosavesPerMap);
	APP()->WriteProfileInt(pszGeneral, "Autosaves Enabled", general.bEnableAutosave);
	APP()->WriteProfileInt(pszGeneral, "Closed Correctly", general.bClosedCorrectly);
	APP()->WriteProfileString(pszGeneral, "Autosave Dir", general.szAutosaveDir);
	APP()->SetDirectory( DIR_AUTOSAVE, general.szAutosaveDir );
	APP()->WriteProfileInt(pszGeneral, "VGUI Model Browser", general.bUseVGUIModelBrowser );
	APP()->WriteProfileInt(pszGeneral, "Show Hidden Targets As Broken", general.bShowHiddenTargetsAsBroken);
	APP()->WriteProfileInt(pszGeneral, "Use Radius Culling", general.bRadiusCulling);


	
	// write view2d
	APP()->WriteProfileInt(pszView2D, "Crosshairs", view2d.bCrosshairs);
	APP()->WriteProfileInt(pszView2D, "GroupCarve", view2d.bGroupCarve);
	APP()->WriteProfileInt(pszView2D, "Scrollbars", view2d.bScrollbars);
	APP()->WriteProfileInt(pszView2D, "RotateConstrain", view2d.bRotateConstrain);
	APP()->WriteProfileInt(pszView2D, "Draw Vertices", view2d.bDrawVertices);
	APP()->WriteProfileInt(pszView2D, "Draw Models", view2d.bDrawModels);
	APP()->WriteProfileInt(pszView2D, "Default Grid", view2d.iDefaultGrid);
	APP()->WriteProfileInt(pszView2D, "WhiteOnBlack", view2d.bWhiteOnBlack);
	APP()->WriteProfileInt(pszView2D, "GridHigh1024", view2d.bGridHigh1024);
	APP()->WriteProfileInt(pszView2D, "GridHigh10", view2d.bGridHigh10);
	APP()->WriteProfileInt(pszView2D, "GridIntensity", view2d.iGridIntensity);
	APP()->WriteProfileInt(pszView2D, "HideSmallGrid", view2d.bHideSmallGrid);
	APP()->WriteProfileInt(pszView2D, "Nudge", view2d.bNudge);
	APP()->WriteProfileInt(pszView2D, "OrientPrimitives", view2d.bOrientPrimitives);
	APP()->WriteProfileInt(pszView2D, "AutoSelect", view2d.bAutoSelect);
	APP()->WriteProfileInt(pszView2D, "SelectByHandles", view2d.bSelectbyhandles);
	APP()->WriteProfileInt(pszView2D, "GridHighSpec", view2d.iGridHighSpec);
	APP()->WriteProfileInt(pszView2D, "KeepCloneGroup", view2d.bKeepclonegroup);
	APP()->WriteProfileInt(pszView2D, "Gridhigh64", view2d.bGridHigh64);
	APP()->WriteProfileInt(pszView2D, "GridDots", view2d.bGridDots);
	APP()->WriteProfileInt(pszView2D, "Centeroncamera", view2d.bCenteroncamera);
	APP()->WriteProfileInt(pszView2D, "Usegroupcolors", view2d.bUsegroupcolors);

	// write view3d
	APP()->WriteProfileInt(pszView3D, "Hardware", view3d.bHardware);
	APP()->WriteProfileInt(pszView3D, "Reverse Y", view3d.bReverseY);
	APP()->WriteProfileInt(pszView3D, "BackPlane", view3d.iBackPlane);
	APP()->WriteProfileInt(pszView3D, "UseMouseLook", view3d.bUseMouseLook);
	APP()->WriteProfileInt(pszView3D, "ModelDistance", view3d.nModelDistance);
	APP()->WriteProfileInt(pszView3D, "DetailDistance", view3d.nDetailDistance);
	APP()->WriteProfileInt(pszView3D, "AnimateModels", view3d.bAnimateModels);
	APP()->WriteProfileInt(pszView3D, "ForwardSpeedMax", view3d.nForwardSpeedMax);
	APP()->WriteProfileInt(pszView3D, "TimeToMaxSpeed", view3d.nTimeToMaxSpeed);
	APP()->WriteProfileInt(pszView3D, "FilterTextures", view3d.bFilterTextures);
	APP()->WriteProfileInt(pszView3D, "ReverseSelection", view3d.bReverseSelection);

	//
	// We don't write custom color settings because there is no GUI for them yet.
	//

	// Write out the game configurations
	if ( fSaveConfigs )
	{
		configs.SaveGameConfigs();
	}
}


void COptions::SetClosedCorrectly(BOOL bClosed)
{
	APP()->WriteProfileInt( pszGeneral, "Closed Correctly", bClosed );	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptions::SetDefaults(void)
{
	BOOL bWrite = FALSE;

	if (APP()->GetProfileInt("Configured", "Configured", 0) != iThisVersion)
	{
		bWrite = TRUE;
	}

	if (APP()->GetProfileInt("Configured", "Installed", 42151) == 42151)
	{
		APP()->WriteProfileInt("Configured", "Installed", time(NULL));
	}

	uDaysSinceInstalled = 0;

	// textures
	textures.nTextureFiles = 1;
	textures.TextureFiles.Add("textures.wad");
	textures.fBrightness = 1.0;

	// general
	general.bIndependentwin = FALSE;
	general.bLoadwinpos = TRUE;
	general.iUndoLevels = 50;
	general.nMaxCameras = 100;
	general.bGroupWhileIgnore = FALSE;
	general.bStretchArches = TRUE;
	general.bLockingTextures = TRUE;
	general.bScaleLockingTextures = FALSE;
	general.bShowHelpers = TRUE;
	general.iTimeBetweenSaves = 15;
	general.iMaxAutosaveSpace = 100;
	general.iMaxAutosavesPerMap = 5;
	general.bEnableAutosave = TRUE;
	general.bClosedCorrectly = TRUE;
	general.bUseVGUIModelBrowser = TRUE;
	general.bShowCollisionModels = FALSE;
	general.bShowDetailObjects = TRUE;
	general.bShowNoDrawBrushes = TRUE;
	
	// view2d
	view2d.bCrosshairs = FALSE;
	view2d.bGroupCarve = TRUE;
	view2d.bScrollbars = TRUE;
	view2d.bRotateConstrain = FALSE;
	view2d.bDrawVertices = TRUE;
	view2d.bDrawModels = TRUE;
	view2d.iDefaultGrid = 64;
	view2d.bWhiteOnBlack = TRUE;
	view2d.bGridHigh1024 = TRUE;
	view2d.bGridHigh10 = TRUE;
	view2d.iGridIntensity = 30;
	view2d.bHideSmallGrid = TRUE;
	view2d.bNudge = FALSE;
	view2d.bOrientPrimitives = FALSE;
	view2d.bAutoSelect = FALSE;
	view2d.bSelectbyhandles = FALSE;
	view2d.iGridHighSpec = 8;
	view2d.bKeepclonegroup = TRUE;
	view2d.bGridHigh64 = FALSE;
	view2d.bGridDots = FALSE;
	view2d.bCenteroncamera = FALSE;
	view2d.bUsegroupcolors = TRUE;

	// view3d
	view3d.bUseMouseLook = TRUE;
	view3d.bHardware = FALSE;
	view3d.bReverseY = FALSE;
	view3d.iBackPlane = 5000;
	view3d.nModelDistance = 400;
	view3d.nDetailDistance = 1200;
	view3d.bAnimateModels = FALSE;
	view3d.nForwardSpeedMax = 1000;
	view3d.nTimeToMaxSpeed = 500;
	view3d.bFilterTextures = TRUE;
	view3d.bReverseSelection = FALSE;
	view3d.bPreviewModelFade = false;

	if ( bWrite )
	{
		Write( FALSE, FALSE );
	}
}


//-----------------------------------------------------------------------------
// Purpose: This is called by the user interface itself when changes are made. 
//			tells the COptions object to notify the parts of the interface.
// Input  : dwOptionsChanged - Flags indicating which options changed.
//-----------------------------------------------------------------------------
void COptions::PerformChanges(DWORD dwOptionsChanged)
{
	CMainFrame *pMainWnd = GetMainWnd();

	if (dwOptionsChanged & secTextures)
	{
		if (pMainWnd != NULL)
		{
			pMainWnd->SetBrightness(textures.fBrightness);
		}
	}

	if (dwOptionsChanged & secView2D)
	{
		ReadColorSettings();

		if (pMainWnd != NULL)
		{
			pMainWnd->UpdateAllDocViews( MAPVIEW_UPDATE_ONLY_2D | MAPVIEW_OPTIONS_CHANGED | MAPVIEW_RENDER_NOW );
		}
	}

	if (dwOptionsChanged & secView3D)
	{
		if (pMainWnd != NULL)
		{
			pMainWnd->UpdateAllDocViews(MAPVIEW_UPDATE_ONLY_3D | MAPVIEW_OPTIONS_CHANGED | MAPVIEW_RENDER_NOW );
		}
	}

	if (dwOptionsChanged & secConfigs)
	{
		if (pMainWnd != NULL)
		{
			pMainWnd->GlobalNotify(WM_GAME_CHANGED);
		}
	}
}

