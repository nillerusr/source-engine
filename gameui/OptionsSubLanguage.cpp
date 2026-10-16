//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Language selection options page
//
//=============================================================================//
#include "OptionsSubLanguage.h"

#include "EngineInterface.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Label.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include "tier1/convar.h"
#include "common/language.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

// Languages offered in the UI selection
struct LanguageOption_t
{
	ELanguage	eLang;
	const char	*pchName;		// Display name (localization token)
};

static const LanguageOption_t s_LanguageOptions[] =
{
	{ k_Lang_English,				"#GameUI_Language_English" },
	{ k_Lang_Simplified_Chinese,	"#GameUI_Language_Simplified_Chinese" },
	{ k_Lang_Traditional_Chinese,	"#GameUI_Language_Traditional_Chinese" },
	{ k_Lang_Japanese,				"#GameUI_Language_Japanese" },
	{ k_Lang_Korean,				"#GameUI_Language_Korean" },
	{ k_Lang_German,				"#GameUI_Language_German" },
	{ k_Lang_French,				"#GameUI_Language_French" },
	{ k_Lang_Spanish,				"#GameUI_Language_Spanish" },
	{ k_Lang_Russian,				"#GameUI_Language_Russian" },
};

COptionsSubLanguage::COptionsSubLanguage(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	m_pLanguageLabel = new Label(this, "LanguageLabel", "#GameUI_Language");

	m_pLanguageCombo = new ComboBox(this, "LanguageCombo", 10, false);
	m_pLanguageCombo->AddActionSignalTarget(this);

	for ( int i = 0; i < Q_ARRAYSIZE(s_LanguageOptions); ++i )
	{
		KeyValues *pData = new KeyValues("Language", "lang", (int)s_LanguageOptions[i].eLang);
		m_pLanguageCombo->AddItem(s_LanguageOptions[i].pchName, pData);
	}

	LoadControlSettings("Resource\\OptionsSubLanguage.res");
}

COptionsSubLanguage::~COptionsSubLanguage()
{
}

void COptionsSubLanguage::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

void COptionsSubLanguage::OnResetData()
{
	// Read the current language from cl_language
	ConVarRef cl_language("cl_language");
	const char *pchLang = cl_language.IsValid() ? cl_language.GetString() : "english";

	ELanguage eLang = PchLanguageToELanguage(pchLang, k_Lang_English);
	m_nCurrentLanguage = (int)eLang;

	// Select the matching item
	for ( int i = 0; i < m_pLanguageCombo->GetItemCount(); ++i )
	{
		KeyValues *pData = m_pLanguageCombo->GetItemUserData(i);
		if ( pData && pData->GetInt("lang") == m_nCurrentLanguage )
		{
			m_pLanguageCombo->ActivateItem(i);
			break;
		}
	}
}

void COptionsSubLanguage::OnApplyChanges()
{
	KeyValues *pData = m_pLanguageCombo->GetActiveItemUserData();
	if ( !pData )
		return;

	int nLang = pData->GetInt("lang", k_Lang_English);
	if ( nLang == m_nCurrentLanguage )
		return;

	ELanguage eLang = (ELanguage)nLang;
	const char *pchShortName = GetLanguageShortName(eLang);

	// Update cl_language
	ConVarRef cl_language("cl_language");
	if ( cl_language.IsValid() )
	{
		cl_language.SetValue(pchShortName);
	}

	// Reload all localization files so the new language takes effect
	if ( g_pVGuiLocalize )
	{
		g_pVGuiLocalize->RemoveAll();
		g_pVGuiLocalize->AddFile("Resource/gameui_%language%.txt", "GAME", true);
		g_pVGuiLocalize->AddFile("Resource/valve_%language%.txt", "GAME", true);
		g_pVGuiLocalize->AddFile("Resource/platform_%language%.txt");
		g_pVGuiLocalize->AddFile("Resource/vgui_%language%.txt");
	}

	m_nCurrentLanguage = nLang;
}

void COptionsSubLanguage::OnControlModified(Panel *panel)
{
	if ( panel == m_pLanguageCombo )
	{
		SetModified(true);
	}
}

void COptionsSubLanguage::OnTextChanged(Panel *panel)
{
	OnControlModified(panel);
}
