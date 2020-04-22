//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: QWERTY keyboard text entry method for Steam controller
//=============================================================================//

#ifndef PANORAMA_TEXTINPUT_DUALTOUCH_H
#define PANORAMA_TEXTINPUT_DUALTOUCH_H

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
	class CTextInputDualTouch;
	class CTextEntry;
	class ITextInputSuggest;
	class CLabel;
	
	// this weights the frequency of words typed without any possible typos above
	// autocorrected words, as otherwise if you legitimately type 'test' it'll
	// auto-correct into 'rest'
	static const float k_flExactWordFrequencyWeight = 1.2f;
		
	class CTextInputDualTouch : public panorama::CTextInputHandler
	{
		DECLARE_PANEL2D( CTextInputDualTouch, panorama::CPanel2D );
		
	public:
		// Constructor
		CTextInputDualTouch( panorama::IUIWindow *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );
		CTextInputDualTouch( panorama::CPanel2D *parent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );
		
		// Destructor
		~CTextInputDualTouch();
		
		// CTextInputHandler overrides
		virtual void OpenHandler() OVERRIDE;
		virtual void CloseHandlerImpl( bool bCommitText ) OVERRIDE;
		virtual ETextInputHandlerType_t GetType() OVERRIDE;
		virtual ITextInputControl *GetControlInterface() OVERRIDE;
		virtual void SuggestWord( const wchar_t *pwch, int ich ) OVERRIDE;
		virtual void SetYButtonAction( const char *pchLabel, IUIEvent *pEvent ) OVERRIDE;
		
		static void GetSupportedLanguages( CUtlVector<ELanguage> &vecLangs );

	private:
		static const int k_DualTouchRowCount = 4;
		static const int k_DualTouchColumnCount = 11;
		static const int k_SuggestionCount = 4;
		
		enum EDualTouchModifier_t
		{
			k_EDualTouchModifierNone = 0,
			k_EDualTouchModifierShift = 1,
			k_EDualTouchModifierAlt = 2,
			k_EDualTouchModifierCount = 3,
		};
		
		class CTouchPad
		{
		public:
			bool Initialize( CTextInputDualTouch *pParent, const char *pointerID, const char *padID,
							 IUIEngine::EHapticFeedbackPosition eHapticsPosition );
			void UpdatePointerState( bool bPointersEnabled, uint32 nTextureID );
			void OnTouch( void );
			void OnRelease( void );
			bool OnMove( float touchX, float touchY );
			void OnButtonDown( void );
			
			SteamPadPointer_t m_renderPointerState;
			
			CPanel2D *m_pPointerPanel;
			CPanel2D *m_pPadPanel;
			CPanel2D *m_pHoverKey;				// what key are we currently hovering over?
			CPanel2D *m_pLastHoverKey;			// what was the last key we were hovering over? this will either match m_pHoverKey or have the last value m_pHoverKey had if its currently nullptr
			CUtlVector< IUIPanel * > m_vecTouchKeys;
			bool m_bFingerOnPad;
			
			float m_hoverX;
			float m_hoverY;
			
			CTextInputDualTouch *m_pTextInputDualTouch;
			
			IUIEngine::EHapticFeedbackPosition m_eHapticsPosition;
		};
		
		void Initialize( const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );
		void SetMode( ETextInputMode_t mode );
		
		// CPanel2D overrides
		virtual bool OnGamePadUp( const panorama::GamePadData_t &code ) OVERRIDE;
		virtual bool OnGamePadDown( const panorama::GamePadData_t &code ) OVERRIDE;
		virtual bool OnGamePadAnalog( const panorama::GamePadData_t &code ) OVERRIDE;
		virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;
		virtual bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
		virtual bool OnKeyUp( const KeyData_t &code ) OVERRIDE;
		
		void UpdateSteamPadPointers( void );
		bool OnPropertyTransitionEnd( const CPanelPtr< IUIPanel > &pPanel, CStyleSymbol prop );
		bool OnRemoveStyleFromLinkedKeys( CPanelPtr<CPanel2D> pPanel, const char *pszStyle );
		
		bool TouchPadClicked( CTouchPad* pTouchPad );
		
		// Listen for focus lost
		bool HandleInputFocusLost( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
		bool OnActiveControllerTypeChanged( EActiveControllerType eActiveControllerType );

		bool BConvertNextSpaceToPeriod( void );
		bool TypeWchar( uchar16 wch, const char *pUTFf8Char = NULL );
		bool TypeKeyDown( panorama::KeyCode eCode );
		
		bool SwitchLanguage( void );
		bool LoadInputConfigurationFile( ELanguage language );
		bool LoadInputConfigurationFile( char const *szConfigFile, const char *szConfigRootDir );
		bool LoadConfigurationBuffer( char const *pszIncoming );
		
		void SetModifierKeyState( EDualTouchModifier_t modifier, bool bIsButtonPressEvent );
		EDualTouchModifier_t CalculateDesiredModifierState() const;
		void ApplyCurrentModifierLayout();
		
		void TouchKeyClicked( CPanel2D *pTouchKey, CTouchPad *pTouchPad );
		
		//	Process scheduled key repeat
		void ScheduleKeyRepeats( panorama::GamePadCode eCode );
		void CancelOutstandingRepeats() { ScheduleKeyRepeats( XK_NULL ); }
		
		void ScheduledKeyRepeatFunction();
		
		// auto-suggestion
		void ClearSuggestionVisual( void )
		{
			for ( int i = 0; i < k_SuggestionCount; i++ )
			{
				m_pSuggestionLabels[i]->SetText( "" );
			}
		}
		void ClearSuggestionState( bool bFlush = true )
		{
			// flush what we have into the buffer
			if ( bFlush && m_vecPossibleWordsBeingTyped.Count() )
			{
				CStrAutoEncode s( m_vecPossibleWordsBeingTyped[0] );
				m_pTextInputControl->InsertCharactersAtCursor( s.ToWString(), V_wcslen( s.ToWString() ) );
			}
			m_vecPossibleWordsBeingTyped.Purge();
			ClearSuggestionVisual();
			UpdateTextPreview();
		}
		void ProcessSuggestions( void );
		void ApplySuggestion( int iSuggestion );
		void UpdateTextPreview( void );
		
		bool PerformBackspace( void );
		
		void CursorMove( const panorama::GamePadData_t &code );
		void DisableCursorMode( void );

	private:
		void ChangeTouchkeyStyle( CPanel2D *pTouchKey, const char *pchStyle, bool bAddStyle );
	
	public:
		ITextInputControl *m_pTextInputControl; // control interface for moving text input between a control and daisy wheel
		
		CPanel2D *m_pBodyContainer;
		
		IUIEvent *m_pYbuttonAction; // the action to fire if the Y button is hit
		CLabel *m_pYButtonText; // label for ybutton text
		CLabel *m_pLang;
		
		CLabel *m_pSuggestionLabels[k_SuggestionCount];
		CUtlVector< CUtlString > m_vecPossibleWordsBeingTyped;
		
		ELanguage m_language;						// currently loaded language
		
		ITextInputSuggest *m_pSuggest;				// suggestion engine
		
		CTextEntry *m_pTextPreview;
		
		bool m_bDoubleSpaceToDotSpace;
		bool m_bOnlySpacesEnteredSinceBackspace;
		
		ETextInputMode_t m_mode;
		
		bool m_bAutoComplete;
		bool m_bDisplaySuggestions;
		bool m_bHidePreviewField;
		bool m_bAutoCaps;
		
		// This controls the direct-rendered steampad crosshairs; we can only
		// have them up while we're not animating around, otherwise use higher-latency
		// panel crosshairs
		bool m_bSteamPadPointersEnabled;
		IImageSource *m_pSteamPadPointerImage;
		
		CTouchPad m_leftTouchPad;
		CTouchPad m_rightTouchPad;
		
		CUtlVector< CTouchPad * > m_vecTouchPads;
		
		uchar32 m_keyLayout[k_DualTouchColumnCount][k_DualTouchRowCount][k_EDualTouchModifierCount];
		EDualTouchModifier_t m_currentModifier;
		int m_iCharactersTypedSinceModifierStateChanged;
		
		// Tracking key repeats
		CCubicBezierCurve< Vector2D > m_repeatCurve;	// Curve for key repeats
		double m_repeatStartTime;					// Time when the key was initially pressed
		double m_repeatNextTime;						// Time when the key will repeat next
		panorama::GamePadCode m_repeatGamePadCode;	// Which key was pressed (low level, for key-up tracking)
		uint32 m_repeatCounter;						// How many key repeats have happened
		panorama::CUIScheduledDel m_repeatFunction;	// Scheduled function triggering key repeats
		
		bool m_bCursorMode;
		CPanel2D *m_pCursorKey;
		bool m_bUseTouchPads;

		bool m_bModifierKeysHeld[ k_EDualTouchModifierCount ];
	};
	
} // namespace panorama

#endif // PANORAMA_TEXTINPUT_DUALTOUCH_H