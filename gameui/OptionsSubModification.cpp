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

#include "EngineInterface.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include "tier1/convar.h"
#include <vgui_controls/TextEntry.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

COptionsSubModification::COptionsSubModification(vgui::Panel *parent) : PropertyPage(parent, nullptr)
{
    // Create the slider for aspect ratio adjustments
    m_pEnableModificationsCheckBox = new CCvarSlider(
        this,
        "AspectRatioSlider",
        "Aspect Ratio",
        0.1f, 3.0f, // Slider range: 0.1 to 3.0
        "r_aspectratio",
        true // Allow fractional values
    );
    m_aspectRatioLabel = new TextEntry(this, "AspectRatioLabel");
    m_aspectRatioLabel->AddActionSignalTarget(this);

    // Load settings from the associated resource file
    LoadControlSettings("Resource\\OptionsSubModification.res");

    UpdateLabel(m_pEnableModificationsCheckBox, m_aspectRatioLabel);
}
COptionsSubModification::~COptionsSubModification() = default;

void COptionsSubModification::OnTextChanged(Panel *panel)
{
    if (panel == m_aspectRatioLabel)
    {
        char buf[64];
        m_aspectRatioLabel->GetText(buf, 64);

        float fValue;
        int numParsed = sscanf(buf, "%f", &fValue);
        if ((numParsed == 1) && (fValue >= 0.0f))
        {
            m_pEnableModificationsCheckBox->SetSliderValue(fValue);
            PostActionSignal(new KeyValues("ApplyButtonEnable"));
        }
    }
}
//-----------------------------------------------------------------------------
// Purpose: Resets the data to the initial state
//-----------------------------------------------------------------------------
void COptionsSubModification::OnResetData()
{
    m_pEnableModificationsCheckBox->Reset();
    // m_aspectRatioLabel->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Applies changes made by the user
//-----------------------------------------------------------------------------
void COptionsSubModification::OnApplyChanges()
{
    m_pEnableModificationsCheckBox->ApplyChanges();
    // m_aspectRatioLabel->ApplyChanges();
}

//-----------------------------------------------------------------------------
// Purpose: Handles slider modifications
//-----------------------------------------------------------------------------
void COptionsSubModification::OnControlModified(Panel *panel)
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));

    // Update the label based on slider changes
    if (panel == m_pEnableModificationsCheckBox && m_pEnableModificationsCheckBox->HasBeenModified())
    {
        UpdateLabel(m_pEnableModificationsCheckBox, m_aspectRatioLabel);
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