
#
#   Arguments parsing
#

local $script = $0;

while ( 1 )
{
	local $arg = shift;
	if ( $arg =~ /^-patch$/i )
		{	goto RUN_PATCH;	}
	if ( $arg =~ /^-patchall$/i )
		{	goto RUN_PATCH_ALL;	}

	# default mode - forward to -patch:
	exit system( "dir /s /b *.vtf | perl $script -patchall" );
}




RUN_PATCH_ALL:
#
#   -----------  Patch a list of VTF files --------
#

while ( <> )
{
	local $line = $_;
	$line =~ s/^\s*(.*)\s*$/$1/;
	PatchVtfFile( $line );
}
exit 0;


RUN_PATCH:
#
#   -----------  Patch a VTF file --------
#

local $filename = shift;
exit PatchVtfFile( $filename );

sub PatchVtfFile
{

local *fileptr;
local $filename = shift;
local $vtfFlags;

print "$filename ... ";
`p4 edit "$filename" >nul 2>&1`;

if ( !open( fileptr, "+<$filename" ) )
{
	print "ERROR!\n";
	return 1;
}
binmode( fileptr );

seek( fileptr, 5 * 4, 0 );
read( fileptr, $vtfFlags, 4 );
$vtfFlags = unpack( "I", $vtfFlags );

# dropping the NOCOMPRESS flag that will be used for SRGB
# dropping all the removed flags too
$vtfFlags = $vtfFlags & 0x2E87FFBF; # mask and

seek( fileptr, 5 * 4, 0 );
print fileptr pack( "I", $vtfFlags );

close( fileptr );

print "ok.\n";
return 0;

}


