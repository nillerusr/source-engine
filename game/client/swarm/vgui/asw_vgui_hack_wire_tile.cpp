#include "cbase.h"
#include "asw_vgui_hack_wire_tile.h"
#include "vgui/ISurface.h"
#include "c_asw_hack_wire_tile.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui/IInput.h>
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>
#include "controller_focus.h"
#include <keyvalues.h>
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define ASW_WIRE_LEFT (GetWide() * 0.15f)

#define ASW_FOOTER_SIZE  (GetTall() * 0.0f)
#define ASW_TILE_SIZE    (32.0f * m_fScale)			
#define BAR_X			(GetWide() * 0.9f )
#define ASW_HEADER_SIZE  (ASW_TILE_SIZE * 2.5f)
#define ASW_SPACE_PER_WIRE m_iSpacePerWire
//#define ASW_WIRE_SPACE	 (GetTall() - ASW_HEADER_SIZE - ASW_FOOTER_SIZE)
//#define ASW_SPACE_PER_WIRE (float(ASW_WIRE_SPACE) / float(m_hHack->m_iNumWires))

#define BAR_HEIGHT		(12.0f * m_fScale)
#define BAR_WIDTH		(GetWide() * 0.8f)
#define BAR_PADDING		(6.0f * m_fScale)
#define BAR_TOP			(ASW_HEADER_SIZE - (BAR_HEIGHT + BAR_PADDING * 3) - ASW_TILE_SIZE * 0.5f)
#define BAR_Y(x)		(x * ASW_SPACE_PER_WIRE + ASW_HEADER_SIZE - (BAR_HEIGHT + BAR_PADDING * 3))

#define ASW_WIRE_LEFT (ASW_TILE_SIZE * 2.5f)

//int CASW_VGUI_Hack_Wire_Tile::s_nBackDropTexture = -1;
int CASW_VGUI_Hack_Wire_Tile::s_nTileHoriz = -1;
int CASW_VGUI_Hack_Wire_Tile::s_nTileLeft = -1;
int CASW_VGUI_Hack_Wire_Tile::s_nTileRight = -1;
int CASW_VGUI_Hack_Wire_Tile::s_nLeftConnect = -1;
int CASW_VGUI_Hack_Wire_Tile::s_nRightConnect = -1;
bool CASW_VGUI_Hack_Wire_Tile::s_bLoadedTextures = false;

CASW_VGUI_Hack_Wire_Tile::CASW_VGUI_Hack_Wire_Tile( vgui::Panel *pParent, const char *pElementName, C_ASW_Hack_Wire_Tile* pHack ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel()
{
	m_hHack = pHack;
	if (!s_bLoadedTextures)
		PrecacheHackingTextures();

	m_pProgressBarBackdrop = new vgui::Panel(this, "ProgressBackdrop");
	m_pProgressBar = new vgui::Panel(this, "ProgressBackdrop");	

	for (int i=0;i<4;i++)
	{
		m_pLeftConnect[i] = new vgui::ImagePanel(this, "LeftConnect");		
		//m_pLeftConnect[i]->SetImage("swarm/Computer/IconWeather");
		m_pLeftConnect[i]->SetImage("swarm/Hacking/LeftConnect");
		m_pLeftConnect[i]->SetShouldScaleImage(true);
		m_pRightConnect[i] = new vgui::ImagePanel(this, "RightConnect");
		//m_pRightConnect[i]->SetImage("swarm/Computer/IconStocks");
		m_pRightConnect[i]->SetImage("swarm/Hacking/RightConnect");
		m_pRightConnect[i]->SetShouldScaleImage(true);		
		
		m_pOpenLight[i] = new vgui::ImagePanel(this, "OpenLight");
		m_pOpenLight[i]->SetImage("swarm/Hacking/SwarmDoorHackOpen");
		m_pOpenLight[i]->SetShouldScaleImage(true);

		for (int x=0;x<ASW_MAX_TILE_COLUMNS;x++)
		{
			for (int y=0;y<ASW_MAX_TILE_ROWS;y++)
			{
				m_pTile[i][x][y] = NULL;
			}
		}
	}
	m_pWireLabel = new vgui::Label(this, "WireLabel", "#asw_wire_1_label");	
	m_pCompleteLabel = new vgui::Label(this, "CompleteLabel", "#asw_access_granted");	
	m_pCompleteLabel->SetVisible(false);	
	m_pFastMarker = new vgui::ImagePanel(this, "FastMarker");
	m_pFastMarker->SetImage("swarm/Hacking/FastMarker");
	m_pFastMarker->SetShouldScaleImage(true);
	m_pFastMarker->SetVisible(false);

	m_pAccessLoggedLabel = new vgui::Label(this, "AccessLogged", "#asw_hacking_access_logged");
	m_pAccessLoggedLabel->SetVisible(false);
	m_pAccessLoggedLabel->SetContentAlignment(vgui::Label::a_northeast);

	m_iSpacePerWire = 100;
	m_bLastAllLit = false;

	m_fLastThinkTime = gpGlobals->curtime;
}

void CASW_VGUI_Hack_Wire_Tile::ASWInit()
{
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(0,0,0,255) );

	InvalidateLayout(true);

	m_fScale = ScreenHeight() / 768.0f;
}

void CASW_VGUI_Hack_Wire_Tile::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);
	SetBgColor( Color(0,0,0,255) );
	//SetBgColor( Color(65,74,96,255) );
	
	SetMouseInputEnabled(true);

	vgui::HFont LabelFont = pScheme->GetFont( "Default", IsProportional() );
	vgui::HFont SmallCourier = pScheme->GetFont( "SmallCourier", IsProportional() );
	
	m_pProgressBarBackdrop->SetPaintBackgroundEnabled(true);
	m_pProgressBarBackdrop->SetPaintBackgroundType(2);
	m_pProgressBarBackdrop->SetBgColor( Color(65,74,96,128) );	
	m_pProgressBar->SetPaintBackgroundEnabled(true);
	m_pProgressBar->SetPaintBackgroundType(0);
	m_pProgressBar->SetBgColor(Color(255,255,255,192));
	m_pProgressBarBackdrop->SetAlpha(255);
	m_pProgressBar->SetAlpha(255);
	m_pWireLabel->SetFgColor(Color(255,255,255,255));
	m_pWireLabel->SetFont(LabelFont);	
	m_pCompleteLabel->SetFgColor(Color(255,0,0,255));
	m_pCompleteLabel->SetFont(LabelFont);
	m_pFastMarker->SetDrawColor(Color(255,0,0,255));
	m_pFastMarker->SetAlpha(255);

	m_pAccessLoggedLabel->SetFgColor(Color(255,0,0,255));
	m_pAccessLoggedLabel->SetFont(SmallCourier);
}

void CASW_VGUI_Hack_Wire_Tile::OnThink()
{
	int x,y,w,t;
	GetBounds(x,y,w,t);

	//float deltatime = gpGlobals->curtime - m_fLastThinkTime;
	m_fLastThinkTime = gpGlobals->curtime;

	for (int i=0;i<4;i++)
	{
		if ( !m_hHack || i > m_hHack->m_iNumWires - 1
#ifdef SHRINK_WHEN_ALL_WIRES_LIT
			|| m_bLastAllLit
#endif
			)
		{
			//Msg("connect %d more than numwires(%d) -1\n", i, m_hHack->m_iNumWires);
			m_pLeftConnect[i]->SetVisible(false);
			m_pRightConnect[i]->SetVisible(false);
			m_pOpenLight[i]->SetVisible(false);
		}
		else
		{
			//Msg("connect %d okay\n", i);
			if (!m_pLeftConnect[i]->IsVisible())
				m_pLeftConnect[i]->SetVisible(true);
			if (!m_pRightConnect[i]->IsVisible())
				m_pRightConnect[i]->SetVisible(true);
			if (!m_pOpenLight[i]->IsVisible())
				m_pOpenLight[i]->SetVisible(true);
			
			int left_connect_x = ASW_WIRE_LEFT - ASW_TILE_SIZE * 2;
			int right_connect_x = ASW_WIRE_LEFT + ASW_TILE_SIZE * m_hHack->m_iNumColumns;
			int left_connect_y = i * ASW_SPACE_PER_WIRE + ASW_HEADER_SIZE;
			int right_connect_y = i * ASW_SPACE_PER_WIRE + ASW_HEADER_SIZE;
			if (m_hHack->m_iNumRows == 1)
				left_connect_y += ASW_TILE_SIZE;
			if (m_hHack->m_iNumRows == 3)
				right_connect_y += ASW_TILE_SIZE;
			m_pLeftConnect[i]->SetBounds(left_connect_x, left_connect_y, ASW_TILE_SIZE * 2, ASW_TILE_SIZE * 2);
			m_pRightConnect[i]->SetBounds(right_connect_x, right_connect_y, ASW_TILE_SIZE * 2, ASW_TILE_SIZE * 2);
			m_pLeftConnect[i]->SetDrawColor(Color(255,255,255,255));
			if (!m_hHack->GetTileLit(i+1, m_hHack->m_iNumColumns-1, m_hHack->m_iNumRows-1)
				|| !m_hHack->EndTileConnected(i+1))
				m_pRightConnect[i]->SetDrawColor(Color(64,64,64,255));
			else
				m_pRightConnect[i]->SetDrawColor(Color(255,255,255,255));							

			if (m_hHack->IsWireLit(i+1))
			{
				if (m_pOpenLight[i]->GetDrawColor()[0] != 255)
				{
					// play sound
					CLocalPlayerFilter filter;
					C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWButtonPanel.WireActive" );
				}
				m_pOpenLight[i]->SetDrawColor(Color(255,255,255,255));
			}
			else
				m_pOpenLight[i]->SetDrawColor(Color(32,32,32,255));
		}
	}
	m_pProgressBarBackdrop->SetVisible(true);			
	m_pProgressBar->SetVisible(true);

	float fFraction = 1.0f;

	if (m_hHack.Get())
	{
		fFraction = m_hHack->GetWireCharge();		
	}
	else
		Msg("vgui wire hack panel with no hack object\n");

	if (m_hHack.Get() && m_pFastMarker)
	{
		if (m_hHack->m_fStartedHackTime == 0)
			m_pFastMarker->SetVisible(false);
		else
		{
			m_pFastMarker->SetVisible(true);
			m_pFastMarker->SetSize(BAR_HEIGHT * 0.6f, BAR_HEIGHT * 0.6f);
			float total_time = m_hHack->m_fFastFinishTime - m_hHack->m_fStartedHackTime;
			if (total_time <= 0)
				total_time = 1.0f;
			float fMarkerFraction = (gpGlobals->curtime - m_hHack->m_fStartedHackTime) / total_time;
			if (fFraction >= 1.0f)
			{
				// if we've finished the hack, we should calculate the marker position from the actual finish time, in case
				// the player has switched back into this panel after the hack completed
				fMarkerFraction = (m_hHack->m_fFinishedHackTime - m_hHack->m_fStartedHackTime) / total_time;
			}
			if (fMarkerFraction >= 1.0f)
			{
				fMarkerFraction = 1.0f;
				m_pAccessLoggedLabel->SetFgColor(Color(255,0,0,255));
			}
			if (fMarkerFraction < 0)
				fMarkerFraction = 0;
			//if (fFraction < 1.0f)
				m_pFastMarker->SetPos((GetWide() * 0.5f) - (BAR_WIDTH * 0.5f) + BAR_WIDTH * fMarkerFraction - BAR_HEIGHT * 0.3f,
										BAR_TOP + BAR_PADDING);			

			if (!m_pAccessLoggedLabel->IsVisible() && fMarkerFraction >= 1.0f && fFraction < 1.0f)
			{
				m_pAccessLoggedLabel->SetVisible(true);
			}
		}
	}	
	
	m_pProgressBar->SetSize(BAR_WIDTH * fFraction, BAR_HEIGHT);
	m_pProgressBar->SetPos((GetWide() * 0.5f) - (BAR_WIDTH * 0.5f),
									BAR_TOP + BAR_PADDING);

	bool bFinished = (fFraction >= 1.0f);
	
	m_pCompleteLabel->SetVisible(bFinished);
	if (bFinished && GetAlpha() >= 255)
	{		
		C_BaseEntity *pUsing = m_hHack.Get() ? m_hHack->GetHackTarget() : NULL;
		// check if the player's using the hack still
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pUsing && pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->m_hUsingEntity.Get() == pUsing)
		{
			// if he is, fade us out
			SetAlpha(254);
			vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0.3f, 1.0f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
		else if (GetParent())
		{	
			// otherwise, instadelete!
			SetVisible(false);
			GetParent()->SetVisible(false);
			GetParent()->MarkForDeletion();
		}
	}
	else if (bFinished)		
	{
		if (GetAlpha() <= 0)
		{
			C_BaseEntity *pUsing = m_hHack.Get() ? m_hHack->GetHackTarget() : NULL;
			// once it's faded out, stop the marine using this panel
			C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
			if (pUsing && pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->m_hUsingEntity.Get() == pUsing)
				pPlayer->StopUsing();
			else if (GetParent())
			{	
				GetParent()->SetVisible(false);
				GetParent()->MarkForDeletion();
			}
		}
	}

	if (!bFinished && m_hHack.Get() && GetParent())
	{
		if (m_bLastAllLit != m_hHack->AllWiresLit())
		{
#ifdef SHRINK_WHEN_ALL_WIRES_LIT
			// if we're already minimized, just instantly update to the new shape
			CASW_VGUI_Frame *pFrame = dynamic_cast<CASW_VGUI_Frame*>(GetParent());
			bool bFrameMinimized = pFrame ? pFrame->m_bFrameMinimized : false;
			if (m_hHack->AllWiresLit() && bFrameMinimized)
			{
				InvalidateLayout();
				GetParent()->InvalidateLayout();
				return;
			}
			if (GetAlpha() >= 255)
			{
				GetParent()->SetAlpha(254);
				vgui::GetAnimationController()->RunAnimationCommand(GetParent(), "Alpha", 0, 0.8f, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
				SetAlpha(254);
				vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0.8f, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			}
			else if (GetAlpha() <= 0)
			{
				SetAlpha(1);
				GetParent()->SetAlpha(1);
				vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 255, 0.0f, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
				vgui::GetAnimationController()->RunAnimationCommand(GetParent(), "Alpha", 255, 0.0f, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
				InvalidateLayout();
				GetParent()->InvalidateLayout();
			}
#endif
		}
	}
}

void CASW_VGUI_Hack_Wire_Tile::PerformLayout()
{
	// set our x/y inset inside our container
	m_fScale = ScreenHeight() / 768.0f;
	int border = 10.0f * m_fScale;
	int title_border = 30.0f * m_fScale;
	SetPos(border, title_border);

	// set size according to how many wires we have
	int t = 600;
	int w = 380;
	if (m_hHack.Get())
	{
		w = ASW_TILE_SIZE * (7 + m_hHack->m_iNumColumns);
		m_iSpacePerWire = ASW_TILE_SIZE * 3;
		if (m_hHack->m_iNumRows == 1 || m_hHack->m_iNumRows == 3)
			m_iSpacePerWire = ASW_TILE_SIZE * 4;
		if (m_hHack->AllWiresLit())
			t = ASW_TILE_SIZE * 3;
		else
			t = m_iSpacePerWire * m_hHack->m_iNumWires + ASW_TILE_SIZE * 3;		
	}
	SetSize(w, t);
	SetAlpha(255);
	
	BaseClass::PerformLayout();

	int label_x = ASW_WIRE_LEFT - ASW_TILE_SIZE;
	m_pProgressBarBackdrop->SetSize(BAR_WIDTH + BAR_PADDING * 2, BAR_HEIGHT + BAR_PADDING * 2);
	m_pProgressBarBackdrop->SetPos((GetWide() *0.5f) - ((BAR_WIDTH + BAR_PADDING * 2) * 0.5f),
									BAR_TOP);

	m_pProgressBar->SetSize(0, BAR_HEIGHT);
	m_pProgressBar->SetPos(BAR_X - (BAR_WIDTH),
									BAR_TOP + BAR_PADDING);

	m_pAccessLoggedLabel->SetSize(BAR_WIDTH + BAR_PADDING * 2, BAR_HEIGHT * 1.5f );
	m_pAccessLoggedLabel->SetPos((GetWide() *0.5f) - ((BAR_WIDTH + BAR_PADDING * 2) * 0.5f),
									BAR_TOP + BAR_HEIGHT + BAR_PADDING * 2.0f);

	m_pWireLabel->SetPos(label_x, BAR_TOP + BAR_PADDING - ASW_TILE_SIZE);
	m_pWireLabel->SetSize((BAR_X - (BAR_WIDTH * 0.1f)) - label_x, ASW_TILE_SIZE);

	if (m_hHack.Get())
	{
		m_pCompleteLabel->SetPos(label_x, BAR_Y(m_hHack->m_iNumWires) + ASW_TILE_SIZE * 0.5f);
		m_pCompleteLabel->SetSize((BAR_X - (BAR_WIDTH * 0.5f)) - label_x, BAR_HEIGHT);

		m_bLastAllLit = m_hHack->AllWiresLit();
	}
	
	for (int i=0;i<4;i++)
	{		
		int light_wide = GetWide() * 0.15f;
		m_pOpenLight[i]->SetSize(light_wide, light_wide);
		m_pOpenLight[i]->SetPos(GetWide() - light_wide * 1.2f, BAR_Y(i) +  BAR_HEIGHT + BAR_PADDING * 3 + ASW_TILE_SIZE);
	}
}

void CASW_VGUI_Hack_Wire_Tile::Paint()
{
	BaseClass::Paint();

	int x,y,w,t;
	GetBounds(x,y,w,t);

	if (!m_hHack.Get())
		return;

	for (int w=1;w<=m_hHack->m_iNumWires;w++)
	{
		for (int x=0;x<m_hHack->m_iNumColumns;x++)
		{
			for (int y=0;y<m_hHack->m_iNumRows;y++)
			{
				DrawTile(w, x, y);
			}
		}
		/*
		// draw end pieces	
		int left_connect_x = ASW_WIRE_LEFT - ASW_TILE_SIZE * 2;
		int left_connect_y = (w-1) * ASW_SPACE_PER_WIRE + ASW_HEADER_SIZE;
		vgui::surface()->DrawSetColor(Color(255,255,255,255));
		vgui::surface()->DrawSetTexture(s_nLeftConnect);
		//vgui::surface()->DrawTexturedRect(left_connect_x,left_connect_y,
				//left_connect_x + (ASW_TILE_SIZE * 2), left_connect_y + (ASW_TILE_SIZE * 2));
		vgui::Vertex_t points[4] = 
		{ 
		vgui::Vertex_t( Vector2D(left_connect_x, left_connect_y), Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(left_connect_x + ASW_TILE_SIZE, left_connect_y), Vector2D(1,0) ), 
		vgui::Vertex_t( Vector2D(left_connect_x + ASW_TILE_SIZE, left_connect_y + ASW_TILE_SIZE), Vector2D(1,1) ), 
		vgui::Vertex_t( Vector2D(left_connect_x, left_connect_y + ASW_TILE_SIZE), Vector2D(0,1) ) 
		}; 	
		vgui::surface()->DrawTexturedPolygon( 4, points );

		int right_connect_x = left_connect_x + ASW_TILE_SIZE * m_hHack->m_iNumColumns;
		if (!m_hHack->GetTileLit(w, m_hHack->m_iNumColumns-1, m_hHack->m_iNumRows-1)
			|| !m_hHack->EndTileConnected(w))
			vgui::surface()->DrawSetColor(Color(64,64,64,255));
		vgui::surface()->DrawSetTexture(s_nRightConnect);
		vgui::surface()->DrawTexturedRect(right_connect_x,left_connect_y,
				right_connect_x + (ASW_TILE_SIZE * 2), left_connect_y + (ASW_TILE_SIZE * 2));
		*/
	}

	
}

void CASW_VGUI_Hack_Wire_Tile::DrawTile(int iWire, int x, int y)
{
	//int iTileIndex = m_hHack->m_iNumColumns * y + x;
	// find top left corner of this tile
	int cy = ASW_SPACE_PER_WIRE * (iWire - 1) + ASW_HEADER_SIZE + y * ASW_TILE_SIZE;
	int cx = ASW_TILE_SIZE * x + ASW_WIRE_LEFT;
	
	if (m_hHack->m_iNumRows == 1)
		cy += ASW_TILE_SIZE;

	if (m_pTile[iWire-1][x][y] == NULL)
	{
		CASW_Wire_Tile *pTile = new CASW_Wire_Tile(this, "tile", this);
		if (!pTile)
		{
			Msg("Error, couldn't create tile for drawing\n");
			return;
		}
		pTile->SetActivationType(ImageButton::ACTIVATE_ONPRESSED);
		pTile->AddActionSignalTarget(this);
		KeyValues *msg = new KeyValues("Command");	
		char buffer[40];
		Q_snprintf(buffer, sizeof(buffer), "Tile %d %d %d", iWire, x, y);
		msg->SetString("command", buffer);
		pTile->SetCommand(msg);

		KeyValues *cmsg = new KeyValues("Command");			
		Q_snprintf(buffer, sizeof(buffer), "Cancel");
		cmsg->SetString("command", buffer);
		pTile->SetCancelCommand(cmsg);
		m_pTile[iWire-1][x][y] = pTile;
		
		if (GetControllerFocus())
		{
			GetControllerFocus()->AddToFocusList(m_pTile[iWire-1][x][y], false);
			if (iWire == 1 && x == 0 && y == 0)
			{
				GetControllerFocus()->SetFocusPanel(m_pTile[iWire-1][x][y], false);
			}
		}
	}
	if (m_pTile[iWire-1][x][y] == NULL)
		return;
	m_pTile[iWire-1][x][y]->m_iWire = iWire;
	m_pTile[iWire-1][x][y]->m_x = x;
	m_pTile[iWire-1][x][y]->m_y = y;

	m_pTile[iWire-1][x][y]->SetBounds(cx, cy, ASW_TILE_SIZE, ASW_TILE_SIZE);
}

void CASW_VGUI_Hack_Wire_Tile::PaintBackground()
{
	BaseClass::PaintBackground();
}

void CASW_VGUI_Hack_Wire_Tile::PrecacheHackingTextures()
{
//	s_nBackDropTexture = vgui::surface()->CreateNewTextureID();
	s_nTileHoriz = vgui::surface()->CreateNewTextureID();
	s_nTileLeft = vgui::surface()->CreateNewTextureID();
	s_nTileRight = vgui::surface()->CreateNewTextureID();
	//s_nLeftConnect = vgui::surface()->CreateNewTextureID();
	//s_nRightConnect = vgui::surface()->CreateNewTextureID();
	//vgui::surface()->DrawSetTextureFile( s_nBackDropTexture, "vgui/swarm/Hacking/SwarmDoorHackBackdrop", true, false);
	vgui::surface()->DrawSetTextureFile( s_nTileHoriz, "vgui/swarm/Hacking/TileHoriz", true, false);
	vgui::surface()->DrawSetTextureFile( s_nTileLeft, "vgui/swarm/Hacking/TileLeft", true, false);
	vgui::surface()->DrawSetTextureFile( s_nTileRight, "vgui/swarm/Hacking/TileRight", true, false);
	//vgui::surface()->DrawSetTextureFile( s_nLeftConnect, "vgui/swarm/Hacking/LeftConnect", true, false);
	//vgui::surface()->DrawSetTextureFile( s_nRightConnect, "vgui/swarm/Hacking/RightConnect", true, false);
	s_bLoadedTextures = true;
}

bool CASW_VGUI_Hack_Wire_Tile::CursorOverTile( int x, int y, int &iWire, int &tilex, int &tiley )
{
	// todo: check if we're over a tile.  If so, send a command to the server to rotate it
	int wx, wy;
	wx = wy = 0;
	LocalToScreen(wx, wy);
	int tx = x - wx;
	int ty = y - wy;

	//Msg("tx = %d ty = %d\n", tx, ty);

	if (m_hHack.Get() && ty > ASW_HEADER_SIZE && ty < ASW_HEADER_SIZE + ASW_SPACE_PER_WIRE * m_hHack->m_iNumWires)
	{
		iWire = -1;
		if (ty < ASW_HEADER_SIZE + ASW_SPACE_PER_WIRE)
			iWire = 1;
		else if (ty < ASW_HEADER_SIZE + ASW_SPACE_PER_WIRE * 2)
			iWire = 2;
		else if (ty < ASW_HEADER_SIZE + ASW_SPACE_PER_WIRE * 3)
			iWire = 3;
		else if (ty < ASW_HEADER_SIZE + ASW_SPACE_PER_WIRE *4)
			iWire = 4;
		if (iWire != -1)
		{
			ty -= (iWire - 1) * ASW_SPACE_PER_WIRE;
			ty -= ASW_HEADER_SIZE;
			tx -= ASW_WIRE_LEFT;

			if (m_hHack->m_iNumRows == 1)
				ty -= ASW_TILE_SIZE;

			if (tx < 0 || ty < 0)
				return false;

			tilex = tx / ASW_TILE_SIZE;
			tiley = ty / ASW_TILE_SIZE;

			return true;
		}		
	}

	return false;
}

bool CASW_VGUI_Hack_Wire_Tile::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if (bDown)
		return true;

	int iWire, tilex, tiley;
	if ( CursorOverTile( x, y, iWire, tilex, tiley ) )
	{
		TileClicked(iWire, tilex, tiley);
	}

	return true;	// always swallow clicks in our window
}

void CASW_VGUI_Hack_Wire_Tile::TileClicked(int iWire, int tilex, int tiley)
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer || !m_hHack.Get())
		return;

	// don't allow clicks if the wire is already lit up
	if (m_hHack->IsWireLit(iWire))
		return;

	bool bLocked = m_hHack->GetTileLocked(iWire, tiley*m_hHack->m_iNumColumns + tilex) > 0;
	if (bLocked)
		return;

	if (tilex >=0 && tilex < m_hHack->m_iNumColumns && tiley >= 0 && tiley < m_hHack->m_iNumRows)
	{
		int iOption = tiley * m_hHack->m_iNumColumns + tilex + (m_hHack->m_iNumColumns * m_hHack->m_iNumRows * (iWire -1));
		pPlayer->SelectHackOption(iOption);
		//Msg("pPlayer->SelectHackOption %d\n", iOption);
		
		// predict change
		int iTile = tiley * m_hHack->m_iNumColumns + tilex;
		int iRot = m_hHack->GetTileRotation(iWire, iTile);
		iRot++;
		if (iRot >= 4)
			iRot = 0;
		//bool bWasLit = (m_hHack->GetTileLit(iWire, iTile));
		m_hHack->SetTileRotation(iWire, iTile, iRot);
		//bool bLit = (m_hHack->GetTileLit(iWire, iTile));

		/*
		CLocalPlayerFilter filter;
		if (!bWasLit && bLit)
		{
			C_BaseEntity::EmitSound( filter, -1, "ASWButtonPanel.TileLit" );
		}
		else
		{
			C_BaseEntity::EmitSound( filter, -1, "ASWButtonPanel.TileTurn" );
		}
		*/
		
		CSoundParameters params;
		if ( C_BaseEntity::GetParametersForSound( "ASWComputer.MenuButton", params, NULL ) )
		{
			CLocalPlayerFilter filter;
			EmitSound_t ep;
			ep.m_nChannel = params.channel;
			ep.m_pSoundName =  params.soundname;
			ep.m_flVolume = 1.0f;
			ep.m_SoundLevel = params.soundlevel;
			ep.m_nPitch = params.pitch;

			//if (!bWasLit && bLit)
			//{
				C_BaseEntity::EmitSound( filter, -1, ep );
			//}
			//else	// lower pitch if it's not correct yet
			//{
				//ep.m_nPitch = 85;
				//C_BaseEntity::EmitSound( filter, -1, ep );
			//}
		}
		
	}
}

bool CASW_VGUI_Hack_Wire_Tile::IsCursorOverWireTile( int x, int y )
{
	int iWire, tilex, tiley;
	if ( CursorOverTile( x, y, iWire, tilex, tiley ) )
	{
		if ( m_hHack->IsWireLit( iWire ) )
			return false;

		bool bLocked = m_hHack->GetTileLocked( iWire, tiley * m_hHack->m_iNumColumns + tilex ) > 0;
		if ( bLocked )
			return false;

		if ( tilex >=0 && tilex < m_hHack->m_iNumColumns && tiley >= 0 && tiley < m_hHack->m_iNumRows )
		{
			return true;
		}
	}

	return false;
}

void CASW_VGUI_Hack_Wire_Tile::OnCommand(const char *command)
{
	if (!Q_strnicmp(command, "Tile", 4))
	{
		int wire, x, y;
		if ( sscanf( command+4, "%d %d %d",&wire, &x, &y ) == 3 )
		{
			TileClicked(wire, x, y);
		}
	}	
	else if (!Q_strcmp(command, "Cancel"))
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if (pPlayer)
			pPlayer->StopUsing();
	}
}

CASW_Wire_Tile::CASW_Wire_Tile(vgui::Panel *pParent, const char *name, CASW_VGUI_Hack_Wire_Tile* pHackWireTile) :
	ImageButton(pParent, name, " ")
{
	m_pHackWireTile = pHackWireTile;
	m_iWire = -1;
}

void CASW_Wire_Tile::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBorders("TitleButtonBorder", "TitleButtonBorder");
}

void CASW_Wire_Tile::Paint()
{
	if (!m_pHackWireTile || m_iWire == -1 || !m_pHackWireTile->m_hHack.Get())
		return;

#ifdef SHRINK_WHEN_ALL_WIRES_LIT
	if (m_pHackWireTile->m_bLastAllLit)
		return;
#endif
	bool bLocked = m_pHackWireTile->m_hHack->GetTileLocked(m_iWire, (m_y * m_pHackWireTile->m_hHack->m_iNumColumns) + m_x) > 0;

	float m_fScale = m_pHackWireTile->m_fScale;
	// set up vectors for the 4 points of this tile
	Vector vTopLeft(-ASW_TILE_SIZE * 0.5f,-ASW_TILE_SIZE * 0.5f,0);
	Vector vTopRight(ASW_TILE_SIZE * 0.5f,-ASW_TILE_SIZE * 0.5f,0);
	Vector vBottomRight(ASW_TILE_SIZE * 0.5f,ASW_TILE_SIZE * 0.5f,0);
	Vector vBottomLeft(-ASW_TILE_SIZE * 0.5f,ASW_TILE_SIZE * 0.5f,0);

	// rotate the points by the angle of the tile
	Vector vTopLeft_rotated;
	Vector vTopRight_rotated;
	Vector vBottomRight_rotated;
	Vector vBottomLeft_rotated;
	QAngle angTileRot(0, m_pHackWireTile->m_hHack->GetTileRotation(m_iWire, m_x, m_y) * 90.0f, 0);
	if (bLocked)
	{
		vTopLeft_rotated = vTopLeft;
		vTopRight_rotated = vTopRight;
		vBottomRight_rotated = vBottomRight;
		vBottomLeft_rotated = vBottomLeft;
	}
	else
	{
		VectorRotate(vTopLeft, angTileRot, vTopLeft_rotated);
		VectorRotate(vTopRight, angTileRot, vTopRight_rotated);
		VectorRotate(vBottomRight, angTileRot, vBottomRight_rotated);
		VectorRotate(vBottomLeft, angTileRot, vBottomLeft_rotated);
	}

	int cx = ASW_TILE_SIZE * 0.5f;
	int cy = ASW_TILE_SIZE * 0.5f;

	vgui::Vertex_t points[4] = 
	{ 
	vgui::Vertex_t( Vector2D(cx + vTopLeft_rotated.x, cy + vTopLeft_rotated.y), Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(cx + vTopRight_rotated.x, cy + vTopRight_rotated.y), Vector2D(1,0) ), 
	vgui::Vertex_t( Vector2D(cx + vBottomRight_rotated.x, cy + vBottomRight_rotated.y), Vector2D(1,1) ), 
	vgui::Vertex_t( Vector2D(cx + vBottomLeft_rotated.x, cy + vBottomLeft_rotated.y), Vector2D(0,1) ) 
	}; 		
	
	// draw locked tiles in red
	if (bLocked)
	{
		if (m_nLockedTexture != -1)
		{
			vgui::surface()->DrawSetColor(Color(255,255,255,255));
			vgui::surface()->DrawSetTexture(m_nLockedTexture);
			vgui::surface()->DrawTexturedPolygon( 4, points );
		}
		return;	// we're not showing you anything if it's locked!
	}
	else
	{
		// draw highlight if we're over any of them
		if (IsCursorOver() && !m_pHackWireTile->m_hHack->IsWireLit(m_iWire))
		{
			if (m_nWhiteTexture != -1)
			{
				vgui::surface()->DrawSetColor(Color(65,74,96,128));
				vgui::surface()->DrawSetTexture(m_nWhiteTexture);
				vgui::surface()->DrawTexturedPolygon( 4, points );
			}
		}
	}

	// draw the actual tile
	if (m_pHackWireTile->m_hHack->GetTileLit(m_iWire, m_x, m_y))
		vgui::surface()->DrawSetColor(Color(255, 255, 255, 255));
	else
		vgui::surface()->DrawSetColor(Color(64, 64, 64, 255));
	
	// get the texture for this type of tile
	if (m_pHackWireTile->m_hHack->GetTileType(m_iWire, m_x, m_y) == ASW_WIRE_TILE_HORIZ)
		vgui::surface()->DrawSetTexture(m_pHackWireTile->s_nTileHoriz);
	else if (m_pHackWireTile->m_hHack->GetTileType(m_iWire, m_x, m_y) == ASW_WIRE_TILE_LEFT)
		vgui::surface()->DrawSetTexture(m_pHackWireTile->s_nTileLeft);
	else if (m_pHackWireTile->m_hHack->GetTileType(m_iWire, m_x, m_y) == ASW_WIRE_TILE_RIGHT)
		vgui::surface()->DrawSetTexture(m_pHackWireTile->s_nTileRight);
	else
		return;

	// draw it
	vgui::surface()->DrawTexturedPolygon( 4, points );
}

// ================================== Container ==================================================

CASW_VGUI_Hack_Wire_Tile_Container::CASW_VGUI_Hack_Wire_Tile_Container(Panel *parent, const char *panelName, C_ASW_Hack_Wire_Tile* pHack) :
	CASW_VGUI_Frame(parent, panelName, "#asw_maint_panel")
{
	m_hHack = pHack;
}

CASW_VGUI_Hack_Wire_Tile_Container::~CASW_VGUI_Hack_Wire_Tile_Container()
{
}

void CASW_VGUI_Hack_Wire_Tile_Container::PerformLayout()
{
	BaseClass::PerformLayout();

	float m_fScale = ScreenHeight() / 768.0f;
	int border = 10.0f * m_fScale;
	int title_border = 30.0f * m_fScale;

	// calculate size of our child hack panel
	int childw = 600;
	int childt = 380;
	if (m_hHack.Get())
	{
		childw = ASW_TILE_SIZE * (7 + m_hHack->m_iNumColumns);
		if (
#ifdef SHRINK_WHEN_ALL_WIRES_LIT
			m_hHack->AllWiresLit() || 
#endif
			m_bFrameMinimized )
		{
			childt = ASW_TILE_SIZE * 3;
		}
		else
		{
			int iSpacePerWire = ASW_TILE_SIZE * 3;
			if (m_hHack->m_iNumRows == 1 || m_hHack->m_iNumRows == 3)
				iSpacePerWire = ASW_TILE_SIZE * 4;
			childt = iSpacePerWire * m_hHack->m_iNumWires + ASW_TILE_SIZE * 3;		
		}
	}
	
	// resize container to encompass it with a border	
	int w = border + childw + border;
	int h = title_border + childt + border;

	if (m_bFrameMinimized)
	{
		w = m_fScale * 0.3f * 1024.0f;
		vgui::HFont title_font = m_pTitleLabel->GetFont();
		int title_tall = vgui::surface()->GetFontTall(title_font);
		h = title_tall * 1.2f;
		SetBounds((ScreenWidth() * 0.5f) - (w * 0.5f), (ScreenHeight() * 0.08f), 
									w , h);
	}
#ifdef SHRINK_WHEN_ALL_WIRES_LIT
	else if (m_hHack->AllWiresLit())
	{
		SetBounds((ScreenWidth() * 0.5f) - (w * 0.5f), (ScreenHeight() * 0.1), 
									w , h);
	}
#endif
	else
	{
		SetBounds((ScreenWidth() * 0.5f) - (w * 0.5f), (ScreenHeight() * 0.5f) - (h * 0.5f), 
									w , h);
	}
}

void CASW_VGUI_Hack_Wire_Tile_Container::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);
}

void CASW_VGUI_Hack_Wire_Tile_Container::OnThink()
{
	BaseClass::OnThink();

	float fDeathCamInterp = ASWGameRules()->GetMarineDeathCamInterp();

	if ( fDeathCamInterp > 0.0f && GetAlpha() >= 255 )
	{
		SetAlpha( 254 );
		vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 0, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
	else if ( fDeathCamInterp <= 0.0f && GetAlpha() <= 0 )
	{
		SetAlpha( 1 );
		vgui::GetAnimationController()->RunAnimationCommand(this, "Alpha", 255, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}
}