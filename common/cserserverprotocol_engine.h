//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSERSERVERPROTOCOL_ENGINE_H
#define CSERSERVERPROTOCOL_ENGINE_H
#ifdef _WIN32
#pragma once
#endif

// NOTE:  These defined must match the ones in Steam's CSERServerProtocol.h!!!

#define C2M_REPORT_GAMESTATISTICS				'k'
	#define C2M_REPORT_GAMESTATISTICS_PROTOCOL_VERSION_1	1
	#define C2M_REPORT_GAMESTATISTICS_PROTOCOL_VERSION	2

	typedef enum
	{
		GS_UNKNOWN = 0,
		GS_NO_UPLOAD,
		GS_UPLOAD_REQESTED,

		// Must be last
		GS_NUM_TYPES
	} EGameStatsEnum;

	// C2M_REPORT_GAMESTATISTICS details (OLD VERSION)
	//		u8(C2M_REPORT_GAMESTATISTICS_PROTOCOL_VERSION_1)
	//		u32(build_number)
	//		string( exename )
	//		string( gamedir )
	//		string( mapname )
	//		u32 requested upload data length

	// C2M_REPORT_GAMESTATISTICS details (current version)
	//		u8(C2M_REPORT_GAMESTATISTICS_PROTOCOL_VERSION)
	//		u32(appID)
	//		u32 requested upload data length

#define M2C_ACKREPORT_GAMESTATISTICS			'l'
	// M2C_ACKREPORT_GAMESTATISTICS details
	//	u8(protocol okay (bool))
	//	u8(GS_NO_UPLOAD or GS_UPLOAD_REQESTED )
	//  iff GS_UPLOAD_REQESTED then add:
	//    u32(harvester ip address)
	//	  u16(harvester port #)
	//	  u32(upload context id)

#define C2M_PHONEHOME							'm'
	#define C2M_PHONEHOME_PROTOCOL_VERSION	3

	// C2M_PHONEHOME
	//	u8( C2M_PHONEHOME_PROTOCOL_VERSION )
	//	u32( sessionid ) or 0 to request a new sessionid
	//  u16(encryptedlength)
	//  remainder = encrypteddata:
		// u8 corruption id == 1
		//  string build unique id
		//  string computername
		//	string username
		//  string gamedir
		//  float( enginetimestamp )
		//  u8 messagetype:
		//    1:  engine startup 
		//    2:  engine shutdown
		//    3:  map started + mapname
		//    4:  map finished + mapname
		//	string( mapname )

#define M2C_ACKPHONEHOME						'n'
	// M2C_ACKPHONEHOME details
	//	u8(connection allowed (bool))
	//  u32(sessionid)

#define C2M_BUGREPORT						'o'

	#define C2M_BUGREPORT_PROTOCOL_VERSION			3

		// C2M_BUGREPORT details
		//		u8(C2M_BUGREPORT_PROTOCOL_VERSION)
		//		u16(encryptedlength)
		//		remainder=encrypteddata

		// encrypted payload:
		//		byte corruptionid = 1
		//		u32(buildnumber)
		//		string(exename 64)
		//		string(gamedir 64)
		//		string(mapname 64)
		//		u32 RAM
		//		u32 CPU
		//		string(processor)
		//		u32 DXVerHigh
		//		u32 DXVerLow
		//		u32	DXVendorID
		//		u32 DXDeviceID
		//		string(OSVer)
		
		// Version 2+:
		//	{
		//			reporttype(char 32)
		//			email(char 80)
		//			accountname(char 80)
		//	}

		// Version 3+
		//  {
		//			userid( sizeof( TSteamGlobalUserID ) )
		//  }

		// --- all versions
		//		string(title 128)
		//		u32(.zip file size, or 0 if none available)
		//		u32(text length > max 1024)
		//		text(descriptive text -- capped to text length bytes)

#define M2C_ACKBUGREPORT					'p'

	typedef enum
	{
		BR_UNKNOWN = 0,
		BR_NO_FILES,
		BR_REQEST_FILES,

		// Must be last
		BR_NUM_TYPES
	} EBugReportAckEnum;

		// M2C_ACKBUGREPORT details
		//	u8(protocol okay (bool))
		//	u8(BR_NO_FILES or BR_REQEST_FILES )
		//  iff BR_REQEST_FILES then add:
		//    u32(harvester ip address)
		//	  u16(harvester port #)
		//	  u32(upload context id)

// Arbitrary encrypted data upload
#define C2M_UPLOADDATA						'q'

	#define C2M_UPLOADDATA_PROTOCOL_VERSION			1

	#define C2M_UPLOADDATA_DATA_VERSION				1

		// C2M_BUGREPORT details
		//		u8(C2M_UPLOADDATA_PROTOCOL_VERSION)
		//		u16(encryptedlength)
		//		remainder=encrypteddata

		// encrypted payload:
		//		byte(corruptionid)
		//		byte(protocolid) // C2M_UPLOADDATA_DATA_VERSION
		//		string(tablename 40)
		//		u8(numvalues)
		//		for each value:
		//			string(fieldname 32)
		//			string(value 128)

#define M2C_ACKUPLOADDATA					'r'

		// M2C_ACKUPLOADDATA details
		//	u8(protocol okay (bool))


#endif // CSERSERVERPROTOCOL_ENGINE_H
