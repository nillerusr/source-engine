//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ISCENEMANAGERSOUND_H
#define ISCENEMANAGERSOUND_H
#ifdef _WIN32
#pragma once
#endif

class CAudioSource;
class CAudioMixer;
class CAudioOuput;

class ISceneManagerSound
{
public:

	virtual void		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;
	virtual void		Update( float time ) = 0;
	virtual void		Flush( void ) = 0;

	virtual CAudioSource *LoadSound( const char *wavfile ) = 0;

	virtual void		PlaySound( const char *wavfile, CAudioMixer **ppMixer ) = 0;

	virtual void		PlaySound( CAudioSource *source, CAudioMixer **ppMixer ) = 0;

	virtual bool		IsSoundPlaying( CAudioMixer *pMixer ) = 0;
	virtual CAudioMixer *FindMixer( CAudioSource *source ) = 0;

	virtual void		StopAll( void ) = 0;
	virtual void		StopSound( CAudioMixer *mixer ) = 0;

	virtual CAudioOuput	*GetAudioOutput( void ) = 0;

	virtual CAudioSource	*FindOrAddSound( const char *filename ) = 0;
};

extern ISceneManagerSound *sound;

#endif // ISCENEMANAGERSOUND_H
