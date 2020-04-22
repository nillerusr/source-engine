if( scalar( @ARGV ) != 2 )
{
	die "Usage: playback_getframe.pl framenum blah.txt\n";
}

# getframe.pl framenum blah.txt
$line = 0;
$desiredFrame = shift;
$frame = 1;
open INPUT, shift || die;
while( <INPUT> )
{
	$line++;
	if( /Dx8Present/ )
	{
		$frame++;
	}
	if( $frame == $desiredFrame )
	{
		print "$line: $_";
	}
	last if $frame > $desiredFrame;
}
close INPUT;
