//========= Copyright Valve Corporation, All rights reserved. ============//
/* CDbgOutput.h

*****************************************************************************/
#ifndef CDBGOUTPUT_H
#	define CDBGOUTPUT_H


class CDbgOutput : public IDebugOutputCallbacks
{
public:
	// Ctor/dtor
	CDbgOutput();
	~CDbgOutput();

    // IUnknown.
    STDMETHOD( QueryInterface )( THIS_ IN REFIID InterfaceId,
								 OUT PVOID* Interface );
    STDMETHOD_( ULONG, AddRef )( THIS );
    STDMETHOD_( ULONG, Release )( THIS );

    // IDebugOutputCallbacks.
    STDMETHOD( Output )( THIS_ IN ULONG Mask, IN PCSTR Text );

	void SetOutputPanel( vgui::VPANEL Target );

private:
	int				m_iRefCount;
	vgui::VPANEL	m_Target;
};


#endif /* CDBGOUTPUT_H */

/* CDbgOutput.h */
