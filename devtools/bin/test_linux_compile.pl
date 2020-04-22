#perl

#! perl

use Getopt::Long;

$mail = 1;
$mailto="cgreen";
$mailfrom="cgreen\@valvesoftware.com";
$branch="main";


$result = GetOptions(
					 "nosync" => \$nosync,
					 "clean" => \$clean,
					 "mailto:s" => \$mailto,
					 "branch:s" => \$branch,
					 "mailfrom:s" => \$mailfrom,
					 "mail" => \$mail );

print STDERR `p4 sync` unless ($nosync);
print STDERR `perl devtools/bin/vpc2linuxmake.pl`;
print STDERR `make clean` if ($clean);

open(ERROROUT, "./linux_makeinorder.sh 2>&1 |" ) || die "can't create pipe to compile";
while(<ERROROUT>)
  {
	my $iserror = /error:/;
	$iserror = 1 if (/^.*:\d+:\d+:/);
	$errtxt .= $_ if ($iserror );
	print $_;
  }
if (length($errtxt) )
  {
	if ($mail)
	  {
		use Net::SMTP;
		
		open CHANGES, "p4 changes -m 10 -s submitted //ValveGames/$branch/src/...|";
		my @changes = <CHANGES>;
		close CHANGES;
		
		$smtp = Net::SMTP->new('exchange2.valvesoftware.com');
		$smtp->mail($mailfrom);
		$smtp->to($mailto);
		
		$smtp->data();
		$smtp->datasend("To: $mailto\n");
		$smtp->datasend("Subject: [$branch broken in linux]\n");
		$smtp->datasend("\nThere are errors building $branch for linux.\nSome help is available at http://intranet.valvesoftware.com/wiki/index.php/Writing_code_that_is_compatible_between_gcc_and_visual_studio\n\n$errtxt");
		$smtp->datasend("-" x 75);
		$smtp->datasend("\nLAST 10 SUBMITS TO MAIN:\n");
		$smtp->datasend(join("",@changes ) );
		$smtp->dataend();
		$smtp->quit;
	  }
	else
	  {
		print STDERR "*****ERRORS****\n$errtxt\n";
	  }
  }


  
