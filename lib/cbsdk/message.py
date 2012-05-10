"""
Currently this module contains the class hierarchy for various messages.
In general, there are currently requests and response types.

Requests contain the following information:

- Command: The actual command to perform

- ReqID: A unique integer used to tag this command

- Handle: The handle at which this request is directed (in the future there may
    be a 'super' handle ID (maybe 0) which will designate control messages for
    all handles)

- CommandData: Command-specific data, or arguments to the command itself.

A request may receive one or more responses.
Each response has the following fields:

- Command: The command type for which this is a response, this is effectively
    the response's type

- ReqID: The unique integer identifying this transaction

- Handle: The handle ID from which this response originates

- Status: boolean indicating whether this response was OK

- Phase: Can either be 'COMPLETE', 'IN_PROGRESS', or something else identifying
    at what stage this response is. A 'COMPLETE' response means the sdkd has purged
    the ReqID from its database.
    
- ResponseData: Response-specific data or payload
"""

import json
from warnings import warn
try:
    from . import constants as _C
except Exception as e:
    warn("Errors likely ahead (ignore this if just verifying.." + str(e))

## Private utilities

class _Resphandlers(object):
    def __init__(self):
        self.handlers = {}
    def register_handler(self, command, handler):
        if self.handlers.has_key(command):
            raise Exception("A handler is already registered for " + command)
        self.handlers[command] = handler
    
    def get_handler(self, command):
        return self.handlers[command]

Response_Handlers = _Resphandlers()
    

def _verify_dict(allowed, d):
    for k in d.keys():
        if not k in allowed:
            emsg = "Invalid option '{0}'. Valid options are {1}".format(
                k, allowed
            )
            raise SDKDriverMessageException(emsg)


class SDKDriverMessageException(Exception): pass
class SDKDriverInvalidArgument(Exception): pass


# Some global constants:

# Mutation operations



class SDKDriverMessage(object):
    """
    Base class for all messages. This just requires and encodes the basic
    JSON implementation of the protocol
    """
    def __init__(self, reqid, jsondict):
        """ Requires the request ID and a dict of other options """
        assert isinstance(jsondict, dict)
        self.jsondict = jsondict
        self.request_id = reqid
        
    def encode(self):
        return json.dumps(self.jsondict,
                          check_circular = False,
                          encoding = 'ascii')
    
    
            
    
class SDKDriverRequest(SDKDriverMessage):
    """
    This implements the base class for all requests. Requires the
    command and request id.
    """
    def __init__(self, cmd, reqid):
        if reqid is None:
            raise SDKDriverInvalidArgument("Must have request ID")
        jsondict = {
            "Command" : cmd,
            "ReqID" : reqid,
            "CommandData" : {}
        }
        
        super(SDKDriverRequest, self).__init__(reqid, jsondict)
                
class SDKHandleRequest(SDKDriverRequest):
    """
    This adds the required 'handle id' argument. Also implements a pretty-print
    __repr__
    """
    def __init__(self, cmd, reqid, hid):
        super(SDKHandleRequest, self).__init__(cmd, reqid)
        self.jsondict["Handle"] = hid
    
    
    def __repr__(self):
        msg = "%s:%d @%d %s" % (
            self.jsondict["Command"],
            self.jsondict["ReqID"],
            self.jsondict["Handle"],
            self.jsondict["CommandData"]
        )
        return msg
        
    __str__ = __repr__
        
    
class SDKCreateHandle(SDKHandleRequest):
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
        
        super(SDKCreateHandle, self).__init__("NEWHANDLE", reqid, hid)
        
        jsondict = self.jsondict        
        jsondict["CommandData"] = {
            "Hostname" : host,
            "Port" : port,
            "Bucket" : bucket
        }
        
        if (options):
            _verify_dict(self._allowed_options, options)
            jsondict["CommandData"]["Options"] = options
        


class SDKCreateDataset(SDKDriverRequest):
    SOURCE_INLINE = "INLINE"
    
    
    def __init__(self, reqid, dsid, ds_spec,
                 source = None):
        
        super(SDKCreateDataset, self).__init__('NEWDATASET', reqid)
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
                raise SDKDriverInvalidArgument("ds_spec is not valid")
            
        else:
            raise SDKDriverInvalidArgument("Unsupported/Invalid source type")
                
            
class SDKDatasetOperation(SDKHandleRequest):
    _allowed_options = ("Detailed","DelayMsec", "Dataset")
    
    def __init__(self, reqid, hid, dsid,
                 subcmd = None,
                 inline_dataset = None,
                 **options):
        cmd = 'MC_DS_' + subcmd
        
        if not subcmd:
            raise SDKDriverInvalidArgument("Must have subcommand!")
        
        super(SDKDatasetOperation, self).__init__(cmd, reqid, hid)
        
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
        
        

class SDKDatasetMutation(SDKDatasetOperation):
    
    _allowed_options = SDKDatasetOperation._allowed_options + \
        ("Expiry", "CAS")
    
    def __init__(self, reqid, hid, dsid,
                 op, **options):
        
        super(SDKDatasetMutation, self).__init__(reqid, hid, dsid,
                                                 subcmd = op, **options)
        
    
class SDKDatasetKeyOp(SDKDatasetOperation):
    _allowed_options = SDKDatasetOperation._allowed_options + \
        ("Expiry", "CAS")
        
    def __init__(self, reqid, hid, dsid,
                 op, **options):
        super(SDKDatasetKeyOp, self).__init__(reqid, hid, dsid,
                                              subcmd = op, **options)
    
class SDKDatasetRetrieve(SDKDatasetOperation):    
    def __init__(self, reqid, hid, dsid,
                 **options):
        
        if options.has_key("inline_dataset"):
            options["inline_dataset"] = list(options["inline_dataset"])
            
        super(SDKDatasetRetrieve, self).__init__(reqid, hid, dsid,
                                                 subcmd = "GET", **options)
    
    
### Responses ####

class SDKDriverStatus(_C.StatusCodes):
    """
    This class implements the status class for command responses.
    A response status is always 0 if successful. Otherwise, the status
    code is comprised of two components. The first is a subsystem component,
    which represents the subsystem which raised the error. The second component
    is the actual error, whose value an interpretation depends on the subsystem
    """
    # Further bits depend on the underlying subsystem..
    def __init__(self, code, errstr = None):
        code = int(code)
        self.raw = code
        self.subsys = code & 0xff
        self.subsys_err = code >> 8
        
        self.errstr = errstr
        
        if self.subsys and (not self.subsys in self.error_types.values()):
            raise SDKDriverMessageException("Unknown subsystem: "
                                            + str(self.subsys))
    
    def is_subsys(self, subsys):
        return self.subsys & subsys
    
    def __repr__(self):
        # First, figure out what the subsystems are..
        
        
        if self.is_ok():
            return "OK"

        msg = "%d SUBSYS[" % (self.raw)
        subsys_list = []
        
        for k, v in self.error_types.items():
            if self.subsys & v:
                subsys_list.append(k)
        
        msg += str.join(",", subsys_list) + "] CODE: " + str(self.subsys_err)
        
        if self.errstr:
            msg += " (%s)" % (self.errstr)
        
        return msg
    
    def is_ok(self):
        return self.subsys == 0 and self.subsys_err == 0


class SDKDriverResponse(SDKDriverMessage):
    """
    Base class for responses. Like the request base class, a request ID
    is required here
    """
    def __init__(self, reqid, data):
        super(SDKDriverResponse, self).__init__(reqid, data)
        self.status = SDKDriverStatus(self.jsondict["Status"])
        if (self.jsondict.has_key("ErrorString")):
            self.status.errstr = self.jsondict["ErrorString"]
    
    def is_ok(self):
        return self.status.is_ok()
        
    def __repr__(self):
        msg = "{cmd}:{rid} {status}".format(cmd = self.jsondict["Command"],
                                       status = self.status,
                                       rid = self.request_id)
        return msg
    
    
    __nonzero__ = is_ok
    
    @classmethod
    def parse(cls, txt):
        """
        Parse a response from an encoded JSON string
        """
        jsondict = None
        
        try:
            jsondict = json.loads(txt)
            
        except ValueError as e:
            raise SDKDriverMessageException("Couldn't decode {0} ({1}) ".format(
                txt, e))
            
        if not jsondict.has_key("Status"):
            raise SDKDriverMessageException("Expected Status but found none")
        if not jsondict.has_key("Command"):
            raise SDKDriverMessageException("Expected command, but found none")
        
        subcls = Response_Handlers.get_handler(jsondict["Command"])
        return subcls(jsondict["ReqID"], jsondict)
        

class SDKHandleResponse(SDKDriverResponse):
    """
    Subclass for handle-related responses..
    """
    def __init__(self, reqid, data):
        super(SDKHandleResponse, self).__init__(reqid, data)
        self.handle_id = self.jsondict["Handle"]
        
    def __repr__(self):        
        return "{cmd}:{reqid} @{hid} ({status}) {payload}".format(
            cmd = self.jsondict["Command"],
            reqid = self.request_id,
            hid = self.handle_id,
            status = self.status,
            payload = self.jsondict["ResponseData"]
        )
    

class SDKDriverHandleControlResponse(SDKHandleResponse):
    """
    This contains various control responses from driver messages.
    These are smaller responses and always have a success or failure.
    """
    
    def __init__(self, reqid, data):
        super(SDKDriverHandleControlResponse, self).__init__(reqid, data)
        
        
class SDKDatasetOperationResponse(SDKHandleResponse):
    
    class _DetailItem(SDKDriverStatus):
        """
        Miniature implementation of Couchbase::Client::Return
        """
        def __init__(self, status, value = None):
            super(self.__class__, self).__init__(status)
            self.value = value
            
        def __str__(self):
            return self.value
        
        def __cmp__(self, other):
            return cmp(str(self), str(other))
        
        def get_status(self):
            return super(DetailItem, self)
        
        
    class _DetailHash(dict):
        """
        Object which behaves like a list and does on-demand conversion
        from a value to its status, when and if requested
        """
        
        def __init__(self, details = {}):
            super(self.__class__, self).__init__()
            self.update(details)
            
        def __getitem__(self, key):
            item = super(self.__class__, self).__getitem__(key)
            
            if not isinstance(item,
                              SDKDatasetOperationResponse._DetailItem):
                status = item[0]
                value = item[1] if len(item) == 2 else None
                item = SDKDatasetOperationResponse._DetailItem(
                    status, value = value)
                self[key] = item
                
            return item
    
    # Main class init
    
    def __init__(self, reqid, data):
        super(SDKDatasetOperationResponse, self).__init__(reqid, data)
        self._details = self._DetailHash()
        
        if not self.is_ok():
            self._summary = {}
            return
        
        rdata = self.jsondict["ResponseData"]
        if rdata.has_key("Details"):
            self._details.update(rdata["Details"])
        
        self._summary = rdata["Summary"]
        
    def summary(self):
        return self._summary
        
    def value(self):
        if len(self._details) > 1:
            warn("Requesting 'Value' on multi-result response")
        if not len(self._details):
            return None
        
        return self._details[self._details.keys()[0]]
        
Response_Handlers.register_handler("NEWHANDLE", SDKDriverHandleControlResponse)
Response_Handlers.register_handler("NEWDATASET", SDKDriverResponse)
[
    Response_Handlers.register_handler("MC_DS_" + subop,
                                       SDKDatasetOperationResponse)
    for subop in ('MUTATE_SET', 'MUTATE_APPEND', 'MUTATE_PREPEND',
                  'MUTATE_REPLACE', 'MUTATE_ADD', 'GET', 'DELETE', 'TOUCH')
]