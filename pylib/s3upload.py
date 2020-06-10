#!/usr/bin/env python

# This module contains the s3_upload function which uploads stuff into our
# S3 bucket. The access and secret keys are my own, but I've deemed them rather
# benign (not tied to any credit card or anything). While nasty, I can't think
# of a better replacement currently.

import os
import os.path
import boto3
from botocore.exceptions import ClientError
import argparse
import gzip
import sys
from datetime import datetime


S3_DIR = "sdkd"

def upload_to_aws(file, sdk):
    s3 = boto3.client('s3', aws_access_key_id=S3_ACCESS,
                      aws_secret_access_key=S3_SECRET)
    try:
        with open(file, "rb") as f:
            response = s3.upload_fileobj(f, S3_BUCKET, S3_DIR+"-"+sdk+"/"+file, ExtraArgs={
                'ACL': 'public-read'})
    except ClientError as e:
        print(e)
        return False
    return True

if __name__ == "__main__":

    today = datetime.utcnow().timetuple()
    compressed_log = ""

    for i in today:
        compressed_log += "{0}".format(i)

    S3_BUCKET = sys.argv[1]
    S3_ACCESS =  sys.argv[2]
    S3_SECRET =  sys.argv[3]
    log_file = sys.argv[4]
    sdk = sys.argv[5]

    with open(log_file, 'rb') as f_in, gzip.open('{0}.gz'.format(compressed_log), 'wb') as f_out:
        f_out.writelines(f_in)
        f_out.close()
        f_in.close()
    compressed_file = '{0}.gz'.format(compressed_log)
    upload = upload_to_aws(compressed_file, sdk)
    if upload:
        print("http://{0}.s3.amazonaws.com/{1}-{2}/{3}".format(S3_BUCKET, S3_DIR, sdk, compressed_file))
    else:
        print("Failed to upload logs")
    os.remove("{0}.gz".format(compressed_log))
    os.remove(log_file)