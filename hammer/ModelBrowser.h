//========= Copyright Valve Corporation, All rights reserved. ============//
#pragma once

#include "resource.h"
#include "VGuiWnd.h"

// CModelBrowser dialog

namespace vgui
{
	class TextEntry;
	class Splitter;
	class Button;
}

class CModelBrowserPanel;
class CMDLPicker;


class CModelBrowser : public CDialog
{
	DECLARE_DYNAMIC(CModelBrowser)

public:
	CModelBrowser(CWnd* pParent = NULL);   // standard constructor
	virtual ~CModelBrowser();

	void	SetModelName( const char *pModelName );
	void	GetModelName( char *pModelName, int length );
	void	GetSkin( int &nSkin );
	void	SetSkin( int nSkin );

// Dialog Data
	enum { IDD = IDD_MODEL_BROWSER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL PreTranslateMessage( MSG* pMsg ); 


	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();

	virtual BOOL OnInitDialog();

	void UpdateStatusLine();
	void SaveLoadSettings( bool bSave ); 
	void Resize( void );

	CVGuiPanelWnd	m_VGuiWindow;

	CMDLPicker		*m_pPicker;
	vgui::Button	*m_pButtonOK;
	vgui::Button	*m_pButtonCancel;
	vgui::TextEntry	*m_pStatusLine;

	void Show();
	void Hide();
	
};
