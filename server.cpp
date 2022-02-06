// ...wait, this WASN'T an OOP homework?
// Oh well, I did it badly anyway. Here's like 500 lines of code for your pleasure.

/*
    REQUIREMENT:
    - server should output new fd when connection occured
*/

#include "utils.hpp"

void makeServerDirectory(int port) {
    mkdir("b08902068_server_folder", 0777);
}

struct Connection;

// calls every time the connection could read/write
// must also take care of its connection's state, in terms of R/W
// note that it should kill Connection if needed, by returning 0 in call(conn)
struct ConnectionTask {
    enum State { READ, WRITE };

    // return bool: keep_alive
    // if return false/0, then ConnectionHandler will help it kill the connection
    function<bool(shared_ptr<Connection>)> callback;
    State state;
    
    ConnectionTask(function<bool(shared_ptr<Connection>)> f, ConnectionTask::State s) {
        callback = f;
        state = s;
    }

    bool call(shared_ptr<Connection> conn) {
        return callback(conn);
    }
};

// records everything about a connection
// also its callback function, in terms of ConnectionTask
struct Connection: enable_shared_from_this<Connection> {
    ConnectionTask task;
    int socket_fd;

    Connection(int fd, ConnectionTask tsk): 
        socket_fd(fd), task(tsk) { }

    int getFd() { return socket_fd; }
    ConnectionTask::State getState() { return task.state; }

    bool call() {
        return task.call(shared_from_this());
    }

    void changeTask(ConnectionTask new_task) {
        task = new_task;
    }
    void changeState(ConnectionTask::State new_state) {
        task.state = new_state;
    }
};


using SvrCommand = Command<shared_ptr<Connection>>;

// parses command and send the command to its designated SvrCommand object
struct CommandParser {
    vector<shared_ptr<SvrCommand>> cmds;
    CommandParser(vector<shared_ptr<SvrCommand>> &&in_cmds): cmds(move(in_cmds)) {}

    void execCommand(const vector<string> &args, shared_ptr<Connection> conn) {
        for(const shared_ptr<SvrCommand> &cmd: cmds) {
            if(cmd->sameName(args) && cmd->argValid(args)) {
                cmd->execCommand(args, conn);
                return;
            }
        }
        // command not found. this means my ./client have some problem...
        cerr << "Command not found." << endl;
        return;
    }
};

struct LsServerCommand: public SvrCommand {
    LsServerCommand(): SvrCommand("ls") {}
    void execCommand(const vector<string> &args, shared_ptr<Connection> conn) override {
        DIR *dp = opendir("b08902068_server_folder");
        function<int(shared_ptr<Connection>)> callback = 
            [dp] (shared_ptr<Connection> conn) mutable -> bool {
                struct dirent *de;
                while((de = readdir(dp)) != NULL) {
                    if(string(".") != de->d_name && string("..") != de->d_name) {
                        // found a file that should be written to client
                        string msg = string(de->d_name) + "\n";
                        if(write(conn->getFd(), msg.c_str(), strlen(msg.c_str())) <= 0) {
                            return 0;
                        }

                        // give back control, waiting for next select()
                        return 1;
                    }
                }
                // finish iterating all file, close connection
                return 0;
            };
        conn->changeTask(ConnectionTask(callback, ConnectionTask::WRITE));
    }
};

// Only gets 1 file each socket
// will first get message until \n, and start getting file content
struct GetServerCommand: public SvrCommand {
    GetServerCommand(): SvrCommand("get") {}
    void execCommand(const vector<string> &args, shared_ptr<Connection> conn) override {
        string path = string("b08902068_server_folder/") + args[1];
        int fd = open(path.c_str(), O_RDONLY);
        int err = errno;
        string fname = args[1];

        // handle open error. assume open error is always ENOENT...
        if(fd < 0) {
            function<int(shared_ptr<Connection>)> err_callback = 
                [fname] (shared_ptr<Connection> conn) -> bool {
                    string msg = string("The '") + fname + "' doesn't exist.\n";
                    write(conn->getFd(), msg.c_str(), strlen(msg.c_str()));
                    return 0;
                };
            conn->changeTask(ConnectionTask(err_callback, ConnectionTask::WRITE));
            return;
        }

        // normal execution
        int flag = 0;
        function<int(shared_ptr<Connection>)> callback = 
            [fd, flag] (shared_ptr<Connection> conn) mutable -> bool {
                if(!flag) {
                    // first execution
                    flag = 1;
                    const char *msg = "OK\n";
                    if(write(conn->getFd(), msg, strlen(msg)) < 0) {
                        close(fd);
                        return 0;
                    }
                    return 1;
                }

                // read some of the file
                char buf[BUFF_SIZE] = {0};
                int read_len;
                if((read_len = read(fd, buf, sizeof(buf))) <= 0) {
                    // already read all the file
                    close(fd);
                    return 0;
                }

                // try send content
                if(write(conn->getFd(), buf, read_len) <= 0) {
                    close(fd);
                    return 0;
                }

                // send other content in next select()
                return 1;
            };

        conn->changeTask(ConnectionTask(callback, ConnectionTask::WRITE));
    }
};


struct PutServerCommand: public SvrCommand {
    PutServerCommand(): SvrCommand("put") {}
    void execCommand(const vector<string> &args, shared_ptr<Connection> conn) override {
        string path = string("b08902068_server_folder/") + args[1];
        int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);

        int flag = 0;
        function<int(shared_ptr<Connection>)> callback = 
            [fd, flag] (shared_ptr<Connection> conn) mutable -> bool {
                if(!flag) {
                    // first execution
                    flag = 1;
                    const char *msg = "OK\n";
                    if(write(conn->getFd(), msg, strlen(msg)) < 0) {
                        close(fd);
                        return 0;
                    }
                    conn->changeState(ConnectionTask::READ);
                    return 1;
                }

                // basically same as get, but the other way around
                char buf[BUFF_SIZE] = {0};
                int read_len;
                if((read_len = read(conn->getFd(), buf, BUFF_SIZE)) <= 0) {
                    close(fd);
                    return 0;
                }
                if(write(fd, buf, read_len) < 0) {
                    close(fd);
                    return 0;
                }
                
                return 1;
            };

        conn->changeTask(ConnectionTask(callback, ConnectionTask::WRITE));
    }
};

bool endsWith(const string &str, const string &substr) {
    size_t find_pos = str.rfind(substr);
    return find_pos != string::npos && find_pos == str.size() - substr.size();
}

struct PlayServerCommand: public SvrCommand {
    PlayServerCommand(): SvrCommand("play") {}
    void execCommand(const vector<string> &args, shared_ptr<Connection> conn) {
        string path = string("b08902068_server_folder/") + args[1];
        string fname = args[1];
        string msg;

        // pre-defined error callback, for those error checking below
        auto makeCallback = [](string msg) {
            return [msg] (shared_ptr<Connection> conn) -> bool {
                write(conn->getFd(), msg.c_str(), strlen(msg.c_str()));
                return 0;
            };
        };

        // see if it exists and is a regular file
        struct stat buf;
        if(stat(path.c_str(), &buf) < 0) {
            msg = string("The '") + fname + "' doesn't exist.\n";
            conn->changeTask(ConnectionTask(makeCallback(msg), ConnectionTask::WRITE));
            return;
        }

        // see if it is actually a .mpg file
        if(!endsWith(args[1], ".mpg")) {
            string msg = string("The '") + fname + "' is not a mpg file.\n";
            conn->changeTask(ConnectionTask(makeCallback(msg), ConnectionTask::WRITE));
            return;
        }

        // tries to open file
        cv::VideoCapture cap(path);
        if(!cap.isOpened()) {
            cap.release();
            string msg = string("Error when opening ") + fname + ".\n";
            conn->changeTask(ConnectionTask(makeCallback(msg), ConnectionTask::WRITE));
            return;
        }

        // normal execution
        int flag = 0;
        cv::Mat img;
        function<int(shared_ptr<Connection>)> callback = 
            [cap, flag, img] (shared_ptr<Connection> conn) mutable -> bool {
                if(!flag) {
                    // first execution
                    flag = 1;

                    int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
                    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
                    img = cv::Mat::zeros(height, width, CV_8UC3);
                    if(!img.isContinuous()){
                        img = img.clone();
                    }

                    string msg = string("OK ") + to_string(width) + " " + to_string(height) + "\n";
                    if(write(conn->getFd(), msg.c_str(), strlen(msg.c_str())) < 0) {
                        cap.release();
                        return 0;
                    }
                    return 1;
                }

                // read only one frame
                int buf_size = img.total() * img.elemSize();
                cap >> img;
                if(img.empty()) {
                    cap.release();
                    return 0;
                }
                
                // try send content
                if(write(conn->getFd(), img.data, buf_size) < 0) {
                    cap.release();
                    return 0;
                }
                
                // and wait for next select()
                return 1;
            };

        conn->changeTask(ConnectionTask(callback, ConnectionTask::WRITE));
    }
};

// handles multiple connections, also handles controls to Connection by callback function
// (i.e. where select() happens)
// also helps kill Connection by callback's return value (bool keep_alive)
struct ConnectionHandler {
    map<int, shared_ptr<Connection>> conns;
    fd_set working_read_set, working_write_set, read_set, write_set;
    int fd_set_size;
    CommandParser cp;

    ConnectionHandler(int fd_set_size, CommandParser cp): fd_set_size(fd_set_size), cp(cp) {
        FD_ZERO(&working_read_set);
        FD_ZERO(&working_write_set);
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
    }

    // main loop for main() to call
    void mainLoop() {
        while(1) {
            // select and make vectors of Connections
            vector<shared_ptr<Connection>> pending_conn = selectPendingConn();

            // call each and every of their callback functions
            for(shared_ptr<Connection> &conn: pending_conn) {
                if(!conn->call()) {
                    // callback returns 0, kill this Connection
                    close(conn->getFd());
                    deleteConn(conn);
                }
                else {
                    // update state
                    // (assumes that a Connection will only change state
                    // when its callback function is called!)
                    changeState(conn);
                }
            }
        }
    }
    
    void addConn(shared_ptr<Connection> conn) {
        // add to map and fd_set
        conns[conn->getFd()] = conn;
        if(conn->getState() == ConnectionTask::READ)
            FD_SET(conn->getFd(), &read_set);
        else
            FD_SET(conn->getFd(), &write_set);
    }

    void deleteConn(shared_ptr<Connection> conn) {
        // delete from map and fd_set (assumes that fd in connection never changes)
        // only sets state, does not close() or anything
        conns.erase(conn->getFd());
        if(conn->getState() == ConnectionTask::READ)
            FD_CLR(conn->getFd(), &read_set);
        else
            FD_CLR(conn->getFd(), &write_set);
    }

    void changeState(shared_ptr<Connection> conn) {
        FD_CLR(conn->getFd(), &read_set);
        FD_CLR(conn->getFd(), &write_set);

        if(conn->getState() == ConnectionTask::READ)
            FD_SET(conn->getFd(), &read_set);
        else
            FD_SET(conn->getFd(), &write_set);
    }

    vector<shared_ptr<Connection>> selectPendingConn() {
        // select and for all fd: get its connection and make a vector for it
        // put both R/W Connections together into one vector!
        // (they will set their own state, s.t. they know they want to read() or write())
        
        memcpy(&working_read_set, &read_set, sizeof(read_set));
        memcpy(&working_write_set, &write_set, sizeof(write_set));

        int total_fds = select(fd_set_size, &working_read_set, &working_write_set, NULL, NULL);
        if(total_fds <= 0)
            return {};

        vector<shared_ptr<Connection>> pending_conn;
        for(int cur_fd = 0; cur_fd != fd_set_size; ++cur_fd) {
            if(FD_ISSET(cur_fd, &working_read_set) || FD_ISSET(cur_fd, &working_write_set))
                pending_conn.push_back(conns[cur_fd]);
        }
        return pending_conn;
    }
};

// a callback that deals with reading arguments
// it should get something like `get fname1\n`
int clientSockReadInitCmd(shared_ptr<Connection> conn, ConnectionHandler &ch) {
    // read. before the server responds, the client won't send anything else.
    char buf[BUFF_SIZE] = {0};
    if(read(conn->getFd(), buf, sizeof(buf)) <= 0)
        return 0;

    // parse into token
    vector<string> tokens = split(buf);

    // change state of Connection by SvrCommand 
    ch.cp.execCommand(tokens, conn);

    return 1;
}

// makes a connection for server socket
// this connection must accept all incoming connection and let them fetch arguments from client
shared_ptr<Connection> makeServerSocketConn(int port, ConnectionHandler &ch) {
    // make a server socket
    int server_sockfd;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);

    // Get socket file descriptor
    if((server_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        cout << "socket failed" << endl;
        exit(1);
    }

    // Set server address information
    bzero(&server_addr, sizeof(server_addr)); // erase the data
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    
    // Bind the server file descriptor to the server address
    if(bind(server_sockfd, (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0){
        cout << "bind failed\n" << endl;
        exit(1);
    }
        
    // Listen on the server file descriptor
    if(listen(server_sockfd, 15) < 0){
        cout << "listen failed\n" << endl;
        exit(1);
    }
    
    // for every new client sockets, they must read arguments first
    function<int(shared_ptr<Connection>)> client_callback = 
        [&ch](shared_ptr<Connection> conn) -> bool {
            return clientSockReadInitCmd(conn, ch);
        };
    
    // make server socket accept all incoming connections
    function<int(shared_ptr<Connection>)> server_callback = 
        [&ch, client_callback](shared_ptr<Connection> server_conn) -> bool {
            int client_sockfd;
            struct sockaddr_in client_addr;
            int client_addr_len = sizeof(client_addr);

            // tries to accept
            if((client_sockfd = 
                    accept(server_conn->getFd(), (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len)) < 0){

                cout << "accept failed" << endl;
                return 1;
            }

            // make Connection and assign callback to them
            shared_ptr<Connection> new_conn = 
                make_shared<Connection>(client_sockfd, ConnectionTask(client_callback, ConnectionTask::READ));
            ch.addConn(new_conn);
            cout << "[*] Accepted connection: fd = " << new_conn->getFd() << endl;
            return 1;
        };

    // make server socket Connection
    shared_ptr<Connection> server_conn = 
        make_shared<Connection>(server_sockfd, ConnectionTask(server_callback, ConnectionTask::READ));
    return server_conn;
}


int main(int argc, char *argv[]) {
    if(argc != 2) {
        cout << "./server [port]" << endl;
        exit(1);
    }

    int port = atoi(argv[1]);

    // just don't do anything, let write() return -1 and close socket
    signal(SIGPIPE, SIG_IGN);
    
    // dependency-inject SvrCommands in main()
    CommandParser cp({
        make_shared<LsServerCommand>(), 
        make_shared<GetServerCommand>(), 
        make_shared<PutServerCommand>(), 
        make_shared<PlayServerCommand>()
    });

    ConnectionHandler ch(100, cp);

    // add main connection
    shared_ptr<Connection> server_conn = makeServerSocketConn(port, ch);
    ch.addConn(server_conn);

    makeServerDirectory(port);

    // goes into main loop and never return
    ch.mainLoop();

    return 0;
}
