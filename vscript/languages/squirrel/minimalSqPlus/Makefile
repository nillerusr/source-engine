SQUIRREL= ..

OUT= ./minimalSqPlus
INCDIRS= -I$(SQUIRREL)/include -I. -I$(SQUIRREL)/sqlibs -I$(SQUIRREL)/sqplus
LIBDIR= -L$(SQUIRREL)/lib 
#LIBS= -lsquirrel -lsqstdlib -lsqplus
LIBS= -lsqplus -lsquirrel -lsqstdlib 

SRCS= minimalSqPlus.cpp

all: minimalSqPlus

FORCE: ;
	
minimalSqPlus: FORCE
#	g++ -fno-rtti -o $(OUT) $(SRCS) $(INCDIRS) $(LIBDIR) $(LIBS)
	g++ -O3 -Wall -o $(OUT) $(SRCS) $(INCDIRS) $(LIBDIR) $(LIBS)
	#g++ -O3 -fno-rtti -Os -o $(OUT) $(SRCS) $(INCDIRS) $(LIBDIR) $(LIBS)
