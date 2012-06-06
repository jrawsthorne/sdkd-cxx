from sdk.sdkbasetestcase import SDKBaseTestCase
import basetestcase

from cbsdk.dataset import DSSeed, DSInline
import cbsdk.driver
from cbsdk.constants import StatusCodes as _E
import cbsdk.constants as _C

from couchbase.cluster import Cluster
from logger import Logger
from TestInput import TestInputSingleton
from membase.helper.bucket_helper import BucketOperationHelper
from membase.helper.cluster_helper import ClusterOperationHelper
import time

class RebalanceBasic(SDKBaseTestCase):
    
    def setUp(self):
        super(RebalanceBasic, self).setUp()
        self.resetCluster()        
        self.log.info("setUp complete")
        
    def tearDown(self):
        self.cluster.shutdown()
    """
    This test will have two concurrent handles, one functioning as a getter
    and one functioning as a setter during a rebalance.
    
    The setter must mainly return successes (the getter, not necessarily).
    Both handle operations must not report any network errors.
    
    nthreads is the amount of threads for both getters and setters, thus the
    effective number of threads is twice this number
    """
    def getset_simple(self, nthreads = 10, rmcount = 2):
        
        self.log.info("Requesting to create new bucket..")
                
        self.log.info("Rebalance complete")
        begin = time.time()
        
        # Set up a four node cluster
        self.setupCluster(4)
        
        duration = time.time() - begin
        self.log.info("Creating default bucket took %d seconds", duration)
        
        driver = self.getDriver()
        
        
        # Make our worker threads:
        setters = []
        getters = []
        for x in xrange(nthreads):
            for a in (setters, getters):
                a.append(driver.create_handle(**self.getBucketParams()))
        
        self.log.info("Created %d getter and setter threads", nthreads)
        
        # Create a dataset..
        ds = DSSeed(1000,
                    kseed = 'SimpleKey_',
                    vseed = 'SimpleValue_',
                    ksize = 12,
                    vsize = 100,
                    repeat = '_REP_')
        
        
        # Remove a node asynchronously (together with a rebalance)
        task = self.removeNodes(num_nodes = rmcount)
        
        # Now schedule the commands for the sdkd workers
        for h in getters:
            req = h.ds_retrieve(ds, DelayMax = 10, DelayMin = 1, IterWait = True)
            h.invoke_command(req, wait = False)
            
        for h in setters:
            req = h.ds_mutate(ds, DelayMax = 10, DelayMin = 1, IterWait = True)
            h.invoke_command(req, wait = False)            
            
        
        # Wait for the rebalance to complete
        task.result()
        
        # Rebalance is complete. The pending operations should now gracefully
        # complete with a CANCEL command
        for h in (getters + setters):
            h.cancel()
        
        # All the CANCEL messages have been sent. Now we retrieve the results
        # from all the workers
        for h in (getters + setters):
            resp = h.conduit.recv_msg()
            
            # We shouldn't be seeing any specific 'NETWORK' errors at all
            self.assertFalse(
                resp.summary().error_count(subsys = _E.SUBSYSf_NETWORK),
                "Got no network errors")
            
            # If this is a setter thread, then it shouldn't have that many total
            # errors. Getter threads may have ENOENT errors
            # TODO: implement an 'except' keyword argument which excludes
            # certain types of errors from actually counting as an error.
            if h in setters:
                self.assertOkRatio(resp, ok_ratio = 9)
        
        # Store these handles for later use..
        [ self.addHandle(h) for h in (getters + setters) ]