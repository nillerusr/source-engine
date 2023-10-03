//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef KEYBINDINGLISTENER_H
#define KEYBINDINGLISTENER_H
#ifdef _WIN32
#pragma once
#endif

enum ButtonCode_t;

class IKeyBindingListener
{
public:
	virtual void OnKeyBindingChanged( int nSplitScreenSlot, ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding ) = 0;
};

class IKeyBindingListenerMgr
{
public:
	// Callback when button is bound
	virtual void AddListenerForCode( IKeyBindingListener *pListener, ButtonCode_t buttonCode ) = 0;
	// Callback whenver binding is set to a button
	virtual void AddListenerForBinding( IKeyBindingListener *pListener, char const *pchBindingString ) = 0;

	virtual void RemoveListener( IKeyBindingListener *pListener ) = 0;
};

extern IKeyBindingListenerMgr *g_pKeyBindingListenerMgr;

#endif // KEYBINDINGLISTENER_H
