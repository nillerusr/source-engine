
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"
//#include "kbutton.h"
#include "input.h"
#include "iclientmode.h"

#define PAIN_NAME "sprites/%d_pain.vmt"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "filesystem.h"
#include <keyvalues.h>
#include "asw_hud_minimap.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "c_asw_game_resource.h"
#include "c_asw_scanner_info.h"
#include "vgui/ISurface.h"
#include <vgui/IInput.h>
#include "fx.h"
#include "tier0/vprof.h"
#include "asw_gamerules.h"
#include "ScanLinePanel.h"
#include "c_asw_scanner_noise.h"
#include "SoftLine.h"
#include "c_asw_objective.h"

#include "ConVar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_draw_hud;
extern ConVar asw_hud_alpha;
extern ConVar asw_hud_scale;
ConVar asw_map_range("asw_map_range", "1200", FCVAR_CHEAT, "Range in world units of the minimap");
ConVar asw_scanner_ring_scale("asw_scanner_ring_scale", "1.0f", FCVAR_CHEAT, "Overdraw in the scanner ring size from the blip boundary");
ConVar asw_scanner_pitch_change("asw_scanner_pitch_change", "0.2f", FCVAR_NONE, "Change in pitch (from 0 to 1.0) of scanner blips depending on distance from the tech marine");
ConVar asw_scanner_pitch_base("asw_scanner_pitch_base", "1.2f", FCVAR_NONE, "Starting pitch");
ConVar asw_scanner_warning_volume("asw_scanner_warning_volume", "0.3f", FCVAR_NONE, "Volume of scanner warning beeps");
ConVar asw_scanner_idle_volume("asw_scanner_idle_volume", "0.4f", FCVAR_NONE, "Volume of scanner idle loop");
ConVar asw_scanner_scanline_alpha("asw_scanner_scanline_alpha", "80", 0, "Alpha of scanlines on the scanner");
ConVar asw_scanner_scanline_double("asw_scanner_scanline_double", "0", 0, "Whether scanlines should be single or double pixel");
ConVar asw_scanner_interlace_alpha("asw_scanner_interlace_alpha", "0", 0, "Alpha of interlace effect on the scanner");
ConVar asw_scanner_noise_alpha("asw_scanner_noise_alpha", "0", 0, "Alpha of noise effect on the scanner");
ConVar asw_scanner_idle_sound("asw_scanner_idle_sound", "3", 0, "Which scanner idle sound is used (from 1 to 4)");
ConVar asw_scanner_warning_sound("asw_scanner_warning_sound", "1", 0, "Which scanner warning sound is used (from 1 to 3)");
ConVar asw_debug_scanner_sound("asw_debug_scanner_sound", "0", FCVAR_CHEAT, "Prints debug output on scanner pulses");
ConVar asw_minimap_clicks("asw_minimap_clicks", "1", FCVAR_ARCHIVE, "Is enabled, clicking on the minimap will draw on it.  If disabled, clicking there will fire your weapon as normal");
ConVar asw_scanner_background("asw_scanner_background", "1", FCVAR_NONE, "Draw black background behind minimap" );
ConVar asw_scanner_classic("asw_scanner_classic", "0", FCVAR_NONE, "Scanner has white blips, is always pinging." );

// was 0.75f..
#define ASW_SCREENSHOT_SCALE 1.0f

MapLine::MapLine()
{
	worldpos.Init(0,0);
	player_index = 0;
	linkpos.Init(0,0);
	created_time = 0;
	bSetLinkBlipCentre = false;
	bSetBlipCentre = false;
	blipcentre.Init(0,0);
	linkblipcentre.Init(0,0);
	bLink = false;	
}

class CASWHudMinimap;

class CASWHudMinimapLinePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASWHudMinimapLinePanel, vgui::Panel );
public:
	CASWHudMinimapLinePanel(Panel *parent, const char *panelName, CASWHudMinimap* pMap);
	void TextureToLinePanel(CASWHudMinimap* pMap, const Vector2D &blip_centre, float &x, float &y);
	virtual void Paint();

	void PaintFollowLine(C_BaseEntity *pMarine, C_BaseEntity *pTarget);
	void PaintFollowLines();
	virtual void PaintScannerRing();

	CPanelAnimationVarAliasType( int, m_nScannerRingTexture, "ScannerRingTexture", "vgui/swarm/HUD/ScannerRing", "textureid" );

	CASWHudMinimap* m_pMap;
};

class CASWHudMinimapFramePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASWHudMinimapFramePanel, vgui::Panel );
public:
	CASWHudMinimapFramePanel(Panel *parent, const char *panelName);
	virtual void Paint();
};


DECLARE_HUDELEMENT( CASWHudMinimap );

CASWHudMinimapLinePanel::CASWHudMinimapLinePanel(Panel *parent, const char *panelName, CASWHudMinimap* pMap) :
	vgui::Panel(parent, panelName),
		m_pMap(pMap)
{

}

CASWHudMinimapFramePanel::CASWHudMinimapFramePanel(Panel *parent, const char *panelName) :
	vgui::Panel(parent, panelName)
{
}

void MsgFunc_ASWMapLine(bf_read &msg) 
{
	int linetype = msg.ReadByte();
	int player_index = msg.ReadByte();	
	int world_x	= msg.ReadLong();
	int world_y	= msg.ReadLong();

	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if ( local )
	{
		// figure out the position of this map line dot
		Vector vecMapLinePos = vec3_origin;
		vecMapLinePos.x = world_x;
		vecMapLinePos.y = world_y;

		CASWHudMinimap *pMiniMap = GET_HUDELEMENT(CASWHudMinimap);
		if (!pMiniMap)
			return;

		// make the HUD store it
		MapLine line;
		line.player_index = player_index;
		line.worldpos.x = world_x;
		line.worldpos.y = world_y;
		line.created_time = gpGlobals->curtime;
		if (linetype == 1)		// links to a previous
		{
			line.bLink = true;
		}
		else
		{
			if (gpGlobals->curtime > pMiniMap->m_fLastMinimapDrawSound + 5.0f)
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1, "ASWScanner.Drawing" );
				pMiniMap->m_fLastMinimapDrawSound = gpGlobals->curtime;
			}
		}
		pMiniMap->m_MapLines.AddToTail(line);
	}
}

//////////
// CASWMap
//////////

// converts a world coord into map texture coords (0->1023)
Vector2D CASWMap::WorldToMapTexture( const Vector &worldpos )
{
	if ( !m_pMinimap )
	{
		m_pMinimap = GET_HUDELEMENT( CASWHudMinimap );

		if ( !m_pMinimap )
		{
			return vec2_origin;
		}
	}

	Vector2D offset( worldpos.x - m_pMinimap->m_MapOrigin.x, worldpos.y - m_pMinimap->m_MapOrigin.y);

	offset.x /= m_pMinimap->m_fMapScale;
	offset.y /= -m_pMinimap->m_fMapScale;

	offset.x -= 128;	// I'd be lying if I knew why this needs to be here

	return offset;
}

void CASWMap::AddBlip( const MapBlip_t &blip )
{
	m_MapBlips.AddToTail( blip );
}

void CASWMap::ClearBlips( void )
{
	m_MapBlips.RemoveAll();
}

void CASWMap::PaintMarineBlips()
{
	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if ( pGameResource )
	{
		// paint the blips			
		for ( int i= 0; i < ASW_MAX_MARINE_RESOURCES; i++ )
		{
			C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			if ( pMR )
			{
				C_ASW_Marine *pMarine = pMR->GetMarineEntity();
				if ( pMarine && pMarine->GetHealth() > 0 )
				{
					PaintWorldBlip( pMarine->GetAbsOrigin(), pMarine->GetBlipStrength(), Color(0,192,0,255) );
					PaintWorldFacingArc(pMarine->GetAbsOrigin(), pMarine->ASWEyeAngles().y, Color(0,192,0,255 - 127.0f * pMarine->GetBlipStrength()));		
				}
			}
		}
	}
}

void CASWMap::PaintExtraBlips()
{			
	for ( int i= 0; i < m_MapBlips.Count(); i++ )
	{
		PaintWorldBlip( m_MapBlips[ i ].vWorldPos, sinf( gpGlobals->curtime * 8.0f ) * 0.25 + 0.25f, m_MapBlips[ i ].rgbaColor, m_MapBlips[ i ].nTextureType );
	}
}

void CASWMap::PaintWorldBlip(const Vector &worldpos, float fBlipStrength, Color BlipColor, MapBlipTexture_t nBlipTexture /*= MAP_BLIP_TEXTURE_NORMAL*/ )
{
	int nStrengthIndex = clamp<int>( fBlipStrength * 7, 0, 7 );

	if ( nStrengthIndex >= 7 || !m_nBlipTexture[ nStrengthIndex ] )
		return;

	Vector2D blip_centre = WorldToMapTexture(worldpos);	// convert from world coords to the pixel on the 0->1023 map texture
	//blip_centre = blip_centre - m_MapCentre + m_MapCentreInPanel;
	blip_centre = MapTextureToPanel(blip_centre);	// convert from the map texture to its position on the panel

	Vector2D vMapCornerInPanel = GetMapCornerInPanel();

	// don't draw out of bounds	
	if ( blip_centre.x < vMapCornerInPanel.x || blip_centre.x > vMapCornerInPanel.x + GetMapSize() || 
		 blip_centre.y < vMapCornerInPanel.y || blip_centre.y > vMapCornerInPanel.y + GetMapSize() )
		return;

	surface()->DrawSetColor( BlipColor );

	int nTextureID;
	int iBlipSize = GetBlipSize();

	switch ( nBlipTexture )
	{
	case MAP_BLIP_TEXTURE_USABLE:
		nTextureID = m_nTriBlipTexture[ nStrengthIndex ];
		break;

	case MAP_BLIP_TEXTURE_DEATH:
		nTextureID = m_nBlipTextureDeath;
		iBlipSize *= 1.5;
		break;

	case MAP_BLIP_TEXTURE_NORMAL:
	default:
		nTextureID = m_nBlipTexture[ nStrengthIndex ];
		break;
	}

	surface()->DrawSetTexture( nTextureID );

	float Dest1X = blip_centre.x - (iBlipSize * 0.5f);
	float Dest1Y = blip_centre.y - (iBlipSize * 0.5f);
	float Dest2X = Dest1X + iBlipSize;
	float Dest2Y = Dest1Y + iBlipSize;

	Vertex_t points[4] = 
	{ 
		Vertex_t( Vector2D(Dest1X, Dest1Y), Vector2D(0,0) ), 
		Vertex_t( Vector2D(Dest2X, Dest1Y), Vector2D(1,0) ), 
		Vertex_t( Vector2D(Dest2X, Dest2Y), Vector2D(1,1) ), 
		Vertex_t( Vector2D(Dest1X, Dest2Y), Vector2D(0,1) ) 
	}; 
	surface()->DrawTexturedPolygon( 4, points );
}

void CASWMap::PaintWorldFacingArc(const Vector &worldpos, float fFacingYaw, Color FacingColor)
{	
	if ( GetFacingArcTexture() == -1)
		return;

	Vector2D blip_centre = WorldToMapTexture(worldpos);	// convert from world coords to the pixel on the 0->1023 map texture
	blip_centre = MapTextureToPanel(blip_centre);	// convert from the map texture to its position on the panel

	Vector2D vMapCornerInPanel = GetMapCornerInPanel();

	// don't draw out of bounds
	if ( blip_centre.x < vMapCornerInPanel.x || blip_centre.y < vMapCornerInPanel.y || 
		 blip_centre.x > vMapCornerInPanel.x + GetMapSize() || blip_centre.y > vMapCornerInPanel.y + GetMapSize() )
		return;

	int iFacingSize = GetArcSize();

	// set up a square to the right
	int xoffset = -2;	// temp? to make the arc look nice next to blips..
	int yoffset = 1;
	Vector vecCornerTL(xoffset, iFacingSize * -0.5f + yoffset, 0);
	Vector vecCornerTR(iFacingSize + xoffset, iFacingSize * -0.5f+ yoffset, 0);
	Vector vecCornerBR(iFacingSize + xoffset, iFacingSize * 0.5f+ yoffset, 0);
	Vector vecCornerBL(xoffset, iFacingSize * 0.5f+ yoffset, 0);
	Vector vecCornerTL_rotated, vecCornerTR_rotated, vecCornerBL_rotated, vecCornerBR_rotated;

	// rotate it by our facing yaw
	QAngle angFacing(0, -fFacingYaw, 0);
	VectorRotate(vecCornerTL, angFacing, vecCornerTL_rotated);
	VectorRotate(vecCornerTR, angFacing, vecCornerTR_rotated);
	VectorRotate(vecCornerBR, angFacing, vecCornerBR_rotated);
	VectorRotate(vecCornerBL, angFacing, vecCornerBL_rotated);

	surface()->DrawSetColor(FacingColor);
	surface()->DrawSetTexture( GetFacingArcTexture() );
	//surface()->DrawTexturedRect(Dest1X,Dest1Y,Dest2X,Dest2Y);
	Vertex_t points[4] = 
	{ 
		Vertex_t( Vector2D(blip_centre.x + vecCornerTL_rotated.x, blip_centre.y + vecCornerTL_rotated.y), Vector2D(0,0) ), 
		Vertex_t( Vector2D(blip_centre.x + vecCornerTR_rotated.x, blip_centre.y + vecCornerTR_rotated.y), Vector2D(1,0) ), 
		Vertex_t( Vector2D(blip_centre.x + vecCornerBR_rotated.x, blip_centre.y + vecCornerBR_rotated.y), Vector2D(1,1) ), 
		Vertex_t( Vector2D(blip_centre.x + vecCornerBL_rotated.x, blip_centre.y + vecCornerBL_rotated.y), Vector2D(0,1) ) 
	}; 
	surface()->DrawTexturedPolygon( 4, points );
}

void CASWMap::LoadBlipTextures()
{
	char buffer[64];
	for (int i=0;i<7;i++)
	{
		m_nBlipTexture[i] = vgui::surface()->CreateNewTextureID();
		Q_snprintf(buffer, sizeof(buffer), "%s%d", "vgui/swarm/HUD/blip", i+1);
		vgui::surface()->DrawSetTextureFile( m_nBlipTexture[i], buffer, true, false);

		m_nTriBlipTexture[i] = vgui::surface()->CreateNewTextureID();
		Q_snprintf(buffer, sizeof(buffer), "%s%d", "vgui/swarm/HUD/triblip", i+1);
		vgui::surface()->DrawSetTextureFile( m_nTriBlipTexture[i], buffer, true, false);
	}

	m_nBlipTextureDeath = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_nBlipTextureDeath, "vgui/swarm/Briefing/deadmarine", true, false);

	m_nBlipTextureFriendlyDamage = m_nBlipTextureKill = m_nBlipTextureHeal = m_nBlipTextureFoundAmmo = m_nBlipTextureNoAmmo = m_nBlipTexture[ 0 ];
}


CASWHudMinimap::CASWHudMinimap( const char *pElementName ) : CASW_HudElement( pElementName ), CHudNumericDisplay(NULL, "ASWHudMinimap"), CASW_VGUI_Ingame_Panel()
{
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_REMOTE_TURRET);
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);

	m_pLinePanel = new CASWHudMinimapLinePanel(this, "CASWHudMinimapLinePanel", this);
	//m_pFramePanel = new CASWHudMinimapFramePanel(this, "CASWHudMinimapFramePanel");
	
	m_nWhiteTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_nWhiteTexture, "vgui/white" , true, false);

	m_pScanLinePanel = new ScanLinePanel(this, "ScanlinePanel", false);
	m_pInterlacePanel = new vgui::ImagePanel(this, "InterlacePanel");
	m_pNoisePanel = new vgui::ImagePanel(this, "NoisePanel");

	if ( asw_scanner_background.GetBool() )
	{
		m_pBackdrop = new CASWHudMinimap_Border(GetParent(), "MapBackdrop", this);
	}
	else
	{
		m_pBackdrop = NULL;
	}

	if (m_pBackdrop)
	{
		m_pBackdrop->SetPaintBackgroundEnabled(true);
		m_pBackdrop->SetPaintBackgroundType(0);
		m_pBackdrop->SetBgColor(Color(0,0,0,asw_hud_alpha.GetInt()));
		MoveToFront();
	}
	m_bHasOverview = false;
	m_szServerName[0] = '\0';

	Q_snprintf(m_szMissionTitle, sizeof(m_szMissionTitle), "");
}

CASWHudMinimap::~CASWHudMinimap()
{
	if ( m_MapKeyValues )
		m_MapKeyValues->deleteThis();
	if (m_pLinePanel)
		m_pLinePanel->MarkForDeletion();
}

void CASWHudMinimap::PerformLayout()
{
	BaseClass::PerformLayout();
	
	int wide, tall, x, y;
	GetBounds(x, y, wide, tall);
	int ox, oy;
	GetScaledOffset(ox, oy);
	wide *= asw_hud_scale.GetFloat();
	tall *= asw_hud_scale.GetFloat();
	m_iMapSize = wide * 0.7f;
	m_MapCornerInPanel.x = int(wide - m_iMapSize) + ox;
	m_MapCornerInPanel.y = int(tall - m_iMapSize) + oy;
	int black_border = m_iMapSize * 0.0455f;		
	if (m_pBackdrop)
	{
		m_pBackdrop->SetBounds(x + m_MapCornerInPanel.x - black_border, y + m_MapCornerInPanel.y - black_border,
			m_iMapSize + black_border * 2 + 1, m_iMapSize + black_border * 2 + 1);
		m_pBackdrop->SetBgColor(Color(0,0,0,asw_hud_alpha.GetInt()));
	}

	m_pScanLinePanel->SetBounds(m_MapCornerInPanel.x, m_MapCornerInPanel.y, m_iMapSize, m_iMapSize);
	m_pInterlacePanel->SetBounds(m_MapCornerInPanel.x, m_MapCornerInPanel.y, m_iMapSize, m_iMapSize);
	m_pNoisePanel->SetBounds(m_MapCornerInPanel.x, m_MapCornerInPanel.y, m_iMapSize, m_iMapSize);
}

void CASWHudMinimap::Init()
{
	gameeventmanager->AddListener(this, "game_newmap", false );	
	gameeventmanager->AddListener(this, "server_spawn", false);

	m_pMinimap = this;

	m_nMapTextureID = -1;
	m_bHasOverview = false;
	m_MapKeyValues = NULL;
	m_bDrawingMapLines = false;

	m_MapOrigin = Vector( 0, 0, 0 );
	m_fMapScale = 1.0f;
	m_fLastMapLine = 0;
	m_fLastBlipSpeechTime = -100.0f;
	m_fLastBlipHitTime = 0.0f;
	m_MapCentre = Vector2D( 0, 0 );
	m_MapCentreInPanel = Vector2D( 0, 0 );
	Reset();

	usermessages->HookMessage( "ASWMapLine", MsgFunc_ASWMapLine );
}

void CASWHudMinimap::Reset()
{
	SetLabelText(L" ");
	m_bDrawingMapLines = false;
	m_fLastMapLine = 0;
	m_fLastMinimapDrawSound = 0;

	SetShouldDisplayValue(false);
}

void CASWHudMinimap::VidInit()
{
	Reset();
}

void CASWHudMinimap::OnThink()
{
	VPROF_BUDGET( "CASWHudMinimap::OnThink", VPROF_BUDGETGROUP_ASW_CLIENT );

	if (m_pScanLinePanel)
	{
		m_pScanLinePanel->SetAlpha(asw_scanner_scanline_alpha.GetInt());
		m_pScanLinePanel->m_bDouble = asw_scanner_scanline_double.GetBool();			
	}
	if (m_pInterlacePanel)
	{
		m_pInterlacePanel->SetAlpha(asw_scanner_interlace_alpha.GetInt());
	}	

	// remove any map lines that have faded out
	for (int i=0;i<m_MapLines.Count();i++)
	{
		if (gpGlobals->curtime - m_MapLines[i].created_time > MAP_LINE_SOLID_TIME + MAP_LINE_FADE_TIME)
			m_MapLines.Remove(i);
	}

	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if ( local )
	{
		if (m_bDrawingMapLines && gpGlobals->curtime >= m_fLastMapLine + MAP_LINE_INTERVAL)
		{
			if (IsWithinMapBounds(m_iMouseX,m_iMouseY))
			{
				int ox, oy;
				GetScaledOffset(ox, oy);
				SendMapLine(m_iMouseX + ox,m_iMouseY + oy,false);				
			}
			else
			{
				m_bDrawingMapLines = false;
			}
		}
		C_ASW_Marine *marine = local->GetMarine();
		if (marine)
		{			
			if (m_pNoisePanel)
			{
				float fScannerNoise = g_pScannerNoise ? g_pScannerNoise->GetScannerNoiseAt(marine->GetAbsOrigin()) : 0;
				m_pNoisePanel->SetAlpha(float(asw_scanner_noise_alpha.GetInt()) * fScannerNoise);
			}
		}
		else
		{
			m_pNoisePanel->SetAlpha(0);
		}		
	}
}

void CASWHudMinimap::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(255,255,255,0));

	m_pInterlacePanel->SetImage("swarm/Computer/ComputerCameraBlack");
	m_pInterlacePanel->SetShouldScaleImage(true);
	
	m_pNoisePanel->SetImage("swarm/Computer/TVNoise");
	m_pNoisePanel->SetShouldScaleImage(true);
}

void CASWHudMinimapFramePanel::Paint()
{	
	VPROF_BUDGET( "CASWHudMinimapFramePanel::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	if (ASWGameRules())
	{
		if (ASWGameRules()->IsIntroMap() || ASWGameRules()->IsOutroMap())
			return;
	}
	CASWHudMinimap *m_pMap = dynamic_cast<CASWHudMinimap*>(GetParent());
	if ( !m_pMap || !asw_draw_hud.GetBool() )
		return;

	int x, y, wide, tall;
	m_pMap->GetBounds(x,y,wide,tall);
	wide *= asw_hud_scale.GetFloat();
	tall *= asw_hud_scale.GetFloat();
	int ox, oy;
	m_pMap->GetScaledOffset(ox, oy);
	SetPos(ox,oy);
	SetSize(wide, tall);
	
	//if ( m_pMap->m_nFrameTexture == -1 )
		//return;

	//surface()->DrawSetColor(m_pMap->GetBgColor());
	//surface()->DrawSetTexture(m_pMap->m_nFrameTexture);
	//surface()->DrawTexturedRect(0, 0, wide, tall);
}

void CASWHudMinimap::GetScaledOffset(int &ox, int &oy)
{
	int wide, tall;
	GetSize(wide,tall);
	int scaled_wide = wide * asw_hud_scale.GetFloat();
	int scaled_tall = tall * asw_hud_scale.GetFloat();

	ox = wide - scaled_wide;
	oy = tall - scaled_tall;
}

void CASWHudMinimap::PaintMapSection()
{
	int wide, tall;
	GetSize(wide,tall);
	wide *= asw_hud_scale.GetFloat();
	tall *= asw_hud_scale.GetFloat();

	int ox, oy;
	GetScaledOffset(ox, oy);
	m_iMapSize = wide * 0.7f;
	m_MapCornerInPanel.x = int(wide - m_iMapSize) + ox;
	m_MapCornerInPanel.y = int(tall - m_iMapSize) + oy;
	m_MapCentreInPanel.x = m_MapCornerInPanel.x + m_iMapSize * 0.5f;
	m_MapCentreInPanel.y = m_MapCornerInPanel.y + m_iMapSize * 0.5f;

	m_pLinePanel->SetBounds(m_MapCornerInPanel.x, m_MapCornerInPanel.y,
							m_MapCornerInPanel.x + m_iMapSize, m_MapCornerInPanel.y + m_iMapSize);
		
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if ( local )
	{
		C_ASW_Marine *marine = local->GetMarine();
		if (marine)
		{
			m_MapCentre = WorldToMapTexture(marine->GetAbsOrigin());
			//Msg("Centering minimap at %f,%f\n", m_MapCentre.x, m_MapCentre.y);

			if ( m_nMapTextureID == -1 )
				return;

			// source should be a fixed number of world units around the centre
			float source_size = asw_map_range.GetFloat() * ASW_SCREENSHOT_SCALE / m_fMapScale;

			int map_left = m_MapCornerInPanel.x;
			int map_right = wide + ox;
			int map_top = m_MapCornerInPanel.y;
			int map_bottom = tall + oy;

			if (m_bHasOverview)	// draw a section of the minimap
			{
				float Source1X = m_MapCentre.x - source_size;
				float Source1Y = m_MapCentre.y - source_size;
				float Source2X = m_MapCentre.x + source_size;
				float Source2Y = m_MapCentre.y + source_size;
				// if we're off an edge of the map texture, pull in our draw coords so we don't get stretching
				if (Source1X < 0)
				{
					map_left -= (Source1X / source_size) * (m_iMapSize * 0.5f);
				}
				if (Source2X > 1023)
				{
					map_right -= ((Source2X - 1023) / source_size) * (m_iMapSize * 0.5f);
				}
				if (Source1Y < 0)
				{
					map_top -= (Source1Y / source_size) * (m_iMapSize * 0.5f);
				}
				if (Source2Y > 1023)
				{
					map_bottom -= ((Source2Y - 1023) / source_size) * (m_iMapSize * 0.5f);
				}
					
				// clamp uvs
				Source1X = MAX(0, Source1X);
				Source1Y = MAX(0, Source1Y);
				Source2X = MIN(1024, Source2X);
				Source2Y = MIN(1024, Source2Y);

				surface()->DrawSetColor(Color(255,255,255,255));
				surface()->DrawSetTexture(m_nMapTextureID);
				
				Vertex_t points[4] = 
				{ 
				Vertex_t( Vector2D(map_left, map_top),					Vector2D(Source1X/1024.0f,Source1Y/1024.0f) ), 
				Vertex_t( Vector2D(map_right, map_top),					Vector2D(Source2X/1024.0f,Source1Y/1024.0f) ), 
				Vertex_t( Vector2D(map_right, map_bottom),				Vector2D(Source2X/1024.0f,Source2Y/1024.0f) ), 
				Vertex_t( Vector2D(map_left, map_bottom),				Vector2D(Source1X/1024.0f,Source2Y/1024.0f) ) 
				}; 
				surface()->DrawTexturedPolygon( 4, points );
			}
			else
			{
				// no overview, we're just drawing our scanner texture
				surface()->DrawSetColor(Color(255,255,255,255));
				surface()->DrawSetTexture(m_nMapTextureID);
				
				Vertex_t points[4] = 
				{ 
				Vertex_t( Vector2D(map_left, map_top),					Vector2D(0,0) ), 
				Vertex_t( Vector2D(map_right, map_top),					Vector2D(1,0) ), 
				Vertex_t( Vector2D(map_right, map_bottom),				Vector2D(1,1) ), 
				Vertex_t( Vector2D(map_left, map_bottom),				Vector2D(0,1) ) 
				}; 
				surface()->DrawTexturedPolygon( 4, points );
			}
		}		
	}
}

void CASWHudMinimap::Paint()
{
	VPROF_BUDGET( "CASWHudMinimap::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	
	BaseClass::Paint();
	PaintMapSection();

	PaintObjectiveMarkers();

	if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME)
	{
		// hack to make our child panel draw the follow lines underneath the marine blips painted by this panel
		if (m_pLinePanel)
		{
			vgui::VPANEL vpanel = m_pLinePanel->GetVPanel();
			vgui::surface()->PushMakeCurrent( vpanel, true );
			m_pLinePanel->PaintFollowLines();
			vgui::surface()->PopMakeCurrent( vpanel );
		}
		PaintBlips();
	}
	else
	{
		C_ASW_Game_Resource* pGameResource = ASWGameResource();
		if (!pGameResource)
			return;

		C_ASW_Scanner_Info* pScanner = pGameResource->GetScannerInfo();
		if (!pScanner)
			return;

		// fade blips out
		pScanner->CopyServerBlips();
		pScanner->FadeBlips();
	}
	//PaintFrame();
}


void CASWHudMinimap::PaintObjectiveMarkers()
{
	const int nMaxMarks = 3;
	ObjectiveMapMark *(pMarks[ nMaxMarks ]);
	float fMarkDistances[ nMaxMarks ];
	int nMarks = 0;

	for ( int nObjective = 0; nObjective < ASW_MAX_OBJECTIVES; ++nObjective )
	{
		C_ASW_Objective *pObjective = ASWGameResource()->GetObjective( nObjective );
		if ( pObjective )
		{
			if ( pObjective->IsObjectiveComplete() || pObjective->IsObjectiveFailed() || pObjective->IsObjectiveHidden() || pObjective->IsObjectiveDummy() )
			{
				continue;
			}

			int iNumMapMarks = pObjective->GetMapMarkingsCount();
			ObjectiveMapMark *m_pMapMarks = pObjective->GetMapMarkings();

			for ( int i = 0; i < iNumMapMarks; ++i )
			{
				if ( m_pMapMarks[ i ].bComplete || !m_pMapMarks[ i ].bEnabled )
				{
					// Don't draw completed or hidden ones
					continue;
				}

				float fDist = m_MapCentre.DistTo( Vector2D( m_pMapMarks[ i ].x + m_pMapMarks[ i ].w / 2, m_pMapMarks[ i ].y + m_pMapMarks[ i ].h / 2 ) );

				if ( nMarks < nMaxMarks )
				{
					pMarks[ nMarks ] = &( m_pMapMarks[ i ] );
					fMarkDistances[ nMarks ] = fDist;
					++nMarks;
				}
				else
				{
					int nFarthest = 0;
					float fFarthestDist = fMarkDistances[ 0 ];

					for ( int nMark = 1; nMark < nMarks; ++nMark )
					{
						if ( fFarthestDist < fMarkDistances[ nMark ] )
						{
							fFarthestDist = fMarkDistances[ nMark ];
							nFarthest = nMark;
						}
					}

					if ( fDist < fMarkDistances[ nFarthest ] )
					{
						pMarks[ nFarthest ] = &( m_pMapMarks[ i ] );
						fMarkDistances[ nFarthest ] = fDist;
					}
				}
			}
		}
	}

	int nAlpha = 50.0f + 100.0f * ( sinf( gpGlobals->curtime * 5.0f ) + 2.0f ) * 0.5f;

	for ( int nMark = 0; nMark < nMarks; ++nMark )
	{
		PaintRect( pMarks[ nMark ]->x, pMarks[ nMark ]->y, pMarks[ nMark ]->w, pMarks[ nMark ]->h, Color( 230, 192, 0, nAlpha ) );
	}
}

void CASWHudMinimap::PaintBlips()
{
	PaintMarineBlips();
	PaintScannerBlips();
}

void CASWHudMinimap::PaintScannerBlips()
{
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	C_ASW_Scanner_Info* pScanner = pGameResource->GetScannerInfo();
	if (!pScanner)
		return;

	// draw blips blindly from server for now
	pScanner->CopyServerBlips();
	pScanner->FadeBlips();
	Color red(250,110,110,255);
	Color blue(66,142,192,255);
	Color white(255,255,255,255);
	for (int i=0;i<ASW_SCANNER_MAX_BLIPS;i++)	//todo: draw overflow blips
	{
		if (pScanner->m_ClientBlipIndex[i] != 0)
		{
			Vector vecWorldPos(0,0,0);
			C_BaseEntity* pClientEnt = C_BaseEntity::Instance(pScanner->m_ClientBlipIndex[i]);
			if (pClientEnt)
			{
				vecWorldPos.x = pClientEnt->GetAbsOrigin().x;
				vecWorldPos.y = pClientEnt->GetAbsOrigin().y;
			}
			else
			{
				vecWorldPos.x = pScanner->m_fClientBlipX[i];
				vecWorldPos.y = pScanner->m_fClientBlipY[i];
			}
			//float f = abs(0.5f - pScanner->m_fBlipStrength[i]) * 2.0f;	// fade in/out
			float f = 1.0f - pScanner->m_fBlipStrength[i];	// just fade out

			PaintWorldBlip( vecWorldPos, f,
							( pScanner->m_BlipType[i] == 1 ) ? blue : ( asw_scanner_classic.GetBool() ? white : red ),
							MapBlipTexture_t( pScanner->m_BlipType[ i ] ) );		// draw the blip in blue triangle if it's a computer/button panel	
		}
	}
}

// paints the scanner ring and strengthens any blips the ring passes
#define ASW_SCANNER_MAX_SOUND_DIST 1324
void CASWHudMinimapLinePanel::PaintScannerRing()
{
	// skip scanner ring if we're not ingame
	if (!ASWGameRules() || ASWGameRules()->GetGameState() != ASW_GS_INGAME)
		return;

	if (m_nScannerRingTexture == -1)
		return;

	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	C_ASW_Scanner_Info* pScanner = pGameResource->GetScannerInfo();
	if (!pScanner)
		return;

	// each tech marine has his own time and ring, so go through them
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		C_ASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetProfile() && pMR->GetProfile()->CanScanner() && pMR->GetMarineEntity() && pMR->GetHealthPercent() > 0)
		{
			bool bCheckRing = (pMR->m_fScannerTime <= 1.0f);

			pMR->m_fScannerTime += gpGlobals->frametime * 2.2f;						
			if (bCheckRing)
			{
				float fScannerRangeWorldUnits =	MarineSkills()->GetSkillBasedValueByMarineResource(pMR, ASW_MARINE_SKILL_SCANNER); // asw_scanner_range.GetFloat()
				float scanner_range = m_pMap->WorldDistanceToPixelDistance(fScannerRangeWorldUnits) * pMR->m_fScannerTime * asw_scanner_ring_scale.GetFloat();

				Vector2D marine_pos(0,0);
				Vector marine_world_pos = pMR->GetMarineEntity()->GetAbsOrigin();
				// check for refreshing the strength of any blips the ring just passed
				float ring_world_distance = fScannerRangeWorldUnits * MIN(pMR->m_fScannerTime, 1.0f) * asw_scanner_ring_scale.GetFloat();
				float ring_world_distance_sq = ring_world_distance * ring_world_distance; // square it to save doing any square roots below
				for (int k=0;k<ASW_SCANNER_MAX_BLIPS;k++)
				{
					if (pScanner->m_ClientBlipIndex[k] != 0 && pScanner->m_fBlipStrength[k]==0)
					{
						float xdiff = pScanner->m_fClientBlipX[k] - marine_world_pos.x;
						float ydiff = pScanner->m_fClientBlipY[k] - marine_world_pos.y;
						float dist = xdiff * xdiff + ydiff * ydiff;
						if (dist < ring_world_distance_sq)
						{
							// our ring is outside this dot, so refresh it
							pScanner->m_fBlipStrength[k] = 1.0f;
							// check for making the first blip sound
							if (!pMR->m_bPlayedBlipSound && pScanner->m_BlipType[k] != 1)	// don't blip on computers/buttons
							{
								pMR->m_bPlayedBlipSound = true;
								CLocalPlayerFilter filter;
								EmitSound_t ep;
								ep.m_nChannel = CHAN_AUTO;
								static char szWarningSound[32];
								Q_snprintf(szWarningSound, sizeof(szWarningSound), "ASWScanner.Warning%d", asw_scanner_warning_sound.GetInt());
								ep.m_pSoundName = szWarningSound;
								ep.m_flVolume = asw_scanner_warning_volume.GetFloat();
								ep.m_SoundLevel = SNDLVL_NORM;
								ep.m_nFlags |= SND_CHANGE_PITCH;
								ep.m_nFlags |= SND_CHANGE_VOL;
								float max_ring_dist = fScannerRangeWorldUnits * asw_scanner_ring_scale.GetFloat();
								if (max_ring_dist <= 0)
									ep.m_nPitch = 1.0f * 100.0f;
								else
								{									
									float fPitchScale = (asw_scanner_pitch_base.GetFloat() - asw_scanner_pitch_change.GetFloat() * (sqrt(dist)/max_ring_dist));
									ep.m_nPitch = fPitchScale * 100.0f;
								}
								//Msg("Pitch is = %d\n", ep.m_nPitch);
								// adjust volume by distance to tech marine
								if (pPlayer->GetMarine())
								{
									float dist_to_tech = pPlayer->GetMarine()->GetAbsOrigin().DistTo(marine_world_pos);
									float fraction = dist_to_tech / ASW_SCANNER_MAX_SOUND_DIST;
									if (fraction > 0.3f)	// give a buffer of max volume
										ep.m_flVolume *= (1.0f - ((fraction-0.3f)*0.7f));
								}
								if (ep.m_flVolume>0)
								{
									m_pMap->m_fLastBlipHitTime = gpGlobals->curtime;

									C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, ep);
									// if the tech is ours, then check for saying something about the incoming aliens
									if (pPlayer->entindex() == pMR->GetCommanderIndex() && m_pMap && pScanner->m_BlipType[k] == 0)	// only do the speech when it's an alien, not a door
									{
										//Msg("client checking blip speech %d\n", i);
										m_pMap->CheckBlipSpeech(i);
									}
								}
							}
						}
					}
				}
				if ( pMR->m_fScannerTime <= 1.0f && ( gpGlobals->curtime < m_pMap->m_fLastBlipHitTime + 3.0f || asw_scanner_classic.GetBool() ) )
				{
					// fade the ring out at the ends
					if (pMR->m_fScannerTime < 0.5f)
						surface()->DrawSetColor(Color(255,255,255,255));
					else
					{
						float f = (1.0f - pMR->m_fScannerTime) * 2;
						surface()->DrawSetColor(Color(255,255,255,255.0f * f));
					}
					surface()->DrawSetTexture(m_nScannerRingTexture);

					marine_pos = m_pMap->WorldToMapTexture(marine_world_pos);
					float ring_center_x = 0;
					float ring_center_y = 0;
					TextureToLinePanel(m_pMap, marine_pos, ring_center_x, ring_center_y);
					Vertex_t points[4] = 
					{ 
					Vertex_t( Vector2D(ring_center_x - scanner_range, ring_center_y - scanner_range), Vector2D(0,0) ), 
					Vertex_t( Vector2D(ring_center_x + scanner_range, ring_center_y - scanner_range), Vector2D(1,0) ), 
					Vertex_t( Vector2D(ring_center_x + scanner_range, ring_center_y + scanner_range), Vector2D(1,1) ), 
					Vertex_t( Vector2D(ring_center_x - scanner_range, ring_center_y + scanner_range), Vector2D(0,1) ) 
					}; 
					surface()->DrawTexturedPolygon( 4, points );
				}
			}
			// check for resetting the ring to center again
			if (pMR->m_fScannerTime > 2.2f)
			{
				// play idle sound
				pMR->m_iScannerSoundSkip++;
				//if ((pMR->m_iScannerSoundSkip % 4) == 0)
				{
					if (asw_debug_scanner_sound.GetBool())
						Msg("%f: Playing scanner sound! (not mod 4).\n", gpGlobals->curtime);

					pMR->m_iScannerSoundSkip = 0;

					if ( asw_scanner_classic.GetBool() || gpGlobals->curtime < m_pMap->m_fLastBlipHitTime + 3.0f )
					{
						CLocalPlayerFilter filter;
						EmitSound_t ep;
						ep.m_nChannel = CHAN_AUTO;
						static char szIdleSound[32];
						Q_snprintf(szIdleSound, sizeof(szIdleSound), "ASWScanner.Idle%d", asw_scanner_idle_sound.GetInt());
						ep.m_pSoundName = szIdleSound;

						ep.m_flVolume = asw_scanner_idle_volume.GetFloat();
						ep.m_nFlags |= SND_CHANGE_VOL;
						ep.m_SoundLevel = SNDLVL_NORM;
						// adjust volume by distance to tech marine					
						if (pPlayer->GetMarine())
						{
							float dist_to_tech = pPlayer->GetMarine()->GetAbsOrigin().DistTo(pMR->GetMarineEntity()->GetAbsOrigin());
							float fraction = dist_to_tech / ASW_SCANNER_MAX_SOUND_DIST;
							if (fraction > 0.3f)	// give a buffer of max volume
								ep.m_flVolume *= (1.0f - ((fraction-0.3f)*0.7f));
						}
						if (ep.m_flVolume > 0)
						{
							if (asw_debug_scanner_sound.GetBool())
								Msg("emitting scanner idle sound with volume %f\n", ep.m_flVolume);
							C_BaseEntity::StopSound( -1, ep.m_pSoundName );
							C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, ep );					
						}
					}
				}

				pMR->m_bPlayedBlipSound = false;		//todo: what effect does this have with multi tech marines?
				pMR->m_fScannerTime = 0;
			}
		}
	}
}

void CASWHudMinimapLinePanel::Paint()
{
	//CASWHudMinimap *m_pMap = dynamic_cast<CASWHudMinimap*>(GetParent());
	if (!m_pMap)
		return;
	if (!ASWGameRules() && ASWGameRules()->GetGameState() < ASW_GS_INGAME)
		return;
	// paint a black outline over the lines
	for (int i=0;i<m_pMap->m_MapLines.Count();i++)
	{		
		float x,y;
		if (!m_pMap->m_MapLines[i].bSetBlipCentre)
		{
			Vector vecBlipPos;
			vecBlipPos.x = m_pMap->m_MapLines[i].worldpos.x;
			vecBlipPos.y = m_pMap->m_MapLines[i].worldpos.y;
			vecBlipPos.z = 0;
			m_pMap->m_MapLines[i].blipcentre = m_pMap->WorldToMapTexture(vecBlipPos);
			m_pMap->m_MapLines[i].bSetBlipCentre = true;
		}
		Vector2D vecBlipCentre = m_pMap->m_MapLines[i].blipcentre;
		TextureToLinePanel(m_pMap, vecBlipCentre, x, y);
		if (m_pMap->m_MapLines[i].bLink)
		{
			bool bFound = false;
			if (i>1)
			{
				for (int k=i-1; k>0; k--)	// find the previous line from this player, if any
				{
					if (m_pMap->m_MapLines[i].player_index == m_pMap->m_MapLines[k].player_index)
					{
						m_pMap->m_MapLines[i].linkpos = m_pMap->m_MapLines[k].worldpos;
						bFound = true;
						break;
					}
				}				
			}
			if (bFound)
			{				
				float x2,y2;
				if (!m_pMap->m_MapLines[i].bSetLinkBlipCentre)
				{
					Vector vecBlipPos2;
					vecBlipPos2.x = m_pMap->m_MapLines[i].linkpos.x;
					vecBlipPos2.y = m_pMap->m_MapLines[i].linkpos.y;
					vecBlipPos2.z = 0;
					m_pMap->m_MapLines[i].linkblipcentre = m_pMap->WorldToMapTexture(vecBlipPos2);
					m_pMap->m_MapLines[i].bSetLinkBlipCentre = true;
				}
				Vector2D vecBlipCentre2 = m_pMap->m_MapLines[i].linkblipcentre;
				TextureToLinePanel(m_pMap, vecBlipCentre2, x2, y2);
				float t = gpGlobals->curtime - m_pMap->m_MapLines[i].created_time;
				int alpha = 255;
				if (t < MAP_LINE_SOLID_TIME)
				{
					
				}
				else if (t < MAP_LINE_SOLID_TIME + MAP_LINE_FADE_TIME)
				{
					alpha = 255 - ((t - MAP_LINE_SOLID_TIME) / MAP_LINE_FADE_TIME) * 255.0f;
				}
				else
				{
					continue;
				}

				surface()->DrawSetTexture(m_pMap->m_nWhiteTexture);
				vgui::Vertex_t start, end;

				// draw black outline around the line to give it some softness	
				surface()->DrawSetColor(Color(0,0,0, alpha));

				start.Init(Vector2D(x - 1.50f,y - 1.50f), Vector2D(0,0));
				end.Init(Vector2D(x2 - 1.50f,y2 - 1.50f), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);

				start.Init(Vector2D(x + 1.50f,y - 1.50f), Vector2D(0,0));
				end.Init(Vector2D(x2 + 1.50f,y2 - 1.50f), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);

				start.Init(Vector2D(x - 1.50f,y + 1.50f), Vector2D(0,0));
				end.Init(Vector2D(x2 - 1.50f,y2 + 1.50f), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);

				start.Init(Vector2D(x + 1.50f,y + 1.50f), Vector2D(0,0));
				end.Init(Vector2D(x2 + 1.50f,y2 + 1.50f), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);
			}
		}
	}

	// paint map line dots	
	for (int i=0;i<m_pMap->m_MapLines.Count();i++)
	{		
		//m_pMap->PaintWorldBlip(vecBlipPos, 0.5f, Color(0,255,0,255));
		float x,y;
		if (!m_pMap->m_MapLines[i].bSetBlipCentre)
		{
			Vector vecBlipPos;
			vecBlipPos.x = m_pMap->m_MapLines[i].worldpos.x;
			vecBlipPos.y = m_pMap->m_MapLines[i].worldpos.y;
			vecBlipPos.z = 0;
			m_pMap->m_MapLines[i].blipcentre = m_pMap->WorldToMapTexture(vecBlipPos);
			m_pMap->m_MapLines[i].bSetBlipCentre = true;
		}
		Vector2D vecBlipCentre = m_pMap->m_MapLines[i].blipcentre;
		TextureToLinePanel(m_pMap, vecBlipCentre, x, y);
		if (m_pMap->m_MapLines[i].bLink)
		{
			bool bFound = false;
			if (i>1)
			{
				for (int k=i-1; k>0; k--)	// find the previous line from this player, if any
				{
					if (m_pMap->m_MapLines[i].player_index == m_pMap->m_MapLines[k].player_index)
					{
						m_pMap->m_MapLines[i].linkpos = m_pMap->m_MapLines[k].worldpos;
						bFound = true;
						break;
					}
				}				
			}
			if (bFound)
			{
				
				float x2,y2;
				if (!m_pMap->m_MapLines[i].bSetLinkBlipCentre)
				{
					Vector vecBlipPos2;
					vecBlipPos2.x = m_pMap->m_MapLines[i].linkpos.x;
					vecBlipPos2.y = m_pMap->m_MapLines[i].linkpos.y;
					vecBlipPos2.z = 0;
					m_pMap->m_MapLines[i].linkblipcentre = m_pMap->WorldToMapTexture(vecBlipPos2);
					m_pMap->m_MapLines[i].bSetLinkBlipCentre = true;
				}
				Vector2D vecBlipCentre2 = m_pMap->m_MapLines[i].linkblipcentre;
				TextureToLinePanel(m_pMap, vecBlipCentre2, x2, y2);
				float t = gpGlobals->curtime - m_pMap->m_MapLines[i].created_time;
				int alpha = 255;
				if (t < MAP_LINE_SOLID_TIME)
				{
					//surface()->DrawSetColor(Color(255,255,255,255));
					//surface()->DrawLine(x,y,x2,y2);
				}
				else if (t < MAP_LINE_SOLID_TIME + MAP_LINE_FADE_TIME)
				{
					alpha = 255 - ((t - MAP_LINE_SOLID_TIME) / MAP_LINE_FADE_TIME) * 255.0f;
				}
				else
				{
					continue;
				}

				surface()->DrawSetTexture(m_pMap->m_nWhiteTexture);
				vgui::Vertex_t start, end;				

				// draw main line
				surface()->DrawSetColor(Color(DRAWING_LINE_R, DRAWING_LINE_G, DRAWING_LINE_B, 0.5f * alpha));
				//surface()->DrawLine(x,y,x2,y2);
				start.Init(Vector2D(x,y), Vector2D(0,0));
				end.Init(Vector2D(x2,y2), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);
				
				// draw translucent ones around it to give it some softness	
				surface()->DrawSetColor(Color(DRAWING_LINE_R, DRAWING_LINE_G, DRAWING_LINE_B, 0.5f * alpha));

				start.Init(Vector2D(x - 0.50f,y - 0.50f), Vector2D(0,0));
				end.Init(Vector2D(x2 - 0.50f,y2 - 0.50f), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);

				start.Init(Vector2D(x + 0.50f,y - 0.50f), Vector2D(0,0));
				end.Init(Vector2D(x2 + 0.50f,y2 - 0.50f), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);

				start.Init(Vector2D(x - 0.50f,y + 0.50f), Vector2D(0,0));
				end.Init(Vector2D(x2 - 0.50f,y2 + 0.50f), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);

				start.Init(Vector2D(x + 0.50f,y + 0.50f), Vector2D(0,0));
				end.Init(Vector2D(x2 + 0.50f,y2 + 0.50f), Vector2D(1,1));
				SoftLine::DrawPolygonLine(start, end);
			}
		}
	}

	// paint the frame over the top
	//int wide, tall;
	//m_pMap->GetSize(wide,tall);
	
	//if ( m_pMap->m_nFrameTexture == -1 )
		//return;

	//surface()->DrawSetColor(m_pMap->GetBgColor());
	//surface()->DrawSetTexture(m_pMap->m_nFrameTexture);
	//surface()->DrawTexturedRect(-20, -20, wide, tall);
	//surface()->DrawTexturedSubRect(0, 0, wide, tall,);
		//-m_pMap->m_MapCornerInPanel.x*2, -m_pMap->m_MapCornerInPanel.y*2, wide, tall);

	//surface()->DrawTexturedSubRect(m_MapCornerInPanel.x, m_MapCornerInPanel.y, wide, tall,
					//Source1X/1024.0, Source1Y/1024.0, Source2X/1024.0, Source2Y/1024.0);
	PaintScannerRing();
}

void CASWHudMinimapLinePanel::PaintFollowLines()
{
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	// paint follow lines
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		C_ASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetMarineEntity())
		{
			C_ASW_Marine* pMarine = pMR->GetMarineEntity();
			if (pMarine && pMarine->m_hMarineFollowTarget && !pMarine->IsInhabited())
			{
				PaintFollowLine(pMarine, pMarine->m_hMarineFollowTarget);
			}
		}
	}
}

void CASWHudMinimap::PaintRect( int nX, int nY, int nWidth, int nHeight, Color color )
{
	Vector2D rectCorner1;
	Vector2D rectCorner2;

	rectCorner1.x = nX;
	rectCorner1.y = nY;
	rectCorner2.x = nX + nWidth;
	rectCorner2.y = nY + nHeight;

	rectCorner1 = MapTextureToPanel( rectCorner1 );
	rectCorner2 = MapTextureToPanel( rectCorner2 );

	if ( rectCorner1.x > m_MapCornerInPanel.x + m_iMapSize || rectCorner1.y > m_MapCornerInPanel.y + m_iMapSize || 
		 rectCorner2.x < m_MapCornerInPanel.x || rectCorner2.y < m_MapCornerInPanel.y )
	{
		// Out of bounds
		Vector2D vArrowCenter = Vector2D( ( rectCorner1.x + rectCorner2.x ) / 2, ( rectCorner1.y + rectCorner2.y ) / 2 );
		Vector2D vPanelCenter = Vector2D( m_MapCornerInPanel.x + m_iMapSize / 2, m_MapCornerInPanel.y + m_iMapSize / 2 );

		Vector2D vDirection = vArrowCenter - vPanelCenter;
		Vector2DNormalize( vDirection );

		float fFacingYaw = RAD2DEG( atanf( vDirection.y / vDirection.x ) ) - ( vDirection.x < 0.0f ? 0.0f : 180.0f );

		int iFacingSize = GetWide() * 0.05f * asw_hud_scale.GetFloat();

		vArrowCenter.x = clamp( vArrowCenter.x, m_MapCornerInPanel.x, m_MapCornerInPanel.x + m_iMapSize - iFacingSize / 2 );
		vArrowCenter.y = clamp( vArrowCenter.y, m_MapCornerInPanel.y, m_MapCornerInPanel.y + m_iMapSize - iFacingSize / 2 );

		vArrowCenter -= vDirection * 4.0f * ( sinf( gpGlobals->curtime * 7.0f ) + 1.0f );

		// set up a square to the right
		int xoffset = -2;	// temp? to make the arc look nice next to blips..
		int yoffset = 1;
		Vector vecCornerTL(xoffset, iFacingSize * -0.5f + yoffset, 0);
		Vector vecCornerTR(iFacingSize + xoffset, iFacingSize * -0.5f+ yoffset, 0);
		Vector vecCornerBR(iFacingSize + xoffset, iFacingSize * 0.5f+ yoffset, 0);
		Vector vecCornerBL(xoffset, iFacingSize * 0.5f+ yoffset, 0);
		Vector vecCornerTL_rotated, vecCornerTR_rotated, vecCornerBL_rotated, vecCornerBR_rotated;

		// rotate it by our facing yaw
		QAngle angFacing( 0, fFacingYaw, 0 );
		VectorRotate(vecCornerTL, angFacing, vecCornerTL_rotated);
		VectorRotate(vecCornerTR, angFacing, vecCornerTR_rotated);
		VectorRotate(vecCornerBR, angFacing, vecCornerBR_rotated);
		VectorRotate(vecCornerBL, angFacing, vecCornerBL_rotated);

		surface()->DrawSetColor( color );
		surface()->DrawSetTexture(m_nFacingArcTexture);
		//surface()->DrawTexturedRect(Dest1X,Dest1Y,Dest2X,Dest2Y);
		Vertex_t points[4] = 
		{ 
			Vertex_t( Vector2D(vArrowCenter.x + vecCornerTL_rotated.x, vArrowCenter.y + vecCornerTL_rotated.y), Vector2D(0,0) ), 
			Vertex_t( Vector2D(vArrowCenter.x + vecCornerTR_rotated.x, vArrowCenter.y + vecCornerTR_rotated.y), Vector2D(1,0) ), 
			Vertex_t( Vector2D(vArrowCenter.x + vecCornerBR_rotated.x, vArrowCenter.y + vecCornerBR_rotated.y), Vector2D(1,1) ), 
			Vertex_t( Vector2D(vArrowCenter.x + vecCornerBL_rotated.x, vArrowCenter.y + vecCornerBL_rotated.y), Vector2D(0,1) ) 
		}; 
		surface()->DrawTexturedPolygon( 4, points );
	}
	else
	{
		rectCorner1.x = MAX( rectCorner1.x, m_MapCornerInPanel.x );
		rectCorner1.y = MAX( rectCorner1.y, m_MapCornerInPanel.y );

		rectCorner2.x = MIN( rectCorner2.x, m_MapCornerInPanel.x + m_iMapSize );
		rectCorner2.y = MIN( rectCorner2.y, m_MapCornerInPanel.y + m_iMapSize );

		surface()->DrawSetColor( color );
		surface()->DrawOutlinedRect( rectCorner1.x, rectCorner1.y, rectCorner2.x, rectCorner2.y );

		color[ 3 ] /= 8;

		surface()->DrawSetColor( color );
		surface()->DrawFilledRect( rectCorner1.x, rectCorner1.y, rectCorner2.x, rectCorner2.y );
	}
}

// fixme: blips should be sent from the server.. per player?
//    so as to be sent information about things you can't see (i.e. to give the tracker some range)
//    change networking to send entities to players if they're near his current marine
//					(and perhaps near any he's controlling?)
//    then we can send blips from server->client only if they're outside this range
//      these blips will be static
//    we'll also drops blips of every alien/marine entity the client knows about, these can move
//void CASWHudMinimap::CreateBlips()
//{
	//m_iNumBlips = 0;

//#define MAX_BLIPS 64
	//int	m_iBlipX[MAX_BLIPS];
	//int m_iBlipY[MAX_BLIPS];
	//int m_iNumBlips;
//}

float CASWHudMinimap::WorldDistanceToPixelDistance(float fWorldDistance)
{
	// convert to texture scale
	float result = fWorldDistance / m_fMapScale;
	result *= ASW_SCREENSHOT_SCALE;
	// find how many pixels on the texture correspond to the map panel halfwidth:
	float source_size = asw_map_range.GetFloat() * ASW_SCREENSHOT_SCALE / m_fMapScale;
	// find what fraction our offset is compared to the full texture width of the map:
	result /= source_size;
	// multiply that out by the pixel halfwidth of this panel:
	result *= (m_iMapSize * 0.5f);
	
	return result;
}


// loads in the script file for a particular map to set scale/origin
void CASWHudMinimap::SetMap(const char * levelname)
{
	// load new KeyValues
	//Msg("minimap SetMap: %s\n", levelname);
	if ( m_MapKeyValues && Q_strcmp( levelname, m_MapKeyValues->GetName() ) == 0 )
	{
		return;	// map didn't change
	}

	if ( m_MapKeyValues )
		m_MapKeyValues->deleteThis();

	m_MapKeyValues = new KeyValues( levelname );

	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", levelname );
	
	if ( !m_MapKeyValues->LoadFromFile( filesystem, tempfile, "GAME" ) )
	{
		// try to load it directly from the maps folder
		Q_snprintf( tempfile, sizeof( tempfile ), "maps/%s.txt", levelname );
		if ( !m_MapKeyValues->LoadFromFile( filesystem, tempfile, "GAME" ) )
		{
			//DevMsg( 1, "CASWHudMinimap::SetMap: couldn't load overview file for map %s.\n", levelname );
			m_nMapTextureID = surface()->CreateNewTextureID();
			surface()->DrawSetTextureFile( m_nMapTextureID, "vgui/swarm/hud/scanner", true, false);
			// put in some default numbers so the scanner works
			m_MapOrigin.x	= 0;
			m_MapOrigin.y	= 0;
			m_fMapScale		= 25.0f;
			Q_snprintf(m_szMissionTitle, sizeof(m_szMissionTitle), "Unnamed Mission");
			m_bHasOverview = false;
			return;
		}
	}	

	// TODO release old texture ?

	m_nMapTextureID = surface()->CreateNewTextureID();

	//if we have not uploaded yet, lets go ahead and do so	
	surface()->DrawSetTextureFile( m_nMapTextureID, m_MapKeyValues->GetString("material"), true, false);

	m_MapOrigin.x	= m_MapKeyValues->GetInt("pos_x");
	m_MapOrigin.y	= m_MapKeyValues->GetInt("pos_y");
	m_fMapScale		= m_MapKeyValues->GetFloat("scale", 1.0f);
	Q_snprintf(m_szMissionTitle, sizeof(m_szMissionTitle),m_MapKeyValues->GetString("missiontitle"));
	m_bHasOverview = true;

}

void CASWHudMinimap::FireGameEvent( IGameEvent * event )
{
	const char * type = event->GetName();
	//Msg("Minimap firegameevent %s\n", type);
	if ( Q_strcmp(type, "game_newmap") == 0 )
	{
		SetMap( event->GetString("mapname") );
		m_MapLines.RemoveAll();
		m_fLastBlipSpeechTime = -100.0f;
	}
	else if ( Q_strcmp(type, "server_spawn") == 0 )
	{
		const char *hostname = event->GetString( "hostname" );
		Q_snprintf(m_szServerName, sizeof(m_szServerName), "%s", hostname);
	}
	CASW_HudElement::FireGameEvent(event);
}

bool CASWHudMinimap::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (!ShouldDraw() || !asw_minimap_clicks.GetBool())
		return false;

	if (!bDown)
	{
		m_bDrawingMapLines = false;
		return false;
	}

	if (bRightClick)
		return false;

	if (!IsWithinMapBounds(x,y))
		return false;

	int ox, oy;
	GetScaledOffset(ox, oy);

	SendMapLine(x+ox,y+oy,true);
	m_bDrawingMapLines = true;

	return true;
}

bool CASWHudMinimap::IsWithinMapBounds(int x, int y)
{
	//int wide, tall;
	int posx, posy;
	if (m_nMapTextureID == -1)
		return false;
	GetPos(posx,posy);
	//int ox, oy;
	//GetScaledOffset(ox, oy);
	//posx += ox;
	//posy += oy;
	//GetSize(wide,tall);
	//wide *= asw_hud_scale.GetFloat();
	//tall *= asw_hud_scale.GetFloat();
	x -= posx;
	y -= posy;
	return (x >= m_MapCornerInPanel.x
		&& y >= m_MapCornerInPanel.y
		&& x <= (m_MapCornerInPanel.x + m_iMapSize)
		&& y <= (m_MapCornerInPanel.y + m_iMapSize));	
}

void CASWHudMinimap::ClipToMapBounds(int &x, int &y)
{
	int wide, tall;
	int posx, posy;
	GetPos(posx,posy);
	int ox, oy;
	GetScaledOffset(ox, oy);
	posx += ox;
	posy += oy;
	GetSize(wide,tall);	
	wide *= asw_hud_scale.GetFloat();
	tall *= asw_hud_scale.GetFloat();
	if (x > m_MapCornerInPanel.x + posx + wide)
		x = m_MapCornerInPanel.x + posx + wide;
	if (x < m_MapCornerInPanel.x + posx)
		x = m_MapCornerInPanel.x + posx;
	if (y > m_MapCornerInPanel.y + posy + tall)
		y = m_MapCornerInPanel.y + posy + tall;
	if (y < m_MapCornerInPanel.y + posy)
		y = m_MapCornerInPanel.y + posy;	
}

// drawing a map line at point x and y on the hud element
void CASWHudMinimap::SendMapLine(int x, int y, bool bInitial)
{
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if ( local )
	{
		C_ASW_Marine *marine = local->GetMarine();
		if (marine)
		{
			int wide, tall, posx, posy;
			GetSize(wide,tall);
			wide *= asw_hud_scale.GetFloat();
			tall *= asw_hud_scale.GetFloat();
			GetPos(posx, posy);
			int ox, oy;
			GetScaledOffset(ox, oy);
			posx += ox;
			posy += oy;
			x -= posx;
			y -= posy;

			Vector vecMapCentre = marine->GetAbsOrigin();
						
			// pixel difference between where we clicked and the centre of the panel
			int diff_x = x - m_MapCentreInPanel.x;
			int diff_y = m_MapCentreInPanel.y - y;

			// find the world position we clicked on
			Vector vecMapLinePos;
			// world difference should be: ( diff_x / panel size ) * map range
			vecMapLinePos.x = vecMapCentre.x + (diff_x / (m_iMapSize*0.5f)) * asw_map_range.GetFloat();
			vecMapLinePos.y = vecMapCentre.y + (diff_y / (m_iMapSize*0.5f)) * asw_map_range.GetFloat();
			//vecMapLinePos.x = vecMapCentre.x + (diff_x * m_fMapScale / ASW_SCREENSHOT_SCALE);
			//vecMapLinePos.y = vecMapCentre.y + (diff_y * m_fMapScale / ASW_SCREENSHOT_SCALE);			
			vecMapLinePos.z = marine->GetAbsOrigin().z;
			//Msg("vecMapCentre = %f %f\n", vecMapCentre.x, vecMapCentre.y);
			//Msg("VecMapLinePos = %f %f %f\n", vecMapLinePos.x, vecMapLinePos.y, vecMapLinePos.z);

			//FX_MicroExplosion(vecMapLinePos, Vector(0,0,1));
			
			// notify the server of this!
			char buffer[64];
			int linetype = bInitial ? 0 : 1;
			Q_snprintf(buffer, sizeof(buffer), "cl_mapline %d %d %d", linetype, (int) vecMapLinePos.x, (int) vecMapLinePos.y);
			engine->ClientCmd(buffer);

			m_fLastMapLine = gpGlobals->curtime;

			// short circuit add it to your own list
			MapLine line;
			line.player_index = local->entindex();
			line.worldpos.x = (int) vecMapLinePos.x;
			line.worldpos.y = (int) vecMapLinePos.y;
			line.created_time = gpGlobals->curtime;
			if (linetype == 1)		// links to a previous
			{
				line.bLink = true;
			}
			else
			{
				if (gpGlobals->curtime > m_fLastMinimapDrawSound + 5.0f)
				{
					CLocalPlayerFilter filter;
					C_BaseEntity::EmitSound( filter, -1, "ASWScanner.Drawing" );
					m_fLastMinimapDrawSound = gpGlobals->curtime;
				}
			}
			m_MapLines.AddToTail(line);
		}
	}
}

// converts a coord from 0->1023 map texture into x+y in this panel
Vector2D CASWHudMinimap::MapTextureToPanel( const Vector2D &texturepos )
{
	if ( !m_pMinimap )
	{
		m_pMinimap = GET_HUDELEMENT( CASWHudMinimap );

		if ( !m_pMinimap )
		{
			return vec2_origin;
		}
	}

	Vector2D offset(texturepos);

	// find how many pixels on the texture correspond to the map panel halfwidth:
	float source_size = asw_map_range.GetFloat() * ASW_SCREENSHOT_SCALE / m_pMinimap->m_fMapScale;

	// convert our tex coords to an offset from the map pixel at the centre of the panel:
	offset -= m_pMinimap->m_MapCentre;

	// find what fraction our offset is compared to the full texture width of the map:
	offset /= source_size;

	// multiply that out by the pixel halfwidth of this panel:
	offset *= (GetMapSize() * 0.5f);

	offset += m_pMinimap->m_MapCentreInPanel;

	return offset;
}

int CASWHudMinimap::GetArcSize( void )
{
	return GetWidth() * 0.1f * asw_hud_scale.GetFloat();
}


//void CASWHudMinimapLinePanel::TextureToLinePanel(CASWHudMinimap* pMap, const Vector &worldpos, int &x, int &y)
// converts a 0->1024 map texture coord to the x/y in the panel coord
void CASWHudMinimapLinePanel::TextureToLinePanel(CASWHudMinimap* pMap, const Vector2D &blip_centre, float &x, float &y)
{	
	if (!pMap)
		return;
	Vector2D result = pMap->MapTextureToPanel(blip_centre);
	int wx, wy;
	GetPos(wx,wy);
	int ox, oy;
	pMap->GetScaledOffset(ox, oy);
	//wx += ox;
	//wy += oy;
	
	x = result.x - wx;
	y = result.y - wy;
	//int w, t;
	//GetSize(w, t);
	//Vector2D blip_centre = pMap->WorldToMapTexture(worldpos);
	//float source_size = asw_map_range.GetFloat() * ASW_SCREENSHOT_SCALE / pMap->m_fMapScale;
	//x = blip_centre.x - pMap->m_MapCentre.x + source_size;
	//y = blip_centre.y - pMap->m_MapCentre.y + source_size;
	//x = blip_centre.x - pMap->m_MapCentre.x + pMap->m_iMapSize * 0.5f;
	//y = blip_centre.y - pMap->m_MapCentre.y + pMap->m_iMapSize * 0.5f;
}

bool CASWHudMinimap::UseDrawCrosshair(float x, float y)
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer || !pPlayer->GetMarine() || !asw_minimap_clicks.GetBool())
		return false;
	return IsWithinMapBounds(x,y);
}

void CASWHudMinimap::CheckBlipSpeech(int iMarine)
{
	// check if it's been a while since we had any movement
	if (gpGlobals->curtime - m_fLastBlipSpeechTime > ASW_BLIP_SPEECH_INTERVAL)
	{		
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		pPlayer->SendBlipSpeech(iMarine);
	}
	m_fLastBlipSpeechTime = gpGlobals->curtime;		// bump the time to current evne if we didn't say anything, so we don't continually say movement when there are blips all the time
}

void CASWHudMinimapLinePanel::PaintFollowLine(C_BaseEntity *pMarine, C_BaseEntity *pTarget)
{
	if (!pMarine || !pTarget)
		return;
	surface()->DrawSetTexture(m_pMap->m_nWhiteTexture);
	vgui::Vertex_t start, end;

	Vector2D startTexturePos = m_pMap->WorldToMapTexture(pMarine->GetAbsOrigin());
	Vector2D endTexturePos = m_pMap->WorldToMapTexture(pTarget->GetAbsOrigin());
	Vector2D start2D, end2D;
	TextureToLinePanel(m_pMap, startTexturePos, start2D.x, start2D.y);
	TextureToLinePanel(m_pMap, endTexturePos, end2D.x, end2D.y);
	//Vector2D start2D = m_pMap->MapTextureToPanel(startTexturePos);
	//Vector2D end2D = m_pMap->MapTextureToPanel(endTexturePos);
	
	float x, y, x2,y2;
	x = start2D.x;
	y = start2D.y;
	x2 = end2D.x;
	y2 = end2D.y;
	float alpha = 255;

	// draw black outline around the line to give it some softness	
	surface()->DrawSetColor(Color(0,0,0, alpha));

	start.Init(Vector2D(x - 1.50f,y - 1.50f), Vector2D(0,0));
	end.Init(Vector2D(x2 - 1.50f,y2 - 1.50f), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);

	start.Init(Vector2D(x + 1.50f,y - 1.50f), Vector2D(0,0));
	end.Init(Vector2D(x2 + 1.50f,y2 - 1.50f), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);

	start.Init(Vector2D(x - 1.50f,y + 1.50f), Vector2D(0,0));
	end.Init(Vector2D(x2 - 1.50f,y2 + 1.50f), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);

	start.Init(Vector2D(x + 1.50f,y + 1.50f), Vector2D(0,0));
	end.Init(Vector2D(x2 + 1.50f,y2 + 1.50f), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);

	// draw main line
	surface()->DrawSetColor(Color(255,0,0, 0.5f * alpha));
	//surface()->DrawLine(x,y,x2,y2);
	start.Init(Vector2D(x,y), Vector2D(0,0));
	end.Init(Vector2D(x2,y2), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);
	
	// draw translucent ones around it to give it some softness	
	surface()->DrawSetColor(Color(255,0,0, 0.5f * alpha));

	start.Init(Vector2D(x - 0.50f,y - 0.50f), Vector2D(0,0));
	end.Init(Vector2D(x2 - 0.50f,y2 - 0.50f), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);

	start.Init(Vector2D(x + 0.50f,y - 0.50f), Vector2D(0,0));
	end.Init(Vector2D(x2 + 0.50f,y2 - 0.50f), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);

	start.Init(Vector2D(x - 0.50f,y + 0.50f), Vector2D(0,0));
	end.Init(Vector2D(x2 - 0.50f,y2 + 0.50f), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);

	start.Init(Vector2D(x + 0.50f,y + 0.50f), Vector2D(0,0));
	end.Init(Vector2D(x2 + 0.50f,y2 + 0.50f), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);
}

CASWHudMinimap_Border::CASWHudMinimap_Border(vgui::Panel *pParent, const char *pElementName, CASWHudMinimap *pMinimap) :
	vgui::Panel(pParent, pElementName)
{
	m_pMinimap = pMinimap;
}

void CASWHudMinimap_Border::PaintBackground()
{
	if (ASWGameRules())
	{
		if (ASWGameRules()->IsIntroMap() || ASWGameRules()->IsOutroMap())
			return;
	}
	if (m_pMinimap && !m_pMinimap->ShouldDraw())
		return;

	if ( !asw_draw_hud.GetBool() || m_nBlackBarTexture == -1 )
	{
		return;
	}
	if ( !asw_scanner_background.GetBool() )
		return;
	//BaseClass::PaintBackground();
	
	vgui::surface()->DrawSetColor(Color(255,255,255,asw_hud_alpha.GetInt()));
	vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
	vgui::Vertex_t points[4] = 
	{ 
	vgui::Vertex_t( Vector2D(0, 0),										Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(GetWide(), 0),									Vector2D(1,0) ), 
	vgui::Vertex_t( Vector2D(GetWide(), GetTall()),		Vector2D(1,1) ),
	vgui::Vertex_t( Vector2D(0, GetTall()),		Vector2D(0,1) )
	};
	vgui::surface()->DrawTexturedPolygon( 4, points );	
}
