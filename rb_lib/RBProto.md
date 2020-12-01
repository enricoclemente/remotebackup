# RBProto - RemoteBackup Protobuf schema

In the typical scenario the client sends a `RBRequest` protobuf message and the server replies with a `RBResponse`. The socket is closed right after.

## RBRequest

Sent from client to the server.
`Request->type` can be
- `upload`__*__, and there will be a `Request->fileSegment` field containing the file segment containing all the fields
- `remove`__*__, and there will be a `Request->fileSegment` field containing the file segment containing just the path
- `probe`__*__
- `auth`, and there will be a `Request->authRequest` field

Note: requests with __*__ will need to have `Request->token` field set with the correct authorization token
## RBResponse

Sent back from the server to the client.
`Response->type` can be
- `probe`, and there will be a `Response->probeResponse` field
- `auth`, and there will be a `Response->authResponse` field
- `general` for general confirmation/error reporting

Note: all responses will have the `Response->success` field and can include and `Response->error` string. 

## ProtoVer
Both `RBRequest` and `RBResponse` have the `protoVer` field that has to contain the minimum required version of the RBProto to understand the message.
