# This package exists simply to bootstrap other dependendies from testrunner
import sys
import os.path

"""
Bootstraps testrunner libraries.
"""
def bootstrapTestrunnerLibs(tr_base, relative_to = None):
    if not relative_to:
        relative_to = os.path.curdir
    
    # Now, import the libraries we need for testrunner
    extpaths = ['lib', 'pytests']
    extpaths += [ os.path.join(tr_base, p)
                 for p in ('.', 'lib', 'pytests', 'pytests/performance')
                 ]
    
    if (relative_to !='/' and not tr_base.startswith('/')):
        extpaths = map(
            lambda x: os.path.abspath(os.path.join(relative_to, x)),
            extpaths)
    
    sys.path += extpaths
