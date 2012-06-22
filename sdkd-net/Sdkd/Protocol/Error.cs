using System;
namespace Sdkd.Protocol
{
	public enum ErrorCode {
		SUBSYSf_UNKNOWN      = 0x1,
		SUBSYSf_CLUSTER      = 0x2,
		SUBSYSf_CLIENT       = 0x4,
		SUBSYSf_MEMD         = 0x8,
		SUBSYSf_NETWORK      = 0x10,
		SUBSYSf_SDKD         = 0x20,
		SUBSYSf_KVOPS        = 0x40,
		
		KVOPS_EMATCH         = 0x200,
		
		SDKD_EINVAL          = 0x200,
		SDKD_ENOIMPL         = 0x300,
		SDKD_ENOHANDLE       = 0x400,
		SDKD_ENODS           = 0x500,
		SDKD_ENOREQ          = 0x600,
		
		ERROR_GENERIC        = 0x100,
		
		CLIENT_ETMO          = 0x200,
		
		CLUSTER_EAUTH        = 0x200,
		CLUSTER_ENOENT       = 0x300,
		
		MEMD_ENOENT          = 0x200,
		MEMD_ECAS            = 0x300,
		MEMD_ESET            = 0x400,
		MEMD_EVBUCKET        = 0x500
	}
	
	public class Error {
		
		
		
		public int errnum { get; private set; }
		public string errstr { get; private set; }
		
		public Error (ErrorCode subsys, ErrorCode minor, string desc = "Error")
		{
			errnum = (int)subsys | (int)minor;
			errstr = desc;
		}
		
		public Error (int code, string desc = "Error")
		{
			errnum = code;
			errstr = desc;
		}		
	}
}


