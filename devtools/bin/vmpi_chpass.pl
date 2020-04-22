#!/usr/bin/perl -w
use IO::Socket::INET;
use Sys::Hostname;
use Data::Dumper;
use Getopt::Long;
use Pod::Usage;
use strict;

use constant VMPI_PROTOCOL_VERSION => 5;

use constant VMPI_MESSAGE_BASE => 71;
use constant VMPI_PING_REQUEST => VMPI_MESSAGE_BASE+2;
use constant VMPI_PING_RESPONSE => VMPI_MESSAGE_BASE+3;
use constant VMPI_FORCE_PASSWORD_CHANGE => VMPI_MESSAGE_BASE+11;

use constant VMPI_PASSWORD_OVERRIDE => -111;

use constant VMPI_SERVICE_PORT => 23397;
use constant VMPI_LAST_SERVICE_PORT => VMPI_SERVICE_PORT + 15;

my $list = undef;
my $pass = "";
my $help = 0;
my $man = 0;

GetOptions("file=s" => \$list,
	   "pass=s" => \$pass,
	   "help|?" => \$help,
	   "man" => \$man) or pod2usage(1);
pod2usage(2) if $help;
pod2usage(-exitstatus=>0, -verbose=>2) if $man;

my @machines = @ARGV;
if ($list) {
    if (open(my $listfh, $list)) {
	while(my $line = <$listfh>) {
	    chomp($line);
	    next unless $line =~ /\S/;
	    push @machines, $line;
	}
    }
}

if (!@machines) {
    warn "No machines specified\n";
    pod2usage(3);
}

my $message = BuildMessage(VMPI_PROTOCOL_VERSION, VMPI_FORCE_PASSWORD_CHANGE);
$message .= pack("Z*", $pass);
my $length = length($message);

my $socket = CreateSocket();
# send the message 3 times to make sure it gets it
for (1..3) {
    for my $host (@machines) {
	SendMessage($socket, $host, $message);	
    }
    sleep(1);
}


sub CreateSocket {
    return IO::Socket::INET->new(Proto=>'udp');
}

sub BuildMessage {
    my $ver = shift;
    my $type = shift;

    my $message = pack("CcCC", $ver, VMPI_PASSWORD_OVERRIDE, 0, $type);
    return $message;
}

sub SendMessage {
    my $socket = shift;
    my $host = shift;
    my $message = shift;

    my $ip = gethostbyname($host);
    if (!$ip) {
	warn "Can't resolve: $host\n";
	return;
    }

    for my $port (VMPI_SERVICE_PORT..VMPI_LAST_SERVICE_PORT) {
	my $ipaddr = sockaddr_in($port, $ip);
	defined(send($socket, $message, 0, $ipaddr)) || warn("SEND: $!\n");
    }
}

__END__

=head1 NAME

vmpi_chpass.pl - Sets the VMPI password on a set of machines

=head1 SYNOPSIS

vmpi_chpass.pl [-pass <password>] [-help|-?] [-man] 
    -file <host file> | <host> ...

 Options:
  -file		A file that contains the names of machines to use
  -pass		Password to set
  -help|-?	Display command line usage
  -man		Display full documentation

=head1 DESCRIPTION

B<vmpi_chpass.pl> sets the password on the given list of machines. The
machines can be given as separate arguments on the command line or as
a list in a text file. If the password is not given, the password is
removed from the machines, opening them up for general use.

=cut
