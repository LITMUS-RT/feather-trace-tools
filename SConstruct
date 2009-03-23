# #####################################################################
# User configuration: Nothing to configure in this version.

# #####################################################################
# Internal configuration.
DEBUG_FLAGS  = '-Wall -g -Wdeclaration-after-statement'
INCLUDE_DIRS = 'include/'
X86_32_FLAGS = '-m32'
X86_64_FLAGS = '-m64'
V9_FLAGS     = '-mcpu=v9 -m64'
SUPPORTED_ARCHS = {
    'sparc64' : V9_FLAGS,
    'i686'    : X86_32_FLAGS,
    'i386'    : X86_32_FLAGS,
    'x86_64'  : X86_64_FLAGS,
}
# #####################################################################
# Build configuration.
from os import uname, environ

# sanity check
(os, _, _, _, arch) = uname()
if os != 'Linux':
    print 'Error: Building ft_tools is only supported on Linux.'
    Exit(1)

# override arch if ARCH is set in environment or command line
if 'ARCH' in ARGUMENTS:
    arch = ARGUMENTS['ARCH']
elif 'ARCH' in environ:
    arch = environ['ARCH']

if arch not in SUPPORTED_ARCHS:
    print 'Error: Building ft_tools is only supported for the following', \
        'architectures: %s.' % ', '.join(sorted(SUPPORTED_ARCHS))
    Exit(1)
else:
    print 'Building %s binaries.' % arch
    arch_flags = Split(SUPPORTED_ARCHS[arch])

env = Environment(
    CC = 'gcc',
    CPPPATH = Split(INCLUDE_DIRS),
    CCFLAGS = Split(DEBUG_FLAGS) + arch_flags,
    LINKFLAGS = arch_flags,
)

# #####################################################################
# Targets:
env.Program('ftcat', ['src/ftcat.c', 'src/timestamp.c'])
env.Program('ft2csv', ['src/ft2csv.c', 'src/timestamp.c', 'src/mapping.c'])
env.Program('ftdump', ['src/ftdump.c', 'src/timestamp.c', 'src/mapping.c'])
