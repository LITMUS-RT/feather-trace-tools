# #####################################################################
# User configuration: Nothing to configure in this version.

# #####################################################################
# Internal configuration.
DEBUG_FLAGS  = '-Wall -g -Wdeclaration-after-statement'
INCLUDE_DIRS = 'include/'
# #####################################################################
# Build configuration.
from os import uname

# sanity check
(os, _, _, _, arch) = uname()
if os != 'Linux':
    print 'Warning: Building ft_tools is only supported on Linux.'

if arch not in ('sparc64', 'i686'):
    print 'Warning: Building ft_tools is only supported on i686 and sparc64.'

env = Environment(
    CC = 'gcc',
    CPPPATH = Split(INCLUDE_DIRS),
    CCFLAGS = Split(DEBUG_FLAGS)
)

if arch == 'sparc64':
    # build 64 bit sparc v9 binaries
    v9 = Split('-mcpu=v9 -m64')
    env.Append(CCFLAGS = v9, LINKFLAGS = v9)

# #####################################################################
# Targets:
env.Program('ftcat', ['src/ftcat.c', 'src/timestamp.c'])
env.Program('ft2csv', ['src/ft2csv.c', 'src/timestamp.c', 'src/mapping.c'])
env.Program('ftdump', ['src/ftdump.c', 'src/timestamp.c', 'src/mapping.c'])
