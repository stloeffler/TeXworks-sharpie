CXX ?= g++

CXXFLAGS := -g -O0 -I. $(shell pkg-config freetype2 poppler poppler-qt4 QtCore QtGui QtXml --cflags)
LDFLAGS := $(shell pkg-config freetype2 poppler poppler-qt4 QtCore QtGui QtXml --libs)

speed-test-mupdf : speed-test-mupdf.o
	g++ ${LDFLAGS} -o $@ $< -lmupdf -lfitz -ljbig2dec -ljpeg -lopenjpeg -lz

speed-test-poppler : speed-test-poppler.o

