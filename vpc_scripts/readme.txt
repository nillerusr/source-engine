Usage for the vpc-generating perl scripts::

(Note, perl is case sensitive.)


"generateVPC.pl"
"generatesimpleVPC.pl"

- Run from the vpc_scripts directory.
- Use full or relative path to the vcproj file. 
- Use -o to specify an output directory. If no output directory is specified, The vpc script(s) will be generated in the same directory as the source vcproj.
("generateSimpleVPC.pl" produces a stripped-down version of the output scripts)

Example:

> generateVPC.pl ..\cl_dll\client.vcproj -o ..\cl_dll\tempdir



"generateSnapshot.pl"
"generatesimpleSnapshot.pl"

Generates vpc scripts for the entire tree, and places them in a local mirror directory tree named "snapshot". If a snapshot tree already exists, it is renamed to "backup" and a new snapshot is created. This allows running a diff of the two trees to see which vcproj's have changed since the last snapshot.

No arguments are necessary:

> generateSnapshot.pl

 

NOTES:

generateVPC.pl always assumes a common base script and optional additional leaf scripts. If a file or property is identical across all project configurations, then it is placed in the base script. If any configuration is different, then that file or property is moved into the appropriate mod's leaf script. This is expected behavior for client and server, but most other projects should only have a base script.
