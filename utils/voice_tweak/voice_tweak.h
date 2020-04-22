//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// voice_tweak.h : main header file for the VOICE_TWEAK application
//

#if !defined(AFX_VOICE_TWEAK_H__7343765B_B2E9_4E37_AA34_242E0B635AF1__INCLUDED_)
#define AFX_VOICE_TWEAK_H__7343765B_B2E9_4E37_AA34_242E0B635AF1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "voice_mixer_controls.h"
#include "ivoicerecord.h"
#include "iwaveout.h"
#include "ivoicecodec.h"
#include "interface.h"
#include "voice_gain.h"


#define VOICE_TWEAK_SAMPLE_RATE	22050


// Pass in the english string ID, and this returns the string ID in the current language.
int MapLanguageStringID( int idEnglish );



/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakApp:
// See voice_tweak.cpp for the implementation of this class
//

class CVoiceTweakApp : public CWinApp
{
public:
	CVoiceTweakApp();
	~CVoiceTweakApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVoiceTweakApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

	// Init and deinit the record, waveOut, and voice codec.
	bool				StartDevices();
	void				StopDevices();

	IVoiceRecord		*m_pVoiceRecord;
	IWaveOut			*m_pWaveOut;

	IMixerControls		*m_pMixerControls;


// Implementation

	//{{AFX_MSG(CVoiceTweakApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VOICE_TWEAK_H__7343765B_B2E9_4E37_AA34_242E0B635AF1__INCLUDED_)
