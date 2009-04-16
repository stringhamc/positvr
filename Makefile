#CFLAGS = -g
EXES = posit.exe calibration.exe
CFLAGS = -g -I/usr/local/include/opencv -I.
LDFLAGS = -L/usr/local/lib -lcxcore -lcv -lhighgui -lcvaux -lml -L. -lblob
LIBS =  -L. -lblob -L/usr/local/lib -lcxcore -lcv -lhighgui -lcvaux -lml -I/usr/local/include/opencv 
CXX=g++

CPPFILES= \
	Blob.cpp\
	BlobResult.cpp \
	BlobExtraction.cpp

all: $(EXES) libblob.a

clean:
	@echo Cleaning...
	rm -f $(CFILES:.cpp=.o)
	rm -f libblob.a
	rm -rf $(EXES)

.SUFFIXES: .cpp.o
.cpp.o:	; echo 'Compiling $*.cpp' ; $(CXX) $(CFLAGS) -c $*.cpp

.SILENT:

libblob.a: $(CPPFILES:.cpp=.o)
	ar ru libblob.a $(CPPFILES:.cpp=.o) 2> /dev/null
	ranlib libblob.a
#
#	@echo Build tests, examples and tools...
#	$(CXX) -g blobdemo.cpp $(LDFLAGS) $(CFLAGS) -o blobdemo
#	$(CXX) -g blobdemo2.cpp $(LDFLAGS) $(CFLAGS) -o blobdemo2
#
#	@echo Copy include files...
#
#	@echo Cleaning objects...
#	rm -f $(CPPFILES:.cpp=.o)



posit.exe: posit.cpp libblob.a
	g++ -o $@ $? $(CFLAGS) $(LIBS)

run: posit.exe
	./posit.exe tryAgain.avi

calibration.exe: calibration.cpp
	g++ -o $@ $? $(CFLAGS) $(LIBS)
