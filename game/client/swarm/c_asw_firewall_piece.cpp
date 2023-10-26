#include "cbase.h"
#include "c_asw_firewall_piece.h"
#include "c_asw_generic_emitter.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Firewall_Piece, DT_ASW_Firewall_Piece, CASW_Firewall_Piece)
	
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Firewall_Piece )

END_PREDICTION_DATA()



C_ASW_Firewall_Piece::C_ASW_Firewall_Piece()
{

}


C_ASW_Firewall_Piece::~C_ASW_Firewall_Piece()
{
	
}

void C_ASW_Firewall_Piece::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// NOTE: Removed this - instead firewall pieces use FireSystem_StartFire on the server,
		//  which creates normal fires with their own emitters
		//CreateFireEmitter();		
		return;
	}
}

void C_ASW_Firewall_Piece::CreateFireEmitter()
{
	m_hFireEmitter = CASWGenericEmitter::Create( "asw_emitter" );

	if ( m_hFireEmitter.IsValid() )
	{			
		m_hFireEmitter->UseTemplate("incendiary");
		m_hFireEmitter->SetActive(true);
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		Warning("Failed to create a firewall's fire emitter\n");
	}
}

void C_ASW_Firewall_Piece::ClientThink()
{
	BaseClass::ClientThink();
	if ( m_hFireEmitter.IsValid() )
	{
		m_hFireEmitter->Think(gpGlobals->frametime, GetAbsOrigin(), GetAbsAngles());
	}
}