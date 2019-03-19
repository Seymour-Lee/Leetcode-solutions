#ifndef _PRIMARY_H_
#define _PRIMARY_H_

#include "router.h"
#include "timers.hh"

class Primary: public Router{
public:
    Primary(int _s, int _stage) {
        this->s = _s;
        this->stage = _stage;
        this->octane_counter = 0;
        for(int i = 0; i < global::num_routers; i++) router2ip[i] = "10.5.51.1"+to_string(i+1), ip2router["10.5.51.1"+to_string(i+1)] = i;
    }

    ~Primary() {
        ostream& logger = Logger::getInstance();
        logger<<"router 0 closed"<<endl;
    }

    void run(){
        log_file = "stage" + to_string(stage) + ".r0.out";
        global::log_file_name = log_file;
        std::ofstream fout;
        fout.open(log_file);
        Logger::getInstance().rdbuf(fout.rdbuf());
        ostream& logger = Logger::getInstance();
        if(this->stage <= 5)this->stage1();
        else this->wait_hello_from_routers();
        if(this->stage >= 2) this->stage2();
        close(s);
        close(global::tun_fd);
        logger<<"router 0 closed"<<endl;
    }

private:
    int stage;
    int s;
    string log_file;
    char buff[BUF_SIZE];
    unsigned char rbuf[BUF_SIZE];
    int stelen;
    int nsize;
    struct sockaddr_in router_addr;

    // stage4 and above
    int octane_counter;
    unordered_map<uint16_t, unordered_set<string>> pro2addr; // octane_protocol to "src_ip, src_port, dst_ip, dst_port"
    Timers *timersManager_;
    
    // stage6 and above
    unordered_map<int, string> router2ip;
    unordered_map<string, int> ip2router;
    // vector<struct sockaddr_in> routers_addr(8);
    struct sockaddr_in *routers_addr;
    unordered_map<string, unsigned int> ip2port;


    void wait_hello_from_routers(){
        ostream& logger = Logger::getInstance();
        logger<<"primary port: "<<global::primary_port<<endl;
        char buffer[BUF_SIZE];
        int num = global::num_routers;
        global::routers_pid = (int*)malloc(sizeof(int)*num);
        global::routers_port = (int*)malloc(sizeof(int)*num);
        routers_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in)*num);
        struct sockaddr_in cur_router_addr;
        while(num--){
            int len = recvfrom(s, buffer, BUF_SIZE, 0, (struct sockaddr*) &cur_router_addr, (socklen_t*) &nsize);
            string data(buffer);
            cout<<"data is "<<data<<endl;
            
            int router_id = stoi(data.substr(0, 1));
            data = data.substr(2);
            // cout<<router_id<<" "<<data<<endl;
            if(router_id == 0) router_addr = cur_router_addr;
            // cout<<router_id<<"----------------------"<<cur_router_addr.sin_port<<endl;
            
            int pos = data.find(',');
            int router_pid = stoi(data.substr(0, pos));
            int router_port = stoi(data.substr(pos+1));
            cur_router_addr.sin_port = router_port;
            global::routers_pid[router_id] = router_pid;
            global::routers_port[router_id] = router_port;
            routers_addr[router_id] = cur_router_addr;
            logger<<"router: "<<router_id+1<<", pid: "<<router_pid<<", port: "<<router_port<<endl;
        }
    }

    void stage1(){
        cout<<"Primary:: get into stage1 "<<this->s<<endl;
        ostream& logger = Logger::getInstance();
        logger<<"primary port: "<<global::primary_port<<endl;

        nsize = sizeof(struct sockaddr);
        socklen_t addrlen = sizeof(struct sockaddr_in);

        /* Stage 1 */
        cout<<"Primary:: Primary router is waiting for I am up message"<<endl;
        stelen = recvfrom(s, buff, BUF_SIZE, 0, (struct sockaddr*) &router_addr, (socklen_t*) &nsize);
        cout<<"Primary:: got an the up message from Secondary"<<endl;

        pthread_mutex_lock(&global::mutex);
        global::pid = atoi(buff);
        logger<<"router: "<<global::num_routers<<", pid: "<<global::pid<<", port: "<<router_addr.sin_port<<endl;
        pthread_mutex_unlock(&global::mutex);
    }

    void stage2(){
        cout<<"Primary:: start stage2"<<endl;
        ostream& logger = Logger::getInstance();
        char tun_name[IFNAMSIZ];
        global::tun_fd = utils::tun_alloc(strdup("tun1"), IFF_TUN | IFF_NO_PI);
        if(global::tun_fd < 0){
        	cout<<"Primary:: Failed to open tunnel interface"<<endl;;
        	exit(1);
        }

        fd_set readset;
        int select_ans;
        int maxfd = (global::tun_fd>s)?(global::tun_fd+1):(s+1);
        struct timeval tv;

        while (true){
            cout<<"Primary:: stage2 start to listen"<<endl;
            FD_ZERO(&readset);
            FD_SET(global::tun_fd, &readset);
            FD_SET(s, &readset);
            cout<<"Primary:: waiting"<<endl;
            
            tv.tv_sec = 1000;
            tv.tv_usec = 0;
            select_ans = select(maxfd+1, &readset, NULL, NULL, &tv);
            cout<<"Primary:: get an info"<<endl;
            // pthread_mutex_lock(&mutex);
            memset(&buff, 0, sizeof(buff));
            if(select_ans == -1) cout<<"Primary:: select error"<<endl;
            else if(select_ans == 0){
                cout<<"Primary:: time out"<<endl;
                logger<<"router 0 closed"<<endl;
                // int status;
                // kill(global::pid, SIGTERM);
                // wait(&status);
                // if (WIFSIGNALED(status))printf("Child process received singal %d\n", WTERMSIG(status));

                if(this->stage <= 5){
                    int status;
                    kill(global::pid, SIGTERM);
                    wait(&status);
                    if (WIFSIGNALED(status)) cout<<"Secondary got killed"<<endl;
                }
                else{
                    for(int i = 0; i < global::num_routers; i++){
                        int status;
                        kill(global::routers_pid[i], SIGTERM);
                        wait(&status);
                        cout<<"router: "<<i<<" got killed"<<endl;
                        if (WIFSIGNALED(status)) cout<<"router: "<<i<<" got killed"<<endl;
                    }
                }

                close(global::service);
                raise(SIGTERM);
            }
            else{
                if(FD_ISSET(s, &readset)){ // handle message between primary and secondarys
                    stelen = recvfrom(s, buff, BUF_SIZE, 0, (struct sockaddr*) &router_addr, (socklen_t*) &nsize);
                    cout<<"Primary:: Read a packet from Secondary, packet length: "<<stelen<<endl;
                    Packer *p = new Packer(buff, stelen);
                    p->parse();
                    

                    // if (p->recieve()){
                    if(p->type == 1){
                        logger<<"ICMP from port: "<<router_addr.sin_port<<", src: "<<p->src.data()<<", dst: "<< p->dst.data()<<", type: "<<p->icmptype<<endl;
                        cout<<"Primary:: ICMP from port: "<<router_addr.sin_port<<", src: "<<p->src.data()<<", dst: "<< p->dst.data()<<", type: "<<p->icmptype<<endl;
                        write(global::tun_fd, p->getpacket(), p->getlen());
                    }
                    else if(p->type == 253){
                        unsigned char *buffer = (unsigned char *)p->getpacket();
                        struct octane_control *octane = (struct octane_control *)(buffer+sizeof(struct iphdr));
                        cout<<"Primary:: get ack from secondary!! seqno is "<<octane->octane_seqno<<endl;
                        //
                    }
                    else if(p->type == 6){
                        // p->parse_tcp();
                        cout<<"Primary:: get tcp packet from secondary"<<", src: "<<p->src.data()<<", dst: "<< p->dst.data()<<" "<<p->dstport<<endl;
                        cout<<ip2port[p->src]<<endl;
                        p->change_dst("10.0.2.15");
                        p->recheck_tcp();  // recheck tcp checksum
                        // p->recheck_pkt();
                        
                        // p->setchecksum(); // recheck ip checksum
                        // p->parse_tcp();
                        cout<<"Primary:: get tcp packet from secondary"<<", src: "<<p->src.data()<<", dst: "<< p->dst.data()<<" "<<p->dstport<<endl;
                        write(global::tun_fd, p->getpacket(), p->getlen());
                    }
                    else cout<<"Primary:: Invalid packet from Secondary!"<<endl;
                    delete p;
                }
                if(FD_ISSET(global::tun_fd, &readset)){ // handle message between primary and tunnel
                    // read from tunnel
                    int nread = read(global::tun_fd,buff,BUF_SIZE);
                    if(nread < 0){
                        cout<<"Primary:: Reading from tunnel interface"<<endl;
                        close(global::tun_fd);
                        exit(1);
                    }
                    else{
                        cout<<"Primary:: Read a packet from tunnel, packet length: "<<nread<<endl;
                        Packer *p = new Packer(buff, nread);
                        p->parse();
                        // if (p->recieve()){
                        if(p->type == 1){
                            logger<<"ICMP from tunnel, src: "<<p->src.data()<<", dst: "<< p->dst.data()<<", type: "<<p->icmptype<<endl;
                            cout<<"Primary:: ICMP from tunnel, src: "<<p->src.data()<<", dst: "<< p->dst.data()<<", type: "<<p->icmptype<<endl;
                            // judge the action of message from tunnel
                            if(this->stage >= 4){
                                handle_control_message(p, p->type);
                                
                            }
                            if(this->stage >= 6){
                                int id = p->dst.back()-'1';
                                p->send(&routers_addr[id], s, p->getpacket(), p->getlen());
                            }
                            else{
                                p->send(&router_addr, s, p->getpacket(), p->getlen());
                            }
                            
                        }
                        else if(p->type == 6){
                            // handle tcp
                            // cout<<"find tcp packet!!!!!!"<<endl;
                            if(this->stage >= 6){
                                handle_control_message(p, p->type);
                                
                            }
                            if(this->stage >= 6){
                                p->parse_tcp();
                                // send control message
                                handle_tcp_from_tunnel(p);
                            }

                        }
                        else{
                            cout<<"Primary:: Invalid packet from tunnel! p->type is "<<p->type<<endl;
                        }
                        delete p;
                    }
                }
            }
            // pthread_mutex_unlock(&mutex);
        }
        close(global::tun_fd);
    }

    void handle_tcp_from_tunnel(Packer *p){
        // cout<<"Primary:: get a tcp packet from tunnel, src: "<<p->src<<", dst: "<<p->dst<<endl;
        ostream& logger = Logger::getInstance();
        if(p->dstport != 80 && p->dstport != 443) return; // discard others
        logger<<"TCP from tunnel, ("<<p->src<<", "<<p->srcport<<", "<<p->dst<<", "<<p->dstport<<")"<<endl;
        cout<<"TCP from tunnel, ("<<p->src<<", "<<p->srcport<<", "<<p->dst<<", "<<p->dstport<<")"<<endl;
        ip2port[p->dst] = p->srcport;
        if(p->dstport == 80){
            send_tcp_to_r1(p);
        }
        else if(p->dstport == 443){
            if(this->stage == 6) send_tcp_to_r2(p);
            else if(this->stage == 7) send_jump_tcp_to_r1(p);
        }
    }

    void send_tcp_to_r1(Packer *p){
        p->send(&routers_addr[0], s, p->getpacket(), p->getlen());
    }

    void send_tcp_to_r2(Packer *p){
        p->send(&routers_addr[1], s, p->getpacket(), p->getlen());
    }

    void send_jump_tcp_to_r1(Packer *p){
        send_tcp_to_r1(p);
    }

    void write_flow_table(int protocol, string addr){
        cout<<"in primary:;write_flow_table()"<<endl;
        pro2addr[protocol].insert(addr);
    }

    void handle_control_message(Packer *p, int protocol_type){
        cout<<"Primary: distributing octane control message"<<endl;
        ostream& logger = Logger::getInstance();

        // if(this->stage >= 6){

        // }
        
        if(this->stage >= 4){
            // judge hit or not
            string addr0 = p->src+", "+to_string(p->srcport)+", "+p->dst+", "+to_string(p->dstport);
            string addr1 = p->dst+", "+to_string(p->dstport)+", "+p->src+", "+to_string(p->srcport);
            // if not hit
            if(pro2addr[p->type].find(addr0) == pro2addr[p->type].end()){
                self_install_rule(addr0, p->type);

                if(this->octane_counter != 0 && this->octane_counter%(global::drop_after) == 0){
                    send_octane_drop(p, p->src_int, p->srcport, p->dst_int, p->dstport, p->type);
                }
                else if(p->dst.find("10.5.51") != string::npos){
                    send_octane_reply(p, p->src_int, p->srcport, p->dst_int, p->dstport, p->type);
                }
                else{ // send to outside world
                    send_octane_forward(p, p->src_int, p->srcport, p->dst_int, p->dstport, p->type);
                }
            }
            // if hit
            else{
                logger<<"router: 0, rule hit ("<<addr0<<", "<<p->type<<") action 1"<<endl;
                cout<<"router: 0, rule hit ("<<addr0<<", "<<p->type<<") action 1"<<endl;
            }
            // reverse: if not hit and packet is sent to outside
            if(pro2addr[p->type].find(addr1) == pro2addr[p->type].end()){
                // install this rule to self table
                self_install_rule(addr1, p->type);
                if(p->dst.find("10.5.51") == string::npos){ // packet is to outside world
                    if(this->octane_counter != 0 && this->octane_counter%(global::drop_after) == 0){
                        send_octane_drop(p, p->dst_int, p->dstport, p->src_int, p->srcport, p->type);
                    }
                    else{ // send to outside world
                        send_octane_forward(p, p->dst_int, p->dstport, p->src_int, p->srcport, p->type);
                    }
                }
                else{
                    cout<<"After reverse src and dst, the dst is not outside"<<endl;
                }
            }
            // if hit
            else{
                // cout<<"hittttttttt"<<endl;
                logger<<"router: 0, rule hit ("<<addr1<<", "<<p->type<<") action 1"<<endl;
                cout<<"router: 0, rule hit ("<<addr1<<", "<<p->type<<") action 1"<<endl;
            }
            // send_octane_forward(p);

            if(this->stage >= 6){
                if(pro2addr[p->type].find(addr0) != pro2addr[p->type].end()){
                    logger<<"router: 0, rule hit ("<<addr0<<", "<<p->type<<") action 1"<<endl;
                    cout<<"router: 0, rule hit ("<<addr0<<", "<<p->type<<") action 1"<<endl;
                }
                if(pro2addr[p->type].find(addr1) != pro2addr[p->type].end()){
                    logger<<"router: 0, rule hit ("<<addr1<<", "<<p->type<<") action 1"<<endl;
                    cout<<"router: 0, rule hit ("<<addr1<<", "<<p->type<<") action 1"<<endl;
                }
            }
        }
        
    }

    void self_install_rule(string addr, int protocol){
        ostream& logger = Logger::getInstance();
        logger<<"router: 0, rule installed ("<<addr<<", "<<protocol<<") action 1"<<endl;
        cout<<"router: 0, rule installed ("<<addr<<", "<<protocol<<") action 1"<<endl;
        pro2addr[protocol].insert(addr);
    }

    void send_octane(Packer *p, int src, int srcport, int dst, int dstport, int type, int action){
        unsigned char* buffer = (unsigned char*)malloc(BUF_SIZE);
        memset(buffer, 0, sizeof(BUF_SIZE));
        struct iphdr *ori = p->get_iphdr();
        // utils::serialize_octane_messgae(buffer, ori);

        struct iphdr *iph = (struct iphdr*)buffer;
        *iph = *ori;
        iph->protocol = 253;
        cout<<"compare iph and ori: "<<endl; 
        cout<<iph->saddr<<" "<<ori->saddr<<endl;
        cout<<iph->daddr<<" "<<ori->daddr<<endl;
        
        struct octane_control* octane = (struct octane_control*)(buffer+sizeof(struct iphdr));
        // memset(octane, 0, sizeof(struct octane_control));
        octane->octane_action = action;
        octane->octane_flags = 0; // take an action
        octane->octane_seqno = this->octane_counter++;
        octane->octane_source_ip = src;
        octane->octane_dest_ip = dst;
        octane->octane_source_port = srcport;
        octane->octane_dest_port = dstport;
        octane->octane_protocol = type;
        octane->octane_port = 0;
        
        if(this->stage >= 6 && (action == 2 || action == 3)){
            int id = p->dst.back()-'0'-1;
            int ans = sendto(s, buffer, BUF_SIZE, 0, (struct sockaddr*) &routers_addr[id], BUF_SIZE);
            cout<<"Sent forward to primary: "<<ans<<endl;
        }
        else{
            int ans = sendto(s, buffer, BUF_SIZE, 0, (struct sockaddr*) &router_addr, BUF_SIZE);
            cout<<"Sent forward to primary: "<<ans<<endl;
        }
        
    }

    void send_octane_forward(Packer *p, int src, int srcport, int dst, int dstport, int type){
        cout<<"Send forward message to secondary"<<endl;
        send_octane(p, src, srcport, dst, dstport, type, 1);
    }

    void send_octane_reply(Packer *p, int src, int srcport, int dst, int dstport, int type){
        cout<<"Send reply message to secondary"<<endl;
        send_octane(p, src, srcport, dst, dstport, type, 2);
    }

    void send_octane_drop(Packer *p, int src, int srcport, int dst, int dstport, int type){
        cout<<"Send drop message to secondary"<<endl;
        send_octane(p, src, srcport, dst, dstport, type, 3);
    }

    void send_octane_remove(Packer *p, int src, int srcport, int dst, int dstport, int type){
        cout<<"Send remove message to secondary"<<endl;
        send_octane(p, src, srcport, dst, dstport, type, 4);
    }
};

#endif