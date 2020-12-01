# RBProto - RemoteBackup Protobuf schema

In the typical scenario the client sends a `RBRequest` protobuf message and the server replies with a `RBResponse`. The socket is closed right after.

## RBMsgType
Depending on the `RBRequest->type` and `RBResponse->type`, the following table shows the required field(s).

| RBMsgType | RBRequest     | RBResponse      |
|-----------|---------------|-----------------|
|  AUTH      | `authRequest` | `authResponse`  |
| *UPLOAD    | `fileSegment` |                 |
| *REMOVE    | `fileSegment` |                 |
| *PROBE     |               | `probeResponse` |


## RBRequest
Requests with __*__ will need to have `Request->token` field set with the correct authorization token

## RBResponse
All responses will have the `Response->success` field and can include and `Response->error` string. 

## ProtoVer
Both `RBRequest` and `RBResponse` have the `protoVer` field that has to contain the minimum required version of the RBProto to understand the message.
