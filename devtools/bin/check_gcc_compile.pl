#! perl

# scan all .o files for illegal instructions from passing aggregates to varargs functions.

use File::Find;

find( \&CheckFile, "." );

sub CheckFile
  {
	return unless (/\.o$/ );
	open( DIS, "objdump --disassemble -C $_|" ) || die "can't process $_";
	while( <DIS> )
	  {
		$symbol = $1 if ( /^[0-9]+ \<(.*)\>:/ );
		if ( /\s+ud2a/ )
		  {
			print "Illegal instruction in $symbol in ", $File::Find::name, "\n";
		  }

	  }

  }
