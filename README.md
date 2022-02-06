# Simple-Network-Storage-System
Homework 2 for Computer Network course in National Taiwan University, in Fall 2021.


## Brief Specification
Implement a client-server structured network storage system, that can:
+ List / upload / download files on system
+ Watch a `.mpg` video (streaming)

Use `select()` to accomplish multiple client connections / request to one server without blocking.
## Usage
Use makefile to compile source:
```
$ make client // To compile client code
$ make server // To compile server code
```

Start server & client:
```
$ ./server [port]
$ ./client [client_id] [server_ip]:[port]  // client_id will be an integer, and should be unique that there won't be multiple clients using same client_id at the same time
                                           // each client_id will have its own folder / space on server.
```

Client looks like a sftp shell. There are 4 operations that a client can use:
```
$ ls                                          // list directory
$ put <filename1> <filename2> … <filenameN>   // put file to server
$ get <filename1> <filename2> … <filenameN>   // get file from server
$ play <mpg_videofile>                        // stream a video, press esc to cancel
```

## Report
+ Draw a flowchart of the file transferring and explains how it works in detail.
+ Draw a flowchart of the video streaming and explains how it works in detail.
+ What is `SIGPIPE`? It is possible that `SIGPIPE` is sent to your process? If so, how do you handle it?
+ Is blocking I/O equal to synchronized I/O? Please give some examples to explain it.
