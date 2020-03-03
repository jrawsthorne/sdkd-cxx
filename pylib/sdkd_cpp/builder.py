#!/usr/bin/env python

import sys
import os.path

sys.path.append(os.path.dirname(__file__))
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import os
from subprocess import Popen, PIPE
import time
import json
import sdkd_cpp.lcbpkg as sdkd_build

class Builder(object):
    """
    This is a wrapper to execute the build process, either locally or remotely.
    """

    def __init__(self, version, dist_type, **kwargs):

        sdkd_cls = getattr(sdkd_build, dist_type)
        instance = sdkd_cls(version)

        instance.extract()
        instance.build()
        self.instance = instance
        self.debugger = None
        self.extra_args = None
        self.po = None
        self.generate_script()

    def generate_script(self):
        # Generate a shell script to execute. This normally wraps
        # LD_LIBRARY_PATH and other stuff
        fname = self.instance.get_sdkd_dest()
        fname += "launcher-" + str(id(self))

        fp = open(fname, "w")

        fp.write("#!/bin/sh\n")
        #fp.write("set -x\n")

        fp.write(
            "export LD_LIBRARY_PATH='{0}'\n".format(
                self.instance.get_lcb_libdir()))

        fp.write("exec '{0}' $@\n".format(
            self.instance.get_sdkd_dest()))

        os.chmod(fname, 0o755)

        fp.close()

        self.scriptname = fname

    def set_debugger(self, debugger):
        self.debugger = debugger

    def set_extra_args(self, extra):
        self.extra_args = extra_args

    def launch_sdkd(self, portfile_name = "portinfo.txt"):
        """
        Launches the sdkd.
        Returns the name of the portfile
        """
        cmd = [ self.scriptname ]

        cmd += ["infofile=" + portfile_name]

        if self.debugger:
            cmd += self.debugger.split(" ")

        if self.extra_args:
            cmd += self.extra_args.split(" ")

        self.po = Popen(cmd)
        return portfile_name

def read_json():
    line = sys.stdin.readline()
    line = line.strip()
    line = json.loads(line)
    return line

if __name__ == "__main__":
    sys.stderr.write("Waiting for commands on stdin\n")

    js = read_json()
    builder = Builder(*js['args'], **js['kwargs'])
    jsinfo = {}
    jsinfo["script"] = os.path.abspath(builder.scriptname)
    jsinfo["portfile"] = jsinfo["script"] + "-portfile"
    sys.stdout.write(json.dumps(jsinfo) + "\n")
