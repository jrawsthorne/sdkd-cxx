import logger
from logger import Logger
import basetestcase
from cbsdk.constants import StatusCodes as _E
import unittest

from TestInput import TestInputSingleton
from membase.helper.bucket_helper import BucketOperationHelper as BOP
from membase.helper.cluster_helper import ClusterOperationHelper as COP
from membase.api.rest_client import RestConnection

from couchbase.cluster import Cluster


class SDKBaseTestCase(unittest.TestCase):
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
        if not self._current_driver:
            self._current_driver = self.getDriverFactory().create_driver()
        
        return self._current_driver
            
    def __del__(self):
        """
        Cleanup any driver handle by gracefully sending a GOODBYE
        """
        if not self._current_driver:
            return
        
        self._current_driver.close()
        self._current_driver = None
        
    
    
    def resetCluster(self):
        for s in self.servers:
            try:
                r = RestConnection(s)
                r.stop_rebalance()
                self.log.debug("Successfully stopped rebalance on " +
                               str(s))
                
            except Exception as e:
                self.log.warn("Got error while stopping rebalance: "
                              + str(e))
        
        BOP.delete_all_buckets_or_assert(self.servers, self)
        COP.cleanup_cluster(self.servers)
        COP.wait_for_ns_servers_or_assert(self.servers, self)
        
    def setupCluster(self, num_nodes, bucket_size = 512):
        to_add = self._nodes_free[0:num_nodes]
        
        known = self._nodes_joined[::]
        if not len(known):
            known = [ to_add[0] ]
            to_add.pop(0)
        to_remove = []
        
        self.cluster.rebalance(known, to_add, [])
        # Nodes free are the ones we didn't join yet, if any
        self._nodes_free = self._nodes_free[num_nodes:]
        self._nodes_joined = known + to_add
        self.cluster.create_default_bucket(known[0], bucket_size)
        
    def removeNodes(self, num_nodes = 1):
        
        """
        Asynchronously removes nodes from the cluster and rebalances
        it.
        @param num_nodes How many nodes to remove
        @return A future (task.future)
        """
        
        to_remove = []
        
        for x in xrange(0, num_nodes):
            to_remove.append(self._nodes_joined.pop())
            
        self._nodes_free += to_remove
        
        self.log.info("Removing nodes " + str(to_remove))
        self.log.info("Remaining: " + str(self._nodes_joined))
        
        f = self.cluster.async_rebalance(self._nodes_joined,
                                         [],
                                         to_remove)
        return f
    
    def addNodes(self, num_nodes):
        """
        Asynchronously adds nodes to the cluster and triggers a rebalance.
        @param num_nodes the amount of nodes to add
        @return a future (task.future)
        """
        
        available = self._nodes_free[::]
        to_add = []
        
        for x in xrange(0, num_nodes):
            newnode = self._nodes_free.pop()
            to_add.append(newnode)
            self._nodes_joined.append(newnode)
        
        f = self.cluster.async_rebalance(self._nodes_joined,
                                         to_add,
                                         [])
        return f
    
    
    def setUp(self):
        super(SDKBaseTestCase, self).setUp()
        self.log = Logger.get_logger()
        
        self.handles = []
        self._current_driver = None
        self._nodes_joined = []
        self._nodes_free = []
        
        self.input = TestInputSingleton.input
        self.servers = self.input.servers
        self._nodes_free = self.servers[::]
        self.cluster = Cluster()
        
        
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
    