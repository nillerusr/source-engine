#ifndef _DEFINED_C_ASW_BUTTON_AREA_H
#define _DEFINED_C_ASW_BUTTON_AREA_H

#include "c_asw_use_area.h"

class C_ASW_Door;

class C_ASW_Button_Area : public C_ASW_Use_Area
{
	DECLARE_CLASS( C_ASW_Button_Area, C_ASW_Use_Area );
	DECLARE_CLIENTCLASS();
public:
	C_ASW_Button_Area();

	bool IsLocked() { return m_bIsLocked; }
	int GetHackLevel() { return m_iHackLevel; }
	bool IsDoorButton() { return m_bIsDoorButton; }
	C_ASW_Door* GetDoor();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_BUTTON_PANEL; }
	
	// accessors for icons
	int GetLockedIconTextureID();
	const char* GetLockedIconText() { return "#asw_requires_tech"; }
	int GetOpenIconTextureID();
	const char* GetOpenIconText() { return "#asw_open"; }
	int GetCloseIconTextureID();
	const char* GetCloseIconText() { return "#asw_close"; }
	int GetUseIconTextureID();
	const char* GetUseIconText() { return "#asw_use_panel"; }
	int GetHackIconTextureID();
	const char* GetHackIconText(C_ASW_Marine *pUser);
	int GetNoPowerIconTextureID();
	const char* GetNoPowerText();

	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser);
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon ) { }
	virtual C_BaseEntity* GetGlowEntity() { return m_hPanelProp.Get(); }

	// traditional Swarm hacking
	float GetHackProgress() { return m_fHackProgress; }
	CNetworkVar(bool, m_bIsInUse);
	CNetworkVar(float, m_fHackProgress);
	
	virtual const char* GetNoPowerMessage() { return m_NoPowerMessage; }
	char		m_NoPowerMessage[255];	

	bool HasPower() { return !m_bNoPower; }
	bool IsWaitingForInput( void ) const { return m_bWaitingForInput; }
	
protected:
	bool m_bIsLocked;
	bool m_bNoPower;
	bool m_bWaitingForInput;
	bool m_bOldWaitingForInput;
	int m_iHackLevel;
	bool m_bIsDoorButton;
	C_ASW_Button_Area( const C_ASW_Button_Area & ); // not defined, not accessible		

	// icons used to interact with buttons
	static bool s_bLoadedLockedIconTexture;
	static int s_nLockedIconTextureID;

	static bool s_bLoadedOpenIconTexture;
	static int s_nOpenIconTextureID;

	static bool s_bLoadedCloseIconTexture;
	static int s_nCloseIconTextureID;

	static bool s_bLoadedUseIconTexture;
	static int s_nUseIconTextureID;

	static bool s_bLoadedHackIconTexture;
	static int s_nHackIconTextureID;

	static bool s_bLoadedNoPowerIconTexture;
	static int s_nNoPowerIconTextureID;	
};

#endif /* _DEFINED_C_ASW_BUTTON_AREA_H */