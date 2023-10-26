#ifndef _INCLUDED_C_ASW_ORDER_ARROW_H
#define _INCLUDED_C_ASW_ORDER_ARROW_H
#ifdef _WIN32
#pragma once
#endif

class C_ASW_Order_Arrow : public C_BaseAnimating
{
	typedef C_BaseAnimating BaseClass;
public:
	
	static C_ASW_Order_Arrow* CreateOrderArrow();
	void RefreshArrow();

	float m_fCreatedTime;
};

#endif // _INCLUDED_C_ASW_ORDER_ARROW_H