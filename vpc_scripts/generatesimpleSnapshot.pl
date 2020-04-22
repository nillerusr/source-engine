use strict;
use File::Path;

print( "\nDeleting old backup\n" );
system( "rmdir \/s \/q backup" );

print( "\nMoving previous snapshot to backup\n" );
system( "ren snapshot backup" );

print( "\nSearching for .vcproj files...\n" );

# Find all vcproj's in src
system( "dir \/s U:\\main\\src\\*.vcproj \> vcproj.txt" );

# Read in the source file

open(INFILE, "vcproj.txt" );
my @lines = <INFILE>;
close( INFILE );

# Process the file one line at a time

my @output;

# print the header lines
push( @output, "\/\/ VGC file\n" );
push( @output, "\n" );
push( @output, "\/\/\n" );
push( @output, "\/\/ Project definitions\n" );
push( @output, "\/\/\n" );
push( @output, "\n" );

for( my($i) = 0; $i < @lines; ++$i )
{
	# Grab the path
	if ( $lines[$i] =~ /Directory of U:\\main\\src\\(.*)/ )
	{
		my($path) = $1;
		++$i;

		# ignore projects in vpc_scripts!
		if ( $path =~ /vpc_scripts/ )
		{
			next;
		}

		# Grab the .vcproj filenames
		while ( $lines[++$i] =~ /[\d+\/]{2}\d+\s+\S+\s+\w{2}\s+\S+\s+(\w+).vcproj/ )
		{
			if ( $1 =~ /_x360/ )
			{
				next;
			}
			my($projectName) = $1;

			my($fullpath) = join('\\', "snapshot", $path );
			mkpath ($fullpath);

			my($fullname) = join('\\', $fullpath, $projectName );

			print "\nProcessing $projectName\n\n";

			# generate the vpc
			system ( "generatesimpleVPC.pl ..\\$path\\$projectName -o $fullpath" );

			# copy the .vcproj into the script tree
			system ( "copy /y ..\\$path\\$projectName.vcproj $fullpath" );
		}
	}
}




