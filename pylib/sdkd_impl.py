import sys
import os
import os.path
import json
import re
from warnings import warn

sys.path.append(os.path.dirname(__file__))

from . import sdkd_cpp.lcbpkg as lcbpkg
from .sdkd_cpp.lcbpkg import Deb, VERSION_INFO
from .sdkd_cpp.builder import Builder
from cbsdk.driver import DriverInet
from cbsdk.sdkdfactory.driver2 import Driver2Config
from cbsdk.testconfig import ConfigOptionCollection, ConfigOption
from cbsdk.sdkdfactory.remote import Remote, RemoteExecutor
from cbsdk.sdkdfactory.executor import PortWaitExecutor


lcb_confcoll = ConfigOptionCollection('libcouchbase sdkd options', '', [

    ConfigOption('lcb_version', lcbpkg.DEFAULT_VERSION,
                 str, "libcouchbase version to use"),

    ConfigOption('lcb_dist_type', lcbpkg.DEFAULT_CLASS,
                 str, "Distribution type to use",
                 choices = ['Deb', 'Source', 'RPM']),

    ConfigOption('lcb_list_versions', False, bool, "List available versions"),

    ConfigOption('lcb_sdkd_debug', False, bool, "Verbose output for sdkd"),

    ConfigOption('lcb_remote', None, str, "Remote path to sdkd, i.e. "
                 "user@host:/path/to/sdkd-cpp"),
    ConfigOption('lcb_password', None, str, "SSH Password"),
    ConfigOption('lcb_force_build', False, bool, "Force rebuilding of sdkd")
])


class LcbRemote(Remote):
    def __init__(self, *args, **kwargs):
        self.lcb_version = kwargs.pop('lcb_version')
        self.lcb_dist_type = kwargs.pop('lcb_dist_type')

        super(LcbRemote, self).__init__(*args, **kwargs)

    def get_builder_name(self):
        return self.join_path(
            [ self.r_dir, "pylib", "sdkd_cpp", "builder.py" ]
        )

    def get_portfile(self):
        return self.get_build_output()["portfile"]

    def get_sdkd_name(self):
        bo = self.get_build_output()
        if not bo:
            raise Exception("Were we bootstrapped yet?")
        return bo["script"]

    def make_portfile_arg(self):
        return [ "infofile=" + self.get_portfile() ]

    def invoke_builder(self, *args):
        # Make our options:
        buildspec = {
            'args' : [
                self.lcb_version, self.lcb_dist_type ],

            'kwargs' : {}
        }
        return super(LcbRemote, self).invoke_builder(buildspec)

# Loader
class Config2(Driver2Config):
    _confcoll = lcb_confcoll
    def get_config_options(self):
        return self._confcoll

    def make_effective_instance(self):
        remote = self._confcoll['lcb_remote'].get()

        if remote:
            return RemoteConfig2(self._confcoll)
        else:
            return LocalConfig2(self._confcoll)

class LocalConfig2(Driver2Config):
    def __init__(self, confcoll):
        super(LocalConfig2, self).__init__()

        self._confcoll = confcoll
        self.builder = None
        self.sdkd_dest = None
        self._portfile = None

    def bootstrap(self):
        self.builder = Builder(
            self._confcoll['lcb_version'].get(),
            self._confcoll['lcb_dist_type'].get()
        )

        if self._confcoll['lcb_sdkd_debug'].get():
            self.cmdopts.append_arg_string("debug=1")

        self.sdkd_dest = self.builder.scriptname
        self._portfile = self.sdkd_dest + "-portfile.txt"

        self.cmdopts.append_arg_string("infofile=" + self._portfile)

        self.cmdopts.set_executable(self.sdkd_dest)

    def create_driver(self, **kwargs):
        """
        Builds a DriverInet with a CLIExecutor
        """
        executor = PortWaitExecutor(self.cmdopts, self._portfile)
        return DriverInet(executor, **kwargs)

class RemoteConfig2(Driver2Config):
    def __init__(self, confcoll):
        super(RemoteConfig2, self).__init__()

        self._confcoll = confcoll
        self.remote = None

    def bootstrap(self):
        # Builder proxy
        rstr = self._confcoll['lcb_remote'].get()
        rspec = Remote.parse_remote_spec(rstr)
        passwd = self._confcoll['lcb_password'].get()

        self.remote = LcbRemote(
            r_pass = passwd,
            r_mode = LcbRemote.MODE_UNIX,


            lcb_version = self._confcoll['lcb_version'].get(),
            lcb_dist_type = self._confcoll['lcb_dist_type'].get(),

            # Contains user password and dir
            **rspec
        )

        self.remote.invoke_builder()

        # Extract the arguments.
        if self._confcoll['lcb_sdkd_debug'].get():
            self.remote.cmdopts.append_arg_string("debug=1")

    def get_cmdopts(self):
        return self.remote.cmdopts

    def create_driver(self, **kwargs):
        """
        Returns a DriverInet with a RemoteExecutor
        """
        executor = RemoteExecutor(self.remote)
        return DriverInet(executor, **kwargs)
