#! perl

# make a simple fixed pak file for testing code. This utility is only for testing the code
# before writing the "real" utility. The files that are packed are fake



$ndatfileindex=0;
$ndatoffset=0;

$nullbyte = pack("C",0);

foreach $ext ("txt","vtf")
  {
	$dirout.=$ext.$nullbyte;
	foreach $dir("dir1","dir2")
	  {
		$dirout.=$dir.$nullbyte;
		foreach $file("test1","test2")
		  {
			$fdata=$file x 5;
			$dirout.=$dir.$nullbyte;
			$dirout.=pack("V",0); # fake crc
			$dirout.=pack("v",0); #meta data size
			$dirout.=pack("C",$ndatfileindex);
			$dirout.=pack("V",$ndatoffset);
			$dirout.=pack("V",length($dataout));
			$dataout.=$fdata;
			$dirout.=pack("V",-1);
		  }
	  }
	$dirout.=$nullbyte;
  }
$dirout.=$nullbyte;

open(DIROUT,">test.dir") || die;
binmode DIROUT;
print DIROUT $dirout;
close DIROUT;
open(DATAOUT,">test_000.dat") || die;
binmode DATAOUT;
print DATAOUT $dataout;
close DATAOUT;
