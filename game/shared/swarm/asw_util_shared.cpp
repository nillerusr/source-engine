#include "cbase.h"
#include "asw_shareddefs.h"
#include "gamestringpool.h"
#ifdef GAME_DLL
	#include "asw_player.h"
	#include "asw_marine.h"
	#include "asw_marine_resource.h"
	#include "asw_gamerules.h"
	#include "asw_game_resource.h"
	#include "props.h"
	#include "vphysics_interface.h"
	#include "physics.h"
	#include "vphysics/friction.h"
	#include "asw_computer_area.h"
	#include "point_camera.h"
	#include "asw_remote_turret_shared.h"
	#include "asw_computer_area.h"
	#include "asw_button_area.h"
	#include "fogcontroller.h"
	#include "asw_point_camera.h"
#else
	#include "asw_gamerules.h"
	#include "c_asw_drone_advanced.h"
	#include "c_asw_fx.h"
	#include "c_asw_marine.h"
	#include "c_asw_game_resource.h"
	#include "c_asw_marine_resource.h"
	#include "c_asw_computer_area.h"
	#include "c_point_camera.h"
	#include "asw_remote_turret_shared.h"
    #include "c_asw_player.h"
	#include "c_playerresource.h"
	#include "c_asw_computer_area.h"
	#include "c_asw_button_area.h"
	#include "vgui/cursor.h"
	#include "iinput.h"
	#include <vgui/ISurface.h>
	#include "vguimatsurface/imatsystemsurface.h"
	#include "vgui_controls\Controls.h"
	#include <vgui/IVGUI.h>
	#include "ivieweffects.h"
	#include "asw_input.h"
	#include "c_asw_point_camera.h"
	#include "baseparticleentity.h"
	#include "vgui/ILocalize.h"
	#include "asw_hud_floating_number.h"
	#include "takedamageinfo.h"
	#include "clientmode_asw.h"
	#include "engine/IVDebugOverlay.h"
	#include "c_user_message_register.h"
	#include "prediction.h"
	#define CASW_Marine C_ASW_Marine
	#define CASW_Game_Resource C_ASW_Game_Resource
	#define CASW_Marine_Resource C_ASW_Marine_Resource
	#define CASW_Computer_Area C_ASW_Computer_Area
	#define CPointCamera C_PointCamera
	#define CASW_PointCamera C_ASW_PointCamera
	#define CASW_Remote_Turret C_ASW_Remote_Turret
	#define CASW_Button_Area C_ASW_Button_Area
	#define CASW_Computer_Area C_ASW_Computer_Area
#endif
#include "shake.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef CLIENT_DLL
ConVar asw_debug_marine_can_see("asw_debug_marine_can_see", "0", FCVAR_CHEAT, "Display lines for waking up aliens");
#else
extern int g_asw_iGUIWindowsOpen;
#endif

ConVar asw_marine_view_cone_dist("asw_marine_view_cone_dist", "700", FCVAR_REPLICATED, "Distance for marine view cone checks");
ConVar asw_marine_view_cone_dot("asw_marine_view_cone_dot", "0.5", FCVAR_REPLICATED, "Dot for marine view cone checks");
extern ConVar asw_rts_controls;


ConVar asw_shake_test_punch_dirx("asw_shake_test_punch_dirx","0", FCVAR_REPLICATED|FCVAR_HIDDEN );
ConVar asw_shake_test_punch_diry("asw_shake_test_punch_diry","0", FCVAR_REPLICATED|FCVAR_HIDDEN );
ConVar asw_shake_test_punch_dirz("asw_shake_test_punch_dirz","1", FCVAR_REPLICATED|FCVAR_HIDDEN );
ConVar asw_shake_test_punch_freq("asw_shake_test_punch_freq","1.5", FCVAR_REPLICATED|FCVAR_HIDDEN );
ConVar asw_shake_test_punch_amp("asw_shake_test_punch_amp","60", FCVAR_REPLICATED|FCVAR_HIDDEN );
ConVar asw_shake_test_punch_dura("asw_shake_test_punch_dura","0.75", FCVAR_REPLICATED|FCVAR_HIDDEN );

// rotates one angle towards another, with a fixed turning rate over the time
float ASW_ClampYaw( float yawSpeedPerSec, float current, float target, float time )
{
	if (current != target)
	{
		float speed = yawSpeedPerSec * time;
		float move = target - current;

		if (target > current)
		{
			if (move >= 180)
				move = move - 360;
		}
		else
		{
			if (move <= -180)
				move = move + 360;
		}

		if (move > 0)
		{// turning to the npc's left
			if (move > speed)
				move = speed;
		}
		else
		{// turning to the npc's right
			if (move < -speed)
				move = -speed;
		}
		
		return anglemod(current + move);
	}
	
	return target;
}

float ASW_Linear_Approach( float current, float target, float delta)
{
	if (current < target)
		current = MIN(current + delta, target);
	else if (current > target)
		current = MAX(current - delta, target);

	return current;
}

// time independent movement of one angle to a fraction of the desired
float ASW_ClampYaw_Fraction( float fraction, float current, float target, float time )
{
	if (current != target)
	{		
		float move = target - current;

		if (target > current)
		{
			if (move >= 180)
				move = move - 360;
		}
		else
		{
			if (move <= -180)
				move = move + 360;
		}
		move = move * pow(fraction, time);
		float r = anglemod(target - move);
		
		return r;
	}
	
	return target;
}

bool ASW_LineCircleIntersection(
	const Vector2D &center,
	const float radius,
	const Vector2D &vLinePt,
	const Vector2D &vLineDir,
	float *fIntersection1,
	float *fIntersection2)
{
	// Line = P + Vt
	// Sphere = r (assume we've translated to origin)
	// (P + Vt)^2 = r^2
	// VVt^2 + 2PVt + (PP - r^2)
	// Solve as quadratic:  (-b  +/-  sqrt(b^2 - 4ac)) / 2a
	// If (b^2 - 4ac) is < 0 there is no solution.
	// If (b^2 - 4ac) is = 0 there is one solution (a case this function doesn't support).
	// If (b^2 - 4ac) is > 0 there are two solutions.
	Vector2D P;
	float a, b, c, sqr, insideSqr;


	// Translate circle to origin.
	P[0] = vLinePt[0] - center[0];
	P[1] = vLinePt[1] - center[1];
	
	a = vLineDir.Dot(vLineDir);
	b = 2.0f * P.Dot(vLineDir);
	c = P.Dot(P) - (radius * radius);

	insideSqr = b*b - 4*a*c;
	if(insideSqr <= 0.000001f)
		return false;

	// Ok, two solutions.
	sqr = (float)FastSqrt(insideSqr);

	float denom = 1.0 / (2.0f * a);
	
	*fIntersection1 = (-b - sqr) * denom;
	*fIntersection2 = (-b + sqr) * denom;

	return true;
}

#ifdef GAME_DLL
// a local helper to normalize some code below -- gets inlined
static void ASW_WriteScreenShakeToMessage( CBasePlayer *pPlayer, ShakeCommand_t eCommand, float amplitude, float frequency, float duration, const Vector &direction )
{
	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();
	if ( direction.IsZeroFast() ) // nondirectional shake
	{
		UserMessageBegin( user, "Shake" );
		WRITE_BYTE( eCommand );				// shake command (SHAKE_START, STOP, FREQUENCY, AMPLITUDE)
		WRITE_FLOAT( amplitude );			// shake magnitude/amplitude
		WRITE_FLOAT( frequency );				// shake noise frequency
		WRITE_FLOAT( duration );				// shake lasts this long
		MessageEnd();
	}
	else // directional shake
	{
		UserMessageBegin( user, "ShakeDir" );
		WRITE_BYTE( eCommand );				// shake command (SHAKE_START, STOP, FREQUENCY, AMPLITUDE)
		WRITE_FLOAT( amplitude );			// shake magnitude/amplitude
		WRITE_FLOAT( frequency );				// shake noise frequency
		WRITE_FLOAT( duration );				// shake lasts this long
		WRITE_VEC3NORMAL( direction );
		MessageEnd();
	}
}
#endif

//-----------------------------------------------------------------------------
// Transmits the actual shake event
//-----------------------------------------------------------------------------
 void ASW_TransmitShakeEvent( CBasePlayer *pPlayer, float localAmplitude, float frequency, float duration, ShakeCommand_t eCommand, const Vector &direction )
{
	if (( localAmplitude > 0 ) || ( eCommand == SHAKE_STOP ))
	{
		if ( eCommand == SHAKE_STOP )
			localAmplitude = 0;

#ifdef GAME_DLL
		ASW_WriteScreenShakeToMessage( pPlayer, eCommand, localAmplitude, frequency, duration, direction );
#else
		ScreenShake_t shake;

		shake.command	= eCommand;
		shake.amplitude = localAmplitude;
		shake.frequency = frequency;
		shake.duration	= duration;
		shake.direction = direction;

		ASW_TransmitShakeEvent( pPlayer, shake );
#endif
	}
}

void ASW_TransmitShakeEvent( CBasePlayer *pPlayer, const ScreenShake_t &shake )
{
	if ( shake.command == SHAKE_STOP && shake.amplitude != 0 )
	{
		// create a corrected screenshake and recursively call myself
		AssertMsg1( false, "A ScreenShake_t had a SHAKE_STOP command but a nonzero amplitude %.1f; this is meaningless.\n", shake.amplitude);
		ScreenShake_t localShake = shake;
		localShake.amplitude = 0;
		ASW_TransmitShakeEvent( pPlayer, localShake );
	}
#ifdef GAME_DLL
	ASW_WriteScreenShakeToMessage( pPlayer, shake.command, shake.amplitude, shake.frequency, shake.duration, shake.direction );
#else
	if ( !( prediction && prediction->InPrediction() && !prediction->IsFirstTimePredicted() ) )
	{
		GetViewEffects()->Shake( shake );
	}
#endif
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Compute shake amplitude
//-----------------------------------------------------------------------------
inline float ASW_ComputeShakeAmplitude( const Vector &center, const Vector &shakePt, float amplitude, float radius ) 
{
	if ( radius <= 0 )
		return amplitude;

	float localAmplitude = -1;
	Vector delta = center - shakePt;
	float distance = delta.Length();

	if ( distance <= radius )
	{
		// Make the amplitude fall off over distance
		float flPerc = 1.0 - (distance / radius);
		localAmplitude = amplitude * flPerc;
	}

	return localAmplitude;
}

//-----------------------------------------------------------------------------
// Purpose: Shake the screen of all clients within radius.
//			radius == 0, shake all clients
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
// Input  : center - Center of screen shake, radius is measured from here.
//			amplitude - Amplitude of shake
//			frequency - 
//			duration - duration of shake in seconds.
//			radius - Radius of effect, 0 shakes all clients.
//			command - One of the following values:
//				SHAKE_START - starts the screen shake for all players within the radius
//				SHAKE_STOP - stops the screen shake for all players within the radius
//				SHAKE_AMPLITUDE - modifies the amplitude of the screen shake
//									for all players within the radius
//				SHAKE_FREQUENCY - modifies the frequency of the screen shake
//									for all players within the radius
//			bAirShake - completely ignored
//-----------------------------------------------------------------------------
const float ASW_MAX_SHAKE_AMPLITUDE = 16.0f;
void UTIL_ASW_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake )
{
	int			i;
	float		localAmplitude;

	if ( amplitude > ASW_MAX_SHAKE_AMPLITUDE )
	{
		amplitude = ASW_MAX_SHAKE_AMPLITUDE;
	}
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		//
		// Only start shakes for players that are on the ground unless doing an air shake.
		//
		if ( !pPlayer )
		{
			continue;
		}

		// find the player's marine
		CASW_Player *pASWPlayer = dynamic_cast<CASW_Player*>(pPlayer);
		if (!pASWPlayer || !pASWPlayer->GetMarine())
			continue;

		Vector vecMarinePos = pASWPlayer->GetMarine()->WorldSpaceCenter();
		if (pASWPlayer->GetMarine()->IsControllingTurret() && pASWPlayer->GetMarine()->GetRemoteTurret())
			vecMarinePos = pASWPlayer->GetMarine()->GetRemoteTurret()->GetAbsOrigin();

		localAmplitude = ASW_ComputeShakeAmplitude( center, vecMarinePos, amplitude, radius );

		// This happens if the player is outside the radius, in which case we should ignore 
		// all commands
		if (localAmplitude < 0)
			continue;

		ASW_TransmitShakeEvent( (CBasePlayer *)pPlayer, localAmplitude, frequency, duration, eCommand );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Perform a directional "punch" on the screen of all clients within radius.
//			radius == 0, shake all clients
// Input  : center - Center of screen shake, radius is measured from here.
//          direction - (world space) direction in which to punch camera. punching down makes it look like the world is moving up. must be normal.
//			amplitude - Amplitude of shake, in world units for the camera
//			frequency - controls number of bounces before shake settles; a frequency of 1 means three peaks (forward, back, little forward, settle)
//			duration - duration of shake in seconds.
//			radius - Radius of effect, 0 shakes all clients.
//-----------------------------------------------------------------------------
void UTIL_ASW_ScreenPunch( const Vector &center, const Vector &direction, float amplitude, float frequency, float duration, float radius )
{
	ScreenShake_t shake;
	shake.command = SHAKE_START;
	shake.direction = direction;
	shake.amplitude = amplitude;
	shake.frequency = frequency;
	shake.duration = duration;

	UTIL_ASW_ScreenPunch( center, radius, shake );
}

void UTIL_ASW_ScreenPunch( const Vector &center, float radius, const ScreenShake_t &shake )
{
	int			i;
	const float radiusSqr = radius * radius;

	AssertMsg( CloseEnough(shake.direction.LengthSqr(), 1), "Direction param to ASW_ScreenPunch is abnormal\n" );

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		//
		// Only start shakes for players that are on the ground unless doing an air shake.
		//
		if ( !pPlayer )
		{
			continue;
		}

		// find the player's marine
		CASW_Player *pASWPlayer = assert_cast<CASW_Player*>(pPlayer);
		if (!pASWPlayer || !pASWPlayer->GetMarine())
			continue;

		Vector vecMarinePos = pASWPlayer->GetMarine()->WorldSpaceCenter();
		if (pASWPlayer->GetMarine()->IsControllingTurret() && pASWPlayer->GetMarine()->GetRemoteTurret())
			vecMarinePos = pASWPlayer->GetMarine()->GetRemoteTurret()->GetAbsOrigin();

		if ( vecMarinePos.DistToSqr(center) > radiusSqr )
			continue;

		ASW_TransmitShakeEvent( (CBasePlayer *)pPlayer, shake );
	}
}


// returns the nearest marine to this point
CASW_Marine* UTIL_ASW_NearestMarine( const Vector &pos, float &marine_distance, ASW_Marine_Class marineClass, bool bAIOnly )
{
	// check through all marines, finding the closest that we're aware of
	CASW_Game_Resource* pGameResource = ASWGameResource();
	float distance = 0.0f;
	marine_distance = -1.0f;
	CASW_Marine *pNearest = NULL;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
		if (pMR!=NULL && pMR->GetMarineEntity()!=NULL && pMR->GetMarineEntity()->GetHealth() > 0)
		{
			if ( bAIOnly && pMR->IsInhabited() )
				continue;
			if ( marineClass != MARINE_CLASS_UNDEFINED && pMR->GetProfile() && pMR->GetProfile()->GetMarineClass() != marineClass )
				continue;

			distance = pMR->GetMarineEntity()->GetAbsOrigin().DistTo(pos);
			if (marine_distance == -1.0f || distance < marine_distance)
			{
				marine_distance = distance;
				pNearest = pMR->GetMarineEntity();
			}
		}
	}
	return pNearest;
}

CASW_Marine* UTIL_ASW_NearestMarine( const CASW_Marine *pMarine, float &marine_distance )
{
	// check through all marines, finding the closest that we're aware of
	CASW_Game_Resource* pGameResource = ASWGameResource();
	float distance = 0;
	marine_distance = -1.0f;
	CASW_Marine *pNearest = NULL;
	for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
	{
		CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
		if ( pMR != NULL && pMR->GetMarineEntity() != NULL && pMR->GetMarineEntity() != pMarine && pMR->GetMarineEntity()->GetHealth() > 0 )
		{
			distance = pMR->GetMarineEntity()->GetAbsOrigin().DistTo( pMarine->GetAbsOrigin() );
			if ( marine_distance == -1.0f || distance < marine_distance )
			{
				marine_distance = distance;
				pNearest = pMR->GetMarineEntity();
			}
		}
	}
	return pNearest;
}

int UTIL_ASW_NumCommandedMarines( const CASW_Player *pPlayer )
{
	int nNumMarines = 0;

	// check through all marines
	CASW_Game_Resource* pGameResource = ASWGameResource();
	for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
	{
		CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
		if ( pMR && pMR->GetMarineEntity() && pMR->GetMarineEntity()->GetHealth() > 0 )
		{
			if ( pMR->GetMarineEntity()->GetCommander() == pPlayer )
			{
				nNumMarines++;
			}
		}
	}
	return nNumMarines;
}

#else
	// make a specific clientside entity gib
	bool UTIL_ASW_ClientsideGib(C_BaseAnimating* pEnt)
	{
		if (!pEnt)
			return false;
		C_BaseAnimating* pAnimating = dynamic_cast<C_BaseAnimating*>(pEnt);
		if (!pAnimating)
			return false;
		if (!stricmp(STRING(pAnimating->GetModelName()), SWARM_DRONE_MODEL))
		{
			Vector vMins, vMaxs, vGibOrigin, vGibVelocity(0,0,1);
			if ( pEnt->m_pRagdoll )
			{
				pEnt->m_pRagdoll->GetRagdollBounds( vMins, vMaxs );
				vGibOrigin =pEnt->m_pRagdoll->GetRagdollOrigin() + ( ( vMins + vMaxs ) / 2.0f );
				pEnt->m_pRagdoll->GetElement(0)->GetVelocity( &vGibVelocity, NULL );
			}
			else
			{
				vGibOrigin = pEnt->WorldSpaceCenter();
			}

			FX_DroneGib( vGibOrigin, Vector(0,0,1), 0.5f, pAnimating->GetSkin(), pEnt->IsOnFire() );
			return true;
		}
		else if (!stricmp(STRING(pAnimating->GetModelName()), SWARM_HARVESTER_MODEL))
		{
			FX_HarvesterGib( pEnt->WorldSpaceCenter(), Vector(0,0,1), 0.5f, pAnimating->GetSkin(), pEnt->IsOnFire() );
			return true;
		}
		else if (!stricmp(STRING(pAnimating->GetModelName()), SWARM_SHIELDBUG_MODEL))
		{
			FX_HarvesterGib( pEnt->WorldSpaceCenter(), Vector(0,0,1), 0.5f, 1, pEnt->IsOnFire() );
			return true;
		}
		// todo: code to gib other types of things clientside?
		return false;
	}
#endif

//void UTIL_ASW_ValidateSoundName( string_t &name, const char *defaultStr )
void UTIL_ASW_ValidateSoundName( char *szString, int stringlength, const char *defaultStr )
{
	Assert(szString);
	if (szString[0] == '\0')
	{
		Q_snprintf(szString, stringlength, "%s", defaultStr);
	}
}

#ifdef GAME_DLL
void UTIL_ASW_PoisonBlur(CBaseEntity *pEntity, float duration)
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	CSingleUserRecipientFilter user( (CBasePlayer *)pEntity );
	user.MakeReliable();

	UserMessageBegin( user, "ASWBlur" );		// use the magic #1 for "one client"
		WRITE_SHORT( (int) (duration * 10.0f) );		// blur lasts this long / 10
	MessageEnd();
}

// tests if a particular entity is blocking any marines (used by phys props to see if they should leave pushaway mode)
bool UTIL_ASW_BlockingMarine( CBaseEntity *pEntity )
{
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return false;

	int iCurrentGroup = pEntity->GetCollisionGroup();
	pEntity->SetCollisionGroup(COLLISION_GROUP_NONE);
	
	bool bBlockedMarine = false;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
		if (pMR!=NULL && pMR->GetMarineEntity()!=NULL && pMR->GetMarineEntity()->GetHealth() > 0)
		{
			CASW_Marine *pMarine = pMR->GetMarineEntity();
			// check if this marine's bounding box trace collides with the specified entity
			Ray_t ray;
			trace_t tr;
			ray.Init( pMarine->GetAbsOrigin(), pMarine->GetAbsOrigin() - Vector(0,0,1),
					pMarine->CollisionProp()->OBBMins(), pMarine->CollisionProp()->OBBMaxs() );
			if (pEntity->TestCollision( ray, MASK_PLAYERSOLID, tr ))
			{
				bBlockedMarine = true;
				break;
			}
		}
	}

	pEntity->SetCollisionGroup(iCurrentGroup);

	return bBlockedMarine;
}

CASW_Marine* UTIL_ASW_Marine_Can_Chatter_Spot(CBaseEntity *pEntity, float fDist)
{	
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return NULL;

	// find how many marines can see us
	int iFound = 0;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMarineResource = pGameResource->GetMarineResource(i);
		if (!pMarineResource)
			continue;

		CASW_Marine *pMarine = pMarineResource->GetMarineEntity();
		if (!pMarine)
			continue;

		if (pMarine->GetAbsOrigin().DistTo(pEntity->GetAbsOrigin()) < fDist)
		{
			Vector vecFacing;
			AngleVectors(pMarine->GetAbsAngles(), &vecFacing);
			Vector vecDir = pEntity->GetAbsOrigin() - pMarine->GetAbsOrigin();
			vecDir.NormalizeInPlace();
			if (vecFacing.Dot(vecDir) > 0.5f)
				iFound++;				
		}
	}
	if (iFound <= 0)
		return NULL;

	// randomly pick one
	int iChosen = random->RandomInt(0, iFound-1);
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMarineResource = pGameResource->GetMarineResource(i);
		if (!pMarineResource)
			continue;

		CASW_Marine *pMarine = pMarineResource->GetMarineEntity();
		if (!pMarine)
			continue;

		if (pMarine->GetAbsOrigin().DistTo(pEntity->GetAbsOrigin()) < 600.0f)
		{
			Vector vecFacing;
			AngleVectors(pMarine->GetAbsAngles(), &vecFacing);
			Vector vecDir = pEntity->GetAbsOrigin() - pMarine->GetAbsOrigin();
			vecDir.NormalizeInPlace();
			if (vecFacing.Dot(vecDir) > 0.5f)
			{
				if (iChosen <= 0)
					return pMarine;
				iChosen--;				
			}
		}
	}
	return NULL;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Trace filter that only hits aliens (all NPCS but the marines, eggs, goo)
//-----------------------------------------------------------------------------
bool CTraceFilterAliensEggsGoo::ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
{
	if ( CTraceFilterSimple::ShouldHitEntity(pServerEntity, contentsMask) )
	{		
#ifndef CLIENT_DLL
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );
		if ( pEntity->Classify() == CLASS_ASW_MARINE )		// we dont hit marines
			return false; 

		if ( IsAlienClass( pEntity->Classify() ) )
			return true;
#endif // !CLIENT_DLL
	}
	return false;
}

// NOTE: This function assumes 75 fov and 4:3 ratio (todo: support widescreen all the time?)
bool CanFrustumSee(const Vector &vecCameraCenter, const QAngle &angCameraFacing, 
				   const Vector &pos, const int padding, const int forward_limit, float fov=75.0f)
{	
	Vector vForward, vRight, vUp;
	AngleVectors(angCameraFacing, &vForward, &vRight, &vUp);

	Vector vecTestPos = pos;
	// bring in the x coord by the padding
	if (vecTestPos.x < vecCameraCenter.x)
		vecTestPos.x = MIN(vecCameraCenter.x, vecTestPos.x + padding);
	else if (vecTestPos.x > vecCameraCenter.x)
		vecTestPos.x = MAX(vecCameraCenter.x, vecTestPos.x - padding);
	// bring in the y coord by the padding
	if (vecTestPos.y < vecCameraCenter.y)
		vecTestPos.y = MIN(vecCameraCenter.y, vecTestPos.y + padding);
	else if (vecTestPos.y > vecCameraCenter.y)
		vecTestPos.y = MAX(vecCameraCenter.y, vecTestPos.y - padding);

	float ratio = 4.0f / 3.0f;	// assume 4:3 res	
	float fov_tangent = tan(DEG2RAD(fov) * 0.5f);

	Vector vDiff = vecTestPos - vecCameraCenter;	// vector from camera to testing position
	
	float forward_diff = vDiff.Dot(vForward);
	if (forward_diff < 0)	// behind the camera
		return false;
	if (forward_limit > 0 && forward_diff > forward_limit)
		return false;	// too far away

	//int padding_at_this_distance = padding * (forward_diff / 405.0f);	// adjust padding by ratio of distance to default camera height
	float up_diff = vDiff.Dot(vUp);	
	float max_up_diff = forward_diff * fov_tangent;

	if (up_diff > max_up_diff || up_diff < -max_up_diff)
		return false;
	
	float right_diff = vDiff.Dot(vRight);	
	float max_right_diff = max_up_diff * ratio;

	if (right_diff > max_right_diff || right_diff < -max_right_diff)
		return false;
			
	return true;
}

CASW_Marine* UTIL_ASW_MarineCanSee(CASW_Marine_Resource* pMarineResource, const Vector &pos, const int padding, bool &bCorpseCanSee, const int forward_limit)
{
	if (!pMarineResource)
		return NULL;

	Vector vecMarinePos;
	CASW_Marine* pMarine = NULL;
#ifndef CLIENT_DLL
	if (pMarineResource->GetHealthPercent() <=0 || !pMarineResource->IsAlive())	// if we're dead, take the corpse position
	{
		vecMarinePos = pMarineResource->m_vecDeathPosition;
	}
	else
#endif
	{
		pMarine = pMarineResource->GetMarineEntity();
		if (!pMarine)
			return NULL;

		vecMarinePos = pMarine->GetAbsOrigin();
	}
	
	// note: assumes 60 degree pitch camera and 405 dist (actual convars for these are on the client...)
	QAngle angCameraFacing(60, 90, 0);
	Vector vForward, vRight, vUp;
	AngleVectors(angCameraFacing, &vForward, &vRight, &vUp);
	Vector vecCameraCenter = vecMarinePos - vForward * 405;

	// see if they're beyond the fog plane	
#ifdef CLIENT_DLL
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer)
	{		
		if (pPlayer->GetPlayerFog().m_hCtrl->m_fog.enable)
		{
			float dist = (vecCameraCenter - pos).Length();
			if (dist > pPlayer->GetPlayerFog().m_hCtrl->m_fog.end)
				return NULL;
		}
	}
#else
	// ASWTODO - no worldfogparams anymore?
	/*
	fogparams_t fog;
	GetWorldFogParams(fog);	
	if (fog.enable.Get())
	{
		float dist = (vecCameraCenter - pos).Length();
		if (dist > fog.end.Get())
			return NULL;
	}
	*/
#endif	

	// check for plain near the marine
	bool bNearby = CanFrustumSee(vecCameraCenter, angCameraFacing, pos, padding, forward_limit);
	// check if he's looking through a remote turret and can possibly see us from that
	if (!bNearby && pMarine && pMarine->IsControllingTurret() && pMarine->GetRemoteTurret())
	{
		// a turret can look in any direction, so let's just do a radius check (would match up with the fog anyways)
		bNearby = (pMarine->GetRemoteTurret()->GetAbsOrigin().DistTo(pos) <= 1024);	// assume fog distance of 1024 when in first person
	}
	// check if he's looking through a security cam
	if (!bNearby && pMarine && pMarine->m_hUsingEntity.Get())
	{
		CASW_Computer_Area *pComputer = dynamic_cast<CASW_Computer_Area*>(pMarine->m_hUsingEntity.Get());
		if (pComputer)
		{
			if (pComputer->m_iActiveCam == 1 && pComputer->m_hSecurityCam1.Get())
			{
				CPointCamera* pCam = dynamic_cast<CPointCamera*>(pComputer->m_hSecurityCam1.Get());
				if (pCam)
				{
					Vector vecCamFacing;
					AngleVectors(pCam->GetAbsAngles(), &vecCamFacing);
					bNearby = (vecCamFacing.Dot(pos - pCam->GetAbsOrigin()) > 0) &&		// check facing
						(pCam->GetAbsOrigin().DistTo(pos) <= 1024);	// assume fog distance of 1024 when in first person
				}
			}
		}
	}
#ifndef CLIENT_DLL
	if (asw_debug_marine_can_see.GetBool())
	{
		if (bNearby)
		{
			NDebugOverlay::Line(pos, vecMarinePos + Vector(0,0,10), 255, 255, 0, true, 0.1f);
		}
		else
		{
			NDebugOverlay::Line(pos, vecMarinePos + Vector(0,0,10), 255, 0, 0, true, 0.1f);
		}
	}
#endif
	if (bNearby)
	{
		if (!pMarine)
			bCorpseCanSee = true;
		return pMarine;
	}

	return NULL;
}

CASW_Marine* UTIL_ASW_AnyMarineCanSee(const Vector &pos, const int padding, bool &bCorpseCanSee, const int forward_limit)
{
	// find the closest marine
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return NULL;

	bCorpseCanSee = false;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMarineResource = pGameResource->GetMarineResource(i);
		bool bCorpse = false;
		CASW_Marine *pMarine = (UTIL_ASW_MarineCanSee(pMarineResource, pos, padding, bCorpse, forward_limit));
		bCorpseCanSee |= bCorpse;
		if (pMarine)
			return pMarine;
	}
	return NULL;
}

bool UTIL_ASW_MarineViewCone(const Vector &pos)
{
	// find the closest marine
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return false;

	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMarineResource = pGameResource->GetMarineResource(i);
		if (!pMarineResource)
			continue;

		Vector vecMarinePos;
		CASW_Marine* pMarine = pMarineResource->GetMarineEntity();
		if (!pMarine)
			continue;

		// check it's not too far away
		Vector vecDiff = pos - pMarine->GetAbsOrigin();
		if (vecDiff.LengthSqr() > asw_marine_view_cone_dist.GetFloat())		
			continue;

		// check dot
		Vector vecFacing;
		AngleVectors(pMarine->EyeAngles(), &vecFacing);
		float dot = vecDiff.Dot(vecFacing);
		if (dot < asw_marine_view_cone_dot.GetFloat())
			continue;

		return true;
	}
	return false;
}

#ifdef CLIENT_DLL
extern ConVar asw_cam_mode;
extern ConVar joy_pan_camera;
#else
extern ConVar asw_debug_medals;
extern ConVar asw_wire_full_random;
#endif

float UTIL_ASW_CalcFastDoorHackTime(int iNumRows, int iNumColumns, int iNumWires, int iHackLevel, float fSpeedScale)
{		
	float ideal_time = 1.0f;
	// assume 0.5 seconds per row
	float seconds_per_column = 0.5f;
	if (iNumRows == 2)
		seconds_per_column = 1.0f;
	else if (iNumRows == 3)
		seconds_per_column = 1.5f;
	
	float time_to_assemble_wire = seconds_per_column * iNumColumns;		
#ifndef CLIENT_DLL
	if (!asw_wire_full_random.GetBool())
	{
		time_to_assemble_wire = 3.0f;	// assumes 5 mistakes per wire
	}
	if (asw_debug_medals.GetBool())
		Msg("time_to_assemble_wire = %f\n", time_to_assemble_wire);
#endif
	// ok so after this amount of time, the wire would be charging	
	if (iNumWires <= 0)
		iNumWires = 1;
	float speed_per_wire = 1.0f / iNumWires;
	speed_per_wire *= fSpeedScale;
	
	float charge_before_assembling_wire_2 = speed_per_wire * time_to_assemble_wire;
	if (charge_before_assembling_wire_2 >= iHackLevel || iNumWires < 2)
	{
		// if we're here, it means we would have finished the hack before wire two was assembled
		// so the ideal time is just how long it takes to charge up with 1 wire, plus the time it took us to assemble
		ideal_time = (float(iHackLevel) / speed_per_wire) + time_to_assemble_wire;
	}
	else
	{
		// if we're in here, then wire 1 and 2 will be charging
		float charge_before_assembling_wire_3 = charge_before_assembling_wire_2
												+ speed_per_wire * time_to_assemble_wire * 2;	// wire 2's contribution
		if (charge_before_assembling_wire_3 >= iHackLevel || iNumWires < 3)
		{
			// if we're here, it means we would have finished the hack before wire three was assembled
			float first_wire_time = time_to_assemble_wire + time_to_assemble_wire;
			float duo_charge = float(iHackLevel) - charge_before_assembling_wire_2;	// how much charge up to do with both wires
			ideal_time = (duo_charge / (speed_per_wire * 2)) + first_wire_time;
		}
		else
		{
			// if we're in here, then wires 1, 2 and 3 will be charging
			float charge_before_assembling_wire_4 = charge_before_assembling_wire_3
												+ speed_per_wire * time_to_assemble_wire * 3;	// wire 3's contribution
			if (charge_before_assembling_wire_3 >= iHackLevel || iNumWires < 4)
			{
				// if we're here, it means we would have finished the hack before wire 4 was assembled
				float first_wire_time = time_to_assemble_wire + time_to_assemble_wire;
				float two_wire_time = time_to_assemble_wire + first_wire_time;
				float triple_charge = float(iHackLevel) - charge_before_assembling_wire_3;	// how much charge do we with 3 wires
				ideal_time = (triple_charge / (speed_per_wire * 3)) + two_wire_time;
			}
			else
			{
				// if we're in here, then wires 1,2,3 and 4 will be charging
				float first_wire_time = time_to_assemble_wire + time_to_assemble_wire;
				float two_wire_time = time_to_assemble_wire + first_wire_time;
				float three_wire_time = time_to_assemble_wire + two_wire_time;
				float quad_charge = float(iHackLevel) - charge_before_assembling_wire_4;	// how much charge do we with 3 wires
				ideal_time = (quad_charge / (speed_per_wire * 4)) + three_wire_time;
			}
		}
	}

	int iSkill = ASWGameRules() ? ASWGameRules()->GetSkillLevel() : 2;
	if (iSkill == 1)
		ideal_time *= 1.05f;	// 5% slower on easy mode
	else if (iSkill == 3)
		ideal_time *= 0.95f;	// 5% faster on hard mode
	else if (iSkill == 4 || iSkill == 5)
		ideal_time *= 0.90f;	// 10% faster on insane mode
	
	return ideal_time;
}

int UTIL_ASW_GetNumPlayers()
{
	int count = 0;
	for (int i=0;i<MAX_PLAYERS;i++)
	{
		// found a connected player who isn't ready?
#ifdef CLIENT_DLL
		if (g_PR->IsConnected(i+1))
			count++;
#else
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i + 1);
		// if they're not connected, skip them
		if (pPlayer && pPlayer->IsConnected())
			count++;
#endif
	}

	return count;
}

bool UTIL_ASW_MissionHasBriefing(const char* mapname)
{
	bool bSpecialMap = (!Q_strnicmp(mapname, "intro_", 6) ||
			!Q_strnicmp(mapname, "outro_", 6) ||
			!Q_strnicmp(mapname, "tutorial", 8) ||
			!Q_strnicmp(mapname, "swarmselectionscreen", 20));

	return !bSpecialMap;
}

bool ASW_IsSecurityCam(CPointCamera *pCameraEnt)
{
	CASW_PointCamera *pASW_Cam = dynamic_cast<CASW_PointCamera*>(pCameraEnt);
	return pASW_Cam && pASW_Cam->m_bSecurityCam;
}

// copies a string
char* ASW_AllocString( const char *szString )
{
	if ( !szString )
		return NULL;

	int len = Q_strlen( szString ) + 1;
	if ( len <= 1 )
		return NULL;

	char *text = new char[ len ];
	Q_strncpy( text, szString, len );
	return text;
}


#ifdef CLIENT_DLL
CNewParticleEffect *UTIL_ASW_CreateFireEffect( C_BaseEntity *pEntity )
{
	CNewParticleEffect	*pBurningEffect = pEntity->ParticleProp()->Create( "ent_on_fire", PATTACH_ABSORIGIN_FOLLOW );
	if (pBurningEffect)
	{
		Vector vecOffest1 = (pEntity->WorldSpaceCenter() - pEntity->GetAbsOrigin()) + Vector( 0, 0, 16 );
		pEntity->ParticleProp()->AddControlPoint( pBurningEffect, 1, pEntity, PATTACH_ABSORIGIN_FOLLOW, NULL, vecOffest1 );

		// all bounding boxes are the same, skip this for now
		Vector vecSurroundMins, vecSurroundMaxs;
		vecSurroundMins = pEntity->CollisionProp()->OBBMins();
		vecSurroundMaxs = pEntity->CollisionProp()->OBBMaxs();

		// this sets the maximum bounds for scaling up or down the fire
		float flMaxBounds = 34.0;
		flMaxBounds = MAX( flMaxBounds, vecSurroundMaxs.x - vecSurroundMins.x );
		flMaxBounds = MAX( flMaxBounds, vecSurroundMaxs.y - vecSurroundMins.y );
		flMaxBounds = MAX( flMaxBounds, vecSurroundMaxs.z - vecSurroundMins.z );

		float flScalar = 1.0f;
		flMaxBounds /= 115.0f;
		flMaxBounds = clamp( flMaxBounds, 0.75f, 1.75f );

		// position 0 of CP2 controls the scale of the flames, we want to scale them a bit based on how big the creature is
		// position 1 is the number generated
		if ( flMaxBounds > 220 )
			flScalar = 2.0f;

		pBurningEffect->SetControlPoint( 2, Vector( flMaxBounds, flMaxBounds * flScalar, 0 ) );
	}
	return pBurningEffect;
}

// attempts to localize a string
//  if it fails, it just fills the destination with the token name
void TryLocalize(const char *token, wchar_t *unicode, int unicodeBufferSizeInBytes)
{
	if ( token[0] == '#' )
	{
		wchar_t *pLocalized = g_pVGuiLocalize->Find( token );
		if ( pLocalized )
		{
			_snwprintf( unicode, unicodeBufferSizeInBytes, L"%s", pLocalized );
			return;
		}
	}
	g_pVGuiLocalize->ConvertANSIToUnicode( token, unicode, unicodeBufferSizeInBytes);
}

ConVar asw_floating_number_type( "asw_floating_number_type", "0", FCVAR_NONE, "1 = vgui, 2 = particles" );

void UTIL_ASW_ClientFloatingDamageNumber( const CTakeDamageInfo &info )
{
	// TODO: Move this to some rendering step?
	if ( asw_floating_number_type.GetInt() == 1 )
	{
		Vector screenPos;

		Vector vecPos = info.GetDamagePosition(); // WorldSpaceCenter()
		//debugoverlay->ScreenPosition( vecPos, screenPos );

		floating_number_params_t params;
		params.x = 0;//screenPos.x;
		params.y = 0;//screenPos.y;
		params.bShowPlus = false;
		//params.hFont = m_fontLargeFloatingText;
		params.flMoveDuration = 0.9f;
		params.flFadeStart = 0.6f;
		params.flFadeDuration = 0.3f;
		params.rgbColor = Color( 200, 200, 200, 255 );
		if ( info.GetDamageCustom() & DAMAGE_FLAG_WEAKSPOT )
		{
			params.rgbColor = Color( 255, 170, 150, 255 );
			params.flFadeStart = 0.65f;
			params.flFadeDuration = 0.4f;
			params.flMoveDuration = 0.95f;
		}
		params.alignment = vgui::Label::a_center;
		params.bWorldSpace = true;
		params.vecPos = vecPos;

		new CFloatingNumber( (int) info.GetDamage(), params, GetClientMode()->GetViewport() );
	}
	else if ( asw_floating_number_type.GetInt() == 2 )
	{
		C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>( info.GetAttacker() );
		if ( !pMarine )
			return;

		C_ASW_Player *pAttackingPlayer = pMarine->GetCommander();
		if ( !pAttackingPlayer )
			return;

		if ( pAttackingPlayer != C_BasePlayer::GetLocalPlayer() )
			return;

		UTIL_ASW_ParticleDamageNumber( info.GetAttacker(), info.GetDamagePosition(), int(info.GetDamage()), info.GetDamageCustom(), 1.0f, false );
	}
}

void UTIL_ASW_ParticleDamageNumber( C_BaseEntity *pEnt, Vector vecPos, int iDamage, int iDmgCustom, float flScale, bool bRandomVelocity )
{
	if ( asw_floating_number_type.GetInt() != 2 )
		return;

	if ( !pEnt )
		return;

	QAngle vecAngles;
	vecAngles[PITCH] = 0.0f;
	vecAngles[YAW] = ASWInput()->ASW_GetCameraYaw();
	vecAngles[ROLL] = ASWInput()->ASW_GetCameraPitch();

	//Msg( "DMG # angles ( %f, %f, %f )\n", vecAngles[PITCH], vecAngles[YAW], vecAngles[ROLL] );
	Vector vecForward, vecRight, vecUp;
	AngleVectors( vecAngles, &vecForward, &vecRight, &vecUp );

	Color cNumber = Color( 255, 240, 240 );

	int iCrit = 0;
	float flNewScale = MAX( flScale, 1.0f );
	float flLifetime = 1.0f;
	int r, g, b;
	if ( iDmgCustom & DAMAGE_FLAG_CRITICAL )
	{
		flNewScale *= 1.8f;
		flLifetime = 3.0f;
		iCrit = 1;
		cNumber = Color( 255, 0, 0 );
	}
	else if ( iDmgCustom & DAMAGE_FLAG_WEAKSPOT )
	{
		flNewScale *= 1.3f;
		flLifetime = 1.25f;
		cNumber = Color( 255, 128, 128 );
	}
	else if ( iDmgCustom & DAMAGE_FLAG_T75 )
	{
		flNewScale = 1.3f;
		cNumber = Color( 255, 0, 0 );
		// TODO: Stop these numbers from moving randomly
	}

	r = cNumber.r();
	g = cNumber.g();
	b = cNumber.b();

	CUtlReference<CNewParticleEffect> pEffect;
	if ( bRandomVelocity )
	{
		pEffect = pEnt->ParticleProp()->Create( "damage_numbers", PATTACH_CUSTOMORIGIN );
	}
	else
	{
		pEffect = pEnt->ParticleProp()->Create( "floating_numbers", PATTACH_CUSTOMORIGIN );
	}
	pEffect->SetControlPoint( 0, vecPos );
	pEffect->SetControlPoint( 1, Vector( 0, iDamage, iCrit ) );
	pEffect->SetControlPoint( 2, Vector( r, g, b ) );
	pEffect->SetControlPoint( 3, Vector( flNewScale, flLifetime, 0 ) );
	pEffect->SetControlPointOrientation( 5, vecForward, vecRight, vecUp );
}

void __MsgFunc_ASWDamageNumber( bf_read &msg )
{
	int iAmount = msg.ReadShort();
	int iFlags = msg.ReadShort();
	int iEntIndex = msg.ReadShort();		
	C_BaseEntity *pEnt = iEntIndex > 0 ? ClientEntityList().GetEnt( iEntIndex ) : NULL;
	if ( !pEnt )
		return;

	if ( asw_floating_number_type.GetInt() == 1 )
	{
		Vector vecPos;
		vecPos.x = msg.ReadFloat();
		vecPos.y = msg.ReadFloat();
		vecPos.z = msg.ReadFloat();

		if ( pEnt )
		{
			vecPos = pEnt->WorldSpaceCenter();
		}

		Vector screenPos;
		debugoverlay->ScreenPosition( vecPos, screenPos );

		floating_number_params_t params;
		params.x = 0;//screenPos.x;
		params.y = 0;// screenPos.y;
		params.bShowPlus = false;
		//params.hFont = m_fontLargeFloatingText;
		params.flMoveDuration = 0.85f;
		params.flFadeStart = 0.6f;
		params.flFadeDuration = 0.3f;
		params.rgbColor = Color( 200, 200, 200, 255 );
		if ( iFlags & DAMAGE_FLAG_WEAKSPOT )
		{
			params.rgbColor = Color( 255, 170, 150, 255 );
			params.flFadeStart = 0.65f;
			params.flFadeDuration = 0.4f;
			params.flMoveDuration = 0.95f;
		}

		params.alignment = vgui::Label::a_center;
		params.bWorldSpace = true;
		params.vecPos = vecPos;

		new CFloatingNumber( iAmount, params, GetClientMode()->GetViewport() );
	}
	else if ( asw_floating_number_type.GetInt() == 2 )
	{
		UTIL_ASW_ParticleDamageNumber( pEnt, pEnt->WorldSpaceCenter(), iAmount, iFlags, 1.25f, false );
	}
}
USER_MESSAGE_REGISTER( ASWDamageNumber );
#endif


/// @desc This function can be used as a convenience for when you want to
/// rapidly experiment with different screenshakes for a gameplay feature.
/// You have a single "scratchpad" screen shake which you can fill out with 
/// the concommand asw_shake_setscratch . 
/// Then you can read it in code with the ASW_DefaultScreenShake. 
/// So, the way you use it is,
/// if you have a function Kaboom() that needs to do a screenpunch,
/// but you don't know what numbers you want for that punch yet, 
/// you write the function to use the default screen shake:
///
/// void Kaboom() {  
///   ASW_TransmitShakeEvent( player, ASW_DefaultScreenShake() );
/// }
/// 
/// and then, while the game is running, you can fiddle the numbers around 
/// with asw_shake_setscratch  and try the Kaboom() function over and over
/// again to see the results without having to recompile.
/// Once you have numbers you are happy with, you can go back and hardcode
/// them into Kaboom(), freeing up the "Default" shake to be used somewhere
/// else.
ScreenShake_t ASW_DefaultScreenShake( void )
{
	return ScreenShake_t( SHAKE_START,
		asw_shake_test_punch_amp.GetFloat(),
		asw_shake_test_punch_freq.GetFloat(),
		asw_shake_test_punch_dura.GetFloat(),
		Vector( asw_shake_test_punch_dirx.GetFloat(), asw_shake_test_punch_diry.GetFloat(), asw_shake_test_punch_dirz.GetFloat() ) 
		);
}

static void ASW_PrintDefaultScreenShake( void )
{
	// x y z f a d
	Msg( "< %.3f,%.3f,%.3f > %.3f %.3f %.3f\n",
		asw_shake_test_punch_dirx.GetFloat(), asw_shake_test_punch_diry.GetFloat(), asw_shake_test_punch_dirz.GetFloat(),
		asw_shake_test_punch_freq.GetFloat(),
		asw_shake_test_punch_amp.GetFloat(),
		asw_shake_test_punch_dura.GetFloat()
		);
}

#ifndef CLIENT_DLL
/// convenient console command for setting the default screen shake parameters

//-----------------------------------------------------------------------------
// Purpose: Test a punch-type screen shake
//-----------------------------------------------------------------------------
static void CC_ASW_Shake_SetScratch( const CCommand &args )
{
	if ( args.ArgC() < 7 )
	{
		Msg("Usage: %s x y z f a d\n"
			"where x,y,z are direction of screen punch\n"
			"      f     is  frequency (1 means three bounces before settling)\n"
			"      a     is  amplitude\n"
			"      d     is  duration\n"
			"you can specify a direction 0 0 0 to mean a classic 'vibrating' shake rather than a directional punch.\n"
			"The current default screen shake is:\n\t",
			args[0]
			);
		ASW_PrintDefaultScreenShake();
	}

	const float x = atof( args[1] );
	const float y = atof( args[2] );
	const float z = atof( args[3] );
	const float f = atof( args[4] );
	const float a = atof( args[5] );
	const float d = atof( args[6] );


	asw_shake_test_punch_dirx.SetValue( x ); 
	asw_shake_test_punch_diry.SetValue( y ); 
	asw_shake_test_punch_dirz.SetValue( z );
	asw_shake_test_punch_freq.SetValue( f );
	asw_shake_test_punch_amp.SetValue( a );
	asw_shake_test_punch_dura.SetValue( d );
}
static ConCommand asw_shake_setscratch("asw_shake_setscratch", CC_ASW_Shake_SetScratch, "Set values for the \"default\" screenshake used for rapid iteration.\n", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

#endif //#ifndef CLIENT_DLL


/// get a parabola that goes from source to destination in specified time
Vector UTIL_LaunchVector( const Vector &src, const Vector &dest, float gravity, float flightTime )
{
	Assert( gravity > 0 );
	Assert( !AlmostEqual(src,dest) );

	if ( flightTime == 0.0f )
	{
		flightTime = MAX( 0.8f, sqrt( ( (dest - src).Length2D() * 1.5f ) / gravity ) );
	}

	// delta high from start to end
	float H = dest.z - src.z ; 
	// azimuth vector
	Vector azimuth = dest-src;
	azimuth.z = 0;
	// get horizontal distance start to end
	float D = azimuth.Length2D();
	// normalize azimuth
	azimuth /= D;

	float Vy = ( H / flightTime + 0.5 * gravity * flightTime );
	float Vx = ( D / flightTime );
	Vector ret = azimuth * Vx;
	ret.z = Vy;
	return ret;
}

extern ConVar sv_maxvelocity;

void UTIL_Bound_Velocity( Vector &vec )
{
	for ( int i=0 ; i<3 ; i++ )
	{
		if ( IS_NAN(vec[i]) )
		{
			vec[i] = 0;
		}

		if ( vec[i] > sv_maxvelocity.GetFloat() ) 
		{
			vec[i] = sv_maxvelocity.GetFloat();
		}
		else if ( vec[i] < -sv_maxvelocity.GetFloat() )
		{
			vec[i] = -sv_maxvelocity.GetFloat();
		}
	}
}

extern ConVar sv_gravity;

Vector UTIL_Check_Throw( const Vector &vecSrc, const Vector &vecThrowVelocity, float flGravity, const Vector &vecHullMins, const Vector &vecHullMaxs,
							int iCollisionMask, int iCollisionGroup, CBaseEntity *pIgnoreEnt, bool bDrawArc )
{
	Vector vecVelocity = vecThrowVelocity;
	const int iMaxSteps = 200;
	Vector vecPos = vecSrc;
	float flInterval = 0.016667f;
	float flActualGravity = sv_gravity.GetFloat() * flGravity;
	for ( int i = 0; i < iMaxSteps; i++ )
	{
		// add gravity
		Vector vecAbsVelocity = vecVelocity;

		Vector vecMove;
		vecMove.x = (vecVelocity.x ) * flInterval;
		vecMove.y = (vecVelocity.y ) * flInterval;

		// linear acceleration due to gravity
		float newZVelocity = vecVelocity.z - flActualGravity * flInterval;

		vecMove.z = ((vecVelocity.z + newZVelocity) / 2.0 ) * flInterval;

		vecVelocity.z = newZVelocity;

		UTIL_Bound_Velocity( vecVelocity );

		// trace to new pos
		trace_t tr;
		Vector vecNewPos = vecPos + vecVelocity * flInterval;		
		UTIL_TraceHull( vecPos, vecNewPos, vecHullMins, vecHullMaxs, iCollisionMask, pIgnoreEnt, iCollisionGroup, &tr );

		if ( bDrawArc )
		{
			debugoverlay->AddLineOverlay( vecPos, vecNewPos, 65, 65, 255, true, 3.0f );
		}

		if ( tr.fraction < 1.0f || tr.startsolid )
			break;

		vecPos = tr.endpos;
	}

	return vecPos;
}