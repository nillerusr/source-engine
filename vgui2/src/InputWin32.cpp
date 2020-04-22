//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <imm.h>
#define DO_IME
#endif

#include <string.h>

#include "vgui_internal.h"
#include "VPanel.h"
#include "utlvector.h"
#include <KeyValues.h>
#include "tier0/vcrmode.h"

#include <vgui/VGUI.h>
#include <vgui/ISystem.h>
#include <vgui/IClientPanel.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include "vgui/Cursor.h"
#include <vgui/keyrepeat.h>

#include "utllinkedlist.h"
#include "tier0/icommandline.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

/* 
> Subject: RE: l4d2 & motd 
>  
> From: Alfred Reynolds
>   I'd go with the if it ain't broke don't touch it route, might as well 
> leave win32 as is and just knobble the asserts where we know we won't implement it.
> 
>> From: Mike Sartain
>>   Well now that's interesting. Is it ok to remove it for win32 then?
>> 
>>> From: Alfred Reynolds
>>>   We never did the IME work, AFAIK it only ever worked on the game's 
>>> console in game which isn't useful for users. So, no demand, hard 
>>> (actually, really hard) to implement so it wasn't done.
>>> 
>>>> From: Mike Sartain
>>>>   There are also a bunch of IME Language functions in 
>>>> vgui2/src/inputwin32.cpp that are NYI on Linux as well - but it looks 
>>>> like those haven't ever been implemented on OSX either. Alfred, what 
>>>> is the story there?
*/
#if 0 // !defined( DO_IME ) && !defined( _X360 )
#define ASSERT_IF_IME_NYI()	Assert( !"IME Support NYI" )
#else
#define ASSERT_IF_IME_NYI()
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool IsDispatchingMessageQueue( void );

using namespace vgui;

class CInputSystem : public IInputInternal
{
public:
	CInputSystem();
	~CInputSystem();

	virtual void RunFrame();

	virtual void PanelDeleted(VPANEL panel);

	virtual void UpdateMouseFocus(int x, int y);
	virtual void SetMouseFocus(VPANEL newMouseFocus);

	virtual void SetCursorPos(int x, int y);
	virtual void UpdateCursorPosInternal( int x, int y );
	virtual void GetCursorPos(int &x, int &y);
	virtual void SetCursorOveride(HCursor cursor);
	virtual HCursor GetCursorOveride();


	virtual void SetMouseCapture(VPANEL panel);

	virtual VPANEL GetFocus();
	virtual VPANEL GetCalculatedFocus();
	virtual VPANEL GetMouseOver();

	virtual bool WasMousePressed(MouseCode code);
	virtual bool WasMouseDoublePressed(MouseCode code);
	virtual bool IsMouseDown(MouseCode code);
	virtual bool WasMouseReleased(MouseCode code);
	virtual bool WasKeyPressed(KeyCode code);
	virtual bool IsKeyDown(KeyCode code);
	virtual bool WasKeyTyped(KeyCode code);
	virtual bool WasKeyReleased(KeyCode code);

	virtual void GetKeyCodeText(KeyCode code, char *buf, int buflen);

	virtual bool InternalCursorMoved(int x,int y); //expects input in surface space
	virtual bool InternalMousePressed(MouseCode code);
	virtual bool InternalMouseDoublePressed(MouseCode code);
	virtual bool InternalMouseReleased(MouseCode code);
	virtual bool InternalMouseWheeled(int delta);
	virtual bool InternalKeyCodePressed(KeyCode code);
	virtual void InternalKeyCodeTyped(KeyCode code);
	virtual void InternalKeyTyped(wchar_t unichar);
	virtual bool InternalKeyCodeReleased(KeyCode code);
	virtual void SetKeyCodeState( KeyCode code, bool bPressed );
	virtual void SetMouseCodeState( MouseCode code, MouseCodeState_t state );
	virtual void UpdateButtonState( const InputEvent_t &event );

	virtual VPANEL GetAppModalSurface();
	// set the modal dialog panel.
	// all events will go only to this panel and its children.
	virtual void SetAppModalSurface(VPANEL panel);
	// release the modal dialog panel
	// do this when your modal dialog finishes.
	virtual void ReleaseAppModalSurface();

	// returns true if the specified panel is a child of the current modal panel
	// if no modal panel is set, then this always returns TRUE
	virtual bool IsChildOfModalPanel(VPANEL panel, bool checkModalSubTree = true );

	// Creates/ destroys "input" contexts, which contains information
	// about which controls have mouse + key focus, for example.
	virtual HInputContext CreateInputContext();
	virtual void DestroyInputContext( HInputContext context ); 

	// Associates a particular panel with an input context
	// Associating NULL is valid; it disconnects the panel from the context
	virtual void AssociatePanelWithInputContext( HInputContext context, VPANEL pRoot );

	// Activates a particular input context, use DEFAULT_INPUT_CONTEXT
	// to get the one normally used by VGUI
	virtual void ActivateInputContext( HInputContext context );
	virtual void PostCursorMessage( );
	virtual void HandleExplicitSetCursor( );

	virtual void ResetInputContext( HInputContext context );

	virtual void GetCursorPosition( int &x, int &y );

	virtual void SetIMEWindow( void *hwnd );
	virtual void *GetIMEWindow();

	// Change keyboard layout type
	virtual void OnChangeIME( bool forward );
	virtual int  GetCurrentIMEHandle();
	virtual int  GetEnglishIMEHandle();

	// Returns the Language Bar label (Chinese, Korean, Japanese, Russion, Thai, etc.)
	virtual void GetIMELanguageName( wchar_t *buf, int unicodeBufferSizeInBytes );
	// Returns the short code for the language (EN, CH, KO, JP, RU, TH, etc. ).
	virtual void GetIMELanguageShortCode( wchar_t *buf, int unicodeBufferSizeInBytes );

	// Call with NULL dest to get item count
	virtual int	 GetIMELanguageList( LanguageItem *dest, int destcount );
	virtual int	 GetIMEConversionModes( ConversionModeItem *dest, int destcount );
	virtual int	 GetIMESentenceModes( SentenceModeItem *dest, int destcount );

	virtual void OnChangeIMEByHandle( int handleValue );
	virtual void OnChangeIMEConversionModeByHandle( int handleValue );
	virtual void OnChangeIMESentenceModeByHandle( int handleValue );

	virtual void OnInputLanguageChanged();
	virtual void OnIMEStartComposition();
	virtual void OnIMEComposition( int flags );
	virtual void OnIMEEndComposition();

	virtual void OnIMEShowCandidates();
	virtual void OnIMEChangeCandidates();
	virtual void OnIMECloseCandidates();

	virtual void OnIMERecomputeModes();

	virtual int  GetCandidateListCount();
	virtual void GetCandidate( int num, wchar_t *dest, int destSizeBytes );
	virtual int  GetCandidateListSelectedItem();
	virtual int  GetCandidateListPageSize();
	virtual int  GetCandidateListPageStart();

	virtual void SetCandidateWindowPos( int x, int y );
	virtual bool GetShouldInvertCompositionString();
	virtual bool CandidateListStartsAtOne();

	virtual void SetCandidateListPageStart( int start );

	// Passes in a keycode which allows hitting other mouse buttons w/o cancelling capture mode
	virtual void SetMouseCaptureEx(VPANEL panel, MouseCode captureStartMouseCode );

	virtual void RegisterKeyCodeUnhandledListener( VPANEL panel );
	virtual void UnregisterKeyCodeUnhandledListener( VPANEL panel );

	// Posts unhandled message to all interested panels
	virtual void OnKeyCodeUnhandled( int keyCode );

	// Assumes subTree is a child panel of the root panel for the vgui contect
	//  if restrictMessagesToSubTree is true, then mouse and kb messages are only routed to the subTree and it's children and mouse/kb focus
	//   can only be on one of the subTree children, if a mouse click occurs outside of the subtree, and "UnhandledMouseClick" message is sent to unhandledMouseClickListener panel
	//   if it's set
	//  if restrictMessagesToSubTree is false, then mouse and kb messages are routed as normal except that they are not routed down into the subtree
	//   however, if a mouse click occurs outside of the subtree, and "UnhandleMouseClick" message is sent to unhandledMouseClickListener panel
	//   if it's set
	virtual void	SetModalSubTree( VPANEL subTree, VPANEL unhandledMouseClickListener, bool restrictMessagesToSubTree = true );
	virtual void	ReleaseModalSubTree();
	virtual VPANEL	GetModalSubTree();

	// These toggle whether the modal subtree is exclusively receiving messages or conversely whether it's being excluded from receiving messages
	virtual void	SetModalSubTreeReceiveMessages( bool state );
	virtual bool	ShouldModalSubTreeReceiveMessages() const;

	virtual VPANEL 	GetMouseCapture();

	virtual VPANEL	GetMouseFocus();
private:

	VPanel			*GetMouseFocusIgnoringModalSubtree();

	void InternalSetCompositionString( const wchar_t *compstr );
	void InternalShowCandidateWindow();
	void InternalHideCandidateWindow();
	void InternalUpdateCandidateWindow();

	bool PostKeyMessage(KeyValues *message);

	void DestroyCandidateList();
	void CreateNewCandidateList();

	VPanel *CalculateNewKeyFocus();

	void PostModalSubTreeMessage( VPanel *subTree, bool state );
	// returns true if the specified panel is a child of the current modal panel
	// if no modal panel is set, then this always returns TRUE
	bool IsChildOfModalSubTree(VPANEL panel);

	void SurfaceSetCursorPos( int x, int y );
	void SurfaceGetCursorPos( int &x, int &y );

	struct InputContext_t
	{
		VPANEL _rootPanel;

		bool _mousePressed[MOUSE_COUNT];
		bool _mouseDoublePressed[MOUSE_COUNT];
		bool _mouseDown[MOUSE_COUNT];
		bool _mouseReleased[MOUSE_COUNT];
		bool _keyPressed[BUTTON_CODE_COUNT];
		bool _keyTyped[BUTTON_CODE_COUNT];
		bool _keyDown[BUTTON_CODE_COUNT];
		bool _keyReleased[BUTTON_CODE_COUNT];

		VPanel *_keyFocus;
		VPanel *_oldMouseFocus;
		VPanel *_mouseFocus;   // the panel that has the current mouse focus - same as _mouseOver unless _mouseCapture is set
		VPanel *_mouseOver;	 // the panel that the mouse is currently over, NULL if not over any vgui item

		VPanel *_mouseCapture; // the panel that has currently captured mouse focus
		MouseCode m_MouseCaptureStartCode; // The Mouse button which was pressed to initiate mouse capture
		VPanel *_appModalPanel; // the modal dialog panel.

		int m_nCursorX;
		int m_nCursorY;

		int m_nLastPostedCursorX;
		int m_nLastPostedCursorY;

		int m_nExternallySetCursorX;
		int m_nExternallySetCursorY;
		bool m_bSetCursorExplicitly;

		CUtlVector< VPanel * >	m_KeyCodeUnhandledListeners;

		VPanel	*m_pModalSubTree;
		VPanel	*m_pUnhandledMouseClickListener;
		bool	m_bRestrictMessagesToModalSubTree;

		CKeyRepeatHandler m_keyRepeater;
	};

	void InitInputContext( InputContext_t *pContext );
	InputContext_t *GetInputContext( HInputContext context );
	void PanelDeleted(VPANEL focus, InputContext_t &context);

	HCursor _cursorOverride;

	const char *_keyTrans[KEY_LAST];

	InputContext_t m_DefaultInputContext; 
	HInputContext m_hContext; // current input context

	CUtlLinkedList< InputContext_t, HInputContext > m_Contexts;

#ifdef DO_IME
	void			*_imeWnd;
	CANDIDATELIST	*_imeCandidates;
#endif

	int		m_nDebugMessages;
};

CInputSystem g_Input;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CInputSystem, IInput, VGUI_INPUT_INTERFACE_VERSION, g_Input); // export IInput to everyone else, not IInputInternal!
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CInputSystem, IInputInternal, VGUI_INPUTINTERNAL_INTERFACE_VERSION, g_Input); // for use in external surfaces only! (like the engine surface)

namespace vgui
{
vgui::IInputInternal *g_pInput = &g_Input;
}


CInputSystem::CInputSystem()
{
	m_nDebugMessages = -1;
#ifdef DO_IME
	_imeWnd = null;
	_imeCandidates = null;
#endif
	InitInputContext( &m_DefaultInputContext );
	m_hContext = DEFAULT_INPUT_CONTEXT;

	// build key to text translation table
	// first byte unshifted key
	// second byte shifted key
	// the rest is the name of the key
	_keyTrans[KEY_0]			="0)KEY_0";
	_keyTrans[KEY_1]			="1!KEY_1";
	_keyTrans[KEY_2]			="2@KEY_2";
	_keyTrans[KEY_3]			="3#KEY_3";
	_keyTrans[KEY_4]			="4$KEY_4";
	_keyTrans[KEY_5]			="5%KEY_5";
	_keyTrans[KEY_6]			="6^KEY_6";
	_keyTrans[KEY_7]			="7&KEY_7";
	_keyTrans[KEY_8]			="8*KEY_8";
	_keyTrans[KEY_9]			="9(KEY_9";
	_keyTrans[KEY_A]			="aAKEY_A";
	_keyTrans[KEY_B]			="bBKEY_B";
	_keyTrans[KEY_C]			="cCKEY_C";
	_keyTrans[KEY_D]			="dDKEY_D";
	_keyTrans[KEY_E]			="eEKEY_E";
	_keyTrans[KEY_F]			="fFKEY_F";
	_keyTrans[KEY_G]			="gGKEY_G";
	_keyTrans[KEY_H]			="hHKEY_H";
	_keyTrans[KEY_I]			="iIKEY_I";
	_keyTrans[KEY_J]			="jJKEY_J";
	_keyTrans[KEY_K]			="kKKEY_K";
	_keyTrans[KEY_L]			="lLKEY_L"", L";
	_keyTrans[KEY_M]			="mMKEY_M";
	_keyTrans[KEY_N]			="nNKEY_N";
	_keyTrans[KEY_O]			="oOKEY_O";
	_keyTrans[KEY_P]			="pPKEY_P";
	_keyTrans[KEY_Q]			="qQKEY_Q";
	_keyTrans[KEY_R]			="rRKEY_R";
	_keyTrans[KEY_S]			="sSKEY_S";
	_keyTrans[KEY_T]			="tTKEY_T";
	_keyTrans[KEY_U]			="uUKEY_U";
	_keyTrans[KEY_V]			="vVKEY_V";
	_keyTrans[KEY_W]			="wWKEY_W";
	_keyTrans[KEY_X]			="xXKEY_X";
	_keyTrans[KEY_Y]			="yYKEY_Y";
	_keyTrans[KEY_Z]			="zZKEY_Z";
	_keyTrans[KEY_PAD_0]		="0\0KEY_PAD_0";
	_keyTrans[KEY_PAD_1]		="1\0KEY_PAD_1";
	_keyTrans[KEY_PAD_2]		="2\0KEY_PAD_2";
	_keyTrans[KEY_PAD_3]		="3\0KEY_PAD_3";
	_keyTrans[KEY_PAD_4]		="4\0KEY_PAD_4";
	_keyTrans[KEY_PAD_5]		="5\0KEY_PAD_5";
	_keyTrans[KEY_PAD_6]		="6\0KEY_PAD_6";
	_keyTrans[KEY_PAD_7]		="7\0KEY_PAD_7";
	_keyTrans[KEY_PAD_8]		="8\0KEY_PAD_8";
	_keyTrans[KEY_PAD_9]		="9\0KEY_PAD_9";
	_keyTrans[KEY_PAD_DIVIDE]	="//KEY_PAD_DIVIDE";
	_keyTrans[KEY_PAD_MULTIPLY]	="**KEY_PAD_MULTIPLY";
	_keyTrans[KEY_PAD_MINUS]	="--KEY_PAD_MINUS";
	_keyTrans[KEY_PAD_PLUS]		="++KEY_PAD_PLUS";
	_keyTrans[KEY_PAD_ENTER]	="\0\0KEY_PAD_ENTER";
	_keyTrans[KEY_PAD_DECIMAL]	=".\0KEY_PAD_DECIMAL"", L";
	_keyTrans[KEY_LBRACKET]		="[{KEY_LBRACKET";
	_keyTrans[KEY_RBRACKET]		="]}KEY_RBRACKET";
	_keyTrans[KEY_SEMICOLON]	=";:KEY_SEMICOLON";
	_keyTrans[KEY_APOSTROPHE]	="'\"KEY_APOSTROPHE";
	_keyTrans[KEY_BACKQUOTE]	="`~KEY_BACKQUOTE";
	_keyTrans[KEY_COMMA]		=",<KEY_COMMA";
	_keyTrans[KEY_PERIOD]		=".>KEY_PERIOD";
	_keyTrans[KEY_SLASH]		="/?KEY_SLASH";
	_keyTrans[KEY_BACKSLASH]	="\\|KEY_BACKSLASH";
	_keyTrans[KEY_MINUS]		="-_KEY_MINUS";
	_keyTrans[KEY_EQUAL]		="=+KEY_EQUAL"", L";
	_keyTrans[KEY_ENTER]		="\0\0KEY_ENTER";
	_keyTrans[KEY_SPACE]		="  KEY_SPACE";
	_keyTrans[KEY_BACKSPACE]	="\0\0KEY_BACKSPACE";
	_keyTrans[KEY_TAB]			="\0\0KEY_TAB";
	_keyTrans[KEY_CAPSLOCK]		="\0\0KEY_CAPSLOCK";
	_keyTrans[KEY_NUMLOCK]		="\0\0KEY_NUMLOCK";
	_keyTrans[KEY_ESCAPE]		="\0\0KEY_ESCAPE";
	_keyTrans[KEY_SCROLLLOCK]	="\0\0KEY_SCROLLLOCK";
	_keyTrans[KEY_INSERT]		="\0\0KEY_INSERT";
	_keyTrans[KEY_DELETE]		="\0\0KEY_DELETE";
	_keyTrans[KEY_HOME]			="\0\0KEY_HOME";
	_keyTrans[KEY_END]			="\0\0KEY_END";
	_keyTrans[KEY_PAGEUP]		="\0\0KEY_PAGEUP";
	_keyTrans[KEY_PAGEDOWN]		="\0\0KEY_PAGEDOWN";
	_keyTrans[KEY_BREAK]		="\0\0KEY_BREAK";
	_keyTrans[KEY_LSHIFT]		="\0\0KEY_LSHIFT";
	_keyTrans[KEY_RSHIFT]		="\0\0KEY_RSHIFT";
	_keyTrans[KEY_LALT]			="\0\0KEY_LALT";
	_keyTrans[KEY_RALT]			="\0\0KEY_RALT";
	_keyTrans[KEY_LCONTROL]		="\0\0KEY_LCONTROL"", L";
	_keyTrans[KEY_RCONTROL]		="\0\0KEY_RCONTROL"", L";
	_keyTrans[KEY_LWIN]			="\0\0KEY_LWIN";
	_keyTrans[KEY_RWIN]			="\0\0KEY_RWIN";
	_keyTrans[KEY_APP]			="\0\0KEY_APP";
	_keyTrans[KEY_UP]			="\0\0KEY_UP";
	_keyTrans[KEY_LEFT]			="\0\0KEY_LEFT";
	_keyTrans[KEY_DOWN]			="\0\0KEY_DOWN";
	_keyTrans[KEY_RIGHT]		="\0\0KEY_RIGHT";
	_keyTrans[KEY_F1]			="\0\0KEY_F1";
	_keyTrans[KEY_F2]			="\0\0KEY_F2";
	_keyTrans[KEY_F3]			="\0\0KEY_F3";
	_keyTrans[KEY_F4]			="\0\0KEY_F4";
	_keyTrans[KEY_F5]			="\0\0KEY_F5";
	_keyTrans[KEY_F6]			="\0\0KEY_F6";
	_keyTrans[KEY_F7]			="\0\0KEY_F7";
	_keyTrans[KEY_F8]			="\0\0KEY_F8";
	_keyTrans[KEY_F9]			="\0\0KEY_F9";
	_keyTrans[KEY_F10]			="\0\0KEY_F10";
	_keyTrans[KEY_F11]			="\0\0KEY_F11";
	_keyTrans[KEY_F12]			="\0\0KEY_F12";
}

CInputSystem::~CInputSystem()
{
	DestroyCandidateList();
}

//-----------------------------------------------------------------------------
// Resets an input context 
//-----------------------------------------------------------------------------
void CInputSystem::InitInputContext( InputContext_t *pContext )
{
	pContext->_rootPanel = NULL;
	pContext->_keyFocus = NULL;
	pContext->_oldMouseFocus = NULL;
	pContext->_mouseFocus = NULL;
	pContext->_mouseOver = NULL;
	pContext->_mouseCapture = NULL;
	pContext->_appModalPanel = NULL;

	pContext->m_nCursorX = pContext->m_nCursorY = 0;
	pContext->m_nLastPostedCursorX = pContext->m_nLastPostedCursorY = -9999;
	pContext->m_nExternallySetCursorX = pContext->m_nExternallySetCursorY = 0;
	pContext->m_bSetCursorExplicitly = false;

	// zero mouse and keys
	memset(pContext->_mousePressed, 0, sizeof(pContext->_mousePressed));
	memset(pContext->_mouseDoublePressed, 0, sizeof(pContext->_mouseDoublePressed));
	memset(pContext->_mouseDown, 0, sizeof(pContext->_mouseDown));
	memset(pContext->_mouseReleased, 0, sizeof(pContext->_mouseReleased));
	memset(pContext->_keyPressed, 0, sizeof(pContext->_keyPressed));
	memset(pContext->_keyTyped, 0, sizeof(pContext->_keyTyped));
	memset(pContext->_keyDown, 0, sizeof(pContext->_keyDown));
	memset(pContext->_keyReleased, 0, sizeof(pContext->_keyReleased));

	pContext->m_MouseCaptureStartCode = (MouseCode)-1;

	pContext->m_KeyCodeUnhandledListeners.RemoveAll();

	pContext->m_pModalSubTree = NULL;
	pContext->m_pUnhandledMouseClickListener = NULL;
	pContext->m_bRestrictMessagesToModalSubTree = false;
}

void CInputSystem::ResetInputContext( HInputContext context )
{
	// FIXME: Needs to release various keys, mouse buttons, etc...?
	// At least needs to cause things to lose focus
	InitInputContext( GetInputContext(context) );
}


//-----------------------------------------------------------------------------
// Creates/ destroys "input" contexts, which contains information
// about which controls have mouse + key focus, for example.
//-----------------------------------------------------------------------------
HInputContext CInputSystem::CreateInputContext()
{
	HInputContext i = m_Contexts.AddToTail();
	InitInputContext( &m_Contexts[i] );
	return i;
}

void CInputSystem::DestroyInputContext( HInputContext context )
{
	Assert( context != DEFAULT_INPUT_CONTEXT );
	if ( m_hContext == context )
	{
		ActivateInputContext( DEFAULT_INPUT_CONTEXT );
	}
	m_Contexts.Remove(context);
}


//-----------------------------------------------------------------------------
// Returns the current input context
//-----------------------------------------------------------------------------
CInputSystem::InputContext_t *CInputSystem::GetInputContext( HInputContext context )
{
	if (context == DEFAULT_INPUT_CONTEXT)
		return &m_DefaultInputContext;
	return &m_Contexts[context];
}


//-----------------------------------------------------------------------------
// Associates a particular panel with an input context
// Associating NULL is valid; it disconnects the panel from the context
//-----------------------------------------------------------------------------
void CInputSystem::AssociatePanelWithInputContext( HInputContext context, VPANEL pRoot )
{
	// Changing the root panel should invalidate keysettings, etc.
	if (GetInputContext(context)->_rootPanel != pRoot)
	{
		ResetInputContext( context );
		GetInputContext(context)->_rootPanel = pRoot;
	}
}


//-----------------------------------------------------------------------------
// Activates a particular input context, use DEFAULT_INPUT_CONTEXT
// to get the one normally used by VGUI
//-----------------------------------------------------------------------------
void CInputSystem::ActivateInputContext( HInputContext context )
{
	Assert( (context == DEFAULT_INPUT_CONTEXT) || m_Contexts.IsValidIndex(context) );
	m_hContext = context;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputSystem::RunFrame()
{
	if ( m_nDebugMessages == -1 )
	{
		m_nDebugMessages = CommandLine()->FindParm( "-vguifocus" ) ? 1 : 0;
	}

	InputContext_t *pContext = GetInputContext(m_hContext);

	// tick whoever has the focus
	if (pContext->_keyFocus)
	{
		// when modal dialogs are up messages only get sent to the dialogs children.
		if (IsChildOfModalPanel((VPANEL)pContext->_keyFocus))
		{	
			g_pIVgui->PostMessage((VPANEL)pContext->_keyFocus, new KeyValues("KeyFocusTicked"), NULL);
		}
	}

	// tick whoever has the focus
	if (pContext->_mouseFocus)
	{
		// when modal dialogs are up messages only get sent to the dialogs children.
		if (IsChildOfModalPanel((VPANEL)pContext->_mouseFocus))
		{	
			g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MouseFocusTicked"), NULL);
		}
	}
	// Mouse has wandered "off" the modal panel, just force a regular arrow cursor until it wanders back within the proper bounds
	else if ( pContext->_appModalPanel )
	{
		g_pSurface->SetCursor( vgui::dc_arrow );
	}

	//clear mouse and key states
	int i;
	for (i = 0; i < MOUSE_COUNT; i++)
	{
		pContext->_mousePressed[i] = 0;
		pContext->_mouseDoublePressed[i] = 0;
		pContext->_mouseReleased[i] = 0;
	}
	for (i = 0; i < BUTTON_CODE_COUNT; i++)
	{
		pContext->_keyPressed[i] = 0;
		pContext->_keyTyped[i] = 0;
		pContext->_keyReleased[i] = 0;
	}

	VPanel *wantedKeyFocus = CalculateNewKeyFocus();

	// make sure old and new focus get painted
	if (pContext->_keyFocus != wantedKeyFocus)
	{
		if (pContext->_keyFocus != NULL)
		{
			pContext->_keyFocus->Client()->InternalFocusChanged(true);

			// there may be out of order operations here, since we're directly calling SendMessage,
			// but we need to have focus messages happen immediately, since otherwise mouse events
			// happen out of order - more specifically, they happen before the focus changes

			// send a message to the window saying that it's losing focus
			{
				MEM_ALLOC_CREDIT();
				KeyValues *pMessage = new KeyValues( "KillFocus" );
				KeyValues::AutoDelete autodelete_pMessage( pMessage );
				pMessage->SetPtr( "newPanel", wantedKeyFocus );
				pContext->_keyFocus->SendMessage( pMessage, 0 );
			}

			if ( pContext->_keyFocus )
			{
				pContext->_keyFocus->Client()->Repaint();
			}

			// repaint the nearest popup as well, since it will need to redraw after losing focus
			VPanel *dlg = pContext->_keyFocus;
			while (dlg && !dlg->IsPopup())
			{
				dlg = dlg->GetParent();
			}
			if (dlg)
			{
				dlg->Client()->Repaint();
			}
		}
		if (wantedKeyFocus != NULL)
		{
			wantedKeyFocus->Client()->InternalFocusChanged(false);

			// there may be out of order operations here, since we're directly calling SendMessage,
			// but we need to have focus messages happen immediately, since otherwise mouse events
			// happen out of order - more specifically, they happen before the focus changes

			// send a message to the window saying that it's gaining focus
			{
				MEM_ALLOC_CREDIT();
				KeyValues *pMsg = new KeyValues("SetFocus");
				KeyValues::AutoDelete autodelete_pMsg( pMsg );
				wantedKeyFocus->SendMessage( pMsg, 0 );
			}
			wantedKeyFocus->Client()->Repaint();

			// repaint the nearest popup as well, since it will need to redraw after gaining focus
			VPanel *dlg = wantedKeyFocus;
			while (dlg && !dlg->IsPopup())
			{
				dlg = dlg->GetParent();
			}
			if (dlg)
			{
				dlg->Client()->Repaint();
			}
		}

		if ( m_nDebugMessages > 0 )
		{
			g_pIVgui->DPrintf2( "changing kb focus from %s to %s\n", 
				pContext->_keyFocus ? pContext->_keyFocus->GetName() : "(no name)",
				wantedKeyFocus ? wantedKeyFocus->GetName() : "(no name)" );
		}

		// accept the focus request
		pContext->_keyFocus = wantedKeyFocus;
		if (pContext->_keyFocus)
		{
			pContext->_keyFocus->MoveToFront();
		}
	}

	// Pump any key repeats
	KeyCode repeatCode = pContext->m_keyRepeater.KeyRepeated();
	if (repeatCode)
	{
		InternalKeyCodePressed( repeatCode );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the new key focus
//-----------------------------------------------------------------------------
VPanel *CInputSystem::CalculateNewKeyFocus()
{
	InputContext_t *pContext = GetInputContext(m_hContext);

	// get the top-order panel
	VPanel *wantedKeyFocus = NULL;

	VPanel *pRoot = (VPanel *)pContext->_rootPanel;
	VPanel *top = pRoot;
	if ( g_pSurface->GetPopupCount() > 0 )
	{
		// find the highest-level window that is both visible and a popup
		int nIndex = g_pSurface->GetPopupCount();

		while ( nIndex )
		{			
			top = (VPanel *)g_pSurface->GetPopup( --nIndex );

			// traverse the hierarchy and check if the popup really is visible
			if (top &&
				// top->IsPopup() &&  // These are right out of of the popups list!!!
				top->IsVisible() && 
				top->IsKeyBoardInputEnabled() && 
				!g_pSurface->IsMinimized((VPANEL)top) &&
				IsChildOfModalSubTree( (VPANEL)top ) &&
				(!pRoot || top->HasParent( pRoot )) )
			{
				bool bIsVisible = top->IsVisible();
				VPanel *p = top->GetParent();
				// drill down the hierarchy checking that everything is visible
				while(p && bIsVisible)
				{
					if( p->IsVisible()==false)
					{
						bIsVisible = false;
						break;
					}
					p=p->GetParent();
				}

				if ( bIsVisible && !g_pSurface->IsMinimized( (VPANEL)top ) )
					break;
			}

			top = pRoot;
		} 
	}

	if (top)
	{
		// ask the top-level panel for what it considers to be the current focus
		wantedKeyFocus = (VPanel *)top->Client()->GetCurrentKeyFocus();
		if (!wantedKeyFocus)
		{
			wantedKeyFocus = top;
		}
	}

	// check to see if any of this surfaces panels have the focus
	if (!g_pSurface->HasFocus())
	{
		wantedKeyFocus=NULL;
	}

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel((VPANEL)wantedKeyFocus))
	{	
		wantedKeyFocus=NULL;
	}

	return wantedKeyFocus;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputSystem::PanelDeleted(VPANEL vfocus, InputContext_t &context)
{
	VPanel *focus = (VPanel *)vfocus;
	if (context._keyFocus == focus)
	{
		if ( m_nDebugMessages > 0 )
		{
			g_pIVgui->DPrintf2( "removing kb focus %s\n", 
				context._keyFocus ? context._keyFocus->GetName() : "(no name)" );
		}
		context._keyFocus = NULL;
	}
	if (context._mouseOver == focus)
	{
		/*
		if ( m_nDebugMessages > 0 )
		{
			g_pIVgui->DPrintf2( "removing kb focus %s\n", 
				context._keyFocus ? pcontext._keyFocus->GetName() : "(no name)" );
		}
		*/
		context._mouseOver = NULL;
	}
	if (context._oldMouseFocus == focus)
	{
		context._oldMouseFocus = NULL;
	}
	if (context._mouseFocus == focus)
	{
		context._mouseFocus = NULL;
	}

	// NOTE: These two will only ever happen for the default context at the moment
	if (context._mouseCapture == focus)
	{
		SetMouseCapture(NULL);
		context._mouseCapture = NULL;
	}
	if (context._appModalPanel == focus)
	{
		ReleaseAppModalSurface();
	}
	if ( context.m_pUnhandledMouseClickListener == focus )
	{
		context.m_pUnhandledMouseClickListener = NULL;
	}
	if ( context.m_pModalSubTree == focus )
	{
		context.m_pModalSubTree = NULL;
		context.m_bRestrictMessagesToModalSubTree = false;
	}

	context.m_KeyCodeUnhandledListeners.FindAndRemove( focus );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *focus - 
//-----------------------------------------------------------------------------
void CInputSystem::PanelDeleted(VPANEL focus)
{
	HInputContext i;
	for (i = m_Contexts.Head(); i != m_Contexts.InvalidIndex(); i = m_Contexts.Next(i) )
	{
		PanelDeleted( focus, m_Contexts[i] );
	}
	PanelDeleted( focus, m_DefaultInputContext );
}




//-----------------------------------------------------------------------------
// Purpose: Sets the new mouse focus
//			won't override _mouseCapture settings
// Input  : newMouseFocus - 
//-----------------------------------------------------------------------------
void CInputSystem::SetMouseFocus(VPANEL newMouseFocus)
{
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel(newMouseFocus))
	{	
		return;	
	}

	bool wantsMouse, isPopup; // =  popup->GetMouseInput();
	VPanel *panel = (VPanel *)newMouseFocus;

	InputContext_t *pContext = GetInputContext( m_hContext );

	wantsMouse = false;
	if ( newMouseFocus )
	{
		do 
		{
			wantsMouse = panel->IsMouseInputEnabled();
			isPopup = panel->IsPopup();
			panel = panel->GetParent();
		}
		while ( wantsMouse && !isPopup && panel && panel->GetParent() ); // only consider panels that want mouse input
	}

	// if this panel doesn't want mouse input don't let it get focus
	if (newMouseFocus && !wantsMouse) 
	{
		return;
	}

	if ((VPANEL)pContext->_mouseOver != newMouseFocus || (!pContext->_mouseCapture && (VPANEL)pContext->_mouseFocus != newMouseFocus) )
	{
		pContext->_oldMouseFocus = pContext->_mouseOver;
		pContext->_mouseOver = (VPanel *)newMouseFocus;

		//tell the old panel with the mouseFocus that the cursor exited
		if ( pContext->_oldMouseFocus != NULL )
		{
			// only notify of entry if the mouse is not captured or we're the captured panel
			if ( !pContext->_mouseCapture || pContext->_oldMouseFocus == pContext->_mouseCapture )
			{
				g_pIVgui->PostMessage( (VPANEL)pContext->_oldMouseFocus, new KeyValues( "CursorExited" ), NULL );
			}
		}

		//tell the new panel with the mouseFocus that the cursor entered
		if ( pContext->_mouseOver != NULL )
		{
			// only notify of entry if the mouse is not captured or we're the captured panel
			if ( !pContext->_mouseCapture || pContext->_mouseOver == pContext->_mouseCapture )
			{
				g_pIVgui->PostMessage( (VPANEL)pContext->_mouseOver, new KeyValues( "CursorEntered" ), NULL );
			}
		}

		// set where the mouse is currently over
		// mouse capture overrides destination
		VPanel *newFocus = pContext->_mouseCapture ? pContext->_mouseCapture : pContext->_mouseOver;

		if ( m_nDebugMessages > 0 )
		{
			g_pIVgui->DPrintf2( "changing mouse focus from %s to %s\n", 
				pContext->_mouseFocus ? pContext->_mouseFocus->GetName() : "(no name)",
				newFocus ? newFocus->GetName() : "(no name)" );
		}


		pContext->_mouseFocus = newFocus;
	}
}

VPanel *CInputSystem::GetMouseFocusIgnoringModalSubtree()
{
	// find the panel that has the focus
	VPanel *focus = NULL; 

	InputContext_t *pContext = GetInputContext( m_hContext );

	int x, y;
	x = pContext->m_nCursorX;
	y = pContext->m_nCursorY;

	if (!pContext->_rootPanel)
	{
		if (g_pSurface->IsCursorVisible() && g_pSurface->IsWithin(x, y))
		{
			// faster version of code below
			// checks through each popup in order, top to bottom windows
			for (int i = g_pSurface->GetPopupCount() - 1; i >= 0; i--)
			{
				VPanel *popup = (VPanel *)g_pSurface->GetPopup(i);
				VPanel *panel = popup;
				bool wantsMouse = panel->IsMouseInputEnabled();
				bool isVisible = !g_pSurface->IsMinimized((VPANEL)panel);

				while ( isVisible && panel && panel->GetParent() ) // only consider panels that want mouse input
				{
					isVisible = panel->IsVisible();
					panel = panel->GetParent();
				}
				

				if ( wantsMouse && isVisible ) 
				{
					focus = (VPanel *)popup->Client()->IsWithinTraverse(x, y, false);
					if (focus)
						break;
				}
			}
			if (!focus)
			{
				focus = (VPanel *)((VPanel *)g_pSurface->GetEmbeddedPanel())->Client()->IsWithinTraverse(x, y, false);
			}
		}
	}
	else
	{
		focus = (VPanel *)((VPanel *)(pContext->_rootPanel))->Client()->IsWithinTraverse(x, y, false);
	}


	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( !IsChildOfModalPanel((VPANEL)focus, false ))
	{	
		// should this be _appModalPanel?
		focus = NULL;
	}

	return focus;
}



//-----------------------------------------------------------------------------
// Purpose: Calculates which panel the cursor is currently over and sets it up
//			as the current mouse focus.
//-----------------------------------------------------------------------------
void CInputSystem::UpdateMouseFocus(int x, int y)
{
	// find the panel that has the focus
	VPanel *focus = NULL; 

	InputContext_t *pContext = GetInputContext( m_hContext );

	if (g_pSurface->IsCursorVisible() && g_pSurface->IsWithin(x, y))
	{
		// faster version of code below
		// checks through each popup in order, top to bottom windows
		int c = g_pSurface->GetPopupCount();
		for (int i = c - 1; i >= 0; i--)
		{
			VPanel *popup = (VPanel *)g_pSurface->GetPopup(i);
			VPanel *panel = popup;

			if ( pContext->_rootPanel && !popup->HasParent((VPanel*)pContext->_rootPanel) )
			{
				// if we have a root panel, only consider popups that belong to it
				continue;
			}
#if defined( _DEBUG )
			char const *pchName = popup->GetName();
			NOTE_UNUSED( pchName );
#endif
			bool wantsMouse = panel->IsMouseInputEnabled() && IsChildOfModalSubTree( (VPANEL)panel );
			if ( !wantsMouse )
				continue;

			bool isVisible = !g_pSurface->IsMinimized((VPANEL)panel);
			if ( !isVisible )
				continue;

			while ( isVisible && panel && panel->GetParent() ) // only consider panels that want mouse input
			{
				isVisible = panel->IsVisible();
				panel = panel->GetParent();
			}
			

			if ( !wantsMouse || !isVisible ) 
				continue;

			focus = (VPanel *)popup->Client()->IsWithinTraverse(x, y, false);
			if (focus)
				break;
		}
		if (!focus)
		{
			focus = (VPanel *)((VPanel *)g_pSurface->GetEmbeddedPanel())->Client()->IsWithinTraverse(x, y, false);
		}
	}

	// mouse focus debugging code
	/*
	static VPanel *oldFocus = (VPanel *)0x0001;
	if (oldFocus != focus)
	{
		oldFocus = focus;
		if (focus)
		{
			g_pIVgui->DPrintf2("mouse over: (%s, %s)\n", focus->GetName(), focus->GetClassName());
		}
		else
		{
			g_pIVgui->DPrintf2("mouse over: (NULL)\n");
		}
	}
	*/

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel((VPANEL)focus))
	{	
		// should this be _appModalPanel?
		focus = NULL;
	}

	SetMouseFocus((VPANEL)focus);
}

// Passes in a keycode which allows hitting other mouse buttons w/o cancelling capture mode
void CInputSystem::SetMouseCaptureEx(VPANEL panel, MouseCode captureStartMouseCode )
{
	// This sets m_MouseCaptureStartCode to -1, so we set the real value afterward
	SetMouseCapture( panel );

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel(panel))
	{	
		return;	
	}

	InputContext_t *pContext = GetInputContext( m_hContext );
	Assert( pContext );
	pContext->m_MouseCaptureStartCode = captureStartMouseCode;
}

VPANEL CInputSystem::GetMouseCapture() 
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	return (VPANEL)pContext->_mouseCapture;
}

//-----------------------------------------------------------------------------
// Purpose: Sets or releases the mouse capture
// Input  : panel - pointer to the panel to get mouse capture
//			a NULL panel means that you want to clear the mouseCapture
//			MouseCaptureLost is sent to the panel that loses the mouse capture
//-----------------------------------------------------------------------------
void CInputSystem::SetMouseCapture(VPANEL panel)
{
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel(panel))
	{	
		return;	
	}

	InputContext_t *pContext = GetInputContext( m_hContext );
	Assert( pContext );

	pContext->m_MouseCaptureStartCode = (MouseCode)-1;

	// send a message if the panel is losing mouse capture
	if (pContext->_mouseCapture && panel != (VPANEL)pContext->_mouseCapture)
	{
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("MouseCaptureLost"), NULL);
	}

	if (panel == NULL)
	{
		if (pContext->_mouseCapture != NULL)
		{
			g_pSurface->EnableMouseCapture((VPANEL)pContext->_mouseCapture, false);
		}
	}
	else
	{
		g_pSurface->EnableMouseCapture(panel, true);
	}

	pContext->_mouseCapture = (VPanel *)panel;
}

// returns true if the specified panel is a child of the current modal panel
// if no modal panel is set, then this always returns TRUE
bool CInputSystem::IsChildOfModalSubTree(VPANEL panel)
{
	if ( !panel )
		return true;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->m_pModalSubTree )
	{
		// If panel is child of modal subtree, the allow messages to route to it if restrict messages is set
		bool isChildOfModal = ((VPanel *)panel)->HasParent(pContext->m_pModalSubTree );
		if ( isChildOfModal )
		{
			return pContext->m_bRestrictMessagesToModalSubTree;
		}
		// If panel is not a child of modal subtree, then only allow messages if we're not restricting them to the modal subtree
		else
		{
			return !pContext->m_bRestrictMessagesToModalSubTree;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: check if we are in modal state, 
// and if we are make sure this panel has the modal panel as a parent
//-----------------------------------------------------------------------------
bool CInputSystem::IsChildOfModalPanel(VPANEL panel, bool checkModalSubTree /*= true*/ )
{
	// NULL is ok.
	if (!panel)
		return true;

	InputContext_t *pContext = GetInputContext( m_hContext );

	// if we are in modal state, make sure this panel is a child of us.
	if (pContext->_appModalPanel)
	{	
		if (!((VPanel *)panel)->HasParent(pContext->_appModalPanel))
		{
			return false;
		}
	}

	if ( !checkModalSubTree )
		return true;

	// Defer to modal subtree logic instead...
	return IsChildOfModalSubTree( panel );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CInputSystem::GetFocus()
{
	return (VPANEL)( GetInputContext( m_hContext )->_keyFocus );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CInputSystem::GetCalculatedFocus()
{
	return (VPANEL) CalculateNewKeyFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CInputSystem::GetMouseOver()
{
	return (VPANEL)( GetInputContext( m_hContext )->_mouseOver );
}

VPANEL CInputSystem::GetMouseFocus()
{
	return (VPANEL)( GetInputContext( m_hContext )->_mouseFocus );
}

bool CInputSystem::WasMousePressed( MouseCode code )
{
	return GetInputContext( m_hContext )->_mousePressed[ code - MOUSE_FIRST ];
}

bool CInputSystem::WasMouseDoublePressed( MouseCode code )
{
	return GetInputContext( m_hContext )->_mouseDoublePressed[ code - MOUSE_FIRST ];
}

bool CInputSystem::IsMouseDown( MouseCode code )
{
	return GetInputContext( m_hContext )->_mouseDown[ code - MOUSE_FIRST ];
}

bool CInputSystem::WasMouseReleased( MouseCode code )
{
	return GetInputContext( m_hContext )->_mouseReleased[ code - MOUSE_FIRST ];
}

bool CInputSystem::WasKeyPressed( KeyCode code )
{
	return GetInputContext( m_hContext )->_keyPressed[ code - KEY_FIRST ];
}

bool CInputSystem::IsKeyDown( KeyCode code )
{
	return GetInputContext( m_hContext )->_keyDown[ code - KEY_FIRST ];
}

bool CInputSystem::WasKeyTyped( KeyCode code )
{
	return GetInputContext( m_hContext )->_keyTyped[ code - KEY_FIRST ];
}

bool CInputSystem::WasKeyReleased( KeyCode code )
{
	// changed from: only return true if the key was released and the passed in panel matches the keyFocus
	return GetInputContext( m_hContext )->_keyReleased[ code - KEY_FIRST ];
}


//-----------------------------------------------------------------------------
// Cursor position; this is the current position read from the input queue.
// We need to set it because client code may read this during Mouse Pressed
// events, etc.
//-----------------------------------------------------------------------------
void CInputSystem::UpdateCursorPosInternal( int x, int y )
{
	// Windows sends a CursorMoved message even when you haven't actually
	// moved the cursor, this means we are going into this fxn just by clicking
	// in the window. We only want to execute this code if we have actually moved
	// the cursor while dragging. So this code has been added to check
	// if we have actually moved from our previous position.
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->m_nCursorX == x && pContext->m_nCursorY == y )
		return;

	pContext->m_nCursorX = x;
	pContext->m_nCursorY = y;

	// Cursor has moved, so make sure the mouseFocus is current
	UpdateMouseFocus( x, y );
}


//-----------------------------------------------------------------------------
// This is called by panels to teleport the cursor
//-----------------------------------------------------------------------------
void CInputSystem::SetCursorPos( int x, int y )
{
	if ( IsDispatchingMessageQueue() )
	{
		InputContext_t *pContext = GetInputContext( m_hContext );
		pContext->m_nExternallySetCursorX = x;
		pContext->m_nExternallySetCursorY = y;
		pContext->m_bSetCursorExplicitly = true;
	}
	else
	{
		SurfaceSetCursorPos( x, y );
	}
}


void CInputSystem::GetCursorPos(int &x, int &y)
{
	if ( IsDispatchingMessageQueue() )
	{
		GetCursorPosition( x, y );
	}
	else
	{
		SurfaceGetCursorPos( x, y );
	}
}


// Here for backward compat
void CInputSystem::GetCursorPosition( int &x, int &y )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	x = pContext->m_nCursorX;
	y = pContext->m_nCursorY;
}

//-----------------------------------------------------------------------------
// Purpose: Converts a key code into a full key name
//-----------------------------------------------------------------------------
void CInputSystem::GetKeyCodeText(KeyCode code, char *buf, int buflen)
{
	if (!buf)
		return;

	// copy text into buf up to buflen in length
	// skip 2 in _keyTrans because the first two are for GetKeyCodeChar
	for (int i = 0; i < buflen; i++)
	{
		char ch = _keyTrans[code][i+2];
		buf[i] = ch;
		if (ch == 0)
			break;
	}

}


//-----------------------------------------------------------------------------
// Low-level cursor getting/setting functions 
//-----------------------------------------------------------------------------
void CInputSystem::SurfaceSetCursorPos(int x, int y)
{
	if ( g_pSurface->HasCursorPosFunctions() ) // does the surface export cursor functions for us to use?
	{
		g_pSurface->SurfaceSetCursorPos(x,y);
	}
	else
	{
		// translate into coordinates relative to surface
		int px, py, pw, pt;
		g_pSurface->GetAbsoluteWindowBounds(px, py, pw, pt);
		x += px;
		y += py;
		// set windows cursor pos
#ifdef WIN32
		::SetCursorPos(x, y);
#else
		// From Alfred on 8/15/2012.
		//   For l4d2, the vguimatsurface/cursor.cpp functions fire in the engine, the vgui2 ones
		// should be dormant (this isn't true for Steam however).
		//
		// If we ever do need to implement this, look at SDL_GetMouseState(), etc.
		Assert( !"CInputSystem::SurfaceSetCursorPos NYI" );
#endif
	}
}

void CInputSystem::SurfaceGetCursorPos( int &x, int &y )
{
#ifndef _X360 // X360TBD
	if ( g_pSurface->HasCursorPosFunctions() ) // does the surface export cursor functions for us to use?
	{
		g_pSurface->SurfaceGetCursorPos( x,y );
	}
	else
	{
#ifdef WIN32
		// get mouse position in windows
		POINT pnt;
		VCRHook_GetCursorPos(&pnt);
		x = pnt.x;
		y = pnt.y;

		// translate into coordinates relative to surface
		int px, py, pw, pt;
		g_pSurface->GetAbsoluteWindowBounds(px, py, pw, pt);
		x -= px;
		y -= py;
#else
		// From Alfred on 8/15/2012.
		//   For l4d2, the vguimatsurface/cursor.cpp functions fire in the engine, the vgui2 ones
		// should be dormant (this isn't true for Steam however).
		Assert( !"CInputSystem::SurfaceGetCursorPos NYI" );
		x = 0;
		y = 0;
#endif
	}
#else
	x = 0;
	y = 0;
#endif
}

void CInputSystem::SetCursorOveride(HCursor cursor)
{
	_cursorOverride = cursor;
}

HCursor CInputSystem::GetCursorOveride()
{
	return _cursorOverride;
}


//-----------------------------------------------------------------------------
// Called when we've detected cursor has moved via a windows message
//-----------------------------------------------------------------------------
bool CInputSystem::InternalCursorMoved(int x, int y)
{
	g_pIVgui->PostMessage((VPANEL) MESSAGE_CURSOR_POS, new KeyValues("SetCursorPosInternal", "xpos", x, "ypos", y), NULL);
	return true;
}


//-----------------------------------------------------------------------------
// Makes sure the windows cursor is in the right place after processing input 
//-----------------------------------------------------------------------------
void CInputSystem::HandleExplicitSetCursor( )
{
	InputContext_t *pContext = GetInputContext( m_hContext );

	if ( pContext->m_bSetCursorExplicitly )
	{
		pContext->m_nCursorX = pContext->m_nExternallySetCursorX;
		pContext->m_nCursorY = pContext->m_nExternallySetCursorY;
		pContext->m_bSetCursorExplicitly = false;

		// NOTE: This forces a cursor moved message to be posted next time
		pContext->m_nLastPostedCursorX = pContext->m_nLastPostedCursorY = -9999;

		SurfaceSetCursorPos( pContext->m_nCursorX, pContext->m_nCursorY );
		UpdateMouseFocus( pContext->m_nCursorX, pContext->m_nCursorY ); 
	}
}


//-----------------------------------------------------------------------------
// Called when we've detected cursor has moved via a windows message
//-----------------------------------------------------------------------------
void CInputSystem::PostCursorMessage( )
{
	InputContext_t *pContext = GetInputContext( m_hContext );

	if ( pContext->m_bSetCursorExplicitly )
	{
		// NOTE m_bSetCursorExplicitly will be reset to false in HandleExplicitSetCursor
		pContext->m_nCursorX = pContext->m_nExternallySetCursorX;
		pContext->m_nCursorY = pContext->m_nExternallySetCursorY;
	}

	if ( pContext->m_nLastPostedCursorX == pContext->m_nCursorX && pContext->m_nLastPostedCursorY == pContext->m_nCursorY )
		return;

	pContext->m_nLastPostedCursorX = pContext->m_nCursorX;
	pContext->m_nLastPostedCursorY = pContext->m_nCursorY;

	if ( pContext->_mouseCapture )
	{
		if (!IsChildOfModalPanel((VPANEL)pContext->_mouseCapture))
			return;	

		// the panel with mouse capture gets all messages
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("CursorMoved", "xpos", pContext->m_nCursorX, "ypos", pContext->m_nCursorY), NULL);
	}
	else if (pContext->_mouseFocus != NULL)
	{
		// mouse focus is current from UpdateMouse focus
		// so the appmodal check has already been made.
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("CursorMoved", "xpos", pContext->m_nCursorX, "ypos", pContext->m_nCursorY), NULL);
	}
}

bool CInputSystem::InternalMousePressed(MouseCode code)
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;

	InputContext_t *pContext = GetInputContext( m_hContext );
	VPanel *pTargetPanel = pContext->_mouseOver;
	if ( pContext->_mouseCapture && IsChildOfModalPanel((VPANEL)pContext->_mouseCapture))
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		bFilter = true;

		bool captureLost = code == pContext->m_MouseCaptureStartCode || pContext->m_MouseCaptureStartCode == (MouseCode)-1;

		// the panel with mouse capture gets all messages
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("MousePressed", "code", code), NULL);
		pTargetPanel = pContext->_mouseCapture;

		if ( captureLost )
		{
			// this has to happen after MousePressed so the panel doesn't Think it got a mouse press after it lost capture
			SetMouseCapture(NULL);
		}
	}
	else if ( (pContext->_mouseFocus != NULL) && IsChildOfModalPanel((VPANEL)pContext->_mouseFocus) )
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		bFilter = true;

		// tell the panel with the mouseFocus that the mouse was presssed
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MousePressed", "code", code), NULL);
//		g_pIVgui->DPrintf2("MousePressed: (%s, %s)\n", _mouseFocus->GetName(), _mouseFocus->GetClassName());
		pTargetPanel = pContext->_mouseFocus;
	}
	else if ( pContext->m_pModalSubTree && pContext->m_pUnhandledMouseClickListener )
	{
		VPanel *p = GetMouseFocusIgnoringModalSubtree();
		if ( p )
		{
			bool isChildOfModal = IsChildOfModalSubTree( (VPANEL)p );
			bool isUnRestricted = !pContext->m_bRestrictMessagesToModalSubTree;

			if ( isUnRestricted != isChildOfModal )
			{
				// The faked mouse wheel button messages are specifically ignored by vgui
				if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
					return true;

				g_pIVgui->PostMessage( ( VPANEL )pContext->m_pUnhandledMouseClickListener, new KeyValues( "UnhandledMouseClick", "code", code ), NULL );
				pTargetPanel = pContext->m_pUnhandledMouseClickListener;
				bFilter = true;
			}
		}
	}


	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( IsChildOfModalPanel( (VPANEL)pTargetPanel ) )
	{	
		g_pSurface->SetTopLevelFocus( (VPANEL)pTargetPanel );
	}

	return bFilter;
}

bool CInputSystem::InternalMouseDoublePressed(MouseCode code)
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;

	InputContext_t *pContext = GetInputContext( m_hContext );
	VPanel *pTargetPanel = pContext->_mouseOver;
	if ( pContext->_mouseCapture && IsChildOfModalPanel((VPANEL)pContext->_mouseCapture))
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		// the panel with mouse capture gets all messages
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("MouseDoublePressed", "code", code), NULL);
		pTargetPanel = pContext->_mouseCapture;
		bFilter = true;
	}
	else if ( (pContext->_mouseFocus != NULL) && IsChildOfModalPanel((VPANEL)pContext->_mouseFocus))
	{			
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		// tell the panel with the mouseFocus that the mouse was double presssed
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MouseDoublePressed", "code", code), NULL);
		pTargetPanel = pContext->_mouseFocus;
		bFilter = true;
	}

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (IsChildOfModalPanel((VPANEL)pTargetPanel))
	{	
		g_pSurface->SetTopLevelFocus((VPANEL)pTargetPanel);
	}

	return bFilter;
}

bool CInputSystem::InternalMouseReleased( MouseCode code )
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if (pContext->_mouseCapture && IsChildOfModalPanel((VPANEL)pContext->_mouseCapture))
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		// the panel with mouse capture gets all messages
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("MouseReleased", "code", code), NULL );
		bFilter = true;
	}
	else if ((pContext->_mouseFocus != NULL) && IsChildOfModalPanel((VPANEL)pContext->_mouseFocus))
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		//tell the panel with the mouseFocus that the mouse was release
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MouseReleased", "code", code), NULL );
		bFilter = true;
	}

	return bFilter;
}

bool CInputSystem::InternalMouseWheeled(int delta)
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ((pContext->_mouseFocus != NULL) && IsChildOfModalPanel((VPANEL)pContext->_mouseFocus))
	{
		// the mouseWheel works with the mouseFocus, not the keyFocus
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MouseWheeled", "delta", delta), NULL);
		bFilter = true;
	}
	return bFilter;
}

//-----------------------------------------------------------------------------
// Updates the internal key/mouse state associated with the current input context without sending messages
//-----------------------------------------------------------------------------
void CInputSystem::SetMouseCodeState( MouseCode code, MouseCodeState_t state )
{
	if ( !IsMouseCode( code ) )
		return;

	InputContext_t *pContext = GetInputContext( m_hContext );
	switch( state )
	{
	case BUTTON_RELEASED:
		pContext->_mouseReleased[ code - MOUSE_FIRST ] = 1;
		break;

	case BUTTON_PRESSED:
		pContext->_mousePressed[ code - MOUSE_FIRST ] = 1;
		break;

	case BUTTON_DOUBLECLICKED:
		pContext->_mouseDoublePressed[ code - MOUSE_FIRST ] = 1;
		break;
	}

	pContext->_mouseDown[ code - MOUSE_FIRST ] = ( state != BUTTON_RELEASED );
}

void CInputSystem::SetKeyCodeState( KeyCode code, bool bPressed )
{
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( bPressed )
	{
		//set key state
		pContext->_keyPressed[ code - KEY_FIRST ] = 1;
	}
	else
	{
		// set key state
		pContext->_keyReleased[ code - KEY_FIRST ] = 1;
	}
	pContext->_keyDown[ code - KEY_FIRST ] = bPressed;
}

void CInputSystem::UpdateButtonState( const InputEvent_t &event )
{
	switch( event.m_nType )
	{
	case IE_ButtonPressed:
	case IE_ButtonReleased:
	case IE_ButtonDoubleClicked:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;

			// FIXME: Workaround hack
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				SetKeyCodeState( code, ( event.m_nType != IE_ButtonReleased ) );
				break;
			}

			if ( IsMouseCode( code ) )
			{
				MouseCodeState_t state;
				state = ( event.m_nType == IE_ButtonReleased ) ? vgui::BUTTON_RELEASED : vgui::BUTTON_PRESSED;
				if ( event.m_nType == IE_ButtonDoubleClicked )
				{
					state = vgui::BUTTON_DOUBLECLICKED;
				}

				SetMouseCodeState( code, state );
				break;
			}
		}
		break;
	}
}

bool CInputSystem::InternalKeyCodePressed( KeyCode code )
{
	InputContext_t *pContext = GetInputContext( m_hContext );

	// mask out bogus keys
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return false;

	bool bFilter = PostKeyMessage( new KeyValues("KeyCodePressed", "code", code ) );
	if ( bFilter )
	{
		// Only notice the key down for repeating if we actually used the key
		pContext->m_keyRepeater.KeyDown( code );
	}
	return bFilter;
}

void CInputSystem::InternalKeyCodeTyped( KeyCode code )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	// mask out bogus keys
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return;

	// set key state
	pContext->_keyTyped[ code - KEY_FIRST ] = 1;

	// tell the current focused panel that a key was typed
	PostKeyMessage(new KeyValues("KeyCodeTyped", "code", code));
}

void CInputSystem::InternalKeyTyped(wchar_t unichar)
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	// set key state
	if( unichar <= KEY_LAST )
	{
		pContext->_keyTyped[unichar]=1;
	}

	// tell the current focused panel that a key was typed
	PostKeyMessage(new KeyValues("KeyTyped", "unichar", unichar));
}

bool CInputSystem::InternalKeyCodeReleased( KeyCode code )
{	
	InputContext_t *pContext = GetInputContext( m_hContext );

	// mask out bogus keys
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return false;

	pContext->m_keyRepeater.KeyUp( code );

	return PostKeyMessage(new KeyValues("KeyCodeReleased", "code", code));
}

//-----------------------------------------------------------------------------
// Purpose: posts a message to the key focus if it's valid
//-----------------------------------------------------------------------------
bool CInputSystem::PostKeyMessage(KeyValues *message)
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if( (pContext->_keyFocus!= NULL) && IsChildOfModalPanel((VPANEL)pContext->_keyFocus))
	{
#ifdef _X360
		g_pIVgui->PostMessage((VPANEL) MESSAGE_CURRENT_KEYFOCUS, message, NULL );
#else
		//tell the current focused panel that a key was released
		g_pIVgui->PostMessage((VPANEL)pContext->_keyFocus, message, NULL );
#endif
		return true;
	}

	message->deleteThis();
	return false;
}

VPANEL CInputSystem::GetAppModalSurface()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	return (VPANEL)pContext->_appModalPanel;
}

void CInputSystem::SetAppModalSurface(VPANEL panel)
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	pContext->_appModalPanel = (VPanel *)panel;
}


void CInputSystem::ReleaseAppModalSurface()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	pContext->_appModalPanel = NULL;
}


#ifdef DO_IME

enum LANGFLAG
{
   ENGLISH,				
   TRADITIONAL_CHINESE,	
   JAPANESE,
   KOREAN,
   SIMPLIFIED_CHINESE,
   UNKNOWN,

   NUM_IMES_SUPPORTED
} LangFlag;  

struct LanguageIds
{
	// char const			*idname;
	unsigned short			id;
	int						languageflag;
	wchar_t	const			*shortcode;
	wchar_t const			*displayname;
	bool					invertcomposition;
};

LanguageIds g_LanguageIds[] =
{
	{ 0x0000, UNKNOWN, L"",		L"Neutral" }, 
	{ 0x007f, UNKNOWN, L"",		L"Invariant" }, 
	{ 0x0400, UNKNOWN, L"",		L"User Default Language" },  
	{ 0x0800, UNKNOWN, L"",		L"System Default Language" },  
	{ 0x0436, UNKNOWN, L"AF",	L"Afrikaans" },  
	{ 0x041c, UNKNOWN, L"SQ",	L"Albanian" },  
	{ 0x0401, UNKNOWN, L"AR",	L"Arabic (Saudi Arabia)" },  
	{ 0x0801, UNKNOWN, L"AR",	L"Arabic (Iraq)" },  
	{ 0x0c01, UNKNOWN, L"AR",	L"Arabic (Egypt)" },  
	{ 0x1001, UNKNOWN, L"AR",	L"Arabic (Libya)" },  
	{ 0x1401, UNKNOWN, L"AR",	L"Arabic (Algeria)" },  
	{ 0x1801, UNKNOWN, L"AR",	L"Arabic (Morocco)" },  
	{ 0x1c01, UNKNOWN, L"AR",	L"Arabic (Tunisia)" },  
	{ 0x2001, UNKNOWN, L"AR",	L"Arabic (Oman)" },  
	{ 0x2401, UNKNOWN, L"AR",	L"Arabic (Yemen)" },  
	{ 0x2801, UNKNOWN, L"AR",	L"Arabic (Syria)" },  
	{ 0x2c01, UNKNOWN, L"AR",	L"Arabic (Jordan)" },  
	{ 0x3001, UNKNOWN, L"AR",	L"Arabic (Lebanon)" },  
	{ 0x3401, UNKNOWN, L"AR",	L"Arabic (Kuwait)" },  
	{ 0x3801, UNKNOWN, L"AR",	L"Arabic (U.A.E.)" },  
	{ 0x3c01, UNKNOWN, L"AR",	L"Arabic (Bahrain)" },  
	{ 0x4001, UNKNOWN, L"AR",	L"Arabic (Qatar)" },  
	{ 0x042b, UNKNOWN, L"HY",	L"Armenian" },  
	{ 0x042c, UNKNOWN, L"AZ",	L"Azeri (Latin)" },  
	{ 0x082c, UNKNOWN, L"AZ",	L"Azeri (Cyrillic)" },  
	{ 0x042d, UNKNOWN, L"ES",	L"Basque" },  
	{ 0x0423, UNKNOWN, L"BE",	L"Belarusian" },  
	{ 0x0445, UNKNOWN, L"",		L"Bengali (India)" },  
	{ 0x141a, UNKNOWN, L"",		L"Bosnian (Bosnia and Herzegovina)" },  
	{ 0x0402, UNKNOWN, L"BG",	L"Bulgarian" },  
	{ 0x0455, UNKNOWN, L"",		L"Burmese" },  
	{ 0x0403, UNKNOWN, L"CA",	L"Catalan" },  
	{ 0x0404, TRADITIONAL_CHINESE, L"CHT", L"#IME_0404", true },  
	{ 0x0804, SIMPLIFIED_CHINESE, L"CHS", L"#IME_0804", true },  
	{ 0x0c04, UNKNOWN, L"CH",	L"Chinese (Hong Kong SAR, PRC)" },  
	{ 0x1004, UNKNOWN, L"CH",	L"Chinese (Singapore)" },  
	{ 0x1404, UNKNOWN, L"CH",	L"Chinese (Macao SAR)" },  
	{ 0x041a, UNKNOWN, L"HR",	L"Croatian" },  
	{ 0x101a, UNKNOWN, L"HR",	L"Croatian (Bosnia and Herzegovina)" },  
	{ 0x0405, UNKNOWN, L"CZ",	L"Czech" },  
	{ 0x0406, UNKNOWN, L"DK",	L"Danish" },  
	{ 0x0465, UNKNOWN, L"MV",	L"Divehi" },  
	{ 0x0413, UNKNOWN, L"NL",	L"Dutch (Netherlands)" },  
	{ 0x0813, UNKNOWN, L"BE",	L"Dutch (Belgium)" },  
	{ 0x0409, ENGLISH, L"EN",	L"#IME_0409" },  
	{ 0x0809, ENGLISH, L"EN",	L"English (United Kingdom)" },  
	{ 0x0c09, ENGLISH, L"EN",	L"English (Australian)" },  
	{ 0x1009, ENGLISH, L"EN",	L"English (Canadian)" },  
	{ 0x1409, ENGLISH, L"EN",	L"English (New Zealand)" },  
	{ 0x1809, ENGLISH, L"EN",	L"English (Ireland)" },  
	{ 0x1c09, ENGLISH, L"EN",	L"English (South Africa)" },  
	{ 0x2009, ENGLISH, L"EN",	L"English (Jamaica)" },  
	{ 0x2409, ENGLISH, L"EN",	L"English (Caribbean)" },  
	{ 0x2809, ENGLISH, L"EN",	L"English (Belize)" },  
	{ 0x2c09, ENGLISH, L"EN",	L"English (Trinidad)" },  
	{ 0x3009, ENGLISH, L"EN",	L"English (Zimbabwe)" },  
	{ 0x3409, ENGLISH, L"EN",	L"English (Philippines)" },  
	{ 0x0425, UNKNOWN, L"ET",	L"Estonian" },  
	{ 0x0438, UNKNOWN, L"FO",	L"Faeroese" },  
	{ 0x0429, UNKNOWN, L"FA",	L"Farsi" },  
	{ 0x040b, UNKNOWN, L"FI",	L"Finnish" },  
	{ 0x040c, UNKNOWN, L"FR",	L"#IME_040c" },  
	{ 0x080c, UNKNOWN, L"FR",	L"French (Belgian)" },  
	{ 0x0c0c, UNKNOWN, L"FR",	L"French (Canadian)" },  
	{ 0x100c, UNKNOWN, L"FR",	L"French (Switzerland)" },  
	{ 0x140c, UNKNOWN, L"FR",	L"French (Luxembourg)" },  
	{ 0x180c, UNKNOWN, L"FR",	L"French (Monaco)" },  
	{ 0x0456, UNKNOWN, L"GL",	L"Galician" },  
	{ 0x0437, UNKNOWN, L"KA",	L"Georgian" },  
	{ 0x0407, UNKNOWN, L"DE",	L"#IME_0407" },  
	{ 0x0807, UNKNOWN, L"DE",	L"German (Switzerland)" },  
	{ 0x0c07, UNKNOWN, L"DE",	L"German (Austria)" },  
	{ 0x1007, UNKNOWN, L"DE",	L"German (Luxembourg)" },  
	{ 0x1407, UNKNOWN, L"DE",	L"German (Liechtenstein)" },  
	{ 0x0408, UNKNOWN, L"GR",	L"Greek" },  
	{ 0x0447, UNKNOWN, L"IN",	L"Gujarati" },  
	{ 0x040d, UNKNOWN, L"HE",	L"Hebrew" },  
	{ 0x0439, UNKNOWN, L"HI",	L"Hindi" },  
	{ 0x040e, UNKNOWN, L"HU",	L"Hungarian" },  
	{ 0x040f, UNKNOWN, L"IS",	L"Icelandic" },  
	{ 0x0421, UNKNOWN, L"ID",	L"Indonesian" },  
	{ 0x0434, UNKNOWN, L"",		L"isiXhosa/Xhosa (South Africa)" },  
	{ 0x0435, UNKNOWN, L"",		L"isiZulu/Zulu (South Africa)" },  
	{ 0x0410, UNKNOWN, L"IT",	L"#IME_0410" },  
	{ 0x0810, UNKNOWN, L"IT",	L"Italian (Switzerland)" },  
	{ 0x0411, JAPANESE, L"JP",	L"#IME_0411" },  
	{ 0x044b, UNKNOWN, L"IN",	L"Kannada" },  
	{ 0x0457, UNKNOWN, L"IN",	L"Konkani" },  
	{ 0x0412, KOREAN, L"KR",	L"#IME_0412" },  
	{ 0x0812, UNKNOWN, L"KR",	L"Korean (Johab)" },  
	{ 0x0440, UNKNOWN, L"KZ",	L"Kyrgyz." },  
	{ 0x0426, UNKNOWN, L"LV",	L"Latvian" },  
	{ 0x0427, UNKNOWN, L"LT",	L"Lithuanian" },  
	{ 0x0827, UNKNOWN, L"LT",	L"Lithuanian (Classic)" },  
	{ 0x042f, UNKNOWN, L"MK",	L"FYRO Macedonian" },  
	{ 0x043e, UNKNOWN, L"MY",	L"Malay (Malaysian)" },  
	{ 0x083e, UNKNOWN, L"MY",	L"Malay (Brunei Darussalam)" },  
	{ 0x044c, UNKNOWN, L"IN",	L"Malayalam (India)" },  
	{ 0x0481, UNKNOWN, L"",		L"Maori (New Zealand)" },  
	{ 0x043a, UNKNOWN, L"",		L"Maltese (Malta)" },  
	{ 0x044e, UNKNOWN, L"IN",	L"Marathi" },  
	{ 0x0450, UNKNOWN, L"MN",	L"Mongolian" },  
	{ 0x0414, UNKNOWN, L"NO",	L"Norwegian (Bokmal)" },  
	{ 0x0814, UNKNOWN, L"NO",	L"Norwegian (Nynorsk)" },  
	{ 0x0415, UNKNOWN, L"PL",	L"Polish" },  
	{ 0x0416, UNKNOWN, L"PT",	L"Portuguese (Brazil)" },  
	{ 0x0816, UNKNOWN, L"PT",	L"Portuguese (Portugal)" },  
	{ 0x0446, UNKNOWN, L"IN",	L"Punjabi" },  
	{ 0x046b, UNKNOWN, L"",		L"Quechua (Bolivia)" },  
	{ 0x086b, UNKNOWN, L"",		L"Quechua (Ecuador)" },  
	{ 0x0c6b, UNKNOWN, L"",		L"Quechua (Peru)" },  
	{ 0x0418, UNKNOWN, L"RO",	L"Romanian" },  
	{ 0x0419, UNKNOWN, L"RU",	L"#IME_0419" },  
	{ 0x044f, UNKNOWN, L"IN",	L"Sanskrit" },  
	{ 0x043b, UNKNOWN, L"",		L"Sami, Northern (Norway)" },  
	{ 0x083b, UNKNOWN, L"",		L"Sami, Northern (Sweden)" },  
	{ 0x0c3b, UNKNOWN, L"",		L"Sami, Northern (Finland)" },  
	{ 0x103b, UNKNOWN, L"",		L"Sami, Lule (Norway)" },  
	{ 0x143b, UNKNOWN, L"",		L"Sami, Lule (Sweden)" },  
	{ 0x183b, UNKNOWN, L"",		L"Sami, Southern (Norway)" },  
	{ 0x1c3b, UNKNOWN, L"",		L"Sami, Southern (Sweden)" },  
	{ 0x203b, UNKNOWN, L"",		L"Sami, Skolt (Finland)" },  
	{ 0x243b, UNKNOWN, L"",		L"Sami, Inari (Finland)" },  
	{ 0x0c1a, UNKNOWN, L"SR",	L"Serbian (Cyrillic)" },  
	{ 0x1c1a, UNKNOWN, L"SR",	L"Serbian (Cyrillic, Bosnia, and Herzegovina)" },  
	{ 0x081a, UNKNOWN, L"SR",	L"Serbian (Latin)" },  
	{ 0x181a, UNKNOWN, L"SR",	L"Serbian (Latin, Bosnia, and Herzegovina)" }, 
	{ 0x046c, UNKNOWN, L"",		L"Sesotho sa Leboa/Northern Sotho (South Africa)" },  
	{ 0x0432, UNKNOWN, L"",		L"Setswana/Tswana (South Africa)" },  
	{ 0x041b, UNKNOWN, L"SK",	L"Slovak" },  
	{ 0x0424, UNKNOWN, L"SI",	L"Slovenian" },  
	{ 0x040a, UNKNOWN, L"ES",	L"#IME_040a" },  
	{ 0x080a, UNKNOWN, L"ES",	L"Spanish (Mexican)" },  
	{ 0x0c0a, UNKNOWN, L"ES",	L"Spanish (Spain, Modern Sort)" },  
	{ 0x100a, UNKNOWN, L"ES",	L"Spanish (Guatemala)" },  
	{ 0x140a, UNKNOWN, L"ES",	L"Spanish (Costa Rica)" },  
	{ 0x180a, UNKNOWN, L"ES",	L"Spanish (Panama)" },  
	{ 0x1c0a, UNKNOWN, L"ES",	L"Spanish (Dominican Republic)" },  
	{ 0x200a, UNKNOWN, L"ES",	L"Spanish (Venezuela)" },  
	{ 0x240a, UNKNOWN, L"ES",	L"Spanish (Colombia)" },  
	{ 0x280a, UNKNOWN, L"ES",	L"Spanish (Peru)" },  
	{ 0x2c0a, UNKNOWN, L"ES",	L"Spanish (Argentina)" },  
	{ 0x300a, UNKNOWN, L"ES",	L"Spanish (Ecuador)" },  
	{ 0x340a, UNKNOWN, L"ES",	L"Spanish (Chile)" },  
	{ 0x380a, UNKNOWN, L"ES",	L"Spanish (Uruguay)" },  
	{ 0x3c0a, UNKNOWN, L"ES",	L"Spanish (Paraguay)" },  
	{ 0x400a, UNKNOWN, L"ES",	L"Spanish (Bolivia)" },  
	{ 0x440a, UNKNOWN, L"ES",	L"Spanish (El Salvador)" },  
	{ 0x480a, UNKNOWN, L"ES",	L"Spanish (Honduras)" },  
	{ 0x4c0a, UNKNOWN, L"ES",	L"Spanish (Nicaragua)" },  
	{ 0x500a, UNKNOWN, L"ES",	L"Spanish (Puerto Rico)" },  
	{ 0x0430, UNKNOWN, L"",		L"Sutu" },  
	{ 0x0441, UNKNOWN, L"KE",	L"Swahili (Kenya)" },  
	{ 0x041d, UNKNOWN, L"SV",	L"Swedish" },  
	{ 0x081d, UNKNOWN, L"SV",	L"Swedish (Finland)" },  
	{ 0x045a, UNKNOWN, L"SY",	L"Syriac" },  
	{ 0x0449, UNKNOWN, L"IN",	L"Tamil" },  
	{ 0x0444, UNKNOWN, L"RU",	L"Tatar (Tatarstan)" },  
	{ 0x044a, UNKNOWN, L"IN",	L"Telugu" },  
	{ 0x041e, UNKNOWN, L"TH",	L"#IME_041e" },  
	{ 0x041f, UNKNOWN, L"TR",	L"Turkish" },  
	{ 0x0422, UNKNOWN, L"UA",	L"Ukrainian" },  
	{ 0x0420, UNKNOWN, L"PK",	L"Urdu (Pakistan)" },  
	{ 0x0820, UNKNOWN, L"IN",	L"Urdu (India)" },  
	{ 0x0443, UNKNOWN, L"UZ",	L"Uzbek (Latin)" },  
	{ 0x0843, UNKNOWN, L"UZ",	L"Uzbek (Cyrillic)" },  
	{ 0x042a, UNKNOWN, L"VN",	L"Vietnamese" },  
	{ 0x0452, UNKNOWN, L"",		L"Welsh (United Kingdom)" }, 
};

static LanguageIds *GetLanguageInfo( unsigned short id )
{
	for ( int j = 0; j < sizeof( g_LanguageIds ) / sizeof( g_LanguageIds[ 0 ] ); ++j )
	{
		if ( g_LanguageIds[ j ].id == id )
		{
			return &g_LanguageIds[ j ];
			break;
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CIMEDlg message handlers
static bool IsIDInList( unsigned short id, int count, HKL *list )
{
	for ( int i = 0; i < count; ++i )
	{
		if ( LOWORD( list[ i ] ) == id )
		{
			return true;
		}
	}
	return false;
}

static const wchar_t *GetLanguageName( unsigned short id )
{
	wchar_t const *name = L"???";
	for ( int j = 0; j < sizeof( g_LanguageIds ) / sizeof( g_LanguageIds[ 0 ] ); ++j )
	{
		if ( g_LanguageIds[ j ].id == id )
		{
			name = g_LanguageIds[ j ].displayname;
			break;
		}
	}
	return name;
}

#endif // DO_IME

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *hwnd - 
//-----------------------------------------------------------------------------
void CInputSystem::SetIMEWindow( void *hwnd )
{
#ifdef DO_IME
	_imeWnd = hwnd;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void *CInputSystem::GetIMEWindow()
{
#ifdef DO_IME
	return _imeWnd;
#else
	return NULL;
#endif
}

#ifdef DO_IME
static void SpewIMEInfo( int langid )
{
	LanguageIds *info = GetLanguageInfo( langid );
	if ( info )
	{
		wchar_t const *name = info->shortcode ? info->shortcode : L"???";
		wchar_t outstr[ 512 ];
		V_swprintf_safe( outstr, L"IME language changed to:  %s", name );
		OutputDebugStringW( outstr );
		OutputDebugStringW( L"\n" );
	}
}
#endif // DO_IME

// Change keyboard layout type
void CInputSystem::OnChangeIME( bool forward )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	HKL currentKb = GetKeyboardLayout( 0 );

	UINT numKBs = GetKeyboardLayoutList( 0, NULL );
	if ( numKBs > 0 )
	{
		HKL *list = new HKL[ numKBs ];

		GetKeyboardLayoutList( numKBs, list );

		int oldKb = 0;
		CUtlVector< HKL >	selections;

		for ( unsigned int i = 0; i < numKBs; ++i )
		{
			BOOL first = !IsIDInList( LOWORD( list[ i ] ), i, list );

			if ( !first )
				continue;

			selections.AddToTail( list[ i ] );
			if ( list[ i ] == currentKb )
			{
				oldKb = selections.Count() - 1;
			}
		}

		oldKb += forward ? 1 : -1;
		if ( oldKb < 0 )
		{
			oldKb = max( 0, selections.Count() - 1 );
		}
		else if ( oldKb >= selections.Count() )
		{
			oldKb = 0;
		}

		ActivateKeyboardLayout( selections[ oldKb ], 0 );

		int langid = LOWORD( selections[ oldKb ] );
		SpewIMEInfo( langid );

		delete[] list;
	}
#endif
}

int CInputSystem::GetCurrentIMEHandle()
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	HKL hkl = (HKL)GetKeyboardLayout( 0 );
	return (int)hkl;
#else
	return 0;
#endif
}

int CInputSystem::GetEnglishIMEHandle()
{
#ifdef DO_IME
	HKL hkl = (HKL)0x04090409;
	return (int)hkl;
#else
	return 0;
#endif
}

void CInputSystem::OnChangeIMEByHandle( int handleValue )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	HKL hkl = (HKL)handleValue;

	ActivateKeyboardLayout( hkl, 0 );

	int langid = LOWORD( hkl);

	SpewIMEInfo( langid );
#endif
}

	// Returns the Language Bar label (Chinese, Korean, Japanese, Russion, Thai, etc.)
void CInputSystem::GetIMELanguageName( wchar_t *buf, int unicodeBufferSizeInBytes )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	wchar_t const *name = GetLanguageName( LOWORD( GetKeyboardLayout( 0 ) ) );
	wcsncpy( buf, name, unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1 );
	buf[ unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1 ] = L'\0';
#else
	buf[0] = L'\0';
#endif
}
	// Returns the short code for the language (EN, CH, KO, JP, RU, TH, etc. ).
void CInputSystem::GetIMELanguageShortCode( wchar_t *buf, int unicodeBufferSizeInBytes )
{
#ifdef DO_IME
	LanguageIds *info = GetLanguageInfo( LOWORD( GetKeyboardLayout( 0 ) ) );
	if ( !info )
	{
		buf[ 0 ] = L'\0';
	}
	else
	{
		wcsncpy( buf, info->shortcode, unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1 );
		buf[ unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1 ] = L'\0';
	}
#else
	buf[0] = L'\0';
#endif
}

// Call with NULL dest to get item count
int CInputSystem::GetIMELanguageList( LanguageItem *dest, int destcount )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	int iret = 0;

	UINT numKBs = GetKeyboardLayoutList( 0, NULL );
	if ( numKBs > 0 )
	{
		HKL *list = new HKL[ numKBs ];

		GetKeyboardLayoutList( numKBs, list );

		CUtlVector< HKL >	selections;

		for ( unsigned int i = 0; i < numKBs; ++i )
		{
			BOOL first = !IsIDInList( LOWORD( list[ i ] ), i, list );

			if ( !first )
				continue;

			selections.AddToTail( list[ i ] );
		}

		iret = selections.Count();
		if ( dest )
		{
			for ( int i = 0; i < min(iret,destcount); ++i )
			{
				HKL hkl = selections[ i ];

				IInput::LanguageItem *p = &dest[ i ];

				LanguageIds *info = GetLanguageInfo( LOWORD( hkl ) );

				memset( p, 0, sizeof( IInput::LanguageItem ) );

				wcsncpy( p->shortname, info->shortcode, sizeof( p->shortname ) / sizeof( wchar_t ) );
				p->shortname[ sizeof( p->shortname ) / sizeof( wchar_t ) - 1 ] = L'\0';

				wcsncpy( p->menuname, info->displayname, sizeof( p->menuname ) / sizeof( wchar_t ) );
				p->menuname[ sizeof( p->menuname ) / sizeof( wchar_t ) - 1 ] = L'\0';

				p->handleValue = (int)hkl;
				p->active = ( hkl == GetKeyboardLayout( 0 ) ) ? true : false;
			}
		}

		delete[] list;
	}
	return iret;
#else
	return 0;
#endif
}

/*
// Flag for effective options in conversion mode 
BOOL fConvMode[NUM_IMES_SUPPORTED][13] = 
{	
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	// EN
	{1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0},	// Trad CH
	{1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},	// Japanese
	{1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},	// Kor
	{1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0},	// Simp CH
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	// UNK(same as EN)
}

// Flag for effective options in sentence mode 
BOOL fSentMode[NUM_IMES_SUPPORTED][6] = 
{	
	{0, 0, 0, 0, 0, 0},		// EN
	{0, 1, 0, 0, 0, 0},		// Trad CH
	{1, 1, 1, 1, 1, 1},		// Japanese
	{0, 0, 0, 0, 0, 0},		// Kor
	{0, 0, 0, 0, 0, 0}		// Simp CH
	{0, 0, 0, 0, 0, 0},		// UNK(same as EN)
};

// Conversion mode message 
DWORD dwConvModeMsg[13] = {
	IME_CMODE_ALPHANUMERIC,		IME_CMODE_NATIVE,		IME_CMODE_KATAKANA, 
	IME_CMODE_LANGUAGE,			IME_CMODE_FULLSHAPE,	IME_CMODE_ROMAN, 
	IME_CMODE_CHARCODE,			IME_CMODE_HANJACONVERT, IME_CMODE_SOFTKBD, 
	IME_CMODE_NOCONVERSION,		IME_CMODE_EUDC,			IME_CMODE_SYMBOL, 
	IME_CMODE_FIXED};

// Sentence mode message 
DWORD dwSentModeMsg[6] = {
	IME_SMODE_NONE,			IME_SMODE_PLAURALCLAUSE,	IME_SMODE_SINGLECONVERT,	
	IME_SMODE_AUTOMATIC,	IME_SMODE_PHRASEPREDICT,	IME_SMODE_CONVERSATION };

//	ENGLISH,				
//	TRADITIONAL_CHINESE,	
//	JAPANESE,
//	KOREAN,
//	SIMPLIFIED_CHINESE,
//	UNKNOWN,
*/

#ifdef DO_IME

struct IMESettingsTransform
{
	IMESettingsTransform( unsigned int cmr, unsigned int cma, unsigned int smr, unsigned int sma ) :
		cmode_remove( cmr ),
		cmode_add( cma ),
		smode_remove( smr ),
		smode_add( sma )
	{
	}

	void Apply( HWND hwnd )
	{
		HIMC hImc = ImmGetContext( hwnd );
		if ( hImc )
		{
			DWORD	dwConvMode, dwSentMode;

			ImmGetConversionStatus( hImc, &dwConvMode, &dwSentMode );

			dwConvMode &= ~cmode_remove;
			dwSentMode &= ~smode_remove;

			ImmSetConversionStatus( hImc, dwConvMode, dwSentMode );

			dwConvMode |= cmode_add;
			dwSentMode |= smode_add;

			ImmSetConversionStatus( hImc, dwConvMode, dwSentMode );

			ImmReleaseContext( hwnd, hImc );
		}
	}

	bool ConvMatches( DWORD convFlags )
	{
		// To match, the active flags have to have none of the remove flags and have to have all of the "add" flags
		if ( convFlags & cmode_remove )
			return false;

		if ( ( convFlags & cmode_add ) == cmode_add )
		{
			return true;
		}
		return false;
	}

	bool SentMatches( DWORD sentFlags )
	{
		// To match, the active flags have to have none of the remove flags and have to have all of the "add" flags
		if ( sentFlags & smode_remove )
			return false;

		if ( ( sentFlags & smode_add ) == smode_add )
		{
			return true;
		}
		return false;
	}

	unsigned int		cmode_remove;
	unsigned int		cmode_add;
	unsigned int		smode_remove;
	unsigned int		smode_add;
};

static IMESettingsTransform g_ConversionMode_CHT_ToChinese( 
	IME_CMODE_ALPHANUMERIC,
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	0,
	0 );
static IMESettingsTransform g_ConversionMode_CHT_ToEnglish( 
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	IME_CMODE_ALPHANUMERIC,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_CHS_ToChinese( 
	IME_CMODE_ALPHANUMERIC,
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	0,
	0 );
static IMESettingsTransform g_ConversionMode_CHS_ToEnglish( 
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	IME_CMODE_ALPHANUMERIC,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_KO_ToKorean( 
	IME_CMODE_ALPHANUMERIC,
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_KO_ToEnglish( 
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	IME_CMODE_ALPHANUMERIC,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_Hiragana( 
	IME_CMODE_ALPHANUMERIC | IME_CMODE_KATAKANA,
	IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_DirectInput( 
	IME_CMODE_NATIVE | ( IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE ) | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
	IME_CMODE_ALPHANUMERIC,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_FullwidthKatakana( 
	IME_CMODE_ALPHANUMERIC,
	IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN | IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_HalfwidthKatakana( 
	IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE,
	IME_CMODE_NATIVE | IME_CMODE_ROMAN | ( IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE ),
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_FullwidthAlphanumeric( 
	IME_CMODE_NATIVE | ( IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE ),
	IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_HalfwidthAlphanumeric( 
	IME_CMODE_NATIVE | ( IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE ) | IME_CMODE_FULLSHAPE,
	IME_CMODE_ALPHANUMERIC | IME_CMODE_ROMAN,
	0,
	0 );

#endif // DO_IME

int CInputSystem::GetIMEConversionModes( ConversionModeItem *dest, int destcount )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	if ( dest )
	{
		memset( dest, 0, destcount * sizeof( ConversionModeItem ) );
	}

	DWORD	dwConvMode = 0, dwSentMode = 0;

	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		ImmGetConversionStatus( hImc, &dwConvMode, &dwSentMode );
		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}

	LanguageIds *info = GetLanguageInfo( LOWORD( GetKeyboardLayout( 0 ) ) );
	switch ( info->languageflag )
	{
	default:
		return 0;
	case TRADITIONAL_CHINESE:
		// This is either native or alphanumeric
		if ( dest )
		{
			ConversionModeItem *item;
			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_Chinese", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_CHT_ToChinese;
			item->active = g_ConversionMode_CHT_ToChinese.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_English", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_CHT_ToEnglish;
			item->active = g_ConversionMode_CHT_ToEnglish.ConvMatches( dwConvMode );
		}
		return 2;
	case JAPANESE:
		// There are 6 Japanese modes
		if ( dest )
		{
			ConversionModeItem *item;
			
			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_Hiragana", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_Hiragana;
			item->active = g_ConversionMode_JP_Hiragana.ConvMatches( dwConvMode );
			
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_FullWidthKatakana", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_FullwidthKatakana;
			item->active = g_ConversionMode_JP_FullwidthKatakana.ConvMatches( dwConvMode );
			
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_FullWidthAlphanumeric", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_FullwidthAlphanumeric;
			item->active = g_ConversionMode_JP_FullwidthAlphanumeric.ConvMatches( dwConvMode );
			
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_HalfWidthKatakana", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_HalfwidthKatakana;
			item->active = g_ConversionMode_JP_HalfwidthKatakana.ConvMatches( dwConvMode );
			
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_HalfWidthAlphanumeric", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_HalfwidthAlphanumeric;
			item->active = g_ConversionMode_JP_HalfwidthAlphanumeric.ConvMatches( dwConvMode );
			
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_English", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_DirectInput;
			item->active = g_ConversionMode_JP_DirectInput.ConvMatches( dwConvMode );
			
		}
		return 6;
	case KOREAN:
		// This is either native or alphanumeric
		if ( dest )
		{
			ConversionModeItem *item;
			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_Korean", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_KO_ToKorean;
			item->active = g_ConversionMode_KO_ToKorean.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_English", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_KO_ToEnglish;
			item->active = g_ConversionMode_KO_ToEnglish.ConvMatches( dwConvMode );
		}
		return 2;
	case SIMPLIFIED_CHINESE:
		// This is either native or alphanumeric
		if ( dest )
		{
			ConversionModeItem *item;
			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_Chinese", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_CHS_ToChinese;
			item->active = g_ConversionMode_CHS_ToChinese.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_English", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_CHS_ToChinese;
			item->active = g_ConversionMode_CHS_ToChinese.ConvMatches( dwConvMode );
		}
		return 2;
	}
#endif

	return 0;
}

#ifdef DO_IME

static IMESettingsTransform g_SentenceMode_JP_None( 
	0,
	0,
	IME_SMODE_PLAURALCLAUSE | IME_SMODE_SINGLECONVERT | IME_SMODE_AUTOMATIC | IME_SMODE_PHRASEPREDICT | IME_SMODE_CONVERSATION,
	IME_SMODE_NONE );

static IMESettingsTransform g_SentenceMode_JP_General( 
	0,
	0,
	IME_SMODE_NONE | IME_SMODE_PLAURALCLAUSE | IME_SMODE_SINGLECONVERT | IME_SMODE_AUTOMATIC | IME_SMODE_CONVERSATION,
	IME_SMODE_PHRASEPREDICT
	 );

static IMESettingsTransform g_SentenceMode_JP_BiasNames( 
	0,
	0,
	IME_SMODE_NONE | IME_SMODE_PHRASEPREDICT | IME_SMODE_SINGLECONVERT | IME_SMODE_AUTOMATIC | IME_SMODE_CONVERSATION,
	IME_SMODE_PLAURALCLAUSE
	);

static IMESettingsTransform g_SentenceMode_JP_BiasSpeech( 
	0,
	0,
	IME_SMODE_NONE | IME_SMODE_PHRASEPREDICT | IME_SMODE_SINGLECONVERT | IME_SMODE_AUTOMATIC | IME_SMODE_PLAURALCLAUSE,
	IME_SMODE_CONVERSATION
	);

#endif // _X360

int CInputSystem::GetIMESentenceModes( SentenceModeItem *dest, int destcount )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	if ( dest )
	{
		memset( dest, 0, destcount * sizeof( SentenceModeItem ) );
	}

	DWORD	dwConvMode = 0, dwSentMode = 0;

	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		ImmGetConversionStatus( hImc, &dwConvMode, &dwSentMode );
		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}

	LanguageIds *info = GetLanguageInfo( LOWORD( GetKeyboardLayout( 0 ) ) );
	switch ( info->languageflag )
	{
	default:
		return 0;
//	case TRADITIONAL_CHINESE:
//		break;
	case JAPANESE:
		// There are 4 Japanese sentence modes
		if ( dest )
		{
			SentenceModeItem *item;
			
			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_General", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_SentenceMode_JP_General;
			item->active = g_SentenceMode_JP_General.SentMatches( dwSentMode );
			
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_BiasNames", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_SentenceMode_JP_BiasNames;
			item->active = g_SentenceMode_JP_BiasNames.SentMatches( dwSentMode );
			
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_BiasSpeech", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_SentenceMode_JP_BiasSpeech;
			item->active = g_SentenceMode_JP_BiasSpeech.SentMatches( dwSentMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_NoConversion", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_SentenceMode_JP_None;
			item->active = g_SentenceMode_JP_None.SentMatches( dwSentMode );
		}
		return 4;
	}
#endif

	return 0;
}

void CInputSystem::OnChangeIMEConversionModeByHandle( int handleValue )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	if ( handleValue == 0 )
		return;

	IMESettingsTransform *txform = ( IMESettingsTransform * )handleValue;
	txform->Apply( (HWND)GetIMEWindow() );
#endif
}

void CInputSystem::OnChangeIMESentenceModeByHandle( int handleValue )
{
}

void CInputSystem::OnInputLanguageChanged()
{
}

void CInputSystem::OnIMEStartComposition()
{
}

#ifdef DO_IME
void DescribeIMEFlag( char const *string, bool value )
{
	if ( value )
	{
		Msg( "   %s\n", string );
	}
}

#define IMEDesc( x )	DescribeIMEFlag( #x, flags & x );
#endif // DO_IME

void CInputSystem::OnIMEComposition( int flags )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	/*
	Msg( "OnIMEComposition\n" );

	IMEDesc( VGUI_GCS_COMPREADSTR );
	IMEDesc( VGUI_GCS_COMPREADATTR );
	IMEDesc( VGUI_GCS_COMPREADCLAUSE );
	IMEDesc( VGUI_GCS_COMPSTR );
	IMEDesc( VGUI_GCS_COMPATTR );
	IMEDesc( VGUI_GCS_COMPCLAUSE );
	IMEDesc( VGUI_GCS_CURSORPOS );
	IMEDesc( VGUI_GCS_DELTASTART );
	IMEDesc( VGUI_GCS_RESULTREADSTR );
	IMEDesc( VGUI_GCS_RESULTREADCLAUSE );
	IMEDesc( VGUI_GCS_RESULTSTR );
	IMEDesc( VGUI_GCS_RESULTCLAUSE );
	IMEDesc( VGUI_CS_INSERTCHAR );
	IMEDesc( VGUI_CS_NOMOVECARET );
	*/

	HIMC hIMC = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hIMC )
	{
		if ( flags & VGUI_GCS_RESULTSTR )
		{
			wchar_t tempstr[ 32 ];

			int len = ImmGetCompositionStringW( hIMC, GCS_RESULTSTR, (LPVOID)tempstr, sizeof( tempstr ) );
			if ( len > 0 )
			{
				if ((len % 2) != 0)
					len++;
				int numchars = len / sizeof( wchar_t );

				for ( int i = 0; i < numchars; ++i )
				{
					InternalKeyTyped( tempstr[ i ] );
				}
			}
		}
		if ( flags & VGUI_GCS_COMPSTR )
		{
			wchar_t tempstr[ 256 ];

			int len = ImmGetCompositionStringW( hIMC, GCS_COMPSTR, (LPVOID)tempstr, sizeof( tempstr ) );
			if ( len > 0 )
			{
				if ((len % 2) != 0)
					len++;
				int numchars = len / sizeof( wchar_t );
				tempstr[ numchars ] = L'\0';

				InternalSetCompositionString( tempstr );
			}
		}

		ImmReleaseContext( ( HWND )GetIMEWindow(), hIMC );
	}
#endif
}

void CInputSystem::OnIMEEndComposition()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		// tell the current focused panel that a key was typed
		PostKeyMessage( new KeyValues( "DoCompositionString", "string", L"" ) );
	}
}

void CInputSystem::DestroyCandidateList()
{
#ifdef DO_IME
	if ( _imeCandidates )
	{
		delete[] (char *)_imeCandidates;
		_imeCandidates = null;
	}
#endif
}

void CInputSystem::OnIMEShowCandidates() 
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	DestroyCandidateList();
	CreateNewCandidateList();

	InternalShowCandidateWindow();
#endif
}

void CInputSystem::OnIMECloseCandidates() 
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	InternalHideCandidateWindow();
	DestroyCandidateList();
#endif
}

void CInputSystem::OnIMEChangeCandidates() 
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	DestroyCandidateList();
	CreateNewCandidateList();

	InternalUpdateCandidateWindow();
#endif
}

void CInputSystem::CreateNewCandidateList()
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	Assert( !_imeCandidates );

	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		DWORD numCandidates = 0;

		DWORD bytes = ImmGetCandidateListCountW( hImc, &numCandidates );
		if ( numCandidates > 0 )
		{
			DWORD buflen = bytes + 1;

			char *buf = new char[ buflen ];
			Q_memset( buf, 0, buflen );

			CANDIDATELIST *list = ( CANDIDATELIST *)buf;
			DWORD copyBytes = ImmGetCandidateListW( hImc, 0, list, buflen );
			if ( copyBytes > 0 )
			{
				_imeCandidates = list;
			}
			else
			{
				delete[] buf;
			}
		}
		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}
#endif
}

int  CInputSystem::GetCandidateListCount()
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	if ( !_imeCandidates )
		return 0;

	return (int)_imeCandidates->dwCount;
#else
	return 0;
#endif
}

void CInputSystem::GetCandidate( int num, wchar_t *dest, int destSizeBytes )
{
	ASSERT_IF_IME_NYI();

	dest[ 0 ] = L'\0';
#ifdef DO_IME
	if ( num < 0 || num >= (int)_imeCandidates->dwCount )
	{
		return;
	}

	DWORD offset = *( DWORD *)( (char *)( _imeCandidates->dwOffset + num ) );
	wchar_t *s = ( wchar_t *)( (char *)_imeCandidates + offset );

	wcsncpy( dest, s, destSizeBytes / sizeof( wchar_t ) - 1 );
	dest[ destSizeBytes / sizeof( wchar_t ) - 1 ] = L'\0';
#endif
}

int CInputSystem::GetCandidateListSelectedItem()
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	if ( !_imeCandidates )
		return 0;

	return (int)_imeCandidates->dwSelection;
#else
	return 0;
#endif
}

int  CInputSystem::GetCandidateListPageSize()
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	if ( !_imeCandidates )
		return 0;
	return (int)_imeCandidates->dwPageSize;
#else
	return 0;
#endif
}

int CInputSystem::GetCandidateListPageStart()
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	if ( !_imeCandidates )
		return 0;
	return (int)_imeCandidates->dwPageStart;
#else
	return 0;
#endif
}

void CInputSystem::SetCandidateListPageStart( int start )
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		ImmNotifyIME( hImc, NI_SETCANDIDATE_PAGESTART, 0, start );
		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}
#endif
}

void CInputSystem::OnIMERecomputeModes()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CInputSystem::CandidateListStartsAtOne()
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
	DWORD prop = ImmGetProperty( GetKeyboardLayout( 0 ), IGP_PROPERTY );
	if ( prop &	IME_PROP_CANDLIST_START_FROM_1 )
	{
		return true;
	}
#endif
	return false;
}

void CInputSystem::SetCandidateWindowPos( int x, int y ) 
{
	ASSERT_IF_IME_NYI();

#ifdef DO_IME
    POINT		point;
    CANDIDATEFORM Candidate;

	point.x = x;
	point.y = y;

	HIMC hIMC = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hIMC ) 
	{
		// Set candidate window position near caret position
		Candidate.dwIndex = 0;
		Candidate.dwStyle = CFS_FORCE_POSITION;
		Candidate.ptCurrentPos.x = point.x;
		Candidate.ptCurrentPos.y = point.y;
		ImmSetCandidateWindow( hIMC, &Candidate );

		ImmReleaseContext( ( HWND )GetIMEWindow(),hIMC );
	}
#endif
}

void CInputSystem::InternalSetCompositionString( const wchar_t *compstr )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		// tell the current focused panel that a key was typed
		PostKeyMessage( new KeyValues( "DoCompositionString", "string", compstr ) );
	}
}

void CInputSystem::InternalShowCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoShowIMECandidates" ) );
	}
}

void CInputSystem::InternalHideCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoHideIMECandidates" ) );
	}
}

void CInputSystem::InternalUpdateCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoUpdateIMECandidates" ) );
	}
}

bool CInputSystem::GetShouldInvertCompositionString()
{
#ifdef DO_IME
	LanguageIds *info = GetLanguageInfo( LOWORD( GetKeyboardLayout( 0 ) ) );
	if ( !info )
		return false;

	// Only Chinese (simplified and traditional)
	return info->invertcomposition;
#else
	return false;
#endif
}

void CInputSystem::RegisterKeyCodeUnhandledListener( VPANEL panel )
{
	if ( !panel )
		return;

	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	VPanel *listener = (VPanel *)panel;

	if ( pContext->m_KeyCodeUnhandledListeners.Find( listener ) == pContext->m_KeyCodeUnhandledListeners.InvalidIndex() )
	{
		pContext->m_KeyCodeUnhandledListeners.AddToTail( listener );
	}
}

void CInputSystem::UnregisterKeyCodeUnhandledListener( VPANEL panel )
{
	if ( !panel )
		return;

	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	VPanel *listener = (VPanel *)panel;

	pContext->m_KeyCodeUnhandledListeners.FindAndRemove( listener );
}


// Posts unhandled message to all interested panels
void CInputSystem::OnKeyCodeUnhandled( int keyCode )
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	int c = pContext->m_KeyCodeUnhandledListeners.Count();
	for ( int i = 0; i < c; ++i )
	{
		VPanel *listener = pContext->m_KeyCodeUnhandledListeners[ i ];
		g_pIVgui->PostMessage((VPANEL)listener, new KeyValues( "KeyCodeUnhandled", "code", keyCode ), NULL );
	}
}

void CInputSystem::PostModalSubTreeMessage( VPanel *subTree, bool state )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if( pContext->m_pModalSubTree == NULL )
		return;

	//tell the current focused panel that a key was released
	KeyValues *kv = new KeyValues( "ModalSubTree", "state", state ? 1 : 0 );
	g_pIVgui->PostMessage( (VPANEL)pContext->m_pModalSubTree, kv, NULL );
}

// Assumes subTree is a child panel of the root panel for the vgui contect
//  if restrictMessagesToSubTree is true, then mouse and kb messages are only routed to the subTree and it's children and mouse/kb focus
//   can only be on one of the subTree children, if a mouse click occurs outside of the subtree, and "UnhandledMouseClick" message is sent to unhandledMouseClickListener panel
//   if it's set
//  if restrictMessagesToSubTree is false, then mouse and kb messages are routed as normal except that they are not routed down into the subtree
//   however, if a mouse click occurs outside of the subtree, and "UnhandleMouseClick" message is sent to unhandledMouseClickListener panel
//   if it's set
void CInputSystem::SetModalSubTree( VPANEL subTree, VPANEL unhandledMouseClickListener, bool restrictMessagesToSubTree /*= true*/ )
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	if ( pContext->m_pModalSubTree && 
		pContext->m_pModalSubTree != (VPanel *)subTree )
	{
		ReleaseModalSubTree();
	}

	if ( !subTree )
		return;

	pContext->m_pModalSubTree = (VPanel *)subTree;
	pContext->m_pUnhandledMouseClickListener = (VPanel *)unhandledMouseClickListener;
	pContext->m_bRestrictMessagesToModalSubTree = restrictMessagesToSubTree;

	PostModalSubTreeMessage( pContext->m_pModalSubTree, true );
}

void CInputSystem::ReleaseModalSubTree()
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	if ( pContext->m_pModalSubTree )
	{
		PostModalSubTreeMessage( pContext->m_pModalSubTree, false );
	}

	pContext->m_pModalSubTree = NULL;
	pContext->m_pUnhandledMouseClickListener = NULL;
	pContext->m_bRestrictMessagesToModalSubTree = false;

}

VPANEL CInputSystem::GetModalSubTree()
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return 0;

	return (VPANEL)pContext->m_pModalSubTree;
}

// These toggle whether the modal subtree is exclusively receiving messages or conversely whether it's being excluded from receiving messages
void CInputSystem::SetModalSubTreeReceiveMessages( bool state )
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	Assert( pContext->m_pModalSubTree );
	if ( !pContext->m_pModalSubTree )
		return;

	pContext->m_bRestrictMessagesToModalSubTree = state;
	
}

bool CInputSystem::ShouldModalSubTreeReceiveMessages() const
{
	InputContext_t *pContext = const_cast< CInputSystem * >( this )->GetInputContext(m_hContext);
	if ( !pContext )
		return true;

	return pContext->m_bRestrictMessagesToModalSubTree;
}
