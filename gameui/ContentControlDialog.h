//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CONTENTCONTROLDIALOG_H
#define CONTENTCONTROLDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CContentControlDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CContentControlDialog, vgui::Frame );

public:
	CContentControlDialog(vgui::Panel *parent);
	~CContentControlDialog();

	virtual void OnCommand( const char *command );
	virtual void OnClose();
	virtual void Activate();

    void        ResetPassword();
    void        ApplyPassword();
    bool        IsPasswordEnabledInDialog();
    bool        IsPasswordEnabled()             { return ( m_szGorePW[0] != 0 ); }

protected:
	void			WriteToken( const char *str );
	bool			CheckPassword( char const *oldPW, char const *newPW, bool enableContentControl );
	void			UpdateContentControlStatus( void );

	void			Explain( PRINTF_FORMAT_STRING char const *fmt, ... );

    void            HashPassword(const char *newPW, char *hashBuffer, int maxlen );
    bool            EnablePassword(const char *newPW);
    bool            DisablePassword(const char *oldPW);

	enum
	{
		MAX_GORE_PW = 64,
	};

	char			m_szGorePW[ MAX_GORE_PW ];

    bool            m_bDefaultPassword;
	vgui::Label		*m_pStatus;
	vgui::Button	*m_pOK;
	vgui::TextEntry	*m_pPassword;
    vgui::Label     *m_pPasswordLabel;
    vgui::Label     *m_pPassword2Label;
	vgui::TextEntry	*m_pPassword2;

	vgui::Label		*m_pExplain;
};


#endif // CONTENTCONTROLDIALOG_H
