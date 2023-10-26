#include "cbase.h"
#include "c_asw_t75.h"
#include "dlight.h"
#include "iefx.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "vguimatsurface/imatsystemsurface.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_T75, DT_ASW_T75, CASW_T75)
	RecvPropBool( RECVINFO( m_bArmed ) ),
	RecvPropBool(RECVINFO(m_bIsInUse)),
	RecvPropFloat(RECVINFO(m_flArmProgress)),
	RecvPropTime(RECVINFO(m_flDetonateTime)),
END_RECV_TABLE()

bool C_ASW_T75::s_bLoadedArmIcons = false;
int  C_ASW_T75::s_nArmIconTextureID = -1;

C_ASW_T75::C_ASW_T75()
{

}

C_ASW_T75::~C_ASW_T75()
{

}

int C_ASW_T75::GetT75IconTextureID()
{
	if (!s_bLoadedArmIcons)
	{
		// load the portrait textures
		s_nArmIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nArmIconTextureID, "vgui/swarm/UseIcons/UseIconTakeAmmoDrop", true, false);
		s_bLoadedArmIcons = true;
	}

	return s_nArmIconTextureID;
}

bool C_ASW_T75::IsUsable(C_BaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

bool C_ASW_T75::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	if ( IsArmed() )
	{
		action.iUseIconTexture = GetT75IconTextureID();
		TryLocalize( "#asw_t75_armed", action.wszText, sizeof( action.wszText ) );
		action.UseTarget = this;
		action.fProgress = -1;
	}
	else
	{
		action.iUseIconTexture = GetT75IconTextureID();
		TryLocalize( "#asw_t75_arm", action.wszText, sizeof( action.wszText ) );
		action.UseTarget = this;
		action.fProgress = GetArmProgress();
	}
	action.UseIconRed = 255;
	action.UseIconGreen = 255;
	action.UseIconBlue = 255;
	action.bShowUseKey = true;
	action.iInventorySlot = -1;
	return true;
}

void C_ASW_T75::CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon)
{
	// TODO: arming interface, sound, etc.	
}