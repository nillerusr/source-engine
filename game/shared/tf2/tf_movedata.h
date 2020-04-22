//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_MOVEDATA_H
#define TF_MOVEDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "igamemovement.h"
#include "tfclassdata_shared.h"

class CPlayerClassData;

// This class contains TF-specific prediction data. CMoveData can be casted to this class in
// CTFPlayerMove and CTFGameMovement to do TF-specific movement.
class CTFMoveData : public CMoveData
{
public:

	Vector		m_vecPosDelta;

	// Revisit this!!!
	enum { MOMENTUM_MAXSIZE = 10 };
	float		m_aMomentum[MOMENTUM_MAXSIZE];
	int			m_iMomentumHead;

	int			m_nClassID;

	inline PlayerClassCommandoData_t	&CommandoData()		{ return m_CommandoData; }
	inline PlayerClassDefenderData_t	&DefenderData()		{ return m_DefenderData; }
	inline PlayerClassEscortData_t		&EscortData()		{ return m_EscortData; }
	inline PlayerClassInfiltratorData_t	&InfiltratorData()	{ return m_InfiltratorData; }
	inline PlayerClassMedicData_t		&MedicData()		{ return m_MedicData; }
	inline PlayerClassReconData_t		&ReconData()		{ return m_ReconData; }
	inline PlayerClassSniperData_t		&SniperData()		{ return m_SniperData; }
	inline PlayerClassSupportData_t		&SupportData()		{ return m_SupportData; }
	inline PlayerClassSapperData_t		&SapperData()		{ return m_SapperData; }
	inline PlayerClassPyroData_t		&PyroData()			{ return m_PyroData; }
	inline void*						VehicleData()		{ return m_VehicleData; }
	inline int							VehicleDataMaxSize()
	{
		return VEHICLE_DATA_SIZE;
	}

private:
	enum
	{
		VEHICLE_DATA_SIZE = 256
	};

	PlayerClassCommandoData_t m_CommandoData;
	PlayerClassDefenderData_t m_DefenderData;
	PlayerClassEscortData_t m_EscortData;
	PlayerClassInfiltratorData_t m_InfiltratorData;
	PlayerClassMedicData_t m_MedicData;
	PlayerClassReconData_t m_ReconData;
	PlayerClassSniperData_t m_SniperData;
	PlayerClassSupportData_t m_SupportData;
	PlayerClassSapperData_t m_SapperData;
	PlayerClassPyroData_t m_PyroData;

	unsigned char m_VehicleData[VEHICLE_DATA_SIZE];
};


#endif // TF_MOVEDATA_H
