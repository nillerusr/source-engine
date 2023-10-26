#ifndef _INCLUDED_CONTROLLER_FOCUS_H
#define _INCLUDED_CONTROLLER_FOCUS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/PHandle.h>
#include "asw_input.h"

// This class provides a way of using VGUI panels with a controller.  Instead of specific controller responses being coded
//  into every control and panel, this class allows the controller to manipulate any set of panels that respond to
//  IsCursorOver and OnMousePressed/Released events.  Panels that want to allow controller manipulation simply have to
//  register themselves with this on creation and remove themselves from this on destruction.

// holds a list of vgui panels that can have controller cursor focus
//  along with the currently focused one

// responds to controller codes to move this focus about and cause mouseclicks when a confirm button is pressed

#define JF_KEY_REPEAT_DELAY 400
#define JF_KEY_REPEAT_INTERVAL 150

namespace vgui
{
	class ImagePanelColored;
};

class CControllerOutline;

class CControllerFocus
{
public:
	enum ControllerFocusKey
	{
		JF_KEY_UP = 0,
		JF_KEY_DOWN,
		JF_KEY_LEFT,
		JF_KEY_RIGHT,
		JF_KEY_CONFIRM,
		JF_KEY_CANCEL,

		// "alternate" up/down buttons - eg, dpad on 360. nothing on pc.
		JF_KEY_UP_ALT,
		JF_KEY_DOWN_ALT,
		JF_KEY_LEFT_ALT,
		JF_KEY_RIGHT_ALT,


		NUM_JF_KEYS
	};
	struct FocusArea
	{		
		vgui::PHandle hPanel;
		bool bClickOnFocus;
		bool bModal;
	};
	CControllerFocus();

	// registering for focus
	void AddToFocusList(vgui::Panel* pPanel, bool bClickOnFocus=false, bool bModal=false);
	void RemoveFromFocusList(vgui::Panel* pPanel);

	// changing focus
	void SetFocusPanel(int index);
	void SetFocusPanel(vgui::Panel* pPanel, bool bClickOnFocus=false);
	vgui::Panel* GetFocusPanel();
	int FindNextPanel(vgui::Panel *pSource, float angle);	

	// clicking
	bool OnControllerButtonPressed(ButtonCode_t keynum);	
	bool OnControllerButtonReleased(ButtonCode_t keynum);
	void CheckKeyRepeats();
	void ClickFocusPanel(bool bDown, bool bRightMouse);
	void DoubleClickFocusPanel(bool bRightMouse);
	
	void SetControllerCodes(ButtonCode_t iUpCode, ButtonCode_t iDownCode, ButtonCode_t iLeftCode, ButtonCode_t iRightCode, ButtonCode_t iConfirmCode, ButtonCode_t iCancelCode);	
	void SetControllerMode(bool b) { m_bControllerMode = b; }
	bool IsControllerMode() { return m_bControllerMode; }

	// checks a panel and all its parents are visible
	static bool IsPanelReallyVisible(vgui::Panel *pPanel);

	void PrintFocusListToConsole();
	
	// KF_ numbers for the controller buttons
	ButtonCode_t m_KeyNum[NUM_JF_KEYS];

	// status of the controller buttons
	bool m_bKeyDown[NUM_JF_KEYS];
	float m_fNextKeyRepeatTime[NUM_JF_KEYS];

	// list of panels that have registered themselves for controller focus
	CUtlVector<FocusArea> m_FocusAreas;
	FocusArea m_CurrentFocus;
	vgui::DHANDLE<CControllerOutline> m_hOutline;

	int m_iModal;	// how many modal-type focus panels we have.  If there are more than 1, all non-modal panels will be ignored when moving around
	bool m_bControllerMode;
	bool m_bDebugOutput;
};

CControllerFocus* GetControllerFocus();

// graphical representaion of controller cursor focus - attaches to the focus' parent and puts itself
//  in front of the focus, sized to match
class CControllerOutline : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CControllerOutline, vgui::Panel );
public:
	CControllerOutline(vgui::Panel *parent, const char *name);
	void ApplySchemeSettings(vgui::IScheme* pScheme);
	virtual void OnThink();
	virtual void Paint();
	void SizeTo(int x, int y, int w, int t);
	virtual void GetCornerTextureSize( int& w, int& h );
	vgui::ImagePanelColored* m_pImagePanel;
	vgui::PHandle m_hLastFocusPanel;
};

#endif // _INCLUDED_CONTROLLER_FOCUS_H