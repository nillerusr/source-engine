//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_HAPTICS_H
#define OPTIONS_SUB_HAPTICS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>
#include "HapticControlBox.h"
 

class CCvarNegateCheckButton;
class CKeyToggleCheckButton;
class CCvarToggleCheckButton;
class CCvarSlider;

namespace vgui
{
    class Label;
    class Panel;
}

//-----------------------------------------------------------------------------
// Purpose: Mouse Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubHaptics : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubHaptics, vgui::PropertyPage );

public:
	COptionsSubHaptics(vgui::Panel *parent);
	~COptionsSubHaptics();

	virtual void OnResetData();
	virtual void OnApplyChanges();
	virtual void OnCommand(const char *command);
	virtual void UpdateVehicleEnabled(void);

protected:
    virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	MESSAGE_FUNC_PTR( OnControlModified, "ControlModified", panel );
    MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel )
	{
		OnControlModified( panel );
	}

	vgui::Label *m_pForceMasterPreLabel;
	CCvarSlider	*m_pForceMasterSlider;

	vgui::Label *m_pForceRecoilPreLabel;
	CCvarSlider	*m_pForceRecoilSlider;


	vgui::Label *m_pForceDamagePreLabel;
	CCvarSlider	*m_pForceDamageSlider;

	vgui::Label *m_pForceMovementPreLabel;
	CCvarSlider	*m_pForceMovementSlider;


	
	//Player
	vgui::Label *m_pPlayerBoxPreLabel;
	vgui::Label *m_pPlayerBoxScalePreLabel;
	CCvarSlider	* m_pPlayerBoxScale;
	ControlBoxVisual* m_pPlayerBoxVisual;
	vgui::Label *m_pPlayerBoxStiffnessPreLabel;
	CCvarSlider	*m_pPlayerBoxStiffnessSlider;
	vgui::Label *m_pPlayerBoxTurnPreLabel;
	CCvarSlider	*m_pPlayerBoxTurnSlider;
	vgui::Label *m_pPlayerBoxAimPreLabel;
	CCvarSlider	*m_pPlayerBoxAimSlider;

	//Vehicle
	vgui::Label *m_pVehicleBoxPreLabel;
	vgui::Label *m_pVehicleBoxScalePreLabel;
	CCvarSlider	* m_pVehicleBoxScale;
	ControlBoxVisual* m_pVehicleBoxVisual;
	vgui::Label *m_pVehicleBoxStiffnessPreLabel;
	CCvarSlider	* m_pVehicleBoxStiffnessSlider;
	vgui::Label *m_pVehicleBoxTurnPreLabel;
	CCvarSlider	*m_pVehicleBoxTurnSlider;
	vgui::Label *m_pVehicleBoxAimPreLabel;
	CCvarSlider	*m_pVehicleBoxAimSlider;

	//Control Box Stuff
};



#endif // OPTIONS_SUB_MOUSE_H