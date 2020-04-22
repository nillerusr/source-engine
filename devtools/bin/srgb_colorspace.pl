
#
#   Arguments parsing
#

local $script = $0;

while ( 1 )
{
	local $arg = shift;
	if ( $arg =~ /-catsrgb1/i )
		{	goto RUN_CAT_SRGB_1;	}
	if ( $arg =~ /-safecat/i )
		{	goto RUN_SAFE_CAT;	}
	if ( $arg =~ /-delaywrite/i )
		{	goto RUN_DELAY_WRITE;	}
	if ( $arg =~ /-processvmts/i )
		{	goto RUN_PROCESS_VMTS;	}

	# default mode - forward to processvmts:
	# exit system( "dir" );
	exit system( "dir /s /b *.vmt | perl $script -processvmts" );
}




RUN_CAT_SRGB_1:
#
#   -----------  Adding "srgb 1" to options list --------
#

local $bAlreadyHasSrgb1 = 0;
while ( <> )
{
	local $line = $_;
	print $line;
	$bAlreadyHasSrgb1 = 1 if $line =~ /srgb(.*)1/i;
}
print "\nsrgb 1\n" if !$bAlreadyHasSrgb1;
exit 0;



RUN_DELAY_WRITE:
#
#   -----------  Delay write to a file (essentially delayed shell redirection) ----
#
local *fileptr;
local $filename = shift;

local @lines;
while ( <> ) { push( @lines, $_ ); }

open( fileptr, ">$filename" );
foreach ( @lines ) { print fileptr $_ ; }
close( fileptr );

exit 0;



RUN_SAFE_CAT:
#
#   -----------  Cat of a non-existing file will succeed  ----
#
local *fileptr;
local $filename = shift;

exit 1 if !open( fileptr, "<$filename" );
while ( <fileptr> ) { print $_ ; }
close( fileptr );

exit 0;




RUN_PROCESS_VMTS:
#
#   -----------  Processing the list of vmts ------------
#


#
#   Lookup array of directories under which to search for
#   textures when resolving texture paths from vmt.
#
local @textureslookup = (
#	"u:/data/content/tf/",
#	"u:/data/content/portal/",

#	"u:/data/content/ep2/",
#	"u:/data/content/episodic/",
	"u:/data/content/hl2/",
);

#
#   Mapping of shaders that require srgb conversion for
#   basetextures. Entries listed lowercase, mapped to 1.
#
local %srgbshaders = (
	vertexlitgeneric            => 1,
	lightmappedgeneric          => 1,
	unlitgeneric                => 1,
	eyerefract                  => 1,
	portalrefract               => 1,
	aftershock                  => 1,
	sky                         => 1,
	lightmappedreflective       => 1,
	portalstaticoverlay         => 1,
	particlesphere              => 1,
	shatteredglass              => 1,
	sprite                      => 1,
	spritecard                  => 1,
	teeth                       => 1,
	treeleaf                    => 1,
	vortwarp                    => 1,
	water                       => 1,
	windowimposter              => 1,
	worldtwotextureblend        => 1,
	worldvertexalpha            => 1,
);

#
#   Mapping of variables that we will be looking for.
#
local %shadervarscheck = (
	basetexture					=> 1,
	basetexture2                => 1,
	basetexture3                => 1,
	basetexture4                => 1,
	envmap                      => 1,
	iris                        => 1,
	ambientoccltexture          => 1,
	refracttinttexture          => 1,
	albedo                      => 1,
	compress                    => 1,
	stretch                     => 1,
	emissiveblendbasetexture    => 1,
	emissiveblendtexture        => 1,
	fleshinteriortexture        => 1,
	fleshbordertexture1d        => 1,
	fleshsubsurfacetexture      => 1,
	fleshcubetexture            => 1,
	texture2                    => 1,
	hdrcompressedtexture        => 1,
	hdrcompressedtexture0       => 1,
	hdrcompressedtexture1       => 1,
	hdrcompressedtexture2       => 1,
	portalcolortexture          => 1,
	monitorscreen               => 1,
	staticblendtexture          => 1,
	refracttexture              => 1,
	reflecttexture              => 1,
);

#
#   After parsing the list of vmt files contains a map of
#   texture names as keys to the list of two elements:
#   [0]
#     0: texture is non-srgb
#     1: texture is srgb and needs conversion
#    -1: texture is ambiguous
#   [1]
#     list of vmts referring to the texture
#
local %textures2convert;


#
#   Parse our list of vmt files
#   Generate the list with "dir /s /b *.vmt"
#
while ( <> )
{

	local $fname = $_;
	$fname =~ s/^\s*(.*)\s*$/$1/;
	$fname =~ s/\\/\//g;
#	print "-" . $fname . "-\n";

	#	Open the file
	local *vmtfileptr;
	open( vmtfileptr, "<$fname" ) || die;

	local $line;
	local $shadername;
	local %basetexture;
	local $basetexture_real;
	local $basetexture_blacklisted = 0;

	#   Find shadername and textures
	while ( $line = <vmtfileptr> )
	{
		$line =~ s/^\s*(.*)\s*$/$1/;
		$line = lc( $line );

		if ( $. == 1 )
		{
			$shadername = $line;
			$shadername =~ s/\"//g;
		}

		while ( ( $varname, undef ) = each %shadervarscheck )
		{
			next if $line !~ /^\s*["]?\$$varname["]?\s+(.*)$/i;

			$line = $1;
			if ( $line =~ /^\"([^\"])*\"(.*)$/ ) {
				$line =~ s/^\"([^\"]*)\"(.*)$/$1/;
			} else {
				$line =~ s/^(\S*)(.*)$/$1/;
			}
			$line =~ s/^\s*(.*)\s*$/$1/;
			$line =~ s/\\/\//g;

			next if $line =~ /^\_rt/i;

			$basetexture{ $line } = 1;
			$basetexture_real = $line if $varname =~ /^basetexture$/i;
			last;
		}

		# blacklist stuff
		$basetexture_blacklisted = 1 if $line =~ /^\s*["]?\$extractgreenalpha["]?\s*(.*)$/i;
		print ">>> $fname blacklisted by >>> $line >>>\n" if $line =~ /^\s*["]?\$extractgreenalpha["]?\s*(.*)$/i;
	}

	delete $basetexture{ $basetexture_real } if $basetexture_blacklisted;
	print ">>> blacklisted texture: $basetexture_real .\n" if $basetexture_blacklisted;
	delete $basetexture{ "env_cubemap" };

	close vmtfileptr;

	#   Print the shader and the base texture
#	print "\tshader  = " . $shadername . "\n";
#	foreach ( keys %basetexture ) { print "\ttexture = " . $_ . "\n" };

	#   If the texture needs a conversion stick it into the map
	local $isSrgb = 1; #--shadercheck--( exists $srgbshaders{ $shadername } ? 1 : 0 );

	foreach ( keys %basetexture )
	{
		local $basetexture = $_;
		if ( exists $textures2convert{ $basetexture } )
		{
			#   Check for validity
			$isSrgb = -1 if $textures2convert{ $basetexture }->[0] != $isSrgb;
		}
		else
		{
			$textures2convert{ $basetexture } = [ $isSrgb, [] ];
		}
	
		$textures2convert{ $basetexture }->[0] = $isSrgb;
		push( @{ $textures2convert{ $basetexture }->[1] }, $fname );
	}

}


#
#   Now walk over the accumulated textures and establish the srgb mapping
#
local @failedtextures;
while ( ( $basetexture, $value ) = each %textures2convert )
{

	local $isSrgb = $value->[0];

	if ( -1 == $isSrgb )
	{
		print "Warning: Ambiguity for ";
		print "$basetexture from ";
		foreach ( @{ $value->[1] } ) { print " $_"; };
		print "!\n";
	}

	next if 1 != $isSrgb;

	#
	# Need to convert this texture
	#
	local $res = ConvertTexture( $basetexture );
	push( @failedtextures, $basetexture ) if !$res;

}

#   Dump failed textures
if ( $#failedtextures > 0 )
{
	print "\n---- errors ----\n";
	foreach ( @failedtextures )
	{
		local $basetexture = $_;
		print "ERROR: $basetexture  in  ";
		foreach ( @{ $textures2convert{ $basetexture }->[1] } ) { print " $_"; }
		print ";\n";
	}
	print "----  end   ----\n";
}


#
#   Converts a texture.
#   Returns 1 for txt, 2 for psd, 3 for newly created txt.
#   Returns 0 when nothing found.
#
sub ConvertTexture
{

	local $basetexture = shift;
	local $result = 0;
	print "-- $basetexture ... ";

	foreach ( @textureslookup )
	{
		# Try to open the txt file
		local $txpath = $_ . "materialsrc/" . $basetexture;
		if ( -f "$txpath.txt" || -f "$txpath.tga" )
		{
			# Check it out
			print "txt, p4 ... ";
			`p4 edit "$txpath.txt" >nul 2>&1`;
			`p4 lock "$txpath.txt" >nul 2>&1`;

			`perl $script -safecat "$txpath.txt" | perl $script -catsrgb1 | perl $script -delaywrite "$txpath.txt"`;

			`p4 add "$txpath.txt" >nul 2>&1` if -f "$txpath.txt";

			# Also patch the VTF
			$txpath =~ s,/content/,/game/,;
			$txpath =~ s,/materialsrc/,/materials/,;
			#PatchVtfFile( "$txpath.vtf" );

			print "done ... ";
			++ $result;
		}
		if ( -f "$txpath.psd" )
		{
			# Check it out
			print "psd, p4 ... ";
			`p4 edit "$txpath.psd" >nul 2>&1`;
			`p4 lock "$txpath.psd" >nul 2>&1`;

			`psdinfo -read "$txpath.psd" | perl $script -catsrgb1 | psdinfo -write "$txpath.psd"`;

			`p4 add "$txpath.psd" >nul 2>&1`;

			# Also patch the VTF
			$txpath =~ s,/content/,/game/,;
			$txpath =~ s,/materialsrc/,/materials/,;
			#PatchVtfFile( "$txpath.vtf" );

			print "done ... ";
			++ $result;
		}
	}

	foreach ( @textureslookup )
	{
		# Try to find the vtf file
		local $txpath = $_ . "materials/" . $basetexture;
		$txpath =~ s,/content/,/game/,i;
		if ( -f "$txpath.vtf" )
		{
			if ( !$result )
			{
				# Create the text file then
				print "src missing ... ";
				$txpath =~ s,/game/,/content/,,i;
				$txpath =~ s,/materials/,/materialsrc/,;
			
				`p4 edit "$txpath.txt" >nul 2>&1`;
				`p4 lock "$txpath.txt" >nul 2>&1`;

				`perl $script -safecat "$txpath.txt" | perl $script -catsrgb1 | perl $script -delaywrite "$txpath.txt"`;

				`p4 add "$txpath.txt" >nul 2>&1` if -f "$txpath.txt";

				# Also patch the VTF
				$txpath =~ s,/content/,/game/,;
				$txpath =~ s,/materialsrc/,/materials/,;
			}
			PatchVtfFile( "$txpath.vtf" );

			print "done ... ";
			++ $result;
		}
	}

	if ( !$result )
	{
		print "ERROR: not found.\n";
	}
	else
	{
		print "ok.\n";
	}
	return $result;

}

exit 0;


RUN_PATCH_VTF:
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

	print "vtf, p4 ... ";
	`p4 edit "$filename" >nul 2>&1`;
	`p4 lock "$filename" >nul 2>&1`;

	if ( !open( fileptr, "+<$filename" ) )
	{
		print "VTF missing ... ";
		return 1;
	}
	binmode( fileptr );

	seek( fileptr, 5 * 4, 0 );
	read( fileptr, $vtfFlags, 4 );
	$vtfFlags = unpack( "I", $vtfFlags );

	# adding SRGB flag
	$vtfFlags = $vtfFlags | 0x00000040; # mask or

	seek( fileptr, 5 * 4, 0 );
	print fileptr pack( "I", $vtfFlags );

	close( fileptr );

	print "vtf ok ... ";
	return 0;

}


