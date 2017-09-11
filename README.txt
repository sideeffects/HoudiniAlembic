This directory contains unpolished files which show how it's possible to use a
custom Alembic library with Houdini.  This has been minimally tested with
Linux.  Additional work would have to be done for other platforms.

- MakeHuge
    Makefile to make a "packed" Alembic library for use with the custom code.
    Since Linux will only load a .so file one time (based on the soname), you
    will need to create a different set of libraries (i.e. other than
    libAlembicAbc.so.

    Instructions are embedded in the file.

- Makefile
    Example of a makefile which will untar the source code shipped with Houdini
    and make custom versions using the alternate Alembic libraries (created
    with MakeHuge).

    You might have to fix library paths etc.

    Instructions are embedded in the file.

If the Alembic library is ABI compliant with the version of Houdini's
versions, you should be able to just replace the Alembic libraries shipped
with Houdini (or preload the alternate libraries).

In the complicated case where the Alembic library is *not* ABI compliant, you
will likely have to put the Alembic code in a separate namespace (if it's not
already).

There are at least two different strategies to customizing the Alembic code:

1)  Replace all Houdini .so files
    With this strategy, you would replace GU_Alembic.so, SOP_AlembicIn.so,
    etc. with your versions (removing them from the distribution, and using
    your custom versions).

    Provided your code is linked against a different Alembic library, you
    should be good to go -- with a small caveat.  We ship *almost* all of the
    Alembic code.  At the current time, there are one or two calls outside the
    shipped public code:
	- One call to GABC_Util::getObjectList() in the
	    "File->Import->Alembic Scene" code
	- gabc.C
    If any of these code-paths get invoked after you replace GU_Alembic or
    GABC, your mileage may vary.

2)  Run in parallel with Houdini
    With this strategy, the custom versions of plug-ins (GU_Alembic,
    SOP_AlembicIn, etc.) are given unique internal names.  This means that
    your code will be working with a different primitive type, while the
    Houdini primitive remains in place.

    The Makefile included here is set up to use this strategy.
