#ifndef C_NPC_CrabSynth_H
#define C_NPC_CrabSynth_H
#ifdef _WIN32
#pragma once
#endif

#include "c_ai_basenpc.h"
#include "flashlighteffect.h"

//-----------------------------------------------------------------------------
// Crab Synth
//-----------------------------------------------------------------------------
class C_NPC_CrabSynth : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_CrabSynth, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	C_NPC_CrabSynth();
	~C_NPC_CrabSynth();

	void OnDataChanged( DataUpdateType_t type );
	void UpdateLight( CCrabSynthLightEffect **ppLight, int nAttachment );
	void Simulate( void );

	int m_nAttachmentLightL = -1;
	int m_nAttachmentLightR = -1;

	bool m_bProjectedLightEnabled;
	bool m_bProjectedLightShadowsEnabled;
	float m_flProjectedLightBrightnessScale;
	float m_flProjectedLightFOV;
	float m_flProjectedLightHorzFOV;

	CCrabSynthLightEffect *m_LightL;
	CCrabSynthLightEffect *m_LightR;
};

#endif // C_NPC_CrabSynth_H