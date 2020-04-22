//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMEMDLPANEL_H
#define DMEMDLPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include "matsys_controls/mdlpanel.h"


class CDmeMDLMakefile;


//-----------------------------------------------------------------------------
// MDL Viewer Panel (hooked into DMEPanel)
//-----------------------------------------------------------------------------
class CDmeMDLPanel : public CMDLPanel
{
	DECLARE_CLASS_SIMPLE( CDmeMDLPanel, CMDLPanel );

public:
	// constructor, destructor
	CDmeMDLPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmeMDLPanel();

	// DMEPanel..
	void SetDmeElement( CDmeMDLMakefile *pMDLMakefile );
};


#endif // DMEMDLPANEL_H
