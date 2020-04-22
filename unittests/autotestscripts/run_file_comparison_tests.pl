#!perl

# read stdio_test_list.cfg and perform all tests

$create_refs=0;
$subset_string=shift;

@months = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
@weekDays = qw(Sun Mon Tue Wed Thu Fri Sat Sun);
($second, $minute, $hour, $dayOfMonth, $month, $yearOffset, $dayOfWeek, $dayOfYear, $daylightSavings) = localtime();
$year = 1900 + $yearOffset;
$dstamp = "$weekDays[$dayOfWeek] $months[$month] $dayOfMonth $year".sprintf(" %02d:%02d:%02d", $hour,$minute,$second);
$changelist_counter=`p4 counter main_changelist`;         # grab value of p4 counter

$dstamp.=" $changelist_counter";
$dstamp=~ s/[\n\r]//g;

$computername=$ENV{'COMPUTERNAME'};


# first, set our priority to high and affinity to 1 to try to get more repeatable benchmark results
#my $pid = $$;

#my $cmd="datafiles\\process.exe -p $pid High";
#print STDERR `$cmd`;
#$cmd="datafiles\\process.exe -a $pid 01";
#print STDERR `$cmd`;

if ( open(CFGFILE, "filecompare_tests.cfg") )
  {
	while(<CFGFILE>)
	  {
		s/[\n\r]//g;
		s@//.*$@@;				# kill comments
		if (/^([^,]*),([^,]*),(.*$)/)
		  {
			$testname=$1;
			$testfile=$2;
			$testcommand=$3;

			next if ( length($subset_string) && ( ! ( $testname=~/$subset_string/i) ) );

			$ext=".txt";
			if ( length($testfile ) )
			  {
				$ext="";
				$ext = $1 if ( $testfile=~/(\..*)$/ ); # copy extension
				unlink $testfile if ( -e $testfile); # kill it if it exists
			  }

			print STDOUT "running $testname :  $testcommand ($testfile)\n";
			# suck the reference output in. use binary mode unless stdio
			$refname="reference_output/$testname$ext";

			# run the test
			my $stime=time;
			$output=`$testcommand`;
			$stime=time-$stime;

			if ( open(REF,$refname))
			  {
				if ( length($testfile ))
				  {
					binmode REF;
				  }
				$ref_output= do { local( $/ ) ; <REF> } ; # slurp comparison output in
				close REF;

				if ( length( $testfile ) )
				  {
					print STDERR $output;
					# file case
					if ( open(TESTFILE, $testfile ))
					  {
						binmode TESTFILE;
						$new_output= do { local( $/ ) ; <TESTFILE> } ; # slurp comparison output in
						close TESTFILE;
						if ($new_output ne $ref_output )
						  {
							$errout.="ERROR: test $testname ($testcommand) : test produced file $testfile (length=".
							  length($new_output).") but it doesn't match the reference (length=".length($ref_output).")\n";
						  }
						else
						  {
							&UpdateMetrics( $testname, $output, $stime );
						  }
					  }
					else
					  {
						$errout.="ERROR: test $testname ($testcommand) : test was supposed to create $testfile, but didn't.\n";
					  }
				  }
				else
				  {
					# strip metrics (like timing) for comparison
					my $massaged_ref = $ref_output;
					my $massaged_output = $output;
					$massaged_ref =~    s/:=\s*[0-9\.]+//g;
					$massaged_output =~ s/:=\s*[0-9\.]+//g;
					if ($massaged_output ne $massaged_ref )
					  {
#						print STDERR "o=$massaged_output r=$massaged_ref\n";
						$errout.="ERROR: test $testname ($testcommand) : output does not match reference output.\n";
					  }
					else
					  {
						&UpdateMetrics( $testname, $output, $stime );
					  }
				  }
			  }
			else
			  {
				$errout.="ERROR: Can't open reference $refname for $testname\n";
				if ($create_refs)
				  {
					if ( length($testfile ) )
					  {
						if ( -e $testfile )
						  {
							$oname=$refname;
							$oname=~s@/@\\@g;
							print STDERR "copy $testfile $oname";
							print STDERR `copy $testfile $oname`;
							print STDERR `p4 add $oname`;
						  }
					  }
					else
					  {
						if ( open(REFOUT,">$refname") )
						  {
							print REFOUT $output;
						  }
						close REFOUT;
						print STDERR `p4 add $refname`;
					  }
				  }
			  }
		  }
	  }
  }
else
  {
	$errout.="Can't open stdio_test_list.cfg\n";
  }

if (length($errout))
{
  print STDERR "There were errors: $errout";
  open(ERRORS,">errors.txt") || die "huh - can't write";
  print ERRORS $errout;
  close ERRORS;
}




sub UpdateMetrics
  {
	return unless length($computername);
	local( $tname, $output, $runtime) = @_;
	$output .= "\ntest runtime := $runtime\n";
	foreach $_ ( split(/\n/,$output))
	  {
		if (/^(.+):=(.*$)/)
		  {
			my $varname=$1;
			my $etime=$2;
			$varname=~s@^\s*@@g;
			$varname=~s@\s*$@@g;
			mkdir "\\\\fileserver\\user\\perf\\$computername";
			mkdir "\\\\fileserver\\user\\perf\\$computername\\$tname";
			if ( open(COUT,">>\\\\fileserver\\user\\perf\\$computername\\$tname\\$varname.csv") )
			  {
				print COUT "\"$dstamp\",$etime\n";
				close COUT;
			  }

		  }
	  }

  }
