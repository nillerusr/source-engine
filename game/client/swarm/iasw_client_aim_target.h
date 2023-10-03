#ifndef _INCLUDED_ASW_CLIENT_AIM_TARGET_H
#define _INCLUDED_ASW_CLIENT_AIM_TARGET_H


DECLARE_AUTO_LIST( IClientAimTargetsAutoList );

abstract_class IASW_Client_Aim_Target : public IClientAimTargetsAutoList
{
public:
	IASW_Client_Aim_Target( bool bAutoAdd = true ) : IClientAimTargetsAutoList( bAutoAdd ) {}
	virtual float GetRadius() = 0;
	virtual bool IsAimTarget() = 0;
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming=false) = 0;
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) = 0;
};


#endif // _INCLUDED_ASW_CLIENT_AIM_TARGET_H