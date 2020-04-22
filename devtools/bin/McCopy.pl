use strict;

# todo: make sure and create the target directory if it doesn't exist.
# todo: make this work either direction
# todo: fix any case problems

my $g_protocol_version = "mccopy1\n";
my $g_opt_port = 7070;
my $g_server_md5 = 0;

use IO::Socket;

sub GetMD5
{
	my $filename = shift;
	$filename =~ s,/,\\,g;
#	print "$filename\n";
	print ".";
	my( $cmd ) = "md5.exe < \"$filename\"";
#	print $cmd . "\n";
	open MD5, "$cmd|";
	my $out = <MD5>;
	close MD5;
	$out =~ s/^.*\=\s+//;
	$out =~ s/\n//g;
#	print "'" . $out . "'" . "\n";
	return $out;	
}

################################################################################################
# SERVER CODE
################################################################################################
sub SendFileName
{
	my( $sock ) = shift;
	my( $file ) = shift;
	my( $isdir ) = -d $file;

	if( $isdir )
	{
		if( $file =~ m/\/\.$/ ||		# "."
			$file =~ m/\/\.\.$/ )	# ".."
		{
			return;
		}
	}

	my( @statinfo ) = stat $file;
	if( @statinfo )
	{
		my( $mtime ) = $statinfo[9];
		my( $mode ) = $statinfo[2];
#		my( $mtimestr ) = scalar( localtime( $mtime ) );
		my( $size ) = $statinfo[7];
		if( $isdir )
		{
			print $sock "d\n";
		}
		else
		{
			print $sock "f\n";
		}
		print $sock &RemoveBaseDir( $file ) . "\n";
		print $sock $mtime . "\n";
		printf $sock "%o\n", $mode;
		print $sock $size . "\n";
		if( !$isdir && $g_server_md5 )
		{
			print $sock &GetMD5( $file ) . "\n";
		}
#		print $file . "\n";
#		print $mtime . "\n";
#		print $mode . "\n";
#		print $md5 . "\n";
	}
	else
	{
		print "CAN'T STAT $file\n";
	}

	if( $isdir )
	{
		SendDirName( $sock, $file );
	}
}


sub SendDirName
{
	my( $sock ) = shift;
	my( $dirname ) = shift;
	if( $dirname =~ m/\/\.$/ ||		# "."
	    $dirname =~ m/\/\.\.$/ )	# ".."
	{
		return;
	}

	local( *SRCDIR );
	opendir SRCDIR, $dirname;
	my( @dir ) = readdir SRCDIR;
	closedir SRCDIR;

	my( $item );
	while( $item = shift @dir )
	{
		&SendFileName( $sock, $dirname . "/" . $item );
	}
}

sub GetFile
{
	my( $sock ) = shift;
	my( $filename ) = shift;
	my( $localfilename ) = &AddBaseDir( $filename );
	print "GetFile: $filename ($localfilename)\n";
	local( *FILE );
	open FILE, "<$localfilename";
	binmode( FILE );
	my( $filebits );
	seek FILE, 0, 2;
	my( $size ) = tell FILE;
	if( $size < 0 )
	{
		die "$filename \$size == $size\n";
	}
	seek FILE, 0, 0;
	read FILE, $filebits, $size;
	close FILE;
#	print "sending $filename: $size\n";
	print $sock $filebits;
#	print "finished sending $filename\n";
}

sub HandleCommand
{
	my( $sock ) = shift;
	my( $cmd ) = shift;

	if( $cmd =~ m/dirlistmd5\s+(.*)$/ )
	{
		$g_server_md5 = 1;
		&SetBaseDir( $1 );
		&SendDirName( $sock, $1 );
		print $sock "\n"; # terminating newline to end reply
	}
	elsif( $cmd =~ m/dirlist\s+(.*)$/ )
	{
		$g_server_md5 = 0;
		&SetBaseDir( $1 );
		&SendDirName( $sock, $1 );
		print $sock "\n"; # terminating newline to end reply
	}
	elsif( $cmd =~ m/getfile\s+(.*)$/ )
	{
		&GetFile( $sock, $1 );
	}
}

sub RunServer
{
	my( $hostname ) = `hostname`;
#	my( $hostname ) = "localhost";
	$hostname =~ s/\n//; # remove newlines

	my $sock = new IO::Socket::INET (
									  LocalHost => $hostname,
									  LocalPort => $g_opt_port,
									  Proto => 'tcp',
									  Listen => 1,
									  Reuse => 1,
									 );
	die "Could not create socket: $!\n" unless $sock;

	my( $clientnum ) = 0;
	while( 1 )
	{
		my $new_sock = $sock->accept();
		print "accept!\n";
		if( fork() == 0 )
		{
			print "$clientnum: opening connection...\n";
			my $version = <$new_sock>;
			if( $version ne $g_protocol_version )
			{
				die "wrong protocol version: server: $g_protocol_version client: %version\n";
			}
			my $command;
			while(defined($command = <$new_sock>)) 
			{
				print "$clientnum: command: $command";
			   	&HandleCommand( $new_sock, $command );
			   	print "$clientnum: done with $command";
			}
			print "$clientnum: closing connection...\n";
			close( $new_sock );
			exit;
		}
		$clientnum++;
	}

	close($sock); # never get here.
}

################################################################################################
# CLIENT CODE
################################################################################################


# all options that we might care about from rsync:
# -v, --verbose               increase verbosity
# -q, --quiet                 decrease verbosity
# -c, --checksum              always checksum
# -a, --archive               archive mode
# -r, --recursive             recurse into directories
# -R, --relative              use relative path names
# -b, --backup                make backups (default ~ suffix)
#     --backup-dir=DIR        put backups in the specified directory
#     --suffix=SUFFIX         override backup suffix
# -u, --update                update only (don't overwrite newer files)
# -p, --perms                 preserve permissions
# -t, --times                 preserve times
# -n, --dry-run               show what would have been transferred
#     --existing              only update files that already exist
#     --delete                delete files that don't exist on the sending side
#     --delete-excluded       also delete excluded files on the receiving side
#     --delete-after          delete after transferring, not before
#     --max-delete=NUM        don't delete more than NUM files
#     --force                 force deletion of directories even if not empty
#     --timeout=TIME          set IO timeout in seconds
# -I, --ignore-times          don't exclude files that match length and time
#     --size-only             only use file size when determining if a file should be transferred
# -T  --temp-dir=DIR          create temporary files in directory DIR
#     --compare-dest=DIR      also compare destination files relative to DIR
# -P                          equivalent to --partial --progress
# -z, --compress              compress file data
#     --exclude=PATTERN       exclude files matching PATTERN
#     --exclude-from=FILE     exclude patterns listed in FILE
#     --include=PATTERN       don't exclude files matching PATTERN
#     --include-from=FILE     don't exclude patterns listed in FILE
#     --version               print version number
#     --daemon                run as a rsync daemon
#     --address               bind to the specified address
#     --stats                 give some file transfer stats
#     --progress              show progress during transfer
# -h, --help                  show this help screen

# options that are actually implemented:
#
sub Usage
{
	print "\n";
	print "Usage:\n";
	print "perl McCopy.pl --server\n";
	print "or\n";
	print "perl McCopy.pl [options] srcdir dstdir\n";
	print "\n";
	print "ie:\n";
	print "perl McCopy.pl --verbose --recursive remotemachine:u:/hl u:/hl.work\n";
	print "\n";
	print "where \"remotemachine\" is running a McCopy server\n";
	print "\n";
	print "options are:\n";
	print "--server              run as server (ignores other options)\n";
	print "--port N (default 7070)\n";
	print "--verbose\n";
	print "--test                don't actually copy or delete any files\n";
	print "--ignore-time         don't use file mtimes as a criterion for file that need\n";
	print "                      to be copied.\n";
	print "--ignore-permissions  don't use file permissions as a criterion for file that\n";
	print "                      need to be copied.\n";
	print "--ignore-size         don't use file size as a criterion for file that need\n";
	print "                      to be copied.\n";
	print "--md5         	     Use md5 checksums\n";
	print "--recursive\n";
	print "--delete-readonly     delete readonly files in dst\n";
	print "                      that don't exist in src\n";
	print "--delete-writable     delete writable files in dst\n";
	print "--delete-dirs         delete directories in dst that don't exist in src\n";
	print "--clobber-writable-newer\n";
	print "                      write over upon copy files that have been modified locally\n";
	print "--delete-excluded     delete files on dst that are excluded using --exclude\n";
	print "--exclude             exclude files containing perl regular expression, ie:\n";
	print "                      --exclude /.*release.*/i\n";
	print "--include             override --exclude for files containing perl regular expression, ie:\n";
	print "                      --include /.*debughlp.*/i\n";
	print "--mirror              same as --recursive --delete-readonly --delete-writable\n";
	print "                      --clobber-writable-newer --delete-excluded --delete-dirs\n";
	print "--mirror-safe         same as --recursive --delete-readonly --delete-dirs\n";
	exit;
}

# default command line options
my $g_opt_server = 0;
my $g_opt_test = 0;
my $g_opt_verbose = 0;
my $g_opt_recursive = 0;
my $g_opt_deletereadonly = 0;
my $g_opt_deletewritable = 0;
my $g_opt_clobberwritablenewer = 0;
my $g_opt_deleteexcluded = 0;
my $g_opt_deletedirs = 0;
my $g_opt_ignoretime = 0;
my $g_opt_ignoreperms = 0;
my $g_opt_ignoresize = 0;
my @g_opt_exclude;
my @g_opt_include;
my $g_opt_src;
my $g_opt_dst;
my $g_opt_md5;

my $g_src_is_local;
my $g_src_machine = "";
my $g_src_path;

my $g_dst_is_local;
my $g_dst_machine = "";
my $g_dst_path;

# indexed by $filename
my %g_remote_mtime;
my %g_remote_mode;
my %g_remote_isdir;
my %g_remote_size;
my %g_remote_md5;

my %g_local_mtime;
my %g_local_mode;
my %g_local_isdir;
my %g_local_size;
my %g_local_md5;

my %g_alreadycomparedtime;
my %g_alreadycomparedpermissions;
my %g_alreadycomparedsize;
my %g_alreadycomparedmd5;

my %g_files_to_delete;
my %g_dirs_to_delete;
my %g_files_to_copy;
my %g_dirs_to_create;

my $g_max_time_delta = 2; # in seconds

my $g_basedir;

sub BackToForwardSlash
{
	my( $path ) = shift;
	$path =~ s,\\,/,g;
	return $path;
}

sub ForwardToBackSlash
{
	my( $path ) = shift;
	$path =~ s,/,\\,g;
	return $path;
}

sub SetBaseDir
{
	$g_basedir = shift;
#	print "\$g_basedir: $g_basedir\n";
}

sub RemoveFileName
{
	my( $in ) = shift;
	$in = &BackToForwardSlash( $in );
	$in =~ s,/[^/]*$,,;
	return $in;
}

sub RemovePath
{
	my( $in ) = shift;
	$in = &BackToForwardSlash( $in );
	$in =~ s,^(.*)/([^/]*)$,$2,;
	return $in;
}


sub MakeDirHier
{
	my( $in ) = shift;
#	print "MakeDirHier( $in )\n";
	$in = &BackToForwardSlash( $in );
	my( @path );
	while( $in =~ m,/, ) # while $in still has a slash
	{
		my( $end ) = &RemovePath( $in );
		push @path, $end;
#		print $in . "\n";
		$in = &RemoveFileName( $in );
	}
	my( $i );
	my( $numelems ) = scalar( @path );
	my( $curpath );
	for( $i = $numelems - 1; $i >= 0; $i-- )
	{
		$curpath .= "/" . $path[$i];
		my( $dir ) = $in . $curpath;
		if( !stat $dir )
		{
			print "mkdir $dir\n";
			mkdir $dir, 0777;
		}
	}
}

sub RemoveBaseDir
{
	my( $path ) = shift;
#	print "removebasedir: $path ";
	$path =~ s,^$g_basedir/,,;
#	print "$path\n";
	return $path;
}

sub AddBaseDir
{
	my( $path ) = shift;
	return $g_basedir . "/" . $path;
}

sub FixPath
{
	my( $path ) = shift;
	# backslash to forward slash
	$path =~ s,\\,/,g;  
	# remove trailing slash
	$path =~ s,/$,,;	
	return $path;
}

sub ProcessLongCommand
{
	my( $cmd ) = shift;
	if( $cmd =~ m/--verbose/ )
	{
		$g_opt_verbose = 1;
	}
	elsif( $cmd =~ m/--server/ )
	{
		$g_opt_server = 1;
	}
	elsif( $cmd =~ m/--test/ )
	{
		$g_opt_test = 1;
	}
	elsif( $cmd =~ m/--ignore-time/ )
	{
		$g_opt_ignoretime = 1;
	}
	elsif( $cmd =~ m/--ignore-permissions/ )
	{
		$g_opt_ignoreperms = 1;
	}
	elsif( $cmd =~ m/--ignore-size/ )
	{
		$g_opt_ignoresize = 1;
	}
	elsif( $cmd =~ m/--md5/ )
	{
		$g_opt_md5 = 1;
	}
	elsif( $cmd =~ m/--recursive/ )
	{
		$g_opt_recursive = 1;
	}
	elsif( $cmd =~ m/--mirror-safe/ )
	{
		$g_opt_recursive = 1;
		$g_opt_deletereadonly = 1;
		$g_opt_deletewritable = 0;
		$g_opt_clobberwritablenewer = 0;
		$g_opt_deleteexcluded = 0;
		$g_opt_deletedirs = 1;
	}
	elsif( $cmd =~ m/--mirror/ )
	{
		$g_opt_recursive = 1;
		$g_opt_deletereadonly = 1;
		$g_opt_deletewritable = 1;
		$g_opt_clobberwritablenewer = 1;
		$g_opt_deleteexcluded = 1;
		$g_opt_deletedirs = 1;
	}
	elsif( $cmd =~ m/--delete-readonly/ )
	{
		$g_opt_deletereadonly = 1;
	}
	elsif( $cmd =~ m/--delete-writable/ )
	{
		$g_opt_deletewritable = 1;
	}
	elsif( $cmd =~ m/--clobber-writable-newer/ )
	{
		$g_opt_clobberwritablenewer = 1;
	}
	elsif( $cmd =~ m/--delete-excluded/ )
	{
		$g_opt_deleteexcluded = 1;
	}
	elsif( $cmd =~ m/--delete-dirs/ )
	{
		$g_opt_deletedirs = 1;
	}
}

sub ProcessCommandLine
{
	my( $cmd );
	while( $cmd = shift )
	{
		# hack - special case for exclude since it has an argument
		if( $cmd =~ m/^--exclude/ )
		{
			push @g_opt_exclude, shift;
		}
		elsif( $cmd =~ m/^--include/ )
		{
			push @g_opt_include, shift;
		}
		elsif( $cmd =~ m/^--port/ )
		{
			$g_opt_port = shift;
		}
		elsif( $cmd =~ m/^--/ )
		{
			&ProcessLongCommand( $cmd );
		}
		elsif( $cmd =~ m/^-/ )
		{
			print "short command $cmd\n";
		}
		else
		{
			if( !defined( $g_opt_src ) )
			{
				$g_opt_src = &FixPath( $cmd );
			}
			elsif( !defined( $g_opt_dst ) )
			{
				$g_opt_dst = &FixPath( $cmd );
			}
			else
			{
				print "Don't understand $cmd\n";
				&Usage();
			}
		}
	}
}

sub PrintOptions
{
	if( $g_opt_verbose )
	{
		print "\n";
		print "Options:\n";
		print "\$g_opt_src = $g_opt_src\n";
		print "\$g_opt_dst = $g_opt_dst\n";
		print "\$g_opt_test = $g_opt_test\n";
		print "\$g_opt_ignoretime = $g_opt_ignoretime\n";
		print "\$g_opt_ignoreperms = $g_opt_ignoreperms\n";
		print "\$g_opt_ignoresize = $g_opt_ignoresize\n";
		print "\$g_opt_verbose = $g_opt_verbose\n";
		print "\$g_opt_recursive = $g_opt_recursive\n";
		print "\$g_opt_deletereadonly = $g_opt_deletereadonly\n";
		print "\$g_opt_deletewritable = $g_opt_deletewritable\n";
		print "\$g_opt_clobberwritablenewer = $g_opt_clobberwritablenewer\n";
		print "\$g_opt_deleteexcluded = $g_opt_deleteexcluded\n";
		print "\@g_opt_exclude = @g_opt_exclude\n";
		print "\n";
	}
}

sub ValidateOptions
{
	if( $g_opt_server )
	{
		return;
	}
	if( !defined( $g_opt_src ) )
	{
		print "src not defined\n";
		Usage();
	}
	if( !defined( $g_opt_dst ) )
	{
		print "dst not defined\n";
		Usage();
	}
	if( !$g_opt_recursive )
	{
		print "--recursive must be used. . non-recursive operation not supported\n";
		Usage();
	}
}

# src/dst looks like:
# gary:u:/hl2/hl2
# u:/hl2/hl2
# /hl2/hl2

sub ParseSrcDstPaths
{
#	print $g_opt_src . "\n";
	if( $g_opt_src =~ m/(\S+)\:(\S\:\S*)/ )
	{
		$g_src_is_local = 0;
		$g_src_machine = $1;
		$g_src_path = $2;
	}
	elsif( $g_opt_src =~ m/(\S+)\:(\/\/.*)/ )
	{
		# //gary://maxwell/common/
		$g_src_is_local = 0;
		$g_src_machine = $1;
		$g_src_path = $2;
	}
	elsif( $g_opt_src =~ m/^(\S:.*)/ )
	{
		$g_src_is_local = 1;
		$g_src_path = $1;
	}
	else
	{
		$g_src_is_local = 1;
		$g_src_path = $1;
	}

	if( $g_opt_dst =~ m/(\S+):(\S:\S+)/ )
	{
		$g_dst_is_local = 0;
		$g_dst_machine = $1;
		$g_dst_path = $2;
	}
	elsif( $g_opt_dst =~ m/^(\S:\S+)/ )
	{
		$g_dst_is_local = 1;
		$g_dst_path = $1;
	}
	else
	{
		$g_dst_is_local = 1;
		$g_dst_path = $1;
	}

	if( $g_src_is_local )
	{
		die "my src directories not supported yet. . run the server on the other end.\n";
	}
	if( !$g_dst_is_local )
	{
		die "remote dst directories not supported yet. . run the server on the other end.\n";
	}
	if( $g_src_is_local == $g_dst_is_local )
	{
		die "src and dst on the same machine not supported. . use robocopy\n";
	}

	&MakeDirHier( $g_dst_path );

	&SetBaseDir( $g_dst_path );

	if( $g_opt_verbose )
	{
		print "\n\$g_src_is_local = $g_src_is_local\n";
		print "\$g_src_machine = $g_src_machine\n";
		print "\$g_src_path = $g_src_path\n";
		print "\$g_dst_is_local = $g_dst_is_local\n";
		print "\$g_dst_machine = $g_dst_machine\n";
		print "\$g_dst_path = $g_dst_path\n\n";
	}
}

&ProcessCommandLine( @ARGV );
&ValidateOptions();
&PrintOptions();

if( $g_opt_server )
{
	&RunServer();
	exit;
}

&ParseSrcDstPaths();

sub GetRemoteDirList
{
	my( $sock ) = shift;
	my( $remotedirname ) = shift;
	if( $g_opt_md5 )
	{
		print $sock "dirlistmd5 $remotedirname\n";
	}
	else
	{
		print $sock "dirlist $remotedirname\n";
	}
	my( $filename, $mtime, $mode, $size );
	my $fileordir;
#	while( defined( $fileordir = <$sock> ) )
	while( 1 )
	{
		$fileordir = <$sock>;
		die "Lost connection!!!" if !defined( $fileordir );
		last if $fileordir=~ m/^\n$/;
		
		$filename = <$sock>;
		$mtime = <$sock>;
		$mode = <$sock>;
		$size = <$sock>;
		my $md5 = "";
		if( ( $fileordir =~ /f/i ) && $g_opt_md5 )
		{
			$md5 = <$sock>;
		}
		if( 0 )
		{
			print "file: $filename";
			print "fileordir: $fileordir";
			print "mtime: $mtime";
			print "mode: $mode";
			print "size: $size\n";
		}
		$filename =~ s/\n//;
		$mtime =~ s/\n//;
		$mode =~ s/\n//;
		$size =~ s/\n//;
		$md5 =~ s/\n//;
		if( $fileordir =~ m/d/ )
		{
#			print $filename . " is a dir\n";
			$g_remote_isdir{$filename} = 1;	
		}
		else
		{
			$g_remote_isdir{$filename} = 0;	
		}
		$g_remote_mtime{$filename} = $mtime;
#		print $g_remote_mtime{$filename};
		$g_remote_mode{$filename} = $mode;
		$g_remote_size{$filename} = $size;
		if( $g_opt_md5 )
		{
			$g_remote_md5{$filename} = $md5;
		}
	}
}

sub GetLocalDirList_File
{
	my( $filename ) = shift;
	my( $isdir ) = -d $filename;
	if( $isdir )
	{
		if( $filename =~ m/\/\.$/ ||		# "."
			$filename =~ m/\/\.\.$/ )	# ".."
		{
			return;
		}
	}

	if( $isdir )
	{
		GetLocalDirList_Dir( $filename );
	}
	my( @statinfo ) = stat $filename;
	if( @statinfo )
	{
		my( $mtime ) = $statinfo[9];
		my( $mode ) = $statinfo[2];
#			my( $mtimestr ) = scalar( localtime( $mtime ) );
		my( $size ) = $statinfo[7];
		my( $filename_nobase ) = &RemoveBaseDir( $filename );
		$g_local_isdir{$filename_nobase} = $isdir;
		$g_local_mtime{$filename_nobase} = $mtime;
		$g_local_mode{$filename_nobase} = sprintf "%o", $mode;
		$g_local_size{$filename_nobase} = $size;
		if( !$isdir && $g_opt_md5 )	
		{
			$g_local_md5{$filename_nobase} = &GetMD5( $filename );
		}
	}
	else
	{
		print "CAN'T STAT $filename\n";
	}
}

sub GetLocalDirList_Dir
{
	my( $dirname ) = shift;
	if( $dirname =~ m/\/\.$/ ||		# "."
		$dirname =~ m/\/\.\.$/ )	# ".."
	{
		return;
	}

	local( *SRCDIR );
	opendir SRCDIR, $dirname;
	my( @dir ) = readdir SRCDIR;
	closedir SRCDIR;

	my( $item );
	while( $item = shift @dir )
	{
		&GetLocalDirList_File( $dirname . "/" . $item );
	}
}

sub IsExcluded
{
	my( $filename ) = shift;
	my( $regexp );
	foreach $regexp ( @g_opt_include )
	{
		if( eval "\$filename =~ $regexp" )
		{
			return 0;
		}
	}
	foreach $regexp ( @g_opt_exclude )
	{
		if( eval "\$filename =~ $regexp" )
		{
			if( defined( $g_local_mtime{$filename} ) )
			{
				if( $g_opt_deleteexcluded )
				{
					print "excluding and deleting $filename\n" if( $g_opt_verbose );
					if( defined $g_remote_isdir{$filename} )
					{
						if( $g_remote_isdir{$filename} )
						{
							$g_dirs_to_delete{$filename} = 1;
						}
						else
						{
							$g_files_to_delete{$filename} = 1;
						}
					}
					elsif( defined $g_local_isdir{$filename} )
					{
						if( $g_local_isdir{$filename} )
						{
							$g_dirs_to_delete{$filename} = 1;
						}
						else
						{
							$g_files_to_delete{$filename} = 1;
						}
					}
					else
					{
						die;
					}
				}
				else
				{
					print "excluding but keeping my copy of $filename\n" if( $g_opt_verbose );
				}
			}
			else
			{
				print "excluding $filename, which doesn't exist locally\n" if( $g_opt_verbose );
			}
			return 1;
		}
	}
	return 0;
}

sub CompareTime
{
	if( $g_opt_ignoretime )
	{
		return;
	}
	my( $filename ) = shift;
	# hack! ignore directories. . seems like robocopy does too.
	if( $g_remote_isdir{$filename} || $g_local_isdir{$filename} )
	{
		return;
	}

	if( defined( $g_alreadycomparedtime{$filename} ) )
	{
		return;
	}
	$g_alreadycomparedtime{$filename} = 1;

	# compare times
	my( $deltatime ) = $g_local_mtime{$filename} - $g_remote_mtime{$filename};
#	print "g_remote_mtime: " . $g_remote_mtime{$filename} . "\n";
#	print "g_local_mtime: " . $g_local_mtime{$filename} . "\n";
#	print "deltatime: $deltatime\n";
	if( !( ( $deltatime >= -$g_max_time_delta && $deltatime <= $g_max_time_delta ) ||
               ( $deltatime + 3600 >= -$g_max_time_delta && $deltatime + 3600 <= $g_max_time_delta ) ||
	       ( $deltatime - 3600 >= -$g_max_time_delta && $deltatime - 3600 <= $g_max_time_delta ) ) )
	{
		$g_files_to_copy{$filename} = 1;
		if( $g_opt_verbose )
		{
			print "timedelta of $deltatime for $filename\n";
		}
	}
}

sub ComparePermissions
{
	if( $g_opt_ignoreperms )
	{
		return;
	}
	my( $filename ) = shift;
	if( defined( $g_alreadycomparedpermissions{$filename} ) )
	{
		return;
	}
	$g_alreadycomparedpermissions{$filename} = 1;

	# compare permissions
	if( $g_remote_mode{$filename} != $g_local_mode{$filename} )
	{
		if( !$g_remote_isdir{$filename} )
		{
			$g_files_to_copy{$filename} = 1;
		}
		else
		{
			MakeLocalFileAttribsMatchRemote( $filename );
		}
		if( $g_opt_verbose )
		{
			printf "permissions different for $filename: %s %s\n", $g_remote_mode{$filename}, $g_local_mode{$filename};
		}
	}
}

sub CompareSize
{
	if( $g_opt_ignoresize )
	{
		return;
	}
	my( $filename ) = shift;
	if( defined( $g_alreadycomparedsize{$filename} ) )
	{
		return;
	}
	$g_alreadycomparedsize{$filename} = 1;

	# compare permissions
	if( $g_remote_size{$filename} != $g_local_size{$filename} )
	{
		$g_files_to_copy{$filename} = 1;
		if( $g_opt_verbose )
		{
			printf "size different for $filename: %d!=%d\n", $g_remote_size{$filename}, $g_local_size{$filename};
		}
	}
}

sub CompareMD5
{
	if( !$g_opt_md5 )
	{
		return;
	}
	my( $filename ) = shift;
	if( defined( $g_alreadycomparedmd5{$filename} ) )
	{
		return;
	}
	$g_alreadycomparedmd5{$filename} = 1;

	# compare md5
	if( $g_remote_md5{$filename} ne $g_local_md5{$filename} )
	{
		$g_files_to_copy{$filename} = 1;
		if( $g_opt_verbose )
		{
			printf "md5 different for $filename: %s!=%s\n", $g_remote_md5{$filename}, $g_local_md5{$filename};
		}
	}
	else
	{
		my( $diff ) = 0;
		my( $deltatime ) = $g_local_mtime{$filename} - $g_remote_mtime{$filename};
		if( $g_remote_size{$filename} != $g_local_size{$filename} )
		{
			$diff = 1;
			print "size different\n";
		}
		if( $g_remote_mode{$filename} != $g_local_mode{$filename} )
		{
			$diff = 1;
			print "mode different\n";
		}
		if( !( ( $deltatime >= -$g_max_time_delta && $deltatime <= $g_max_time_delta ) ||
                       ( $deltatime + 3600 >= -$g_max_time_delta && $deltatime + 3600 <= $g_max_time_delta ) ||
	               ( $deltatime - 3600 >= -$g_max_time_delta && $deltatime - 3600 <= $g_max_time_delta ) ) )
		{
			$diff = 1;
			print "time different\n";
		}
		if( $diff )
		{
			print "fixing up file attribs for $filename\n";
			MakeLocalFileAttribsMatchRemote( $filename );
		}
	}
}

#print "socket: PeerAddr: \"$g_src_machine\"\n";
my $sock = new IO::Socket::INET (
                                  PeerAddr => $g_src_machine,
                                  PeerPort => $g_opt_port,
                                  Proto => 'tcp',
                                 );
die "Could not create socket: $!\n" unless $sock;

print $sock $g_protocol_version;
my $remotedirname = $g_src_path;
print "Getting remote dirlist for $remotedirname\n";
&GetRemoteDirList( $sock, $remotedirname );

my $localdirname = $g_dst_path;
print "Getting local dirlist for $localdirname\n";
&GetLocalDirList_Dir( $localdirname );

my $remote_filename;
foreach $remote_filename (keys %g_remote_mtime)
{
	next if( &IsExcluded( $remote_filename ) );
	if( !defined( $g_local_mtime{$remote_filename} ) )
	{
		if( $g_remote_isdir{$remote_filename} )
		{
#			print "dir ";
			$g_dirs_to_create{$remote_filename} = 1;
		}
		else
		{
#			print "file ";
			$g_files_to_copy{$remote_filename} = 1;
		}
		if( $g_opt_verbose )
		{
			print "doesn't exist locally and will be copied: $remote_filename\n";
		}
	}
	else
	{
		&CompareTime( $remote_filename );
		&ComparePermissions( $remote_filename );
		&CompareSize( $remote_filename );
		&CompareMD5( $remote_filename );
	}
}

my $local_filename;
foreach $local_filename (keys %g_local_mtime)
{
	next if( &IsExcluded( $local_filename ) );
	if( !defined( $g_remote_mtime{$local_filename} ) )
	{
		if( $g_local_isdir{$local_filename} )
		{
			$g_dirs_to_delete{$local_filename} = 1;
#			print "dir ";
		}
		else
		{
			$g_files_to_delete{$local_filename} = 1;
#			print "file ";
		}
#		print $local_filename . " doesn't exist remotely\n";
	}
	else
	{
		&CompareTime( $local_filename );
		&ComparePermissions( $local_filename );
		&CompareSize( $local_filename );
	}
}

#%g_files_to_delete;
#%g_dirs_to_delete;
#%g_files_to_copy;
#%g_dirs_to_create;

sub IsWritable
{
	my( $file ) = shift;
	my( $perms ) = oct( $g_local_mode{$file} );
	if( $perms & 2 )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

sub UnlinkFile
{
	my( $file ) = shift;
	
	print "del " . &ForwardToBackSlash( $file ) . "\n";
	if( !$g_opt_test )
	{
		chmod 0666, $file || die;
		unlink $file || die;
	}
}

sub UnlinkDir
{
	my( $file ) = shift;
	
	print "rmdir " . &ForwardToBackSlash( $file ) . "\n";
	if( !$g_opt_test )
	{
		chmod 0777, $file || die;
		if( !( rmdir $file ) )
		{
			print "Couldn't rmdir " . &ForwardToBackSlash( $file ) . "\n";
		}
	}
}

sub DeleteOrphanFile
{
	my( $file ) = shift;
#	print "\$g_local_isdir{$file} = $g_local_isdir{$file}\n";
	#	print "DeleteOrphanFile( $file )\n";
	my( $localfile ) = &AddBaseDir( $file );
	my( $iswritable ) = &IsWritable( $file );
	if( $iswritable )
	{
		if( $g_opt_deletewritable )
		{
			&UnlinkFile( &AddBaseDir( $file ) );
		}
		else
		{
			print "Would have deleted \"$file\" (use --delete-writable to delete)\n";
		}
	}
	else
	{
		if( $g_opt_deletereadonly )
		{
			&UnlinkFile( &AddBaseDir( $file ) );
		}
		else
		{
			print "Would have deleted \"$file\" (use --delete-readonly to delete)\n";
		}
	}
}

sub DeleteOrphanDir
{
	my $dir = shift;
	my( $localdir ) = &AddBaseDir( $dir );
	if( $g_opt_deletedirs )
	{
		&UnlinkDir( $localdir );
	}
	else
	{
		print "Would have deleted \"$dir\" (use --delete-dirs to delete)\n";
	}
}

sub MakeLocalFileAttribsMatchRemote
{
	return if( $g_opt_test );
	my( $file ) = shift;
	my( $localfilename ) = &AddBaseDir( $file );

	# Make it writable so that we can tweak permissions/time.
	chmod 0666, $localfilename || die $!;

	# Set the time on the file to match the remote
	my( @statresult );
	if( !( @statresult = stat $localfilename ) )
	{
		die "couldn't stat $localfilename locally\n";
	}
	# @statresult[8] == access time. . we don't care to change this.
	utime $statresult[8], $g_remote_mtime{$file}, $localfilename || die $!;

	# Set the permissions on the file to match the remote
	chmod oct( $g_remote_mode{$file} ), $localfilename || die $!;
}

sub CopyFileFromRemote
{
	my( $file ) = shift;
	if( &IsWritable( $file ) && !$g_opt_clobberwritablenewer && $g_local_mtime{$file} > $g_remote_mtime{$file} )
	{
		print "Would have copied \"$file\" (use --clobber-writable-newer to copy)\n";
		return;
	}
	print "copy $file from remote ($g_remote_size{$file} bytes)\n";
	if( !$g_opt_test )
	{
		my( $localfilename ) = &AddBaseDir( $file );
		# make the my version writable so that we can write over it, if it exists
		if( $g_local_mtime{$file} )
		{
			chmod 0666, $localfilename || die $!;
		}
		print $sock "getfile $file\n";
		my( $size ) = $g_remote_size{$file};
		my( $filebits );
#		print "reading $file: $size\n";
		my( $readsize ) = read $sock, $filebits, $size;
		if( $readsize != $size )
		{
			die "read size ($readsize) != expected size ($size) for $file\nYou either:\n\t1) lost your connection\n\t2) the remote file has changed since start of mccopy\n";
		}

#		print "finished with $file\n";
		local( *FILE );
#		print "opening $localfilename\n";
		open FILE, ">$localfilename" || die $!;
#		print "binmode $localfilename";
		binmode( FILE ) || die $!;
#		print "after binmode $localfilename";
		print FILE $filebits;
		close FILE || die $!;
#		print "closed $localfilename\n";

		MakeLocalFileAttribsMatchRemote( $file );
	}
}

sub PrettifyTime
{
	my( $inseconds ) = shift;
	my( $hours, $minutes, $seconds );
	my( @blah ) = gmtime( $inseconds );
	$hours = $blah[2];
	$minutes = $blah[1];
	$seconds = $blah[0];
	return sprintf "%02d:%02d:%02d", $hours, $minutes, $seconds;
}

# remove my files that aren't on remote
my $file;
foreach $file (keys %g_files_to_delete)
{
	&DeleteOrphanFile( $file );
}

# remove my dirs that aren't on remote
my $dir;
foreach $dir (sort { length $b <=> length $a } keys( %g_dirs_to_delete ) )
{
	&DeleteOrphanDir( $dir );
}


# create my dirs that are only on remote
foreach $dir (sort { length $a <=> length $b } keys( %g_dirs_to_create) )
{
	my( $localdir ) = &AddBaseDir( $dir );
	print "mkdir $localdir, 0777\n";
	if( !$g_opt_test )
	{
		mkdir $localdir, 0777 || die $!;
	}
}

# calculate the total size of files to transfer
my $totalbytes = 0;
foreach $file ( keys %g_files_to_copy )
{
	$totalbytes += $g_remote_size{$file};	
}

print "$totalbytes bytes to copy\n";

# copy files from remote that are different
my $bytescopied = 0;
my $starttime = time;
foreach $file (sort( keys %g_files_to_copy ) )
{
	&CopyFileFromRemote( $file );
	$bytescopied += $g_remote_size{$file};
	my $curtime = time;
	my $deltatime = $curtime - $starttime;
	my $percentdone;
	if( $totalbytes )
	{
		$percentdone = ( $bytescopied * 1.0 ) / $totalbytes;
	}
	if( $totalbytes && $percentdone && $deltatime )
	{
		printf "progress: %.1f%% %s/%s %d bytes/sec\n",
				$percentdone * 100,
				&PrettifyTime( $deltatime ),
				&PrettifyTime( 1.0 / $percentdone * $deltatime ),
				$bytescopied / $deltatime;
	}
}

print "done!\n";

close($sock);


