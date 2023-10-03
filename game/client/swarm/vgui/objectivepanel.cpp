#include "cbase.h"
#include "ObjectivePanel.h"

#include "vgui_controls/ImagePanel.h"
#include "WrappedLabel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

ObjectivePanel::ObjectivePanel(Panel *parent, const char *name) : Panel(parent, name)
{	
	// create a blank imagepanel for the objective pic
	m_ObjectiveImagePanel = new vgui::ImagePanel(this, "ObjectivePanelImagePanel");
	m_ObjectiveImagePanel->SetShouldScaleImage(true);

	// create the blank objective text - note, label isn't actually a child of this class!
	wchar_t *text = L"<objective>";
	m_ObjectiveLabel = new vgui::WrappedLabel(this, "ObjectivePanelLabel", text);
	m_ObjectiveLabel->SetContentAlignment(vgui::Label::a_northwest);
	
	m_bImageSet = false;
	m_bTextSet = false;
}
	
ObjectivePanel::~ObjectivePanel()
{

}

// a single panel showing objectives, should take up the width of the screen, and some portion of the height
void ObjectivePanel::PerformLayout()
{
	int width = ScreenWidth();
	int height = ScreenHeight();

	SetSize(width * 0.9, height * 0.5);
	m_ObjectiveLabel->SetSize(width * 0.6, height * 0.25);		// fixme: height should vary depending on text
	m_ObjectiveLabel->SetPos(width * 0.27, 0);
	//int w = 0;
	//int h = 0;
	//m_ObjectiveLabel->GetContentSize(w, h);
	//m_ObjectiveLabel->SetSize(width * 0.6, h);		// fixme: height should vary depending on text
	//m_ObjectiveLabel->SetPos(width * 0.2, 0);
	//m_ObjectiveImagePanel->SetSize(width * 0.25, height * 0.25);
	m_ObjectiveImagePanel->SetSize(width * 0.2, height * 0.2);
	m_ObjectiveImagePanel->SetPos(0, 0);	
}