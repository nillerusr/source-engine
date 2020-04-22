#!perl

use File::Find;

# customize here
print "Running file size monitor\n";

LogDirectorySize("PC shader size", "../../../game/hl2/shaders","\.vcs","\.360\.vcs");
LogDirectorySize("PC Game Bin DLL size", "../../../game/bin/","\.dll","_360\.dll");
LogDirectorySize("360 shader size", "../../../game/hl2/shaders","\.360\.vcs");
LogDirectorySize("360 Game Bin DLL size", "../../../game/bin/","_360\.dll");
LogDirectorySize("tf texture size","../../../game/tf/materials/","\.vtf");







sub LogDirectorySize
  {
	my ($label, $basedir, $filepattern, $excludepattern ) = @_;
	undef @FileList;
	find(\&ProcessFile, $basedir);
	my $total_size = 0;
	foreach $_ (@FileList)
	  {
		next if ( length($excludepattern) && ( /$excludepattern/i ) );
		if (/$filepattern/i)
		{
		  $total_size += (-s $_ );
		}
	  }
	print "$label := $total_size\n";
  }

sub ProcessFile
  {
	push @FileList, $File::Find::name;
  }
