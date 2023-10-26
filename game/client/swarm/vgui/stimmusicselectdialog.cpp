#include "cbase.h"
#include "StimMusicSelectDialog.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_stim_music;

StimMusicSelectDialog::StimMusicSelectDialog( Panel *parent, const char *title, vgui::FileOpenDialogType_t type, KeyValues *pContextKeyValues ) :
	FileOpenDialog( parent, title, type, pContextKeyValues )
{
	// get notified when the file has been picked
	AddActionSignalTarget( this );
}

void StimMusicSelectDialog::OnFileSelected(const char *fullpath)
{
	if ( fullpath )
	{
		asw_stim_music.SetValue( fullpath );
	}
}


vgui::DHANDLE<StimMusicSelectDialog> g_hSelectStimMusicDialog;

void asw_pick_stim_music_f()
{
	if (g_hSelectStimMusicDialog.Get() == NULL)
	{
		g_hSelectStimMusicDialog = new StimMusicSelectDialog(NULL, "#asw_pick_stim_music", vgui::FOD_OPEN, NULL);
		g_hSelectStimMusicDialog->AddFilter("*.mp3,*.wav", "#asw_stim_music_types", true);
	}
	g_hSelectStimMusicDialog->DoModal(false);
	g_hSelectStimMusicDialog->Activate();
}
ConCommand asw_pick_stim_music( "asw_pick_stim_music", asw_pick_stim_music_f, "Shows a dialog for picking custom stim music", FCVAR_NONE );