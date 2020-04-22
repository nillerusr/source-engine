//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef SND_DEV_XAUDIO_H
#define SND_DEV_XAUDIO_H
#pragma once
#include "audio_pch.h"
#include "inetmessage.h"
#include "netmessages.h"

class IAudioDevice;
IAudioDevice *Audio_CreateXAudioDevice( void );

#if defined ( _X360 )

class CClientInfo;
void VOICE_AddPlayerToVoiceList( CClientInfo *pClient, bool bLocal );

class CXboxVoice
{
public:

	static const DWORD MAX_VOICE_BUFFER_TIME = 200;  // 200ms
	CXboxVoice( void );
	void		VoiceInit( void );
	void		VoiceShutdown( void );
	void		AddPlayerToVoiceList( CClientInfo *pClient, bool bLocal );
	void		RemovePlayerFromVoiceList( CClientInfo *pClient, bool bLocal );
	bool		VoiceUpdateData( void );
	void		GetVoiceData( CLC_VoiceData *pData );
	void		VoiceSendData( INetChannel	*pChannel );
	void		VoiceResetLocalData( void );
	void		PlayIncomingVoiceData( XUID xuid, const byte *pbData, DWORD pdwSize );
	void		UpdateHUDVoiceStatus( void );
	void		GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers );
	void		SetPlaybackPriority( XUID remoteTalker, DWORD dwUserIndex, XHV_PLAYBACK_PRIORITY playbackPriority );
	bool		IsPlayerTalking( XUID uid, bool bLocal );
	bool		IsHeadsetPresent( int id );
	void		RemoveAllTalkers( CClientInfo *pLocal );

private:
	PIXHV2ENGINE			m_pXHVEngine;


	// Local chat data
	static const WORD m_ChatBufferSize = XHV_VOICECHAT_MODE_PACKET_SIZE * XHV_MAX_VOICECHAT_PACKETS;
	BYTE        m_ChatBuffer[ m_ChatBufferSize ];
	WORD        m_wLocalDataSize;

	// Last voice data sent
	DWORD       m_dwLastVoiceSend;
};

CXboxVoice *Audio_GetXVoice( void );
IXAudio2 *Audio_GetXAudio2( void );

#endif



#endif // SND_DEV_XAUDIO_H
