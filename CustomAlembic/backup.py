#!/usr/bin/env python

## This document is under CC-3.0 Attribution-Share Alike 3.0
##       http://creativecommons.org/licenses/by-sa/3.0/
##  Attribution:  There is no requirement to attribute the author.

import os, time

BDIR = 'Backups'
FILES = [
    'Makefile',
    'MakeHuge',
    'backup.py',
]

if not os.path.isdir(BDIR):
    os.mkdir(BDIR)

path = os.path.join(BDIR, time.strftime('backup-%Y%m%d%H%M%S.tgz'))
cmd = 'tar czvf %s %s' % (path, ' '.join(FILES))
print cmd
os.system(cmd)
