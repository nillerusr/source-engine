This doc describes the setup requirements to allow python to run as an embedded vscript language within your Valve game application.  It also describes a C++ <-> python development environment.

Running Python Scripts From Your Game
-------------------------------------
1. You must install the python 2.5.1 interpreter onto you machine.  We only support python version 2.5.1.  It can be found online from python.org http://python.org/download/releases/2.5.1/.  Install python-2.5.1.msi.  

2. You must at minimum modify your system PATH (my computer->properties->advanced->environment variables) to point to the python.exe in the python installation.  Mine points to c:\python25, which is where I installed python. 

3. You should also create a new system variable called PYTHONPATH.  This is where python looks for new 'import' modules as it runs.  For now, it can be empty.  Later, you may want to point it at directories containing your python script files.

4. Run hl2.exe with '-scriptlang python' to invoke python as the default vscript language.

Debugging Python
----------------

Using: Wing IDE
---------------
I recommend installing Wing IDE Professional.  It's far easier to set up than Eclipse+PyDev as an IDE. 
Wing's project view is trivial to set up over an existing directory structure.  The debugger integrates easily with any python code that is launched from an exe running in Visual Studio (our vscript scenario).  Just follow the setup instructions after downloading Wing IDE 3.1.8 from WINGWARE.COM.  Look in Wing's help documentation, under 'advanced debugging', to find instructions on how to debug a python module which is launched from outside the IDE.  It's about 3 steps.

Using: PyDev & Eclipse
----------------------
Eclipse is a free IDE originally developed by IBM to be a general purpose IDE. It requires you to install a java runtime version 1.4 to run it. Pydev and Pydev Extensions are also both needed if you want to debug python in Eclipse. When the Pydev plugins are installed in Eclipse, they allow excellent - and free - python development and debugging support.  This configuration is quite tricky to set up, but very nice to use once you get it running. It has a somewhat obscure 'Workspace' project structure that imposes itself on your directory structure, which can be messy. 

You can download eclipse from eclipse.org - you'll need an older version - no newer than 3.4

You can download pydev and pydev extensions from pydev.org.  You must follow the extensive installation instructions carefully.  You MUST install pydev extensions in addition to basic pydev, or you will not be able to debug from VC using remote debugging.

Follow the complete instructions for remote debugging here: 

http://fabioz.com/pydev/manual_adv_remote_debugger.html

