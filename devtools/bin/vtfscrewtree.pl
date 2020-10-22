sub CreateFile
{
	local( $filename ) = shift;
	local( *FILE );

	open FILE, ">$filename";
	close FILE;
}

sub ProcessFile
{
	local( $filename ) = shift;
	local( @fileContents );
#	print "$filename\n";
	if( $filename =~ /\.vtf/i )
	{
		return if( $filename =~ /_normal/i );
		return if( $filename =~ /_dudv/i );
		local( $cmd ) = "..\\..\\..\\bin\\vtfscrew \"$filename\" $r $g $b";
		print $cmd . "\n";
		system $cmd;
	}
}

sub ProcessFileOrDirectory
{
	local( $name ) = shift;

#	If the file has "." at the end, skip it.
	if( $name eq "." || $name eq ".." || $name =~ /\.$/ )
	{
#		print "skipping: $name\n";
		return;
	}

#   Figure out if it's a file or a directory.
	if( -d $name )
	{
		local( *SRCDIR );
#		print "$name is a directory\n";
		opendir SRCDIR, $name;
		local( @dir ) = readdir SRCDIR;
		closedir SRCDIR;

		local( $item );
		while( $item = shift @dir )
		{
			&ProcessFileOrDirectory( $name . "/" . $item );
		}
	}
	elsif( -f $name )
	{
		&ProcessFile( $name );
	}
	else
	{
		print "$name is neither a file or a directory\n";
	}
	return;
}

$baseDirectory		= shift;
$r = shift;
$g = shift;
$b = shift;

if( !$baseDirectory )
{
	die "Usage: createvmt.pl baseDir";
}

print "baseDirectory = \"$baseDirectory\"\n";

opendir SRCDIR, $baseDirectory;
@dir = readdir SRCDIR;
closedir SRCDIR;

while( $item = shift @dir )
{
	&ProcessFileOrDirectory( "$baseDirectory/$item" );
}


