include ../Makefile.common

ifneq ("yes","$(strip $(IMAS_CPP))")
all clean:
	$(warning "Ignoring plugin compilation (IMAS_CPP=no).")
else

DBGFLAGS= -g
DBGFLAGS+= -DDEBUG -DBOOST_ALL_DYN_LINK
NOT_SUPPORTED_COMPILER=
CC = gcc
CXX = g++
ifeq "$(strip $(CC))" "icc"
CXXFLAGS= -O0 -fPIC -shared-intel ${DBGFLAGS}
LDFLAGS=
else ifeq "$(strip $(CC))" "gcc"
CXXFLAGS= -std=c++11 -pthread -O0 -fPIC ${DBGFLAGS}
LDFLAGS= -shared -pthread -Wl,--no-undefined
LDFLAGS_HLI=-pthread -Wl,--no-undefined
else
NOT_SUPPORTED_COMPILER=unsupported_compiler
endif
INCDIR_PKGCONFIG=`pkg-config blitz --cflags`
INCDIR= -I$(UAL_BUILD)/lowlevel
LIBS= -lal `pkg-config blitz --libs` -L$(BOOST_ROOT)/lib -L$(UAL_BUILD)/lowlevel -lboost_log -lboost_thread

INCDIR_HLI=$(INCDIR) -I$(UAL_BUILD)/cppinterface/src $(INCDIR_PKGCONFIG)
LIBS_HLI=-L$(UAL_BUILD)/cppinterface/lib $(LIBS) -lal-cpp

OBJ_DEBUG_PLUGIN = debug_plugin.o simple_logger.o
OBJ_NBC_PLUGIN = nbc_plugin.o 
OBJ_CREATION_DATE_PLUGIN = creation_date_plugin.o
OBJ_PARAMETER_PLUGIN = parameter_plugin.o al_reader_helper_plugin.o
OBJ_PARTIAL_GET_PLUGIN = partial_get_plugin.o simple_logger.o
OBJ_PATCH_READER_PLUGIN = patch_reader_plugin.o simple_logger.o al_reader_helper_plugin.o

PLUGINS = debug_plugin creation_date_plugin parameter_plugin partial_get_plugin patch_reader_plugin
EXE = $(PLUGINS)


all: $(NOT_SUPPORTED_COMPILER) $(EXE)

debug_plugin: $(OBJ_DEBUG_PLUGIN)
	@echo Linking $^
	$(CXX) $(LDFLAGS) -o $@.so $^ $(LIBDIR) $(LIBS)

nbc_plugin: $(OBJ_NBC_PLUGIN)
	@echo Linking $^
	$(CXX) $(LDFLAGS) -o $@.so $^ $(LIBDIR) $(LIBS)

creation_date_plugin: $(OBJ_CREATION_DATE_PLUGIN)
	@echo Linking $^
	$(CXX) $(LDFLAGS) -o $@.so $^ $(LIBDIR) $(LIBS)

parameter_plugin: $(OBJ_PARAMETER_PLUGIN)
	@echo Linking $^
	$(CXX) $(LDFLAGS) -o $@.so $^ $(LIBDIR) $(LIBS)

partial_get_plugin: $(OBJ_PARTIAL_GET_PLUGIN)
	@echo Linking $^
	$(CXX) $(LDFLAGS) -o $@.so $^ $(LIBDIR) $(LIBS)

patch_reader_plugin: $(OBJ_PATCH_READER_PLUGIN)
	@echo Linking $^
	$(CXX) $(LDFLAGS) -o $@.so $^ $(LIBDIR) $(LIBS)


.cpp.o:
	@echo compiling $<
	$(CXX) $(INCDIR) $(INCDIR_PKGCONFIG) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) *.log *.o *.so $(OBJ) $(EXE) $(PLUGINS)

unsupported_compiler:
	@echo Makefile does not support $(CC) compiler \(try CC=gcc or icc\).
	exit 1
endif