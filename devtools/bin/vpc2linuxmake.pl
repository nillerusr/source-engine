#!perl
use IO::File;
use File::Basename;
use File::Find;
use Cwd;
use Cwd 'abs_path';



$nprocs=`grep vendor_id /proc/cpuinfo | wc -l `;
$nprocs=~s/[\n\r]//g;
print "$nprocs processors found\n";

#find where to include master make file from
$srcdir=getcwd;
die "can't determine path to src" 
    unless ($srcdir=~s@/src.*$@/src@);


find( { wanted=> \&handle_vpc_file } ,"$srcdir");			# search through all directories for .vpc files

@MAINTARGETS=("all", "clean", "objs");
@TARGETS=("all", "clean", "objs", "tags");



# now, write a master makefile in each dir, and a master-master makefile in ~/src
foreach $dir ( keys %dir_written )
{
	open( MAKEOUT,">$dir/Makefile" ) || die "can't write $dir/Makefile";
	foreach $target ( @TARGETS )
	{
		print MAKEOUT "$target:\n";
		foreach $_ (split(/,/,$dir_written{$dir}) )
		{
			print MAKEOUT "\tmake -j $nprocs -f $_ $target\n" if length($_);
		}
	}
	close MAKEOUT;
}

# now, write a master makefile in ~/src
open( MAKEOUT,">$srcdir/Makefile" ) || die "can't write master makefile to $srcdir";
foreach $target ( @MAINTARGETS )
{
	print MAKEOUT "$target:\n";
	foreach $dir ( keys %dir_written )
	{
		print MAKEOUT "\tmake -j $nprocs -C $dir $target   #$dir_conf_type{$dir}\n" if ($dir_conf_type{$dir} eq "lib");
	}
	foreach $dir ( keys %dir_written )
	{
		print MAKEOUT "\tmake -j $nprocs -C $dir $target   #$dir_conf_type{$dir}\n" if ($dir_conf_type{$dir} ne "lib");
	}

}
print MAKEOUT "\n\nmakefiles:\n\tperl $srcdir/devtools/bin/vpc2linuxmake.pl\n";
print MAKEOUT "\ntags:\n\trm -f $srcdir/TAGS\n";
print MAKEOUT "\tetags -a -C -o $srcdir/TAGS public/*.cpp public/*.h public/*/*.cpp public/*/*.h common/*.cpp common/*.h\n";

foreach $dir ( keys %dir_written )
{
	print MAKEOUT "\tmake -C $dir tags\n";
}


close MAKEOUT;

sub handle_vpc_file
{
	# called for each file in the callers dir tree
	my $dir=$File::Find::dir;
	return if ( $dir=~/vpc_scripts/i );
    if ( /_base\.vpc$/i )
	{
		unless ( /hk_base\.vpc$/i )
		{
			return;
		}
	}

    if ( /_include\.vpc$/i )
        {
                unless ( /hk_base\.vpc$/i )
                {
                        return;
                }
        }

    if (/\.vpc$/)
    {
		(%ignore_file,@DEFINES, @CPPFILES, @CXXFILES,@CFILES, @LITERAL_LIBFILES,@LIBFILES, %define_seen,%macros,%include_seen,@INCLUDEDIRS)=undef;
		undef $buildforlinux;
		undef $conf_type;
		$OptimizerLevel='$(SAFE_OPTFLAGS_GCC_422)';
		$SymbolVisibility='hidden';


		# some defines to ignore in vpc files when generating linux include files

		$define_seen{'WIN32'}=1;
		$define_seen{'_WIN32'}=1;
		$define_seen{'_WINDOWS'}=1;
		$define_seen{'_USRDLL'}=1;
		$define_seen{'DEBUG'}=1;
		$define_seen{'_DEBUG'}=1;
		$define_seen{'NDEBUG'}=1;
		$define_seen{'_CRT_SECURE_NO_DEPRECATE'}=1;
		$define_seen{'_CRT_NONSTDC_NO_DEPRECATE'}=1;
		$define_seen{'fopen'}=1;
		#print "$_\n";
		&ParseVPC($_);

		$pname=lc($pname);
		$pname=~s/\s+/_/g;
		$pname=~s/[\(\)]//g;
		# if anything seen, output a makefile
		if ( $buildforlinux && ( @CPPFILES || @CXXFILES || @CFILES || @LIBFILES ) )
		{
			print STDERR "writing project $pname\n";
			$projdir=getcwd;
			$projdir=~s@/$@@;
			$dir_written{$projdir}.=",$pname.mak";
			$dir_conf_type{$projdir}=$conf_type;
			&WriteMakefile("$projdir/$pname.mak");
			&WriteCodeBlocksProj("$projdir/$pname.cbp");
		}
		else
		{
			die "no .lib or source files found in .vpc" if ( $buildforlinux );
		}
    }
}


sub WriteCodeBlocksProj
{
    local($_)=@_;

	open(CBPROJ,">$_") || die "can't write $_";

    print CBPROJ <<HEADER
<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
<CodeBlocks_project_file>
<FileVersion major="1" minor="6" />
<Project>
<Option title="$pname" />
<Option pch_mode="2" />
<Option compiler="gcc" />
<Build>
    <Target title="Release">

    </Target>
</Build>
HEADER
;

	foreach $fl (@CPPFILES)
	{
		push @cppfiles2, $fl unless ( $ignore_file{$fl} > 0 );
	}

	foreach $fl (@CXXFILES)
	{
		push @cxxfiles2, $fl unless ( $ignore_file{$fl} > 0 );
	}

    printf CBPROJ "\t\t<Compiler>\n";

    foreach $_ (@DEFINES)
    {
        print CBPROJ "\t\t\t<Add option=\"-DSWDS\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-D_LINUX\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-fpermissive\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-Dstricmp=strcasecmp\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-D_stricmp=strcasecmp\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-D_strnicmp=strncasecmp\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-Dstrnicmp=strncasecmp\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-D_snprintf=snprintf\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-D_vsnprintf=vsnprintf\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-D_alloca=alloca\" />\n";
        print CBPROJ "\t\t\t<Add option=\"-Dstrcmpi=strcasecmp\" />\n";

        print CBPROJ "\t\t\t<Add option=\"-D$_\" />\n";
    }

    foreach $_ (@INCLUDEDIRS)
    {
        print CBPROJ "\t\t\t<Add directory=\"$_\" />\n";
    }

    printf CBPROJ "\t\t</Compiler>\n";

    @CPPFILES = sort(@CPPFILES);
    @CXXFILES = sort(@CXXFILES);
    @CFILES = sort(@CFILES);

    # now, output obj dependencies
    foreach $_ (@CPPFILES, @CFILES, @CXXFILES)
    {
      unless (( $ignore_file{$_} > 0 ) || ( length($_) < 2 ) )
        {
          ($filename,$dir,$suffix) = fileparse($_,qr/\.[^.]*/);

          print CBPROJ "\t\t<Unit filename=\"".$dir . $filename. ".cpp\" />\n";
        }
    }

    print CBPROJ <<FOOTER
<Extensions>
    <code_completion />
</Extensions>
</Project>
</CodeBlocks_project_file>
FOOTER
;

    close CBPROJ;

}


sub WriteMakefile
{
	local($_)=@_;
	my $objdir ="obj";
	open(MAKEFILE,">$_") || die "can't write $_";
	print MAKEFILE "NAME=$pname\n\n";
	if ( $pname =~ /(server|client)_(\S+)/i )
	{
		print MAKEFILE "OBJSUFFIX=_$2\n";
		$objdir .= "_$2";
	}
	print MAKEFILE "SRCROOT=$srcdir\n";
	print MAKEFILE "PROJDIR=$projdir\n";
	print MAKEFILE "CONFTYPE=$conf_type\n";
	print MAKEFILE "OptimizerLevel=$OptimizerLevel\n";
	print MAKEFILE "SymbolVisibility=$SymbolVisibility\n";


	if (@DEFINES)
	{
		print MAKEFILE "DEFINES= -D",join(" -D", @DEFINES),"\n";
	}
	if (@INCLUDEDIRS)
	{
		print MAKEFILE "INCLUDEDIRS= -I",join(" -I", @INCLUDEDIRS),"\n";
	}
	undef @cppfiles2;
	undef @cxxfiles2;
	foreach $fl (@CPPFILES)
	{
		if ( length($fl) )
		{
			print "warning file $fl does not exist\n" unless( -e $fl);
			push @cppfiles2, $fl unless ( $ignore_file{$fl} > 0 );
		}
	}
	foreach $fl (@CXXFILES)
	{
		push @cxxfiles2, $fl unless ( $ignore_file{$fl} > 0 );
	}
	
	if (@cppfiles2)
	{
		print MAKEFILE "CPPFILES= \\\n  ", join(" \\\n  ",@cppfiles2), "\n";
	}
	if (@cxxfiles2)
	{
		print MAKEFILE "CXXFILES= \\\n  ", join(" \\\n  ",@cxxfiles2), "\n";
	}
	if (@CFILES)
	{
		print MAKEFILE "CFILES= \\\n  ", join(" \\\n  ",@CFILES), "\n";
	}
	if (@LIBFILES)
	{
		undef @LIBNAMES;
		print MAKEFILE "\nLIBFILES= \\\n";
		unless( $pname=~/(tier0)|(mathlib)|(tier1)/i)
		{
			print MAKEFILE "  $srcdir/lib/linux/tier1_486.a \\\n"
		}
		foreach $lib (@LIBFILES)
		{
			my @DLLNAMES=("tier0", "vstdlib", "steam_api");
			unless ( $ignore_file{$lib} > 0 )
			{
				$lib=lc($lib);
				my ($filename,$dir,$suffix) = fileparse($lib,qr/\.[^.]*/);
				my $dll=0;
				foreach $dllname (@DLLNAMES)
				{
					$dll=1 if ( $dllname eq $filename);
				}
				if ( $dll )
				{
					$lib=~s@^.*/([^/]+)\.lib@$1_i486.so@i;
					$lib=~s@^.*/lib/(\S+)@$1@g;
				}
				else
				{
					$lib=~s/\.lib/_486.a/i;
					$lib=~s@/lib/(\S+)/@/lib/linux/@g;
				}
				push @LIBNAMES, $lib;
			}
		}
		foreach $lib (@LITERAL_LIBFILES)
		{
			unless ( $ignore_file{$lib} > 0 )
			{
				$lib=~s/\\/\//g;
				$lib=~s@/linux/([a-zA-Z_0-9\.]+)$@/linux/$1@;
				$lib=~s@^.*/linux/([a-zA-Z_0-9]+)\.so$@$1.so@;
				push @LIBNAMES, $lib;
			}
		}
		# now, sort libs for link order
		foreach $lib ( sort bypriority @LIBNAMES )
		{
			print MAKEFILE "  $lib \\\n";
		}
		print MAKEFILE "\n\n";
	}

	if ( $conf_type eq "dll" )
	{
		print MAKEFILE "OUTPUT_SO_FILE=$srcdir/linux/$pname","_i486.so\n\n";
	}
	elsif ( $conf_type eq "exe" )
	{
		if ( $macros{'OUTBINNAME'} eq "" )
		{
			die "Missing OUTBINNAME macro";
		}

		print MAKEFILE "OUTPUT_EXECUTABLE=$srcdir/linux/$macros{'OUTBINNAME'}\n\n";
	}

	print MAKEFILE "\n\n\# include base make file\ninclude $srcdir/devtools/makefile_base_linux.mak\n";

	# now, output obj dependencies
	foreach $_ (@CPPFILES, @CFILES)
	{
		unless (( $ignore_file{$_} > 0 ) || ( length($_) < 2 ) )
		{
			($filename) = fileparse($_,qr/\.[^.]*/);
			print MAKEFILE getcwd,"/$objdir/$filename.o : $_\n\t\$(DO_CC)\n";
		}
	}
	foreach $_ (@CXXFILES)
	{
		unless (( $ignore_file{$_} > 0 ) || ( length($_) < 2 ) )
		{
			($filename) = fileparse($_,qr/\.[^.]*/);
			print MAKEFILE getcwd,"/$objdir/$filename.oxx : $_\n\t\$(DO_CC)\n";
		}
	}

	close MAKEFILE;
}

sub bypriority
{
# sort libs for gcc linkgoodness
	$priority{"mathlib"}="0005";
	$priority{"tier1"}="0010";
	$priority{"tier2"}="0020";
	$priority{"tier3"}="0030";

	my ($filenamea) = fileparse($a,qr/\.[^.]*/);
	my ($filenameb) = fileparse($b,qr/\.[^.]*/);
	$filenamea =~ s/_.86.*$//;		# lose _i486
	$filenameb =~ s/_.86.*$//;
	my $pa=$priority{$filenamea} || 1000;
	my $pb=$priority{$filenameb} || 1000;
	return $pb cmp $pa;
}

sub ParseVPC
{
    local($fname)=@_;
    &startreading($fname);
    while(&nextvpcline)
    {
#		print "$fname -  $_\n";
		if ( (/^\$linux/i) )
		{
			&skipblock(0,\&handlelinuxline);
		}
		if ( (/^\$configuration/i) )
		{
			&skipblock(0,\&handleconfigline);
		}
		elsif (/^\s*\$project/i)
		{
			&parseproject;
		}
    }
}

sub massageline
{
    # strip leading and trailing spaces and carriage returns and comments from vpc lines
    s/[\n\r]//g;
    s@//.*$@@g;
    s@^\s*@@g;
    s@\s*$@@g;
}

sub submacros
{
    # replace all macros within a line
    my $mac;
    foreach $mac (keys %macros)
    {
	s/\$$mac/$macros{$mac}/g;
    }
}


sub startreading
{
    # initialize recursive file reader
    my( $fname)=@_;
    #print STDERR "opening $fname\n";
	$curfile=IO::File->new($fname) || die "can't open $fname";
}

sub nextvpcline
{
    # get the next line from the file, handling line continuations, macro substitution, and $include
    # return 0 if out of lines
    my $ret=0;
    if ( $_ = <$curfile> )
    {
		$ret=1;
		&massageline;
		while(s@\\$@ @)
		{
			my $old=$_;
			$_=<$curfile>;
			&massageline;
			$_=$old.$_;
		}
		s@\s+@ @g;
		my $old=$_;
		&submacros;
		# now, parse
		if (/\$macro (\S+) \"(\S+)\"$/i)
		{
			$macros{$1}=$2;
			return &nextvpcline;
		}
		s/\[\$WIN32\]//g;
		return &nextvpcline if (/\[\$X360\]/);
		if ( /^\s*\$include\s+\"(.*)\"/i)
		{
			# process $include
			my $incfile=$1;
			push @filestack, $curfile;
			$incfile=~s@\\@/@g;
			#print STDERR "opening $incfile\n";
			#$curfile=IO::File->new($incfile) || die "can't open include $incfile ($pname)";
			if ( -e $incfile )
			{
				push @filestack, $curfile;
				#print STDERR "opening $incfile\n";
				$curfile=IO::File->new($incfile) || die "can't open include $incfile";
			}
			else
			{
				print "can't find include $incfile, ignoring\n";
			}
			
			return &nextvpcline;
		}
    }
    else
    {
		$curfile->close;
		if (@filestack)
		{
			$curfile=pop(@filestack);
			return &nextvpcline;
		}
		else
		{
			return 0;
		}
    }
    return $ret;
}

sub skipblock
{
    # skip a named block in the key values, handling nested {} pairs
    my($empty_ok, $callback)=@_;
    my $lnstat=&nextvpcline;
    die "parse error eof in block" if ( (! $lnstat) && ( ! $empty_ok) );
	
    my $nest=0;
    if (/^\{/)
    {
		$nest++;
    }
    else
    {
		die "no start block found, $_ found instead" unless($empty_ok);
    }
    while ($nest)
    {
		die "prematur eof" unless &nextvpcline;
		&$callback($_) if ( $callback );
		$nest++ if (/^\{/);
		$nest-- if (/^\}/);
    }
}

sub parseproject
{
    # handle a project block, picking up files mentioned
    $pname="";
    $pname=$1 if (/^\s*\$project\s*\"(.*)\"/i);
    local($_);
    my $nest=0;
    &nextvpcline || die "empty project?";
    $nest++ if (/^\s*\{/);
    while($nest )
    {
		&nextvpcline || die "premature eof in project?";
		$nest++ if (/^\{/);
		$nest-- if (/^\}/);
		&CheckForFileLine($_);
	}
}

sub CheckForFileLine
{
	local($_)=@_;
	if (/^\s*\-\$File\s+(.*$)/i)
	{
		foreach $_ (split(/ /,$1))
		{
		    s/\"//g;
			$ignore_file{&process_path($_)} = 1;
		}
	}
	
	elsif (/^\s*\$File\s+(.*$)/i)
	{
		foreach $_ (split(/ /,$1))
		{
		    s/\"//g;
		    &handlefile($_);
		}
	}
}

sub handlefile
{
    # given a project file (.cpp, etc), figure out what to do with it
    local($_)=@_;

	# hardcoded exclusions for linux
    return if (/dx9sdk/i);
    return if (/_360/i);
    return if (/xbox_console.cpp/i);
    return if (/xbox_system.cpp/i);
    return if (/xbox_win32stubs.cpp/i);
    return if (/binkw32/i || /binkxenon/i );

	if (/\.cpp$/)
    {
		push @CPPFILES,process_path($_);
    }
	if (/\.cxx$/)
    {
		push @CXXFILES,process_path($_);
    }
    elsif (/\.c$/)
    {
		push @CFILES,process_path($_);
    }
    elsif (/\.lib$/)
    {
		push @LIBFILES,process_path($_);
    }
    elsif (/\.a$/)
    {
		push @LITERAL_LIBFILES, process_path($_);
    }
    elsif (/\.so$/)
    {
		push @LITERAL_LIBFILES, process_path($_);
    }
}

sub process_path
{
	local($_)=@_;
    s@\\@/@g;
    if ( (! -e $_) && ( -e lc($_)) )
	  {
#		print STDERR "$_ does not exist try lc($_)\n";
		$_=lc($_);
	  }
    my $ap=abs_path($_);
	if ( (! length($ap) ) && length($_))
	{
#		print "abs path of $_ is empty. bad dir?\n";
	}
	$_=$ap;
	s@i686@i486@g;
    if ( (! -e $_) && ( -e lc($_)) )
	  {
#		print STDERR "$_ does not exist try lc($_)\n";
		$_=lc($_);
	  }
	# kill ..s for prettyness
    s@/[^/]+/\.\./@/@g;
	if (! -e $_)
	  {
#		print STDERR "$_ does not exist\n";
	  }
    return $_;
}

sub handlelinuxline
{
    local($_)=@_;
    $buildforlinux = 1 if ( /^\s*\$buildforlinux.*1/i);
    $OptimizerLevel= $1 if (/^\s*\$OptimizerLevel\s+(.+)/i);
    $SymbolVisibility = $1 if (/^\s*\$SymbolVisibility\s+(.+)/i);
    $buildforlinux = 1 if ( /^\s*\$buildforlinux.*1/i);
    &CheckForFileLine($_); # allows linux-specific file includes and excludes
    &handleconfigline($_);			   # allow linux-specific #defines

}


sub CheckPreprocessorDefs
{
	local($_)=@_;
	if (/^\s*\$PreprocessorDefinitions\s+\"(.*)\"/i)
    {
		foreach $_ (split(/[;,]/,$1) )
		{
			unless( /\$/ || $define_seen{$_} || /fopen/i)
			{
				push(@DEFINES,$_);
				$define_seen{$_}=1;
			}
		}
	}
}
sub handleconfigline
{
    # handle a line within a $Configuration block
    local($_)=@_;				# the line
    if (/^\s*\$AdditionalIncludeDirectories\s+\"(.*)\"/i)
    {
		foreach $_ (split(/[;,]/,$1) )
		{
			unless( /\$/ || $include_seen{$_} )
			{
				push(@INCLUDEDIRS,process_path($_));
				$include_seen{$_}=1;
			}
		}
    }
	if (/^\s*\$ConfigurationType\s*\"(.*)\"/)
	{
		undef $conf_type;
		$conf_type="lib" if ($1 =~ /Static Library/i);
		$conf_type="dll" if ($1 =~ /Dynamic Library/i);
		$conf_type="exe" if ($1 =~ /Application/i);
		print STDERR " unknown conf type $1\n" if (! length($conf_type) );

	}

    &CheckPreprocessorDefs($_);
}
