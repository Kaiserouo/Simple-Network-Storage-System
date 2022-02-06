#include "utils.hpp"

void makeClientDirectory(char *id_cstr) {
    string fname = string("b08902068_") + id_cstr + "_client_folder";
    mkdir(fname.c_str(), 0777);
}

// assumes read message < buf_sz
// if success return the remaining content's length (will be stored in buf)
// else return -1
// will omit LF, won't appear in buf and str
int readMsgUntilLF(string &str, char *buf, int fd, int buf_sz) {
    auto lbuf = make_unique<char[]>(buf_sz);
    int read_cnt;
    int offset = 0;
    char *pos = NULL;
    while(offset < buf_sz) {
        if((read_cnt = read(fd, lbuf.get() + offset, buf_sz - offset - 1)) <= 0) {
            return -1;
        }
        lbuf[read_cnt + offset] = '\0';
        if((pos = strstr(lbuf.get() + offset, "\n")) != NULL) {
            // found
            *pos = '\0';
            str = string(lbuf.get());
            
            int remain_cnt = read_cnt + offset - (pos - lbuf.get() + 1);
            memcpy(buf, pos+1, remain_cnt);
            return remain_cnt;
        }
        offset += read_cnt;
    }
    return -1;
}

struct ServerInfo {
    string hname; int port;
    struct sockaddr_in addr;
    ServerInfo(vector<string> &server_info): hname(server_info[0]), port(stoi(server_info[1])) {
        bzero(&addr,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        // read address, can tackle with hostname & IPv4 
        struct hostent *e;
        if((e = gethostbyname(hname.c_str())) == NULL) {
            cout << "Invalid IP / hostname." << endl;
            exit(1);
        }

        // just get the first one
        addr.sin_addr = **(struct in_addr **)e->h_addr_list;
    }

    // returns file descriptor
    int makeConnection() {
        int sockfd;
        if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            cout << "socket failed" << endl;
            exit(1);
        }
        if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
            cout << "connect failed" << endl;
            exit(1);
        }
        return sockfd;
    }
};

// insert hella lot of command

using CliCommand = Command<ServerInfo &>;

struct LsClientCommand: public CliCommand {
    LsClientCommand(): CliCommand("ls") {}
    bool argValid(const vector<string> &args) override {
        return args.size() == 1;
    }
    void execCommand(const vector<string> &args, ServerInfo &svr_info) override {
        int conn_fd = svr_info.makeConnection();
        const char *msg = "ls\n";
        if(write(conn_fd, msg, strlen(msg)) < 0) {
            close(conn_fd);
            return;
        }

        char buf[BUFF_SIZE] = {0};
        int read_cnt;
        while((read_cnt = read(conn_fd, buf, BUFF_SIZE-1)) > 0) {
            buf[read_cnt] = '\0';
            cout << buf;
        }
        close(conn_fd);
        return;
    }
};
struct GetClientCommand: public CliCommand {
    string id;
    GetClientCommand(string id): CliCommand("get"), id(id) {}
    bool argValid(const vector<string> &args) override {
        return args.size() > 1;
    }
    void execCommand(const vector<string> &args, ServerInfo &svr_info) override {
        for(int i = 1; i != args.size(); ++i) {
            cout << "getting " << args[i] << "......" << endl;
            int conn_fd = svr_info.makeConnection();
            
            string msg = args[0] + " " + args[i] + "\n";
            if(write(conn_fd, msg.c_str(), strlen(msg.c_str())) < 0) {
                close(conn_fd);
                return;
            }

            // need to read server message
            char buf[BUFF_SIZE];
            msg = string();
            int read_cnt;
            if((read_cnt = readMsgUntilLF(msg, buf, conn_fd, BUFF_SIZE)) < 0) {
                cout << "error while reading server message" << endl;
                close(conn_fd);
                continue;
            }
            if(msg != "OK") {
                // error occured
                cout << msg << endl;
                close(conn_fd);
                continue;
            }
            
            // open file
            int file_fd;
            string fname = string("b08902068_") + id + "_client_folder/" + args[i];
            if((file_fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0) {
                cout << "error when opening file" << endl;
                close(conn_fd);
                continue;
            }

            // write remain content
            if(write(file_fd, buf, read_cnt) < 0) {
                close(conn_fd); close(file_fd);
                continue;
            }

            // write more content to file from server
            while((read_cnt = read(conn_fd, buf, BUFF_SIZE)) > 0) {
                if(write(file_fd, buf, read_cnt) < 0) {
                    break;
                }
            }
            close(conn_fd); close(file_fd);
        }
        return;
    }
};
struct PutClientCommand: public CliCommand {
    string id;
    PutClientCommand(string id): CliCommand("put"), id(id) {}
    bool argValid(const vector<string> &args) override {
        return args.size() > 1;
    }
    void execCommand(const vector<string> &args, ServerInfo &svr_info) override {
        for(int i = 1; i != args.size(); ++i) {
            cout << "putting " << args[i] << "......" << endl;

            // open file
            int file_fd;
            string fname = string("b08902068_") + id + "_client_folder/" + args[i];
            if((file_fd = open(fname.c_str(), O_RDONLY)) < 0) {
                cout << "The '" << args[i] << "' doesn't exist." << endl;
                continue;
            }

            // communicate with server
            int conn_fd = svr_info.makeConnection();
            string msg = args[0] + " " + args[i] + "\n";
            if(write(conn_fd, msg.c_str(), strlen(msg.c_str())) < 0) {
                close(conn_fd); close(file_fd);
                return;
            }

            // read server message
            char buf[BUFF_SIZE];
            msg = string();
            int read_cnt;
            if((read_cnt = readMsgUntilLF(msg, buf, conn_fd, BUFF_SIZE)) < 0) {
                cout << "error while reading server message" << endl;
                close(conn_fd); close(file_fd);
                continue;
            }
            if(msg != "OK") {
                // error occured
                cout << msg << endl;
                close(conn_fd); close(file_fd);
                continue;
            }
            
            // write file content to server
            while((read_cnt = read(file_fd, buf, BUFF_SIZE)) > 0) {
                if(write(conn_fd, buf, read_cnt) < 0) {
                    break;
                }
            }
            close(conn_fd); close(file_fd);
        }
        return;
    }
};

// try to actually get that many bytes
int readFull(int fd, void *ret_buf, int sz) {
    int offset = 0;
    int read_cnt = 0;
    while(offset < sz) {
        read_cnt = read(fd, (uchar *)ret_buf + offset, sz - offset);
        if(read_cnt <= 0)
            break;
        offset += read_cnt;
    }
    return offset;
}

struct PlayClientCommand: public CliCommand {
    string id;
    PlayClientCommand(string id): CliCommand("play"), id(id) {}
    bool argValid(const vector<string> &args) override {
        return args.size() == 2;
    }
    void execCommand(const vector<string> &args, ServerInfo &svr_info) override {
        int conn_fd = svr_info.makeConnection();
        
        string msg = args[0] + " " + args[1] + "\n";
        if(write(conn_fd, msg.c_str(), strlen(msg.c_str())) < 0) {
            close(conn_fd);
            return;
        }

        // need to read server message
        char buf[BUFF_SIZE];
        msg = string();
        int read_cnt;
        if((read_cnt = readMsgUntilLF(msg, buf, conn_fd, BUFF_SIZE)) < 0) {
            cout << "error while reading server message" << endl;
            close(conn_fd);
            return;
        }
        if(msg.find("OK") == string::npos) {
            // error occured
            cout << msg << endl;
            close(conn_fd);
            return;
        }

        vector<string> video_msg = split(msg);
        int width = stoi(video_msg[1]);
        int height = stoi(video_msg[2]);
        
        cv::Mat img = cv::Mat::zeros(height, width, CV_8UC3);
        int buf_size = img.total() * img.elemSize();
        while((read_cnt = readFull(conn_fd, img.data, buf_size)) == buf_size) {
            cv::imshow("video", img);
            char c = (char)cv::waitKey(33.3333);
            if(c==27)
                break;
        }
        cv::destroyAllWindows();
        close(conn_fd);
    }
};

// simplified in-program shell
struct Shell {
    vector<shared_ptr<CliCommand>> cmds;
    ServerInfo svr_info;
    Shell(const vector<shared_ptr<CliCommand>> &cmds, const ServerInfo &svr_info):
        cmds(cmds), svr_info(svr_info) {}
    
    void mainLoop() {
        while(1) {
            string response;
            cout << "$ " << flush;
            getline(cin, response);
            
            vector<string> args = split(response);
            if(args.size() == 0)
                continue;
            
            execCommand(args);
        }
    }
    
    void execCommand(const vector<string> &args) {
        for(const shared_ptr<CliCommand> &cmd: cmds) {
            if(cmd->sameName(args)) {
                if(!cmd->argValid(args)) {
                    cout << "Command format error." << endl;
                    return;
                }
                cmd->execCommand(args, svr_info);
                return;
            }
        }
        cout << "Command not found." << endl;
        return;
    }
};


int main(int argc, char *argv[]) {
    if(argc != 3) {
        cout << "./client [client_id] [ip]:[port]" << endl;
        return 1;
    }

    // just ignore that
    signal(SIGPIPE, SIG_IGN);

    // initial settings
    makeClientDirectory(argv[1]);
    vector<string> server_info = split(argv[2], ":");
    ServerInfo svr_info(server_info);
    Shell sh({
        make_shared<LsClientCommand>(),
        make_shared<GetClientCommand>(argv[1]),
        make_shared<PutClientCommand>(argv[1]),
        make_shared<PlayClientCommand>(argv[1])
    }, svr_info);

    sh.mainLoop();
}