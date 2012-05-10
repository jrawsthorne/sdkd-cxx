import cbsdk.constants as _C
from cbsdk.msgbase import Message as MessageBase
from cbsdk.msgbase import ProtocolError
import json
from warnings import warn

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


class Status(_C.StatusCodes):
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
            raise ProtocolError("Unknown subsystem: "
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


class Response(MessageBase):
    """
    Base class for responses. Like the request base class, a request ID
    is required here
    """
    def __init__(self, reqid, data):
        super(Response, self).__init__(reqid, data)
        self.status = Status(self.jsondict["Status"])
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
            raise ProtocolError("Couldn't decode {0} ({1}) ".format(
                txt, e))
            
        if not jsondict.has_key("Status"):
            raise ProtocolError("Expected Status but found none")
        if not jsondict.has_key("Command"):
            raise ProtocolError("Expected command, but found none")
        
        subcls = Response_Handlers.get_handler(jsondict["Command"])
        return subcls(jsondict["ReqID"], jsondict)
        

class HandleResponse(Response):
    """
    Subclass for handle-related responses..
    """
    def __init__(self, reqid, data):
        super(HandleResponse, self).__init__(reqid, data)
        self.handle_id = self.jsondict["Handle"]
        
    def __repr__(self):        
        return "{cmd}:{reqid} @{hid} ({status}) {payload}".format(
            cmd = self.jsondict["Command"],
            reqid = self.request_id,
            hid = self.handle_id,
            status = self.status,
            payload = self.jsondict["ResponseData"]
        )
    

class ControlResponse(HandleResponse):
    """
    This contains various control responses from driver messages.
    These are smaller responses and always have a success or failure.
    """
    
    def __init__(self, reqid, data):
        super(ControlResponse, self).__init__(reqid, data)
        
        
class DSResponse(HandleResponse):
    
    class _DetailItem(Status):
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
                              DSResponse._DetailItem):
                status = item[0]
                value = item[1] if len(item) == 2 else None
                item = DSResponse._DetailItem(
                    status, value = value)
                self[key] = item
                
            return item
    
        
    def __init__(self, reqid, data):
        super(DSResponse, self).__init__(reqid, data)
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
        
Response_Handlers.register_handler("NEWHANDLE", ControlResponse)
Response_Handlers.register_handler("NEWDATASET", Response)
[
    Response_Handlers.register_handler("MC_DS_" + subop,
                                       DSResponse)
    for subop in ('MUTATE_SET', 'MUTATE_APPEND', 'MUTATE_PREPEND',
                  'MUTATE_REPLACE', 'MUTATE_ADD', 'GET', 'DELETE', 'TOUCH')
]