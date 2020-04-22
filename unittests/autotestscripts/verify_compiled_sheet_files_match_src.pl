#! perl


use File::Find;
use Cwd;
use File::Basename;

#  find(\&Visitfile,"../../../content/tf");

foreach $_ ( @sheetfiles )
  {
	$dest_sheet_name=$_;
	$dest_sheet_name =~ s/\.mks/.sht/i;
	$dest_sheet_name =~ s@/content/([^/]+)/materialsrc/@/game/\1/materials/@gi;
	print "**Checking $_\n";
	if (! -e $dest_sheet_name )
	  {
		push @errors,"sheet $_ exists but not $dest_sheet_name";
	  }
	else
	  {
		# buid it and make sure they match
		$cmd="cd ".dirname($_)." & mksheet ".basename($_)." test.sht";
		$cmd=~tr/\//\\/;
		$cmdout=`$cmd`;
		if ( open(NEWFILE, dirname($_)."/test.sht") )
		  {
			binmode NEWFILE;
			open(OLDFILE, $dest_sheet_name ) || die "strange error - cant find $dest_sheet_name";
			binmode OLDFILE;
			{
			  local( $/, *FH ) ;
			  $old_data=<OLDFILE>;
			  $new_data=<NEWFILE>;
			  if ( $new_data ne $old_data )
				{
				  push @errors,"Sheet source file $_ does not compile to the same output as the checked in $dest_sheet_name";
				}
			  close OLDFILE;
			  close NEWFILE;
			  unlink dirname($_)."/test.sht";
			}
		  }
		else
		  {
			push @errors, "Couldn't compile sheet $_ by running $cmd : \n$cmdout";
		  }
	  }
  }

$errout=join("\n", @errors);
print $errout;
if (length($errout))
  {
	open(ERRFILE,">errors.txt") || die "can't write errors.txt";
	print ERRFILE $errout;
	close ERRFILE;
  }


sub Visitfile
  {
	local($_)= $File::Find::name;
	s@\\@\/@g;
	if (m@content/(\S+)/.*\.mks$@i)
	  {
		push @sheetfiles, $_;
	  }
  }
