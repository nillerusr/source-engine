#! perl

# make a simple fixed pak file for testing code. This utility is only for testing the code
# before writing the "real" utility. 

use File::Find;
use String::CRC32;
use File::Basename;




$ndatfileindex=0;
$ndatoffset=0;

$nullbyte = pack("C",0);


# now, search for files
find( {wanted => \&gotfile, no_chdir=>1 }, "." );

undef $curext;

$datlimit=50 * 1000 * 1000;

foreach $_ ( sort ByExtensionAndDirectory @FilesToPack )
  {
	local($basename, $dir, $ext ) = fileparse( $_, qr/\.[^.]*/);
	$dir=~s@\\@/@g;
	$dir=~s@^\./@@;
	$dir=~s@/$@@;
	$dir=" " unless ( length($dir) );
	$ext=~s@^\.@@;
	$ext=" " unless ( length($ext) );
	print "Add $_ ($dir)\n";

	if ( $curext ne $ext )
	  {
		$dirout.=$nullbyte.$nullbyte if length($curext);		# mark no more files and end of extension
		$dirout.=$ext.$nullbyte;
		$dirout.=$dir.$nullbyte;
		$curext=$ext;
		$curdir=$dir;
	  }
	elsif ( $curdir ne $dir )
	  {
		$dirout.=$nullbyte if length($curdir);		# mark no more files
		$dirout.=$dir.$nullbyte;
		$curdir = $dir;
	  }
	$dirout.=$basename.$nullbyte;
 	open(DATAIN, $_) || die "can't open $_";
 	binmode DATAIN;
 	{ local($/); $fdata=<DATAIN>; }
 	close DATAIN;
	$dirout.=pack("V", CRC32( $fdata) );
	$dirout.=pack("v",0); #meta data size
	$dirout.=pack("C",$ndatfileindex);
	$dirout.=pack("V",length($dataout));
	$dirout.=pack("V",length($fdata));
	$dataout.=$fdata;
	$dirout.=pack("C",-1);
	if (length($dataout) > $datlimit )
	  {
		&writedata;
		undef $dataout;
		$ndatfileindex++;
	  }

  }
$dirout.=$nullbyte.$nullbyte;

open(DIROUT,">test_dir.vpk") || die;
binmode DIROUT;
print DIROUT $dirout;
close DIROUT;
&writedata;


sub writedata
  {
	my $fname=sprintf("test_%03d.vpk", $ndatfileindex );
	print STDERR "\nWriting $fname, length ", length($dataout),"\n";
	open(DATAOUT,">$fname") || die;
	binmode DATAOUT;
	print DATAOUT $dataout;
	close DATAOUT;
  }

sub gotfile
  {
	return if ( -d $_ );
	s@\\@/@g;
	s@^\./@@;				# kill leading "./"
	$_=lc($_);
	local($basename, $dir, $ext ) = fileparse( $_, qr/\.[^.]*/);
	return if ($basename=~/\.360$/);
	return if ( $ext eq ".dll" );
	return if ( $ext eq ".vpk" );
	return unless length($ext);
	#	return unless ( $ext eq ".vtf" );
	push @FilesToPack, $_;
  }


sub ByExtensionAndDirectory
  {
	local($basenamea, $dira, $exta ) = fileparse( $a, qr/\.[^.]*/);
	local($basenameb, $dirb, $extb ) = fileparse( $b, qr/\.[^.]*/);
	return $exta cmp $extb if ( $extb ne $exta );
	return $dira cmp $dirb if ( $dira ne $dirb );
	return $basenamea cmp $basenameb;
  }
