//========= Copyright Valve Corporation, All rights reserved. ============//
/* CDbgOutput.cpp

*****************************************************************************/
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <dbghelp.h>
#include <dbgeng.h>

#include "vgui/IVGui.h"
#include "vgui/IPanel.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Frame.h"

#include "CMDRipperMain.h"

#include "CDbgOutput.h"

using namespace vgui;


CDbgOutput::CDbgOutput()
{
	m_iRefCount		= 0;
	m_Target		= 0;
}


CDbgOutput::~CDbgOutput()
{
}


STDMETHODIMP CDbgOutput::QueryInterface( THIS_ IN REFIID InterfaceId,
										 OUT PVOID* Interface)
{
    *Interface = NULL;

    if ( IsEqualIID( InterfaceId, __uuidof( IUnknown ) ) ||
         IsEqualIID( InterfaceId, __uuidof( IDebugOutputCallbacks ) ) )
    {
        *Interface = ( IDebugOutputCallbacks * )this;
        AddRef( );
        return S_OK;
    }
    else
    {
        return E_NOINTERFACE;
    }
}


STDMETHODIMP_( ULONG )CDbgOutput::AddRef( THIS )
{
    return ( ++m_iRefCount );
}


STDMETHODIMP_( ULONG )CDbgOutput::Release( THIS )
{
    return ( --m_iRefCount );
}


STDMETHODIMP CDbgOutput::Output( THIS_ IN ULONG Mask, IN PCSTR Text )
{
	if (Text)
	{
		KeyValues *pkv = new KeyValues( "DebugOutput", "iMask", Mask );
		pkv->SetString( "pszDebugText", Text );
		
		ivgui()->DPrintf( "CDbgOutput::Output() about to post [%s]", Text );
		g_pCMDRipperMain->PostMessage( m_Target, pkv );
	}

    return S_OK;
}


void CDbgOutput::SetOutputPanel( vgui::VPANEL Target )
{
	m_Target = Target;
}

/* CDbgOutput.cpp */
