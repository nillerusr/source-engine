//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef RADIO_STATUS_H
#define RADIO_STATUS_H
#pragma once

class IMaterial;

class CRadioStatus : public CAutoGameSystem
{
public:
	CRadioStatus();

	virtual bool Init();
	virtual void Shutdown();

	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();
	
public:

	// Called when a player uses the radio
	void	UpdateRadioStatus(int entindex, float duration);

	// Called when a player (bot) speaks a wav file
	void	UpdateVoiceStatus(int entindex, float duration);

	// Call from the HUD_CreateEntities function so it can add sprites above player heads.
	void	DrawHeadLabels();

private:


	float		m_radioUntil[MAX_PLAYERS];	// Who is currently talking. Indexed by client index.
	IMaterial	*m_pHeadLabelMaterial;		// For labels above players' heads.

	void		ExpireBotVoice( bool force = false );
	float		m_voiceUntil[MAX_PLAYERS];	// Who is currently talking. Indexed by client index.
};


// Get the (global) voice manager. 
CRadioStatus* RadioManager();


#endif // RADIO_STATUS_H
