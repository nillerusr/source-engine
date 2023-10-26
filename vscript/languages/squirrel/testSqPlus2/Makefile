SQUIRREL= ..

OUT= ./testSqPlus2
INCDIRS= -I$(SQUIRREL)/include -I. -I$(SQUIRREL)/sqlibs -I$(SQUIRREL)/sqplus
LIBDIR= -L$(SQUIRREL)/lib 
#LIBS= -lsquirrel -lsqstdlib -lsqplus
LIBS= -lsqplus -lsquirrel -lsqstdlib 

SRCS= testSqPlus2.cpp

all: testSqPlus2

FORCE: ;
	
testSqPlus2: FORCE 
	g++ -O3 -fno-rtti -Os -o $(OUT) $(SRCS) $(INCDIRS) $(LIBDIR) $(LIBS)
