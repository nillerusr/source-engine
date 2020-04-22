//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( TECHTREE_H )
#define TECHTREE_H
#ifdef _WIN32
#pragma once
#endif


// Evil, Game DLL only code
#ifndef CLIENT_DLL
class CBaseTFPlayer;
class CTFTeam;
class CInfoCustomTechnology;
#endif

class IFileSystem;

//===========================================================================================
// Technology tree defines
#define MAX_TF_TECHLEVELS				6		// Number of TF2 tech levels
#define TECHLEVEL_PERCENTAGE_NEEDED		0.5		// Percentage of a tech level that must be owned before the next tech level becomes available
#define MAX_TECHNOLOGIES				128		// Max number of resource types
#define MAX_ASSOCIATED_WEAPONS			2		// Max number of weapons that a tech can be associated with

// Indexes into resource arrays
#define NORMAL_RESOURCES				0
#define PROCESSED_RESOURCES				1
#define RESOURCE_TYPES					2

// Tech
#define TECHNOLOGY_NAME_LENGTH				64		// Max length of a tech name
#define TECHNOLOGY_PRINTNAME_LENGTH			128		// Max length of a tech's print name
#define TECHNOLOGY_DESC_LENGTH				256		// Max length of a tech's description
#define MAX_CONTAINED_TECHNOLOGIES			16		// Max number of technologies that can be contained within another technology
#define MAX_DEPENDANT_TECHNOLOGIES			16		// Max number of technologies that a tech can depend on
#define TECHNOLOGY_SOUNDFILENAME_LENGTH		256
#define TECHNOLOGY_TEXTURENAME_LENGTH		128
#define TECHNOLOGY_BUTTONNAME_LENGTH		64
#define TECHNOLOGY_WEAPONNAME_LENGTH		128

// Color codes for Resources
struct rescolor
{
	int r;
	int g;
	int b;
};

// Class result structure for technologies
struct classresult_t
{
	bool	bClassTouched;									// This technology directly affects this class
	char	pszSoundFile[TECHNOLOGY_SOUNDFILENAME_LENGTH];	// Filename of the sound
	int		iSound;											// Sound played to members of this class when this technology is achieved
	char	pszDescription[TECHNOLOGY_DESC_LENGTH];			// Description for this technology shown only to this class

	// If true, then we should determine what weapons should be given to the player
	//  of this class when this technology is received by looking at the "associated_weapons"
	// data
	bool	m_bAssociateWeaponsForClass;
};

extern char		sResourceName[32];
extern rescolor sResourceColor;


#include "utlvector.h"
#include "tf_shareddefs.h"


//===========================================================================================
//-----------------------------------------------------------------------------
// Purpose: A Technology
//-----------------------------------------------------------------------------
class CBaseTechnology
{
public:
	// Constructions
						CBaseTechnology( void );
	virtual				~CBaseTechnology( void );

	// Data read from the data file
	virtual void		SetName( const char *pName );
	virtual void		SetPrintName( const char *pName );
	virtual void		SetDescription( const char *pDesc );
	virtual void		SetButtonName( const char *pName );
	virtual void		SetLevel( int iLevel );
	virtual void		SetCost( float fResourceCost );
	virtual void		SetClassResultSound( int iClass, const char *pSound );
	virtual void		SetClassResultSound( int iClass, int iSound );
	virtual void		SetClassResultDescription( int iClass, const char *pDesc );
	virtual void		SetClassResultAssociateWeapons( int iClass, bool associate );
	virtual void		AddContainedTechnology( const char *pszTech );
	virtual void		AddDependentTechnology( const char *pszTech );

	// Returns true if the specified class is affected by this technology, or any contained techs
	virtual bool		AffectsClass( int iClass );

	virtual bool		IsClassUpgrade( void );
	virtual bool		IsVehicle( void );
	virtual bool		IsTechLevelUpgrade( void );
	virtual bool		IsResourceTech( void );

	virtual void		SetHidden( bool hide );
	virtual bool		IsHidden( void );

	// Used by client to avoid giving you the same hint twice for a technology
	//  during a game/session
	virtual void		ResetHintsGiven( void );
	virtual bool		GetHintsGiven( int type );
	virtual void		SetHintsGiven( int type, bool given );

	// Returns the level to which this technology belongs
	virtual int			GetLevel( void );
	// Returns the internal name of the technology ( no spaces )
	virtual const char	*GetName( void );
	// Returns the printable name of the technology
	virtual const char	*GetPrintName( void );
	// Returns the button name of the technology;
	virtual const char	*GetButtonName( void );
	// Returns the non-class specific description of this technology
	virtual const char  *GetDescription( int iPlayerClass );
	// Returns the sound to play for this technology
	virtual const char	*GetSoundFile( int iClass );
	virtual int			GetSound( int iClass );
	// Set availability of the technology for the specified team
	virtual void		SetAvailable( bool state );
	// Returns true if the team has the technology
	virtual int			GetAvailable( void );
	// Zero out all preference/voting by players
	virtual void		ZeroPreferences( void );
	// Add one to the preference count for this technology for the specified team
	virtual void		IncrementPreferences( void );
	// Retrieve the number of player's who want to vote for this technology
	virtual int			GetPreferenceCount( void );

	// Retrieves the cost of purchasing the technology (doesn't factor in the resource levels)
	float				GetResourceCost( void );
	
	// Retrieves the current amount of resources spent on the technology
	float				GetResourceLevel( void );

	// Sets a resource level to an amount
	void				SetResourceLevel( float flResourceLevel );
	// Spends resources on buying this technology
	bool				IncreaseResourceLevel( float flResourcesToSpend );
	// Figure out my overall owned percentage
	void				RecalculateOverallLevel( void );
	float				GetOverallLevel( void );
	void				ForceComplete( void );

	// Goal technologies ( Techs related to a team's goal in a map )
	bool				IsAGoalTechnology( void );
	void				SetGoalTechnology( bool bGoal );

	// Check if class wants to enumerate weapon associations
	bool				GetAssociateWeaponsForClass( int iClass );

	// Weapon associatations
	int					GetNumWeaponAssociations( void );
	char const			*GetAssociatedWeapon( int index );
	void				AddAssociatedWeapon( const char *weaponname );

	// Contained Technology access
	int					GetNumberContainedTechs( void );
	const char			*GetContainedTechName( int iTech );
	void				SetContainedTech( int iTech, CBaseTechnology *pTech );

	// Dependent Technology access
	int					GetNumberDependentTechs( void );
	const char			*GetDependentTechName( int iTech );
	void				SetDependentTech( int iTech, CBaseTechnology *pTech );
	bool				DependsOn( CBaseTechnology *pTech );
	bool				HasInactiveDependencies( void );

	// Dirty bit, used for fast knowledge of when to resend techs
	bool				IsDirty( void );
	void				SetDirty( bool bDirty );

// Evil, Game DLL only code
#ifndef CLIENT_DLL
	// The technology has been acquired by the team.
	virtual void		AddTechnologyToTeam( CTFTeam *pTeam );
	// The technology has just been acquired, for each player on the acquiring team
	//  ask the technology to add any necessary weapons/items/abilities/modifiers, etc.
	virtual void		AddTechnologyToPlayer( CBaseTFPlayer *player );
	// A technology watcher entity wants to register as a watcher for this technology
	virtual void		RegisterWatcher( CInfoCustomTechnology *pWatcher );
	CUtlVector< CInfoCustomTechnology* > m_aWatchers;
#endif

	void				UpdateWatchers( void );

	// Hud Data
	void				SetActive( bool state );
	bool				GetActive( void );
	void				SetPreferred( bool state );
	bool				GetPreferred( void );
	void				SetVoters( int voters );
	int					GetVoters( void );

	void				SetTextureName( const char *texture );
	const char			*GetTextureName( void );
	void				SetTextureId( int id );
	int					GetTextureId( void );

private:
	// Name of the technology. Used to identify it in code.
	char				m_pszName[ TECHNOLOGY_NAME_LENGTH ];
	// Print name of the technology. Used to print the name of this technology to users.
	char				m_pszPrintName[ TECHNOLOGY_PRINTNAME_LENGTH ];
	// Button name of technology in the tech tree
	char				m_szButtonName[ TECHNOLOGY_BUTTONNAME_LENGTH ];
	// Description of the technology
	char				m_pszDescription[ TECHNOLOGY_DESC_LENGTH ];
	// Level to which the technology belongs
	int					m_nTechLevel;
	// Sound played to the entire team when this technology is received
	char				m_pszTeamSoundFile[ TECHNOLOGY_SOUNDFILENAME_LENGTH ];
	int					m_iTeamSound;
	// Results for this technology when it's achieved, on a per-class basis
	classresult_t		m_ClassResults[ TFCLASS_CLASS_COUNT ];

	// Resource costs
	float				m_fResourceCost;
	// Resource levels (amount of resource spent on the technology so far)
	float				m_fResourceLevel; 
	float				m_flOverallOwnedPercentage;

	// Technologies contained within this one
	char				m_apszContainedTechs[ MAX_CONTAINED_TECHNOLOGIES ][ TECHNOLOGY_NAME_LENGTH ];
	int					m_iContainedTechs;
	CBaseTechnology		*m_pContainedTechs[ MAX_CONTAINED_TECHNOLOGIES ];

	// Technologies this tech depends on
	char				m_apszDependentTechs[ MAX_DEPENDANT_TECHNOLOGIES ][ TECHNOLOGY_NAME_LENGTH ];
	int					m_iDependentTechs;
	CBaseTechnology		*m_pDependentTechs[ MAX_DEPENDANT_TECHNOLOGIES ];

	// Weapon association
	int					m_nNumWeaponAssociations;
	char				m_rgszWeaponAssociation[ MAX_ASSOCIATED_WEAPONS ][ TECHNOLOGY_WEAPONNAME_LENGTH ];

	// Does the team have access to the technology
	bool				m_bAvailable;

	// Is this a "placeholder" tech that shouldn't show up in the real tree
	bool				m_bHidden;

	CUtlVector< int >	m_HintsGiven;

	// Count of how many team members voted for this technology for spending resources
	int					m_nPreferenceCount;

	bool				m_bGoalTechnology;	// True if this tech's related to a team's goal in the current map
	bool				m_bClassUpgrade; // True if the tech unlocks a new class
	bool				m_bVehicle; // True if the tech unlocks a vehicle
	bool				m_bTechLevelUpgrade;  // True if the tech unlocks a new tech level
	bool				m_bResourceTech; // True if related to resource gathering

	// Dirty bit, used for fast knowledge of when to resend techs
	bool				m_bDirty;

	// Hud data
	bool				m_bActive;
	bool				m_bPreferred;
	int					m_nVoters;
	
	int					m_nTextureID;
	char				m_szTextureName[ TECHNOLOGY_TEXTURENAME_LENGTH ];
};

//-----------------------------------------------------------------------------
// Purpose: The Technology Tree.
//-----------------------------------------------------------------------------
class CTechnologyTree
{
public:
	// Construction
					CTechnologyTree( IFileSystem* pFileSystem, int nTeamNumber );
	virtual			~CTechnologyTree( void );

	// Startup/shutdown
	void			Shutdown( void );
	
	// Accessors
	void			AddTechnologyFile( IFileSystem* pFileSystem, int nTeamNumber, char *sFileName );
	void			LinkContainedTechnologies( void );
	void			LinkDependentTechnologies( void );
	int				GetIndex( CBaseTechnology *pItem );		// Get the index of the specified item
	CBaseTechnology *GetTechnology( int index );
	CBaseTechnology *GetTechnology( const char *pName );
	float			GetPercentageOfTechLevelOwned( int iTechLevel );

	// Size of list
	int				GetNumberTechnologies( void );

	// Local client's preferred item
	void			SetPreferredTechnology( CBaseTechnology *pItem );
	CBaseTechnology *GetPreferredTechnology( void );

	// Preference handling
	void			ClearPreferenceCount( void );
	void			IncrementPreferences( void );
	int				GetPreferenceCount( void );				// Get the number of players who've voted on techs
	CBaseTechnology *GetDesiredTechnology( int iDesireLevel );

	// Growable list of technologies
	CUtlVector< CBaseTechnology * > m_Technologies;

	int				m_nPreferenceCount;
};


#endif // TECHTREE_H