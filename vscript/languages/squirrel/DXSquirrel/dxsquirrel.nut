local presentparams = {
	Windowed = true,
	SwapEffect = DX.D3DSWAPEFFECT_DISCARD,
	BackBufferFormat = DX.D3DFMT_A8R8G8B8,
	EnableAutoDepthStencil = true,
	AutoDepthStencilFormat = DX.D3DFMT_D16,
	BackBufferWidth = 800,
	BackBufferHeight = 600
}


local dev = Device(presentparams);

//INIT the mesh
local mesh = Mesh(dev); 
local perspective = Matrix();
local view = Matrix();
local world = Matrix();
local eye = Vector3( 0.0, 3.0,-5.0 );
local lookat = Vector3( 0.0, 0.0, 0.0 );
local upVec = Vector3( 0.0, 1.0, 0.0 );

view.CreateLookAtMatrix(eye,lookat,upVec);
perspective.CreatePerspectiveFovMatrix(DX.D3DX_PI/4, 640.0/480, 1.0, 100.0)

dev.SetTransform(DX.D3DTS_VIEW,view);
dev.SetTransform(DX.D3DTS_PROJECTION,perspective);

dev.SetRenderState(DX.D3DRS_ZENABLE,true);
dev.SetRenderState(DX.D3DRS_AMBIENT,0xFFFFFFFF);

//MAIN LOOP
local rot = Vector3(0,0,0);
while(DX.Update()) {
	dev.Clear(DX.D3DCLEAR_TARGET|DX.D3DCLEAR_ZBUFFER,0xFF0000FF);
	dev.BeginScene();
		rot.y += 0.1;
		world.RotateAngles(rot);
		dev.SetTransform(DX.D3DTS_WORLD,world);
		mesh.DrawSubset(0);	
	//
	dev.EndScene();
	dev.Present();
}
