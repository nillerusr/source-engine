//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: interface class between viewport msg funcs and "c" based client dll
//
// $NoKeywords: $
//=============================================================================//
#if !defined( IDODVIEWPORTMSGS_H )
#define IDODVIEWPORTMSGS_H
#ifdef _WIN32
#pragma once
#endif

//#include "vgui/IViewPortMsgs.h"

class IDODViewPortMsgs /*: public IViewPortMsgs*/
{
public:
	virtual int MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf ) = 0;

};


extern IDODViewPortMsgs *gDODViewPortMsgs;

#endif // IDODVIEWPORTMSGS_H