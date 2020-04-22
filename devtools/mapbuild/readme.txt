These map autobuild scripts are set up to detect whever a vmf has been checked into Perforce, then check out the bsp, rebuild the bsp with cubemaps, and check it back in.  It's a work-in-progress, so there is room for improvements (better error-handling, building on multiple machines, etc.).

If you have any questions or problems, contact me at kerry@valvesoftware.com


To set up a build machine to use the autobuild scripts:


1. The build machine must have a Perforce client, and be fully synced to the game tree and the content/<mod>/maps directory. To test that your environment is setup correctly, the build machine must be able to do the following:

a) Run your full game in ldr and hdr.
b) Compile a bsp from a vmf (Run vbsp, vvis, vrad - make sure 'vproject' is set to the correct mod.)
c) Check files in and out from the tree that you will be building.
c) Run 'p4.exe' from the command line. (Make sure the build machine's DEFAULT Perforce client is set to the Client that will be doing the checkins.)
d) Run 'perl.exe' from the command line


2. Sync to the 'mapbuild' directory, located at src/devtools/mapbuild.


3. Copy 'mapautocompile.txt'from game/hl2/scripts into your game/<mod>/scripts directory. Delete all map entries except for "default".

This file specifies the command line arguments that should be passed to vbsp, vvis, and vrad.
The 'defualt' block of arguments is used for any maps that don't have their own arguments specified.
If a map has its own arguments, they will override the default arguments.
NOTE: This file isn't required - without it, the tools will run with no command line arguments.  This file should be checked into Perforce to let level designers tweak their own compile settings.


4. For each mod you want to autobuild, add this line to 'buildall.bat':

call buildmod <mod> <relative path to gamedir>

NOTE: The 'Sleep' call is just there to prevent the script from spamming Perforce. The sleep time is entirely up to you.


5. In 'buildMaps.pl', search for the comment "# Generate the checkin file".  This is where the text file is created that Perforce needs to do a checkin from the command line.  You will need to update the following fields to match the setup of the build machine:

'Client: <default client>'
'User: <name>'
'File: <file path in Perforce>'

NOTE: To see exactly what the values of these fields should be, you can type 'p4 submit' on the command line to see a Perforce-generated checkin file.  


6. Run 'buildall.bat'.  Check in a vmf and make sure the entire process runs successfully.
