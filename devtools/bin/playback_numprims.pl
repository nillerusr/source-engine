if( scalar( @ARGV ) != 1 )
{
	die "Usage: playback_numprims.pl frame.txt\n";
}
open INPUT, shift || die;
$numprims = 0;
$numcalls = 0;
while( <INPUT> )
{
	if( /DrawIndexedPrimitive.*numPrimitives:\s*(\d+)\s*$/i )
	{
		$numprims += $1;		
		if( $1 > 85 )
		{
			$numfreeprims += $1;
		}
		else
		{
			$numfreeprims += 85;
		}
		$numcalls++;
	}
}
close INPUT;
print "$numprims primitives\n";
print "$numfreeprims freeprimitives\n";
print "$numcalls calls\n";
