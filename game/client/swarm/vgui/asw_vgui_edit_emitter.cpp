#include "cbase.h"
#include "asw_vgui_edit_emitter.h"
#include "c_asw_generic_emitter.h"
#include "asw_vgui_edit_emitter_dialogs.h"
#include <KeyValues.h>
#include <filesystem.h>
#include "fmtstr.h"
#include "convar.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include "vgui_controls/combobox.h"
#include "vgui_controls/checkbutton.h"
#include "vgui_controls/ScrollBar.h"
#include "iclientmode.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls\PanelListPanel.h"
#include "c_asw_generic_emitter_entity.h"
#include "gamestringpool.h"
#include "precache_register.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// positions
#define ASW_EDIT_EMITTER_INDENT_X			XRES(20)
#define ASW_EDIT_EMITTER_INDENT_Y			YRES(10)

// Scoreboard dimensions
#define ASW_EDIT_EMITTER_TITLE_SIZE_Y			YRES(22)

#define ASW_EDIT_EMITTER_X_BORDER					XRES(4)
#define ASW_EDIT_EMITTER_Y_BORDER					YRES(4)

#define ASW_EDIT_EMITTER_MIN_X -400
#define ASW_EDIT_EMITTER_MAX_X 400

#define ASW_ADD_SLIDER(name, minval, maxval, label) \
	AddSliderAndTextEntry(m_p##name##Slider, m_p##name##Text, #name, #name "Text", label, minval, maxval)

#define ASW_ADD_SIMPLE_NODE(name, minval, maxval) \
	AddSimpleNode( m_p##name##Check,  m_p##name##Time, m_p##name##Value, m_p##name##ValueText, m_p##name##TimeText, \
					#name "Check", #name "Time", #name "Value", #name "TimeText", #name "ValueText", \
						0.0f, 100.0f, minval, maxval)

#define ASW_ADD_COLOR_TR_NODE(name, minval, maxval, minval2, maxval2) \
	AddSimpleNode( m_p##name##Check,  m_p##name##Time, m_p##name##Red, m_p##name##RedText, m_p##name##TimeText, \
					#name "Check", #name "Time", #name "Red", #name "TimeText", #name "RedText", \
						minval, maxval, minval2, maxval2)

#define ASW_ADD_COLOR_GB_NODE(name, minval, maxval, minval2, maxval2) \
	AddSimpleNode( NULL,  m_p##name##Green, m_p##name##Blue, m_p##name##BlueText, m_p##name##GreenText, \
				NULL, #name "Green", #name "Blue", #name "GreenText", #name "BlueText", \
						minval, maxval, minval2, maxval2)

#define ASW_ADD_COLOR_NODE(name) \
	AddColorNode( m_p##name##Check,  m_p##name##Time, m_p##name##TimeText, \
					m_p##name##Red, m_p##name##Green, m_p##name##Blue, \
					m_p##name##RedText, m_p##name##GreenText, m_p##name##BlueText, \
					#name "Check", #name "Time", #name "TimeText", \
					#name "Red", #name "Green", #name "Blue", \
					#name "RedText", #name "GreenText", #name "BlueText")

CASW_VGUI_Edit_Emitter::CASW_VGUI_Edit_Emitter( vgui::Panel *pParent, const char *pElementName ) 
 :	vgui::Frame( pParent, pElementName )
{	
	LoadTextures();
	SetProportional(false); 

	int x = ASW_EDIT_EMITTER_INDENT_X;
	int y = ASW_EDIT_EMITTER_INDENT_Y;
	int wide = 380.0 - x * 2;
	int tall = GetClientMode()->GetViewport()->GetTall() - y * 2;
	m_pEmitter = NULL;

	SetBounds( x, y, wide, tall );
	SetSizeable(false);

	SetBgColor(Color(0, 0, 0, 175));

	// Initialize the top title.
	SetTitle("Edit Particle System", false);
	m_pResetButton = new vgui::Button(this, "ResetButton", "Reset Particles");
	m_pResetButton->SetCommand(new KeyValues("ResetEmitter"));
	m_pResetButton->SetWide(100);
	m_pResetButton->SetTall(20);
	m_pResetButton->SetPos(ASW_EDIT_EMITTER_X_BORDER, tall - ((ASW_EDIT_EMITTER_Y_BORDER * 2) + (ASW_EDIT_EMITTER_TITLE_SIZE_Y * 0.7f)));

	m_ListPanel = new vgui::PanelListPanel( this, "listpanel_edit_emitter" );
	m_ListPanel->SetPos(ASW_EDIT_EMITTER_X_BORDER, ASW_EDIT_EMITTER_Y_BORDER + ASW_EDIT_EMITTER_TITLE_SIZE_Y);
	m_ListPanel->SetWide(wide - ASW_EDIT_EMITTER_X_BORDER * 2);
	m_ListPanel->SetTall(tall - ((ASW_EDIT_EMITTER_Y_BORDER * 2) + (ASW_EDIT_EMITTER_TITLE_SIZE_Y * 2)));
	m_ListPanel->SetFirstColumnWidth(0);

	m_LayoutCursorY = 0;
	// add our sliders + text entries
	AddLabel("Template:");
	AddTemplateBoxAndButtons();
	AddLabel("Material:");
	AddMaterialDropDown();
	AddLabel("Particles per second:");
	ASW_ADD_SLIDER(ParticlesPerSecond, 0, 60, "");
	AddLabel("Particle supply:");
	ASW_ADD_SLIDER(InitialParticleSupply, -1, 1000, "");	
	AddNumParticlesLabel();
	AddLabel("Lifetime:");
	ASW_ADD_SLIDER(ParticleLifetimeMin, 0, 20, "Min");
	ASW_ADD_SLIDER(ParticleLifetimeMax, 0, 20, "Max");
	AddLabel("Presimulate Time:");
	ASW_ADD_SLIDER(PresimulateTime, 0, 30, "");
	AddLabel("Start roll:");
	ASW_ADD_SLIDER(ParticleStartRollMin, 0, 360, "Min");
	ASW_ADD_SLIDER(ParticleStartRollMax, 0, 360, "Max");
	AddLabel("Roll rate:");
	ASW_ADD_SLIDER(ParticleRollRateMin, -360, 360, "Min");
	ASW_ADD_SLIDER(ParticleRollRateMax, -360, 360, "Max");
	AddLabel("Start Position Min:");
	ASW_ADD_SLIDER(ParticlePositionMinX, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "X");
	ASW_ADD_SLIDER(ParticlePositionMinY, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Y");
	ASW_ADD_SLIDER(ParticlePositionMinZ, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Z");
	AddLabel("Start Position Max:");
	ASW_ADD_SLIDER(ParticlePositionMaxX, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "X");
	ASW_ADD_SLIDER(ParticlePositionMaxY, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Y");
	ASW_ADD_SLIDER(ParticlePositionMaxZ, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Z");
	AddLabel("Velocity Min:");
	ASW_ADD_SLIDER(ParticleVelocityMinX, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "X");
	ASW_ADD_SLIDER(ParticleVelocityMinY, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Y");
	ASW_ADD_SLIDER(ParticleVelocityMinZ, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Z");
	AddLabel("Velocity Max:");
	ASW_ADD_SLIDER(ParticleVelocityMaxX, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "X");
	ASW_ADD_SLIDER(ParticleVelocityMaxY, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Y");
	ASW_ADD_SLIDER(ParticleVelocityMaxZ, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Z");
	AddLabel("Acceleration Min:");
	ASW_ADD_SLIDER(ParticleAccnMinX, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "X");
	ASW_ADD_SLIDER(ParticleAccnMinY, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Y");
	ASW_ADD_SLIDER(ParticleAccnMinZ, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Z");
	AddLabel("Acceleration Max:");
	ASW_ADD_SLIDER(ParticleAccnMaxX, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "X");
	ASW_ADD_SLIDER(ParticleAccnMaxY, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Y");
	ASW_ADD_SLIDER(ParticleAccnMaxZ, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Z");
	AddLabel("Gravity:");
	ASW_ADD_SLIDER(Gravity, ASW_EDIT_EMITTER_MIN_X, ASW_EDIT_EMITTER_MAX_X, "Z");
	m_LayoutCursorY += 10;
	AddLabel("Local movement:");
	ASW_ADD_SLIDER(ParticleLocal, 0, 100, "%");
	m_LayoutCursorY += 10;
	AddLabel("Collision:");
	AddCollisionDropDown();
	AddLabel("Velocity retained on collision:");
	ASW_ADD_SLIDER(CollisionDampening, 0, 100, "%");
	AddLabel("Life lost on collision:");
	ASW_ADD_SLIDER(LifeLostOnCollision, 0, 10, "s");
	AddLabel("Collision sound:");	
	AddCollisionSoundEdit();
	AddCollisionDecalEdit();
	AddLabel("Spawn particles on collision:");
	AddCollisionTemplateDropDown();
	m_LayoutCursorY += 10;
	AddLabel("Particles spawn children:");
	AddDropletTemplateDropDown();
	ASW_ADD_SLIDER(DropletChance, 0, 100, "Freq.");
	m_LayoutCursorY += 10;
			
	AddLabel("Glow Material:");
	AddGlowMaterialDropDown();
	//AddLabel("Glow deviation:");
	ASW_ADD_SLIDER(GlowDeviation, 0, 100, "Dev.");
	ASW_ADD_SLIDER(GlowScale, 0, 10, "Scale");	
	m_LayoutCursorY += 10;
	AddLabel("Draw Type:");
	AddDrawTypeDropDown();
	AddLabel("Beam Length:");
	ASW_ADD_SLIDER(BeamLength, 0, 100, "Length");
	AddLabel("Beam Positioning:");
	AddBeamPositionDropDown();
	AddCheckbox(m_pScaleBeamByVelocityCheck, "Scale Beam by Velocity");
	AddCheckbox(m_pScaleBeamByLifeLeftCheck, "Scale Beam by life left");
	AddLabel("Lighting Type:");
	AddLightingDropDown();
	AddLabel("Lighting Apply:");
	ASW_ADD_SLIDER(LightApply, 0, 100, "%");
	AddLabel("Alpha:");
	AddSimpleNodeLabel("Time", "Value");
	ASW_ADD_SIMPLE_NODE(Alpha0, 0.0f, 255.0f);
	ASW_ADD_SIMPLE_NODE(Alpha1, 0.0f, 255.0f);
	ASW_ADD_SIMPLE_NODE(Alpha2, 0.0f, 255.0f);
	ASW_ADD_SIMPLE_NODE(Alpha3, 0.0f, 255.0f);
	ASW_ADD_SIMPLE_NODE(Alpha4, 0.0f, 255.0f);
	AddLabel("Size:");
	AddSimpleNodeLabel("Time", "Value");
	ASW_ADD_SIMPLE_NODE(Scale0, 0.0f, 100.0f);
	ASW_ADD_SIMPLE_NODE(Scale1, 0.0f, 100.0f);
	ASW_ADD_SIMPLE_NODE(Scale2, 0.0f, 100.0f);
	ASW_ADD_SIMPLE_NODE(Scale3, 0.0f, 100.0f);
	ASW_ADD_SIMPLE_NODE(Scale4, 0.0f, 100.0f);
	AddLabel("Color:");
	AddSimpleNodeLabel("Time", "Red");
	ASW_ADD_COLOR_NODE(Color0);
	AddSimpleNodeLabel("Time", "Red");
	ASW_ADD_COLOR_NODE(Color1);
	AddSimpleNodeLabel("Time", "Red");
	ASW_ADD_COLOR_NODE(Color2);
	AddSimpleNodeLabel("Time", "Red");
	ASW_ADD_COLOR_NODE(Color3);
	AddSimpleNodeLabel("Time", "Red");
	ASW_ADD_COLOR_NODE(Color4);			
	
	SetZPos( 1002 );
	bIgnoreNextSliderChange = 0;

	m_pSaveDialog = NULL;
}

CASW_VGUI_Edit_Emitter::~CASW_VGUI_Edit_Emitter( void )
{
}

void CASW_VGUI_Edit_Emitter::AddLabel(const char *pText)
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(12);
	pPanel->SetPos(0,m_LayoutCursorY);

	vgui::Label *p = new vgui::Label(pPanel, "Edit_emitter_label", pText);
	p->SetWide(220);
	p->SetTall(12);

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 12;
}

void CASW_VGUI_Edit_Emitter::AddNumParticlesLabel()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(12);
	pPanel->SetPos(0, m_LayoutCursorY);

	m_pNumParticlesLabel = new vgui::Label(pPanel, "Edit_emitter_label", "Current Num Particles:");
	m_pNumParticlesLabel->SetWide(220);
	m_pNumParticlesLabel->SetTall(12);
	m_pNumParticlesLabel->SetPos(45, 0);

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 12;
}

void CASW_VGUI_Edit_Emitter::AddTemplateBoxAndButtons()
{
	BuildTemplateList();	// scan the particle templates folder for templates to load

	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(260);
	pPanel->SetTall(20);
	pPanel->SetPos(0, m_LayoutCursorY);

	/*
	m_TemplateTextEntry = new vgui::TextEntry(pPanel, "TemplateText");
	m_TemplateTextEntry->SetWide(220);
	m_TemplateTextEntry->SetTall(20);
	m_TemplateTextEntry->SetPos(20, 0);
	m_TemplateTextEntry->SetEditable(false);
	*/
	m_pTemplateCombo = new vgui::ComboBox(pPanel, "TemplateCombo",iszTemplateNames.Count(),false);
	m_pTemplateCombo->SetTall(20);
	m_pTemplateCombo->SetWide(200);
	m_pTemplateCombo->SetPos(0,0);
	m_pTemplateCombo->AddActionSignalTarget(this);
	for (int i=0; i<iszTemplateNames.Count();i++)
	{
		m_pTemplateCombo->AddItem(iszTemplateNames[i], NULL);
	}

	m_pTemplateSaveButton = new vgui::Button(pPanel, "SaveTemplateButton", "Save");
	m_pTemplateSaveButton->SetCommand(new KeyValues("SaveTemplate"));
	m_pTemplateSaveButton->AddActionSignalTarget(this);
	m_pTemplateSaveButton->SetWide(50);
	m_pTemplateSaveButton->SetTall(20);
	m_pTemplateSaveButton->SetPos(205, 00);

	/*m_pTemplateLoadButton = new vgui::Button(pPanel, "LoadTemplateButton", "Load");
	m_pTemplateLoadButton->SetCommand(new KeyValues("LoadTemplate"));
	m_pTemplateLoadButton->SetWide(60);
	m_pTemplateLoadButton->SetTall(20);
	m_pTemplateLoadButton->SetPos(20, 20);

	m_pTemplateSaveAsButton = new vgui::Button(pPanel, "SaveTemplateButton", "Save As");
	m_pTemplateSaveAsButton->SetCommand(new KeyValues("SaveTemplateAs"));
	m_pTemplateSaveAsButton->SetWide(60);
	m_pTemplateSaveAsButton->SetTall(20);
	m_pTemplateSaveAsButton->SetPos(160, 20);*/

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::UpdateNumParticlesLabel()
{
	char buf[64];
	if (m_pEmitter && m_pNumParticlesLabel)
	{
		Q_snprintf(buf, 64, "Current Num Particles:      %d", m_pEmitter->m_hEmitter->GetNumParticles());
		m_pNumParticlesLabel->SetText(buf);
	}
}

void CASW_VGUI_Edit_Emitter::AddSimpleNodeLabel(const char *pColumn, const char *pColumn2)
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(12);
	pPanel->SetPos(0,m_LayoutCursorY);

	vgui::Label *p = new vgui::Label(pPanel, "Edit_emitter_label", pColumn);
	vgui::Label *p2 = new vgui::Label(pPanel, "Edit_emitter_label", pColumn2);
	p->SetWide(60);
	p->SetTall(12);

	p2->SetWide(60);
	p2->SetTall(12);
	p->SetPos(84, 0);
	p2->SetPos(204, 0);

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 12;
}

void CASW_VGUI_Edit_Emitter::AddSliderAndTextEntry(	vgui::Slider*& pSlider, vgui::TextEntry*& pTextEntry, const char *pszSliderName,
										const char *pszTextEntryName, const char *pText, float minval, float maxval)
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(18);
	pPanel->SetPos(0,m_LayoutCursorY);

	pSlider = new vgui::Slider( pPanel, pszSliderName );	
	pSlider->SetRange( minval, maxval );
	pSlider->SetValue( minval );
	pSlider->AddActionSignalTarget( this );
		
	pTextEntry = new vgui::TextEntry(pPanel, pszTextEntryName);
    pTextEntry->AddActionSignalTarget(this);

	pSlider->SetWide(148);
	pSlider->SetTall(18);
	pTextEntry->SetWide(60);
	pTextEntry->SetTall(18);

	vgui::Label *p = new vgui::Label(pPanel, "Label", pText);
	p->SetWide(45);
	p->SetTall(18);
	p->SetPos(0,0);

	m_ListPanel->AddItem(NULL, pPanel);

	pSlider->SetPos(45, 0);
	pTextEntry->SetPos(200, 0); //m_LayoutCursorY);

	m_LayoutCursorY += 18;
}

void CASW_VGUI_Edit_Emitter::AddSimpleNode(	vgui::CheckButton*& pCheck, vgui::Slider*& pTime, vgui::Slider*& pValue, vgui::TextEntry*& pValueText, vgui::TextEntry*& pTimeText,
										   const char *pszCheckName, const char *pszTimeName, const char *pszValueName,
										   const char *pszTimeTextName, const char *pszValueTextName,
										   float minval, float maxval, float minval2, float maxval2)
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Node_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(18);
	pPanel->SetPos(0, m_LayoutCursorY);

	if (pszCheckName != NULL)
	{
		pCheck = new vgui::CheckButton( pPanel, pszCheckName, "Use:");
		pCheck->AddActionSignalTarget(this);
	}
	
	pTimeText = new vgui::TextEntry( pPanel, pszTimeTextName);
	pTimeText->AddActionSignalTarget(this);
	pTime = new vgui::Slider(pPanel, pszTimeName);
	pTime->AddActionSignalTarget(this);
	pTime->SetRange( minval, maxval );
	pTime->SetValue( minval );

	pValueText = new vgui::TextEntry( pPanel, pszValueTextName);
	pValueText->AddActionSignalTarget(this);
	pValue = new vgui::Slider(pPanel, pszValueName);
	pValue->AddActionSignalTarget(this);
	pValue->SetRange( minval2, maxval2 );
	pValue->SetValue( minval );
    
	
	pTime->SetTall(18);
	
	pTimeText->SetTall(18);
	
	pValueText->SetTall(18);
	
	pValue->SetTall(18);

	m_ListPanel->AddItem(NULL, pPanel);

	if (pszCheckName != NULL)
	{
		pCheck->SetPos(-7, -3);			pCheck->SetWide(20);
	}
	pTime->SetPos(20, 0);			pTime->SetWide(60);
	pTimeText->SetPos(84, 0);		pTimeText->SetWide(40);
	pValue->SetPos(140, 0);			pValue->SetWide(60);
	pValueText->SetPos(204, 0);		pValueText->SetWide(40);
	

	m_LayoutCursorY += 18;
}

void CASW_VGUI_Edit_Emitter::AddCheckbox(vgui::CheckButton*& pCheck, const char *pszCheckName)
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Node_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(18);
	pPanel->SetPos(0, m_LayoutCursorY);

	if (pszCheckName != NULL)
	{
		pCheck = new vgui::CheckButton( pPanel, pszCheckName, pszCheckName);
		pCheck->AddActionSignalTarget(this);
	}
	
	m_ListPanel->AddItem(NULL, pPanel);

	if (pszCheckName != NULL)
	{
		pCheck->SetPos(-7, -3);			pCheck->SetWide(200);
	}

	m_LayoutCursorY += 18;
}

void CASW_VGUI_Edit_Emitter::AddColorNode(	vgui::CheckButton*& pCheck, vgui::Slider*& pTime, vgui::TextEntry*& pTimeText,
											vgui::Slider*& pRed, vgui::Slider*& pGreen, vgui::Slider*& pBlue, 
											vgui::TextEntry*& pRedText, vgui::TextEntry*& pGreenText, vgui::TextEntry*& pBlueText,
										   const char *pszCheckName, const char *pszTimeName, const char *pszTimeTextName,
										   const char *pszRedName, const char *pszGreenName, const char *pszBlueName,
										   const char *pszRedTextName, const char *pszGreenTextName, const char *pszBlueTextName)
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Node_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(100);
	pPanel->SetPos(0, m_LayoutCursorY);

	pCheck = new vgui::CheckButton( pPanel, pszCheckName, "Use:");
	pCheck->AddActionSignalTarget(this);
	
	pTimeText = new vgui::TextEntry( pPanel, pszTimeTextName);
	pTimeText->AddActionSignalTarget(this);
	pTime = new vgui::Slider(pPanel, pszTimeName);
	pTime->AddActionSignalTarget(this);
	pTime->SetRange( 0.0f, 100.0f );
	pTime->SetValue( 0.0f );

	pRedText = new vgui::TextEntry( pPanel, pszRedTextName);
	pRedText->AddActionSignalTarget(this);
	pRed = new vgui::Slider(pPanel, pszRedName);
	pRed->AddActionSignalTarget(this);
	pRed->SetRange( 0, 255 );
	pRed->SetValue( 0 );
	pRed->SetTickCaptions("RED", NULL);

	pGreenText = new vgui::TextEntry( pPanel, pszGreenTextName);
	pGreenText->AddActionSignalTarget(this);
	pGreen = new vgui::Slider(pPanel, pszGreenName);
	pGreen->AddActionSignalTarget(this);
	pGreen->SetRange( 0, 255 );
	pGreen->SetValue( 0 );
	pRed->SetTickCaptions("GREEN", NULL);

	pBlueText = new vgui::TextEntry( pPanel, pszBlueTextName);
	pBlueText->AddActionSignalTarget(this);
	pBlue = new vgui::Slider(pPanel, pszBlueName);
	pBlue->AddActionSignalTarget(this);
	pBlue->SetRange( 0, 255 );
	pBlue->SetValue( 0 );
	pRed->SetTickCaptions("BLUE", NULL);
    			
	pTimeText->SetTall(18);
	pRedText->SetTall(18);
	pGreenText->SetTall(18);
	pBlueText->SetTall(18);
	pTime->SetTall(18);
	pRed->SetTall(18);
	pGreen->SetTall(18);
	pBlue->SetTall(18);

	vgui::Label *greenLabel = new vgui::Label(pPanel, "Edit_emitter_label", "Green");
	vgui::Label *blueLabel = new vgui::Label(pPanel, "Edit_emitter_label", "Blue");
	greenLabel->SetWide(60);
	greenLabel->SetTall(12);
	blueLabel->SetWide(60);
	blueLabel->SetTall(12);
	greenLabel->SetPos(204, 21);
	blueLabel->SetPos(204, 57);

	m_ListPanel->AddItem(NULL, pPanel);

	pCheck->SetPos(-7, -3);			pCheck->SetWide(20);
	pTime->SetPos(20, 0);			pTime->SetWide(60);
	pTimeText->SetPos(84, 0);		pTimeText->SetWide(40);
	pRed->SetPos(140, 0);			pRed->SetWide(60);
	pGreen->SetPos(140, 36);		pGreen->SetWide(60);
	pBlue->SetPos(140, 72);			pBlue->SetWide(60);
	pRedText->SetPos(204, 0);		pRedText->SetWide(40);
	pGreenText->SetPos(204, 36);	pGreenText->SetWide(40);
	pBlueText->SetPos(204, 72);		pBlueText->SetWide(40);

	m_LayoutCursorY += 100;
}

CASW_VGUI_Edit_Emitter_List_Item::CASW_VGUI_Edit_Emitter_List_Item(Panel *parent, const char *name) : Panel(parent, name)
{	
	
}

CASW_VGUI_Edit_Emitter_List_Item::~CASW_VGUI_Edit_Emitter_List_Item()
{
}

void CASW_VGUI_Edit_Emitter::SliderMoved(vgui::Panel* pSlider)
{
	if (bIgnoreNextSliderChange > 0)
	{
		bIgnoreNextSliderChange--;
		return;
	}

	char buf[64];
	Q_snprintf(buf, sizeof( buf ), "%sText", pSlider->GetName());	
	vgui::TextEntry* pTextEntry = (vgui::TextEntry*) pSlider->GetParent()->FindChildByName(buf);

	if (pTextEntry)
	{
		UpdateTextBox((vgui::Slider*) pSlider, pTextEntry);
	}

	if (m_pEmitter)
		ApplyValuesTo(m_pEmitter);
}

void CASW_VGUI_Edit_Emitter::CheckButtonChanged(vgui::Panel* pTextEntry)
{
	if (m_pEmitter)
		ApplyValuesTo(m_pEmitter);
}

void CASW_VGUI_Edit_Emitter::ResetButtonClicked()
{
	if (m_pEmitter)
		m_pEmitter->m_hEmitter->ResetEmitter();
}

void CASW_VGUI_Edit_Emitter::TextEntryChanged(vgui::Panel* pTextEntry)
{
	char buf[64];
	if (pTextEntry == m_pMaterialCombo)
	{
		if (m_pEmitter)
		{
			m_pMaterialCombo->GetText(buf, 64);
			m_pEmitter->m_hEmitter->SetMaterial(buf);
		}
		return;
	}
	else if (pTextEntry == m_pGlowMaterialCombo)
	{
		if (m_pEmitter)
		{
			m_pGlowMaterialCombo->GetText(buf, 64);
			m_pEmitter->m_hEmitter->SetGlowMaterial(buf);
		}
		return;
	}
	else if (pTextEntry == m_pTemplateCombo)
	{
		if (m_pEmitter && !bIgnoreTemplateComboChange)
		{
			m_pTemplateCombo->GetText(buf, 64);
			LoadTemplate(buf);
		}
		return;
	}
	else if (pTextEntry == m_pCollisionCombo)
	{
		m_pCollisionCombo->GetText(buf, 64);
		if (!stricmp(buf, "All"))
		{
			m_pEmitter->m_hEmitter->m_UseCollision = (ASWParticleCollision) 2;
		}
		else if (!stricmp(buf, "Brush Only"))
		{
			m_pEmitter->m_hEmitter->m_UseCollision = (ASWParticleCollision) 1;
		}
		else
		{
			m_pEmitter->m_hEmitter->m_UseCollision = (ASWParticleCollision) 0;
		}
	}
	else if (pTextEntry == m_pDrawTypeCombo)
	{
		m_pDrawTypeCombo->GetText(buf, 64);
		if (!stricmp(buf, "Sprite"))
		{
			m_pEmitter->m_hEmitter->m_DrawType = (ASWParticleDrawType) 0;
		}
		else if (!stricmp(buf, "Beam"))
		{
			m_pEmitter->m_hEmitter->m_DrawType = (ASWParticleDrawType) 1;
		}
	}
	else if (pTextEntry == m_pDropletTemplateCombo)
	{
		m_pDropletTemplateCombo->GetText(buf, 64);
		m_pEmitter->m_hEmitter->SetDropletTemplate(buf);
	}
	else if (pTextEntry == m_pCollisionSoundText)
	{
		m_pCollisionSoundText->GetText(buf, 64);
		m_pEmitter->m_hEmitter->SetCollisionSound(buf);
	}	
	else if (pTextEntry == m_pCollisionDecalText)
	{
		m_pCollisionDecalText->GetText(buf, 64);
		m_pEmitter->m_hEmitter->SetCollisionDecal(buf);
	}
	else if (pTextEntry == m_pCollisionTemplateCombo)
	{
		m_pCollisionTemplateCombo->GetText(buf, 64);
		m_pEmitter->m_hEmitter->SetCollisionTemplate(buf);
	}
	else if (pTextEntry == m_pBeamPositionCombo)
	{
		m_pBeamPositionCombo->GetText(buf, 64);
		if (!stricmp(buf, "Front"))
		{
			m_pEmitter->m_hEmitter->m_iBeamPosition = 2;
		}
		else if (!stricmp(buf, "Center"))
		{
			m_pEmitter->m_hEmitter->m_iBeamPosition = 1;
		}
		else
		{
			m_pEmitter->m_hEmitter->m_iBeamPosition = 0;
		}
	}
	else if (pTextEntry == m_pLightingCombo)
	{
		m_pLightingCombo->GetText(buf, 64);
		if (!stricmp(buf, "Scale Alpha"))
		{
			m_pEmitter->m_hEmitter->m_iLightingType = 2;
		}
		else if (!stricmp(buf, "Scale Color"))
		{
			m_pEmitter->m_hEmitter->m_iLightingType = 1;
		}
		else if (!stricmp(buf, "Scale Alpha+Color"))
		{
			m_pEmitter->m_hEmitter->m_iLightingType = 3;
		}
		else
		{
			m_pEmitter->m_hEmitter->m_iLightingType = 0;
		}
	}
	
	int i = strlen(pTextEntry->GetName());
	Q_strncpy(buf, pTextEntry->GetName(), i-3 );
	buf[i-4] = '\0';	
	vgui::Slider* pSlider = (vgui::Slider*) pTextEntry->GetParent()->FindChildByName(buf);
	
	if (pSlider)
	{
		char buf2[64];
		vgui::TextEntry* pT= (vgui::TextEntry*) pTextEntry;
		pT->GetText(buf2, 64);
		//int old = pSlider->GetValue();
		pSlider->SetValue(atoi(buf2), false);	
		//if (old != pSlider->GetValue())
			//bIgnoreNextSliderChange++;		
	}

	if (m_pEmitter)
		ApplyValuesTo(m_pEmitter);
}

void CASW_VGUI_Edit_Emitter::UpdateTextBox(vgui::Slider* pSlider, vgui::TextEntry* pTextEntry)
{
	char buf[64];
	//Q_snprintf(buf, sizeof( buf ), "%.2f", (float) pSlider->GetValue());
	Q_snprintf(buf, sizeof( buf ), "%d", pSlider->GetValue());
	pTextEntry->SetText(buf);
}

void CASW_VGUI_Edit_Emitter::SetEmitter(C_ASW_Emitter* pEmitter)
{
	m_pEmitter = pEmitter;
}

void CASW_VGUI_Edit_Emitter::SetTextEntry(float value, vgui::TextEntry* pTextEntry)
{
	char buf[64];
	if ((int) value == value)
		Q_snprintf(buf, sizeof(buf), "%d", (int) value);
	else
		Q_snprintf(buf, sizeof(buf), "%.2f", value);

	pTextEntry->SetText(buf);

	int i = strlen(pTextEntry->GetName());
	Q_strncpy(buf, pTextEntry->GetName(), i-3 );
	buf[i-4] = '\0';	
	vgui::Slider* pSlider = (vgui::Slider*) pTextEntry->GetParent()->FindChildByName(buf);
	
	if (pSlider)
	{
		char buf2[64];
		vgui::TextEntry* pT= (vgui::TextEntry*) pTextEntry;
		pT->GetText(buf2, 64);
		pSlider->SetValue(atoi(buf2), false);			
	}
}

void CASW_VGUI_Edit_Emitter::GetFromTextEntry(float& value, vgui::TextEntry* pTextEntry)
{
	char buf[64];
	pTextEntry->GetText(buf, 64);
	value = atof(buf);
}

void CASW_VGUI_Edit_Emitter::GetIntFromTextEntry(int& value, vgui::TextEntry* pTextEntry)
{
	char buf[64];
	pTextEntry->GetText(buf, 64);
	value = atoi(buf);
}

void CASW_VGUI_Edit_Emitter::GetByteFromTextEntry(byte& value, vgui::TextEntry* pTextEntry)
{
	char buf[64];
	pTextEntry->GetText(buf, 64);
	int b = atoi(buf);

	if (b>255)
		b = 255;

	value = b;
}

void CASW_VGUI_Edit_Emitter::SetCheckbox(bool b, vgui::CheckButton* pCheck)
{
	pCheck->SetSelected(b);
}

void CASW_VGUI_Edit_Emitter::GetFromCheckbox(bool& b, vgui::CheckButton* pCheck)
{
	b = pCheck->IsSelected();
}

// fill in all the text boxes with values from an actual emitter
void CASW_VGUI_Edit_Emitter::InitFrom(C_ASW_Emitter* pEmitter)
{
	SetTextEntry(pEmitter->m_hEmitter->m_ParticlesPerSecond, m_pParticlesPerSecondText);
	SetTextEntry(pEmitter->m_hEmitter->m_iInitialParticleSupply, m_pInitialParticleSupplyText);	
	SetTextEntry(pEmitter->m_hEmitter->m_fParticleLifeMin, m_pParticleLifetimeMinText);
	SetTextEntry(pEmitter->m_hEmitter->m_fParticleLifeMax, m_pParticleLifetimeMaxText);
	SetTextEntry(pEmitter->m_hEmitter->m_fPresimulateTime, m_pPresimulateTimeText);
	SetTextEntry(pEmitter->m_hEmitter->fRollMin, m_pParticleStartRollMinText);
	SetTextEntry(pEmitter->m_hEmitter->fRollMax, m_pParticleStartRollMaxText);
	SetTextEntry(pEmitter->m_hEmitter->fRollDeltaMin, m_pParticleRollRateMinText);
	SetTextEntry(pEmitter->m_hEmitter->fRollDeltaMax, m_pParticleRollRateMaxText);
	SetTextEntry(pEmitter->m_hEmitter->positionMin.x, m_pParticlePositionMinXText);
	SetTextEntry(pEmitter->m_hEmitter->positionMin.y, m_pParticlePositionMinYText);
	SetTextEntry(pEmitter->m_hEmitter->positionMin.z, m_pParticlePositionMinZText);
	SetTextEntry(pEmitter->m_hEmitter->positionMax.x, m_pParticlePositionMaxXText);
	SetTextEntry(pEmitter->m_hEmitter->positionMax.y, m_pParticlePositionMaxYText);
	SetTextEntry(pEmitter->m_hEmitter->positionMax.z, m_pParticlePositionMaxZText);
	SetTextEntry(pEmitter->m_hEmitter->velocityMin.x, m_pParticleVelocityMinXText);
	SetTextEntry(pEmitter->m_hEmitter->velocityMin.y, m_pParticleVelocityMinYText);
	SetTextEntry(pEmitter->m_hEmitter->velocityMin.z, m_pParticleVelocityMinZText);
	SetTextEntry(pEmitter->m_hEmitter->velocityMax.x, m_pParticleVelocityMaxXText);
	SetTextEntry(pEmitter->m_hEmitter->velocityMax.y, m_pParticleVelocityMaxYText);
	SetTextEntry(pEmitter->m_hEmitter->velocityMax.z, m_pParticleVelocityMaxZText);
	SetTextEntry(pEmitter->m_hEmitter->accelerationMin.x, m_pParticleAccnMinXText);
	SetTextEntry(pEmitter->m_hEmitter->accelerationMin.y, m_pParticleAccnMinYText);
	SetTextEntry(pEmitter->m_hEmitter->accelerationMin.z, m_pParticleAccnMinZText);
	SetTextEntry(pEmitter->m_hEmitter->accelerationMax.x, m_pParticleAccnMaxXText);
	SetTextEntry(pEmitter->m_hEmitter->accelerationMax.y, m_pParticleAccnMaxYText);
	SetTextEntry(pEmitter->m_hEmitter->accelerationMax.z, m_pParticleAccnMaxZText);
	SetTextEntry(pEmitter->m_hEmitter->m_fParticleLocal, m_pParticleLocalText);	
	SetTextEntry(pEmitter->m_hEmitter->m_fLightApply * 100.0f, m_pLightApplyText);	
	SetTextEntry(pEmitter->m_hEmitter->fGravity, m_pGravityText);
	SetTextEntry(pEmitter->m_hEmitter->m_fGlowScale, m_pGlowScaleText);
	SetTextEntry(pEmitter->m_hEmitter->m_fGlowDeviation, m_pGlowDeviationText);
	SetTextEntry(pEmitter->m_hEmitter->m_fBeamLength, m_pBeamLengthText);
	SetCheckbox(pEmitter->m_hEmitter->m_bScaleBeamByVelocity, m_pScaleBeamByVelocityCheck);
	SetCheckbox(pEmitter->m_hEmitter->m_bScaleBeamByLifeLeft, m_pScaleBeamByLifeLeftCheck);	
	SetTextEntry(pEmitter->m_hEmitter->m_fDropletChance, m_pDropletChanceText);	
	SetTextEntry(pEmitter->m_hEmitter->m_fLifeLostOnCollision, m_pLifeLostOnCollisionText);
	
	SetTextEntry(pEmitter->m_hEmitter->m_fCollisionDampening, m_pCollisionDampeningText);	
	
	m_pDropletTemplateCombo->SetText(pEmitter->m_hEmitter->GetDropletTemplate());
	m_pCollisionTemplateCombo->SetText(pEmitter->m_hEmitter->GetCollisionTemplate());
	
	// node check boxes
	SetCheckbox(pEmitter->m_hEmitter->m_Alphas[0].bUse, m_pAlpha0Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Alphas[1].bUse, m_pAlpha1Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Alphas[2].bUse, m_pAlpha2Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Alphas[3].bUse, m_pAlpha3Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Alphas[4].bUse, m_pAlpha4Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Scales[0].bUse, m_pScale0Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Scales[1].bUse, m_pScale1Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Scales[2].bUse, m_pScale2Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Scales[3].bUse, m_pScale3Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Scales[4].bUse, m_pScale4Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Colors[0].bUse, m_pColor0Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Colors[1].bUse, m_pColor1Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Colors[2].bUse, m_pColor2Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Colors[3].bUse, m_pColor3Check);
	SetCheckbox(pEmitter->m_hEmitter->m_Colors[4].bUse, m_pColor4Check);
	// node time
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[0].fTime, m_pAlpha0TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[1].fTime, m_pAlpha1TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[2].fTime, m_pAlpha2TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[3].fTime, m_pAlpha3TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[4].fTime, m_pAlpha4TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[0].fTime, m_pScale0TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[1].fTime, m_pScale1TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[2].fTime, m_pScale2TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[3].fTime, m_pScale3TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[4].fTime, m_pScale4TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[0].fTime, m_pColor0TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[1].fTime, m_pColor1TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[2].fTime, m_pColor2TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[3].fTime, m_pColor3TimeText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[4].fTime, m_pColor4TimeText);
	// simple node value
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[0].fAlpha, m_pAlpha0ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[1].fAlpha, m_pAlpha1ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[2].fAlpha, m_pAlpha2ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[3].fAlpha, m_pAlpha3ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Alphas[4].fAlpha, m_pAlpha4ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[0].fScale, m_pScale0ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[1].fScale, m_pScale1ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[2].fScale, m_pScale2ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[3].fScale, m_pScale3ValueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Scales[4].fScale, m_pScale4ValueText);
	// color node value
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[0].Color.r,   m_pColor0RedText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[0].Color.g, m_pColor0GreenText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[0].Color.b,  m_pColor0BlueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[1].Color.r,   m_pColor1RedText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[1].Color.g, m_pColor1GreenText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[1].Color.b,  m_pColor1BlueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[2].Color.r,   m_pColor2RedText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[2].Color.g, m_pColor2GreenText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[2].Color.b,  m_pColor2BlueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[3].Color.r,   m_pColor3RedText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[3].Color.g, m_pColor3GreenText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[3].Color.b,  m_pColor3BlueText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[4].Color.r,   m_pColor4RedText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[4].Color.g, m_pColor4GreenText);
	SetTextEntry(pEmitter->m_hEmitter->m_Colors[4].Color.b,  m_pColor4BlueText);
	//m_TemplateTextEntry->SetText(m_pEmitter->m_hEmitter->GetTemplateName());
	bIgnoreTemplateComboChange = true;
	m_pTemplateCombo->SetText(m_pEmitter->m_hEmitter->GetTemplateName());
	bIgnoreTemplateComboChange = false;
	// set material combo
	for (int i=0;i<iszEmitterTextureNames.Count();i++)
	{
		if (!strcmp(iszEmitterTextureNames[i], pEmitter->m_hEmitter->GetMaterial()))
		{
			m_pMaterialCombo->SetText(iszEmitterTextureNames[i]);
		}
	}
	// set glow material combo
	m_pGlowMaterialCombo->SetText("None");
	for (int i=0;i<iszEmitterTextureNames.Count();i++)
	{
		if (!strcmp(iszEmitterTextureNames[i], pEmitter->m_hEmitter->GetGlowMaterial()))
		{
			m_pGlowMaterialCombo->SetText(iszEmitterTextureNames[i]);
		}
	}
	// collision
	switch(pEmitter->m_hEmitter->m_UseCollision)
	{
		case 2: m_pCollisionCombo->SetText("All"); break;
		case 1: m_pCollisionCombo->SetText("Brush Only"); break;
		default: m_pCollisionCombo->SetText("None"); break;
	}
	m_pCollisionSoundText->SetText(pEmitter->m_hEmitter->m_szCollisionSoundName);
	m_pCollisionDecalText->SetText(pEmitter->m_hEmitter->m_szCollisionDecalName);
	// drawtype
	switch(pEmitter->m_hEmitter->m_DrawType)
	{
		case 1: m_pDrawTypeCombo->SetText("Beam"); break;		
		default: m_pDrawTypeCombo->SetText("Sprite"); break;
	}
	switch(pEmitter->m_hEmitter->m_iBeamPosition)
	{
		case 2: m_pBeamPositionCombo->SetText("Front"); break;	
		case 1: m_pBeamPositionCombo->SetText("Center"); break;	
		default: m_pBeamPositionCombo->SetText("Behind"); break;	
	}
	switch(pEmitter->m_hEmitter->m_iLightingType)
	{		
		case 3: m_pLightingCombo->SetText("Scale Alpha+Color"); break;	
		case 2: m_pLightingCombo->SetText("Scale Alpha"); break;	
		case 1: m_pLightingCombo->SetText("Scale Color"); break;	
		default: m_pLightingCombo->SetText("None"); break;	
	}
}

void CASW_VGUI_Edit_Emitter::ApplyValuesTo(C_ASW_Emitter* pEmitter)
{
	GetFromTextEntry(pEmitter->m_hEmitter->m_ParticlesPerSecond, m_pParticlesPerSecondText);
	GetIntFromTextEntry(pEmitter->m_hEmitter->m_iInitialParticleSupply, m_pInitialParticleSupplyText);		
	GetFromTextEntry(pEmitter->m_hEmitter->m_fParticleLifeMin, m_pParticleLifetimeMinText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_fParticleLifeMax, m_pParticleLifetimeMaxText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_fPresimulateTime, m_pPresimulateTimeText);	
	GetFromTextEntry(pEmitter->m_hEmitter->fRollMin, m_pParticleStartRollMinText);
	GetFromTextEntry(pEmitter->m_hEmitter->fRollMax, m_pParticleStartRollMaxText);
	GetFromTextEntry(pEmitter->m_hEmitter->fRollDeltaMin, m_pParticleRollRateMinText);
	GetFromTextEntry(pEmitter->m_hEmitter->fRollDeltaMax, m_pParticleRollRateMaxText);
	GetFromTextEntry(pEmitter->m_hEmitter->positionMin.x, m_pParticlePositionMinXText);
	GetFromTextEntry(pEmitter->m_hEmitter->positionMin.y, m_pParticlePositionMinYText);
	GetFromTextEntry(pEmitter->m_hEmitter->positionMin.z, m_pParticlePositionMinZText);
	GetFromTextEntry(pEmitter->m_hEmitter->positionMax.x, m_pParticlePositionMaxXText);
	GetFromTextEntry(pEmitter->m_hEmitter->positionMax.y, m_pParticlePositionMaxYText);
	GetFromTextEntry(pEmitter->m_hEmitter->positionMax.z, m_pParticlePositionMaxZText);
	GetFromTextEntry(pEmitter->m_hEmitter->velocityMin.x, m_pParticleVelocityMinXText);
	GetFromTextEntry(pEmitter->m_hEmitter->velocityMin.y, m_pParticleVelocityMinYText);
	GetFromTextEntry(pEmitter->m_hEmitter->velocityMin.z, m_pParticleVelocityMinZText);
	GetFromTextEntry(pEmitter->m_hEmitter->velocityMax.x, m_pParticleVelocityMaxXText);
	GetFromTextEntry(pEmitter->m_hEmitter->velocityMax.y, m_pParticleVelocityMaxYText);
	GetFromTextEntry(pEmitter->m_hEmitter->velocityMax.z, m_pParticleVelocityMaxZText);
	GetFromTextEntry(pEmitter->m_hEmitter->accelerationMin.x, m_pParticleAccnMinXText);
	GetFromTextEntry(pEmitter->m_hEmitter->accelerationMin.y, m_pParticleAccnMinYText);
	GetFromTextEntry(pEmitter->m_hEmitter->accelerationMin.z, m_pParticleAccnMinZText);
	GetFromTextEntry(pEmitter->m_hEmitter->accelerationMax.x, m_pParticleAccnMaxXText);
	GetFromTextEntry(pEmitter->m_hEmitter->accelerationMax.y, m_pParticleAccnMaxYText);
	GetFromTextEntry(pEmitter->m_hEmitter->accelerationMax.z, m_pParticleAccnMaxZText);	
	GetFromTextEntry(pEmitter->m_hEmitter->m_fParticleLocal, m_pParticleLocalText);	
	GetFromTextEntry(pEmitter->m_hEmitter->m_fLightApply, m_pLightApplyText);	
	pEmitter->m_hEmitter->m_fLightApply *= 0.01f;
	GetFromTextEntry(pEmitter->m_hEmitter->fGravity, m_pGravityText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_fGlowScale, m_pGlowScaleText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_fGlowDeviation, m_pGlowDeviationText);	
	GetFromTextEntry(pEmitter->m_hEmitter->m_fBeamLength, m_pBeamLengthText);
	GetFromCheckbox(pEmitter->m_hEmitter->m_bScaleBeamByVelocity, m_pScaleBeamByVelocityCheck);
	GetFromCheckbox(pEmitter->m_hEmitter->m_bScaleBeamByLifeLeft, m_pScaleBeamByLifeLeftCheck);	
	GetFromTextEntry(pEmitter->m_hEmitter->m_fDropletChance, m_pDropletChanceText);	
	GetFromTextEntry(pEmitter->m_hEmitter->m_fLifeLostOnCollision, m_pLifeLostOnCollisionText);	
	
	GetFromTextEntry(pEmitter->m_hEmitter->m_fCollisionDampening, m_pCollisionDampeningText);	
	
	// node check boxes
	GetFromCheckbox(pEmitter->m_hEmitter->m_Alphas[0].bUse, m_pAlpha0Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Alphas[1].bUse, m_pAlpha1Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Alphas[2].bUse, m_pAlpha2Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Alphas[3].bUse, m_pAlpha3Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Alphas[4].bUse, m_pAlpha4Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Scales[0].bUse, m_pScale0Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Scales[1].bUse, m_pScale1Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Scales[2].bUse, m_pScale2Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Scales[3].bUse, m_pScale3Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Scales[4].bUse, m_pScale4Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Colors[0].bUse, m_pColor0Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Colors[1].bUse, m_pColor1Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Colors[2].bUse, m_pColor2Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Colors[3].bUse, m_pColor3Check);
	GetFromCheckbox(pEmitter->m_hEmitter->m_Colors[4].bUse, m_pColor4Check);
	// node time
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[0].fTime, m_pAlpha0TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[1].fTime, m_pAlpha1TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[2].fTime, m_pAlpha2TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[3].fTime, m_pAlpha3TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[4].fTime, m_pAlpha4TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[0].fTime, m_pScale0TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[1].fTime, m_pScale1TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[2].fTime, m_pScale2TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[3].fTime, m_pScale3TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[4].fTime, m_pScale4TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Colors[0].fTime, m_pColor0TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Colors[1].fTime, m_pColor1TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Colors[2].fTime, m_pColor2TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Colors[3].fTime, m_pColor3TimeText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Colors[4].fTime, m_pColor4TimeText);
	// simple node value
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[0].fAlpha, m_pAlpha0ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[1].fAlpha, m_pAlpha1ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[2].fAlpha, m_pAlpha2ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[3].fAlpha, m_pAlpha3ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Alphas[4].fAlpha, m_pAlpha4ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[0].fScale, m_pScale0ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[1].fScale, m_pScale1ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[2].fScale, m_pScale2ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[3].fScale, m_pScale3ValueText);
	GetFromTextEntry(pEmitter->m_hEmitter->m_Scales[4].fScale, m_pScale4ValueText);
	// color node value
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[0].Color.r,   m_pColor0RedText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[0].Color.g, m_pColor0GreenText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[0].Color.b,  m_pColor0BlueText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[1].Color.r,   m_pColor1RedText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[1].Color.g, m_pColor1GreenText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[1].Color.b,  m_pColor1BlueText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[2].Color.r,   m_pColor2RedText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[2].Color.g, m_pColor2GreenText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[2].Color.b,  m_pColor2BlueText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[3].Color.r,   m_pColor3RedText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[3].Color.g, m_pColor3GreenText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[3].Color.b,  m_pColor3BlueText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[4].Color.r,   m_pColor4RedText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[4].Color.g, m_pColor4GreenText);
	GetByteFromTextEntry(pEmitter->m_hEmitter->m_Colors[4].Color.b,  m_pColor4BlueText);
	//m_TemplateTextEntry->GetText(pEmitter->m_hEmitter->m_szTemplateName, MAX_PATH);

	pEmitter->m_hEmitter->Update();
}

// load the texture strings in for our drop down texture selection box
void CASW_VGUI_Edit_Emitter::LoadTextures()
{
	KeyValues* m_TextureKeyValues = new KeyValues( "ParticleEmitters" );

	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/ParticleEmitters.txt" );
	
	if ( !m_TextureKeyValues->LoadFromFile( filesystem, tempfile, "GAME" ) )
	{
		DevMsg( 1, "CASW_VGUI_Edit_Emitter::LoadTextures: couldn't load file: %s\n", tempfile );
		return;
	}

	KeyValues* kv = m_TextureKeyValues->GetFirstValue();
	while (kv)
	{
		string_t pooledName = AllocPooledString( kv->GetString() );
		iszEmitterTextureNames.AddToTail( pooledName );
		kv = kv->GetNextValue();
	}
}

void CASW_VGUI_Edit_Emitter::AddMaterialDropDown()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(20);
	pPanel->SetPos(0,m_LayoutCursorY);

	m_pMaterialCombo = new vgui::ComboBox(pPanel, "MaterialCombo",iszEmitterTextureNames.Count(),false);
	m_pMaterialCombo->SetTall(20);
	m_pMaterialCombo->SetWide(200);
	m_pMaterialCombo->SetPos(20,0);
	m_pMaterialCombo->AddActionSignalTarget(this);
	for (int i=0; i<iszEmitterTextureNames.Count();i++)
	{
		m_pMaterialCombo->AddItem(iszEmitterTextureNames[i], NULL);
	}

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::AddGlowMaterialDropDown()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(20);
	pPanel->SetPos(0,m_LayoutCursorY);

	m_pGlowMaterialCombo = new vgui::ComboBox(pPanel, "GlowMaterialCombo",iszEmitterTextureNames.Count()+1,false);
	m_pGlowMaterialCombo->SetTall(20);
	m_pGlowMaterialCombo->SetWide(200);
	m_pGlowMaterialCombo->SetPos(20,0);
	m_pGlowMaterialCombo->AddActionSignalTarget(this);
	m_pGlowMaterialCombo->AddItem("None", NULL);
	for (int i=0; i<iszEmitterTextureNames.Count();i++)
	{
		m_pGlowMaterialCombo->AddItem(iszEmitterTextureNames[i], NULL);
	}

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::AddCollisionSoundEdit()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(20);
	pPanel->SetPos(0,m_LayoutCursorY);

	m_pCollisionSoundText = new vgui::TextEntry(pPanel, "Edit_Emitter_CollisiOn_Sound_edit");
	m_pCollisionSoundText->SetTall(20);
	m_pCollisionSoundText->SetWide(200);
	m_pCollisionSoundText->SetPos(20,0);
	m_pCollisionSoundText->AddActionSignalTarget(this);

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::AddCollisionDecalEdit()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(20);
	pPanel->SetPos(0,m_LayoutCursorY);

	m_pCollisionDecalText = new vgui::TextEntry(pPanel, "Edit_Emitter_CollisiOn_Decal_edit");
	m_pCollisionDecalText->SetTall(20);
	m_pCollisionDecalText->SetWide(200);
	m_pCollisionDecalText->SetPos(20,0);
	m_pCollisionDecalText->AddActionSignalTarget(this);

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::AddCollisionDropDown()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(20);
	pPanel->SetPos(0,m_LayoutCursorY);

	m_pCollisionCombo = new vgui::ComboBox(pPanel, "CollisionCombo",3,false);
	m_pCollisionCombo->SetTall(20);
	m_pCollisionCombo->SetWide(200);
	m_pCollisionCombo->SetPos(20,0);
	m_pCollisionCombo->AddActionSignalTarget(this);
	m_pCollisionCombo->AddItem("None", NULL);
	m_pCollisionCombo->AddItem("Brush Only", NULL);
	m_pCollisionCombo->AddItem("All", NULL);

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::AddLightingDropDown()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(20);
	pPanel->SetPos(0,m_LayoutCursorY);

	m_pLightingCombo = new vgui::ComboBox(pPanel, "lightingcombo",3,false);
	m_pLightingCombo->SetTall(20);
	m_pLightingCombo->SetWide(200);
	m_pLightingCombo->SetPos(20,0);
	m_pLightingCombo->AddActionSignalTarget(this);
	m_pLightingCombo->AddItem("None", NULL);
	m_pLightingCombo->AddItem("Scale Color", NULL);
	m_pLightingCombo->AddItem("Scale Alpha", NULL);
	m_pLightingCombo->AddItem("Scale Alpha+Color", NULL);	

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::AddBeamPositionDropDown()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(20);
	pPanel->SetPos(0,m_LayoutCursorY);

	m_pBeamPositionCombo = new vgui::ComboBox(pPanel, "beamposcombo",3,false);
	m_pBeamPositionCombo->SetTall(20);
	m_pBeamPositionCombo->SetWide(200);
	m_pBeamPositionCombo->SetPos(20,0);
	m_pBeamPositionCombo->AddActionSignalTarget(this);
	m_pBeamPositionCombo->AddItem("Behind", NULL);
	m_pBeamPositionCombo->AddItem("Center", NULL);
	m_pBeamPositionCombo->AddItem("Front", NULL);

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}


void CASW_VGUI_Edit_Emitter::AddDrawTypeDropDown()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(220);
	pPanel->SetTall(20);
	pPanel->SetPos(0,m_LayoutCursorY);

	m_pDrawTypeCombo = new vgui::ComboBox(pPanel, "DrawTypeCombo",2,false);
	m_pDrawTypeCombo->SetTall(20);
	m_pDrawTypeCombo->SetWide(200);
	m_pDrawTypeCombo->SetPos(20,0);
	m_pDrawTypeCombo->AddActionSignalTarget(this);
	m_pDrawTypeCombo->AddItem("Sprite", NULL);
	m_pDrawTypeCombo->AddItem("Beam", NULL);	

	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::AddDropletTemplateDropDown()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(260);
	pPanel->SetTall(20);
	pPanel->SetPos(0, m_LayoutCursorY);

	m_pDropletTemplateCombo = new vgui::ComboBox(pPanel, "DropletTemplateCombo",iszTemplateNames.Count()+1,false);
	m_pDropletTemplateCombo->SetTall(20);
	m_pDropletTemplateCombo->SetWide(200);
	m_pDropletTemplateCombo->SetPos(20,0);
	m_pDropletTemplateCombo->AddActionSignalTarget(this);
	m_pDropletTemplateCombo->AddItem("None", NULL);
	for (int i=0; i<iszTemplateNames.Count();i++)
	{
		m_pDropletTemplateCombo->AddItem(iszTemplateNames[i], NULL);
	}
	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::AddCollisionTemplateDropDown()
{
	CASW_VGUI_Edit_Emitter_List_Item* pPanel = new CASW_VGUI_Edit_Emitter_List_Item(m_ListPanel, "Edit_Emitter_List_Item");
	pPanel->SetWide(260);
	pPanel->SetTall(20);
	pPanel->SetPos(0, m_LayoutCursorY);

	m_pCollisionTemplateCombo = new vgui::ComboBox(pPanel, "CollisionTemplateCombo",iszTemplateNames.Count()+1,false);
	m_pCollisionTemplateCombo->SetTall(20);
	m_pCollisionTemplateCombo->SetWide(200);
	m_pCollisionTemplateCombo->SetPos(20,0);
	m_pCollisionTemplateCombo->AddActionSignalTarget(this);
	m_pCollisionTemplateCombo->AddItem("None", NULL);
	for (int i=0; i<iszTemplateNames.Count();i++)
	{
		m_pCollisionTemplateCombo->AddItem(iszTemplateNames[i], NULL);
	}
	m_ListPanel->AddItem(NULL, pPanel);

	m_LayoutCursorY += 20;
}

void CASW_VGUI_Edit_Emitter::OnThink()
{
	BaseClass::OnThink();
	UpdateNumParticlesLabel();
}



void CASW_VGUI_Edit_Emitter::TemplateSaveButtonClicked()
{
	bNeedSave = false;

	if (m_pSaveDialog == NULL)
	{
		m_pSaveDialog = new CASW_VGUI_Edit_Emitter_SaveDialog(this, "EmitterSaveDialog", this);
		m_pSaveDialog->SetScheme(vgui::scheme()->LoadSchemeFromFile("resource/SwarmFrameScheme.res", "SwarmFrameScheme"));	
		m_pSaveDialog->Activate();
	}
}
/*
void CASW_VGUI_Edit_Emitter::TemplateLoadButtonClicked()
{

}

void CASW_VGUI_Edit_Emitter::TemplateSaveAsButtonClicked()
{

}*/

void CASW_VGUI_Edit_Emitter::BuildTemplateList()
{
	iszTemplateNames.Purge();

	// Search the directory structure.
	char templatewild[MAX_PATH];
	Q_strncpy(templatewild,"resource/particletemplates/*.ptm", sizeof( templatewild ) );	
	FileFindHandle_t findHandle;
	char const *findfn = filesystem->FindFirst(templatewild, &findHandle);	
	while ( findfn )
	{
		char sz[ MAX_PATH ];
		Q_strncpy(sz, findfn, strlen(findfn)-3);
		sz[strlen(findfn)-3]='\0';
	
		string_t pooledName = AllocPooledString( sz );
		iszTemplateNames.AddToTail( pooledName );

		findfn = filesystem->FindNext( findHandle );
	}

	filesystem->FindClose(findHandle);	
}

void CASW_VGUI_Edit_Emitter::LoadTemplate(const char* pTemplateName)
{
	m_pEmitter->UseTemplate(pTemplateName,false);
	InitFrom(m_pEmitter);
	bNeedSave = false;
}

void CASW_VGUI_Edit_Emitter::SaveDialogClosed()
{
	m_pSaveDialog = NULL;
}

void CASW_VGUI_Edit_Emitter::SaveDialogSave(const char *pTemplateName)
{
	// add it to the template list if need be
	bool bFound = false;
	for (int i=0;i<iszTemplateNames.Count();i++)
	{
		if (!strcmp(iszTemplateNames[i], pTemplateName))
		{
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		string_t pooledName = AllocPooledString( pTemplateName );
		iszTemplateNames.AddToTail( pooledName );
	}
	// resetup the combo box and set its text to the current
	m_pTemplateCombo->RemoveAll();
	for (int i=0; i<iszTemplateNames.Count();i++)
	{
		m_pTemplateCombo->AddItem(iszTemplateNames[i], NULL);
	}
	m_pTemplateCombo->SetText(pTemplateName);
	// tell the emitter to save itself under this name and change its template name
	m_pEmitter->SaveAsTemplate(pTemplateName);
}