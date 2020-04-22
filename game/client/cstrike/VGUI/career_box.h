//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#ifndef CAREER_BOX_H
#define CAREER_BOX_H
#ifdef _WIN32
#pragma once
#endif

#include <KeyValues.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextEntry.h>
#include "weapon_csbase.h"
#include "buy_presets/buy_presets.h"
#include "buypreset_listbox.h"
#include "buypreset_weaponsetlabel.h"

class ConVarToggleCheckButton;

//--------------------------------------------------------------------------------------------------------------
/**
 *  Base class for career popup dialogs (handles custom backgrounds, etc)
 */
class CCareerBaseBox : public vgui::Frame
{
public:
	CCareerBaseBox(vgui::Panel *parent, const char *panelName, bool loadResources = true, bool useCareerButtons = false );
	virtual void ShowWindow();
	void DoModal();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();
	virtual void PaintBorder();
	virtual void PerformLayout();
	void SetLabelText( const char *text );
	void SetLabelText( const wchar_t *text );
	void SetCancelButtonAsDefault();
	vgui::Button * GetOkButton() { return m_pOkButton; }
	virtual vgui::Panel *CreateControlByName(const char *controlName);

private:
	typedef vgui::Frame BaseClass;
	vgui::Button		*m_pOkButton;
	vgui::Button		*m_pCancelButton;
	vgui::Dar<vgui::Button *> m_buttons;
	vgui::Dar<ConVarToggleCheckButton *> m_conVarCheckButtons;

	Color					 m_bgColor;
	Color					 m_borderColor;
	vgui::Label				*m_pTextLabel;
	bool					 m_cancelFocus;

protected:
	void SetLabelVisible( bool visible ) { m_pTextLabel->SetVisible( visible ); }
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnCommand( const char *command );			///< Handle button presses
	void AddButton( vgui::Button *pButton );				///< Add a button to our list of buttons for rollover sounds
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog with functionality similar to QueryBox, bot with different layout
 */
class CCareerQueryBox : public CCareerBaseBox
{
public:
	CCareerQueryBox(vgui::Panel *parent, const char *panelName, const char *resourceName = NULL);
	CCareerQueryBox(const char *title, const char *labelText, const char *panelName, vgui::Panel *parent = NULL);
	CCareerQueryBox(const wchar_t *title, const wchar_t *labelText, const char *panelName, vgui::Panel *parent = NULL);

	virtual ~CCareerQueryBox();
 
private:
	typedef CCareerBaseBox BaseClass;
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog for selecting and editing a primary weapon (ammo to buy, side-availability)
 */
class CWeaponSelectBox : public CCareerBaseBox
{
public:
	CWeaponSelectBox( vgui::Panel *parent, WeaponSet *pWeaponSet, bool isSecondary );

	virtual ~CWeaponSelectBox();

	virtual void ActivateBuildMode();

	void UpdateClips();

protected:
	virtual void OnCommand( const char *command );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	void PopulateControls();
	void SetClipsVisible( bool visible );

	CSWeaponID GetSelectedWeaponID();

	typedef CCareerBaseBox BaseClass;
	vgui::ComboBox *m_pClips;	///< Number of clips to purchase
	vgui::Label *m_pBullets;	///< Label showing "M of N Bullets"

	WeaponSet *m_pWeaponSet;	///< WeaponSet being edited
	bool m_isSecondary;			///< is this weapon primary or secondary?
	BuyPresetListBox *m_pListBox;	///< List of weapons from which to choose

	int m_numWeapons;
	CSWeaponID *m_weaponIDs;
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Base class for editing grenades and equipment
 */
class CBaseSelectBox : public CCareerBaseBox
{
	typedef CCareerBaseBox BaseClass;

public:
	CBaseSelectBox( vgui::Panel *parent, const char *panelName, bool loadResources = true ) : BaseClass( parent, panelName, loadResources ) {}

	virtual void OnControlChanged() = 0;
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog for editing grenades in a buy preset fallback
 */
class CGrenadeSelectBox : public CBaseSelectBox
{
	typedef CBaseSelectBox BaseClass;

public:
	CGrenadeSelectBox( vgui::Panel *parent, WeaponSet *pWeaponSet );

	void OnControlChanged();		///< Updates item costs

private:
	WeaponSet *m_pWeaponSet;		///< WeaponSet being edited

	// Equipment controls
	vgui::ComboBox		*m_pHEGrenade;
	EquipmentLabel		*m_pHEGrenadeImage;
	vgui::Label			*m_pHELabel;

	vgui::ComboBox		*m_pSmokeGrenade;
	EquipmentLabel		*m_pSmokeGrenadeImage;
	vgui::Label			*m_pSmokeLabel;

	vgui::ComboBox		*m_pFlashbangs;
	EquipmentLabel		*m_pFlashbangImage;
	vgui::Label			*m_pFlashLabel;

	virtual void OnCommand( const char *command );
};

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog for selecting an equipment set
 */
class CEquipmentSelectBox : public CBaseSelectBox
{
	typedef CBaseSelectBox BaseClass;

public:
	CEquipmentSelectBox( vgui::Panel *parent, WeaponSet *pWeaponSet );

	void OnControlChanged();

private:
	WeaponSet *m_pWeaponSet;		///< WeaponSet being edited

	// Equipment controls
	vgui::ComboBox		*m_pKevlar;
	vgui::Label			*m_pKevlarLabel;
	EquipmentLabel		*m_pKevlarImage;

	vgui::ComboBox		*m_pHelmet;
	vgui::Label			*m_pHelmetLabel;
	EquipmentLabel		*m_pHelmetImage;

	vgui::ComboBox		*m_pDefuser;
	vgui::Label			*m_pDefuserLabel;
	EquipmentLabel		*m_pDefuserImage;

	vgui::ComboBox		*m_pNightvision;
	vgui::Label			*m_pNightvisionLabel;
	EquipmentLabel		*m_pNightvisionImage;

	virtual void OnCommand( const char *command );
};

#endif // CAREER_BOX_H
