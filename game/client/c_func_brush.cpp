#include "cbase.h"
#include "c_func_brush.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_FuncBrush, DT_FuncBrush, CFuncBrush )
END_RECV_TABLE()


void C_FuncBrush::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
	VPhysicsShadowDataChanged(bCreate, this);
}


