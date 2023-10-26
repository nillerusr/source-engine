local x=10;
local y="asd"
local testy = function(){return 0;}

TestTable <- {
	livello2 = {
		livello3 = {
			["yooo"]="I'm cool",
			["jaaaa"]=[1,2,3,4,5,6,7,8,9,10],
			
		}
	}
}

function TestFunc(a,b) 
{
	local z=100
	local s="I'm a string"
	for(local i=0;i<10;i++)
		TestTable.cappero(z,i);
	//index_that_desnt_exist=100; //error
	return 0;
}

function TestTable::cappero(a,b)
{
	local ueueueu=100
	local s={x="I'm a string"}
	oioioi(1,2)
}

function TestTable::oioioi(a,b)
{
	local frrrr=100
	local xyz={x="I'm a string"}
	
}	
	


local i = 0;
while(1)
{
	local ret;
	local testweak = "asdasd";
	local weako =  testweak.weakref();
	i++;
	ret=TestFunc("param 1","param 2");
}

