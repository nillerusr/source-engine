#!perl

use File::Find;
use Win32::API;

find(\&ProcessFile, "../../../game/bin/" );

sub ProcessFile
  {
	return if (/360/);
	return unless( /\.dll$/i );
	my $LoadLibrary = Win32::API->new( "kernel32", "LoadLibrary","P","L" );
	my $GetProcAddress = Win32::API->new( "kernel32", "GetProcAddress","LP","L" );
	my $FreeLibrary = Win32::API->new( "kernel32", "FreeLibrary", "P", "V" );
	my $handle=$LoadLibrary->Call($_);
	if ( $handle )
	  {
		my $proc = $GetProcAddress->Call($handle, "BuiltDebug\0");
		if ( $proc )
		  {
			print "Error $_ is built debug\n";
		  }
		$FreeLibrary->Call( $handle );
	  }
  }
