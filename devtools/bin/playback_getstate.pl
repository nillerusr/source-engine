# getfinalstate.pl blah.txt line
# note: the state is *before* the line that you specify
die "Usage: playback_getstate.pl blah.txt cmd\n" if scalar( @ARGV ) != 2;

open INPUT, shift || die;
$desiredcmd = shift;
$line = 0;

sub PadNumber
{
	local( $str ) = shift;
	local( $desiredLen ) = shift;
	local( $len ) = length $str;
	while( $len < $desiredLen )
	{
		$str = "0" . $str;
		$len++;
	}
	return $str;
}


while( <INPUT> )
{
	m/^cmd: (\d+) /;
	$cmdnum = $1;
#	print "$cmdnum\n";
	if( $cmdnum == $desiredcmd )
	{
		print "$_";
		last;
	}
	if( /Dx8SetTextureStageState:\s*stage:\s*(\d+)\s*state:\s*(\S+)\s*,\s*value:\s*(\S+)/ )
	{
		$texturestate{"$2 $1"} = $3;
#		print "$1 $2 $3\n";
	}
#	elsif( /Dx8SetTextureStageState/i )
#	{
#		die "$_";
#	}

	if( /Dx8SetRenderState:\s*(\S+)\s+(\S+.*)*$/ )
	{
		$renderstate{$1} = $2;
	}
#	elsif( /Dx8SetRenderState/i )
#	{
#		die "$_";
#	}

	if( /Dx8SetVertexShaderConstant:\s*reg:\s*(\d+)\s*numvecs:\s*(\d+)\s*val:\s*(.*)$/ )
	{
		local( $reg ) = $1;
		local( $count ) = $2;
		local( $val ) = $3;
		local( $i );
		for( $i = 0; $i < $count; $i++, $reg++ )
		{
			$val =~ s/^\s*(\[\s*\S+\s+\S+\s+\S+\s+\S+\s*\])(.*$)/$2/;
			$vertconst{ "vshconst " . &PadNumber( $reg, 2 ) } = $1;
		}
	}
#	elsif( /Dx8SetVertexShaderConstant/i )
#	{
#		die "$_";
#	}

	if( /Dx8SetPixelShaderConstant:\s*reg:\s*(\d+)\s*numvecs:\s*(\d+)\s*val:\s*(.*)$/ )
	{
		local( $reg ) = $1;
		local( $count ) = $2;
		local( $val ) = $3;
		local( $i );
		for( $i = 0; $i < $count; $i++, $reg++ )
		{
			$val =~ s/^\s*(\[\s*\S+\s+\S+\s+\S+\s+\S+\s*\])(.*$)/$2/;
			$pixelconst{ "pshconst " . &PadNumber( $reg, 2 ) } = $1;
		}
	}
#	elsif( /Dx8SetPixelShaderConstant/i )
#	{
#		die "$_";
#	}

	if( /Dx8SetStreamSource:\s*vertexBufferID:\s*(\d+)\s*streamID:\s*(\d+)\s*vertexSize:\s*(\d+)\s*$/ )
	{
		$streamsrc{$2} = "vertexBufferID: $1 vertexSize: $3";
	}
#	elsif( /Dx8SetStreamSource/i )
#	{
#		die "$_";
#	}

	if( /Dx8CreateVertexShader:\s*id:\s*(\d+)\s+dwDecl:\s*(.*)$/ )
	{
		$vshdecl{$1} = $2;
	}
	elsif( /Dx8CreateVertexShader/i )
	{
		die "$_";
	}

	if( /Dx8SetVertexShader:\s*vertexShader:\s*(\d+)\s*$/ )
	{
		$currentVertexShader = $1;
	}
	elsif( /Dx8SetVertexShader:/i )
	{
		die "$_";
	}

	if( /Dx8SetPixelShader:\s*pixelShader:\s*(\d+)\s*$/ )
	{
		$currentPixelShader = $1;
	}
	elsif( /Dx8SetPixelShader:/i )
	{
		die "$_";
	}

	if( /Dx8SetVertexBufferFormat:\s*id:\s*(\d+)\s*vertexFormat:\s*(.*)$/ )
	{
		$vbformat{$1} = $2;
	}
	elsif( /Dx8SetVertexBufferFormat/i )
	{
		die "$_";
	}
}
close INPUT;

foreach $state ( sort( keys( %texturestate ) ) )
{
	print "$state = $texturestate{$state}\n";
}

foreach $state ( sort( keys( %renderstate ) ) )
{
	print "$state = $renderstate{$state}\n";
}

foreach $state ( sort( keys( %vertconst ) ) )
{
	print "$state = $vertconst{$state}\n";
}

foreach $state ( sort( keys( %pixelconst ) ) )
{
	print "$state = $pixelconst{$state}\n";
}

foreach $state ( sort( keys( %streamsrc ) ) )
{
	$streamsrc{$state} =~ m/vertexBufferID:\s*(\d+)\s+/;
	local( $vertbufid ) = $1;
	print "stream $state = $streamsrc{$state} vbformat: $vbformat{$vertbufid}\n";
}

#foreach $state ( sort( keys( %vbformat ) ) )
#{
#	print "vbformat $state = $vbformat{$state}\n";
#}

print "current vsh: $currentVertexShader vshdecl: $vshdecl{$currentVertexShader}\n";
print "current psh: $currentPixelShader\n";



