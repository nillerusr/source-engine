# getframe.pl vbid blah.txt
$desiredVB = shift;
open INPUT, shift || die;
while( <INPUT> )
{
	if( /Dx8VertexData: id: (\d+)\s+/ )
	{
		if( $1 == $desiredVB )
		{
			print;
			$gotit = 1;
		}
	}
	elsif( $gotit && /^vertex:/ )
	{
		print;
	}
	else
	{
		$gotit = 0;
	}
}
close INPUT;
