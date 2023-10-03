#!perl
open( HANDLE,shift) || die;
undef $/;
binmode HANDLE;
$data=<HANDLE>;
$ctr=0;
for($i=0;$i<length($data);$i++)
  {
 $out.=sprintf("0x%02x,", unpack("C", substr($data,$i,1)) );
 $out.="\n" if ( ( $ctr % 20) == 19);
 $ctr++;
  }
print $out;