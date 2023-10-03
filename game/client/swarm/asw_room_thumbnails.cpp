#include "cbase.h"
#include "view.h"
#include "view_shared.h"
#include "KeyValues.h"
#include "bitmap/tgawriter.h"
#include "iviewrender.h"
#include "filesystem.h"
#include "p4lib/ip4.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// This takes a top down orthographic thumbnail screenshot of all rooms specified in roomthumbnails.txt

ConVar asw_building_room_thumbnails( "asw_building_room_thumbnails", "0", FCVAR_CHEAT, "Set to 1 to cause room thumbnails to be saved on next map load" );
ConVar asw_add_room_thumbnails_to_perforce( "asw_add_room_thumbnails_to_perforce", "0", FCVAR_CHEAT, "Set to 1 to cause room thumbnails to be added to Perforce on creation" );

struct CRoomThumbnail
{
	char m_szThumbnailName[MAX_PATH];
	float m_fRoomX;
	float m_fRoomY;
	float m_fRoomWide;
	float m_fRoomTall;
	int m_iOutputWide;
	int m_iOutputTall;
};

static void SetupThumbnailView( CViewSetup &setup, CRoomThumbnail *pRoom )
{
	memset( &setup, 0, sizeof(setup) );
	static int oldCRC = 0;

	setup.m_bOrtho = true;
	setup.m_flAspectRatio = 1.0f;
	setup.m_bRenderToSubrectOfLargerScreen = true;
	setup.zNear = 7.0f;
	setup.zFar = 28400.0f;
	setup.fov = 90.0f;

	float size_y = pRoom->m_fRoomTall;
	float size_x = pRoom->m_fRoomWide;

	setup.origin.x = pRoom->m_fRoomX;
	setup.origin.y = pRoom->m_fRoomY;
	setup.origin.z = 400.0f;

	setup.x = 0;
	setup.y = 0;
	setup.width = pRoom->m_iOutputWide;
	setup.height = pRoom->m_iOutputTall;

	setup.m_OrthoLeft   = 0;
	setup.m_OrthoTop    = -size_y;
	setup.m_OrthoRight  = size_x;
	setup.m_OrthoBottom = 0;

	setup.angles = QAngle( 90, 90, 0 );
}

static void TakeRoomThumbnailSnapshot( CRoomThumbnail *pRoom )
{
	if ( IsX360() )
		return;

	CViewSetup	setup;
	SetupThumbnailView( setup, pRoom );

	view->RenderView( setup, setup, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_FULL_TARGET, 0 );

	unsigned char *pImage = ( unsigned char * )malloc( setup.width * 3 * setup.height );

	// Get Bits from the material system
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ReadPixels( 0, 0, setup.width, setup.height, pImage, IMAGE_FORMAT_RGB888 );

	// allocate a buffer to write the tga into
	int iMaxTGASize = 1024 + (setup.width * setup.height * 4);
	void *pTGA = malloc( iMaxTGASize );
	CUtlBuffer buffer( pTGA, iMaxTGASize );

	if( !TGAWriter::WriteToBuffer( pImage, buffer, setup.width, setup.height, IMAGE_FORMAT_RGB888, IMAGE_FORMAT_RGB888 ) )
	{
		Error( "Couldn't write bitmap data snapshot.\n" );
	}

	free( pImage );

	// async write to disk (this will take ownership of the memory)
	char szPathedFileName[_MAX_PATH];
	Q_snprintf( szPathedFileName, sizeof(szPathedFileName), "//MOD/%s", pRoom->m_szThumbnailName );

	filesystem->AsyncWrite( szPathedFileName, buffer.Base(), buffer.TellPut(), true );

	//videomode->TakeSnapshotTGARect( pRoom->m_szThumbnailName, 0, 0, setup.width, setup.height, setup.width, setup.height );
	Msg( "Took snapshot %s x=%f y=%f w=%f h=%f ow=%d oh=%d\n", pRoom->m_szThumbnailName,
		pRoom->m_fRoomX, pRoom->m_fRoomY, pRoom->m_fRoomWide, pRoom->m_fRoomTall,
		pRoom->m_iOutputWide, pRoom->m_iOutputTall );
	if ( p4 && asw_add_room_thumbnails_to_perforce.GetBool() )
	{
		char fullPath[MAX_PATH];
		g_pFullFileSystem->RelativePathToFullPath( pRoom->m_szThumbnailName, "GAME", fullPath, MAX_PATH );
		p4->OpenFileForAdd( fullPath );
	}
}


#if !defined( DEDICATED ) && !defined( _X360 )

#define THUMBNAILS_FILE "resource/roomthumbnails.txt"

CON_COMMAND( asw_buildroomthumbnails, "Outputs room thumbnail TGAs for all rooms specific in roomthumbnails.txt" )
{
	// load roomthumbnails file
	KeyValues *pKV = new KeyValues( "RoomThumbnails" );
	if ( !pKV->LoadFromFile( filesystem, THUMBNAILS_FILE, "GAME" ) )
	{
		Msg( "Error: Couldn't open %s\n", THUMBNAILS_FILE );
		pKV->deleteThis();
		return;
	}

	// build a list of room thumbnails
	CUtlVector<CRoomThumbnail*> thumbnails;
	KeyValues *pkvEntry = pKV->GetFirstSubKey();
	while ( pkvEntry )
	{		
		if ( !Q_stricmp( pkvEntry->GetName(), "Thumbnail" ) )
		{
			CRoomThumbnail *pThumbnail = new CRoomThumbnail;
			Q_strcpy( pThumbnail->m_szThumbnailName, pkvEntry->GetString( "Filename", "screenshots/thumbnail.tga" ) );
			pThumbnail->m_fRoomX = pkvEntry->GetFloat( "RoomX", 0.0f );
			pThumbnail->m_fRoomY = pkvEntry->GetFloat( "RoomY", 0.0f );
			pThumbnail->m_fRoomWide = pkvEntry->GetFloat( "RoomWide", 256.0f );
			pThumbnail->m_fRoomTall = pkvEntry->GetFloat( "RoomTall", 256.0f );
			pThumbnail->m_iOutputWide = pkvEntry->GetInt( "OutputWide", 20 );
			pThumbnail->m_iOutputTall = pkvEntry->GetInt( "OutputTall", 20 );
			thumbnails.AddToTail( pThumbnail );
		}
		pkvEntry = pkvEntry->GetNextKey();
	}

	// go through the list and take a screenshot of each area
	for ( int i=0; i<thumbnails.Count(); i++ )
	{
		TakeRoomThumbnailSnapshot( thumbnails[i] );
	}

	thumbnails.PurgeAndDeleteElements();
	pKV->deleteThis();
}
#endif // DEDICATED