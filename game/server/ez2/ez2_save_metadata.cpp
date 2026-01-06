//=============================================================================//
//
// Purpose: Creates custom metadata for each save file
//
//=============================================================================//

#include "cbase.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "npc_wilson.h"
#include "filesystem.h"

//=============================================================================
//=============================================================================
class CCustomSaveMetadata : public CAutoGameSystem
{
public:
	bool Init()
	{
		ConVarRef save_history_count("save_history_count");
		ConVarRef sv_save_history_count_archived("sv_save_history_count_archived");
		Msg("Setting save_history_count to %d....\n", sv_save_history_count_archived.GetInt());
		save_history_count.SetValue(sv_save_history_count_archived.GetInt());
		return true;
	}


	void OnSave()
	{
		char const *pchSaveFile = engine->GetSaveFileName();
		if (!pchSaveFile || !pchSaveFile[0])
		{
			Msg( "NO SAVE FILE\n" );
			return;
		}

		char name[MAX_PATH];
		Q_strncpy( name, pchSaveFile, sizeof( name ) );
		Q_strlower( name );
		Q_SetExtension( name, ".txt", sizeof( name ) );
		Q_FixSlashes( name );

		ConVarRef save_history_count("save_history_count");
		RotateFile(name, pchSaveFile, save_history_count.GetInt());

		KeyValues *pCustomSaveMetadata = new KeyValues( "CustomSaveMetadata" );
		if (pCustomSaveMetadata)
		{
			// E:Z2 Version
			{
				ConVarRef ez2Version( "ez2_version" );
				pCustomSaveMetadata->SetString( "ez2_version", ez2Version.GetString() );
			}

			// Map Version
			pCustomSaveMetadata->SetInt( "mapversion", gpGlobals->mapversion );

			// Map name
			pCustomSaveMetadata->SetString("map", STRING(gpGlobals->mapname));

			// UNDONE: Host Name
			//{
			//	char szHostName[128];
			//	gethostname( szHostName, sizeof( szHostName ) );
			//	pCustomSaveMetadata->SetString( "hostname", szHostName );
			//}

			// OS Platform
			{
#ifdef _WIN32
				pCustomSaveMetadata->SetString( "platform", "windows" );
#elif defined(LINUX)
				pCustomSaveMetadata->SetString( "platform", "linux" );
#endif
			}

			// Steam Deck
			{
				const char *pszSteamDeckEnv = getenv( "SteamDeck" );
				pCustomSaveMetadata->SetBool( "is_deck", (pszSteamDeckEnv && *pszSteamDeckEnv) ); // g_pSteamInput->IsSteamRunningOnSteamDeck()
			}

			// Wilson
			if (CNPC_Wilson *pWilson = CNPC_Wilson::GetWilson())
			{
				pCustomSaveMetadata->SetBool("wilson", true );
			}

			Msg( "Saving custom metadata to %s\n", name );

			pCustomSaveMetadata->SaveToFile( filesystem, name, "MOD" );
		}
		pCustomSaveMetadata->deleteThis();
	}

protected:

	// Recursive function to rotate metadata files
	void RotateFile(char pszFilenameToRotate[MAX_PATH], char const* pchOriginalSaveFilename, int iSaveHistoryCount, int iCounter=1)
	{
		char pszNewMetadataFilename[MAX_PATH];
		char pszNewSaveFilename[MAX_PATH];


		// Base case: File does not exist
		if (!filesystem->FileExists(pszFilenameToRotate, "MOD"))
		{
			return;
		}

		// If this file is not an autosave or quicksave, do not rotate
		const char* pszFileName = V_GetFileName(pchOriginalSaveFilename);
		if (!(V_stricmp(pszFileName, "autosave.sav") == 0 || V_stricmp(pszFileName, "quick.sav") == 0))
		{
			return;
		}

		// autosave.txt -> autosave01.txt
		// quick.txt -> quick01.txt
		Q_StripExtension(pchOriginalSaveFilename, pszNewMetadataFilename, sizeof(pszNewMetadataFilename));
		Q_strlower(pszNewMetadataFilename);
		Q_FixSlashes(pszFilenameToRotate);

		// If the counter is below 10, append a leading 0
		const char* pszMetadataFileSuffix = iCounter < 10 ? CFmtStr("0%d", iCounter) : CFmtStr("%d", iCounter);

		Q_strncat(pszNewMetadataFilename, pszMetadataFileSuffix, sizeof(pszNewMetadataFilename));
		Q_strncpy(pszNewSaveFilename, pszNewMetadataFilename, sizeof(pszNewMetadataFilename));

		// Add file extensions
		Q_strncat(pszNewMetadataFilename, ".txt", sizeof(pszNewMetadataFilename));
		Q_strncat(pszNewSaveFilename, ".sav", sizeof(pszNewSaveFilename));

		// If the save associated with the next filename exists, rotate the next filename first
		if (filesystem->FileExists(pszNewSaveFilename, "MOD"))
		{
			RotateFile(pszNewMetadataFilename, pchOriginalSaveFilename, iSaveHistoryCount, iCounter + 1);
		}
		else if(filesystem->FileExists(pszNewMetadataFilename, "MOD"))
		{
			Msg("File %s does not exist. Removing metadata files %s and %s\n", pszNewSaveFilename, pszFilenameToRotate, pszNewMetadataFilename);
			filesystem->RemoveFile(pszFilenameToRotate, "MOD");
			filesystem->RemoveFile(pszNewMetadataFilename, "MOD");
			return;
		}
		// If we have passed the save history count, remove the files instead of rotating
		else if (iCounter > iSaveHistoryCount)
		{
			Msg("Tried to rotate to save metadata file %s but save history count is %d. Removing file %s\n", pszNewMetadataFilename, iSaveHistoryCount, pszFilenameToRotate);
			filesystem->RemoveFile(pszFilenameToRotate, "MOD");
			return;
		}

		filesystem->RenameFile(pszFilenameToRotate, pszNewMetadataFilename, "MOD");
	}
} g_CustomSaveMetadata;


//=============================================================================
// Archived ConVar to set save_history_count
//=============================================================================
void CV_SaveHistoryCountUpdate(IConVar* var, const char* pOldValue, float flOldValue);
ConVar	sv_save_history_count_archived("sv_save_history_count_archived", "10", FCVAR_ARCHIVE, "Archived variable which sets the engine var save_history_count.", CV_SaveHistoryCountUpdate);

void CV_SaveHistoryCountUpdate(IConVar* var, const char* pOldValue, float flOldValue)
{
	int iNewValue = sv_save_history_count_archived.GetInt();
	ConVarRef save_history_count("save_history_count");
	save_history_count.SetValue(iNewValue);
}