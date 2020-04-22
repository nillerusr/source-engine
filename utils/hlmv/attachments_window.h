//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ATTACHMENTS_WINDOW_H
#define ATTACHMENTS_WINDOW_H
#ifdef _WIN32
#pragma once
#endif


#ifndef INCLUDED_MXWINDOW
#include <mxtk/mxWindow.h>
#endif
#include <mxtk/mx.h>
#include "mxLineEdit2.h"
#include "mathlib/vector.h"


class ControlPanel;


class CAttachmentsWindow : public mxWindow
{
public:
	CAttachmentsWindow( ControlPanel* pParent );
	void Init( );

	void OnLoadModel();
	
	void OnTabSelected();
	void OnTabUnselected();
	
	virtual int handleEvent( mxEvent *event );


private:

	void OnSelChangeAttachmentList();

	void PopulateAttachmentsList();
	void PopulateBoneList();
	void UpdateStrings( bool bUpdateQC=true, bool bUpdateTranslation=true, bool bUpdateRotation=true );

	Vector GetCurrentTranslation();
	Vector GetCurrentRotation();


private:

	ControlPanel *m_pControlPanel;
	mxListBox *m_cAttachmentList;
	mxListBox *m_cBoneList;
	
	mxLineEdit2 *m_cTranslation;
	mxLineEdit2 *m_cRotation;
	mxLineEdit2 *m_cQCString;
};


#endif // ATTACHMENTS_WINDOW_H
