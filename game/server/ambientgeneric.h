//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef AMBIENTGENERIC_H
#define AMBIENTGENERIC_H
#ifdef _WIN32
#pragma once
#endif

// runtime pitch shift and volume fadein/out structure

// NOTE: IF YOU CHANGE THIS STRUCT YOU MUST CHANGE THE SAVE/RESTORE VERSION NUMBER
// SEE BELOW (in the typedescription for the class)
typedef struct dynpitchvol
{
	// NOTE: do not change the order of these parameters 
	// NOTE: unless you also change order of rgdpvpreset array elements!
	int preset;

	int pitchrun;		// pitch shift % when sound is running 0 - 255
	int pitchstart;		// pitch shift % when sound stops or starts 0 - 255
	int spinup;			// spinup time 0 - 100
	int spindown;		// spindown time 0 - 100

	int volrun;			// volume change % when sound is running 0 - 10
	int volstart;		// volume change % when sound stops or starts 0 - 10
	int fadein;			// volume fade in time 0 - 100
	int fadeout;		// volume fade out time 0 - 100

	// Low Frequency Oscillator
	int	lfotype;		// 0) off 1) square 2) triangle 3) random
	int lforate;		// 0 - 1000, how fast lfo osciallates

	int lfomodpitch;	// 0-100 mod of current pitch. 0 is off.
	int lfomodvol;		// 0-100 mod of current volume. 0 is off.

	int cspinup;		// each trigger hit increments counter and spinup pitch


	int	cspincount;

	int pitch;			
	int spinupsav;
	int spindownsav;
	int pitchfrac;

	int vol;
	int fadeinsav;
	int fadeoutsav;
	int volfrac;

	int	lfofrac;
	int	lfomult;


} dynpitchvol_t;

#define SF_AMBIENT_SOUND_EVERYWHERE			1
#define SF_AMBIENT_SOUND_START_SILENT		16
#define SF_AMBIENT_SOUND_NOT_LOOPING		32

class CAmbientGeneric : public CPointEntity
{
public:
	DECLARE_CLASS( CAmbientGeneric, CPointEntity );

	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate( void );
	void RampThink( void );
	void InitModulationParms(void);
	void ComputeMaxAudibleDistance( );

	// Rules about which entities need to transmit along with me
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
	virtual void UpdateOnRemove( void );

	virtual void ToggleSound();
	virtual void SendSound( SoundFlags_t flags );

	// Input handlers
	void InputPlaySound( inputdata_t &inputdata );
	void InputStopSound( inputdata_t &inputdata );
	void InputToggleSound( inputdata_t &inputdata );
	void InputPitch( inputdata_t &inputdata );
	void InputVolume( inputdata_t &inputdata );
	void InputFadeIn( inputdata_t &inputdata );
	void InputFadeOut( inputdata_t &inputdata );

	DECLARE_DATADESC();

	float m_radius;
	float m_flMaxRadius;
	soundlevel_t m_iSoundLevel;		// dB value
	dynpitchvol_t m_dpv;	

	bool m_fActive;		// only true when the entity is playing a looping sound
	bool m_fLooping;		// true when the sound played will loop

	string_t m_iszSound;			// Path/filename of WAV file to play.
	string_t m_sSourceEntName;
	EHANDLE m_hSoundSource;	// entity from which the sound comes
	int		m_nSoundSourceEntIndex; // In case the entity goes away before we finish stopping the sound...
};

#endif // AMBIENTGENERIC_H

