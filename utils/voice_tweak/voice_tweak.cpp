//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// voice_tweak.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include <assert.h>
#include "voice_tweak.h"
#include "waveout.h"
#include "voice_tweakDlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//extern IVoiceRecord*	CreateVoiceRecord_WaveIn(int sampleRate);
extern IVoiceRecord*	CreateVoiceRecord_DSound(int sampleRate);


typedef enum
{
	LANGUAGE_ENGLISH=0,
	LANGUAGE_SPANISH=1,
	LANGUAGE_FRENCH=2,
	LANGUAGE_ITALIAN=3,
	LANGUAGE_GERMAN=4,
	LANGUAGE_COUNT=5
} VoiceTweakLanguageID;

VoiceTweakLanguageID g_CurrentLanguage = LANGUAGE_ENGLISH;

#define LANGENTRY(name) {IDS_##name##, IDS_SPANISH_##name##, IDS_FRENCH_##name##, IDS_ITALIAN_##name##, IDS_GERMAN_##name##}

int g_StringIDs[][LANGUAGE_COUNT] =
{
	LANGENTRY(HELPTEXT),
	LANGENTRY(ERROR),
	LANGENTRY(CANTFINDMICBOOST),
	LANGENTRY(CANTFINDMICVOLUME),
	LANGENTRY(CANTFINDMICMUTE),
	LANGENTRY(CANTCREATEWAVEIN),
	LANGENTRY(CANTLOADVOICEMODULE),
	LANGENTRY(CANTCREATEWAVEOUT),
	LANGENTRY(NODPLAYVOICE),
	LANGENTRY(WINDOWTITLE),
	LANGENTRY(OKAY),
	LANGENTRY(CANCEL),
	LANGENTRY(SYSTEMSETUP),
	LANGENTRY(HELP),
	LANGENTRY(VOICEINPUT),
	LANGENTRY(VOLUME),
	LANGENTRY(ENABLEGAIN),
};
#define NUM_STRINGIDS	(sizeof(g_StringIDs)/sizeof(g_StringIDs[0]))

// Pass in the english string ID, and this returns the string ID in the current language.
int MapLanguageStringID( int idEnglish )
{
	for( int i=0; i < NUM_STRINGIDS; i++ )
	{
		if( idEnglish == g_StringIDs[i][LANGUAGE_ENGLISH] )
			return g_StringIDs[i][g_CurrentLanguage];
	}

	assert( !"MapLanguageStringID: unknown string ID" );
	return 0;
}



/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakApp

BEGIN_MESSAGE_MAP(CVoiceTweakApp, CWinApp)
	//{{AFX_MSG_MAP(CVoiceTweakApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakApp construction

CVoiceTweakApp::CVoiceTweakApp()
{
	m_pVoiceRecord = 0;
	m_pWaveOut = 0;
	m_pMixerControls = 0;
}

CVoiceTweakApp::~CVoiceTweakApp()
{
	StopDevices();
}


bool CVoiceTweakApp::StartDevices()
{
	StopDevices();

	CString str, errStr;

	// Setup wave in.
	if( !(m_pVoiceRecord = CreateVoiceRecord_DSound(VOICE_TWEAK_SAMPLE_RATE)) )
	{
		//if( !(m_pVoiceRecord = CreateVoiceRecord_WaveIn(VOICE_TWEAK_SAMPLE_RATE)) )
		{
			str.LoadString( MapLanguageStringID(IDS_CANTCREATEWAVEIN) );
			::MessageBox(NULL, str, errStr, MB_OK);
			return false;
		}
	}


	m_pVoiceRecord->RecordStart();


	if( !(m_pWaveOut = CreateWaveOut(VOICE_TWEAK_SAMPLE_RATE)) )
	{
		str.LoadString( MapLanguageStringID(IDS_CANTCREATEWAVEOUT) );
		::MessageBox(NULL, str, errStr, MB_OK);
		return false;
	}

	return true;
}


void CVoiceTweakApp::StopDevices()
{
	if(m_pVoiceRecord)
	{
		m_pVoiceRecord->Release();
		m_pVoiceRecord = NULL;
	}

	if(m_pWaveOut)
	{
		m_pWaveOut->Release();
		m_pWaveOut = NULL;
	}
}


/////////////////////////////////////////////////////////////////////////////
// The one and only CVoiceTweakApp object

CVoiceTweakApp theApp;

char const* FindArg(char const *pName)
{
	for(int i=0; i < __argc; i++)
		if(stricmp(__argv[i], pName) == 0)
			return ((i+1) < __argc) ? __argv[i+1] : "";

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakApp initialization
BOOL CVoiceTweakApp::InitInstance()
{
	// Set the thread locale so it grabs the string resources for the right language. If 
	// we don't have resources for the system default language, it just uses English.
	if( FindArg("-french") )
		g_CurrentLanguage = LANGUAGE_FRENCH;
	else if( FindArg("-spanish") )
		g_CurrentLanguage = LANGUAGE_SPANISH;
	else if(FindArg("-italian"))
		g_CurrentLanguage = LANGUAGE_ITALIAN;
	else if(FindArg("-german"))
		g_CurrentLanguage = LANGUAGE_GERMAN;
	else
		g_CurrentLanguage = LANGUAGE_ENGLISH;
	
	CString errStr, str;
	errStr.LoadString( MapLanguageStringID(IDS_ERROR) );

	m_pMixerControls = GetMixerControls(); 

	// Initialize the mixer controls.
	bool bFoundVolume, bFoundMute;	

	float volume, mute;
	bFoundVolume = m_pMixerControls->GetValue_Float(IMixerControls::Control::MicVolume, volume);
	bFoundMute   = m_pMixerControls->GetValue_Float(IMixerControls::Control::MicMute, mute);

	if(!bFoundVolume)
	{
		str.LoadString( MapLanguageStringID(IDS_CANTFINDMICVOLUME) );
		::MessageBox(NULL, str, errStr, MB_OK);
		return FALSE;
	}

	if(!bFoundMute)
	{
		str.LoadString( MapLanguageStringID(IDS_CANTFINDMICMUTE) );
		::MessageBox(NULL, str, errStr, MB_OK);
		return FALSE;
	}

	// Set mute and boost for them automatically.
	m_pMixerControls->SetValue_Float(IMixerControls::Control::MicMute, 1);	

	// We cycle the mic boost because for some reason Windows misses the first call to set it to 1, but
	// if the user clicks the checkbox on and off again, it works.
	m_pMixerControls->SetValue_Float(IMixerControls::Control::MicBoost, 1);	
	m_pMixerControls->SetValue_Float(IMixerControls::Control::MicBoost, 0);	
	m_pMixerControls->SetValue_Float(IMixerControls::Control::MicBoost, 1);	

	// Enable the mic for wave input.
	m_pMixerControls->SelectMicrophoneForWaveInput();

	if(!StartDevices())
		return false;


	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	CVoiceTweakDlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
