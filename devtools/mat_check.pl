#!perl

while(<>)
  {
	s/[\n\r]//g;
	s@\\@/@g;
	if (/\.vtf$/)
	  {
		$origname=$_;
		if (s@/game/(\S+)/materials/@/content/\1/materialsrc/@i)
		  {
			s/\.vtf$/.tga/i;
			unless (-e $_)
			  {
				print "Missing :$_ \n    for $origname\n";
			  }
			else
			  {
				#print "***found content $_ for $origname\n";
			  }

		  }
		else
		  {
			print "I don't know where the materialsrc for $origname is supposed to be!\n";
		  }

	  }

  }
