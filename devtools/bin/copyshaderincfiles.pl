BEGIN {use File::Basename; push @INC, dirname($0); }
require "valve_perl_helpers.pl";
use Cwd;
use String::CRC32;

my $txtfilename = shift;
my $arg = shift;

my $is360 = 0;
my $platformextension = "";
if( $arg =~ m/-x360/i )
{
	$is360 = 1;
	$platformextension = ".360";
}

# Get the changelist number for the Shader Auto Checkout changelist. Will create the changelist if it doesn't exist.
my $changelistnumber = `valve_p4_create_changelist.cmd ..\\..\\..\\game\\hl2\\shaders \"Shader Auto Checkout VCS\"`;
# Get rid of the newline
$changelistnumber =~ s/\n//g;

my $changelistarg = "";
if( $changelistnumber != 0 )
{
	$changelistarg = "-c $changelistnumber"
}

open TXTFILE, "<$txtfilename";

my $src;
my $dst;
while( $src = <TXTFILE> )
{
	# get rid of comments
	$src =~ s,//.*,,g;

	# skip blank lines
	if( $src =~ m/^\s*$/ )
	{
		next;
	}

	# Get rid of newlines.
	$src =~ s/\n//g;
	
	# Save off the shader source filename.
	my $dst = $src;
	
	$dst =~ s/_tmp//gi;

	# Does the dst exist?
	my $dstexists = -e $dst;
	my $srcexists = -e $src;
	# What are the time stamps for the src and dst?
	my $srcmodtime = ( stat $src )[9];
	my $dstmodtime = ( stat $dst )[9];

	# Open for edit or add if different than what is in perforce already.
	if( !$dstexists || ( $srcmodtime != $dstmodtime ) )
	{
		# Make the target writable if it exists
		if( $dstexists )
		{
			MakeFileWritable( $dst );
		}

		my $dir = $dst;
		$dir =~ s,([^/\\]*$),,;  # rip the filename off the end
		my $filename = $1;

		# create the target directory if it doesn't exist
		if( !$dstexists )
		{
			&MakeDirHier( $dir, 0777 );
		}

		# copy the file to its targets. . . we want to see STDERR here if there is an error.
		my $cmd = "copy $src $dst > nul";
#		print STDERR "$cmd\n";
		system $cmd;
		
		MakeFileReadOnly( $dst );
	}
}

close TXTFILE;
