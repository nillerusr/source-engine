#include "vgui_controls/ImagePanel.h"
#include "VFlyoutMenu.h"

using namespace BaseModUI;
using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Same as an image panel, but closes flyouts on click (most of the menu backgrounds are image panels)
//-----------------------------------------------------------------------------
class BaseModMenuBackground : public ImagePanel
{
	DECLARE_CLASS_SIMPLE( BaseModMenuBackground, ImagePanel );
public:
	BaseModMenuBackground(Panel *parent, const char *name) : BaseClass( parent, name ) {}
	virtual void OnMousePressed( vgui::MouseCode code )
	{
		if( FlyoutMenu::GetActiveMenu() )
			FlyoutMenu::CloseActiveMenu( FlyoutMenu::GetActiveMenu()->GetNavFrom() );

		BaseClass::OnMousePressed( code );
	}
};
DECLARE_BUILD_FACTORY( BaseModMenuBackground );
