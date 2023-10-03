#include "cbase.h"
#include "asw_vgui_music_importer.h"
#include "filesystem.h"
#include "strtools.h"
#include "fmtstr.h"
#include "c_asw_jukebox.h"
#include "gamestringpool.h"

const char *g_ID3MusicGenres[] =
{
	"Blues",
	"Classic Rock",
	"Country",
	"Dance",
	"Disco",
	"Funk",
	"Grunge",
	"Hip-Hop",
	"Jazz",
	"Metal",
	"New Age",
	"Oldies",
	"Other",
	"Pop",
	"R&B",
	"Rap",
	"Reggae",
	"Rock",
	"Techno",
	"Industrial",
	"Alternative",
	"Ska",
	"Death Metal",
	"Pranks",
	"Soundtrack",
	"Euro-Techno",
	"Ambient",
	"Trip-Hop",
	"Vocal",
	"Jazz+Funk",
	"Fusion",
	"Trance",
	"Classical",
	"Instrumental",
	"Acid",
	"House",
	"Game",
	"Sound Clip",
	"Gospel",
	"Noise",
	"AlternRock",
	"Bass",
	"Soul",
	"Punk",
	"Space",
	"Meditative",
	"Instrumental Pop",
	"Instrumental Rock",
	"Ethnic",
	"Gothic",
	"Darkwave",
	"Techno-Industrial",
	"Electronic",
	"Pop-Folk",
	"Eurodance",
	"Dream",
	"Southern Rock",
	"Comedy",
	"Cult",
	"Gangsta",
	"Top 40",
	"Christian Rap",
	"Pop/Funk",
	"Jungle",
	"Native American",
	"Cabaret",
	"New Wave",
	"Psychadelic",
	"Rave",
	"Showtunes",
	"Trailer",
	"Lo-Fi",
	"Tribal",
	"Acid Punk",
	"Acid Jazz",
	"Polka",
	"Retro",
	"Musical",
	"Rock & Roll",
	"Hard Rock",
	"Folk",
	"Folk-Rock",
	"National Folk",
	"Swing",
	"Fast Fusion",
	"Bebob",
	"Latin",
	"Revival",
	"Celtic",
	"Bluegrass",
	"Avantgarde",
	"Gothic Rock",
	"Progressive Rock",
	"Psychedelic Rock",
	"Symphonic Rock",
	"Slow Rock",
	"Big Band",
	"Chorus",
	"Easy Listening",
	"Acoustic",
	"Humour",
	"Speech",
	"Chanson",
	"Opera",
	"Chamber Music",
	"Sonata",
	"Symphony",
	"Booty Bass",
	"Primus",
	"Porn Groove",
	"Satire",
	"Slow Jam",
	"Club",
	"Tango",
	"Samba",
	"Folklore",
	"Ballad",
	"Power Ballad",
	"Rhythmic Soul",
	"Freestyle",
	"Duet",
	"Punk Rock",
	"Drum Solo",
	"Acapella",
	"Euro-House",
	"Dance Hall"
};

unsigned int g_ID3SupportedVersions[] = 
{
	2,
	3,
	4,
};

int ReadID3FrameSize( int nVersion, byte **ppBuffer, unsigned int unBufferSize )
{
	int nBytesToRead = nVersion < 3 ? 3 : 4;
	int nBitsToShift = nVersion < 4 ? 8 : 7;

	// Read the ridiculous tag size format
	int nTagSize = 0;
	for( int i=0; i<nBytesToRead; ++i )
	{
		nTagSize |= *( *ppBuffer + nBytesToRead - i - 1 ) << nBitsToShift*i;
	}

	// Advance the buffer pointer
	*ppBuffer += nBytesToRead;

	return nTagSize;
}

bool ID3Info_t::Deserialize( const byte *pBuffer, unsigned int unBufferSize )
{
	if( !pBuffer )
		return false;

	// Check for supported ID3v2 format
	char *pHeader = (char*)pBuffer;
	if( Q_strncmp( "ID3", pHeader, 3 ) )
		return false;

	unsigned int unMajorVersion = *( pHeader + 3 );
	bool bSupportedVersion = false;
	for( int i=0; i < ARRAYSIZE( g_ID3SupportedVersions ); ++i )
	{
		bSupportedVersion |= unMajorVersion == g_ID3SupportedVersions[i];
	}
	if( !bSupportedVersion )
		return false;

	// Read the ridiculous tag size format
	int nTagSize = 0;
	for( int i=0; i<3; ++i )
	{
		nTagSize |= *( pHeader + 9 - i ) << 7*i;
	}
	
	if( nTagSize <= 0 )
		return false;

	// Read in each frame
	byte *pCurr = (byte*)pHeader + 10;
	while( pCurr - pBuffer < nTagSize )
	{
		const int nLabelSize = unMajorVersion < 3 ? 3 : 4;

		// If we're into padding, break
		if( *pCurr == 0 )
			break;

		// Read the frame header
		char *szLabel = (char*)pCurr;
		pCurr += nLabelSize;
		int nFrameSize = ReadID3FrameSize( unMajorVersion, (byte**)&pCurr, unBufferSize );
		
		// Skip the flags bytes (1 for V3 or less, 2 otherwise)
		if( unMajorVersion >= 3 )
			pCurr += 2;

		// Check for a text frame
		if( *szLabel == 'T' )
		{
			const int nMaxStringLen = 64;
			// Only care about unicode (the first byte in a text frame, all versions)
			if( *pCurr == 0 )
			{
				if( !Q_strncmp( szLabel, "TP1", 3 ) || !Q_strncmp( szLabel, "TOLY", 4 ) || !Q_strncmp( szLabel, "TOPE", 4 ) || !Q_strncmp( szLabel, "TPE1", 4 ) || !Q_strncmp( szLabel, "TPE2", 4 ) )
				{
					// Read the artist
					char szTemp[ nMaxStringLen ];
					Q_strncpy( szTemp, (char*)( pCurr + 1 ), ( nMaxStringLen > nFrameSize ) ? nFrameSize : nMaxStringLen );
					szArtistName = (char*)AllocPooledString( szTemp );
				}
				else if( !Q_strncmp( szLabel, "TT2", 3 ) || !Q_strncmp( szLabel, "TIT2", 4 ) )
				{
					// Read the title
					char szTemp[ nMaxStringLen ];
					Q_strncpy( szTemp, (char*)( pCurr + 1 ), ( nMaxStringLen > nFrameSize ) ? nFrameSize : nMaxStringLen );
					szTrackName = (char*)AllocPooledString( szTemp );
				}
				else if( !Q_strncmp( szLabel, "TAL", 3 ) )
				{
					// Read the album
					char szTemp[ nMaxStringLen ];
					Q_strncpy( szTemp, (char*)( pCurr + 1 ), ( nMaxStringLen > nFrameSize ) ? nFrameSize : nMaxStringLen );
					szAlbumName = (char*)AllocPooledString( szTemp );
				}
				else if( !Q_strncmp( szLabel, "TCO", 3 ) )
				{
					// In newer versions, make sure it's a TCON frame
					if( unMajorVersion > 2 && !Q_strncmp( szLabel, "TCON", 4 ) || unMajorVersion <= 2 )
					{
						// Get the genre
						if( *( pCurr + 1) == '(' )
						{
							const char *szGenreIndex = (char*)( pCurr + 2 );
							int nGenre = atoi( szGenreIndex );
							if( nGenre < ARRAYSIZE(g_ID3MusicGenres) )
								szGenre = (char*)g_ID3MusicGenres[ nGenre ];
							else
								szGenre = "UNKNOWN";
						}
					}
				}
			}

		}

		// Advance to the next frame
		pCurr += nFrameSize;
		assert( pCurr < pBuffer + unBufferSize );
	}
	
	return true;
}

ID3Info_t::ID3Info_t( void )
: szAlbumName( null )
, szArtistName( null )
, szGenre( null )
, szTrackName( null )
{}

MusicImporterDialog::MusicImporterDialog( Panel *parent, const char *title, vgui::FileOpenDialogType_t type, KeyValues *pContextKeyValues /*= 0 */ )
: FileOpenDialog(parent, title, type, pContextKeyValues )
{
	AddActionSignalTarget( this );
}

MusicImporterDialog::~MusicImporterDialog()
{

}

void MusicImporterDialog::OnMusicItemSelected( KeyValues *pInfo )
{
	if( !pInfo )
		return;

	// Parse the keyvalues
	const char* szDirectory = pInfo->GetString("activedirectory");
	KeyValues *pSelections = pInfo->FindKey("selectedfiles");
	if( !pSelections || !szDirectory )
		return;

	for( KeyValues *pSub = pSelections->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey() )
	{
		const char* szAttributes = pSub->GetString("attributes");
		if( !szAttributes )
			continue;

		const char* szFilename = CFmtStr( "%s%s", szDirectory, pSub->GetString("text"));
		//char szOutFilename[MAX_PATH];

		// If the item selected was a directory, search it recursively for music
		if( strstr( szAttributes, "D" ) )
		{
			ImportAllMusicInDirectory( szFilename );
			continue;
		}

		ImportMusic( pSub->GetString("text"), szDirectory );
	}
	PostActionSignal( new KeyValues( "MusicImportComplete" ) );
}

void MusicImporterDialog::ImportMusic( const char* szSrcFilename, const char *szDirectory )
{
	char fn[ 512 ];
	Q_snprintf( fn, sizeof( fn ), "%s%s", szDirectory, szSrcFilename );

	// Get temp filename from crc
	CRC32_t crc;
	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, fn, Q_strlen( fn ) );
	CRC32_Final( &crc );

	char hexname[ 16 ];
	Q_binarytohex( (const byte *)&crc, sizeof( crc ), hexname, sizeof( hexname ) );

	char hexfilename[ 512 ];
	Q_snprintf( hexfilename, sizeof( hexfilename ), "sound/music/_mp3/%s.mp3", hexname );

	Q_FixSlashes( hexfilename );

	if ( g_pFullFileSystem->FileExists( fn ) )
	{
		// Make a local copy
		char mp3_temp_path[ 512 ];
		Q_snprintf( mp3_temp_path, sizeof( mp3_temp_path ), "sound/music/_mp3" );
		g_pFullFileSystem->CreateDirHierarchy( mp3_temp_path, "MOD" );

		char destpath[ 512 ];
		Q_snprintf( destpath, sizeof( destpath ), "%s/%s", engine->GetGameDirectory(), hexfilename );
		Q_FixSlashes( destpath );

		FileHandle_t hFile = g_pFullFileSystem->Open( fn, "rb", "MOD" );
		FileHandle_t hFileOut = g_pFullFileSystem->Open( destpath, "wb", "MOD" );
		if ( FILESYSTEM_INVALID_HANDLE != hFile && FILESYSTEM_INVALID_HANDLE != hFileOut )
		{
			unsigned int unSize = g_pFullFileSystem->Size( hFile );

			byte *pBuffer = (byte*) malloc( unSize );
			if ( g_pFullFileSystem->Read( pBuffer, unSize, hFile ) == (int) unSize )
			{
				g_pFullFileSystem->Write( pBuffer, unSize, hFileOut );
			}

			// Read ID3 header info
			ID3Info_t id3Header;
			id3Header.Deserialize( pBuffer, unSize );
			g_ASWJukebox.AddMusicToPlaylist( szSrcFilename, hexname, id3Header.szAlbumName, id3Header.szArtistName, id3Header.szGenre );

			free( pBuffer );
			g_pFullFileSystem->Close( hFile );
			g_pFullFileSystem->Close( hFileOut );
		}
	}
}

void MusicImporterDialog::ImportAllMusicInDirectory( const char *szDirectory )
{
	// Check to make sure the specified directory exists
	if( !g_pFullFileSystem->IsDirectory( szDirectory ) )
		return;

	// Search the directory structure.
	const char *musicwildcard = CFmtStr( "%s%s", szDirectory, "/*.mp3");

	FileFindHandle_t findHandle;
	char const *findfn = filesystem->FindFirst(musicwildcard, &findHandle);	
	while ( findfn )
	{
		//const char*szFilename = CFmtStr( "%s/%s", szDirectory, findfn );
		ImportMusic( szDirectory, findfn );

		findfn = filesystem->FindNext( findHandle );
	}

	filesystem->FindClose(findHandle);	
}

vgui::DHANDLE<MusicImporterDialog> g_hMusicImportDialog;

void MusicImporterDialog::OpenImportDialog( Panel *pParent )
{
	if (g_hMusicImportDialog.Get() == NULL)
	{
		g_hMusicImportDialog = new MusicImporterDialog( NULL, "#asw_music_import_dialog", vgui::FOD_OPEN_MULTIPLE, NULL);
		g_hMusicImportDialog->AddFilter("*.mp3", "#asw_music_types", true);
	}
	if( pParent )
		g_hMusicImportDialog->AddActionSignalTarget( pParent );
	g_hMusicImportDialog->DoModal(false);
	g_hMusicImportDialog->Activate();
}

void MusicImporterDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetAlpha( 255 );
}

void asw_open_music_dialog_f( void )
{
	MusicImporterDialog::OpenImportDialog( null );
}

ConCommand asw_open_music_dialog( "asw_open_music_dialog", asw_open_music_dialog_f, "Shows a dialog for picking custom combat music", FCVAR_DEVELOPMENTONLY );
