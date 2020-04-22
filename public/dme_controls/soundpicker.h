//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef SOUNDPICKER_H
#define SOUNDPICKER_H
#ifdef _WIN32
#pragma once
#endif

#include "matsys_controls/BaseAssetPicker.h"
#include "vgui_controls/Frame.h"
#include "datamodel/dmehandle.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Panel;
}


//-----------------------------------------------------------------------------
// Purpose: Sound picker panel
//-----------------------------------------------------------------------------
class CSoundPicker : public CBaseAssetPicker
{
	DECLARE_CLASS_SIMPLE( CSoundPicker, CBaseAssetPicker );

public:
	enum PickType_t
	{
		PICK_NONE		= 0,
		PICK_GAMESOUNDS	= 0x1,
		PICK_WAVFILES	= 0x2,
		PICK_ALL		= 0x7FFFFFFF,

		ALLOW_MULTISELECT = 0x80000000,
	};

	CSoundPicker( vgui::Panel *pParent, int nFlags );
	~CSoundPicker();

	// overridden frame functions
	virtual void Activate();

	// Forward arrow keys to the list
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	// Sets the current sound choice
	void SetSelectedSound( PickType_t type, const char *pSoundName );

	// Returns the selceted sound name
	PickType_t GetSelectedSoundType();
	const char *GetSelectedSoundName( int nSelectionIndex = -1 );
	int GetSelectedSoundCount();

private:
	// Purpose: Called when a page is shown
	void RequestGameSoundFilterFocus( );

	// Updates the column header in the chooser
	void UpdateGameSoundColumnHeader( int nMatchCount, int nTotalCount );

	void BuildGameSoundList();
	void RefreshGameSoundList();
	void PlayGameSound( const char *pSoundName );
	void PlayWavSound( const char *pSoundName );
	void StopSoundPreview( );
	void OnGameSoundFilterTextChanged( );

	// Derived classes have this called when the previewed asset changes
	void OnSelectedAssetPicked( const char *pAssetName );

	// Don't play a sound when the next selection is a default selection
	void OnNextSelectionIsDefault();

	// Purpose: builds the gamesound list
	bool IsGameSoundVisible( int hGameSound );

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", kv );
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );
	MESSAGE_FUNC( OnPageChanged, "PageChanged" );

	vgui::TextEntry *m_pGameSoundFilter;
	vgui::PropertySheet *m_pViewsSheet;
	vgui::PropertyPage *m_pGameSoundPage;
	vgui::PropertyPage *m_pWavPage;
	vgui::ListPanel *m_pGameSoundList;
	CUtlString m_GameSoundFilter;
	int m_nPlayingSound;
	unsigned char m_nSoundSuppressionCount;

	friend class CSoundPickerFrame;
};


//-----------------------------------------------------------------------------
// Purpose: Modal sound picker window
//-----------------------------------------------------------------------------
class CSoundPickerFrame : public CBaseAssetPickerFrame
{
	DECLARE_CLASS_SIMPLE( CSoundPickerFrame, CBaseAssetPickerFrame );

public:
	CSoundPickerFrame( vgui::Panel *pParent, const char *pTitle, int nFlags );
	virtual ~CSoundPickerFrame();

	// Purpose: Activate the dialog
	// The message "SoundSelected" will be sent if a sound is picked
	// Pass in optional context keyvalues to be added to any messages sent by the sound picker
	void DoModal( CSoundPicker::PickType_t initialType, const char *pInitialValue, KeyValues *pContextKeyValues = NULL );

	virtual void OnCommand( const char *pCommand );
};


#endif // SOUNDPICKER_H
