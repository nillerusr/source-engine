//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Gravity well device
//
//=====================================================================================//

#include "cbase.h"
#include "grenade_hopwire.h"
#include "rope.h"
#include "rope_shared.h"
#include "beam_shared.h"
#include "physics.h"
#include "physics_saverestore.h"
#include "explode.h"
#include "physics_prop_ragdoll.h"
#include "movevars_shared.h"
#ifdef EZ2
#include "ai_basenpc.h"
#include "npc_barnacle.h"
#include "ez2/npc_basepredator.h"
#include "hl2_player.h"
#include "particle_parse.h"
#include "npc_crow.h"
#include "AI_ResponseSystem.h"
#include "saverestore.h"
#include "saverestore_utlmap.h"
#include "items.h"
#include "hl2_shareddefs.h"

#include "ai_hint.h"
#include "ai_network.h"
#include "ai_node.h"
#include "ai_link.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar hopwire_vortex( "hopwire_vortex", "1" );

ConVar hopwire_strider_kill_dist_h( "hopwire_strider_kill_dist_h", "300" );
ConVar hopwire_strider_kill_dist_v( "hopwire_strider_kill_dist_v", "256" );
ConVar hopwire_strider_hits( "hopwire_strider_hits", "1" );
ConVar hopwire_trap("hopwire_trap", "0");
#ifndef EZ2
ConVar hopwire_hopheight( "hopwire_hopheight", "400" );
#endif

ConVar hopwire_minheight("hopwire_minheight", "100");
ConVar hopwire_radius("hopwire_radius", "300");
#ifndef EZ2
ConVar hopwire_strength( "hopwire_strength", "150" );
#endif
ConVar hopwire_duration("hopwire_duration", "3.0");
ConVar hopwire_pull_player("hopwire_pull_player", "1");
#ifdef EZ2
ConVar hopwire_strength( "hopwire_strength", "256" );
ConVar hopwire_hopheight( "hopwire_hopheight", "200" );
ConVar hopwire_ragdoll_radius( "hopwire_ragdoll_radius", "96" );
ConVar hopwire_conusme_radius( "hopwire_conusme_radius", "48" );
ConVar hopwire_spawn_life("hopwire_spawn_life", "1");
ConVar hopwire_spawn_node_radius( "hopwire_spawn_node_radius", "256" );
ConVar hopwire_spawn_interval_min( "hopwire_spawn_interval_min", "0.35" );
ConVar hopwire_spawn_interval_max( "hopwire_spawn_interval_max", "0.65" );
ConVar hopwire_spawn_interval_fail( "hopwire_spawn_interval_fail", "0.1" );
ConVar hopwire_guard_mass("hopwire_guard_mass", "1500");
ConVar hopwire_zombine_mass( "hopwire_zombine_mass", "800" );
ConVar hopwire_zombigaunt_mass( "hopwire_zombigaunt_mass", "1000" );
ConVar hopwire_bullsquid_mass("hopwire_bullsquid_mass", "500");
ConVar hopwire_stukabat_mass( "hopwire_stukabat_mass", "425" );
ConVar hopwire_antlion_mass("hopwire_antlion_mass", "325");
ConVar hopwire_zombie_mass( "hopwire_zombie_mass", "250" ); // TODO Zombies should require a rebel as an ingredient
ConVar hopwire_babysquid_mass( "hopwire_babysquid_mass", "200" );
ConVar hopwire_headcrab_mass("hopwire_headcrab_mass", "100");
ConVar hopwire_boid_mass( "hopwire_boid_mass", "50" );
ConVar hopwire_schlorp_small_mass( "hopwire_schlorp_small_mass", "30" );
ConVar hopwire_schlorp_medium_mass( "hopwire_schlorp_medium_mass", "100" );
ConVar hopwire_schlorp_large_mass( "hopwire_schlorp_large_mass", "350" );
ConVar hopwire_schlorp_huge_mass( "hopwire_schlorp_huge_mass", "600" );
ConVar hopwire_timer("hopwire_timer", "2.0");

ConVar stasis_freeze_player("stasis_freeze_player", "1");
ConVar stasis_radius("stasis_radius", "256");
ConVar stasis_strength("stasis_strength", "150");
ConVar stasis_duration("stasis_duration", "3.0");
ConVar stasis_timer("stasis_timer", "0.25");

// Move this elsewhere if this concept is expanded
ConVar ez2_spoilers_enabled( "ez2_spoilers_enabled", "0", FCVAR_NONE, "Enables the you-know-whats and you-know-whos that shouldn't shown in streams, but might make accidental cameos. This is on by default as a precaution." );
#endif

ConVar g_debug_hopwire( "g_debug_hopwire", "0" );

#define	DENSE_BALL_MODEL	"models/props_junk/metal_paintcan001b.mdl"

#define	MAX_HOP_HEIGHT		(hopwire_hopheight.GetFloat())		// Maximum amount the grenade will "hop" upwards when detonated
#define	MIN_HOP_HEIGHT		(hopwire_minheight.GetFloat())		// Minimum amount the grenade will "hop" upwards when detonated

#ifdef EZ2
// Xen Grenade Interactions
int	g_interactionXenGrenadePull			= 0;
int	g_interactionXenGrenadeConsume		= 0;
int	g_interactionXenGrenadeRelease		= 0;
int g_interactionXenGrenadeCreate		= 0;
int g_interactionXenGrenadeHop			= 0;
int g_interactionXenGrenadeRagdoll      = 0;

int	g_interactionStasisGrenadeFreeze	= 0;
int	g_interactionStasisGrenadeUnfreeze  = 0;
#endif

#ifdef EZ2
extern ISaveRestoreOps *responseSystemSaveRestoreOps;

extern IResponseSystem *PrecacheXenGrenadeResponseSystem( const char *scriptfile );
extern void GetXenGrenadeResponseFromSystem( char *szResponse, size_t szResponseSize, IResponseSystem *sys, const char *szIndex );

#define XEN_GRENADE_RECIPE_SCRIPT "scripts/talker/xen_grenade_recipes.txt"

static const Color XenColor = Color(0, 255, 0, 255);
template<class ... Args> void XenGrenadeDebugMsg( PRINTF_FORMAT_STRING const char *szMsg, Args ... args ) FMTFUNCTION( 1, 0 );
template<class ... Args> void XenGrenadeDebugMsg( const char *szMsg, Args ... args )
{
	if (!g_debug_hopwire.GetBool())
		return;

	ConColorMsg( XenColor, szMsg, args... );
}

static const char *g_SpawnThinkContext = "SpawnThink";
static const char *g_SchlorpThinkContext = "SchlorpThink";

//-----------------------------------------------------------------------------
// Purpose: Ensures Xen recipes are loaded and precached when they are needed.
//-----------------------------------------------------------------------------
class CXenGrenadeRecipeManager : public CAutoGameSystem
{
public:
	CXenGrenadeRecipeManager() : CAutoGameSystem( "CXenGrenadeRecipeManager" )
	{
	}

	virtual bool Init()
	{
		return true;
	}

	virtual void LevelInitPreEntity()
	{
		// Unless cheats are on, assume Xen doesn't exist by default
		if (sv_cheats->GetBool())
		{
			m_bXenExists = true;
			m_pszXenCreator = "sv_cheats";
		}
		else
		{
			m_bXenExists = false;
			m_pszXenCreator = "Unknown"; // Default value
		}

		m_bXenPrecached = false;

		m_bPrecachedLate = false;
	}

	virtual void LevelInitPostEntity()
	{
		// If Xen exists, precache Xen
		if (m_bXenExists)
		{
			PrecacheXenRecipes();
			m_bXenPrecached = true;
		}
	}

	virtual void LevelShutdownPreEntity()
	{
	}

	virtual void LevelShutdownPostEntity()
	{
	}

	virtual void OnRestore()
	{
		if (m_bXenExists)
		{
			PrecacheXenRecipes();
		}
	}

	//------------------------------------------------------------------------------------

	inline bool XenExists() const { return m_bXenExists; }

	inline bool PrecachedLate() const { return m_bPrecachedLate; }
	inline const char *GetXenCreator() const { return m_pszXenCreator; }

	// Affirms the existence of Xen.
	void VerifyRecipeManager( const char *pszActivator )
	{
		if (!m_bXenExists)
		{
			XenGrenadeDebugMsg( "Xen recipe manager activated\n" );
			m_pszXenCreator = pszActivator;
		}

		m_bXenExists = true;

		if (!m_bXenPrecached && gpGlobals->curtime > 2.0f)
		{
			// Uh-oh. Late precache is a serious hang.
			// There's not much more we could do about it, unfortunately.
			UTIL_CenterPrintAll( "Precaching Xen...\n" );

			PrecacheXenRecipes();

			//Warning( "************************************\n!!!!! Late precache of Xen recipe manager !!!!!\n************************************\n" );

			m_bPrecachedLate = true;
			m_bXenPrecached = true;
		}
	}

	void PrecacheXenRecipes();

	bool FindBestRecipe( char *szRecipe, size_t szRecipeSize, AI_CriteriaSet &set, CGravityVortexController *pActivator );

	bool ParseRecipe( char *szRecipe, CUtlMap<string_t, string_t> &spawnList );
	bool ParseRecipeBlock( char *szRecipe, CUtlMap<string_t, string_t> &spawnList );

	inline IResponseSystem *GetResponseSystem() { return m_pInstancedResponseSystem; }

	friend class CXenRecipeSaveRestoreBlockHandler;

protected:
	IResponseSystem *m_pInstancedResponseSystem;

	bool m_bXenExists;
	bool m_bXenPrecached;

	// The creator of Xen.
	const char *m_pszXenCreator;

	bool m_bPrecachedLate;
};

CXenGrenadeRecipeManager	g_XenGrenadeRecipeManager;

//-----------------------------------------------------------------------------
// Recipe Response System Save/Restore
//-----------------------------------------------------------------------------
static short XEN_RECIPE_SAVE_RESTORE_VERSION = 1;

class CXenRecipeSaveRestoreBlockHandler : public CDefSaveRestoreBlockHandler
{
public:
	const char *GetBlockName()
	{
		return "XenRecipes";
	}

	//---------------------------------

	void Save( ISave *pSave )
	{
		bool bDoSave = g_XenGrenadeRecipeManager.XenExists() && g_XenGrenadeRecipeManager.m_pInstancedResponseSystem != NULL;

		pSave->WriteBool( &bDoSave );
		if ( bDoSave )
		{
			pSave->StartBlock( "InstancedResponseSystem" );
			{
				SaveRestoreFieldInfo_t fieldInfo = { &g_XenGrenadeRecipeManager.m_pInstancedResponseSystem, 0, NULL };
				responseSystemSaveRestoreOps->Save( fieldInfo, pSave );
			}
			pSave->EndBlock();
		}
	}

	//---------------------------------

	void WriteSaveHeaders( ISave *pSave )
	{
		pSave->WriteShort( &XEN_RECIPE_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void ReadRestoreHeaders( IRestore *pRestore )
	{
		// No reason why any future version shouldn't try to retain backward compatability. The default here is to not do so.
		short version;
		pRestore->ReadShort( &version );
		m_fDoLoad = ( version == XEN_RECIPE_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void Restore( IRestore *pRestore, bool createPlayers )
	{
		if ( m_fDoLoad )
		{
			bool bDoRestore = false;

			pRestore->ReadBool( &bDoRestore );
			if ( bDoRestore )
			{
				char szResponseSystemBlockName[SIZE_BLOCK_NAME_BUF];
				pRestore->StartBlock( szResponseSystemBlockName );
				if ( !Q_stricmp( szResponseSystemBlockName, "InstancedResponseSystem" ) )
				{
					if ( !g_XenGrenadeRecipeManager.m_pInstancedResponseSystem )
					{
						// TODO: Verify that using 'PrecacheXenGrenadeResponseSystem' instead of 'PrecacheCustomResponseSystem' on restore is a good thing
						g_XenGrenadeRecipeManager.m_pInstancedResponseSystem = PrecacheXenGrenadeResponseSystem( XEN_GRENADE_RECIPE_SCRIPT );
						if (g_XenGrenadeRecipeManager.m_pInstancedResponseSystem)
						{
							SaveRestoreFieldInfo_t fieldInfo =
							{
								&g_XenGrenadeRecipeManager.m_pInstancedResponseSystem,
								0,
								NULL
							};
							responseSystemSaveRestoreOps->Restore( fieldInfo, pRestore );
						}
					}
				}
				pRestore->EndBlock();
			}

			// Tell the manager Xen has been restored
			g_XenGrenadeRecipeManager.m_bXenExists = true;
		}
	}

private:
	bool m_fDoLoad;
};

//-----------------------------------------------------------------------------

CXenRecipeSaveRestoreBlockHandler g_XenRecipeSaveRestoreBlockHandler;

//-------------------------------------

ISaveRestoreBlockHandler *GetXenRecipeSaveRestoreBlockHandler()
{
	return &g_XenRecipeSaveRestoreBlockHandler;
}

//-----------------------------------------------------------------------------
// Purpose: Designed to be used externally
//-----------------------------------------------------------------------------
void VerifyXenRecipeManager( const char *pszActivator )
{
	g_XenGrenadeRecipeManager.VerifyRecipeManager( pszActivator );
}

//-----------------------------------------------------------------------------
// Purpose: Debug commands
//-----------------------------------------------------------------------------
CON_COMMAND( hopwire_xen_print_state, "Prints the current state of Xen." )
{
	char szMsg[512];
	if (g_XenGrenadeRecipeManager.XenExists())
	{
		Q_strncpy( szMsg, "Xen exists.\n{\n", sizeof( szMsg ) );

		Q_snprintf( szMsg, sizeof( szMsg ), "%s	Creator of Xen: %s\n", szMsg, g_XenGrenadeRecipeManager.GetXenCreator() );
		Q_snprintf( szMsg, sizeof( szMsg ), "%s	Precached late: %s\n", szMsg, g_XenGrenadeRecipeManager.PrecachedLate() ? "Yes" : "No" );

		Q_snprintf( szMsg, sizeof( szMsg ), "%s}\n", szMsg );
	}
	else
	{
		Q_strncpy( szMsg, "Xen does not exist.\n", sizeof( szMsg ) );
	}

	ConColorMsg( XenColor, "%s", szMsg );
}

CON_COMMAND( hopwire_xen_force_create, "Forces the creation of Xen if it doesn't exist already." )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();

	if (!g_XenGrenadeRecipeManager.XenExists())
	{
		VerifyXenRecipeManager( pPlayer ? pPlayer->GetPlayerName() : "You, apparently!" );
		ConColorMsg( XenColor, "Xen created!\n" );
	}
	else
	{
		ConColorMsg( XenColor, "Xen already exists.\n" );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Precaches Xen
//-----------------------------------------------------------------------------
void CXenGrenadeRecipeManager::PrecacheXenRecipes( void )
{
	if ( m_bXenPrecached )
		return;

	if ( !m_pInstancedResponseSystem )
	{
		m_pInstancedResponseSystem = PrecacheXenGrenadeResponseSystem( XEN_GRENADE_RECIPE_SCRIPT );
	}

#ifdef NEW_RESPONSE_SYSTEM
	CUtlVector<AI_Response> pResponses;
#else
	CUtlVector<AI_Response*> pResponses;
#endif
	m_pInstancedResponseSystem->GetAllResponses( &pResponses );
	if ( pResponses.Count() <= 0 )
	{
		XenGrenadeDebugMsg( "Precache: No responses!?!?\n" );
		return;
	}

	char szRecipe[512];
	int szRecipeSize = sizeof(szRecipe);
	CUtlMap<string_t, string_t> recipeEntities;

	SetDefLessFunc( recipeEntities );

	CUtlSymbolTable precachedEntities;

	for (int i = 0; i < pResponses.Count(); i++)
	{
		// Handle the response here...
#ifdef NEW_RESPONSE_SYSTEM
		pResponses[i].GetResponse( szRecipe, szRecipeSize );
#else
		pResponses[i]->GetResponse( szRecipe, szRecipeSize );
#endif
		GetXenGrenadeResponseFromSystem( szRecipe, szRecipeSize, m_pInstancedResponseSystem, szRecipe );

		recipeEntities.EnsureCapacity( 16 );
		ParseRecipe( szRecipe, recipeEntities );

		for (unsigned int i2 = 0; i2 < recipeEntities.Count(); i2++)
		{
			const char *pszClass = STRING( recipeEntities.Key( i2 ) );

			if (g_debug_hopwire.GetBool())
			{
				CUtlSymbol sym = precachedEntities.Find( pszClass );
				if (!sym.IsValid())
				{
					precachedEntities.AddString( pszClass );
					XenGrenadeDebugMsg( "Precache: Precaching %s (%i:%i)\n", pszClass, i, i2 );
				}
				else
				{
					XenGrenadeDebugMsg( "Precache: Symbol %s (%i:%i) already valid\n", pszClass, i, i2 );
				}
			}

			UTIL_PrecacheXenVariant( pszClass );
		}

		recipeEntities.Purge();
	}

	if (g_debug_hopwire.GetBool())
	{
		// List all of the entities we precached (reuse szRecipe)
		Q_strncpy( szRecipe, "All Precached Entities:\n{\n", szRecipeSize );

		for (int i = 0; i < precachedEntities.GetNumStrings(); i++)
		{
			precachedEntities.String( i );
			Q_snprintf( szRecipe, sizeof( szRecipe ), "%s	%s\n", szRecipe, precachedEntities.String( i ) );
		}

		Q_snprintf( szRecipe, sizeof( szRecipe ), "%s}\n", szRecipe );

		XenGrenadeDebugMsg( "%s", szRecipe );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *conceptName - 
//-----------------------------------------------------------------------------
bool CXenGrenadeRecipeManager::FindBestRecipe( char *szRecipe, size_t szRecipeSize, AI_CriteriaSet &set, CGravityVortexController *pActivator )
{
	XenGrenadeDebugMsg( "	FindBestRecipe: Start\n" );

	IResponseSystem *rs = GetResponseSystem();
	if ( !rs )
		return false;

	AI_Response result;
	bool found = rs->FindBestResponse( set, result );
	if ( !found )
	{
		XenGrenadeDebugMsg( "	FindBestRecipe: Didn't find response\n" );
		return false;
	}

	// Handle the response here...
	result.GetResponse( szRecipe, szRecipeSize );
	GetXenGrenadeResponseFromSystem( szRecipe, szRecipeSize, rs, szRecipe );

	XenGrenadeDebugMsg( "	FindBestRecipe: Found response \"%s\"\n", szRecipe );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *conceptName - 
//-----------------------------------------------------------------------------
bool CXenGrenadeRecipeManager::ParseRecipe( char *szRecipe, CUtlMap<string_t, string_t> &spawnList )
{
	XenGrenadeDebugMsg( "	ParseRecipe: Start\n" );

	bool bSucceed = false;

	// Check for recipe blocks
	CUtlStringList vecBlocks;
	V_SplitString( szRecipe, ";", vecBlocks );
	FOR_EACH_VEC( vecBlocks, i )
	{
		if (ParseRecipeBlock( vecBlocks[i], spawnList ))
			bSucceed = true;
	}

	return bSucceed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *conceptName - 
//-----------------------------------------------------------------------------
bool CXenGrenadeRecipeManager::ParseRecipeBlock( char *szRecipe, CUtlMap<string_t, string_t> &spawnList )
{
	XenGrenadeDebugMsg( "	ParseRecipeBlock: Start \"%s\"\n", szRecipe );

	char *colon1 = Q_strstr( szRecipe, ":" );
	char szheader[128];

	char *header = NULL;
	if ( colon1 )
	{
		Q_strncpy( szheader, szRecipe, colon1 - szRecipe + 1 );
		header = szheader;
	}
	else
	{
		// No colon
		header = szRecipe;
	}

	string_t iszClass = NULL_STRING;
	int iNumEnts = 1;
	CUtlDict<char*> EntKV;
	
	// Check the header
	CUtlStringList vecParams;
	V_SplitString( header, " ", vecParams );
	FOR_EACH_VEC( vecParams, i )
	{
		switch (i)
		{
			// Classname
			case 0:
				iszClass = AllocPooledString( vecParams[i] );
				break;

			// Number of Entities
			case 1:
				interval_t interval = ReadInterval( vecParams[i] );
				iNumEnts = (int)(RandomInterval( interval )+0.5f);
				break;
		}
	}

	XenGrenadeDebugMsg( "	ParseRecipeBlock: Header is \"%s\", \"%i\"\n", STRING(iszClass), iNumEnts );

	string_t kv = NULL_STRING;
	if (colon1)
	{
		kv = AllocPooledString( colon1+1 );

		XenGrenadeDebugMsg( "		ParseRecipeBlock: KV is \"%s\"\n", STRING(kv) );
	}

	if (iszClass != NULL_STRING)
	{
		for (int i = 0; i < iNumEnts; i++)
		{
			spawnList.Insert( iszClass, kv );
			XenGrenadeDebugMsg( "	ParseRecipeBlock: Added \"%s\" number %i to spawn list\n", STRING( iszClass ), iNumEnts );
		}
	}

	return true;
}

// This is used for when an entity is close enough to be consumed, but shouldn't be consumed through
// solid objects.
class CXenGrenadeTraceFilter : public CTraceFilterSimple
{
public:

	CXenGrenadeTraceFilter( const IHandleEntity *passentity, int collisionGroup, CGravityVortexController *vortex ) : CTraceFilterSimple( passentity, collisionGroup )
	{
		m_pVortex = vortex;
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		bool base = CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );

		if ( base )
		{
			// Just don't hit entities which are also going to be consumed.
			// (Also don't hit the grenade itself!)
			CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
			return pEntity != m_pVortex->GetOwnerEntity() && !m_pVortex->CanConsumeEntity( pEntity );
		}

		return base;
	}

	CGravityVortexController *m_pVortex;

};

//-----------------------------------------------------------------------------
// Purpose: Returns whether an entity can be consumed
//-----------------------------------------------------------------------------
bool CGravityVortexController::CanConsumeEntity( CBaseEntity *pEnt )
{
	// Don't consume my owner entity
	if (pEnt == GetOwnerEntity())
		return false;

	// Don't try to consume the player! What would even happen!?
	if (pEnt->IsPlayer())
		return false;

	// Don't consume objects that are protected
	if ( pEnt->IsDisplacementImpossible() )
		return false;

	// Don't consume entities deferred for removal
	if ( (pEnt->GetEffects() & EF_NODRAW) && (pEnt->GetSolidFlags() & FSOLID_NOT_SOLID) && (pEnt->GetMoveType() == MOVETYPE_NONE) )
		return false;

	// Don't consume barnacle parts! (Weird edge case)
	CBarnacleTongueTip *pBarnacleTongueTip = dynamic_cast< CBarnacleTongueTip* >(pEnt);
	if (pBarnacleTongueTip != NULL)
		return false;

	// Get our base physics object
	if ( pEnt->VPhysicsGetObject() == NULL )
		return false;

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Returns the amount of mass consumed by the vortex
//-----------------------------------------------------------------------------
float CGravityVortexController::GetConsumedMass( void ) const 
{
	return m_flMass;
}

//-----------------------------------------------------------------------------
// Purpose: Adds the entity's mass to the aggregate mass consumed
//-----------------------------------------------------------------------------
void CGravityVortexController::ConsumeEntity( CBaseEntity *pEnt )
{
#ifdef EZ2
	// Make sure we can consume this entity
	if (!CanConsumeEntity(pEnt))
		return;

	IPhysicsObject *pPhysObject = pEnt->VPhysicsGetObject();
#else
	// Don't try to consume the player! What would even happen!?
	if (pEnt->IsPlayer())
		return;

#ifdef EZ
	// Don't consume objects that are protected
	if ( pEnt->IsDisplacementImpossible() )
		return;


	// Don't consume barnacle parts! (Weird edge case)
	CBarnacleTongueTip *pBarnacleTongueTip = dynamic_cast< CBarnacleTongueTip* >(pEnt);
	if (pBarnacleTongueTip != NULL)
	{
		return;
	}
#endif

	// Get our base physics object
	IPhysicsObject *pPhysObject = pEnt->VPhysicsGetObject();
	if ( pPhysObject == NULL )
		return;
#endif

#ifdef EZ2
	float flMass = 0.0f;

	pEnt->FireNamedOutput( "OnConsumed", variant_t(), this, pEnt );

	// Ragdolls need to report the sum of all their parts
	CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( pEnt );
	if ( pRagdoll != NULL )
	{		
		// Find the aggregate mass of the whole ragdoll
		ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll();
		for ( int j = 0; j < pRagdollPhys->listCount; ++j )
		{
			flMass += pRagdollPhys->list[j].pObject->GetMass();
		}
	}
	else
	{
		DisplacementInfo_t dinfo( this, this, &GetAbsOrigin(), &GetAbsAngles() );
		if (pEnt->DispatchInteraction( g_interactionXenGrenadeConsume, &dinfo, GetThrower() ))
		{
			// Do not remove
			m_hReleaseEntity = pEnt;
			return;
		}
		else
		{
			// Otherwise we just take the normal mass
			flMass += pPhysObject->GetMass();
		}
	}

	// Some NPCs are sucked up before they ragdoll, which stops them from dying. Stop them from breaking stuff.
	if (pEnt->MyNPCPointer())
	{
		// Run Vscript OnDeath hook
		if (pEnt->m_ScriptScope.IsInitialized() && pEnt->g_Hook_OnDeath.CanRunInScope( pEnt->m_ScriptScope ))
		{
			CTakeDamageInfo info( this, this, 10000.0, DMG_GENERIC | DMG_REMOVENORAGDOLL );
			HSCRIPT hInfo = g_pScriptVM->RegisterInstance( const_cast<CTakeDamageInfo*>(&info) );

			// info
			ScriptVariant_t functionReturn;
			ScriptVariant_t args[] ={ ScriptVariant_t( hInfo ) };
			if (pEnt->g_Hook_OnDeath.Call( pEnt->m_ScriptScope, &functionReturn, args ) && (functionReturn.m_type == FIELD_BOOLEAN && functionReturn.m_bool == false))
			{
				// Make this entity cheat death
				g_pScriptVM->RemoveInstance( hInfo );
				return;
			}

			g_pScriptVM->RemoveInstance( hInfo );
		}

		// If the NPC is to be remvoed, fire the output
		pEnt->FireNamedOutput( "OnDeath", variant_t(), this, pEnt );
	}

	// Add this to our lists
	if (hopwire_spawn_life.GetInt() == 1)
	{
		bool bCountAsNPC = pEnt->IsNPC();
		string_t iszClassName = pEnt->m_iClassname;
		if (iszClassName != NULL_STRING)
		{
			if (pRagdoll && pRagdoll->GetSourceClassName() != NULL_STRING)
			{
				// Use the ragdoll's source classname
				iszClassName = pRagdoll->GetSourceClassName();
				bCountAsNPC = true;
			}
			else if (FClassnameIs( pEnt, "combine_mine" ))
			{
				// Mines are to be considered NPCs so they can spawn monsters
				bCountAsNPC = true;
			}

			short iC = m_ClassMass.Find( iszClassName );
			if (iC != m_ClassMass.InvalidIndex())
			{
				// Add to the mass
				m_ClassMass.Element(iC) += flMass;
				XenGrenadeDebugMsg( "Adding %s to existing class mass\n", STRING(iszClassName) );
			}
			else
			{
				// Add a new key
				m_ClassMass.Insert(iszClassName, flMass);
				XenGrenadeDebugMsg( "Adding %s new class mass\n", STRING(iszClassName) );
			}
		}

		string_t iszModelName = pEnt->GetModelName();
		if (iszModelName != NULL_STRING)
		{
			const char *slash = strrchr(STRING(iszModelName), '/' );
			if (slash)
				iszModelName = MAKE_STRING(slash+1);

			if ( iszModelName == NULL_STRING )
				iszModelName = pEnt->GetModelName();

			short iM = m_ModelMass.Find( iszModelName );
			if (iM != m_ModelMass.InvalidIndex())
			{
				// Add to the mass
				m_ModelMass.Element(iM) += flMass;
				XenGrenadeDebugMsg( "Adding %s to existing model mass\n", STRING(iszModelName) );
			}
			else
			{
				// Add a new key
				m_ModelMass.Insert(iszModelName, flMass);
				XenGrenadeDebugMsg( "Adding %s new model mass\n", STRING(iszModelName) );
			}
		}

		if (bCountAsNPC)
			m_iSuckedNPCs++;
		else
			m_iSuckedProps++;
	}

	m_flMass += flMass;
	m_flSchlorpMass += flMass;

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (pPlayer)
	{
		// Some specific entities being consumed should be taken note of for later
		if (pEnt->GetServerVehicle() && FClassnameIs(pEnt, "prop_vehicle_drivable_apc"))
			pPlayer->AddContext("sucked_apc", "1");
	}

	// Blixibon - Things like turrets use looping sounds that need to be interrupted
	// before being removed, otherwise they play forever
	pEnt->EmitSound( "AI_BaseNPC.SentenceStop" );
	pEnt->EmitSound( "AI_BaseNPC.SentenceStop2" );
#else
	// Ragdolls need to report the sum of all their parts
	CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( pEnt );
	if ( pRagdoll != NULL )
	{		
		// Find the aggregate mass of the whole ragdoll
		ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll();
		for ( int j = 0; j < pRagdollPhys->listCount; ++j )
		{
			m_flMass += pRagdollPhys->list[j].pObject->GetMass();
		}
	}
	else
	{
		// Otherwise we just take the normal mass
		m_flMass += pPhysObject->GetMass();
	}
#endif

	// Destroy the entity
#ifdef EZ
	if ( pPhysObject->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		pPlayer->ForceDropOfCarriedPhysObjects( pEnt );
	}

	// Remove all children first
	CBaseEntity *pChild = pEnt->FirstMoveChild();
	while ( pChild )
	{
		pChild->RemoveDeferred();
		pChild = pChild->NextMovePeer();
	}

	pEnt->RemoveDeferred();

	if (GetThrower() && GetThrower()->IsPlayer())
	{
		CBasePlayer *pPlayer = ((CBasePlayer *)GetThrower());
		if (pPlayer->GetBonusChallenge() == EZ_CHALLENGE_MASS)
		{
			// Add this mass to the bonus progress
			// TODO: Prevent Xen spawns from contributing to this value?
			pPlayer->SetBonusProgress( pPlayer->GetBonusProgress() - flMass );
		}
	}
#else
	UTIL_Remove( pEnt );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Causes players within the radius to be sucked in
//-----------------------------------------------------------------------------
void CGravityVortexController::PullPlayersInRange( void )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer || !pPlayer->VPhysicsGetObject())
		return;
	
	Vector	vecForce = GetAbsOrigin() - pPlayer->WorldSpaceCenter();
	float	dist = VectorNormalize( vecForce );
	
	// FIXME: Need a more deterministic method here
	if ( dist < 128.0f )
	{
		// Kill the player (with falling death sound and effects)
#ifndef EZ2
		CTakeDamageInfo deathInfo( this, this, GetAbsOrigin(), GetAbsOrigin(), 200, DMG_FALL );
#else
		CTakeDamageInfo deathInfo( this, this, GetAbsOrigin(), GetAbsOrigin(), 5, DMG_FALL );
#endif
		pPlayer->TakeDamage( deathInfo );
		
		if ( pPlayer->IsAlive() == false )
		{
#ifdef EZ2
			color32 green = { 32, 252, 0, 255 };
			UTIL_ScreenFade( pPlayer, green, 0.1f, 0.0f, (FFADE_OUT|FFADE_STAYOUT) );
#else
			color32 black = { 0, 0, 0, 255 };
			UTIL_ScreenFade( pPlayer, black, 0.1f, 0.0f, (FFADE_OUT|FFADE_STAYOUT) );
#endif
			return;
		}
	}

	// Must be within the radius
	if ( dist > m_flRadius )
		return;

#ifdef EZ2
	// Don't pull unless convar set
	if ( !hopwire_pull_player.GetBool() )
		return;

	// Don't pull noclipping players
	if ( pPlayer->GetMoveType() == MOVETYPE_NOCLIP )
		return;
#endif

	float mass = pPlayer->VPhysicsGetObject()->GetMass();
	float playerForce = m_flStrength * 0.05f;

#ifdef EZ2
	if (m_flPullFadeTime > 0.0f)
	{
		playerForce *= ((gpGlobals->curtime - m_flStartTime) / m_flPullFadeTime);
	}
#endif

	// Find the pull force
	// NOTE: We might want to make this non-linear to give more of a "grace distance"
	vecForce *= ( 1.0f - ( dist / m_flRadius ) ) * playerForce * mass;
	vecForce[2] *= 0.025f;
	
	pPlayer->SetBaseVelocity( vecForce );
	pPlayer->AddFlag( FL_BASEVELOCITY );
	
	// Make sure the player moves
	if ( vecForce.z > 0 && ( pPlayer->GetFlags() & FL_ONGROUND) )
	{
		pPlayer->SetGroundEntity( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attempts to kill an NPC if it's within range and other criteria
// Input  : *pVictim - NPC to assess
//			**pPhysObj - pointer to the ragdoll created if the NPC is killed
// Output :	bool - whether or not the NPC was killed and the returned pointer is valid
//-----------------------------------------------------------------------------
bool CGravityVortexController::KillNPCInRange( CBaseEntity *pVictim, IPhysicsObject **pPhysObj )
{
	CBaseCombatCharacter *pBCC = pVictim->MyCombatCharacterPointer();

	// Don't kill owner
	if (pBCC == GetOwnerEntity())
		return false;

	// See if we can ragdoll
	if ( pBCC != NULL && pBCC->CanBecomeRagdoll() )
	{
		// Don't bother with striders
		if ( FClassnameIs( pBCC, "npc_strider" ) )
			return false;

		// TODO: Make this an interaction between the NPC and the vortex

		// Become ragdoll
		CTakeDamageInfo info( this, this, 1.0f, DMG_GENERIC );
#ifdef EZ2
		// Don't infinitely generate ragdolls of entities we can't kill
		if ( !pVictim->PassesDamageFilter( info ) )
		{
			*pPhysObj = NULL;
			return false;
		}

		if ( pVictim->DispatchInteraction( g_interactionXenGrenadePull, &info, GetThrower() ) )
		{
			// Pull the object in if there was no special handling
			*pPhysObj = NULL;
			return false;
		}

		// See if there is custom handling for ragdolls
		if (pVictim->DispatchInteraction( g_interactionXenGrenadeRagdoll, &info, GetThrower() ))
		{
			*pPhysObj = NULL;
			return false;
		}
#endif
		CBaseEntity *pRagdoll = CreateServerRagdoll( pBCC, 0, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
		pRagdoll->SetCollisionBounds( pVictim->CollisionProp()->OBBMins(), pVictim->CollisionProp()->OBBMaxs() );

		// Necessary to cause it to do the appropriate death cleanup
		CTakeDamageInfo ragdollInfo( this, this, 10000.0, DMG_GENERIC | DMG_REMOVENORAGDOLL );
		pVictim->TakeDamage( ragdollInfo );

		// Return the pointer to the ragdoll
		*pPhysObj = pRagdoll->VPhysicsGetObject();
		return true;
	}

	// Wasn't able to ragdoll this target
	*pPhysObj = NULL;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a dense ball with a mass equal to the aggregate mass consumed by the vortex
//-----------------------------------------------------------------------------
void CGravityVortexController::CreateDenseBall( void )
{
	CBaseEntity *pBall = CreateEntityByName( "prop_physics" );
	
	pBall->SetModel( DENSE_BALL_MODEL );
	pBall->SetAbsOrigin( GetAbsOrigin() );
	pBall->Spawn();

	IPhysicsObject *pObj = pBall->VPhysicsGetObject();
	if ( pObj != NULL )
	{
		pObj->SetMass( GetConsumedMass() );
	}
}
#ifdef EZ2
bool VectorLessFunc( const Vector &lhs, const Vector &rhs )
{
	return lhs.LengthSqr() < rhs.LengthSqr();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &restore - 
//-----------------------------------------------------------------------------
int	CGravityVortexController::Restore( IRestore &restore )
{
	// This needs to be done before the game is restored
	SetDefLessFunc( m_ModelMass );
	SetDefLessFunc( m_ClassMass );
	m_ModelMass.EnsureCapacity( 128 );
	m_ClassMass.EnsureCapacity( 128 );

	m_HullMap.SetLessFunc( VectorLessFunc );
	m_HullMap.EnsureCapacity( 32 );

	SetDefLessFunc( m_SpawnList );
	m_SpawnList.EnsureCapacity( 16 );

	return BaseClass::Restore( restore );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a xen lifeform with a mass equal to the aggregate mass consumed by the vortex
//-----------------------------------------------------------------------------
void CGravityVortexController::CreateXenLife()
{
	VerifyXenRecipeManager( GetClassname() );

	// Create a list of conditions based on the consumed entities
	AI_CriteriaSet set;

	ModifyOrAppendCriteria( set );

	// Append local player criteria to set,too
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer )
		pPlayer->ModifyOrAppendPlayerCriteria( set );

	AppendContextToCriteria( set );

	//set.AppendCriteria( "thrower", CFmtStrN<64>( "%s", ????? ) );
	set.AppendCriteria( "mass", CFmtStrN<32>( "%0.2f", m_flMass ) );

	// Figure out which entities had the most prominent roles
	string_t iszBestStr = NULL_STRING;
	float flBestMass = 0.0f;
	FOR_EACH_MAP_FAST(m_ClassMass, i)
	{
		if (m_ClassMass.Element(i) > flBestMass)
		{
			iszBestStr = m_ClassMass.Key(i);
			flBestMass = m_ClassMass.Element(i);
		}
	}

	if (iszBestStr != NULL_STRING)
	{
		XenGrenadeDebugMsg( "Best class is %s with %f\n", STRING(iszBestStr), flBestMass );
		set.AppendCriteria( "best_class", CFmtStrN<64>( "%s", STRING(iszBestStr) ) );
	}

	iszBestStr = NULL_STRING;
	flBestMass = 0.0f;

	FOR_EACH_MAP_FAST(m_ModelMass, i)
	{
		if (m_ModelMass.Element(i) > flBestMass)
		{
			iszBestStr = m_ModelMass.Key(i);
			flBestMass = m_ModelMass.Element(i);
		}
	}

	if (iszBestStr != NULL_STRING)
	{
		XenGrenadeDebugMsg( "Best model is %s with %f\n", STRING( iszBestStr ), flBestMass );
		set.AppendCriteria( "best_model", CFmtStrN<MAX_PATH>( "%s", STRING(iszBestStr) ) );
	}

	set.AppendCriteria( "num_npcs", CNumStr( m_iSuckedNPCs ) );
	set.AppendCriteria( "num_props", CNumStr( m_iSuckedProps ) );

	// Measure the NPC-to-prop ratio
	float flNPCToPropRatio = 0.0f;
	if (m_iSuckedNPCs > 0)
	{
		if (m_iSuckedProps <= 0)
			flNPCToPropRatio = 1.0f;
		else
			flNPCToPropRatio = (float)m_iSuckedNPCs / (float)(m_iSuckedNPCs + m_iSuckedProps);
	}

	set.AppendCriteria( "npc_to_prop_ratio", CNumStr( flNPCToPropRatio ) );

	// Now we'll test hull permissions
	// Go through each node and find available links.
	int iTotalHulls = 0;

	iTotalHulls = AddNodesToHullMap( GetAbsOrigin() );

	// If we couldn't find any hulls, jump to the ground
	if ( iTotalHulls <= 0 )
	{
		trace_t tr;
		CXenGrenadeTraceFilter traceFilter( NULL, COLLISION_GROUP_NONE, this );
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -8192 ), MASK_SOLID, &traceFilter, &tr );

		iTotalHulls = AddNodesToHullMap( tr.endpos );
	}

	set.AppendCriteria( "available_nodes", CNumStr(m_HullMap.Count()) );
	set.AppendCriteria( "available_hulls", CNumStr(iTotalHulls) );

	if (m_flMass >= hopwire_boid_mass.GetFloat())
	{
		// Look for fly nodes
		CHintCriteria hintCriteria;
		hintCriteria.SetHintType( HINT_CROW_FLYTO_POINT );
		hintCriteria.AddIncludePosition( GetAbsOrigin(), 4500 );
		CAI_Hint *pHint = CAI_HintManager::FindHint( GetAbsOrigin(), hintCriteria );
		// If the vortex controller can't find a bird node, don't spawn any birds!
		if ( pHint )
		{
			set.AppendCriteria( "available_flyto", "1" );
		}
	}

	char szRecipe[512];
	if (g_XenGrenadeRecipeManager.FindBestRecipe(szRecipe, sizeof(szRecipe), set, this))
	{
		if (g_XenGrenadeRecipeManager.ParseRecipe(szRecipe, m_SpawnList))
			StartSpawning();
	}
}

//------------------------------------------------------------------------------
// Search for nodes near the vortex and add into its hull map for spawns.
// Returns the total hull count
//------------------------------------------------------------------------------
int CGravityVortexController::AddNodesToHullMap( const Vector vecSearchOrigin )
{
	int iTotalHulls = 0;

	for (int node = 0; node < g_pBigAINet->NumNodes(); node++)
	{
		CAI_Node *pNode = g_pBigAINet->GetNode( node );

		if ( (vecSearchOrigin - pNode->GetOrigin() ).LengthSqr() > Square( m_flNodeRadius ))
			continue;

		// Only use ground nodes
		if (pNode->GetType() != NODE_GROUND)
			continue;

		// Only use nodes which we can trace from the vortex
		if (!FVisible( pNode->GetOrigin(), MASK_SOLID ))
			continue;

		// Iterate through these hulls to find each space we should care about.
		// Should be sorted largest to smallest.
		static Hull_t iTestHulls[] =
		{
			HULL_LARGE,
			HULL_MEDIUM,
			HULL_WIDE_SHORT,
			HULL_WIDE_HUMAN,
			HULL_HUMAN,
			HULL_TINY,
		};

		static int iTestHullBits = 0;
		if (iTestHullBits == 0)
		{
			for (int i = 0; i < ARRAYSIZE( iTestHulls ); i++)
				iTestHullBits |= HullToBit( iTestHulls[i] );
		}

		int hullbits = 0;

		CAI_Link *pLink = NULL;
		for (int i = 0; i < pNode->NumLinks(); i++)
		{
			pLink = pNode->GetLinkByIndex( i );
			if (pLink)
			{
				for (int hull = 0; hull < ARRAYSIZE( iTestHulls ); hull++)
				{
					Hull_t iHull = iTestHulls[hull];
					if (pLink->m_iAcceptedMoveTypes[iHull] & bits_CAP_MOVE_GROUND)
						hullbits |= HullToBit( iHull );
				}

				// Break early if this link has all of them
				if (hullbits == iTestHullBits)
					break;
			}
		}

		if (g_debug_hopwire.GetBool())
			NDebugOverlay::Cross3D( pNode->GetOrigin(), 3.0f, 0, 255, 0, true, 5.0f );

		if (hullbits > 0)
		{
			// Add to our hull map
			m_HullMap.Insert( GetAbsOrigin() - pNode->GetOrigin(), hullbits );
			iTotalHulls |= hullbits;

			if (m_HullMap.Count() >= (unsigned int)m_HullMap.MaxElement())
				break;
		}
	}

	return iTotalHulls;
}

void CGravityVortexController::StartSpawning()
{
	SetContextThink( &CGravityVortexController::SpawnThink, gpGlobals->curtime, g_SpawnThinkContext );
}

void CGravityVortexController::SpawnThink()
{
	if ( m_iCurSpawned >= m_SpawnList.Count() )
	{
		SetContextThink( NULL, TICK_NEVER_THINK, g_SpawnThinkContext );
		return;
	}

	XenGrenadeDebugMsg( "	SpawnThink: Trying \"%s\" (%i/%i)\n", m_SpawnList.Key(m_iCurSpawned), m_iCurSpawned+1, m_SpawnList.Count() );

	if ( TryCreateRecipeNPC(STRING(m_SpawnList.Key(m_iCurSpawned)), STRING(m_SpawnList.Element(m_iCurSpawned))) )
		SetContextThink( &CGravityVortexController::SpawnThink, gpGlobals->curtime + RandomFloat( hopwire_spawn_interval_min.GetFloat(), hopwire_spawn_interval_max.GetFloat() ), g_SpawnThinkContext );
	else
		SetContextThink( &CGravityVortexController::SpawnThink, gpGlobals->curtime + hopwire_spawn_interval_fail.GetFloat(), g_SpawnThinkContext );

	m_iCurSpawned++;
}

//------------------------------------------------------------------------------
// A small wrapper around SV_Move that never clips against the supplied entity.
//------------------------------------------------------------------------------
static bool TestXentityPosition( CBaseEntity *pEntity )
{	
	trace_t	trace;
	UTIL_TraceEntity( pEntity, pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin(), pEntity->IsNPC() ? MASK_NPCSOLID : MASK_SOLID, &trace );
	return (trace.startsolid == 0);
}


//------------------------------------------------------------------------------
// Searches along the direction ray in steps of "step" to see if 
// the entity position is passible.
// Used for putting the player in valid space when toggling off noclip mode.
//------------------------------------------------------------------------------
static bool FindPassableXenGrenadeSpace( CBaseEntity *pEntity, const Vector& direction, float step, Vector& oldorigin )
{
	int i;
	for ( i = 0; i < 100; i++ )
	{
		Vector origin = pEntity->GetAbsOrigin();
		VectorMA( origin, step, direction, origin );
		pEntity->SetAbsOrigin( origin );
		if ( TestXentityPosition( pEntity ) )
		{
			VectorCopy( pEntity->GetAbsOrigin(), oldorigin );
			return true;
		}
	}
	return false;
}

static bool FindPassableSpaceForXentity( CBaseEntity *pEntity )
{
	Vector vecOrigin = pEntity->GetAbsOrigin();
	Vector forward, right, up;

	AngleVectors( pEntity->GetAbsAngles(), &forward, &right, &up);
	
	// Try to move into the world
	if ( !FindPassableXenGrenadeSpace( pEntity, forward, 1, vecOrigin ) )
	{
		if ( !FindPassableXenGrenadeSpace( pEntity, right, 1, vecOrigin ) )
		{
			if ( !FindPassableXenGrenadeSpace( pEntity, right, -1, vecOrigin ) )		// left
			{
				if ( !FindPassableXenGrenadeSpace( pEntity, up, 1, vecOrigin ) )	// up
				{
					if ( !FindPassableXenGrenadeSpace( pEntity, up, -1, vecOrigin ) )	// down
					{
						if ( !FindPassableXenGrenadeSpace( pEntity, forward, -1, vecOrigin ) )	// back
						{
							return false;
						}
					}
				}
			}
		}
	}

	pEntity->SetAbsOrigin( vecOrigin );
	return true;
}

bool CGravityVortexController::TryCreateRecipeNPC( const char *szClass, const char *szKV )
{
	if (!ez2_spoilers_enabled.GetBool())
	{
		if (FStrEq(szClass, "npc_zassassin"))
		{
			Warning("Stopping spoiler NPC from loading due to ez2_spoilers_enabled; replacing with npc_zombigaunt\n");
			szClass = "npc_zombigaunt";
		}
	}

	CBaseEntity * pEntity = CreateEntityByName( szClass );
	if (pEntity == NULL)
		return false;

	XenGrenadeDebugMsg( "	TryCreateRecipeNPC: \"%s\" created\n", szClass );

	// Spawning a large number of entities at the origin tends to cause crashes,
	// even though they're teleported later on in the function
	pEntity->SetAbsOrigin( GetAbsOrigin() + RandomVector(64.0f, 64.0f) );

	// Randomize yaw, but nothing else
	QAngle angSpawnAngles = GetAbsAngles();
	angSpawnAngles.y = RandomFloat(0, 360);
	pEntity->SetAbsAngles( angSpawnAngles );

	pEntity->SetEZVariant( EZ_VARIANT_XEN );

	CAI_BaseNPC * baseNPC = pEntity->MyNPCPointer();
	if (baseNPC)
	{
		baseNPC->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );

		const char * squadnameContext = GetContextValue( "squadname" );

		// If the context "squadname" does not match an empty string, use that instead of the default Xen squad name		
		if (squadnameContext && squadnameContext[0])
		{
			DevMsg( "Adding xenpc '%s' to squad '%s' \n", baseNPC->GetDebugName(), squadnameContext );
			baseNPC->SetSquadName( AllocPooledString( squadnameContext ) );
		}
		else
		{
			// Set the squad name of a new Xen NPC to 'xenpc_headcrab', 'xenpc_bullsquid', etc
			char * squadname = UTIL_VarArgs( "xe%s", szClass );
			DevMsg( "Adding xenpc '%s' to squad '%s' \n", baseNPC->GetDebugName(), squadname );
			baseNPC->SetSquadName( AllocPooledString( squadname ) );
		}
	}

	if (szKV != NULL)
	{
		// Append any keyvalues
		ParseKeyValues( pEntity, szKV );
	}

	return TrySpawnRecipeNPC( pEntity, true );
}

bool CGravityVortexController::TrySpawnRecipeNPC( CBaseEntity *pEntity, bool bCallSpawnFuncs )
{
	DisplacementInfo_t dinfo( this, this, &pEntity->GetAbsOrigin(), &pEntity->GetAbsAngles() );
	pEntity->DispatchInteraction( g_interactionXenGrenadeCreate, &dinfo, GetThrower() );

	if (bCallSpawnFuncs)
		DispatchSpawn( pEntity );

	XenGrenadeDebugMsg( "	TryCreateRecipeNPC: \"%s\" spawned\n", pEntity->GetDebugName() );

	CAI_BaseNPC *baseNPC = pEntity->MyNPCPointer();

	// Now find a valid space
	Vector vecBestSpace = vec3_invalid;
	float flBestDistSqr = FLT_MAX;
	short iBestIndex = -1;

	trace_t tr;
	int hull;
	Vector vecHullMins;
	Vector vecHullMaxs;
	if (baseNPC)
	{
		hull = HullToBit(baseNPC->GetHullType());
		vecHullMins = baseNPC->GetHullMins();
		vecHullMaxs = baseNPC->GetHullMaxs();
	}
	else
	{
		// Approximation for non-NPCs
		hull = bits_TINY_HULL;
		vecHullMins = pEntity->WorldAlignMins();
		vecHullMaxs = pEntity->WorldAlignMaxs();
	}

	for (unsigned int i = 0; i < m_HullMap.Count(); i++)
	{
		if (m_HullMap.Element(i) & hull && (m_HullMap.Key(i).LengthSqr() < flBestDistSqr))
		{
			// See if we could actually fit at this space
			Vector vecSpace = GetAbsOrigin() - m_HullMap.Key(i);
			Vector vUpBit = vecSpace;
			vUpBit.z += 1;
			AI_TraceHull( vecSpace, vUpBit, vecHullMins, vecHullMaxs,
				baseNPC ? MASK_NPCSOLID : MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tr );
			if ( tr.startsolid || (tr.fraction < 1.0) )
			{
				if (g_debug_hopwire.GetBool())
				{
					NDebugOverlay::Box( vecSpace, vecHullMins, vecHullMaxs, 255, 0, 0, 0, 2.0f );
				}
			}
			else
			{
				if (g_debug_hopwire.GetBool())
				{
					// This space works
					NDebugOverlay::Box( vecSpace, vecHullMins, vecHullMaxs, 0, 255, 0, 0, 2.0f );
				}

				vecBestSpace = vecSpace;
				flBestDistSqr = m_HullMap.Key(i).LengthSqr();
				iBestIndex = i;
			}
		}
	}

	if ( vecBestSpace == vec3_invalid )
	{
		// Just check the origin if there's no available node space
		vecBestSpace = GetAbsOrigin();
		Vector vUpBit = vecBestSpace;
		vUpBit.z += 1;
		AI_TraceHull( vecBestSpace, vUpBit, vecHullMins, vecHullMaxs,
			baseNPC ? MASK_NPCSOLID : MASK_SOLID, pEntity, COLLISION_GROUP_NONE, &tr );
		if ( tr.startsolid || (tr.fraction < 1.0) )
		{
			if (FindPassableSpaceForXentity( pEntity ))
				DevMsg( "Xen grenade found space for %s\n", pEntity->GetDebugName() );
			else
			{
				DevMsg( "Xen grenade can't create %s.  Bad Position!\n", pEntity->GetDebugName() );
				UTIL_Remove( pEntity );
				return false;
			}
		}
	}

	if (m_HullMap.IsValidIndex(iBestIndex))
		m_HullMap.RemoveAt(iBestIndex);

	// SetAbsOrigin() doesn't seem to work here for some reason
	// and ragdoll Teleport() always crashes
	pEntity->CBaseEntity::Teleport( &vecBestSpace, NULL, NULL );

	if ( !pEntity->IsEFlagSet( EFL_SERVER_ONLY ) )
	{
		// Now that the XenPC was created successfully and we know it isn't a server-only entity, play a sound and display a particle effect
		SpawnEffects( pEntity );
	}

	// XenPC - ACTIVATE! Especially important for antlion glows
	if (bCallSpawnFuncs)
		pEntity->Activate();

	// Notify the NPC of the player's position
	// Sometimes Xen grenade spawns just kind of hang out in one spot. They should always have at least one potential enemy to aggro
	// XenPCs that are 'predators' don't need this because they already wander
	/*
	CHL2_Player *pPlayer = (CHL2_Player *)UTIL_GetLocalPlayer();
	if ( pPredator == NULL && pPlayer != NULL )
	{
		DevMsg( "Updating xenpc '%s' enemy memory \n", baseNPC->GetDebugName(), squadname );
		baseNPC->UpdateEnemyMemory( pPlayer, pPlayer->GetAbsOrigin(), this );
	}
	*/

	m_OutEntity.Set( pEntity, pEntity, this );

	XenGrenadeDebugMsg( "	TryCreateRecipeNPC: \"%s\" reached end of function\n", pEntity->GetDebugName() );
	return true;
}

void CGravityVortexController::ParseKeyValues( CBaseEntity *pEntity, const char *szKV )
{
	// Now check the rest of the string for KV
	CUtlStringList vecParams;
	V_SplitString( szKV, " ", vecParams );
	FOR_EACH_VEC( vecParams, i )
	{
		if ( (i + 1) < vecParams.Count() )
		{
			if (vecParams[i][0] == '^')
			{
				// Ragdoll override animation strings can be fairly large
				char szValue[384];
				Q_strncpy(szValue, vecParams[i]+1, sizeof(szValue));

				// Treat ^ as quotes
				for (i++ ; i < vecParams.Count(); i++)
				{
					if (vecParams[i][strlen(vecParams[i])-1] == '^')
					{
						vecParams[i][strlen(vecParams[i])-1] = '\0';
						Q_snprintf( szValue, sizeof( szValue ), "%s %s", szValue, vecParams[i] );
						break;
					}
					else
						Q_snprintf( szValue, sizeof( szValue ), "%s %s", szValue, vecParams[i] );
				}

				XenGrenadeDebugMsg( "	ParseKeyValues: Quoted text is \"%s\"\n", szValue );

				pEntity->KeyValue( vecParams[i], szValue );
			}
			else
			{
				pEntity->KeyValue( vecParams[i], vecParams[i+1] );
			}
		}
	}
}

void CGravityVortexController::SpawnEffects( CBaseEntity *pEntity )
{
	pEntity->EmitSound( "WeaponXenGrenade.SpawnXenPC" );
	DispatchParticleEffect( "xenpc_spawn", pEntity->WorldSpaceCenter(), pEntity->GetAbsAngles(), pEntity );

	CBeam *pBeam = CBeam::BeamCreate( "sprites/rollermine_shock.vmt", 4 );
	if ( pBeam != NULL )
	{
		pBeam->EntsInit( pEntity, this );

		pBeam->SetEndWidth( 8 );
		pBeam->SetNoise( 4 );
		pBeam->LiveForTime( 0.3f );
		
		pBeam->SetWidth( 1 );
		pBeam->SetBrightness( 255 );
		pBeam->SetColor( 0, 255, 0 );
		pBeam->RelinkBeam();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates a xen lifeform with a mass equal to the aggregate mass consumed by the vortex
//-----------------------------------------------------------------------------
void CGravityVortexController::CreateOldXenLife( void )
{
	if ( GetConsumedMass() >= hopwire_guard_mass.GetFloat() )
	{
		if ( TryCreateNPC( "npc_antlionguard" ) )
			return;
	}
	if (GetConsumedMass() >= hopwire_zombigaunt_mass.GetFloat())
	{
		if (TryCreateNPC( "npc_zombigaunt" ) )
			return;
	}
	if ( GetConsumedMass() >= hopwire_zombine_mass.GetFloat() )
	{
		if ( TryCreateNPC( "npc_zombine" ) )
			return;
	}
	if ( GetConsumedMass() >= hopwire_bullsquid_mass.GetFloat() )
	{
		if ( TryCreateNPC( "npc_bullsquid" ) )
			return;
	}
	if (GetConsumedMass() >= hopwire_stukabat_mass.GetFloat())
	{
		if (TryCreateNPC( "npc_stukabat" ))
			return;
	}
	if ( GetConsumedMass() >= hopwire_antlion_mass.GetFloat() )
	{
		if ( TryCreateNPC( "npc_antlion" ) )
			return;
	}
	if ( GetConsumedMass() >= hopwire_zombie_mass.GetFloat() )
	{
		if ( TryCreateNPC( "npc_zombie" ) )
			return;
	}
	if (GetConsumedMass() >= hopwire_babysquid_mass.GetFloat())
	{
		if ( TryCreateBaby( "npc_bullsquid" ) )
			return;
	}
	if ( GetConsumedMass() >= hopwire_headcrab_mass.GetFloat() )
	{
		if ( TryCreateNPC( "npc_headcrab" ) )
			return;
	}
	if ( GetConsumedMass() >= hopwire_boid_mass.GetFloat() )
	{
		if ( TryCreateBird( "npc_boid" ) )
			return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create an NPC if possible.
//	Returns true if the NPC was created, false if no NPC created
//-----------------------------------------------------------------------------
bool CGravityVortexController::TryCreateNPC( const char *className ) {
	return TryCreateComplexNPC( className, false, false );
}

bool CGravityVortexController::TryCreateBaby( const char *className ) {
	return TryCreateComplexNPC( className, true, false );
}

#define SF_CROW_FLYING		16
bool CGravityVortexController::TryCreateBird ( const char *className ) {

	// Look for fly nodes
	CHintCriteria hintCriteria;
	hintCriteria.SetHintType( HINT_CROW_FLYTO_POINT );
	hintCriteria.AddIncludePosition( GetAbsOrigin(), 4500 );
	CAI_Hint *pHint = CAI_HintManager::FindHint( GetAbsOrigin(), hintCriteria );
	// If the vortex controller can't find a bird node, don't spawn any birds!
	if ( pHint == NULL )
	{
		return false;
	}

	return TryCreateComplexNPC( className, false, true );
}

bool CGravityVortexController::TryCreateComplexNPC( const char *className, bool isBaby, bool isBird) {
	CBaseEntity * pEntity = CreateEntityByName( className );
	CAI_BaseNPC * baseNPC = pEntity->MyNPCPointer();
	if (pEntity == NULL || baseNPC == NULL) {
		return false;
	}

	baseNPC->SetAbsOrigin( GetAbsOrigin() );
	baseNPC->SetAbsAngles( GetAbsAngles() );
	if (isBird)
	{
		baseNPC->AddSpawnFlags( SF_CROW_FLYING );
	}
	else
	{
		baseNPC->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );
	}
	baseNPC->m_tEzVariant = EZ_VARIANT_XEN;
	// Set the squad name of a new Xen NPC to 'xenpc_headcrab', 'xenpc_bullsquid', etc
	char * squadname = UTIL_VarArgs( "xe%s", className );
	DevMsg( "Adding xenpc '%s' to squad '%s' \n", baseNPC->GetDebugName(), squadname );
	baseNPC->SetSquadName( AllocPooledString( squadname ) );

	// If the created NPC is a bullsquid or other predatory alien, make sure it has the appropriate fields set
	CNPC_BasePredator * pPredator = dynamic_cast< CNPC_BasePredator * >(baseNPC);
	if (pPredator != NULL)
	{
		pPredator->SetIsBaby( isBaby );
		{
			inputdata_t dummy;
			pPredator->InputSetWanderAlways( dummy );
		}
		{
			inputdata_t dummy;
			pPredator->InputEnableSpawning( dummy );
		}
		if ( !isBaby )
		{
			// Xen bullsquids come into the world ready to spawn.
			// Ready for the infestation?
			pPredator->SetReadyToSpawn( true );
			pPredator->SetNextSpawnTime( gpGlobals->curtime + 15.0f ); // 15 seconds to first spawn
			pPredator->SetHungryTime( gpGlobals->curtime ); // Be ready to eat as soon as we come through
			pPredator->SetTimesFed( 2 ); // Twins!
		}
	}

	DispatchSpawn( baseNPC );

	Vector vUpBit = baseNPC->GetAbsOrigin();
	vUpBit.z += 1;
	trace_t tr;
	AI_TraceHull( baseNPC->GetAbsOrigin(), vUpBit, baseNPC->GetHullMins(), baseNPC->GetHullMaxs(),
		MASK_NPCSOLID, baseNPC, COLLISION_GROUP_NONE, &tr );
	if ( tr.startsolid || (tr.fraction < 1.0) )
	{
		DevMsg( "Xen grenade can't create %s.  Bad Position!\n", className );
		baseNPC->SUB_Remove();
		if (g_debug_hopwire.GetBool())
			NDebugOverlay::Box( baseNPC->GetAbsOrigin(), baseNPC->GetHullMins(), baseNPC->GetHullMaxs(), 255, 0, 0, 0, 0 );
		return false;
	}

	// Now that the XenPC was created successfully, play a sound and display a particle effect
	EmitSound( "WeaponXenGrenade.SpawnXenPC" );
	DispatchParticleEffect( "xenpc_spawn", baseNPC->WorldSpaceCenter(), baseNPC->GetAbsAngles(), baseNPC );

	// XenPC - ACTIVATE! Especially important for antlion glows
	baseNPC->Activate();

	// Notify the NPC of the player's position
	// Sometimes Xen grenade spawns just kind of hang out in one spot. They should always have at least one potential enemy to aggro
	// XenPCs that are 'predators' don't need this because they already wander
	CHL2_Player *pPlayer = (CHL2_Player *)UTIL_GetLocalPlayer();
	if ( pPredator == NULL && pPlayer != NULL )
	{
		DevMsg( "Updating xenpc '%s' enemy memory \n", baseNPC->GetDebugName() );
		baseNPC->UpdateEnemyMemory( pPlayer, pPlayer->GetAbsOrigin(), this );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Schlorp
//-----------------------------------------------------------------------------
void CGravityVortexController::SchlorpThink( void )
{
	if (m_flSchlorpMass > 0.0f)
	{
		// 
		// Play a sound based on the amount of mass consumed in the past 0.2 seconds
		// 
		const char *pszSoundName;
		if (m_flSchlorpMass >= hopwire_schlorp_huge_mass.GetFloat())
			pszSoundName = "WeaponXenGrenade.Schlorp_Huge";
		else if (m_flSchlorpMass >= hopwire_schlorp_large_mass.GetFloat())
			pszSoundName = "WeaponXenGrenade.Schlorp_Large";
		else if (m_flSchlorpMass >= hopwire_schlorp_medium_mass.GetFloat())
			pszSoundName = "WeaponXenGrenade.Schlorp_Medium";
		else if (m_flSchlorpMass >= hopwire_schlorp_small_mass.GetFloat())
			pszSoundName = "WeaponXenGrenade.Schlorp_Small";
		else
			pszSoundName = "WeaponXenGrenade.Schlorp_Tiny"; // Tiny (default)

		EmitSound( pszSoundName );
	}

	m_flSchlorpMass = 0.0f;
	SetNextThink( gpGlobals->curtime + 0.2f, g_SchlorpThinkContext );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Pulls physical objects towards the vortex center, killing them if they come too near
//-----------------------------------------------------------------------------
void CGravityVortexController::PullThink( void )
{
	float flStrength = m_flStrength;

#ifdef EZ2
	// Pull any players close enough to us
	PullPlayersInRange();

	// Draw debug information
	if ( g_debug_hopwire.GetInt() >= 2 )
	{
		NDebugOverlay::Sphere( GetAbsOrigin(), m_flRadius, 0, 255, 0, 16, 4.0f );
	}

	// First, loop through entities within the consume radius and try consume
	CBaseEntity *pEnts[128];
	int numEnts = UTIL_EntitiesInSphere( pEnts, 128, GetAbsOrigin(), m_flConsumeRadius, 0 );
	for (int i = 0; i < numEnts; i++)
	{
		// The Xen grenade should not consume entities through grates and other non-consumable objects
		trace_t tr;
		CXenGrenadeTraceFilter traceFilter( pEnts[i], COLLISION_GROUP_NONE, this );
		UTIL_TraceLine( pEnts[i]->WorldSpaceCenter(), GetAbsOrigin(), MASK_SOLID, &traceFilter, &tr );
		if (tr.fraction == 1.0f)
		{
			ConsumeEntity( pEnts[i] );
			continue;
		}
	}

	// Next, loop through all entities within the pull radius
	numEnts = UTIL_EntitiesInSphere( pEnts, 128, GetAbsOrigin(), m_flRadius, 0 );

	if (!m_bPVSCreated)
	{
		// Create a PVS for this entity
		engine->GetPVSForCluster( engine->GetClusterForOrigin( GetAbsOrigin() ), sizeof(m_PVS), m_PVS );
	}

	if (m_flPullFadeTime > 0.0f)
	{
		flStrength *= ((gpGlobals->curtime - m_flStartTime) / m_flPullFadeTime);
	}
#else
	// Pull any players close enough to us
	if (hopwire_pull_player.GetBool())
		PullPlayersInRange();

	Vector mins, maxs;
	mins = GetAbsOrigin() - Vector( m_flRadius, m_flRadius, m_flRadius );
	maxs = GetAbsOrigin() + Vector( m_flRadius, m_flRadius, m_flRadius );

	// Draw debug information
#ifdef EZ2
	if ( g_debug_hopwire.GetInt() >= 2 )
#else
	if ( g_debug_hopwire.GetBool() )
#endif
	{
		NDebugOverlay::Box( GetAbsOrigin(), mins - GetAbsOrigin(), maxs - GetAbsOrigin(), 0, 255, 0, 16, 4.0f );
	}

	CBaseEntity *pEnts[128];
	int numEnts = UTIL_EntitiesInBox( pEnts, 128, mins, maxs, 0 );
#endif

	for ( int i = 0; i < numEnts; i++ )
	{
		if (pEnts[i]->IsPlayer())
			continue;

		IPhysicsObject *pPhysObject = NULL;

#ifdef EZ2
		// Don't consume entities already in the process of being removed.
		// NPCs which generate ragdolls might not be removed in time, so check for EF_NODRAW as well.
		if ( pEnts[i]->IsMarkedForDeletion() || pEnts[i]->IsEffectActive(EF_NODRAW) )
			continue;

		// Don't pull objects that are protected
		if ( pEnts[i]->IsDisplacementImpossible() )
			continue;

		const Vector& vecEntCenter = pEnts[i]->WorldSpaceCenter();

		// We do a PVS check here to make sure the entity isn't on another floor or behind a thick wall.
		if ( !engine->CheckOriginInPVS( vecEntCenter, m_PVS, sizeof( m_PVS ) ) )
			continue;

		Vector	vecForce = GetAbsOrigin() - vecEntCenter;
		Vector	vecForce2D = vecForce;
		vecForce2D[2] = 0.0f;
		float	dist2D = VectorNormalize( vecForce2D );
		float	dist = VectorNormalize( vecForce );
		
		// First, pull npcs outside of the ragdoll radius towards the vortex
		if ( abs( dist ) > hopwire_ragdoll_radius.GetFloat() ) 
		{
			CAI_BaseNPC * pNPC = pEnts[i]->MyNPCPointer();
			if (pEnts[i]->IsNPC() && pNPC != NULL && pNPC->CanBecomeRagdoll())
			{
				// Find the pull force
				// Minimum pull force is 10% of strength here
				vecForce *= MAX( 1.0f - ( abs( dist2D ) / m_flRadius), 0.1f ) * flStrength;

				// Physics damage info
				CTakeDamageInfo info( this, this, vecForce, GetAbsOrigin(), flStrength, DMG_BLAST );

				// Dispatch interaction. Skip pulling if it returns true
				if ( pEnts[i]->DispatchInteraction( g_interactionXenGrenadePull, &info, GetThrower() ) )
				{
					continue;
				}

				// Pull
				pNPC->ApplyAbsVelocityImpulse( vecForce );

				// We already handled this NPC, move on
				continue;
			}
		}
		if ( KillNPCInRange( pEnts[i], &pPhysObject ) )
		{
			DevMsg( "Xen grenade turned NPC '%s' into a ragdoll! \n", pEnts[i]->GetDebugName() );
		}
		else
		{
			// If we didn't have a valid victim, see if we can just get the vphysics object
			pPhysObject = pEnts[i]->VPhysicsGetObject();
			if ( pPhysObject == NULL )
			{
				continue;
			}
		}

		float mass = 0.0f;

		CRagdollProp * pRagdoll = dynamic_cast< CRagdollProp* >( pEnts[i] );
		ragdoll_t * pRagdollPhys = NULL;
		if (pRagdoll != NULL)
		{
			pRagdollPhys = pRagdoll->GetRagdoll();
		}

		if( pRagdollPhys != NULL)
		{			
			// Find the aggregate mass of the whole ragdoll
			for ( int j = 0; j < pRagdollPhys->listCount; ++j )
			{
				mass += pRagdollPhys->list[j].pObject->GetMass();
			}
		}
		else if ( pPhysObject != NULL )
		{
			mass = pPhysObject->GetMass();
		}

#else
		// Attempt to kill and ragdoll any victims in range
		if ( KillNPCInRange( pEnts[i], &pPhysObject ) == false )
		{	
			// If we didn't have a valid victim, see if we can just get the vphysics object
			pPhysObject = pEnts[i]->VPhysicsGetObject();
			if ( pPhysObject == NULL )
				continue;
		}

		float mass;

		CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( pEnts[i] );
		if ( pRagdoll != NULL )
		{
			ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll();
			mass = 0.0f;
			
			// Find the aggregate mass of the whole ragdoll
			for ( int j = 0; j < pRagdollPhys->listCount; ++j )
			{
				mass += pRagdollPhys->list[j].pObject->GetMass();
			}
		}
		else
		{
			mass = pPhysObject->GetMass();
		}
		Vector	vecForce = GetAbsOrigin() - pEnts[i]->WorldSpaceCenter();
		Vector	vecForce2D = vecForce;
		vecForce2D[2] = 0.0f;
		float	dist2D = VectorNormalize( vecForce2D );
		float	dist = VectorNormalize( vecForce );

		// FIXME: Need a more deterministic method here
		if ( dist < 48.0f )
		{
			ConsumeEntity( pEnts[i] );
			continue;
		}

		// Must be within the radius
		if ( dist > m_flRadius )
			continue;
#endif

		// Find the pull force
		// Minimum pull force is 10% of strength here
		vecForce *= MAX( 1.0f - ( abs( dist2D ) / m_flRadius ), 0.1f ) * flStrength * mass;
		

#ifdef EZ2
		CTakeDamageInfo info( this, this, vecForce, GetAbsOrigin(), flStrength, DMG_BLAST );
		if ( !pEnts[i]->DispatchInteraction( g_interactionXenGrenadePull, &info, GetThrower() ) && pPhysObject != NULL )
		{
			// Pull the object in if there was no special handling
			pEnts[i]->VPhysicsTakeDamage( info );
		}
#else
		if ( pEnts[i]->VPhysicsGetObject() )
		{
			// Pull the object in
			pEnts[i]->VPhysicsTakeDamage( CTakeDamageInfo( this, this, vecForce, GetAbsOrigin(), flStrength, DMG_BLAST ) );
		}
#endif
	}

	// Keep going if need-be
	if ( m_flEndTime > gpGlobals->curtime )
	{
		SetThink( &CGravityVortexController::PullThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
#ifdef EZ2
		DevMsg( "Consumed %.2f kilograms\n", m_flMass );

		// Fire game event for player xen grenades
		IGameEvent *event = gameeventmanager->CreateEvent( "xen_grenade" );
		if (event)
		{
			if (GetThrower())
			{
				event->SetInt( "entindex_attacker", GetThrower()->entindex() );
			}

			event->SetFloat( "mass", m_flMass );
			gameeventmanager->FireEvent( event );
		}

		DisplacementInfo_t dinfo( this, this, &GetAbsOrigin(), &GetAbsAngles() );
		if ( m_hReleaseEntity && m_hReleaseEntity->DispatchInteraction( g_interactionXenGrenadeRelease, &dinfo, GetThrower() ) )
		{
			// Act as if it's the Xen life we were supposed to spawn
			m_hReleaseEntity = NULL;
		}

		SetContextThink( NULL, TICK_NEVER_THINK, g_SchlorpThinkContext );
		
		if (!HasSpawnFlags( SF_VORTEX_CONTROLLER_DONT_SPAWN_LIFE ))
		{
			// Spawn Xen lifeform
			if ( hopwire_spawn_life.GetInt() == 1 )
				CreateXenLife();
			else if ( hopwire_spawn_life.GetInt() == 2 )
				CreateOldXenLife();
		}

		m_OnPullFinished.FireOutput( this, this );

		if (!HasSpawnFlags(SF_VORTEX_CONTROLLER_DONT_REMOVE))
		{
			// Remove at the maximum possible time of when we would be finished spawning entities
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( gpGlobals->curtime + (m_SpawnList.Count() * hopwire_spawn_interval_max.GetFloat() + 1.0f) );
			XenGrenadeDebugMsg("REMOVE TIME: Count %i * interval %f + 1.0 = %f\n", m_SpawnList.Count(), hopwire_spawn_interval_max.GetFloat(), (m_SpawnList.Count() * hopwire_spawn_interval_max.GetFloat() + 1.0f) );
		}
#else
		//Msg( "Consumed %.2f kilograms\n", m_flMass );
		//CreateDenseBall();
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: Starts the vortex working
//-----------------------------------------------------------------------------
void CGravityVortexController::StartPull( const Vector &origin, float radius, float strength, float duration )
{
	SetAbsOrigin( origin );
	m_flEndTime	= gpGlobals->curtime + duration;
	m_flRadius	= radius;
	m_flStrength= strength;

#ifdef EZ2
	// Play a danger sound throughout the duration of the vortex so that NPCs run away
	CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin(), radius, duration, this );

	SetDefLessFunc( m_ModelMass );
	SetDefLessFunc( m_ClassMass );
	m_ModelMass.EnsureCapacity( 64 );
	m_ClassMass.EnsureCapacity( 64 );

	m_HullMap.SetLessFunc( VectorLessFunc );
	m_HullMap.EnsureCapacity( 32 );

	SetDefLessFunc( m_SpawnList );
	m_SpawnList.EnsureCapacity( 16 );

	m_flStartTime = gpGlobals->curtime;
#endif

	SetThink( &CGravityVortexController::PullThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

#ifdef EZ2
	SetContextThink( &CGravityVortexController::SchlorpThink, gpGlobals->curtime + 0.2f, g_SchlorpThinkContext );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Creation utility
//-----------------------------------------------------------------------------
#ifdef EZ2
CGravityVortexController *CGravityVortexController::Create( const Vector &origin, float radius, float strength, float duration, CBaseEntity *pGrenade )
#else
CGravityVortexController *CGravityVortexController::Create( const Vector &origin, float radius, float strength, float duration )
#endif
{
	// Create an instance of the vortex
	CGravityVortexController *pVortex = (CGravityVortexController *)CreateEntityByName( "vortex_controller" );
	if ( pVortex == NULL )
		return NULL;

#ifdef EZ2
	pVortex->SetOwnerEntity( pGrenade );
	CBaseGrenade * pGrenadeCast = static_cast<CBaseGrenade*>(pGrenade);
	if (pGrenadeCast)
	{
		pVortex->SetThrower( static_cast<CBaseGrenade*>(pGrenade)->GetThrower() );
	}
	else if ( pGrenade->GetOwnerEntity() )
	{
		pVortex->SetThrower( pGrenade->GetOwnerEntity()->MyCombatCharacterPointer() );
	}

	pVortex->SetNodeRadius( hopwire_spawn_node_radius.GetFloat() );
	pVortex->SetConsumeRadius( hopwire_conusme_radius.GetFloat() );
#endif

	// Start the vortex working
	pVortex->StartPull( origin, radius, strength, duration );

	return pVortex;
}

BEGIN_DATADESC( CGravityVortexController )

#ifdef EZ2
	DEFINE_KEYFIELD( m_flMass, FIELD_FLOAT, "mass" ),
	DEFINE_KEYFIELD( m_flEndTime, FIELD_TIME, "duration" ),
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
	DEFINE_KEYFIELD( m_flStrength, FIELD_FLOAT, "strength" ),

	DEFINE_KEYFIELD( m_flNodeRadius, FIELD_FLOAT, "node_radius" ),

	DEFINE_KEYFIELD( m_flConsumeRadius, FIELD_FLOAT, "consume_radius" ),

	DEFINE_FIELD( m_flStartTime, FIELD_TIME ),
	DEFINE_KEYFIELD( m_flPullFadeTime, FIELD_FLOAT, "pull_fade_in" ),

	DEFINE_FIELD( m_hReleaseEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hThrower, FIELD_EHANDLE ),

	DEFINE_UTLMAP( m_ClassMass, FIELD_STRING, FIELD_FLOAT ),
	DEFINE_UTLMAP( m_ModelMass, FIELD_STRING, FIELD_FLOAT ),
	DEFINE_UTLMAP( m_HullMap, FIELD_VECTOR, FIELD_INTEGER ),

	DEFINE_UTLMAP( m_SpawnList, FIELD_STRING, FIELD_STRING ),
	DEFINE_FIELD( m_iCurSpawned, FIELD_INTEGER ),

	DEFINE_FIELD( m_iSuckedProps, FIELD_INTEGER ),
	DEFINE_FIELD( m_iSuckedNPCs, FIELD_INTEGER ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Detonate", InputDetonate ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "FakeSpawnEntity", InputFakeSpawnEntity ),
	DEFINE_INPUTFUNC( FIELD_VOID, "CreateXenLife", InputCreateXenLife ),

	DEFINE_OUTPUT( m_OnPullFinished, "OnPullFinished" ),
	DEFINE_OUTPUT( m_OutEntity, "OutEntity" ),
#else
	DEFINE_FIELD( m_flMass, FIELD_FLOAT ),
	DEFINE_FIELD( m_flEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_flRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flStrength, FIELD_FLOAT ),
#endif

	DEFINE_THINKFUNC( PullThink ),
#ifdef EZ2
	DEFINE_THINKFUNC( SpawnThink ),
	DEFINE_THINKFUNC( SchlorpThink ),
#endif
END_DATADESC()

LINK_ENTITY_TO_CLASS( vortex_controller, CGravityVortexController );

#ifdef EZ2
#define GRENADE_MODEL_CLOSED	"models/weapons/w_XenGrenade.mdl" // was roller.mdl
#define GRENADE_MODEL_OPEN		"models/weapons/w_XenGrenade.mdl" // was roller_spikes.mdl

#define XEN_GRENADE_BLIP_FREQUENCY 0.5f
#define XEN_GRENADE_BLIP_FAST_FREQUENCY 0.175f
#define XEN_GRENADE_SPRITE_INTERVAL 0.1f

#define XEN_GRENADE_WARN_TIME 0.7f

static const char *g_SpriteOffContext = "SpriteOff";
#else
#define GRENADE_MODEL_CLOSED	"models/roller.mdl"
#define GRENADE_MODEL_OPEN		"models/roller_spikes.mdl"
#endif

BEGIN_DATADESC( CGrenadeHopwire )
	DEFINE_FIELD( m_hVortexController, FIELD_EHANDLE ),

#ifdef EZ2
	DEFINE_FIELD( m_pMainGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flNextBlipTime, FIELD_TIME ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTimer", InputSetTimer ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DetonateImmediately", InputDetonateImmediately),

	DEFINE_THINKFUNC( DelayThink ),
	DEFINE_THINKFUNC( SpriteOff ),
#endif

	DEFINE_THINKFUNC( EndThink ),
	DEFINE_THINKFUNC( CombatThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_grenade_hopwire, CGrenadeHopwire );

IMPLEMENT_SERVERCLASS_ST( CGrenadeHopwire, DT_GrenadeHopwire )
END_SEND_TABLE()

#ifdef EZ2
BEGIN_DATADESC( CStasisVortexController )

DEFINE_THINKFUNC( PullThink ),
DEFINE_THINKFUNC( UnfreezePhysicsObjectThink ),
DEFINE_THINKFUNC( UnfreezeNPCThink ),

DEFINE_FIELD( m_bFreezingPlayer, FIELD_BOOLEAN )

END_DATADESC()

LINK_ENTITY_TO_CLASS( stasis_controller, CStasisVortexController );

BEGIN_DATADESC( CGrenadeStasis )
DEFINE_THINKFUNC( EndThink ),
DEFINE_THINKFUNC( CombatThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_grenade_stasis, CGrenadeStasis );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopwire::Spawn( void )
{
	if (szWorldModelClosed == NULL || szWorldModelClosed[0] == '\0') {
		DevMsg("Warning: Missing primary hopwire model, using placeholder models \n");
		SetWorldModelClosed(GRENADE_MODEL_CLOSED);
		SetWorldModelOpen(GRENADE_MODEL_OPEN);
	} else if (szWorldModelOpen == NULL || szWorldModelOpen[0] == '\0') {
		DevMsg("Warning: Missing secondary hopwire model, using primary model as secondary \n");
		SetWorldModelOpen(szWorldModelClosed);
	}

	Precache();

	SetModel( szWorldModelClosed );
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	
	CreateVPhysics();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGrenadeHopwire::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, 0, false );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopwire::Precache( void )
{
#ifndef EZ2
	// FIXME: Replace
	PrecacheScriptSound("NPC_Strider.Charge");
	PrecacheScriptSound("NPC_Strider.Shoot");
	//PrecacheSound("d3_citadel.weapon_zapper_beam_loop2");
#endif

	PrecacheModel( szWorldModelOpen );
	PrecacheModel( szWorldModelClosed );

#ifndef EZ2	
	PrecacheModel( DENSE_BALL_MODEL );
#else
	switch (GetHopwireStyle())
	{
	case HOPWIRE_STASIS:
		PrecacheScriptSound("WeaponStasisGrenade.Explode");
		PrecacheScriptSound("WeaponStasisGrenade.Blip");
		PrecacheScriptSound("WeaponStasisGrenade.Hop");
		break;
	default:
		PrecacheScriptSound("WeaponXenGrenade.Explode");
		PrecacheScriptSound("WeaponXenGrenade.SpawnXenPC");
		PrecacheScriptSound("WeaponXenGrenade.Blip");
		PrecacheScriptSound("WeaponXenGrenade.Hop");

		PrecacheScriptSound("WeaponXenGrenade.Schlorp_Huge");
		PrecacheScriptSound("WeaponXenGrenade.Schlorp_Large");
		PrecacheScriptSound("WeaponXenGrenade.Schlorp_Medium");
		PrecacheScriptSound("WeaponXenGrenade.Schlorp_Small");
		PrecacheScriptSound("WeaponXenGrenade.Schlorp_Tiny");
		break;
	}



	PrecacheParticleSystem( "xenpc_spawn" );

	PrecacheMaterial( "sprites/rollermine_shock.vmt" );

	// Interactions
	if (g_interactionXenGrenadePull == 0)
	{
		g_interactionXenGrenadePull = CBaseCombatCharacter::GetInteractionID();
		g_interactionXenGrenadeConsume = CBaseCombatCharacter::GetInteractionID();
		g_interactionXenGrenadeRelease = CBaseCombatCharacter::GetInteractionID();
		g_interactionXenGrenadeCreate = CBaseCombatCharacter::GetInteractionID();
		g_interactionXenGrenadeHop = CBaseCombatCharacter::GetInteractionID();
		g_interactionXenGrenadeRagdoll = CBaseCombatCharacter::GetInteractionID();

		g_interactionStasisGrenadeFreeze = CBaseCombatCharacter::GetInteractionID();
		g_interactionStasisGrenadeUnfreeze = CBaseCombatCharacter::GetInteractionID();
	}

	if (GetHopwireStyle() == HOPWIRE_XEN)
		VerifyXenRecipeManager( GetClassname() );
#endif

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : timer - 
//-----------------------------------------------------------------------------
#ifdef EZ2
void CGrenadeHopwire::SetTimer( float timer )
{
	m_flDetonateTime = gpGlobals->curtime + timer;
	m_flWarnAITime = gpGlobals->curtime + (timer - XEN_GRENADE_WARN_TIME);
	m_flNextBlipTime = gpGlobals->curtime;
	SetThink( &CGrenadeHopwire::DelayThink );
	SetNextThink( gpGlobals->curtime );

	CreateEffects();
}

void CGrenadeHopwire::InputSetTimer( inputdata_t &inputdata )
{
	SetTimer( inputdata.value.Float() );
}

void CGrenadeHopwire::DelayThink()
{
#ifdef EZ2
	int i_ColorRed = 255;
	int i_ColorGreen = 255;
	int i_ColorBlue = 255;
	int i_ColorAlpha = 255;

	switch (GetHopwireStyle())
	{
		case HOPWIRE_STASIS:
			i_ColorRed = 0;
			i_ColorGreen = 200;
			break;
		default:
			i_ColorRed = 0;
			i_ColorBlue = 0;
			break;
	}

#endif

	if( gpGlobals->curtime > m_flDetonateTime )
	{
		if (m_pMainGlow)
		{
#ifndef EZ2
			m_pMainGlow->SetTransparency( kRenderTransAdd, 0, 255, 0, 255, kRenderFxNoDissipation );
#else
			m_pMainGlow->SetTransparency( kRenderTransAdd, i_ColorRed, i_ColorGreen, i_ColorBlue, i_ColorAlpha, kRenderFxNoDissipation );
#endif
			m_pMainGlow->SetBrightness( 255 );
			m_pMainGlow->TurnOn();
			SetContextThink( NULL, TICK_NEVER_THINK, g_SpriteOffContext );
		}

		Detonate();
		return;
	}

	if( !m_bHasWarnedAI && gpGlobals->curtime >= m_flWarnAITime )
	{
		CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this );
		m_bHasWarnedAI = true;
	}
	
	if( gpGlobals->curtime > m_flNextBlipTime )
	{
		BlipSound();

		if (m_pMainGlow)
		{
			m_pMainGlow->TurnOn();
			SetContextThink( &CGrenadeHopwire::SpriteOff, gpGlobals->curtime + XEN_GRENADE_SPRITE_INTERVAL, g_SpriteOffContext );
		}
		
		if( m_bHasWarnedAI )
		{
			m_flNextBlipTime = gpGlobals->curtime + XEN_GRENADE_BLIP_FAST_FREQUENCY;
		}
		else
		{
			m_flNextBlipTime = gpGlobals->curtime + XEN_GRENADE_BLIP_FREQUENCY;
		}
	}

	SetNextThink( gpGlobals->curtime + 0.05 );
}

void CGrenadeHopwire::SpriteOff()
{
	if (m_pMainGlow)
		m_pMainGlow->TurnOff();
}

void CGrenadeHopwire::BlipSound()
{
	switch (GetHopwireStyle())
	{
	case HOPWIRE_STASIS:
		EmitSound("WeaponStasisGrenade.Blip");
		break;
	default:
		EmitSound("WeaponXenGrenade.Blip");
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopwire::OnRestore( void )
{
	// If we were primed and ready to detonate, put FX on us.
	if (m_flDetonateTime > 0)
		CreateEffects();

	BaseClass::OnRestore();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopwire::CreateEffects( void )
{
#ifdef EZ2
	int i_ColorRed = 255;
	int i_ColorGreen = 255;
	int i_ColorBlue = 255;
	int i_ColorAlpha = 255;

	switch (GetHopwireStyle())
	{
	case HOPWIRE_STASIS:
		i_ColorRed = 0;
		i_ColorGreen = 200;
		break;
	default:
		i_ColorRed = 0;
		break;
	}

#endif

	// Start up the eye glow
	m_pMainGlow = CSprite::SpriteCreate( "sprites/redglow2.vmt", GetLocalOrigin(), false );

	int	nAttachment = LookupAttachment( "fuse" );

	if ( m_pMainGlow != NULL )
	{
		m_pMainGlow->FollowEntity( this );
		m_pMainGlow->SetAttachment( this, nAttachment );

#ifndef EZ2
		m_pMainGlow->SetTransparency( kRenderGlow, 0, 255, 255, 255, kRenderFxNoDissipation );
#else
		m_pMainGlow->SetTransparency( kRenderGlow, i_ColorRed, i_ColorGreen, i_ColorBlue, i_ColorAlpha, kRenderFxNoDissipation );
#endif

		m_pMainGlow->SetBrightness( 192 );
		m_pMainGlow->SetScale( 0.5f );
		m_pMainGlow->SetGlowProxySize( 4.0f );
	}
}
#else
void CGrenadeHopwire::SetTimer( float timer )
{
	SetThink( &CBaseGrenade::PreDetonate );
	SetNextThink( gpGlobals->curtime + timer );
}
#endif

#define	MAX_STRIDER_KILL_DISTANCE_HORZ	(hopwire_strider_kill_dist_h.GetFloat())		// Distance a Strider will be killed if within
#define	MAX_STRIDER_KILL_DISTANCE_VERT	(hopwire_strider_kill_dist_v.GetFloat())		// Distance a Strider will be killed if within

#define MAX_STRIDER_STUN_DISTANCE_HORZ	(MAX_STRIDER_KILL_DISTANCE_HORZ*2)	// Distance a Strider will be stunned if within
#define MAX_STRIDER_STUN_DISTANCE_VERT	(MAX_STRIDER_KILL_DISTANCE_VERT*2)	// Distance a Strider will be stunned if within

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopwire::KillStriders( void )
{
	CBaseEntity *pEnts[128];
	Vector	mins, maxs;

	ClearBounds( mins, maxs );
	AddPointToBounds( -Vector( MAX_STRIDER_STUN_DISTANCE_HORZ, MAX_STRIDER_STUN_DISTANCE_HORZ, MAX_STRIDER_STUN_DISTANCE_HORZ ), mins, maxs );
	AddPointToBounds(  Vector( MAX_STRIDER_STUN_DISTANCE_HORZ, MAX_STRIDER_STUN_DISTANCE_HORZ, MAX_STRIDER_STUN_DISTANCE_HORZ ), mins, maxs );
	AddPointToBounds( -Vector( MAX_STRIDER_STUN_DISTANCE_VERT, MAX_STRIDER_STUN_DISTANCE_VERT, MAX_STRIDER_STUN_DISTANCE_VERT ), mins, maxs );
	AddPointToBounds(  Vector( MAX_STRIDER_STUN_DISTANCE_VERT, MAX_STRIDER_STUN_DISTANCE_VERT, MAX_STRIDER_STUN_DISTANCE_VERT ), mins, maxs );

	// FIXME: It's probably much faster to simply iterate over the striders in the map, rather than any entity in the radius - jdw

	// Find any striders in range of us
	int numTargets = UTIL_EntitiesInBox( pEnts, ARRAYSIZE( pEnts ), GetAbsOrigin()+mins, GetAbsOrigin()+maxs, FL_NPC );
	float targetDistHorz, targetDistVert;

	for ( int i = 0; i < numTargets; i++ )
	{
		// Only affect striders
		if ( FClassnameIs( pEnts[i], "npc_strider" ) == false )
			continue;

		// We categorize our spatial relation to the strider in horizontal and vertical terms, so that we can specify both parameters separately
		targetDistHorz = UTIL_DistApprox2D( pEnts[i]->GetAbsOrigin(), GetAbsOrigin() );
		targetDistVert = fabs( pEnts[i]->GetAbsOrigin()[2] - GetAbsOrigin()[2] );

		if ( targetDistHorz < MAX_STRIDER_KILL_DISTANCE_HORZ && targetDistHorz < MAX_STRIDER_KILL_DISTANCE_VERT )
		{
			// Kill the strider
			float fracDamage = ( pEnts[i]->GetMaxHealth() / hopwire_strider_hits.GetFloat() ) + 1.0f;
			CTakeDamageInfo killInfo( this, this, fracDamage, DMG_GENERIC );
			Vector	killDir = pEnts[i]->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize( killDir );

			killInfo.SetDamageForce( killDir * -1000.0f );
			killInfo.SetDamagePosition( GetAbsOrigin() );

			pEnts[i]->TakeDamage( killInfo );
		}
		else if ( targetDistHorz < MAX_STRIDER_STUN_DISTANCE_HORZ && targetDistHorz < MAX_STRIDER_STUN_DISTANCE_VERT )
		{
			// Stun the strider
			CTakeDamageInfo killInfo( this, this, 200.0f, DMG_GENERIC );
			pEnts[i]->TakeDamage( killInfo );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopwire::EndThink( void )
{
	if ( hopwire_vortex.GetBool() )
	{
		EntityMessageBegin( this, true );
			WRITE_BYTE( 1 );
		MessageEnd();
	}

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopwire::CombatThink( void )
{
#ifdef EZ2
	if ( VPhysicsGetObject() && VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
	{
		// Players must stop holding us here
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		pPlayer->ForceDropOfCarriedPhysObjects( this );
	}
#endif

	// Stop the grenade from moving
	AddEFlags( EF_NODRAW );
	AddFlag( FSOLID_NOT_SOLID );
	VPhysicsDestroyObject();
	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );

#ifndef EZ2 // No special behavior for striders in EZ2 - wouldn't want to Xen Grenade our allies!
	// Do special behaviors if there are any striders in the area
	KillStriders();

	// FIXME: Replace
	EmitSound("NPC_Strider.Charge"); // Sound to emit during detonation
	//EmitSound("d3_citadel.weapon_zapper_beam_loop2");
#endif

	// Quick screen flash
	CBasePlayer *pPlayer = ToBasePlayer( GetThrower() );
#ifdef EZ2
	color32 green = { 32,252,0,100 }; // my bits Xen Green - was 255,255,255,255
	UTIL_ScreenFade( pPlayer, green, 0.2f, 0.0f, FFADE_IN );
#else
	color32 white = { 255,255,255,255 };
	UTIL_ScreenFade( pPlayer, white, 0.2f, 0.0f, FFADE_IN );
#endif

	// Create the vortex controller to pull entities towards us
	if ( hopwire_vortex.GetBool() )
	{
#ifdef EZ2
		m_hVortexController = CGravityVortexController::Create( GetAbsOrigin(), hopwire_radius.GetFloat(), hopwire_strength.GetFloat(), hopwire_duration.GetFloat(), this );
#else
		m_hVortexController = CGravityVortexController::Create( GetAbsOrigin(), hopwire_radius.GetFloat(), hopwire_strength.GetFloat(), hopwire_duration.GetFloat() );
#endif

		// Start our client-side effect
		EntityMessageBegin( this, true );
			WRITE_BYTE( 0 );
		MessageEnd();
		
		// Begin to stop in two seconds
		SetThink( &CGrenadeHopwire::EndThink );
		SetNextThink( gpGlobals->curtime + 2.0f );
	}
	else
	{
		// Remove us immediately
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeHopwire::SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	
	if ( pPhysicsObject != NULL )
	{
		pPhysicsObject->AddVelocity( &velocity, &angVelocity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hop off the ground to start deployment
//-----------------------------------------------------------------------------
void CGrenadeHopwire::Detonate( void )
{
#ifdef EZ2
	if ( GetThrower() )
	{
		// Tell the thrower we're hopping
		GetThrower()->DispatchInteraction( g_interactionXenGrenadeHop, this, GetThrower() );
	}

	switch (GetHopwireStyle())
	{
		case HOPWIRE_STASIS:
			SetModel(szWorldModelOpen);

			EmitSound("WeaponStasisGrenade.Hop");
			break;
		default:
			EmitSound("WeaponXenGrenade.Explode");
			SetModel(szWorldModelOpen);

			EmitSound("WeaponXenGrenade.Hop");
	}

	//Find out how tall the ceiling is and always try to hop halfway
	trace_t	tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, MAX_HOP_HEIGHT*2 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	// Jump half the height to the found ceiling
	float hopHeight = MIN( MAX_HOP_HEIGHT, (MAX_HOP_HEIGHT*tr.fraction) );
	hopHeight =	MAX(hopHeight, MIN_HOP_HEIGHT);

	// Go through each node and find a link we should hop to.
	Vector vecHopTo = vec3_invalid;
	for ( int node = 0; node < g_pBigAINet->NumNodes(); node++)
	{
		CAI_Node *pNode = g_pBigAINet->GetNode(node);

		if ((GetAbsOrigin() - pNode->GetOrigin()).LengthSqr() > Square(192.0f))
			continue;

		// Only hop towards ground nodes
		if (pNode->GetType() != NODE_GROUND)
			continue;

		// Iterate through these hulls to find the largest possible space we can hop to.
		// Should be sorted largest to smallest.
		static Hull_t iHopHulls[] =
		{
			HULL_LARGE_CENTERED,
			HULL_MEDIUM,
			HULL_HUMAN,
		};

		// The "3" here should be kept up-to-date with iHopHulls
		for (int hull = 0; hull < 3; hull++)
		{
			Hull_t iHull = iHopHulls[hull];
			CAI_Link *pLink = NULL;
			float flNearestSqr = Square(192.0f);
			int iNearestLink = -1;
			for (int i = 0; i < pNode->NumLinks(); i++)
			{
				pLink = pNode->GetLinkByIndex( i );
				if (pLink)
				{
					if (pLink->m_iAcceptedMoveTypes[iHull] & bits_CAP_MOVE_GROUND)
					{
						int iOtherID = node == pLink->m_iSrcID ? pLink->m_iDestID : pLink->m_iSrcID;
						CAI_Node *pOtherNode = g_pBigAINet->GetNode(iOtherID);
						Vector vecBetween = (pNode->GetOrigin() - pOtherNode->GetOrigin()) * 0.5f + (pOtherNode->GetOrigin());
						float flDistSqr = (GetAbsOrigin() - vecBetween).LengthSqr();

						if (flDistSqr > flNearestSqr)
							continue;

						UTIL_TraceLine( GetAbsOrigin() + Vector( 0, 0, hopHeight ), vecBetween, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
						if (tr.fraction == 1.0f)
						{
							vecHopTo = vecBetween;
							flNearestSqr = (GetAbsOrigin() - vecBetween).LengthSqr();
							iNearestLink = i;

							// DEBUG
							//DebugDrawLine(GetAbsOrigin(), vecHopTo, 100 * hull, 0, 255, false, 2.0f);
						}
					}
				}
			}

			// We found a link
			if (iNearestLink != -1)
				break;
		}
	}

	AngularImpulse hopAngle = RandomAngularImpulse( -300, 300 );

	// Hop towards the point we found
	Vector hopVel( 0.0f, 0.0f, hopHeight );
	if (vecHopTo != vec3_invalid)
	{
		// DEBUG
		//DebugDrawLine(GetAbsOrigin(), vecHopTo, 0, 255, 0, false, 3.0f);

		Vector vecDelta = (vecHopTo - GetAbsOrigin());
		float flLength = vecDelta.Length();
		if (flLength != 0.0f)
			hopVel += (vecDelta * (64.0f / flLength));
	}

	SetVelocity( hopVel, hopAngle );

	// Get the time until the apex of the hop
	float apexTime = sqrt( hopHeight / GetCurrentGravity() );

	// If this is a stasis grenade, use the stasis grenade think function to detonate
	if (GetHopwireStyle() == HOPWIRE_STASIS)
	{
		SetThink( &CGrenadeStasis::CombatThink );
		SetNextThink( gpGlobals->curtime + apexTime );
		return;
	}
#else
	EmitSound("NPC_Strider.Shoot"); // Sound to emit before detonating
	SetModel( szWorldModelOpen );

	AngularImpulse	hopAngle = RandomAngularImpulse( -300, 300 );

	//Find out how tall the ceiling is and always try to hop halfway
	trace_t	tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, MAX_HOP_HEIGHT*2 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	// Jump half the height to the found ceiling
	float hopHeight = MIN( MAX_HOP_HEIGHT, (MAX_HOP_HEIGHT*tr.fraction) );
	hopHeight =	MAX(hopHeight, MIN_HOP_HEIGHT);

	//Add upwards velocity for the "hop"
	Vector hopVel( 0.0f, 0.0f, hopHeight );
	SetVelocity( hopVel, hopAngle );

	// Get the time until the apex of the hop
	float apexTime = sqrt( hopHeight / GetCurrentGravity() );
#endif

	// Explode at the apex
	SetThink( &CGrenadeHopwire::CombatThink );
	SetNextThink( gpGlobals->curtime + apexTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseGrenade *HopWire_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer, const char * modelClosed, const char * modelOpen)
{
	// Fall back to a default model if none specified
	static const char *szHopwireModel = "models/weapons/w_xengrenade.mdl";
	if (modelClosed == NULL)
		modelClosed = szHopwireModel;
	if (modelOpen == NULL)
		modelOpen = szHopwireModel;

	CGrenadeHopwire *pGrenade = (CGrenadeHopwire *)CBaseEntity::CreateNoSpawn( "npc_grenade_hopwire", position, angles, pOwner ); // Don't spawn the hopwire until models are set!
	pGrenade->SetWorldModelClosed(modelClosed);
	pGrenade->SetWorldModelOpen(modelOpen);

	DispatchSpawn(pGrenade);

	// Only set ourselves to detonate on a timer if we're not a trap hopwire
	if ( hopwire_trap.GetBool() == false )
	{
		pGrenade->SetTimer( timer );
	}

	pGrenade->SetVelocity( velocity, angVelocity );
	pGrenade->SetThrower( ToBaseCombatCharacter( pOwner ) );

	return pGrenade;
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseGrenade *StasisGrenade_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer, const char * modelClosed, const char * modelOpen )
{
	// Fall back to a default model if none specified
	static const char *szHopwireModel = "models/weapons/w_xengrenade.mdl";
	if (modelClosed == NULL)
		modelClosed = szHopwireModel;
	if (modelOpen == NULL)
		modelOpen = szHopwireModel;

	CGrenadeStasis *pGrenade = (CGrenadeStasis *)CBaseEntity::CreateNoSpawn( "npc_grenade_stasis", position, angles, pOwner ); // Don't spawn the hopwire until models are set!
	pGrenade->SetWorldModelClosed( modelClosed );
	pGrenade->SetWorldModelOpen( modelOpen );

	DispatchSpawn( pGrenade );

	// Only set ourselves to detonate on a timer if we're not a trap hopwire
	if (hopwire_trap.GetBool() == false)
	{
		pGrenade->SetTimer( timer );
	}

	pGrenade->SetVelocity( velocity, angVelocity );
	pGrenade->SetThrower( ToBaseCombatCharacter( pOwner ) );

	return pGrenade;
}


//-----------------------------------------------------------------------------
// Purpose: Creation utility
//-----------------------------------------------------------------------------
CStasisVortexController *CStasisVortexController::Create( const Vector &origin, float radius, float strength, float duration, CBaseEntity *pGrenade )
{
	// Create an instance of the vortex
	CStasisVortexController *pVortex = (CStasisVortexController *)CreateEntityByName( "stasis_controller" );
	if (pVortex == NULL)
		return NULL;

	pVortex->SetOwnerEntity( pGrenade );
	if (pGrenade)
		pVortex->SetThrower( static_cast<CGrenadeHopwire*>(pGrenade)->GetThrower() );

	pVortex->SetNodeRadius( hopwire_spawn_node_radius.GetFloat() );
	pVortex->SetConsumeRadius( hopwire_conusme_radius.GetFloat() );

	// Start the vortex working
	pVortex->StartPull( origin, radius, strength, duration );

	trace_t	tr;
	AI_TraceLine(origin + Vector(0, 0, 1), origin - Vector(0, 0, 128), MASK_SOLID_BRUSHONLY, pVortex, COLLISION_GROUP_NONE, &tr);

	UTIL_DecalTrace(&tr, "StasisGrenade.Splash");

	return pVortex;
}


//-----------------------------------------------------------------------------
// Purpose: Holds everything in a stasis field
//-----------------------------------------------------------------------------
void CStasisVortexController::PullThink( void )
{
	float flStrength = m_flStrength;
	CRagdollProp *pRagdoll = NULL;

	// Pull any players close enough to us
	FreezePlayersInRange();

	// Draw debug information
	if (g_debug_hopwire.GetInt() >= 2)
	{
		NDebugOverlay::Sphere( GetAbsOrigin(), m_flRadius, 0, 255, 0, 16, 4.0f );
	}

	CBaseEntity *pEnts[128];
	int numEnts = 0;

	// Next, loop through all entities within the pull radius
	numEnts = UTIL_EntitiesInSphere( pEnts, 128, GetAbsOrigin(), m_flRadius, 0 );

	if (!m_bPVSCreated)
	{
		// Create a PVS for this entity
		engine->GetPVSForCluster( engine->GetClusterForOrigin( GetAbsOrigin() ), sizeof( m_PVS ), m_PVS );
	}

	if (m_flPullFadeTime > 0.0f)
	{
		flStrength *= ((gpGlobals->curtime - m_flStartTime) / m_flPullFadeTime);
	}

	for (int i = 0; i < numEnts; i++)
	{
		if (pEnts[i]->IsPlayer())
			continue;

		IPhysicsObject *pPhysObject = NULL;

		// Don't consume entities already in the process of being removed.
		// NPCs which generate ragdolls might not be removed in time, so check for EF_NODRAW as well.
		if (pEnts[i]->IsMarkedForDeletion() || pEnts[i]->IsEffectActive( EF_NODRAW ))
			continue;

		// Don't pull objects that are protected
		if (pEnts[i]->IsDisplacementImpossible())
			continue;

		const Vector& vecEntCenter = pEnts[i]->WorldSpaceCenter();

		// We do a PVS check here to make sure the entity isn't on another floor or behind a thick wall.
		if (!engine->CheckOriginInPVS( vecEntCenter, m_PVS, sizeof( m_PVS ) ))
			continue;

		// Dispatch interaction. Skip freezing if it returns true
		if (pEnts[i]->DispatchInteraction( g_interactionStasisGrenadeFreeze, NULL, GetThrower() ))
		{
			continue;
		}

		// Reset ragdoll
		pRagdoll = NULL;

		// Freeze NPCs
		if (pEnts[i]->MyNPCPointer())
		{
			// If this NPC is in the freeze schedule, set it's think to the end of the vortex
			if (pEnts[i]->MyNPCPointer()->IsCurSchedule(SCHED_NPC_FREEZE, false))
			{
				pEnts[i]->SetNextThink(MAX(pEnts[i]->GetNextThink(), m_flEndTime - TICK_INTERVAL));
				pEnts[i]->SetAbsVelocity(vec3_origin);
				continue;
			}

			pEnts[i]->MyNPCPointer()->SetCondition(COND_NPC_FREEZE);
			pEnts[i]->MyNPCPointer()->TaskInterrupt();

			// Add tick interval to the unfreeze time to make sure that the unfreeze is after the end time
			pEnts[i]->SetContextThink(&CStasisVortexController::UnfreezeNPCThink, m_flEndTime + TICK_INTERVAL, "StasisGrenadeUnfreeze");

			continue;
		}

		if (FClassnameIs( pEnts[i], "prop_ragdoll" ))
		{
			pRagdoll = dynamic_cast< CRagdollProp* >(pEnts[i]);
		}

		if (pRagdoll && pRagdoll->IsMotionEnabled())
		{
			pRagdoll->DisableMotion();
			// Add tick interval to the unfreeze time to make sure that the unfreeze is after the end time
			pEnts[i]->SetContextThink( &CStasisVortexController::UnfreezePhysicsObjectThink, m_flEndTime + TICK_INTERVAL, "StasisGrenadeUnfreeze" );
			continue;
		}

		// If this is not an NPC, get the physics object
		pPhysObject = pEnts[i]->VPhysicsGetObject();
		if (pPhysObject == NULL)
		{
			continue;
		}

		if (!pPhysObject->IsMotionEnabled())
			continue;

		// TODO - we should consider separating contexts for gravity and motion so that they can be toggled independently
		if (!pPhysObject->IsGravityEnabled())
			continue;

		pPhysObject->EnableMotion(false);
		pPhysObject->EnableGravity( false );
		// Add tick interval to the unfreeze time to make sure that the unfreeze is after the end time
		pEnts[i]->SetContextThink( &CStasisVortexController::UnfreezePhysicsObjectThink, m_flEndTime + TICK_INTERVAL, "StasisGrenadeUnfreeze" );
	}

	// Keep going if need-be
	if (m_flEndTime > gpGlobals->curtime)
	{
		SetThink( &CStasisVortexController::PullThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		// Fire game event for player xen grenades
		IGameEvent *event = gameeventmanager->CreateEvent( "stasis_grenade" );
		if (event)
		{
			if (GetThrower())
			{
				event->SetInt( "entindex_attacker", GetThrower()->entindex() );
			}

			// event->SetFloat( "mass", m_flMass );
			gameeventmanager->FireEvent( event );
		}

		m_OnPullFinished.FireOutput( this, this );

		if (!HasSpawnFlags( SF_VORTEX_CONTROLLER_DONT_REMOVE ))
		{
			// Remove at the maximum possible time of when we would be finished spawning entities
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( gpGlobals->curtime + (m_SpawnList.Count() * hopwire_spawn_interval_max.GetFloat() + 1.0f) );
			XenGrenadeDebugMsg( "REMOVE TIME: Count %i * interval %f + 1.0 = %f\n", m_SpawnList.Count(), hopwire_spawn_interval_max.GetFloat(), (m_SpawnList.Count() * hopwire_spawn_interval_max.GetFloat() + 1.0f) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think function to unfreeze an efntity
//-----------------------------------------------------------------------------
void CStasisVortexController::UnfreezeNPCThink( void )
{
	if (MyNPCPointer() == NULL)
		return;

	// Dispatch interaction. Skip unfreezing if it returns true
	if (MyNPCPointer()->DispatchInteraction(g_interactionStasisGrenadeUnfreeze, NULL, GetThrower()))
	{
		return;
	}

	MyNPCPointer()->ClearCondition( COND_NPC_FREEZE );
	MyNPCPointer()->SetCondition( COND_NPC_UNFREEZE );
}

//-----------------------------------------------------------------------------
// Purpose: Think function to unfreeze an efntity
//-----------------------------------------------------------------------------
void CStasisVortexController::UnfreezePhysicsObjectThink( void )
{
	CRagdollProp *pRagdoll = NULL;

	SetContextThink( NULL, TICK_NEVER_THINK, "StasisGrenadeUnfreeze" );

	if (FClassnameIs( GetBaseEntity(), "prop_ragdoll"))
	{
		pRagdoll = dynamic_cast< CRagdollProp* >(GetBaseEntity());
	}

	if (pRagdoll)
	{
		inputdata_t ragdollData;
		ragdollData.value.SetFloat(0.1f);
		pRagdoll->InputEnableMotion(ragdollData);
		pRagdoll->InputStartRadgollBoogie(ragdollData);
		return;
	}

	IPhysicsObject *pPhysObject = VPhysicsGetObject();
	if (pPhysObject == NULL)
	{
		return;
	}

	pPhysObject->EnableMotion( true );
	pPhysObject->EnableGravity( true );
	pPhysObject->Wake();
}

//-----------------------------------------------------------------------------
// Purpose: Starts the vortex working
//-----------------------------------------------------------------------------
void CStasisVortexController::StartPull( const Vector &origin, float radius, float strength, float duration )
{
	SetAbsOrigin( origin );
	m_flEndTime	= gpGlobals->curtime + duration;
	m_flRadius	= radius;
	m_flStrength= strength;

	// Play a danger sound throughout the duration of the vortex so that NPCs run away
	CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin(), radius * 1.25f, duration * 1.25f, this );

	SetDefLessFunc( m_SpawnList );
	m_SpawnList.EnsureCapacity( 16 );

	m_flStartTime = gpGlobals->curtime;

	SetThink( &CStasisVortexController::PullThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

#define STASIS_MOVEMENT_SPEED 0.101123f // HACKHACK - the speed value is something no one would use in a speedmod

//-----------------------------------------------------------------------------
// Purpose: Causes players within the radius to be frozen
//-----------------------------------------------------------------------------
void CStasisVortexController::FreezePlayersInRange( void )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (!pPlayer || !pPlayer->VPhysicsGetObject())
		return;

	// If we are currently freezing the player, reset for this function
	if (m_bFreezingPlayer && pPlayer->GetLaggedMovementValue() == STASIS_MOVEMENT_SPEED)
	{
		pPlayer->SetLaggedMovementValue(1.0f);
	}

	m_bFreezingPlayer = false;

	// Don't try to freeze the player if the vortex is collapsing
	if (m_flEndTime <= gpGlobals->curtime)
		return;

	Vector	vecForce = GetAbsOrigin() - pPlayer->WorldSpaceCenter();
	float	dist = VectorNormalize( vecForce );

	// FIXME: Need a more deterministic method here
	if (dist < 128.0f)
	{
		// Kill the player (with falling death sound and effects)
		CTakeDamageInfo deathInfo( this, this, GetAbsOrigin(), GetAbsOrigin(), 5, DMG_FALL );

		pPlayer->TakeDamage( deathInfo );

		if (pPlayer->IsAlive() == false)
		{
			color32 blue ={ 0, 150, 252, 255 };
			UTIL_ScreenFade( pPlayer, blue, 0.1f, 0.0f, (FFADE_OUT|FFADE_STAYOUT) );
			return;
		}
	}

	// Must be within the radius
	if (dist > m_flRadius)
		return;

	// Don't pull unless convar set
	if (!stasis_freeze_player.GetBool())
		return;

	// Don't pull noclipping players
	if (pPlayer->GetMoveType() == MOVETYPE_NOCLIP)
		return;

	if (pPlayer->GetLaggedMovementValue() == 1.0f)
	{
		m_bFreezingPlayer = true;
		pPlayer->SetLaggedMovementValue(STASIS_MOVEMENT_SPEED);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeStasis::EndThink( void )
{
	EntityMessageBegin( this, true );
	WRITE_BYTE( 1 );
	MessageEnd();

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadeStasis::CombatThink( void )
{
	if (VPhysicsGetObject() && VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD)
	{
		// Players must stop holding us here
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		pPlayer->ForceDropOfCarriedPhysObjects( this );
	}

	// Stop the grenade from moving
	AddEFlags( EF_NODRAW );
	AddFlag( FSOLID_NOT_SOLID );
	VPhysicsDestroyObject();
	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );

	// Stasis grenades play explosion sound when the vortex is created
	EmitSound("WeaponStasisGrenade.Explode");

	m_hVortexController = CStasisVortexController::Create( GetAbsOrigin(), stasis_radius.GetFloat(), stasis_strength.GetFloat(), stasis_duration.GetFloat(), this );

	// Start our client-side effect
	EntityMessageBegin( this, true );
	WRITE_BYTE( 3 );
	MessageEnd();

	// Begin to stop in two seconds
	SetThink( &CGrenadeStasis::EndThink );
	SetNextThink( gpGlobals->curtime + 2.0f );
}
#endif