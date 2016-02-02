/**

RTSP的主要命令：

方法关键字表示将要用于请求-URL所指示的资源上的方法。方法是大小写敏感的。将来可能会定义新的方法。
方法名称不应以$符号（数字24）开始，并且必须是一个关键字。所有的方法被列在下表。

       方法             方向             对象       要求                含义
      DESCRIBE          C->S             P,S        推荐                检查演示或媒体对象的描述，
      ANNOUNCE          C->S, S->C       P,S        可选
      GET_PARAMETER     C->S, S->C       P,S        可选
      OPTIONS           C->S, S->C       P,S        必要 (S->C: 可选)
      PAUSE             C->S             P,S        推荐
      PLAY              C->S             P,S        必要
      RECORD            C->S             P,S        可选
      REDIRECT          S->C             P,S        可选
      SETUP             C->S             S          必要
      SET_PARAMETER     C->S, S->C       P,S        可选
      TEARDOWN          C->S             P,S        必要

RTSP方法一览，它们的方向，以及它们用来操作的对象（P：（presentation）表示； S：流）
注意关于上表：
    PAUSE是推荐的，但是不强制要求一个实现了完整功能的服务器支持此方法，例如，对于直播节目。
    如果一个服务器不支持一个特定的方法，它必须返回"501 未实现"，同时客户端不应该在此服务器上再次尝试此方法。


    
    

RTSP的 报文结构
两类报文：
    请求报文和响应报文。请求报文是指从客户向服务器发送请求报文，响应报文是指从服务器到客户的回答。

由 于 RTSP 是面向正文的(text-oriented)，因此在报文中的每一个字段都是一些 ASCII 码串，因而每个字段的长度都是不确定的。
RTSP报 文由三部分组成，即开始行、首部行和实体主体。在请求报文中，开始行就是请求行；响 应报文的开始行是状态行；




RTSP交 互过程
    C表 示RTSP客户端，S表示RTSP服务端
    ①  C->S: OPTION request            //询问S有 哪些方法可用
        S->C: OPTION response        //S回 应信息中包括提供的所有可用方法
    ②  C->S: DESCRIBE request      //要求得到S提供 的媒体初始化描述信息
        S->C: DESCRIBE response      //S回 应媒体初始化描述信息，主要是sdp
    ③  C->S: SETUP request         //设置会话属性，以及传输模式，提醒S建 立会话
        S->C: SETUP response         //S建 立会话，返回会话标识符及会话相关信息
    ④  C->S: PLAY request          //C请求播放
        S->C: PLAY response          //S回 应请求信息
        S->C: 发 送流媒体数据
    ⑤  C->S: TEARDOWN request     //C请 求关闭会话
        S->C: TEARDOWN response     //S回应请求

上 述的过程是标准的RTSP流程，其中第3步和第4步是必需的。
RTSP，实时流协议，是一个C/S多媒体节目协议， 它可以控制流媒体数据在IP网络上的发送，同时提供用于音频和视频流的“VCR模式”远程控制功能，
如停止、快进、快退和定位。同时RTSP又是一个应用 层协议，用来与诸如RTP、RSVP等更低层的协议一起，提供基于Internet的整套流化服务。
基于RTSP协议流媒体服务器的实现方案可以让流媒体在IP 上自由翱翔。









6.RTSP状态码

Status-Code = "100" ; Continue
| "200" ; OK
| "201" ; Created
| "250" ; Low on Storage Space
| "300" ; Multiple Choices
| "301" ; Moved Permanently
| "302" ; Moved Temporarily
| "303" ; See Other
| "304" ; Not Modified
| "305" ; Use Proxy
| "400" ; Bad Request
| "401" ; Unauthorized
| "402" ; Payment Required
| "403" ; Forbidden
| "404" ; Not Found
| "405" ; Method Not Allowed
| "406" ; Not Acceptable
| "407" ; Proxy Authentication Required
| "408" ; Request Time-out
| "410" ; Gone
| "411" ; Length Required
| "412" ; Precondition Failed
| "413" ; Request Entity Too Large
| "414" ; Request-URI Too Large
| "415" ; Unsupported Media Type
| "451" ; Parameter Not Understood
| "452" ; Conference Not Found
| "453" ; Not Enough Bandwidth
| "454" ; Session Not Found
| "455" ; Method Not Valid in This State
| "456" ; Header Field Not Valid for Resource
| "457" ; Invalid Range
| "458" ; Parameter Is Read-Only
| "459" ; Aggregate operation not allowed
| "460" ; Only aggregate operation allowed
| "461" ; Unsupported transport
| "462" ; Destination unreachable
| "500" ; Internal Server Error
| "501" ; Not Implemented
| "502" ; Bad Gateway
| "503" ; Service Unavailable
| "504" ; Gateway Time-out
| "505" ; RTSP Version not supported
| "551" ; Option not supported
| extension-code
extension-code = 3DIGIT
Reason-Phrase = *<TEXT, excluding CR, LF


**/






















