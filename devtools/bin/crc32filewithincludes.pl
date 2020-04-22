use String::CRC32;

sub ReadInputFileWithIncludes
{
	local( $filename ) = shift;

	local( *INPUT );
	local( $output );

	open INPUT, "<$filename" || die;

	local( $line );
	local( $linenum ) = 1;
	while( $line = <INPUT> )
	{
		if( $line =~ m/\#include\s+\"(.*)\"/i )
		{
			$output.= ReadInputFileWithIncludes( $1 );
		}
		else
		{
			$output .= $line;
		}
	}

	close INPUT;
	return $output;
}

if( !scalar( @ARGV ) )
{
	die "Usage: crc32filewithincludes.pl filename\n";
}

$data = &ReadInputFileWithIncludes( shift );

#print "data: $data\n";

$crc = crc32( $data );
print $crc;
exit 0;
