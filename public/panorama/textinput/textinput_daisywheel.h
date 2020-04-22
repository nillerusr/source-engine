//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TEXTINPUT_DAISYWHEEL_H
#define PANORAMA_TEXTINPUT_DAISYWHEEL_H

#include "panorama/textinput/textinput.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/label.h"
#include "panorama/input/iuiinput.h"
#include "mathlib/beziercurve.h"
#include "tier1/utlptr.h"
#include "panorama/uischeduleddel.h"

namespace panorama
{

// Forward declaration
class CTextInputDaisyWheel;
class CTextEntry;
class ITextInputSuggest;
class CLabel;

//-----------------------------------------------------------------------------
// Purpose: Implementation of daisy wheel text input handler
//-----------------------------------------------------------------------------
class CTextInputDaisyWheel : public panorama::CTextInputHandler
{
	DECLARE_PANEL2D( CTextInputDaisyWheel, panorama::CPanel2D );

public:
	// Constructor
	CTextInputDaisyWheel( panorama::IUIWindow *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );
	CTextInputDaisyWheel( panorama::CPanel2D *parent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );

	// Destructor
	~CTextInputDaisyWheel();

	// CTextInputHandler overrides
	virtual void OpenHandler() OVERRIDE;
	virtual void CloseHandlerImpl( bool bCommitText ) OVERRIDE;
	virtual ETextInputHandlerType_t GetType() OVERRIDE;
	virtual ITextInputControl *GetControlInterface() OVERRIDE;
	virtual void SuggestWord( const wchar_t *pwch, int ich ) OVERRIDE;
	virtual void SetYButtonAction( const char *pchLabel, IUIEvent *pEvent ) OVERRIDE;

	static void GetSupportedLanguages( CUtlVector<ELanguage> &vecLangs );
private:
	void Initialize( const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );

	// This isn't part of CTextInputHandler because currently there is no need to set this dynamically
	void SetMode( ETextInputMode_t mode );

	// CPanel2D overrides
	virtual bool OnGamePadUp( const panorama::GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadDown( const panorama::GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadAnalog( const panorama::GamePadData_t &code ) OVERRIDE;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;
	virtual bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
	virtual bool OnKeyUp( const KeyData_t &code ) OVERRIDE;

	//
	//	Daisy wheel input type
	//
	enum EDaisyInputType_t
	{
		k_EDaisyInputTypeABXY,		// ABXY layout has color buttons printing keys
		k_EDaisyInputTypeRS,		// RS layout requires Right Stick usage to print keys
		k_EDaisyInputTypePIN,		// Button map to numbers for secret PIN entry
	};

	//	Set daisy wheel type
	void SetDaisyInputType( EDaisyInputType_t eType );

	// Add an emoticon to the list of emoji this daisywheel will show
	// TODO: Currently not part of CTextInputHandler because emoticons need work
	void AddEmoticon( const char *szInsert, const char *szImageURL );
	void CommitEmoticons();

#ifdef DBGFLAG_VALIDATE
	void ValidateClientPanel( CValidator &validator, const tchar *pchName );
#endif

	// User wants to move focus to the next field on the page
	bool NextFocus();

	static const int k_cPetals = 8;										// # petals in flower (hardcoded for now)
	static const int k_cItemsPerPetal = 4;									// # letters visible on each petal, max
	static const int k_cItemsPerLayoutMax = k_cPetals * k_cItemsPerPetal;		// # letters total per layout, max

	struct Emoticon_t
	{
		CUtlString sType;
		CUtlString sImageURL;
	};

	//
	//	A single wheel layout configuration: name and UTF-8 sequences of characters
	//	Structure is allocated with more memory at the end of the structure, name and
	//	UTF-8 sequences follow this structure object
	//
	class CDaisyConfig
	{
	public:
		CDaisyConfig( const char *pchName ) : m_sName( pchName )
		{
		}

		// Get the name of wheel layout
		char const * GetName() const { return m_sName.String(); }

		// Get number of items in this wheel layout
		int GetNumItems() const { return m_cItems; }

		// Get a given item in this wheel layout, must be >= zero and < number of items
		// double the index, because each item is NUL terminated
		const char * GetItem( int idx ) const
		{
			if ( idx >= k_cItemsPerLayoutMax || idx < 0 )
			{
				Assert( false );
				return "";
			}

			return m_vecText.Base() + m_rgich[ idx ];
		}

		#ifdef DBGFLAG_VALIDATE
		void Validate( CValidator &validator, const tchar *pchName )
		{
			VALIDATE_SCOPE();
			ValidateObj( m_sName );
			ValidateObj( m_vecText );
		}
		#endif

		// name of this layout
		CUtlString m_sName;

		// Number of items in this wheel layout
		int m_cItems;

		// offsets into the text block for each item in this layout
		int m_rgich[ k_cItemsPerLayoutMax ];

		// block of text for this layout, UTF-8
		CUtlVector< char > m_vecText;
	};

	//	After configuration has been loaded and assigned walk through all
	//	controls and set their values to represent the loaded config
	void SetControlsFromConfiguration();

	// map the trigger inputs to a config
	enum EDaisyConfig_t
	{
		k_EDaisyConfigNone = -1,
		k_EDaisyConfigCaps,
		k_EDaisyConfigLetters,
		k_EDaisyConfigNumbers,
		k_EDaisyConfigSpecial,
		k_EDaisyConfigNumbersOnly,		
		k_EDaisyConfigPhoneNumber,
		k_EDaisyConfigSteamCodeChars,
		k_EDaisyConfigEmoji,
	};
	typedef CUtlMap< EDaisyConfig_t, CUtlPtr< CDaisyConfig >, int, CDefLess< EDaisyConfig_t > > MapConfigEntries_t;

	EDaisyConfig_t EDaisyConfigFromString( const char *pchValue );
	EDaisyConfig_t ConfigFromTriggerState( bool bLeftTrigger, bool bRightTrigger );

	// update the trigger legends based on the current trigger state
	void UpdateTriggerLegends();

	// moves controls to existing config (caps -> lowercase)
	void AdvanceControlsConfiguration( EDaisyConfig_t eConfig );

	//	Types a character from selected group's side of world: "E" | "W" | "N" | "S"
	bool TypeCharacterFromSide( char chSide );

	//	Types a given wide character into text entry
	bool TypeWchar( uchar16 wch );

	//	Simulates a key down event
	bool TypeKeyDown( panorama::KeyCode eCode );

	//	Loads configuration file for specified language
	bool LoadInputConfigurationFile( ELanguage language );

	//	Loads configuration file
	bool LoadInputConfigurationFile( const char *szConfig, const char *szConfigRootDir );

	//	Builds configuration structure based on buffer loaded from config file
	bool LoadConfigurationBuffer( char const *pszBase, MapConfigEntries_t *pmapConfigs );

	// Switch between most recent languages
	bool SwitchLanguage();
	bool ShowThisLanguage( ELanguage language );

	//	For a given config item determine which group and group side the item should be at
	void GetItemLocation( CDaisyConfig *pCfg, int iItem, char const *&szGroup, char const *&szItem );

	//	Get name of the group square indexed by -1|0|1 pair of x,y coordinates; returns side or wolrd like: "E" | "NE" | "N" | "NW" | etc.
	char const * GetGroupNameSq( int x, int y );

	//	Gets a sequential index of the group square indexed by -1|0|1 pair of x,y coordinates
	int GetGroupIdxSq( int x, int y );

	//	Gets the side of world name of group by its sequential index
	char const * GetGroup( int idxGroup );
	
	//	Gets the side of world name of item by its sequential index
	char const * GetSide( int idxSide );

	//	Process scheduled key repeat
	void ScheduleKeyRepeats( panorama::GamePadCode eCode );
	void CancelOutstandingRepeats() { ScheduleKeyRepeats( XK_NULL ); }

	void ScheduledKeyRepeatFunction();

	bool HandlePropertyTransitionEnd( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, CStyleSymbol sym );

	// Listen for focus lost
	bool HandleInputFocusLost( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );

	// events bound in window_keybinds.cfg
	bool ShowHideSettings();

	// settings events
	bool CancelSettings();

	// auto-suggestion
	void ShowSuggestion( const char *szPrefix, const char *szSuffix );
	void ClearSuggestionVisual()
	{
		ShowSuggestion( "", "" );
	}
	void ClearSuggestionState()
	{
		m_sSuggestion.Clear();
		ClearSuggestionVisual();
	}

	bool BCursorAtStartOfSentence();
	bool BConvertNextSpaceToPeriod();

	// Play sound for a give daisy wheel activity
	enum EDaisyAction_t
	{
		k_EDaisySound_ButtonA,
		k_EDaisySound_ButtonB,
		k_EDaisySound_ButtonX,
		k_EDaisySound_ButtonY,
		k_EDaisySound_KeySpacebar,
		k_EDaisySound_KeyBackspace,
		k_EDaisySound_KeyLeft,
		k_EDaisySound_KeyRight,
		k_EDaisySound_KeyHome,
		k_EDaisySound_KeyEnd,
		k_EDaisySound_FocusAreaChanged,
		k_EDaisySound_ConfigChanged,
		k_EDaisySound_FocusAreaCold,
		k_EDaisySound_PerformAutosuggest,
	};
	void PlayDaisyActionSound( EDaisyAction_t eAction );

	//	Function to handle gamepad data depending on daisy wheel settings
	typedef bool (CTextInputDaisyWheel::*PFNGamePadData)( const panorama::GamePadData_t &code );
	
	MapConfigEntries_t m_mapConfigEntries;		//	Processed configuration entries
	EDaisyConfig_t m_eConfigCurrent;			//	Current config, can be cycled with triggers
	bool m_bRestrictConfig;						//	if true, stick to current config (no changing layouts)

	panorama::CPanel2D *m_pStickPri;		// Primary stick control
	float m_flStickPriSelectOct[2];			// Selection octant angles (std: lo=M_PI/6, hi=M_PI/3)
	float m_flStickPriMoveScale[2];			// Scale for primary stick movement
	float m_flStickPriSelectDist[2];		// Selection distances
	float m_flStickPriSelectAngleSticky;	// Selection angle to stick to area
	float m_flStickPriColdTime;				// How long we need primary stick to remain cold
	int m_nSelectionGroup[2];				// Currently selected group

	panorama::CPanel2D *m_pStickSnd;		// Secondary stick control
	float m_flStickSndMoveScale[2];			// Scale for secondary stick movement
	float m_flStickSndSelectDist[2];		// Selection distances for secondary stick
	float m_flStickSndSelectAngleSticky;	// Selection angle for secondary stick to stick to area

	panorama::CLabel *m_pLang;				// Language legend label

	double m_flPickedItemTransitionTime;		// Time for picked item to highlight

	PFNGamePadData m_pfnOnGamePadDown;		// Current config for gamepad down
	PFNGamePadData m_pfnOnGamePadAnalog;	// Current config for gamepad analog

	// settings
	bool m_bDoubleSpaceToDotSpace;
	bool m_bAutoCaps;

	// Tracking doublespace = dot+space
	bool m_bOnlySpacesEnteredSinceBackspace;

	// Tracking trigger state for typing
	bool m_bTriggersDownState[2];			// Whether trigger is down

	// Tracking stick cold state
	double m_flTimeStickCold;				// When stick went into cold state (rolls when hot)

	// Tracking key repeats
	CCubicBezierCurve< Vector2D > m_repeatCurve;	// Curve for key repeats
	double m_repeatStartTime;					// Time when the key was initially pressed
	double m_repeatNextTime;						// Time when the key will repeat next
	panorama::GamePadCode m_repeatGamePadCode;	// Which key was pressed (low level, for key-up tracking)
	uint32 m_repeatCounter;						// How many key repeats have happened
	panorama::CUIScheduledDel m_repeatFunction;	// Scheduled function triggering key repeats

	// Stick processing routines
	bool OnGamePadAnalog_ProcessLeftStickForGroup( const panorama::GamePadData_t &code );
	bool OnGamePadAnalog_ProcessRightStickForSide( const panorama::GamePadData_t &code );
	bool OnGamePadAnalog_Trigger( const panorama::GamePadData_t &code );
	bool HandleTextInputDaisyWheelOnGamePadAnalogTriggersChanged();

	// ABXY handlers
	bool DaisyABXY_OnGamePadDown( const panorama::GamePadData_t &code );
	bool DaisyABXY_OnGamePadAnalog( const panorama::GamePadData_t &code );

	// RS handlers
	bool DaisyRS_OnGamePadDown( const panorama::GamePadData_t &code );
	bool DaisyRS_OnGamePadAnalog( const panorama::GamePadData_t &code );

	// PINpad handlers
	bool DaisyPIN_OnGamePadDown( const panorama::GamePadData_t & code );
	bool DaisyPIN_OnGamePadAnalog( const panorama::GamePadData_t &code );

	ELanguage m_language;						// currently loaded language

	ITextInputSuggest *m_psuggest;				// suggestion engine
	CUtlString m_sSuggestion;					// result of suggestion

	panorama::CLabel *m_plabelSuggestionPrefix;	// label containing prefix of current suggestion
	panorama::CLabel *m_plabelSuggestionSuffix;	// label containing prefix of current suggestion

	double m_flInputStartTime; // when did the user first start typing
	bool m_bUsedKeyboard; // true if the kb was used for ANY input
	bool m_bUsedGamepad; // true if the gamepad was used for ANY input

	ITextInputControl *m_pTextInputControl; // control interface for moving text input between a control and daisy wheel
	IUIEvent *m_pYbuttonAction; // the action to fire if the Y button is hit
	panorama::CLabel *m_pYButtonText; // label for ybutton text
	ETextInputMode_t m_mode;	// text input mode
	bool m_bDisplaySuggestions;	// If true, allow suggestions to be displayed

	// mode can disable specific footer sections
	bool m_bDisableRightTrigger;
	bool m_bDisableRightBumper;
	bool m_bDisableLeftTrigger;
	bool m_bDisableLanguageSelect;

	enum ERightStickPos 
	{
		k_RightStick_None,
		k_RightStick_Up,
		k_RightStick_Down,
		k_RightStick_Left,
		k_RightStick_Right,
	};
	ERightStickPos m_eSteamRightStickPos;
	Vector2D m_vecRightPadPos;

	CUtlVector< Emoticon_t > m_vecEmoji;
	bool m_bLoadedEmoji;
};

} // namespace panorama

#endif // PANORAMA_TEXTINPUT_DAISYWHEEL_H

