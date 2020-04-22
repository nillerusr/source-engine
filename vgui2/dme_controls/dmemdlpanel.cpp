//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/dmemdlpanel.h"
#include "dme_controls/dmecontrols.h"
#include "dme_controls/dmepanel.h"
#include "movieobjects/dmemdl.h"
#include "movieobjects/dmemdlmakefile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


IMPLEMENT_DMEPANEL_FACTORY( CDmeMDLPanel, DmeMDLMakefile, "DmeMakeFileOutputPreview", "MDL MakeFile Output Preview", false );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmeMDLPanel::CDmeMDLPanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
}

CDmeMDLPanel::~CDmeMDLPanel()
{
}


//-----------------------------------------------------------------------------
// DMEPanel..
//-----------------------------------------------------------------------------
void CDmeMDLPanel::SetDmeElement( CDmeMDLMakefile *pMDLMakefile )
{
	if ( pMDLMakefile != NULL )
	{
		CDmeMDL *pMDL = CastElement< CDmeMDL >( pMDLMakefile->GetOutputElement( true ) );
		if ( pMDL )
		{
			SetMDL( pMDL->GetMDL() );
		}
	}
}
