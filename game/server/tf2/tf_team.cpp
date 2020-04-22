//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team management class. Contains all the details for a specific team
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "team.h"
#include "tf_team.h"
#include "tf_func_resource.h"
#include "tf_player.h"
#include "techtree.h"
#include "tf_obj.h"
#include "tf_obj_resupply.h"
#include "orders.h"
#include "entitylist.h"
#include "team_spawnpoint.h"
#include "team_messages.h"
#include "tf_obj_powerpack.h"
#include "tf_gamerules.h"
#include "engine/IEngineSound.h"
#include "tier1/strtools.h"
#include "tf_stats.h"
#include "tf_obj_buff_station.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define OBJECT_COVERED_DIST	1000
#define RESOURCE_GIVE_TIME 30
#define RESOURCE_GIVE_AMOUNT 150
#define RESOURCE_DONATION_AMT_PER_PLAYER 10

bool IsEntityVisibleToTactical( int iLocalTeamNumber, int iLocalTeamPlayers, 
	int iLocalTeamObjects, int iEntIndex, const char *pEntName, int pEntTeamNumber, const Vector &pEntOrigin );
extern ConVar tf_destroyobjects;

//-----------------------------------------------------------------------------
// Purpose: SendProxy that converts the UtlVector list of radar scanners to entindexes, where it's reassembled on the client
//-----------------------------------------------------------------------------
void SendProxy_ObjectList( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CTFTeam *pTeam = (CTFTeam*)pData;

	// If this fails, then SendProxyArrayLength_TeamObjects didn't work.
	Assert( iElement < pTeam->GetNumObjects() );

	CBaseObject *pObject = pTeam->GetObject(iElement);
	EHANDLE hObject;
	hObject = pObject;

	SendProxy_EHandleToInt( pProp, pStruct, &hObject, pOut, iElement, objectID );
}


int SendProxyArrayLength_TeamObjects( const void *pStruct, int objectID )
{
	CTFTeam *pTeam = (CTFTeam*)pStruct;
	int iObjects = pTeam->GetNumObjects();
	Assert( iObjects < MAX_OBJECTS_PER_TEAM );
	return iObjects;
}


// Datatable
IMPLEMENT_SERVERCLASS_ST(CTFTeam, DT_TFTeam)
	SendPropFloat( SENDINFO(m_fResources), 16, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_fPotentialResources), 16, SPROP_NOSCALE ),
	SendPropInt( SENDINFO(m_bHaveZone), 1, SPROP_UNSIGNED ), 
	
	SendPropArray2( 
		SendProxyArrayLength_TeamObjects,
		SendPropInt("object_array_element", 0, SIZEOF_IGNORE, NUM_NETWORKED_EHANDLE_BITS, SPROP_UNSIGNED, SendProxy_ObjectList), 
		MAX_OBJECTS_PER_TEAM, 
		0, 
		"object_array"
		 )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( tf_team_manager, CTFTeam );

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to the specified TF team manager
//-----------------------------------------------------------------------------
CTFTeam *GetGlobalTFTeam( int iIndex )
{
	return (CTFTeam*)GetGlobalTeam( iIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Needed because this is an entity, but should never be used
//-----------------------------------------------------------------------------
void CTFTeam::Init( const char *pName, int iNumber )
{
	BaseClass::Init( pName, iNumber );

	InitializeTeamResources();
	InitializeTechTree();
	InitializeOrders();
	ClearMessages();

	m_flNextResourceTime = 0;

	// Only detect changes every half-second.
	NetworkProp()->SetUpdateInterval( 0.75f );

	m_flTotalResourcesSoFar = m_iLastUpdateSentAt = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFTeam::~CTFTeam( void )
{
	m_aResourcesBeingCollected.Purge();
	m_aResupplyBeacons.Purge();
	m_aObjects.Purge();
	m_aOrders.Purge();

	delete m_pTechnologyTree;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::Precache( void )
{
	// Precache all the technologies in the techtree
	for ( int i = 0; i < m_pTechnologyTree->GetNumberTechnologies(); i++ )
	{
		PrecacheTechnology( m_pTechnologyTree->GetTechnology(i) );
	}

	PrecacheScriptSound( "TFTeam.CapturedZone" );
	PrecacheScriptSound( "TFTeam.LostZone" );
	PrecacheScriptSound( "TFTeam.ObtainStolenTechnology" );
	PrecacheScriptSound( "TFTeam.BoughtPreferredTechnology" );
	PrecacheScriptSound( "TFTeam.AddOrder" );
}


//-----------------------------------------------------------------------------
// Purpose: Precache a technology's files
//-----------------------------------------------------------------------------
void CTFTeam::PrecacheTechnology( CBaseTechnology *pTech )
{
	// Precache sounds for every class result
	for (int i = 0; i < TFCLASS_CLASS_COUNT; i++ )
	{
		if ( pTech->GetSoundFile(i) && (pTech->GetSoundFile(i)[0] != 0) )
		{
			PrecacheScriptSound( pTech->GetSoundFile(i) );
			pTech->SetClassResultSound( i, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CTFTeam::Think( void )
{
	UpdateOrders();
	UpdateMessages();

	// FIXME: Try this out?
	/*
	// Give resources to the team at regular intervals
	if (gpGlobals->curtime >= m_flNextResourceTime)
	{
		AddTeamResources( RESOURCE_GIVE_AMOUNT );
		m_flNextResourceTime = gpGlobals->curtime + RESOURCE_GIVE_TIME;
	}
	*/

	UpdateTechnologies();
		    
	/* FIXME: Re-enable once we figure out what the correct orders should be
	// Create new personal orders
	if ( m_flPersonalOrderUpdateTime < gpGlobals->curtime )
	{
		CreatePersonalOrders();
		m_flPersonalOrderUpdateTime = gpGlobals->curtime + PERSONAL_ORDER_UPDATE_TIME;
	}
	*/
}

//-----------------------------------------------------------------------------
// DATA HANDLING
//-----------------------------------------------------------------------------
// Purpose: Check to see if we should resend the entire tech tree to a player on Hud reinitialisation, which happens every player respawn
//-----------------------------------------------------------------------------
void CTFTeam::UpdateClientData( CBasePlayer *pPlayer )
{
	CBaseTFPlayer *pTFPlayer = (CBaseTFPlayer *)pPlayer;
	// If we're initialising the hud, update all technologies
	if ( pTFPlayer->HUDNeedsRestart() )
	{
		// Check all the technologies and resend any that differ from this client's representation
		for ( int i = 0; i < m_pTechnologyTree->GetNumberTechnologies(); i++ )
		{
			// Update all technologies
			CBaseTechnology *technology = m_pTechnologyTree->GetTechnology(i);
			if ( technology )
			{
				// Check to see if any resource levels have changed
				if ( pTFPlayer->AvailableTech(i).m_nResourceLevel != technology->GetResourceLevel() )
				{
					UpdateClientTechnology( i, pTFPlayer );
					continue;
				}

				if ( technology->GetAvailable() != pTFPlayer->AvailableTech(i).m_nAvailable )
				{
					UpdateClientTechnology( i, pTFPlayer );
					continue;
				}

				byte pcount = technology->GetPreferenceCount();
				if ( pTFPlayer->GetPreferredTechnology() == i )
				{
					pcount |= 0x80;
				}

				if ( pcount != pTFPlayer->AvailableTech(i).m_nUserCount )
				{
					UpdateClientTechnology( i, pTFPlayer );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update a technology for a player
//-----------------------------------------------------------------------------
void CTFTeam::UpdateClientTechnology( int iTechID, CBaseTFPlayer *pPlayer )
{
	CBaseTechnology *pTechnology = m_pTechnologyTree->GetTechnology( iTechID );
	if ( !pTechnology )
		return;

	byte pcount = pTechnology->GetPreferenceCount();

	if ( pPlayer->GetPreferredTechnology() == iTechID )
	{
		pcount |= 0x80;
	}

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	// Update this technology
	UserMessageBegin( user, "Technology" );
		WRITE_BYTE( iTechID );
		WRITE_BYTE( pTechnology->GetAvailable() );
		WRITE_BYTE( pcount );
		WRITE_SHORT( (short)pTechnology->GetResourceLevel() );
	MessageEnd();

	// Update the player's client tech representation
	pPlayer->AvailableTech(iTechID).m_nAvailable = pTechnology->GetAvailable();
	pPlayer->AvailableTech(iTechID).m_nUserCount = pcount;
	pPlayer->AvailableTech(iTechID).m_nResourceLevel = pTechnology->GetResourceLevel();

	/*
	Msg( "Sent %s(%d) to %s:\n", pTechnology->GetName(), iTechID, pPlayer->GetPlayerName() );
	Msg( "   Available: %d\n", pTechnology->GetAvailable() );
	Msg( "   PrefCount: %d\n", pTechnology->GetPreferenceCount() );
	Msg( "   Level    : %0.2f\n", pTechnology->GetResourceLevel() );
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if any technology has changed, and resend it to players if it has
//-----------------------------------------------------------------------------
void CTFTeam::UpdateTechnologyData( void )
{
	for ( int i = 0; i < m_pTechnologyTree->GetNumberTechnologies(); i++ )
	{
		// Update all technologies
		CBaseTechnology *pTechnology = m_pTechnologyTree->GetTechnology(i);
		if ( pTechnology && pTechnology->IsDirty() )
		{
			// Send it to all our clients
			for ( int iPlayer = 0; iPlayer < m_aPlayers.Count(); iPlayer++ )
			{
				CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)m_aPlayers[iPlayer];
				UpdateClientTechnology( i, pPlayer );
			}

			pTechnology->SetDirty( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFTeam::ShouldTransmitToPlayer( CBasePlayer* pRecipient, CBaseEntity* pEntity )
{
	return IsEntityVisibleToTactical( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Is the specified entity visible on this team's tactical view?
//-----------------------------------------------------------------------------
bool CTFTeam::IsEntityVisibleToTactical( CBaseEntity *pEntity )
{
	return ::IsEntityVisibleToTactical( GetTeamNumber(), GetNumPlayers(), 
		GetNumObjects(), pEntity->entindex(), (char*)STRING(pEntity->m_iClassname), 
		pEntity->GetTeamNumber(), pEntity->GetAbsOrigin() );
}

//-----------------------------------------------------------------------------
// RESOURCES
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: Add a resource zone to the list of zones to collect from
//-----------------------------------------------------------------------------
void CTFTeam::AddResourceZone( CResourceZone *pResource )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CTFTeam::AddResourceZone adding res zone %p to team %s\n", gpGlobals->curtime, 
		pResource, GetName() ) );

	// If this resource is already owned by another team, remove it from them
	CTFTeam *pOwners = pResource->GetOwningTeam();
	if ( pOwners )
	{
		pOwners->RemoveResourceZone( pResource );
	}

	pResource->SetOwningTeam( GetTeamNumber() );

	m_aResourcesBeingCollected.AddToTail( pResource );
	m_bHaveZone = true;

	// Tell all the team's members
	for ( int i = 0; i < m_aPlayers.Count(); i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)m_aPlayers[i];
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		CBaseEntity::EmitSound( filter, pPlayer->entindex(), "TFTeam.CapturedZone" );
	}

	// Recalculate team's orders
	RecalcOrders();
}

//-----------------------------------------------------------------------------
// Purpose: Remove a resource zone from the list of zones being collected from
//-----------------------------------------------------------------------------
void CTFTeam::RemoveResourceZone( CResourceZone *pResource )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CTFTeam::RemoveResourceZone removing res zone %p from team %s\n", gpGlobals->curtime, 
		pResource, GetName() ) );

	// Now remove the zone from our list
	m_aResourcesBeingCollected.FindAndRemove( pResource );

	// Still have a zone if there are other zones in the list
	m_bHaveZone = ( m_aResourcesBeingCollected.Count() > 0 );

	// Tell all the team's members
	for ( int i = 0; i < m_aPlayers.Count(); i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)m_aPlayers[i];
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		CBaseEntity::EmitSound( filter, pPlayer->entindex(),"TFTeam.LostZone" );
	}

	// Recalculate team's orders
	RecalcOrders();
}

//-----------------------------------------------------------------------------
// Purpose: Recalculate the potential resources
//-----------------------------------------------------------------------------
void CTFTeam::UpdatePotentialResources( void )
{
	// Set potential to current amount
	m_fPotentialResources = GetTeamResources();

	// This used to be used for collectors.
	// It could be updated to count all incoming resources in en-route resource boxes.
}

//-----------------------------------------------------------------------------
// Purpose: Return the amount of resources a player should get when joining this team
//-----------------------------------------------------------------------------
float CTFTeam::GetJoiningPlayerResources( void )
{
	// If we had our banks set recently, use that amount
	if ( gpGlobals->curtime < (m_flLastBankSetTime + 30.0) )
		return m_flLastBankSetAmount;

	if ( !GetNumPlayers() )
		return 0;

	// Otherwise, take the average of all the players on the team
	RecomputeTeamResources();
	return ( GetTeamResources() / GetNumPlayers() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::SetRecentBankSet( float flResources )
{
	m_flLastBankSetAmount = flResources;
	m_flLastBankSetTime = gpGlobals->curtime;
}

//------------------------------------------------------------------------------------------------------------------
// TECHNOLOGY TREE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::InitializeTechTree( void )
{
	m_pTechnologyTree = new CTechnologyTree( filesystem, GetTeamNumber() );

	// Now iterate through added techs and automatically make level 0 techs available
	for (int i = 0; i < m_pTechnologyTree->GetNumberTechnologies(); i++ )
	{
		CBaseTechnology *tech = m_pTechnologyTree->GetTechnology(i);
		if ( !tech )
			continue;

		if ( tech->GetLevel() == 0 )
		{
			EnableTechnology( tech );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTechnologyTree *CTFTeam::GetTechnologyTree( void )
{
	return m_pTechnologyTree;
}

//-----------------------------------------------------------------------------
// Purpose: A new technology has been attained by this team. Give it to every player.
//-----------------------------------------------------------------------------
void CTFTeam::EnableTechnology( CBaseTechnology *technology, bool bStolen )
{
	CTeamFortress *rules = TFGameRules();
	if ( rules )
	{
		// Disable autoswitching if we are getting a weapon
		rules->SetAllowWeaponSwitch( false );
	}

	// Set the technology's resources to the costs
	// Needed because technologies can be enabled through other means than resource spending
	technology->ForceComplete();

	// Apply technology to team first.
	technology->AddTechnologyToTeam( this );

	// Iterate though players
	for (int i = 0; i < m_aPlayers.Count(); i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)m_aPlayers[i];
		technology->AddTechnologyToPlayer( pPlayer );

		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();

		// Play the sound
		if (bStolen)
		{
			CBaseEntity::EmitSound( filter, pPlayer->entindex(), "TFTeam.ObtainStolenTechnology" );
		}
		else
		{
			if ( technology->GetSoundFile(0) && technology->GetSoundFile(0)[0] )
			{
				EmitSound_t ep;
				ep.m_nChannel = CHAN_STATIC;
				ep.m_pSoundName = technology->GetSoundFile(0);

				CBaseEntity::EmitSound( filter, pPlayer->entindex(), ep );
			}
		}

		// Remove all the player's votes on this technology
		CBaseTechnology *pPreferredTech = m_pTechnologyTree->GetTechnology( pPlayer->GetPreferredTechnology() );
		if ( pPreferredTech && pPreferredTech == technology )
		{
			// Tell the player his preferred tech has been bought
			if (!bStolen)
			{
				CBaseEntity::EmitSound( filter, pPlayer->entindex(), "TFTeam.BoughtPreferredTechnology" );
			}

			pPlayer->SetPreferredTechnology( m_pTechnologyTree, -1 );
		}
	}

	// Let the team see if it wants to do anything with this specific technology
	GainedNewTechnology( technology );

	// Reenable autoswitching
	if ( rules )
	{
		rules->SetAllowWeaponSwitch( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: For debugging..
//-----------------------------------------------------------------------------
void CTFTeam::EnableAllTechnologies()
{
	for ( int i = 0; i < m_pTechnologyTree->GetNumberTechnologies(); i++ )
	{
		CBaseTechnology *technology = m_pTechnologyTree->GetTechnology(i);
		if ( !technology || technology->IsHidden() )
			continue;

		EnableTechnology( technology );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called any time a player votes/changes preferences on the client
//-----------------------------------------------------------------------------
void CTFTeam::RecomputePreferences( void )
{
	// Zero total counters
	m_pTechnologyTree->ClearPreferenceCount();

	// Zero out all preferences and iterate through active players
	int i;
	for ( i = 0; i < m_pTechnologyTree->GetNumberTechnologies(); i++ )
	{
		CBaseTechnology *technology = m_pTechnologyTree->GetTechnology(i);
		if ( technology )
		{
			// Zero internal counters
			technology->ZeroPreferences();
		}
	}

	// Now loop through players and see what's preferred
	for ( i = 0; i < m_aPlayers.Count(); i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)m_aPlayers[i];
		if ( !pPlayer )
			continue;
	
		int preferred =  pPlayer->GetPreferredTechnology();
		// No preference set, don't worry about this player
		if ( preferred == -1 )
			continue;

		if ( preferred < 0 || preferred >= MAX_TECHNOLOGIES )
		{
			Msg( "Player %s tried to set preference to out of range tech %i\n", preferred );
			continue;
		}

		// Reference technology
		CBaseTechnology *technology = m_pTechnologyTree->GetTechnology(preferred);
		Assert( technology );
		if ( !technology )
			continue;

		// Msg( "player %s prefers %s\n", pPlayer->GetPlayerName(), technology->GetPrintName() );

		// Add one vote
		technology->IncrementPreferences();
		// Add one vote to totals
		m_pTechnologyTree->IncrementPreferences();
	}

	// Any time preferences are changed/set, see if we should make any purchases immediately.
	RecomputePurchases();
}

//-----------------------------------------------------------------------------
// Purpose: Figure our how many resources we've got in the team
//-----------------------------------------------------------------------------
void CTFTeam::RecomputeTeamResources( void )
{
	// Recalculate the total amount of resources the team has
	m_fResources = 0.0f;
	for ( int i = 0; i < GetNumPlayers(); i++ )
	{
		m_fResources += ((CBaseTFPlayer*)GetPlayer(i))->GetBankResources();
	}

	UpdatePotentialResources();
}	

//-----------------------------------------------------------------------------
// Purpose: Attempt to spend resources according to player's preferences
//-----------------------------------------------------------------------------
void CTFTeam::RecomputePurchases( void )
{
	RecomputeTeamResources();

	// Cycle through all players, and spend their resources on the technologies they're voting for
	for ( int iPlayer = 0; iPlayer < m_aPlayers.Count(); iPlayer++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)GetPlayer( iPlayer );

		// See if he has any resources to spend on a tech
		if ( pPlayer->GetBankResources() <= 0 )
			continue;

		// Has he got a preffered tech?
		if ( pPlayer->GetPreferredTechnology() != -1 )
		{
			// Get the player's voted-for technology
			CBaseTechnology *pPreferredTech = m_pTechnologyTree->GetTechnology( pPlayer->GetPreferredTechnology() );
			if ( pPreferredTech && pPreferredTech->GetAvailable() == false )
			{
				if ( !pPreferredTech->GetResourceCost() )
					continue;

				// Try to spend resources on the tech
				int iResourcesSpent = MIN( pPlayer->GetBankResources(), pPreferredTech->GetResourceCost() - pPreferredTech->GetResourceLevel() );
				if ( pPreferredTech->IncreaseResourceLevel( iResourcesSpent ) )
				{
					// The technology's had enough resources spent to buy it, so enable it
					EnableTechnology( pPreferredTech );
					Msg( "%s bought %s\n",	GetName(), pPreferredTech->GetPrintName() );
				}

				// Reduce the player's bank
				if ( iResourcesSpent )
				{
					pPlayer->RemoveBankResources( iResourcesSpent );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the team owns the specified technology
//-----------------------------------------------------------------------------
bool CTFTeam::HasNamedTechnology( const char *name )
{
	// Look it up
	// FIXME:  This could be too slow, consider using #define'd/indexed names?
	CBaseTechnology *tech = m_pTechnologyTree->GetTechnology( name ); 
	if ( !tech )
		return false;
	if ( !tech->GetAvailable() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: A new technology has been received by the team. Do anything specific to this technology here.
//-----------------------------------------------------------------------------
void CTFTeam::GainedNewTechnology( CBaseTechnology *pTechnology )
{
}


//-----------------------------------------------------------------------------
// Purpose: Called by the team's Think function
//-----------------------------------------------------------------------------
void CTFTeam::UpdateTechnologies( void )
{
	// Update clients
	UpdateTechnologyData();
}


//------------------------------------------------------------------------------------------------------------------
// PLAYERS
//-----------------------------------------------------------------------------
// Purpose: Add the specified player to this team. Remove them from their current team, if any.
//-----------------------------------------------------------------------------
void CTFTeam::AddPlayer( CBasePlayer *pPlayer )
{
	BaseClass::AddPlayer( pPlayer );
	
	// Give the player this team's technology
	for ( int i = 0; i < m_pTechnologyTree->GetNumberTechnologies(); i++ )
	{
		CBaseTechnology *technology = m_pTechnologyTree->GetTechnology(i);
		if ( !technology )
			continue;

		if ( technology->IsHidden() )
			continue;

		// Not yet available to team, skip
		if ( !technology->GetAvailable() )
			continue;

		// Add it.
		technology->AddTechnologyToPlayer( (CBaseTFPlayer*)pPlayer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clean up the player's objects when they leave
//-----------------------------------------------------------------------------
void CTFTeam::RemovePlayer( CBasePlayer *pPlayer )
{
	BaseClass::RemovePlayer( pPlayer );

	// Destroy all objects belonging to this player
	if ( tf_destroyobjects.GetFloat() )
	{
		// Work backwards through the list because objects remove themselves
		int iSize = m_aObjects.Count();
		for (int i = iSize-1; i >= 0; i--)
		{
			if ( (m_aObjects[i]->GetBuilder() == pPlayer) && (m_aObjects[i]->ShouldAutoRemove()) )
			{
				UTIL_Remove( m_aObjects[i] );
			}
		}
	}

	RecomputePreferences();
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of team members of the specified class
//-----------------------------------------------------------------------------
int	CTFTeam::GetNumOfClass( TFClass iClass )
{
	int iNumber = 0;
	for ( int i = 0; i < GetNumPlayers(); i++ )
	{
		if ( ((CBaseTFPlayer*)GetPlayer(i))->IsClass(iClass) )
		{
			iNumber++;
		}
	}
	return iNumber;
}

//------------------------------------------------------------------------------------------------------------------
// RESOURCE BANK
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::InitializeTeamResources( void )
{
	m_fResources = 0.0f;
	m_fPotentialResources = 0.0f;
	m_bHaveZone = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFTeam::GetTeamResources( void )
{
	return m_fResources;
}

//-----------------------------------------------------------------------------
// Purpose: Add resources to this team
//-----------------------------------------------------------------------------
int CTFTeam::AddTeamResources( float fAmount, int nStat )
{
	fAmount = clamp(fAmount, 0, 9999.f);

	m_flTotalResourcesSoFar += fAmount;

	TFStats()->IncrementTeamStat( GetTeamNumber(), TF_TEAM_STAT_RESOURCES_COLLECTED, fAmount );

	// Divvy the resources out to the players
	int iAmountPerPlayer = Ceil2Int( fAmount / GetNumPlayers() );	// Yes, this does create some resources in the roundoff.
	for ( int i = 0; i < GetNumPlayers(); i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetPlayer(i);
		pPlayer->AddBankResources( iAmountPerPlayer );
		TFStats()->IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_RESOURCES_ACQUIRED, fAmount );

		if (nStat >= 0)
		{
			TFStats()->IncrementPlayerStat( pPlayer, (TFPlayerStatId_t)nStat, fAmount );
		}
	}

	ResourceLoadDeposited();

	return iAmountPerPlayer;
}

//-----------------------------------------------------------------------------
// Purpose: Give resources to the player
//-----------------------------------------------------------------------------
void CTFTeam::DonateResources( CBaseTFPlayer *pPlayer )
{
	int nPlayerCount = GetNumPlayers();
	if (nPlayerCount <= 1)
		return;

	int pResourceCount;
	int pResourcePerPlayer;
	int pDonationCount;

	bool bDonating = false;
	pResourceCount = pPlayer->GetBankResources();

	// Figure out how many resources per player to donate
	pResourcePerPlayer = pResourceCount / (nPlayerCount - 1); 
	if (pResourceCount % (nPlayerCount - 1) != 0)
		++pResourcePerPlayer;

	// Clamp to max amt per teammate for each hit...
	if (pResourcePerPlayer > RESOURCE_DONATION_AMT_PER_PLAYER)
		pResourcePerPlayer = RESOURCE_DONATION_AMT_PER_PLAYER;

	// Figure out if we are donating anything at all
	if (pResourceCount > 0)
		bDonating = true;
	if (!bDonating)
		return;

	// Now that we've figured how much to donate, do it!
	for ( int i = 0; i < nPlayerCount; i++ )
	{
		CBaseTFPlayer *pDest = (CBaseTFPlayer*)GetPlayer(i);
		if (pDest == pPlayer)
			continue;

		// The last guy(s) gets the scraps... too bad.
		int nCountToDonate = pResourceCount;
		if (nCountToDonate > pResourcePerPlayer)
			nCountToDonate = pResourcePerPlayer;
		pResourceCount -= nCountToDonate;
		pDonationCount = nCountToDonate;

		pPlayer->DonateResources( pDest, pDonationCount );
	}
}


//-----------------------------------------------------------------------------
// Purpose: New resources have just been dumped in the bank
//-----------------------------------------------------------------------------
void CTFTeam::ResourceLoadDeposited( void )
{
	// HACK TEST CODE
	// Remove after Resource Experiment!
	static int iIncrements = 250;
	if ( m_flTotalResourcesSoFar >= (m_iLastUpdateSentAt + iIncrements) )
	{
		while ( m_flTotalResourcesSoFar >= (m_iLastUpdateSentAt + iIncrements) )
		{
			m_iLastUpdateSentAt += iIncrements;
		}

		EntityMessageBegin( (CBaseEntity*)this );
			WRITE_LONG( m_iLastUpdateSentAt );
		MessageEnd();
	}

	// Now see if we should buy anything
	RecomputePurchases();
}

//------------------------------------------------------------------------------------------------------------------
// RESUPPLY BEACONS
//-----------------------------------------------------------------------------
// Purpose: Add the specified resupply beacon to this team. 
//-----------------------------------------------------------------------------
void CTFTeam::AddResupply( CObjectResupply *pResupply )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CTFTeam::AddResupply adding resupply %p to team %s\n", gpGlobals->curtime, 
		pResupply, GetName() ) );

	m_aResupplyBeacons.AddToTail( pResupply );
}

//-----------------------------------------------------------------------------
// Purpose: Remove this resupply beacon from the team
//-----------------------------------------------------------------------------
void CTFTeam::RemoveResupply( CObjectResupply *pResupply )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CTFTeam::RemoveResupply remove resupply %p from team %s\n", gpGlobals->curtime, 
		pResupply, GetName() ) );

	// Now remove the beacon from our list
	m_aResupplyBeacons.FindAndRemove( pResupply );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFTeam::GetNumObjects( int iObjectType )
{
	// Asking for a count of a specific object type?
	if ( iObjectType > 0 )
	{
		int iCount = 0;
		for ( int i = 0; i < GetNumObjects(); i++ )
		{
			CBaseObject *pObject = GetObject(i);
			if ( pObject && pObject->GetType() == iObjectType )
			{
				iCount++;
			}
		}
		return iCount;
	}

	return m_aObjects.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject *CTFTeam::GetObject( int num )
{
 	Assert( num >= 0 && num < m_aObjects.Count() );
	return m_aObjects[ num ];
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFTeam::GetNumResupplies( void )
{
	return m_aResupplyBeacons.Count();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectResupply *CTFTeam::GetResupply( int num )
{
	Assert( num >= 0 && num < m_aResupplyBeacons.Count() );
	return m_aResupplyBeacons[ num ];
}


bool CTFTeam::IsCoveredBySentryGun( const Vector &vPos )
{
	for( int i=0; i < m_aObjects.Count(); i++ )
	{
		CBaseObject *pObj = m_aObjects[i];
		
		if ( pObj->IsSentrygun() && vPos.DistTo( pObj->GetAbsOrigin() ) < OBJECT_COVERED_DIST )
			return true;

	}

	return false;
}


int CTFTeam::GetNumShieldWallsCoveringPosition( const Vector &vPos )
{
	int count = 0;

	for ( int i=0; i < m_aObjects.Count(); i++ )
	{
		CBaseObject *pObj = m_aObjects[i];
		
		if ( pObj->GetType() == OBJ_SHIELDWALL )
		{
			if ( vPos.DistTo( pObj->GetAbsOrigin() ) < OBJECT_COVERED_DIST )
				++count;
		}
	}

	return count;
}


int CTFTeam::GetNumResuppliesCoveringPosition( const Vector &vPos )
{
	int count = 0;

	for ( int i=0; i < m_aResupplyBeacons.Count(); i++ )
	{
		CBaseObject *pObj = m_aResupplyBeacons[i];
		if ( vPos.DistTo( pObj->GetAbsOrigin() ) < RESUPPLY_COVER_DIST )
			++count;
	}

	return count;
}


int CTFTeam::GetNumRespawnStationsCoveringPosition( const Vector &vPos )
{
	int count = 0;

	for ( int i=0; i < m_aObjects.Count(); i++ )
	{
		CBaseObject *pObj = m_aObjects[i];

		if ( pObj->GetType() == OBJ_RESPAWN_STATION )
		{
			if ( vPos.DistTo( pObj->GetAbsOrigin() ) < OBJECT_COVERED_DIST )
			{
				++count;
			}
		}
	}

	return count;
}


//------------------------------------------------------------------------------------------------------------------
// OBJECTS
//-----------------------------------------------------------------------------
// Purpose: Add the specified object to this team.
//-----------------------------------------------------------------------------
void CTFTeam::AddObject( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CTFTeam::AddObject adding object %p:%s to team %s\n", gpGlobals->curtime, 
		pObject, pObject->GetClassname(), GetName() ) );

	bool alreadyInList = IsObjectOnTeam( pObject );
	Assert( !alreadyInList );
	if ( !alreadyInList )
	{
		m_aObjects.AddToTail( pObject );
	}
}

//-----------------------------------------------------------------------------
// Returns true if the object is in the team's list of objects
//-----------------------------------------------------------------------------
bool CTFTeam::IsObjectOnTeam( CBaseObject *pObject ) const
{
	return ( m_aObjects.Find( pObject ) != -1 );
}


//-----------------------------------------------------------------------------
// Purpose: Remove this object from the team
//  Removes all references from all sublists as well
//-----------------------------------------------------------------------------
void CTFTeam::RemoveObject( CBaseObject *pObject )
{									   
	if ( m_aObjects.Count() <= 0 )
		return;

	if ( m_aObjects.Find( pObject ) != -1 )
	{
		TRACE_OBJECT( UTIL_VarArgs( "%0.2f CTFTeam::RemoveObject removing %p:%s from %s\n", gpGlobals->curtime, 
			pObject, pObject->GetClassname(), GetName() ) );

		m_aObjects.FindAndRemove( pObject );
	}
	else
	{
		TRACE_OBJECT( UTIL_VarArgs( "%0.2f CTFTeam::RemoveObject couldn't remove %p:%s from %s\n", gpGlobals->curtime, 
			pObject, pObject->GetClassname(), GetName() ) );
	}
}


//------------------------------------------------------------------------------------------------------------------
// ORDERS
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::InitializeOrders( void )
{
	m_flPersonalOrderUpdateTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Add a new order to our list. If it already exists, bump it's priority to the new priority.
//-----------------------------------------------------------------------------
COrder* CTFTeam::AddOrder( 
	int iOrderType, 
	CBaseEntity *pTarget, 
	CBaseTFPlayer *pPlayer, 
	float flDistanceToRemove,
	float flLifetime,
	COrder *pNewOrder
	)
{
	// Remove any orders to the player.
	RemoveOrdersToPlayer( pPlayer );

	// The new system requires order class to be passed in.
	Assert( pNewOrder );

	// All the order create functions should just use new to create the order class,
	// then we'll attach the edict in here. There's no reason to use LINK_ENTITY_TO_CLASS
	// and CreateEntityByName.
	Assert( !pNewOrder->edict() );
	pNewOrder->NetworkProp()->AttachEdict();
	
	pNewOrder->ChangeTeam( GetTeamNumber() );
	OrderHandle hOrder;
	hOrder = pNewOrder;
	m_aOrders.AddToTail( hOrder );

	// Update target
	pNewOrder->SetTarget( pTarget );
	pNewOrder->SetDistance( flDistanceToRemove );

	// Update lifetime.
	pNewOrder->SetLifetime( flLifetime );

	Assert( pPlayer->GetOrder() == NULL );

	pNewOrder->SetOwner( pPlayer );
	pPlayer->SetOrder( pNewOrder );

	// "New Order Received!"
	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	CBaseEntity::EmitSound( filter, pPlayer->entindex(),"TFTeam.AddOrder" );

	// Debug check.. it should never create an order with its termination conditions
	// already met.	
	Assert( !pNewOrder->Update() );

	return pNewOrder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::RemoveOrder( COrder *pOrder )
{
	OrderHandle hOrder;
	hOrder = pOrder;

	m_aOrders.FindAndRemove( hOrder );
}

//-----------------------------------------------------------------------------
// Purpose: Recalculate the team's orders & their priorities
//-----------------------------------------------------------------------------
void CTFTeam::RecalcOrders( void )
{
	// Update all existing orders
	UpdateOrders();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::UpdateOrders( void )
{
	// Tell all our current orders to update themselves. Walk backwards because we may remove them.
	int iSize = m_aOrders.Count();
	for (int i = iSize-1; i >= 0; i--)
	{
		// Orders without owners should be removed
		bool bShouldRemove = ( 
			!m_aOrders[i] || 
			!m_aOrders[i]->GetOwner() || 
			m_aOrders[i]->Update() );
		if ( bShouldRemove )
		{
			COrder *pOrder = m_aOrders[i];
			m_aOrders.Remove( i );
			UTIL_Remove( pOrder );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: An event has just occurred that affects orders. Tell all our orders that
//			have the specified entity as a target.
//-----------------------------------------------------------------------------
void CTFTeam::UpdateOrdersOnEvent( COrderEvent_Base *pOrder )
{
	// Tell all our current orders to update themselves. Walk backwards because we may remove them.
	int iSize = m_aOrders.Count();
	for (int i = iSize-1; i >= 0; i--)
	{
		bool bShouldRemove = m_aOrders[i]->UpdateOnEvent( pOrder );
		if ( bShouldRemove )
		{
			COrder *pOrder = m_aOrders[i];
			m_aOrders.Remove( i );
			UTIL_Remove( pOrder );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create personal orders for all the team's members
//-----------------------------------------------------------------------------
void CTFTeam::CreatePersonalOrders( void )
{
	// Create personal orders for each player
	for ( int i = 0; i < m_aPlayers.Count(); i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)m_aPlayers[i];

		// Don't create orders for bots, undefined or dead people
		if ( !(pPlayer->GetFlags() & FL_FAKECLIENT) && pPlayer->IsAlive() && !pPlayer->IsClass( TFCLASS_UNDECIDED ) )
		{
			CreatePersonalOrder( pPlayer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create personal orders for specified player
//-----------------------------------------------------------------------------
void CTFTeam::CreatePersonalOrder( CBaseTFPlayer *pPlayer )
{
	// We still haven't made a personal order, so ask the class if it wants to
	if ( pPlayer->GetPlayerClass() )
	{
		pPlayer->GetPlayerClass()->CreatePersonalOrder();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::RemoveOrdersToPlayer( CBaseTFPlayer *pPlayer )
{
	// Walk backwards because we're removing them.
	int iSize = m_aOrders.Count();
	for (int i = iSize-1; i >= 0; i--)
	{
		// Orders without owners should be removed
		if ( m_aOrders[i].Get() )
		{
			if( m_aOrders[i]->GetOwner() == pPlayer )
			{
				COrder *pOrder = m_aOrders[i];
				m_aOrders.Remove( i );
				
				pOrder->DetachFromPlayer();
				UTIL_Remove( pOrder );
			}
		}
		else
		{
			m_aOrders.Remove( i );
		}
	}

	pPlayer->SetOrder( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Count and return the number of orders of the type with the specified target
//-----------------------------------------------------------------------------
int CTFTeam::CountOrders( int flags, int iOrderType, CBaseEntity *pTarget, CBaseTFPlayer *pOwner )
{
	int iOrderCount = 0;

	// Count the number of global orders
	for ( int i = 0; i < m_aOrders.Count(); i++ )
	{
		COrder *pOrder = m_aOrders[i];

		if( flags & COUNTORDERS_TYPE )
			 if( pOrder->GetType() != iOrderType )
				continue;

		if( flags & COUNTORDERS_TARGET )
			if( pOrder->GetTargetEntity() != pTarget )
				continue;

		if( flags & COUNTORDERS_OWNER )
			if( pOrder->GetOwner() != pOwner )
				continue;
		
		// Ok, this order matches the criteria.
		iOrderCount++;
	}

	return iOrderCount;
}


int CTFTeam::CountOrdersOwnedByPlayer( CBaseTFPlayer *pPlayer )
{
	return CountOrders( COUNTORDERS_OWNER, 0, 0, pPlayer );
}


//------------------------------------------------------------------------------------------------------------------
// MESSAGES
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::ClearMessages( void )
{
	int iSize = m_aMessages.Count();
	for (int i = iSize-1; i >= 0; i--)
	{
		CTeamMessage *pMessage = m_aMessages[i];
		m_aMessages.Remove( i );
		delete pMessage;
	}

	m_aMessages.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Post a message of the specified type
//-----------------------------------------------------------------------------
void CTFTeam::PostMessage( int iMessageID, CBaseEntity *pEntity, char *sData )
{
	// First see if we've got this message in the queue already
	for ( int i = 0; i < m_aMessages.Count(); i++ )
	{
		CTeamMessage *pMessage = m_aMessages[i];
		if ( (pMessage->GetID() == iMessageID) && (pMessage->GetEntity() == pEntity) )
		{
			// Already in the queue, abort.
			return;
		}
	}

	// Create a new message and add it to my tail
	CTeamMessage *pMessage = CTeamMessage::Create( this, iMessageID, pEntity );
	if ( sData && sData[0] )
	{
		pMessage->SetData( sData );
	}
	m_aMessages.AddToTail( pMessage );

	// Tell the message to fire
	pMessage->FireMessage();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeam::UpdateMessages( void )
{
	// Go through my messages and kill any that have reached their TTL
	int iSize = m_aMessages.Count();
	for (int i = iSize-1; i >= 0; i--)
	{
		CTeamMessage *pMessage = m_aMessages[i];
		if ( gpGlobals->curtime > pMessage->GetTTL() )
		{
			m_aMessages.Remove( i );
			delete pMessage;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tell all our powerpacks to update their powered objects.
//			pPackToIgnore: Pack to ignore, because it's dying.
//			pObjectToTarget: An object looking for power, because it's being placed
//-----------------------------------------------------------------------------
void CTFTeam::UpdatePowerpacks( CObjectPowerPack *pPackToIgnore, CBaseObject *pObjectToTarget )
{
	for ( int i = 0; i < GetNumObjects(); i++ )
	{
		CBaseObject *pObject = GetObject(i);
		assert(pObject);
		if ( pObject == pPackToIgnore || pObject->GetType() != OBJ_POWERPACK )
			continue;

		((CObjectPowerPack*)pObject)->PowerNearbyObjects( pObjectToTarget );

		// Quit as soon as we've powered the specified one, if there is one
		if ( pObjectToTarget && pObjectToTarget->IsPowered() )
			break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tell all our buff stations to update and look for objects.
//   Input:	pBuffStationToIgnore: Buff station to ignore, because it is dying
//          pObjectToTarget: an object looking for a buff station, because it is being placed
//-----------------------------------------------------------------------------
void CTFTeam::UpdateBuffStations( CObjectBuffStation *pBuffStationToIgnore, CBaseObject *pObjectToTarget, bool bPlacing )
{
	for ( int iObject = 0; iObject < GetNumObjects(); ++iObject )
	{
		CBaseObject *pObject = GetObject( iObject );
		assert( pObject );
	
		if ( pObject->GetType() != OBJ_BUFF_STATION )
			continue;

		CObjectBuffStation *pBuffStation = static_cast<CObjectBuffStation*>( pObject );
		if ( pBuffStation == pBuffStationToIgnore )
			continue;

		pBuffStation->BuffNearbyObjects( pObjectToTarget, bPlacing );

		// Quit as soon as we've powered the specified one, if there is one.
		if ( pObjectToTarget && pObjectToTarget->IsHookedAndBuffed() )
			break;
	}
}

//------------------------------------------------------------------------------------------------------------------
// UTILITY FUNCS
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFTeam* CTFTeam::GetEnemyTeam()
{
	// Look for nearby enemy objects we can capture.
	int iMyTeam = GetTeamNumber();
	if( iMyTeam == 0 )
		return NULL;

	int iEnemyTeam = !(iMyTeam - 1) + 1;
	return (CTFTeam*)GetGlobalTeam( iEnemyTeam );
}


