#Create s3creds file
echo "
[default]
S3_BUCKET = sdkqe-testresults.couchbase.com
access_key = $AWS_ACCESS_KEY_ID
secret_key = $AWS_SECRET_ACCESS_KEY
rpmDownloadPassword = $LATEST_BUILDS_PASS

" > ~/.s3cfg