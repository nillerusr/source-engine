//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VPASSWORDENTRY_H__
#define __VPASSWORDENTRY_H__

#include "basemodui.h"
#include "vgui_controls/TextEntry.h"
#include "tier1/UtlString.h"

namespace BaseModUI {

	class PasswordEntry : public CBaseModFrame
	{
		DECLARE_CLASS_SIMPLE( PasswordEntry, CBaseModFrame );
	public:
		PasswordEntry(vgui::Panel *parent, const char *panelName);
		~PasswordEntry();

		void GetPassword( char *buf, size_t bufsize );

		// 
		// Public types
		//
		typedef void (*Callback_t)(void);

		struct Data_t
		{
			const char* pWindowTitle;
			const char* pMessageText;

			bool        bOkButtonEnabled;
			Callback_t	pfnOkCallback;

			bool        bCancelButtonEnabled;
			Callback_t	pfnCancelCallback;

			CUtlString	m_szCurrentPW;

			Data_t();
		};

		int  SetUsageData( const Data_t & data );     // returns the usageId, different number each time this is called
		int  GetUsageId() const { return m_usageId; }

		// 
		// Accessors
		//
		const Data_t & GetUsageData() const { return m_data; }

	protected:
		virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
		virtual void OnCommand(const char *command);
		virtual void OnKeyCodePressed(vgui::KeyCode code);
#ifndef _X360
		virtual void OnKeyCodeTyped( vgui::KeyCode code );
#endif
		virtual void OnOpen();
		virtual void LoadLayout();
		virtual void PaintBackground();

		vgui::Label	*m_pLblOkButton;
		vgui::Label *m_pLblOkText;
		vgui::Label *m_pLblCancelButton;
		vgui::Label *m_pLblCancelText;
		vgui::Panel *m_pPnlLowerGarnish;

		vgui::TextEntry *m_pInputField;

	private:
		vgui::Label *m_pLblMessage;

		Data_t		 m_data;
		int			 m_usageId;

		static int sm_currentUsageId;

		bool	m_bNeedsMoveToFront;
	};

};

#endif // __VPASSWORDENTRY_H__