#include "cbase.h"
#include "asw_input.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_alien.h"
#include "c_asw_weapon.h"
#include "asw_util_shared.h"
#include "asw_vgui_info_message.h"
#include "fx.h"
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include "asw_shareddefs.h"
#include "tier0/vprof.h"
#include "controller_focus.h"
#include "input.h"
#include "inputsystem/iinputsystem.h"
#include "c_asw_computer_area.h"
#include "c_asw_button_area.h"
#include "in_buttons.h"
#include "asw_gamerules.h"
#include "asw_melee_system.h"
#ifdef _WIN32
#undef INVALID_HANDLE_VALUE
#include <windows.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_controls;
ConVar asw_marine_linear_turn_rate("asw_marine_linear_turn_rate", "600", FCVAR_CHEAT, "Linear turning rate of the marine (used as minimum when fractional turning is employed)");
ConVar asw_marine_fraction_turn_scale("asw_marine_fraction_turn_scale", "0", FCVAR_CHEAT, "Scale for the fractional marine turning (large turns)");

ConVar asw_marine_turn_firing_fraction("asw_marine_turn_firing_fraction", "0.6", FCVAR_CHEAT, "Fractional turning value while firing, if using asw_marine_fraction_turn_scale");
ConVar asw_marine_turn_normal_fraction("asw_marine_turn_normal_fraction", "0.9", FCVAR_CHEAT, "Fractional turning value if using asw_marine_fraction_turn_scale");
ConVar asw_marine_turn_y_pos("asw_marine_turn_y_pos", "0.55", FCVAR_ARCHIVE, "Normalized height position for where the cursor changes the player from looking north to south.");

ConVar joy_autoattack( "joy_autoattack", "0", FCVAR_NONE, "If enabled, marine will fire when you push the right analogue stick" );
ConVar joy_lock_firing_angle( "joy_lock_firing_angle", "0", FCVAR_NONE, "If enabled, your facing direction will be locked while firing instead of aiming to movement" );
ConVar joy_autoattack_threshold( "joy_autoattack_threshold", "0.6", FCVAR_NONE, "Threshold for joy_autoattack" );
ConVar joy_autoattack_angle( "joy_autoattack_angle", "10", FCVAR_NONE, "Facing has to be within this many degrees of aim for the marine to auto fire" );
ConVar joy_cursor_speed( "joy_cursor_speed", "2.0f", FCVAR_NONE, "Cursor speed of joystick when used in targeting mode" );
ConVar joy_cursor_scale( "joy_cursor_scale", "1.3f", FCVAR_NONE, "Cursor extent scale of joystick when used in targeting mode" );
ConVar joy_radius_snap_factor( "joy_radius_snap_factor", "2.0f", FCVAR_NONE, "Rate at which joystick targeting radius tracks the current cursor radius" );
ConVar joy_aim_to_movement_time( "joy_aim_to_movement_time", "1.0f", FCVAR_NONE, "Time before the player automatically aims in the direction of movement." );
ConVar joy_tilted_view( "joy_tilted_view", "0", FCVAR_NONE, "Set to 1 when using maps with tilted view to rotate player movement." );
ConVar asw_horizontal_autoaim( "asw_horizontal_autoaim", "1", FCVAR_CHEAT, "Applies horizontal correction towards best aim ent." );

ConVar joy_disable_movement_in_ui( "joy_disable_movement_in_ui", "1", 0, "Disables joystick character movement when UI is active." );

extern kbutton_t in_attack;
extern int g_asw_iPlayerListOpen;
extern ConVar asw_controls;
extern ConVar asw_DebugAutoAim;
extern ConVar in_forceuser;
extern ConVar asw_item_hotbar_hud;
extern ConVar in_joystick;

//-----------------------------------------------------------------------------
// Purpose: ASW Input interface
//-----------------------------------------------------------------------------
static CASWInput g_Input;

// Expose this interface
IInput *input = ( IInput * )&g_Input;

CASWInput *ASWInput() 
{ 
	return &g_Input;
}



void GetVGUICursorPos( int& x, int& y );

#define ASW_NUM_HUMAN_READABLE 5
static char const *s_HumanKeynames[ASW_NUM_HUMAN_READABLE][2]={
	{"MWHEELUP", "#asw_mouse_wheel_up"},
	{"MWHEELDOWN", "#asw_mouse_wheel_down"},
	{"MOUSE1", "#asw_mouse1"},
	{"MOUSE2", "#asw_mouse2"},
	{"MOUSE3", "#asw_mouse3"},
};

#ifdef _X360

// human readable names for xbox 360 controller buttons on Xbox360
#define ASW_NUM_360_READABLE 24
static char const *s_360Keynames[ASW_NUM_360_READABLE][2]={
	{"A_BUTTON", "#asw_360_a"},
	{"B_BUTTON", "#asw_360_b"},
	{"X_BUTTON", "#asw_360_x"},
	{"Y_BUTTON", "#asw_360_y"},
	{"L_SHOULDER", "#asw_360_left_bumper"},
	{"R_SHOULDER", "#asw_360_right_bumper"},
	{"BACK", "#asw_360_back"},
	{"START", "#asw_360_start"},
	{"STICK1", "#asw_360_left_stick_in"},
	{"STICK2", "#asw_360_right_stick_in"},
	{"UP", "#asw_360_dpad_up"},
	{"RIGHT", "#asw_360_dpad_right"},
	{"DOWN", "#asw_360_dpad_down"},
	{"LEFT", "#asw_360_dpad_left"},
	{"R_TRIGGER", "#asw_360_right_trigger"},
	{"L_TRIGGER", "#asw_360_left_trigger"},
	{"S1_RIGHT", "#asw_360_left_stick_right"},
	{"S1_LEFT", "#asw_360_left_stick_left"},
	{"S1_DOWN", "#asw_360_left_stick_down"},
	{"S1_UP", "#asw_360_left_stick_up"},
	{"S2_RIGHT", "#asw_360_right_stick_right"},
	{"S2_LEFT", "#asw_360_right_stick_left"},
	{"S2_DOWN", "#asw_360_right_stick_down"},
	{"S2_UP", "#asw_360_right_stick_up"},
};

#else // !_X360

// human readable names for xbox 360 controller buttons under windows
#define ASW_NUM_360_READABLE 24
static char const *s_360Keynames[ASW_NUM_360_READABLE][2]={
	{"JOY1", "#asw_360_a"},
	{"JOY2", "#asw_360_b"},
	{"JOY3", "#asw_360_x"},
	{"JOY4", "#asw_360_y"},
	{"AUX5", "#asw_360_left_bumper"},
	{"AUX6", "#asw_360_right_bumper"},
	{"AUX7", "#asw_360_back"},
	{"AUX8", "#asw_360_start"},
	{"AUX9", "#asw_360_left_stick_in"},
	{"AUX10", "#asw_360_right_stick_in"},
	{"AUX29", "#asw_360_dpad_up"},
	{"AUX30", "#asw_360_dpad_right"},
	{"AUX31", "#asw_360_dpad_down"},
	{"AUX32", "#asw_360_dpad_left"},
	{"Z AXIS NEG", "#asw_360_right_trigger"},
	{"Z AXIS POS", "#asw_360_left_trigger"},
	{"X AXIS POS", "#asw_360_left_stick_right"},
	{"X AXIS NEG", "#asw_360_left_stick_left"},
	{"Y AXIS POS", "#asw_360_left_stick_down"},
	{"Y AXIS NEG", "#asw_360_left_stick_up"},
	{"U AXIS POS", "#asw_360_right_stick_right"},
	{"U AXIS NEG", "#asw_360_right_stick_left"},
	{"R AXIS POS", "#asw_360_right_stick_down"},
	{"R AXIS NEG", "#asw_360_right_stick_up"},
};

#endif

#define ASW_NUM_STORES 25
static float StoreAlienPosX[ASW_NUM_STORES];
static float StoreAlienPosY[ASW_NUM_STORES];
static float StoreAlienRadius[ASW_NUM_STORES];
static float StoreMarinePosX[ASW_NUM_STORES];
static float StoreMarinePosY[ASW_NUM_STORES];
static Vector2D StoreLineDir[ASW_NUM_STORES];
static int StoreCol[ASW_NUM_STORES];
void ASW_StoreLineCircle(int index, float alien_x, float alien_y, float alien_radius, float marine_x, float marine_y, Vector2D LineDir, int iCol)
{
	if (index >= ASW_NUM_STORES)
		return;

	StoreAlienPosX[index] = alien_x;
	StoreAlienPosY[index] = alien_y;
	StoreAlienRadius[index] = alien_radius;
	StoreMarinePosX[index] = marine_x;
	StoreMarinePosY[index] = marine_y;
	StoreLineDir[index] = LineDir;
	StoreCol[index] = iCol;
}
void ASW_GetLineCircle(int index, float &alien_x, float &alien_y, float &alien_radius, float &marine_x, float &marine_y, Vector2D &LineDir, int &iCol)
{
	if (index >= ASW_NUM_STORES)
		return;

	alien_radius = StoreAlienRadius[index];
	if ( alien_radius == 0 )
		return;

	alien_x = StoreAlienPosX[index];
	alien_y = StoreAlienPosY[index];
	marine_x = StoreMarinePosX[index];
	marine_y = StoreMarinePosY[index];
	LineDir = StoreLineDir[index];
	iCol = StoreCol[index];
}
void ASW_StoreClearAll()
{
	for (int index=0;index<ASW_NUM_STORES;index++)
	{
		StoreAlienPosX[index] = 0;
		StoreAlienPosY[index] = 0;
		StoreAlienRadius[index] = 0;
		StoreMarinePosX[index] = 0;
		StoreMarinePosY[index] = 0;
		StoreLineDir[index] = Vector2D(0,0);
	}
}

// asw
bool MarineControllingTurret()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	return (pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->IsControllingTurret());
}

#define PI 3.14159265358979
#define ASW_MARINE_HULL_MINS Vector(-13, -13, 0)
#define ASW_MARINE_HULL_MAXS Vector(13, 13, 72)

bool ValidOrderingSurface(const trace_t &tr)
{
	if (!Q_strcmp("TOOLS/TOOLSNOLIGHT", tr.surface.name))
		return false;

	// check marine hull can fit in this spot
	Ray_t ray;
	trace_t pm;
	ray.Init( tr.endpos, tr.endpos, ASW_MARINE_HULL_MINS, ASW_MARINE_HULL_MAXS );
	UTIL_TraceRay( ray, MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &pm );
	if ( (pm.contents & MASK_PLAYERSOLID) && pm.m_pEnt )
	{
		return false;
	}

	return true;
};

// trace used by marine ordering - skips over toolsnolight and clip brushes, etc
bool HUDTraceToWorld(float screenx, float screeny, Vector &HitLocation, bool bUseMarineHull)
{
	// Verify that we have input.
	Assert( ASWInput() != NULL );

	Vector X, Y, Z, projected;	
	Vector traceEnd;
	Vector vCameraLocation;
	Vector CamResult, TraceDirection;
	QAngle CameraAngle;
	HitLocation.Init(0,0,0);

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return false;

	int omx, omy;
	ASWInput()->ASW_GetCameraLocation(pPlayer, vCameraLocation, CameraAngle, omx, omy, false);

	float fRatio = float( ScreenHeight() ) / float( ScreenWidth() );
	AngleVectors(CameraAngle, &X, &Y, &Z);
	float FOVAngle = pPlayer->GetFOV();
	projected = X 
				- tanf(FOVAngle*PI/180*0.5) * 2 * Y * (screenx) * ( 0.75f / fRatio )
		          + tanf(FOVAngle*PI/180*0.5) * 2 * Z * (screeny) * 0.75f;

	TraceDirection = projected;
	TraceDirection.NormalizeInPlace();
	Vector traceStart = vCameraLocation;
	traceEnd = traceStart + TraceDirection * 3000;

	if (bUseMarineHull)
	{
		// do a trace into the world to see what we've pointing directly at		
		Ray_t ray2;
		trace_t tr;
		ray2.Init( traceStart, traceEnd, ASW_MARINE_HULL_MINS, ASW_MARINE_HULL_MAXS );
		UTIL_TraceRay( ray2, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction >= 1.0f )
			return false;

		bool bValidSurface = ValidOrderingSurface(tr);

		HitLocation = tr.endpos;

		if (!bValidSurface)
		{
			// trace down from above the hitlocation with a marine hull
			Ray_t ray;
			trace_t pm;
			ray.Init( HitLocation + Vector(0, 0, 40), HitLocation, ASW_MARINE_HULL_MINS, ASW_MARINE_HULL_MAXS );
			UTIL_TraceRay( ray, MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &pm );
			if ( pm.startsolid || pm.fraction < 0.01f )
			{
				return false;
			}
			HitLocation = pm.endpos;
			//Msg("Using raised pos\n");
		}
		else
		{
			// trace down in-case this spot is in the air
			Ray_t ray;
			trace_t pm;
			ray.Init( HitLocation, HitLocation - Vector(0, 0, 2200), ASW_MARINE_HULL_MINS, ASW_MARINE_HULL_MAXS );
			UTIL_TraceRay( ray, MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &pm );		
			if (pm.startsolid)
			{
				return false;
			}
			else
			{
				HitLocation = pm.endpos;
			}
		}
	}
	else
	{
		// do a trace into the world to see what we've pointing directly at
		trace_t tr;
		UTIL_TraceLine(traceStart, traceEnd, MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr);
		if ( tr.fraction >= 1.0f )
			return false;
		// if we hit tools no light texture, retrace through it

		bool bValidSurface = ValidOrderingSurface(tr);
		//int iRetrace = 4;

		HitLocation = vCameraLocation + tr.fraction * 3000 * TraceDirection;

		return bValidSurface;
	}

	// Note: disabled this code that repeatedly retraces through bad surfaces
	/*
	while (!bValidSurface && iRetrace > 0)
	{
		traceStart = tr.endpos + TraceDirection * 2;		
		traceEnd = traceStart + TraceDirection * 3000;
		UTIL_TraceLine(traceStart, traceEnd, MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr);
		if ( tr.fraction >= 1.0f )
			return false;
		HitLocation = traceStart + tr.fraction * 3000 * TraceDirection;
		bValidSurface = ValidOrderingSurface(tr);
		iRetrace--;
	}*/
	
	//Msg("trace hit %s flags %d surfaceprops %d\n", tr.surface.name, tr.surface.flags, tr.surface.surfaceProps);

	//FX_MicroExplosion(HitLocation, Vector(0,0,1));
	return true;
}

// Finds the world location we should be aiming at, given the XY coords of our mouse cursor
// bIgnoreCursorPosition - if true, don't perform a trace beneath the cursor; just use the facing direction
C_BaseEntity* HUDToWorld(float screenx, float screeny,
	Vector &HitLocation, IASW_Client_Aim_Target* &pAutoAimEnt,
	bool bPreferFlatAiming, bool bIgnoreCursorPosition, float flForwardMove, float flSideMove)
{
	// Verify that we have input.
	Assert( ASWInput() != NULL );

	HitLocation.Init(0,0,0);

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return NULL;

	ASWInput()->SetAutoaimEntity( NULL );

	C_ASW_Marine* pMarine = pPlayer->GetMarine();
	C_ASW_Weapon *pWeapon = pMarine ? pMarine->GetActiveASWWeapon() : NULL;
	float flWeaponRadiusScale = pWeapon ? pWeapon->GetAutoAimRadiusScale() : 1.0f;
	bool bWeaponHasRadiusScale = ( flWeaponRadiusScale > 1.0f );

	// our source firing point
	Vector marineScreenPos, alienScreenPos, bestAlienScreenPos;
	Vector vWorldSpaceCameraToCursor;
	IASW_Client_Aim_Target* pBestAlien = NULL;
	IASW_Client_Aim_Target* pHighlightAlien = NULL;  // this the ent our cursor is over
	float flBestAlienRadius = 0;
	bool bBestAlienUsingFlareAutoaim = false;
	int omx, omy;
	Vector vCameraLocation;
	QAngle cameraAngle;
	Vector vTraceEnd;
	int nTraceMask = (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE);		// CONTENTS_PLAYERCLIP
	trace_t tr;
	int nDebugLine = 3;

	ASWInput()->ASW_GetCameraLocation(pPlayer, vCameraLocation, cameraAngle, omx, omy, false);

	Vector X, Y, Z;	
	Vector CamResult;

	float fRatio = float( ScreenHeight() ) / float( ScreenWidth() );
	AngleVectors(cameraAngle, &X, &Y, &Z);
	float FOVAngle = pPlayer->GetFOV();
	vWorldSpaceCameraToCursor = X 
		- tanf(FOVAngle*PI/180*0.5) * 2 * Y * (screenx) * ( 0.75f / fRatio )
		+ tanf(FOVAngle*PI/180*0.5) * 2 * Z * (screeny) *  0.75f;

	vWorldSpaceCameraToCursor.NormalizeInPlace();
	vTraceEnd = vCameraLocation + vWorldSpaceCameraToCursor * ASW_MAX_AIM_TRACE;

	// do a trace into the world to see what we've pointing directly at
	UTIL_TraceLine(vCameraLocation, vTraceEnd, nTraceMask, pPlayer, COLLISION_GROUP_NONE, &tr);
	if ( tr.fraction >= 1.0f )
	{
		if (!pMarine)
			return NULL;

		float flFloorZ = ( pMarine->GetRenderOrigin() + Vector( 0,0, ASW_MARINE_GUN_OFFSET_Z ) ).z;
		float trace_dist_to_ground = -(vCameraLocation.z - flFloorZ) / vWorldSpaceCameraToCursor.z;
		HitLocation = vCameraLocation + vWorldSpaceCameraToCursor * trace_dist_to_ground;
	}
	else
	{
		HitLocation = vCameraLocation + tr.fraction * 3000 * vWorldSpaceCameraToCursor;
		HitLocation.z = 0;
	}

	
	if (!pMarine)
	{
		// no marine, just use the trace spot
		return NULL;
	}

	Vector vecWeaponPos = pMarine->GetRenderOrigin() + Vector( 0,0, ASW_MARINE_GUN_OFFSET_Z );
	float fFloorZ = vecWeaponPos.z;	// normally aim flat

	if ( !bIgnoreCursorPosition )
	{
		// if we're clicking right on something
		if (tr.m_pEnt && tr.m_pEnt!=pMarine)
		{
			// store if we're clicking right on an aim target
			IASW_Client_Aim_Target* pPossibleAimEnt = dynamic_cast<IASW_Client_Aim_Target*>(tr.m_pEnt);
			if (pPossibleAimEnt && pPossibleAimEnt->IsAimTarget())
			{
				pBestAlien = pPossibleAimEnt;
				debugoverlay->ScreenPosition( pBestAlien->GetAimTargetPos(vecWeaponPos, bPreferFlatAiming), bestAlienScreenPos );	// tr.m_pEnt->WorldSpaceCenter()			
				if (asw_DebugAutoAim.GetBool())
					debugoverlay->AddLineOverlay(pBestAlien->GetAimTargetPos(vecWeaponPos, bPreferFlatAiming), pBestAlien->GetAimTargetPos(vecWeaponPos) + Vector(0,0,10),
					0, 255, 0, true, 0.01f);
			}
		}
	}

	// go through possible aim targets and see if we're pointing the cursor in the direction of any	
	float best_d = -1;
	debugoverlay->ScreenPosition( vecWeaponPos, marineScreenPos );	// asw should be hacked gun pos?
	Vector mins, maxs, corner;
	if (asw_DebugAutoAim.GetBool())
		ASW_StoreClearAll();

	// if we have our cursor over a target (and not using the controller), make sure we have LOS to him before we continue
	if ( !ASWInput()->ControllerModeActive() && pBestAlien )
	{
		// check we have LOS to the target
		CTraceFilterLOS traceFilter( pMarine, COLLISION_GROUP_NONE );
		trace_t tr2;
		UTIL_TraceLine( vecWeaponPos, pBestAlien->GetAimTargetRadiusPos( vecWeaponPos ), MASK_OPAQUE, &traceFilter, &tr2 );
		C_BaseEntity *pEnt = pBestAlien->GetEntity();
		bool bHasLOS = (!tr2.startsolid && (tr2.fraction >= 1.0 || tr2.m_pEnt == pEnt));
		// we can't shoot it, so skip autoaiming to it, but still return it as an entity that we want to highlight
		if ( !bHasLOS )
		{
			pHighlightAlien = pBestAlien;
			pBestAlien = NULL;
		}
		else
		{
			if ( !bWeaponHasRadiusScale )
			{
				if ( ASWGameRules()->CanFlareAutoaimAt( pMarine, pEnt ) )
				{
					bBestAlienUsingFlareAutoaim = true;
				}
			}
		}
	}

	if ( !pBestAlien && !pHighlightAlien )		// if we don't already have an ideal aim target from above, do the loop
	{
		int iStoreNum = 0;		
		for ( int i = 0; i < IASW_Client_Aim_Target::AutoList().Count(); i++ )
		{
			IASW_Client_Aim_Target *pAimTarget = static_cast< IASW_Client_Aim_Target* >( IASW_Client_Aim_Target::AutoList()[ i ] );
			C_BaseEntity *pEnt = pAimTarget->GetEntity();
			if ( !pEnt || !pAimTarget->IsAimTarget() )
				continue;
			// check it isn't attached to our marine (infesting parasites)
			if (pEnt->GetMoveParent() == pMarine)
				continue;
			// check he's in range
			Vector vecAlienPos = pAimTarget->GetAimTargetRadiusPos(vecWeaponPos); //pEnt->WorldSpaceCenter();
			if ( vecAlienPos.DistToSqr(vecWeaponPos) > ASW_MAX_AUTO_AIM_RANGE )
				continue;

			Vector vDirection = vecAlienPos - vecWeaponPos;
			float fZDist = vDirection.z;
			if ( fZDist > 250.0f )
			{
				VectorNormalize( vDirection );
				if ( vDirection.z > 0.85f )
				{
					// Don't aim at stuff that's high up and directly above us
					continue;
				}
			}

			debugoverlay->ScreenPosition( vecAlienPos, alienScreenPos );

			Vector AlienEdgeScreenPos;
			float flRadiusScale = ASWInput()->ControllerModeActive() ? 2.0f : 1.0f;
			flRadiusScale *= flWeaponRadiusScale;
			bool bFlareAutoaim = false;
			if ( !bWeaponHasRadiusScale )
			{
				if ( ASWGameRules()->CanFlareAutoaimAt( pMarine, pEnt ) )
				{
					flRadiusScale *= 2.0f;
					bFlareAutoaim = true;
				}
			}
			debugoverlay->ScreenPosition( vecAlienPos + Vector( pAimTarget->GetRadius() * flRadiusScale, 0, 0 ), AlienEdgeScreenPos);
			float alien_radius = (alienScreenPos - AlienEdgeScreenPos).Length2D();
			if (alien_radius <= 0)
				continue;

			float intersect1, intersect2;
			Vector2D LineDir(omx - marineScreenPos.x, omy - marineScreenPos.y);
			LineDir.NormalizeInPlace();	
			
			if (asw_DebugAutoAim.GetBool())
			{
				char buffer[255];
				sprintf(buffer, "r=%f aim=%d\n", alien_radius, pEnt->GetFlags() & FL_AIMTARGET);
				debugoverlay->AddTextOverlay(vecAlienPos, 0, 0.01f, buffer);
			}
			
			if (ASW_LineCircleIntersection(Vector2D(alienScreenPos.x, alienScreenPos.y),	// center
										alien_radius,									// radius
										Vector2D(marineScreenPos.x, marineScreenPos.y),	// line start
										LineDir,										// line direction
										&intersect1, &intersect2))
			{
				if (asw_DebugAutoAim.GetBool())
					ASW_StoreLineCircle(iStoreNum++, alienScreenPos.x, alienScreenPos.y, alien_radius, marineScreenPos.x, marineScreenPos.y, LineDir, 1);
				// midpoint of intersection is closest to circle center
				float midintersect = (intersect1 + intersect2) * 0.5f;
				if (midintersect > 0)
				{
					//Vector2D Midpoint = Vector2D(marineScreenPos.x, marineScreenPos.y) + LineDir * midintersect;
					//Midpoint -= Vector2D(alienScreenPos.x, alienScreenPos.y);
					// use how near we are to the center to prioritise aim target
					//float dist = Midpoint.LengthSqr();

					// we are now prioritizing enemies which are closer to the player that intersect the trace
					float dist = midintersect;
					if (dist < best_d || best_d == -1)
					{
						// check we have LOS to the target
						CTraceFilterLOS traceFilter( pMarine, COLLISION_GROUP_NONE );
						trace_t tr2;
						UTIL_TraceLine( vecWeaponPos, pAimTarget->GetAimTargetRadiusPos( vecWeaponPos ), MASK_OPAQUE, &traceFilter, &tr2 );
						bool bHasLOS = (!tr2.startsolid && (tr2.fraction >= 1.0 || tr2.m_pEnt == pEnt));
						if ( !bHasLOS )
						{
							// just skip aliens that we can't shoot to
							continue;
							//dist += 50.0f;		// bias against aliens that we don't have LOS to - we'll only aim up/down at them if we have no other valid targets
						}

						if ( dist < best_d || best_d == -1 )
						{
							best_d = dist;
							pBestAlien = pAimTarget;
							bestAlienScreenPos = alienScreenPos;
							bBestAlienUsingFlareAutoaim = bFlareAutoaim;
							flBestAlienRadius = alien_radius;
						}
					}
				}
			}
			else
			{
				if ( asw_DebugAutoAim.GetBool() )
					ASW_StoreLineCircle( iStoreNum++, alienScreenPos.x, alienScreenPos.y, alien_radius, marineScreenPos.x, marineScreenPos.y, LineDir, 0 );
			}
		}
	}

	// we found something to aim at
	if ( pBestAlien )
	{
		if ( asw_DebugAutoAim.GetBool() )
		{
			debugoverlay->AddLineOverlay( pBestAlien->GetAimTargetPos(vecWeaponPos, bPreferFlatAiming), pBestAlien->GetAimTargetPos(vecWeaponPos, bPreferFlatAiming) + Vector(10,10,1),
				0, 0, 255, true, 0.01f );
		}

		fFloorZ = pBestAlien->GetAimTargetPos( vecWeaponPos, bPreferFlatAiming ).z;
		pAutoAimEnt = pBestAlien;
	}

	// in controller mode, if we have an alien to autoaim at, then aim directly at it
	if ( ASWInput()->ControllerModeActive() && pBestAlien )
	{
		HitLocation = pBestAlien->GetAimTargetPos( vecWeaponPos, bPreferFlatAiming );

		if ( asw_DebugAutoAim.GetBool() )
		{			
			C_BaseEntity *pEnt = pBestAlien ? pBestAlien->GetEntity() : NULL;
			engine->Con_NPrintf( nDebugLine++, "BestAlien = %d (%s)", pEnt ? pEnt->entindex() : 0, pEnt ? pEnt->GetClassname() : "NULL" );
			engine->Con_NPrintf( nDebugLine++, "CONTROLLER AA" );
		}

		pAutoAimEnt = pBestAlien;
		return NULL;
	}

	if ( asw_DebugAutoAim.GetBool() )
	{
		C_BaseEntity *pEnt = pBestAlien ? pBestAlien->GetEntity() : NULL;
		engine->Con_NPrintf( nDebugLine++, "BestAlien = %d (%s)", pEnt ? pEnt->entindex() : 0, pEnt ? pEnt->GetClassname() : "NULL" );
		engine->Con_NPrintf( nDebugLine++, "fFloorZ = %f", fFloorZ );
	}

	// calculate where our trace direction intersects with a simulated floor
	Vector vecFlatAim;
	float trace_dist_to_ground = 0;
	if (vWorldSpaceCameraToCursor.z != 0)
	{
		pPlayer->SmoothAimingFloorZ(fFloorZ);
		trace_dist_to_ground = -(vCameraLocation.z - fFloorZ) / vWorldSpaceCameraToCursor.z;
		vecFlatAim = vCameraLocation + vWorldSpaceCameraToCursor * trace_dist_to_ground;
		
		// just do a little bit of additional x/y auto aim
		Vector vecHitLoc = vecFlatAim;
		if ( pBestAlien && asw_horizontal_autoaim.GetBool() && pBestAlien->GetEntity() && pBestAlien->GetEntity()->IsNPC() )
		{
			if ( bWeaponHasRadiusScale || bBestAlienUsingFlareAutoaim )
			{
				Vector vecAimTargetPos = pBestAlien->GetAimTargetPos( vecWeaponPos, bPreferFlatAiming );
				vecHitLoc.x = vecAimTargetPos.x;
				vecHitLoc.y = vecAimTargetPos.y;
				if ( asw_DebugAutoAim.GetBool() )
				{
					Vector vMarineForward, vMarineRight, vMarineUp;
					QAngle ang = pPlayer->EyeAngles();
					ang[PITCH] = 0;
					ang[ROLL] = 0;
					AngleVectors( ang, &vMarineForward, &vMarineRight, &vMarineUp );

					Vector vecDebugStartPos = pMarine->GetRenderOrigin()
						+ vMarineForward * ASW_MARINE_GUN_OFFSET_X
						+ vMarineRight * ASW_MARINE_GUN_OFFSET_Y 
						+ vMarineUp * ASW_MARINE_GUN_OFFSET_Z;
					debugoverlay->AddLineOverlay( vecDebugStartPos, vecHitLoc,
						12, 255, 255, true, 0.01f);
				}
				ASWInput()->SetAutoaimEntity( pBestAlien->GetEntity() );
			}
		}

		HitLocation = vecHitLoc;

		return pHighlightAlien ? pHighlightAlien->GetEntity() : NULL;
	}
	
	// fall back to a simple trace into the world, reporting the collision point
	UTIL_TraceLine(vCameraLocation, vTraceEnd, nTraceMask, pPlayer, COLLISION_GROUP_NONE, &tr);
	if ( tr.fraction >= 1.0f )
		return pHighlightAlien ? pHighlightAlien->GetEntity() : NULL;
	

	HitLocation = vCameraLocation + tr.fraction * 3000 * vWorldSpaceCameraToCursor;
	HitLocation.z = 0;	

	return pHighlightAlien ? pHighlightAlien->GetEntity() : NULL;
}

// rounds world coordinate to a screen pixel
//    works by tracing from the camera to the position as a screen coordinate
void RoundToPixel(Vector &vecPos)
{
	Vector vecScreenPos;
	debugoverlay->ScreenPosition(vecPos, vecScreenPos);
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	QAngle CameraAngle;
	Vector CamResult;
	::input->CAM_GetCameraOffset( CamResult );
	CameraAngle[ PITCH ] = CamResult[ PITCH ];
	CameraAngle[ YAW ] = CamResult[ YAW ];
	CameraAngle[ ROLL ] = 0;

	float fRatio = float( ScreenHeight() ) / float( ScreenWidth() );
	Vector vCameraLocation = pPlayer->m_vecLastCameraPosition;
	Vector X, Y, Z;
	AngleVectors(CameraAngle, &X, &Y, &Z);
	float FOVAngle = pPlayer->GetFOV();
	Vector projected = X 
				- tanf(FOVAngle*PI/180*0.5) * 2 * Y * (vecScreenPos.x) * ( 0.75f / fRatio )
		          + tanf(FOVAngle*PI/180*0.5) * 2 * Z * (vecScreenPos.y) * 0.75f;

	projected.NormalizeInPlace();
	
	float trace_dist_to_ground = 0;
	if (projected.z != 0)
	{
		trace_dist_to_ground = -(vCameraLocation.z - vecPos.z) / projected.z;
		vecPos = vCameraLocation + projected * trace_dist_to_ground;
	}
}

// smoothly rotates the current marine's turning yaw to the desired
void SmoothTurningYaw(CASW_Player *pPlayer, float &yaw)
{
	if (!pPlayer || !pPlayer->GetMarine())
		return;

	// no change if we have no marine or just starting out or just changed marine
	if (pPlayer->m_vecLastCameraPosition == vec3_origin)
	{
		pPlayer->GetMarine()->m_fLastTurningYaw = yaw;
		pPlayer->m_hLastTurningMarine = pPlayer->GetMarine();
		return;
	}

	float dt = MIN( 0.2, gpGlobals->frametime );

	// fraction turning method	
	
	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	Assert(pMarine);
	C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();	

	float fLinearTurnRate = asw_marine_linear_turn_rate.GetFloat();

	if ( pMarine->GetActiveASWWeapon() )
	{
		fLinearTurnRate *= pMarine->GetActiveASWWeapon()->GetTurnRateModifier();
	}

	if ( asw_marine_fraction_turn_scale.GetFloat() == 0 )
	{
		yaw = ASW_ClampYaw( fLinearTurnRate, pMarine->m_fLastTurningYaw, yaw, dt );	// linear turning method
	}
	else
	{	
		// fractional turning system (not currently used)
		float fFraction = 0.9f;
		if (pWeapon)
		{
			if (pWeapon->m_bIsFiring)
			{
				fFraction *= asw_marine_turn_firing_fraction.GetFloat(); //pWeapon->m_fFiringTurnRateModifier;
			}
			else
			{
				fFraction *= asw_marine_turn_normal_fraction.GetFloat(); //pWeapon->m_fTurnRateModifier;
			}
		}

		float old_yaw = yaw;
		yaw = ASW_ClampYaw_Fraction( 1.0f - fFraction, pMarine->m_fLastTurningYaw, yaw, dt * asw_marine_fraction_turn_scale.GetFloat() );
		
		float min_move = fLinearTurnRate * dt;
		// make sure we're moving at least the minimum yaw speed
		if (abs(AngleDiff(yaw, old_yaw)) < min_move)
		{
			yaw = ASW_ClampYaw( fLinearTurnRate, pMarine->m_fLastTurningYaw, old_yaw, dt );	// linear turning method
		}
	}
				
	pMarine->m_fLastTurningYaw = yaw;
	pPlayer->m_hLastTurningMarine = pMarine;
}

CASWInput::CASWInput() : 
CInput(),
m_bCursorPlacement( false ),
m_nRelativeCursorX( 0 ),
m_nRelativeCursorY( 0 ),
m_flDesiredCursorRadius( 0.0f ),
m_flTimeSinceLastTurn( 0.0f ),
//m_MouseOverGlowObject( NULL, Vector( 1.0f, 0.5f, 0.0f ), 0.5f, true, true ),
m_HighLightGlowObject( NULL, Vector( 0.4f, 0.7f, 0.9f ), 0.8f, true, true ),
m_UseGlowObject( NULL, Vector( 0.4f, 0.7f, 0.9f ), 0.8f, true, true )
{
	m_fJoypadPitch = 0;
	m_fJoypadYaw = 0;
	m_fJoypadFacingYaw = 0;
	m_LastMouseX = -1;
	m_LastMouseY = -1;
	m_bDontTurnMarine = true;
	m_hLastMarine = NULL;
	m_vecCrosshairAimingPos = vec3_origin;
	m_vecCrosshairTracePos = vec3_origin;
	m_bAutoAttacking = false;

	cl_entitylist->AddListenerEntity( this );

#ifdef _WIN32
// 	HWND mainWnd = (HWND)g_pEngineWindow->GetWindowHandle();
// 	RECT windowClip;
// 	GetWindowRect( mainWnd, &windowClip );
// 	ClipCursor( &windowClip );
#endif
}

void CASWInput::ComputeNewMarineFacing( C_ASW_Player *pPlayer, const Vector &HitLocation, C_BaseEntity *pHitEnt, IASW_Client_Aim_Target* pAutoAimEnt, bool bPreferFlatAiming, float *pPitch, Vector *pNewMarineFacing )
{
	*pPitch = 0;
	if ( pPlayer && pPlayer->GetMarine() )
	{	
		C_ASW_Marine *pMarine = pPlayer->GetMarine();

		Vector vecMarinePos = (pMarine->GetRenderOrigin() + Vector( 0,0,ASW_MARINE_GUN_OFFSET_Z ) );

		m_vecCrosshairAimingPos = HitLocation;
		if ( !pHitEnt && pAutoAimEnt )
			SetMouseOverEntity( pAutoAimEnt->GetEntity() );
		else
			SetMouseOverEntity( pHitEnt );

		Vector vMarineForward, vMarineRight, vMarineUp;
		QAngle ang = pPlayer->EyeAngles();
		ang[PITCH] = 0;
		ang[ROLL] = 0;
		AngleVectors( ang, &vMarineForward, &vMarineRight, &vMarineUp );

		C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();

		// Marine position already has Z offset added to it,
		// choose a reasonable default in case pWeapon is NULL
		Vector vMuzzlePosition = vecMarinePos + 
			vMarineForward * ASW_MARINE_GUN_OFFSET_X +
			vMarineRight * ASW_MARINE_GUN_OFFSET_Y;

		// Get the muzzle position on the gun (where the laser sight comes from)
		if ( pWeapon )
		{
			int nAttachment = pWeapon->GetMuzzleAttachment();
			pWeapon->GetAttachment( nAttachment, vMuzzlePosition );
		}

		Vector vWeaponOffset = vMuzzlePosition - vecMarinePos;

		// Vector from the marine's rough center to the world space cursor 
		Vector vMarineToTarget = HitLocation - vecMarinePos;
		Vector vUnitUp = vMarineUp.Normalized();

		// How far to the marine's right is the gun?
		float flMarineGunOffsetY = vWeaponOffset.Dot( vMarineRight );

		// Ignore height differences when computing desired yaw; subtract out vertical component
		Vector vMarineToTargetUp = vMarineToTarget.Dot( vUnitUp ) * vUnitUp;
		Vector vMarineToTargetFlat = vMarineToTarget - vMarineToTargetUp;

		// If target is closer than threshold, offset the target position by at least this much to avoid twitchy angle changes
		const float flMinRadius = 300.0f;
		float flGunToTargetDistance = vMarineToTargetFlat.LengthSqr() - flMarineGunOffsetY * flMarineGunOffsetY;
		if ( flGunToTargetDistance < flMinRadius * flMinRadius )
		{
			float flLength = vMarineToTargetFlat.NormalizeInPlace();
			if ( flLength < 1.0f )
			{
				vMarineToTargetFlat = vMarineForward;
			}
			vMarineToTargetFlat *= flMinRadius;
			flGunToTargetDistance = vMarineToTargetFlat.LengthSqr() - flMarineGunOffsetY * flMarineGunOffsetY;

			if ( flGunToTargetDistance < 0.0f ) 
			{
				flGunToTargetDistance = flMinRadius;
			}
		}
		flGunToTargetDistance = sqrtf( flGunToTargetDistance );

		// Compute (along x-y plane) the marine's new forward vector such that a gun, offset to his right and pointed in the same direction, will shoot at the crosshair.
		// You can visualize this by constructing a right triangle: hypotenuse is the vector from marine's center to target, short leg is from marine's center to bullet start position, medium leg is from bullet start to target.
		// Assuming the bullet is fired in the marine's "forward" direction, we can solve for that vector with this math
		Assert( flMarineGunOffsetY != flGunToTargetDistance );
		float flNewForwardX = ( flMarineGunOffsetY * vMarineToTargetFlat.y - flGunToTargetDistance * vMarineToTargetFlat.x ) / ( flMarineGunOffsetY * flMarineGunOffsetY - flGunToTargetDistance * flGunToTargetDistance );
		float flNewForwardY = ( flMarineGunOffsetY * vMarineToTargetFlat.x + flGunToTargetDistance * vMarineToTargetFlat.y ) / ( flMarineGunOffsetY * flMarineGunOffsetY - flGunToTargetDistance * flGunToTargetDistance );

		// New forward/right vectors in world-space along the XY-plane
		Vector vNewForward( flNewForwardX, -flNewForwardY, 0 );
		Vector vNewRight( -flNewForwardY, -flNewForwardX, 0 );

		*pNewMarineFacing = vNewForward;

		Vector vecHitPos = HitLocation;
		if (pAutoAimEnt)
		{
			//vecHitPos = pAutoAimEnt->GetAimTargetPos(vMuzzlePosition, bPreferFlatAiming);	
		}

		// Use the standard bullet start position to try and figure out the exact weapon pitch needed
		vecMarinePos = pMarine->GetRenderOrigin()
			+ vMarineForward * ASW_MARINE_GUN_OFFSET_X
			+ vMarineRight * ASW_MARINE_GUN_OFFSET_Y 
			+ vMarineUp * ASW_MARINE_GUN_OFFSET_Z;

		// The yaw of this vector is not quite right (hence "approximate") but the pitch is correct
		Vector vApproximateMarineFacingVector = vecHitPos - vecMarinePos;
		// Approximate a better laser sight direction; this will be used so long as it does not deviate from the marine's forward direction by too much
		Vector vecLaserDir = ( vecHitPos - vMuzzlePosition );
		pMarine->m_flLaserSightLength = vecLaserDir.NormalizeInPlace();
		//pMarine->m_vLaserSightCorrection = vecLaserDir - vNewForward;
		pMarine->m_vLaserSightCorrection = vecLaserDir;

		if ( fabsf( vApproximateMarineFacingVector.z ) > 1e-3f )
		{					
			if (asw_DebugAutoAim.GetInt()==3)
				FX_MicroExplosion(vecHitPos, Vector(0,0,1));					

			*pPitch = UTIL_VecToPitch(vApproximateMarineFacingVector);
			if ( !IsFinite( *pPitch ) )
				*pPitch = 0;

			if (asw_DebugAutoAim.GetInt() == 2)
			{
				Msg("[%s]%f: Setting pitch to %f (marine z:%f hitlocZ:%f\n", pPlayer->IsClient() ? "c" : "s",
					gpGlobals->curtime, *pPitch, vMuzzlePosition.z, vecHitPos.z);
			}
			if (asw_DebugAutoAim.GetBool())
			{
				debugoverlay->AddLineOverlay(vecHitPos, vMuzzlePosition,
					255, 255, 0, true, 0.01f);
			}
		}
	}
}

// asw
void CASWInput::TurnTowardMouse(QAngle& viewangles, CUserCmd *cmd)
{
	VPROF_BUDGET( "CASWInput::TurnTowardMouse", VPROF_BUDGETGROUP_ASW_CLIENT );

	if ( !engine->IsActiveApp() )
	{
		return;
	}

	float mx, my;

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();	
	C_ASW_Marine *pMarine = pPlayer ? pPlayer->GetMarine() : NULL;

	bool bPreferFlatAiming = false;

	int iScreenWidth, iScreenHeight;
	engine->GetScreenSize( iScreenWidth, iScreenHeight );

	if ( iScreenWidth <= 0 || iScreenHeight <= 0 )
	{
		return;
	}

	int x, y;
	GetWindowCenter( x,  y );

	int current_posx, current_posy;
	GetMousePos(current_posx, current_posy);

	//current_posy -= ( 0.020833f * iScreenHeight ); // slight vertical correction to account for the gun being lower than the marine's eyes

	// don't change the marine's yaw if the mouse cursor hasn't moved and marine is still
	//  (this allows the player to switch between his marines without messing up their facing)
	if (pPlayer && pPlayer->GetMarine())
	{
		if (pPlayer->GetMarine() != m_hLastMarine)
		{
			m_bDontTurnMarine = true;
		}
		m_hLastMarine = pPlayer->GetMarine();
		if (current_posx != m_LastMouseX || current_posy != m_LastMouseY || pPlayer->GetMarine()->GetAbsVelocity().LengthSqr() > 10)
		{
			m_bDontTurnMarine = false;
			m_LastMouseX = current_posx;
			m_LastMouseY = current_posy;			
		}
		else
		{
			pPlayer->GetMarine()->m_bUseLastRenderedEyePosition = true;
		}

		C_ASW_Weapon *pWeapon = pPlayer->GetMarine()->GetActiveASWWeapon();
		if (pWeapon && pWeapon->PrefersFlatAiming())
		{
			bPreferFlatAiming = true;
		}
	}

	mx = current_posx - x;
	my = current_posy - y;

	Vector vNewMarineFacing;
	vNewMarineFacing.x = mx;
	vNewMarineFacing.y = -my;
	vNewMarineFacing.z = 0;

	Vector HitLocation;
	float mx_ratio =((float) mx) / ((float) x);
	float my_ratio =((float) my) / ((float) y);

	// grab the location of the world directly underneath the crosshair
	HUDTraceToWorld( -mx_ratio * 0.5f, -my_ratio * 0.5f, m_vecCrosshairTracePos );
	
	IASW_Client_Aim_Target* pAutoAimEnt = NULL;
	C_BaseEntity *pHitEnt = HUDToWorld(-mx_ratio * 0.5f, -my_ratio * 0.5f, HitLocation, pAutoAimEnt, bPreferFlatAiming, false, cmd->forwardmove, cmd->sidemove);
	float pitch = 0;
	ComputeNewMarineFacing( pPlayer, HitLocation, pHitEnt, pAutoAimEnt, bPreferFlatAiming, &pitch, &vNewMarineFacing );

	float yaw = UTIL_VecToYaw( vNewMarineFacing );
	if ( !IsFinite( yaw ) )
		yaw = 0;	

	// make our yaw look towards a particular item if we're using it or towards the attached entity
	if ( pPlayer && pPlayer->GetMarine() )
	{
		CBaseEntity* pUsing = pPlayer->GetMarine()->m_hUsingEntity;
		if (pUsing)
		{
			vNewMarineFacing = pUsing->GetAbsOrigin() - pPlayer->GetMarine()->GetRenderOrigin();
			vNewMarineFacing.z = 0;
			yaw = UTIL_VecToYaw(vNewMarineFacing);
			if (!IsFinite(yaw))
				yaw = 0;			
		}
		else if ( pMarine->GetFacingPoint() != vec3_origin )
		{
			vNewMarineFacing = pMarine->GetFacingPoint() - pMarine->GetRenderOrigin();
			vNewMarineFacing.z = 0;
			yaw = UTIL_VecToYaw( vNewMarineFacing );
			if ( !IsFinite( yaw ) )
				yaw = 0;			
		}
		else if ( pMarine->GetCurrentMeleeAttack() && !pMarine->GetCurrentMeleeAttack()->m_bAllowRotation )
		{
			// lock facing to our melee animation unless it's relinquished movement control
			yaw = pMarine->m_flMeleeYaw;
		}

		// if we're not meant to be turning the marine, then just use his current yaw
		if (m_bDontTurnMarine)
		{
			yaw = pMarine->GetAbsAngles()[YAW];
			pMarine->m_fLastTurningYaw = yaw;			
		}
	}	
	
	// blend our current marine's yaw to the desired, taking into account weapon turn rates, etc	
	if ( !m_bDontTurnMarine )
		SmoothTurningYaw( pPlayer, yaw );
			
	viewangles[YAW] = yaw;
	viewangles[PITCH] = pitch;

	// set the roll so the marine is looking at the cursor's location at foot level	
	float dist = sqrt((mx * mx) + (my * my));
	float sx = ScreenWidth() * 0.5f;
	float sy = ScreenHeight() * 0.5f;
	float max_dist = sqrt((sx * sx) + (sy * sy));
	float ground_x = (dist / max_dist) * 500.0f;
	float marine_h = 50.0f;

	viewangles[ROLL] = 90 - RAD2DEG(atan(ground_x/marine_h));

	if (pPlayer->GetMarine())
		pPlayer->GetMarine()->m_bUseLastRenderedEyePosition = false;
}

void CASWInput::TurnTowardController(QAngle& viewangles)
{
	VPROF_BUDGET( "CASWInput::TurnTowardController", VPROF_BUDGETGROUP_ASW_CLIENT );
	float mx, my;

	int x, y;
	GetWindowCenter( x,  y );

	int current_posx = 0;
	int current_posy = 0;
	GetSimulatedFullscreenMousePos(&current_posx, &current_posy);
	
	mx = current_posx - x;
	my = current_posy - y;

	Vector vNewMarineFacing;
	vNewMarineFacing.x = mx;
	vNewMarineFacing.y = -my;
	vNewMarineFacing.z = 0;

	Vector HitLocation;
	float mx_ratio =((float) mx) / ((float) x);
	float my_ratio =((float) my) / ((float) y);
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	C_ASW_Marine *pMarine = pPlayer ? pPlayer->GetMarine() : NULL;
	bool bPreferFlatAiming = false;

	// grab the location of the world directly underneath the crosshair
	HUDTraceToWorld( -mx_ratio * 0.5f, -my_ratio * 0.5f, m_vecCrosshairTracePos );

	if (pPlayer && pPlayer->GetMarine())
	{
		C_ASW_Weapon *pWeapon = pPlayer->GetMarine()->GetActiveASWWeapon();
		if (pWeapon && pWeapon->PrefersFlatAiming())
		{
			bPreferFlatAiming = true;
		}
	}
	IASW_Client_Aim_Target* pAutoAimEnt = NULL;
	C_BaseEntity *pHitEnt = HUDToWorld(-mx_ratio * 0.5f, -my_ratio * 0.5f, HitLocation, pAutoAimEnt, bPreferFlatAiming, true );
	float pitch = 0;

	ComputeNewMarineFacing( pPlayer, HitLocation, pHitEnt, pAutoAimEnt, bPreferFlatAiming, &pitch, &vNewMarineFacing );

	float yaw = UTIL_VecToYaw(vNewMarineFacing);
	if (!IsFinite(yaw))
		yaw = 0;
	m_fJoypadFacingYaw = yaw;

	// make our yaw look towards a particular item if we're using it
	if (pPlayer && pPlayer->GetMarine())
	{
		if (pPlayer->GetMarine()->GetFacingPoint() != vec3_origin)
		{
			vNewMarineFacing = pPlayer->GetMarine()->GetFacingPoint() - pPlayer->GetMarine()->GetRenderOrigin();
			vNewMarineFacing.z = 0;
			yaw = UTIL_VecToYaw(vNewMarineFacing);
			if (!IsFinite(yaw))
				yaw = 0;			
		}
		else if ( pMarine->GetCurrentMeleeAttack() && !pMarine->GetCurrentMeleeAttack()->m_bAllowRotation )
		{
			// lock facing to our melee animation unless it's relinquished movement control
			yaw = pMarine->m_flMeleeYaw;
		}
	}
	// blend our current marine's yaw to the desired, taking into account weapon turn rates, etc
	SmoothTurningYaw(pPlayer, yaw);
	
	viewangles[YAW] = yaw;
	viewangles[PITCH] = pitch;

	// set the roll so the marine is looking at the cursor's location at foot level	
	float dist = sqrt((mx * mx) + (my * my));
	float sx = ScreenWidth() * 0.5f;
	float sy = ScreenHeight() * 0.5f;
	float max_dist = sqrt((sx * sx) + (sy * sy));
	float ground_x = (dist / max_dist) * 500.0f;
	float marine_h = 50.f;
	viewangles[ROLL] = 90 - RAD2DEG(atan(ground_x/marine_h));

	if (pPlayer->GetMarine())
		pPlayer->GetMarine()->m_bUseLastRenderedEyePosition = false;
}

void SmoothControllerYaw(CASW_Player *pPlayer, float &yaw)
{
	if (!pPlayer || !pPlayer->GetMarine())
		return;
	// no change if we have no marine or just starting out or just changed marine
	static C_ASW_Marine *s_pLastJoypadMarine = NULL;
	static float s_fLastJoypadYaw = 0;
	if (pPlayer->GetMarine()!=s_pLastJoypadMarine || pPlayer->m_vecLastCameraPosition == vec3_origin)
	{
		s_fLastJoypadYaw = yaw;
		s_pLastJoypadMarine = pPlayer->GetMarine();
		return;
	}

	float dt = MIN( 0.2, gpGlobals->frametime );
#if 0
	
#endif

	// fraction turning method
	float fFraction = 0.9f;
	float fLinearTurnRate = asw_marine_linear_turn_rate.GetFloat();
	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	Assert(pMarine);
	C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();	
	if (pWeapon)
	{
		if (pWeapon->m_bIsFiring)
		{
			fFraction *= asw_marine_turn_firing_fraction.GetFloat(); //pWeapon->m_fFiringTurnRateModifier;
		}
		else
		{
			fFraction *= asw_marine_turn_normal_fraction.GetFloat(); //pWeapon->m_fTurnRateModifier;
		}
	}
	if (asw_marine_fraction_turn_scale.GetFloat() == 0)
	{
		yaw = ASW_ClampYaw( fLinearTurnRate, s_fLastJoypadYaw, yaw, dt );	// linear turning method
	}
	else
	{	
		float old_yaw = yaw;
		yaw = ASW_ClampYaw_Fraction( 1.0f - fFraction, s_fLastJoypadYaw, yaw, dt * asw_marine_fraction_turn_scale.GetFloat() );
		
		float min_move = fLinearTurnRate * dt;
		// make sure we're moving at least the minimum yaw speed
		if (abs(AngleDiff(yaw, old_yaw)) < min_move)
		{
			yaw = ASW_ClampYaw( fLinearTurnRate, s_fLastJoypadYaw, old_yaw, dt );	// linear turning method
		}
	}
				
	s_fLastJoypadYaw = yaw;
	s_pLastJoypadMarine = pMarine;
}

// Returns the mouse cursor location.  If in controller mode we simulate a cursor position based on the analogue stick.
void CASWInput::GetSimulatedFullscreenMousePos( int *mx, int *my, int *unclampedx /*=NULL*/, int *unclampedy /*=NULL*/ )
{	
	if ( ASWInput()->ControllerModeActive() )
	{
		GetSimulatedFullscreenMousePosFromController( mx, my, m_fJoypadPitch, m_fJoypadYaw );
	}
	else
	{
		GetFullscreenMousePos( mx, my, unclampedx, unclampedy );
	}
}

float MoveToward( float cur, float goal, float lag );

ConVar asw_controller_lag( "asw_controller_lag", "40.0", FCVAR_NONE );

void CASWInput::GetSimulatedFullscreenMousePosFromController( int *mx, int *my, float fControllerPitch, float fControllerYaw, float flForwardFraction )
{
	int x, y;
	GetWindowCenter( x,  y );

	if ( m_bCursorPlacement )
	{
		*mx = x + m_nRelativeCursorX;
		*my = y + m_nRelativeCursorY;
	}
	else
	{
		// simulate a mouse position based on the direction the analogue stick was pushed last
		static float last_joy_yaw = 0;
		Vector vecJoyLook( fControllerPitch, fControllerYaw, 0 );
		float length = vecJoyLook.Length();
		float joy_yaw = UTIL_VecToYaw( vecJoyLook );
		if ( !IsFinite( joy_yaw ) || length < 0.3f )	// if the player isn't really pushing the right stick in any direction, look the same way we looked last
			joy_yaw = last_joy_yaw;

		float lag = MAX( 1, 1 + asw_controller_lag.GetFloat() );
		joy_yaw = MoveToward( last_joy_yaw, joy_yaw, lag );
		last_joy_yaw = joy_yaw;
		joy_yaw = ( 360.0f - joy_yaw ) + 90.0f;
		if ( joy_yaw > 360 )
			joy_yaw -= 360.0f;
		//SmoothControllerYaw( C_ASW_Player::GetLocalASWPlayer(), joy_yaw );
		if ( asw_DebugAutoAim.GetBool() )
		{
			Msg( "joy yaw %f len %f p %f y %f last %f ", joy_yaw, length, m_fJoypadPitch, m_fJoypadYaw, last_joy_yaw );	
			Msg( "cos %f sin %f\n", cos(DEG2RAD(joy_yaw)), sin(DEG2RAD(joy_yaw)) );
		}
		// float dist_fraction = 1.0f;	// always put crosshair a fixed distance from the marine
		//float dist_fraction = length;
		//if (dist_fraction < 0.9f)
		//dist_fraction = 0.9f;
		int nScreenMin = MIN( ScreenWidth(), ScreenHeight() );
		*mx = x + ( ( nScreenMin * flForwardFraction ) * cos( DEG2RAD( joy_yaw ) ) );
		*my = y + ( ( nScreenMin * flForwardFraction ) * sin( DEG2RAD( joy_yaw ) ) );
	}
}

bool MarineBusy()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer && pPlayer->GetMarine() )	
	{
		//|| pPlayer->GetMarine()->IsUsingComputerOrButtonPanel()
		if ( g_asw_iPlayerListOpen > 0 || CASW_VGUI_Info_Message::HasInfoMessageOpen() )
			return true;
	}

	return false;
}

// asw - check if we're driving or not, since this changes controls
bool PlayerDriving()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	return (pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->IsDriving());
}

// asw - make sure our vgui joypad focus panel knows which buttons we're using for up/down/left/right
void ASW_UpdateControllerCodes()
{
	if ( GetControllerFocus() )
	{
		// find joystick convars
		ConVar* axes[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
		axes[0] = cvar->FindVar("joy_advaxisx");
		axes[1] = cvar->FindVar("joy_advaxisy");
		axes[2] = cvar->FindVar("joy_advaxisz");
		axes[3] = cvar->FindVar("joy_advaxisr");
		axes[4] = cvar->FindVar("joy_advaxisu");
		axes[5] = cvar->FindVar("joy_advaxisv");
		// find up/down/left/right axes
		int iForward = 1;
		int iSide = 0;
		for (int i=0;i<6;i++)
		{
			if (axes[i])
			{
				if (axes[i]->GetInt() == 1)
				{
					iForward = i;				
				}
				else if (axes[i]->GetInt() == 3)
				{
					iSide = i;
				}
			}
		}
		// notify vgui
		// JOYPAD REMOVED
		//GetControllerFocus()->SetJoypadCodes(
		//	K_AXISX_POS + (iForward*2) + 1, K_AXISX_POS + (iForward*2),	// up/down
		//	K_AXISX_POS + (iSide*2) + 1, K_AXISX_POS + (iSide*2),		// left/right
		//	K_JOY1, K_JOY2);	// confirm/cancel (assume is always joy1/2)

		GetControllerFocus()->SetControllerCodes(
			(ButtonCode_t) (JOYSTICK_FIRST_AXIS_BUTTON + (iForward*2) + 1),
			(ButtonCode_t) (JOYSTICK_FIRST_AXIS_BUTTON + (iForward*2) ),	// up/down
			(ButtonCode_t) (JOYSTICK_FIRST_AXIS_BUTTON + (iSide*2) + 1 ),
			(ButtonCode_t) (JOYSTICK_FIRST_AXIS_BUTTON + (iSide*2) ),		// left/right
			JOYSTICK_FIRST_BUTTON,
			(ButtonCode_t) (JOYSTICK_FIRST_BUTTON+1) );	// confirm/cancel (assume is always joy1/2)		
	}
}


const char* ASW_FindKeyBoundTo(const char *binding)
{
	// JOYPAD REMOVED
	//const char* pKeyText = engine->Key_LookupBindingEx(binding, ASWInput()->ControllerModeActive());
	const char *pKeyText = engine->Key_LookupBinding( binding );
	if ( !pKeyText )
	{
		return "<NOT BOUND>";
	}
	return MakeHumanReadable(pKeyText);
}

const char* MakeHumanReadable(const char *key)
{
	// JOYPAD REMOVED
	// if (ASWInput()->ControllerModeActive() && inputsystem->Is360WindowsPad(0))	// NOTE: assumes they're using joypad 0
	if ( 0 )
	{
		for (int i=0;i<ASW_NUM_360_READABLE;i++)
		{
			if (!Q_stricmp(s_360Keynames[i][0], key))
			{
				return s_360Keynames[i][1];
			}
		}
	}	
	for (int i=0;i<ASW_NUM_HUMAN_READABLE;i++)
	{
		if (!Q_stricmp(s_HumanKeynames[i][0], key))
		{
			return s_HumanKeynames[i][1];
		}
	}
	char friendlyName[64];
	Q_snprintf( friendlyName, sizeof(friendlyName), "#%s", key );
	Q_strupr( friendlyName );
	return key;
}

// this alters the viewangle to aim at the ground (normally weapons aim parallel to the ground, rather than shooting into it)
//   used when player is firing grenades with the 'shoot at ground' option turned on (so they can make use of splash damage, etc.)
void ASW_AdjustViewAngleForGroundShooting( QAngle &viewangles )
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	// first check if we're using a weapon that supports ground alt-shooting
	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if ( !pMarine )
		return;

	// Don't adjust if they already have a near target
	IASW_Client_Aim_Target *pTarget = dynamic_cast< IASW_Client_Aim_Target* >( ASWInput()->GetMouseOverEntity() );
	if ( pTarget && pTarget->GetEntity()->GetAbsOrigin().DistTo( pMarine->GetAbsOrigin() ) < 512.0f )
	{
		return;
	}

	C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();	
	if ( !pWeapon || !pWeapon->SupportsGroundShooting() || gpGlobals->curtime < pWeapon->m_flNextSecondaryAttack )
		return;
	
	// now find a spot to aim at
	int mousex, mousey;
	mousex = mousey = 0;
	if ( ASWInput()->ControllerModeActive() )
	{
		ASWInput()->GetSimulatedFullscreenMousePos( &mousex, &mousey );
	}
	else
	{
		GetVGUICursorPos( mousex, mousey );
	}
	
	int x, y;
	engine->GetScreenSize( x, y );
	x = x >> 1;
	y = y >> 1;

	float mx, my;
	mx = mousex - x;
	my = mousey - y;
	float mx_ratio =((float) mx) / ((float) x);
	float my_ratio =((float) my) / ((float) y);
	Vector vecGrenadePos = vec3_origin;
	HUDTraceToWorld(-mx_ratio * 0.5f, -my_ratio * 0.5f, vecGrenadePos);	// store the spot we'll send a marine to

	float fOriginalPitch = viewangles.x;
	float fYawMin = viewangles.y - 5.0f;
	float fYawMax = viewangles.y + 5.0f;

	if ( vecGrenadePos != vec3_origin )
	{
		Vector dir = vecGrenadePos - pMarine->Weapon_ShootPosition();
		dir.NormalizeInPlace();
		VectorAngles( dir, viewangles );

		// Don't let it change the pitch or yaw too much (aiming above walls or directly at feet)
		if ( fOriginalPitch > 180.0f )
		{
			fOriginalPitch -= 360.0f;
		}

		if ( viewangles.x > 180.0f )
		{
			viewangles.x -= 360.0f;
		}

		viewangles.x = clamp( viewangles.x, fOriginalPitch - 5.0f, fOriginalPitch + 15.0f );

		if ( viewangles.x < 0.0f )
		{
			viewangles.x += 360.0f;
		}

		float fLowDistance = 360.0f;
		float fHighDistance = 360.0f;
		bool bValid = false;

		if ( viewangles.y > fYawMin && viewangles.y < fYawMax )
		{
			bValid = true;
		}
		else
		{
			float fCurrentShiftUp = viewangles.y + 360.0f;
			float fCurrentShiftDown = viewangles.y - 360.0f;

			if ( fCurrentShiftUp > fYawMin && fCurrentShiftUp < fYawMax )
			{
				bValid = true;
			}
			else if ( fCurrentShiftDown > fYawMin && fCurrentShiftDown < fYawMax )
			{
				bValid = true;
			}
			else
			{
				fLowDistance = MIN( fLowDistance, fabsf( viewangles.y - fYawMin ) );
				fLowDistance = MIN( fLowDistance, fabsf( fCurrentShiftUp - fYawMin ) );
				fLowDistance = MIN( fLowDistance, fabsf( fCurrentShiftDown - fYawMin ) );

				fHighDistance = MIN( fHighDistance, fabsf( viewangles.y - fYawMax ) );
				fHighDistance = MIN( fHighDistance, fabsf( fCurrentShiftUp - fYawMax ) );
				fHighDistance = MIN( fHighDistance, fabsf( fCurrentShiftDown - fYawMax ) );
			}
		}

		if ( !bValid )
		{
			viewangles.y = ( fLowDistance < fHighDistance ? fYawMin : fYawMax );
			viewangles.y = AngleNormalize( viewangles.y );
		}
	}
}


/*
===========
ControllerMove
===========
*/
void CASWInput::ControllerMove( int nSlot, float frametime, CUserCmd *cmd )
{
	Assert( engine->IsLocalPlayerResolvable() );
	if ( engine->GetActiveSplitScreenPlayerSlot() == in_forceuser.GetInt() )
	{
		if ( IsPC() )
		{
			int current_posx, current_posy;
			GetMousePos(current_posx, current_posy);

			if ( ASWGameRules()->GetGameState() == ASW_GS_CAMPAIGNMAP )
			{
				// Convert to centered dimensions relative to the Y (0,0 is center 1,1 is half height to the right and bottom)
				m_vecCrosshairTracePos.x = ( static_cast<float>( current_posx - ( ScreenWidth() / 2 ) ) / ScreenHeight() );
				m_vecCrosshairTracePos.y = ( static_cast<float>( current_posy - ( ScreenHeight() / 2 ) ) / ScreenHeight() );
			}
			else
			{
				if ( !GetPerUser().m_fCameraInterceptingMouse && m_fMouseActive )
				{
					MouseMove( nSlot, cmd );
				}
			}

			static float joypad_start_mouse_x = 0;
			static float joypad_start_mouse_y = 0;

			if (ASWInput()->ControllerModeActive())
			{				
				// accumulate mouse movements and if we go over a certain threshold, switch out of controller mode
				float mouse_x_diff = current_posx - joypad_start_mouse_x;
				float mouse_y_diff = current_posy - joypad_start_mouse_y;
				float total_mouse_move = mouse_x_diff * mouse_x_diff + mouse_y_diff * mouse_y_diff;
				//Msg("total_mouse_move = %f\n", total_mouse_move);
				if (total_mouse_move > 1000)
				{
					ASWInput()->SetControllerMode( false );
					if (GetControllerFocus())
						GetControllerFocus()->SetControllerMode( false );
				}
			}
			else
			{
				joypad_start_mouse_x = current_posx;
				joypad_start_mouse_y = current_posy;
			}
		}
	}

	JoyStickMove( frametime, cmd);

	if ( engine->GetActiveSplitScreenPlayerSlot() == in_forceuser.GetInt() )
	{
		// TrackIR
		TrackIRMove(frametime, cmd);
		// TrackIR
	}

	UpdateHighlightEntity();
}

void CASWInput::SetControllerMode( bool bControllerMode ) 
{
	if ( m_bControllerMode == bControllerMode )
		return;

	m_bControllerMode = bControllerMode;
	if ( bControllerMode )
	{
		Msg("[Controller] Starting controller mode\n");
	}
	else
	{
		Msg("[Controller] Leaving controller mode\n");
	}
}

void CASWInput::EngageControllerMode() 
{
	ASWInput()->SetControllerMode( true );
	if ( GetControllerFocus() )
	{
		GetControllerFocus()->SetControllerMode( true );
	}
	static bool bSetControllerCodes = false;
	if ( !bSetControllerCodes )
	{
		bSetControllerCodes = true;
		ASW_UpdateControllerCodes();
	}
}

void CASWInput::JoyStickForwardSideControl( float forward, float side, float &joyForwardMove, float &joySideMove )
{
	float flAdjustedForward;
	float flAdjustedSide;

	if ( joy_tilted_view.GetBool() ) 
	{
		flAdjustedForward = side;
		flAdjustedSide = -forward;
	}
	else
	{
		flAdjustedForward = forward;
		flAdjustedSide = side;
	}

	CInput::JoyStickForwardSideControl( flAdjustedForward, flAdjustedSide, joyForwardMove, joySideMove );

	if (fabs(joyForwardMove) > 150  || fabs(joySideMove) > 150)
	{
		EngageControllerMode();
	}

	// Player is operating UI of some kind
	if ( joy_disable_movement_in_ui.GetBool() && GetControllerFocus()->GetFocusPanel() != NULL )
	{
		joyForwardMove = 0;
		joySideMove = 0;
		return;
	}

	// store the aiming axes, so in_mouse can use them to simulate a cursor position
	if ( m_flTimeSinceLastTurn > joy_aim_to_movement_time.GetFloat() && !m_bCursorPlacement )
	{
		if ( !joy_autoattack.GetBool() && IsAttacking() && joy_lock_firing_angle.GetBool() )	// lock firing angle
		{
			m_fJoypadPitch = 0;
			m_fJoypadYaw = 0;
		}
		else
		{
			m_fJoypadPitch = forward;	
			m_fJoypadYaw = side;
		}
	}
}

void CASWInput::JoyStickTurn( CUserCmd *cmd, float &yaw, float &pitch, float frametime, bool bAbsoluteYaw, bool bAbsolutePitch )
{
	if ( !ASWInput()->ControllerModeActive() )
		return;

	// Player is operating UI of some kind
	if ( joy_disable_movement_in_ui.GetBool() && GetControllerFocus()->GetFocusPanel() != NULL )
	{
		yaw = 0;
		pitch = 0;
		return;
	}

	// check for changing direction based on right analogue stick
	bool bFiringThreshold = false;
	if ( joy_autoattack.GetBool() )
	{
		float fDist = sqrt( yaw * yaw + pitch * pitch );
		bFiringThreshold = ( fDist >= joy_autoattack_threshold.GetFloat() );
	}

	float dt = MIN( 0.2, gpGlobals->frametime );

	// Amount of time we haven't been actively turning (right stick) with the gamepad.
	// Use this to snap player movement back to the movement direction after some time has passed.
	if ( yaw == 0.0f && pitch == 0.0f )
	{
		m_flTimeSinceLastTurn += dt;
	}
	else
	{
		m_flTimeSinceLastTurn = 0.0f;
	}

	float flCursorSpeed = joy_cursor_speed.GetFloat();
	float flCursorScale = joy_cursor_scale.GetFloat();
	float flRadiusSnapFactor = joy_radius_snap_factor.GetFloat();
	int nHalfScreenX = ScreenWidth() / 2;
	int nHalfScreenY = ScreenHeight() / 2;

	if ( m_bCursorPlacement )
	{
		int nHomeX, nHomeY; // Position the cursor wants to return to
		m_bCursorPlacement = false;
		GetSimulatedFullscreenMousePosFromController( &nHomeX, &nHomeY, m_fJoypadPitch, m_fJoypadYaw, m_flDesiredCursorRadius );
		m_bCursorPlacement = true;
		nHomeX -= nHalfScreenX;
		nHomeY -= nHalfScreenY;

		int nScreenMin = MIN( ScreenWidth(), ScreenHeight() );

		// Map the joystick thumbstick directly to a screen position relative to the "home" position (a set radius in front of the player)
		int nDesiredX = nScreenMin / 2 * yaw * flCursorScale + nHomeX;
		int nDesiredY = nScreenMin / 2 * pitch * flCursorScale + nHomeY;

		// Move the cursor with a spring-like force to its desired location
		m_nRelativeCursorX += ( nDesiredX - m_nRelativeCursorX ) * dt * flCursorSpeed;
		m_nRelativeCursorY += ( nDesiredY - m_nRelativeCursorY ) * dt * flCursorSpeed;
		m_nRelativeCursorX = clamp( m_nRelativeCursorX, -nHalfScreenX, nHalfScreenX );
		m_nRelativeCursorY = clamp( m_nRelativeCursorY, -nHalfScreenY, nHalfScreenY );

		// Set the facing direction for the character based on the cursor position
		m_fJoypadYaw = ( float )m_nRelativeCursorX / ( nScreenMin / 2 * flCursorScale );
		m_fJoypadPitch = ( float )m_nRelativeCursorY / ( nScreenMin / 2 * flCursorScale );

		// Over time, adjust the desired radial position of the cursor in or out based on its current location
		float nRadius = sqrtf( ( float )( m_nRelativeCursorX * m_nRelativeCursorX + m_nRelativeCursorY * m_nRelativeCursorY ) );
		nRadius /= ( float )nScreenMin;
		m_flDesiredCursorRadius += ( nRadius - m_flDesiredCursorRadius ) * dt * flRadiusSnapFactor;
	}
	else if ( m_flTimeSinceLastTurn <= joy_aim_to_movement_time.GetFloat() )
	{
		// face right analogue stick direction if it's pushed past the firing threshold, or we're not in 'aim to movement' mode
		m_fJoypadPitch = pitch;	
		m_fJoypadYaw = yaw;	
	}

	// Get view angles from engine
	QAngle	viewangles;	
	engine->GetViewAngles( viewangles );

	TurnTowardController( viewangles );

	// Store out the new viewangles.
	engine->SetViewAngles( viewangles );

	bool bAutoFire = false;
	if ( bFiringThreshold )
	{
		// check we're facing the right way
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if ( pPlayer )
		{
			C_ASW_Marine *pMarine = pPlayer->GetMarine();
			if ( pMarine && !MarineBusy() )
			{
				// compare to see if we're facing the right way
				float flAngle = AngleDiff( m_fJoypadFacingYaw, pPlayer->EyeAngles()[YAW] );
				if ( fabs( flAngle ) < joy_autoattack_angle.GetFloat() )
				{
					bAutoFire = true;
				}
			}
			//Msg( "%f: m_fJoypadFacingYaw = %f eyeangles = %f flAngle = %f\n", gpGlobals->curtime, m_fJoypadFacingYaw, pPlayer->EyeAngles()[YAW], fabs( flAngle ) );
		}
	}

	if ( bAutoFire && !m_bCursorPlacement )
	{
		if ( !m_bAutoAttacking )
		{
			KeyDown( &in_attack, NULL );
			m_bAutoAttacking = true;
		}
	}
	else if ( m_bAutoAttacking )
	{
		KeyUp( &in_attack, NULL );
		m_bAutoAttacking = false;
	}
}

// allow the joystick to be active even if VGUI cursor is up
bool CASWInput::JoyStickActive()
{
	// verify joystick is available and that the user wants to use it
	if ( !in_joystick.GetInt() || 0 == inputsystem->GetJoystickCount() )
		return false; 
	
	return true;
}

void CASWInput::JoyStickApplyMovement( CUserCmd *cmd, float joyForwardMove, float joySideMove )
{
	CInput::JoyStickApplyMovement( cmd, joyForwardMove, joySideMove );
}

void CASWInput::OnEntityDeleted( C_BaseEntity *pEntity )
{
	if ( pEntity )
	{
		if ( pEntity == m_hMouseOverEntity.Get() )
		{
			//m_MouseOverGlowObject.SetEntity( NULL );
		}
		if ( pEntity == m_hHighlightEntity.Get() )
		{
			m_HighLightGlowObject.SetEntity( NULL );
		}
		if ( pEntity == m_hUseGlowEntity.Get() )
		{
			m_UseGlowObject.SetEntity( NULL );
		}
	}
}

int CASWInput::CAM_IsThirdPerson( int nSlot )
{
	C_ASW_Marine *pMarine = C_ASW_Marine::GetLocalMarine();
	if ( pMarine && pMarine->IsControllingTurret() )
	{
		return 0;
	}
	
	return CInput::CAM_IsThirdPerson( nSlot );
}