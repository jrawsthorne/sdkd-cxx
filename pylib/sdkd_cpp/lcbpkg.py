#!/usr/bin/env python
import sys
import os.path
import os
import imp
import shutil
import platform
import urlparse
import json
import warnings
import multiprocessing
from subprocess import Popen, PIPE

COMMON_LIB = os.path.dirname(__file__)
COMMON_CACHE = os.path.join(COMMON_LIB, "..", "pkgcache")
COMMON_CACHE = os.path.abspath(COMMON_CACHE)

SRC_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                        "..", "..", "src"))

MAKECMD = "make -s -j " + str(multiprocessing.cpu_count()) + " "

_versions_json = os.path.join(COMMON_LIB, 'versions.json')
_versions_json = open(_versions_json, "r")

VERSION_INFO = json.load(_versions_json)

def get_version_option_strings(self):
    strs = []
    for k,v in VERSION_INFO['debs'].items():
        strs.append(v)

    return strs


def download_if_empty(dst, src):
    do_download = False
    if not os.path.exists(dst):
        do_download = True

    else:
        if os.stat(dst)[6] == 0:
            os.unlink(dst)
            do_download = True

    if not do_download:
        return True

    rv = run_command("wget --progress=dot "
                     "-O {0} {1}".format(dst, src),

                     assert_ok = True)
    return True



_CMD_VERBOSE = os.environ.get('SDKD_BUILD_VERBOSE', False)

def run_command(cmd, assert_ok = False):
    if _CMD_VERBOSE:
        print "== RUN (START): " + cmd

    rv = os.system(cmd)

    if _CMD_VERBOSE:
        print "== RUN (EC={0})".format(rv)
        print ""

    if assert_ok and rv != 0:
        assert rv == 0, "Command failed to execute"

    return rv

class Common(object):
    git_base = 'git://github.com/mrtazz/json-cpp.git'


    @classmethod
    def get_jsoncpp_libdir(self):
        ret = os.path.join(COMMON_CACHE,
                           "json-cpp-lib-{0}".format(self.get_host_md5()))
        return ret

    @classmethod
    def get_jsoncpp_incdir(self):
        return os.path.join(COMMON_CACHE, "json-cpp", "include")


    def extract(self):
        jp = os.path.join(COMMON_CACHE, "json-cpp")
        if not os.path.exists(jp):
            run_command("git clone {0} {1}".format(self.git_base, jp))

        jplib = self.get_jsoncpp_libdir()

        if not os.path.exists(jplib):
            oldpwd = os.getcwd()

            os.mkdir(jplib)
            os.chdir(jp)
            mkcmd = MAKECMD + " -f makefiles/gcc/Makefile"
            run_command(mkcmd + " clean")
            run_command(mkcmd + " staticlibrary")

            src = os.path.join(jp, "lib/libjson-cpp.a")
            target = os.path.join(jplib, "libjson-cpp.a")
            shutil.copy(src, target)
            os.chdir(oldpwd)


    def get_lcb_libdir(self):
        raise NotImplementedError()

    def get_lcb_incdir(self):
        raise NotImplementedError()

    def get_sdkd_dest(self):
        raise NotImplementedError()

    def get_lcb_dso(self):
        raise NotImplementedError()

    def build_sdkd(self):
        # TODO, make this smarter about re-building targets
        sdkd_path = self.get_sdkd_dest()
        
        # For now, the following block is commented out as we want to build
        # for multiple versions, and with possible changes to the sdkd itself
        # we'd like to rebuild the sdkd itself.

        #if os.path.exists(sdkd_path):
        #    return

        run_command(MAKECMD + " -C {0} clean".format(SRC_PATH))
        mklines = [
            'JSONCPP_LFLAGS=' + self.get_jsoncpp_libdir() + "/libjson-cpp.a",
            'JSONCPP_CPPFLAGS=-I' + self.get_jsoncpp_incdir(),
            'LCB_CPPFLAGS=-I' + self.get_lcb_incdir(),
        ]

        lcb_lflags = 'LCB_LFLAGS="-L{0} '.format(self.get_lcb_libdir())
        lcb_lflags += ' -Wl,-rpath={0} '.format(self.get_lcb_libdir())
        lcb_lflags += '-lcouchbase"'

        mklines.append(lcb_lflags)

        cmd = MAKECMD + ' -C {0} '.format(SRC_PATH)
        cmd += " ".join(mklines)
        run_command(cmd)

        shutil.copy(os.path.join(SRC_PATH, "sdkd_lcb"), self.get_sdkd_dest())

        libutil = os.path.join(SRC_PATH, "libcbsdkd.so")

        if os.path.exists(libutil):
            shutil.copy(libutil, os.path.join(os.path.dirname(self.get_sdkd_dest()),
                                              "libcbsdkd.so"))

    @staticmethod
    def get_host_md5():
        po = Popen("gcc -v 2>&1 | md5sum -", stdout = PIPE, shell = True)
        stdout, stderr = po.communicate()
        md5, f = stdout.split()
        return md5

    @staticmethod
    def make_base_dir(tag):
        pkgcache = os.path.dirname(__file__)
        pkgcache = os.path.join(pkgcache, '..', 'pkgcache')
        pkgcache = os.path.abspath(pkgcache)

        tagstr = "{0}-{1}".format(tag, Common.get_host_md5())

        basedir = os.path.join(pkgcache, tagstr)

        if not os.path.exists(basedir):
            run_command("mkdir -p " + basedir)

        return basedir


class PkgCollectionAbstract(object):
    class Package(object):
        def __init__(self, s):
            self.name = s
            self.filename = None
            self.url = None

    def __init__(self, urlbase, strlist):
        self.urlbase = urlbase
        l = []

        arch = platform.uname()[4]
        if arch == 'x86_64':
            arch = self.ARCHSTR_64
        else:
            arch = self.ARCHSTR_32

        for pkg in strlist:
            pkg = pkg.format(arch = arch)
            l.append(pkg)

        self._l = l
        self.arch = arch

    def __iter__(self):
        return iter(self._l)

    def to_url(self, pkgname, **fmtextra):
        s = pkgname

        if not s.startswith('http'):
            s = self.urlbase + pkgname

        s = s.format(arch = self.arch, **fmtextra)
        return s

    def download(self, pkg, basedir):
        """
        Tries to download the package pkg into the directory basedir.
        Returns the local path of the package
        """
        pkgpath = os.path.join(basedir, os.path.basename(pkg))
        download_if_empty(pkgpath, self.to_url(pkg))
        return pkgpath

class Deb(Common):

    class DebPkgCollection(PkgCollectionAbstract):
        ARCHSTR_64 = 'amd64'
        ARCHSTR_32 = 'i386'

    versions = {}
    for k, v in VERSION_INFO['debian'].items():
        versions[k] = DebPkgCollection(v['urlbase'], v['debs'])

    def __init__(self, version):
        self.basedir = self.make_base_dir(version + "-deb")
        self.versions[version]
        self.curvers = version

    def extract(self):
        super(Deb, self).extract()

        pkgs = self.versions[self.curvers]
        inst_dir = os.path.join(self.basedir, "inst")

        if not os.path.exists(inst_dir):
            run_command("mkdir " + inst_dir)

        for pkg in pkgs:
            pkgpath = pkgs.download(pkg, self.basedir)
            run_command("dpkg -x {0} {1}".format(pkgpath, inst_dir))

        return self

    def get_lcb_libdir(self):
        return os.path.join(self.basedir, 'inst', 'usr', 'lib')

    def get_lcb_incdir(self):
        return os.path.join(self.basedir, 'inst', 'usr', 'include')

    def get_sdkd_dest(self):
        return os.path.join(self.basedir, 'sdkd')

    def build(self):
        self.build_sdkd()
        return self

class RPM(Common):
    class RpmPkgCollection(PkgCollectionAbstract):
        ARCHSTR_64 = 'x86_64'
        ARCHSTR_32 = 'i686'

        elversion = None

        @classmethod
        def detect_el_version(cls):
            """
            Call this to initialize the class
            """
            if cls.elversion:
                return

            el = platform.dist()[2]

            if el.startswith('5'):
                cls.elversion = '5.5'

            elif el.startswith('6'):
                cls.elversion = '6.2'

            else:
                warnings.warn("Can't detect EL version. Fallback to 5.5")
                cls.elversion = '5.5'

        def to_url(self, pkg):
            pkg = super(self.__class__, self).to_url(pkg, el = self.elversion)
            return pkg

    versions = {}
    for k, v in VERSION_INFO['redhat'].items():
        versions[k] = RpmPkgCollection(v['urlbase'], v['rpms'])

    def __init__(self, version):
        self.RpmPkgCollection.detect_el_version()
        self.basedir = self.make_base_dir(
            "{0}-rpm{1}".format(version, self.RpmPkgCollection.elversion))

        self.versions[version]
        self.curvers = version


    @property
    def _pkg_collection(self):
        return self.versions[self.curvers]

    def extract(self):
        super(RPM, self).extract()
        pkgs = self._pkg_collection


        inst_dir = os.path.join(self.basedir, "inst")
        if not os.path.exists(inst_dir):
            os.mkdir(inst_dir)

        for pkg in pkgs:
            pkgpath = pkgs.download(pkg, self.basedir)

            oldpwd = os.getcwd()

            # Now for the funky command string..
            cmd = "rpm2cpio < " + pkgpath
            cmd += "| cpio --extract --make-directories --unconditional --quiet"
            cmd += " --no-preserve-owner"

            os.chdir(inst_dir)
            run_command(cmd)
            os.chdir(oldpwd)

        return self

    @property
    def _usrpath(self):
        return os.path.join(self.basedir, 'inst', 'usr')

    def get_lcb_libdir(self):
        if self._pkg_collection.arch == self._pkg_collection.ARCHSTR_32:
            return os.path.join(self._usrpath, 'lib')
        else:
            return os.path.join(self._usrpath, 'lib64')

    def get_lcb_incdir(self):
        return os.path.join(self._usrpath, 'include')

    def get_sdkd_dest(self):
        return os.path.join(self.basedir, 'sdkd')


    def build(self):
        return self.build_sdkd()


def _is_archive(name):
    suffixes = ['.gz', '.bz2', '.tar']
    for suffix in suffixes:
        if name.endswith(suffix):
            return True

    return False

class Source(Common):
    def __init__(self, srcpath, configure_args = [], force_rebuild = False):
        self.srcpath = srcpath
        self.configure_args = configure_args
        self.configure_args.append("--enable-debug")

    def _handle_non_dir(self):
        tarball = None

        if os.path.basename(self.srcpath) == self.srcpath:
            # Version string, old-style linking
            self.srcpath = "{0}libcouchbase-{1}.tar.gz".format(
                VERSION_INFO['tarball']['urlbase'], self.srcpath)

        if (self.srcpath.lower().startswith("http")):
            url = urlparse.urlparse(self.srcpath)
            path = url[2]

            tarball = os.path.basename(path)
            tarball = os.path.join(COMMON_CACHE, tarball)
            download_if_empty(tarball, self.srcpath)

        elif _is_archive(self.srcpath):
            tarball = self.srcpath

        if not tarball:
            return

        # Extract the tarball:
        basepath = os.path.splitext(tarball)[0]
        basepath = os.path.basename(basepath)
        basepath = "{0}-tarball-{1}".format(basepath, self.get_host_md5())
        basepath = os.path.join(COMMON_CACHE, basepath)
        if not os.path.exists(basepath):
            os.mkdir(basepath)
            cmd = "tar xf {0} --strip-components=1 -C {1}".format(
                tarball, basepath)
            print cmd
            run_command(cmd)

        self.srcpath = basepath

    def extract(self):
        super(Source, self).extract()
        self._handle_non_dir()
        self._inst = os.path.join(self.srcpath, 'INST')
        return self

    def get_sdkd_dest(self):
        return os.path.join(self.srcpath, 'sdkd')

    def get_lcb_libdir(self):
        return os.path.join(self._inst, 'lib')

    def get_lcb_incdir(self):
        return os.path.join(self._inst, 'include')

    def build(self):
        if os.path.exists(self._inst):
            return self.build_sdkd()

        oldpwd = os.getcwd()
        os.chdir(self.srcpath)
        cmd = " ".join( ["./configure "] + self.configure_args)
        cmd += " --prefix={0}".format(self._inst)

        rv = run_command(cmd)
        if rv != 0:
            raise Exception("Configure failed")

        rv = run_command(MAKECMD + " install")
        if rv != 0:
            raise Exception("Build failed")

        os.chdir(oldpwd)

        return self.build_sdkd()

DEFAULT_CLASS = None
DEFAULT_VERSION = '2.0.0-beta2'

dist = platform.dist()
if dist[0] in ('debian', 'ubuntu'):
    DEFAULT_CLASS = 'Deb'

elif dist[0] == 'redhat':
    DEFAULT_CLASS = 'RPM'

else:
    DEFAULT_CLASS = 'Source'
    DEFAULT_VERSION = '2.0.0-beta2'

if __name__ == "__main__":
    # Test the classes, if possible..

    if len(sys.argv) > 1:
        cls = sys.argv[1]
        version = sys.argv[2]
        globals()[cls](version).extract().build()
        sys.exit(0)


    Deb('1.0.4').extract().build()
    Deb('2.0.0-beta2').extract().build()
    RPM('1.0.4').extract().build()
    RPM('2.0.0-beta2').extract().build()

    Source('1.0.4').extract().build()
    Source('1.0.5').extract().build()
    Source('1.0.6').extract().build()
    #Source('1.0.7').extract().build()

    Source('2.0.0beta2').extract().build()
