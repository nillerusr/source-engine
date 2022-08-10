//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: TF implementation of the IPresence interface
//
//=============================================================================

#ifndef TF_TIPS_H
#define TF_TIPS_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"

//-----------------------------------------------------------------------------
// Purpose: helper class for TF tips
//-----------------------------------------------------------------------------
class CTFTips : public CAutoGameSystem
{
public:
	CTFTips();

	virtual bool Init();
	virtual char const *Name() { return "CTFTips"; }

	const wchar_t *GetRandomTip();
	const wchar_t *GetNextClassTip( int iClass );
private:
	const wchar_t *GetTip( int iClass, int iTip );

	int m_iTipCount[TF_LAST_NORMAL_CLASS+1];		// how many tips there are for each class
	int m_iTipCountAll;								// how many tips there are total
	int m_iCurrentClassTip;							// index of current per-class tip
	bool m_bInited;									// have we been initialized
};

extern CTFTips g_TFTips;
#endif // TF_TIPS_H
