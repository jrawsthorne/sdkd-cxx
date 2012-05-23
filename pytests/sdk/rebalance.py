from sdk.sdkbasetestcase import SDKBaseTestCase
import basetestcase

from cbsdk.dataset import DSSeed, DSInline
import cbsdk.driver
from cbsdk.constants import StatusCodes as _E
import cbsdk.constants as _C

from logger import Logger

class RebalanceBasic(SDKBaseTestCase):
    
    def setUp(self):
        self.log = Logger.get_logger()
        super(RebalanceBasic, self).setUp()
        self.handles = []
        
    """
    This test will have two concurrent handles, one functioning as a getter
    and one functioning as a setter during a rebalance.
    
    The setter must mainly return successes (the getter, not necessarily).
    Both handle operations must not report any network errors.
    """
    def getset_simple(self):
        self.log.info("Requesting to create new bucket..")
        server = self.servers[0]
        
        driver = self.getDriver()
        
        # create a handle..
        setter = driver.create_handle(**self.getBucketParams())
        self.log.info("Created setter handle")
        
        getter = driver.create_handle(**self.getBucketParams())
        self.log.info("Created getter handle")
        
        # Create a dataset..
        ds = DSSeed(1000,
                    kseed = 'SimpleKey_',
                    vseed = 'SimpleValue_',
                    ksize = 12,
                    vsize = 100,
                    repeat = '_REP_')
        
        self.log.debug("Pre-deleting all keys..")
        resp = setter.invoke_command(setter.ds_keyop(ds,
                                                     _C.KOP_DELETE),
                                     wait = True)
        self.assertTrue(resp.is_ok())
        
        
        task = None
        for i in range(self.num_servers)[1:]:
            task = self.cluster.async_rebalance(self.servers[:i],
                                                     [self.servers[i]],
                                                     [])
            break
        
        setreq = setter.ds_mutate(ds, DelayMsec = 10)
        getreq = getter.ds_retrieve(ds, DelayMsec = 10)
        
        for h, r in ([setter, setreq], [getter,getreq]):
            h.invoke_command(r, wait = False)
        
        
        # Now, wait for both commands to get a response..
        for h in (getter,setter):
            resp = h.conduit.recv_msg()
            
            self.assertFalse(
                resp.summary().error_count(subsys = _E.SUBSYSf_NETWORK),
                "Got no network errors")
            
            if h is setter:
                self.assertOkRatio(resp, ok_ratio = 9)
        
        task.result()
        
        [ self.addHandle(h) for h in (getter, setter) ]