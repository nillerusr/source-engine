#include <stdio.h>

#include <vgui/IPanel.h>
#include <vgui/IClientPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/Cursor.h>

#include "../vgui2/src/vgui_internal.h"
#include "../vgui2/src/VPanel.h"

#include "tier0/minidump.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

VPanel::VPanel()
{
}

VPanel::~VPanel()
{
}

void VPanel::TraverseLevel( int val )
{
}

void VPanel::Init(IClientPanel *attachedClientPanel)
{
}

void VPanel::Solve()
{
}

void VPanel::SetPos(int x, int y)
{
}

void VPanel::GetPos(int &x, int &y)
{
}

void VPanel::SetSize(int wide,int tall)
{
}

void VPanel::GetSize(int& wide,int& tall)
{
}

void VPanel::SetMinimumSize(int wide,int tall)
{
}

void VPanel::GetMinimumSize(int &wide, int &tall)
{
}

void VPanel::SetVisible(bool state)
{
}

void VPanel::SetEnabled(bool state)
{
}

bool VPanel::IsVisible()
{
	return false;
}

bool VPanel::IsEnabled()
{
	return false;
}

void VPanel::GetAbsPos(int &x, int &y)
{
}

void VPanel::GetInternalAbsPos(int &x, int &y)
{
}

void VPanel::GetClipRect(int &x0, int &y0, int &x1, int &y1)
{
}

void VPanel::SetInset(int left, int top, int right, int bottom)
{
}

void VPanel::GetInset(int &left, int &top, int &right, int &bottom)
{
}

void VPanel::SetParent(VPanel *newParent)
{
}

int VPanel::GetChildCount()
{
	return 0;
}

VPanel *VPanel::GetChild(int index)
{
	return NULL;
}

CUtlVector< VPanel *> panels;
CUtlVector< VPanel *> &VPanel::GetChildren()
{
	return panels;
//	return NULL;
}

VPanel *VPanel::GetParent()
{
	return NULL;
}

void VPanel::SetZPos(int z)
{
}

int VPanel::GetZPos()
{
	return 0;
}

void VPanel::MoveToFront(void)
{
}

void VPanel::MoveToBack()
{
}

bool VPanel::HasParent(VPanel *potentialParent)
{
	return true;
}

SurfacePlat *VPanel::Plat()
{
	return NULL;
}

void VPanel::SetPlat(SurfacePlat *Plat)
{
}

bool VPanel::IsPopup()
{
	return false;
}

void VPanel::SetPopup(bool state)
{
}

bool VPanel::IsTopmostPopup() const
{
	return true;
}

void VPanel::SetTopmostPopup( bool bEnable )
{
}

bool VPanel::IsFullyVisible()
{
	return true;
}

const char *VPanel::GetName()
{
	return "";
}

const char *VPanel::GetClassName()
{
	return "";
}

HScheme VPanel::GetScheme()
{
	return NULL;
}


void VPanel::SendMessage(KeyValues *params, VPANEL ifrompanel)
{
}


void VPanel::SetKeyBoardInputEnabled(bool state)
{
}

void VPanel::SetMouseInputEnabled(bool state)
{
}

bool VPanel::IsKeyBoardInputEnabled()
{
	return true;
}

bool VPanel::IsMouseInputEnabled()
{
	return true;
}

void VPanel::SetSiblingPin(VPanel *newSibling, byte iMyCornerToPin, byte iSiblingCornerToPinTo )
{
}
