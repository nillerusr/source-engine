//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#ifdef _XBOX
#include "xbox/xbox_platform.h"
#include "xbox/xbox_win32stubs.h"
#endif

#if !defined( _X360 )
#include <windows.h>
#endif
#include <stdio.h>

#include "ChangeGameDialog.h"
#include "ModInfo.h"
#include "EngineInterface.h"

#include <vgui_controls/ListPanel.h>
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CChangeGameDialog::CChangeGameDialog(vgui::Panel *parent) : Frame(parent, "ChangeGameDialog")
{
	SetSize(400, 340);
	SetMinimumSize(400, 340);
	SetTitle("#GameUI_ChangeGame", true);

	m_pModList = new ListPanel(this, "ModList");
	m_pModList->SetEmptyListText("#GameUI_NoOtherGamesAvailable");
	m_pModList->AddColumnHeader(0, "ModName", "#GameUI_Game", 128);

	LoadModList();
	LoadControlSettings("Resource/ChangeGameDialog.res");

	// if there's a mod in the list, select the first one
	if (m_pModList->GetItemCount() > 0)
	{
		m_pModList->SetSingleSelectedItem(m_pModList->GetItemIDFromRow(0));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CChangeGameDialog::~CChangeGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Fills the mod list
//-----------------------------------------------------------------------------
void CChangeGameDialog::LoadModList()
{
	// look for third party games
	char szSearchPath[_MAX_PATH + 5];
	Q_strncpy(szSearchPath, "*.*", sizeof( szSearchPath ) );

	// use local filesystem since it has to look outside path system, and will never be used under steam
	WIN32_FIND_DATA wfd;
	HANDLE hResult;
	memset(&wfd, 0, sizeof(WIN32_FIND_DATA));
	
	hResult = FindFirstFile( szSearchPath, &wfd);
	if (hResult != INVALID_HANDLE_VALUE)
	{
		BOOL bMoreFiles;
		while (1)
		{
			if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (Q_strnicmp(wfd.cFileName, ".", 1)))
			{
				// Check for dlls\*.dll
				char szDllDirectory[MAX_PATH + 16];
				Q_snprintf(szDllDirectory, sizeof( szDllDirectory ), "%s\\gameinfo.txt", wfd.cFileName);

				FILE *f = fopen(szDllDirectory, "rb");
				if (f)
				{
					// find the description
					fseek(f, 0, SEEK_END);
					unsigned int size = ftell(f);
					fseek(f, 0, SEEK_SET);
					char *buf = (char *)malloc(size + 1);
					if (fread(buf, 1, size, f) == size)
					{
						buf[size] = 0;

						CModInfo modInfo;
						modInfo.LoadGameInfoFromBuffer(buf);

						if (strcmp(modInfo.GetGameName(), ModInfo().GetGameName()))
						{
							// Add the game directory.
							strlwr(wfd.cFileName);
							KeyValues *itemData = new KeyValues("Mod");
							itemData->SetString("ModName", modInfo.GetGameName());
							itemData->SetString("ModDir", wfd.cFileName);
							m_pModList->AddItem(itemData, 0, false, false);
						}
					}
					free(buf);
					fclose(f);
				}
			}
			bMoreFiles = FindNextFile(hResult, &wfd);
			if (!bMoreFiles)
				break;
		}
		
		FindClose(hResult);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChangeGameDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "OK"))
	{
		if (m_pModList->GetSelectedItemsCount() > 0)
		{
			KeyValues *kv = m_pModList->GetItem(m_pModList->GetSelectedItem(0));
			if (kv)
			{
				// change the game dir and restart the engine
				char szCmd[256];
				Q_snprintf(szCmd, sizeof( szCmd ), "_setgamedir %s\n", kv->GetString("ModDir"));
				engine->ClientCmd_Unrestricted(szCmd);

				// Force restart of entire engine
				engine->ClientCmd_Unrestricted("_restart\n");
			}
		}
	}
	else if (!stricmp(command, "Cancel"))
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}






