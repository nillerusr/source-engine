#!perl
use strict;

BEGIN {use File::Basename; push @INC, dirname($0); }
require "valve_perl_helpers.pl";

sub PrintCleanPerforceOutput
{
	my $line;
	while( $line = shift )
	{
		if( $line =~ m/currently opened/i )
		{
			next;
		}
		if( $line =~ m/already opened for edit/i )
		{
			next;
		}
		if( $line =~ m/also opened/i )
		{
			next;
		}
		if( $line =~ m/add of existing file/i )
		{
			next;
		}
		print $line;
	}
}

# HACK!!!!  Need to pass something in to do this rather than hard coding.
sub NormalizePerforceFilename
{
	my $line = shift;

	# remove newlines.
	$line =~ s/\n//;
	# downcase.
	$line =~ tr/[A-Z]/[a-z]/;
	# backslash to forwardslash
	$line =~ s,\\,/,g;

	# for inc files HACK!
	$line =~ s/^.*(fxctmp9.*)/$1/i;
	$line =~ s/^.*(vshtmp9.*)/$1/i;

	# for vcs files. HACK!
	$line =~ s,^.*game/platform/shaders/,,i;

	return $line;
}

# COMMAND-LINE ARGUMENTS
my $x360 = 0;
my $ps3 = 0;
my $filename = shift;
if( $filename =~ m/-x360/i )
{
	$x360 = 1;
	$filename = shift;
}
elsif( $filename =~ m/-ps3/i )
{
	$ps3 = 1;
	$filename = shift;
}
my $changelistname = shift;
my $perforcebasepath = shift;
my $diffpath = join " ", @ARGV;

#print STDERR "\$filename: $filename\n";
#print STDERR "\$changelistname: $changelistname\n";
#print STDERR "\$perforcebasepath: $perforcebasepath\n";
#print STDERR "\$diffpath: $diffpath\n";

# Read the input file list before changing to the perforce directory.
open FILELIST, "<$filename";
my @inclist = <FILELIST>;
close FILELIST;

# change from the perforce directory so that our client will be correct from here out.
#print STDERR "chdir $perforcebasepath\n";
chdir $perforcebasepath || die "can't cd to $perforcebasepath";

#print "inclist before @inclist\n";
# get rid of newlines and fix slashes
@inclist = 
map 
{ 
	$_ =~ s,_tmp,,g;	# remove _tmp so that we check out in the proper directory
	$_ =~ s,\\,/,g;		# backslash to forwardslash
    $_ =~ s/\n//g;		# remove newlines
	$_ =~ tr/[A-Z]/[a-z]/;	# downcase
#	$_ =~ s,.*platform/shaders/,,i;
#	$_ =~ s,$perforcebasepath/,,i;
	$_ =~ s,../../../game/platform/shaders/,,i; # hack. . .really want something here that works generically.
  	$_
} @inclist;
#print "inclist after @inclist\n";

my $prevline;
my @outlist;
foreach $_ ( sort( @inclist ) )
{
	next if( defined( $prevline ) && $_ eq $prevline );
	$prevline = $_;
	push @outlist, $_;
}
@inclist = @outlist;

#print "\@inclist: @inclist\n";

# Get list of files on the client
# -sl     Every unopened file, along with the status of
#         'same, 'diff', or 'missing' as compared to its
#         revision in the depot.
my @unopenedlist = &RunCommand( "p4 diff -sl $diffpath" );

#print "\@unopenedlist: @unopenedlist\n";

my %sameunopened;
my %diffunopened;
my %missingunopened;

my $line;
foreach $line ( @unopenedlist )
{
	my $same = 0;
	my $diff = 0;
	my $missing = 0;
	if( $line =~ s/^same //i )
	{
		$same = 1;
	}
	elsif( $line =~ s/^diff //i )
	{
		$diff = 1;
	}
	elsif( $line =~ s/^missing //i )
	{
		$missing = 1;
	}
	else
	{
		die "checkoutincfiles.pl don't understand p4 diff -sl results: $line\n";
	}

	# clean up the filename
#	print "before: $line\n" if $line =~ m/aftershock_vs20/i;
	$line = NormalizePerforceFilename( $line );
#	print "after: \"$line\"\n" if $line =~ m/aftershock_vs20/i;
#	if( $line =~ m/aftershock/i )
#	{
#		print "unopenedlist: $line same: $same diff: $diff missing: $missing\n";
#	}

	# Save off the results for each line so that we can query them later.
	if( $same )
	{
		$sameunopened{$line} = 1;
	}
	elsif( $diff )
	{
		$diffunopened{$line} = 1;
	}
	elsif( $missing )
	{
		$missingunopened{$line} = 1;
	}
	else
	{
		die;
	}
}

# -sr     Opened files that are the same as the revision in the
#         depot.
my @openedbutsame = &RunCommand( "p4 diff -sr $diffpath" );

my %sameopened;

foreach $line ( @openedbutsame )
{
	if( $line =~ m/not opened on this client/i )
	{
		next;
	}
	# clean up the filename
#	print "before: $line\n" if $line =~ m/aftershock_vs20/i;
	$line = NormalizePerforceFilename( $line );
#	print "after: $line\n" if $line =~ m/aftershock_vs20/i;
#	if( $line =~ m/aftershock/i )
#	{
#		print STDERR "sameopened: $line\n";
#	}
	$sameopened{$line} = 1;
}

my @sameunopened;
my @revert;
my @edit;
my @add;

foreach $line ( @inclist )
{
	if( defined( $sameunopened{$line} ) )
	{
		push @sameunopened, $line;
	}
	elsif( defined( $sameopened{$line} ) )
	{
		push @revert, $line;
	}
	elsif( defined( $diffunopened{$line} ) )
	{
		push @edit, $line;
	}
	elsif( defined( $missingunopened{$line} ) )
	{
		printf STDERR "p4autocheckout.pl: $line missing\n";
	}
	else
	{
		push @add, $line;
	}
}

#print "\@sameunopened = @sameunopened\n";
#print "\@revert = @revert\n";
#print "\@edit = @edit\n";
#print "\@add = @add\n";

# Get the changelist number for the named changelist if we are actually going to edit or add anything.
# We don't need it for deleting.
my $changelistarg = "";
# Get the changelist number for the Shader Auto Checkout changelist. Will create the changelist if it doesn't exist.
my $changelistnumber = `valve_p4_create_changelist.cmd . \"$changelistname\"`;
# Get rid of the newline
$changelistnumber =~ s/\n//g;

#print STDERR "changelistnumber: $changelistnumber\n";

if( $changelistnumber != 0 )
{
	$changelistarg = "-c $changelistnumber"
}

#my %sameunopened;
#my %diffunopened;
#my %missingunopened;
#my %sameopened;

if( scalar @edit )
{
	while( scalar @edit )	
	{	
		# Grab 10 files at a time so that we don't blow cmd.exe line limits.
		my @files = splice @edit, 0, 10;
		my $cmd = "p4 edit $changelistarg @files";
#		print STDERR $cmd . "\n";
		my @results = &RunCommand( $cmd );
#		print STDERR @results;
		&PrintCleanPerforceOutput( @results );
	}
}

if( scalar @revert )
{
	while( scalar @revert )	
	{	
		# Grab 10 files at a time so that we don't blow cmd.exe line limits.
		my @files = splice @revert, 0, 10;
		my $cmd = "p4 revert @files";
#		print STDERR $cmd . "\n";
		my @results = &RunCommand( $cmd );
		&PrintCleanPerforceOutput( @results );
	}
}

if( scalar @add )
{
	while( scalar @add )	
	{	
		# Grab 10 files at a time so that we don't blow cmd.exe line limits.
		my @files = splice @add, 0, 10;
		my $cmd = "p4 add $changelistarg @files";
#		print STDERR $cmd . "\n";
		my @results = &RunCommand( $cmd );
#		print STDERR "@results\n";
		&PrintCleanPerforceOutput( @results );
	}
}

