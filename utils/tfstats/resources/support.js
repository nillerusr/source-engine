function SuperImage(imgName,onsrc,offsrc)
{
	this.HTMLimgName=imgName;
	this.onSrc=onsrc;
	this.offSrc=offsrc;
	this.selected=false;
	this.hover=false;
//	alert(this.HTMLimgName.toString());
//	alert(this.offSrc.toString());
//	alert(this.onSrc.toString());
}

 new SuperImage("crap","crapon.gif","crapoff.gif");

function SuperImage_over()
{
	this.hover=true;
//	alert("blap");
	this.update();
}
SuperImage.prototype.over=SuperImage_over;
	
function SuperImage_on()
{
	this.selected=true;
	this.update();
}
SuperImage.prototype.on=SuperImage_on;

function SuperImage_off()
{
	this.selected=false;
	this.update();
}
SuperImage.prototype.off=SuperImage_off;

function SuperImage_out()
{
	this.hover=false;
	this.update();
}
SuperImage.prototype.out=SuperImage_out;

function SuperImage_update()
{
	if (document.images)
	{ 
		if (this.hover == true || this.selected == true)
		    document[this.HTMLimgName].src = this.onSrc;
		else
			document[this.HTMLimgName].src = this.offSrc; 
	}
}
SuperImage.prototype.update=SuperImage_update;

