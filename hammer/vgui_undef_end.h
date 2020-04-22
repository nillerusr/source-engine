//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

// define windows function macros again 
#ifdef UNICODE
	#define PostMessage  PostMessageW
	#define GetClassName GetClassNameW
	#define SendMessage  SendMessageW
#else
	#define PostMessage  PostMessageA
	#define GetClassName GetClassNameA
	#define SendMessage  SendMessageA
#endif // !UNICODE

