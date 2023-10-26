#include "cbase.h"
#include "c_asw_scanner_info.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "c_asw_marine.h"
#include "asw_gamerules.h"
#include "c_asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Scanner_Info, DT_ASW_Scanner_Info, CASW_Scanner_Info)
	RecvPropArray3( RECVINFO_ARRAY(m_iBlipX), RecvPropInt( RECVINFO(m_iBlipX[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iBlipY), RecvPropInt( RECVINFO(m_iBlipY[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_index),		RecvPropInt( RECVINFO(m_index[0]) ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_BlipType),		RecvPropInt( RECVINFO(m_BlipType[0]) ) ),
END_RECV_TABLE()

C_ASW_Scanner_Info::C_ASW_Scanner_Info()
{
	for (int i=0;i<ASW_SCANNER_MAX_BLIPS;i++)
	{
		m_iBlipX.Set(i, 0);
		m_iBlipY.Set(i, 0);
		m_index.Set(i, 0);
		m_BlipType.Set(i, 0);
		m_fClientBlipX[i] = 0;
		m_fClientBlipY[i] = 0;
		m_ClientBlipIndex[i] = 0;
		m_ClientBlipType[i] = 0;
		m_bClientNewBlip[i] = false;
	}

}

C_ASW_Scanner_Info::~C_ASW_Scanner_Info()
{

}

// copies server blip values over to the client rendered ones
void C_ASW_Scanner_Info::CopyServerBlips()
{
	for (int i=0;i<ASW_SCANNER_MAX_BLIPS;i++)
	{
		m_fClientBlipX[i] = m_iBlipX.Get(i);
		m_fClientBlipY[i] = m_iBlipY.Get(i);
		m_bClientNewBlip[i] = (m_ClientBlipIndex[i] == m_index.Get(i));		
		m_ClientBlipIndex[i] = m_index.Get(i);
		m_ClientBlipType[i] = m_BlipType.Get(i);
		//todo: if this is a new blip and our blip still had some strength, copy the old blip into an overflow slot so it can fade out completely
	}
}

// fades out the blips
#define ASW_SCANNER_BLIP_FADE_RATE 1.5f
void C_ASW_Scanner_Info::FadeBlips()
{
	for (int i=0;i<ASW_SCANNER_MAX_BLIPS;i++)	// todo: fade out overflow blips too
	{
		m_fBlipStrength[i] = MAX(0, m_fBlipStrength[i] - gpGlobals->frametime * ASW_SCANNER_BLIP_FADE_RATE);		
	}
}