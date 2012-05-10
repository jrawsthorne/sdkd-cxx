"""
This defines common message functions for the sdkd protocol
"""
import json

class ProtocolError(Exception): pass

class Message(object):
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
