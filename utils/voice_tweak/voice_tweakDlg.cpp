//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// voice_tweakDlg.cpp : implementation file
//

#include "stdafx.h"
#include "voice_tweak.h"
#include "voice_tweakDlg.h"
#include "voice_gain.h"
#include "dvoice.h"


void TermDPlayVoice( HINSTANCE &hInst, IDirectPlayVoiceTest* &pVoice )
{
	if( pVoice )
	{
		pVoice->Release();
		pVoice = NULL;
	}

	if( hInst )
	{
		FreeLibrary( hInst );
		hInst = NULL;
	}
}


bool InitDPlayVoice( HINSTANCE &hInst, IDirectPlayVoiceTest* &pVoice )
{
	typedef HRESULT (WINAPI *DirectPlayVoiceCreateFn)(
	  GUID* pcIID, 
	  void** ppvInterface, 
	  IUnknown* pUnknown
	);

	hInst = NULL;
	pVoice = NULL;

	hInst = LoadLibrary( "dpvoice.dll" );
	if(hInst)
	{
		DirectPlayVoiceCreateFn fn = (DirectPlayVoiceCreateFn)GetProcAddress(hInst, "DirectPlayVoiceCreate");
		if(fn)
		{			
			HRESULT hr = fn((GUID*)&IID_IDirectPlayVoiceTest, (void**)&pVoice, NULL);
			if( SUCCEEDED( hr ) )
				return true;
		}
	}

	TermDPlayVoice( hInst, pVoice );
	return false;
}


bool IsDPlayVoiceAvailable()
{
	HINSTANCE hInst;
	IDirectPlayVoiceTest *pVoice;

	bool bRet = InitDPlayVoice( hInst, pVoice );
	TermDPlayVoice( hInst, pVoice );

	return bRet;
}


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define VOLUMESLIDER_RANGE	1000


CVoiceTweakApp*	TweakApp()	{return (CVoiceTweakApp*)AfxGetApp();}


extern "C" 
{
	void Con_DPrintf (char *fmt, ...);
	void Con_Printf (char *fmt, ...);
}


void PrintToTraceWindow(const char *fmt, va_list marker)
{
	char msg[2048];
	_vsnprintf(msg, sizeof(msg), fmt, marker);
	OutputDebugString(msg);
}

void Con_DPrintf (char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	PrintToTraceWindow(fmt, marker);
	va_end(marker);
}

void Con_Printf (char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	PrintToTraceWindow(fmt, marker);
	va_end(marker);
}


/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakDlg dialog

CVoiceTweakDlg::CVoiceTweakDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CVoiceTweakDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CVoiceTweakDlg)
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CVoiceTweakDlg::~CVoiceTweakDlg()
{
	Term();
}

void CVoiceTweakDlg::Term()
{
	m_WinIdle.EndIdle();
}


void CVoiceTweakDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVoiceTweakDlg)
	DDX_Control(pDX, IDC_HARDWAREGAIN, m_HardwareGain);
	DDX_Control(pDX, IDC_VOICEVOLUME, m_VoiceVolume);
	DDX_Control(pDX, IDC_VOLUMESLIDER, m_VolumeSlider);
	DDX_Control(pDX, IDC_INSTRUCTIONTEXT, m_InstructionText);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CVoiceTweakDlg, CDialog)
	//{{AFX_MSG_MAP(CVoiceTweakDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_MESSAGE(WM_TWEAKIDLE, OnIdle)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_HARDWAREGAIN, OnHardwareGain)
	ON_BN_CLICKED(IDFURTHERHELP, OnFurtherhelp)
	ON_BN_CLICKED(IDSYSTEMSETUP, OnSystemSetup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


IMixerControls* GetAppMixerControls()
{
	return ((CVoiceTweakApp*)AfxGetApp())->m_pMixerControls;
}


void CVoiceTweakDlg::SetString(int childControl, int stringID)
{
	if(CWnd *pWnd = GetDlgItem(childControl))
	{
		CString str;
		str.LoadString(stringID);
		pWnd->SetWindowText(str);
	}
}


/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakDlg message handlers

BOOL CVoiceTweakDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	CString str;
	str.LoadString( MapLanguageStringID(IDS_HELPTEXT) );
	m_InstructionText.SetWindowText(str);


	// Save off their old settings so we can restore if they hit cancel.
	GetAppMixerControls()->GetValue_Float(IMixerControls::Control::MicVolume, m_OldVolume);

	float bBoostOn;
	if(GetAppMixerControls()->GetValue_Float(IMixerControls::Control::MicBoost, bBoostOn))
	{
		m_HardwareGain.SetCheck((int)bBoostOn);
	}
	else
	{
		m_HardwareGain.ShowWindow(SW_HIDE);
	}

	// Initialize the volume control.
	m_VolumeSlider.SetRange(0, 1000);
	m_VolumeSlider.SetPos((int)(VOLUMESLIDER_RANGE - m_OldVolume * VOLUMESLIDER_RANGE));

	m_VoiceVolume.SetRange32(0, (1 << (BYTES_PER_SAMPLE*8-1)) - 1);

	// Get idle messages...
	m_WinIdle.StartIdle(GetSafeHwnd(), WM_TWEAKIDLE, 0,0, 10);
	m_WinIdle.NextIdle();

	// Set all the dialog item strings for localization.
	SetString(IDOK, MapLanguageStringID(IDS_OKAY));
	SetString(IDCANCEL, MapLanguageStringID(IDS_CANCEL));
	SetString(IDC_VOICEINPUTFRAME, MapLanguageStringID(IDS_VOICEINPUT));
	SetString(IDC_VOLUMELABEL, MapLanguageStringID(IDS_VOLUME));
	SetString(IDC_HARDWAREGAIN, MapLanguageStringID(IDS_ENABLEGAIN));
	SetString(IDSYSTEMSETUP, MapLanguageStringID(IDS_SYSTEMSETUP));
	SetString(IDFURTHERHELP, MapLanguageStringID(IDS_HELP));
	
	CString titleStr;
	titleStr.LoadString( MapLanguageStringID(IDS_WINDOWTITLE) );
	SetWindowText(titleStr);

	if( !IsDPlayVoiceAvailable() )
	{
		CWnd *pWnd = GetDlgItem( IDSYSTEMSETUP );
		if( pWnd )
			pWnd->EnableWindow( false );
	}
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CVoiceTweakDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CVoiceTweakDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

BOOL CVoiceTweakDlg::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult) 
{
	NMHDR *pHdr = (NMHDR*)lParam;
	if(pHdr->hwndFrom == m_VolumeSlider.m_hWnd)
	{
		GetAppMixerControls()->SetValue_Float(IMixerControls::Control::MicVolume, (VOLUMESLIDER_RANGE - m_VolumeSlider.GetPos()) / (float)VOLUMESLIDER_RANGE);
	}

	return CDialog::OnNotify(wParam, lParam, pResult);
}


LONG CVoiceTweakDlg::OnIdle(UINT a, LONG b)
{
	static DWORD startTime = GetTickCount();

	if( TweakApp()->m_pVoiceRecord )
	{
		short samples[22 * 1024];
	
		// If the output has too many buffered samples, skip some data.
		TweakApp()->m_pWaveOut->Idle();
		int nBufferedSamples = TweakApp()->m_pWaveOut->GetNumBufferedSamples();
		float nSeconds = nBufferedSamples / (float)VOICE_TWEAK_SAMPLE_RATE;
		int nMinSamples = VOICE_TWEAK_SAMPLE_RATE / 5;
		if( nBufferedSamples < nMinSamples )
		{
			// We want at least a certain amount of buffered data.
			int nSamplesToAdd = nMinSamples - nBufferedSamples;
			memset( samples, 0, nSamplesToAdd*2 );
			TweakApp()->m_pWaveOut->PutSamples( samples, nSamplesToAdd );
		}
		else
		{
			// Get the samples.
			int nSamples = TweakApp()->m_pVoiceRecord->GetRecordedData(samples, sizeof(samples)/BYTES_PER_SAMPLE);
			if( nSeconds < 0.5f )
			{
				// Find the highest value.
				int highValue = -100000;
				for(int i=0; i < nSamples; i++)
					highValue = max(abs(samples[i]), highValue);

				// Set our status bar accordingly.
				highValue = (highValue >> 9) << 9;	// Get rid of flicker.
				m_VoiceVolume.SetPos(highValue);

				// Give the samples to the wave output...
				if(TweakApp()->m_pWaveOut)
				{
					// Ignore the first second or so.. it's usually garbage.				
					DWORD curTime = GetTickCount();
					static DWORD silentTime = 500;
					static DWORD fadeInTime = 1000;
					if( curTime - startTime < silentTime )
					{
						memset( samples, 0, nSamples*2 );
					}
					else if( (curTime-silentTime) - startTime < fadeInTime )
					{
						float flFade = ((curTime-silentTime) - startTime) / (float)fadeInTime;
						flFade = flFade*flFade;
						for( int i=0; i < nSamples; i++ )
							samples[i] = (short)( samples[i] * flFade );
					}					
					
					TweakApp()->m_pWaveOut->PutSamples(samples, nSamples);
				}
			}
		}
	}

	// Tell the idle thread we're ready for another idle message.
	m_WinIdle.NextIdle();
	return 0;
}


void CVoiceTweakDlg::OnDestroy() 
{
	Term();

	CDialog::OnDestroy();
}

void CVoiceTweakDlg::OnHardwareGain() 
{
	if(m_HardwareGain.GetCheck())
		GetAppMixerControls()->SetValue_Float(IMixerControls::Control::MicBoost, true);
	else
		GetAppMixerControls()->SetValue_Float(IMixerControls::Control::MicBoost, false);
}

void CVoiceTweakDlg::OnCancel() 
{
	// Restore old settings.
	GetAppMixerControls()->SetValue_Float(IMixerControls::Control::MicVolume, m_OldVolume);
	
	CDialog::OnCancel();
}

void CVoiceTweakDlg::OnFurtherhelp() 
{
}


void CVoiceTweakDlg::OnSystemSetup() 
{
	TweakApp()->StopDevices();

	bool bSucceeded = false;
	HINSTANCE hInst;
	IDirectPlayVoiceTest *pVoice;
	if( InitDPlayVoice( hInst, pVoice ) )
	{
		pVoice->CheckAudioSetup(NULL, NULL, m_hWnd, DVFLAGS_ALLOWBACK);
		TermDPlayVoice( hInst, pVoice );
	}
	else
	{
		CString str;
		str.LoadString( MapLanguageStringID(IDS_NODPLAYVOICE) );
		MessageBox(str);
	}

	if(!TweakApp()->StartDevices())
		AfxPostQuitMessage(0);
}
