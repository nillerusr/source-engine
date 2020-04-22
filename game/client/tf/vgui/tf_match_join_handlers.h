//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_MATCH_JOIN_HANDLERS_H
#define TF_MATCH_JOIN_HANDLERS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include "tf_controls.h"

class IMatchJoiningHandler
{
public:
	IMatchJoiningHandler();
	virtual ~IMatchJoiningHandler();

	virtual void MatchFound() = 0;

protected:

	void JoinMatch();
};

class CTFImmediateAutoJoinHandler : public IMatchJoiningHandler
{
public:
	CTFImmediateAutoJoinHandler();
	virtual void MatchFound() OVERRIDE;

private:

	float m_flNextAutoJoinTime;
};

class CTFRejoinConfirmDialog;
class CTFMatchMakingPopupPrompJoinHandler : public IMatchJoiningHandler
{
public:
	CTFMatchMakingPopupPrompJoinHandler();

	virtual void MatchFound() OVERRIDE;

	static void OnJoinLobbyInProgressCallback( bool bConfirmed, void *pContext );
private:

	void UpdatePromptState();

	static CTFRejoinConfirmDialog* m_pRejoinPrompt;
	float m_flNextRejoinThinkTime;
};

#endif // TF_MATCH_JOIN_HANDLERS_H
