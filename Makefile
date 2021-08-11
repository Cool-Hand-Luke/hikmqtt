
CXX   =  g++
CXXFLAGS = -Wall -Wno-strict-aliasing -Wno-unused-variable

SUBDIR   = $(shell ls src -R | grep :)
SUBDIRS  = $(subst :,/,$(SUBDIR))
INCPATHS = -Ihikvision/include
INCPATHS += -Iinclude/ -Iconcurrentqueue/ 

VPATH = $(subst : ,:,$(SUBDIR))./
SOURCE = $(foreach dir,$(SUBDIRS),$(wildcard $(dir)*.cpp))

OBJS = $(patsubst %.cpp,%.o,$(SOURCE))
OBJFILE  = $(foreach dir,$(OBJS),$(notdir $(dir)))
OBJSPATH = $(addprefix obj/,$(OBJFILE))-

LIBPATH = hikvision/lib
LIBS = -Wl,-rpath=hikvision/lib:./lib -pthread -lhcnetsdk -lmosquitto -lcjson -lconfig++ -lstdc++fs
EXE = hikmqtt

$(warning SUBDIR is $(SUBDIR))
$(warning SUBDIRS is $(SUBDIRS))
$(warning INCPATHS is $(INCPATHS))
$(warning VPATH is $(VPATH))
$(warning SOURCE is $(SOURCE))

all: ${EXE}

#debug: CXXFLAGS += -DDEBUG -ggdb -gdward2 -Og
debug: CXXFLAGS += -DDEBUG -ggdb -gdward2 -O0
debug: ${EXE}

$(EXE):$(OBJFILE)
	$(CXX) -L$(LIBPATH)  -o $(EXE) $(OBJFILE) $(INCPATHS) $(LIBS)

$(OBJFILE):%.o:%.cpp
	$(CXX)  -std=c++1z -c -o $@ $<  $(INCPATHS) -pipe -g -Wall

DPPS = $(patsubst %.cpp,%.dpp,$(SOURCE))
$(warning DPPS is $(DPPS))
include $(DPPS)
%.dpp: %.cpp
	g++ $(INCPATHS) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$


.PHONY:clean
clean:
	rm -rf $(OBJFILE)
	rm -rf $(DPPS)
	rm -rf $(EXE)

