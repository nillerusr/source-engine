SQUIRREL= ..

OUT= ./testSqPlus
INCDIRS= -I$(SQUIRREL)/include -I. -I$(SQUIRREL)/sqlibs -I$(SQUIRREL)/sqplus
LIBDIR= -L$(SQUIRREL)/lib 
LIBS= -lsqplus -lsquirrel -lsqstdlib 

SRCS= testSqPlus.cpp

all: testSqPlus

FORCE: ;
	
testSqPlus: FORCE
	#g++ -fno-rtti -o $(OUT) $(SRCS) $(INCDIRS) $(LIBDIR) $(LIBS)
	g++ -O3 -fno-rtti -Os -o $(OUT) $(SRCS) $(INCDIRS) $(LIBDIR) $(LIBS)
