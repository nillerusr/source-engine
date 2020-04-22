//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IRESPONSE_H
#define IRESPONSE_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Purpose: Callback interface for server updates
//-----------------------------------------------------------------------------
class IResponse
{
public:
	// called when the server has successfully responded
	virtual void ServerResponded() = 0;

	// called when a server response has timed out
	virtual void ServerFailedToRespond() = 0;

};


#endif // IRESPONSE_H
