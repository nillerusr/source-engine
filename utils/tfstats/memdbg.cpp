//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: code to track allocations via replacing the global new operator
//	some of this code was written by Paul Andre LeBlanc 
//	<paul.a.leblanc@sympatico.ca> I got it off of dejanews.com
//	usage: new TRACKED object-type
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include <new.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream.h>


#ifdef _DEBUG
#ifdef _MEMDEBUG
#define _MDEBUG
#endif
#endif

#ifdef _MDEBUG
static int numBytesAllocated=0;
//these were written by me, wes cumberland, not paul andre leblanc
void * operator new(size_t size)
{
	void *ptr = malloc(size);
	numBytesAllocated+=size;
	return ptr;
}

void * operator new[](size_t size)
{
	void *ptr = malloc(size);
	numBytesAllocated+=size;
	return ptr;
}

void operator delete(void* ptr)
{
	 free(ptr);
}

void operator delete[](void* ptr)
{
	free(ptr);
}


//this code will track allocations
//this code was written by Paul Andre LeBlanc <paul.a.leblanc@sympatico.ca>
//I got it off of dejanews.com
void *operator new(size_t size, const char *file, const int line) 
{
  void *ptr = new char[size];
  numBytesAllocated+=size;
  cout << "new: Allocating " << size << " bytes in file " << file << ", line " << line << ", address is " << ptr << " (" << numBytesAllocated<<" total allocated)"<< endl;
  return ptr;
}
 
void *operator new[](size_t size, const char *file, const int line) {
  void *ptr = new char[size];
  numBytesAllocated+=size;
  cout << "new[]: Allocating " << size << " bytes in file " << file << ", line " << line << ", address is " << ptr << " (" << numBytesAllocated<<" total allocated)" << endl;
  return ptr;
}
 
void operator delete(void *ptr, const char *file, const int line) {
  cout << "delete: Freeing memory allocated at file " << file << ", line " << line << ", address is " << ptr << endl;
  delete [] (char *) ptr;
}
 
void operator delete[](void *ptr, const char *file, const int line) 
{  
	cout << "delete[]: Freeing memory allocated at file " << file << ", line " << line << ", address is " << ptr << endl;
	delete [] (char *) ptr;
}

#endif
//------------------------------------------------------------------------------------------------------
// Function:	TFStats_win32_new_handler
// Purpose:	this function will be called if TFStats runs out of memory (unlikely)
//		this is a win32 specific version, the linux version does not pass an argument
// Input:	sz - the size of the allocation that failed
// Output:	int
//------------------------------------------------------------------------------------------------------
int TFStats_win32_new_handler(size_t sz)
{
	printf("TFStats ran out of memory trying to allocate %li bytes\n",sz);
	return 0;
}
//------------------------------------------------------------------------------------------------------
// Function:	TFStats_linux_new_handler
// Purpose:	this function will be called if TFStats runs out of memory (unlikely)
//		this is a linux specific version, the win32 version passes an argument
//------------------------------------------------------------------------------------------------------
void TFStats_linux_new_handler(void)
{
	printf("TFStats ran out of memory!\n");
}


//------------------------------------------------------------------------------------------------------
// Function:	TFStats_setNewHandler
// Purpose:	 sets the new handler to the TFStats new handler
//------------------------------------------------------------------------------------------------------
void TFStats_setNewHandler()
{
#ifdef WIN32
	_set_new_handler(TFStats_win32_new_handler);
	_set_new_mode(1);
#else
	std::set_new_handler(TFStats_linux_new_handler);
	//std::set_new_mode(1);
#endif
}
