#ifndef _INCLUDED_ASW_SYSTEM_H
#define _INCLUDED_ASW_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "filesystem.h"

bool ASW_System_GetCurrentTimeAndDate(int *year, int *month, int *dayOfWeek, int *day, int *hour, int *minute, int *second);

const char *Sys_FindFirst (FileFindHandle_t &searchhandle, const char *path, char *basename, int namelength );
const char* Sys_FindNext (FileFindHandle_t &searchhandle, char *basename, int namelength);
void Sys_FindClose (FileFindHandle_t &searchhandle);

#endif _INCLUDED_ASW_SYSTEM_H