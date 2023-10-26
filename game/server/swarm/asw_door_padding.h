#ifndef _INCLUDED_ASW_DOOR_PADDING_H
#define _INCLUDED_ASW_DOOR_PADDING_H

#include "asw_door.h"

class CASW_Door_Padding : public CBaseAnimating
{
public:
	DECLARE_CLASS( CASW_Door_Padding, CBaseAnimating );	
	DECLARE_DATADESC();

	virtual ~CASW_Door_Padding();	

	void	Spawn( void );
	void	Precache();
	void	OnRestore( void );

	static CASW_Door_Padding *CreateDoorPadding( CASW_Door *pDoor );
};

class CASW_Fallen_Door_Padding : public CBaseAnimating
{
public:
	DECLARE_CLASS( CASW_Fallen_Door_Padding, CBaseAnimating );	
	DECLARE_DATADESC();	

	void	Spawn( void );
	void	Precache();
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual int UpdateTransmitState();

	static CASW_Fallen_Door_Padding *CreateFallenDoorPadding( CASW_Door *pDoor );
};


#endif /* _INCLUDED_ASW_DOOR_PADDING_H */