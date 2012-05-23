import ConfigParser
import cbsdk.driver


def _force_ve(a):
    if isinstance(a, str):
        a = a.split(' ')
    return a


class DriverFactory(object):
    """
    Simple factory which generates a new driver based on a configuration
    file
    """
    
    def __init__(self, config,
                 section = None,
                 extra_args = None,
                 debugger = None):
        
        p = ConfigParser.RawConfigParser()
        p.read(config)
        
        if not section:
            global_conf = dict(p.items("global"))
            section = global_conf["default_config"]
        
        driver_config = dict(p.items(section))
        
        # Determine the command line
        
        cmdline = eval(driver_config["exe"])
        
        if debugger:
            cmdline =  _force_ve(debugger) + cmdline
            
        if extra_args:
            cmdline += _force_ve(extra_args)
        
        
        self.cmdline = cmdline
        
        driverclass = getattr(cbsdk.driver, driver_config["driver"])
        driveropts = eval(driver_config["driver_options"])
        
        self.driverclass = driverclass
        self.driveropts = driveropts
        
    def create_driver(self, **extra_args):
        """
        Create a new instance of the defined driver. If you want
        to pass more arguments, pass them here.
        Returns a Driver object
        """
        dargs = {}
        dargs.update(self.driveropts)
        dargs.update(extra_args)
        
        driver = self.driverclass(self.cmdline,**dargs)
        return driver
    
    