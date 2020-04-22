use strict;

###################################################
# CONFIG VARS
###################################################

# FIXME: load these from a separate file so that each user can have their own config without editingthis file.
my $g_P4UserName = "gary";

my $g_LocalBaseDir = "u:\\hl2";
my $g_LocalBranchSubdir = "src5";
my $g_LocalBranchName = "gary_src5";
my $g_LocalBranchClient = "gary_work_src5";

my $g_MainCopyBaseDir = "u:\\hl2";
my $g_LocalBranchMainCopySubdir = "src5_main";
my $g_LocalBranchMainCopyName = "gary_src5_main";
#my $g_LocalBranchMainCopyClient = "gary_work_src5_main";

my $g_MainBaseDir = "u:\\hl2_main";
my $g_MainBranchSubdir = "src_main";
#my $g_MainBranchName = "gary_src_main";
#my $g_MainBranchClient = "gary_work_src_main";

my $g_UseIncredibuildForMain = 1;

# FIXME: need to make this work for those that don't work on HL2.
my $g_LocalBranchHasHL1Port = 0;
my $g_LocalBranchHasCSPort = 0;
my $g_LocalBranchHasTF2 = 0;

my $g_CheckinFileStampTimes = "c:\\checkin_filetimes.txt";

# either use VSS directly via the command-line, or use Tom's tool.
my $g_UseVSS = 0;

###################################################
# Helper vars made up of config vars
###################################################

my $g_LocalBranchDir = "$g_LocalBaseDir\\$g_LocalBranchSubdir";
my $g_LocalBranchMainCopyDir = "$g_MainCopyBaseDir\\$g_LocalBranchMainCopySubdir";
my $g_MainBranchDir = "$g_MainBaseDir\\$g_MainBranchSubdir";

###################################################

my $g_DebugStub = 0;

my $stage = shift;
if( $stage != 1 && 
    $stage != 2 && 
    $stage != 3 && 
    $stage ne "syncmain" && 
    $stage ne "syncmainsrc" && 
    $stage ne "synclocal" && 
    $stage ne "synclocalrelease" && 
    $stage ne "synclocaldebug" && 
    $stage ne "synclocalsrc" && 
    $stage ne "synclocalsrcrelease" && 
    $stage ne "sync" && 
    $stage ne "syncmaincontent" )
{
	print "checkin.pl 1                   : to get started with a checkin\n";
	print "checkin.pl 2                   : second stage of checkin\n";
	print "checkin.pl 3                   : third stage of checkin\n";
	print "checkin.pl syncmain            : sync main source and content, then build\n";
	print "checkin.pl syncmainsrc         : sync main source then build\n";
	print "checkin.pl syncmaincontent     : sync main content only\n";
	print "checkin.pl synclocal           : merge personal branch, sync content,\n"; 
	print "                                 then build.\n";
	print "checkin.pl synclocalrelease    : merge personal branch, sync content,\n";
	print "                                 then build (release only).\n";
	print "checkin.pl synclocaldebug      : merge personal branch, sync content,\n";
	print "                                 then build (debug only).\n";
	print "checkin.pl synclocalsrc        : merge personal branch, then build.\n";
	print "checkin.pl synclocalsrcrelease : merge personal branch, then build\n";
	print "                                 (release only).\n";
	print "checkin.pl sync                : merge personal branch, sync main src,\n";
        print "                                 sync content for both, and then build\n";
	print "                                 the whole thing.\n";
	die;
}

sub RunCmd
{
	my $cmd = shift;
	print $cmd . "\n";
	if( !$g_DebugStub )
	{
		return system $cmd;
	}
}

sub CD
{
	my $dir = shift;
	print "cd $dir\n";
	if( !$g_DebugStub )
	{
		chdir $dir;
	}
}

sub SSGet
{
	my $vssdir = shift;
	my $localdir = shift;

	&CD( $localdir );
	&RunCmd( "ss WorkFold $vssdir $localdir" );
	print "\n";
	my $cmd = "ss get $vssdir -R";
	local( *SS );
	open SS, "$cmd|";
	my $workingdir;
	while( <SS> )
	{
		# FIXME: clean up this output.
		$_ =~ s/\n//;
		if( m/^\$/ )
		{
			$workingdir = $_;
#			print "WORKING DIR: $_\n";
		}
		elsif( m/^getting/i )
		{
			print "GETTING: $workingdir $_\n";
		}
		elsif( m/^replacing local file/i )
		{
			print "REPLACING: $workingdir $_\n";
		}
		elsif( m/^File/ )
		{
			print "ALREADY EXISTS: $workingdir $_\n";
		}
		else
		{
#			print "WTF: $_\n";
		}
	}
	close SS;
}

sub FastSSGet
{
	my $localdir = shift;
	my $option = shift;

	&RunCmd( "\\\\hl2vss\\hl2vss\\win32\\syncfrommirror.bat $option $localdir" );
}

sub FileIsWritable
{
	my $file = shift;

	my @statresult = stat $file;
	die if !@statresult;
	my $perms = oct( $statresult[2] );
	if( $perms & 2 )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

sub CompareDirs
{
	my $filesThatHaveChanged = shift;
	my $filesThatHaveNotChanged = shift;
	my @out = `robocopy $g_MainBaseDir\\checkinbins\\. $g_MainBaseDir\\. /S /L /V`;
	my $line;
	my $cwd;
	foreach $line ( @out )
	{
		next if( $line =~ /\*EXTRA Dir/ );
		next if( $line =~ /\*EXTRA Dir/ );
		next if( $line =~ /\*EXTRA File/ );
		if( $line =~ m/\s*\d+\s+(\S+\\)/ )
		{
			$cwd = $1;
			next;
		}
		if( $line =~ m/\s+Older\s+\d+\s+(\S+)/ )
		{
			my $testfilename = $cwd . $1;
			my $filename = $testfilename;
			$filename =~ s/\\checkinbins//i;
			my $diffresult = system "diff $testfilename $filename > nil";
			if( $diffresult != 0 )
			{
				push @{$filesThatHaveChanged}, $filename;
			}
			else
			{
				if( &FileIsWritable( $filename ) )
				{
					push @{$filesThatHaveNotChanged}, $filename;
				}
			}
			next;
		}
		elsif( $line =~ m/\s+same\s+\d+\s+(\S+)/ )
		{
			my $filename = $cwd . $1;
			$filename =~ s/\\checkinbins//i;
			if( &FileIsWritable( $filename ) )
			{
				push @{$filesThatHaveNotChanged}, $filename;
			}
			next;
		}
		if( $line =~ m/\s+New File\s+\d+\s+(\S+)/ )
		{
			die "$cwd $1 didn't build!\n";
		}
		print "DEBUG: unhandled line: $line\n";
	}
}

sub CheckoutFile
{
	my $file = shift;
	print "-----------------\nchecking out $file\n";
	if( $file =~ /src_main/i )
	{
		# need to use p4 to check this one out.
		my $dir = $file;
		$dir =~ s/\\([^\\]*)$//;
		&CD( $dir );
		&RunCmd( "p4 edit $1" );
	}
	else
	{
		my $dir = $file;
		$dir =~ s/\\([^\\]*)$//;
		&CD( $dir );
		$file =~ s,\\,/,g;
		if( $file =~ m/cstrike/i || $file =~ m/hl1/i )
		{
			$file =~ s,u:/hl2_main/,\$/hl1ports/release/dev/,gi;
			&RunCmd( "ss WorkFold \$/hl1ports/release/dev $g_MainBaseDir" );
		}
		elsif( $file =~ m/\/tf2/i  )
		{
			$file =~ s,u:/hl2_main/,\$/tf2/release/dev/,gi;
			&RunCmd( "ss WorkFold \$/tf2/release/dev $g_MainBaseDir" );
		}
		else
		{
			$file =~ s,u:/hl2_main/,\$/hl2/release/dev/,gi;
			&RunCmd( "ss WorkFold \$/hl2/release/dev $g_MainBaseDir" );
		}
		print "\n";
		&RunCmd( "ss Checkout -G- $file" );
	}
}

sub RevertFile
{
	my $file = shift;
	print "-----------------\nreverting $file\n";
	if( $file =~ /src_main/i )
	{
		# need to use p4 to revert this one
		my $dir = $file;
		$dir =~ s/\\([^\\]*)$//;
		&CD( $dir );
		&RunCmd( "p4 sync -f $1" );
	}
	else
	{
		my $dir = $file;
		$dir =~ s/\\([^\\]*)$//;
		&CD( $dir );
		$file =~ s,\\,/,g;
		my $vssfile = $file;
		if( $file =~ m/cstrike/i || $file =~ m/hl1/i )
		{
			$vssfile =~ s,u:/hl2_main/,\$/hl1ports/release/dev/,gi;
			&RunCmd( "ss WorkFold \$/hl1ports/release/dev $g_MainBaseDir" );
		}
		elsif( $file =~ m/\/tf2/i  )
		{
			$vssfile =~ s,u:/hl2_main/,\$/tf2/release/dev/,gi;
			&RunCmd( "ss WorkFold \$/tf2/release/dev $g_MainBaseDir" );
		}
		else
		{
			$vssfile =~ s,u:/hl2_main/,\$/hl2/release/dev/,gi;
			&RunCmd( "ss WorkFold \$/hl2/release/dev $g_MainBaseDir" );
		}
		print "\n";
		unlink $file;
		&RunCmd( "ss Get -I- $vssfile" );
	}
}

sub SyncMainSource
{
	# SYNC MAIN
	&CD( $g_MainBranchDir );
	&RunCmd( "p4 sync" );
}

sub SyncMainContent
{
	# SYNC VSS
	&CD( $g_MainBranchDir );
	&RunCmd( "clean.bat" );
	if( $g_UseVSS )
	{
		&SSGet( "\$/hl2/release/dev", $g_MainBaseDir );
		&SSGet( "\$/hl1ports/release/dev", $g_MainBaseDir );
		# NOTE: only get tf2 bin since we aren't testing tf2 right now
		&SSGet( "\$/tf2/release/dev/tf2/bin", "$g_MainBaseDir/tf2/bin" );
	}
	else
	{
		&FastSSGet( $g_MainBaseDir, "all" );
	}
}

sub BuildMain
{
	if( $g_UseIncredibuildForMain )
	{
		$ENV{"USE_INCREDIBUILD"} = "1";
	}
	&CD( $g_MainBranchDir );
	&RunCmd( "clean.bat" );
	&RunCmd( "build_hl2.bat" );
	&RunCmd( "build_hl1_game.bat" );
	&RunCmd( "build_cs_game.bat" );
	&RunCmd( "build_tf2_game.bat" );
	if( $g_UseIncredibuildForMain )
	{
		undef $ENV{"USE_INCREDIBUILD"};
	}
}

sub SyncLocalBranchSource
{
	&CD( $g_LocalBranchDir );
	&RunCmd( "p4mf.bat $g_LocalBranchName $g_LocalBranchClient pause" );
	# FIXME: This needs to specify the changelist since p4mf makes a new changelist.
#	&RunCmd( "p4 submit" );

}

sub SyncMainCopySource
{
	&CD( $g_LocalBranchMainCopyDir );
	&RunCmd( "p4 integrate -d -i -b $g_LocalBranchMainCopyName" );
	&RunCmd( "p4 resolve -at ..." );
	# Update the changelist and submit
	&RunCmd( "p4 change -o | sed -e \"s/<enter description here>/Merge from main/g\" | p4 submit -i" );
}

sub SyncLocalBranchContent
{
	&CD( $g_LocalBranchDir );

	# CLEAN LOCAL BRANCH
	&RunCmd( "clean.bat" );

	# SYNC VSS
	if( $g_UseVSS )
	{
		&SSGet( "\$/hl2/release/dev", $g_LocalBaseDir );
		if( $g_LocalBranchHasHL1Port || $g_LocalBranchHasCSPort )
		{
			&SSGet( "\$/hl1ports/release/dev", $g_LocalBaseDir );
		}
		if( $g_LocalBranchHasTF2 )
		{
			&SSGet( "\$/tf2/release/dev", $g_LocalBaseDir );
		}
	}
	else
	{
		if( $g_LocalBranchHasHL1Port || $g_LocalBranchHasCSPort )
		{
			&FastSSGet( $g_LocalBaseDir, "all" );
		}
		else
		{
			&FastSSGet( $g_LocalBaseDir, "hl2" );
		}
		# FIXME: screwed on tf2 here.
	}
}

sub BuildLocalBranch
{
	&CD( $g_LocalBranchDir );

	# BUILD DEBUG if we don't want release only
	if( !( $stage =~ /release/i ) )
	{
		# CLEAN LOCAL BRANCH
		&RunCmd( "clean.bat" );

		&RunCmd( "build_hl2.bat debug" );
		if( $g_LocalBranchHasHL1Port )
		{
			&RunCmd( "build_hl1_game.bat debug" );
		}
		if( $g_LocalBranchHasCSPort )
		{
			&RunCmd( "build_cs_game.bat debug" );
		}
		if( $g_LocalBranchHasTF2 )
		{
			&RunCmd( "build_tf2_game.bat debug" );
		}
	}

	if( !( $stage =~ /debug/i ) )
	{
		# CLEAN LOCAL BRANCH
		&RunCmd( "clean.bat" );

		# BUILD RELEASE
		&RunCmd( "build_hl2.bat" );
		if( $g_LocalBranchHasHL1Port )
		{
			&RunCmd( "build_hl1_game.bat" );
		}
		if( $g_LocalBranchHasCSPort )
		{
			&RunCmd( "build_cs_game.bat" );
		}
		if( $g_LocalBranchHasTF2 )
		{
			&RunCmd( "build_tf2_game.bat" );
		}
	}
}

sub GetMainUpToDate
{
	&SyncMainSource();
	&SyncMainContent();
	&BuildMain();
}

sub LockPerforce
{
	while( 1 )
	{
		my $thing = `p4mutex lock main_src 0 $g_P4UserName 207.173.178.12:1666`;
		print $thing;
		last if $thing =~ /Success/;
		sleep 30;
	}
}

sub SaveMainTimeStampsBeforeIntegrate
{
	&CD( $g_MainBranchDir );
	# Get a list of files that are going to be integrated into main so that we can save off their time stamp info.
	my @filestointegrate = `p4 integrate -n -r -b $g_LocalBranchName`;
	my $file;
	local( * TIMESTAMPS );
	open TIMESTAMPS, ">$g_CheckinFileStampTimes";
	foreach $file ( @filestointegrate )
	{
		$file =~ s,//ValveGames/main/src/([^#]*)\#.*,$1,gi;
		$file =~ s/\n//;
		$file =~ s,\\,/,g;
		my $localfilename = "$g_MainBranchDir/$file";
		my @statinfo = stat $localfilename;
		next if !@statinfo;
		my $mtime = $statinfo[9];
		print TIMESTAMPS $file . "|" . $mtime . "\n";
	}
	close TIMESTAMPS;
}

sub SetMainTimeStampsOnRevertedFiles
{
	&CD( $g_MainBranchDir );
	# Get a list of files that we might have to revert times on if they aren't in the changelist.
	local( *TIMESTAMPS );
	open TIMESTAMPS, "<$g_CheckinFileStampTimes";
	my @timestamps = <TIMESTAMPS>;
	my %filetotimestamp;
	my $i;
	for( $i = 0; $i < scalar( @timestamps ); $i++ )
	{
		$timestamps[$i] =~ s/\n//;
		$timestamps[$i] =~ m/^(.*)\|(.*)$/;
		$filetotimestamp{$1} = $2;
	}
	close TIMESTAMPS;
	
	my $key;
	foreach $key( keys( %filetotimestamp ) )
	{
		print "before: \'$key\" \"$filetotimestamp{$key}\"\n";
	}

	local( *CHANGELIST );
	open CHANGELIST, "p4 change -o|";
	while( <CHANGELIST> )
	{
		if( m,//ValveGames/main/src/(.*)\s+\#,i )
		{
			if( defined $filetotimestamp{$1} )
			{
				undef $filetotimestamp{$1};
			}
		}
	}
	close CHANGELIST;

	foreach $key( keys( %filetotimestamp ) )
	{
		if( defined $filetotimestamp{$key} )
		{
			my $filename = $g_MainBranchDir . "/" . $key; 
			$filename =~ s,/,\\,g;
			my @statresults;
			if( @statresults = stat $filename )
			{
				my $mode = $statresults[2];
				my $atime = $statresults[8];
				my $mtime = $statresults[9];
				
				print "reverting timestamp for $filename\n";

				chmod 0666, $filename || die $!;

				utime $atime, $filetotimestamp{$key}, $filename || die $!;
				
				chmod $mode, $filename || die $!;
			}
		}
	}
}


if( $stage eq "synclocal" || $stage eq "synclocalrelease" || $stage eq "synclocaldebug" || $stage eq "sync" )
{
	&SyncLocalBranchSource();
	&SyncMainCopySource();
	&SyncLocalBranchContent();
	&BuildLocalBranch();
}

if( $stage eq "synclocalsrc" || $stage eq "synclocalsrcrelease" )
{
	&SyncLocalBranchSource();
	&SyncMainCopySource();
	&BuildLocalBranch();
}

if( $stage eq "syncmainsrc" )
{
	&SyncMainSource();
	&BuildMain();
}

if( $stage eq "syncmain" || $stage eq "sync" )
{
	&GetMainUpToDate();
}

if( $stage eq "syncmaincontent" )
{
	&SyncMainContent();
}

if( $stage == 1 )
{
	# lock p4
#	&LockPerforce();

	&GetMainUpToDate();

	# merge main into local branch
	&SyncLocalBranchSource();

	# TODO: need a way to detect if there are conflicts or not.  If there are, pause here.

	# Make a copy of targets so that we can tell which ones changed.
	&RunCmd( "robocopy $g_MainBaseDir\\ $g_MainBaseDir\\checkinbins\\ /purge" );
	&RunCmd( "robocopy $g_MainBaseDir\\bin\\. $g_MainBaseDir\\checkinbins\\bin\\. /mir" );
	&RunCmd( "robocopy $g_MainBaseDir\\hl2\\bin\\. $g_MainBaseDir\\checkinbins\\hl2\\bin\\. /mir" );
	&RunCmd( "robocopy $g_MainBaseDir\\hl1\\bin\\. $g_MainBaseDir\\checkinbins\\hl1\\bin\\. /mir" );
	&RunCmd( "robocopy $g_MainBaseDir\\cstrike\\bin\\. $g_MainBaseDir\\checkinbins\\cstrike\\bin\\. /mir" );
	&RunCmd( "robocopy $g_MainBaseDir\\tf2\\bin\\. $g_MainBaseDir\\checkinbins\\tf2\\bin\\. /mir" );
	&RunCmd( "robocopy $g_MainBaseDir\\platform\\servers\\. $g_MainBaseDir\\checkinbins\\platform\\servers\\. /mir" );
	&RunCmd( "robocopy $g_MainBranchDir\\lib\\. $g_MainBaseDir\\checkinbins\\$g_MainBranchSubdir\\lib\\. /mir" );

	# integrate from personal branch into main. . accept theirs.
	# TODO: need to check if main has a changelist or not and warn!
	&CD( $g_MainBranchDir );

	&SaveMainTimeStampsBeforeIntegrate();

	&RunCmd( "p4 integrate -r -b $g_LocalBranchName" );

	&RunCmd( "p4 resolve -at ..." );

	# revert unchanged files.
	my @unchanged = `p4 diff -sr`;
	my $file;
	foreach $file ( @unchanged )
	{
		&RunCmd( "p4 revert $file" );
	}

	print "Do \"checkin.pl 2\" when you are done reverting unchanging files and fixing up any other diffs in your main client.\n";
}
elsif( $stage == 2 )
{
	&SetMainTimeStampsOnRevertedFiles();

	# build main with the new changes
	&BuildMain();

	# compare what we just built to what we saved off earlier
	my @filesToCheckOut;
	my @filesThatHaveNotChanged;
	&CompareDirs( \@filesToCheckOut, \@filesThatHaveNotChanged );

	my $file;
#	$g_DebugStub = 1;
	foreach $file ( @filesThatHaveNotChanged )
	{
		&RevertFile( $file );
	}

	foreach $file ( @filesToCheckOut )
	{
		&CheckoutFile( $file );
	}

	print "-----------------\n";
	print "Do \"checkin.pl 3\" when you are finished testing to checkin files and release the mutex.\n";
}
elsif( $stage == 3 )
{
	my @filesToCheckOut;
	my @filesThatHaveNotChanged;
	&CompareDirs( \@filesToCheckOut, \@filesThatHaveNotChanged );

	# TODO: check stuff in here and unlock p4
	# TODO: go ahead and sync src_main to main so that they match
	# TODO: merge main into src again so any changes that you made while checking in are propogated

	&SyncMainCopySource();
}


