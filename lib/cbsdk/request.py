import json
from warnings import warn
import cbsdk.constants as _C
from cbsdk.msgbase import Message as MessageBase

def _verify_dict(allowed, d):
    for k in d.keys():
        if not k in allowed:
            emsg = "Invalid option '{0}'. Valid options are {1}".format(
                k, allowed
            )
            raise ProtocolError(emsg)



class Request(MessageBase):
    """
    This implements the base class for all requests. Requires the
    command and request id.
    """
    def __init__(self, cmd, reqid, hid):
        if reqid is None:
            raise ValueError("Must have request ID")
        jsondict = {
            "Command" : cmd,
            "ReqID" : reqid,
            "CommandData" : {},
            "Handle" : hid
        }
        
        super(Request, self).__init__(reqid, jsondict)
        
    def __repr__(self):
        hid = self.jsondict["Handle"]
        if not hid:
            hid = "Control"
        else:
            hid = str(hid)
            
        msg = "%s:%d @%s %s" % (
            self.jsondict["Command"],
            self.jsondict["ReqID"],
            hid,
            self.jsondict["CommandData"]
        )
        return msg
        
    __str__ = __repr__

                        
    
class CreateHandle(Request):
    """ Request a new handle to be created"""
    _allowed_options = set( [ 'Timeout', 'Username', 'Password' ] )
        
    def __init__(self, reqid, hid,
                 host, port, bucket,
                 **options):
        """
        Create a new-handle request.
        @param reqid the request ID
        @param hid the new handle ID
        @param host the hostname
        @param port the port
        @param bucket the bucket name
        @param options hash of additional options. Must exist in ALLOWED_HANDLE_OPTIONS
        """
        
        super(CreateHandle, self).__init__("NEWHANDLE", reqid, hid)
        
        jsondict = self.jsondict        
        jsondict["CommandData"] = {
            "Hostname" : host,
            "Port" : port,
            "Bucket" : bucket
        }
        
        if (options):
            _verify_dict(self._allowed_options, options)
            jsondict["CommandData"]["Options"] = options
        


class CreateDataset(Request):
    SOURCE_INLINE = "INLINE"
    
    
    def __init__(self, reqid, dsid, ds_spec,
                 source = None):
        
        super(CreateDataset, self).__init__('NEWDATASET', reqid, 0)
        self.jsondict["Handle"] = 0
        
        
        cmd_data = self.jsondict["CommandData"]
        cmd_data["Type"] = source
        cmd_data["ID"] = dsid
        
        if not source:
            source = self.SOURCE_INLINE
            
        if source == self.SOURCE_INLINE:
            try:
                cmd_data["Count"] = len(ds_spec)
                cmd_data["Items"] = ds_spec.copy()
            except Exception:
                raise ValueError("ds_spec is not valid")
            
        else:
            raise ValueError("Unsupported/Invalid source type")
                
            
class DSOperation(Request):
    _allowed_options = ("Detailed","DelayMsec", "Dataset")
    
    def __init__(self, reqid, hid, dsid,
                 subcmd = None,
                 inline_dataset = None,
                 **options):
        cmd = 'MC_DS_' + subcmd
        
        if not subcmd:
            raise ValueError("Must have subcommand!")
        
        super(DSOperation, self).__init__(cmd, reqid, hid)
        
        self.jsondict["CommandData"].update({
            "DSID" : dsid,
            "Options" : options
        })
        
        if inline_dataset:
            self.jsondict["CommandData"]["Options"].update({
                "Dataset" : inline_dataset
            })
            assert(not dsid)
        
        if len(options):
            _verify_dict(self._allowed_options, options)
        
        

class DSMutation(DSOperation):
    
    _allowed_options = DSOperation._allowed_options + \
        ("Expiry", "CAS")
    
    def __init__(self, reqid, hid, dsid,
                 op, **options):
        
        super(DSMutation, self).__init__(reqid, hid, dsid,
                                                 subcmd = op, **options)
        
    
class DSKeyOperation(DSOperation):
    _allowed_options = DSOperation._allowed_options + \
        ("Expiry", "CAS")
        
    def __init__(self, reqid, hid, dsid,
                 op, **options):
        super(DSKeyOperation, self).__init__(reqid, hid, dsid,
                                              subcmd = op, **options)
    
class DSRetrieval(DSOperation):    
    def __init__(self, reqid, hid, dsid,
                 **options):
        
        if options.has_key("inline_dataset"):
            options["inline_dataset"] = list(options["inline_dataset"])
            
        super(DSRetrieval, self).__init__(reqid, hid, dsid,
                                          subcmd = "GET", **options)
        