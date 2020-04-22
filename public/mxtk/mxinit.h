//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxInit.h
// implementation: all
// last modified:  Apr 28 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#ifndef INCLUDED_MXINIT
#define INCLUDED_MXINIT



#ifdef WIN32
#include <windows.h>
#endif



class mxWindow;



class mx  
{
public:
	// NO CREATORS
	mx() {}
	virtual ~mx () {}

	// MANIPULATORS
	static int init (int argc, char *argv[]);
	static int run ();
	static int check ();
	static void quit ();
	static int setDisplayMode (int w, int h, int bpp);
	static void setIdleWindow (mxWindow *window);

	// ACCESSORS
	static int getDisplayWidth ();
	static int getDisplayHeight ();
	static mxWindow *getMainWindow ();
	static const char *getApplicationPath ();
	static int getTickCount ();

	enum
	{
		ACCEL_ALT		= (1<<0),  // The ALT key must be held down when the accelerator key is pressed.
		ACCEL_CONTROL	= (1<<1), // The CTRL key must be held down when the accelerator key is pressed.
		ACCEL_SHIFT		= (1<<2), // The SHIFT key must be held down when the accelerator key is pressed.
		ACCEL_VIRTKEY	= (1<<3), // The key member specifies a virtual-key code. If this flag is not specified, key is assumed to specify a character code.
	};

	// Based on windows.h ACCEL structure!!!
	struct Accel_t
	{
		Accel_t() :
			flags( 0 ),
			key( 0 ),
			command( 0 )
		{
		}
		unsigned char  flags; // one or more of above ACCEL_ flags
		unsigned short key; // Specifies the accelerator key. This member can be either a virtual-key code or a character code. 
		unsigned short command; // Specifies the accelerator identifier. This value is placed in the low-order word of the wParam parameter of the WM_COMMAND or WM_SYSCOMMAND message when the accelerator is pressed. 
	};

	static void createAccleratorTable( int numentries, Accel_t *entries );

private:
	// NOT IMPLEMENTED
	mx (const mx&);
	mx& operator= (const mx&);
};




#endif // INCLUDED_MXINIT
