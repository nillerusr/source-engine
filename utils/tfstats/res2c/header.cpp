//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
char* szHeaderFile=
"//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========\n"\
"//\n"\
"// The copyright to the contents herein is the property of Valve, L.L.C.\n"\
"// The contents may be used and/or copied only with the written permission of\n"\
"// Valve, L.L.C., or in accordance with the terms and conditions stipulated in\n"\
"// the agreement/contract under which the contents have been supplied.\n"\
"//\n"\
"// Purpose: \n"\
"//\n"\
"// $Workfile:     $\n"\
"// $Date:         $\n"\
"//\n"\
"//------------------------------------------------------------------------------------------------------\n"\
"// $Log: $\n"\
"//\n"\
"// $NoKeywords: $\n"\
"//=============================================================================\n"\
"#ifndef BINARYRESOURCE_H\n"\
"#define BINARYRESOURCE_H\n"\
"#ifdef WIN32\n"\
"#pragma once\n"\
"#endif\n"\
"#include <string>\n"\
"#include <stdio.h>\n"\
"\n"\
"class CBinaryResource\n"\
"{\n"\
"private:\n"\
"	std::string filename;\n"\
"	size_t numBytes;\n"\
"	unsigned char* pData;\n"\
"public:\n"\
"	CBinaryResource(char* name, size_t bytes,unsigned char* data)\n"\
"	:filename(name),numBytes(bytes),pData(data)\n"\
"	{}\n"\
"	\n"\
"	bool writeOut()\n"\
"	{\n"\
"		FILE* f=fopen(filename.c_str(),\"wb\");\n"\
"		if (!f)\n"\
"			return false;\n"\
"		fwrite(pData,1,numBytes,f);\n"\
"		fclose(f);\n"\
"		return true;\n"\
"	}\n"\
"};\n"\
"\n"\
"#endif // BINARYRESOURCE_H\n"\
"\n";