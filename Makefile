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

ABCINC = /usr/local/alembic-1.7.11/include
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

GABCLIB_SONAME = libCustomGABC.so
GABCLIB = libs/$(GABCLIB_SONAME)
GABCLIB_C = \
	src/GABC/GABC_Error.C \
	src/GABC/GABC_GEOWalker.C \
	src/GABC/GABC_IArchive.C \
	src/GABC/GABC_IArray.C \
	src/GABC/GABC_IGTArray.C \
	src/GABC/GABC_IItem.C \
	src/GABC/GABC_IObject.C \
	src/GABC/GABC_LayerOptions.C \
	src/GABC/GABC_OArrayProperty.C \
	src/GABC/GABC_OGTGeometry.C \
	src/GABC/GABC_OOptions.C \
	src/GABC/GABC_OScalarProperty.C \
	src/GABC/GABC_PackedGT.C \
	src/GABC/GABC_PackedImpl.C \
	src/GABC/GABC_Types.C \
	src/GABC/GABC_Util.C
GABCLIB_O = $(GABCLIB_C:.C=.o)

ALEMBIC_IN_SO	= src/SOP/SOP_AlembicIn.so
ALEMBIC_IN_C	= $(ALEMBIC_IN_SO:.so=.C)
ALEMBIC_IN_O	= $(ALEMBIC_IN_C:.C=.o)

ALEMBIC_GROUP_SO	= src/SOP/SOP_AlembicGroup.so
ALEMBIC_GROUP_C		= $(ALEMBIC_GROUP_SO:.so=.C)
ALEMBIC_GROUP_O		= $(ALEMBIC_GROUP_C:.C=.o)

ALEMBIC_PRIMITIVE_SO	= src/SOP/SOP_AlembicPrimitive.so
ALEMBIC_PRIMITIVE_C	= $(ALEMBIC_PRIMITIVE_SO:.so=.C)
ALEMBIC_PRIMITIVE_O	= $(ALEMBIC_PRIMITIVE_C:.C=.o)

ALEMBIC_OUT_SO	= src/ROP/ROP_AlembicOut.so
ALEMBIC_OUT_C	= \
	src/ROP/ROP_AbcArchive.C \
	src/ROP/ROP_AbcHierarchy.C \
	src/ROP/ROP_AbcHierarchySample.C \
	src/ROP/ROP_AbcNode.C \
	src/ROP/ROP_AbcNodeCamera.C \
	src/ROP/ROP_AbcNodeInstance.C \
	src/ROP/ROP_AbcNodeShape.C \
	src/ROP/ROP_AbcNodeXform.C \
	src/ROP/ROP_AbcRefiner.C \
	src/ROP/ROP_AbcUserProperties.C \
	$(ALEMBIC_OUT_SO:.so=.C)
ALEMBIC_OUT_O = $(ALEMBIC_OUT_C:.C=.o)

GU_ALEMBIC_SO	= src/GU/GU_Alembic.so
GU_ALEMBIC_C	= $(GU_ALEMBIC_SO:.so=.C)
GU_ALEMBIC_O	= $(GU_ALEMBIC_C:.C=.o)

TARGETS = \
	$(GABCLIB) \
	$(ALEMBIC_IN_SO) \
	$(ALEMBIC_GROUP_SO) \
	$(ALEMBIC_PRIMITIVE_SO) \
	$(ALEMBIC_OUT_SO) \
	$(GU_ALEMBIC_SO)

OFILES	= \
	$(GABCLIB_O) \
	$(ALEMBIC_IN_O) \
	$(ALEMBIC_GROUP_O) \
	$(ALEMBIC_PRIMITIVE_O) \
	$(ALEMBIC_OUT_O) \
	$(GU_ALEMBIC_O)

%.o:	%.C
	$(CXX) -DCXX11_ENABLED=1 $(ABCFLAGS) $(OBJFLAGS) -c -o $@ $<

all:	$(TARGETS)

install:	all

$(GABCLIB):	$(GABCLIB_O)
	mkdir -p libs
	$(CXX) -shared \
	    -Wl,-soname,$(GABCLIB_SONAME) \
	    -Wl,--exclude-libs,ALL  \
	    -o $@ $(GABCLIB_O) $(SAFLAGS) $(EXTRALIBS)

$(ALEMBIC_IN_SO):	$(ALEMBIC_IN_O) $(GABCLIB)
	$(CXX) -shared -o $@ $(ALEMBIC_IN_O) $(SAFLAGS) -Llibs -lCustomGABC

$(ALEMBIC_GROUP_SO):	$(ALEMBIC_GROUP_O) $(GABCLIB)
	$(CXX) -shared -o $@ $(ALEMBIC_GROUP_O) $(SAFLAGS) -Llibs -lCustomGABC

$(ALEMBIC_PRIMITIVE_SO):	$(ALEMBIC_PRIMITIVE_O) $(GABCLIB)
	$(CXX) -shared -o $@ $(ALEMBIC_PRIMITIVE_O) $(SAFLAGS) -Llibs -lCustomGABC

$(ALEMBIC_OUT_SO):	$(ALEMBIC_OUT_O) $(GABCLIB)
	$(CXX) -shared -o $@ $(ALEMBIC_OUT_O) $(SAFLAGS) -Llibs -lCustomGABC

$(GU_ALEMBIC_SO):	$(GU_ALEMBIC_O) $(GABCLIB)
	$(CXX) -shared -o $@ $(GU_ALEMBIC_O) $(SAFLAGS) -Llibs -lCustomGABC

clean:
	rm -f $(OFILES) $(TARGETS)

test:	$(GABCLIB_O)
	@echo All objects compiled properly

rmtargets:
	rm -rf $(TARGETS)
