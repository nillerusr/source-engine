//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//

#include "OptionsSubModification.h"
#include "CvarSlider.h"

#include "CvarToggleCheckButton.h"
#include "EngineInterface.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include "vgui_controls/Controls.h"
#include <vgui_controls/TextEntry.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

COptionsSubModification::COptionsSubModification(vgui::Panel *parent) : PropertyPage(parent, nullptr)
{
    // Create the slider for aspect ratio adjustments
    m_pAspectRatioSlider = new CCvarSlider(
        this,
        "AspectRatioSlider",
        "Aspect Ratio",
        0.1f, 3.0f, // Slider range: 0.1 to 3.0
        "r_aspectratio",
        true // Allow fractional values
    );
 
    m_pAspectRatioLabel = new TextEntry(this, "AspectRatioLabel");
    m_pAspectRatioLabel->AddActionSignalTarget(this);

    m_pChangeCheatFlag = new CCvarToggleCheckButton(
       this , "ChangeCheatFlag" , "Change Cheat Flag" , "sv_cheats"
    );
    m_pGiveWeaponButton = new Button(this, "GiveWeapon", "Give Weapon");
    // Load settings from the associated resource file
    LoadControlSettings("Resource\\OptionsSubModification.res");
    m_pGiveWeaponButton->SetCommand("GiveWeapon");
    m_pGiveWeaponButton->AddActionSignalTarget(this);

    m_pGiveHealthKitButton = new Button(this, "GiveHealthKit", "Give Health Kit");
    m_pGiveHealthKitButton->SetCommand("GiveHealthKit");
    m_pGiveHealthKitButton->AddActionSignalTarget(this);


    UpdateLabel(m_pAspectRatioSlider, m_pAspectRatioLabel);

    m_pGiveWeaponButton->SetEnabled(true);
}
COptionsSubModification::~COptionsSubModification() = default;

void COptionsSubModification::OnTextChanged(Panel *panel)
{
    if (panel == m_pAspectRatioLabel)
    {
        char buf[64];
        m_pAspectRatioLabel->GetText(buf, 64);

        float fValue;
        int numParsed = sscanf(buf, "%f", &fValue);
        if ((numParsed == 1) && (fValue >= 0.0f))
        {
            m_pAspectRatioSlider->SetSliderValue(fValue);
            PostActionSignal(new KeyValues("ApplyButtonEnable"));
        }
    }
}
//-----------------------------------------------------------------------------
// Purpose: Resets the data to the initial state
//-----------------------------------------------------------------------------
void COptionsSubModification::OnResetData()
{
    m_pAspectRatioSlider->Reset();
    m_pChangeCheatFlag->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Applies changes made by the user
//-----------------------------------------------------------------------------
void COptionsSubModification::OnApplyChanges()
{
    m_pAspectRatioSlider->ApplyChanges();
    m_pChangeCheatFlag->ApplyChanges();
    
}

//-----------------------------------------------------------------------------
// Purpose: Handles slider modifications
//-----------------------------------------------------------------------------
void COptionsSubModification::OnControlModified(Panel *panel)
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));

    // Update the label based on slider changes
    if (panel == m_pAspectRatioSlider && m_pAspectRatioSlider->HasBeenModified())
    {
        UpdateLabel(m_pAspectRatioSlider, m_pAspectRatioLabel);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Sets scheme-specific properties
//-----------------------------------------------------------------------------
void COptionsSubModification::ApplySchemeSettings(IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
}

//-----------------------------------------------------------------------------
// Purpose: Updates the label text based on slider value
//-----------------------------------------------------------------------------
void COptionsSubModification::UpdateLabel(CCvarSlider *slider, vgui::TextEntry *label)
{
    char buf[64];
    Q_snprintf(buf, sizeof(buf), "%.2f", slider->GetSliderValue());
    label->SetText(buf);
}
void COptionsSubModification::OnCommand(const char *command)
{
    if (!stricmp(command, "GiveWeapon"))
    {
        engine->ExecuteClientCmd("impulse 101");
        Msg("GiveWeapon command triggered\n"); // Debug message
    }
    if(!stricmp(command, "GiveHealthKit"))
    {
        engine->ExecuteClientCmd("give item_healthkit");
        Msg("GiveHealthKit command triggered\n");
    }
    else
    {
        BaseClass::OnCommand(command);  // Make sure to call the base class for any other commands
    }
}