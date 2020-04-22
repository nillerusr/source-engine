#! perl

use File::Find;
use Cwd;
use File::Basename;
use Getopt::Long;



$last_errors="";
$nosync = 0;
$mail = 0;
$daemon = 0;
$mailto="cgreen";
$mailfrom="autotest";
$ignoremutex = 0;

$result = GetOptions(
					 "nosync" => \$nosync,
					 "daemon" => \$daemon,
					 "mailto:s" => \$mailto,
					 "mailfrom:s" => \$mailfrom,
					 "ignoremutex" => \$ignoremutex,
					 "mail" => \$mail );

$iter = 0;
while( 1 )
{
  my $hasChange = 1;
  my $mutex_held = 0;
  unless($nosync)
	{
	  # check for mutex held
	  unless( $ignoremutex || $nosync )
		{
		  open(MUTEX,"p4 counters|") || die "cant' run p4";
		  while(<MUTEX>)
			{
			  $mutex_held = 1 if ( /main_src_lock_\S+\s*=\s*1/);
			}
		  close MUTEX;
		}
	  unless( $mutex_held )
		{
		  print STDERR "Syncing...\n";
		  system "p4 sync > sync.txt 2>&1";
		  open SYNC, "<sync.txt";
		  while( <SYNC> )
			{
			  if( m/File\(s\) up-to-date/ )
				{
				  $hasChange = 0;
				}
			  print;
			}
		  close SYNC;
		}
	}
  if ( $mutex_held )
	{
	  print STDERR "mutex held, waiting\n";
	}
  else
	{
	  if( $hasChange || ($iter == 0 ) )
		{
		  print "Running tests\n";
		  &RunUnitTests;
		}
	  else
		{
		  print "no changes\n";
		}
	  $iter++;
	  last unless ($daemon);
	}
  sleep 30;
}



sub RunUnitTests
{
  $error_output ="";
  find(\&Visitfile,".");
  if ( length($error_output ) )
	{
	  print STDERR "errors detected\n";
	  open CHANGES, "p4 changes -m 10 -s submitted //ValveGames/main/src/...|";
	  my @changes = <CHANGES>;
	  close CHANGES;

	  if ( $mail && ($error_output ne $last_errors ) )
		{
		  use Net::SMTP;
		  
		  $smtp = Net::SMTP->new('exchange2.valvesoftware.com');
		  $smtp->mail($mailfrom);
		  $smtp->to($mailto);
		  
		  $smtp->data();
		  $smtp->datasend("To: $mailto\n");
		  $smtp->datasend("Subject: Errors from Unit tests\n");
		  $smtp->datasend($error_output);
		  $smtp->datasend("-" x 75);
		  $smtp->datasend("\nLAST 10 SUBMITS TO MAIN:\n");
		  $smtp->datasend(join("",@changes ) );
		  $smtp->dataend();
		  $smtp->quit;
		}
	}
  $last_errors = $error_output;
}

sub Visitfile
  {
	local($_)= $File::Find::name;
	next unless( -e "test_error_reporting.pl" );
	if (/\.(pl|exe|bat)$/i)
	  {
		unlink("errors.txt");
		my $extension=$1;
		$extension=~tr/A-Z/a-z/;
		my $cmd;
		$cmd="perl $_" if ( $extension eq "pl");
		$cmd="$_" if ( $extension eq "exe");
		$cmd="$_" if ( $extension eq "bat");
		print STDERR "run $cmd: ",`$cmd`,"\n";
		if (open(ERRIN,"errors.txt" ) )
		  {
			local($/);
			print STDERR "errors found!\n";
			$error_output.="* Failed test: $cmd: ".<ERRIN>."\n";
			close ERRIN;
		  }
	  }
}
