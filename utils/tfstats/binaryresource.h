//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef BINARYRESOURCE_H
#define BINARYRESOURCE_H
#ifdef WIN32
#pragma once
#endif
#include <string>
#include <stdio.h>
#include "util.h"

class CBinaryResource
{
private:
	std::string filename;
	size_t numBytes;
	unsigned char* pData;
public:
	CBinaryResource(char* name, size_t bytes,unsigned char* data)
	:filename(name),numBytes(bytes),pData(data)
	{}
	
	bool writeOut()
	{
		FILE* f=fopen(filename.c_str(),"wb");
		if (!f)
			return false;
		fwrite(pData,1,numBytes,f);
		fclose(f);
#ifndef WIN32
		chmod(filename.c_str(),PERMIT);
#endif
		return true;
	}
};

#endif // BINARYRESOURCE_H

