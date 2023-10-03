#include "cbase.h"
#include "asw_hud_3dmarinenames.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#define PAIN_NAME "sprites/%d_pain.vmt"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

#include <filesystem.h>
#include <keyvalues.h>

#include "asw_hudelement.h"
#include "hud_numericdisplay.h"

#include "c_asw_door.h"
#include "c_asw_game_resource.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "c_asw_use_area.h"
#include "asw_weapon_medical_satchel_shared.h"
#include "ConVar.h"
#include "tier0/vprof.h"
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "iasw_client_vehicle.h"
#include "c_playerresource.h"
#include "datacache/imdlcache.h"
#include "asw_util_shared.h"
#include "asw_hud_objective.h"
#include "voice_status.h"
#include "cdll_bounded_cvars.h"
#include "asw_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_draw_hud;
extern ConVar asw_hud_alpha;
extern ConVar asw_DebugAutoAim;
extern ConVar asw_fast_reload_enabled;

ConVar asw_voice_side_icon("asw_voice_side_icon", "0", FCVAR_CHEAT, "Set to 1 to use the voice indicators on the side of the screen instead of the ones next to the 3d player names");
ConVar asw_marine_names("asw_marine_names", "1", FCVAR_NONE, "Whether to show the marine name");
ConVar asw_player_names("asw_player_names", "1", FCVAR_NONE, "Whether to show player names under marines or not.  Set to 2 to show both player and marine name.");
ConVar asw_marine_edge_names("asw_marine_edge_names", "1", FCVAR_NONE, "Prevent marine names from going off the edge of the screen");
ConVar asw_world_healthbars("asw_world_healthbars", "1", FCVAR_NONE, "Shows health bars in the game world");
ConVar asw_world_usingbars("asw_world_usingbars", "1", FCVAR_NONE, "Shows using bars in the game world");
ConVar asw_marine_labels_cursor_maxdist( "asw_marine_labels_cursor_maxdist", "70", FCVAR_NONE, "Only marines within this distance of the cursor will get their health bar drawn" );
ConVar asw_fast_reload_under_marine( "asw_fast_reload_under_marine", "0", FCVAR_NONE, "Draw the active reload bar under the marine?" );
ConVar asw_world_healthbar_class_icon( "asw_world_healthbar_class_icon", "0", FCVAR_NONE, "Show class icon on mouse over" );

#define ASW_MAX_MARINE_NAMES 8
#define ASW_MIN_MARINE_ARROW_SIZE 20
#define ASW_MAX_MARINE_ARROW_SIZE 60
#define ASW_MAX_MARINE_OFFSCREEN_START_DISTANCE 600.0f
#define ASW_MAX_MARINE_OFFSCREEN_END_DISTANCE 1800.0f

//-----------------------------------------------------------------------------
// Purpose: Shows the marines names drawn on top of the 3D view
//-----------------------------------------------------------------------------

CASWHud3DMarineNames::CASWHud3DMarineNames( const char *pElementName ) : vgui::Panel( GetClientMode()->GetViewport(), "ASWHud3DMarineNames" ), CASW_HudElement( "ASWHud3DMarineNames" )
{
	SetHiddenBits( HIDEHUD_REMOTE_TURRET );
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);

	for( int i = 0; i < ASW_MAX_PLAYERS; i++ )
	{
		m_flLastTalkingTime[i] = 0;
	}
	m_3DSpeakingList.ClearAll();
}

CASWHud3DMarineNames::~CASWHud3DMarineNames()
{

}

DECLARE_HUDELEMENT( CASWHud3DMarineNames );

void CASWHud3DMarineNames::Init()
{
	Reset();
}

void CASWHud3DMarineNames::Reset()
{	
	SetPaintBackgroundEnabled(false);

	m_hHealthQueuedMarine = NULL;
	m_hHealthMarine = NULL;
	m_bHealthQueuedMarine = false;
	m_fHealthAlpha = 0;

	for( int i = 0; i < ASW_MAX_PLAYERS; i++ )
	{
		m_flLastTalkingTime[i] = 0;
	}
	m_3DSpeakingList.ClearAll();

	m_flLastReloadProgress = 0;
	m_flLastNextAttack = 0;
	m_flLastFastReloadStart = 0;
	m_flLastFastReloadEnd = 0;
}

void CASWHud3DMarineNames::VidInit()
{
	Reset();
}

void CASWHud3DMarineNames::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0,0,0,0));
	//m_hMarineNameFont = ASW_GetDefaultFont(false);
}

bool CASWHud3DMarineNames::ShouldDraw( void )
{
	// Undone! -Jeep
	//CASWHudObjective *pObjectives = GET_HUDELEMENT( CASWHudObjective );

	//if ( pObjectives && pObjectives->GetBgColor().a() != 0 )
	//{
	//	// Don't draw this stuff while we're showing objectives in the center!
	//	return false;
	//}

	return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw();
}
void CASWHud3DMarineNames::OnThink()
{
	BaseClass::OnThink();
	UpdateHealthTooltip();
}

void CASWHud3DMarineNames::Paint()
{
	VPROF_BUDGET( "CASWHud3DMarineNames::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );

	BaseClass::Paint();
	//PaintFontTest();
	MDLCACHE_CRITICAL_SECTION();
	PaintMarineNameLabels();
	PaintBoxesAroundUseEntities();
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		/*
		// check for drawing autoaim crosshair
		if (pPlayer->m_ASWLocal.m_hAutoAimTarget.Get() && pPlayer->GetMarine())
		{
			C_ASW_Marine *pMarine = pPlayer->GetMarine();
			C_ASW_Marine_Resource *pMR = pMarine->GetMarineResource();
			if (pMR && pMR->IsFiring())
			{
				C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
				if (pWeapon->IsOffensiveWeapon())
					PaintAutoaimCrosshairOn(pPlayer->m_ASWLocal.m_hAutoAimTarget.Get());
			}			
		}
		*/
		if (pPlayer->GetHighlightEntity())
		{
			PaintBoxAround(pPlayer->GetHighlightEntity(), 6);
		}
	}

	if ( ASWInput() && ASWInput()->GetAutoaimEntity() )
	{
		PaintAutoaimCrosshairOn( ASWInput()->GetAutoaimEntity() );
	}

	PaintTrackedHealth();
	if ( asw_DebugAutoAim.GetBool() )
	{
		PaintAimingDebug();
	}

}

/// simple convenience for printing unformatted text with a drop shadow
void CASW_HudElement::DrawColoredTextWithDropShadow( const vgui::HFont &font, int x, int y, int r, int g, int b, int a, char *fmt )
{
	g_pMatSystemSurface->DrawColoredText( font, x+1, y+1, 0, 0, 0, a, fmt );
	g_pMatSystemSurface->DrawColoredText( font, x+0, y+0, r, g, b, a, fmt );
}

void CASWHud3DMarineNames::PaintAutoaimCrosshairOn(C_BaseEntity *pEnt)
{
	if (!pEnt)
		return;

	Vector pos = (pEnt->WorldSpaceCenter() - pEnt->GetAbsOrigin()) + pEnt->GetRenderOrigin();
	Vector screenPos;
	debugoverlay->ScreenPosition( pos, screenPos );

	surface()->DrawSetColor(Color(255,255,255,255));
	surface()->DrawSetTexture(m_nAutoaimCrosshairTexture);

	int xhair_size = 26.0f * (ScreenHeight() / 768.0f);

	Vertex_t points[4] = 
	{ 
	Vertex_t( Vector2D(screenPos.x - xhair_size, screenPos.y - xhair_size), Vector2D(0,0) ), 
	Vertex_t( Vector2D(screenPos.x + xhair_size, screenPos.y - xhair_size), Vector2D(1,0) ), 
	Vertex_t( Vector2D(screenPos.x + xhair_size, screenPos.y + xhair_size), Vector2D(1,1) ), 
	Vertex_t( Vector2D(screenPos.x - xhair_size, screenPos.y + xhair_size), Vector2D(0,1) ) 
	}; 
	surface()->DrawTexturedPolygon( 4, points );
}

void CASWHud3DMarineNames::PaintBoxAround(C_BaseEntity* pEnt, int padding)
{
	// find the volume this entity takes up
		Vector mins, maxs;
		//mins = pEnt->WorldAlignMins() + pEnt->GetAbsOrigin();
		//maxs = pEnt->WorldAlignMaxs() + pEnt->GetAbsOrigin();
		pEnt->GetRenderBoundsWorldspace(mins,maxs);


		// pull out all 8 corners of this volume
		Vector worldPos[8];
		Vector screenPos[8];
		worldPos[0] = mins;
		worldPos[1] = mins; worldPos[1].x = maxs.x;
		worldPos[2] = mins; worldPos[2].y = maxs.y;
		worldPos[3] = mins; worldPos[3].z = maxs.z;
		worldPos[4] = mins;
		worldPos[5] = maxs; worldPos[5].x = mins.x;
		worldPos[6] = maxs; worldPos[6].y = mins.y;
		worldPos[7] = maxs; worldPos[7].z = mins.z;

		// convert them to screen space
		for (int k=0;k<8;k++)
		{
			debugoverlay->ScreenPosition( worldPos[k], screenPos[k] );
		}

		// find the rectangle bounding all screen space points
		Vector topLeft = screenPos[0];
		Vector bottomRight = screenPos[0];
		for (int k=0;k<8;k++)
		{
			topLeft.x = MIN(screenPos[k].x, topLeft.x);
			topLeft.y = MIN(screenPos[k].y, topLeft.y);
			bottomRight.x = MAX(screenPos[k].x, bottomRight.x);
			bottomRight.y = MAX(screenPos[k].y, bottomRight.y);
		}
		int BracketSize = padding;	// todo: set by screen res?

		// pad it a bit
		topLeft.x -= BracketSize * 2;
		topLeft.y -= BracketSize * 2;
		bottomRight.x += BracketSize * 2;
		bottomRight.y += BracketSize * 2;

		// draw it
		vgui::surface()->DrawSetColor(Color(255,255,255,192));
		
		vgui::surface()->DrawLine(topLeft.x, topLeft.y, topLeft.x + BracketSize, topLeft.y);
		vgui::surface()->DrawLine(topLeft.x, topLeft.y, topLeft.x, topLeft.y + BracketSize);
		vgui::surface()->DrawLine(topLeft.x, bottomRight.y, topLeft.x + BracketSize, bottomRight.y);
		vgui::surface()->DrawLine(topLeft.x, bottomRight.y, topLeft.x, bottomRight.y - BracketSize);
		vgui::surface()->DrawLine(bottomRight.x, topLeft.y, bottomRight.x - BracketSize, topLeft.y);
		vgui::surface()->DrawLine(bottomRight.x, topLeft.y, bottomRight.x, topLeft.y + BracketSize);
		vgui::surface()->DrawLine(bottomRight.x, bottomRight.y, bottomRight.x, bottomRight.y - BracketSize);
		vgui::surface()->DrawLine(bottomRight.x, bottomRight.y, bottomRight.x - BracketSize, bottomRight.y);
}

void CASWHud3DMarineNames::PaintBoxesAroundUseEntities()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer || !pPlayer->GetMarine() || pPlayer->GetMarine()->GetHealth() <= 0 )
		return;

	C_BaseEntity *pEnt = NULL;

	if ( pPlayer->GetNumUseEntities() > 0 )
	{
		pEnt = pPlayer->GetUseEntity();
		if ( !pEnt )
		{
			pEnt = pPlayer->GetUseEntity( 0 );
		}

		IASW_Client_Usable_Entity* pUsable = dynamic_cast< IASW_Client_Usable_Entity* >( pEnt );
		if ( !pUsable || !pUsable->ShouldPaintBoxAround() )
		{
			pEnt = NULL;
		}
	}

	if ( pEnt )
	{
		C_ASW_Use_Area *pArea = dynamic_cast< C_ASW_Use_Area* >( pEnt );
		if ( pArea )
		{
			if ( pArea->GetGlowEntity() )
			{
				ASWInput()->SetUseGlowEntity( pArea->GetGlowEntity() );
			}
			else
			{
				PaintBoxAround( pEnt, 6 );
			}
		}
		else
		{
			ASWInput()->SetUseGlowEntity( pEnt );
		}
	}
	else
	{
		ASWInput()->SetUseGlowEntity( NULL );
	}
}

void CASWHud3DMarineNames::PaintMarineNameLabels()
{
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if ( !local )
		return;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	int count = 0;
	int my_count = 0;
	for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
	{
		C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
		if ( pMR && pMR->GetHealthPercent() > 0 )
		{
			C_ASW_Marine *pMarine = pMR->GetMarineEntity();
			if ( pMarine )
			{
				if ( pMarine->GetCommander() == local )
				{
					PaintMarineLabel( my_count, pMarine, pMR, local->GetMarine() == pMarine );
					my_count++;
				}
				else
				{
					PaintMarineLabel( -1, pMarine, pMR, false );
				}

				count++;
			}
		}		
	}
}


/// function: round UP to the specified power of 2
template < unsigned int N >
int RoundUpToPowerOfTwo( int x )
{
	const int mask = ( 1 << N ) - 1; // known at compile time
	// branchless
	return ( ( x - 1 ) & ( ~mask ) ) + ( 1 << N );

	// branchy:
	/*
	return ( x & mask ) != 0    ? 
	( x & ~mask ) + ( 1 << N ) :
	x;
	*/
}

/// for the case where the marine's label is outside
/// the bounds of the screen, compute clipping coordinates in a way
/// that properly preserves directionality
static Vector ComputeClippedMarineLabelCoordinates( float xPos, float yPos,
												   int screenMinX, int screenMaxX, 
												   int screenMinY, int screenMaxY )
{
	int x_edge = 0;
	int y_edge = 0;

	int screenWidth = screenMaxX - screenMinX;
	int screenHeight = screenMaxY - screenMinY;
	int screenhalfx = ( screenWidth  / 2 );
	int screenhalfy = ( screenHeight / 2 );
	// the name is off the edge, calculate a better position to draw at than clipped (which changes direction)
	int diffx = xPos - screenhalfx;	// grab the direction of the marine
	int diffy = yPos - screenhalfy;

	// find intersection point with the right edge of the screen
	bool bFound = false;
	if (diffx != 0)
	{
		float yxratio = float(diffy) / float(diffx);
		int right_x = screenhalfx;
		int right_y = yxratio * right_x;

		if (right_y >= -screenhalfy && right_y <= screenhalfy)
		{
			// we've found a valid side edge to use
			if (diffx > 0)
			{
				xPos = screenhalfx + right_x;
				yPos = screenhalfy + right_y;
				x_edge = 1;
			}
			else
			{
				xPos = screenhalfx - right_x;
				yPos = screenhalfy - right_y;
				x_edge = 1;
			}
			bFound = true;
		}
	}

	if (!bFound)				
	{
		// find the intersection point with the top edge
		if (diffy != 0)
		{
			float xyratio = float(diffx) / float(diffy);						
			int top_y = screenhalfy;
			int top_x = xyratio * top_y;

			if (top_x >= -screenhalfx && top_x <= screenhalfx)
			{
				// we've found a valid top/bottom edge to use
				if (diffy > 0)
				{
					xPos = screenhalfx + top_x;
					yPos = screenhalfy + top_y;
					y_edge = 1;
				}
				else
				{
					xPos = screenhalfx - top_x;
					yPos = screenhalfy - top_y;
					y_edge = -1;
				}
				bFound = true;
			}
		}
	}
	return Vector( xPos, yPos, 0 );
}


void CASWHud3DMarineNames::PaintMarineLabel( int iMyMarineNum, C_ASW_Marine * RESTRICT pMarine, C_ASW_Marine_Resource * RESTRICT pMR, bool bLocal )
{	
	VPROF( "CASWHud3DMarineNames::PaintMarineLabel" );

	if ( !pMarine )
		return;

	C_ASW_Player *pLocal = C_ASW_Player::GetLocalASWPlayer();
	if ( !pLocal )
		return;
	
	// Verify we have input.
	Assert( ASWInput() != NULL );

	wchar_t wszMarineName[ ASW_MAX_PLAYER_NAME_LENGTH_3D ];
	pMR->GetDisplayName( wszMarineName, sizeof( wszMarineName ) );

	/*
	// make sure name is not too long
	wchar_t wszMarineName[ASW_MAX_PLAYER_NAME_LENGTH_3D];
	Q_wcsncpy( wszMarineName, wszMarineNameLong, sizeof( wszMarineName ) );
	*/

	CASW_Marine_Profile *pProfile = pMarine->GetMarineProfile();
	if ( !pProfile )
		return;

	const wchar_t *pwszMarineProfileName = NULL;
	
	if ( asw_player_names.GetInt() == 2 && pMarine->IsInhabited() && gpGlobals->maxClients > 1 )
	{
		pwszMarineProfileName = g_pVGuiLocalize->Find( pProfile->GetShortName() );
	}

	if ( pMarine->entindex() == ASWInput()->ASW_GetOrderingMarine() )
	{
		PaintBoxAround( pMarine, 6 );
	}
	
	// find the color to use
	Color nMarineTextColor(255,255,255,255);
	if ( bLocal )	// yellow if currently selected marine
	{
		nMarineTextColor[2] = 0;
	}
	else if ( pMR->IsInhabited() )		// other player's inhabited marines go blue
	{
		nMarineTextColor.SetColor( 0, 144, 188, 255 );
	}

	// if marine is injured pulse it red	
	float diffr = ( 255.0f - nMarineTextColor.r() ) * pMarine->m_fRedNamePulse;
	float diffg = (   0.0f - nMarineTextColor.g() ) * pMarine->m_fRedNamePulse;
	float diffb = (   0.0f - nMarineTextColor.b() ) * pMarine->m_fRedNamePulse;
	nMarineTextColor[0] += diffr;
	nMarineTextColor[1] += diffg;
	nMarineTextColor[2] += diffb;

	// find the position of this marine
	Vector screenPos;
	Vector vMarinePos = pMarine->GetRenderOrigin();
	//bool bMarineIsKnockedOut = pMarine->m_hKnockedOutRagdoll.Get() || pMarine->IsIncap() ;
	bool bMarineIsKnockedOut = false;
	if ( pMarine->IsInVehicle() && pMarine->GetASWVehicle() && pMarine->GetASWVehicle()->GetEntity() )
	{
		vMarinePos = pMarine->GetASWVehicle()->GetEntity()->GetAbsOrigin();
		if ( gpGlobals->maxClients>1 && pMarine->GetClientsideVehicle() && pMarine->GetClientsideVehicle()->GetEntity() )
		{
			vMarinePos = pMarine->GetClientsideVehicle()->GetEntity()->GetAbsOrigin();		
		}
	}
	//if ( bMarineIsKnockedOut )
	//{
	//	CBaseEntity *pRagdoll = pMarine->m_hKnockedOutRagdoll.Get();
	//	MarinePos = pRagdoll ? pRagdoll->GetAbsOrigin() : pMarine->GetAbsOrigin() ;
	//}

	Vector vecCameraFocus;
	QAngle ang;
	int omx, omy;
	ASWInput()->ASW_GetCameraLocation( pLocal, vecCameraFocus, ang, omx, omy, false);

	float flMarineDistanceFromCamera = vecCameraFocus.DistTo( vMarinePos );

	// if marine is behind the player, convert it to a shortened vector, so it doesn't go behind the plane of the camera
	if ( !!debugoverlay->ScreenPosition( vMarinePos - Vector( 0.0f, 10.0f, 0.0f ), screenPos ) )
	{
		Vector offset;
		AngleVectors( ang, &offset );
		if ( ::input->CAM_IsThirdPerson() )
		{
			offset *= ASWInput()->ASW_GetCameraDist();
		}

		vecCameraFocus += offset;
		Vector dir = vMarinePos - vecCameraFocus;
		VectorNormalize( dir );
		vMarinePos = vecCameraFocus + dir * 500;
	}

	if ( !debugoverlay->ScreenPosition( vMarinePos - Vector(0,10,0), screenPos ) )
	{
		const int nMaxX = ScreenWidth() - YRES( 50 );
		const int nMaxY = ScreenHeight() - YRES( 75 );
		const int nMinX = 0 + YRES( 50 );
		const int nMinY = 0 + YRES( 25 );
		// we're going to draw a box. The box contains some combination of these elements:
		/*
		+-----+
		| _   |
		||\   | 
		|  \  |
		|   \ |
		+-----+
		+-------------+
		| MARINE NAME |
		+-------------+
		[ health bar  ]
		[ use bar     ]
		+-------------+
		*/
		// the presence or absence and size of these elements depends on a variety of factors.
		// first, is the marine on screen?
		bool bMarineOnScreen = ( screenPos.x  >= 0 ) && ( screenPos.x <= nMaxX ) &&
			( screenPos.y  >= 0 ) && ( screenPos.y <= nMaxY ) ;

		// COPYPASTA: if the marine isn't on screen, compute an appropriate screen point to use
		// (don't just clip to screen coords, which makes direction change inappropriately)
		if ( !bMarineOnScreen )
		{
			screenPos = ComputeClippedMarineLabelCoordinates( screenPos.x, screenPos.y,
				nMinX, nMaxX, nMinY, nMaxY );
		}

		// compute the size of the arrow. it draws only if a marine is off the screen.
		// if the marine is alive, it's 2/57.6 the height of the screen. Otherwise, it is
		// 6/57.6.  It is encapsulated in a square.
		int iArrowSize = 0;

		if ( !bMarineOnScreen )
		{
			int iArrowScale = ASW_MAX_MARINE_ARROW_SIZE;

			if ( !bMarineIsKnockedOut )
			{
				float flInterpArrowScale = 0.0f;

				if ( flMarineDistanceFromCamera > ASW_MAX_MARINE_OFFSCREEN_END_DISTANCE )
				{
					flInterpArrowScale = 1.0f;
				}
				else if ( flMarineDistanceFromCamera > ASW_MAX_MARINE_OFFSCREEN_START_DISTANCE )
				{
					flInterpArrowScale = ( flMarineDistanceFromCamera - ASW_MAX_MARINE_OFFSCREEN_START_DISTANCE ) / ( ASW_MAX_MARINE_OFFSCREEN_END_DISTANCE - ASW_MAX_MARINE_OFFSCREEN_START_DISTANCE );
				}

				iArrowScale = ASW_MIN_MARINE_ARROW_SIZE + flInterpArrowScale * ( ASW_MAX_MARINE_ARROW_SIZE - ASW_MIN_MARINE_ARROW_SIZE );
			}

			iArrowSize = iArrowScale * nMaxY / 576;
			iArrowSize = RoundUpToPowerOfTwo<1>(iArrowSize);
		}

		// compute the size of the health bar.
		int nHealthBarWidth  = 0;
		int nHealthBarHeight = 0;

		if ( asw_world_healthbars.GetBool() && bMarineOnScreen )		//  || !bMarineOnScreen
		{
			// only draw health bars for marines near the cursor or their health is low/healing
			int idx = ASWGameResource()->GetMarineCrosshairCache()->FindIndexForMarine( pMarine );
			if ( //!bMarineOnScreen ||
				 ( idx >= 0 && ASWGameResource()->GetMarineCrosshairCache()->GetElement(idx).m_fDistToCursor < asw_marine_labels_cursor_maxdist.GetFloat() ) || 
				 pMR->GetHealthPercent() < 0.4f || pMR->IsInfested() || pMarine->m_fLastHealTime + 1.0f > gpGlobals->curtime )
			{
				nHealthBarWidth  = GetHealthBarMaxWidth( bMarineIsKnockedOut );
				nHealthBarHeight = MAX( GetClassIconSize( bMarineOnScreen ), GetHealthBarMaxHeight( bMarineIsKnockedOut ) );
			}
		}

		// compute the size of the using bar.
		int nUsingBarWidth  = 0;
		int nUsingBarHeight = 0;  
		if ( asw_world_usingbars.GetBool() && bMarineOnScreen && !bLocal && GetUsingFraction( pMarine ) > 0 )
		{
			nUsingBarWidth  = GetUsingBarMaxWidth();
			nUsingBarHeight = GetUsingBarMaxHeight();
		}

		// compute the height and width of the marine's name.
		const vgui::HFont nMarineFont = bMarineOnScreen ? m_hMarineNameFont : m_hSmallMarineNameFont;

		int nMarineNameWidth = 0, nMarineNameHeight = 0;
		int nMarineProfileNameWidth = 0, nMarineProfileNameHeight = 0;
		if ( asw_marine_names.GetBool() )
		{
			g_pMatSystemSurface->GetTextSize( nMarineFont, wszMarineName, nMarineNameWidth, nMarineNameHeight );
		}
		if ( pwszMarineProfileName && bMarineOnScreen )
		{
			g_pMatSystemSurface->GetTextSize( m_hSmallMarineNameFont, pwszMarineProfileName, nMarineProfileNameWidth, nMarineProfileNameHeight );
		}

		// this is the vertical padding between the arrow and the marine name, or the marine name and the player name
		int nLineSpacing = nMarineNameHeight >> 3;

		// compute the total height of the label box
#define PAD(x) ((x) + isel((x), nLineSpacing, 0))
		int nTotalBoxHeight = PAD( iArrowSize ) +
			PAD( nHealthBarHeight ) + 
			PAD( nUsingBarHeight ) + 
			nMarineNameHeight + 
			nMarineProfileNameHeight;
		int nTotalBoxWidth = MAX( nMarineNameWidth, MAX( iArrowSize, nHealthBarWidth ) );
		nTotalBoxWidth = MAX( nMarineProfileNameWidth, nTotalBoxWidth );
#undef PAD

		// round the figures *UP* to the nearest multiple of 2
		nTotalBoxHeight = RoundUpToPowerOfTwo<1>(nTotalBoxHeight); // ( nTotalBoxHeight & 1 ) ? nTotalBoxHeight + 1 : nTotalBoxHeight;
		nTotalBoxWidth  = RoundUpToPowerOfTwo<1>(nTotalBoxWidth);  // ( nTotalBoxWidth  & 1 ) ? nTotalBoxWidth  + 1 : nTotalBoxWidth ;
		Assert( nTotalBoxHeight < nMaxY );
		Assert( nTotalBoxWidth < nMaxX );

		// now, determine where to draw the box. we would like it to be centered on the marine,
		// but if necessary we'll push it in from the sides 
		int nBoxCenterX = screenPos.x;
		int nBoxCenterY = screenPos.y + nTotalBoxHeight/2;

		// if off to the left, push to the right
		if ( nBoxCenterX - ( nTotalBoxWidth / 2 ) < nMinX )
		{
			nBoxCenterX -= nBoxCenterX - ( nTotalBoxWidth / 2 ) - nMinX;
		}
		else if ( nBoxCenterX + ( nTotalBoxWidth / 2 ) >= nMaxX )
			// if off to the right, push to the left
		{
			nBoxCenterX -= nBoxCenterX + ( nTotalBoxWidth / 2 ) - nMaxX;
		}

		// if too high, push down
		if ( nBoxCenterY - ( nTotalBoxHeight / 2 ) < nMinY )
		{
			nBoxCenterY -= nBoxCenterY - ( nTotalBoxHeight / 2 ) - nMinY;
		}
		else if ( nBoxCenterY + ( nTotalBoxHeight / 2 ) > nMaxY )
		{	// if too high, push up
			nBoxCenterY -= nBoxCenterY + ( nTotalBoxHeight / 2 ) - nMaxY;
		}

		// compute the top left, top right, bot left, and bot right coords for convenience
		// int nBoxLeftExtent  = nBoxCenterX - ( nTotalBoxWidth  / 2 );
		// int nBoxRightExtent = nBoxCenterX + ( nTotalBoxWidth  / 2 );
		int nBoxTopExtent   = nBoxCenterY - ( nTotalBoxHeight / 2 );
		// int nBoxBotExtent   = nBoxCenterY + ( nTotalBoxHeight / 2 );

		/////////////////////////////// 
		// now draw from top to bottom
		///////////////////////////////
		int nCursorY = nBoxTopExtent;
		if ( iArrowSize > 0 )
		{
			int iArrowHalfSize = iArrowSize / 2;
			// draw a red pointing, potentially blinking, arrow
			Vector vecFacing(screenPos.x - (ScreenWidth() * 0.5f), screenPos.y - (ScreenHeight() * 0.5f), 0);
			float fFacingYaw = -UTIL_VecToYaw(vecFacing);
			Vector2D vArrowCenter( nBoxCenterX , nCursorY + iArrowHalfSize );

			Vector vecCornerTL(-iArrowHalfSize, -iArrowHalfSize, 0);
			Vector vecCornerTR(iArrowHalfSize, -iArrowHalfSize, 0);
			Vector vecCornerBR(iArrowHalfSize, iArrowHalfSize, 0);
			Vector vecCornerBL(-iArrowHalfSize, iArrowHalfSize, 0);
			Vector vecCornerTL_rotated, vecCornerTR_rotated, vecCornerBL_rotated, vecCornerBR_rotated;

			// rotate it by our facing yaw
			QAngle angFacing(0, -fFacingYaw, 0);
			VectorRotate(vecCornerTL, angFacing, vecCornerTL_rotated);
			VectorRotate(vecCornerTR, angFacing, vecCornerTR_rotated);
			VectorRotate(vecCornerBR, angFacing, vecCornerBR_rotated);
			VectorRotate(vecCornerBL, angFacing, vecCornerBL_rotated);

			// if the pointee marine is KO'd, make the arrow blink.
			if ( bMarineIsKnockedOut )
			{
				surface()->DrawSetColor( fmod( gpGlobals->curtime , 0.5f ) <= 0.25f ? Color(255,0,0,255) : Color(0,0,0,0) );
			}
			else 
			{
				surface()->DrawSetColor( Color(131,151,160,255) );
			}

			surface()->DrawSetTexture(m_nMarinePointerTexture);

			Vertex_t points[4] = 
			{ 
				Vertex_t( Vector2D(vArrowCenter.x + vecCornerTL_rotated.x, vArrowCenter.y + vecCornerTL_rotated.y), 
				Vector2D(0,0) ), 
				Vertex_t( Vector2D(vArrowCenter.x + vecCornerTR_rotated.x, vArrowCenter.y + vecCornerTR_rotated.y), 
				Vector2D(1,0) ), 
				Vertex_t( Vector2D(vArrowCenter.x + vecCornerBR_rotated.x, vArrowCenter.y + vecCornerBR_rotated.y), 
				Vector2D(1,1) ), 
				Vertex_t( Vector2D(vArrowCenter.x + vecCornerBL_rotated.x, vArrowCenter.y + vecCornerBL_rotated.y), 
				Vector2D(0,1) ) 
			}; 
			surface()->DrawTexturedPolygon( 4, points );

			nCursorY += iArrowSize + nLineSpacing;
		}

		// draw the marine name
		if ( asw_marine_names.GetBool() )
		{
			Assert( nMarineNameWidth > 0 );
			{
				int nTextPosX = nBoxCenterX - ( nMarineNameWidth / 2 ) ;	// center it on the x
				int nNameLength = Q_wcslen( wszMarineName );

				// drop shadow
				vgui::surface()->DrawSetTextFont( nMarineFont );
				vgui::surface()->DrawSetTextColor( 0, 0, 0, 200 );
				vgui::surface()->DrawSetTextPos( nTextPosX+1, nCursorY+1 );
				vgui::surface()->DrawPrintText( wszMarineName, nNameLength );

				// actual text
				vgui::surface()->DrawSetTextColor( nMarineTextColor.r(), nMarineTextColor.g(), nMarineTextColor.b(), 200 );
				vgui::surface()->DrawSetTextPos( nTextPosX, nCursorY );
				vgui::surface()->DrawPrintText( wszMarineName, nNameLength );

				// advance cursor
				nCursorY += nMarineNameHeight + MAX( nLineSpacing, YRES(2) );
			}
			if ( pwszMarineProfileName && bMarineOnScreen )
			{
				int nTextPosX = nBoxCenterX - ( nMarineProfileNameWidth / 2 ) ;	// center it on the x
				int nNameLength = Q_wcslen( pwszMarineProfileName );

				// drop shadow
				vgui::surface()->DrawSetTextFont( m_hSmallMarineNameFont );
				vgui::surface()->DrawSetTextColor( 0, 0, 0, 200 );
				vgui::surface()->DrawSetTextPos( nTextPosX+1, nCursorY+1 );
				vgui::surface()->DrawPrintText( pwszMarineProfileName, nNameLength );

				// actual text
				vgui::surface()->DrawSetTextColor( nMarineTextColor.r(), nMarineTextColor.g(), nMarineTextColor.b(), 200 );
				vgui::surface()->DrawSetTextPos( nTextPosX, nCursorY );
				vgui::surface()->DrawPrintText( pwszMarineProfileName, nNameLength );

				// advance cursor
				nCursorY += nMarineProfileNameHeight + MAX( nLineSpacing, YRES(2) );
			}
		}

		// draw the talk icon to the left of the name
		if ( !asw_voice_side_icon.GetBool() )
		{
			int nNameWidth = nMarineNameWidth;
			int nNameHeight = nMarineNameHeight;
			PaintTalkingIcon( pMarine, nBoxCenterX - (nNameWidth/2) - (GetTalkingIconSize() + YRES(2)), nCursorY -nNameHeight - (GetTalkingIconSize()-nNameHeight) );
		}

		// draw the health bar
		if ( nHealthBarHeight > 0 )
		{
			if ( GetClassIconSize( bMarineOnScreen ) <= 0 )
				nCursorY += YRES(2);

			PaintHealthBar( pMarine, nBoxCenterX, nCursorY, bMarineOnScreen );
			nCursorY += nHealthBarHeight + nLineSpacing;
		}

		C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
		if ( !pWeapon )
			return;

		// draw the reload bar
		if ( bLocal && pWeapon && pWeapon->IsReloading() && asw_fast_reload_enabled.GetBool() && asw_fast_reload_under_marine.GetBool() )
		{
			PaintReloadBar( pWeapon, nBoxCenterX, nCursorY );
			nCursorY += nHealthBarHeight + nLineSpacing;
		}

		// draw the using bar
		if ( nUsingBarHeight > 0 )
		{
			PaintUsingBar( pMarine, nBoxCenterX, nCursorY );
			nCursorY += nUsingBarHeight + nLineSpacing;
		}

	}
	else 
	{
		//AssertMsg1( false, "debugoverlay->ScreenPosition(MarinePos-Vec(0,10,0),screenPos) returned %d", debugoverlay->ScreenPosition( MarinePos - Vector(0,10,0), screenPos ) );
	}
}

#define PORTRAIT_SIZE (80.0f * ( ScreenHeight() / 576.0f ) * 0.6f)

float CASWHud3DMarineNames::GetHealthBarMaxWidth( bool bKnockedOut )	const
{
	if ( bKnockedOut )
	{
		return PORTRAIT_SIZE * 1.5f;
	}
	return PORTRAIT_SIZE;
}
float CASWHud3DMarineNames::GetHealthBarMaxHeight( bool bKnockedOut )	const
{
	if ( bKnockedOut )
	{
		return 12.0f * ( ScreenHeight() / 576.0f ) * 0.6f;
	}
	return 8.0f * ( ScreenHeight() / 576.0f ) * 0.6f;
}

float CASWHud3DMarineNames::GetClassIconSize( bool bOnScreen )
{
	if ( bOnScreen )
		return 0;

	//return YRES( 14 );
	return (GetHealthBarMaxHeight( false ) * 2.5);
}

float CASWHud3DMarineNames::GetTalkingIconSize()
{
	return YRES( 14 );
	//return (GetHealthBarMaxHeight( false ) * 2.5);
}

bool CASWHud3DMarineNames::PaintHealthBar( C_ASW_Marine *pMarine, float xPos, float yPos, bool bOnScreen )
{
	if ( !pMarine || !pMarine->GetMarineResource() )
		return false;

	// check if the med satchel is out and if we are highlighting another marine

	int portrait_size = GetHealthBarMaxWidth( pMarine->m_bKnockedOut );
	int class_icon_size = GetClassIconSize( bOnScreen );
	int bar_height = GetHealthBarMaxHeight( pMarine->m_bKnockedOut );

	int buffer = YRES( 2 );
	int overall_width = portrait_size + buffer + class_icon_size;
	xPos = xPos - ( ( overall_width ) * 0.5f );

	// paint class icon next to health bar	
	int nClassIcon = -1;
	switch( pMarine->GetMarineProfile()->GetMarineClass() )
	{
	case MARINE_CLASS_NCO: nClassIcon = m_nOfficerClassIcon; break;
	case MARINE_CLASS_SPECIAL_WEAPONS: nClassIcon = m_nSWClassIcon; break;
	case MARINE_CLASS_MEDIC: nClassIcon = m_nMedicClassIcon; break;
	case MARINE_CLASS_TECH: nClassIcon = m_nTechClassIcon; break;
	}
	if ( nClassIcon != -1 && asw_world_healthbar_class_icon.GetBool() )
	{
		vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		vgui::surface()->DrawSetTexture( nClassIcon );
		vgui::surface()->DrawTexturedRect( xPos, yPos, xPos + class_icon_size, yPos + class_icon_size );
	}
	else
	{
		class_icon_size = 0;
	}

	xPos += class_icon_size + buffer;

	// if we have the medsatchel out, draw the healthbar bigger
	if ( pMarine->IsInfested() )
	{
		bar_height *= 1.75f;
		yPos += bar_height - (bar_height / 1.75f);
	}

	yPos += ( class_icon_size - bar_height ) * 0.5f;

	int portrait_x = xPos;

	// draw health bar
	int bar_y = yPos;	
	int bar_y2 = bar_y + bar_height;
	float fHealth = pMarine->GetMarineResource()->GetHealthPercent();
	if (pMarine->GetMarineResource()->m_bHealthHalved)		// if he was wounded from the last mission, halve the size of his health bar
	{
		fHealth *= 0.5f;
	}

	//if ( pMarine->m_bKnockedOut || pMarine->IsInfested() )
	//{
	//	float flDevourTotalTime = pMarine->GetDevourEndTime() - pMarine->GetDevourStartTime();
	//	float flDevourCurrentTime = gpGlobals->curtime - pMarine->GetDevourStartTime();
	//	fHealth = 1.0f - clamp( flDevourCurrentTime / flDevourTotalTime, 0.0f, 1.0f );
	//}

	int health_pixels = (portrait_size * fHealth);

	if (fHealth > 0 && health_pixels <= 0)
		health_pixels = 1;
	int bar_x2 = portrait_x + health_pixels;

	// white background
	vgui::surface()->DrawSetColor( Color( 255, 255, 255, 128 ) );
	vgui::surface()->DrawFilledRect( portrait_x - 1, bar_y - 1, portrait_x + portrait_size + 1, bar_y2 + 1 );

	// if he's wounded, draw a grey part
	if ( pMarine->GetMarineResource()->m_bHealthHalved )
	{
		vgui::surface()->DrawSetTexture(m_nWhiteTexture);
		vgui::surface()->DrawSetColor(Color(128,128,128,255));
		vgui::surface()->DrawTexturedRect(portrait_x + (portrait_size * 0.5f), bar_y, portrait_x + portrait_size, bar_y2);
	}

	// colored part
	Color rgbaBarBrightColor = Color(0,150,150,255);
	Color rgbaBarDarkColor = Color(0,75,75,255);

	if ( fHealth < 0.5f )
	{
		rgbaBarBrightColor = Color(200,50,0,255);
		rgbaBarDarkColor = Color(100,25,0,255);
	}

	if ( pMarine->m_bKnockedOut || pMarine->IsInfested()  )
	{
		float flFlash = 0.75f + 0.25f * sin( gpGlobals->curtime * 4.0f );
		rgbaBarBrightColor = Color( 255 * flFlash, 0,0,255 );
		rgbaBarDarkColor = Color( 64 * flFlash, 0,0,255 );
	}

	vgui::surface()->DrawSetColor(rgbaBarBrightColor);
	vgui::surface()->DrawSetTexture(m_nHorizHealthBar);
	vgui::Vertex_t hPoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(portrait_x, bar_y),	Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(bar_x2, bar_y),		Vector2D(fHealth,0) ), 
		vgui::Vertex_t( Vector2D(bar_x2, bar_y2),		Vector2D(fHealth,1) ), 
		vgui::Vertex_t( Vector2D(portrait_x, bar_y2),	Vector2D(0,1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, hPoints );

	vgui::surface()->DrawSetColor(rgbaBarDarkColor);
	vgui::surface()->DrawSetTexture(m_nHorizHealthBar);
	vgui::Vertex_t hRemainingPoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(bar_x2, bar_y),	Vector2D(fHealth,0) ), 
		vgui::Vertex_t( Vector2D(portrait_x + portrait_size, bar_y),		Vector2D(1,0) ), 
		vgui::Vertex_t( Vector2D(portrait_x + portrait_size, bar_y2),		Vector2D(1,1) ), 
		vgui::Vertex_t( Vector2D(bar_x2, bar_y2),	Vector2D(fHealth,1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, hRemainingPoints );

	// draw hurt over the top
	if ( m_nWhiteTexture != -1 && pMarine->GetMarineResource()->GetHurtPulse() > 0 )
	{
		rgbaBarBrightColor[3] *= pMarine->GetMarineResource()->GetHurtPulse();
		vgui::surface()->DrawSetColor( rgbaBarBrightColor );
		vgui::surface()->DrawSetTexture( m_nWhiteTexture );
		vgui::surface()->DrawTexturedPolygon( 4, hPoints );
	}

	// draw a green bit at the end of the health bar if he's infested
	if (pMarine->GetMarineResource()->IsInfested())
	{
		float fInfestPercent = pMarine->GetMarineResource()->GetInfestedPercent();
		if ( fInfestPercent > fHealth )
		{
			fInfestPercent = fHealth;
		}

		vgui::surface()->DrawSetTexture(m_nWhiteTexture);
		vgui::surface()->DrawSetColor( Color( 190, 225 + 15 * sinf(gpGlobals->curtime * 5.0f ), 20, 255 ) );
		int fOffset = (fHealth - fInfestPercent) * portrait_size;			
		vgui::surface()->DrawTexturedRect(portrait_x + fOffset, bar_y, bar_x2, bar_y2);			
		vgui::surface()->DrawSetColor(Color(255,255,255,255));
	}

	return true;
}

// find if our marine is using something
float CASWHud3DMarineNames::GetUsingFraction( C_ASW_Marine *pMarine )
{
	if ( !pMarine || !pMarine->GetMarineResource() )
		return 0;

	if (!pMarine->m_hUsingEntity.Get())
	{
		if (pMarine->GetMarineResource()->m_hWeldingDoor.Get())
		{
			if (pMarine->GetMarineResource()->IsFiring())
				return pMarine->GetMarineResource()->m_hWeldingDoor->GetSealAmount();
		}
		return 0;
	}

	IASW_Client_Usable_Entity* pUsable = dynamic_cast<IASW_Client_Usable_Entity*>(pMarine->m_hUsingEntity.Get());
	if (!pUsable)
		return 0;

	ASWUseAction action;
	if (!pUsable->GetUseAction(action, pMarine))
		return 0;

	return action.fProgress;
}


float CASWHud3DMarineNames::GetUsingBarMaxWidth()	const
{
	return PORTRAIT_SIZE;
}
float CASWHud3DMarineNames::GetUsingBarMaxHeight()	const
{
	return 7.0f * ( ScreenHeight() / 576.0f ) * 0.6f;
}

bool CASWHud3DMarineNames::PaintUsingBar( C_ASW_Marine *pMarine, float xPos, float yPos )
{
	if ( !pMarine || !pMarine->GetMarineResource() )
		return false;

	float fScale = ( ScreenHeight() / 576.0f ) * 0.6f;
	int portrait_size = GetUsingBarMaxWidth();
	xPos -= PORTRAIT_SIZE * 0.5f;
	int portrait_x = xPos;
	int portrait_y = yPos;

	vgui::surface()->DrawSetColor(Color(255,255,255,255));
	// draw black back
	vgui::surface()->DrawSetTexture(m_nBlackBarTexture);

	// draw health bar
	int bar_y = portrait_y + (2 * fScale);
	int bar_height = GetUsingBarMaxHeight();
	int bar_y2 = bar_y + bar_height;
	float fFraction = GetUsingFraction( pMarine );
	if ( fFraction <= 0 )
		return false;
	int using_pixels = (portrait_size * fFraction);

	if (fFraction > 0 && using_pixels <= 0)
		using_pixels = 1;
	int bar_x2 = portrait_x + using_pixels;
	int border = YRES(1);
	// black part first
	vgui::surface()->DrawTexturedRect(portrait_x - border, bar_y - border, portrait_x + portrait_size + border * 2 - 1, bar_y2 + border * 2 - 1);

	// red part
	vgui::surface()->DrawSetTexture(m_nVertAmmoBar);
	vgui::Vertex_t hpoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(portrait_x, bar_y),	Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(bar_x2, bar_y),		Vector2D(fFraction,0) ), 
		vgui::Vertex_t( Vector2D(bar_x2, bar_y2),		Vector2D(fFraction,1) ), 
		vgui::Vertex_t( Vector2D(portrait_x, bar_y2),	Vector2D(0,1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, hpoints );

	return true;
}

bool CASWHud3DMarineNames::PaintReloadBar( C_ASW_Weapon *pWeapon, float xPos, float yPos )
{
	if ( !pWeapon )
		return false;

	bool bFailure = pWeapon->m_bFastReloadFailure;
	bool bSuccess = pWeapon->m_bFastReloadSuccess;

	float flBGAlpha = 255.0f;
	Color colorBG =		Color( 32, 32, 32, flBGAlpha );
	Color colorWindow =	Color( 170, 170, 170, 255 );
	Color colorBar =	Color( 255, 255, 255, 128 );
	float flAlphaFade = 1.0f;

	int class_icon_size = GetClassIconSize( true );
	int t = GetHealthBarMaxHeight( false );
	int w = GetHealthBarMaxWidth( false );

	int iGap = YRES( 2 );
	int overall_width = w + iGap + class_icon_size;

	xPos += class_icon_size + iGap;
	yPos += ( class_icon_size - t ) * 0.5f;

	int iYPos = yPos;
	xPos = xPos - ( ( overall_width ) * 0.5f );
	int iXPos = xPos;

	float fStart = pWeapon->m_fReloadStart;
	if ( !bFailure && !bSuccess )
	{
		m_flLastNextAttack = pWeapon->m_flNextPrimaryAttack;
	}

	float fTotalTime = m_flLastNextAttack - fStart;
	if (fTotalTime <= 0)
		fTotalTime = 0.1f;

	float flProgress = 0.0f;
	// if we're in single player, the progress code in the weapon doesn't run on the client because we aren't predicting
	if ( !cl_predict->GetInt() )
		flProgress = (gpGlobals->curtime - fStart) / fTotalTime;
	else
		flProgress = pWeapon->m_fReloadProgress;

	if ( !bFailure && !bSuccess )
	{
		m_flLastReloadProgress = flProgress;
		m_flLastFastReloadStart = ((pWeapon->m_fFastReloadStart - fStart) / fTotalTime)+0.015f;
		m_flLastFastReloadEnd = ((pWeapon->m_fFastReloadEnd - fStart) / fTotalTime)-0.015f;
	}
	//Msg( "C: %f - %f - %f Reload Progress = %f\n", gpGlobals->curtime, fFastStart, fFastEnd, flProgress );

	if ( bFailure )
	{
		flBGAlpha = 255.0f;
		if ( flProgress > 0.75f )
			flAlphaFade = (1.0f - flProgress) / 0.25f;

		colorBG = Color( 128, 32, 16, flBGAlpha * flAlphaFade );
		colorWindow = Color( 200, 128, 128, 250 * flAlphaFade );
		colorBar =	Color( 255, 255, 255, 128 * flAlphaFade  );
	}
	else if ( bSuccess )
	{
		flAlphaFade = 1.0f - flProgress;
		colorBG = Color( 170, 170, 170, flBGAlpha * flAlphaFade );
		colorWindow = Color( 190, 220, 190, 255 * flAlphaFade );
		colorBar =	Color( 255, 255, 255, 128 * flAlphaFade  );
	}

	// white border
	vgui::surface()->DrawSetColor( colorBar );
	vgui::surface()->DrawFilledRect( iXPos - 1, iYPos - 1, iXPos + w + 1, iYPos+t + 1 );

	// draw the BG first
	vgui::surface()->DrawSetColor(colorBG);
	vgui::surface()->DrawSetTexture(m_nFastReloadBarBG);
	vgui::Vertex_t bgpoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(iXPos,				iYPos),				Vector2D(0, 0) ), 
		vgui::Vertex_t( Vector2D(iXPos+w,			iYPos),				Vector2D(1, 0) ),
		vgui::Vertex_t( Vector2D(iXPos+w,			iYPos+t),			Vector2D(1, 1) ), 
		vgui::Vertex_t( Vector2D(iXPos,				iYPos+t),			Vector2D(0, 1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, bgpoints );

	// then the charging bar
	vgui::surface()->DrawSetColor(colorWindow);
	vgui::surface()->DrawSetTexture(m_nHorizHealthBar);
	vgui::Vertex_t chargepoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(iXPos+w*m_flLastFastReloadStart,		iYPos+1),		Vector2D(m_flLastFastReloadStart, 0) ), 
		vgui::Vertex_t( Vector2D(iXPos+w*m_flLastFastReloadEnd,		iYPos+1),		Vector2D(m_flLastFastReloadEnd, 0) ), 
		vgui::Vertex_t( Vector2D(iXPos+w*m_flLastFastReloadEnd,		iYPos+t-1),		Vector2D(m_flLastFastReloadEnd, 1) ), 
		vgui::Vertex_t( Vector2D(iXPos+w*m_flLastFastReloadStart,		iYPos+t-1),		Vector2D(m_flLastFastReloadStart, 1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, chargepoints );

	// now the delay bar
	vgui::surface()->DrawSetColor(colorBar);
	vgui::surface()->DrawSetTexture(m_nWhiteTexture);
	vgui::Vertex_t delaypoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(iXPos+w*(MAX(m_flLastReloadProgress-0.01f,0)),		iYPos),			Vector2D( MAX(m_flLastReloadProgress-0.02f,0), 0 ) ), 
		vgui::Vertex_t( Vector2D(iXPos+w*(MAX(m_flLastReloadProgress+0.01f,0)),		iYPos),			Vector2D( m_flLastReloadProgress, 0) ), 
		vgui::Vertex_t( Vector2D(iXPos+w*(MAX(m_flLastReloadProgress+0.01f,0)),		iYPos+t),		Vector2D( m_flLastReloadProgress, 0) ), 
		vgui::Vertex_t( Vector2D(iXPos+w*(MAX(m_flLastReloadProgress-0.01f,0)),		iYPos+t),		Vector2D( MAX(m_flLastReloadProgress-0.02f,0), 0 ) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, delaypoints );

	///////////
	return true;
}

extern void ASW_GetLineCircle(int index, float &alien_x, float &alien_y, float &alien_radius, float &marine_x, float &marine_y, Vector2D &LineDir, int &iCol);
void CASWHud3DMarineNames::PaintAimingDebug()
{
	vgui::surface()->DrawSetColor(Color(255,255,255,192));
	for ( int i = 0; i < 25; i++ )
	{
		float alien_x, alien_y, alien_radius, marine_x, marine_y;
		int col;
		Vector2D Line;
		ASW_GetLineCircle(i, alien_x, alien_y, alien_radius, marine_x, marine_y, Line, col);
		if (alien_radius != 0)
		{
			if (col == 0)
				g_pMatSystemSurface->DrawColoredCircle(alien_x, alien_y, alien_radius, 255, 0, 0, 255);
			else
				g_pMatSystemSurface->DrawColoredCircle(alien_x, alien_y, alien_radius, 255, 255, 255, 255);
			//g_pMatSystemSurface->DrawColoredCircle(marine_x, marine_y, alien_radius, 255, 0, 0, 255);
			vgui::surface()->DrawLine(marine_x, marine_y, marine_x + Line.x * 100, marine_y + Line.y * 100);
		}	
	}
}


void CASWHud3DMarineNames::UpdateHealthTooltip()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{
		if (pPlayer->GetMarine() && dynamic_cast<C_ASW_Weapon_Medical_Satchel*>(pPlayer->GetMarine()->GetActiveASWWeapon()))
		{			
			C_ASW_Marine* pHealMarine = dynamic_cast<C_ASW_Marine*>( ASWInput()->GetHighlightEntity() );
			SetHealthMarine(pHealMarine);
		}
		else
		{
			SetHealthMarine(NULL);
		}		
	}
	// fade in/out as the marine changes
	if ( m_bHealthQueuedMarine )
	{
		// if we have a marine queued up, we better fade out
		m_fHealthAlpha -= gpGlobals->frametime * 5;

		if ( m_fHealthAlpha <= 0 )
		{
			// finished fading out, set our new marine and clear the queued
			if ( m_hHealthMarine != m_hHealthQueuedMarine )
			{
				m_hHealthMarine = m_hHealthQueuedMarine;

				// Fire event
				if ( m_hHealthMarine.Get() )
				{
					IGameEvent * event = gameeventmanager->CreateEvent( "player_heal_target" );
					if ( event )
					{
						event->SetInt( "userid", pPlayer->GetUserID() );
						event->SetInt( "entindex", m_hHealthMarine.Get()->entindex() );
						gameeventmanager->FireEventClientSide( event );
					}
				}
				else
				{
					IGameEvent * event = gameeventmanager->CreateEvent( "player_heal_target_none" );
					if ( event )
					{
						event->SetInt( "userid", pPlayer->GetUserID() );
						gameeventmanager->FireEventClientSide( event );
					}
				}
			}
			m_hHealthQueuedMarine = NULL;
			m_bHealthQueuedMarine = false;			
			m_fHealthAlpha = 0;
		}
	}
	else if (m_hHealthMarine.Get())
	{
		// no queue and a valid current, make sure it's shown
		m_fHealthAlpha += gpGlobals->frametime * 5;
		if (m_fHealthAlpha > 1.0f)
			m_fHealthAlpha = 1.0f;
	}

	
	// check for updating health
	C_ASW_Marine* pMarine = m_hHealthMarine.Get();
	
	// hide the panel if we have no marine to report on
	if (!pMarine && m_fHealthAlpha > 0)
	{
		if (!m_bHealthQueuedMarine)
		{
			m_hHealthQueuedMarine = NULL;
			m_bHealthQueuedMarine = true;	
		}		
	}
}

void CASWHud3DMarineNames::SetHealthMarine(C_ASW_Marine *pMarine)
{
	if (m_hHealthMarine.Get() != pMarine && !(m_bHealthQueuedMarine && pMarine == m_hHealthQueuedMarine.Get()))
	{
		m_hHealthQueuedMarine = pMarine;
		m_bHealthQueuedMarine = true;		
	}
}

static const char* s_FontTestNames[]={
	"Sansman8",
	"Sansman9",
	"Sansman10",
	"Sansman11",
	"Sansman12",
	"Sansman13",
	"Sansman14",
	"Sansman16",
	"Sansman17",
	"Sansman18",
	"Sansman19",
	"Sansman20"
};

void CASWHud3DMarineNames::PaintFontTest()
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	if (!pScheme)
		return;
	 
	int num_fonts = NELEMS(s_FontTestNames);
	int y = ScreenHeight() * 0.1f;
	int x = ScreenHeight() * 0.3f;
	for (int i=0;i<num_fonts;i++)
	{
		vgui::HFont hFont = pScheme->GetFont(s_FontTestNames[i], false);
		g_pMatSystemSurface->DrawColoredText(hFont, x, y, 255, 255, 
					255, 255, "%s Bastille", s_FontTestNames[i]);
		y += vgui::surface()->GetFontTall(hFont);
	}

	vgui::HFont hFont = pScheme->GetFont( "Default", IsProportional() );
	g_pMatSystemSurface->DrawColoredText(hFont, x, y, 255, 255, 
				255, 255, "Default Bastille");
	y += vgui::surface()->GetFontTall(hFont);
}

void CASWHud3DMarineNames::PaintTalkingIcon( C_ASW_Marine *pMarine, float xPos, float yPos )
{
	if ( !pMarine || !ASWGameResource() || ASWGameResource()->IsOfflineGame() )
		return;

	C_ASW_Player* pPlayer = pMarine->GetCommander();
	if ( !pPlayer )
		return;

	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if (!local)
		return;

	CVoiceStatus *pVoiceMgr = GetClientVoiceMgr();	
	int index = pPlayer->entindex();

	bool bTalking = false;

	if ( pPlayer == local )
	{
		bTalking = pVoiceMgr->IsLocalPlayerSpeakingAboveThreshold( FirstValidSplitScreenSlot() );
	}
	else
	{
		bTalking = pVoiceMgr->IsPlayerSpeaking( index );
	}

	float flColorSinR = 0;
	float flColorSinG = 0;
	float flColorSinB = 0;
	float flAlpha = 255;

	if ( pPlayer == local && bTalking )
	{
		flColorSinR = 234+(sin( gpGlobals->curtime * 8) + 1) * 10;
		flColorSinG = flColorSinR;
		flColorSinB = 130+(sin( gpGlobals->curtime * 8) + 1) * 60;
	}
	else if ( bTalking )
	{
		flColorSinR = 170+(sin( gpGlobals->curtime * 8) + 1) * 30;
		flColorSinG = flColorSinR;
		flColorSinB = 210+(sin( gpGlobals->curtime * 8) + 1) * 20;
	}

	if ( m_3DSpeakingList.IsBitSet( index ) || bTalking )
	{
		if ( bTalking )
		{
			m_3DSpeakingList.Set( index, true );
			m_flLastTalkingTime[index-1] = gpGlobals->curtime;
		}
		else
		{
			flColorSinR = 230;
			flColorSinG = 230;
			flColorSinB = 230;
			flAlpha *= clamp( ((m_flLastTalkingTime[index-1]+1.5f) - gpGlobals->curtime)/1.5f, 0.0f, 1.0f );
		}

		if ( flAlpha <= 0 )
		{
			m_3DSpeakingList.Set( index, false );
		}

		if ( pMarine->IsInhabited() )
		{
			int iSize = GetTalkingIconSize();
			vgui::surface()->DrawSetColor( flColorSinR, flColorSinG, flColorSinB, flAlpha );
			vgui::surface()->DrawSetTexture( m_nTalkingIcon );
			vgui::surface()->DrawTexturedRect( xPos, yPos, xPos + iSize, yPos + iSize );
		}
	}
}

void CASWHud3DMarineNames::PaintTrackedHealth()
{
	for ( int i = 0; i < IHealthTracked::AutoList().Count(); i++ )
	{
		IHealthTracked *pTracked = static_cast< IHealthTracked* >( IHealthTracked::AutoList()[ i ] );
		if ( !pTracked )
			continue;

		pTracked->PaintHealthBar( this );	
	}
}


bool CASWHud3DMarineNames::PaintGenericBar( Vector vWorldPos, float flProgress, Color rgbaBarColor, float flSizeMultiplier, const Vector2D &offset )
{
	Vector screenPos;

	const int nMaxX = ScreenWidth() - 150;
	const int nMaxY = ScreenHeight() - 100;
	debugoverlay->ScreenPosition( vWorldPos - Vector(0,10,0), screenPos );
	if ( screenPos.x < 0 || screenPos.x > nMaxX || screenPos.y < 0 || screenPos.y > nMaxY )
	{
		return false;
	}

	screenPos.x += offset.x;
	screenPos.y += offset.y;

	float fScale = ( ScreenHeight() / 576.0f ) * 0.6f;
	int portrait_size = GetUsingBarMaxWidth() * flSizeMultiplier;
	screenPos.x -= PORTRAIT_SIZE * 0.5f;
	int portrait_x = screenPos.x;
	int portrait_y = screenPos.y;

	// draw health bar
	int bar_y = portrait_y + (2 * fScale);
	int bar_height = GetUsingBarMaxHeight() * flSizeMultiplier;
	int bar_y2 = bar_y + bar_height;
	if ( flProgress <= 0 )
		return false;
	int using_pixels = (portrait_size * flProgress);

	if ( flProgress > 0 && using_pixels <= 0 )
		using_pixels = 1;
	int bar_x2 = portrait_x + using_pixels;
	int border = YRES(1);

	// black part first
	vgui::surface()->DrawSetColor( Color( 255, 255, 255, rgbaBarColor.a() ) );
	vgui::surface()->DrawSetTexture( m_nBlackBarTexture );
	vgui::surface()->DrawTexturedRect( portrait_x - border, bar_y - border, portrait_x + portrait_size + border * 2, bar_y2 + border * 2 );

	// red part
	vgui::surface()->DrawSetColor( rgbaBarColor );
	vgui::surface()->DrawSetTexture( m_nVertAmmoBar );
	vgui::Vertex_t hpoints[4] = 
	{ 
		vgui::Vertex_t( Vector2D(portrait_x, bar_y),	Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(bar_x2, bar_y),		Vector2D(flProgress,0) ), 
		vgui::Vertex_t( Vector2D(bar_x2, bar_y2),		Vector2D(flProgress,1) ), 
		vgui::Vertex_t( Vector2D(portrait_x, bar_y2),	Vector2D(0,1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, hpoints );

	return true;
}

bool CASWHud3DMarineNames::PaintGenericText( Vector vWorldPos, char *pText, Color vTextColor, float flSizeMultiplier, const Vector2D &offset  )
{
	Vector screenPos;

	const int nMaxX = ScreenWidth() - 150;
	const int nMaxY = ScreenHeight() - 100;
	debugoverlay->ScreenPosition( vWorldPos - Vector(0,10,0), screenPos );
	if ( screenPos.x < 0 || screenPos.x > nMaxX || screenPos.y < 0 || screenPos.y > nMaxY )
	{
		return false;
	}

	// work around error in design of IMatSystemSurface
	int nTextWidth = g_pMatSystemSurface->DrawTextLen( m_hPlayerNameFont, pText );
	int nTextHeight = vgui::surface()->GetFontTall(m_hPlayerNameFont);

	screenPos.x += offset.x;
	screenPos.y += offset.y * nTextHeight;

	// g_pMatSystemSurface->GetTextSize( nMarineFont, UNICODE VERSION OF pText, nMarineNameWidth, nMarineNameHeight );

	int nTextPosX = screenPos.x - (nTextWidth/2) ;	// centre it on the x,y
	int nTextPosY = screenPos.y - (nTextHeight/2);

	//drop shadow
	g_pMatSystemSurface->DrawColoredText(m_hPlayerNameFont, nTextPosX+1, nTextPosY+1, 
		0, 0, 0, 200, 
		pText );
	// actual text
	g_pMatSystemSurface->DrawColoredText(m_hPlayerNameFont, nTextPosX, nTextPosY, 
		vTextColor.r(), vTextColor.g(), vTextColor.b(), 200, 
		pText );

	return true;
}