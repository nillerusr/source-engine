#!/usr/bin/perl -w
use Getopt::Long;
use Pod::Usage;
use Sys::Hostname;
use File::Copy;
use File::Path;
use Time::HiRes qw(gettimeofday tv_interval);
use Cwd;
use strict;

use vars qw(%TESTS %STATS $ABORT_RUN $MPI_GRAPHICS);

# To add a new test, just create a new hash entry that has code
# references for the Prep, Run and Clean stages of the test.
# The new test can  be selected using the -test option. 

%TESTS = (
    'vrad' => {
	'PREP'  => \&VRADPrep,
	'RUN'   => \&VRADRun,
	'CLEAN' => \&VRADClean,
    },

    'vvis' => {
	'PREP'  => \&VVISPrep,
	'RUN'   => \&VVISRun,
	'CLEAN' => \&VVISClean,
    },

    'shadercompile' => {
	'PREP'  => \&ShaderPrep,
	'RUN'   => \&ShaderRun,
	'CLEAN' => \&ShaderClean,
    }   
    );

%STATS = ();
$ABORT_RUN = 0;
$MPI_GRAPHICS = 0;

local $SIG{INT} = sub {
    $ABORT_RUN = 1;
};

my $start = 4;
my $stop = 32;
my $step = 4;
my $test = "vrad";
my $list = undef;
my $help = 0;
my $man = 0;

my @work_list = ();
GetOptions("file=s" => \$list,
	   "test=s" => \$test,
	   "workerlist=s" => sub { 
	       shift; local $_ = shift; 
	       @work_list = split(',', $_) 
	   },
	   "start|s=i" => \$start,
	   "stop|e=i" => \$stop,
	   "step=i" => \$step,
	   "graphics" => \$MPI_GRAPHICS,
	   "help|?" => \$help,
	   "man" => \$man) or pod2usage(2);
pod2usage(1) if $help;
pod2usage(-exitstatus => 0, -verbose => 2) if $man;

my @extra_args = @ARGV;

unless (@work_list) {
    for (my $workers = $stop; $workers >= $start; $workers -= $step) {
	push @work_list, $workers;
    }
}

if (defined($list)) {
    @work_list = ReadMachineList($list, \@work_list);
}

unless (@work_list) {
    die "No workers in list\n";
}

my $logfile = "$test-$$.log";
print "Testing: ", join(", ", @work_list), "\n";
print "Logging to $logfile\n";

# Redirect console to log file and unbuffer the output
open STDOUT, ">$logfile";
open STDERR, ">>$logfile";
my $oldfh = select(STDOUT); $| = 1;
select(STDERR); $| = 1;
select($oldfh);

# Lock the list of machines if given
# Prepare for the test
# Run the test over the work list
# Clean up after the test
# Release lock on list of machines if given

my $pass = defined($list) ? ReserveMachines($list, $test) : '';
TestPrep($test, @extra_args);
for my $workers (@work_list) {
    last if $ABORT_RUN;
    TestRun($test, $workers, $pass, @extra_args);
}
TestClean($test, @extra_args);
ReleaseMachines($list) if defined($list);

sub ReadMachineList 
{
    my $list = shift;
    my $work_list = shift;

    my @machines = ();

    if (open(my $listfh, $list)) {
	while(my $line = <$listfh>) {
	    chomp($line);
	    next unless $line =~ /\S/;
	    push @machines, $line;
	}
    }

    my @capped_list = grep { $_ <= scalar(@machines) } @{$work_list};
    if ($#{$work_list} > $#capped_list) {
	print "Not enough machines to run test\n";
	print "Reducing max workers\n\n";
    }
    return @capped_list;
}

sub SetVMPIPass {
    my $machines = shift;
    my $pass = shift;

    system("vmpi_chpass.pl", "-p", $pass, "-f", $machines);
}

sub ReserveMachines 
{
    my $list = shift;
    my $pass = shift;

    my $host = lc hostname();
    $pass .= "-test-$host-$$";
    SetVMPIPass($list, $pass);
    return $pass;
}

sub ReleaseMachines 
{
    my $machines = shift;
    SetVMPIPass($machines, '');
}

sub DoTestFunc
{
    my $test = shift;
    my $func = shift;
    my $workers = $_[0];

    if (exists($TESTS{$test}{$func})) {
	my $start = [gettimeofday];
	&{$TESTS{$test}{$func}}(@_);
	my $stop = [gettimeofday];
	my $time = tv_interval($start, $stop);
	$STATS{$func}{$workers} = $time / 60;
    } 
    else {
	die "Failed to locate test function for: $test($func)\n";
    }
}

sub TestPrep
{
    my $test = shift;
    DoTestFunc($test, 'PREP', 0, '', @_);
}

sub TestRun
{
    my $test = shift;
    DoTestFunc($test, 'RUN', @_);
}

sub TestClean
{
    my $test = shift;
    DoTestFunc($test, 'CLEAN', 0, '', @_);
}

sub GetMPIArgs 
{
    my $n_workers = shift;
    my $pass = shift;

    my @args = ("-mpi");
    push(@args, "-mpi_workercount", $n_workers) if $n_workers > 0;
    push(@args, "-mpi_pw", $pass) if $pass;
    push(@args, "-mpi_graphics", "-mpi_trackevents") if $MPI_GRAPHICS;
    return @args;
}


sub VRADPrep
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;
    my @extra_args = @_;
    my @mpi_args = GetMPIArgs($n_workers, $pass);

    system("vbsp", $basename);
    system("vvis", @mpi_args, @extra_args, $basename);
    copy("$basename.bsp", "$basename-$$.bsp");
}

sub VRADRun
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;
    my @extra_args = @_;
    my @mpi_args = GetMPIArgs($n_workers, $pass);

    copy("$basename-$$.bsp", "$basename.bsp");
    system("vrad", "-final", "-staticproppolys", "-staticproplighting", 
	   @mpi_args, @extra_args, $basename);

}

sub VRADClean 
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;
    
    unlink("$basename.bsp", "$basename-$$.bsp");
}


sub VVISPrep
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;
    my @mpi_args = GetMPIArgs($n_workers, $pass);

    system("vbsp", $basename);
    copy("$basename.bsp", "$basename-$$.bsp");
}

sub VVISRun
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;
    my @extra_args = @_;
    my @mpi_args = GetMPIArgs($n_workers, $pass);

    copy("$basename-$$.bsp", "$basename.bsp");
    system("vvis", @mpi_args, $pass, @extra_args, $basename);
}

sub VVISClean 
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;
    
    unlink("$basename.bsp", "$basename-$$.bsp");
}

sub ShaderPrep
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;

    $ENV{DIRECTX_SDK_VER}='pc09.00';
    $ENV{DIRECTX_SDK_BIN_DIR}='dx9sdk\\utilities';
    $ENV{PATH} .= ";..\\..\\devtools\\bin";

    my $src_base = "../..";
    my $dos_base = $src_base;
    $dos_base =~ s|/|\\|g;

    unlink("makefile.$basename");
    unlink(qw(filelist.txt filestocopy.txt filelistgen.txt inclist.txt vcslist.txt));
    rmtree("shaders");
    mkpath(["shaders/fxc", "shaders/vsh", "shaders/psh"]);

    print "Update Shaders\n";
    system("updateshaders.pl", "-source", $dos_base, $basename);

    print "Prep Shaders\n";
    system("nmake", "/S", "/C", "-f", "makefile.$basename");
    if (open(my $fh, ">>filestocopy.txt")) {
	print $fh "$dos_base\\$ENV{DIRECTX_SDK_BIN_DIR}\\dx_proxy.dll\n";
	print $fh "$dos_base\\..\\game\\bin\\shadercompile.exe\n";
	print $fh "$dos_base\\..\\game\\bin\\shadercompile_dll.dll\n";
	print $fh "$dos_base\\..\\game\\bin\\vstdlib.dll\n";
	print $fh "$dos_base\\..\\game\\bin\\tier0.dll\n";
    }

    print "Uniqify List\n";
    system("uniqifylist.pl < filestocopy.txt > uniquefilestocopy.txt");
    copy("filelistgen.txt", "filelist.txt");
    print "Done Prep\n";
}

sub ShaderRun
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;
    my @extra_args = @_;
    my @mpi_args = GetMPIArgs($n_workers, $pass);

    my $old_dir = getcwd();
    my $dos_dir = $old_dir;
    $dos_dir =~ s|/|\\|g;

    system("shadercompile", "-allowdebug", "-shaderpath", $dos_dir,  @mpi_args, @extra_args);
}

sub ShaderClean
{
    my $n_workers = shift;
    my $pass = shift;
    my $basename = shift;

    unlink("makefile.$basename");
    unlink(qw(filelist.txt filestocopy.txt filelistgen.txt inclist.txt vcslist.txt));
    mkpath(["shaders/fxc", "shaders/vsh", "shaders/psh"]);
}

END {
    if (%STATS) {
	print "\n\n", "-"x70, "\n\n";
	for my $func (qw(PREP RUN CLEAN)) {
	    print "$func\n";
	    print "="x length($func), "\n";
	    for my $workers (sort {$a <=> $b} keys %{$STATS{$func}}) {
		printf("%3d, %6.3f\n", $workers, $STATS{$func}{$workers});
	    }
	    print "\n";
	}
    }
}

__END__

=head1 NAME

vmpi_test.pl - Test utility to automate execution of VMPI tools

=head1 SYNOPSIS

vmpi_test.pl [-test <test name>] [-file <host file>] [-start <num>] [-stop <num>] [-step <num>] [-workerlist <list>]  [-graphics] [-help|-?] [-man]

 Options:
  -test		The name of the test to run
  -file		A file that contains the names of machines to use
  -start	Lowest worker count to test
  -stop		Highest worker count to test
  -step		Interval to increment worker count
  -workerlist	A comma separated list of worker counts to test
  -graphics	Enable MPI visual work unit tracker
  -help|-?	Display command line usage
  -man		Display full documentation

=head1 DESCRIPTION

B<vmpi_test.pl> executes a specified test for each number of worker
counts given on the command line. The worker counts can be provided as
a start, stop and step relationship, or it can be specified using a
comma separated list. An optional host list file can be provided to
restrict the test to a given set of machines. These machines will have
a VMPI password applied to them so that you will get exclusive access
to them.

=cut
