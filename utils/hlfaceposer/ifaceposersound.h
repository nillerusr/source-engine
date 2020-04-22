//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef IFACEPOSERSOUND_H
#define IFACEPOSERSOUND_H
#ifdef _WIN32
#pragma once
#endif

class StudioModel;
class CAudioSource;
class CAudioMixer;
class CAudioOuput;

class IFacePoserSound
{
public:

	virtual void		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;
	virtual void		Update( float time ) = 0;
	virtual void		Flush( void ) = 0;

	virtual CAudioSource *LoadSound( const char *wavfile ) = 0;

	virtual void		PlaySound( StudioModel *model, float volume, const char *wavfile, CAudioMixer **ppMixer ) = 0;
	virtual void		PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer ) = 0;
	virtual void		PlayPartialSound( StudioModel *model, float volume, const char *wavfile, CAudioMixer **ppMixer, int startSample, int endSample ) = 0;

	virtual bool		IsSoundPlaying( CAudioMixer *pMixer ) = 0;
	virtual CAudioMixer *FindMixer( CAudioSource *source ) = 0;

	virtual void		StopAll( void ) = 0;
	virtual void		StopSound( CAudioMixer *mixer ) = 0;

	virtual void		RenderWavToDC( HDC dc, RECT& outrect, COLORREF clr, 
		float starttime, float endtime, CAudioSource *pWave, 
		bool selected = false, int selectionstart = 0, int selectionend = 0 ) = 0;

	virtual float		GetAmountofTimeAhead( void ) = 0;
	virtual int			GetNumberofSamplesAhead( void ) = 0;

	// virtual void		InstallPhonemecallback( IPhonemeTag *pTagInterface ) = 0;

	virtual CAudioOuput	*GetAudioOutput( void ) = 0;

	virtual void		EnsureNoModelReferences( CAudioSource *source ) = 0;
};

extern IFacePoserSound *sound;

#endif // IFACEPOSERSOUND_H
