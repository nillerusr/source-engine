#ifndef __VDROPDOWNMENU_H__
#define __VDROPDOWNMENU_H__

#include "basemodui.h"

namespace BaseModUI
{

class BaseModHybridButton;
class FlyoutMenu;

class DropDownMenu : public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( DropDownMenu, vgui::EditablePanel );

	DropDownMenu( vgui::Panel* parent, const char* panelName );
	virtual ~DropDownMenu();

	virtual void OnCommand( const char* command );

	virtual void NavigateTo();
	virtual void NavigateFrom();
	virtual void NavigateToChild( Panel *pNavigateTo ); //mouse support

	void SetFlyout( const char* flyoutName );
	void SetEnabled( bool state );
	void SetFlyoutItemEnabled( const char* selection, bool state );
	void SetSelectedTextEnabled( bool state );

	FlyoutMenu* GetCurrentFlyout();
	void SetCurrentSelection( const char* selection );

	enum SelectionChange_t
	{
		SELECT_NEXT,
		SELECT_PREV
	};
	void ChangeSelection( SelectionChange_t eNext );
	const char* GetCurrentSelection();

	void CloseDropDown();

	typedef void (*Callback_t)( DropDownMenu *pDropDownMenu, FlyoutMenu *pFlyoutMenu );
	void SetOpenCallback( Callback_t callBack );

	BaseModHybridButton	*GetButton() { return m_pButton; }

protected:
	virtual void ApplySettings( KeyValues* inResourceData );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnMouseWheeled(int delta);

	BaseModHybridButton	*m_pButton;
	vgui::Panel			*m_pnlBackground;
	vgui::DHANDLE<FlyoutMenu>			m_hCurrentFlyout;

	char m_curSelText[ MAX_PATH ];
	bool m_SelectedTextEnabled;

	Callback_t			m_openCallback;
};

};

#endif
