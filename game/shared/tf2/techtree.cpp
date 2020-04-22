//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared interface to the tech tree & individual technologies
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#ifndef CLIENT_DLL
#include "tf_player.h"
#include "tf_team.h"
#include "info_customtech.h"
#endif
#include "techtree.h"

bool ParseTechnologyFile( CUtlVector< CBaseTechnology* > &pTechnologyList, IFileSystem* pFileSystem, int nTeamNumber, char *sFileName );

// Color codes for resources
rescolor sResourceColor = { 64,	255, 64 };

// Prototype names for resources
char sResourceName[] = "Jojierium";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTechnology::CBaseTechnology( void )
{
	m_nTechLevel = 0;
	ZeroPreferences();
	SetAvailable( false );
	SetActive( false );
	SetPreferred( false );
	SetVoters( 0 );
	SetCost( 0 );
	SetDirty( false );
	m_fResourceLevel = 0;
	memset( m_ClassResults, 0, sizeof( m_ClassResults ) );
	memset( m_pszName, 0, sizeof( m_pszName ) );
	memset( m_pszPrintName, 0, sizeof( m_pszPrintName ) );
	memset( m_pszDescription, 0, sizeof( m_pszDescription ) );
	memset( m_pszTeamSoundFile, 0, sizeof( m_pszTeamSoundFile ) );
	memset( m_apszContainedTechs, 0, sizeof( m_apszContainedTechs ) );
	memset( m_pContainedTechs, 0, sizeof(m_pContainedTechs) );
	memset( m_apszDependentTechs, 0, sizeof( m_apszDependentTechs ) );
	memset( m_pDependentTechs, 0, sizeof(m_pDependentTechs) );
	m_iContainedTechs = 0;
	m_iDependentTechs = 0;
	m_iTeamSound = 0;
	m_bGoalTechnology = false;
	m_bClassUpgrade = false;
	m_bVehicle = false;
	m_bTechLevelUpgrade = false;
	m_bResourceTech = false;
	
	memset( m_szTextureName, 0, sizeof( m_szTextureName ) );
	m_nTextureID = 0;

	memset( m_szButtonName, 0, sizeof( m_szButtonName ) );

	m_nNumWeaponAssociations = 0;
	memset( m_rgszWeaponAssociation, 0, sizeof( m_rgszWeaponAssociation ) );

	SetHidden( false );
	ResetHintsGiven();
}	

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTechnology::~CBaseTechnology( void )
{
}

static bool NameStartsWith( const char *name, const char *prefix )
{
	if ( !name || !name[ 0 ] || !prefix || !prefix[ 0 ] )
		return false;

	if ( !strnicmp( name, prefix, strlen( prefix ) ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Set the technology name
//-----------------------------------------------------------------------------
void CBaseTechnology::SetName( const char *pName )
{
	Q_strncpy( m_pszName, pName, sizeof(m_pszName) );

	// Determine special information about this technology
	m_bClassUpgrade		= NameStartsWith( pName, "class_" );
	m_bVehicle			= NameStartsWith( pName, "vehicle_" );
	m_bTechLevelUpgrade	= NameStartsWith( pName, "tech_level_" ); 
	// HACK: Assume global techs relate to resources for now
	m_bResourceTech		= NameStartsWith( pName, "g_" );
}

//-----------------------------------------------------------------------------
// Purpose: Set the technology print name
//-----------------------------------------------------------------------------
void CBaseTechnology::SetPrintName( const char *pName )
{
	Q_strncpy( m_pszPrintName, pName, sizeof(m_pszPrintName) );
}

//-----------------------------------------------------------------------------
// Purpose: Set the technology description
//-----------------------------------------------------------------------------
void CBaseTechnology::SetDescription( const char *pDesc )
{
	Q_strncpy( m_pszDescription, pDesc, sizeof(m_pszDescription) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetButtonName( const char *pName )
{
	Q_strncpy( m_szButtonName, pName, sizeof(m_szButtonName) );
}

//-----------------------------------------------------------------------------
// Purpose: Set the technology level
//-----------------------------------------------------------------------------
void CBaseTechnology::SetLevel( int iLevel )
{
	m_nTechLevel = iLevel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *texture - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetTextureName( const char *texture )
{
	Q_strncpy( m_szTextureName, texture, sizeof( m_szTextureName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetTextureId( int id )
{
	m_nTextureID = id;
}

//-----------------------------------------------------------------------------
// Purpose: Set the technologies resource costs
//-----------------------------------------------------------------------------
void CBaseTechnology::SetCost( float fResourceCost )
{
	m_fResourceCost = fResourceCost;

	RecalculateOverallLevel();
}

//-----------------------------------------------------------------------------
// Purpose: Set a specific class's sound
//-----------------------------------------------------------------------------
void CBaseTechnology::SetClassResultSound( int iClass, const char *pSound )
{
	if (!pSound)
		return;

	Assert( iClass >= 0 && iClass < TFCLASS_CLASS_COUNT );

	// Class of 0 is the team's sound file
	if ( iClass == 0 )
	{
		Q_strncpy( m_pszTeamSoundFile, pSound, sizeof(m_pszTeamSoundFile) );
	}
	else
	{
		m_ClassResults[iClass].bClassTouched = true;
		Q_strncpy( m_ClassResults[iClass].pszSoundFile, pSound, sizeof(m_ClassResults[iClass].pszSoundFile) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : associate - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetClassResultAssociateWeapons( int iClass, bool associate )
{
	Assert( iClass >= 0 && iClass < TFCLASS_CLASS_COUNT );

	m_ClassResults[iClass].m_bAssociateWeaponsForClass = associate;
}

//-----------------------------------------------------------------------------
// Purpose: Set a specific class's precached sound 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetClassResultSound( int iClass, int iSound )
{
	Assert( iClass >= 0 && iClass < TFCLASS_CLASS_COUNT );

	// Class of 0 is the team's sound file
	if ( iClass == 0 )
	{
		m_iTeamSound = iSound;
	}
	else
	{
		m_ClassResults[iClass].iSound = iSound;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set a specific class's description
//-----------------------------------------------------------------------------
void CBaseTechnology::SetClassResultDescription( int iClass, const char *pDesc )
{
	if (!pDesc)
		return;

	Assert( iClass > 0 && iClass < TFCLASS_CLASS_COUNT );
	m_ClassResults[iClass].bClassTouched = true;

	Q_strncpy( m_ClassResults[iClass].pszDescription, pDesc, sizeof(m_ClassResults[iClass].pszDescription)  );
}

//-----------------------------------------------------------------------------
// Purpose: Add a technology contained within this one
//-----------------------------------------------------------------------------
void CBaseTechnology::AddContainedTechnology( const char *pszTech )
{
	Q_strncpy( m_apszContainedTechs[ m_iContainedTechs ], pszTech, sizeof(m_apszContainedTechs[0])  );
	m_iContainedTechs++;
}

//-----------------------------------------------------------------------------
// Purpose: Add a technology dependency within this one
//-----------------------------------------------------------------------------
void CBaseTechnology::AddDependentTechnology( const char *pszTech )
{
	Q_strncpy( m_apszDependentTechs[ m_iDependentTechs ], pszTech, sizeof(m_apszDependentTechs[0])  );
	m_iDependentTechs++;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this technology affects the specified class
//-----------------------------------------------------------------------------
bool CBaseTechnology::AffectsClass( int iClass )
{
	// If this technology directly affects this class, return true
	if ( m_ClassResults[ iClass ].bClassTouched )
		return true;

	// If not, do any of our contained techs affect the specified class
	for (int i = 0; i < m_iContainedTechs; i++ )
	{
		if ( m_pContainedTechs[i] )
		{
			if ( m_pContainedTechs[i]->AffectsClass( iClass ) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTechnology::IsClassUpgrade( void )
{
	return m_bClassUpgrade;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTechnology::IsVehicle( void )
{
	return m_bVehicle;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTechnology::IsResourceTech( void )
{
	return m_bResourceTech;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTechnology::IsTechLevelUpgrade( void )
{
	return m_bTechLevelUpgrade;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the level to which this technology belongs
// Output : int
//-----------------------------------------------------------------------------
int CBaseTechnology::GetLevel( void )
{
	return m_nTechLevel;
}

//-----------------------------------------------------------------------------
// Purpose: Cost of technology in resource specified
//-----------------------------------------------------------------------------
float CBaseTechnology::GetResourceCost( void )
{
	return m_fResourceCost;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves the current amount of resources spent on the technology
//-----------------------------------------------------------------------------
float CBaseTechnology::GetResourceLevel( void )
{
	return m_fResourceLevel;
}

//-----------------------------------------------------------------------------
// Purpose: Sets a resource level to an amount
//-----------------------------------------------------------------------------
void CBaseTechnology::SetResourceLevel( float flResourceLevel )
{
	if ( m_fResourceLevel == flResourceLevel )
		return;

	m_fResourceLevel = flResourceLevel;

	// Update my level & watchers
	RecalculateOverallLevel();
	UpdateWatchers();

	// Force me to be resent to clients
	SetDirty( true );
}

//-----------------------------------------------------------------------------
// Purpose: Increase the level of the specified resource spent on this technology
// Output : Returns true if the technology's had enough resources to be bought
//-----------------------------------------------------------------------------
bool CBaseTechnology::IncreaseResourceLevel( float flResourcesToSpend )
{
	SetResourceLevel( m_fResourceLevel + flResourcesToSpend );

	// Have my costs been met?
	if ( GetResourceLevel() < GetResourceCost() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Figure out my overall owned percentage 
//-----------------------------------------------------------------------------
void CBaseTechnology::RecalculateOverallLevel( void )
{
	if ( !GetResourceCost() )
	{
		m_flOverallOwnedPercentage = 0;
	}
	else
	{
		m_flOverallOwnedPercentage = GetResourceLevel() / GetResourceCost();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CBaseTechnology::GetOverallLevel( void )
{
	return m_flOverallOwnedPercentage;
}

//-----------------------------------------------------------------------------
// Purpose: Force this technology to complete itself
//-----------------------------------------------------------------------------
void CBaseTechnology::ForceComplete( void )
{
	SetResourceLevel( GetResourceCost() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTechnology::IsAGoalTechnology( void )
{
	return m_bGoalTechnology;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetGoalTechnology( bool bGoal )
{
	m_bGoalTechnology = bGoal;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the name of this technology
// Output : const
//-----------------------------------------------------------------------------
const char	*CBaseTechnology::GetName( void )
{
	return m_pszName;
}

//-----------------------------------------------------------------------------
// Purpose: Returns printable name of this technology
// Output : const
//-----------------------------------------------------------------------------
const char	*CBaseTechnology::GetPrintName( void )
{
	return m_pszPrintName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseTechnology::GetButtonName( void )
{
	return m_szButtonName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseTechnology::GetTextureName(void )
{
	return m_szTextureName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CBaseTechnology::GetTextureId( void )
{
	return m_nTextureID;
}

//-----------------------------------------------------------------------------
// Purpose: Returns description for item
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseTechnology::GetDescription( int iPlayerClass )
{
	// If we have a custom description for the local player's class, return that instead
	if ( AffectsClass( iPlayerClass ) )
	{
		if ( m_ClassResults[iPlayerClass].pszDescription )
			return m_ClassResults[iPlayerClass].pszDescription;
	}

	// Otherwise, return the general description
	return m_pszDescription;
}

//-----------------------------------------------------------------------------
// Purpose: Returns sound filename to play for this technology
//-----------------------------------------------------------------------------
const char *CBaseTechnology::GetSoundFile( int iClass )
{
	// Class of 0 is the team sound
	if ( !iClass )
		return m_pszTeamSoundFile;

	Assert(iClass > 0 && iClass < TFCLASS_CLASS_COUNT );
	return m_ClassResults[iClass].pszSoundFile;
}

//-----------------------------------------------------------------------------
// Purpose: Returns sound to play for this technology
//-----------------------------------------------------------------------------
int	CBaseTechnology::GetSound( int iClass )
{
	// Class of 0 is the team sound
	if ( !iClass )
		return m_iTeamSound;

	Assert(iClass > 0 && iClass < TFCLASS_CLASS_COUNT );
	return m_ClassResults[iClass].iSound;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iClass - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTechnology::GetAssociateWeaponsForClass( int iClass )
{
	if ( !iClass )
		return false;

	Assert(iClass > 0 && iClass < TFCLASS_CLASS_COUNT );
	return m_ClassResults[iClass].m_bAssociateWeaponsForClass;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the technology is available to the team
// Input  : state - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetAvailable( bool state )
{
	if ( state != m_bAvailable )
	{
		SetDirty( true );
	}

	m_bAvailable = state;
}

//-----------------------------------------------------------------------------
// Purpose: Determine whether team posses the technology
// Output : Returns 1 if available, 0 otherwise
//-----------------------------------------------------------------------------
int CBaseTechnology::GetAvailable( void )
{
	return m_bAvailable;
}

//-----------------------------------------------------------------------------
// Purpose: Zero out team preference counters
//-----------------------------------------------------------------------------
void CBaseTechnology::ZeroPreferences( void )
{
	if ( m_nPreferenceCount )
	{
		SetDirty( true );
	}

	m_nPreferenceCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Increment preference counter
//-----------------------------------------------------------------------------
void CBaseTechnology::IncrementPreferences( void )
{
	m_nPreferenceCount++;
	SetDirty( true );
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve preference count
//-----------------------------------------------------------------------------
int CBaseTechnology::GetPreferenceCount( void )
{
	return m_nPreferenceCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CBaseTechnology::GetNumWeaponAssociations( void )
{
	return m_nNumWeaponAssociations;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CBaseTechnology::GetAssociatedWeapon( int index )
{
	if ( index < 0 || index >= m_nNumWeaponAssociations )
	{
		return "";
	}

	return m_rgszWeaponAssociation[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *weaponname - 
//-----------------------------------------------------------------------------
void CBaseTechnology::AddAssociatedWeapon( const char *weaponname )
{
	if ( m_nNumWeaponAssociations >= MAX_ASSOCIATED_WEAPONS )
	{
		Assert( !"CBaseTechnology::AddAssociatedWeapon:  m_nNumWeaponAssociations >= MAX_ASSOCIATED_WEAPONS" );
		return;
	}
	Q_strncpy( m_rgszWeaponAssociation[ m_nNumWeaponAssociations++ ], weaponname, TECHNOLOGY_WEAPONNAME_LENGTH );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseTechnology::GetNumberContainedTechs( void )
{
	return m_iContainedTechs;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CBaseTechnology::GetContainedTechName( int iTech )
{
	Assert( iTech >= 0 && iTech < m_iContainedTechs );
	return m_apszContainedTechs[ iTech ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetContainedTech( int iTech, CBaseTechnology *pTech )
{
	Assert( iTech >= 0 && iTech < m_iContainedTechs );
	m_pContainedTechs[iTech] = pTech;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseTechnology::GetNumberDependentTechs( void )
{
	return m_iDependentTechs;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CBaseTechnology::GetDependentTechName( int iTech )
{
	Assert( iTech >= 0 && iTech < m_iDependentTechs );
	return m_apszDependentTechs[ iTech ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetDependentTech( int iTech, CBaseTechnology *pTech )
{
	Assert( iTech >= 0 && iTech < m_iDependentTechs );
	Assert( pTech );
	m_pDependentTechs[iTech] = pTech;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this tech depends on the specified tech
//-----------------------------------------------------------------------------
bool CBaseTechnology::DependsOn( CBaseTechnology *pTech )
{
	for ( int i = 0; i < GetNumberDependentTechs(); i++ )
	{
		if ( m_pDependentTechs[i] == pTech )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this tech has a dependent tech that's not been completed yet
//-----------------------------------------------------------------------------
bool CBaseTechnology::HasInactiveDependencies( void )
{
	for ( int i = 0; i < GetNumberDependentTechs(); i++ )
	{
		// This condition can occur if there's a bug in the .txt file
		// where a tech is told to depend on a non-existent tech
		if (!m_pDependentTechs[i])
			continue;

		// Client uses m_bActive, Server uses m_bAvailable (?)
		if ( m_pDependentTechs[i]->GetAvailable() == false && m_pDependentTechs[i]->GetActive() == false )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Dirty bit, used for fast knowledge of when to resend techs
//-----------------------------------------------------------------------------
bool CBaseTechnology::IsDirty( void )
{
	return m_bDirty;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetDirty( bool bDirty )
{
	m_bDirty = bDirty;
}

//-----------------------------------------------------------------------------
// Purpose: Set availability state for item
// Input  : state - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetActive( bool state )
{
	m_bActive = state;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve availability state
//-----------------------------------------------------------------------------
bool CBaseTechnology::GetActive( void )
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Purpose: Set whether this is our local preferred item
// Input  : state - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetPreferred( bool state )
{
	m_bPreferred = state;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve local preferred state
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTechnology::GetPreferred( void )
{
	return m_bPreferred;
}

//-----------------------------------------------------------------------------
// Purpose: Set the number of players preferring this tech
// Input  : voters - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetVoters( int voters )
{
	m_nVoters = voters;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of players preferring this tech
// Output : int
//-----------------------------------------------------------------------------
int CBaseTechnology::GetVoters( void )
{
	return m_nVoters;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hide - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetHidden( bool hide )
{
	m_bHidden = hide;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTechnology::IsHidden( void )
{
	return m_bHidden;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTechnology::ResetHintsGiven( void )
{
	m_HintsGiven.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTechnology::GetHintsGiven( int type )
{
	if ( m_HintsGiven.Find( type ) != m_HintsGiven.InvalidIndex() )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : given - 
//-----------------------------------------------------------------------------
void CBaseTechnology::SetHintsGiven( int type, bool given )
{
	if ( given )
	{
		if ( m_HintsGiven.Find( type ) != m_HintsGiven.InvalidIndex() )
			return;

		m_HintsGiven.AddToTail( type );
	}
	else
	{
		int idx = m_HintsGiven.Find( type );
		if ( idx == m_HintsGiven.InvalidIndex() )
			return;

		m_HintsGiven.Remove( idx );
	}
}

//====================================================================================================================
// EVIL GAME DLL ONLY CODE FOR TECHNOLOGIES
//====================================================================================================================
#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTechnology::AddTechnologyToPlayer( CBaseTFPlayer *pPlayer )
{
	// Tell playerclasses listed to recalculate their technologies, only if they're listed in the class results
	if ( pPlayer->PlayerClass() )
	{
		if ( m_ClassResults[ pPlayer->PlayerClass() ].bClassTouched )
		{
			CPlayerClass *pPlayerClass = pPlayer->GetPlayerClass();
			pPlayerClass->GainedNewTechnology( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTechnology::AddTechnologyToTeam( CTFTeam *pTeam )
{
	SetAvailable( true );

	// Enable all the technologies this group contains
	if ( m_iContainedTechs )
	{
		for (int i = 0; i < m_iContainedTechs; i++ )
		{
			if ( m_pContainedTechs[i] )
			{
				pTeam->EnableTechnology( m_pContainedTechs[i] );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: A technology watcher entity wants to register as a watcher for this technology
//-----------------------------------------------------------------------------
void CBaseTechnology::RegisterWatcher( CInfoCustomTechnology *pWatcher )
{
	m_aWatchers.AddToTail( pWatcher );
	pWatcher->UpdateTechPercentage( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTechnology::UpdateWatchers( void )
{
	// Tell all my watchers
	for (int i = 0; i < m_aWatchers.Size(); i++ )
	{
		m_aWatchers[i]->UpdateTechPercentage( GetOverallLevel() );
	}
}
#else
//-----------------------------------------------------------------------------
// Purpose: Client DLL UpdateWatchers does nothing
//-----------------------------------------------------------------------------
void CBaseTechnology::UpdateWatchers( void )
{
}
#endif




//====================================================================================================================
// SHARED TECHNOLOGY TREE
//====================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: Construct raw technology tree
// Output : 
//-----------------------------------------------------------------------------
CTechnologyTree::CTechnologyTree( IFileSystem* pFileSystem, int nTeamNumber )
{
	// Reset preference counter
	ClearPreferenceCount();

	// Parse the list from the data file
	if ( ParseTechnologyFile( m_Technologies, pFileSystem, nTeamNumber, "scripts/technologytree.txt" ) == false )
	{
		Assert(0);
		return;
	}

	LinkContainedTechnologies();
	LinkDependentTechnologies();
}

//-----------------------------------------------------------------------------
// Purpose: Link all the contained technologies
//-----------------------------------------------------------------------------
void CTechnologyTree::LinkContainedTechnologies( void )
{
	for ( int i=0; i < m_Technologies.Size(); i++)
	{
		for ( int j = 0; j < m_Technologies[i]->GetNumberContainedTechs(); j++ )
		{
			const char *pName = m_Technologies[i]->GetContainedTechName( j );
			CBaseTechnology *pTech = GetTechnology( pName );
			if ( pTech )
			{
				m_Technologies[i]->SetContainedTech( j, pTech );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Link all the dependent technologies
//-----------------------------------------------------------------------------
void CTechnologyTree::LinkDependentTechnologies( void )
{
	for ( int i=0; i < m_Technologies.Size(); i++)
	{
		for ( int j = 0; j < m_Technologies[i]->GetNumberDependentTechs(); j++ )
		{
			const char *pName = m_Technologies[i]->GetDependentTechName( j );
			CBaseTechnology *pTech = GetTechnology( pName );
			if ( pTech )
			{
				m_Technologies[i]->SetDependentTech( j, pTech );
			}
			else
			{
				Warning("Unable to find dependent technology %s!\n", pName );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTechnologyTree::~CTechnologyTree( void )
{
	Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: Delete all items in the tree
//-----------------------------------------------------------------------------
void CTechnologyTree::Shutdown( void )
{
	// Loop through all used items
	int iSize = m_Technologies.Size();
	for ( int i = iSize-1; i >= 0; i-- )
	{
		CBaseTechnology *pItem = m_Technologies[ i ];
		delete pItem;
	}
	m_Technologies.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Add a new technology to the tree
//-----------------------------------------------------------------------------
void CTechnologyTree::AddTechnologyFile( IFileSystem* pFileSystem, int nTeamNumber, char *sFileName )
{
	ParseTechnologyFile( m_Technologies, pFileSystem, nTeamNumber, sFileName );
}

//-----------------------------------------------------------------------------
// Purpose: Get the index of the specified item
//-----------------------------------------------------------------------------
int	CTechnologyTree::GetIndex( CBaseTechnology *pItem )
{
	for ( int i=0; i < m_Technologies.Size(); i++)
	{
		if ( m_Technologies[i] == pItem )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve item by index
//-----------------------------------------------------------------------------
CBaseTechnology *CTechnologyTree::GetTechnology( int index )
{
	if ( index < 0 || index >= m_Technologies.Size() )
		return NULL;

	return m_Technologies[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTechnology* CTechnologyTree::GetTechnology( const char *pName )
{
	for ( int i=0; i < m_Technologies.Size(); i++)
	{
		if( stricmp( pName, m_Technologies[i]->GetName() ) == 0 )
			return m_Technologies[i];
	}
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return current number of objects
// Output : int
//-----------------------------------------------------------------------------
int CTechnologyTree::GetNumberTechnologies( void )
{
	return m_Technologies.Size();
}

//-----------------------------------------------------------------------------
// Purpose: Set preferred item ( turns off all other preferences first )
//-----------------------------------------------------------------------------
void CTechnologyTree::SetPreferredTechnology( CBaseTechnology *pItem )
{
	// Turn all of the others off
	for ( int i = 0; i < m_Technologies.Size(); i++ )
	{
		CBaseTechnology *item = m_Technologies[ i ];
		item->SetPreferred( false );
	}

	// Turn this one on
	if ( pItem )
	{
		pItem->SetPreferred( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the technology preferred by this client
//-----------------------------------------------------------------------------
CBaseTechnology* CTechnologyTree::GetPreferredTechnology( void )
{
	// Turn all of the others off
	for ( int i = 0; i < m_Technologies.Size(); i++ )
	{
		CBaseTechnology *item = m_Technologies[ i ];
		if ( item->GetPreferred() )
			return item;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of players who've voted on techs
//-----------------------------------------------------------------------------
int	CTechnologyTree::GetPreferenceCount( void )
{
	return m_nPreferenceCount;
}

//-----------------------------------------------------------------------------
// Purpose: Zero global preference counters
//-----------------------------------------------------------------------------
void CTechnologyTree::ClearPreferenceCount( void )
{
	m_nPreferenceCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Increment preference counter
//-----------------------------------------------------------------------------
void CTechnologyTree::IncrementPreferences( void )
{
	m_nPreferenceCount++;
}

//-----------------------------------------------------------------------------
// Purpose: Return the most voted-for technologies 
// Input  : iDesireLevel: 1 = Most voted for, 2 = 2nd most voted for, etc
//-----------------------------------------------------------------------------
CBaseTechnology *CTechnologyTree::GetDesiredTechnology( int iDesireLevel )
{
	Assert( iDesireLevel > 0 && iDesireLevel < m_Technologies.Size() );

	// Dump the techs into a temporary array
	CBaseTechnology *pSortedTechs[ MAX_TECHNOLOGIES ];
	int iMaxTech = 0;
	for ( int i = 0; i < m_Technologies.Size(); i++ )
	{
		// Skip Techs that are already available
		CBaseTechnology *technology = m_Technologies[i];
		if(!technology)
			continue;

		if ( technology->GetAvailable() == false )
		{
			pSortedTechs[iMaxTech] = m_Technologies[i];
			iMaxTech++;
		}
	}

	// Not enough unresearched techs?
	if ( iMaxTech < iDesireLevel )
		return NULL;

	// Bubble sort the tech array into order of desire
	int swapped = 1;
	while ( swapped )
	{
		swapped = 0;
		for ( int i = 1; i < iMaxTech; i++ )
		{
			if ( pSortedTechs[i]->GetPreferenceCount() > pSortedTechs[i-1]->GetPreferenceCount() )
			{
				CBaseTechnology *pTemp = pSortedTechs[i];
				pSortedTechs[i] = pSortedTechs[i-1];
				pSortedTechs[i-1] = pTemp;
				swapped = 1;
			}
		}
	}

	return pSortedTechs[ iDesireLevel - 1 ];
}

//-----------------------------------------------------------------------------
// Purpose: Find out what percentage of techs in a given level this team owns
//-----------------------------------------------------------------------------
float CTechnologyTree::GetPercentageOfTechLevelOwned( int iTechLevel )
{
	float fTotalTechs = 0;
	float fTechsOwned = 0;

	for ( int i = 0; i < m_Technologies.Size(); i++ )
	{
		CBaseTechnology *technology = m_Technologies[i];
		if ( !technology )
			continue;

		if ( technology->GetLevel() != iTechLevel )
			continue;

		fTotalTechs++;

		// Do we have it?
		if ( technology->GetAvailable() )
		{
			fTechsOwned++;
		}
	}

	if ( !fTotalTechs )
		return 0.0;

	return ( fTechsOwned / fTotalTechs );
}
