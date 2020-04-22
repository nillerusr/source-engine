//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "toolutils/ConsolePage.h"
#include "toolutils/enginetools_int.h"
#include "toolframework/ienginetool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CConsolePage::CConsolePage( Panel *parent, bool bStatusVersion ) : BaseClass( parent, "ToolsConsole", bStatusVersion )
{
	AddActionSignalTarget( this );
}


//-----------------------------------------------------------------------------
// Submits a command
//-----------------------------------------------------------------------------
void CConsolePage::OnCommandSubmitted( const char *pCommand )
{
	enginetools->Command( pCommand );
}


//-----------------------------------------------------------------------------
// Purpose: sets up colors
//-----------------------------------------------------------------------------
void CConsolePage::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_PrintColor = GetSchemeColor("IFMConsole.TextColor", pScheme);
	m_DPrintColor = GetSchemeColor("IFMConsole.DevTextColor", pScheme);
}
