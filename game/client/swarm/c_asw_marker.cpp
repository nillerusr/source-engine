#include "cbase.h"
#include "c_asw_marker.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_AUTO_LIST( IObjectiveMarkerList );


IMPLEMENT_CLIENTCLASS_DT(C_ASW_Marker, DT_ASW_Marker, CASW_Marker)
	RecvPropString( RECVINFO( m_ObjectiveName ) ),
	RecvPropInt( RECVINFO( m_nMapWidth ) ),
	RecvPropInt( RECVINFO( m_nMapHeight ) ),
	RecvPropBool( RECVINFO( m_bComplete ) ),
	RecvPropBool( RECVINFO( m_bEnabled ) ),
END_RECV_TABLE()

