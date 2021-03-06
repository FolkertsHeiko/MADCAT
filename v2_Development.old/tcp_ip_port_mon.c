/*******************************************************************************
This file is part of MADCAT, the Mass Attack Detection Acceptance Tool.

    MADCAT is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MADCAT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MADCAT.  If not, see <http://www.gnu.org/licenses/>.

    Diese Datei ist Teil von MADCAT, dem Mass Attack Detection Acceptance Tool.

    MADCAT ist Freie Software: Sie können es unter den Bedingungen
    der GNU General Public License, wie von der Free Software Foundation,
    Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
    veröffentlichten Version, weiter verteilen und/oder modifizieren.

    MADCAT wird in der Hoffnung, dass es nützlich sein wird, aber
    OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
    Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
    Siehe die GNU General Public License für weitere Details.

    Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
    Programm erhalten haben. Wenn nicht, siehe <https://www.gnu.org/licenses/>.
*******************************************************************************/
/* MADCAT - Mass Attack Detecion Connection Acceptance Tool
 * TCP-IP port monitor.
 *
 * Example Netfilter Rule to work properly:
 *       iptables -t nat -A PREROUTING -i enp0s8 -p tcp --dport 1:65534 -j DNAT --to 192.168.8.42:65535
 * Listening Port is 65535 and hostaddress is 192.168.8.42 in this example.
 *
 * Compile with libpcap and includes from local directory:
 * gcc -I . -I/usr/include/luajit-2.1/ -Wl,-rpath,/usr/local/lib -o tcp_ip_port_mon tcp_ip_port_mon.c -lluajit-5.1 -lpcap -pthread
 *
 * Heiko Folkerts, BSI 2018-2019
*/

//Header includes, defintions and globals
#include <tcp_ip_port_mon.h>

//Main

int main(int argc, char *argv[])
{
        //Start time
        char start_time[64] = ""; //Human readable start time (actual time zone)
        char start_time_unix[64] = ""; //Unix timestamp (UTC)
        struct timeval begin , now;
        gettimeofday(&begin , NULL);
        time_str(NULL, 0, start_time, sizeof(start_time)); //Get Human readable string only

        //semaphores for output globally defined for easy access inside functions
        //sem_t *hdrsem; //Semaphore for named pipe containing TCP/IP data
        //sem_t *consem; //Semaphore for named pipe containing connection data

        sem_unlink ("hdrsem");
        sem_unlink ("consem");

        //char* global_json = 0; //JSON Output defined global, to make all information visibel to functions for concatination and output.

        //Parse command line. 
        //char hostaddr[INET6_ADDRSTRLEN] = ""; Hostaddress to bind to. Globally defined to make it visible to functions for filtering.
        int port = 65535;
        char interface[16]= "";
        double timeout = 30;
        struct user_t user;
        char data_path[PATH_LEN] = "";
        int max_file_size = -1;
        int max_conn = 0;

        // Checking if number of arguments is
        // 5 or 6 or not.
        if (argc < 7 || argc > 8)
        {
                fprintf(stderr, "%s\nSYNTAX:\n    %s interface hostaddress listening_port connection_timeout user path_to_save_tcp-streams [max_file_size]\n\
    Path to directory MUST end with a trailing slash, e.g.  \"/path/to/my/dir/\"\n\
    The last paramteter, max_file_size, is the maximum size of saved streams,\n\
      but the last TCP Datagramm exceeding this size will be saved anyway.\n", VERSION, argv[0]);
                fprintf(stderr,"\nExample Netfilter Rule to work properly:\n\
    iptables -t nat -A PREROUTING -i enp0s8 -p tcp --dport 1:65534 -j DNAT --to 192.168.8.42:65535\n\
      Listening Port is 65535 and hostaddress is 192.168.8.42 in this example.\n\n\
Must be run as root, but the priviliges will be droped to \"user\".\n\n\
Opens two named pipes (FiFo) containing live JSON output:\n\
    \"%s\" for stream connection data, \"%s\" for header data.\n", CONNECT_FIFO, HEADER_FIFO);
                return -1;
        }

        //copy arguments to variables
        strncpy(interface, argv[1], sizeof(interface)); hostaddr[sizeof(interface)-1] = 0;  //copy hostaddress and ensure null termination of this string. Ugly.
        strncpy(hostaddr, argv[2], sizeof(hostaddr)); hostaddr[sizeof(hostaddr)-1] = 0;
        port = atoi(argv[3]); //convert string type to integer type (port)
        timeout = (double) atof(argv[4]); //set timeout and convert to integer type.
        strncpy(user.name, argv[5], sizeof(user.name)); user.name[sizeof(user.name)-1] = 0;
        strncpy(data_path, argv[6], sizeof(data_path)); data_path[sizeof(data_path)-1] = 0;

        if (argc == 8) //get max. file-size.
        {
            max_file_size = atoi(argv[7]);
        }

        if(port < 1 || port > 65535) //Range checks
        {
                fprintf(stderr, "Port %d out of range.\n", port);
                return -2;
        }

        fprintf(stderr, "%s\n%s [PID %d] Starting on interface %s with hostaddress %s on port %d, timeout is %lfs...\n", VERSION, start_time, getpid(), interface, hostaddr, port, timeout);

        //Variabels for PCAP sniffing

        pcap_t *handle; //pcap Session handle 
        struct pcap_pkthdr header; // The pcap header it gives back
        const unsigned char* packet; //The Packet from pcap
        //int pcap_pid = 0; //PID of the Child doing the PCAP-Sniffing. Globally defined, cause it's used in CHECK-Makro.
        //int accept_pid = 0; //PID of the Child doing the TCP Connection handling. Globally defined, cause it's used in CHECK-Makro.
        int parent_pid = getpid();

        CHECK(signal(SIGINT, sig_handler_parent), != SIG_ERR); //register handler for SIGINT for parent process
        CHECK(signal(SIGTERM, sig_handler_parent), != SIG_ERR); //register handler for SIGTERM for parent process

        //Fork in child, init pcap , drop priviliges, sniff for SYN-Packets and log them

        if( !(pcap_pid=fork()) )
        {
            prctl(PR_SET_PDEATHSIG, SIGTERM); //request SIGTERM if parent dies.
            CHECK(signal(SIGTERM, sig_handler_child), != SIG_ERR); //re-register handler for SIGTERM for child process
            #if DEBUG >= 2
                fprintf(stderr, "*** DEBUG [PID %d] Initialize PCAP\n", getpid());
            #endif
            CHECK(init_pcap(interface, &handle), == 0); //Init libpcap

            hdrsem = CHECK(sem_open ("hdrsem", O_CREAT | O_EXCL, 0644, 1), !=  SEM_FAILED);  //open semaphore for named pipe containing TCP/IP data

            fprintf(stderr, "%s [PID %d] ", start_time, getpid());
            drop_root_privs(user, "Sniffer"); //drop priviliges

            //Make FIFO for header discribing JSON Output
            unlink(HEADER_FIFO);
            CHECK(mkfifo(HEADER_FIFO, 0660), == 0);
            FILE* hdrfifo = fopen(HEADER_FIFO, "r+");
            fprintf(stderr, "%s [PID %d] FIFO for header JSON: %s\n", start_time, getpid(), HEADER_FIFO);

            int data_bytes = 0; //eventually exisiting data bytes in SYN (yes, this would be akward)
            while (1)
            {
                packet = 0;
                packet = pcap_next(handle, &header); //Wait for and grab TCP-SYN (see PCAP_FILTER) (Maybe of maybe not BLOCKING!)
                if (packet == 0) {continue;}
                //Preserve actuall start time of Connection attempt.
                time_str(start_time_unix, sizeof(start_time_unix), start_time, sizeof(start_time));
                //Begin new global JSON output and...
                json_ptr = global_json;
                memset(json_ptr, 0, JSON_BUF_SIZE);
                //open JSON object
                json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "{\"timestamp\": \"%s\"", start_time); 
                //Analyze Headers and discard malformed packets
                if(analyze_ip_header(packet, header) < 0) {continue;}
                data_bytes = analyze_tcp_header(packet, header);
                if(data_bytes < 0) {continue;}
                //JSON Ouput and close JSON object
                json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "}, \"data_bytes\": %d, \"unixtime\": %s}", data_bytes, start_time_unix);
                sem_wait(hdrsem); //Acquire lock for output
                fprintf(hdrfifo,"%s\n", global_json); //print json output for further analysis
                sem_post(hdrsem); //release lock
                fprintf(stdout,"{\"HEADER\": %s}\n", global_json); //print json output for logging
                fflush(hdrfifo);
                fflush(stdout);
            }
        }

/**********************************TODO: RSP***********************************/
        //Make FIFO for connection discribing JSON Output, both accesible for RSP and accept Childs
        unlink(CONNECT_FIFO);
        CHECK(mkfifo(CONNECT_FIFO, 0660), == 0);
        FILE* confifo = fopen(CONNECT_FIFO, "r+");
        fprintf(stderr, "%s [PID %d] FIFO for connection json: %s\n", start_time, getpid(), CONNECT_FIFO);
        consem = CHECK(sem_open ("consem", O_CREAT | O_EXCL, 0644, 1), !=  SEM_FAILED);

        if ( !fork() )//Create Reverse Proxy child process(es)
        {
            prctl(PR_SET_PDEATHSIG, SIGTERM); //request SIGTERM if parent dies.
            CHECK(signal(SIGTERM, sig_handler_child), != SIG_ERR); //register handler for SIGTERM for child process
            CHECK(rsp("./tcp.conf/8080.lua"), != 0);
            exit(0);
        }
/******************************************************************************/
        if ( !(accept_pid =fork()) ) { //Create listening child process
            //Variables for listning socket
            struct sockaddr_in addr; //Hostaddress
            struct sockaddr_in trgaddr; //Storage for original destination port
            struct sockaddr_storage claddr; //Clientaddress
            char clientaddr[INET6_ADDRSTRLEN] = "";

            prctl(PR_SET_PDEATHSIG, SIGTERM); //request SIGTERM if parent dies.
            CHECK(signal(SIGTERM, sig_handler_child), != SIG_ERR); //re-register handler for SIGTERM for child process
            CHECK(signal(SIGCHLD, sig_handler_sigchld), != SIG_ERR); //register handler for parents to prevent childs becoming Zombies

            accept_pid = getpid();

            socklen_t trgaddr_len = sizeof(trgaddr);
            socklen_t claddr_len = sizeof(claddr);
            socklen_t addr_len = sizeof(addr);
            int listenfd = CHECK(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), != -1); //create socket filedescriptor

            //Initialize address struct (Host)
            bzero(&addr, addr_len);
            addr.sin_family=AF_INET;
            CHECK(inet_aton(hostaddr, &addr.sin_addr), != 0); //set and check listening address
            addr.sin_port = htons(port); //set listening port

            struct linger sl = { 1, 5 };
            int on = 1;

            CHECK(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, (socklen_t)sizeof(on)), != -1);
            CHECK(setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &sl, (socklen_t)sizeof(sl)), != -1);

            //Bind socket and begin listening
            CHECK(bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)), != -1);
            CHECK(listen(listenfd, 5), != -1);

            fprintf(stderr, "%s [PID %d] ", start_time, getpid());
            drop_root_privs(user, "Listner");
        
            //Main listening loop
            while (1) {
	                #if DEBUG >= 2
	                    fprintf(stderr, "*** DEBUG [PID %d] Listner Loop\n", getpid());
	                #endif

                    int openfd = CHECK(accept(listenfd, (struct sockaddr*)&claddr, &claddr_len), != -1);  //Accept incoming connection
                    if (!fork()) { //Create stream accepting child process
	                        #if DEBUG >= 2
	                            fprintf(stderr, "*** DEBUG [PID %d] Accept-Child forked\n", getpid());
	                        #endif	
                            prctl(PR_SET_PDEATHSIG, SIGTERM); //request SIGTERM if parent dies.
                            CHECK(signal(SIGTERM, sig_handler_child), != SIG_ERR); //register handler for SIGTERM for child process
                            //Preserve actual start time of connection attempt.
                            time_str(start_time_unix, sizeof(start_time_unix), start_time, sizeof(start_time));
                            CHECK(getsockopt(openfd, SOL_IP, SO_ORIGINAL_DST, (struct sockaddr*)&trgaddr, &trgaddr_len), != -1); //Read original dst. port from NAT-table
                            struct sockaddr_in *s = (struct sockaddr_in *)&claddr; //create temporary struct to call inet_ntop() properly
                            //retrieve client target IPv4 (important when listening on ANY_ADDR)
                            inet_ntop(AF_INET, &s->sin_addr, clientaddr, sizeof clientaddr);
                            //do stuff: Log, save stream, hold connection, timeout and JSON output
	                        #if DEBUG >= 2
	                            fprintf(stderr, "*** DEBUG [PID %d] Accept-Child entering Worker with destination port %d.\n", getpid(), ntohs(trgaddr.sin_port));
	                        #endif                            
                            /*
                            long int do_stuff(char* dst_addr, int dst_port, char* src_addr, int src_port, double timeout, char* data_path, int max_file_size, int s,\
                                                           char* start_time, char* start_time_unix, FILE* confifo)
                            */
                            long int data_bytes = do_stuff(inet_ntoa(trgaddr.sin_addr), ntohs(trgaddr.sin_port), clientaddr, ntohs(s->sin_port),\
                                                           timeout, data_path, max_file_size, openfd, start_time, start_time_unix, confifo);
	                        #if DEBUG >= 2
	                            fprintf(stderr, "*** DEBUG [PID %d] Accept-Child left Worker\n", getpid());
	                        #endif
                            //Shutdown child process
                            close(openfd); //Close connection
	                        #if DEBUG >= 2
	                            fprintf(stderr, "*** DEBUG [PID %d] Accept-Child openfd closed, returning.\n", getpid());
	                        #endif
                            exit(0); //kill child process
                    }
                    close(openfd); //Close connection
            }

        } else {
            sleep(2);
            //Log start of Watchdog
            gettimeofday(&begin , NULL);
            time_str(NULL, 0, start_time, sizeof(start_time)); //Get Human readable string only
            fprintf(stderr, "%s [PID %d] ", start_time, getpid());
            drop_root_privs(user, "Parent Watchdog");

            // Parent Watchdog Loop.
            int stat_pcap = 0;
            int stat_accept = 0;
            while (1) {
                //fprintf(stderr, "waitpid pcap: P:%d S:%d\n", waitpid(pcap_pid, &stat_pcap, WNOHANG), stat_pcap);
                //fprintf(stderr, "waitpid accept: P:%d S:%d\n", waitpid(accept_pid, &stat_accept, WNOHANG), stat_pcap);
                if ( waitpid(pcap_pid, &stat_pcap, WNOHANG) ) {
                    gettimeofday(&begin , NULL);
                    time_str(NULL, 0, start_time, sizeof(start_time)); //Get Human readable string only, reuse start_time var.
                    fprintf(stderr, "%s [PID %d] Sniffer (PID %d) crashed", start_time, getpid(), accept_pid);
                    sig_handler_parent(SIGTERM);
                    break;
                }
                if ( waitpid(accept_pid, &stat_accept, WNOHANG) ) {
                    gettimeofday(&begin , NULL);
                    time_str(NULL, 0, start_time, sizeof(start_time)); //Get Human readable string only, reuser start_time var. 
                    fprintf(stderr, "%s [PID %d] Listner (PID %d) crashed", start_time, getpid(), accept_pid);
                    sig_handler_parent(SIGTERM);
                    break;
                }
                sleep(2);
            }
        }

        return 0;
}

