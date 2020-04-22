#!perl

open(DLL,"../../../game/bin/shaderapidx9.dll" ) || die "can't open shaderapi";

binmode DLL;
my $dllcode = do { local( $/ ) ; <DLL> } ; # slurp comparison output in
close DLL;

if ( $dllcode =~ /dynamic_shader_compile_is_on/s )
  {
	open(ERRORS,">errors.txt") || die "huh - can't write";
	print ERRORS "stdshader_dx9.dll was built with dynamic shader compile!\n";
	close ERRORS;
  }

