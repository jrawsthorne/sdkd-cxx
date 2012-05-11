import cbsdk.constants as _C
import json

class Dataset(object):
    def as_dict(self):
        raise NotImplementedError()    
    
class DSReference(Dataset):
    dstype = _C.DSTYPE_REFERENCE
    
    def __init__(self, refid):
        self.refid = refid
        
    def as_dict(self):
        return {
            "ID" : str(self.refid)
        }
    
class DSInline(Dataset):
    dstype = _C.DSTYPE_INLINE
    
    def __init__(self, iterable,
                 dsid = None):
        self.data = iterable
        self.dsid = dsid
        
        try:
            self.data.items()
            self.has_values = True
        except Exception:
            self.has_values = False
    
    def keys_only(self):
        if self.has_values:
            return DSInline(self.data.keys())
        else:
            return self
    
    def as_dict(self):
        ret = {}
        ret["HasValues"] = self.has_values
        if self.has_values:
            ret["Items"] = list(self.data.items())
        else:
            ret["Items"] = list(self.data)
        
        if self.dsid:
            ret["ID"] = self.dsid
        
        return ret
    
    def create_reference(self):
        return DSReference(self.dsid)
    

class DSFile(Dataset):
    dstype = _C.DSTYPE_FILE
    
    @classmethod
    def new_file(cls, fname, kvpairs):
        fp = open(fname, "w")
        json.dump(fp, kvpairs)
        return cls(fname)
    
    def __init__(self, fname):
        self.fname = fname
        
    def as_dict(self):
        return {
            "DSType" : self.dstype,
            "Filename" : self.fname
        }
        

class DSSeed(Dataset):
    dstype = _C.DSTYPE_SEEDED
    
    def __init__(self, count,
                 seed = None,
                 kseed = None,
                 vseed = None,
                 ksize = 64,
                 vsize = 512,
                 repeat = None):
        
        if not repeat:
            repeat = 'C'
        
        if not kseed and not vseed:
            kseed = seed
            vseed = seed
        
        if not kseed or not vseed:
            raise ValueError("Missing base for key and value")
        
        self._spec = {
            "KSize" : ksize,
            "VSize" : vsize,
            "Repeat" : repeat,
            "KSeed" : kseed,
            "VSeed" : vseed,
            "Count" : count,
        }
    
    def as_dict(self):
        return self._spec.copy()