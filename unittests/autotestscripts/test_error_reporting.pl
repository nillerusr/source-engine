#! perl
$errfname="//fileserver/user/cgreen/force_an_error.txt";

if (-e $errfname )
  {
	unlink $errfname;
	open(ERROUT,">errors.txt") || die "huh - can't write";
	{
	  print ERROUT "This is not an error - its just a test of the error script system.\n";
	  close ERROUT;
	}

  }
