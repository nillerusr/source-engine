#include "cbase.h"
#include "precache_register.h"
#include "FX_Sparks.h"
#include "iefx.h"
#include "c_te_effect_dispatch.h"
#include "particles_ez.h"
#include "decals.h"
#include "engine/IEngineSound.h"
#include "fx_quad.h"
#include "engine/IVDebugOverlay.h"
#include "shareddefs.h"
#include "fx_blood.h"
#include "effect_color_tables.h"
#include "fx.h"
#include "c_asw_fx.h"
#include "c_asw_generic_emitter_entity.h"
#include "c_asw_mesh_emitter_entity.h"
#include "asw_shareddefs.h"
#include "beamdraw.h"
#include "iviewrender_beams.h"
#include "IEffects.h"
#include "view.h"
#include "soundemittersystem/isoundemittersystembase.h"
#include "studio.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "tier0/vprof.h"
#include "c_user_message_register.h"
#include "c_asw_marine.h"
#include "c_asw_weapon.h"
#include "c_asw_alien.h"
#include "datacache/imdlcache.h"
#include "c_asw_level_exploder.h"
#include "fx_explosion.h"
#include "particles_localspace.h"
#include "toolframework_client.h"
#include "tier1/keyvalues.h"
#include "asw_fx_shared.h"
#include "c_asw_alien.h"
#include "shake.h"
#include "ivieweffects.h"
#include "asw_marine_shared.h"
#include "c_asw_egg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_gib_bleed_time("asw_gib_bleed_time", "1.5", 0, "How long drone gibs bleed for");
ConVar asw_pierce_spark_scale("asw_pierce_spark_scale", "0.9", 0, "Scale applied to piercing spark effect");
ConVar asw_tracer_style("asw_tracer_style", "1", FCVAR_ARCHIVE, "Specify tracer style. 0=none 1=normal 2=grey line");
ConVar asw_use_particle_tracers( "asw_use_particle_tracers", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use particle tracers instead of the old school HL2 ones" );

extern ConVar asw_muzzle_flash_new_type;

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheASW )
PRECACHE( MATERIAL, "swarm/effects/aswtracer" )
PRECACHE( MATERIAL, "swarm/effects/aswtracernormal" )
PRECACHE( MATERIAL, "swarm/effects/aswtracerred" )
PRECACHE( MATERIAL, "swarm/effects/aswtracernormalred" )
PRECACHE( MATERIAL, "swarm/effects/muzzleflashred1" )
PRECACHE( MATERIAL, "swarm/effects/muzzleflashred2" )
PRECACHE( MATERIAL, "swarm/effects/muzzleflashred3" )
PRECACHE( MATERIAL, "swarm/effects/muzzleflashred4" )

// precache textures used by vgui panels (is there a better place for this?)
PRECACHE( MATERIAL, "vgui/swarm/computer/computerscan" )
PRECACHE( MATERIAL, "vgui/swarm/computer/computerbackdrop" )
PRECACHE( MATERIAL, "vgui/swarm/computer/computerbackdropred" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconcamera" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconcamerastriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconcog" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconcogstriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconfiles" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconfilesstriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconfolder" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconfolderstriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconmail" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconmailstriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconnewslcd" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconnewslcdstriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconstocks" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconstocksstriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconweather" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconweatherstriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconsinglefile" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconturret" )
PRECACHE( MATERIAL, "vgui/swarm/computer/iconturretstriped" )
PRECACHE( MATERIAL, "vgui/swarm/computer/windowclose" )
PRECACHE( MATERIAL, "vgui/swarm/computer/synteklogo2" )

PRECACHE( MATERIAL, "vgui/swarm/computer/tumblerswitchdirection" )
PRECACHE( MATERIAL, "vgui/swarm/computer/tumblerswitchdirection_over" )
PRECACHE( MATERIAL, "vgui/swarm/computer/TumblerSwitchUP" )
PRECACHE( MATERIAL, "vgui/swarm/computer/TumblerSwitchUPMOUSE" )
PRECACHE( MATERIAL, "vgui/swarm/computer/TumblerSwitchDOWN" )
PRECACHE( MATERIAL, "vgui/swarm/computer/TumblerSwitchDOWNMOUSE" )

PRECACHE( MATERIAL, "vgui/swarm/computer/windowminimise" )
PRECACHE( MATERIAL, "vgui/swarm/hacking/tilehoriz" )
PRECACHE( MATERIAL, "vgui/swarm/hacking/tileleft" )
PRECACHE( MATERIAL, "vgui/swarm/hacking/tileright" )
PRECACHE( MATERIAL, "vgui/swarm/hacking/leftconnect" )
PRECACHE( MATERIAL, "vgui/swarm/hacking/rightconnect" )
PRECACHE( MATERIAL, "vgui/swarm/hacking/swarmdoorhackopen" )
PRECACHE( MATERIAL, "vgui/swarm/hacking/fastmarker" )
PRECACHE( MATERIAL, "vgui/swarm/hud/tickboxticked" )
PRECACHE( MATERIAL, "vgui/swarm/hud/horizdoorhealthbar" )
PRECACHE( MATERIAL, "vgui/swarm/hud/horizdoorhealthbarr" )
PRECACHE( MATERIAL, "vgui/swarm/hud/horizdoorhealthbari" )
PRECACHE( MATERIAL, "vgui/swarm/hud/doorhealth" )
PRECACHE( MATERIAL, "vgui/swarm/hud/doorweld" )

//PRECACHE( MATERIAL, "vgui/swarm/briefing/statshots" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/statdamage" )
PRECACHE( MATERIAL, "vgui/swarm/briefing/skillparticle" )
PRECACHE( MATERIAL, "vgui/icon_button_arrow_right" )

//PRECACHE( MATERIAL, "vgui/swarm/briefing/statkilled" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/stattime" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/stataccuracy" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/statfriendly" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/statshots2" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/statskillpoints" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/statburned" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/statheal" )
//PRECACHE( MATERIAL, "vgui/swarm/briefing/stathack" )
PRECACHE( MATERIAL, "vgui/swarm/briefing/topleftbracket" )
PRECACHE( MATERIAL, "vgui/swarm/briefing/toprightbracket" )
PRECACHE( MATERIAL, "vgui/swarm/briefing/bottomleftbracket" )
PRECACHE( MATERIAL, "vgui/swarm/briefing/bottomrightbracket" )
PRECACHE( MATERIAL, "vgui/swarm/briefing/shadedbutton" )
PRECACHE( MATERIAL, "vgui/swarm/briefing/shadedbutton_over" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconBloodhound" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconCargoForklift" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconDroneSide" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconDroneTop" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconEgg" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconParasiteSide" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconParasiteTop" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconPlanet" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconSolarSystem" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconStarchart" )
//PRECACHE( MATERIAL, "vgui/swarm/ObjectiveIcons/OIconTerminal" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Accuracy" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Agility" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Autogun" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Drugs" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Engineering" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Grenade" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Hacking" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Healing" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Health" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Leadership" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Melee" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Piercing" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Scanner" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Secondary" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/SkillIncendiary" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/SkillPlus" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/SkillReload" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Vindicator" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Xenowound" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Accuracy_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Agility_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Autogun_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Drugs_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Engineering_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Grenade_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Hacking_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Healing_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Health_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Leadership_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Melee_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Piercing_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Scanner_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Secondary_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/SkillIncendiary_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/SkillPlus_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/SkillReload_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Vindicator_over" )
PRECACHE( MATERIAL, "vgui/swarm/SkillButtons/Xenowound_over" )
PRECACHE( MATERIAL, "vgui/swarm/playerlist/booticon" )
PRECACHE( MATERIAL, "vgui/swarm/playerlist/leadericon" )
PRECACHE( PARTICLE_SYSTEM, "weapon_shell_casing_rifle" )
PRECACHE( PARTICLE_SYSTEM, "weapon_shell_casing_shotgun" )
PRECACHE( PARTICLE_SYSTEM, "snow_explosion" )
PRECACHE( PARTICLE_SYSTEM, "impact_steam" )
PRECACHE( PARTICLE_SYSTEM, "impact_steam_small" )
PRECACHE( PARTICLE_SYSTEM, "impact_steam_short" )
PRECACHE_REGISTER_END()

#define ASW_BLOOD_BRIGHTNESS 0.25f
//#define ASW_DO_BLOOD_LIGHT_CALCS		// define to make blood particle systems scale color/alpha by light at that point

extern void GetBloodColor( int bloodtype, colorentry_t &color );
extern PMaterialHandle g_Material_Spark;
PMaterialHandle g_Material_Blue_nocull;

//-----------------------------------------------------------------------------
// Purpose: Used for bullets hitting bleeding surfaces
//-----------------------------------------------------------------------------
void ASW_FX_BloodBulletImpact( const Vector &origin, const Vector &normal, float scale, unsigned char r, unsigned char g, unsigned char b )
{
	if ( UTIL_IsLowViolence() )
		return;

	Vector offset;
#ifdef ASW_DO_BLOOD_LIGHT_CALCS
	Vector color = engine->GetLightForPointFast(origin, true);
	color.x = LinearToTexture( color.x ) / 255.0f;
	color.y = LinearToTexture( color.y ) / 255.0f;
	color.z = LinearToTexture( color.z ) / 255.0f;

	// only use half of lighting
	color.x += (1.0f - color.x) * ASW_BLOOD_BRIGHTNESS;
	color.y += (1.0f - color.y) * ASW_BLOOD_BRIGHTNESS;
	color.z += (1.0f - color.z) * ASW_BLOOD_BRIGHTNESS;

	color.x *= (r/255.0f);
	color.y *= (g/255.0f);
	color.z *= (b/255.0f);
#else
	Vector color = Vector(r/255.0f,g/255.0f,b/255.0f);
#endif
	float colorRamp;

	Vector	offDir;

	CSmartPtr<CBloodSprayEmitter> pSimple = CBloodSprayEmitter::Create( "bloodgore" );
	if ( !pSimple )
		return;

	pSimple->SetSortOrigin( origin );
	pSimple->SetGravity( 0 );

	// Blood impact
	PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_core" );

	SimpleParticle *pParticle;

	Vector	dir = normal * RandomVector( -0.5f, 0.5f );

	offset = origin + ( 2.0f * normal );

	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, offset );

	if ( pParticle != NULL )
	{
		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 0.25f, 0.5f) * 1.5f;

		pParticle->m_vecVelocity	= dir * random->RandomFloat( 16.0f, 32.0f );
		pParticle->m_vecVelocity[2] -= random->RandomFloat( 8.0f, 16.0f );

		colorRamp = random->RandomFloat( 0.75f, 2.0f );

		pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
		
		pParticle->m_uchStartSize	= random->RandomInt( 3, 3 ) * scale;		//  was 2 -> 4
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 8 * scale;
	
		pParticle->m_uchStartAlpha	= 255.0f * ASW_BLOOD_BRIGHTNESS;
		pParticle->m_uchEndAlpha	= 0;
		
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -2.0f, 2.0f );
	}

	hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_gore" );

	for ( int i = 0; i < 4; i++ )
	{
		offset = origin + ( 2.0f * normal );

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, offset );

		if ( pParticle != NULL )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= random->RandomFloat( 0.5f, 0.75f) * 1.5f;

			pParticle->m_vecVelocity	= dir * random->RandomFloat( 16.0f, 32.0f )*(i+1);
			pParticle->m_vecVelocity[2] -= random->RandomFloat( 32.0f, 64.0f )*(i+1);

			colorRamp = random->RandomFloat( 0.75f, 2.0f );

			pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
			pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
			pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
			
			pParticle->m_uchStartSize	= random->RandomInt( 2, 2 ) * scale * 0.4; // was 1 to 2
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 2 * scale * 0.4;
		
			pParticle->m_uchStartAlpha	= 255;
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
		}
	}

	//
	// Dump out drops
	//
	TrailParticle *tParticle;

	CSmartPtr<CTrailParticles> pTrailEmitter = CTrailParticles::Create( "blooddrops" );
	if ( !pTrailEmitter )
		return;

	pTrailEmitter->SetSortOrigin( origin );

	// Partial gravity on blood drops
	pTrailEmitter->SetGravity( 400.0 ); 
	
	// Enable simple collisions with nearby surfaces
	pTrailEmitter->Setup(origin, &normal, 1, 10, 100, 400, 0.2, 0 );

	hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_drop" );

	//
	// Shorter droplets
	//
	for ( int i = 0; i < 8; i++ )
	{
		// Originate from within a circle 'scale' inches in diameter
		offset = origin + RandomVector(-scale, scale);

		tParticle = (TrailParticle *) pTrailEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( tParticle == NULL )
			break;

		tParticle->m_flLifetime	= 0.0f;

		offDir = RandomVector( -1.0f, 1.0f );

		tParticle->m_vecVelocity = offDir * random->RandomFloat( 64.0f, 128.0f );

		tParticle->m_flWidth		= random->RandomFloat( 0.5f, 2.0f ) * scale;
		tParticle->m_flLength		= random->RandomFloat( 0.05f, 0.15f ) * scale;
		tParticle->m_flDieTime		= random->RandomFloat( 0.25f, 0.5f ) * 1.5f;

		FloatToColor32( tParticle->m_color, color[0], color[1], color[2], 1.0f );
	}

	// TODO: Play a sound?
	//CLocalPlayerFilter filter;
	//C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, CHAN_VOICE, "Physics.WaterSplash", 1.0, ATTN_NORM, 0, 100, &origin );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void ASW_BloodImpactCallback( const CEffectData & data )
{
	Vector vecPosition;
	vecPosition = data.m_vOrigin;
	
	// Fetch the blood color.
	colorentry_t color;
	GetBloodColor( data.m_nColor, color );

	ASW_FX_BloodBulletImpact( vecPosition, data.m_vNormal, data.m_flScale, color.r, color.g, color.b );
}

DECLARE_CLIENT_EFFECT( ASWBloodImpact, ASW_BloodImpactCallback );


void WeldSparkCallback( const CEffectData & data )
{
	Vector vecNormal;
	Vector vecPosition;

	vecPosition = data.m_vOrigin;
	vecNormal = data.m_vNormal;

	FX_Sparks( vecPosition, 1, 2, vecNormal, 5, 32, 160 );
}

DECLARE_CLIENT_EFFECT( ASWWelderSparks, WeldSparkCallback );


// ==========
// Drone Gibs
// ==========

#define	NUM_DRONE_GIBS_UNIQUE	16
const char *pszDroneGibs_Unique[NUM_DRONE_GIBS_UNIQUE] = {
"models/swarm/DroneGibs/dronepart01.mdl",
//"models/swarm\DroneGibs\DronePart02.mdl",	// mouth
"models/swarm/DroneGibs/dronepart20.mdl",
//"models/swarm\DroneGibs\DronePart21.mdl",	// eyeball
"models/swarm/DroneGibs/dronepart29.mdl",
"models/swarm/DroneGibs/dronepart31.mdl",
"models/swarm/DroneGibs/dronepart32.mdl",
//"models/swarm\DroneGibs\DronePart36.mdl",	// wing
//"models/swarm\DroneGibs\DronePart37.mdl",	// wing
//"models/swarm\DroneGibs\DronePart38.mdl",		// wings
//"models/swarm\DroneGibs\DronePart39.mdl",		// wings
"models/swarm/DroneGibs/dronepart44.mdl",
"models/swarm/DroneGibs/dronepart45.mdl",
"models/swarm/DroneGibs/dronepart47.mdl",
"models/swarm/DroneGibs/dronepart49.mdl",
"models/swarm/DroneGibs/dronepart50.mdl",
"models/swarm/DroneGibs/dronepart53.mdl",
"models/swarm/DroneGibs/dronepart54.mdl",
"models/swarm/DroneGibs/dronepart56.mdl",
"models/swarm/DroneGibs/dronepart57.mdl",
"models/swarm/DroneGibs/dronepart58.mdl",
"models/swarm/DroneGibs/dronepart59.mdl"
};

ConVar g_drone_maxgibs( "g_drone_maxgibs", "16", FCVAR_ARCHIVE );

void CDroneGibManager::LevelInitPreEntity( void )
{
	m_LRU.Purge();
}

CDroneGibManager s_DroneGibManager;

void CDroneGibManager::AddGib( C_BaseEntity *pEntity )
{
	m_LRU.AddToTail( pEntity );
}

void CDroneGibManager::RemoveGib( C_BaseEntity *pEntity )
{
	m_LRU.FindAndRemove( pEntity );
}
	

//-----------------------------------------------------------------------------
// Methods of IGameSystem
//-----------------------------------------------------------------------------
void CDroneGibManager::Update( float frametime )
{
	if ( m_LRU.Count() < g_drone_maxgibs.GetInt() )
		 return;
	
	int i = 0;
	i = m_LRU.Head();

	if ( m_LRU[ i ].Get() )
	{
		 m_LRU[ i ].Get()->SetNextClientThink( gpGlobals->curtime );
	}

	m_LRU.Remove(i);
}

// Drone gib - marks surfaces when it bounces

class C_DroneGib : public C_Gib
{
	typedef C_Gib BaseClass;
public:
	
	static C_DroneGib *C_DroneGib::CreateClientsideGib( const char *pszModelName,
		Vector vecOrigin, Vector vecForceDir, AngularImpulse vecAngularImp,
		float m_flLifetime = DEFAULT_GIB_LIFETIME, int skin=0 )
	{
		C_DroneGib *pGib = new C_DroneGib;

		if ( pGib == NULL )
			return NULL;

		if ( pGib->InitializeGib( pszModelName, vecOrigin, vecForceDir, vecAngularImp, m_flLifetime ) == false )
			return NULL;

		pGib->SetSkin( skin );

		s_DroneGibManager.AddGib( pGib );

		// attach an emitter ot it

		C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
		if (pEmitter)
		{
			if (pEmitter->InitializeAsClientEntity( NULL, false ))
			{
				// randomly pick a jet, a drip or a burst
				float f = random->RandomFloat();
				if (f < 0.33f)
					Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "dronebloodjet");
				else if (f < 0.66f)
					Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "dronebloodburst");
				else
					Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "droneblooddroplets");
				pEmitter->m_fScale = 1.0f;
				pEmitter->m_bEmit = true;
				pEmitter->SetAbsOrigin(vecOrigin);
				pEmitter->CreateEmitter();
				pEmitter->SetAbsOrigin(vecOrigin + Vector(0,0,30));
				pEmitter->SetAbsAngles(pGib->GetAbsAngles());

				// randomly pick an attach point
				pEmitter->ClientAttach(pGib, "bleed");
				pEmitter->SetDieTime(gpGlobals->curtime + asw_gib_bleed_time.GetFloat());	// stop emitting once the ragdoll gibs
			}
			else
			{
				UTIL_Remove( pEmitter );
			}
		}

		return pGib;
	}

	// Decal the surface
	virtual	void HitSurface( C_BaseEntity *pOther )
	{

	}
};


void FX_DroneBleed( const Vector &origin, const Vector &direction, float scale )
{
	Vector	offset;

#ifdef ASW_DO_BLOOD_LIGHT_CALCS
	Vector color = engine->GetLightForPointFast(origin, true);
	color.x = 0;
	color.y = LinearToTexture( color.y ) / 255.0f;
	color.z = 0;

	// only use half of lighting	
	color.y += (1.0f - color.y) * ASW_BLOOD_BRIGHTNESS;	
	color.y *= 255.0f;
#else
	Vector color(0, 255, 0);
#endif

	// Throw some blood
	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_DroneGib" );
	pSimple->SetSortOrigin( origin );

	PMaterialHandle	hMaterial = pSimple->GetPMaterial( "effects/blood" );

	Vector	vDir;

	vDir.Random( -1.0f, 1.0f );

	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );
			
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );
			
		float	speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] += 16.0f;

		sParticle->m_uchColor[0]	= 0;
		sParticle->m_uchColor[1]	= color.y;
		sParticle->m_uchColor[2]	= 0;
		sParticle->m_uchStartAlpha	= 255.0f * ASW_BLOOD_BRIGHTNESS;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 8, 16 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}

	hMaterial = pSimple->GetPMaterial( "effects/blood2" );

	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );
			
		if ( sParticle == NULL )
		{
			return;
		}

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );
			
		float	speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] += 16.0f;

		sParticle->m_uchColor[0]	= 0;
		sParticle->m_uchColor[1]	= color.y;
		sParticle->m_uchColor[2]	= 0;
		sParticle->m_uchStartAlpha	= random->RandomInt( 64, 128 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 8, 16 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}
}

void DroneBleedCallback( const CEffectData &data )
{
	FX_DroneBleed( data.m_vOrigin, data.m_vNormal, data.m_flScale );
}

DECLARE_CLIENT_EFFECT( DroneBleed, DroneBleedCallback );

void FX_GibMeshEmitter( const char *szModel, const char *szTemplate, const Vector &origin, const Vector &direction, int skin, float fScale, bool bFrozen )
{
	C_ASW_Mesh_Emitter *pEmitter = new C_ASW_Mesh_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( szModel, false ))
		{
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), szTemplate);
			pEmitter->SetSkin( skin );
			pEmitter->m_fScale = fScale;
			pEmitter->m_bEmit = true;
			pEmitter->SetAbsOrigin(origin);			
			pEmitter->CreateEmitter(direction);
			pEmitter->SetAbsOrigin(origin);
			pEmitter->SetDieTime(gpGlobals->curtime + 15.0f);
			pEmitter->SetFrozen( bFrozen );
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}
}

void FX_DroneGib( const Vector &origin, const Vector &direction, float scale, int skin, bool bOnFire )
{
	Vector	offset;
	// Throw some blood
#ifdef ASW_DO_BLOOD_LIGHT_CALCS
	Vector color = engine->GetLightForPointFast(origin, true);
	color.x = 0;
	color.y = LinearToTexture( color.y ) / 255.0f;
	color.z = 0;

	// only use half of lighting
	color.y += (1.0f - color.y) * ASW_BLOOD_BRIGHTNESS;
	color.y *= 255.0f;
#else
	Vector color(0, 255, 0);
#endif

	QAngle	vecAngles;
	VectorAngles( -direction, vecAngles );
	DispatchParticleEffect( "drone_death", origin, vecAngles );

	// make our gib emitters
	Vector vecForce = direction * 100.0f;
	if (bOnFire)
	{		
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart01.mdl", "dronegibfire1", origin, vecForce, skin);
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart58.mdl", "dronegibfire1", origin, vecForce, skin);
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart29.mdl", "dronegibfire1", origin, vecForce, skin);
	}
	else
	{
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart01.mdl", "dronegib1", origin, vecForce, skin);
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart58.mdl", "dronegib2", origin, vecForce, skin);
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart29.mdl", "dronegib3", origin, vecForce, skin);
	}
}

void DroneGibCallback( const CEffectData &data )
{
	FX_DroneGib( data.m_vOrigin, data.m_vNormal, data.m_flScale, data.m_nColor, (data.m_fFlags & ASW_GIBFLAG_ON_FIRE) );
}

DECLARE_CLIENT_EFFECT( DroneGib, DroneGibCallback );



void FX_HarvesterGib( const Vector &origin, const Vector &direction, float scale, int skin, bool bOnFire )
{
	Vector	offset;
#ifdef ASW_DO_BLOOD_LIGHT_CALCS
	Vector color = engine->GetLightForPointFast(origin, true);
	color.x = 0;
	color.y = LinearToTexture( color.y ) / 255.0f;
	color.z = 0;

	// only use half of lighting
	color.y += (1.0f - color.y) * ASW_BLOOD_BRIGHTNESS;
	color.y *= 255.0f;
#else
	Vector color(0, 255, 0);
#endif

	// Throw some blood
	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_HarvesterGib" );
	pSimple->SetSortOrigin( origin );

	PMaterialHandle	hMaterial = pSimple->GetPMaterial( "effects/blood" );
	Vector	vDir;
	vDir.Random( -1.0f, 1.0f );
	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );
			
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );
			
		float	speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] += 16.0f;

		sParticle->m_uchColor[0]	= 0;
		sParticle->m_uchColor[1]	= color.y;
		sParticle->m_uchColor[2]	= 0;
		sParticle->m_uchStartAlpha	= 255.0f * ASW_BLOOD_BRIGHTNESS;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}

	hMaterial = pSimple->GetPMaterial( "effects/blood2" );

	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );
			
		if ( sParticle == NULL )
		{
			return;
		}

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );
			
		float	speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] += 16.0f;

		sParticle->m_uchColor[0]	= 0;
		sParticle->m_uchColor[1]	= color.y;
		sParticle->m_uchColor[2]	= 0;
		sParticle->m_uchStartAlpha	= random->RandomInt( 64, 128 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}

	// make our gib emitters
	Vector vecForce = direction * 100.0f;
	if (bOnFire)
	{		
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart58.mdl", "dronegibfire1", origin, vecForce, skin);
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart58.mdl", "dronegibfire1", origin, vecForce, skin);
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart59.mdl", "dronegibfire1", origin, vecForce, skin);
	}
	else
	{
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart58.mdl", "dronegib1", origin, vecForce, skin);
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart58.mdl", "dronegib2", origin, vecForce, skin);
		FX_GibMeshEmitter("models/swarm/DroneGibs/dronepart59.mdl", "dronegib3", origin, vecForce, skin);
	}

	CLocalPlayerFilter filter;
	CSoundParameters params;

	// make a gib sound
	if ( C_BaseEntity::GetParametersForSound( "ASW_Drone.GibSplat", params, NULL ) )
	{
		EmitSound_t ep( params );
		
		ep.m_flVolume = 1.0f;
		ep.m_nChannel = CHAN_AUTO;
		ep.m_pOrigin = &origin;

		C_BaseEntity::EmitSound( filter, 0, ep );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void HarvesterGibCallback( const CEffectData &data )
{
	FX_HarvesterGib( data.m_vOrigin, data.m_vNormal, data.m_flScale, data.m_nColor, (data.m_fFlags & ASW_GIBFLAG_ON_FIRE) );
}

DECLARE_CLIENT_EFFECT( HarvesterGib, HarvesterGibCallback );

// Grub Gibs

#define	NUM_GRUB_GIBS_UNIQUE	6
const char *pszGrubGibs_Unique[NUM_GRUB_GIBS_UNIQUE] = {
"models/Swarm/Grubs/GrubGib1.mdl",
"models/Swarm/Grubs/GrubGib2.mdl",
"models/Swarm/Grubs/GrubGib3.mdl",
"models/Swarm/Grubs/GrubGib4.mdl",
"models/Swarm/Grubs/GrubGib5.mdl",
"models/Swarm/Grubs/GrubGib6.mdl"
};

void FX_GrubGib( const Vector &origin, const Vector &direction, float scale, bool bOnFire )
{
	Vector offset = origin + Vector(0,0,8);
	if (bOnFire)
		DispatchParticleEffect( "grub_death_fire", offset, QAngle( 0, 0, 0 ) );
	else
		DispatchParticleEffect( "grub_death", offset, QAngle( 0, 0, 0 ) );
	
	CLocalPlayerFilter filter;						
	CSoundParameters params;

	// make a gib sound
	if ( C_BaseEntity::GetParametersForSound( "ASW_Drone.GibSplatQuiet", params, NULL ) )
	{
		EmitSound_t ep( params );
		ep.m_pOrigin = &origin;

		C_BaseEntity::EmitSound( filter, 0, ep );
	}
}

void GrubGibCallback( const CEffectData &data )
{
	FX_GrubGib( data.m_vOrigin, data.m_vNormal, data.m_flScale, (data.m_fFlags & ASW_GIBFLAG_ON_FIRE) );
}

DECLARE_CLIENT_EFFECT( GrubGib, GrubGibCallback );

// ==========
// Parasite Gibs
// ==========

#define	NUM_PARASITE_GIBS_UNIQUE	8
const char *pszParasiteGibs_Unique[NUM_PARASITE_GIBS_UNIQUE] = {
"models/swarm/Parasite/ParasiteGibAbdomen.mdl",
"models/swarm/Parasite/ParasiteGibHead.mdl",
"models/swarm/Parasite/ParasiteGibFrontLeg.mdl",
"models/swarm/Parasite/ParasiteGibMidLeg.mdl",
"models/swarm/Parasite/ParasiteGibBackLeg.mdl",
"models/swarm/Parasite/ParasiteGibFrontLeg.mdl",
"models/swarm/Parasite/ParasiteGibMidLeg.mdl",
"models/swarm/Parasite/ParasiteGibBackLeg.mdl"
};

void FX_ParasiteGib( const Vector &origin, const Vector &direction, float scale, int skin, bool bUseGibImpactSounds, bool bOnFire )
{
	Vector	offset;
	// make our gib emitters.
	Vector vecForce = direction;
	vecForce.z = 1.0f;
	vecForce *= 100.0f;
	Vector gibspot = origin + Vector(0,0,5);
	if (bUseGibImpactSounds)
	{
		if (bOnFire)
		{
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibMidLeg.mdl", "parasitegibfire1", gibspot, vecForce, 0);
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibHead.mdl", "parasitegibfire1", gibspot, vecForce, 0);
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibAbdomen.mdl", "parasitegibfire1", gibspot, vecForce, 0);
		}
		else
		{
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibMidLeg.mdl", "parasitegib2", gibspot, vecForce, 0);
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibHead.mdl", "parasitegib1", gibspot, vecForce, 0);
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibAbdomen.mdl", "parasitegib1", gibspot, vecForce, 0);
		}
	}
	else
	{
		if (bOnFire)
		{
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibMidLeg.mdl", "parasitegibfire1quiet", gibspot, vecForce, 0);
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibHead.mdl", "parasitegibfire1quiet", gibspot, vecForce, 0);
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibAbdomen.mdl", "parasitegibfire1quiet", gibspot, vecForce, 0);
		}
		else
		{
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibMidLeg.mdl", "parasitegib2quiet", gibspot, vecForce, 0);
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibHead.mdl", "parasitegib1quiet", gibspot, vecForce, 0);
			FX_GibMeshEmitter("Models/Swarm/Parasite/ParasiteGibAbdomen.mdl", "parasitegib1quiet", gibspot, vecForce, 0);
		}
	}
	
	CLocalPlayerFilter filter;						
	CSoundParameters params;

	// make a gib sound
	if ( C_BaseEntity::GetParametersForSound( "ASW_Drone.GibSplatQuiet", params, NULL ) )
	{
		EmitSound_t ep( params );
		ep.m_pOrigin = &origin;

		C_BaseEntity::EmitSound( filter, 0, ep );
	}

	// Throw some blood

	QAngle	vecAngles;
	VectorAngles( direction, vecAngles );
	DispatchParticleEffect( "drone_shot", origin, vecAngles );

	/*
	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_DroneGib" );
	pSimple->SetSortOrigin( origin );

	PMaterialHandle	hMaterial = pSimple->GetPMaterial( "effects/blood" );

	Vector	vDir;

#ifdef ASW_DO_BLOOD_LIGHT_CALCS
	Vector color = engine->GetLightForPointFast(origin, true);
	color.x = 0;
	color.y = LinearToTexture( color.y ) / 255.0f;
	color.z = 0;

	// only use half of lighting
	color.y += (1.0f - color.y) * ASW_BLOOD_BRIGHTNESS;
	color.y *= 255.0f;
#else
	Vector color(0, 255, 0);
#endif

	vDir.Random( -1.0f, 1.0f );

	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );
			
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );
			
		float	speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] += 16.0f;

		sParticle->m_uchColor[0]	= 0;
		sParticle->m_uchColor[1]	= color.y;
		sParticle->m_uchColor[2]	= 0;
		sParticle->m_uchStartAlpha	= 255.0f * ASW_BLOOD_BRIGHTNESS;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}

	hMaterial = pSimple->GetPMaterial( "effects/blood2" );

	for ( int i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );
			
		if ( sParticle == NULL )
		{
			return;
		}

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random->RandomFloat( 0.5f, 0.75f );
			
		float	speed = random->RandomFloat( 16.0f, 64.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] += 16.0f;

		sParticle->m_uchColor[0]	= 0;
		sParticle->m_uchColor[1]	= color.y;
		sParticle->m_uchColor[2]	= 0;
		sParticle->m_uchStartAlpha	= random->RandomInt( 64, 128 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2;
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void ParasiteGibCallback( const CEffectData &data )
{
	FX_ParasiteGib( data.m_vOrigin, data.m_vNormal, data.m_flScale, data.m_nColor, true, (data.m_fFlags & ASW_GIBFLAG_ON_FIRE) );
}

DECLARE_CLIENT_EFFECT( ParasiteGib, ParasiteGibCallback );

void HarvesiteGibCallback( const CEffectData &data )
{
	FX_ParasiteGib( data.m_vOrigin, data.m_vNormal, data.m_flScale, data.m_nColor, false, (data.m_fFlags & ASW_GIBFLAG_ON_FIRE) );
}

DECLARE_CLIENT_EFFECT( HarvesiteGib, HarvesiteGibCallback );


void FX_QueenSpitBurst( const Vector &origin, const Vector &direction, float scale, int skin )
{
	C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( NULL, false ))
		{
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "queenburst");
			pEmitter->m_fScale = scale;
			pEmitter->m_bEmit = true;
			pEmitter->SetAbsOrigin(origin);
			pEmitter->CreateEmitter();
			pEmitter->SetAbsOrigin(origin);
			pEmitter->SetDieTime(gpGlobals->curtime + 3.0f);
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}
}

void QueenSpitBurstCallback( const CEffectData &data )
{
	FX_QueenSpitBurst( data.m_vOrigin, data.m_vNormal, data.m_flScale, 0 );
}

DECLARE_CLIENT_EFFECT( QueenSpitBurst, QueenSpitBurstCallback );

// egg gibs
void FX_EggGibs( const Vector &origin, int flags, int iEntIndex )
{
	C_ASW_Egg *pEgg = dynamic_cast<C_ASW_Egg*>(ClientEntityList().GetEnt(iEntIndex));
	MDLCACHE_CRITICAL_SECTION();
	C_BaseAnimating::PushAllowBoneAccess( true, false, "FX_EggGibs" );

	if (flags & EGG_FLAG_OPEN && pEgg)
	{
		DispatchParticleEffect( "egg_open", PATTACH_POINT_FOLLOW, pEgg, "attach_death" );
	}

	if (flags & EGG_FLAG_HATCH && pEgg)
	{
		DispatchParticleEffect( "egg_hatch", PATTACH_POINT_FOLLOW, pEgg, "attach_death" );
	}

	if (flags & EGG_FLAG_DIE)
	{
		DispatchParticleEffect( "egg_death", origin, QAngle( 0, 0, 0 ) );
	}

	if (flags & EGG_FLAG_GRUBSACK_DIE)
	{
		DispatchParticleEffect( "grubsack_death", origin, QAngle( 0, 0, 0 ) );
	}

	CLocalPlayerFilter filter;						
	CSoundParameters params;

	// make a gib sound
	if ( C_BaseEntity::GetParametersForSound( "ASW_Drone.GibSplatQuiet", params, NULL ) )
	{
		EmitSound_t ep( params );
		ep.m_pOrigin = &origin;

		C_BaseEntity::EmitSound( filter, 0, ep );
	}

	C_BaseAnimating::PopBoneAccess( "FX_EggGibs" );
}


void __MsgFunc_ASWEggEffects( bf_read &msg )
{
	Vector vecOrigin;
	vecOrigin.x = msg.ReadFloat();
	vecOrigin.y = msg.ReadFloat();
	vecOrigin.z = msg.ReadFloat();

	int iFlags = msg.ReadShort();	

	int iEggIndex = msg.ReadShort();		

	FX_EggGibs( vecOrigin, iFlags, iEggIndex );
}
USER_MESSAGE_REGISTER( ASWEggEffects );

/*
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void EggGibsCallback( const CEffectData &data )
{

	FX_EggGibs( data.m_vOrigin, data.m_fFlags, data.m_nOtherEntIndex );
}

DECLARE_CLIENT_EFFECT( EggGibs, EggGibsCallback );
*/

void FX_BuildStunElectroBeam( C_BaseEntity *pEntity, Vector &vecOrigin, Vector &vecEnd )
{
	BeamInfo_t beamInfo;
	beamInfo.m_pStartEnt = pEntity;
	beamInfo.m_nStartAttachment = 0;
	beamInfo.m_pEndEnt = NULL;
	beamInfo.m_nEndAttachment = 0;
	beamInfo.m_nType = TE_BEAMTESLA;
	beamInfo.m_vecStart = vecOrigin;
	beamInfo.m_vecEnd = vecEnd;
	beamInfo.m_pszModelName = "swarm/effects/electrostunbeam.vmt";
	beamInfo.m_flHaloScale = 0.0;
	beamInfo.m_flLife = random->RandomFloat( 0.45f, 0.5f );
	beamInfo.m_flWidth = random->RandomFloat( 4.0f, 7.0f );
	beamInfo.m_flEndWidth = 1.0f;
	beamInfo.m_flFadeLength = 0.5f;
	beamInfo.m_flAmplitude = 24;
	beamInfo.m_flBrightness = 255.0;
	beamInfo.m_flSpeed = 150.0f;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 30.0;
	beamInfo.m_flRed = 255.0;
	beamInfo.m_flGreen = 255.0;
	beamInfo.m_flBlue = 255.0;
	beamInfo.m_nSegments = 18;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = 0; //FBEAM_ONLYNOISEONCE;
	
	beams->CreateBeamEntPoint( beamInfo );
}


void FX_ProbeStunElectroBeam( CBaseEntity *pEntity, mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, bool bRandom, float flYawOffset )
{
	Vector vecOrigin;
	QAngle vecAngles;
	MatrixGetColumn( hitboxToWorld, 3, vecOrigin );
	MatrixAngles( hitboxToWorld, vecAngles.Base() );

	// Make a couple of tries at it
	int iTries = -1;
	Vector vecForward;
	trace_t tr;
	do
	{
		iTries++;

		// Some beams are deliberatly aimed around the point, the rest are random.
		if ( !bRandom )
		{
			QAngle vecTemp = vecAngles;
			vecTemp[YAW] += flYawOffset;
			AngleVectors( vecTemp, &vecForward );

			// Randomly angle it up or down
			vecForward.z = RandomFloat( -1, 1 );
		}
		else
		{
			vecForward = RandomVector( -1, 1 );
		}

		UTIL_TraceLine( vecOrigin, vecOrigin + (vecForward * 48), MASK_SHOT, pEntity, COLLISION_GROUP_NONE, &tr );
	} while ( tr.fraction >= 1.0 && iTries < 3 );

	Vector vecEnd = tr.endpos - (vecForward * 8);

	// Only spark & glow if we hit something
	if ( tr.fraction < 1.0 )
	{
		if ( !EffectOccluded( tr.endpos ) )
		{
			//ASSERT_LOCAL_PLAYER_RESOLVABLE();
			//int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

			DispatchParticleEffect( "electrical_arc_01_system", vecOrigin, vecEnd, vecAngles );
			
			/*
			CUtlReference<CNewParticleEffect> pEffect;
			pEffect = pEntity->ParticleProp()->Create( "ElectroStun_arc_1", PATTACH_ABSORIGIN_FOLLOW );
			pEntity->ParticleProp()->AddControlPoint( pEffect, 1, pEntity, PATTACH_CUSTOMORIGIN );
			pEffect->SetControlPoint( 0, vecOrigin );
			pEffect->SetControlPoint( 1, vecEnd );
			*/
	
			/*
			// Move it towards the camera
			Vector vecFlash = tr.endpos;
			Vector vecForward;
			AngleVectors( MainViewAngles(nSlot), &vecForward );
			vecFlash -= (vecForward * 8);

			FX_ElectroStunSplash( vecFlash, -vecForward, false );

			// End glow
			CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "dust" );
			pSimple->SetSortOrigin( vecFlash );
			SimpleParticle *pParticle;
			pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/tesla_glow_noz" ), vecFlash );
			if ( pParticle != NULL )
			{
				pParticle->m_flLifetime = 0.0f;
				pParticle->m_flDieTime	= RandomFloat( 0.5, 1 );
				pParticle->m_vecVelocity = vec3_origin;
				pParticle->m_uchColor[0]	= 49;
				pParticle->m_uchColor[1]	= 218;
				pParticle->m_uchColor[2]	= 255;
				pParticle->m_uchStartSize	= RandomFloat( 4,6 );
				pParticle->m_uchEndSize		= pParticle->m_uchStartSize - 2;
				pParticle->m_uchStartAlpha	= 255;
				pParticle->m_uchEndAlpha	= 10;
				pParticle->m_flRoll			= RandomFloat( 0,360 );
				pParticle->m_flRollDelta	= 0;
			}
			*/
		}
	}

	//FX_BuildStunElectroBeam( pEntity, vecOrigin, tr.endpos );
}

//-----------------------------------------------------------------------------
// Sorts the components of a vector
//-----------------------------------------------------------------------------
static inline void ASWSortAbsVectorComponents( const Vector& src, int* pVecIdx )
{
	Vector absVec( fabs(src[0]), fabs(src[1]), fabs(src[2]) );

	int maxIdx = (absVec[0] > absVec[1]) ? 0 : 1;
	if (absVec[2] > absVec[maxIdx])
	{
		maxIdx = 2;
	}

	// always choose something right-handed....
	switch(	maxIdx )
	{
	case 0:
		pVecIdx[0] = 1;
		pVecIdx[1] = 2;
		pVecIdx[2] = 0;
		break;
	case 1:
		pVecIdx[0] = 2;
		pVecIdx[1] = 0;
		pVecIdx[2] = 1;
		break;
	case 2:
		pVecIdx[0] = 0;
		pVecIdx[1] = 1;
		pVecIdx[2] = 2;
		break;
	}
}

void ASWComputeRenderInfo( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, 
										 Vector *pVecAbsOrigin, Vector *pXVec, Vector *pYVec )
{
	// Compute the center of the hitbox in worldspace
	Vector vecHitboxCenter;
	VectorAdd( pHitBox->bbmin, pHitBox->bbmax, vecHitboxCenter );
	vecHitboxCenter *= 0.5f;
	VectorTransform( vecHitboxCenter, hitboxToWorld, *pVecAbsOrigin );

	// Get the object's basis
	Vector vec[3];
	MatrixGetColumn( hitboxToWorld, 0, vec[0] );
	MatrixGetColumn( hitboxToWorld, 1, vec[1] );
	MatrixGetColumn( hitboxToWorld, 2, vec[2] );
//	vec[1] *= -1.0f;

	Vector vecViewDir;
	VectorSubtract( CurrentViewOrigin(), *pVecAbsOrigin, vecViewDir );
	VectorNormalize( vecViewDir );

	// Project the shadow casting direction into the space of the hitbox
	Vector localViewDir;
	localViewDir[0] = DotProduct( vec[0], vecViewDir );
	localViewDir[1] = DotProduct( vec[1], vecViewDir );
	localViewDir[2] = DotProduct( vec[2], vecViewDir );

	// Figure out which vector has the largest component perpendicular
	// to the view direction...
	// Sort by how perpendicular it is
	int vecIdx[3];
	ASWSortAbsVectorComponents( localViewDir, vecIdx );

	// Here's our hitbox basis vectors; namely the ones that are
	// most perpendicular to the view direction
	*pXVec = vec[vecIdx[0]];
	*pYVec = vec[vecIdx[1]];

	// Project them into a plane perpendicular to the view direction
	*pXVec -= vecViewDir * DotProduct( vecViewDir, *pXVec );
	*pYVec -= vecViewDir * DotProduct( vecViewDir, *pYVec );
	VectorNormalize( *pXVec );
	VectorNormalize( *pYVec );

	// Compute the hitbox size
	Vector boxSize;
	VectorSubtract( pHitBox->bbmax, pHitBox->bbmin, boxSize );

	// We project the two longest sides into the vectors perpendicular
	// to the projection direction, then add in the projection of the perp direction
	Vector2D size( boxSize[vecIdx[0]], boxSize[vecIdx[1]] );
	size.x *= fabs( DotProduct( vec[vecIdx[0]], *pXVec ) );
	size.y *= fabs( DotProduct( vec[vecIdx[1]], *pYVec ) );

	// Add the third component into x and y
	size.x += boxSize[vecIdx[2]] * fabs( DotProduct( vec[vecIdx[2]], *pXVec ) );
	size.y += boxSize[vecIdx[2]] * fabs( DotProduct( vec[vecIdx[2]], *pYVec ) );

	// Bloat a bit, since the shadow wants to extend outside the model a bit
	size *= 2.0f;

	// Clamp the minimum size
	Vector2DMax( size, Vector2D(10.0f, 10.0f), size );

	// Factor the size into the xvec + yvec
	(*pXVec) *= size.x * 0.5f;
	(*pYVec) *= size.y * 0.5f;
}

void FX_ElectroStun(C_BaseAnimating *pAnimating)
{	
	matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
	if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) == false )
		return;

	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
	if ( pStudioHdr == NULL )
		return;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pAnimating->GetHitboxSet() );
	if ( set == NULL )
		return;

	int nHitbox = random->RandomInt( 0, set->numhitboxes - 1 );
	Vector vecAbsOrigin, xvec, yvec;
	mstudiobbox_t *pBox = set->pHitbox(nHitbox);
	if ( !pBox )
		return;

	Vector vecPosition;
	QAngle vecAngles;
	pAnimating->GetBonePosition( pBox->bone, vecPosition, vecAngles );

	CUtlReference<CNewParticleEffect> pEffect;
	pEffect = pAnimating->ParticleProp()->Create( "ElectroStun_arc_01_system", PATTACH_ABSORIGIN_FOLLOW, -1, vecPosition - pAnimating->GetAbsOrigin() );
}

void FX_ElectoStun(C_BaseAnimating *pAnimating);

void ElectroStunCallback( const CEffectData &data )
{
	C_BaseAnimating *pAnimating = dynamic_cast<C_BaseAnimating*>(data.GetEntity());
	if (pAnimating)
		FX_ElectroStun( pAnimating );
}

DECLARE_CLIENT_EFFECT( ElectroStun, ElectroStunCallback );

//-----------------------------------------------------------------------------
// Purpose: Energy splash for plasma/beam weapon impacts
// Input  : &pos - origin point of effect
//-----------------------------------------------------------------------------
#define	ENERGY_SPLASH_SPREAD	0.7f
#define	ENERGY_SPLASH_MINSPEED	128.0f
#define	ENERGY_SPLASH_MAXSPEED	160.0f
#define	ENERGY_SPLASH_GRAVITY	800.0f
#define	ENERGY_SPLASH_DAMPEN	0.3f

void FX_ElectroStunSplash( const Vector &pos, const Vector &normal, int nFlags )
{
	//VPROF_BUDGET( "FX_ElectroStunSplash", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	Vector	offset = pos + ( normal * 2.0f );

	// Quick flash
	FX_AddQuad( pos,  // origin
				normal,  // normal
				32.0f, // start size
				0,  // end size
				0.75f, // sizeBias
				1.0f, /// startAlpha
				0.0f, // endAlpha
				0.4f, // alphaBias
				random->RandomInt( 0, 360 ),  // yaw 
				0, // deltayaw
				Vector( 1.0f, 1.0f, 1.0f ),	// color 
				0.25f, // lifetime 
				"swarm/effects/bluemuzzle_nocull", // shader
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) ); // flags

	// Lingering burn
	FX_AddQuad( pos,
				normal, 
				8,
				16,
				0.75f, 
				1.0f,
				0.0f,
				0.4f,
				random->RandomInt( 0, 360 ), 
				0,
				Vector( 1.0f, 1.0f, 1.0f ), 
				0.5f, 
				"swarm/effects/bluemuzzle_nocull",
				(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );

	SimpleParticle *sParticle;

	CSmartPtr<CSimpleEmitter> pEmitter;

	pEmitter = CSimpleEmitter::Create( "C_EntityDissolve" );
	pEmitter->SetSortOrigin( pos );

	if ( g_Material_Spark == NULL )
	{
		g_Material_Spark = pEmitter->GetPMaterial( "effects/spark" );
	}

	// Anime ground effects
	for ( int j = 0; j < 8; j++ )
	{
		offset.x = random->RandomFloat( -8.0f, 8.0f );
		offset.y = random->RandomFloat( -8.0f, 8.0f );
		offset.z = random->RandomFloat( 0.0f, 4.0f );

		offset += pos;

		sParticle = (SimpleParticle *) pEmitter->AddParticle( sizeof(SimpleParticle), g_Material_Spark, offset );
		
		if ( sParticle == NULL )
			return;

		sParticle->m_vecVelocity = Vector( Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( 16.0f, 64.0f ) );
		
		sParticle->m_uchStartSize	= random->RandomFloat( 2, 4 );

		sParticle->m_flDieTime = random->RandomFloat( 0.4f, 0.6f );
		
		sParticle->m_flLifetime		= 0.0f;

		sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );

		float alpha = 255;

		sParticle->m_flRollDelta	= Helper_RandomFloat( -4.0f, 4.0f );
		sParticle->m_uchColor[0]	= alpha;
		sParticle->m_uchColor[1]	= alpha;
		sParticle->m_uchColor[2]	= alpha;
		sParticle->m_uchStartAlpha	= alpha;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchEndSize		= 0;
	}
}

void FX_PierceSpark( const Vector &pos, const Vector &normal)
{

	CUtlReference<CNewParticleEffect> pEffect = CNewParticleEffect::CreateOrAggregate( NULL, "piercing_spark", pos, NULL, -1 );
	if ( pEffect.IsValid() && pEffect->IsValid() )
	{
		pEffect->SetSortOrigin( pos );
		pEffect->SetControlPoint( 0, pos );

		pEffect->SetControlPointForwardVector ( 0, -normal );
		//Vector vecForward, vecRight, vecUp;
		//AngleVectors( normal, &vecForward, &vecRight, &vecUp );
		//pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
	}

	/*
	C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( NULL, false ))
		{
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "piercespark");
			pEmitter->m_fScale = asw_pierce_spark_scale.GetFloat();
			pEmitter->m_bEmit = true;
			pEmitter->SetAbsOrigin(pos);
			pEmitter->CreateEmitter();
			pEmitter->SetAbsOrigin(pos);
			float yaw = UTIL_VecToYaw(normal) + 90;
			QAngle ang(0,yaw,0);
			pEmitter->SetAbsAngles(ang);

			pEmitter->SetDieTime(gpGlobals->curtime + 1.0f);
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}
	*/
}

void PierceSparkCallback( const CEffectData & data )
{
	FX_PierceSpark( data.m_vOrigin, data.m_vNormal );
}

DECLARE_CLIENT_EFFECT( PierceSpark, PierceSparkCallback );

void FX_ExtinguisherCloud( C_BaseAnimating *pEnt, const Vector &pos)
{
	C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( NULL, false ))
		{
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "fireextinguisherself");
			pEmitter->m_bEmit = true;
			pEmitter->SetAbsOrigin(pos);
			pEmitter->CreateEmitter();
			pEmitter->SetAbsOrigin(pos);
			QAngle ang(0,0,0);
			pEmitter->SetAbsAngles(ang);
			if (pEnt)
				pEmitter->ClientAttach(pEnt, "Center");

			pEmitter->SetDieTime(gpGlobals->curtime + 1.0f);
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}
}

void ExtinguisherCloudCallback( const CEffectData & data )
{
	C_BaseAnimating *pAnimating = dynamic_cast<C_BaseAnimating*>(data.GetEntity());
	if (pAnimating)
		FX_ExtinguisherCloud( pAnimating, data.m_vOrigin );
	else
		FX_ExtinguisherCloud( NULL, data.m_vOrigin );
}

DECLARE_CLIENT_EFFECT( ExtinguisherCloud, ExtinguisherCloudCallback );

// Used for sorting hitboxes by volume.
//
struct ASWHitboxVolume_t
{
	int nIndex;			// The index of the hitbox in the model.
	float flVolume;		// The volume of the hitbox in cubic inches.
};


//-----------------------------------------------------------------------------
// Purpose: Callback function to sort hitboxes by decreasing volume.
//			To mix up the sort results a little we pick a random result for
//			boxes within 50 cubic inches of another.
//-----------------------------------------------------------------------------
int __cdecl ASWSortHitboxVolumes(ASWHitboxVolume_t *elem1, ASWHitboxVolume_t *elem2)
{
	if (elem1->flVolume > elem2->flVolume + 50)
	{
		return -1;
	}

	if (elem1->flVolume < elem2->flVolume + 50)
	{
		return 1;
	}

	if (elem1->flVolume != elem2->flVolume)
	{
		return random->RandomInt(-1, 1);
	}

	return 0;
}

inline float ASWCalcBoxVolume(const Vector &mins, const Vector &maxs)
{
	return (maxs.x - mins.x) * (maxs.y - mins.y) * (maxs.z - mins.z);
}

void ASW_AttachFireToHitboxes(C_BaseAnimating *pAnimating, int iNumFires, float fMaxScale)
{
#ifdef INFESTED_FIRE
	if (!pAnimating || !pAnimating->GetModel())
		return;

	if (iNumFires > ASW_NUM_FIRE_EMITTERS)
		iNumFires = ASW_NUM_FIRE_EMITTERS;

	CStudioHdr *pStudioHdr = pAnimating->GetModelPtr();
	if (!pStudioHdr)	
		return;
	
	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pAnimating->m_nHitboxSet );
	if ( !set )
		return;

	if ( !set->numhitboxes )
		return;

	//m_pCachedModel = pAnimating->GetModel();

	CBoneCache *pCache = pAnimating->GetBoneCache( pStudioHdr );
	matrix3x4_t *hitboxbones[MAXSTUDIOBONES];
	pCache->ReadCachedBonePointers( hitboxbones, pStudioHdr->numbones() );

	// Sort the hitboxes by volume.
	ASWHitboxVolume_t hitboxvolume[MAXSTUDIOBONES];
	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		mstudiobbox_t *pBox = set->pHitbox(i);
		hitboxvolume[i].nIndex = i;
		hitboxvolume[i].flVolume = ASWCalcBoxVolume(pBox->bbmin, pBox->bbmax);
	}
	qsort(hitboxvolume, set->numhitboxes, sizeof(hitboxvolume[0]), (int (__cdecl *)(const void *, const void *))ASWSortHitboxVolumes);

	for ( int i = 0; i < iNumFires; i++ )
	{
		int hitboxindex;

		if (!pAnimating->m_hFireEmitters[i].Get())
			return;

		// Pick the 2 biggest hitboxes, or random ones if there are less than 5 hitboxes,
		// then pick random ones after that.
		if (( i < 2 ) && ( i < set->numhitboxes ))
		{
			hitboxindex = i;
		}
		else
		{
			hitboxindex = random->RandomInt( 0, set->numhitboxes - 1 );
		}
		
		mstudiobbox_t *pBox = set->pHitbox( hitboxvolume[hitboxindex].nIndex );
		Assert( hitboxbones[pBox->bone] );
		// Calculate a position within the hitbox to place the fire.
		Vector vecFire = Vector(random->RandomFloat(pBox->bbmin.x, pBox->bbmax.x), random->RandomFloat(pBox->bbmin.y, pBox->bbmax.y), random->RandomFloat(pBox->bbmin.z, pBox->bbmax.z));
		Vector vecAbsOrigin;
		VectorTransform( vecFire, *hitboxbones[pBox->bone], vecAbsOrigin);
		pAnimating->m_hFireEmitters[i]->SetAbsOrigin( vecAbsOrigin );

		float flVolume = hitboxvolume[hitboxindex].flVolume;

		Assert( IsFinite(flVolume) );

#define FLAME_HITBOX_MIN_VOLUME 1000.0f
#define FLAME_HITBOX_MAX_VOLUME 2000.0f		// asw

		if( flVolume < FLAME_HITBOX_MIN_VOLUME )
		{
			flVolume = FLAME_HITBOX_MIN_VOLUME;
		}
		else if( flVolume > FLAME_HITBOX_MAX_VOLUME )
		{
			flVolume = FLAME_HITBOX_MAX_VOLUME;
		}

		pAnimating->m_hFireEmitters[i]->m_fScale = MIN(flVolume * 0.00048f, fMaxScale);
	}
	// note: this is missing any code to scale the emitters to match the hitbox sizes
#endif
}

ConVar asw_rg_explosion("asw_rg_explosion", "0", 0, "Should the rg tracer have an explosion at the end?");

void FX_ASW_RGEffect(const Vector &vecStart, const Vector &vecEnd)
{
	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "RGEffect" );
	if ( !pSimple )
		return;

	pSimple->SetSortOrigin( vecStart );			

	// Blood impact
	PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/yellowflare" );

	SimpleParticle *pParticle;
	Color aura(66, 142, 192, 255);
	Color core(192, 192, 192, 255);
	float scale = 1;

	if (asw_rg_explosion.GetBool())
	{
		BaseExplosionEffect().Create( vecEnd, 2, 0.3, TE_EXPLFLAG_NONE );
		// scaled explosion removed
		//BaseExplosionEffect().CreateScaled( vecEnd, 2, 0.3, TE_EXPLFLAG_NONE );
	}

	Vector diff = vecEnd - vecStart;
	float dist = diff.Length();

	Vector	dir = diff;
	dir.NormalizeInPlace();
	Vector up;
	QAngle ang, backwards_ang;
	VectorAngles( dir, ang );
	VectorAngles( -dir, backwards_ang );
	AngleVectors( ang, NULL, NULL, &up);

	// rg smoke
	C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( NULL, false ))
		{
			// randomly pick a jet, a drip or a burst			
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "railgunsmoke");			
			pEmitter->m_fScale = 1.0f;
			pEmitter->m_bEmit = true;
			pEmitter->SetAbsOrigin(vecEnd);
			pEmitter->CreateEmitter();
			pEmitter->SetAbsOrigin(vecEnd);
			pEmitter->SetAbsAngles(backwards_ang);			
			pEmitter->SetDieTime(gpGlobals->curtime + 2.0f);
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}

	// rg spray
	pEmitter = new C_ASW_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( NULL, false ))
		{
			// randomly pick a jet, a drip or a burst			
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "railgunspray");			
			pEmitter->m_fScale = 1.0f;
			pEmitter->m_bEmit = true;
			pEmitter->SetAbsOrigin(vecEnd);
			pEmitter->CreateEmitter();
			pEmitter->SetAbsOrigin(vecEnd);
			pEmitter->SetAbsAngles(backwards_ang);			
			pEmitter->SetDieTime(gpGlobals->curtime + 1.0f);
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}

	// rg circle
	pEmitter = new C_ASW_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( NULL, false ))
		{
			// randomly pick a jet, a drip or a burst			
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "railguncircle");			
			pEmitter->m_fScale = 1.0f;
			pEmitter->m_bEmit = true;
			pEmitter->SetAbsOrigin(vecEnd);
			pEmitter->CreateEmitter();
			pEmitter->SetAbsOrigin(vecEnd);
			pEmitter->SetAbsAngles(backwards_ang);			
			pEmitter->SetDieTime(gpGlobals->curtime + 1.0f);
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}

	int num_particles = dist / 1.5f;	// spawn 1 particle every 3 units (upped to 1.5)

	VMatrix rot;
	float angle = 0;
	Vector offset;
	for (int i=0;i<num_particles;i++)
	{
		float f = float(i) / float(num_particles);
		Vector vecPos = vecStart + diff * f;
		MatrixBuildRotationAboutAxis( rot, dir, angle );
		Vector3DMultiply( rot, up, offset );
		vecPos += offset * 7;
		angle += 10;
		// aura
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, vecPos );

		if ( pParticle != NULL )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= 0.6f; //0.9f;

			//pParticle->m_vecVelocity	= dir * random->RandomFloat( 16.0f, 32.0f ) + offset * 32.0f;
			//pParticle->m_vecVelocity[2] -= random->RandomFloat( 8.0f, 16.0f );
			//pParticle->m_vecVelocity	= offset * 32.0f;
			pParticle->m_vecVelocity.Init();
			pParticle->m_iFlags = 0;

			float colorRamp = 1.5f * f; //random->RandomFloat( 0.75f, 2.0f );

			pParticle->m_uchColor[0]	= MIN( 255.0f, aura[0] * colorRamp );
			pParticle->m_uchColor[1]	= MIN( 255.0f, aura[1] * colorRamp );
			pParticle->m_uchColor[2]	= MIN( 255.0f, aura[2] * colorRamp );
			
			pParticle->m_uchStartSize	= 4 * scale;
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 5 * scale;
		
			pParticle->m_uchStartAlpha	= 64;
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= 0; // random->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= 0;
		}

		// core
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, vecPos );

		if ( pParticle != NULL )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= 0.6f;

			//pParticle->m_vecVelocity	= dir * random->RandomFloat( 16.0f, 32.0f ) + offset * 32.0f;
			//pParticle->m_vecVelocity[2] -= random->RandomFloat( 8.0f, 16.0f );
			//pParticle->m_vecVelocity	= offset * 32.0f;
			pParticle->m_vecVelocity.Init();
			pParticle->m_iFlags = 0;

			float colorRamp = 1.5f * f; //random->RandomFloat( 0.75f, 2.0f );

			pParticle->m_uchColor[0]	= MIN( 255.0f, core[0] * colorRamp );
			pParticle->m_uchColor[1]	= MIN( 255.0f, core[1] * colorRamp );
			pParticle->m_uchColor[2]	= MIN( 255.0f, core[2] * colorRamp );
			
			pParticle->m_uchStartSize	= 2 * scale;
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 3 * scale;
		
			pParticle->m_uchStartAlpha	= 64;
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= 0; // random->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= 0;
		}
	}
}

extern Vector GetTracerOrigin( const CEffectData &data );
void FX_ASWTracer( const Vector& start, const Vector& end, int velocity, bool makeWhiz, bool bRedTracer, int iForceStyle )
{
	VPROF_BUDGET( "FX_ASWTracer", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	if (iForceStyle == -1)
		iForceStyle = asw_tracer_style.GetInt();

	if (iForceStyle == 0)
		return;

	if (iForceStyle == 2)		// line tracer
	{		
		//Don't make small tracers
		float dist;
		Vector dir;

		VectorSubtract( end, start, dir );
		dist = VectorNormalize( dir );

		// Don't make short tracers.
		if ( dist < 64 )
			return;

		float length = dist;
		if (length > 512.0f)
			length = 512;
		
		float life = ( dist + length ) / velocity;	//NOTENOTE: We want the tail to finish its run as well

		//Add it
		FX_AddDiscreetLine( start, dir, velocity, length, dist, random->RandomFloat( 0.5f, 1.5f ),
			life, bRedTracer ? "swarm/effects/aswtracerred" : "swarm/effects/aswtracer" );
	}
	else		//standard short per bullet tracer
	{
		//Don't make small tracers
		float dist;
		Vector dir;

		VectorSubtract( end, start, dir );
		dist = VectorNormalize( dir );

		// Don't make short tracers.
		if ( dist < 64 )
			return;

		//float length = dist;
		//if (length > 512.0f)
			//length = 512;

		// asw test short tracers
		//float length = random->RandomFloat( 128.0f, 256.0f );
		float length = random->RandomFloat( 64.0f, 128.0f );

		float life = ( dist + length ) / velocity;	//NOTENOTE: We want the tail to finish its run as well
		
		NDebugOverlay::Line( start, start + dir * length, 128, 64, 64, true, 0.33f );

		//Add it
		FX_AddDiscreetLine( start, dir, velocity, length, dist, random->RandomFloat( 0.5f, 1.5f ),
			life, bRedTracer ? "swarm/effects/aswtracernormalred" : "swarm/effects/aswtracernormal" );
	}

	// note: not bothering with whiz sounds as the camera is too high up to hear them anyway
}


void ASWDoParticleTracer( const char *pTracerEffectName, const Vector &vecStart, const Vector &vecEnd, bool bRedTracer, int iAttributeEffects = 0 )
{
	VPROF_BUDGET( "ASWDoParticleTracer", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	if ( asw_tracer_style.GetInt() == 0 )
		return;

	// we want to aggregate particles because we are spawning a lot in rapid succession for tracers - this spawns MUCH less systems to spawn them
	// NOTE: you cannot use beams/ropes in any particle system that is aggregated - they don't work properly :(
	CUtlReference<CNewParticleEffect> pTracer = CNewParticleEffect::CreateOrAggregate( NULL, pTracerEffectName, vecStart, NULL );
	if ( pTracer.IsValid() && pTracer->IsValid() )
	{
		pTracer->SetControlPoint( 0, vecStart );
		pTracer->SetControlPoint( 1, vecEnd );
		// the color
		Vector vecColor = Vector( 1, 1, 1 );
		if ( bRedTracer )
			vecColor = Vector( 1, 0.65, 0.65 );

		pTracer->SetControlPoint( 10, vecColor );
	}

	if ( iAttributeEffects > 0 )
	{
		CUtlReference<CNewParticleEffect> pAttribTracer = CNewParticleEffect::CreateOrAggregate( NULL, "attrib_tracer_fx", vecStart, NULL );
		if ( pAttribTracer.IsValid() && pAttribTracer->IsValid() )
		{
			pAttribTracer->SetControlPoint( 0, vecStart );
			pAttribTracer->SetControlPoint( 1, vecEnd );

			/*
			20.0	freeze
			20.1	fire
			20.2	electric
			21.0	chem
			21.1	explode
			*/

			pAttribTracer->SetControlPoint( 20, Vector( (iAttributeEffects&BULLET_ATT_FREEZE)	? 1.1f : 0, (iAttributeEffects&BULLET_ATT_FIRE)		? 1.1f : 0, (iAttributeEffects&BULLET_ATT_ELECTRIC) ? 1.1f : 0 ) );
			pAttribTracer->SetControlPoint( 21, Vector( (iAttributeEffects&BULLET_ATT_CHEMICAL) ? 1.1f : 0, (iAttributeEffects&BULLET_ATT_EXPLODE)	? 1.1f : 0, 0 ) );
		}
	}
}

void ASWDoParticleTracer( C_ASW_Weapon *pWeapon, const Vector &vecStart, const Vector &vecEnd, bool bRedTracer, int iAttributeEffects )
{
	ASWDoParticleTracer( pWeapon->GetTracerEffectName(), vecStart, vecEnd, bRedTracer, iAttributeEffects );
}

void ASWTracerCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	
	if ( player == NULL )
		return;

	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	float flVelocity = data.m_flScale;
	bool bWhiz = (data.m_fFlags & TRACER_FLAG_WHIZ);
	
	C_ASW_Weapon *pWpn = dynamic_cast<C_ASW_Weapon*>( data.m_hEntity.Get() );
	bool bRedTracer = ( pWpn && pWpn->GetMuzzleFlashRed() );
	
	// Use default velocity if none specified
	if ( !flVelocity )
	{
		flVelocity = 3000;
	}

	// Do tracer effect
	Msg("spawning dispatch effect tracer\n");
	FX_ASWTracer( (Vector&)vecStart, (Vector&)data.m_vOrigin, flVelocity, bWhiz, bRedTracer );
}

DECLARE_CLIENT_EFFECT( ASWTracer, ASWTracerCallback );

static int asw_num_u_tracers = 0;
void ASWUTracer(C_ASW_Marine *pMarine, const Vector &vecEnd, int iAttributeEffects )
{
	MDLCACHE_CRITICAL_SECTION();
	Vector vecStart;
	QAngle vecAngles;

	if ( !pMarine || pMarine->IsDormant() )
		return;

	C_ASW_Weapon *pWpn = pMarine->GetActiveASWWeapon();//dynamic_cast<C_BaseCombatWeapon *>( pEnt );	
	if ( !pWpn || pWpn->IsDormant() )
		return;

	C_BaseAnimating::PushAllowBoneAccess( true, false, "ASWUTracer" );

	pWpn->ProcessMuzzleFlashEvent();

	int nAttachmentIndex = pWpn->LookupAttachment( "muzzle" );
	if ( nAttachmentIndex <= 0 )
		nAttachmentIndex = 1;	// default to the first attachment if it doesn't have a muzzle

	// Get the muzzle origin
	if ( !pWpn->GetAttachment( nAttachmentIndex, vecStart, vecAngles ) )
	{
		return;
	}

	asw_num_u_tracers++;
	if ( asw_use_particle_tracers.GetBool() )
		ASWDoParticleTracer( pWpn, vecStart, vecEnd, pWpn->GetMuzzleFlashRed(), iAttributeEffects );
	else
		FX_ASWTracer( vecStart, vecEnd, 3000, false, pWpn->GetMuzzleFlashRed() );

	// do a trace to the hit surface for impacts
	trace_t tr;
	Vector diff = vecStart - vecEnd;
	diff.NormalizeInPlace();
	diff *= 6;	// go 6 inches away from surfaces
	CTraceFilterSimple traceFilter(pMarine ,COLLISION_GROUP_NONE);
	UTIL_TraceLine(vecEnd + diff, vecEnd - diff, MASK_SHOT, &traceFilter, &tr);
	// do impact effect
	UTIL_ImpactTrace( &tr, DMG_BULLET );

	// make the marine do a firing anim

	pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	C_BaseAnimating::PopBoneAccess( "ASWUTracer" );
}

// no tracer, just do the muzzle flash and impact
void ASWUTracerless(C_ASW_Marine *pMarine, const Vector &vecEnd, int iAttributeEffects )
{
	MDLCACHE_CRITICAL_SECTION();
	Vector vecStart;
	QAngle vecAngles;

	if ( !pMarine || pMarine->IsDormant() )
		return;

	C_ASW_Weapon *pWpn = pMarine->GetActiveASWWeapon();//dynamic_cast<C_BaseCombatWeapon *>( pEnt );	
	if ( !pWpn || pWpn->IsDormant() )
		return;
	
	C_BaseAnimating::PushAllowBoneAccess( true, false, "ASWUTracerless" );

	pWpn->ProcessMuzzleFlashEvent();

	int nAttachmentIndex = pWpn->LookupAttachment( "muzzle" );
	if ( nAttachmentIndex <= 0 )
		nAttachmentIndex = 1;	// default to the first attachment if it doesn't have a muzzle

	// Get the muzzle origin
	if ( !pWpn->GetAttachment( nAttachmentIndex, vecStart, vecAngles ) )
	{
		return;
	}

	// do a trace to the hit surface for impacts
	trace_t tr;
	Vector diff = vecStart - vecEnd;
	diff.NormalizeInPlace();
	diff *= 6;	// go 6 inches away from surfaces
	CTraceFilterSimple traceFilter(pMarine ,COLLISION_GROUP_NONE);
	UTIL_TraceLine(vecEnd + diff, vecEnd - diff, MASK_SHOT, &traceFilter, &tr);
	// do impact effect
	UTIL_ImpactTrace( &tr, DMG_BULLET );

	// make the marine do a firing anim

	pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	C_BaseAnimating::PopBoneAccess( "ASWUTracerless" );
}

void ASWUTracerDual( C_ASW_Marine *pMarine, const Vector &vecEnd, int nDualType /* = (ASW_TRACER_DUAL_LEFT | ASW_TRACER_DUAL_RIGHT) */, int iAttributeEffects )
{
	MDLCACHE_CRITICAL_SECTION();
	Vector vecStart;
	QAngle vecAngles;

	if ( !pMarine || pMarine->IsDormant() )
		return;

	C_ASW_Weapon *pWpn = pMarine->GetActiveASWWeapon();//dynamic_cast<C_BaseCombatWeapon *>( pEnt );	
	if ( !pWpn || pWpn->IsDormant() )
		return;

	C_BaseAnimating::PushAllowBoneAccess( true, false, "ASWUTracerDual" );

	Vector diff;
	trace_t tr;

	const char *szAttachmentName = "muzzle";
	if ( nDualType & ASW_FX_TRACER_DUAL_LEFT )
	{
		int nLeftMuzzleAttachment = pWpn->LookupAttachment( szAttachmentName );

		// Get the muzzle origin
		if ( !pWpn->GetAttachment( nLeftMuzzleAttachment, vecStart, vecAngles ) )
		{
			C_BaseAnimating::PopBoneAccess( "ASWUTracerDual" );
			return;
		}

		// TODO: this is sort of hacky-right now, if we want more robust support for tracers coming from left or right hand 
		// TODO: we can look at passing attachments into ProcessMuzzleFlashEvent() or syncing more weapon state to C_ASWWeapon
		int nAttachmentPop = pWpn->GetMuzzleAttachment();
		pWpn->SetMuzzleAttachment( nLeftMuzzleAttachment );
		pWpn->ProcessMuzzleFlashEvent();
		pWpn->SetMuzzleAttachment( nAttachmentPop );

		asw_num_u_tracers++;
		if ( asw_use_particle_tracers.GetBool() )
			ASWDoParticleTracer( pWpn, vecStart, vecEnd, pWpn->GetMuzzleFlashRed(), iAttributeEffects );
		else
			FX_ASWTracer( vecStart, vecEnd, 3000, false, pWpn->GetMuzzleFlashRed() );

		// do a trace to the hit surface for impacts
		diff = vecStart - vecEnd;
		diff.NormalizeInPlace();
		diff *= 6;	// go 6 inches away from surfaces
		CTraceFilterSimple traceFilter(pMarine ,COLLISION_GROUP_NONE);
		UTIL_TraceLine(vecEnd + diff, vecEnd - diff, MASK_SHOT, &traceFilter, &tr);
		// do impact effect
		UTIL_ImpactTrace( &tr, DMG_BULLET );
	}

	if ( nDualType & ASW_FX_TRACER_DUAL_RIGHT )
	{
		// do tracer from other gun
		szAttachmentName = "muzzle_flash_l";

		int nRightMuzzleAttachment = pWpn->LookupAttachment( szAttachmentName );

		if( !pWpn->GetAttachment( nRightMuzzleAttachment, vecStart, vecAngles ) )
		{
			C_BaseAnimating::PopBoneAccess( "ASWUTracerDual" );
			return;
		}

		// TODO: this is sort of hacky-right now, if we want more robust support for tracers coming from left or right hand 
		// TODO: we can look at passing attachments into ProcessMuzzleFlashEvent() or syncing more weapon state to C_ASWWeapon
		int nAttachmentPop = pWpn->GetMuzzleAttachment();
		pWpn->SetMuzzleAttachment( nRightMuzzleAttachment );
		pWpn->ProcessMuzzleFlashEvent();
		pWpn->SetMuzzleAttachment( nAttachmentPop );

		asw_num_u_tracers++;
		if ( asw_use_particle_tracers.GetBool() )
			ASWDoParticleTracer( pWpn, vecStart, vecEnd, pWpn->GetMuzzleFlashRed(), iAttributeEffects );
		else
			FX_ASWTracer( vecStart, vecEnd, 3000, false, pWpn->GetMuzzleFlashRed() );

		// do a trace to the hit surface for impacts
		diff = vecStart - vecEnd;
		diff.NormalizeInPlace();
		diff *= 6;	// go 6 inches away from surfaces
		CTraceFilterSimple traceFilter2( pMarine,COLLISION_GROUP_NONE );
		UTIL_TraceLine(vecEnd + diff, vecEnd - diff, MASK_SHOT, &traceFilter2, &tr);
		// do impact effect
		UTIL_ImpactTrace( &tr, DMG_BULLET );
	}

	// make the marine do a firing anim
	pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	C_BaseAnimating::PopBoneAccess( "ASWUTracerDual" );
}

void ASWUTracerUnattached(C_ASW_Marine *pMarine, const Vector &vecStart, const Vector &vecEnd, int iAttributeEffects)
{
	asw_num_u_tracers++;

	if ( !pMarine || pMarine->IsDormant() )
		return;

	C_ASW_Weapon *pWpn = pMarine->GetActiveASWWeapon();
	if ( !pWpn || pWpn->IsDormant() )
		return;

	if ( asw_use_particle_tracers.GetBool() )
		ASWDoParticleTracer( pWpn, vecStart, vecEnd, pWpn->GetMuzzleFlashRed(), iAttributeEffects );
	else
		FX_ASWTracer( vecStart, vecEnd, 3000, false, pWpn->GetMuzzleFlashRed() );

	// do a trace to the hit surface for impacts
	trace_t tr;
	Vector diff = vecStart - vecEnd;
	diff.NormalizeInPlace();
	diff *= 6;	// go 6 inches away from surfaces
	CTraceFilterSimple traceFilter(NULL ,COLLISION_GROUP_NONE);
	UTIL_TraceLine(vecEnd + diff, vecEnd - diff, MASK_SHOT, &traceFilter, &tr);
	// do impact effect
	UTIL_ImpactTrace( &tr, DMG_BULLET );
}

void ASWUTracerRG(C_ASW_Marine *pMarine, const Vector &vecEnd, int iAttributeEffects)
{
	MDLCACHE_CRITICAL_SECTION();
	Vector vecStart;
	QAngle vecAngles;

	if ( !pMarine || pMarine->IsDormant() )
		return;

	C_ASW_Weapon *pWpn = pMarine->GetActiveASWWeapon();//dynamic_cast<C_BaseCombatWeapon *>( pEnt );	
	if ( !pWpn || pWpn->IsDormant() )
		return;

	C_BaseAnimating::PushAllowBoneAccess( true, false, "ASWUTracerRG" );

	pWpn->ProcessMuzzleFlashEvent();

	int nAttachmentIndex = pWpn->LookupAttachment( "muzzle" );
	if ( nAttachmentIndex <= 0 )
		nAttachmentIndex = 1;	// default to the first attachment if it doesn't have a muzzle

	// Get the muzzle origin
	if ( !pWpn->GetAttachment( nAttachmentIndex, vecStart, vecAngles ) )
	{
		return;
	}

	asw_num_u_tracers++;
	if ( asw_use_particle_tracers.GetBool() )
	{
		ASWDoParticleTracer( pWpn, vecStart, vecEnd, pWpn->GetMuzzleFlashRed(), iAttributeEffects );
	}
	else
	{
		FX_ASWTracer( vecStart, vecEnd, 3000, false, pWpn->GetMuzzleFlashRed(), 2 );
		FX_ASW_RGEffect( vecStart, vecEnd );
	}

	//FX_ASW_RGEffect( vecStart, vecEnd );

	// do a trace to the hit surface for impacts
	trace_t tr;
	Vector diff = vecStart - vecEnd;
	diff.NormalizeInPlace();
	diff *= 2;	// go 2 inches away from surfaces
	CTraceFilterSimple traceFilter(pMarine ,COLLISION_GROUP_NONE);
	UTIL_TraceLine(vecEnd + diff, vecEnd - diff, MASK_SHOT, &traceFilter, &tr);
	// do impact effect
	UTIL_ImpactTrace( &tr, DMG_ENERGYBEAM );

	// make the marine do a firing anim

	pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	C_BaseAnimating::PopBoneAccess( "ASWUTracerRG" );
}

void DoAttributeTracer( const Vector &vecStart, const Vector &vecEnd, int iAttributeEffects )
{
	if ( iAttributeEffects <= 0 )
		return;

	/*
	// do attribute tracer between vecStart and vecEnd
	QAngle vecAngles;
	Vector vecToEnd = vecEnd - vecStart;
	VectorNormalize(vecToEnd);
	VectorAngles( vecToEnd, vecAngles );
	
	bool bDefaultCrit = true;

	if ( iAttributeEffects & BULLET_ATT_EXPLODE )
	{
		DispatchParticleEffect( "tracer_explosive", vecStart, vecEnd, vecAngles );
	}

	if ( iAttributeEffects & BULLET_ATT_FIRE_CRIT )
	{
		DispatchParticleEffect( "tracer_ignite", vecStart, vecEnd, vecAngles );
		bDefaultCrit = false;
	}
	if ( iAttributeEffects & BULLET_ATT_CHEMICAL_CRIT )
	{
		DispatchParticleEffect( "tracer_radiation", vecStart, vecEnd, vecAngles );
		bDefaultCrit = false;
	}
	if ( iAttributeEffects & BULLET_ATT_ELECTRIC_CRIT )
	{
		DispatchParticleEffect( "tracer_electricity", vecStart, vecEnd, vecAngles );
		bDefaultCrit = false;
	}
	if ( iAttributeEffects & BULLET_ATT_FREEZE_CRIT )
	{
		DispatchParticleEffect( "tracer_freeze", vecStart, vecEnd, vecAngles );
		bDefaultCrit = false;
	}
	if ( bDefaultCrit && iAttributeEffects & BULLET_ATT_CRITICAL_HIT )
	{
		DispatchParticleEffect( "tracer_critical_hit", vecStart, vecEnd, vecAngles );
	}
	*/
}

void DoAttributeTracer( C_ASW_Marine *pMarine, const Vector &vecEnd, int iAttributeEffects, bool bUnattached = false )
{
	MDLCACHE_CRITICAL_SECTION();
	Vector vecStart;
	QAngle vecAngles;

	if ( iAttributeEffects <= 0 || !pMarine || pMarine->IsDormant() )
		return;

	C_ASW_Weapon *pWpn = pMarine->GetActiveASWWeapon();
	if ( !pWpn || pWpn->IsDormant() )
		return;

	C_BaseAnimating::PushAllowBoneAccess( true, false, "DoAttributeTracer" );

	pWpn->ProcessMuzzleFlashEvent();

	// Get the muzzle origin
	if ( !bUnattached )
	{
		int nAttachmentIndex = pWpn->LookupAttachment( "muzzle" );
		if ( nAttachmentIndex <= 0 )
			nAttachmentIndex = 1;	// default to the first attachment if it doesn't have a muzzle

		// Get the muzzle origin
		if ( !pWpn->GetAttachment( nAttachmentIndex, vecStart, vecAngles ) )
		{
			return;
		}
	}

	DoAttributeTracer( vecStart, vecEnd, iAttributeEffects );

	C_BaseAnimating::PopBoneAccess( "DoAttributeTracer" );
}

// user message version
static int asw_num_tracers = 0;
void __MsgFunc_ASWUTracer( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();

	asw_num_tracers++;

	int iAttributeEffects = msg.ReadShort();
	ASWUTracer( pMarine, vecEnd, iAttributeEffects );

	if ( iAttributeEffects > 0 )
	{
		DoAttributeTracer( pMarine, vecEnd, iAttributeEffects );
	}
}
USER_MESSAGE_REGISTER( ASWUTracer );

void __MsgFunc_ASWUTracerless( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();

	asw_num_tracers++;

	int iAttributeEffects = msg.ReadShort();
	ASWUTracerless( pMarine, vecEnd, iAttributeEffects );	
	
	if ( iAttributeEffects > 0 )
	{
		DoAttributeTracer( pMarine, vecEnd, iAttributeEffects );
	}
}
USER_MESSAGE_REGISTER( ASWUTracerless );

void __MsgFunc_ASWUTracerDual( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();

	asw_num_tracers++;

	int iAttributeEffects = msg.ReadShort();
	ASWUTracerDual( pMarine, vecEnd, (ASW_FX_TRACER_DUAL_LEFT | ASW_FX_TRACER_DUAL_RIGHT), iAttributeEffects );	
	
	if ( iAttributeEffects > 0 )
	{
		DoAttributeTracer( pMarine, vecEnd, iAttributeEffects );
	}
}
USER_MESSAGE_REGISTER( ASWUTracerDual );

void __MsgFunc_ASWUTracerDualLeft( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();

	asw_num_tracers++;

	int iAttributeEffects = msg.ReadShort();
	ASWUTracerDual( pMarine, vecEnd, ASW_FX_TRACER_DUAL_LEFT, iAttributeEffects );	
	
	if ( iAttributeEffects > 0 )
	{
		DoAttributeTracer( pMarine, vecEnd, iAttributeEffects );
	}
}
USER_MESSAGE_REGISTER( ASWUTracerDualLeft );

void __MsgFunc_ASWUTracerDualRight( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();

	asw_num_tracers++;

	int iAttributeEffects = msg.ReadShort();
	ASWUTracerDual( pMarine, vecEnd, ASW_FX_TRACER_DUAL_RIGHT, iAttributeEffects );	
	
	if ( iAttributeEffects > 0 )
	{
		DoAttributeTracer( pMarine, vecEnd, iAttributeEffects );
	}
}
USER_MESSAGE_REGISTER( ASWUTracerDualRight );

void __MsgFunc_ASWUTracerRG( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();

	asw_num_tracers++;

	int iAttributeEffects = msg.ReadShort();
	ASWUTracerRG( pMarine, vecEnd, iAttributeEffects );
	
	if ( iAttributeEffects > 0 )
	{
		DoAttributeTracer( pMarine, vecEnd, iAttributeEffects );
	}
}
USER_MESSAGE_REGISTER( ASWUTracerRG );

void __MsgFunc_ASWUTracerUnattached( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine

	Vector vecEnd;
	vecEnd.x = msg.ReadFloat();
	vecEnd.y = msg.ReadFloat();
	vecEnd.z = msg.ReadFloat();
	Vector vecStart;
	vecStart.x = msg.ReadFloat();
	vecStart.y = msg.ReadFloat();
	vecStart.z = msg.ReadFloat();

	asw_num_tracers++;

	int iAttributeEffects = msg.ReadShort();
	ASWUTracerUnattached( pMarine, vecStart, vecEnd, iAttributeEffects );
	
	if ( iAttributeEffects > 0 )
	{
		DoAttributeTracer( vecStart, vecEnd, iAttributeEffects );
	}
}
USER_MESSAGE_REGISTER( ASWUTracerUnattached );


void ASWUTracerCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	
	if ( player == NULL )
		return;

	// Do tracer effect
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(data.GetEntity());
	ASWUTracer( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
	DoAttributeTracer( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
}
DECLARE_CLIENT_EFFECT( ASWUTracer, ASWUTracerCallback );

void ASWUTracerRGCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	
	if ( player == NULL )
		return;

	// Do tracer effect
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(data.GetEntity());
	ASWUTracerRG( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
	DoAttributeTracer( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
}
DECLARE_CLIENT_EFFECT( ASWUTracerRG, ASWUTracerRGCallback );

void ASWUTracerlessCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	
	if ( player == NULL )
		return;

	// Do tracer effect
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(data.GetEntity());
	ASWUTracerless( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
	DoAttributeTracer( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
}
DECLARE_CLIENT_EFFECT( ASWUTracerless, ASWUTracerlessCallback );

void ASWUTracerDualCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( player == NULL )
		return;

	// Do tracer effect
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(data.GetEntity());
	ASWUTracerDual( pMarine, (Vector&)data.m_vOrigin, (ASW_FX_TRACER_DUAL_LEFT | ASW_FX_TRACER_DUAL_RIGHT), data.m_nMaterial );
	DoAttributeTracer( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
}
DECLARE_CLIENT_EFFECT( ASWUTracerDual, ASWUTracerDualCallback );

void ASWUTracerDualCallbackRight( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( player == NULL )
		return;

	// Do tracer effect
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(data.GetEntity());
	ASWUTracerDual( pMarine, (Vector&)data.m_vOrigin, ASW_FX_TRACER_DUAL_RIGHT, data.m_nMaterial );
	DoAttributeTracer( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
}
DECLARE_CLIENT_EFFECT( ASWUTracerDualRight, ASWUTracerDualCallbackRight );

void ASWUTracerDualCallbackLeft( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

	if ( player == NULL )
		return;

	// Do tracer effect
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(data.GetEntity());
	ASWUTracerDual( pMarine, (Vector&)data.m_vOrigin, ASW_FX_TRACER_DUAL_LEFT, data.m_nMaterial );
	DoAttributeTracer( pMarine, (Vector&)data.m_vOrigin, data.m_nMaterial );
}
DECLARE_CLIENT_EFFECT( ASWUTracerDualLeft, ASWUTracerDualCallbackLeft );

void ASWUTracerUnattachedCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	
	if ( player == NULL )
		return;

	// Do tracer effect
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(data.GetEntity());
	ASWUTracerUnattached( pMarine, (Vector&)data.m_vStart, (Vector&)data.m_vOrigin, data.m_nMaterial );
	DoAttributeTracer( (Vector&)data.m_vStart, (Vector&)data.m_vOrigin, data.m_nMaterial );
}
DECLARE_CLIENT_EFFECT( ASWUTracerUnattached, ASWUTracerUnattachedCallback );


void asw_tracer_count_f()
{
	Msg("spawned %d tracers from user messages. %d tracers total \n", asw_num_tracers, asw_num_u_tracers);
}
static ConCommand asw_tracer_count("asw_tracer_count", asw_tracer_count_f, "Shows number of tracers spawned", FCVAR_CHEAT);

void FX_ASWWaterRipple( const Vector &origin, float scale, Vector *pColor, float flLifetime, float flAlpha )
{
	VPROF_BUDGET( "FX_ASWWaterRipple", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	trace_t	tr;

	Vector	color = pColor ? *pColor : Vector( 0.8f, 0.8f, 0.75f );	

	Vector vecNormal = Vector(0,0,1);

	{
		//Add a ripple quad to the surface
		FX_AddQuad( origin + ( vecNormal * 0.5f ), 
					vecNormal, 
					64.0f*scale, 
					128.0f*scale, 
					0.7f,
					flAlpha,	// start alpha
					0.0f,		// end alpha
					0.25f,
					random->RandomFloat( 0, 360 ),
					random->RandomFloat( -16.0f, 16.0f ),
					color, 
					flLifetime, 
					"effects/splashwake1", 
					(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );
	}
}

extern ConVar cl_show_splashes;
extern inline void FX_GetSplashLighting( Vector position, Vector *color, float *luminosity );
#define	SPLASH_MIN_SPEED	50.0f
#define	SPLASH_MAX_SPEED	100.0f
void FX_ASWSplash( const Vector &origin, const Vector &normal, float scale )
{
	VPROF_BUDGET( "FX_ASWSplash", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	if ( cl_show_splashes.GetBool() == false )
		return;

	Vector	color;
	float	luminosity;
	
	// Get our lighting information
	FX_GetSplashLighting( origin + ( normal * scale ), &color, &luminosity );

	float flScale = scale / 8.0f;

	if ( flScale > 4.0f )
	{
		flScale = 4.0f;
	}

	// Setup our trail emitter
	CSmartPtr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "splash" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( origin );
	sparkEmitter->m_ParticleCollision.SetGravity( 800.0f );
	sparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
	sparkEmitter->SetVelocityDampen( 2.0f );
	sparkEmitter->GetBinding().SetBBox( origin - Vector( 32, 32, 32 ), origin + Vector( 32, 32, 32 ) );

	PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/splash2" );

	TrailParticle	*tParticle;

	Vector	offDir;
	Vector	offset;
	float	colorRamp;

	//Dump out drops
	for ( int i = 0; i < 16; i++ )
	{
		offset = origin;
		offset[0] += random->RandomFloat( -8.0f, 8.0f ) * flScale;
		offset[1] += random->RandomFloat( -8.0f, 8.0f ) * flScale;

		tParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( tParticle == NULL )
			break;

		tParticle->m_flLifetime	= 0.0f;
		tParticle->m_flDieTime	= random->RandomFloat( 0.25f, 0.5f );

		offDir = normal + RandomVector( -0.8f, 0.8f );

		tParticle->m_vecVelocity = offDir * random->RandomFloat( SPLASH_MIN_SPEED * flScale * 3.0f, SPLASH_MAX_SPEED * flScale * 3.0f );
		//tParticle->m_vecVelocity[2] += random->RandomFloat( 32.0f, 64.0f ) * flScale;
		tParticle->m_vecVelocity[2] += random->RandomFloat( 8.0f, 16.0f ) * flScale;

		tParticle->m_flWidth		= random->RandomFloat( 1.0f, 3.0f );
		tParticle->m_flLength		= random->RandomFloat( 0.025f, 0.05f );

		colorRamp = random->RandomFloat( 0.75f, 1.25f );

		tParticle->m_color.r = MIN( 1.0f, color[0] * colorRamp ) * 255;
		tParticle->m_color.g = MIN( 1.0f, color[1] * colorRamp ) * 255;
		tParticle->m_color.b = MIN( 1.0f, color[2] * colorRamp ) * 255;
		tParticle->m_color.a = luminosity * 255;
	}

	// Setup the particle emitter
	CSmartPtr<CSplashParticle> pSimple = CSplashParticle::Create( "splish" );
	pSimple->SetSortOrigin( origin );
	pSimple->SetClipHeight( origin.z );
	pSimple->SetParticleCullRadius( scale * 2.0f );
	pSimple->GetBinding().SetBBox( origin - Vector( 32, 32, 32 ), origin + Vector( 32, 32, 32 ) );

	SimpleParticle	*pParticle;

	//Main gout
	for ( int i = 0; i < 8; i++ )
	{
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );

		if ( pParticle == NULL )
			break;

		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime	= 2.0f;	//NOTENOTE: We use a clip plane to realistically control our lifespan

		pParticle->m_vecVelocity.Random( -0.2f, 0.2f );
		//pParticle->m_vecVelocity += ( normal * random->RandomFloat( 4.0f, 6.0f ) );
		pParticle->m_vecVelocity += ( normal * random->RandomFloat( 1.0f, 2.0f ) );
		
		VectorNormalize( pParticle->m_vecVelocity );

		pParticle->m_vecVelocity *= 50 * flScale * (8-i);
		
		colorRamp = random->RandomFloat( 0.75f, 1.25f );

		pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
		
		pParticle->m_uchStartSize	= 24 * flScale * RemapValClamped( i, 7, 0, 1, 0.5f );
		pParticle->m_uchEndSize		= MIN( 255, pParticle->m_uchStartSize * 2 );
		
		pParticle->m_uchStartAlpha	= RemapValClamped( i, 7, 0, 255, 32 ) * luminosity;
		pParticle->m_uchEndAlpha	= 0;
		
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
	}
}

void ASWSplashCallback( const CEffectData &data )
{
	Vector	normal;
	AngleVectors( data.m_vAngles, &normal );
	FX_ASWSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );	
}

DECLARE_CLIENT_EFFECT( aswwatersplash, ASWSplashCallback );

void FX_ASW_StunExplosion(const Vector &origin)
{
	DispatchParticleEffect( "stungrenade_core", origin, QAngle( 0, 0, 0 ) );
}

void ASWStunExplosionCallback( const CEffectData &data )
{
	FX_ASW_StunExplosion( data.m_vOrigin );	
}

DECLARE_CLIENT_EFFECT( aswstunexplo, ASWStunExplosionCallback );

void ASWExplodeMapCallback( const CEffectData &data )
{
	C_ASW_Level_Exploder::CreateClientsideLevelExploder();	
}

DECLARE_CLIENT_EFFECT( ASWExplodeMap, ASWExplodeMapCallback );

void ASW_AcidBurnCallback( const CEffectData & data )
{
	int iMarine = data.m_nOtherEntIndex;		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
	if ( !pMarine )
		return;

	Vector vecSourcePos;
	vecSourcePos = data.m_vOrigin;

	//Vector vecMarineOffset = pMarine->GetAbsOrigin() + Vector( 0, 0, 60 );
	Vector	vecKillDir = pMarine->GetAbsOrigin() - vecSourcePos;
	VectorNormalize( vecKillDir );

	CUtlReference<CNewParticleEffect> pEffect;
	pEffect = pMarine->ParticleProp()->Create( "acid_touch", PATTACH_ABSORIGIN_FOLLOW, -1, (-vecKillDir * 16) + Vector( 0, 0, 60 ) );
	pMarine->ParticleProp()->AddControlPoint( pEffect, 1, pMarine, PATTACH_CUSTOMORIGIN );
	pEffect->SetControlPoint( 1, vecSourcePos );

	/*
	unsigned char color[3];
	color[0] = 128;
	color[1] = 30;
	color[2] = 30;
	
	FX_Smoke( vecPosition, QAngle(-90,0,0), 2.0f, 5, &color[0], 128 );
	*/
}

DECLARE_CLIENT_EFFECT( ASWAcidBurn, ASW_AcidBurnCallback );

void ASW_FireBurstCallback( const CEffectData & data )
{
	DispatchParticleEffect( "vindicator_grenade", data.m_vOrigin, QAngle(0,0,0) );
}

DECLARE_CLIENT_EFFECT( ASWFireBurst, ASW_FireBurstCallback );


void FX_ASW_ShotgunSmoke(const Vector& vecOrigin, const QAngle& angFacing)
{
	C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( NULL, false ))
		{
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "shotgunsmoke");
			pEmitter->m_fScale = 1.0f;
			pEmitter->m_bEmit = true;
			pEmitter->SetAbsOrigin(vecOrigin);
			pEmitter->SetAbsAngles(angFacing);
			pEmitter->CreateEmitter();
			pEmitter->SetDieTime(gpGlobals->curtime + 2.0f);
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}
}

/*
void FX_ASW_CivvyCorpse(const Vector &origin, const QAngle& direction, const Vector& force)
{	
	CBaseEntity *pGib = CreateRagGib( "models/swarm/civilianz/civilianz.mdl", origin, direction, force );
}

void ASWCivvyCorpseCallback( const CEffectData &data )
{
	FX_ASW_CivvyCorpse( data.m_vOrigin, data.m_vAngles, data.m_vNormal );	
}

DECLARE_CLIENT_EFFECT( "aswcorpse", ASWCivvyCorpseCallback );
*/
/*
C_BaseAnimating* FX_ASW_Clientside_Ragdoll(const Vector &origin, const QAngle &facing)
{
	C_BaseAnimating *pRagdoll = this;
	
	C_ClientRagdoll *pRagdollCopy = new C_ClientRagdoll( false );
	if ( pRagdollCopy == NULL )
			return NULL;

	C_BaseAnimating *pRagdoll = pRagdollCopy;
		
	const char *pModelName = "models/swarm/civilianz/civilianz.mdl";

	if ( pRagdoll->InitializeAsClientEntity( pModelName, false ) == false )
	{
		pRagdoll->Release();
		return NULL;
	}

	// We need to take these from the entity
	pRagdoll->SetAbsOrigin( origin );
	pRagdoll->SetAbsAngles( facing );
									
	pRagdoll->m_nRenderFX = kRenderFxRagdoll;
	//pRagdoll->SetRenderMode( GetRenderMode() );
	//pRagdoll->SetRenderColor( GetRenderColor().r, GetRenderColor().g, GetRenderColor().b, GetRenderColor().a );

	//pRagdoll->m_nBody = 0;
	//pRagdoll->m_nSkin = 0;
	//pRagdoll->m_vecForce = Vector(0, 0, 0);
	//pRagdoll->m_nForceBone = 0;
	pRagdoll->SetNextClientThink( CLIENT_THINK_ALWAYS );
	
	pRagdoll->SetModelName( AllocPooledString(pModelName) );	
	
	pRagdoll->m_builtRagdoll = true;

	// Store off our old mins & maxs
	//pRagdoll->m_vecPreRagdollMins = WorldAlignMins();
	//pRagdoll->m_vecPreRagdollMaxs = WorldAlignMaxs();

	matrix3x4_t preBones[MAXSTUDIOBONES];
	matrix3x4_t curBones[MAXSTUDIOBONES];

	// Force MOVETYPE_STEP interpolation
	//MoveType_t savedMovetype = GetMoveType();
	//SetMoveType( MOVETYPE_STEP );

	// HACKHACK: force time to last interpolation position
	//m_flPlaybackRate = 1;
	
	GetRagdollPreSequence( preBones, prevanimtime );
	GetRagdollCurSequence( curBones, curanimtime );

	pRagdoll->m_pRagdoll = CreateRagdoll( 
		pRagdoll, 
		hdr, 
		m_vecForce, 
		m_nForceBone, 
		CBoneAccessor( preBones ), 
		CBoneAccessor( curBones ), 
		m_BoneAccessor,
		curanimtime - prevanimtime );

	// Cause the entity to recompute its shadow	type and make a
	// version which only updates when physics state changes
	// NOTE: We have to do this after m_pRagdoll is assigned above
	// because that's what ShadowCastType uses to figure out which type of shadow to use.
	pRagdoll->DestroyShadow();
	pRagdoll->CreateShadow();

	// Cache off ragdoll bone positions/quaternions
	//if ( pRagdoll->m_bStoreRagdollInfo && pRagdoll->m_pRagdoll )
	//{
		//matrix3x4_t parentTransform;
		//AngleMatrix( GetAbsAngles(), GetAbsOrigin(), parentTransform );
		// FIXME/CHECK:  This might be too expensive to do every frame???
		//SaveRagdollInfo( hdr->numbones(), parentTransform, pRagdoll->m_BoneAccessor );
	//}
	
		
	//pRagdoll->m_nRestoreSequence = GetSequence();
    //pRagdoll->SetSequence( SelectWeightedSequence( ACT_DIERAGDOLL ) );
	//pRagdoll->m_nPrevSequence = GetSequence();
	pRagdoll->m_flPlaybackRate = 0;
	pRagdoll->UpdatePartitionListEntry();

	//NoteRagdollCreationTick( pRagdoll );

	//UpdateVisibility();

	return pRagdoll;
}*/

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : scale - 
//			attachmentIndex - 
//			bOneFrame - 
//-----------------------------------------------------------------------------
void FX_ASW_MuzzleEffectAttached( 
							 float scale, 
							 ClientEntityHandle_t hEntity, 
							 int attachmentIndex, 
							 unsigned char *pFlashColor,
							 bool bOneFrame )
{
	VPROF_BUDGET( "FX_ASW_MuzzleEffectAttached", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	CSmartPtr<CLocalSpaceEmitter> pSimple = CLocalSpaceEmitter::Create( "MuzzleFlash", hEntity, attachmentIndex );

	SimpleParticle *pParticle;
	Vector			forward(1,0,0), offset;
	Vector movement;

	float flScale = random->RandomFloat( scale-0.25f, scale+0.25f );

	if ( flScale < 0.5f )
	{
		flScale = 0.5f;
	}
	else if ( flScale > 8.0f )
	{
		flScale = 8.0f;
	}

	//
	// Flash
	//

	int i;
	for ( i = 1; i < 9; i++ )
	{
		offset = (forward * (i*2.0f*scale));

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( VarArgs( "effects/muzzleflash%d", random->RandomInt(1,4) ) ), offset );

		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= bOneFrame ? 0.0001f : 0.1f;

		pParticle->m_vecVelocity.Init();
		pParticle->m_vecVelocity = movement;

		if ( !pFlashColor )
		{
			pParticle->m_uchColor[0]	= 255;
			pParticle->m_uchColor[1]	= 255;
			pParticle->m_uchColor[2]	= 255;
		}
		else
		{
			pParticle->m_uchColor[0]	= pFlashColor[0];
			pParticle->m_uchColor[1]	= pFlashColor[1];
			pParticle->m_uchColor[2]	= pFlashColor[2];
		}

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 128;

		pParticle->m_uchStartSize	= (random->RandomFloat( 6.0f, 9.0f ) * (12-(i))/9) * flScale;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		// asw test
		pParticle->m_uchStartSize		= 0;
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}


	if ( !ToolsEnabled() )
		return;

	if ( !clienttools->IsInRecordingMode() )
		return;

	C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( hEntity );
	if ( pEnt )
	{
		pEnt->RecordToolMessage();
	}

	// NOTE: Particle system destruction message will be sent by the particle effect itself.
	int nId = pSimple->AllocateToolParticleEffectId();

	KeyValues *msg = new KeyValues( "ParticleSystem_Create" );
	msg->SetString( "name", "FX_MuzzleEffectAttached" );
	msg->SetInt( "id", nId );
	msg->SetFloat( "time", gpGlobals->curtime );

	KeyValues *pEmitter = msg->FindKey( "DmeSpriteEmitter", true );
	pEmitter->SetInt( "count", 9 );
	pEmitter->SetFloat( "duration", 0 );
	pEmitter->SetString( "material", "effects/muzzleflash2" ); // FIXME - create DmeMultiMaterialSpriteEmitter to support the 4 materials of muzzleflash
	pEmitter->SetInt( "active", true );

	KeyValues *pInitializers = pEmitter->FindKey( "initializers", true );

	KeyValues *pPosition = pInitializers->FindKey( "DmeLinearAttachedPositionInitializer", true );
	pPosition->SetPtr( "entindex", (void*)pEnt->entindex() );
	pPosition->SetInt( "attachmentIndex", attachmentIndex );
	pPosition->SetFloat( "linearOffsetX", 2.0f * scale );

	// TODO - create a DmeConstantLifetimeInitializer
	KeyValues *pLifetime = pInitializers->FindKey( "DmeRandomLifetimeInitializer", true );
	pLifetime->SetFloat( "minLifetime", bOneFrame ? 1.0f / 24.0f : 0.1f );
	pLifetime->SetFloat( "maxLifetime", bOneFrame ? 1.0f / 24.0f : 0.1f );

	KeyValues *pVelocity = pInitializers->FindKey( "DmeConstantVelocityInitializer", true );
	pVelocity->SetFloat( "velocityX", 0.0f );
	pVelocity->SetFloat( "velocityY", 0.0f );
	pVelocity->SetFloat( "velocityZ", 0.0f );

	KeyValues *pRoll = pInitializers->FindKey( "DmeRandomRollInitializer", true );
	pRoll->SetFloat( "minRoll", 0.0f );
	pRoll->SetFloat( "maxRoll", 360.0f );

	// TODO - create a DmeConstantRollSpeedInitializer
	KeyValues *pRollSpeed = pInitializers->FindKey( "DmeRandomRollSpeedInitializer", true );
	pRollSpeed->SetFloat( "minRollSpeed", 0.0f );
	pRollSpeed->SetFloat( "maxRollSpeed", 0.0f );

	// TODO - create a DmeConstantColorInitializer
	KeyValues *pColor = pInitializers->FindKey( "DmeRandomInterpolatedColorInitializer", true );
	Color color( pFlashColor ? pFlashColor[ 0 ] : 255, pFlashColor ? pFlashColor[ 1 ] : 255, pFlashColor ? pFlashColor[ 2 ] : 255, 255 );
	pColor->SetColor( "color1", color );
	pColor->SetColor( "color2", color );

	// TODO - create a DmeConstantAlphaInitializer
	KeyValues *pAlpha = pInitializers->FindKey( "DmeRandomAlphaInitializer", true );
	pAlpha->SetInt( "minStartAlpha", 255 );
	pAlpha->SetInt( "maxStartAlpha", 255 );
	pAlpha->SetInt( "minEndAlpha", 128 );
	pAlpha->SetInt( "maxEndAlpha", 128 );

	// size = rand(6..9) * indexed(12/9..4/9) * flScale = rand(6..9) * ( 4f + f * i )
	KeyValues *pSize = pInitializers->FindKey( "DmeMuzzleFlashSizeInitializer", true );
	float f = flScale / 9.0f;
	pSize->SetFloat( "indexedBase", 4.0f * f );
	pSize->SetFloat( "indexedDelta", f );
	pSize->SetFloat( "minRandomFactor", 6.0f );
	pSize->SetFloat( "maxRandomFactor", 9.0f );

	/*
	KeyValues *pUpdaters = pEmitter->FindKey( "updaters", true );

	pUpdaters->FindKey( "DmePositionVelocityUpdater", true );
	pUpdaters->FindKey( "DmeRollUpdater", true );
	pUpdaters->FindKey( "DmeAlphaLinearUpdater", true );
	pUpdaters->FindKey( "DmeSizeUpdater", true );
	*/
	ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
	msg->deleteThis();
}

void FX_ASW_RedMuzzleEffectAttached( 
	float scale, 
	ClientEntityHandle_t hEntity, 
	int attachmentIndex, 
	unsigned char *pFlashColor,
	bool bOneFrame )
{
	VPROF_BUDGET( "FX_ASW_RedMuzzleEffectAttached", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	CSmartPtr<CLocalSpaceEmitter> pSimple = CLocalSpaceEmitter::Create( "MuzzleFlash", hEntity, attachmentIndex );
	
	SimpleParticle *pParticle;
	Vector			forward(1,0,0), offset;
	// asw
	Vector movement;

	float flScale = random->RandomFloat( scale-0.25f, scale+0.25f );

	if ( flScale < 0.5f )
	{
		flScale = 0.5f;
	}
	else if ( flScale > 8.0f )
	{
		flScale = 8.0f;
	}

	//
	// Flash
	//

	int i;
	for ( i = 1; i < 9; i++ )
	{
#ifndef INFESTED_DLL
		offset = (forward * (i*2.0f*scale));
#else
		offset = vec3_origin;
		movement = (forward * (i*2.0f*scale)) / 0.1f;
#endif

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( VarArgs("swarm/effects/muzzleflashred%d", random->RandomInt(1,4)) ), offset );
			
		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= bOneFrame ? 0.0001f : 0.1f;

		pParticle->m_vecVelocity.Init();
		// asw test
		pParticle->m_vecVelocity = movement;

		if ( !pFlashColor )
		{
			pParticle->m_uchColor[0]	= 255;
			pParticle->m_uchColor[1]	= 255;
			pParticle->m_uchColor[2]	= 255;
		}
		else
		{
			pParticle->m_uchColor[0]	= pFlashColor[0];
			pParticle->m_uchColor[1]	= pFlashColor[1];
			pParticle->m_uchColor[2]	= pFlashColor[2];
		}

		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 128;

		pParticle->m_uchStartSize	= (random->RandomFloat( 6.0f, 9.0f ) * (12-(i))/9) * flScale;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
		// asw test
		pParticle->m_uchStartSize		= 0;
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= 0.0f;
	}


	if ( !ToolsEnabled() )
		return;

	if ( !clienttools->IsInRecordingMode() )
		return;

	C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( hEntity );
	if ( pEnt )
	{
		pEnt->RecordToolMessage();
	}

	// NOTE: Particle system destruction message will be sent by the particle effect itself.
	int nId = pSimple->AllocateToolParticleEffectId();

	KeyValues *msg = new KeyValues( "ParticleSystem_Create" );
	msg->SetString( "name", "FX_MuzzleEffectAttached" );
	msg->SetInt( "id", nId );
	msg->SetFloat( "time", gpGlobals->curtime );

	KeyValues *pEmitter = msg->FindKey( "DmeSpriteEmitter", true );
	pEmitter->SetInt( "count", 9 );
	pEmitter->SetFloat( "duration", 0 );
	pEmitter->SetString( "material", "effects/muzzleflash2" ); // FIXME - create DmeMultiMaterialSpriteEmitter to support the 4 materials of muzzleflash
	pEmitter->SetInt( "active", true );

	KeyValues *pInitializers = pEmitter->FindKey( "initializers", true );

	KeyValues *pPosition = pInitializers->FindKey( "DmeLinearAttachedPositionInitializer", true );
	pPosition->SetPtr( "entindex", (void*)pEnt->entindex() );
	pPosition->SetInt( "attachmentIndex", attachmentIndex );
	pPosition->SetFloat( "linearOffsetX", 2.0f * scale );

	// TODO - create a DmeConstantLifetimeInitializer
	KeyValues *pLifetime = pInitializers->FindKey( "DmeRandomLifetimeInitializer", true );
	pLifetime->SetFloat( "minLifetime", bOneFrame ? 1.0f / 24.0f : 0.1f );
	pLifetime->SetFloat( "maxLifetime", bOneFrame ? 1.0f / 24.0f : 0.1f );

	KeyValues *pVelocity = pInitializers->FindKey( "DmeConstantVelocityInitializer", true );
	pVelocity->SetFloat( "velocityX", 0.0f );
	pVelocity->SetFloat( "velocityY", 0.0f );
	pVelocity->SetFloat( "velocityZ", 0.0f );

	KeyValues *pRoll = pInitializers->FindKey( "DmeRandomRollInitializer", true );
	pRoll->SetFloat( "minRoll", 0.0f );
	pRoll->SetFloat( "maxRoll", 360.0f );

	// TODO - create a DmeConstantRollSpeedInitializer
	KeyValues *pRollSpeed = pInitializers->FindKey( "DmeRandomRollSpeedInitializer", true );
	pRollSpeed->SetFloat( "minRollSpeed", 0.0f );
	pRollSpeed->SetFloat( "maxRollSpeed", 0.0f );

	// TODO - create a DmeConstantColorInitializer
	KeyValues *pColor = pInitializers->FindKey( "DmeRandomInterpolatedColorInitializer", true );
	Color color( pFlashColor ? pFlashColor[ 0 ] : 255, pFlashColor ? pFlashColor[ 1 ] : 255, pFlashColor ? pFlashColor[ 2 ] : 255, 255 );
	pColor->SetColor( "color1", color );
	pColor->SetColor( "color2", color );

	// TODO - create a DmeConstantAlphaInitializer
	KeyValues *pAlpha = pInitializers->FindKey( "DmeRandomAlphaInitializer", true );
	pAlpha->SetInt( "minStartAlpha", 255 );
	pAlpha->SetInt( "maxStartAlpha", 255 );
	pAlpha->SetInt( "minEndAlpha", 128 );
	pAlpha->SetInt( "maxEndAlpha", 128 );

	// size = rand(6..9) * indexed(12/9..4/9) * flScale = rand(6..9) * ( 4f + f * i )
	KeyValues *pSize = pInitializers->FindKey( "DmeMuzzleFlashSizeInitializer", true );
	float f = flScale / 9.0f;
	pSize->SetFloat( "indexedBase", 4.0f * f );
	pSize->SetFloat( "indexedDelta", f );
	pSize->SetFloat( "minRandomFactor", 6.0f );
	pSize->SetFloat( "maxRandomFactor", 9.0f );

/*
	KeyValues *pUpdaters = pEmitter->FindKey( "updaters", true );

	pUpdaters->FindKey( "DmePositionVelocityUpdater", true );
	pUpdaters->FindKey( "DmeRollUpdater", true );
	pUpdaters->FindKey( "DmeAlphaLinearUpdater", true );
	pUpdaters->FindKey( "DmeSizeUpdater", true );
*/
	ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
	msg->deleteThis();
}

void FX_QueenDie(C_BaseAnimating *pQueen)
{
	if (!pQueen)
		return;

	C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
	if (pEmitter)
	{
		if (pEmitter->InitializeAsClientEntity( NULL, false ))
		{
			Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "queendie");
			pEmitter->m_fScale = 2.0f;
			pEmitter->m_bEmit = true;
			pEmitter->CreateEmitter();
			pEmitter->SetAbsOrigin(pQueen->WorldSpaceCenter());			
			pEmitter->SetAbsAngles(QAngle(0,0,0));
			pEmitter->SetDieTime(gpGlobals->curtime + 4.0f);
			pEmitter->ClientAttach(pQueen, "SpitSource");
		}
		else
		{
			UTIL_Remove( pEmitter );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: create a muzzle flassh using the new particle system
//-----------------------------------------------------------------------------
void FX_ASW_ParticleMuzzleFlashAttached( float scale, ClientEntityHandle_t hEntity, int attachmentIndex, bool bIsRed )
{
	VPROF_BUDGET( "FX_ASW_ParticleMuzzleFlashAttached", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	C_ASW_Weapon *pWeapon = dynamic_cast<C_ASW_Weapon*>( hEntity.Get() );
	if ( !pWeapon )
		return;

	if ( !asw_muzzle_flash_new_type.GetBool() )
	{
		CUtlReference<CNewParticleEffect> pMuzzle = pWeapon->ParticleProp()->Create( pWeapon->GetMuzzleEffectName(), PATTACH_POINT_FOLLOW, attachmentIndex );
		if ( !pMuzzle || !pMuzzle->IsValid() )
			return;

		Vector vecStart;
		QAngle vecAngles;
		// Get the muzzle origin
		if ( !pWeapon->GetAttachment( attachmentIndex, vecStart, vecAngles ) )
			return;

		// scale
		pMuzzle->SetControlPoint( 10, Vector( scale, 0, 0 ) );

		// color
		Vector vecColor = Vector( 1, 1, 1 );
		if ( bIsRed )
			vecColor = Vector( 1, 0.55, 0.55 );
		pMuzzle->SetControlPoint( 20, vecColor );
	}
	else
	{
		pWeapon->m_fMuzzleFlashTime = gpGlobals->curtime + 0.1f;
	}
}

void QueenDieCallback( const CEffectData &data )
{
	C_BaseAnimating *pAnimating = dynamic_cast<C_BaseAnimating*>(data.GetEntity());
	if (pAnimating)
		FX_QueenDie( pAnimating );
}

DECLARE_CLIENT_EFFECT( QueenDie, QueenDieCallback );

void __MsgFunc_ASWEnvExplosionFX( bf_read &msg )
{
	Vector vecPos;
	vecPos.x = msg.ReadFloat();
	vecPos.y = msg.ReadFloat();
	vecPos.z = msg.ReadFloat();
	float flRadius = msg.ReadFloat();
	int iOnGround = msg.ReadOneBit() ? 1 : 0;

	if ( iOnGround )
	{
		trace_t tr;
		CTraceFilterWorldOnly traceFilter;
		UTIL_TraceLine( vecPos, vecPos + Vector( 0, 0, -64), MASK_SOLID, &traceFilter, &tr );
		if ( tr.fraction != 1.0f )
		{
			surfacedata_t *pSurface = physprops->GetSurfaceData( tr.surface.surfaceProps );
			if ( pSurface )
			{
				if ( pSurface->game.material == CHAR_TEX_SNOW )
					DispatchParticleEffect( "snow_explosion", tr.endpos, Vector( flRadius, 0, 0 ), QAngle( 0, 0, 0 ) );
			}
		}
	}

	CUtlReference<CNewParticleEffect> pEffect = CNewParticleEffect::Create( NULL, "asw_env_explosion" );
	if ( pEffect->IsValid() )
	{
		pEffect->SetSortOrigin( vecPos );
		pEffect->SetControlPoint( 0, vecPos );
		pEffect->SetControlPoint( 1, Vector( flRadius, iOnGround, 0 ) );
		// this is the ring on the ground
		pEffect->SetControlPoint( 2, Vector( flRadius, flRadius, flRadius ) );

		//Vector vecForward, vecRight, vecUp;
		//AngleVectors( data.m_vAngles, &vecForward, &vecRight, &vecUp );
		//pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
	}
}
USER_MESSAGE_REGISTER( ASWEnvExplosionFX );

void __MsgFunc_ASWBuzzerDeath( bf_read &msg )
{
	Vector vecPos;
	vecPos.x = msg.ReadFloat();
	vecPos.y = msg.ReadFloat();
	vecPos.z = msg.ReadFloat();

	DispatchParticleEffect( "buzzer_death", vecPos, Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ) );
}
USER_MESSAGE_REGISTER( ASWBuzzerDeath );

void __MsgFunc_ASWGrenadeExplosion( bf_read &msg )
{
	Vector vecPos;
	vecPos.x = msg.ReadFloat();
	vecPos.y = msg.ReadFloat();
	vecPos.z = msg.ReadFloat();
	float flRadius = msg.ReadFloat();

	trace_t tr;
	CTraceFilterWorldOnly traceFilter;
	UTIL_TraceLine( vecPos, vecPos + Vector( 0, 0, -64), MASK_SOLID, &traceFilter, &tr );
	if ( tr.fraction != 1.0f )
	{
		surfacedata_t *pSurface = physprops->GetSurfaceData( tr.surface.surfaceProps );
		if ( pSurface )
		{
			if ( pSurface->game.material == CHAR_TEX_SNOW )
				DispatchParticleEffect( "snow_explosion", tr.endpos, Vector( flRadius, 0, 0 ), QAngle( 0, 0, 0 ) );
		}
	}

	DispatchParticleEffect( "explosion_grenade", vecPos, Vector( flRadius, 0, 0 ), QAngle( 0, 0, 0 ) );
}
USER_MESSAGE_REGISTER( ASWGrenadeExplosion );

void __MsgFunc_ASWBarrelExplosion( bf_read &msg )
{
	Vector vecPos;
	vecPos.x = msg.ReadFloat();
	vecPos.y = msg.ReadFloat();
	vecPos.z = msg.ReadFloat();
	float flRadius = msg.ReadFloat();

	trace_t tr;
	CTraceFilterWorldOnly traceFilter;
	UTIL_TraceLine( vecPos, vecPos + Vector( 0, 0, -64), MASK_SOLID, &traceFilter, &tr );
	if ( tr.fraction != 1.0f )
	{
		surfacedata_t *pSurface = physprops->GetSurfaceData( tr.surface.surfaceProps );
		if ( pSurface )
		{
			if ( pSurface->game.material == CHAR_TEX_SNOW )
				DispatchParticleEffect( "snow_explosion", tr.endpos, Vector( flRadius, 0, 0 ), QAngle( 0, 0, 0 ) );
		}
	}

	DispatchParticleEffect( "explosion_barrel", vecPos, Vector( flRadius, 0, 0 ), QAngle( 0, 0, 0 ) );
}
USER_MESSAGE_REGISTER( ASWBarrelExplosion );

void __MsgFunc_ASWMarineHitByMelee( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
	if ( !pMarine )
		return;

	Vector attachOrigin;
	attachOrigin.x = msg.ReadFloat();
	attachOrigin.y = msg.ReadFloat();
	attachOrigin.z = msg.ReadFloat();

	Vector vecDir = attachOrigin - pMarine->GetAbsOrigin();
	VectorNormalize(vecDir);

	UTIL_ASW_MarineTakeDamage( (pMarine->WorldSpaceCenter() + Vector(0,0,24)) + vecDir*8, vecDir, pMarine->BloodColor(), 5, pMarine );
}
USER_MESSAGE_REGISTER( ASWMarineHitByMelee );

void __MsgFunc_ASWMarineHitByFF( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
	if ( !pMarine )
		return;

	Vector attachOrigin;
	attachOrigin.x = msg.ReadFloat();
	attachOrigin.y = msg.ReadFloat();
	attachOrigin.z = msg.ReadFloat();

	Vector vecDir = attachOrigin - pMarine->WorldSpaceCenter();
	VectorNormalize(vecDir);

	UTIL_ASW_MarineTakeDamage( attachOrigin, vecDir, pMarine->BloodColor(), 5, pMarine, true );
}

USER_MESSAGE_REGISTER( ASWMarineHitByFF );

void __MsgFunc_ASWEnemyZappedByThorns( bf_read &msg )
{
	int iMarine = msg.ReadShort();		
	C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(ClientEntityList().GetEnt(iMarine));		// turn iMarine ent index into the marine
	if ( !pMarine )
		return;

	int iAlien = msg.ReadShort();		
	C_ASW_Alien *pAlien = dynamic_cast<C_ASW_Alien*>(ClientEntityList().GetEnt(iAlien));		// turn iMarine ent index into the marine
	if ( !pAlien )
		return;

	Vector vecDir = pMarine->GetAbsOrigin() - pAlien->GetAbsOrigin();
	VectorNormalize(vecDir);

	Vector vecCtrl0 = (pAlien->WorldSpaceCenter() + Vector(0,0,20)) + vecDir*8;
	Vector vecCtrl1 = (pMarine->WorldSpaceCenter() + Vector(0,0,20)) - vecDir*8;

	//QAngle	vecAngles;
	//VectorAngles( -vecDir, vecAngles );
	//Vector vecForward, vecRight, vecUp;
	//AngleVectors( vecAngles, &vecForward, &vecRight, &vecUp );

	CUtlReference<CNewParticleEffect> pEffect;

	pEffect = pAlien->ParticleProp()->Create( "thorns_zap", PATTACH_ABSORIGIN_FOLLOW );
	pAlien->ParticleProp()->AddControlPoint( pEffect, 1, pMarine, PATTACH_CUSTOMORIGIN );
	pEffect->SetControlPoint( 0, vecCtrl0 );
	pEffect->SetControlPoint( 1, vecCtrl1 );
	//pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );

	CLocalPlayerFilter filter;						
	CSoundParameters params;

	// zap the alien!
	if ( C_BaseEntity::GetParametersForSound( "ASW_ElectrifiedSuit.Zap", params, NULL ) )
	{
		EmitSound_t ep( params );
		ep.m_pOrigin = &vecCtrl0;

		C_BaseEntity::EmitSound( filter, 0, ep );
	}
}
USER_MESSAGE_REGISTER( ASWEnemyZappedByThorns );

void __MsgFunc_ASWEnemyZappedByTesla( bf_read &msg )
{
	Vector attachOrigin;
	attachOrigin.x = msg.ReadFloat();
	attachOrigin.y = msg.ReadFloat();
	attachOrigin.z = msg.ReadFloat();

	int iEnemy = msg.ReadShort();		
	C_BaseEntity *pEnemy = dynamic_cast<C_BaseEntity*>(ClientEntityList().GetEnt(iEnemy));		// turn iMarine ent index into the marine
	if ( !pEnemy )
		return;

	Vector vecDir = attachOrigin - pEnemy->GetAbsOrigin();
	VectorNormalize(vecDir);

	Vector vecCtrl0 = (pEnemy->WorldSpaceCenter() + Vector(0,0,20)) - vecDir*8;
	Vector vecCtrl1 = attachOrigin;

	//QAngle	vecAngles;
	//VectorAngles( -vecDir, vecAngles );
	//Vector vecForward, vecRight, vecUp;
	//AngleVectors( vecAngles, &vecForward, &vecRight, &vecUp );

	CUtlReference<CNewParticleEffect> pEffect;

	pEffect = pEnemy->ParticleProp()->Create( "tesla_zap_fx", PATTACH_ABSORIGIN_FOLLOW, NULL, Vector( 0, 0, 32 ) );
	//pEnemy->ParticleProp()->AddControlPoint( pEffect, 1, pSource, PATTACH_CUSTOMORIGIN );
	pEffect->SetControlPoint( 0, vecCtrl0 );
	pEffect->SetControlPoint( 1, vecCtrl1 );
	//pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );

	CLocalPlayerFilter filter;						
	CSoundParameters params;

	// zap the alien!
	if ( C_BaseEntity::GetParametersForSound( "ASW_Tesla_Trap.Zap", params, NULL ) )
	{
		EmitSound_t ep( params );
		ep.m_pOrigin = &vecCtrl0;

		C_BaseEntity::EmitSound( filter, 0, ep );
	}

	C_ASW_Marine *pMarine = C_ASW_Marine::GetLocalMarine();
	if ( !pMarine )
		return;

	float flDist = pMarine->GetAbsOrigin().DistTo( attachOrigin );

	const float flRadius = 300.0f;
	float flPerc = 1.0 - clamp<float>( flDist / flRadius, 0.0f, 1.0f );

	ScreenShake_t shake;
	shake.command	= SHAKE_START;
	shake.amplitude = 1.0f * flPerc;
	shake.frequency = 80.f;
	shake.duration	= 0.5f;
	
	GetViewEffects()->Shake( shake );
}
USER_MESSAGE_REGISTER( ASWEnemyZappedByTesla );

void __MsgFunc_ASWEnemyTeslaGunArcShock( bf_read &msg )
{
	int iSrcEnt = msg.ReadShort();		
	C_BaseEntity *pSrcEnt = dynamic_cast<C_BaseEntity*>(ClientEntityList().GetEnt(iSrcEnt));		// turn iMarine ent index into the marine
	if ( !pSrcEnt )
		return;

	int iEnemy = msg.ReadShort();		
	C_BaseEntity *pEnemy = dynamic_cast<C_BaseEntity*>(ClientEntityList().GetEnt(iEnemy));		// turn iMarine ent index into the marine
	if ( !pEnemy )
		return;

	Vector vecDir = pSrcEnt->GetAbsOrigin() - pEnemy->GetAbsOrigin();
	VectorNormalize(vecDir);

	Vector vecCtrl0 = (pEnemy->WorldSpaceCenter() + Vector(0,0,20)) - vecDir*8;
	Vector vecCtrl1 = pSrcEnt->WorldSpaceCenter() + vecDir*8;


	CUtlReference<CNewParticleEffect> pEffect;

	pEffect = pEnemy->ParticleProp()->Create( "tesla_zap_fx", PATTACH_ABSORIGIN_FOLLOW, NULL, Vector( 0, 0, 32 ) );
	pEffect->SetControlPoint( 0, vecCtrl0 );
	pEffect->SetControlPoint( 1, vecCtrl1 );

	CLocalPlayerFilter filter;						
	CSoundParameters params;

	/*
	// zap the alien!
	if ( C_BaseEntity::GetParametersForSound( "ASW_Tesla_Trap.Zap", params, NULL ) )
	{
		EmitSound_t ep( params );
		ep.m_pOrigin = &vecCtrl0;

		C_BaseEntity::EmitSound( filter, 0, ep );
	}
	*/

	/*
	C_ASW_Marine *pMarine = C_ASW_Marine::GetLocalMarine();
	if ( !pMarine )
		return;

	float flDist = pMarine->GetAbsOrigin().DistTo( attachOrigin );

	const float flRadius = 300.0f;
	float flPerc = 1.0 - clamp<float>( flDist / flRadius, 0.0f, 1.0f );

	ScreenShake_t shake;
	shake.command	= SHAKE_START;
	shake.amplitude = 1.0f * flPerc;
	shake.frequency = 80.f;
	shake.duration	= 0.5f;

	GetViewEffects()->Shake( shake );
	*/
}
USER_MESSAGE_REGISTER( ASWEnemyTeslaGunArcShock );

// TODO: We're doing this via a user-message since the mining laser attaches to control points - 
// explore integrating this into the ParticleSystem client effect via CBaseEffect?
void __MsgFunc_ASWMiningLaserZap( bf_read &msg )
{
	int iMiningLaser = msg.ReadShort();
	int iTarget = msg.ReadShort();
	int iAttachmentIndex = msg.ReadShort();

	C_BaseEntity *pMiningLaser = dynamic_cast<C_BaseEntity*>(ClientEntityList().GetEnt(iMiningLaser));

	if ( !pMiningLaser )
	{
		return;
	}

	C_BaseEntity *pTarget = ClientEntityList().GetEnt(iTarget);
	Vector impactOrigin;

	impactOrigin.x = msg.ReadFloat();
	impactOrigin.y = msg.ReadFloat();
	impactOrigin.z = msg.ReadFloat();

	C_BaseAnimating::PushAllowBoneAccess( true, false, "ASWMiningLaserZap" );

	CUtlReference<CNewParticleEffect> pEffect;

	// attach to either a bone or just to the entity
	if ( iAttachmentIndex == -1 )
	{
		pEffect = pMiningLaser->ParticleProp()->Create( "electric_weapon_shot", PATTACH_ABSORIGIN_FOLLOW ); 
		pEffect->SetControlPointEntity( 0, pMiningLaser );
	}
	else
	{
		pEffect = pMiningLaser->ParticleProp()->Create( "electric_weapon_shot", PATTACH_POINT_FOLLOW, iAttachmentIndex ); 
	}


	// Use impactOrigin if we didn't hit anything interesting
	if( !pTarget || pTarget->m_takedamage == DAMAGE_NO )
	{
		pEffect->SetControlPoint( 1, impactOrigin );
	}
	else
	{	
		Vector vTargetMins, vTargetMaxs;
		float flHeight = pTarget->BoundingRadius();
		Vector vOffset( 0.0f, 0.0f, flHeight * 0.25 );

		C_ASW_Alien* pAlien = dynamic_cast<C_ASW_Alien*>(pTarget);
		// TODO: get some standardization in the attachment naming
		if ( pAlien && pAlien->LookupAttachment( "eyes" ) )
		{
			pMiningLaser->ParticleProp()->AddControlPoint( pEffect, 1, pTarget, PATTACH_POINT_FOLLOW, "eyes" );
		}
		else
		{
			pMiningLaser->ParticleProp()->AddControlPoint( pEffect, 1, pTarget, PATTACH_ABSORIGIN_FOLLOW, NULL, vOffset );
		}

		// UNDONE: this gives hit feedback, but is too noisy for now
		//CUtlReference<CNewParticleEffect> pTargetEffect;
		//pTargetEffect = pTarget->ParticleProp()->Create( "electrical_arc_01_system", PATTACH_ABSORIGIN_FOLLOW,-1, vOffset );
		//pTargetEffect->SetControlPointEntity( 0, pTarget );
		//pTargetEffect->SetControlPoint( 1, impactOrigin );
	}
	
	//CLocalPlayerFilter filter;
	//C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "ASW_Mining_Laser.BeamImpact", &impactOrigin );

	C_BaseAnimating::PopBoneAccess( "ASWMiningLaserZap" );
}
USER_MESSAGE_REGISTER( ASWMiningLaserZap );

const char *s_pszBurstPipeEffects[]=
{
	"impact_steam",
	"impact_steam_small",
	"impact_steam_short"
};

ConVar asw_burst_pipe_chance( "asw_burst_pipe_chance", "0.25f" );

void FX_ASW_Potential_Burst_Pipe( const Vector &vecImpactPoint, const Vector &vecReflect, const Vector &vecShotBackward, const Vector &vecNormal )
{
	if ( RandomFloat() > asw_burst_pipe_chance.GetFloat() )
		return;

	const char *szEffectName = s_pszBurstPipeEffects[ RandomInt( 0, NELEMS( s_pszBurstPipeEffects) - 1 ) ];
	CUtlReference<CNewParticleEffect> pSteamEffect = CNewParticleEffect::CreateOrAggregate( NULL, szEffectName, vecImpactPoint, NULL );
	if ( pSteamEffect )
	{
		Vector vecImpactY, vecImpactZ;
		VectorVectors( vecNormal, vecImpactY, vecImpactZ ); 
		vecImpactY *= -1.0f;

		pSteamEffect->SetControlPoint( 0, vecImpactPoint );
		pSteamEffect->SetControlPointOrientation( 0, vecImpactZ, vecImpactY, vecNormal );
	}
}