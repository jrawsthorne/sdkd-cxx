import logger
import basetestcase
from cbsdk.constants import StatusCodes as _E

class SDKBaseTestCase(basetestcase.BaseTestCase):
    """
    Base class for SDK tests
    """
    
    #  Set this class attribute from the application entry point.
    _driver_factory = None
    
    @classmethod
    def setDriverFactory(cls, factory):
        if cls._driver_factory:
            raise Exception("Driver factory already exists")
        
        cls._driver_factory = factory
    
    @classmethod
    def getDriverFactory(cls):
        if not cls._driver_factory:
            raise Exception("Factory has not been set")
        return cls._driver_factory
    
    def getDriver(self):
        """
        Create a new driver, or get an existing one
        """
        try:
            return getattr(self, "_current_driver")
        except AttributeError:
            self._current_driver = self.getDriverFactory().create_driver()
            return self.getDriver()
            
    def __del__(self):
        """
        Cleanup any driver handle by gracefully sending a GOODBYE
        """
        if not self._current_driver:
            return
        
        self._current_driver.close()
        self._current_driver = None

    
    def setUp(self):
        super(SDKBaseTestCase, self).setUp()
        self.handles = []
        
    def tearDown(self):
        super(SDKBaseTestCase, self).tearDown()
        for h in self.handles:
            self._current_driver.destroy_handle(h)
        
        
        
    def addHandle(self, handle):
        self.handles.append(handle)
    
    def allHandles(self):
        return list(self.handles)
        
        
    def getBucketParams(self,
                        bucket = 'default',
                        username = '',
                        password = ''):
        """
        Return a dictionary suitable for passing to a NEWHANDLE request
        """
        
        server = self.servers[0]
            
        confhash = {
            'bucket' : bucket,
            'username' : username,
            'password' : password,
            'host' : server.ip,
            #'port' : server.port,
        }
        
        logger.Logger.get_logger().debug(str(confhash))
        
        return confhash
    
            
            
    def assertOkRatio(self,
                      resp,
                      ok_ratio = 9,
                      info = "OK:Err Ratio",
                      subsys = None):
        """
        Ensures that we have no more than a given ratio of errors
        of any kind
        """
        
        self.assertTrue(resp.is_ok())
        summary = resp.summary()
        ok_count = summary.ok_count()
        
        err_count = summary.error_count(subsys = subsys)
        
        if not err_count:
            self.assertTrue(True, info)
            return
        
        if not ok_count:
            self.assertTrue(False, info)
        
        ratio = (float(ok_count) / float(err_count))
        
        self.assertTrue(ratio > ok_ratio, info)
    