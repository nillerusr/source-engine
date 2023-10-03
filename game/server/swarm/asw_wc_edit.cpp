#include "cbase.h"
#include "Mathlib/mathlib.h"
#include "player.h"
#include "wcedit.h"
#include "ndebugoverlay.h"
#include "editor_sendcommand.h"
#include "movevars_shared.h"
#include "asw_wc_edit.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


void CC_ASW_WC_EditEmitter( void )
{
	// Only allowed in wc_edit_mode
	if (engine->IsInEditMode())
	{
		CBaseEntity::m_nDebugPlayer = UTIL_GetCommandClientIndex();
		
		ASW_WCEdit::EditEmitter(UTIL_GetCommandClient());		
	}
}
static ConCommand wc_edit_emitter("wc_edit_emitter", CC_ASW_WC_EditEmitter, "When in WC edit mode, this lets you modify a generic particle emitter", FCVAR_CHEAT);


void ASW_WCEdit::EditEmitter(	CBasePlayer *pPlayer )
{
	// find the emitter closest to the player
	// flash a red box around it
	// set it as the chosen edit emitter and launch the VGUI window for editing it
}