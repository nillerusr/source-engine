//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "SoundBrowser.h"
#include "mmsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static LPCTSTR s_pszSection = "SoundBrowser";
CStringArray CSoundBrowser::m_FilterHistory;
int CSoundBrowser::m_nFilterHistory;

/////////////////////////////////////////////////////////////////////////////
// CSoundBrowser dialog


CSoundBrowser::CSoundBrowser( const char *pCurrentSoundName, CWnd* pParent /*=NULL*/ )
	: CDialog(CSoundBrowser::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSoundBrowser)
	m_Autoplay = FALSE;
	m_SoundFile = _T("");
	m_SoundSource = _T("");
	//}}AFX_DATA_INIT

	m_SoundNameSelected = pCurrentSoundName;
	m_SoundType = AfxGetApp()->GetProfileInt(s_pszSection, "Sound Type", 0);
	m_Autoplay = AfxGetApp()->GetProfileInt(s_pszSection, "Sound Autoplay", 0);
	Q_strncpy(m_szFilter, (LPCSTR)(AfxGetApp()->GetProfileString(s_pszSection, "Sound Filter", "")), 256 ); 
	m_nSelectedSoundIndex = -1;

//	m_bSoundPlayed = false;
}

void CSoundBrowser::SaveValues()
{
	AfxGetApp()->WriteProfileInt(s_pszSection, "Sound Type", m_SoundType);
	AfxGetApp()->WriteProfileInt(s_pszSection, "Sound Autoplay", m_Autoplay);
	AfxGetApp()->WriteProfileString(s_pszSection, "Sound Filter", m_szFilter);
}

void CSoundBrowser::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSoundBrowser)
	DDX_Control(pDX, IDC_SOUND_LIST, m_SoundList);
	DDX_Text(pDX, IDC_SOUNDNAME_SELECTED, m_SoundNameSelected);
	DDX_CBIndex(pDX, IDC_SOUND_TYPE, m_SoundType);
	DDX_Check(pDX, IDC_AUTOPLAY, m_Autoplay);
	DDX_Text(pDX, IDC_SOUND_FILE, m_SoundFile);
	DDX_Text(pDX, IDC_SOUND_SOURCE_FILE, m_SoundSource);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSoundBrowser, CDialog)
	//{{AFX_MSG_MAP(CSoundBrowser)
	ON_WM_CLOSE()
	ON_CBN_EDITCHANGE(IDC_SOUND_FILTER, OnChangeFilter)
	ON_CBN_SELENDOK(IDC_SOUND_FILTER, OnUpdateFilterNOW)
	ON_CBN_SELCHANGE(IDC_SOUND_TYPE, OnSelchangeSoundType)
	ON_LBN_SELCHANGE(IDC_SOUND_LIST, OnSelchangeSoundList)
	ON_LBN_DBLCLK(IDC_SOUND_LIST, OnDblclkSoundList)
	ON_BN_CLICKED(IDC_PREVIEW, OnPreview)
	ON_BN_CLICKED(IDC_AUTOPLAY, OnAutoplay)
	ON_BN_CLICKED(IDC_STOPSOUND, OnBnClickedStopsound)
	ON_BN_CLICKED(IDC_REFRESH_SOUNDS, OnRefreshSounds)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_OPEN_SOURCE, OnOpenSource)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSoundBrowser message handlers

BOOL CSoundBrowser::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_cFilter.SubclassDlgItem(IDC_SOUND_FILTER, this);
	for ( int i = 0; i < m_nFilterHistory; ++i )
	{
		m_cFilter.AddString( m_FilterHistory[i] );
	}

	m_cFilter.SetWindowText(m_szFilter);

	CString temp = m_szFilter;
	OnFilterChanged( temp );

	// Select an entry in the list that has the same name as the one passed in
	int nIndex = m_SoundList.FindString( -1, m_SoundNameSelected );
	if ( nIndex != LB_ERR)
	{
		m_SoundList.SetCurSel( nIndex );
		m_nSelectedSoundIndex = nIndex;
		int nSoundIndex = m_SoundList.GetItemData(nIndex);
		m_SoundFile = g_Sounds.SoundFile( GetSoundType(), nSoundIndex ); 
		m_SoundSource = g_Sounds.SoundSourceFile( GetSoundType(), nSoundIndex ); 
		UpdateData( FALSE );
	}

	SetTimer(1, 500, NULL);

	return TRUE;
}

void CSoundBrowser::OnClose(void)
{
	Shutdown();
	CDialog::OnClose();
}


//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CSoundBrowser::Shutdown()
{
	SaveValues();
	PlaySound( NULL, NULL, SND_FILENAME | SND_NODEFAULT); 

	// save current filter string
	int i;
	for (i = 0; i < m_nFilterHistory; i++)
	{
		if (!m_FilterHistory[i].CompareNoCase(m_szFilter))
			break;
	}

	if(i != m_nFilterHistory)	// delete first
	{
		m_FilterHistory.RemoveAt(i);
		--m_nFilterHistory;
	}
	
	if ( m_szFilter[0] )
	{
		m_FilterHistory.InsertAt(0, m_szFilter);
		++m_nFilterHistory;
	}
}


//-----------------------------------------------------------------------------
// Clears, fills sound list
//-----------------------------------------------------------------------------
void CSoundBrowser::ClearSoundList()
{
	m_SoundList.ResetContent();
}

//-----------------------------------------------------------------------------
// Sound filter
//-----------------------------------------------------------------------------
bool CSoundBrowser::ShowSoundInList( const char *pSoundName )
{
	for (int i = 0; i < m_nFilters; i++)
	{
		if ( Q_stristr(pSoundName, m_Filters[i]) == NULL )
			return false;
	}

	return true;
}

void CSoundBrowser::PopulateSoundList()
{
	m_SoundList.SetRedraw( FALSE );

	ClearSoundList();

	SoundType_t type = GetSoundType();
	for ( int i = g_Sounds.SoundCount( type ); --i >= 0; )
	{
		const char *pSoundName = g_Sounds.SoundName( type, i );
		if ( ShowSoundInList( pSoundName ) )
		{
			CString str;
			str.Format( _T(pSoundName) );
			int nIndex = m_SoundList.AddString( str );
			m_SoundList.SetItemDataPtr( nIndex, (PVOID)i );
		}
	}

	m_SoundList.SetRedraw( TRUE );
}


//-----------------------------------------------------------------------------
// Sound type 
//-----------------------------------------------------------------------------
SoundType_t CSoundBrowser::GetSoundType() const
{
	if ( m_SoundType == 0 )
		return SOUND_TYPE_GAMESOUND;
	else if ( m_SoundType == 1 )
		return SOUND_TYPE_RAW;
	else
		return SOUND_TYPE_SCENE;
}



//-----------------------------------------------------------------------------
// Sound name 
//-----------------------------------------------------------------------------
void CSoundBrowser::CopySoundNameToSelected()
{
	UpdateData( TRUE );

	int nIndex = m_SoundList.GetCurSel();
	if ( nIndex != LB_ERR )
	{
		int nSoundIndex = m_SoundList.GetItemData(nIndex);
		m_SoundNameSelected = g_Sounds.SoundName( GetSoundType(), nSoundIndex );
		m_SoundFile = g_Sounds.SoundFile( GetSoundType(), nSoundIndex ); 
		m_SoundSource = g_Sounds.SoundSourceFile( GetSoundType(), nSoundIndex ); 
		m_nSelectedSoundIndex = nSoundIndex;
		UpdateData( FALSE );
	}
}


//-----------------------------------------------------------------------------
// Update the filter: 
//-----------------------------------------------------------------------------
void CSoundBrowser::OnFilterChanged( const char *pFilter )
{
	Q_strncpy( m_szFilter, pFilter, 256 );
	m_nFilters = 0;
	char *p = strtok(m_szFilter, " ,;");
	while (p != NULL)
	{	
		m_Filters[m_nFilters++] = p;
		p = strtok(NULL, " ,;");
	}
	PopulateSoundList();	
}


//-----------------------------------------------------------------------------
// Purpose: Timer used to control updates when the filter terms change.
// Input  : nIDEvent - 
//-----------------------------------------------------------------------------
void CSoundBrowser::OnTimer(UINT nIDEvent) 
{
	if (!m_bFilterChanged)
		return;

	if ((time(NULL) - m_uLastFilterChange) > 0)
	{
		KillTimer(nIDEvent);
		m_bFilterChanged = FALSE;

		CString str;
		m_cFilter.GetWindowText(str);
		OnFilterChanged( str );

		SetTimer(nIDEvent, 500, NULL);
	}

	CDialog::OnTimer(nIDEvent);
}

//-----------------------------------------------------------------------------
// Purpose: Called when either the filter combo or the keywords combo text changes.
//-----------------------------------------------------------------------------
void CSoundBrowser::OnChangeFilter() 
{
	// Start a timer to repaint the texture window using the new filters.
	m_uLastFilterChange = time(NULL);
	m_bFilterChanged = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSoundBrowser::OnUpdateFilterNOW() 
{
	m_uLastFilterChange = time(NULL);
	m_bFilterChanged = FALSE;

	CString str;
	int iSel = m_cFilter.GetCurSel();
	m_cFilter.GetLBText(iSel, str);
	OnFilterChanged( str );
}


//-----------------------------------------------------------------------------
// Sound type changed
//-----------------------------------------------------------------------------
void CSoundBrowser::OnSelchangeSoundType() 
{
	UpdateData( TRUE );
	PopulateSoundList();	
}


//-----------------------------------------------------------------------------
// Selected sound 
//-----------------------------------------------------------------------------
const char *CSoundBrowser::GetSelectedSound()
{
	return m_SoundNameSelected;
}


void CSoundBrowser::OnSelchangeSoundList() 
{
	CopySoundNameToSelected();
	if ( m_Autoplay )
	{
		OnPreview();
	}
}

void CSoundBrowser::OnDblclkSoundList() 
{
	CopySoundNameToSelected();
	OnOK();
}

void CSoundBrowser::OnPreview() 
{
	if ( m_nSelectedSoundIndex >= 0 )
	{
		g_Sounds.Play( GetSoundType(), m_nSelectedSoundIndex );
	}
}

void CSoundBrowser::OnAutoplay() 
{
	UpdateData( TRUE );
}

void CSoundBrowser::OnRefreshSounds()
{
	// Set the title to "refreshing sounds..."
	CString oldTitle, newTitle;
	newTitle.LoadString( IDS_REFRESHING_SOUNDS );
	GetWindowText( oldTitle );
	SetWindowText( newTitle );
	
	g_Sounds.Initialize();
	PopulateSoundList();
	
	// Restore the title.
	SetWindowText( oldTitle );
}

int CSoundBrowser::DoModal() 
{	
	int nRet = CDialog::DoModal();
	Shutdown();
	return nRet;
}

void CSoundBrowser::OnOpenSource() 
{
	if ( m_nSelectedSoundIndex >= 0 )
	{
		g_Sounds.OpenSource( GetSoundType(), m_nSelectedSoundIndex );
	}
}

void CSoundBrowser::OnBnClickedStopsound()
{
	g_Sounds.StopSound(); 
}
