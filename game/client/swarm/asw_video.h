#ifndef _INCLUDED_ASW_VIDEO_H
#define _INCLUDED_ASW_VIDEO_H
#ifdef _WIN32
#pragma once
#endif

#include "avi/ibik.h"


// Handles the face video material resources

enum ASW_Video_Face_Type
{
	ASW_VIDEO_FACE_STATIC = 0,
	ASW_VIDEO_FACE_HEALTHY,
	ASW_VIDEO_FACE_HEALTHY_ALT00,
	ASW_VIDEO_FACE_NEEDHEALTH,
	ASW_VIDEO_FACE_PAIN,

	ASW_VIDEO_FACE_TYPE_TOTAL
};

class CASW_Video_Face_BIKHandles
{
public:
	CASW_Video_Face_BIKHandles( void );
	~CASW_Video_Face_BIKHandles( void );

	void Init( int nCharacterVoiceType );
	void Shutdown( void );

	bool IsInitialized( void ) { return m_bInitialized; }

	void Buffer( void );

	BIKMaterial_t GetBIKHandle( int nFaceType ) const;

private:

	BIKMaterial_t m_BIKHandles[ ASW_VIDEO_FACE_TYPE_TOTAL ];
	bool m_bInitialized;
	int m_nBufferCount;
};


// this holds state for a bink video

class CASW_Video
{
public:
	CASW_Video();
	~CASW_Video();

	virtual void OnVideoOver();

	void Update();
	bool BeginPlayback( int nFaceType );

	void SetBlackBackground( bool bBlack ){ m_bBlackBackground = bBlack; }
	void SetAllowInterrupt( bool bAllowInterrupt ) { m_bAllowInterruption = bAllowInterrupt; }

	void ReturnToLoopVideo( void );
	void PlayTempVideo( int nFaceType, int nTransitionFaceType = -1 );
	void SetLoopVideo( int nFaceType, int nNumLoopAlternatives = 0, float fAlternateChance = 1.0f );

	int GetCurrentVideo( void ) const;
	int GetLoopVideo( void ) const { return m_nLoopVideo; }
	int GetLastTempVideo( void ) const { return m_nLastTempVideo; }
	int GetTransitionVideo( void ) const { return m_nTransitionVideo; }

	int GetWide() { return m_nWide; }
	int GetTall() { return m_nTall; }

	IMaterial* GetMaterial();

	float			m_flU;	// U,V ranges for video on its sheet
	float			m_flV;

private:
	CASW_Video_Face_BIKHandles* GetVideoFaceBIKHandles( void );

protected:
	int				m_nPlaybackHeight;			// Calculated to address ratio changes
	int				m_nPlaybackWidth;
	char			m_szExitCommand[MAX_PATH];	// This call is fired at the engine when the video finishes or is interrupted	

	bool			m_bAllowInterruption;
	bool			m_bBlackBackground;

	bool			m_bStarted;

	int m_nWide;
	int m_nTall;

private:
	int m_nLoopVideo;
	int m_nLastTempVideo;
	int m_nTransitionVideo;
	int m_nNumLoopAlternatives;
	float m_fAlternateChance;
	bool m_bIsLoopVideo;
	bool m_bIsTransition;

	static CASW_Video_Face_BIKHandles s_VideoFaceBIKHandles[ MAX_SPLITSCREEN_PLAYERS ];
};

#endif	// _INCLUDED_ASW_VIDEO_H
