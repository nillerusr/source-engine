#ifndef _INCLUDED_ASW_VGUI_INFO_MESSAGE
#define _INCLUDED_ASW_VGUI_INFO_MESSAGE

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"
#include "vgui/IScheme.h"
#include "asw_vgui_ingame_panel.h"

class C_ASW_Info_Message;
class ImageButton;
namespace vgui
{
	class Label;
	class Button;
	class ImagePanel;
};

// pop up window shown ingame for simple messages (e.g. tutorial messages)

class CASW_VGUI_Info_Message : public vgui::Panel, public CASW_VGUI_Ingame_Panel
{	
public:
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Info_Message, vgui::Panel );

	CASW_VGUI_Info_Message( vgui::Panel *pParent, const char *pElementName, C_ASW_Info_Message* pMessage );
	virtual ~CASW_VGUI_Info_Message();
	
	virtual void GetMessagePanelSize(int &w, int &t);
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(char const* command);
	virtual void UpdateMessage();
	virtual void CloseMessage();
	void NotifyServerOfClose();
	virtual bool ShouldAddToLog() { return true; }
	virtual bool ShouldAddLogButton();
	virtual bool HasMessageImage();

	// current message
	CHandle<C_ASW_Info_Message> m_hMessage;

	vgui::Label* m_pLine[4];
	ImageButton* m_pOkayButton;
	ImageButton* m_pLogButton;
	vgui::ImagePanel* m_pMessageImage;
	char m_szImageName[255];

	// overall scale of this window
	float m_fScale;
	
	bool m_bClosingMessage;

	static bool CloseInfoMessage();
	static bool HasInfoMessageOpen();
};

#define ASW_MAX_LOGGED_MESSAGES 24

// allows access to previous info messages
class CASW_VGUI_Message_Log : public CASW_VGUI_Info_Message
{	
public:
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Message_Log, CASW_VGUI_Info_Message );

	CASW_VGUI_Message_Log( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_VGUI_Message_Log();
	
	virtual void ShowLoggedMessage(int i);
	virtual void GetMessagePanelSize(int &w, int &t);
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible	
	virtual bool MouseClick(int x, int y, bool bRightClick, bool bDown);
	virtual void OnCommand(char const* command);
	virtual void UpdateMessage();	
	void NotifyServerOfClose() { }
	virtual bool ShouldAddToLog() { return false; }	
	virtual bool ShouldAddLogButton();
	
	ImageButton* m_pMessageButton[ASW_MAX_LOGGED_MESSAGES];
	ImageButton* m_pOkayButton;	
};

#endif /* _INCLUDED_ASW_VGUI_INFO_MESSAGE */