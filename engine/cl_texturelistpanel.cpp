//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "client_pch.h"
#include "ivideomode.h"
#include "client_class.h"
#include "icliententitylist.h"
#include "vgui_basepanel.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/TreeView.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ListViewPanel.h>
#include <vgui_controls/TreeViewListControl.h>
#include <vgui/ISystem.h>
#include "tier0/vprof.h"
#include "KeyValues.h"
#include "vgui_helpers.h"
#include "utlsymbol.h"
#include "tier1/UtlStringMap.h"
#include "bitvec.h"
#include "utldict.h"
#include "vgui/ILocalize.h"
#include "con_nprint.h"
#include "gl_matsysiface.h"

#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "tier2/tier2.h"
#include "smartptr.h"

#include "igame.h"

#include "client.h"

#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"
#include "ivtex.h"
#include "tier2/fileutils.h"

// For character manipulations isupper/tolower
#include <ctype.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define KEYNAME_NAME			"Name"
#define KEYNAME_PATH			"Path"
#define KEYNAME_BINDS_MAX		"BindsMax"
#define KEYNAME_BINDS_FRAME		"BindsFrame"
#define KEYNAME_SIZE			"Size"
#define KEYNAME_FORMAT			"Format"
#define KEYNAME_WIDTH			"Width"
#define KEYNAME_HEIGHT			"Height"
#define KEYNAME_TEXTURE_GROUP	"TexGroup"

#define COPYTOCLIPBOARD_CMDNAME "CopyToClipboard"

#if defined( _X360 )
CON_COMMAND( mat_get_textures, "VXConsole command" )
{
	g_pMaterialSystemDebugTextureInfo->EnableDebugTextureList( true );
	g_pMaterialSystemDebugTextureInfo->EnableGetAllTextures( args.ArgC() >= 2 && ( Q_stricmp( args[1], "all" ) == 0 ) );
}
#endif

static ConVar mat_texture_list( "mat_texture_list", "0", FCVAR_CHEAT, "For debugging, show a list of used textures per frame" );

static enum TxListPanelRequest
{
	TXR_NONE,
	TXR_SHOW,
	TXR_RUNNING,
	TXR_HIDE
} s_eTxListPanelRequest = TXR_NONE;

IVTex* VTex_Load( CSysModule** pModule );
void VTex_Unload( CSysModule *pModule );
void* CubemapsFSFactory( const char *pName, int *pReturnCode );
bool StripDirName( char *pFilename );

// These are here so you can bind a key to +mat_texture_list and toggle it on and off.
void mat_texture_list_on_f();
void mat_texture_list_off_f();

ConCommand mat_texture_list_on( "+mat_texture_list", mat_texture_list_on_f );
ConCommand mat_texture_list_off( "-mat_texture_list", mat_texture_list_off_f );

ConVar mat_texture_list_all( "mat_texture_list_all", "0", FCVAR_NEVER_AS_STRING|FCVAR_CHEAT, "If this is nonzero, then the texture list panel will show all currently-loaded textures." );

ConVar mat_texture_list_view( "mat_texture_list_view", "1", FCVAR_NEVER_AS_STRING|FCVAR_CHEAT, "If this is nonzero, then the texture list panel will render thumbnails of currently-loaded textures." );

ConVar mat_show_texture_memory_usage( "mat_show_texture_memory_usage", "0", FCVAR_NEVER_AS_STRING|FCVAR_CHEAT, "Display the texture memory usage on the HUD." );


static int g_warn_texkbytes = 1499;
static int g_warn_texdimensions = 1024;
static bool g_warn_enable = true;
static bool g_cursorset = false;

class CTextureListPanel;
static CTextureListPanel *g_pTextureListPanel = NULL;
static bool g_bRecursiveRequestToShowTextureList = false;
static int g_nSaveQueueState = INT_MIN;


//
// A class to mimic a list view
//

namespace vgui
{


class TileViewPanelEx : public Panel
{
	DECLARE_CLASS_SIMPLE( TileViewPanelEx, Panel );

public:
	TileViewPanelEx( Panel *parent, const char *panelName );
	~TileViewPanelEx();

public:
	virtual void SetFont( HFont hFont );
	virtual HFont GetFont();

public:
	enum HitTest_t
	{
		HT_NOTHING = 0,
		HT_TILE
	};
	virtual int HitTest( int x, int y, int &iTile );
	virtual bool GetTileOrg( int iTile, int &x, int &y );

protected: // Overrides for contents
	virtual int GetNumTiles() = 0;
	virtual void GetTileSize( int &wide, int &tall ) = 0;
	virtual void RenderTile( int iTile, int x, int y ) = 0;

protected:
	// Handlers
	virtual void OnMouseWheeled( int delta );
	virtual void OnSizeChanged( int wide, int tall ); 
	virtual void PerformLayout();
	virtual void Paint();
	virtual void ApplySchemeSettings( IScheme *pScheme );
	MESSAGE_FUNC( OnSliderMoved, "ScrollBarSliderMoved" );

protected:
	virtual bool ComputeLayoutInfo();
	int m_li_numTiles;							//!< Total number of tiles
	int m_li_numVisibleTiles;					//!< Number of tiles fitting in the client area
	int m_li_wide, m_li_tall;					//!< Tile area width/height
	int m_li_wideItem, m_li_tallItem;			//!< Every item width/height
	int m_li_colVisible, m_li_rowVisible;		//!< Num columns/rows fitting in the client area
	int m_li_rowNeeded;							//!< Num rows required to fit all numTiles
	int m_li_startTile, m_li_endTile;			//!< Start/end tile that are rendered in the client area

private:
	ScrollBar	*m_hbar;
	HFont		m_hFont;
};



//
//  Implementation details
//

TileViewPanelEx::TileViewPanelEx( Panel *parent, const char *panelName ) :
	Panel( parent, panelName ),
	m_hbar( NULL ),
	m_hFont( INVALID_FONT )
{
	m_hbar = new ScrollBar( this, "VerticalScrollBar", true );
	m_hbar->AddActionSignalTarget( this );
	m_hbar->SetVisible( true );
}

TileViewPanelEx::~TileViewPanelEx()
{
	delete m_hbar;
}

void TileViewPanelEx::SetFont( HFont hFont )
{
	m_hFont = hFont;
	Repaint();
}

HFont TileViewPanelEx::GetFont()
{
	return m_hFont;
}

int TileViewPanelEx::HitTest( int x, int y, int &iTile )
{
	iTile = -1;

	if ( !ComputeLayoutInfo() )
		return HT_NOTHING;

	int hitCol = ( x / m_li_wideItem );
	int hitRow = ( y / m_li_tallItem );

	if ( hitCol >= m_li_colVisible )
		return HT_NOTHING;
	if ( hitRow > m_li_rowVisible )
		return HT_NOTHING;

	int hitTile = m_li_startTile + hitCol + hitRow * m_li_colVisible;
	if ( hitTile >= m_li_endTile )
		return HT_NOTHING;

	// Hit tile
	iTile = hitTile;
	return HT_TILE;
}

bool TileViewPanelEx::GetTileOrg( int iTile, int &x, int &y )
{
	if ( m_li_colVisible <= 0 )
		return false;
	if ( iTile < m_li_startTile || iTile >= m_li_endTile )
		return false;

	x = 0 + ( ( iTile - m_li_startTile ) % m_li_colVisible ) * m_li_wideItem;
	y = 0 + ( ( iTile - m_li_startTile ) / m_li_colVisible ) * m_li_tallItem;

	return true;
}

void TileViewPanelEx::OnMouseWheeled( int delta )
{
	if ( m_hbar->IsVisible() )
	{
		int val = m_hbar->GetValue();
		val -= delta;
		m_hbar->SetValue( val );
	}
}

void TileViewPanelEx::OnSizeChanged( int wide, int tall )
{
	BaseClass::OnSizeChanged( wide, tall );
	InvalidateLayout();
	Repaint();
}

void TileViewPanelEx::PerformLayout()
{
	int numTiles = GetNumTiles();
	m_hbar->SetVisible( false );

	int wide, tall;
	GetSize( wide, tall );
	wide -= m_hbar->GetWide();

	m_hbar->SetPos( wide - 2, 0 );
	m_hbar->SetTall( tall );
	
	if ( !numTiles )
		return;

	int wideItem = 1, tallItem = 1;
	GetTileSize( wideItem, tallItem );
	if ( !wideItem || !tallItem )
		return;

	int colVisible = wide / wideItem;
	int rowVisible = tall / tallItem;
	if ( rowVisible <= 0 || colVisible <= 0 )
		return;

	int rowNeeded = ( numTiles + colVisible - 1 ) / colVisible;

	int startTile = 0;
	if ( rowNeeded > rowVisible )
	{
		m_hbar->SetRange( 0, rowNeeded );
		m_hbar->SetRangeWindow( rowVisible );
		m_hbar->SetButtonPressedScrollValue( 1 );

		m_hbar->SetVisible( true );
		m_hbar->InvalidateLayout();

		int val = m_hbar->GetValue();
		startTile = val * colVisible;
	}
}

bool TileViewPanelEx::ComputeLayoutInfo()
{
	m_li_numTiles = GetNumTiles();
	if ( !m_li_numTiles )
		return false;

	GetSize( m_li_wide, m_li_tall );
	m_li_wide -= m_hbar->GetWide();

	m_li_wideItem = 1, m_li_tallItem = 1;
	GetTileSize( m_li_wideItem, m_li_tallItem );
	if ( !m_li_wideItem || !m_li_tallItem )
		return false;

	m_li_colVisible = m_li_wide / m_li_wideItem;
	m_li_rowVisible = m_li_tall / m_li_tallItem;
	if ( m_li_rowVisible <= 0 || m_li_colVisible <= 0 )
		return false;

	m_li_rowNeeded = ( m_li_numTiles + m_li_colVisible - 1 ) / m_li_colVisible;
	m_li_numVisibleTiles = m_li_colVisible * m_li_rowVisible;

	m_li_startTile = 0;
	if ( m_li_rowNeeded > m_li_rowVisible )
	{
		int val = m_hbar->GetValue();
		m_li_startTile = val * m_li_colVisible;
	}

	if ( m_li_startTile >= m_li_numTiles )
		m_li_startTile = m_li_numTiles - m_li_numVisibleTiles;
	if ( m_li_startTile < 0 )
		m_li_startTile = 0;

	m_li_endTile = m_li_startTile + m_li_numVisibleTiles + m_li_colVisible; // draw the partly visible items
	if ( m_li_endTile > m_li_numTiles )
		m_li_endTile = m_li_numTiles;

	return true;
}

void TileViewPanelEx::Paint()
{
	BaseClass::Paint();

	// Now render all our tiles
	if ( !ComputeLayoutInfo() )
		return;

	for ( int iRenderTile = m_li_startTile; iRenderTile < m_li_endTile; ++ iRenderTile )
	{
		int x = 0 + ( ( iRenderTile - m_li_startTile ) % m_li_colVisible ) * m_li_wideItem;
		int y = 0 + ( ( iRenderTile - m_li_startTile ) / m_li_colVisible ) * m_li_tallItem;
		RenderTile( iRenderTile, x, y );
	}
}

void TileViewPanelEx::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBgColor( GetSchemeColor( "ListPanel.BgColor", pScheme ) );
	SetBorder( pScheme->GetBorder("ButtonDepressedBorder" ) );

	SetFont( pScheme->GetFont( "Default", IsProportional() ) );
}

void TileViewPanelEx::OnSliderMoved()
{
	InvalidateLayout();
	Repaint();
}



}; // namespace vgui


//////////////////////////////////////////////////////////////////////////
//
// Material access
//
//////////////////////////////////////////////////////////////////////////

class CAutoMatSysDebugMode
{
public:
	CAutoMatSysDebugMode();
	~CAutoMatSysDebugMode();

	void ScheduleCleanupTextureVar( IMaterialVar *pVar );

protected:
	bool bOldDebugMode;
	CUtlRBTree< IMaterialVar * > arrCleanupVars;
};

CAutoMatSysDebugMode::CAutoMatSysDebugMode() :
	arrCleanupVars( DefLessFunc(IMaterialVar *) )
{
	g_pMaterialSystem->Flush();
	bOldDebugMode = g_pMaterialSystemDebugTextureInfo->SetDebugTextureRendering( true );
}

CAutoMatSysDebugMode::~CAutoMatSysDebugMode()
{
	g_pMaterialSystem->Flush();
	g_pMaterialSystemDebugTextureInfo->SetDebugTextureRendering( bOldDebugMode );

	for ( uint32 k = 0; k < arrCleanupVars.Count(); ++ k )
	{
		IMaterialVar *pVar = arrCleanupVars.Element( k );
		pVar->SetUndefined();
	}
}

void CAutoMatSysDebugMode::ScheduleCleanupTextureVar( IMaterialVar *pVar )
{
	if ( !pVar )
		return;

	arrCleanupVars.InsertIfNotFound( pVar );
}

static IMaterial * UseDebugMaterial( char const *szMaterial, ITexture *pMatTexture, CAutoMatSysDebugMode *pRestoreVars )
{
	if ( !szMaterial || !pMatTexture )
		return NULL;

	bool foundVar;

	IMaterial *pMaterial = materials->FindMaterial( szMaterial, TEXTURE_GROUP_OTHER, false );
	// IMaterial *pMaterial = materials->FindMaterial( "debug/debugempty", TEXTURE_GROUP_OTHER, false );
	if ( !pMaterial )
		return NULL;

	IMaterialVar *BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	// IMaterialVar *BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	if ( !foundVar || !BaseTextureVar )
		return NULL;

	IMaterialVar *FrameVar = pMaterial->FindVar( "$frame", &foundVar, false );
	if ( foundVar && FrameVar )
	{
		int numAnimFrames = pMatTexture->GetNumAnimationFrames();

		if ( pMatTexture->IsRenderTarget() || pMatTexture->IsProcedural() )
			numAnimFrames = 0; // Cannot use the stencil parts of the render targets and shouldn't use the other frames of procedural textures

		FrameVar->SetIntValue( ( numAnimFrames > 0 ) ? ( g_ClientGlobalVariables.tickcount % numAnimFrames ) : 0 );
	}

	BaseTextureVar->SetTextureValue( pMatTexture );

	if ( pRestoreVars )
	{
		pRestoreVars->ScheduleCleanupTextureVar( BaseTextureVar );
	}

	return pMaterial;
}

static void RenderTexturedRect( vgui::Panel *pPanel, IMaterial *pMaterial, int x, int y, int x1, int y1, int xoff = 0, int yoff = 0 )
{
	CAutoMatSysDebugMode auto_matsysdebugmode;

	int tall = pPanel->GetTall();
	float fHeightUV = 1.0f;
	if ( y1 > tall )
	{
		fHeightUV = float( tall - y ) / float( y1 - y );
		y1 = tall;
	}
	if ( y1 <= y )
		return;

	pPanel->LocalToScreen( x, y );
	pPanel->LocalToScreen( x1, y1 );

	x += xoff; x1 += xoff; y += yoff; y1 += yoff;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( x, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x1, y, 0.0f );
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x1, y1, 0.0f );
	meshBuilder.TexCoord2f( 0, 1.0f, fHeightUV );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x, y1, 0.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, fHeightUV );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

static bool ShallWarnTx( KeyValues *kv, ITexture *tx )
{
	if ( !tx )
	{
		return false;
	}

 	if ( tx->GetFlags() & ( TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_ONEBITALPHA	) )
 		return true;

	if ( stricmp( "DXT1", kv->GetString( KEYNAME_FORMAT ) )  && stricmp( "DXT5", kv->GetString( KEYNAME_FORMAT ) ) &&
		 stricmp( "ATI1N", kv->GetString( KEYNAME_FORMAT ) ) && stricmp( "ATI2N", kv->GetString( KEYNAME_FORMAT ) ) &&
		 stricmp( "DXT5_RUNTIME", kv->GetString( KEYNAME_FORMAT ) ) )
		return true;

	if ( kv->GetInt( KEYNAME_SIZE ) > g_warn_texkbytes )
		return true;

	if ( kv->GetInt( KEYNAME_WIDTH ) > g_warn_texdimensions )
		return true;

	if ( kv->GetInt( KEYNAME_HEIGHT ) > g_warn_texdimensions )
		return true;

	return false;
}

static void FmtCommaNumber( char *pchBuffer, unsigned int uiNumber )
{
	pchBuffer[0] = 0;
	for ( unsigned int uiDivisor = 1000 * 1000 * 1000; uiDivisor > 0; uiDivisor /= 1000 )
	{
		if ( uiNumber > uiDivisor )
		{
			unsigned int uiPrint = ( uiNumber / uiDivisor ) % 1000;
			sprintf( pchBuffer + strlen( pchBuffer ), ( uiNumber / uiDivisor < 1000 ) ? "%d," : "%03d,", uiPrint );
		}
	}

	int len = strlen( pchBuffer );
	if ( !len )
		sprintf( pchBuffer, "0" );
	else if ( pchBuffer[ len - 1 ] == ',' )
		pchBuffer[ len - 1 ] = 0;
}

namespace
{

	char s_chLastViewedTextureBuffer[ 0x200 ] = {0};

}; // end `anonymous` namespace

bool CanAdjustTextureSize( char const *szTextureName, bool bMoveSizeUp )
{
	ITexture *pMatTexture = materials->FindTexture( szTextureName, "", false );
	if ( !pMatTexture )
		return false;

	// Determine the texture current size
	if ( !bMoveSizeUp )
	{
		return ( pMatTexture->GetActualWidth() > 4 || pMatTexture->GetActualHeight() > 4 );
	}
	else
	{
		return ( pMatTexture->GetActualWidth() < pMatTexture->GetMappingWidth() ||
			pMatTexture->GetActualHeight() < pMatTexture->GetMappingHeight() );
	}
}

bool AdjustTextureSize( char const *szTextureName, bool bMoveSizeUp )
{
	ITexture *pMatTexture = materials->FindTexture( szTextureName, "", false );

	if ( !pMatTexture )
		return false;

	pMatTexture->ForceLODOverride( bMoveSizeUp ? +1 : -1 );
	return true;
}

CON_COMMAND_F( mat_texture_list_txlod, "Adjust LOD of the last viewed texture +1 to inc resolution, -1 to dec resolution", FCVAR_DONTRECORD )
{
	if ( args.ArgC() == 2 )
	{
		int iAdjustment = atoi( args.Arg( 1 ) );
		switch ( iAdjustment )
		{
			case 1:
			case -1:
				if ( !CanAdjustTextureSize( s_chLastViewedTextureBuffer, iAdjustment > 0 ) )
					Warning( "mat_texture_list_txlod cannot adjust lod for '%s'\n", s_chLastViewedTextureBuffer );
				else if ( !AdjustTextureSize( s_chLastViewedTextureBuffer, iAdjustment > 0 ) )
					Warning( "mat_texture_list_txlod failed adjusting lod for '%s'\n", s_chLastViewedTextureBuffer );
				else
					Msg( "mat_texture_list_txlod adjusted lod %c1 for '%s'\n", ( iAdjustment > 0 ) ? '+' : '-' , s_chLastViewedTextureBuffer );
				return;
			default:
				break;
				
		}
	}
	Warning( "Usage: 'mat_texture_list_txlod +1' to inc lod | 'mat_texture_list_txlod -1' to dec lod\n" );
}

static bool SaveTextureImage( char const *szTextureName )
{
	bool bRet;
	char fileName[ MAX_PATH ];
	char baseName[ MAX_PATH ];
	ITexture *pMatTexture = materials->FindTexture( szTextureName, "", false );

	if ( !pMatTexture )
	{
		Msg( "materials->FindTexture( '%s' ) failed.\n", szTextureName );
		return false;
	}

	V_FileBase( szTextureName, baseName, ARRAYSIZE( baseName ) );
	Q_snprintf( fileName, ARRAYSIZE( fileName ), "//MOD/tex_%s.tga", baseName );

	bRet = pMatTexture->SaveToFile( fileName );

	Msg( "SaveTextureImage( '%s' ): %s\n", fileName, bRet ? "succeeded" : "failed" );
	return bRet;
}

namespace MatViewOverride
{


//
// View override parameters request
//
struct ViewParamsReq
{
	CUtlVector< UtlSymId_t > lstMaterials;
}
s_viewParamsReq;

void RequestSelectNone( void )
{
	s_viewParamsReq.lstMaterials.RemoveAll();
}
void RequestSelected( int nCount, UtlSymId_t const *pNameIds )
{
	s_viewParamsReq.lstMaterials.AddMultipleToTail( nCount, pNameIds );
}

//
// View override last parameters
//
struct ViewParamsLast
{
	ViewParamsLast() : lstMaterials( DefLessFunc( UtlSymId_t ) ), flTime( 0.f ), bHighlighted( false ) {}

	float flTime;
	bool bHighlighted;

	struct TxInfo
	{
		ITexture *pTx;
		CUtlSymbol name;
	};
	
	struct VarMap : public CUtlMap< UtlSymId_t, TxInfo >
	{
		VarMap() : CUtlMap< UtlSymId_t, TxInfo >( DefLessFunc( UtlSymId_t ) ) {}
		VarMap( VarMap const &x ) { const_cast< VarMap & >( x ).Swap( *this ); m_matInfo = x.m_matInfo; } // Fast-swap data

		struct MatInfo
		{
// 			MatInfo() : ignorez( false ) {}
// 			bool ignorez;
			MatInfo() {}
		} m_matInfo;
	};

	CUtlMap< UtlSymId_t, VarMap > lstMaterials;
}
s_viewParamsLast;

//
// DisplaySelectedTextures
//	Executed every frame to toggle the selected/unselected
//	textures for a list of materials.
//
void DisplaySelectedTextures()
{
	// Nothing selected
	if ( !s_viewParamsLast.lstMaterials.Count() &&
		 !s_viewParamsReq.lstMaterials.Count() )
		 return;

	if ( !s_viewParamsLast.lstMaterials.Count() &&
		 s_viewParamsReq.lstMaterials.Count() )
	{
		// First time selection
		s_viewParamsLast.flTime = Plat_FloatTime();
		s_viewParamsLast.bHighlighted = false;
	}
	else
	{
		// Wait for the flash-time
		float fCurTime = Plat_FloatTime();
		if ( fCurTime >= s_viewParamsLast.flTime &&
			 fCurTime < s_viewParamsLast.flTime + 0.4 )
			 return;

		s_viewParamsLast.flTime = fCurTime;
		s_viewParamsLast.bHighlighted = !s_viewParamsLast.bHighlighted;
	}

	// Find the empty texture
	ITexture *txEmpty = materials->FindTexture( "debugempty", "", false );

	typedef unsigned short MapIdx;

	// Now walk over all the materials in the req list and push them to the params
	for ( int k = 0, kEnd = s_viewParamsReq.lstMaterials.Count(); k < kEnd; ++ k )
	{
		UtlSymId_t idMat = s_viewParamsReq.lstMaterials[ k ];
		MapIdx idx = s_viewParamsLast.lstMaterials.Find( idMat );
		if ( idx == s_viewParamsLast.lstMaterials.InvalidIndex() )
		{
			// Insert the new material
			idx = s_viewParamsLast.lstMaterials.Insert( idMat );
			ViewParamsLast::VarMap &vars = s_viewParamsLast.lstMaterials.Element( idx );

			// Locate the material
			char const *szMaterialName = CUtlSymbol( idMat ).String();
			IMaterial *pMat = materials->FindMaterial( szMaterialName, TEXTURE_GROUP_OTHER, false );
			if ( pMat )
			{
				int numParams = pMat->ShaderParamCount();
				IMaterialVar **arrVars = pMat->GetShaderParams();
				for ( int idxParam = 0; idxParam < numParams; ++ idxParam )
				{
// 					if ( !stricmp( arrVars[ idxParam ]->GetName(), "$ignorez" ) )
// 					{
// 						vars.m_matInfo.ignorez = arrVars[ idxParam ]->GetIntValue() ? true : false;
// 						arrVars[ idxParam ]->SetIntValue( 1 );
// 						continue;
// 					}

					if ( !arrVars[ idxParam ]->IsTexture() )
						continue;

					ITexture *pTex = arrVars[ idxParam ]->GetTextureValue();
					if ( !pTex || pTex->IsError() )
						continue;

					ViewParamsLast::TxInfo txinfo;
					txinfo.name = CUtlSymbol( pTex->GetName() );
					txinfo.pTx = pTex;
					vars.Insert( CUtlSymbol( arrVars[ idxParam ]->GetName() ), txinfo );
				}
			}
		}
	}

	// Now walk over all the materials that we have to highlight or unhighlight
	for ( MapIdx idx = s_viewParamsLast.lstMaterials.FirstInorder();
		idx != s_viewParamsLast.lstMaterials.InvalidIndex();
		/* advance inside */ )
	{
		UtlSymId_t idMat = s_viewParamsLast.lstMaterials.Key( idx );
		ViewParamsLast::VarMap &vars = s_viewParamsLast.lstMaterials.Element( idx );

		bool bRemovedSelection = ( s_viewParamsReq.lstMaterials.Find( idMat ) < 0 );

		char const *szMaterialName = CUtlSymbol( idMat ).String();
		IMaterial *pMat = materials->FindMaterial( szMaterialName, TEXTURE_GROUP_OTHER, false );
		if ( pMat )
		{
			int numParams = pMat->ShaderParamCount();
			IMaterialVar **arrVars = pMat->GetShaderParams();
			for ( int idxParam = 0; idxParam < numParams; ++ idxParam )
			{
// 				if ( bRemovedSelection && !stricmp( arrVars[ idxParam ]->GetName(), "$ignorez" ) )
// 				{
// 					arrVars[ idxParam ]->SetIntValue( vars.m_matInfo.ignorez ? 1 : 0 );
// 					continue;
// 				}

				char const *szVarName = arrVars[ idxParam ]->GetName();
				CUtlSymbol symVarName( szVarName );
				MapIdx idxVarsTxInfo = vars.Find( symVarName );
				if ( idxVarsTxInfo != vars.InvalidIndex() )
				{
					ViewParamsLast::TxInfo &ti = vars.Element( idxVarsTxInfo );

					ITexture *pTx = ( s_viewParamsLast.bHighlighted && !bRemovedSelection ) ? txEmpty : ti.pTx;
					Assert( ti.pTx && !ti.pTx->IsError() );

					arrVars[ idxParam ]->SetTextureValue( pTx );
				}
			}
		}

		// Index advance
		if ( bRemovedSelection )
		{
			MapIdx idxRemove = idx;
			idx = s_viewParamsLast.lstMaterials.NextInorder( idx );
			s_viewParamsLast.lstMaterials.RemoveAt( idxRemove );
		}
		else
		{
			idx = s_viewParamsLast.lstMaterials.NextInorder( idx );
		}
	}
}

}; // end namespace MatViewOverride


//////////////////////////////////////////////////////////////////////////
//
// Custom text entry with "Open" for vmt's
//
//////////////////////////////////////////////////////////////////////////

class CVmtTextEntry : public vgui::TextEntry
{
	DECLARE_CLASS_SIMPLE( CVmtTextEntry, vgui::TextEntry );

public:
	CVmtTextEntry( vgui::Panel *parent, char const *szName ) : BaseClass( parent, szName ) {}

public:
	MESSAGE_FUNC( OpenVmtSelected, "DoOpenVmtSelected" );

	virtual void OpenEditMenu();
};

void CVmtTextEntry::OpenEditMenu()
{
	vgui::Menu *pEditMenu = BaseClass::GetEditMenu();

	int pos = pEditMenu->AddMenuItem( "Open VMT", new KeyValues("DoOpenVmtSelected"), this );
	pEditMenu->MoveMenuItem( pos, 0 );

	int x0, x1;
	pEditMenu->SetItemEnabled( "Open VMT", GetSelectedRange(x0, x1) );

	BaseClass::OpenEditMenu();
}

void CVmtTextEntry::OpenVmtSelected()
{
	int x0, x1;
	if ( !GetSelectedRange(x0, x1) )
		return;

	CUtlVector<char> buf;
	buf.SetCount( x1 - x0 + 1 );
	GetTextRange( buf.Base(), x0, x1 - x0 );
	
	for ( char *pchName = buf.Base(), *pchNext = NULL; pchName; pchName = pchNext )
	{
		if ( ( pchNext = strchr( pchName, '\n' ) ) != NULL )
			*( pchNext ++ ) = 0;

		char chResolveName[ 256 ] = {0}, chResolveNameArg[ 256 ] = {0};
		Q_snprintf( chResolveNameArg, sizeof( chResolveNameArg ) - 1, "materials/%s.vmt", pchName );
		char const *szResolvedName = g_pFileSystem->RelativePathToFullPath( chResolveNameArg, "game", chResolveName, sizeof( chResolveName ) - 1 );

		if ( szResolvedName )
			vgui::system()->ShellExecuteEx( "open", szResolvedName, "" );
	}
}

#ifdef IS_WINDOWS_PC
bool IsSpaceOrQuote( char val )
{
	return (isspace(val) || val == '\"');
}

static bool SetBufferValue( char *chTxtFileBuffer, char const *szLookupKey, char const *szNewValue )
{
	bool bResult = false;

	size_t lenTmp = strlen( szNewValue );
	size_t nTxtFileBufferLen = strlen( chTxtFileBuffer );

	for ( char *pch = chTxtFileBuffer;
		( NULL != ( pch = strstr( pch, szLookupKey ) ) );
		++ pch )
	{
		char *val = pch + strlen( szLookupKey );
		if ( !IsSpaceOrQuote( *val ) )
			continue;
		else
			++ val;

		// Skip over the whitespace and quotes
		while ( *val && IsSpaceOrQuote( *val ) )
			++ val;
		char *pValStart = val;

		// Okay, here comes the value
		while ( *val && IsSpaceOrQuote( *val ) )
			++ val;
		while ( *val && !IsSpaceOrQuote( *val ) )
			++ val;

		char *pValEnd = val; // Okay, here ends the value

		memmove( pValStart + lenTmp, pValEnd, chTxtFileBuffer + nTxtFileBufferLen + 1 - pValEnd );
		memcpy( pValStart, szNewValue, lenTmp );

		nTxtFileBufferLen += ( lenTmp - ( pValEnd - pValStart ) );
		bResult = true;
	}

	if ( !bResult )
	{
		char *pchAdd = chTxtFileBuffer + nTxtFileBufferLen;
		strcpy( pchAdd + strlen( pchAdd ), "\n" );
		strcpy( pchAdd + strlen( pchAdd ), szLookupKey );
		strcpy( pchAdd + strlen( pchAdd ), " " );
		strcpy( pchAdd + strlen( pchAdd ), szNewValue );
		strcpy( pchAdd + strlen( pchAdd ), "\n" );
		bResult = true;
	}

	return bResult;
}

// Replaces the first occurrence of "szFindData" with "szNewData"
// Returns the remaining buffer past the replaced data or NULL if
// no replacement occurred.
static char * BufferReplace( char *buf, char const *szFindData, char const *szNewData )
{
	size_t len = strlen( buf ), lFind = strlen( szFindData ), lNew = strlen( szNewData );
	if ( char *pBegin = strstr( buf, szFindData ) )
	{
		memmove( pBegin + lNew, pBegin + lFind, buf + len - ( pBegin + lFind ) );
		memmove( pBegin, szNewData, lNew );
		return pBegin + lNew;
	}
	return NULL;
}

class CP4Requirement
{
public:
	CP4Requirement();
	~CP4Requirement();

protected:
	bool m_bLoadedModule;
	CSysModule *m_pP4Module;
};

CP4Requirement::CP4Requirement() :
	m_bLoadedModule( false ),
	m_pP4Module( NULL )
{
#ifdef STAGING_ONLY
	if ( p4 )
		return;

	// load the p4 lib
	m_pP4Module = Sys_LoadModule( "p4lib" );
	m_bLoadedModule = true;
		
	if ( m_pP4Module )
	{
		CreateInterfaceFn factory = Sys_GetFactory( m_pP4Module );
		if ( factory )
		{
			p4 = ( IP4 * )factory( P4_INTERFACE_VERSION, NULL );

			if ( p4 )
			{
				extern CreateInterfaceFn g_AppSystemFactory;
				p4->Connect( g_AppSystemFactory );
				p4->Init();
			}
		}
	}
#endif // STAGING_ONLY

	if ( !p4 )
	{
		Warning( "Can't load p4lib.dll\n" );
	}
}

CP4Requirement::~CP4Requirement()
{
	if ( m_bLoadedModule && m_pP4Module )
	{
		if ( p4 )
		{
			p4->Shutdown();
			p4->Disconnect();
		}

		Sys_UnloadModule( m_pP4Module );
		m_pP4Module = NULL;
		p4 = NULL;
	}
}
#endif // #ifdef IS_WINDOWS_PC

//////////////////////////////////////////////////////////////////////////
//
// Render textures edit panel
//
//////////////////////////////////////////////////////////////////////////

class CRenderTextureEditor : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CRenderTextureEditor, vgui::Frame );

public:
	CRenderTextureEditor( vgui::Panel *parent, char const *szName );
	~CRenderTextureEditor();

public:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnCommand( const char *command );
	virtual void SetFont( vgui::HFont hFont ) { m_hFont = hFont; Repaint(); }
	virtual vgui::HFont GetFont() { return m_hFont; }

public:
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code ) { OnMousePressed( code ); }
	virtual void Activate();
	virtual void Close();

public:
	void SetDispInfo( KeyValues *kv, int iHint );
	void GetDispInfo( KeyValues *&kv, int &iHint );

public:
	virtual void Paint();

	enum
	{
		TILE_BORDER = 10,
		TILE_SIZE = 550,
		TILE_TEXTURE_SIZE = 256,
		TILE_TEXT = 70
	};

protected:
	vgui::HFont			m_hFont;
	CVmtTextEntry		*m_pMaterials;
	vgui::Button		*m_pExplore;
	vgui::Button		*m_pReload;
	vgui::Button		*m_pRebuild;
	vgui::Button		*m_pToggleNoMip;
	vgui::Button		*m_pCopyTxt;
#ifndef POSIX
	vgui::Button		*m_pCopyImg;
#endif
	vgui::Button		*m_pSaveImg;
	vgui::Button		*m_pSizeControls[2];
	vgui::Button		*m_pFlashBtn;
	KeyValues			*m_pInfo;
	CUtlBuffer			m_bufInfoText;
	CUtlVector< UtlSymId_t > m_lstMaterials;
	int					m_iInfoHint;
};

CRenderTextureEditor::CRenderTextureEditor( vgui::Panel *parent, char const *szName ) :
	BaseClass( parent, szName ),
	m_pInfo( NULL ),
	m_iInfoHint( 0 ),
	m_hFont( vgui::INVALID_FONT ),
	m_bufInfoText( 0, 0, CUtlBuffer::TEXT_BUFFER )
{
	m_pMaterials = vgui::SETUP_PANEL( new CVmtTextEntry( this, "Materials" ) );
	m_pMaterials->SetMultiline( true );
	m_pMaterials->SetEditable( false );
	m_pMaterials->SetEnabled( false );
	m_pMaterials->SetVerticalScrollbar( true );
	m_pMaterials->SetVisible( true );

	m_pExplore = vgui::SETUP_PANEL( new vgui::Button( this, "Explore", "Open", this, "Explore" ) );
	m_pExplore->SetVisible( true );

	m_pReload = vgui::SETUP_PANEL( new vgui::Button( this, "Reload", "Reload", this, "Reload" ) );
	m_pReload->SetVisible( true );

	m_pRebuild = vgui::SETUP_PANEL( new vgui::Button( this, "RebuildVTF", "Rebuild VTF", this, "RebuildVTF" ) );
	m_pRebuild->SetVisible( true );

	m_pToggleNoMip = vgui::SETUP_PANEL( new vgui::Button( this, "ToggleNoMip", "ToggleNoMip", this, "ToggleNoMip" ) );
	m_pToggleNoMip->SetVisible( true );

	m_pCopyTxt = vgui::SETUP_PANEL( new vgui::Button( this, "CopyTxt", "Copy Text", this, "CopyTxt" ) );
	m_pCopyTxt->SetVisible( true );

#ifndef POSIX
	m_pCopyImg = vgui::SETUP_PANEL( new vgui::Button( this, "CopyImg", "Copy Image", this, "CopyImg" ) );
	m_pCopyImg->SetVisible( true );
#endif

	m_pSaveImg = vgui::SETUP_PANEL( new vgui::Button( this, "SaveImg", "Save Image", this, "SaveImg" ) );
	m_pSaveImg->SetVisible( true );

	m_pFlashBtn = vgui::SETUP_PANEL( new vgui::Button( this, "FlashBtn", "Flash in Game", this, "FlashBtn" ) );
	m_pFlashBtn->SetVisible( true );

	m_pSizeControls[0] = vgui::SETUP_PANEL( new vgui::Button( this, "--", "--", this, "size-" ) );
	m_pSizeControls[1] = vgui::SETUP_PANEL( new vgui::Button( this, "+", "+", this, "size+" ) );
}

CRenderTextureEditor::~CRenderTextureEditor()
{
	SetDispInfo( NULL, 0 );
}

void CRenderTextureEditor::GetDispInfo( KeyValues *&kv, int &iHint )
{
	iHint = m_iInfoHint;
	kv = m_pInfo;
}

void CRenderTextureEditor::SetDispInfo( KeyValues *kv, int iHint )
{
	m_iInfoHint = iHint;

	if ( m_pInfo )
		m_pInfo->deleteThis();

	m_pInfo = kv ? kv->MakeCopy() : NULL;


	CUtlStringMap< bool > arrMaterials, arrMaterialsFullNames;

	if ( kv )
	{
		char const *szTextureName = kv->GetString( KEYNAME_NAME );
		for ( MaterialHandle_t hm = materials->FirstMaterial();
			hm != materials->InvalidMaterial(); hm = materials->NextMaterial( hm ) )
		{
			IMaterial *pMat = materials->GetMaterial( hm );
			if ( !pMat )
				continue;

			int numParams = pMat->ShaderParamCount();
			IMaterialVar **arrVars = pMat->GetShaderParams();
			for ( int idxParam = 0; idxParam < numParams; ++ idxParam )
			{
				if ( !arrVars[ idxParam ]->IsTexture() )
					continue;

				ITexture *pTex = arrVars[ idxParam ]->GetTextureValue();
				if ( !pTex || pTex->IsError() )
					continue;

				if ( !stricmp( pTex->GetName(), szTextureName ) )
				{
					bool bRealMaterial = true;

					if ( StringHasPrefix( pMat->GetName(), "debug/debugtexture" ) )
						bRealMaterial = false;

					if ( StringHasPrefix( pMat->GetName(), "maps/" ) )
					{
						// Format:   maps/mapname/[matname]_x_y_z
						bRealMaterial = false;

						char chName[ MAX_PATH ];
						Q_strncpy( chName, pMat->GetName() + 5, sizeof( chName ) - 1 );
						
						if ( char *szMatName = strchr( chName, '/' ) )
						{
							++ szMatName;
							for ( int k = 0; k < 3; ++ k )
							{
								if ( char *rUnderscore = strrchr( szMatName, '_' ) )
									*rUnderscore = 0;
							}

							sprintf( szMatName + strlen( szMatName ), " (cubemap)" );
							arrMaterials[ szMatName ] = true;
						}
						arrMaterialsFullNames[ pMat->GetName() ] = true;
					}

					if ( bRealMaterial )
					{
						arrMaterials[ pMat->GetName() ] = true;
						arrMaterialsFullNames[ pMat->GetName() ] = true;
					}

					break;
				}
			}
		}
	}

	// Now that we have a list of materials make a printable version
	CUtlBuffer bufText( 0, 0, CUtlBuffer::TEXT_BUFFER );

	if ( !arrMaterials.GetNumStrings() )
	{
		bufText.Printf( "-- no materials --" );
	}
	else
	{
		int c = arrMaterials.GetNumStrings();
		bufText.Printf( "  %d material%s:", c, ( c%10 == 1 && c != 11 ) ? "" : "s" );
	}

	for ( int k = 0; k < arrMaterials.GetNumStrings(); ++ k )
	{
		bufText.Printf( "\n%s", arrMaterials.String( k ) );
	}

	m_pSaveImg->SetVisible( false );

	// Set text except for the cases when m_pInfo and no materials found
	if ( !( m_pInfo && !arrMaterials.GetNumStrings() ) )
	{
		if ( kv )
		{
			int iTxWidth = kv->GetInt( KEYNAME_WIDTH );
			int iTxHeight = kv->GetInt( KEYNAME_HEIGHT );
			char const *szTxFormat = kv->GetString( KEYNAME_FORMAT );

			bufText.Printf( "\n%dx%d Format:%s", iTxWidth, iTxHeight, szTxFormat );

			// We support saving the 8888 formats right now: "RGBA8888", "ABGR8888", "ARGB8888", "BGRA8888", "BGRX8888"
			if( Q_strstr( szTxFormat, "8888" ) )
			{
				m_pSaveImg->SetVisible( true );
			}
		}

		m_pMaterials->SetText( ( char const * ) bufText.Base() );

		m_lstMaterials.RemoveAll();
		m_lstMaterials.EnsureCapacity( arrMaterialsFullNames.GetNumStrings() );
		for ( int k = 0; k < arrMaterialsFullNames.GetNumStrings(); ++ k )
		{
			m_lstMaterials.AddToTail( CUtlSymbol( arrMaterialsFullNames.String( k ) ) );
		}
	}

	m_bufInfoText.Clear();

	InvalidateLayout();
}

void CRenderTextureEditor::Close()
{
	BaseClass::Close();

	SetDispInfo( NULL, 0 );
}

void CRenderTextureEditor::Activate()
{
	BaseClass::Activate();
}

void CRenderTextureEditor::PerformLayout()
{
	BaseClass::PerformLayout();

	int iRenderedHeight = 4 * TILE_BORDER + TILE_TEXT + TILE_SIZE;

	SetSize( 4 * TILE_BORDER + TILE_SIZE,
		iRenderedHeight + 90 + TILE_BORDER );
	
	m_pMaterials->SetPos( TILE_BORDER, iRenderedHeight + 2 );
	m_pMaterials->SetSize( 2 * TILE_BORDER + TILE_SIZE, 90 );

	m_pExplore->SetPos( 2 * TILE_BORDER + TILE_SIZE - 50, 2 * TILE_BORDER );
	m_pExplore->SetWide( 50 );

	m_pReload->SetPos( 2 * TILE_BORDER + TILE_SIZE - 50 - 65, 2 * TILE_BORDER );
	m_pReload->SetWide( 60 );
	m_pReload->SetVisible( m_lstMaterials.Count() > 0 );

	m_pRebuild->SetPos( 2 * TILE_BORDER + TILE_SIZE - 50 - 65 - 95, 2 * TILE_BORDER );
	m_pRebuild->SetWide( 90 );
	m_pRebuild->SetVisible( m_lstMaterials.Count() > 0 );

	m_pToggleNoMip->SetPos( 2 * TILE_BORDER + TILE_SIZE - 50 - 95, (2 * TILE_BORDER) + m_pReload->GetTall() + 1 );
	m_pToggleNoMip->SetWide( 90 );
	m_pToggleNoMip->SetVisible( m_lstMaterials.Count() > 0 );

	m_pExplore->SetVisible( false );
	m_pSizeControls[0]->SetVisible( false );
	m_pSizeControls[1]->SetVisible( false );

	if ( m_pInfo )
	{
		char chResolveName[ 256 ] = {0}, chResolveNameArg[ 256 ] = {0};
		Q_snprintf( chResolveNameArg, sizeof( chResolveNameArg ) - 1, "materials/%s.vtf", m_pInfo->GetString( KEYNAME_NAME ) );
		char const *szResolvedName = g_pFileSystem->RelativePathToFullPath( chResolveNameArg, "game", chResolveName, sizeof( chResolveName ) - 1 );
		if ( szResolvedName )
		{
			m_pExplore->SetVisible( true );
		}

		if ( !m_pInfo->GetInt( "SpecialTx" ) )
		{
			m_pSizeControls[0]->SetVisible( true );
			m_pSizeControls[1]->SetVisible( true );

			m_pSizeControls[0]->SetEnabled( CanAdjustTextureSize( m_pInfo->GetString( KEYNAME_NAME ), false ) );
			m_pSizeControls[1]->SetEnabled( CanAdjustTextureSize( m_pInfo->GetString( KEYNAME_NAME ), true ) );

			int posX, posY;
			m_pExplore->GetPos( posX, posY );
			m_pSizeControls[0]->SetPos( posX, posY + m_pExplore->GetTall() + 1 );
			m_pSizeControls[0]->SetWide( m_pExplore->GetWide() / 2 );
			m_pSizeControls[1]->SetPos( posX + m_pSizeControls[0]->GetWide() + 1, posY + m_pExplore->GetTall() + 1 );
			m_pSizeControls[1]->SetWide( m_pExplore->GetWide() - ( m_pSizeControls[0]->GetWide() + 1 ) );
		}
	}

	{
		int posX, posY;
		m_pExplore->GetPos( posX, posY );
		posY += m_pExplore->GetTall() * 2 + 2;
		posX += m_pExplore->GetWide();

		posX -= 80;
		m_pSaveImg->SetPos( posX, posY );
		m_pSaveImg->SetWide( 80 );

#ifndef POSIX
		posX -= 80 + 5;
		m_pCopyImg->SetPos( posX, posY );
		m_pCopyImg->SetWide( 80 );
#endif

		posX -= 80 + 5;
		m_pCopyTxt->SetPos( posX, posY );
		m_pCopyTxt->SetWide( 80 );

		posX -= 95 + 5;
		m_pFlashBtn->SetPos( posX, posY );
		m_pFlashBtn->SetWide( 95 );
	}
}

#ifdef IS_WINDOWS_PC
bool GetTextureContentPath( const char *pszVTF, char *szTextureContentPath, int nBufSize )
{
	char vhRelVTF[MAX_PATH];
	Q_snprintf( vhRelVTF, sizeof( vhRelVTF ) - 1, "materials/%s.vtf", pszVTF );

	ConVarRef mat_texture_list_content_path( "mat_texture_list_content_path" );
	if ( !mat_texture_list_content_path.GetString()[0] )
	{
		szTextureContentPath = const_cast< char * >( g_pFileSystem->RelativePathToFullPath( vhRelVTF, "game", szTextureContentPath, nBufSize - 1 ) );

		if ( !szTextureContentPath )
		{
			Warning( " texture '%s' is not loaded from file system.\n", pszVTF );
			return false;
		}
		if ( !BufferReplace( szTextureContentPath, "\\game\\", "\\content\\" ) ||
			!BufferReplace( szTextureContentPath, "\\materials\\", "\\materialsrc\\" ) )
		{
			Warning( " texture '%s' cannot be mapped to content directory.\n", pszVTF );
			return false;
		}
	}
	else
	{
		V_strncpy( szTextureContentPath, mat_texture_list_content_path.GetString(), MAX_PATH );
		V_strncat( szTextureContentPath, "/", MAX_PATH );
		V_strncat( szTextureContentPath, pszVTF, MAX_PATH );
		V_strncat( szTextureContentPath, ".vtf", MAX_PATH );
	}

	return true;
}
#endif // #ifdef IS_WINDOWS_PC

void CRenderTextureEditor::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );

	if ( !stricmp( command, "Explore" ) && m_pInfo )
	{
		char chResolveName[ 256 ] = {0}, chResolveNameArg[ 256 ] = {0};
		Q_snprintf( chResolveNameArg, sizeof( chResolveNameArg ) - 1, "materials/%s.vtf", m_pInfo->GetString( KEYNAME_NAME ) );
		char const *szResolvedName = g_pFileSystem->RelativePathToFullPath( chResolveNameArg, "game", chResolveName, sizeof( chResolveName ) - 1 );

		char params[256];
		Q_snprintf( params, sizeof( params ) - 1, "/E,/SELECT,%s", szResolvedName );
		vgui::system()->ShellExecuteEx( "open", "explorer.exe", params );
	}

	if ( !stricmp( command, "Reload" ) && m_lstMaterials.Count() )
	{
		CUtlBuffer bufCommand( 0, 0, CUtlBuffer::TEXT_BUFFER );
		int idxMaterial = 0;

		//
		// Issue the console command several times in case we overflow the
		// console command buffer size.
		//
		Cbuf_Execute();
		for ( ; idxMaterial < m_lstMaterials.Count(); )
		{
			int iOldIdx = idxMaterial;
			bufCommand.Printf( "mat_reloadmaterial \"" );

			for ( ; idxMaterial < m_lstMaterials.Count(); ++ idxMaterial )
			{
				int iOldSize = bufCommand.TellPut();
				int const iSizeLimit = CCommand::MaxCommandLength() - 3;

				char const *szString = CUtlSymbol( m_lstMaterials[idxMaterial] ).String();
				bufCommand.Printf( "%s%s", ( idxMaterial > iOldIdx ) ? "*" : "", szString );

				if ( bufCommand.TellPut() > iSizeLimit &&
					 idxMaterial > iOldIdx )
				{
					bufCommand.SeekPut( CUtlBuffer::SEEK_HEAD, iOldSize );
					break;
				}
			}

			bufCommand.Printf( "\"\n" );
			bufCommand.PutChar( 0 );

			Cbuf_AddText( ( char const * ) bufCommand.Base() );
			bufCommand.Clear();
			Cbuf_Execute();
		}
	}

	if ( !stricmp( command, "CopyTxt" ) )
	{
		char const *szName = ( char const * ) m_bufInfoText.Base();
		if ( !m_bufInfoText.TellPut() || !szName )
			szName = "";
		vgui::system()->SetClipboardText( szName, strlen( szName ) + 1 );
	}

	if ( !stricmp( command, "CopyImg" ) )
	{
		int x = 0, y = 0;
		this->LocalToScreen( x, y );

		void *pMainWnd = game->GetMainWindow();
		vgui::system()->SetClipboardImage( pMainWnd, x, y, x + GetWide(), y + GetTall() );
	}

	if ( !stricmp( command, "SaveImg" ) )
	{
		SaveTextureImage( m_pInfo->GetString( KEYNAME_NAME ) );
	}

	if ( !stricmp( command, "FlashBtn" ) )
	{
		MatViewOverride::RequestSelectNone();
		MatViewOverride::RequestSelected( m_lstMaterials.Count(), m_lstMaterials.Base() );
		mat_texture_list_off_f();
	}

	if ( ( !stricmp( command, "size-" ) || !stricmp( command, "size+" ) ) && m_pInfo )
	{
		bool bSizeUp = ( command[4] == '+' );
		bool bResult = AdjustTextureSize( m_pInfo->GetString( KEYNAME_NAME ), bSizeUp );
		if ( bResult )
		{
			CAutoPushPop<bool> auto_g_bRecursiveRequestToShowTextureList( g_bRecursiveRequestToShowTextureList, true );
			mat_texture_list_on_f();
		}

		InvalidateLayout();
	}

	if ( !stricmp( command, "ToggleNoMip" ) )
	{
#ifdef IS_WINDOWS_PC
		CP4Requirement p4req;
		if ( !p4 )
			g_p4factory->SetDummyMode( true );

		char const *szTextureFile = m_pInfo->GetString( KEYNAME_NAME );
		char const *szTextureGroup = m_pInfo->GetString( KEYNAME_TEXTURE_GROUP );

		ITexture *pMatTexture = NULL;
		if ( *szTextureFile )
			pMatTexture = materials->FindTexture( szTextureFile, szTextureGroup, false );
		if ( !pMatTexture )
			pMatTexture = materials->FindTexture( "debugempty", "", false );

		bool bNewNoMip = !(pMatTexture->GetFlags() & TEXTUREFLAGS_NOMIP);

		char szFileName[MAX_PATH];
		if ( !GetTextureContentPath( szTextureFile, szFileName, sizeof(szFileName) ) )
			return;

		// Figure out what kind of source content is there:
		// 1. look for TGA - if found, get the txt file (if txt file missing, create one)
		// 2. otherwise look for PSD - affecting psdinfo
		// 3. else error
		char *pExtPut = szFileName + strlen( szFileName ) - strlen( ".vtf" ); // compensating the TEXTURE_FNAME_EXTENSION(.vtf) extension

		// 1.tga
		sprintf( pExtPut, ".tga" );
		if ( g_pFullFileSystem->FileExists( szFileName ) )
		{
			// Have tga - pump in the txt file
			sprintf( pExtPut, ".txt" );

			CUtlBuffer bufTxtFileBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
			g_pFullFileSystem->ReadFile( szFileName, 0, bufTxtFileBuffer );
			for ( int k = 0; k < 1024; ++ k ) bufTxtFileBuffer.PutChar( 0 );

			// Now fix maxwidth/maxheight settings
			SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "nomip", bNewNoMip ? "1" : "0" );
			bufTxtFileBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, strlen( ( char * ) bufTxtFileBuffer.Base() ) );

			// Check out or add the file
			g_p4factory->SetOpenFileChangeList( "Texture LOD Autocheckout" );
			CP4AutoEditFile autop4_edit( szFileName );

			// Save the file contents
			if ( g_pFullFileSystem->WriteFile( szFileName, 0, bufTxtFileBuffer ) )
			{
				Msg(" '%s' : saved.\n", szFileName );
				CP4AutoAddFile autop4_add( szFileName );
			}
			else
			{
				Warning( " '%s' : failed to save - toggle \"nomip\" manually.\n",
					szFileName );
			}
		}
		else
		{
			// 2.psd
			sprintf( pExtPut, ".psd" );
			if ( g_pFullFileSystem->FileExists( szFileName ) )
			{
				char chCommand[MAX_PATH];
				char szTxtFileName[MAX_PATH] = {0};
				GetModSubdirectory( "tmp_lod_psdinfo.txt", szTxtFileName, sizeof( szTxtFileName ) );
				sprintf( chCommand, "/C psdinfo \"%s\" > \"%s\"", szFileName, szTxtFileName);
				vgui::system()->ShellExecuteEx( "open", "cmd.exe", chCommand );
				Sys_Sleep( 200 );

				CUtlBuffer bufTxtFileBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
				g_pFullFileSystem->ReadFile( szTxtFileName, 0, bufTxtFileBuffer );
				for ( int k = 0; k < 1024; ++ k ) bufTxtFileBuffer.PutChar( 0 );

				// Now fix maxwidth/maxheight settings
				SetBufferValue( ( char * ) bufTxtFileBuffer.Base(), "nomip", bNewNoMip ? "1" : "0" );
				bufTxtFileBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, strlen( ( char * ) bufTxtFileBuffer.Base() ) );

				// Check out or add the file
				// Save the file contents
				if ( g_pFullFileSystem->WriteFile( szTxtFileName, 0, bufTxtFileBuffer ) )
				{
					g_p4factory->SetOpenFileChangeList( "Texture LOD Autocheckout" );
					CP4AutoEditFile autop4_edit( szFileName );

					sprintf( chCommand, "/C psdinfo -write \"%s\" < \"%s\"", szFileName, szTxtFileName );
					Sys_Sleep( 200 );
					vgui::system()->ShellExecuteEx( "open", "cmd.exe", chCommand );
					Sys_Sleep( 200 );

					Msg(" '%s' : saved.\n", szFileName );
					CP4AutoAddFile autop4_add( szFileName );
				}
				else
				{
					Warning( " '%s' : failed to save - toggle \"nomip\" manually.\n",
						szFileName );
				}
			}
			else
			{
				// 3. - error
				sprintf( pExtPut, "" );
				Warning( " '%s' : doesn't specify a valid TGA or PSD file!\n", szFileName );
				return;
			}
		}

		// Now reload the texture
		OnCommand( "RebuildVTF" );
#endif // #ifdef IS_WINDOWS_PC
	}
	
	if ( !stricmp( command, "RebuildVTF" ) )
	{
#ifdef IS_WINDOWS_PC
		CP4Requirement p4req;
		if ( !p4 )
			g_p4factory->SetDummyMode( true );

		CSysModule *pModule;
		IVTex *pIVTex = VTex_Load( &pModule );
		if ( !pIVTex )
			return;

		char const *szTextureFile = m_pInfo->GetString( KEYNAME_NAME );

		char szContentFilename[MAX_PATH];
		if ( !GetTextureContentPath( szTextureFile, szContentFilename, sizeof(szContentFilename) ) )
			return;

		char pGameDir[MAX_OSPATH];
		COM_GetGameDir( pGameDir, sizeof( pGameDir ) );

		// Check out the VTF
		char szVTFFilename[MAX_PATH];
		Q_snprintf( szVTFFilename, sizeof( szVTFFilename ) - 1, "%s/materials/%s.vtf", pGameDir, szTextureFile );
		g_p4factory->SetOpenFileChangeList( "Texture LOD Autocheckout" );
		CP4AutoEditFile autop4_edit( szVTFFilename );

		// Get the directory for the outdir
		StripDirName( szVTFFilename );

		// Now we've modified the source, rebuild the texture
		char *argv[64];
		int iArg = 0;
		argv[iArg++] = "";
		argv[iArg++] = "-quiet";
		argv[iArg++] = "-UseStandardError";	// These are only here for the -currently released- version of vtex.dll.
		argv[iArg++] = "-WarningsAsErrors";
		argv[iArg++] = "-outdir";
		argv[iArg++] = szVTFFilename;
		argv[iArg++] = szContentFilename;
		pIVTex->VTex( CubemapsFSFactory, pGameDir, iArg, argv );

		VTex_Unload( pModule );

		Sys_Sleep( 200 );

		// Now reload the texture
		OnCommand( "Reload" );
#endif // #ifdef IS_WINDOWS_PC
	}
}

void CRenderTextureEditor::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFont( pScheme->GetFont( "Default", IsProportional() ) );
}

void CRenderTextureEditor::OnMousePressed( vgui::MouseCode code )
{
	Close();
}

void CRenderTextureEditor::Paint()
{
	CAutoMatSysDebugMode auto_matsysdebugmode;

	DisableFadeEffect();
	SetBgColor( Color( 10, 50, 10, 255 ) );

	BaseClass::Paint();

	KeyValues *kv = m_pInfo;
	if ( !kv )
		return;

	char const *szTextureFile = kv->GetString( KEYNAME_NAME );
	Q_strncpy( s_chLastViewedTextureBuffer, szTextureFile, sizeof( s_chLastViewedTextureBuffer ) );
	char const *szTextureGroup = kv->GetString( KEYNAME_TEXTURE_GROUP );

	ITexture *pMatTexture = NULL;
	if ( *szTextureFile )
		pMatTexture = materials->FindTexture( szTextureFile, szTextureGroup, false );
	if ( !pMatTexture )
		pMatTexture = materials->FindTexture( "debugempty", "", false );

	// Determine the texture size
	int iTxWidth = kv->GetInt( KEYNAME_WIDTH );
	int iTxHeight = kv->GetInt( KEYNAME_HEIGHT );
	int iTxSize = kv->GetInt( KEYNAME_SIZE );
	char const *szTxFormat = kv->GetString( KEYNAME_FORMAT );

	int x = TILE_BORDER, y = TILE_BORDER;

	// Dimensions to draw
	int iDrawWidth = iTxWidth;
	int iDrawHeight = iTxHeight;

	if ( pMatTexture && pMatTexture->IsCubeMap() )
	{
		iDrawWidth = 1024;
		iDrawHeight = 1024;
	}

	if ( iDrawHeight >= iDrawWidth )
	{
		if ( iDrawHeight > TILE_TEXTURE_SIZE )
		{
			iDrawWidth = iDrawWidth * ( float( TILE_TEXTURE_SIZE ) / iDrawHeight );
			iDrawHeight = TILE_TEXTURE_SIZE;
		}

		if ( iDrawHeight < 64 )
		{
			iDrawWidth = iDrawWidth * ( float( 64 ) / iDrawHeight );
			iDrawHeight = 64;
		}
	}
	else
	{
		if ( iDrawWidth > TILE_TEXTURE_SIZE )
		{
			iDrawHeight = iDrawHeight * ( float( TILE_TEXTURE_SIZE ) / iDrawWidth );
			iDrawWidth = TILE_TEXTURE_SIZE;
		}

		if ( iDrawWidth < 64 )
		{
			iDrawHeight = iDrawHeight * ( float( 64 ) / iDrawWidth );
			iDrawWidth = 64;
		}
	}

	iDrawHeight = iDrawHeight / ( float( TILE_TEXTURE_SIZE ) / float( TILE_SIZE ) );
	iDrawWidth = iDrawWidth / ( float( TILE_TEXTURE_SIZE ) / float( TILE_SIZE ) );

	iDrawHeight = max( iDrawHeight, 4 );
	iDrawWidth = max( iDrawWidth, 4 );

	//
	// Draw frame
	//
	{
		int tileWidth = 2 * TILE_BORDER + TILE_SIZE;
		int tileHeight = 3 * TILE_BORDER + TILE_SIZE + TILE_TEXT;

		g_pMatSystemSurface->DrawSetColor( 255, 255, 255, 255 );
		g_pMatSystemSurface->DrawOutlinedRect( x + 1, y + 1,
			x + tileWidth - 2 , y + tileHeight - 2 );
	}

	//
	// Draw all
	//

	x += TILE_BORDER;
	y += TILE_BORDER;

	char chResolveName[ 256 ] = {0}, chResolveNameArg[ 256 ] = {0};
	Q_snprintf( chResolveNameArg, sizeof( chResolveNameArg ) - 1, "materials/%s.vtf", szTextureFile );
	char const *szResolvedName = g_pFileSystem->RelativePathToFullPath( chResolveNameArg, "game", chResolveName, sizeof( chResolveName ) - 1 );

	char const *szPrintFilePrefix = szResolvedName ? "" : "[?]/";
	char const *szPrintFileName = szResolvedName ? szResolvedName : szTextureFile;

	char chSizeBuf[20] = { 0 };
	if ( iTxSize >= 0 )
		FmtCommaNumber( chSizeBuf, iTxSize );
	else
		chSizeBuf[0] = '-';

	g_pMatSystemSurface->DrawColoredTextRect( GetFont(), x, y, TILE_SIZE, TILE_TEXT / 2,
		255, 255, 255, 255,
		"%s%s\n"
		"%s Kb    %dx%d    %s",
		szPrintFilePrefix, szPrintFileName,
		chSizeBuf,
		iTxWidth, iTxHeight,
		szTxFormat
		);
	
	if ( !m_bufInfoText.TellPut() )
	{
		m_bufInfoText.Printf(
			"%s%s\r\n"
			"%s Kb    %dx%d    %s",
			szPrintFilePrefix, szPrintFileName,
			chSizeBuf,
			iTxWidth, iTxHeight,
			szTxFormat );
	}

	if ( !kv->GetInt( "SpecialTx" ) )
	{
		char chLine1[256] = {0};
		char chLine2[256] = {0};

		//
		// Line 1
		//
		if ( iTxSize > g_warn_texkbytes )
			sprintf( chLine1 + strlen( chLine1 ), "  Size(%s Kb)", chSizeBuf );
		if ( ( iTxWidth > g_warn_texdimensions ) ||
			( iTxHeight > g_warn_texdimensions ) )
			sprintf( chLine1 + strlen( chLine1 ), "  Dimensions(%dx%d)", iTxWidth, iTxHeight );
		if ( stricmp( szTxFormat, "DXT1" ) &&
			stricmp( szTxFormat, "DXT5" ) )
			sprintf( chLine1 + strlen( chLine1 ), "  Format(%s)", szTxFormat );
		if ( pMatTexture->GetFlags() & TEXTUREFLAGS_NOLOD )
			sprintf( chLine1 + strlen( chLine1 ), "  NoLod" );
		if ( pMatTexture->GetFlags() & TEXTUREFLAGS_NOMIP )
			sprintf( chLine1 + strlen( chLine1 ), "  NoMip" );
		if ( pMatTexture->GetFlags() & TEXTUREFLAGS_ONEBITALPHA )
			sprintf( chLine1 + strlen( chLine1 ), "  OneBitAlpha" );

		//
		// Line 2
		//
		// Always show stats for higher/lower mips
		{
			int wmap = pMatTexture->GetMappingWidth(), hmap = pMatTexture->GetMappingHeight(), dmap = pMatTexture->GetMappingDepth();
			int wact = pMatTexture->GetActualWidth(), hact = pMatTexture->GetActualHeight(), dact = pMatTexture->GetActualDepth();
			ImageFormat fmt = pMatTexture->GetImageFormat();

			if ( wact > 4 || hact > 4 )
			{
				char chbuf[50];
				int mem = ImageLoader::GetMemRequired( max( min( wact, 4 ), wact / 2 ), max( min( hact, 4 ), hact / 2 ), max( 1, dact / 2 ), fmt, true );
				mem = ( mem + 511 ) / 1024;
				FmtCommaNumber( chbuf, mem );
				
				sprintf ( chLine2 + strlen( chLine2 ), "  %s Kb @ lower mip", chbuf );
			}

			if ( wmap > wact || hmap > hact || dmap > dact )
			{
				char chbuf[ 50 ];
				int mem = ImageLoader::GetMemRequired( min( wmap, wact * 2 ), min( hmap, hact * 2 ), min( dmap, dact * 2 ), fmt, true );
				mem = ( mem + 511 ) / 1024;
				FmtCommaNumber( chbuf, mem );

				sprintf ( chLine2 + strlen( chLine2 ), "      %s Kb @ higher mip", chbuf );
			}
		}

		if ( chLine1[0] )
		{
			g_pMatSystemSurface->DrawSetColor( 200, 0, 0, 255 );
			g_pMatSystemSurface->DrawFilledRect( x - TILE_BORDER/2, y + TILE_TEXT/2, x + TILE_BORDER/2 + (TILE_SIZE * 0.5), y + TILE_TEXT/2 + TILE_TEXT/4 );
			g_pMatSystemSurface->DrawColoredTextRect( GetFont(), x, y + TILE_TEXT/2, TILE_SIZE, TILE_TEXT / 4,
				255, 255, 255, 255,
				"%s", chLine1 );
		}
		if ( chLine2[0] )
		{
			// g_pMatSystemSurface->DrawSetColor( 200, 0, 0, 255 );
			// g_pMatSystemSurface->DrawFilledRect( x - TILE_BORDER/2, y + TILE_TEXT/2 + TILE_TEXT/4, x + TILE_BORDER/2 + TILE_SIZE, y + TILE_TEXT );
			g_pMatSystemSurface->DrawColoredTextRect( GetFont(), x, y + TILE_TEXT/2 + TILE_TEXT/4, TILE_SIZE, TILE_TEXT / 4,
				255, 255, 255, 255,
				"%s", chLine2 );
		}
	}

	y += TILE_TEXT + TILE_BORDER;

	// Images placement
	bool bHasAlpha = !!stricmp( szTxFormat, "DXT1" );

	int extTxWidth = TILE_SIZE;
	int extTxHeight = TILE_SIZE;

	int orgTxX = 0, orgTxXA = 0;
	int orgTxY = 0, orgTxYA = 0;

	if ( bHasAlpha )
	{
		if ( iTxWidth >= iTxHeight * 2 )
		{
			extTxHeight /= 2;
			orgTxYA = extTxHeight + TILE_BORDER/2;
			extTxHeight -= 1;
		}
		else if ( iTxHeight >= iTxWidth * 2 )
		{
			extTxWidth /= 2;
			orgTxXA = extTxWidth + TILE_BORDER/2;
			extTxWidth -= 3;
		}
		else
		{
			extTxHeight /= 2;
			orgTxYA = extTxHeight + TILE_BORDER/2;
			orgTxX = extTxWidth / 4;
			extTxWidth /= 2;

			if ( iDrawWidth > extTxWidth )
			{
				iDrawWidth /= 2;
				iDrawHeight /= 2;
			}
			extTxWidth -= 1;
			extTxHeight -= 1;
		}
	}

	enum { IMG_FRAME_OFF = 2 };
	if ( IMaterial *pMaterial = UseDebugMaterial( "debug/debugtexturecolor", pMatTexture, &auto_matsysdebugmode ) )
	{
		g_pMatSystemSurface->DrawSetColor( 255, 255, 255, 255 );
		g_pMatSystemSurface->DrawOutlinedRect( x + orgTxX + ( extTxWidth - iDrawWidth ) / 2 - IMG_FRAME_OFF, y + orgTxY + ( extTxHeight - iDrawHeight ) / 2 - IMG_FRAME_OFF,
			x + orgTxX + ( extTxWidth + iDrawWidth ) / 2 + IMG_FRAME_OFF, y + orgTxY + ( extTxHeight + iDrawHeight ) / 2 + IMG_FRAME_OFF );
		RenderTexturedRect( this, pMaterial,
			x + orgTxX + ( extTxWidth - iDrawWidth ) / 2, y + orgTxY + ( extTxHeight - iDrawHeight ) / 2,
			x + orgTxX + ( extTxWidth + iDrawWidth ) / 2, y + orgTxY + ( extTxHeight + iDrawHeight ) / 2 );

		if ( bHasAlpha )
		{
			orgTxX += orgTxXA;
			orgTxY += orgTxYA;
			if ( IMaterial *pMaterialDebug = UseDebugMaterial( "debug/debugtexturealpha", pMatTexture, &auto_matsysdebugmode ) )
			{
				g_pMatSystemSurface->DrawOutlinedRect( x + orgTxX + ( extTxWidth - iDrawWidth ) / 2 - IMG_FRAME_OFF, y + orgTxY + ( extTxHeight - iDrawHeight ) / 2 - IMG_FRAME_OFF,
					x + orgTxX + ( extTxWidth + iDrawWidth ) / 2 + IMG_FRAME_OFF, y + orgTxY + ( extTxHeight + iDrawHeight ) / 2 + IMG_FRAME_OFF );
				RenderTexturedRect( this, pMaterialDebug,
					x + orgTxX + ( extTxWidth - iDrawWidth ) / 2, y + orgTxY + ( extTxHeight - iDrawHeight ) / 2,
					x + orgTxX + ( extTxWidth + iDrawWidth ) / 2, y + orgTxY + ( extTxHeight + iDrawHeight ) / 2 );
			}
		}
	}
	else
	{
		g_pMatSystemSurface->DrawSetColor( 255, 0, 255, 100 );
		g_pMatSystemSurface->DrawFilledRect(
			x + orgTxX + ( extTxWidth - iDrawWidth ) / 2, y + orgTxY + ( extTxWidth - iDrawHeight ) / 2,
			x + orgTxX + ( extTxWidth + iDrawWidth ) / 2, y + orgTxY + ( extTxWidth + iDrawHeight ) / 2 );
	}

	y += TILE_SIZE + TILE_BORDER;
}


//////////////////////////////////////////////////////////////////////////
//
// Render textures view panel
//
//////////////////////////////////////////////////////////////////////////

class CRenderTexturesListViewPanel : public vgui::TileViewPanelEx
{
	DECLARE_CLASS_SIMPLE( CRenderTexturesListViewPanel, vgui::TileViewPanelEx );

public:
	CRenderTexturesListViewPanel( vgui::Panel *parent, char const *szName );
	~CRenderTexturesListViewPanel();

protected:
	virtual int GetNumTiles();
	virtual void GetTileSize( int &wide, int &tall );
	virtual void RenderTile( int iTile, int x, int y );

protected:
	virtual void PerformLayout();
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code ) { OnMousePressed( code ); }

public:
	void SetDataListPanel( vgui::ListPanel *pPanel );
	void SetPaintAlpha( bool bPaintAlpha );

	CRenderTextureEditor * GetRenderTxEditor() const { return m_pRenderTxEditor; }

protected:
	vgui::ListPanel *m_pListPanel;
	KeyValues * GetTileData( int iTile );

protected:
	CRenderTextureEditor *m_pRenderTxEditor;

protected:
	enum
	{
		TILE_BORDER = 20,
		TILE_SIZE = 192,
		TILE_TEXTURE_SIZE = 256,
		TILE_TEXT = 35
	};

	bool m_bPaintAlpha;
};

CRenderTexturesListViewPanel::CRenderTexturesListViewPanel( vgui::Panel *parent, char const *szName ) :
	vgui::TileViewPanelEx( parent, szName ),
	m_pListPanel( NULL ),
	m_bPaintAlpha( false )
{
	m_pRenderTxEditor = new CRenderTextureEditor( this, "TxEdt" );
	m_pRenderTxEditor->SetPos( 10, 10 );
	m_pRenderTxEditor->PerformLayout();
	m_pRenderTxEditor->SetMoveable( true );
	m_pRenderTxEditor->SetSizeable( false );
	m_pRenderTxEditor->SetClipToParent( true );
	m_pRenderTxEditor->SetTitle( "", true );
	m_pRenderTxEditor->SetCloseButtonVisible( false );
	m_pRenderTxEditor->SetVisible( false );
}

CRenderTexturesListViewPanel::~CRenderTexturesListViewPanel()
{
	NULL;
}

void CRenderTexturesListViewPanel::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CRenderTexturesListViewPanel::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );

	// Figure out which tile got hit and pass its data for preview
	m_pRenderTxEditor->Close();

	if ( !m_pListPanel )
		return;

	int x, y;
	vgui::input()->GetCursorPos( x, y );
	this->ScreenToLocal( x, y );

	// Hit test the click
	int iTile, tileX, tileY;
	int htResult = HitTest( x, y, iTile );
	if ( HT_NOTHING == htResult )
		return;
	if ( !GetTileOrg( iTile, tileX, tileY ) )
		return;

	// Now having the tile retrieve the keyvalues
	int itemId = m_pListPanel->GetItemIDFromRow( iTile );
	if ( itemId < 0 )
		return;
	KeyValues *kv = m_pListPanel->GetItem( itemId );
	if ( !kv )
		return;

	// Display the tx editor
	m_pRenderTxEditor->SetDispInfo( kv, itemId );
	
	if ( tileX + m_pRenderTxEditor->GetWide() > m_li_wide - 2 )
		tileX -= tileX + m_pRenderTxEditor->GetWide() - ( m_li_wide - 2 );
	if ( tileY + m_pRenderTxEditor->GetTall() > m_li_tall - 2 )
		tileY -= tileY + m_pRenderTxEditor->GetTall() - ( m_li_tall - 2 );

	int iTopLeftX = 0, iTopLeftY = 0;
	for ( vgui::Panel *pPanel = this; ( pPanel = pPanel->GetParent() ) != NULL; )
	{
		iTopLeftX = iTopLeftY = 0;
		pPanel->LocalToScreen( iTopLeftX, iTopLeftY );
	}
	
	LocalToScreen( tileX, tileY );
	if ( tileX < iTopLeftX ) tileX = iTopLeftX;
	if ( tileY < iTopLeftY ) tileY = iTopLeftY;

	m_pRenderTxEditor->SetPos( tileX, tileY );
	m_pRenderTxEditor->Activate();
}

int CRenderTexturesListViewPanel::GetNumTiles()
{
	return m_pListPanel ? m_pListPanel->GetItemCount() : 0;
}

void CRenderTexturesListViewPanel::GetTileSize( int &wide, int &tall )
{
	wide = 2 * TILE_BORDER + TILE_SIZE;
	tall = 2 * TILE_BORDER + TILE_SIZE + TILE_TEXT;
};

KeyValues * CRenderTexturesListViewPanel::GetTileData( int iTile )
{
	int iData = m_pListPanel->GetItemIDFromRow( iTile );
	if ( iData < 0 )
		return NULL;

	return m_pListPanel->GetItem( iData );
}

void CRenderTexturesListViewPanel::RenderTile( int iTile, int x, int y )
{
	CAutoMatSysDebugMode auto_matsysdebugmode;

	KeyValues *kv = GetTileData( iTile );
	if ( !kv )
		return;

	char const *szTextureFile = kv->GetString( KEYNAME_NAME );
	char const *szTextureGroup = kv->GetString( KEYNAME_TEXTURE_GROUP );
	ITexture *pMatTexture = NULL;
	if ( *szTextureFile )
		pMatTexture = materials->FindTexture( szTextureFile, szTextureGroup, false );
	if ( !pMatTexture )
		pMatTexture = materials->FindTexture( "debugempty", "", false );

	// Determine the texture size
	int iTxWidth = kv->GetInt( KEYNAME_WIDTH );
	int iTxHeight = kv->GetInt( KEYNAME_HEIGHT );
	int iTxSize = kv->GetInt( KEYNAME_SIZE );
	char const *szTxFormat = kv->GetString( KEYNAME_FORMAT );

	int iTxFormatLen = strlen( szTxFormat );
	char *szTxFormatSuffix = "";
	if ( iTxFormatLen > 4 )
	{
fmtlenreduce:
		switch ( szTxFormat[ iTxFormatLen - 1 ] )
		{
		case '8':
			{
				while ( ( iTxFormatLen > 4 ) &&
					( szTxFormat[ iTxFormatLen - 2 ] == '8' ) )
					-- iTxFormatLen;
			}
			break;
		case '6':
			{
				while ( ( iTxFormatLen > 4 ) &&
					( szTxFormat[ iTxFormatLen - 2 ] == '1' ) &&
					( szTxFormat[ iTxFormatLen - 3 ] == '6' ) )
					iTxFormatLen -= 2;
			}
			break;
		case 'F':
			if ( !*szTxFormatSuffix )
			{
				iTxFormatLen --;
				szTxFormatSuffix = "F";
				goto fmtlenreduce;
			}
			break;
		}
	}

	// Dimensions to draw
	int iDrawWidth = iTxWidth;
	int iDrawHeight = iTxHeight;

	if ( pMatTexture && pMatTexture->IsCubeMap() )
	{
		iDrawWidth = 1024;
		iDrawHeight = 1024;
	}

	if ( iDrawHeight >= iDrawWidth )
	{
		if ( iDrawHeight > TILE_TEXTURE_SIZE )
		{
			iDrawWidth = iDrawWidth * ( float( TILE_TEXTURE_SIZE ) / iDrawHeight );
			iDrawHeight = TILE_TEXTURE_SIZE;
		}

		if ( iDrawHeight < 64 )
		{
			iDrawWidth = iDrawWidth * ( float( 64 ) / iDrawHeight );
			iDrawHeight = 64;
		}
	}
	else
	{
		if ( iDrawWidth > TILE_TEXTURE_SIZE )
		{
			iDrawHeight = iDrawHeight * ( float( TILE_TEXTURE_SIZE ) / iDrawWidth );
			iDrawWidth = TILE_TEXTURE_SIZE;
		}

		if ( iDrawWidth < 64 )
		{
			iDrawHeight = iDrawHeight * ( float( 64 ) / iDrawWidth );
			iDrawWidth = 64;
		}
	}

	iDrawHeight = iDrawHeight / ( float( TILE_TEXTURE_SIZE ) / float( TILE_SIZE ) );
	iDrawWidth = iDrawWidth / ( float( TILE_TEXTURE_SIZE ) / float( TILE_SIZE ) );

	iDrawHeight = max( iDrawHeight, 4 );
	iDrawWidth = max( iDrawWidth, 4 );

	//
	// Draw frame
	//
	{
		int tileWidth, tileHeight;
		GetTileSize( tileWidth, tileHeight );
		g_pMatSystemSurface->DrawSetColor( 255, 255, 255, 255 );
		g_pMatSystemSurface->DrawOutlinedRect( x + 1, y + 1,
			x + tileWidth - 2 , y + tileHeight - 2 );
	}

	//
	// Draw all
	//

	x += TILE_BORDER;
	y += TILE_BORDER/2;

	int iLenFile = strlen( szTextureFile );
	char const *szPrintFilePrefix = ( iLenFile > 22 ) ? "..." : "";
	char const *szPrintFileName = ( iLenFile > 22 ) ? ( szTextureFile + iLenFile - 22 ) : szTextureFile;

	char chSizeBuf[20] = {0};
	if ( iTxSize >= 0 ) 
	{
		FmtCommaNumber( chSizeBuf,  iTxSize );
	}
	else
	{
		chSizeBuf[0] = '-';
	}

	static Color clrLblNormal( 25, 50, 25, 255 );
	static Color clrLblWarn( 75, 75, 0, 255 );
	static Color clrLblError( 200, 0, 0, 255 );
	bool bWarnTile = ( !kv->GetInt( "SpecialTx" ) ) && ( g_warn_enable && ShallWarnTx( kv, pMatTexture ) );
	g_pMatSystemSurface->DrawSetColor( bWarnTile ? clrLblWarn : clrLblNormal );
	g_pMatSystemSurface->DrawFilledRect( x - TILE_BORDER/2, y, x + TILE_BORDER/2 + TILE_SIZE, y + TILE_TEXT );

	char chInfoText[256] = { 0 };
	sprintf( chInfoText, "%s Kb  %dx%d  %.*s%s  %s",
		chSizeBuf,
		iTxWidth, iTxHeight,
		iTxFormatLen, szTxFormat, szTxFormatSuffix,
		( pMatTexture->GetFlags() & (
			TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_ONEBITALPHA
			) ) ? "***" : "" );
	int iTextMargins[4] = { 0 };
	int iTextHeight = g_pMatSystemSurface->GetFontTall( GetFont() );
	{
		// Compute text extents
		int iTextLen[4] = { 0 };
		iTextLen[0] = 5 + strlen( chSizeBuf );
		iTextLen[1] = strchr( chInfoText, 'x' ) + 1 - chInfoText;
		while ( chInfoText[ iTextLen[1] ] != ' ' )
			++ iTextLen[1];
		++ iTextLen[1];
		iTextLen[2] = 2 + iTextLen[1] + iTxFormatLen + strlen( szTxFormatSuffix );
		iTextLen[3] = strlen( chInfoText );
		for ( int k = 0; k < 4; ++ k )
			iTextMargins[k] = g_pMatSystemSurface->DrawTextLen( GetFont(), "%.*s", iTextLen[k], chInfoText );
	}

	// Highlights
	if ( bWarnTile )
	{
		g_pMatSystemSurface->DrawSetColor( clrLblError );
		if ( iTxSize > g_warn_texkbytes )
			g_pMatSystemSurface->DrawFilledRect( x - 2, y + iTextHeight + 1, x + iTextMargins[0] - 5, y + TILE_TEXT );
		if ( iTxWidth > g_warn_texdimensions || iTxHeight > g_warn_texdimensions )
			g_pMatSystemSurface->DrawFilledRect( x + iTextMargins[0] - 2, y + iTextHeight + 1, x + iTextMargins[1] - 1, y + TILE_TEXT );
		if ( strcmp( szTxFormat, "DXT1" ) && strcmp( szTxFormat, "DXT5" ) )
			g_pMatSystemSurface->DrawFilledRect( x + iTextMargins[1] + 2, y + iTextHeight + 1, x + iTextMargins[2] - 1, y + TILE_TEXT );
		if ( pMatTexture->GetFlags() & (
			TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_ONEBITALPHA
			) )
			g_pMatSystemSurface->DrawFilledRect( x + iTextMargins[2] + 3, y + iTextHeight + 1, x + iTextMargins[3] + 2, y + TILE_TEXT );
	}

	g_pMatSystemSurface->DrawColoredTextRect( GetFont(), x, y, TILE_SIZE, TILE_TEXT,
		255, 255, 255, 255,
		"%s%s\n"
		"%s",
		szPrintFilePrefix, szPrintFileName,
		chInfoText
		);

	y += TILE_TEXT + TILE_BORDER/2;

	// Images placement
	bool bHasAlpha = m_bPaintAlpha && stricmp( szTxFormat, "DXT1" );

	int extTxWidth = TILE_SIZE;
	int extTxHeight = TILE_SIZE;

	int orgTxX = 0, orgTxXA = 0;
	int orgTxY = 0, orgTxYA = 0;

	if ( bHasAlpha )
	{
		if ( iTxWidth >= iTxHeight * 2 )
		{
			extTxHeight /= 2;
			orgTxYA = extTxHeight + TILE_BORDER/2;
		}
		else if ( iTxHeight >= iTxWidth * 2 )
		{
			extTxWidth /= 2;
			orgTxXA = extTxWidth + TILE_BORDER/2;
			x -= TILE_BORDER/4 + 1;
		}
		else
		{
			extTxHeight /= 2;
			orgTxYA = extTxHeight + TILE_BORDER/2;
			orgTxX = extTxWidth / 4;
			extTxWidth /= 2;
			x -= TILE_BORDER/4 + 1;

			if ( iDrawWidth > extTxWidth )
			{
				iDrawWidth /= 2;
				iDrawHeight /= 2;
			}
		}
	}

	enum { IMG_FRAME_OFF = 2 };
	if ( IMaterial *pMaterial = UseDebugMaterial( "debug/debugtexturecolor", pMatTexture, &auto_matsysdebugmode ) )
	{
		g_pMatSystemSurface->DrawSetColor( 255, 255, 255, 255 );
		g_pMatSystemSurface->DrawOutlinedRect( x + orgTxX + ( extTxWidth - iDrawWidth ) / 2 - IMG_FRAME_OFF, y + orgTxY + ( extTxHeight - iDrawHeight ) / 2 - IMG_FRAME_OFF,
			x + orgTxX + ( extTxWidth + iDrawWidth ) / 2 + IMG_FRAME_OFF, y + orgTxY + ( extTxHeight + iDrawHeight ) / 2 + IMG_FRAME_OFF );
		RenderTexturedRect( this, pMaterial,
			x + orgTxX + ( extTxWidth - iDrawWidth ) / 2, y + orgTxY + ( extTxHeight - iDrawHeight ) / 2,
			x + orgTxX + ( extTxWidth + iDrawWidth ) / 2, y + orgTxY + ( extTxHeight + iDrawHeight ) / 2,
			2, 1 );

		if ( bHasAlpha )
		{
			orgTxX += orgTxXA;
			orgTxY += orgTxYA;
			if ( IMaterial *pMaterialDebug = UseDebugMaterial( "debug/debugtexturealpha", pMatTexture, &auto_matsysdebugmode ) )
			{
				g_pMatSystemSurface->DrawOutlinedRect( x + orgTxX + ( extTxWidth - iDrawWidth ) / 2 - IMG_FRAME_OFF, y + orgTxY + ( extTxHeight - iDrawHeight ) / 2 - IMG_FRAME_OFF,
					x + orgTxX + ( extTxWidth + iDrawWidth ) / 2 + IMG_FRAME_OFF, y + orgTxY + ( extTxHeight + iDrawHeight ) / 2 + IMG_FRAME_OFF );
				RenderTexturedRect( this, pMaterialDebug,
					x + orgTxX + ( extTxWidth - iDrawWidth ) / 2, y + orgTxY + ( extTxHeight - iDrawHeight ) / 2,
					x + orgTxX + ( extTxWidth + iDrawWidth ) / 2, y + orgTxY + ( extTxHeight + iDrawHeight ) / 2,
					2, 1 );
			}
		}
	}
	else
	{
		g_pMatSystemSurface->DrawSetColor( 255, 0, 255, 100 );
		g_pMatSystemSurface->DrawFilledRect(
			x + orgTxX + ( extTxWidth - iDrawWidth ) / 2, y + orgTxY + ( extTxWidth - iDrawHeight ) / 2,
			x + orgTxX + ( extTxWidth + iDrawWidth ) / 2, y + orgTxY + ( extTxWidth + iDrawHeight ) / 2 );
	}

 	y += TILE_SIZE + TILE_BORDER;
}

void CRenderTexturesListViewPanel::SetDataListPanel( vgui::ListPanel *pPanel )
{
	m_pListPanel = pPanel;
	InvalidateLayout();
}

void CRenderTexturesListViewPanel::SetPaintAlpha( bool bPaintAlpha )
{
	m_bPaintAlpha = bPaintAlpha;
	Repaint();
}



//-----------------------------------------------------------------------------
// Purpose: Shows entity status report if cl_entityreport cvar is set
//-----------------------------------------------------------------------------
class CTextureListPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CTextureListPanel, vgui::Frame );

public:
	// Construction
					CTextureListPanel( vgui::Panel *parent );
	virtual			~CTextureListPanel( void );

	// Refresh
	virtual void	Paint();
	void			EndPaint(); // Still inside paint
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool	ShouldDraw();
	virtual void	PerformLayout();
	virtual void	Close();

	void			OnTurnedOn();

private:

	void UpdateTotalUsageLabel();
	virtual void OnCommand( const char *command );
	MESSAGE_FUNC( OnTextChanged, "TextChanged" );

	int AddListItem( KeyValues *kv );

	bool UpdateDisplayedItem( KeyValues *pDispData, KeyValues *kv );


private:

	// Font to use for drawing
	vgui::HFont			m_hFont;
	vgui::ListPanel *m_pListPanel;
	CRenderTexturesListViewPanel *m_pViewPanel;

	vgui::CheckButton *m_pSpecialTexs;
	vgui::CheckButton *m_pResolveTexturePath;
	CConVarCheckButton *m_pShowTextureMemoryUsageOption;
	CConVarCheckButton *m_pAllTextures;
	CConVarCheckButton *m_pViewTextures;
	vgui::Button *m_pCopyToClipboardButton;
	vgui::ToggleButton *m_pCollapse;
	vgui::CheckButton *m_pAlpha;
	vgui::CheckButton *m_pThumbWarnings;
	
	vgui::CheckButton *m_pHideMipped;
	vgui::CheckButton *m_pFilteringChk;
	vgui::TextEntry *m_pFilteringText;
	int				m_numDisplayedSizeKB;

	vgui::Button	*m_pReloadAllMaterialsButton;
	vgui::Button	*m_pCommitChangesButton;
	vgui::Button	*m_pDiscardChangesButton;

	vgui::Label *m_pCVarListLabel;
	vgui::Label *m_pTotalUsageLabel;
};


static int __cdecl KilobytesSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	// Reverse sort order so the bigger textures show up first
	const char *string1 = item1.kv->GetString( KEYNAME_SIZE );
	const char *string2 = item2.kv->GetString( KEYNAME_SIZE );
	int a = atoi( string1 );
	int b = atoi( string2 );
	if( a < b )
	{
		return 1;
	}
	if( a > b )
	{
		return -1;
	}
	return 0;
}


//
// Smart texture list
//
class CSmartTextureKeyValues
{
private:
	CSmartTextureKeyValues( CSmartTextureKeyValues const &x );
	CSmartTextureKeyValues& operator = ( CSmartTextureKeyValues const &x );

public:
	CSmartTextureKeyValues() : m_p( NULL ) { if ( KeyValues *p = g_pMaterialSystemDebugTextureInfo->GetDebugTextureList() ) m_p = p->MakeCopy(); }
	~CSmartTextureKeyValues() { if ( m_p ) m_p->deleteThis(); m_p = NULL; }

	KeyValues * Get() const { return m_p; };

protected:
	KeyValues *m_p;
};


//-----------------------------------------------------------------------------
// Purpose: Instances the entity report panel
// Input  : *parent - 
//-----------------------------------------------------------------------------
CTextureListPanel::CTextureListPanel( vgui::Panel *parent ) :
	BaseClass( parent, "CTextureListPanel" ),
	m_numDisplayedSizeKB( 0 )
{
	// Need parent here, before loading up textures, so getSurfaceBase 
	//  will work on this panel ( it's null otherwise )
	SetSize( videomode->GetModeStereoWidth() - 20, videomode->GetModeStereoHeight() - 20 );
	SetPos( 10, 10 );
	SetVisible( true );
	SetCursor( null );

	SetTitle( "Texture list", false );
	SetMenuButtonVisible( false );

	m_hFont = vgui::INVALID_FONT;

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( true );

	// Init the buttons.
	m_pCVarListLabel = vgui::SETUP_PANEL( new vgui::Label( this, "m_pCVarListLabel", 
		"cvars: mat_texture_limit, mat_texture_list, mat_picmip, mat_texture_list_txlod, mat_texture_list_txlod_sync" ) );
	m_pCVarListLabel->SetVisible( false ); // m_pCVarListLabel->SetVisible( true );

	m_pTotalUsageLabel = vgui::SETUP_PANEL( new vgui::Label( this, "m_pTotalUsageLabel", "" ) );
	m_pTotalUsageLabel->SetVisible( true );

	m_pSpecialTexs = vgui::SETUP_PANEL( new vgui::CheckButton( this, "service", "Render Targets and Special Textures" ) );
	m_pSpecialTexs->SetVisible( true );
	m_pSpecialTexs->AddActionSignalTarget( this );
	m_pSpecialTexs->SetCommand( "service" );

	m_pResolveTexturePath = vgui::SETUP_PANEL( new vgui::CheckButton( this, "resolvepath", "Resolve Full Texture Path" ) );
	m_pResolveTexturePath->SetVisible( true );
	m_pResolveTexturePath->AddActionSignalTarget( this );
	m_pResolveTexturePath->SetCommand( "resolvepath" );

	m_pShowTextureMemoryUsageOption = vgui::SETUP_PANEL( new CConVarCheckButton( this, "m_pShowTextureMemoryUsageOption", "Show Memory Usage on HUD" ) );
	m_pShowTextureMemoryUsageOption->SetVisible( true );
	m_pShowTextureMemoryUsageOption->SetConVar( &mat_show_texture_memory_usage );

	m_pAllTextures = vgui::SETUP_PANEL( new CConVarCheckButton( this, "m_pAllTextures", "Show ALL textures" ) );
	m_pAllTextures->SetVisible( true );
	m_pAllTextures->SetConVar( &mat_texture_list_all );
	m_pAllTextures->AddActionSignalTarget( this );
	m_pAllTextures->SetCommand( "AllTextures" );

	m_pViewTextures = vgui::SETUP_PANEL( new CConVarCheckButton( this, "m_pViewTextures", "View textures thumbnails" ) );
	m_pViewTextures->SetVisible( true );
	m_pViewTextures->SetConVar( &mat_texture_list_view );
	m_pViewTextures->AddActionSignalTarget( this );
	m_pViewTextures->SetCommand( "ViewThumbnails" );

	m_pCopyToClipboardButton = vgui::SETUP_PANEL( new vgui::Button( this, "CopyToClipboard", "Copy to Clipboard" ) );
	if ( m_pCopyToClipboardButton )
	{
		m_pCopyToClipboardButton->AddActionSignalTarget( this );
		m_pCopyToClipboardButton->SetCommand( COPYTOCLIPBOARD_CMDNAME );
	}

	m_pCollapse = vgui::SETUP_PANEL( new vgui::ToggleButton( this, "Collapse", " " ) );
	m_pCollapse->AddActionSignalTarget( this );
	m_pCollapse->SetCommand( "Collapse" );
	m_pCollapse->SetSelected( true );

	m_pAlpha = vgui::SETUP_PANEL( new vgui::CheckButton( this, "ShowAlpha", "Alpha" ) );
	m_pAlpha->AddActionSignalTarget( this );
	m_pAlpha->SetCommand( "ShowAlpha" );
	bool bDefaultTxAlphaOn = true;
	m_pAlpha->SetSelected( bDefaultTxAlphaOn );
	
	m_pThumbWarnings = vgui::SETUP_PANEL( new vgui::CheckButton( this, "ThumbWarnings", "Warns" ) );
	m_pThumbWarnings->AddActionSignalTarget( this );
	m_pThumbWarnings->SetCommand( "ThumbWarnings" );
	m_pThumbWarnings->SetSelected( g_warn_enable );

	// Filtering
	m_pHideMipped = vgui::SETUP_PANEL( new vgui::CheckButton( this, "HideMipped", "Hide Mipped" ) );
	m_pHideMipped->AddActionSignalTarget( this );
	m_pHideMipped->SetCommand( "HideMipped" );
	m_pHideMipped->SetSelected( false );

	// Filtering
	m_pFilteringChk = vgui::SETUP_PANEL( new vgui::CheckButton( this, "FilteringChk", "Filter: " ) );
	m_pFilteringChk->AddActionSignalTarget( this );
	m_pFilteringChk->SetCommand( "FilteringChk" );
	m_pFilteringChk->SetSelected( true );

	m_pFilteringText = vgui::SETUP_PANEL( new vgui::TextEntry( this, "FilteringTxt" ) );
	m_pFilteringText->AddActionSignalTarget( this );

	m_pReloadAllMaterialsButton = vgui::SETUP_PANEL( new vgui::Button( this, "ReloadAllMaterials", "Reload All Materials" ) );
	if ( m_pReloadAllMaterialsButton )
	{
		m_pReloadAllMaterialsButton->AddActionSignalTarget( this );
		m_pReloadAllMaterialsButton->SetCommand( "ReloadAllMaterials" );
	}
	m_pCommitChangesButton = vgui::SETUP_PANEL( new vgui::Button( this, "CommitChanges", "Commit Changes" ) );
	if ( m_pCommitChangesButton )
	{
		m_pCommitChangesButton->AddActionSignalTarget( this );
		m_pCommitChangesButton->SetCommand( "CommitChanges" );
	}
	m_pDiscardChangesButton = vgui::SETUP_PANEL( new vgui::Button( this, "DiscardChanges", "Discard Changes" ) );
	if ( m_pDiscardChangesButton )
	{
		m_pDiscardChangesButton->AddActionSignalTarget( this );
		m_pDiscardChangesButton->SetCommand( "DiscardChanges" );
	}

	// Create the tree control itself.
	m_pListPanel = vgui::SETUP_PANEL( new vgui::ListPanel( this, "List Panel" ) );
	m_pListPanel->SetVisible( !mat_texture_list_view.GetBool() );
	
	int col = -1;
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_NAME, "Texture Name", 200, 100, 700, vgui::ListPanel::COLUMN_RESIZEWITHWINDOW );
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_PATH, "Path", 50, 50, 300, 0 );
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_SIZE, "Kilobytes", 50, 50, 50, 0 );
		m_pListPanel->SetSortFunc( col, KilobytesSortFunc );
		m_pListPanel->SetSortColumnEx( col, 0, true );	// advanced sorting setup
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_TEXTURE_GROUP, "Group", 100, 100, 300, 0 );
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_FORMAT, "Format", 250, 50, 300, 0 );
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_WIDTH, "Width", 50, 50, 50, 0 );
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_HEIGHT, "Height", 50, 50, 50, 0 );
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_BINDS_FRAME, "# Binds", 50, 50, 50, 0 );
	m_pListPanel->AddColumnHeader( ++ col, KEYNAME_BINDS_MAX, "BindsMax", 50, 50, 50, 0 );

	SetBgColor( Color( 0, 0, 0, 100 ) );

	m_pListPanel->SetBgColor( Color( 0, 0, 0, 100 ) );

	// Create the view control itself
	m_pViewPanel = vgui::SETUP_PANEL( new CRenderTexturesListViewPanel( this, "View Panel" ) );
	m_pViewPanel->SetVisible( mat_texture_list_view.GetBool() );
	m_pViewPanel->SetBgColor( Color( 0, 0, 0, 255 ) );
	
	m_pViewPanel->SetDragEnabled( false );
	m_pViewPanel->SetDropEnabled( false );
	m_pViewPanel->SetPaintAlpha( bDefaultTxAlphaOn );

	m_pViewPanel->SetDataListPanel( m_pListPanel );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTextureListPanel::~CTextureListPanel( void )
{
	g_pTextureListPanel = NULL;
}

void CTextureListPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// If you change this font, be sure to mark it with
	// $use_in_fillrate_mode in its .vmt file
	m_hFont = pScheme->GetFont( "DefaultVerySmall", false );
	Assert( m_hFont );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTextureListPanel::ShouldDraw( void )
{
	if ( mat_texture_list.GetInt() )
		return true;
	if ( s_eTxListPanelRequest == TXR_SHOW ||
		 s_eTxListPanelRequest == TXR_RUNNING )
		return true;
	
	return false;
}

void CTextureListPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// Put the collapse button in the corner
	m_pCollapse->SetPos( 2, 10 );
	m_pCollapse->SetSize( 10, 10 );
	m_pCollapse->SetVisible( true );

	bool bCollapsed = m_pCollapse->IsSelected();

	int x, y, w, t;
	GetClientArea( x, y, w, t );

	int yOffset = y;

	// The cvar reminder goes on the top.
	m_pCVarListLabel->SetPos( x, yOffset );
	m_pCVarListLabel->SetWide( w );
	NULL; // yOffset += m_pCVarListLabel->GetTall();
	m_pCVarListLabel->SetVisible( false ); // m_pCVarListLabel->SetVisible( !bCollapsed );

	m_pTotalUsageLabel->SetPos( x, yOffset );
	m_pTotalUsageLabel->SetWide( w );
	yOffset += m_pTotalUsageLabel->GetTall();
	m_pTotalUsageLabel->SetVisible( !bCollapsed );

	// Align the check boxes.
	vgui::Panel *buttons[] = {
		m_pSpecialTexs,
		m_pShowTextureMemoryUsageOption,
		m_pAllTextures,
		m_pViewTextures,
		m_pFilteringChk,
		m_pHideMipped,
		m_pResolveTexturePath,
		m_pCopyToClipboardButton };

	for ( int i=0; i < ARRAYSIZE( buttons ); i++ )
	{
		buttons[i]->SetPos( x, yOffset );
		buttons[i]->SetWide( w/2 );
		yOffset += buttons[i]->GetTall();
		buttons[i]->SetVisible( !bCollapsed );

		if ( buttons[i] == m_pViewTextures )
		{
			m_pViewTextures->SetWide( 170 );
			int accumw = 170;
			
			m_pAlpha->SetPos( x + accumw + 5, yOffset - m_pViewTextures->GetTall() );
			m_pAlpha->SetWide( (accumw += 85, 85) );

			m_pThumbWarnings->SetPos( x + accumw + 5, yOffset - m_pViewTextures->GetTall() );
			m_pThumbWarnings->SetWide( (accumw += 85, 85) );
		}

		if ( buttons[i] == m_pFilteringChk )
		{
			m_pFilteringChk->SetWide( 60 );
			int accumw = 60;

			m_pFilteringText->SetPos( x + accumw + 5, yOffset - m_pFilteringChk->GetTall() );
			m_pFilteringText->SetWide( ( accumw += 170, 170 ) );
			m_pFilteringText->SetTall( m_pFilteringChk->GetTall() );
			m_pFilteringText->SetVisible( !bCollapsed );
		}
	}

	if ( bCollapsed )
	{
		int xOffset = 85, iWidth;

		struct LayoutHorz_t
		{
			vgui::Panel *pPanel;
			int iWidth;
		}
		layout[] =
		{
			{ m_pTotalUsageLabel, 290 },
			{ m_pViewTextures, 170 },
			{ m_pAlpha, 60 },
			{ m_pAllTextures, 135 },
			{ m_pHideMipped, 100 },
			{ m_pFilteringChk, 60 },
			{ m_pFilteringText, 130 },
			{ m_pReloadAllMaterialsButton, 130 },
			{ m_pCommitChangesButton, 130 },
			{ m_pDiscardChangesButton, 130 },
		};

		for ( int k = 0; k < ARRAYSIZE( layout ); ++ k )
		{
			layout[ k ].pPanel->SetPos( xOffset, 2 );
			iWidth = layout[ k ].iWidth;
			iWidth = min( w - xOffset - 30, iWidth );
			layout[ k ].pPanel->SetWide( iWidth );
			layout[ k ].pPanel->SetVisible( iWidth > 50 );
			
			if ( iWidth > 50 )
				xOffset += iWidth + 5;
		}

		yOffset = y;
	}

	m_pAlpha->SetVisible( m_pViewTextures->IsSelected() );
	m_pThumbWarnings->SetVisible( !bCollapsed && m_pViewTextures->IsSelected() );
					  
	m_pListPanel->SetBounds( x, yOffset, w, t - (yOffset - y) );
	m_pViewPanel->SetBounds( x, yOffset, w, t - (yOffset - y) );

	m_pListPanel->SetVisible( !mat_texture_list_view.GetBool() );
	m_pViewPanel->SetVisible( mat_texture_list_view.GetBool() );
}


bool StripDirName( char *pFilename )
{
	if ( pFilename[0] == 0 )
		return false;

	char *pLastSlash = pFilename;
	while ( 1 )
	{
		char *pTestSlash = strchr( pLastSlash, '/' );
		if ( !pTestSlash )
		{
			pTestSlash = strchr( pLastSlash, '\\' );
			if ( !pTestSlash )
				break;
		}
		
		pTestSlash++;	// Skip past the slash.
		pLastSlash = pTestSlash;
	}

	if ( pLastSlash == pFilename )
	{
		return false;
	}
	else
	{
		Assert( pLastSlash[-1] == '/' || pLastSlash[-1] == '\\' );
		pLastSlash[-1] = 0;
		return true;
	}
}

static inline void ToLowerInplace( char *chBuffer )
{
	for ( char *pch = chBuffer; *pch; ++ pch )
	{
		if ( V_isupper( *pch ) )
			*pch = tolower( *pch );
	}
}

void KeepSpecialKeys( KeyValues *textureList, bool bServiceKeys )
{
	KeyValues *pNext;

	for ( KeyValues *pCur = textureList->GetFirstSubKey(); pCur; pCur=pNext )
	{
		pNext = pCur->GetNextKey();

		bool bIsServiceKey = false;
		
		char const *szName = pCur->GetString( KEYNAME_NAME );
		if ( StringHasPrefix( szName, "_" ) ||
			 StringHasPrefix( szName, "[" ) ||
			 !stricmp( szName, "backbuffer" ) ||
			 StringHasPrefix( szName, "colorcorrection" ) ||
			 !stricmp( szName, "depthbuffer" ) ||
			 !stricmp( szName, "frontbuffer" ) ||
			 !stricmp( szName, "normalize" ) ||
			 !*szName )
		{
			bIsServiceKey = true;
		}

		if ( bIsServiceKey != bServiceKeys )
		{
			textureList->RemoveSubKey( pCur );
		}
		else if ( bIsServiceKey )
		{
			pCur->SetInt( "SpecialTx", 1 );
		}
	}
}

void KeepKeysMatchingFilter( KeyValues *textureList, char const *szFilter )
{
	if ( !szFilter || !*szFilter )
		return;

	char chFilter[MAX_PATH] = {0}, chName[MAX_PATH] = {0};
	
	Q_strncpy( chFilter, szFilter, sizeof( chFilter ) - 1 );
	ToLowerInplace( chFilter );

	KeyValues *pNext;
	for ( KeyValues *pCur=textureList->GetFirstSubKey(); pCur; pCur=pNext )
	{
		pNext = pCur->GetNextKey();

		char const *szName = pCur->GetString( KEYNAME_NAME );

		Q_strncpy( chName, szName, sizeof( chName ) - 1 );
		ToLowerInplace( chName );
		
		if ( !strstr( chName, chFilter ) )
		{
			textureList->RemoveSubKey( pCur );
		}
	}
}

void KeepKeysMarkedNoMip( KeyValues *textureList )
{
	KeyValues *pNext;
	for ( KeyValues *pCur=textureList->GetFirstSubKey(); pCur; pCur=pNext )
	{
		pNext = pCur->GetNextKey();

		char const *szTextureFile = pCur->GetString( KEYNAME_NAME );
		char const *szTextureGroup = pCur->GetString( KEYNAME_TEXTURE_GROUP );
		if ( *szTextureFile )
		{
			ITexture *pMatTexture = materials->FindTexture( szTextureFile, szTextureGroup, false );
			if ( pMatTexture && !(pMatTexture->GetFlags() & TEXTUREFLAGS_NOMIP) )
			{
				textureList->RemoveSubKey( pCur );
			}
		}
	}
}

void CTextureListPanel::UpdateTotalUsageLabel()
{
	char data[1024], kb1[20], kb2[20], kb3[20];
	FmtCommaNumber( kb1, (g_pMaterialSystemDebugTextureInfo->GetTextureMemoryUsed( IDebugTextureInfo::MEMORY_BOUND_LAST_FRAME ) + 511) / 1024 );
	FmtCommaNumber( kb2, (g_pMaterialSystemDebugTextureInfo->GetTextureMemoryUsed( IDebugTextureInfo::MEMORY_TOTAL_LOADED ) + 511) / 1024 );
	FmtCommaNumber( kb3, m_numDisplayedSizeKB );

	if ( bool bCollapsed = m_pCollapse->IsSelected() )
	{
		char const *szTitle = "";
		Q_snprintf( data, sizeof( data ), "%s[F %s Kb] / [T %s Kb] / [S %s Kb]", szTitle, kb1, kb2, kb3 );
	}
	else
	{
		char const *szTitle = "Texture Memory Usage";
		char kbMip1[ 20 ], kbMip2[ 20 ];
		FmtCommaNumber( kbMip1, (g_pMaterialSystemDebugTextureInfo->GetTextureMemoryUsed( IDebugTextureInfo::MEMORY_ESTIMATE_PICMIP_1 ) + 511) / 1024 );
		FmtCommaNumber( kbMip2, (g_pMaterialSystemDebugTextureInfo->GetTextureMemoryUsed( IDebugTextureInfo::MEMORY_ESTIMATE_PICMIP_2 ) + 511) / 1024 );
		Q_snprintf( data, sizeof( data ), "%s:  frame %s Kb  /  total %s Kb ( picmip1 = %s Kb, picmip2 = %s Kb )  /  shown %s Kb", szTitle, kb1, kb2, kbMip1, kbMip2, kb3 );
	}

	wchar_t unicodeString[1024];
	g_pVGuiLocalize->ConvertANSIToUnicode( data, unicodeString, sizeof( unicodeString ) );

	m_pTotalUsageLabel->SetText( unicodeString );
}

void CTextureListPanel::OnTextChanged( void )
{
	OnCommand( "FilteringTxt" );
}

void CTextureListPanel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "Close" ) )
	{
		vgui::Frame::OnCommand( command );
		return;
	}

	if ( !Q_stricmp( command, "Collapse" ) )
	{
		InvalidateLayout();
		return;
	}

	if ( !Q_stricmp( command, "ShowAlpha" ) )
	{
		m_pViewPanel->SetPaintAlpha( m_pAlpha->IsSelected() );
		return;
	}

	if ( !Q_stricmp( command, "ThumbWarnings" ) )
	{
		g_warn_enable = m_pThumbWarnings->IsSelected();
		return;
	}

	if ( !Q_stricmp( command, "ViewThumbnails" ) )
	{
		InvalidateLayout();
		return;
	}

	if ( !Q_stricmp( command, COPYTOCLIPBOARD_CMDNAME ) )
	{
		CopyListPanelToClipboard( m_pListPanel );
		return;
	}

	if ( !Q_stricmp( command, "ReloadAllMaterials" ) )
	{
		Cbuf_AddText( "mat_reloadallmaterials" );
		Cbuf_Execute();
		return;
	}

	if ( !Q_stricmp( command, "CommitChanges" ) )
	{
		Cbuf_AddText( "mat_texture_list_txlod_sync save" );
		Cbuf_Execute();
		return;
	}

	if ( !Q_stricmp( command, "DiscardChanges" ) )
	{
		Cbuf_AddText( "mat_texture_list_txlod_sync reset" );
		Cbuf_Execute();
		return;
	}

	mat_texture_list_on_f();
	InvalidateLayout();
}


bool CTextureListPanel::UpdateDisplayedItem( KeyValues *pDispData, KeyValues *kv )
{
	// Update the item?
	bool bUpdate = false;

	// do the stuff that changes often separately.
	if ( pDispData->GetInt( KEYNAME_BINDS_FRAME ) != kv->GetInt( KEYNAME_BINDS_FRAME ) )
	{
		pDispData->SetInt( KEYNAME_BINDS_FRAME, kv->GetInt( KEYNAME_BINDS_FRAME ) );
		bUpdate = true;
	}
	if ( pDispData->GetInt( KEYNAME_BINDS_MAX ) != kv->GetInt( KEYNAME_BINDS_MAX ) )
	{
		pDispData->SetInt( KEYNAME_BINDS_MAX, kv->GetInt( KEYNAME_BINDS_MAX ) );
		bUpdate = true;
	}

	// stuff that changes less frequently
	if( pDispData->GetInt( KEYNAME_SIZE ) != kv->GetInt( KEYNAME_SIZE ) ||
		pDispData->GetInt( KEYNAME_WIDTH ) != kv->GetInt( KEYNAME_WIDTH ) ||
		pDispData->GetInt( KEYNAME_HEIGHT ) != kv->GetInt( KEYNAME_HEIGHT ) ||
		Q_stricmp( pDispData->GetString( KEYNAME_FORMAT ), kv->GetString( KEYNAME_FORMAT ) ) != 0 ||
		Q_stricmp( pDispData->GetString( KEYNAME_PATH ), kv->GetString( KEYNAME_PATH ) ) != 0 ||
		Q_stricmp( pDispData->GetString( KEYNAME_TEXTURE_GROUP ), kv->GetString( KEYNAME_TEXTURE_GROUP ) ) != 0 )
	{
		pDispData->SetInt( KEYNAME_SIZE, kv->GetInt( KEYNAME_SIZE ) );
		pDispData->SetInt( KEYNAME_WIDTH, kv->GetInt( KEYNAME_WIDTH ) );
		pDispData->SetInt( KEYNAME_HEIGHT, kv->GetInt( KEYNAME_HEIGHT ) );
		pDispData->SetString( KEYNAME_FORMAT, kv->GetString( KEYNAME_FORMAT ) );
		pDispData->SetString( KEYNAME_PATH, kv->GetString( KEYNAME_PATH ) );
		pDispData->SetString( KEYNAME_TEXTURE_GROUP, kv->GetString( KEYNAME_TEXTURE_GROUP ) );
		bUpdate = true;
	}

	return bUpdate;
}

int CTextureListPanel::AddListItem( KeyValues *kv )
{
	int iItem = m_pListPanel->GetItem( kv->GetString( KEYNAME_NAME ) );
	if ( iItem == -1 )
	{
		// Set this so the GetItem() call above can use the key's name (as opposed to the value of its
		// "Name" subkey) to find this texture again.
		kv->SetName( kv->GetString( KEYNAME_NAME ) ); 

		// Add this one.
		iItem = m_pListPanel->AddItem( kv, 0, false, false );
		m_pViewPanel->InvalidateLayout();
	}
	else
	{
		KeyValues *pValues = m_pListPanel->GetItem( iItem );
		bool bNeedUpdate = UpdateDisplayedItem( pValues, kv );

		if( bNeedUpdate )
		{
			m_pListPanel->ApplyItemChanges( iItem );
			m_pViewPanel->Repaint();
		}
	}

	return iItem;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CTextureListPanel::OnTurnedOn()
{
	if ( g_bRecursiveRequestToShowTextureList )
		return;

	if ( m_pListPanel )
		m_pListPanel->DeleteAllItems();

	if ( CRenderTextureEditor *pRe = m_pViewPanel->GetRenderTxEditor() )
		pRe->Close();

	MakePopup( false, false );
	MoveToFront();
}

void CTextureListPanel::Close()
{
	mat_texture_list_off_f();
}

void CTextureListPanel::EndPaint()
{
	UpdateTotalUsageLabel();
}

void CTextureListPanel::Paint() 
{
	VPROF( "CTextureListPanel::Paint" );

	if ( !m_hFont )
		return;

	struct EndPaint_t
	{
		CTextureListPanel *m_p;
		EndPaint_t( CTextureListPanel *p ) : m_p( p ) {}
		~EndPaint_t()
		{
			m_p->EndPaint();
		}
	} endpaint( this );

	if ( !mat_texture_list.GetBool() )
		return;

	if ( !g_pMaterialSystemDebugTextureInfo->IsDebugTextureListFresh( 1 ) )
		return;

	CSmartTextureKeyValues textureList;
	if ( !textureList.Get() )
		return;

	CRenderTextureEditor *pRte = m_pViewPanel->GetRenderTxEditor();
	if ( ( s_eTxListPanelRequest == TXR_RUNNING ) &&
		 pRte->IsVisible() )
	{
		KeyValues *kv = NULL;
		int iHint = 0;
		pRte->GetDispInfo( kv, iHint );
		if ( kv && iHint )
		{
			KeyValues *plv = ( m_pListPanel->IsValidItemID( iHint ) ? m_pListPanel->GetItem( iHint ) : NULL );
			if ( plv && !strcmp( plv->GetString( KEYNAME_NAME ), kv->GetString( KEYNAME_NAME ) ) )
			{
				KeyValues *pValData = plv->GetFirstValue(), *pValRendered = kv->GetFirstValue();
				for ( ; pValData && pValRendered; pValData = pValData->GetNextValue(), pValRendered = pValRendered->GetNextValue() )
				{
					if ( strcmp( pValData->GetString(), pValRendered->GetString() ) )
						break;
				}
				if ( pValData || pValRendered ) // Difference found
					pRte->SetDispInfo( plv, iHint );
			}
			else
				kv = NULL;
		}
		
		if ( 0 ) // if ( !kv )
		{
			pRte->Close();
			pRte->SetDispInfo( NULL, 0 );
		}
	}

	// If we are fetching all textures, then stop loading material system:
	if ( mat_texture_list_all.GetBool() )
	{
		if ( s_eTxListPanelRequest == TXR_RUNNING )
		{
			mat_texture_list.SetValue( 0 );
			s_eTxListPanelRequest = TXR_SHOW; // Keep displaying our panel
		}
		else
		{
			s_eTxListPanelRequest = TXR_RUNNING;
		}
	}
	else
	{
		if ( s_eTxListPanelRequest == TXR_SHOW )
		{
			// Either first show or turned off "all textures"
			m_pListPanel->RemoveAll();
			m_pViewPanel->InvalidateLayout();
			s_eTxListPanelRequest = TXR_RUNNING;
			return;
		}
	}

	CBitVec<4096 * 8> itemsTouched;

	// Remove the textures we don't care for
	KeepSpecialKeys( textureList.Get(), m_pSpecialTexs->IsSelected() );

	// If filtering is enabled, then do filtering
	if ( m_pFilteringChk->IsSelected() && m_pFilteringText->GetTextLength() )
	{
		char chFilterString[ MAX_PATH ];
		m_pFilteringText->GetText( chFilterString, sizeof( chFilterString ) - 1 );
		chFilterString[ sizeof( chFilterString ) - 1 ] = 0;
		KeepKeysMatchingFilter( textureList.Get(), chFilterString );
	}

	// If we're to hide mipped, remove anything that isn't marked nomip
	if ( m_pHideMipped->IsSelected() )
	{
		KeepKeysMarkedNoMip( textureList.Get() );
	}

	// Compute the total size of the displayed textures
	int cbTotalDisplayedSizeInBytes = 0;
	
	for ( KeyValues *pCur = textureList.Get()->GetFirstSubKey(); pCur; pCur=pCur->GetNextKey() )
	{
		int sizeInBytes = pCur->GetInt( KEYNAME_SIZE );

		// Accumulate
		cbTotalDisplayedSizeInBytes += sizeInBytes;

		// Factor in frames
		int numCount = pCur->GetInt( "Count" );
		if ( numCount > 1 )
			sizeInBytes *= numCount;

		// Size is in kilobytes.
		int sizeInKilo = ( sizeInBytes + 511 ) / 1024;
		pCur->SetInt( KEYNAME_SIZE, sizeInKilo );

		if ( m_pResolveTexturePath->IsSelected() )
		{
			char chResolveName[ 256 ] = {0}, chResolveNameArg[ 256 ] = {0};
			Q_snprintf( chResolveNameArg, sizeof( chResolveNameArg ) - 1, "materials/%s.vtf", pCur->GetString( KEYNAME_NAME ) );
			char const *szResolvedName = g_pFileSystem->RelativePathToFullPath( chResolveNameArg, "game", chResolveName, sizeof( chResolveName ) - 1 );
			if ( szResolvedName )
			{
				pCur->SetString( KEYNAME_PATH, szResolvedName );
			}
		}

		int iItem = AddListItem( pCur );

		if ( iItem < itemsTouched.GetNumBits() )
			itemsTouched.Set( iItem );
	}

	// Set the displayed size
	m_numDisplayedSizeKB = ( cbTotalDisplayedSizeInBytes + 511 ) / 1024;

	// Now remove from view items that weren't used.
	int iNext, numRemoved = 0;
	for ( int iCur=m_pListPanel->FirstItem(); iCur != m_pListPanel->InvalidItemID(); iCur=iNext )
	{
		iNext = m_pListPanel->NextItem( iCur );

		if ( iCur >= itemsTouched.GetNumBits() || !itemsTouched.Get( iCur ) )
		{
			m_pListPanel->RemoveItem( iCur );
			++ numRemoved;
		}
	}

	// Sorting in list panel
	{
		int iPri, iSec;
		bool bAsc;
		m_pListPanel->GetSortColumnEx( iPri, iSec, bAsc );
		iSec = 0; // always secondary sort by name
		m_pListPanel->SetSortColumnEx( iPri, iSec, bAsc );
		m_pListPanel->SortList();
	}

	if ( numRemoved )
	{
		m_pViewPanel->InvalidateLayout();
	}
}



// ------------------------------------------------------------------------------ //
// Global functions.
// ------------------------------------------------------------------------------ //

void VGui_UpdateTextureListPanel()
{
	if ( mat_show_texture_memory_usage.GetInt() )
	{
		con_nprint_t info;
		info.index = 4;
		info.time_to_live = 0.2;
		info.color[0] = 1;
		info.color[1] = 0.5;
		info.color[2] = 0;
		info.fixed_width_font = true;

		char kb1[20], kb2[20];
		FmtCommaNumber( kb1, (g_pMaterialSystemDebugTextureInfo->GetTextureMemoryUsed( IDebugTextureInfo::MEMORY_BOUND_LAST_FRAME ) + 511) / 1024 );
		FmtCommaNumber( kb2, (g_pMaterialSystemDebugTextureInfo->GetTextureMemoryUsed( IDebugTextureInfo::MEMORY_TOTAL_LOADED ) + 511) / 1024 );

		Con_NXPrintf( &info, "Texture Memory Usage: %s Kb / %s Kb", kb1, kb2 );
	}

	MatViewOverride::DisplaySelectedTextures();

	if ( IsX360() )
		return;

	g_pMaterialSystemDebugTextureInfo->EnableGetAllTextures( mat_texture_list_all.GetBool() );

	g_pMaterialSystemDebugTextureInfo->EnableDebugTextureList( ( mat_texture_list.GetInt() <= 0 ) ? false : true );

	bool bShouldDrawTxListPanel = g_pTextureListPanel->ShouldDraw();
	if ( g_pTextureListPanel->IsVisible() != bShouldDrawTxListPanel )
	{
		g_pTextureListPanel->SetVisible( bShouldDrawTxListPanel );
		bShouldDrawTxListPanel ? mat_texture_list_on_f() : mat_texture_list_off_f();
	}
}


void CL_CreateTextureListPanel( vgui::Panel *parent )
{
	g_pTextureListPanel = new CTextureListPanel( parent );
}

CON_COMMAND( mat_texture_save_fonts, "Save all font textures" )
{
	for( int i = 0; i < 8192; i++ )
	{
		char szTextureName[ MAX_PATH ];

		Q_snprintf( szTextureName, ARRAYSIZE( szTextureName ), "__font_page_%d.tga", i );

		if( !materials->IsTextureLoaded( szTextureName ) )
			break;

		ITexture *pMatTexture = materials->FindTexture( szTextureName, "", false );
		if( pMatTexture && !pMatTexture->IsError() )
		{
			bool bRet = SaveTextureImage( szTextureName );
			Msg( "SaveTextureImage( '%s' ): %s\n", szTextureName, bRet ? "succeeded" : "failed" );
		}
	}
}

void mat_texture_list_on_f()
{
	ConVarRef sv_cheats( "sv_cheats" );
	if ( sv_cheats.IsValid() && !sv_cheats.GetBool() )
		return;

	ConVarRef mat_queue_mode( "mat_queue_mode" );

	if( mat_queue_mode.IsValid() && ( g_nSaveQueueState == INT_MIN ) )
	{
		g_nSaveQueueState = mat_queue_mode.GetInt();
		mat_queue_mode.SetValue( 0 );
	}

	mat_texture_list.SetValue( 1 );
	s_eTxListPanelRequest = TXR_SHOW;

	g_pTextureListPanel->OnTurnedOn();

	MatViewOverride::RequestSelectNone();

	// On Linux, the mouse gets recentered when it's hidden. So if you bring up the texture list
	//	dialog while the game is running, we need to make sure the mouse is shown. Otherwise it's
	//	very tough to use when your mouse keeps getting recentered.
	if( !g_cursorset && g_pVGuiSurface )
	{
		g_pVGuiSurface->SetCursorAlwaysVisible( true );
		g_cursorset = true;
	}
}
void mat_texture_list_off_f()
{
	mat_texture_list.SetValue( 0 );
	s_eTxListPanelRequest = TXR_HIDE;

	if( g_cursorset )
	{
		g_pVGuiSurface->SetCursorAlwaysVisible( false );
		g_cursorset = false;
	}

	if( g_nSaveQueueState != INT_MIN )
	{
		ConVarRef mat_queue_mode( "mat_queue_mode" );

		mat_queue_mode.SetValue( g_nSaveQueueState );
		g_nSaveQueueState = INT_MIN;
	}
}


