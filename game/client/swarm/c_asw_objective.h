#ifndef CASW_OBJECTIVEINFO_H
#define CASW_OBJECTIVEINFO_H
#pragma once

#include "c_baseentity.h"
#include "asw_shareddefs.h"

class CASW_Marine_Profile;


// This class holds information about a particular objective

class C_ASW_Objective : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Objective, C_BaseEntity );
	DECLARE_CLIENTCLASS();

					C_ASW_Objective();
	virtual			~C_ASW_Objective();

	void OnDataChanged( DataUpdateType_t type );

	virtual void ClientThink( void );
	void UpdateMarkings( void );

	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	virtual const wchar_t* GetObjectiveTitle( void );
	const char* GetObjectiveImage( void ) { return m_ObjectiveImage; }
	const char* GetDescription( int i );
	int FindNextToken(const char* pText);
	ObjectiveMapMark* GetMapMarkings( void );
	int GetMapMarkingsCount( void ) const { return m_iNumMapMarks; }
	const char* GetInfoIcon( int i );
	const char* GetObjectiveIconName( void );

	// allows this objective to paint its status on the HUD
	virtual void PaintObjective(float &current_y);

	virtual bool IsObjectiveComplete() { return m_bComplete; }
	virtual bool IsObjectiveFailed() { return m_bFailed; }
	virtual bool IsObjectiveOptional() { return m_bOptional; }
	virtual bool IsObjectiveDummy() { return m_bDummy; }
	virtual bool IsObjectiveHidden() { return !m_bVisible; }
	virtual bool NeedsTitleUpdate() { return false; }	// most objectives don't have constantly changing titles, they can just be set once
	virtual float GetObjectiveProgress() { return IsObjectiveComplete() ? 1.0f : 0.0f; }

	char		m_ObjectiveTitle[255];
	char		m_ObjectiveDescription1[255];
	char		m_ObjectiveDescription2[255];
	char		m_ObjectiveDescription3[255];
	char		m_ObjectiveDescription4[255];
	char		m_ObjectiveImage[255];
	char		m_ObjectiveMarkerName[255];
	char		m_ObjectiveInfoIcon1[255];
	char		m_ObjectiveInfoIcon2[255];
	char		m_ObjectiveInfoIcon3[255];
	char		m_ObjectiveInfoIcon4[255];
	char		m_ObjectiveInfoIcon5[255];
	char		m_ObjectiveIcon[255];
	char		m_MapMarkings[255];	
	int			m_Priority;
	bool		m_bComplete, m_bFailed, m_bOptional, m_bDummy, m_bVisible;

	ObjectiveMapMark m_MapMarks[ ASW_NUM_MAP_MARKS ];
	int m_iNumMapMarks;
	char szTokenBuffer[ 256 ];

	int m_ObjectiveIconTextureID;
	int GetObjectiveIconTextureID();

private:
	C_ASW_Objective( const C_ASW_Objective & ); // not defined, not accessible
};

#endif /* CASW_OBJECTIVEINFO_H */
