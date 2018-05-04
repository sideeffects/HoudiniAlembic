## This document is under CC-3.0 Attribution-Share Alike 3.0
##       http://creativecommons.org/licenses/by-sa/3.0/
##  Attribution:  There is no requirement to attribute the author.
#
#  Proof of concept for building Alembic plug-ins with a custom
#  Alembic library.  This requires you to create a libAbcHuge.so (see
#  MakeHuge)
#
#  How to use this:
#    1)  Untar the Alembic source shipped with Houdini
#	% make untar
#    2)  Edit the definitions for:
#	GABC_NAMESPACE
#	TOKEN_PREFIX
#	LABEL_PREFIX
#    3) Make the plugins
#	% make all
#
# If you get a warning that libAbcHuge.so is not found, you need to
# make sure that you've done "make -f MakeHuge" and installed it in the
# library path somewhere (i.e. $HH/dsolib)

GABC_NAMESPACE = GABC_Custom
TOKEN_PREFIX = "c"
LABEL_PREFIX = "Custom "

OPTIMIZER = -g
include $(HFS)/toolkit/makefiles/Makefile.linux

ifndef CXX
    CXX := g++
endif

ABCINC = /usr/local/alembic-1.7.7/include
ABCINC = $(HFS)/toolkit/include
ABCDEF = \
    -DGABC_NAMESPACE=$(GABC_NAMESPACE) \
    -DGABC_PRIMITIVE_TOKEN='"CustomAlembicRef"' \
    -DGABC_PRIMITIVE_LABEL='"Custom Alembic Delayed Load"' \
    -DCUSTOM_ALEMBIC_TOKEN_PREFIX='$(TOKEN_PREFIX)' \
    -DCUSTOM_ALEMBIC_LABEL_PREFIX='$(LABEL_PREFIX)'
ABCHUGE = libAbcHuge.so

# Extra file for half.h
HALFDIR = $(HFS)/toolkit/include/OpenEXR

HLIB = $(HFS)/dsolib
EXTRALIBS = \
	$(ABCHUGE) \
	$(HLIB)/libtbb.so \
	$(HLIB)/libIex.so

ABCFLAGS = $(ABCDEF) -I$(ABCINC) -I$(HALFDIR)

GABCCFILES = \
	src/GABC/GABC_Error.C \
	src/GABC/GABC_GEOWalker.C \
	src/GABC/GABC_IArchive.C \
	src/GABC/GABC_IArray.C \
	src/GABC/GABC_IGTArray.C \
	src/GABC/GABC_IItem.C \
	src/GABC/GABC_IObject.C \
	src/GABC/GABC_OArrayProperty.C \
	src/GABC/GABC_OGTGeometry.C \
	src/GABC/GABC_OOptions.C \
	src/GABC/GABC_OScalarProperty.C \
	src/GABC/GABC_PackedGT.C \
	src/GABC/GABC_PackedImpl.C \
	src/GABC/GABC_Types.C \
	src/GABC/GABC_Util.C
GABCOFILES = $(GABCCFILES:.C=.o)
GABCLIB = libs/libCustomGABC.so

ALEMBIC_IN	= src/SOP/SOP_AlembicIn.C
ALEMBIC_IN_SO	= $(ALEMBIC_IN:.C=.so)
ALEMBIC_IN_O	= $(ALEMBIC_IN:.C=.o)

ALEMBIC_GROUP		= src/SOP/SOP_AlembicGroup.C
ALEMBIC_GROUP_SO	= $(ALEMBIC_GROUP:.C=.so)
ALEMBIC_GROUP_O		= $(ALEMBIC_GROUP:.C=.o)

GU_ALEMBIC	= src/GU/GU_Alembic.C
GU_ALEMBIC_SO	= $(GU_ALEMBIC:.C=.so)
GU_ALEMBIC_O	= $(GU_ALEMBIC:.C=.o)

PLUGINS	= \
	$(ALEMBIC_IN) \
	$(ALEMBIC_GROUP) \
	$(GU_ALEMBIC)

CFILES	= $(GABCCFILES) $(PLUGINS)
OFILES	= $(GABCOFILES) $(PLUGINS:.C=.o)
TARGETS	= $(GABCLIB) $(PLUGINS:.C=.so)

%.o:	%.C
	$(CXX) -DCXX11_ENABLED=1 $(ABCFLAGS) $(OBJFLAGS) -c -o $@ $<

all:	$(GABCLIB) $(PLUGINS:.C=.so)

install:	all

$(GABCLIB):	$(GABCOFILES)
	mkdir -p libs
	$(CXX) -shared \
	    -Wl,-soname,$(GABCLIB) \
	    -Wl,--exclude-libs,ALL  \
	    -o $@ $(GABCOFILES) $(SAFLAGS) $(EXTRALIBS)

$(ALEMBIC_IN_SO):	$(ALEMBIC_IN_O) $(GABCLIB)
	$(CXX) -shared -o $@ $< $(SAFLAGS) $(GABCLIB) $(ABCHUGE)

$(ALEMBIC_GROUP_SO):	$(ALEMBIC_GROUP_O) $(GABCLIB)
	$(CXX) -shared -o $@ $< $(SAFLAGS) $(GABCLIB) $(ABCHUGE)

$(GU_ALEMBIC_SO):	$(GU_ALEMBIC_O) $(GABCLIB)
	$(CXX) -shared -o $@ $< $(SAFLAGS) $(GABCLIB) $(ABCHUGE)

clean:
	rm -f $(OFILES)

test:	$(GABCOFILES)
	@echo All objects compiled properly

rmtargets:
	rm -rf $(TARGETS)
