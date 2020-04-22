#========= Copyright 1996-2008, Valve Corporation, All rights reserved. ==========
#
# Master script for performing automated compiles of bsp's with cubemaps, reslists,
# and nodegraphs. Compiles can be triggered manually by adding the vmf's absolute
# path to mapbuild\forcelist.txt, or by adding the "autocompile" keyword 
# to the checkin comments for a vmf.
#
#=================================================================================

use strict;
use warnings;
use Net::SMTP;

my $maxCompiles = 3; 		# Maximum number of simultaneous compiles.
my $running = 0;
my @g_CompileList;
my @finalizeList;
my @g_Finalizing;
my @g_CompileThreads;
my %g_ToolArgs;
my %g_RunningCompiles;
my @g_OutputFiles;
my $g_FilenameIdx = 0;
my @g_MainDirs;
my $g_MainDirIdx = 0;
my @g_MapsToBuild;

my $buildbsp = 1;		# Compile the bsp with vbsp, vvis, and vrad
my $buildreslists = 1;		# Build reslists for the map
my $buildnodegraphs = 1;	# Build the nodegraph (.ain) for the map

my $rootdir = "..\\..\\..\\..";	# Relative path from this script to one level above the "main" directory

system( "echo. > finalize.txt" );

$SIG{CHLD}='IGNORE';

# Generate filenames for the compile output files

for ( my $ct = 0; $ct < $maxCompiles * 2; ++$ct )
{
	my $filename = join( '', "$rootdir\\main1\\src\\devtools\\mapbuild\\buildqueue", $g_FilenameIdx++, ".txt" );
	push( @g_OutputFiles, $filename );
}

# Generate main directory names for each simultaneous compile

for ( my $ct = 1; $ct <= $maxCompiles; ++$ct )
{
	my $filename = join( '', "main", $ct );
	push( @g_MainDirs, $filename );
}


# Loop for infinity. If changes are made to this script, the process needs to
# be killed and restarted for those changes to take effect.  Note, any maps that
# are compiling or waiting in the queue will NOT be picked up again when the script
# is restarted. Any maps that were compiling or waiting to compile when the process
# was stopped will need to be started manually by adding the vmf absolute path
# to forcelist.txt. A map can be manually added to the queue at any time while the script
# is running by adding the vmf absolute path to forcelist.txt.

while ( 1 )
{

# Generate a list of maps that need to be built. For vmf's that have been changed in
# Perforce, the checkin comments are parsed and if the word "autocompile"
# is found then that map will be added to the build list. All maps that are listed in 
# forcelist.txt are also added to the build list.

syncChangedMaps();


# Append the maps to the build queue while filtering out duplicates. We only
# want to filter duplicates out of the queue - if a map is already compiling
# then it's reasonable to add it to the queue again because it must have changed
# since the current build began.

my $updated = 0;
for my $map ( @g_MapsToBuild )
{
	if ( $map =~ /(\w*.vmf)/ )
	{
		# make sure it's not already in the queue

		my $duplicate = 0;
		my $testname = $1;
		for ( @g_CompileList )
		{
			if ( /$testname/ )
			{
				$duplicate = 1;
				last;
			}
		}

		system( "echo. >> log.txt" );

		if ( $duplicate != 1 )
		{
			print( "Adding to compile list $map\n" );
			system( "echo Adding to compile list $map >> log.txt" );
			push( @g_CompileList, $map );
			$updated = 1;
		}
		else
		{
			print( "Duplicate map found: $testname\n" );
			system( "echo Duplicate map found: $testname >> log.txt" );
		}
	}
}

splice( @g_MapsToBuild );

# If there is a build slot open, start building the next map

StartCompiles();

sleep( 60 ); # Time is arbitrary - just don't spam Perforce

# Load in the finalize queue

open( INFILE, "<finalize.txt" );
my @finalizeList = <INFILE>;
close( INFILE );

system( "echo. > finalize.txt" );

# Finalize the maps (cubemaps, reslist, nodegraph, and checkin). The finalize list
# also contains the compile output (times,args,etc.) that need to go in the checkin comments.

my $maindir	= "";
my $mod		= "";
my $mapname	= "";
my $strTime 	= "";
my $vbspargs 	= "";
my $vvisargs 	= "";
my $vradargs 	= "";
my $outfile	= "";

for ( @finalizeList )
{
	if ( /Finalize: (\w*) (\w*)/ )
	{
		$mod = $1;
		$mapname = $2;
		next;
	}
	elsif ( /maindir: (.*)/ )
	{
		$maindir = $1;
		next;
	}
	elsif ( /vbspargs: (.*)/ )
	{
		$vbspargs = $1;
		next;
	}
	elsif ( /vvisargs: (.*)/ )
	{
		$vvisargs = $1;
		next;
	}
	elsif ( /vradargs: (.*)/ )
	{
		$vradargs = $1;
		next;
	}
	elsif ( /time: (.*)/ )
	{
		$strTime = $1;
		next;
	}
	elsif ( /outfile: (.*)/ )
	{
		# This is the final entry for a map
		$outfile = $1;
	}
	else
	{
		next;
	}

	print( "\nFinalizing $mod\\$mapname.bsp\n" );
	system( "echo Finalizing $mapname.bsp >> log.txt" );

	# A compile slot has opened up

	--$running;
	push( @g_Finalizing, $mapname );

	# Let another compile start in the just-emptied slot.

	StartCompiles();

	my $ldrResult = 0;
	my $hdrResult = 0;

	# HACK: Make sure a zombie process can't hold up the system

	system( "taskkill /im hl2.exe /f" );

	chdir( "$rootdir\\$maindir\\src\\devtools\\mapbuild" );

	# Get the Perforce client and owner names for the checkin file
	my $g_client;
	my $g_owner;

	my @clientspec = readpipe "p4 client -o";

	for ( @clientspec )
	{
		if ( /^Client:[\s*](.*)/ )
		{
			$g_client = $1;
			print( "Using client $g_client\n" );
		}
		elsif ( /^Owner:[\s*](.*)/ )
		{
			$g_owner = $1;
			print( "Using owner $g_owner\n" );
		}
	}

	if ( $buildbsp == 1 )
	{
		# Build cubemaps

		system( "echo. >> $outfile" );
		system( "time /t >> $outfile" );
		system( "echo Building cubemaps for $mapname. >> $outfile" );

		if ( $vradargs =~ /-hdr|-both/ )
		{
			$hdrResult = system( "$rootdir\\$maindir\\game\\hl2.exe -allowdebug -game $mod -window -w 1152 -h 864 +mat_picmip 0 -dev +mat_hdr_level 2 +sv_cheats 1 +map $mapname -buildcubemaps" );
		}

		if ( $vradargs =~ /-ldr|-both/ )
		{
			$ldrResult = system( "$rootdir\\$maindir\\game\\hl2.exe -allowdebug -game $mod -window -w 1152 -h 864 +mat_picmip 0 -dev +mat_hdr_level 0 +sv_cheats 1 +map $mapname -buildcubemaps" );
		}
	}

	if ( $buildreslists == 1 )
	{	
		system( "echo. >> $outfile" );
		system( "time /t >> $outfile" );
		system( "echo Building reslists and nodegraph for $mapname. >> $outfile" );

		system( "del /s $rootdir\\$maindir\\game\\$mod\\reslists_temp\\*.lst" );

		system( "p4 edit $rootdir\\$maindir\\game\\$mod\\reslists_xbox\\$mapname.lst" );
		system( "p4 add $rootdir\\$maindir\\game\\$mod\\reslists_xbox\\$mapname.lst" );
		system( "del $rootdir\\$maindir\\game\\$mod\\reslists_xbox\\$mapname.lst" );
		my $extraflags = "";
#		if ( $mod =~ /portal/ )
#		{
#			$extraflags = "-tempcontent";
#		}
		system( "del  \/s $rootdir\\$maindir\\game\\modelsounds.cache" );
		system( "$rootdir\\$maindir\\game\\hl2.exe -allowdebug -game $mod -window -dev -makereslists makereslists_xbox.txt $extraflags +map $mapname" );
	}
	elsif ( $buildnodegraphs == 1 )
	{
		# system( "p4 edit $rootdir\\$maindir\\game\\$mod\\maps\\graphs\\$mapname.ain >> log.txt" );
		
		# TODO: Build nodegraphs method. Currently we get this for free when building reslists.
	}

	# Generate the checkin file

	system( "echo. >> $outfile" );
	system( "time /t >> $outfile" );
	system( "echo Submitting to Perforce. >> $outfile" );

	open( OUTFILE, ">checkin.txt" );
	print( OUTFILE "Change: new\n\n" );

	print( OUTFILE "Client: $g_client\n\n" );
	print( OUTFILE "User:	$g_owner\n\n" );

	print( OUTFILE "Status: new\n\n" );
	print( OUTFILE "Description: Autobuild for map $mapname.bsp\n" );
	print( OUTFILE "\n" );
	if ( $hdrResult == 1 )
	{
		print( OUTFILE "\tWARNING! There was an error while building HDR cubemaps.\n" );
		print( OUTFILE "\tHDR Cubemaps may need to be rebuilt before using this map.\n\n" );
	}
	if ( $ldrResult == 1 )
	{
		print( OUTFILE "\tWARNING! There was an error while building LDR cubemaps.\n" );
		print( OUTFILE "\tLDR Cubemaps may need to be rebuilt before using this map.\n\n" );
	}
	
	if ( $buildbsp == 1 )
	{
		print( OUTFILE "\tCOMPILE ARGS\n" );
		print( OUTFILE "\tvbsp:\t",$vbspargs,"\n" );
		print( OUTFILE "\tvvis:\t",$vvisargs,"\n" );
		print( OUTFILE "\tvrad:\t",$vradargs,"\n" );
		print( OUTFILE "\n" );
		print( OUTFILE "\tBuild time: $strTime\n" );
		print( OUTFILE "\n" );
	}

	# Add the comments from the last checkin of this map

	system( "p4 changes -m 1 -s submitted -l $rootdir\\$maindir\\content\\$mod\\maps\\$mapname.vmf > comments.txt" );

	open(INFILE, "comments.txt");
	my @comments = <INFILE>;
	close(INFILE);

	if ( $buildbsp == 1 )
	{
		print( OUTFILE "\tCheckin Comments: " );
		print( OUTFILE @comments,"\n\n" );
		print( OUTFILE "\tRebuilt bsp\n" );
	}
	if ( $buildnodegraphs == 1 )
	{
		print( OUTFILE "\n\tRebuilt nodegraph\n" );
	}
	if ( $buildreslists == 1 )
	{
		print( OUTFILE "\n\tRebuilt reslist\n" );
	}

	print( OUTFILE "Files:\n" );
	if ( $buildbsp == 1 )
	{
		print( OUTFILE "\t//ValveGames/staging/game/$mod/maps/$mapname.bsp\n" );
	}
	if ( $buildnodegraphs == 1 )
	{
		# Copy up to fileserver
		system( "copy $rootdir\\$maindir\\game\\$mod\\maps\\graphs\\$mapname.ain \"\\\\fileserver\\user\\xbox\\xbox_orange\\graphs\\$mod\\$mapname.ain\"" );
	}
	if ( $buildreslists == 1 )
	{
		print( OUTFILE "\t//ValveGames/staging/game/$mod/reslists_xbox/$mapname.lst\n" );
	}
	close( OUTFILE );

	# Check in the map
	
	system( "p4 submit -i < checkin.txt >> log.txt" );	

	# Send the build output in an email to the submitter

	my $email = "kerry\@valvesoftware.com";
	for ( @comments )
	{
		if ( /Change \d* on \S* by (\w*)@/ )
		{
			$email = join( '@', $1, "valvesoftware.com" );
		}
	}

	open(INFILE, "$outfile");
	my @compileoutput = <INFILE>;
	close(INFILE);

	my$smtp;
	$smtp = Net::SMTP->new('exchange2.valvesoftware.com');

	$smtp->mail("Mapbuilder");
	$smtp->to("$email");

	$smtp->data();
	$smtp->datasend( "To: $email\n" );
	$smtp->datasend( "Subject: $mapname has finished compiling.\n" );
	$smtp->datasend( @comments );
	$smtp->datasend( @compileoutput );
	$smtp->dataend();

	$smtp->quit;

	delete( $g_RunningCompiles{$outfile} );
	splice( @g_Finalizing );

	chdir( "$rootdir\\main1\\src\\devtools\\mapbuild" );
}


} # end while(1)


#----------------------------------------
# Start a compile if there is an open slot
#----------------------------------------
sub StartCompiles
{
	my $idx = 0;

	print( "Current maplist:\n" );
	for ( @g_CompileList )
	{
		print( "$_\n" );
	}

	my $skipCt = 0;
	while ( @g_CompileList > $skipCt && $running < $maxCompiles )
	{
		my $nextMap = splice( @g_CompileList, 0, 1 );

			# Make sure this map isn't already compiling in another slot

			my $duplicate = 0;
			for ( values %g_RunningCompiles )
			{
				$nextMap =~ /(\w*).vmf/;

				if ( /^$1$/ )
				{
					system( "echo $nextMap already compiling - skipping for now. >> log.txt" );

					push @g_CompileList, $nextMap;
					++$skipCt;
					$duplicate = 1;
					last;
				}
			}
		
			if ( $duplicate == 0 )
			{
				Compile( $nextMap );

				++$running;
				++$idx;

				my $mapct = @g_CompileList;
				system( "echo Maps waiting: $mapct >> log.txt" );
			}
	}

	UpdateStats();
}

#-----------------------------------------------------------
# Compile a single map - this subroutine forks the process
#-----------------------------------------------------------
sub Compile
{
	# sync the rest of the content and bins.

	my $maindir = $g_MainDirs[$g_MainDirIdx];
	$g_MainDirIdx = ( $g_MainDirIdx + 1 ) % $maxCompiles;

	chdir( "$rootdir\\$maindir\\src\\devtools\\mapbuild" );

	system( "p4 sync $rootdir\\$maindir\\game\\..." );
	system( "p4 sync $rootdir\\$maindir\\src\\..." );


	# parse the map and mod name from the full path

	my $fullpath = shift;
	my $mod;
	my $mapname;
	if ( $fullpath =~ /(\w*)\\maps\\(\w*).vmf/ )
	{
		$mod = $1;
		$mapname = $2;
	}
	else
	{
		print( "Error getting map name and mod\n" );
		exit();
	}

	# Get the next output file. All output from the build tools
	# (vbsp,vvis,vrad) will be redirected to this file.

	my $outfile = undef;
	for ( @g_OutputFiles )
	{
		unless ( defined $g_RunningCompiles{$_} )
		{
			$outfile = $_;
			last;
		}
	}
	unless ( defined $outfile )
	{
		my $filename = join( '', "buildqueue", $g_FilenameIdx++, ".txt" );
		push( @g_OutputFiles, $filename );
		$outfile = $filename;
	}

	$g_RunningCompiles{$outfile} = $mapname;
	
	# Force-sync the vmf and bsp

	system( "echo Compiling $mod\\$mapname.bsp > $outfile" );
	system( "echo. >> log.txt" );
	system( "date /t >> log.txt" );
	system( "time /t >> log.txt" );
	system( "echo Compling $mod\\$mapname.bsp >> log.txt" );

	system( "p4 sync -f ..\\..\\..\\content\\$mod\\maps\\$mapname.vmf >> log.txt" );
	system( "p4 sync -f $rootdir\\$maindir\\content\\$mod\\maps\\$mapname.vmf >> log.txt" );
	system( "p4 sync -f $rootdir\\$maindir\\game\\$mod\\maps\\$mapname.bsp >> log.txt" );

	if ( $buildbsp == 1 )
	{
		print "\nCompiling $mod\\$mapname.bsp\n";

		# load the map compile args for this mod

		if( !open( INFILE, "$rootdir\\$maindir\\game\\$mod\\scripts\\mapautocompile.txt") )
		{
			print( "Error opening autocompile.txt\n" );
		}
		my @lines = <INFILE>;
		close (INFILE);
	
		my $tool;
		my $rest;
		my $toolmap;
		for ( @lines )
		{
			if ( /map: (.*)/ )
			{
				$toolmap = $1;
			}
			elsif ( /\s+(.+)/ )
			{
				($tool, $rest) = split ( /:\s*/, $1, 2 );
				$g_ToolArgs{$toolmap}{$tool} = $rest;
			}
		}

		# Open the bsp for edit

		system( "p4 edit $rootdir\\$maindir\\game\\$mod\\maps\\$mapname.bsp" );
		system( "p4 add $rootdir\\$maindir\\game\\$mod\\maps\\$mapname.bsp" );
		system( "del $rootdir\\$maindir\\game\\$mod\\maps\\$mapname.bsp" );
	}

	chdir( "$rootdir\\main1\\src\\devtools\\mapbuild" );

	#--------------------------------------
	# fork this compile to a new process
	#--------------------------------------

	my $pid = fork();
	print "fork failed!" unless defined $pid;
	if ( $pid != 0 )
	{
		# I am the parent
		return;
	}

	my $strTime = "";
	my $vbspargs = "";
	my $vvisargs = "";
	my $vradargs = "";

	chdir( "$rootdir\\$maindir\\src\\devtools\\mapbuild" );

	if ( $buildbsp == 1 )
	{
		# Compile

		# delete the bsp to ensure that vbsp succeeded
		system( "del $rootdir\\$maindir\\content\\$mod\\maps\\$mapname.bsp" );
		
		my $startTime = time;
	
		$vbspargs = compileTool( $maindir, $mod, $mapname, "vbsp", $outfile );
		$vvisargs = compileTool( $maindir, $mod, $mapname, "vvis", $outfile );
		$vradargs = compileTool( $maindir, $mod, $mapname, "vrad", $outfile );

		my $totalTime = time - $startTime;
		my $hours = int($totalTime / 3600);
		$totalTime -= ($hours * 3600);
		my $minutes = int( $totalTime / 60 );
		$totalTime -= ($minutes * 60);
		my $seconds = $totalTime;
		$strTime = sprintf( "%02d:%02d:%02d", $hours, $minutes, $seconds );
	}

	chdir( "$rootdir\\main1\\src\\devtools\\mapbuild" );

	# Add the map's name to the finalize list

	while( !open( OUTFILE, ">>finalize.txt" ) ) {}
	print OUTFILE "Finalize: $mod $mapname\n";
	print OUTFILE "maindir: $maindir\n";

	if ( $buildbsp == 1 )
	{
		# Output the compile args and time

		print OUTFILE "vbspargs: $vbspargs\n";
		print OUTFILE "vvisargs: $vvisargs\n";
		print OUTFILE "vradargs: $vradargs\n";
		print OUTFILE "time: $strTime\n";
	} 

	print OUTFILE "outfile: $outfile\n";

	close( OUTFILE );
	
	# This child is finished

	exit();
}

#----------------------------------------
# Run a map build tool (vbsp, vvis, vrad)
#----------------------------------------
sub compileTool
{
	my $maindir = shift;
	my $mod = shift;
	my $map = shift;
	my $tool = shift;
	my $outfile = shift;

	# Load the compile arguments that were parsed from mapautocompile.txt.
	# If the map has its own custom args, then use those - otherwise,
	# just use the default set.

	my $toolArgs = $g_ToolArgs{"default"}{$tool};
	if ( exists $g_ToolArgs{$map}{$tool} )
	{
		$toolArgs = $g_ToolArgs{$map}{$tool};
	}

	system( "echo. >> $outfile" );
	system( "time /t >> $outfile" );
	system( "echo Starting $tool for $map >> $outfile" );
	system( "echo. >> $outfile" );

	# HACK: There is a crash bug when using -both in vrad. If the -both
	# flags is present, remove it and do two vrad compiles with -ldr and -hdr.

	if ( $tool =~ /vrad/ && $toolArgs =~ /-both/ )
	{
		system( "echo Found -both argument. Splitting into -ldr and -hdr >> $outfile" );
		my $newArgs = $toolArgs;
		$newArgs =~ s/-both //;

		system( "$rootdir\\$maindir\\game\\bin\\$tool -vproject $rootdir\\$maindir\\game\\$mod $newArgs -ldr $rootdir\\$maindir\\content\\$mod\\maps\\$map >> $outfile" );
		system( "$rootdir\\$maindir\\game\\bin\\$tool -vproject $rootdir\\$maindir\\game\\$mod $newArgs -hdr $rootdir\\$maindir\\content\\$mod\\maps\\$map >> $outfile" );
	}
	else
	{
		system( "$rootdir\\$maindir\\game\\bin\\$tool -vproject $rootdir\\$maindir\\game\\$mod $toolArgs $rootdir\\$maindir\\content\\$mod\\maps\\$map >> $outfile" );
	}

	# copy the new bsp into the maps directory

	system( "copy /Y $rootdir\\$maindir\\content\\$mod\\maps\\$map.bsp $rootdir\\$maindir\\game\\$mod\\maps" );

	system( "echo. >> $outfile" );
	system( "echo Finished >> $outfile" );

	return $toolArgs;
}


#-----------------------------------
# Check if this map should build now
#-----------------------------------
sub syncChangedMaps
{
	# TODO: Get these mod names from a file?
	my @mods = ( "ep2", "portal", "episodic", "hl2", "tf" );

	# Query perforce for changed maps by doing a speculative sync. This generates a list
	# of files that it WOULD sync, without actually doing the sync.
	# BUG: This doesn't return new files that have been added - don't know why yet.

	my @maps;
	for my $mod ( @mods )
	{
		push ( @maps, readpipe "p4 sync -n ..\\..\\..\\content\\$mod\\maps\\*.vmf" );
	}

	# forcelist.txt allows maps to be manually added to the build. 

	open(INFILE, "forcelist.txt");
	my @forcemaps = <INFILE>;
	close( INFILE );

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

			system( "p4 sync -f $1" );
			push( @g_MapsToBuild, $1 );
			system( "echo Adding to the build list: $1 >> log.txt" );
		}
	}

	for( @forcemaps )
	{
		if ( /\.vmf/ )
		{
			# Add this map name to the build list

			$_ =~ /(.*)/;
			system( "p4 sync -f $1" );
			push( @g_MapsToBuild, $1 );
			system( "echo Forcing add to the build list: $1 >> log.txt" );
		}
	}
}


#-----------------------------------
# Check if this map should build now
#-----------------------------------
sub shouldBuild
{
	my $map = shift;

	# if the map line contains the force flag, build the map

	if ( $map =~ /-forcebuild/ )
	{
		return 1;
	}

	# Get the comments from the last checkin of this map
	# and look for the autocompile keyword

	my @comments = readpipe "p4 changes -m 1 -s submitted -l $map";

	for( @comments )
	{
		if ( /autocompile/i )
		{
			return 1;
		}
	}

	return 0;
}


#----------------------------------------------------------------------------------
# Hacky little routine to build an html file that contains the current state
# of the autocompiler (running, maps building, maps waiting, etc.) and a frameset
# that shows the compile output for each map that's currently building. This way
# users of the autocompiler can keep an eye on their map's progress.
#----------------------------------------------------------------------------------
sub UpdateStats
{
	my $noCompiles = 0;
	my $noWaiting = 0;
	if ( keys( %g_RunningCompiles ) == 0 && @g_Finalizing == 0 )
	{
		$noCompiles = 1;
	}
	if ( @g_CompileList == 0 )
	{
		$noWaiting = 1;
	}

	# Start building the main html page

	open( OUTFILE, ">buildlist.htm" );
	print( OUTFILE "<html>\n" );
	print( OUTFILE "<META HTTP-EQUIV=\"Refresh\" CONTENT=\"10; URL=buildlist.htm\">\n" );
	print( OUTFILE "Mapbuilder state: <font color=#009900><b>" );
	if ( $noCompiles == 1 && $noWaiting == 1 )
	{
		print( OUTFILE "Ready</b></font><br>\n" );
	}
	else
	{
		print( OUTFILE "RUNNING</b></font><br>\n" );
	}
	print( OUTFILE "Max simultaneous compiles: $maxCompiles\n" );
	print( OUTFILE "<p>\n" );
	print( OUTFILE "<b>Active:</b>\n<p>\n" );
		
	# Show a list of maps that are currently compiling

	my $filect = 0;
	if ( $noCompiles == 1 )
	{
		print( OUTFILE "None" );
	}
	else
	{
		for ( @g_OutputFiles )
		{
			if ( defined( $g_RunningCompiles{$_} ) )
			{
				my $testname = $g_RunningCompiles{$_};
				my $filename = $_;
				if ( $filename =~ /(\w*\.txt)/ )
				{
					$filename = $1;
				}
				my $finishing = 0;

				for ( @g_Finalizing )
				{
					if ( /$testname/ )
					{
						$finishing = 1;
					}
				}

				print( OUTFILE "<a href=\"$filename\" TARGET=\"_parent\">$testname.bsp</a>" );
				++$filect;

				if ( $finishing == 1 )
				{
					print( OUTFILE " - Finishing (cubemaps, reslist, nodegraph.)\n<br>\n" );
				}
				else
				{
					print( OUTFILE " - Compiling\n<br>\n" );
				}
			}
		}
	}

	# Show a list of maps that are waiting to compile

	print( OUTFILE "<p>\n<b>Waiting:</b>\n<p>\n" );
	if ( $noWaiting == 1 )
	{
		print( OUTFILE "None" );
	}
	else
	{
		for ( @g_CompileList )
		{
			/(\w*).vmf/;
			print( OUTFILE "$1.bsp\n<br>\n" );
		}
	}
	print( OUTFILE "</html>\n" );	
	close( OUTFILE );

	# create the main page

	open( OUTFILE, ">buildqueue.htm" );
	print( OUTFILE "<html>\n" );
	print( OUTFILE "<META HTTP-EQUIV=\"Refresh\" CONTENT=\"30; URL=buildqueue.htm\">\n" );
	my $size = 0;	
	if ( $filect != 0 )
	{
		print( OUTFILE "<frameset rows=\"30%" );
		$size = 70 / $filect;
	}
	else
	{
		print( OUTFILE "<frameset rows=\"100%" );
	}
	for ( my $ct = 0; $ct < $filect; ++$ct )
	{
		print( OUTFILE ",$size%" );
	}
	print( OUTFILE "\">\n" );
	print( OUTFILE "<frame src=\"buildlist.htm\">\n" );
	for ( @g_OutputFiles )
	{
		if ( defined( $g_RunningCompiles{$_} ) )
		{
			my $filename = $_;
			if ( $filename =~ /(\w*\.txt)/ )
			{
				$filename = $1;
			}
			print( OUTFILE "<frame src=\"$filename\">\n" );
		}
	}
	print( OUTFILE "</frameset>\n" );
	print( OUTFILE "</html>\n" );

	close( OUTFILE );
}