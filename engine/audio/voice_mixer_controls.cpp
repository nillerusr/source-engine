//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "audio_pch.h"
#include "voice_mixer_controls.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// NOTE: Vista deprecated these APIs
// Under vista these settings are per-session (not persistent)
// The correct method is to use the AudioEndpoint COM objects to manage controls like this
// The interface is not 1:1 so for now we'll just back the state with convars and reapply it
// on init of the mixer controls.  In the future when XP is no longer the majority of our user base
// we should revisit this and move to the new API.
// http://msdn.microsoft.com/en-us/library/aa964574(VS.85).aspx


class CMixerControls : public IMixerControls
{
public:
					CMixerControls();
	virtual			~CMixerControls();

	virtual bool	GetValue_Float(Control iControl, float &value);
	virtual bool	SetValue_Float(Control iControl, float value);
	virtual bool	SelectMicrophoneForWaveInput();


private:
	bool			Init();
	void			Term();

	void			Clear();

	bool			GetControlOption_Bool(DWORD dwControlID, DWORD cMultipleItems, bool &bValue);
	bool			SetControlOption_Bool(DWORD dwControlID, DWORD cMultipleItems, const bool bValue);

	bool			GetControlOption_Unsigned(DWORD dwControlID, DWORD cMultipleItems, DWORD &value);
	bool			SetControlOption_Unsigned(DWORD dwControlID, DWORD cMultipleItems, const DWORD value);

	bool			GetLineControls( DWORD dwLineID, MIXERCONTROL *controls, DWORD nControls );
	void			FindMicSelectControl( DWORD dwLineID, DWORD nControls );
	

private:
	HMIXER			m_hMixer;

	class ControlInfo
	{
	public:
		DWORD	m_dwControlID;
		DWORD	m_cMultipleItems;
		bool	m_bFound;
	};

	DWORD			m_dwMicSelectControlID;
	DWORD			m_dwMicSelectMultipleItems;
	DWORD			m_dwMicSelectControlType;
	DWORD			m_dwMicSelectIndex;
	
	// Info about the controls we found.
	ControlInfo		m_ControlInfos[NumControls];
};



CMixerControls::CMixerControls()
{
	m_dwMicSelectControlID		= 0xFFFFFFFF;

	Clear();
	Init();
}

CMixerControls::~CMixerControls()
{
	Term();
}

bool CMixerControls::Init()
{
	Term();


	MMRESULT mmr;

	bool bFoundMixer = false;
	bool bFoundConnectionWithMicVolume = false;
	
	CUtlVectorFixedGrowable<MIXERCONTROL, 64> controls;
	// Iterate over all the devices
	// This is done in reverse so the 0th device is our fallback if none of them had the correct MicVolume control
	for ( int iDevice = static_cast<int>( mixerGetNumDevs() ) - 1; iDevice >= 0 && !bFoundConnectionWithMicVolume; --iDevice )
	{
		// Open the mixer.
		mmr = mixerOpen(&m_hMixer, (DWORD)iDevice, 0, 0, 0 );
		if(mmr != MMSYSERR_NOERROR)
		{
			continue;
		}

		// Iterate over each destination line, looking for Play Controls.
		MIXERCAPS mxcaps;
		mmr = mixerGetDevCaps((UINT)m_hMixer, &mxcaps, sizeof(mxcaps));
		if(mmr != MMSYSERR_NOERROR)
		{
			continue;
		}

		bFoundMixer = true;

		for(UINT u = 0; u < mxcaps.cDestinations; u++)
		{
			MIXERLINE recordLine;
			recordLine.cbStruct      = sizeof(recordLine);
			recordLine.dwDestination = u;
			mmr = mixerGetLineInfo((HMIXEROBJ)m_hMixer, &recordLine, MIXER_GETLINEINFOF_DESTINATION);
			if(mmr != MMSYSERR_NOERROR)
				continue;


			// Go through the controls that aren't attached to a specific src connection.
			// We're looking for the checkbox that enables the user's microphone for waveIn.
			if( recordLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN )
			{
				FindMicSelectControl( recordLine.dwLineID, recordLine.cControls );
			}


			// Now iterate over each connection (things like wave out, microphone, speaker, CD audio), looking for Microphone.
			UINT cConnections = (UINT)recordLine.cConnections;
			for (UINT v = 0; v < cConnections; v++)
			{
				MIXERLINE micLine;
				micLine.cbStruct      = sizeof(micLine);
				micLine.dwDestination = u;
				micLine.dwSource      = v;

				mmr = mixerGetLineInfo((HMIXEROBJ)m_hMixer, &micLine, MIXER_GETLINEINFOF_SOURCE);
				if(mmr != MMSYSERR_NOERROR)
					continue;

				// Now look at all the controls (volume, mute, boost, etc).
				controls.RemoveAll();
				controls.SetCount(micLine.cControls);
				if( !GetLineControls( micLine.dwLineID, controls.Base(), micLine.cControls ) )
					continue;

				for(UINT i=0; i < micLine.cControls; i++)
				{
					MIXERCONTROL *pControl = &controls[i];

					if(micLine.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE)
					{
						if( pControl->dwControlType == MIXERCONTROL_CONTROLTYPE_ONOFF &&
							(
								strstr(pControl->szShortName, "Gain") || 
								strstr(pControl->szShortName, "Boos") || 
								strstr(pControl->szShortName, "+20d")
							)
						)
						{
							// This is the (record) boost option.
							m_ControlInfos[MicBoost].m_bFound = true;
							m_ControlInfos[MicBoost].m_dwControlID = pControl->dwControlID;
							m_ControlInfos[MicBoost].m_cMultipleItems = pControl->cMultipleItems;
						}

						if(recordLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_SPEAKERS &&
							pControl->dwControlType == MIXERCONTROL_CONTROLTYPE_MUTE)
						{
							// This is the mute button.
							m_ControlInfos[MicMute].m_bFound = true;
							m_ControlInfos[MicMute].m_dwControlID = pControl->dwControlID;
							m_ControlInfos[MicMute].m_cMultipleItems = pControl->cMultipleItems;
						}
						
						if(recordLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN &&
							pControl->dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME)
						{
							// This is the mic input level.
							m_ControlInfos[MicVolume].m_bFound = true;
							m_ControlInfos[MicVolume].m_dwControlID = pControl->dwControlID;
							m_ControlInfos[MicVolume].m_cMultipleItems = pControl->cMultipleItems;

							// We found a good recording device and can stop looking throught the available devices
							bFoundConnectionWithMicVolume = true;
						}
					}
				}
			}
		}
	}

	if ( !bFoundMixer )
	{
		// Failed to find any mixer (MixVolume or not)
		Term();
		return false;
	}

	return true;
}

void CMixerControls::Term()
{
	if(m_hMixer)
	{
		mixerClose(m_hMixer);
		m_hMixer = 0;
	}

	Clear();
}



bool CMixerControls::GetValue_Float( Control iControl, float &flValue )
{
	if( iControl < 0 || iControl >= NumControls || !m_ControlInfos[iControl].m_bFound )
		return false;

	if(iControl == MicBoost || iControl == MicMute)
	{
		bool bValue = false;
		bool ret = GetControlOption_Bool(m_ControlInfos[iControl].m_dwControlID, m_ControlInfos[iControl].m_cMultipleItems, bValue);
		flValue = (float)bValue;
		return ret;
	}
	else if(iControl == MicVolume)
	{
		DWORD dwValue = (DWORD)0;
		if(GetControlOption_Unsigned(m_ControlInfos[iControl].m_dwControlID, m_ControlInfos[iControl].m_cMultipleItems, dwValue))
		{
			flValue = dwValue / 65535.0f;
			return true;
		}
	}

	return false;
}


bool CMixerControls::SetValue_Float(Control iControl, float flValue )
{
	if(iControl < 0 || iControl >= NumControls || !m_ControlInfos[iControl].m_bFound)
		return false;

	if(iControl == MicBoost || iControl == MicMute)
	{
		bool bValue = !!flValue;
		return SetControlOption_Bool(m_ControlInfos[iControl].m_dwControlID, m_ControlInfos[iControl].m_cMultipleItems, bValue);
	}
	else if(iControl == MicVolume)
	{
		DWORD dwValue = (DWORD)(flValue * 65535.0f);
		return SetControlOption_Unsigned(m_ControlInfos[iControl].m_dwControlID, m_ControlInfos[iControl].m_cMultipleItems, dwValue);
	}
	return false;
}

  
bool CMixerControls::SelectMicrophoneForWaveInput()
{
	if( m_dwMicSelectControlID == 0xFFFFFFFF )
		return false;

	MIXERCONTROLDETAILS_BOOLEAN *pmxcdSelectValue = 
		(MIXERCONTROLDETAILS_BOOLEAN*)_alloca( sizeof(MIXERCONTROLDETAILS_BOOLEAN) * m_dwMicSelectMultipleItems );

	MIXERCONTROLDETAILS mxcd;
	mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
	mxcd.dwControlID = m_dwMicSelectControlID;
	mxcd.cChannels = 1;
	mxcd.cMultipleItems = m_dwMicSelectMultipleItems;
	mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	mxcd.paDetails = pmxcdSelectValue;
	if (mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(m_hMixer),
								 &mxcd,
								 MIXER_OBJECTF_HMIXER |
								 MIXER_GETCONTROLDETAILSF_VALUE)
		== MMSYSERR_NOERROR)
	{
		// MUX restricts the line selection to one source line at a time.
		if( m_dwMicSelectControlType == MIXERCONTROL_CONTROLTYPE_MUX )
		{
			ZeroMemory(pmxcdSelectValue,
						 m_dwMicSelectMultipleItems * sizeof(MIXERCONTROLDETAILS_BOOLEAN));
		}

		// set the Microphone value
		pmxcdSelectValue[m_dwMicSelectIndex].fValue = 1;

		mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
		mxcd.dwControlID = m_dwMicSelectControlID;
		mxcd.cChannels = 1;
		mxcd.cMultipleItems = m_dwMicSelectMultipleItems;
		mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
		mxcd.paDetails = pmxcdSelectValue;
		if (mixerSetControlDetails(reinterpret_cast<HMIXEROBJ>(m_hMixer),
									 &mxcd,
									 MIXER_OBJECTF_HMIXER |
									 MIXER_SETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR)
		{
			return true;
		}
	}
	

	return false;
}


void CMixerControls::Clear()
{ 
	m_hMixer = 0;
	memset(m_ControlInfos, 0, sizeof(m_ControlInfos));
}

bool CMixerControls::GetControlOption_Bool(DWORD dwControlID, DWORD cMultipleItems, bool &bValue)
{
	MIXERCONTROLDETAILS details;
	MIXERCONTROLDETAILS_BOOLEAN controlValue;

	details.cbStruct       = sizeof(details);
	details.dwControlID    = dwControlID;
	details.cChannels      = 1;							// uniform..
	details.cMultipleItems = cMultipleItems;
	details.cbDetails      = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	details.paDetails      = &controlValue;
	
	MMRESULT mmr = mixerGetControlDetails((HMIXEROBJ)m_hMixer, &details, 0L);
	if(mmr == MMSYSERR_NOERROR)
	{
		bValue = !!controlValue.fValue;
		return true;
	}
	else
	{
		return false;
	}
}

bool CMixerControls::SetControlOption_Bool(DWORD dwControlID, DWORD cMultipleItems, const bool bValue)
{
	MIXERCONTROLDETAILS details;
	MIXERCONTROLDETAILS_BOOLEAN controlValue;

	details.cbStruct       = sizeof(details);
	details.dwControlID    = dwControlID;
	details.cChannels      = 1;							// uniform..
	details.cMultipleItems = cMultipleItems;
	details.cbDetails      = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	details.paDetails      = &controlValue;

	controlValue.fValue = bValue;
	
	MMRESULT mmr = mixerSetControlDetails((HMIXEROBJ)m_hMixer, &details, 0L);
	return mmr == MMSYSERR_NOERROR;
}

bool CMixerControls::GetControlOption_Unsigned(DWORD dwControlID, DWORD cMultipleItems, DWORD &value)
{
	MIXERCONTROLDETAILS details;
	MIXERCONTROLDETAILS_UNSIGNED controlValue;

	details.cbStruct       = sizeof(details);
	details.dwControlID    = dwControlID;
	details.cChannels      = 1;							// uniform..
	details.cMultipleItems = cMultipleItems;
	details.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	details.paDetails      = &controlValue;
	
	MMRESULT mmr = mixerGetControlDetails((HMIXEROBJ)m_hMixer, &details, 0L);
	if(mmr == MMSYSERR_NOERROR)
	{
		value = controlValue.dwValue;
		return true;
	}
	else
	{
		return false;
	}
}

bool CMixerControls::SetControlOption_Unsigned(DWORD dwControlID, DWORD cMultipleItems, const DWORD value)
{
	MIXERCONTROLDETAILS details;
	MIXERCONTROLDETAILS_UNSIGNED controlValue;

	details.cbStruct       = sizeof(details);
	details.dwControlID    = dwControlID;
	details.cChannels      = 1;							// uniform..
	details.cMultipleItems = cMultipleItems;
	details.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	details.paDetails      = &controlValue;

	controlValue.dwValue = value;
	
	MMRESULT mmr = mixerSetControlDetails((HMIXEROBJ)m_hMixer, &details, 0L);
	return mmr == MMSYSERR_NOERROR;
}


bool CMixerControls::GetLineControls( DWORD dwLineID, MIXERCONTROL *controls, DWORD nControls )
{
	MIXERLINECONTROLS mxlc;

	mxlc.cbStruct = sizeof(mxlc);
	mxlc.dwLineID = dwLineID;
	mxlc.cControls = nControls;
	mxlc.cbmxctrl = sizeof(MIXERCONTROL);
	mxlc.pamxctrl = controls;
	
	MMRESULT mmr = mixerGetLineControls((HMIXEROBJ)m_hMixer, &mxlc, MIXER_GETLINECONTROLSF_ALL);
	return mmr == MMSYSERR_NOERROR;
}


void CMixerControls::FindMicSelectControl( DWORD dwLineID, DWORD nControls )
{
	m_dwMicSelectControlID = 0xFFFFFFFF;

	MIXERCONTROL *recControls = (MIXERCONTROL*)_alloca( sizeof(MIXERCONTROL) * nControls );
	if( !GetLineControls( dwLineID, recControls, nControls ) )
		return;

	for( UINT iRecControl=0; iRecControl < nControls; iRecControl++ )
	{
		if( recControls[iRecControl].dwControlType == MIXERCONTROL_CONTROLTYPE_MIXER ||
			recControls[iRecControl].dwControlType == MIXERCONTROL_CONTROLTYPE_MUX )
		{
			m_dwMicSelectControlID = recControls[iRecControl].dwControlID;
			m_dwMicSelectControlType = recControls[iRecControl].dwControlType;
			m_dwMicSelectMultipleItems = recControls[iRecControl].cMultipleItems;
			m_dwMicSelectIndex = iRecControl;

			// Get the index of the one that selects the mic.
			MIXERCONTROLDETAILS_LISTTEXT *pmxcdSelectText =
				(MIXERCONTROLDETAILS_LISTTEXT*)_alloca( sizeof(MIXERCONTROLDETAILS_LISTTEXT) * m_dwMicSelectMultipleItems );

			MIXERCONTROLDETAILS mxcd;
			mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
			mxcd.dwControlID = m_dwMicSelectControlID;
			mxcd.cChannels = 1;
			mxcd.cMultipleItems = m_dwMicSelectMultipleItems;
			mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_LISTTEXT);
			mxcd.paDetails = pmxcdSelectText;
			
			if (mixerGetControlDetails((HMIXEROBJ)m_hMixer,
										 &mxcd,
										 MIXER_OBJECTF_HMIXER |
										 MIXER_GETCONTROLDETAILSF_LISTTEXT) == MMSYSERR_NOERROR)
			{
				// determine which controls the Microphone source line
				for (DWORD dwi = 0; dwi < m_dwMicSelectMultipleItems; dwi++)
				{
					// get the line information
					MIXERLINE mxl;
					mxl.cbStruct = sizeof(MIXERLINE);
					mxl.dwLineID = pmxcdSelectText[dwi].dwParam1;
					
					if (mixerGetLineInfo((HMIXEROBJ)m_hMixer,
										   &mxl,
										   MIXER_OBJECTF_HMIXER |
										   MIXER_GETLINEINFOF_LINEID) == MMSYSERR_NOERROR &&
						mxl.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE)
					{
						// found, dwi is the index.
						m_dwMicSelectIndex = dwi;
						break;
					}
				}
			}

			break;
		}
	}
}


IMixerControls* g_pMixerControls = NULL;
void InitMixerControls()
{
	if ( !g_pMixerControls )
	{
		g_pMixerControls = new CMixerControls;
	}
}

void ShutdownMixerControls()
{
	delete g_pMixerControls;
	g_pMixerControls = NULL;
}

