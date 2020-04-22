for( $i = 0; $i < 16; $i++ )
{
	print "\#define SHADER_CLASS$i(name,help";
	for( $j = 0; $j < $i; $j++ )
	{
		print ",p$j,p$j" . "type,p$j" . "default,p$j" . "help\\\n\t\t";
	}
	print ")\\\n";
	print "\tstatic const char *s_HelpString = help;\\\n";
	print "\tstatic const char *s_ParamNames[] = {";
	if( $i == 0 )
	{
		print "NULL";
	}
	else
	{
		for( $j = 0; $j < $i; $j++ )
		{
			print "\\\n\t\t\"\$\" #p$j";
			if( $j != $i - 1 )
			{
				print ",";
			}
		}
	}
	print "\\\n\t};\\\n";

	print "\tstatic ShaderParamType_t s_ParamType[] = {";
	if( $i == 0 )
	{
		print "SHADER_PARAM_TYPE_INTEGER";
	}
	else
	{
		for( $j = 0; $j < $i; $j++ )
		{
			print "\\\n\t\tp$j" . "type";
			if( $j != $i - 1 )
			{
				print ",";
			}
		}
	}
	print "\\\n\t};\\\n";

	print "\tstatic const char *s_ParamDefault[] = {";
	if( $i == 0 )
	{
		print "\"\"";
	}
	else
	{
		for( $j = 0; $j < $i; $j++ )
		{
			print "\\\n\t\tp$j" . "default";
			if( $j != $i - 1 )
			{
				print ",";
			}
		}
	}
	print "\\\n\t};\\\n";

	print "\tstatic const char *s_ParamHelp[] = {";
	if( $i == 0 )
	{
		print "\"\"";
	}
	else
	{
		for( $j = 0; $j < $i; $j++ )
		{
			print "\\\n\t\tp$j" . "help";
			if( $j != $i - 1 )
			{
				print ",";
			}
		}
	}
	print "\\\n\t};\\\n";


	print "\\\n\tenum {";
	if( $i == 0 )
	{
		print "DUMMY_PARAM";
	}
	else	
	{
		for( $j = 0; $j < $i; $j++ )
		{
			print "\\\n\t\tp$j";
			if( $j == 0 )
			{
				print " = NUM_SHADER_MATERIAL_VARS";
			}
			if( $j != $i - 1 )
			{
				print ",";
			}
		}
	}
	print "\\\n\t};\\\n";
	print "\tstatic const char *s_Name = #name;\\\n\tclass CShader_ ## name : public Shader_t\n\n";
}
