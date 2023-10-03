#ifndef _INCLUDED_ASW_INFO_MESSAGE_H
#define _INCLUDED_ASW_INFO_MESSAGE_H
#pragma once

#ifdef CLIENT_DLL
#define CASW_Info_Message C_ASW_Info_Message
#else
class CASW_Player;
#endif

// this entity can be placed by mappers and used to display a message when triggered

class CASW_Info_Message : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Info_Message, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CASW_Info_Message();
	virtual ~CASW_Info_Message();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
	void Precache();
	void Spawn();
	virtual int	 ShouldTransmit( const CCheckTransmitInfo *pInfo );
	void InputShowMessage( inputdata_t &inputdata );
	void InputStopSound( inputdata_t &inputdata );
	void Activate();

	string_t m_Key_WindowTitle;
	string_t m_Key_MessageLine1;
	string_t m_Key_MessageLine2;
	string_t m_Key_MessageLine3;
	string_t m_Key_MessageLine4;
	string_t m_Key_MessageSound;
	string_t m_Key_MessageImage;

	const char* GetTitle() { return m_WindowTitle.Get(); }
	const char* GetLine1() { return m_MessageLine1.Get(); }
	const char* GetLine2() { return m_MessageLine2.Get(); }
	const char* GetLine3() { return m_MessageLine3.Get(); }
	const char* GetLine4() { return m_MessageLine4.Get(); }
	const char* GetSound() { return m_MessageSound.Get(); }

	CNetworkString( m_WindowTitle, 128 );
	CNetworkString( m_MessageLine1, 255 );
	CNetworkString( m_MessageLine2, 255 );
	CNetworkString( m_MessageLine3, 255 );
	CNetworkString( m_MessageLine4, 255 );
	CNetworkString( m_MessageSound, 255 );
	CNetworkString( m_MessageImage, 255 );

	virtual void OnMessageRead(CASW_Player *pPlayer);
	COutputEvent m_OnMessageRead;

#else
	const char* GetTitle() { return m_WindowTitle; }
	const char* GetLine1() { return m_MessageLine1; }
	const char* GetLine2() { return m_MessageLine2; }
	const char* GetLine3() { return m_MessageLine3; }
	const char* GetLine4() { return m_MessageLine4; }
	const char* GetSound() { return m_MessageSound; }
	const char* GetImageName() { return m_MessageImage; }

	char m_WindowTitle[255];
	char m_MessageLine1[255];
	char m_MessageLine2[255];
	char m_MessageLine3[255];
	char m_MessageLine4[255];
	char m_MessageSound[255];
	char m_MessageImage[255];
#endif

	CNetworkVar(int, m_iWindowSize);
};


#endif /* _INCLUDED_ASW_INFO_MESSAGE_H */
