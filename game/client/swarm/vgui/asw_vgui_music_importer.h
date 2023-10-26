#ifndef _DEFINED_ASW_VGUI_MUSIC_IMPORTER_H
#define _DEFINED_ASW_VGUI_MUSIC_IMPORTER_H

#include <vgui/VGUI.h>
#include "vgui_controls/FileOpenDialog.h"

class vgui::IScheme;

//--------------------------------------------------------
// Information about the mp3 file being loaded (artist, genre, etc.)
//--------------------------------------------------------
struct ID3Info_t
{
	ID3Info_t( void );

	char *szTrackName;
	char *szArtistName;
	char *szAlbumName;
	char *szGenre;

	bool Deserialize( const byte *pBuffer, unsigned int unBufferSize );
};

//--------------------------------------------------------
// VGUI control for importing mp3 files
//--------------------------------------------------------
class MusicImporterDialog : public vgui::FileOpenDialog
{
	DECLARE_CLASS_SIMPLE( MusicImporterDialog, vgui::FileOpenDialog );

public:
	// The context keyvalues are added to all messages sent by this dialog if they are specified
	MusicImporterDialog( Panel *parent, const char *title, vgui::FileOpenDialogType_t type, KeyValues *pContextKeyValues = 0 );
	~MusicImporterDialog();

	void ImportMusic( const char *szSrcFilename, const char *szDirectory );

	static void OpenImportDialog( Panel *pParent = null );

protected:
	MESSAGE_FUNC_PARAMS( OnMusicItemSelected, "MultiFilesSelected", pInfo );

	// Imports all the music in a specified directory recursively
	void ImportAllMusicInDirectory( const char *szDirectory );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	Panel *m_pCallingPanel;
};

#endif // #ifndef _DEFINED_ASW_VGUI_MUSIC_IMPORTER_H