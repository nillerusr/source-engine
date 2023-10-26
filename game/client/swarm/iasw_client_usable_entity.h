#ifndef _INCLUDED_ASW_CLIENT_USABLE_ENTITY_H
#define _INCLUDED_ASW_CLIENT_USABLE_ENTITY_H

#include "asw_shareddefs.h"

#define MAX_USE_ACTION_TEXT 128

struct ASWUseAction
{
	ASWUseAction()
	{
		iUseIconTexture = -1;
		szCommand[0] = 0;
		wszText[0] = 0;
		wszHoldButtonText[0] = 0;
		UseTarget = NULL;
		fProgress = -1;
		UseIconRed = 255;
		UseIconGreen = 255;
		UseIconBlue = 255;
		TextRed = 255;
		TextGreen = 255;
		TextBlue = 255;
		bShowUseKey = true;
		bTextGlow = true;
		iInventorySlot = -1;
		wszHoldButtonText[0] = 0;
		bShowHoldButtonUseKey = false;
		bNoFadeIfSameUseTarget = false;
		bWideIcon = false;
	}

	const bool operator==(const ASWUseAction &other) const
	{
      return (iUseIconTexture == other.iUseIconTexture &&
		UseTarget == other.UseTarget &&
		fProgress == other.fProgress &&
		UseIconRed == other.UseIconRed &&
		UseIconGreen == other.UseIconGreen &&
		UseIconBlue == other.UseIconBlue &&
		bShowUseKey == other.bShowUseKey &&
		bShowHoldButtonUseKey == other.bShowHoldButtonUseKey &&
		TextRed == other.TextRed &&
		TextGreen == other.TextGreen &&
		TextBlue == other.TextBlue &&
		bTextGlow == other.bTextGlow &&
		iInventorySlot == other.iInventorySlot &&
		!V_strcmp( szCommand, other.szCommand ) &&
		!wcscoll( wszText, other.wszText ) &&
		!wcscoll( wszHoldButtonText, other.wszHoldButtonText ) &&
		bWideIcon == other.bWideIcon
		);
    }

	// used for use icon actions that shouldn't cause a fade in/out of the icon to change over
	//  i.e. ignores progress bar changing
	const bool RoughlyEqual(const ASWUseAction &other) const
	{
      return (iUseIconTexture == other.iUseIconTexture &&
		UseTarget == other.UseTarget &&
		bShowUseKey == other.bShowUseKey &&
		bShowHoldButtonUseKey == other.bShowHoldButtonUseKey &&
		UseIconRed == other.UseIconRed &&
		UseIconGreen == other.UseIconGreen &&
		UseIconBlue == other.UseIconBlue &&
		TextRed == other.TextRed &&
		TextGreen == other.TextGreen &&
		TextBlue == other.TextBlue &&
		bTextGlow == other.bTextGlow &&
		iInventorySlot == other.iInventorySlot &&
		!V_strcmp( szCommand, other.szCommand ) &&
		!wcscoll( wszText, other.wszText ) &&
		!wcscoll( wszHoldButtonText, other.wszHoldButtonText ) &&
		bWideIcon == other.bWideIcon
		);
    }

	int iUseIconTexture;
	char szCommand[64];
	wchar_t wszText[MAX_USE_ACTION_TEXT];
	wchar_t wszHoldButtonText[MAX_USE_ACTION_TEXT];
	EHANDLE UseTarget;
	float fProgress;
	int UseIconRed;
	int UseIconGreen;
	int UseIconBlue;
	int TextRed;
	int TextGreen;
	int TextBlue;
	bool bShowUseKey;
	bool bShowHoldButtonUseKey;
	bool bTextGlow;
	bool bNoFadeIfSameUseTarget;
	int iInventorySlot;
	bool bWideIcon;
};

class C_ASW_Marine;
class vgui::Panel;

abstract_class IASW_Client_Usable_Entity
{
public:	
	virtual C_BaseEntity* GetEntity() = 0;
	virtual bool IsUsable(C_BaseEntity *pUser) = 0;
	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser) = 0;
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon) = 0;
	virtual bool ShouldPaintBoxAround() = 0;
	virtual bool NeedsLOSCheck() = 0;
};

#endif // _INCLUDED_ASW_CLIENT_USABLE_ENTITY_H