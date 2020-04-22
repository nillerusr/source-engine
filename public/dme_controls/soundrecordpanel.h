//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef SOUNDRECORDPANEL_H
#define SOUNDRECORDPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Button;
}


//-----------------------------------------------------------------------------
// Purpose: Modal sound picker window
//-----------------------------------------------------------------------------
class CSoundRecordPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CSoundRecordPanel, vgui::Frame );

public:
	CSoundRecordPanel( vgui::Panel *pParent, const char *pTitle );
	~CSoundRecordPanel();

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );
	virtual void OnTick();

	// Purpose: Activate the dialog
	// The message "SoundRecorded" will be sent if a sound is recorded
	void DoModal( const char *pFileName );

private:
	void StopSoundPreview( );
	void PlaySoundPreview( );

	// Updates sound record time during recording
	void UpdateTimeRecorded();

	vgui::Button *m_pRecordButton;
	vgui::Button *m_pPlayButton;
	vgui::Button *m_pOkButton;
	vgui::Button *m_pCancelButton;
	vgui::TextEntry *m_pRecordTime;
	vgui::TextEntry *m_pFileName;
	CUtlString m_FileName;
	CUtlString m_EngineFileName;
	int m_nPlayingSound;
	float m_flRecordStartTime;
	bool m_bIsRecording;
};


#endif // SOUNDRECORDPANEL_H
