//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_OBJECTSELECTION_H
#define WEAPON_OBJECTSELECTION_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#define CWeaponObjectSelection C_WeaponObjectSelection
#endif

//-----------------------------------------------------------------------------
// Purpose: This is a 'weapon' that's used to put objects into the client's weapon selection 
//-----------------------------------------------------------------------------
class CWeaponObjectSelection : public CBaseTFCombatWeapon
{ 
	DECLARE_CLASS( CWeaponObjectSelection, CBaseTFCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponObjectSelection();

#if defined( CLIENT_DLL )
	virtual void	PreDataUpdate( DataUpdateType_t updateType );
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
#endif

	// Overridden to handle objects
	virtual bool	CanDeploy( void );
	virtual bool	HasAmmo( void );
	virtual int		GetSubType( void );
	virtual int		GetSlot( void ) const;
	virtual int		GetPosition( void ) const;
	virtual const char *GetPrintName( void ) const;

#ifdef CLIENT_DLL
	virtual CHudTexture const *GetSpriteActive( void ) const;
	virtual CHudTexture const *GetSpriteInactive( void ) const;
#endif

	// Set this selection's object type
	void	SetType( int iType );
	void	SetupObjectSelectionSprite( void );

	virtual bool	SupportsTwoHanded( void ) { return true; };

protected:
	CNetworkVar( int, m_iObjectType );
	int		m_iOldObjectType;

#ifdef CLIENT_DLL
	CHudTexture		*m_pSelectionTexture;
	bool			m_bNeedSpriteSetup;
	vgui::HFont		m_hNumberFont;
#endif

private:														
	CWeaponObjectSelection( const CWeaponObjectSelection & );						
};

#endif // WEAPON_OBJECTSELECTION_H
