# RBProto - RemoteBackup Protobuf schema

In the typical scenario the client sends a `RBRequest` protobuf message and the server replies with a `RBResponse`. At this point the client can send another request, that will provide the client with a response and so on, until the client sets the `final` flag to close the channel. In the high-level implementation of the client, such channel is called `ProtoChannel`.

## RBMsgType
Depending on the `RBRequest->type` and `RBResponse->type`, the following table shows the required field(s).

| RBMsgType  | RBRequest     | RBResponse      |
|------------|---------------|-----------------|
|  AUTH      | `authRequest` | `authResponse`  |
| *UPLOAD    | `fileSegment` |                 |
| *REMOVE    | `fileSegment` |                 |
| *PROBE     |               | `probeResponse` |
| *ABORT     | `fileSegment` |                 |
| *RESTORE   | `fileSegment` | `fileSegment`   |
|  NOP       |               |                 |


## RBRequest
Requests with __*__ will need to have `Request->token` field set with the correct authorization token
In case the `Request->final` flag is set to true, the socket connection has to be closed on both ends. 

## RBResponse
All responses will have the `Response->success` field and can include and `Response->error` string. 

## ProtoVer
Both `RBRequest` and `RBResponse` have the `protoVer` field that has to contain the minimum required version of the RBProto to understand the message.
