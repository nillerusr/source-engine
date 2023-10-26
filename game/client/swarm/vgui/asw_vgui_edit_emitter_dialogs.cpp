#include "cbase.h"
#include "asw_vgui_edit_emitter_dialogs.h"
#include "asw_vgui_edit_emitter.h"
#include <KeyValues.h>
#include <filesystem.h>
#include "fmtstr.h"
#include "convar.h"
#include "vgui/ivgui.h"
#include "vgui_controls/combobox.h"
#include "vgui_controls/checkbutton.h"
#include "vgui_controls/ScrollBar.h"
#include "iclientmode.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls\PanelListPanel.h"
#include "c_asw_generic_emitter_entity.h"
#include "gamestringpool.h"
#include "precache_register.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Edit_Emitter_SaveDialog::CASW_VGUI_Edit_Emitter_SaveDialog( vgui::Panel *pParent, const char *pElementName, CASW_VGUI_Edit_Emitter* pEditFrame ) 
 :	vgui::Frame( pParent, pElementName )
{	
	m_pEditFrame = pEditFrame;
	SetTitle("Save Template", true);
	SetSizeable(false);
	SetMoveable(false);
	SetTall(140);
	SetWide(320);
	SetCloseButtonVisible(false);
	MoveToCenterOfScreen();

	vgui::Label* pPleaseLabel = new vgui::Label(this, "SaveDLabel", "Please enter a name to save this template as:");
	pPleaseLabel->SetPos(20,25);
	pPleaseLabel->SetWide(290);
	pPleaseLabel->SetTall(20);

	//vgui::Label* pLabel = new vgui::Label(this, "SaveDLabel", "Template Name:");
	//pLabel->SetPos(20,45);
	//pLabel->SetWide(220);
	//pLabel->SetTall(20);

	m_pSaveText = new vgui::TextEntry(this, "SaveDText");
	m_pSaveText->SetPos(20,45);
	m_pSaveText->SetWide(220);
	m_pSaveText->SetTall(20);
	char buf[MAX_PATH];
	m_pEditFrame->m_pTemplateCombo->GetText(buf, MAX_PATH);
	m_pSaveText->SetText(buf);

	m_pSaveButton = new vgui::Button(this, "SaveDButton", "Save");
	m_pSaveButton->SetCommand(new KeyValues("SaveDButton"));
	m_pSaveButton->SetWide(60);
	m_pSaveButton->SetTall(20);
	m_pSaveButton->SetPos(190, 115);
	m_pSaveButton->AddActionSignalTarget(this);

	m_pCancelButton = new vgui::Button(this, "CancelDButton", "Cancel");
	m_pCancelButton->SetCommand(new KeyValues("CancelDButton"));
	m_pCancelButton->SetWide(60);
	m_pCancelButton->SetTall(20);
	m_pCancelButton->SetPos(255, 115);
	m_pCancelButton->AddActionSignalTarget(this);

	m_pExistsLabel = new vgui::Label(this, "ExistsLabel", " ");
	m_pExistsLabel->SetPos(20,70);
	m_pExistsLabel->SetWide(300);
	m_pExistsLabel->SetTall(40);

	TextEntryChanged(m_pSaveText);
}

CASW_VGUI_Edit_Emitter_SaveDialog::~CASW_VGUI_Edit_Emitter_SaveDialog( void )
{

}

void CASW_VGUI_Edit_Emitter_SaveDialog::TextEntryChanged(vgui::Panel* pTextEntry)
{
	char fulltemplatename[MAX_PATH];
	char buf[MAX_PATH];
	if (pTextEntry != m_pSaveText)
		return;

	m_pSaveText->GetText(buf, MAX_PATH);
	Q_snprintf( fulltemplatename, sizeof( fulltemplatename ), "resource/particletemplates/%s.ptm", buf );
	if (filesystem->FileExists(fulltemplatename))	// file exists
	{
		m_pExistsLabel->SetText("This template name already exists.  Your changes\nwill affect all emitters using this template.");
	}
	else
	{
		m_pExistsLabel->SetText("");
	}
}

void CASW_VGUI_Edit_Emitter_SaveDialog::SaveButtonClicked()
{
	char buf[MAX_PATH];
	m_pSaveText->GetText(buf, MAX_PATH);
	m_pEditFrame->SaveDialogSave(buf);
	m_pEditFrame->SaveDialogClosed();
	Close();
}

void CASW_VGUI_Edit_Emitter_SaveDialog::CancelButtonClicked()
{
	m_pEditFrame->SaveDialogClosed();
	Close();
}