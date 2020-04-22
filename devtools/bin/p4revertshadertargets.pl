use strict;

BEGIN {use File::Basename; push @INC, dirname($0); }
require "valve_perl_helpers.pl";

my $dynamic_compile = defined $ENV{"dynamic_shaders"} && $ENV{"dynamic_shaders"} != 0;

# ----------------------------------------------
# COMMAND-LINE ARGS
# ----------------------------------------------
my $g_x360			= 0;
my $g_ps3			= 0;
my $g_tmpfolder	= "";
my $g_vcsext		= ".vcs";
my $g_SrcDir = ".";
my $inputbase;
my $g_SourceDir;

while( 1 )
{
	$inputbase = shift;

	if( $inputbase =~ m/-source/ )
	{
		$g_SourceDir = shift;
	}
	elsif( $inputbase =~ m/-x360/ )
	{
		$g_x360 = 1;
		$g_tmpfolder = "_360";
		$g_vcsext = ".360.vcs";
	}
	elsif( $inputbase =~ m/-ps3/ )
	{
		$g_ps3 = 1;
		$g_tmpfolder = "_ps3";
		$g_vcsext = ".ps3.vcs";
	}
	else
	{
		last;
	}
}

# ----------------------------------------------
# Load the list of shaders that we care about.
# ----------------------------------------------
my @srcfiles = &LoadShaderListFile( $inputbase );

my %incHash;
my %vcsHash;
my $shader;
foreach $shader ( @srcfiles )
{
	my $shadertype = &LoadShaderListFile_GetShaderType( $shader );
	my $shaderbase = &LoadShaderListFile_GetShaderBase( $shader );
	my $shadersrc = &LoadShaderListFile_GetShaderSrc( $shader );
	if( $shadertype eq "fxc" || $shadertype eq "vsh" )
	{
		# We only generate inc files for fxc and vsh files.
		my $incFileName = "$shadertype" . "tmp9" . $g_tmpfolder . "/" . $shaderbase . "\.inc";
		$incFileName =~ tr/A-Z/a-z/;
		$incHash{$incFileName} = 1;
	}

	my $vcsFileName = "$shadertype/$shaderbase" . $g_vcsext;
	$vcsFileName =~ tr/A-Z/a-z/;
	$vcsHash{$vcsFileName} = 1;
}

# ----------------------------------------------
# Get the list of inc files to consider for reverting
# ----------------------------------------------
sub RevertIntegratedFiles
{
	my $path = shift;
	my $fileHashRef = shift;

	my $cmd = "p4 fstat $path";
	my @fstat = &RunCommand( $cmd );

	my $depotFile;
	my $action;
	my @openedforintegrate;

	my $line;
	foreach $line ( @fstat )
	{
		if( $line =~ m,depotFile (.*)\n, )
		{
			$depotFile = &NormalizePerforceFilename( $1 );
		}
		elsif( $line =~ m,action (.*)\n, )
		{
			$action = $1;
		}
		elsif( $line =~ m,^\s*$, )
		{
			if( defined $action && defined $fileHashRef->{$depotFile} && $action =~ m/integrate/i )
			{
				push @openedforintegrate, $depotFile;
			}
			undef $depotFile;
			undef $action;
		}
	}

	if( scalar( @openedforintegrate ) )
	{
		my $cmd = "p4 revert @openedforintegrate";
#		print "$cmd\n";
		my @revertOutput = &RunCommand( $cmd );
		&PrintCleanPerforceOutput( @revertOutput );
	}
}

my $path = "vshtmp9" . $g_tmpfolder . "/... fxctmp9" . $g_tmpfolder . "/...";
&RevertIntegratedFiles( $path, \%incHash );

if( !$dynamic_compile )
{
	&MakeDirHier( "../../../game/platform/shaders" );

	# Might be in a different client for the vcs files, so chdir to the correct place.
	chdir "../../../game/platform/shaders" || die;

	my $path = "...";
	&RevertIntegratedFiles( $path, \%vcsHash );
}
