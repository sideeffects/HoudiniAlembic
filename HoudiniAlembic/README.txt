This is Alembic source code used to compile this version of Houdini.

- GABC/
    This is the base Alembic library used by Houdini.  This is where Alembic
    Delayed Load primitives are defined.

    When Houdini writes to an .abc file, it needs to ensure there are no
    objects referenced.  Since Alembic Delayed Load primitives hold onto
    Alembic objects, the GABC library wraps the IObject, IArchive and
    IArraySamplePtr objects so that they can be "purged" before creating a
    file.

- OPalembic/
    This is the source to the "Alembic Archive" and "Alembic Xform" OTLs.

- GU_Alembic.C
    This is the primitive .so file which installs the GABC_GUPrim primitive
    into Houdini.

- HOM_Alemibic.C
    This contains Python extentsion for HOM

- ROP_*
    These files are used to build the Alembic output driver

- SOP_AlembicIn.[Ch]
    These files are used to build the Alembic SOP

- VRAY_ProcAlembic.[Ch]
    These files are the source to the Alembic procedural for mantra.
