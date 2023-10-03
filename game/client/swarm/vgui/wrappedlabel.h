#ifndef WRAPPEDLABEL_H
#define WRAPPEDLABEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Label.h>

// a vgui label that defaults to wrapped text

namespace vgui
{

class WrappedLabel : public Label
{
	DECLARE_CLASS_SIMPLE( WrappedLabel, Label );
public:
	WrappedLabel(Panel *parent, const char *panelName, const char *text);
	WrappedLabel(Panel *parent, const char *panelName, const wchar_t *wszText);
	virtual ~WrappedLabel();
};

} // namespace vgui

#endif // WRAPPEDLABEL_H