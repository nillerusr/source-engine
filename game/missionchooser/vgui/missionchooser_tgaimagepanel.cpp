#include "missionchooser_tgaimagepanel.h"
#include "vgui/ISurface.h"
#include "bitmap/TGALoader.h"
#include "TileGenDialog.h"

using namespace vgui;

#define ASW_ERROR_TGA	"tilegen/roomtemplates/unknownfloorplan.tga"

struct MissionChooserLoadedTGA_t
{
	// for in-game
	char szTGAName[MAX_PATH];
	int m_iWidth;
	int m_iHeight;
	int m_iTextureID;
};

int g_nCacheGenerationIndex = 1;
CUtlVector<MissionChooserLoadedTGA_t*> g_LoadedTGAs;

CMissionChooserTGAImagePanel::CMissionChooserTGAImagePanel( vgui::Panel *parent, const char *name ) : 
BaseClass( parent, name ),
m_nLoadedTextureIndex( -1 ),
m_nCacheGenerationIndex( -1 )
{
	m_szTGAName[0] = 0;
	
	SetPaintBackgroundEnabled( false );
}

CMissionChooserTGAImagePanel::~CMissionChooserTGAImagePanel()
{
	// release the texture memory
}

void CMissionChooserTGAImagePanel::ClearImageCache()
{
	for ( int i = 0; i < g_LoadedTGAs.Count(); ++ i )
	{
		surface()->DestroyTextureID( g_LoadedTGAs[i]->m_iTextureID );
	}
	g_LoadedTGAs.PurgeAndDeleteElements();
	// This will overflow after 4 billion reloads
	++ g_nCacheGenerationIndex;
}

void CMissionChooserTGAImagePanel::SetTGA( const char *filename, char const *pPathID /*= 0*/ )
{
	if ( !pPathID )
	{
		Q_snprintf( m_szTGAName, sizeof(m_szTGAName), "//MOD/%s", filename );
	}
	else
	{
		Q_snprintf( m_szTGAName, sizeof(m_szTGAName), "//%s/%s", pPathID, filename );
	}

	m_nLoadedTextureIndex = -1;
}

bool LoadTGAOrError( char const *szTGAName, char const *errorTGA, CUtlMemory< byte >& tga, int& w, int& h )
{
	if ( TGALoader::LoadRGBA8888( szTGAName, tga, w, h ) )
	{
		return true;
	}

	if ( TGALoader::LoadRGBA8888( errorTGA, tga, w, h ) )
	{
		return true;
	}

	return false;
}

void CMissionChooserTGAImagePanel::Paint()
{
	// Refresh textures in case the cache was cleared
	if ( m_nCacheGenerationIndex != g_nCacheGenerationIndex )
	{
		m_nLoadedTextureIndex = -1;
	}

	// See if texture is valid
	if ( m_nLoadedTextureIndex == -1 )
	{
		// see if this tga is already loaded
		int iLoadedTextures = g_LoadedTGAs.Count();
		for ( int i=0;i<iLoadedTextures;i++ )
		{
			if ( !Q_stricmp( g_LoadedTGAs[i]->szTGAName, m_szTGAName ) )
			{
				m_nLoadedTextureIndex = i;
				m_nCacheGenerationIndex = g_nCacheGenerationIndex;
				break;
			}
		}
	}

	if ( m_nLoadedTextureIndex == -1 )
	{
		// Load the file
		CUtlMemory<unsigned char> tga;
		int nWidth, nHeight;
		if ( LoadTGAOrError( m_szTGAName, ASW_ERROR_TGA, tga, nWidth, nHeight ) )
		{
			// Set the textureID
			int nTextureID = surface()->CreateNewTextureID( true );
			surface()->DrawSetTextureRGBA( nTextureID, tga.Base(), nWidth, nHeight );

			MissionChooserLoadedTGA_t *pLoadedTGA = new MissionChooserLoadedTGA_t;
			Q_snprintf( pLoadedTGA->szTGAName, sizeof( pLoadedTGA->szTGAName ), "%s", m_szTGAName );
			pLoadedTGA->m_iWidth = nWidth;
			pLoadedTGA->m_iHeight = nHeight;
			pLoadedTGA->m_iTextureID = nTextureID;
			g_LoadedTGAs.AddToTail( pLoadedTGA );
			
			// Point to the cached TGA
			m_nLoadedTextureIndex = g_LoadedTGAs.Count() - 1;
			m_nCacheGenerationIndex = g_nCacheGenerationIndex;
		}
	}

	// draw the image
	int wide, tall;
	GetSize( wide, tall );

	if ( m_nLoadedTextureIndex != -1 )
	{
		surface()->DrawSetTexture( g_LoadedTGAs[m_nLoadedTextureIndex]->m_iTextureID );
		surface()->DrawSetColor( 255, 255, 255, 255 );
		surface()->DrawTexturedSubRect( 
			0, 0, wide, tall, 
			0.0f, 0.0f, 1.0f, 1.0f );
	}
	else
	{
		// draw a purplish fill instead
		surface()->DrawSetColor( 200, 50, 150, 255 );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}
}
