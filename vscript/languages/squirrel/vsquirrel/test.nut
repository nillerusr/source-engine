print( "Test\n" );

local value = TestReturn();
print("My value = " + value + "\n");

temp <- [ 1, 2, 3, 4 ];

foreach(idx,val in temp)
{
	print("index="+idx+" value="+val+"\n");
}

temp[0] = 33;
temp[1] = 34;
temp[2] = 35;
temp[3] = 36;

foreach(idx,val in temp)
{
	print("index="+idx+" value="+val+"\n");
}

foreach(idx,val in mytable)
{
	print("index="+idx+" value="+val+"\n");
}

	print("x = " + mytable.controlpoint_1_vector.x + "\n");
	print("y = " + mytable.controlpoint_1_vector.y + "\n");
	print("z = " + mytable.controlpoint_1_vector.z + "\n");



	local string = "controlpoint_" + 1;
	local vectortable = string + "_vector";
	local vector = Vector(0,0,0);
	if ( vectortable in mytable )
	{
		print("here\n");
		vector = Vector( mytable[vectortable].x, mytable[vectortable].y, mytable[vectortable].z );
	}

	print("Vector = " + vector + "\n" );
	