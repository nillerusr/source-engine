

#include "cbase.h"
#include "WrappedLabel.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

WrappedLabel::WrappedLabel(Panel *parent, const char *panelName, const char *text) :
	Label(parent, panelName, text)
{
	KeyValues *kv = new KeyValues( "Dummy", "wrap", 1);
	Label::ApplySettings(kv);
	kv->deleteThis();
}

WrappedLabel::WrappedLabel(Panel *parent, const char *panelName, const wchar_t *wszText) :
	Label(parent, panelName, wszText)
{
	KeyValues *kv = new KeyValues( "Dummy", "wrap", 1);
	Label::ApplySettings(kv);
	kv->deleteThis();
}


WrappedLabel::~WrappedLabel()
{

}
