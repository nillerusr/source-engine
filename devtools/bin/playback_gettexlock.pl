# This gives you all calls between lock and unlock for the specified texture.
# gettexlock.pl texid blah.txt
$desiredTexture = shift;
open INPUT, shift || die;
while( <INPUT> )
{
	if( /Dx8LockTexture: id: (\d+)\s+/ )
	{
		if( $1 == $desiredTexture )
		{
			print "----------------------------------------------------\n";
			print;
			$gotit = 1;
		}
	}
	elsif( $gotit && /Dx8UnlockTexture: id: (\d+)\s+/ )
	{
		die if $1 != $desiredTexture;
		print;
		$gotit = 0;
	}
	else
	{
		print if( $gotit );
	}
}
close INPUT;
