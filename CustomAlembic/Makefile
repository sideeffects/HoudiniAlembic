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

GABC_NAMESPACE = GABC_Custom
TOKEN_PREFIX = "c"
LABEL_PREFIX = "Custom "

OPTIMIZER = -g
include $(HFS)/toolkit/makefiles/Makefile.linux

ABCPATH = /usr/local/alembic-1.1.5
ABCINC = $(ABCPATH)/include
ABCDEF = \
    -DGABC_NAMESPACE=$(GABC_NAMESPACE) \
    -DGABC_PRIMITIVE_TOKEN='"CustomAlembicRef"' \
    -DGABC_PRIMITIVE_LABEL='"Custom Alembic Delayed Load"' \
    -DCUSTOM_ALEMBIC_TOKEN_PREFIX='$(TOKEN_PREFIX)' \
    -DCUSTOM_ALEMBIC_LABEL_PREFIX='$(LABEL_PREFIX)'
ABCHUGE = libs/libAbcHuge.so

# Extra file for half.h
HALFDIR = $(HFS)/toolkit/include/OpenEXR

HLIB = $(HFS)/dsolib
EXTRALIBS = \
	$(ABCHUGE) \
	$(HLIB)/libtbb.so \
	$(HLIB)/libIex.so

ABCFLAGS = $(ABCDEF) -I$(ABCINC) -I$(HALFDIR)

GABCCFILES = \
	HoudiniAlembic/GABC/GABC_ChannelCache.C \
	HoudiniAlembic/GABC/GABC_GEOPrim.C \
	HoudiniAlembic/GABC/GABC_GEOWalker.C \
	HoudiniAlembic/GABC/GABC_GTPrim.C \
	HoudiniAlembic/GABC/GABC_GUPrim.C \
	HoudiniAlembic/GABC/GABC_IArchive.C \
	HoudiniAlembic/GABC/GABC_IArray.C \
	HoudiniAlembic/GABC/GABC_IGTArray.C \
	HoudiniAlembic/GABC/GABC_IItem.C \
	HoudiniAlembic/GABC/GABC_IObject.C \
	HoudiniAlembic/GABC/GABC_OError.C \
	HoudiniAlembic/GABC/GABC_OGTGeometry.C \
	HoudiniAlembic/GABC/GABC_OOptions.C \
	HoudiniAlembic/GABC/GABC_OProperty.C \
	HoudiniAlembic/GABC/GABC_Types.C \
	HoudiniAlembic/GABC/GABC_Util.C \
	HoudiniAlembic/GABC/GABC_Visibility.C
GABCOFILES = $(GABCCFILES:.C=.o)
GABCLIB = libs/libCustomGABC.so

ALEMBIC_IN	= HoudiniAlembic/SOP_AlembicIn.C
ALEMBIC_IN_SO	= $(ALEMBIC_IN:.C=.so)
ALEMBIC_IN_O	= $(ALEMBIC_IN:.C=.o)

ALEMBIC_GROUP		= HoudiniAlembic/SOP_AlembicGroup.C
ALEMBIC_GROUP_SO	= $(ALEMBIC_GROUP:.C=.so)
ALEMBIC_GROUP_O		= $(ALEMBIC_GROUP:.C=.o)

GU_ALEMBIC	= HoudiniAlembic/GU_Alembic.C
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
	g++ $(ABCFLAGS) $(OBJFLAGS) -c -o $@ $<

all:	$(GABCLIB) $(PLUGINS:.C=.so)

install:	all

$(GABCLIB):	$(GABCOFILES)
	g++ -shared \
	    -Wl,-soname,$(GABCLIB) \
	    -Wl,--exclude-libs,ALL  \
	    -o $@ $(GABCOFILES) $(SAFLAGS) $(EXTRALIBS)

$(ALEMBIC_IN_SO):	$(ALEMBIC_IN_O) $(GABCLIB)
	g++ -shared -o $@ $< $(SAFLAGS) $(GABCLIB) $(ABCHUGE)

$(ALEMBIC_GROUP_SO):	$(ALEMBIC_GROUP_O) $(GABCLIB)
	g++ -shared -o $@ $< $(SAFLAGS) $(GABCLIB) $(ABCHUGE)

$(GU_ALEMBIC_SO):	$(GU_ALEMBIC_O) $(GABCLIB)
	g++ -shared -o $@ $< $(SAFLAGS) $(GABCLIB) $(ABCHUGE)

clean:
	rm -f $(OFILES)

untar:
	tar xvf $(HH)/public/Alembic/HoudiniAlembic.tgz

rmtargets:
	rm -rf $(TARGETS)

backup:
	python backup.py
