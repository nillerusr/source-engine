#!/usr/bin/perl -w

BEGIN {
    # Ensure that we have the MIME::Entity package installed first
    eval { require MIME::Entity };
    if ($@) {
	$ENV{http_proxy}='http://squid.valvesoftware.com/';
	system('ppm', 'install', 'MIME::Entity');
    }
}

use Getopt::Long;
use Pod::Usage;
use MIME::Entity;
use File::Basename;
use Archive::Zip;
use FindBin;
use Win32;
use strict;

my @NOTIFICATION_LIST = qw(milton@valvesoftware.com dussault@valvesoftware.com);
my $LOGMAN_EXE = "$ENV{SystemRoot}\\System32\\logman.exe";

my $log = undef;
my $help = 0;
my $man = 0;
my $collection = "bad";
my $run_for = 15;

GetOptions("log=s" => \$log,
	   "bad" => sub { $collection = "bad" },
	   "ok" => sub { $collection = "ok" },
	   "runfor=i" => \$run_for,
	   "help|?" => \$help,
	   "man" => \$man) or pod2usage(2);
pod2usage(1) if $help;
pod2usage(-exitstatus => 0, -verbose => 2) if $man;

if ($log) {
    SendLog($log);  
} 
else {
    StartLogging($collection);
}
exit 0;

sub SendLog {
    my $log = shift;    
    my $logname = basename($log, ".blg");

    print "Compressing $log to $logname.zip\n";
    my $zip = Archive::Zip->new();
    $zip->addFile($log);
    $zip->writeToFileNamed("$logname.zip");

    my $user = Win32::LoginName();
    $user =~ s|^\\valve\\||i;

    my $machine = uc Win32::NodeName();

    print "Sending: $logname.zip from $user\@$machine\n";

    my $message = MIME::Entity->build(Type => "multipart/mixed",
				      From => "$user\@valvesoftware.com",
				      To => join(", ", @NOTIFICATION_LIST),
				      Subject => "WTF: $machine: $logname");
    $message->attach(Path => "$logname.zip", 
		     Type => "binary/octet-stream", 
		     Encoding => "base64");
    
    $message->send("smtp", Server => "exchange3.valvesoftware.com");
    unlink("$logname.zip");
}

sub StartLogging {
    my $collection = shift;

    unless (CheckCollection($collection)) {
	InstallCollection($collection) || die "Failed to install collection\n";
    }

    StopCollection($collection);
    if (StartCollection($collection)) {
	local $| = 1;
	print "Collecting samples: ";
	while($run_for > 0) {
	    print $run_for % 5 ? "." : $run_for;
	    #IsRunningCollection($collection);
	    sleep(1);
	    $run_for--;
	}
	print "Done\n";
	if (StopCollection($collection)) {
	    my $log = FindLog($collection);
	    if ($log) {
		SendLog($log);
	    }
	}
    }
}

sub CheckCollection {
    my $collection = shift;

    if (open(my $pipe, "$LOGMAN_EXE query WTF-$collection |")) {
	while(my $line = <$pipe>) {
	    if ($line =~ /Collection "WTF-$collection" does not exist/) {
		return;
	    }
	    elsif ($line =~ /Name:\s+WTF-$collection/) {
		return 1;
	    }
	}
    }
    return;
}

sub IsRunningCollection {
    my $collection = shift;

    if (open(my $pipe, "$LOGMAN_EXE query WTF-$collection |")) {
	while(my $line = <$pipe>) {
	    if ($line =~ /^Status:\s+(\w+)/) {
		my $status = $1;
		print "STATUS: $status\n";
		return 1 if ($status eq 'Running');
		return 1 if ($status eq 'Pending');
		return 0;
	    }
	}
    }
    return 0;
}

sub InstallCollection {
    my $collection = shift;

    print "Create WTF-$collection collection\n";
    system("$LOGMAN_EXE", "create", "counter", "WTF-$collection", "-si", 1, "-cf", "$FindBin::Bin\\wtf.txt");
    return if ($?);
    return 1;
}

sub StartCollection {
    my $collection = shift;

    print "Start WTF-$collection collection\n";
    eval {
	system("$LOGMAN_EXE", "start", "WTF-$collection");
	die "Starting Collection: $!\n" if ($?);
    };
    return 1;
}

sub StopCollection {
    my $collection = shift;

    print "Stop WTF-$collection collection\n";
    eval {
	system("$LOGMAN_EXE", "stop", "WTF-$collection");
	die "Stopping Collection: $!\n" if ($?);
	while (IsRunningCollection($collection)) {
	    sleep 1;
	}
    };
    return 1;
}

sub FindLog {
    my $collection = shift;
    if (opendir(my $dirh, "C:\\PerfLogs")) {
	my @files = sort { (stat("c:\\PerfLogs\\$a"))[9] <=> (stat("c:\\PerfLogs\\$b"))[9] } grep {
	    /^WTF-$collection\_\d+\.blg$/
	} readdir($dirh);
	my $log = $files[-1];
	print "Located latest log: $log\n";
	return "C:\\PerfLogs\\$log";
    }
    print "No log found\n";
    return;
}

END {
    if (IsRunningCollection($collection)) {
	StopCollection($collection);
    }
}

__END__

=head1 NAME

wtf.pl - Grabs a small capture of the performance data for the local machine and sends the information to the VMPI maintainers

=head1 SYNOPSIS

wtf.pl [-runfor <time>] [-help|-?] [-man] -log <log> | -bad | -good 
 
 Options:
  -bad		Captures the data to the "bad" log (default)
  -good		Captures the data to the "good" log
  -log		Specifies the log to send
  -runfor	Specified the amount of time to sample for
  -help|-?	Display command line usage
  -man		Display full documentation

=head1 DESCRIPTION

B<wtf.pl> is for capturing information about your system when VMPI is
doing something "bad". The default behaviour is to capture 15 seconds
of data and send the performance log to the VMPI maintainers. You can
optionally run another capture to show a "good" situation for a
baseline to compare against.

=head1 BUGS

The logman program that is used by wtf.pl does not support the -rc
command properly, so I cannot register wtf.pl to automatically send
the log when the capture ends. Instead I must manually start/wait/stop. 

=cut
