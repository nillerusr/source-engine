//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== tf_playerclass.cpp ========================================================

  functions dealing with the TF playerclasses

*/
#include "cbase.h"
#include "player.h"
#include "basecombatweapon.h"
#include "tf_player.h"
#include "tf_obj.h"
#include "weapon_builder.h"
#include "tf_team.h"
#include "tf_func_resource.h"
#include "orders.h"
#include "order_repair.h"
#include "engine/IEngineSound.h"
#include "tier1/strtools.h"
#include "textstatsmgr.h"
#include "ammodef.h"
#include "weapon_objectselection.h"
#include "vcollide_parse.h"
#include "weapon_combatshield.h"
#include "tf_vehicle_tank.h"
#include "tf_obj_manned_plasmagun.h"
#include "tf_obj_manned_missilelauncher.h"

extern char *g_pszEMPPulseStart;
extern ConVar tf_fastbuild;


// Stat groupings
enum
{
	STATS_TANK = TFCLASS_CLASS_COUNT,
	STATS_MANNEDGUN_PLASMA,
	STATS_MANNEDGUN_ROCKET,

	STATS_NUM_GROUPS,
};

char *sNonClassStatNames[] =
{
	"Tanks",
	"Manned Plasmaguns",
	"Manned Rocket launchers",
};

// Stats between player classes (like how many snipers killed recons).
class CInterClassStats
{
public:
	
			CInterClassStats()
			{
				m_flTotalDamageInflicted = 0;
				m_nKills = 0;

				m_flTotalEngagementDist = 0;
				m_nEngagements = 0;

				m_flTotalNormalizedEngagementDist = 0;
				m_nNormalizedEngagements = 0;
			}

public:
	//
	// Note: "containing class" refers to the class in the CPlayerClassStats that owns this CInterClassStats.
	// So if there's a CPlayerClassStats for a recon, and it has a CInterClassStats for a sniper,
	// then "containing class" refers to the recon. In this example, m_flTotalDamageInflicted representss
	// how much damage the recon players inflicted on the snipers.
	//
	double	m_flTotalDamageInflicted;	// Total damage inflicted on this class by the containing class.
	double	m_flTotalEngagementDist;	// Used to get the average engagement distance.
	int		m_nEngagements;

	double m_flTotalNormalizedEngagementDist;
	int m_nNormalizedEngagements;

	int		m_nKills;					// Now many times the containing class killed this one.
};

class CPlayerClassStats
{
public:
	CPlayerClassStats()
	{
		m_flPlayerTime = 0;
	}

public:
			
	CInterClassStats	m_InterClassStats[STATS_NUM_GROUPS];
	double				m_flPlayerTime;	// How much player time was spent in this class.
};

		
ConVar	tf2_object_hard_limits( "tf2_object_hard_limits","0", FCVAR_NONE, "If true, use hard object limits instead of resource costs" ); 

CPlayerClassStats g_PlayerClassStats[STATS_NUM_GROUPS];

void AddPlayerClassTime( int classnum, float seconds )
{
	g_PlayerClassStats[ classnum ].m_flPlayerTime += seconds;
}

int GetStatGroupFor( CBaseTFPlayer *pPlayer )
{
	// In a vehicle?
	if ( pPlayer->IsInAVehicle() )
	{
		if ( dynamic_cast<CVehicleTank*>( pPlayer->GetVehicle() ) )
			return STATS_TANK;
		if ( dynamic_cast<CObjectMannedMissileLauncher*>( pPlayer->GetVehicle() ) )
			return STATS_MANNEDGUN_ROCKET;
		if ( dynamic_cast<CObjectMannedPlasmagun*>( pPlayer->GetVehicle() ) )
			return STATS_MANNEDGUN_PLASMA;
	}

	// Otherwise, use the playerclass
	return pPlayer->GetPlayerClass()->GetTFClass();
}

const char* GetGroupNameFor( int iStatGroup )
{
	Assert( iStatGroup > TFCLASS_UNDECIDED && iStatGroup < STATS_NUM_GROUPS );
	if ( iStatGroup < TFCLASS_CLASS_COUNT )
		return GetTFClassInfo( iStatGroup )->m_pClassName;
	else
		return sNonClassStatNames[ iStatGroup - TFCLASS_CLASS_COUNT ];
}


void PrintPlayerClassStats()
{
	IFileSystem *pFileSys = filesystem;

	FileHandle_t hFile = pFileSys->Open( "class_stats.txt", "wt", "LOGDIR" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return;


	pFileSys->FPrintf( hFile, "Class\tPlayer Time (minutes)\tAvg Engagement Dist\t(OLD) Engagement Dist\n" );
	for ( int i=TFCLASS_UNDECIDED+1; i < STATS_NUM_GROUPS; i++ )
	{
		CPlayerClassStats *pStats = &g_PlayerClassStats[i];


		// Figure out the average engagement distance across all classes.
		int j;
		double flAvgEngagementDist = 0;
		int nTotalEngagements = 0;
		double flTotalEngagementDist = 0;

		double flTotalNormalizedEngagementDist = 0;
		int nTotalNormalizedEngagements = 0;

		for ( j=TFCLASS_UNDECIDED+1; j < STATS_NUM_GROUPS; j++ )
		{
			CInterClassStats *pInter = &g_PlayerClassStats[i].m_InterClassStats[j];
			
			nTotalEngagements += pInter->m_nEngagements;
			flTotalEngagementDist += pInter->m_flTotalEngagementDist;

			nTotalNormalizedEngagements += pInter->m_nNormalizedEngagements;
			flTotalNormalizedEngagementDist += pInter->m_flTotalNormalizedEngagementDist;
		}
		flAvgEngagementDist = nTotalEngagements ? ( flTotalEngagementDist / nTotalEngagements ) : 0;
		double flAvgNormalizedEngagementDist = nTotalNormalizedEngagements ? (flTotalNormalizedEngagementDist / nTotalNormalizedEngagements) : 0;
		
		
		pFileSys->FPrintf( hFile, "%s",		GetGroupNameFor( i ) );
		pFileSys->FPrintf( hFile, "\t%.1f", (pStats->m_flPlayerTime / 60.0f) );
		pFileSys->FPrintf( hFile, "\t%d",	(int)flAvgNormalizedEngagementDist );
		pFileSys->FPrintf( hFile, "\t%d",	(int)flAvgEngagementDist );
		pFileSys->FPrintf( hFile, "\n" );
	}

	pFileSys->FPrintf( hFile, "\n" );
	pFileSys->FPrintf( hFile, "\n" );


	pFileSys->FPrintf( hFile, "Class\tTarget Class\tTotal Damage\tKills\tAvg Engagement Dist\t(OLD) Engagement Dist\n" );
	
	for ( i=TFCLASS_UNDECIDED+1; i < STATS_NUM_GROUPS; i++ )
	{
		CPlayerClassStats *pStats = &g_PlayerClassStats[i];

		// Print the inter-class stats.
		for ( int j=TFCLASS_UNDECIDED+1; j < STATS_NUM_GROUPS; j++ )
		{
			CInterClassStats *pInter = &pStats->m_InterClassStats[j];

			pFileSys->FPrintf( hFile, "%s", GetGroupNameFor( i ) );
			pFileSys->FPrintf( hFile, "\t%s", GetGroupNameFor( j ) );
			pFileSys->FPrintf( hFile, "\t%d", (int)pInter->m_flTotalDamageInflicted );
			pFileSys->FPrintf( hFile, "\t%d", pInter->m_nKills );
			pFileSys->FPrintf( hFile, "\t%d", (int)(pInter->m_nNormalizedEngagements ? (pInter->m_flTotalNormalizedEngagementDist / pInter->m_nNormalizedEngagements) : 0) );
			pFileSys->FPrintf( hFile, "\t%d", (int)(pInter->m_nEngagements ? (pInter->m_flTotalEngagementDist / pInter->m_nEngagements) : 0) );
			pFileSys->FPrintf( hFile, "\n" );
		}
	}

	pFileSys->Close( hFile );
}

CTextStatFile g_PlayerClassStatsOutput( PrintPlayerClassStats );


// ---------------------------------------------------------------------------------------------------------------- //
// Detailed stats output.
// ---------------------------------------------------------------------------------------------------------------- //

ConVar tf_DetailedStats( "tf_DetailedStats", "1", 0, "Prints extensive detailed gameplay stats into detailed_stats.txt" );

class CShotInfo
{
public:
	float m_flDistance;
	int m_nDamage;
};

CUtlLinkedList<CShotInfo,int> g_ClassShotInfos[STATS_NUM_GROUPS];

void PrintDetailedPlayerClassStats()
{
	if ( !tf_DetailedStats.GetInt() )
		return;

	IFileSystem *pFileSys = filesystem;

	FileHandle_t hFile = pFileSys->Open( "class_stats_detailed.txt", "wt", "LOGDIR" );
	if ( hFile != FILESYSTEM_INVALID_HANDLE )
	{
		// Print the header.
		for ( int i=TFCLASS_UNDECIDED+1; i < STATS_NUM_GROUPS; i++ )
		{
			pFileSys->FPrintf( hFile, "%s dist\t%s dmg\t", GetGroupNameFor( i ), GetGroupNameFor( i ) );
		}
		pFileSys->FPrintf( hFile, "\n" );

		// Write out each column.
		int iterators[STATS_NUM_GROUPS];
		for ( i=TFCLASS_UNDECIDED+1; i < STATS_NUM_GROUPS; i++ )
			iterators[i] = g_ClassShotInfos[i].Head();

		bool bWroteAnything;
		do
		{
			bWroteAnything = false;
			for ( int i=TFCLASS_UNDECIDED+1; i < STATS_NUM_GROUPS; i++ )
			{
				if ( iterators[i] == g_ClassShotInfos[i].InvalidIndex() )
				{
					pFileSys->FPrintf( hFile, "\t\t" );
				}
				else
				{
					CShotInfo *pInfo = &g_ClassShotInfos[i][iterators[i]];
					iterators[i] = g_ClassShotInfos[i].Next( iterators[i] );

					pFileSys->FPrintf( hFile, "%.2f\t%d\t", pInfo->m_flDistance, pInfo->m_nDamage );
					bWroteAnything = true;
				}
			}
			pFileSys->FPrintf( hFile, "\n" );

		} while ( bWroteAnything );


		pFileSys->Close( hFile );
	}
}

CTextStatFile g_PlayerClassStatsDetailedOutput( PrintDetailedPlayerClassStats );



//=====================================================================
// PLAYER CLASS HANDLING
//=====================================================================
// Base PlayerClass
CPlayerClass::CPlayerClass( CBaseTFPlayer *pPlayer, TFClass iClass )
: m_TFClass( iClass )
{
	m_pPlayer = pPlayer;
	
	for (int i = 0; i <= MAX_TF_TEAMS; ++i)
	{
		m_sClassModel[i] = NULL_STRING;
	}
	m_iNumWeaponTechAssociations = 0;

	m_bTechAssociationsSet = false;

	m_flNormalizedEngagementNextTime = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClass::~CPlayerClass()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::ClassActivate( void )
{
	// Setup the default player movement variables.
	SetupMoveData();

	AddWeaponTechAssociations();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::ClassDeactivate( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::AddWeaponTechAssociations( void )
{
	ClearAllWeaponTechAssoc();

	// Iterate tech tree for this class and find
	Assert( m_pPlayer );
	CTechnologyTree *tree = m_pPlayer->GetTechTree();
	Assert( tree );

	// Loop through all of the techs to see if any of them say yes to being applicable to 
	//  this class specifically
	for ( int i = 0 ; i < tree->GetNumberTechnologies(); i++ )
	{
		CBaseTechnology *tech = tree->GetTechnology( i );
		if ( !tech )
			continue;

		if ( !tech->GetAssociateWeaponsForClass( GetTFClass() ) )
			continue;

		// Associate weapon tech name with class
		AddWeaponTechAssoc( (char *)tech->GetName() );
	}
}


void CPlayerClass::NetworkStateChanged()
{
	if ( m_pPlayer )
		m_pPlayer->NetworkStateChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Setup the default movement parameters, quite a few of these will be
//          overridden with class specific values.
//-----------------------------------------------------------------------------
void CPlayerClass::SetupMoveData( void )
{
	// Set the default walking and sprinting speeds.
	m_flMaxWalkingSpeed = 120;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( PLAYERCLASS_HULL_STAND_MIN, PLAYERCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( PLAYERCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = PLAYERCLASS_STEPSIZE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::CreateClass( void )
{ 
	// Give them a full loadout on the initial spawn
	ResupplyAmmo( 100.0f, RESUPPLY_ALL_FROM_STATION );

	// Create the builder weapon & all objects in it (Helpers are automatically deleted when the weapon's deleted)
	// Make sure they can build at least 1 object
	if ( GetTFClassInfo( m_TFClass )->m_pClassObjects[0] != OBJ_LAST )
	{
		m_pPlayer->GiveNamedItem( "weapon_builder" );

		Assert( m_pPlayer->GetWeaponBuilder() );

		// Do we have a construction yard?
		bool bHaveYard = false;
		CBaseEntity *pEntity = NULL;
		while ((pEntity = gEntList.FindEntityByClassname( pEntity, "func_construction_yard" )) != NULL)
		{
			if ( m_pPlayer->InSameTeam( pEntity ) )
			{
				bHaveYard = true;
				break;
			}
		}

		for ( int i = 0; i < OBJ_LAST; i++ )
		{
			int iClassObject = GetTFClassInfo( m_TFClass )->m_pClassObjects[i];
			
			// Hit the end?
			if ( iClassObject == OBJ_LAST )
				break;

			// Only Humans can build the powerpacks & mortars
			if ( iClassObject == OBJ_POWERPACK || iClassObject == OBJ_MORTAR)
			{
				if ( m_pPlayer->GetTeamNumber() != TEAM_HUMANS )
					continue;
			}

			// Only Aliens can build shields
			if ( iClassObject == OBJ_SHIELDWALL )
			{
				if ( m_pPlayer->GetTeamNumber() != TEAM_ALIENS )
					continue;
			}

			// If my team doesn't have a construction yard, don't allow me to build vehicles
			if ( !tf_fastbuild.GetBool() && !bHaveYard )
			{
				if ( IsObjectAVehicle(iClassObject) ) 
					continue;

				// Don't allow them to build vehicle upgrades either
				if ( iClassObject == OBJ_DRIVER_MACHINEGUN )
					continue;
			}

			// If my team has a construction yard, don't allow me to build defensive buildings
			if ( !tf_fastbuild.GetBool() && bHaveYard && IsObjectADefensiveBuilding(iClassObject) )
				continue;

			m_pPlayer->GetWeaponBuilder()->AddBuildableObject( iClassObject );

			// Give the player a fake weapon to select this object with
			CWeaponObjectSelection *pSelection = (CWeaponObjectSelection *)m_pPlayer->GiveNamedItem( "weapon_objectselection", iClassObject  );
			if ( pSelection )
			{
				pSelection->SetType( iClassObject );
			}
		}
	}

	// Give the player all the weapons from tech associations
	CTechnologyTree *pTechTree = m_pPlayer->GetTechTree();
	if ( pTechTree )
	{
		// Give the player any weapons s/he might have just received the tech for
		for ( int i = 0; i < m_iNumWeaponTechAssociations; i++ )
		{
			if ( m_pPlayer->HasNamedTechnology( m_WeaponTechAssociations[i].pWeaponTech ) )
			{
				CBaseTechnology *tech = pTechTree->GetTechnology( m_WeaponTechAssociations[i].pWeaponTech );
				if ( tech )
				{	
					for ( int j = 0; j < tech->GetNumWeaponAssociations(); j++ )
					{
						const char *weaponname = tech->GetAssociatedWeapon( j );
						Assert( weaponname );
						
						m_pPlayer->GiveNamedItem( weaponname );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called on each respawn
//-----------------------------------------------------------------------------
void CPlayerClass::RespawnClass( void )
{
	ResupplyAmmo( 100.0f, RESUPPLY_ALL_FROM_STATION );
	GainedNewTechnology( NULL );
	SetMaxHealth( GetMaxHealthCVarValue() );
	SetMaxSpeed( GetMaxSpeed() );
	CheckDeterioratingObjects();

	SetupSizeData();

	// Refill the clips of all my weapons
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		CBaseCombatWeapon *pWeapon = m_pPlayer->GetWeapon(i);
		if ( pWeapon )
		{
			if ( pWeapon->UsesClipsForAmmo1() )
			{
				pWeapon->m_iClip1 = pWeapon->GetDefaultClip1();
			}
			if ( pWeapon->UsesClipsForAmmo2() )
			{
				pWeapon->m_iClip2 = pWeapon->GetDefaultClip2();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Supply the player with Ammo. Return true if some ammo was given.
//-----------------------------------------------------------------------------
bool CPlayerClass::ResupplyAmmoType( float flAmount, const char *pAmmoType )
{
	if (flAmount <= 0)
		return false;

	// Make sure at least 1 if given...
	int nAmount;
	if (flAmount <= 1)
		nAmount = 1;
	else
		nAmount = (int)flAmount;

	bool bGiven = false;
	if ( g_pGameRules->CanHaveAmmo( m_pPlayer, pAmmoType ) )
	{
		if ( m_pPlayer->GiveAmmo( nAmount, pAmmoType, true ) > 0 )
			bGiven = true;
	}

	return bGiven;
}

//-----------------------------------------------------------------------------
// Purpose: Supply the player with Ammo. Return true if some ammo was given.
//-----------------------------------------------------------------------------
bool CPlayerClass::ResupplyAmmo( float flPercentage, ResupplyReason_t reason )
{
	bool bGiven = false;

	// Fully resupply shield energy everytime
	if ( m_pPlayer->GetCombatShield() )
	{
		m_pPlayer->GetCombatShield()->AddShieldHealth( 1.0 ); 
	}

	if ((reason == RESUPPLY_RESPAWN) || (reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
		if (ResupplyAmmoType( 1, "Sappers" ))
		{
			bGiven = true;
		}
	}

	return bGiven;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the player's Speed
//-----------------------------------------------------------------------------
float CPlayerClass::GetMaxSpeed( void )
{
	float flMaxSpeed = m_flMaxWalkingSpeed;

	// Is the player in adrenalin mode?
	if ( m_pPlayer->HasPowerup( POWERUP_RUSH ) )
	{
		flMaxSpeed *= ADRENALIN_SPEED_INCREASE;
	}

	// Is the player unable to move right now?
	if ( m_pPlayer->CantMove() )
	{
		flMaxSpeed = 1;
	}

	return flMaxSpeed;
}

//-----------------------------------------------------------------------------
// Purpose: Get the player's maximum walking speed
//-----------------------------------------------------------------------------
float CPlayerClass::GetMaxWalkSpeed( void )
{ 
	return m_flMaxWalkingSpeed; 
}

//-----------------------------------------------------------------------------
// Purpose: Set the player's speed
//-----------------------------------------------------------------------------
void CPlayerClass::SetMaxSpeed( float flMaxSpeed )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetMaxSpeed( flMaxSpeed );

		float curspeed = m_pPlayer->GetAbsVelocity().Length();

		if ( curspeed != 0.0f && curspeed > flMaxSpeed )
		{
			float crop = flMaxSpeed / curspeed;

			Vector vecNewVelocity;
			VectorScale( m_pPlayer->GetAbsVelocity(), crop, vecNewVelocity );
			m_pPlayer->SetAbsVelocity( vecNewVelocity );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the player's max health
//-----------------------------------------------------------------------------
int CPlayerClass::GetMaxHealthCVarValue()
{
	int val = GetTFClassInfo( GetTFClass() )->m_pMaxHealthCVar->GetInt();
	Assert( val > 0 );	// If you hit this assert, then you probably didn't add an entry to skill?.cfg
	return val;
}

//-----------------------------------------------------------------------------
// Purpose: Set the player's health
//-----------------------------------------------------------------------------
void CPlayerClass::SetMaxHealth( float flMaxHealth )
{
	if ( m_pPlayer )
	{
		m_pPlayer->m_iMaxHealth = m_pPlayer->m_iHealth = flMaxHealth;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
string_t CPlayerClass::GetClassModel( int nTeam )
{
	return m_sClassModel[nTeam];
}


const char*	CPlayerClass::GetClassModelString( int nTeam )
{
	// Each derived class should implement this.
	Assert( false );
	return "INVALID";
}


//-----------------------------------------------------------------------------
// Purpose: Called to see if another player should be displayed on the players radar
//-----------------------------------------------------------------------------
bool CPlayerClass::CanSeePlayerOnRadar( CBaseTFPlayer *pl )
{
//	if ( pl->InSameTeam(m_pPlayer) )
//		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::ItemPostFrame()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : TFClass
//-----------------------------------------------------------------------------
TFClass CPlayerClass::GetTFClass( void )
{
	return m_TFClass;
}

//-----------------------------------------------------------------------------
// Purpose: Handle custom commands for this playerclass
//-----------------------------------------------------------------------------
bool CPlayerClass::ClientCommand( const CCommand& args )
{
	if ( FStrEq( args[0], "builder_select_obj" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		// This is a total hack. Eventually this should come in via usercmds.

		// Get our builder weapon
		if ( !m_pPlayer->GetWeaponBuilder() )
			return true;

		// Select a state for the builder weapon
		m_pPlayer->GetWeaponBuilder()->SetCurrentObject( atoi( args[1] ) );
		m_pPlayer->GetWeaponBuilder()->SetCurrentState( BS_PLACING );
		m_pPlayer->GetWeaponBuilder()->StartPlacement(); 
		m_pPlayer->GetWeaponBuilder()->m_flNextPrimaryAttack = gpGlobals->curtime + 0.35f;
		return true;
	}
	else if ( FStrEq( args[0], "builder_select_mode" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		// Get our builder weapon
		if ( !m_pPlayer->GetWeaponBuilder() )
			return true;

		// Select a state for the builder weapon
		m_pPlayer->GetWeaponBuilder()->SetCurrentState( atoi( args[1] ) );
		m_pPlayer->GetWeaponBuilder()->m_flNextPrimaryAttack = gpGlobals->curtime + 0.35f;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Should we take damage-based force?
//-----------------------------------------------------------------------------
bool CPlayerClass::ShouldApplyDamageForce( const CTakeDamageInfo &info )
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: The player has taken damage. Return the damage done.
//-----------------------------------------------------------------------------
float CPlayerClass::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( info.GetAttacker() )
	{
		CBaseTFPlayer *pPlayer = dynamic_cast< CBaseTFPlayer* >( info.GetAttacker() );
		if ( pPlayer && pPlayer->GetPlayerClass() )
		{
			int iStatGroup = GetStatGroupFor( pPlayer );
			CInterClassStats *pInter = &g_PlayerClassStats[iStatGroup].m_InterClassStats[GetTFClass()];
			
			pInter->m_flTotalDamageInflicted += info.GetDamage();
			
			float flDistToAttacker = pPlayer->GetAbsOrigin().DistTo( GetPlayer()->GetAbsOrigin() );
			pInter->m_flTotalEngagementDist += flDistToAttacker;
			pInter->m_nEngagements++;

			if ( gpGlobals->curtime >= m_flNormalizedEngagementNextTime )
			{
				pInter->m_flTotalNormalizedEngagementDist += flDistToAttacker;
				pInter->m_nNormalizedEngagements++;

				m_flNormalizedEngagementNextTime = gpGlobals->curtime + 3;
			}

			// Store detailed stats for the shot?
			if ( tf_DetailedStats.GetInt() )
			{
				CShotInfo shotInfo;
				
				shotInfo.m_flDistance = flDistToAttacker;
				shotInfo.m_nDamage = (int)info.GetDamage();

				g_ClassShotInfos[iStatGroup].AddToTail( shotInfo );
			}
		}
	}
	
	return info.GetDamage();
}

//-----------------------------------------------------------------------------
// Purpose: New technology has been gained. Recalculate any class specific technology dependencies.
//-----------------------------------------------------------------------------
void CPlayerClass::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	int i;

	// Tell the player's weapons that this player's gained new technology
	for ( i = 0; i < m_pPlayer->WeaponCount(); i++ ) 
	{
		if ( m_pPlayer->GetWeapon(i) ) 
		{
			((CBaseTFCombatWeapon*)m_pPlayer->GetWeapon(i))->GainedNewTechnology( pTechnology );
		}
	}

	// Tell the player's objects that this player's gained new technology
	for ( i = 0; i < m_pPlayer->GetObjectCount(); i++ )
	{
		if (m_pPlayer->GetObject(i))
		{
			m_pPlayer->GetObject(i)->GainedNewTechnology( pTechnology );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified objects
//-----------------------------------------------------------------------------
int CPlayerClass::CanBuild( int iObjectType )
{
	int iObjectCount = GetNumObjects( iObjectType );

	// Make sure we haven't hit maximum number
	if ( tf2_object_hard_limits.GetBool() )
	{
		if ( iObjectCount >= GetObjectInfo( iObjectType )->m_nMaxObjects )
			return CB_LIMIT_REACHED;
	}
	else
	{
		// Find out how much the next object should cost
		int iCost = CalculateObjectCost( iObjectType, GetNumObjects( iObjectType ), m_pPlayer->GetTeamNumber() );

		// Make sure we have enough resources
		if ( m_pPlayer->GetBankResources() < iCost )
			return CB_NEED_RESOURCES;
	}

	return CB_CAN_BUILD;
}

//-----------------------------------------------------------------------------
// Purpose: Object built by this player has been destroyed
//-----------------------------------------------------------------------------
void CPlayerClass::OwnedObjectDestroyed( CBaseObject *pObject )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of the specified objects built by the player
//-----------------------------------------------------------------------------
int	CPlayerClass::GetNumObjects( int iObjectType )
{
	// For the purposes of costs/limits, Sentryguns tally up all sentrygun types
	if ( iObjectType == OBJ_SENTRYGUN_PLASMA || iObjectType == OBJ_SENTRYGUN_ROCKET_LAUNCHER  )
	{
		return ( m_pPlayer->GetNumObjects( OBJ_SENTRYGUN_PLASMA ) + m_pPlayer->GetNumObjects( OBJ_SENTRYGUN_ROCKET_LAUNCHER ) );
	}

	return m_pPlayer->GetNumObjects( iObjectType );
}

//-----------------------------------------------------------------------------
// Purpose: Player has started building an object
//-----------------------------------------------------------------------------
int	CPlayerClass::StartedBuildingObject( int iObjectType )
{
	// Deduct the cost of the object
	if ( !tf2_object_hard_limits.GetBool() )
	{
		int iCost = CalculateObjectCost( iObjectType, GetNumObjects( iObjectType ), m_pPlayer->GetTeamNumber() );
		if ( iCost > m_pPlayer->GetBankResources() )
		{
			// Player must have lost resources since he started placing
			return 0;
		}
		m_pPlayer->RemoveBankResources( iCost );

		// If the object costs 0, we need to return non-0 to mean success
		if ( !iCost )
			return 1;

		return iCost;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Player has aborted a build
//-----------------------------------------------------------------------------
void CPlayerClass::StoppedBuilding( int iObjectType )
{
	// Return the cost of the object
	if ( !tf2_object_hard_limits.GetBool() )
	{
		int iCost = CalculateObjectCost( iObjectType, GetNumObjects( iObjectType ), m_pPlayer->GetTeamNumber() );
		m_pPlayer->AddBankResources( iCost );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Object has been built by this player
//-----------------------------------------------------------------------------
void CPlayerClass::FinishedObject( CBaseObject *pObject )
{
}

//-----------------------------------------------------------------------------
// Purpose: Object has been picked up by this player
//-----------------------------------------------------------------------------
void CPlayerClass::PickupObject( CBaseObject *pObject )
{
	// Return the cost of the object
	if ( !tf2_object_hard_limits.GetBool() )
	{
		int iCost = CalculateObjectCost( pObject->GetType(), GetNumObjects( pObject->GetType() ), m_pPlayer->GetTeamNumber(), true );
		m_pPlayer->AddBankResources( iCost );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Create a personal order for this player
//-----------------------------------------------------------------------------
bool CPlayerClass::CreateInitialOrder()
{
	if( AnyNonResourceZoneOrders() )
		return true;

	return false;
}


void CPlayerClass::CreatePersonalOrder()
{
	if( CreateInitialOrder() )
		return;			   

	// Make an order to fix any objects we own.
	if ( COrderRepair::CreateOrder_RepairOwnObjects( this ) )
		return;
}


bool CPlayerClass::AnyResourceZoneOrders()
{
	return !!m_pPlayer->GetNumResourceZoneOrders();
}


bool CPlayerClass::AnyNonResourceZoneOrders()
{
	return 
		m_pPlayer->GetNumResourceZoneOrders() == 0 && 
		m_pPlayer->GetOrder();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::ClassThink( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::PowerupEnd( int iPowerup )
{
}

//-----------------------------------------------------------------------------
// Purpose: My player has just died
//-----------------------------------------------------------------------------
void CPlayerClass::PlayerDied( CBaseEntity *pAttacker )
{
	if ( pAttacker )
	{
		CBaseTFPlayer *pAttackerPlayer = dynamic_cast< CBaseTFPlayer* >( pAttacker );
		if ( pAttackerPlayer )
		{
			CPlayerClass *pAttackerClass = pAttackerPlayer->GetPlayerClass();
			if ( pAttackerClass )
			{
				int iStatGroup = GetStatGroupFor( pAttackerPlayer );
				g_PlayerClassStats[ iStatGroup ].m_InterClassStats[GetTFClass()].m_nKills++;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: My player just killed another player
//-----------------------------------------------------------------------------
void CPlayerClass::PlayerKilledPlayer( CBaseTFPlayer *pVictim )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::SetPlayerHull( void )
{
	// Use the generic player hull if the class doesn't override this function.	
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( PLAYERCLASS_HULL_DUCK_MIN, PLAYERCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( PLAYERCLASS_HULL_STAND_MIN, PLAYERCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax )
{
	// Use the generic player hull if the class doesn't override this function.	
	if ( bDucking )
	{
		VectorCopy( PLAYERCLASS_HULL_DUCK_MIN, vecMin );
		VectorCopy( PLAYERCLASS_HULL_DUCK_MAX, vecMax );
	}
	else
	{
		VectorCopy( PLAYERCLASS_HULL_STAND_MIN, vecMin );
		VectorCopy( PLAYERCLASS_HULL_STAND_MAX, vecMax );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::InitVCollision( void )
{
	CPhysCollide *pStandModel = PhysCreateBbox( PLAYERCLASS_HULL_STAND_MIN, PLAYERCLASS_HULL_STAND_MAX );
	CPhysCollide *pCrouchModel = PhysCreateBbox( PLAYERCLASS_HULL_DUCK_MIN, PLAYERCLASS_HULL_DUCK_MAX );

	solid_t solid;
	solid.params = g_PhysDefaultObjectParams;
	solid.params.mass = 85.0f;
	solid.params.inertia = 1e24f;
	solid.params.enableCollisions = false;
	//disable drag
	solid.params.dragCoefficient = 0;

	// create standing hull
	m_pPlayer->m_pShadowStand = PhysModelCreateCustom( m_pPlayer, pStandModel, m_pPlayer->GetLocalOrigin(), m_pPlayer->GetLocalAngles(), "tfplayer_generic", false, &solid );
	m_pPlayer->m_pShadowStand->SetCallbackFlags( CALLBACK_SHADOW_COLLISION );

	// create crouchig hull
	m_pPlayer->m_pShadowCrouch = PhysModelCreateCustom( m_pPlayer, pCrouchModel, m_pPlayer->GetLocalOrigin(), m_pPlayer->GetLocalAngles(), "tfplayer_generic", false, &solid );
	m_pPlayer->m_pShadowCrouch->SetCallbackFlags( CALLBACK_SHADOW_COLLISION );

	// default to stand
	m_pPlayer->VPhysicsSetObject( m_pPlayer->m_pShadowStand );

	//disable drag
	m_pPlayer->m_pShadowStand->EnableDrag( false );
	m_pPlayer->m_pShadowCrouch->EnableDrag( false );

	// tell physics lists I'm a shadow controller object
	PhysAddShadow( m_pPlayer );	
	m_pPlayer->m_pPhysicsController = physenv->CreatePlayerController( m_pPlayer->m_pShadowStand );

	// init state
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetVCollisionState( VPHYS_CROUCH );
	}
	else
	{
		m_pPlayer->SetVCollisionState( VPHYS_WALK );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//			*pNewOwner - 
//-----------------------------------------------------------------------------
void CPlayerClass::OwnedObjectChangeToTeam( CBaseObject *pObject, CBaseTFPlayer *pNewOwner )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//			*pOldOwner - 
//-----------------------------------------------------------------------------
void CPlayerClass::OwnedObjectChangeFromTeam( CBaseObject *pObject, CBaseTFPlayer *pOldOwner )
{
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if there are any deteriorating objects I once owned that
//			I can now re-own (i.e. I switched teams back to my original team).
//			TODO: Make this use Steam IDs, so disconnecting & reconnecting players
//			get their objects back.
//-----------------------------------------------------------------------------
void CPlayerClass::CheckDeterioratingObjects( void )
{
	if ( !GetTeam() )
		return;

	// Cycle through the team's objects looking for any deteriorating objects once owned by me
	for ( int i = 0; i < GetTeam()->GetNumObjects(); i++ )
	{
		CBaseObject *pObject = GetTeam()->GetObject(i);
		if ( pObject->IsDeteriorating() && pObject->GetOriginalBuilder() == m_pPlayer )
		{
			// Can this class build this object?
			if ( ClassCanBuild( GetTFClass(), pObject->GetType() ) )
			{
				// Give the object back to this player
				pObject->SetBuilder( m_pPlayer );
				m_pPlayer->AddObject( pObject );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : edict_t
//-----------------------------------------------------------------------------
CBaseEntity *CPlayerClass::SelectSpawnPoint( void )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Add a new weapon/tech association. Weapons that the player has the associated tech for
//			will automatically be given out if the player has the tech.
//-----------------------------------------------------------------------------
void CPlayerClass::AddWeaponTechAssoc( char *pWeaponTech )
{
	Assert( m_iNumWeaponTechAssociations < MAX_WEAPONS );

	m_WeaponTechAssociations[m_iNumWeaponTechAssociations].pWeaponTech = pWeaponTech;
	m_iNumWeaponTechAssociations++;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::ClearAllWeaponTechAssoc( )
{
	m_iNumWeaponTechAssociations = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFTeam *CPlayerClass::GetTeam()
{
	return (CTFTeam*)GetPlayer()->GetTeam();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClass::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( PLAYERCLASS_VIEWOFFSET_STAND );
	}
}

