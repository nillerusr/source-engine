use strict;

my $forceBuild = 0;
if ( $ARGV[1] =~ /-forcebuild/ )
{
	$forceBuild = 1;
}

# Read in the list of changed maps

my $filename = "buildlist.txt";

open(INFILE, $filename);
my @maps = <INFILE>;
close( INFILE );

# forcelist.txt allows maps to be manually added to the build. 

open(INFILE, "forcelist.txt");
my @forcemaps = <INFILE>;
close( INFILE );

my $retval = 1;

system( "echo. > $filename" );
system( "echo. > forcelist.txt" );

# Run through the list of maps to see if any should be built

for( @maps) 
{
	if( /updating (.*)|adding (.*)/ )
	{
		# Check if this map should be built now
		if ( shouldBuild( $1 ) == 0 )
		{
			next;
		}

		# Add this map name to the build list

		system( "p4 sync -f $1 >> $filename" );
		system( "echo Adding to the build list: $1 >> log.txt" );
		$retval = 0;
	}
}

for( @forcemaps )
{
	if ( /\.vmf/ )
	{
		# Add this map name to the build list

		$_ =~ /(.*)/;
		system( "p4 sync -f $1 >> $filename" );
		system( "echo Forcing add to the build list: $1 >> log.txt" );
		$retval = 0;
	}
}

exit $retval;

#-----------------------------------
# Check if this map should build now
#-----------------------------------
sub shouldBuild
{
	my $map = shift;

	# if command line flag was set, build the map

	if ( $forceBuild == 1 )
	{
		return 1;
	}

	# if the map line contains the force flag, build the map

	if ( $map =~ /-forcebuild/ )
	{
		return 1;
	}

	# Dump the comments from the last checkin of this map

	system( "p4 changes -m 1 -s submitted -l $map > comments.txt" );

	# parse comments for the autocompile keyword

	open(INFILE, "comments.txt");
	my @comments = <INFILE>;
	close(INFILE);

	for( @comments )
	{
		if ( /autocompile/i )
		{
			return 1;
		}
	}

	return 0;
}
