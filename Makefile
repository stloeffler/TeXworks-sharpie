CXX ?= g++

CXXFLAGS := -g -O0 -I. -I./src $(shell pkg-config freetype2 poppler poppler-qt4 QtCore QtGui QtXml --cflags)
# Disable implicit casts from ASCII to UTF8
CXXFLAGS += -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_CAST_FROM_BYTEARRAY
# Debugging output?
CXXFLAGS += -DDEBUG -DMU_DEBUG
LDFLAGS := $(shell pkg-config freetype2 poppler poppler-qt4 QtCore QtGui QtXml --libs)
# MuPDF libraries. MuPDF builds as a static lib, so its dependencies have to be
# explicitly linked.
#LDFLAGS += -lfitz -lmupdf -ljbig2dec -ljpeg -lopenjpeg -lz

VPATH = src
SRCS := main.cpp PDFViewer.cpp moc_PDFViewer.cpp PDFDocumentView.cpp moc_PDFDocumentView.cpp MuPDF.cpp MuPDF-qt4.cpp
OBJS = $(SRCS:.cpp=.o)

all: pdf_viewer

pdf_viewer: $(OBJS) icons.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o pdf_viewer $^ -lmupdf -lfitz -ljbig2dec -ljpeg -lopenjpeg -lz

%.cpp : %.qrc
	rcc -o $@ $<

moc_%.cpp: %.h
	moc $< > $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean :
	rm -f pdf_viewer $(OBJS) icons.cpp

docs :
	cd docs && rake

.PHONY: all clean docs

