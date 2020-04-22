//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interface of CDialogueReadout
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef DIALOGUEREADOUT_H
#define DIALOGUEREADOUT_H
#ifdef WIN32
#pragma once
#endif
#include "Report.h"

//------------------------------------------------------------------------------------------------------
// Purpose: CDialogueReadout is a full page report element that outputs a listing
// of all the dialogue in the match.  It also reports deaths, and suicides since those
// usually beget lots of smack talking.
//------------------------------------------------------------------------------------------------------
class CDialogueReadout : public CReport
{
private:
public:
	explicit CDialogueReadout(){}
	void writeHTML(CHTMLFile& html);
};
#endif // DIALOGUEREADOUT_H
