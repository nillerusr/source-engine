#ifndef _DEFINED_ASW_VGUI_EDIT_EMITTER_H
#define _DEFINED_ASW_VGUI_EDIT_EMITTER_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Slider.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ComboBox.h"

class C_ASW_Emitter;
class CASW_VGUI_Edit_Emitter_SaveDialog;

#define ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(name) \
	vgui::Slider* m_p##name##Slider;	\
	vgui::TextEntry* m_p##name##Text;
	
#define ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(name) \
	vgui::CheckButton* m_p##name##Check; \
	vgui::Slider* m_p##name##Time; \
	vgui::Slider* m_p##name##Value; \
	vgui::TextEntry* m_p##name##ValueText; \
	vgui::TextEntry* m_p##name##TimeText

#define ASW_VGUI_EDIT_EMITTER_DECLARE_COLOR_NODE(name) \
	vgui::CheckButton* m_p##name##Check; \
	vgui::Slider* m_p##name##Time; \
	vgui::Slider* m_p##name##Red; \
	vgui::Slider* m_p##name##Green; \
	vgui::Slider* m_p##name##Blue; \
	vgui::TextEntry* m_p##name##RedText; \
	vgui::TextEntry* m_p##name##GreenText; \
	vgui::TextEntry* m_p##name##BlueText; \
	vgui::TextEntry* m_p##name##TimeText


class CASW_VGUI_Edit_Emitter_List_Item : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Edit_Emitter_List_Item, Panel );
public:
	CASW_VGUI_Edit_Emitter_List_Item(Panel *parent, const char *name);
	virtual ~CASW_VGUI_Edit_Emitter_List_Item();
};

// this frame shows all the controls for editing a particle emitter

class CASW_VGUI_Edit_Emitter : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Edit_Emitter, vgui::Frame );

public:
	CASW_VGUI_Edit_Emitter( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_VGUI_Edit_Emitter();
	virtual void OnThink();
	//==== Creating controls =======================
	void AddSliderAndTextEntry(	vgui::Slider*& pSlider, vgui::TextEntry*& pTextEntry, const char *pszSliderName,
										const char *pszTextEntryName, const char *pText, float minval, float maxval);
	void AddSimpleNode(	vgui::CheckButton*& pCheck, vgui::Slider*& pTime, vgui::Slider*& pValue, vgui::TextEntry*& pValueText, vgui::TextEntry*& pTimeText,
										   const char *pszCheckName, const char *pszTimeName, const char *pszValueName,
										   const char *pszTimeTextName, const char *pszValueTextName,
										   float minval, float maxval, float minval2, float maxval2);
	void AddColorNode(	vgui::CheckButton*& pCheck, vgui::Slider*& pTime, vgui::TextEntry*& pTimeText,
											vgui::Slider*& pRed, vgui::Slider*& pGreen, vgui::Slider*& pBlue, 
											vgui::TextEntry*& pRedText, vgui::TextEntry*& pGreenText, vgui::TextEntry*& pBlueText,
										   const char *pszCheckName, const char *pszTimeName, const char *pszTimeTextName,
										   const char *pszRedName, const char *pszGreenName, const char *pszBlueName,
										   const char *pszRedTextName, const char *pszGreenTextName, const char *pszBlueTextName);
	void AddCheckbox(	vgui::CheckButton*& pCheck, const char *pszCheckName);
	void AddLabel(const char *pText);	
	void AddSimpleNodeLabel(const char *pColumn, const char *pColumn2);
	void AddMaterialDropDown();
	void AddGlowMaterialDropDown();
	void AddCollisionDropDown();
	void AddDrawTypeDropDown();
	void AddDropletTemplateDropDown();
	void AddCollisionTemplateDropDown();
	void AddBeamPositionDropDown();
	void AddLightingDropDown();
	void AddNumParticlesLabel();
	void AddTemplateBoxAndButtons();
	void UpdateNumParticlesLabel();
	void BuildTemplateList();
	void LoadTemplate(const char* pTemplateName);
	//==== Control changes =========================
	void UpdateTextBox(vgui::Slider* pSlider, vgui::TextEntry* pTextEntry);
	MESSAGE_FUNC_PTR( SliderMoved, "SliderMoved", panel );
	MESSAGE_FUNC_PTR( TextEntryChanged, "TextChanged", panel );
	MESSAGE_FUNC_PTR( CheckButtonChanged, "CheckButtonChecked", panel );
	MESSAGE_FUNC( ResetButtonClicked, "ResetEmitter" );
	MESSAGE_FUNC( TemplateSaveButtonClicked, "SaveTemplate" );
	//MESSAGE_FUNC( TemplateSaveAsButtonClicked, "SaveTemplateAs" );
	//MESSAGE_FUNC( TemplateLoadButtonClicked, "LoadTemplate" );
	//==== Getting/Setting from an emitter =========
	void SetEmitter(C_ASW_Emitter* pEmitter);
	void InitFrom(C_ASW_Emitter* pEmitter);	
	void ApplyValuesTo(C_ASW_Emitter* pEmitter);
	void SetTextEntry(float value, vgui::TextEntry* pTextEntry);		// fills in the text entry box with value
	void GetFromTextEntry(float& value, vgui::TextEntry* pTextEntry);	// sets the value var to the contents of the textentry
	void GetIntFromTextEntry(int& value, vgui::TextEntry* pTextEntry);
	void GetByteFromTextEntry(byte& value, vgui::TextEntry* pTextEntry);
	void SetCheckbox(bool b, vgui::CheckButton* pCheck);
	void GetFromCheckbox(bool& b, vgui::CheckButton* pCheck);
	void LoadTextures();
	C_ASW_Emitter* m_pEmitter;

	vgui::PanelListPanel *m_ListPanel;
	vgui::ComboBox* m_pMaterialCombo;
	vgui::ComboBox* m_pGlowMaterialCombo;
	vgui::Label* m_pNumParticlesLabel;
	int bIgnoreNextSliderChange;
	vgui::Button* m_pResetButton;
	//vgui::TextEntry* m_TemplateTextEntry;
	vgui::ComboBox* m_pTemplateCombo;	
	vgui::ComboBox* m_pDropletTemplateCombo;
	vgui::ComboBox* m_pCollisionTemplateCombo;
	//vgui::Button* m_pTemplateLoadButton;
	vgui::Button* m_pTemplateSaveButton;
	//vgui::Button* m_pTemplateSaveAsButton;

	//==== Declare control pointers =========
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticlesPerSecond);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(InitialParticleSupply);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleLifetimeMin);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleLifetimeMax);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(PresimulateTime);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleStartRollMin);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleStartRollMax);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleRollRateMin);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleRollRateMax);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticlePositionMinX);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticlePositionMinY);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticlePositionMinZ);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticlePositionMaxX);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticlePositionMaxY);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticlePositionMaxZ);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleVelocityMinX);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleVelocityMinY);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleVelocityMinZ);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleVelocityMaxX);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleVelocityMaxY);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleVelocityMaxZ);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleAccnMinX);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleAccnMinY);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleAccnMinZ);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleAccnMaxX);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleAccnMaxY);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleAccnMaxZ);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(Gravity);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(GlowDeviation);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(GlowScale);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(BeamLength);
	vgui::CheckButton* m_pScaleBeamByVelocityCheck;
	vgui::CheckButton* m_pScaleBeamByLifeLeftCheck;
	vgui::ComboBox* m_pBeamPositionCombo;
	vgui::ComboBox* m_pLightingCombo;
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(LightApply);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(ParticleLocal);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(DropletChance);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(LifeLostOnCollision);	
	ASW_VGUI_EDIT_EMITTER_DECLARE_SLIDER(CollisionDampening);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Alpha0);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Alpha1);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Alpha2);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Alpha3);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Alpha4);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Scale0);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Scale1);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Scale2);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Scale3);
	ASW_VGUI_EDIT_EMITTER_DECLARE_SIMPLE_NODE(Scale4);
	ASW_VGUI_EDIT_EMITTER_DECLARE_COLOR_NODE(Color0);
	ASW_VGUI_EDIT_EMITTER_DECLARE_COLOR_NODE(Color1);
	ASW_VGUI_EDIT_EMITTER_DECLARE_COLOR_NODE(Color2);
	ASW_VGUI_EDIT_EMITTER_DECLARE_COLOR_NODE(Color3);
	ASW_VGUI_EDIT_EMITTER_DECLARE_COLOR_NODE(Color4);
	vgui::ComboBox* m_pCollisionCombo;
	vgui::ComboBox* m_pDrawTypeCombo;
	void AddCollisionSoundEdit();
	void AddCollisionDecalEdit();
	vgui::TextEntry* m_pCollisionDecalText;
	vgui::TextEntry* m_pCollisionSoundText;

	float m_LayoutCursorY;	
	CUtlVector<string_t>	iszEmitterTextureNames;
	CUtlVector<string_t>	iszTemplateNames;
	bool bNeedSave;	// set to true when emitter controls are changed.  set to false when a save occurs
	bool bIgnoreTemplateComboChange;
	CASW_VGUI_Edit_Emitter_SaveDialog* m_pSaveDialog;
	void SaveDialogClosed();
	void SaveDialogSave(const char *pTemplateName);
};


#endif /* _DEFINED_ASW_VGUI_EDIT_EMITTER_H */