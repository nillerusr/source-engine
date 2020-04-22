###############################################################
#
# generateDEF.pl
#
# Parses a .map file and generates a .def file
# for exporting functions from a dll.
#
# Note: Map Exports must be enabled in the project properties
#
###############################################################

my @baselist;
my @output;
my $baseLen 	= 63;

######################################
# Adds tabs for better formatting

sub Add_Tabs
{
	my $name = shift;
	my $num = int( ($baseLen - length( $name )) / 8 );

	for( $i = 0; $i < $num; $i++ )
	{
		push( @output, "\t" );
	}
}

######################################
# Open, validate, and read a file (not implemented yet)

sub Read_File
{
	my $name = shift;
	my @file = shift;

	# read in the file
	if ( open(INFILE, "$name" ) )
	{
		@file = <INFILE>;
		close( INFILE );
	}
	else
	{
		print( "Error opening file $name\n" );
		exit 1;
	}	
}

#####################################
# Start of script

print( "Valve Software - make360def.pl\n" );
print( "Copyright (c) 1996-2006, Valve LLC, All rights reserved.\n" );

my $filename 	= $ARGV[0];
my $numArgs 	= 1;
my @lines	= ();
my @deflines	= ();

if ( $ARGV[0] =~ /-check$/i )
{
	$numArgs = 3;
	$check = 1;
	$filename = $ARGV[1];
	$defname = $ARGV[2];
}
elsif ( $ARGV[0] =~ /-checkauto/i )
{
	$defname = $ARGV[1];
	$check = 2;
}

if ( @ARGV < $numArgs )
{
	print( "ERROR: Missing filename(s)\n" );
	exit 1;
}

if ( $check == 1 )
{
	# swap filenames if necessary
	if ( $filename =~ /.def/ )
	{
		my $temp = $filename;
		$filename = $defname;
		$defname = $temp;
	}

	# validate extensions
	unless ( $filename =~ /.map/ && $defname =~ /.def/ )
	{
		print( "ERROR: Invalid file extensions. -check requires a .map file and a .def file.\n" );
		exit 1;
	}

	# read in the def file
	if ( open(INFILE, "$defname" ) )
	{
		@deflines = <INFILE>;
		close( INFILE );
	}
	else
	{
		print( "ERROR: Couldn't open file $defname.\n" );
		exit 1;
	}	
}
elsif ( $check == 2 )
{
	# read in the def file
	if ( open(INFILE, "$defname" ) )
	{
		@deflines = <INFILE>;
		close( INFILE );
	}
	else
	{
		print( "ERROR: Couldn't open file $defname.\n" );
		exit 1;
	}

	# validate that the first export is CreateInterface*
	# validate that the export ordinals are sequential and ordered

	my $line;
	my $start = false;
	my $idx = 1;
	for ( @deflines )
	{
		$line = $_;
		
		if ( $line =~ /@(\d+)[ |\n]/ )
		{
			if ( $1 == 1 )
			{
				unless ( $line =~ /CreateInterface/ )
				{
					# first export must be CreateInterface*
					$line =~ /\s+([\S]*)/;

					print( "**************************************************\n" );
					print( "  ERROR: First export must be CreateInterface*.	  \n" );
					print( "  Export \"", $1, "\" found instead!		  \n" );
					print( "  This is a FATAL ERROR with the def file.	  \n" );
					print( "  Please contact an Xbox 360 engineer immediately.\n" );
					print( "**************************************************\n" );
					exit 1;
				}
			}

			if ( $1 != $idx )
			{
				# exports are out of order
				print( "**************************************************\n" );
				print( "  ERROR: Def file exports are not sequential	  \n" );
				print( "  This may cause unexpected behavior at runtime.  \n" );
				print( "  Please contact an Xbox 360 engineer immediately.\n" );
				print( "**************************************************\n" );
				exit 1;
			}

			++$idx;
		}
	}
	
	exit 0;	
}

# Get the base name

$filename =~ /(\w+(\.360)?).map/;
my $basename = $1;

# Read in the source file

if ( open(INFILE, "$filename" ) )
{
	@lines = <INFILE>;
	close( INFILE );
}
else
{
	print( "ERROR: Couldn't open file $filename.\n" );
	exit 1;
}

# Delete the lines up to the exports section

my $len = 0;
my $exportsFound = 0;
for( @lines ) 
{
	$len++;
	if( /^ Exports$/ )
	{
		splice( @lines, 0, $len+3 );
		$exportsFound = 1;
	}
}

if ( $exportsFound == 0 )
{
	print( "ERROR: No Exports section found in $filename. " );
	print( "Relink the project with 'Map Exports' enabled.\n" );
	exit 1;
}

if ( $check == 1 )
{
	# Check for exports in the map that aren't in the def
	print( "make360def: Comparing $filename and $defname\n" );

	# strip the first 2 lines from the def
	splice( @deflines, 0, 2 );

	my $defEntryCt = $#deflines + 1;
	my $defMatches = 0;

	# for each line in the map
	for( @lines )
	{
		my $found = 0;

		# Pull the export name from the map line

		my $mapline;
		if ( /(\d+)\s+(\S+)/ )
		{
			$mapline = $2;
		}
		else
		{
			# ignore this line
			next;
		}

		# for each line in the def
		for( @deflines )
		{
			/(\S+)/;

			if ( $1 =~ /^\Q$mapline\E$/ )
			{
				$found = 1;
				$defMatches++;
				last;
			}
		}

		if ( $found == 0 )
		{	
			print( "ERROR: New export found in $filename, " );
			print( "so the map file and def file are out of sync. " );
			print( "You must relink the project to generate a new def file.\n" );
			exit 1;
		}
	}

	# Make sure all the def lines were matched
	if ( $defMatches != $defEntryCt )
	{
		print( "ERROR: An export was removed from $filename, " );
		print( "so the map file and def file are out of sync. " );
		print( "You must relink the project to generate a new def file.\n" );
		exit 1;
	}

	print( "make360def: Comparison complete, files match.\n" );
	exit 0;	
}

# start the def file

print( "make360def: Generating $basename.def\n" );

push( @output, "LIBRARY\t$basename.dll\n" );
push( @output, "EXPORTS\n" );

# process each line in the export section

my $interfacePrefix = "0000000000";
for( @lines )
{
	if ( /(\d+)\s+(\S+)/ )
	{
		my $func = $2;
		if ( $func =~ /CreateInterface/ )
		{
			# Force createInterface to sort first
			$func = join( '', $interfacePrefix, $func );
		}

		push( @baselist, $func );
	}
}

# sort the list
my @sortedlist = sort {uc($a) cmp uc($b)} @baselist;
#my @sortedlist = @baselist;

my $ordinal = 1;
for( @sortedlist )
{
	my $func = $_;
	if ( /$interfacePrefix(.*)/ )
	{
		# Strip the added characters
		$func = $1;
	}

	push( @output, "\t$func\t" );
	Add_Tabs( $func );
	push( @output, "\@$ordinal\n" );
	$ordinal++;
}

# write the def file

print( "make360def: Saving $basename.def.\n" );

open ( OUTFILE, ">$basename.def" );
print OUTFILE @output;
close ( OUTFILE );

print( "make360def: Finished.\n" );

exit 0;
