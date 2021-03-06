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
 *
 * Heiko Folkerts, BSI 2018-2019
*/

//TCP and IP header parsers

//Helper function to parse IP Options. Returns tainted status, puts option data in hex string.
bool parse_ipopt(int opt_cpclno, const char* opt_name, \
                 unsigned char** opt_ptr_ptr, const unsigned char* beginofoptions_addr, const unsigned char* endofoptions_addr)
{
    int opt_len = *(*opt_ptr_ptr+1);
    //printf("3. opt_len: %d + *opt_ptr_ptr:%lx = sum:0x%lx > eof:0x%lx\n", opt_len, (unsigned long int) *opt_ptr_ptr,(unsigned long int) (*opt_ptr_ptr + opt_len), (unsigned long int) endofoptions_addr); //Debug
    if((*opt_ptr_ptr + opt_len) > endofoptions_addr) return true; //Check length, signal tainted in case of failure
    //Option data to hex string
    char* hex_string = print_hex_string(*opt_ptr_ptr + 2, opt_len - 2);  //Extract option data as hex string. Has to be freed!
    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "\"%s\": \"%s\", ", opt_name, hex_string); //JSON output
    free(hex_string);
    *opt_ptr_ptr += opt_len; //set pointer to next option
    return false; //Option not tainted.
}

int analyze_ip_header(const unsigned char* packet, struct pcap_pkthdr header)
{
    if(header.caplen - (ETHERNET_HEADER_LEN + IP_OR_TCP_HEADER_MINLEN) <= 0) return -1; //Malformed Paket
    //printf("Jacked a TCP-SYN with total length of [%d] and captured length [%d]\n\n", header.len, header.caplen);
    //print_hex(stdout, packet+14, header.caplen-14);

	struct iphdr *iphdr = (struct iphdr *)(packet + ETHERNET_HEADER_LEN);
    char* ip_saddr = inttoa(iphdr->saddr); //Must be freed!
    char* ip_daddr = inttoa(iphdr->daddr); //Must be freed!

    //printf("\n\nlength: %d\n version:%x\n TOS: %x\n tot_len: %d\nid: 0x%x\n flags: 0x%04x\n ttl: %d\n protocol: %d\n check: 0x%04x\n src_addr: %s dest_addr: %s\n\n",
    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), ", \
\"ip\": {\
\"length\": %d, \
\"version\": %d, \
\"tos\": \"0x%02x\", \
\"tot_len\": %d, \
\"id\": \"0x%04x\", \
\"flags\": \"0x%04x\", \
\"ttl\": %d, \
\"protocol\": %d, \
\"checksum\": \"0x%04x\", \
\"src_addr\": \"%s\", \
\"dest_addr\": \"%s\"",\
iphdr->ihl*4,\
iphdr->version,\
iphdr->tos,\
ntohs(iphdr->tot_len),\
ntohs(iphdr->id),\
ntohs(iphdr->frag_off),\
iphdr->ttl,\
iphdr->protocol,\
ntohs(iphdr->check),\
ip_saddr,\
ip_daddr
);
    /*
    if (iphdr->ihl > 5) //If Options/Padding present (IP Header bigger than 5*4 = 20Byte) print them as hexadecimal string
    {
        char* hex_string = print_hex_string((char*)iphdr + IP_OR_TCP_HEADER_MINLEN, iphdr->ihl*4 - IP_OR_TCP_HEADER_MINLEN);
        json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), ", \"options_hex\": \"%s\"", hex_string);
        free(hex_string);
    }
    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "}"); //close IP Header JSON object
    */

    //Parse IP options
    if (iphdr->ihl > 5) //If Options/Padding present (IP Header longer than 5*4 = 20Byte)
    {
        bool eol = false; //EOL reached ?
        bool tainted = false; //Is somethin unparsable inside the packet / options?
        //calculate begin / end of options
        const unsigned char* beginofoptions_addr = (unsigned char*) packet + ETHERNET_HEADER_LEN + IP_OR_TCP_HEADER_MINLEN;
        const unsigned char* endofoptions_addr = packet + ETHERNET_HEADER_LEN + iphdr->ihl*4;
        if(endofoptions_addr > (packet + header.caplen))
        {
            //Malformed Paket
            endofoptions_addr = (packet + header.caplen); //Repair end of options address
            tainted = true; //mark as tainted, thus do not parse, just dump hexstring
            //printf("1. IP Tainted: %d\n", tainted); //Debug
        }
        //set pointer to beginning of options
        unsigned char* opt_ptr = (unsigned char*) beginofoptions_addr;
        json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), ", \"ip_options\": {"); //open json
        while(!tainted && !eol && opt_ptr < (packet + ETHERNET_HEADER_LEN + iphdr->ihl*4))
        {
            //printf("2. IP Tainted: %d Type:%d\tAddr:0x%lx\n", tainted, *opt_ptr, (long int) opt_ptr); //Debug
            switch(*opt_ptr)
            {
                case  MY_IPOPT_EOOL: //EOL is only one byte, so this is hopefully going to be easy.
                    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "\"EOL\": \"\", ");
                    opt_ptr++;
                    eol = true;
                    break;
                case MY_IPOPT_NOP: //NOP is only one byte, so this is going to be easy, too
                    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "\"NOP\": \"\" , ");
                    opt_ptr++;
                    break;
                case MY_IPOPT_SEC:
                    tainted =  parse_ipopt(MY_IPOPT_SEC, "sec", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    //printf("3. IP Tainted: %d Type:%d\tAddr:0x%lx\n", tainted, *opt_ptr, (long int) opt_ptr); //Debug
                    break;
                case MY_IPOPT_LSR:
                    tainted =  parse_ipopt(MY_IPOPT_LSR, "lsr", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_TS:
                    tainted =  parse_ipopt(MY_IPOPT_TS, "ts", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_ESEC:
                    tainted =  parse_ipopt(MY_IPOPT_ESEC, "esec", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_CIPSO:
                    tainted =  parse_ipopt(MY_IPOPT_CIPSO, "cipso", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_RR:
                    tainted =  parse_ipopt(MY_IPOPT_RR, "rr", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_SID:
                    tainted =  parse_ipopt(MY_IPOPT_SID, "sid", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_SSR:
                    tainted =  parse_ipopt(MY_IPOPT_SSR, "ssr", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_ZSU:
                    tainted =  parse_ipopt(MY_IPOPT_ZSU, "zsu", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_MTUP:
                    tainted =  parse_ipopt(MY_IPOPT_MTUP, "mtup", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_MTUR:
                    tainted =  parse_ipopt(MY_IPOPT_MTUR, "mtur", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_FINN:
                    tainted =  parse_ipopt(MY_IPOPT_FINN, "finn", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_VISA:
                    tainted =  parse_ipopt(MY_IPOPT_VISA, "visa", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_ENCODE:
                    tainted =  parse_ipopt(MY_IPOPT_ENCODE, "encode", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_IMITD:
                    tainted =  parse_ipopt(MY_IPOPT_IMITD, "IMITD", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_EIP:
                    tainted =  parse_ipopt(MY_IPOPT_EIP, "eip", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_TR:
                    tainted =  parse_ipopt(MY_IPOPT_TR, "tr", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_ADDEXT:
                    tainted =  parse_ipopt(MY_IPOPT_ADDEXT, "addext", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_RTRALT:
                    tainted =  parse_ipopt(MY_IPOPT_RTRALT, "rtralt", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_SDB:
                    tainted =  parse_ipopt(MY_IPOPT_SDB, "sdb", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_UN:
                    tainted =  parse_ipopt(MY_IPOPT_UN, "un", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_DPS:
                    tainted =  parse_ipopt(MY_IPOPT_DPS, "dps", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_UMP:
                    tainted =  parse_ipopt(MY_IPOPT_UMP, "ump", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_QS:
                    tainted =  parse_ipopt(MY_IPOPT_QS, "qs", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_IPOPT_EXP:
                    tainted =  parse_ipopt(MY_IPOPT_EXP, "exp", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                default:
                    tainted = true; //Somthing is wrong or not implemented, so this will break the while-loop.
                    break;
           } //End of switch statement
        } //End of loop

        //output tainted status, hex output (even if not tainted, cause padding might be usefull too) and close json
        char* hex_string = print_hex_string(opt_ptr, endofoptions_addr-opt_ptr);    
        json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), \
"\"tainted\": %d, \"padding_hex\": \"%s\"}", tainted, hex_string);
            
        free(hex_string);
    } //End of if
    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "}"); //close JSON object
    //free
    free(ip_saddr);
    free(ip_daddr);
    return 0;
}

//Helper function to parse TCP Options having a length. Returns tainted status, puts option data in hex string
bool parse_tcpopt_w_length(int opt_kind, int opt_len, const char* opt_name, \
                           unsigned char** opt_ptr_ptr, const unsigned char* beginofoptions_addr, const unsigned char* endofoptions_addr)
{
    if( *(*opt_ptr_ptr+1) != opt_len || (*opt_ptr_ptr + opt_len) > endofoptions_addr) return true; //Check length, signal tainted in case of failure
    //Option data to hex string
    char* hex_string = print_hex_string(*opt_ptr_ptr + 2, opt_len - 2);  //Extract option data as hex string. Has to be freed!
    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "\"%s\": \"%s\", ", opt_name, hex_string); //JSON output
    free(hex_string);
    *opt_ptr_ptr += opt_len; //set pointer to next option
    return false; //Option not tainted.
}

int analyze_tcp_header(const unsigned char* packet, struct pcap_pkthdr header)
{
    struct iphdr *iphdr = (struct iphdr *)(packet + ETHERNET_HEADER_LEN);
    if(header.caplen - (ETHERNET_HEADER_LEN + iphdr->ihl*4 + IP_OR_TCP_HEADER_MINLEN) < 0) return -1; //Malformed Paket
    //printf("Jacked a TCP-SYN with total length of [%d] and captured length [%d]\n\n", header.len, header.caplen);
    //print_hex(stdout, packet+14, header.caplen-14);

    struct tcphdr *tcphdr = (struct tcphdr *) (packet + ETHERNET_HEADER_LEN + iphdr->ihl*4);

    //calculate eventually exisiting data bytes in SYN (yes, this would be akward)
    long int data_bytes = header.caplen - (ETHERNET_HEADER_LEN + iphdr->ihl*4 + tcphdr->doff*4);

   //Append header JSON
    //printf("\n\nsrc_port: %d\ndest_port: %d\nseq: %u\nack_seq: %u\nhdr_len: %d\nres1: %x\nres2: %x\nurg: %x\nack: %x\npsh: %x\nrst: %x\nsyn: %x\nfin: %x\nwindow: %d\ncheck: 0x%02x\nurg_ptr: 0x%04x\n\n",\
printf("\n\n
    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), ", \
\"tcp\": {\
\"src_port\": %d, \
\"dest_port\": %d, \
\"seq\": %u, \
\"ack_seq\": %u, \
\"hdr_len\": %d, \
\"res1\": %x, \
\"ecn\": %s, \
\"cwr\": %s, \
\"non\": %s, \
\"urg\": %s, \
\"ack\": %s, \
\"psh\": %s, \
\"rst\": %s, \
\"syn\": %s, \
\"fin\": %s, \
\"tcp_flags\": \"%x\", \
\"window\": %d, \
\"checksum\": \"0x%02x\", \
\"urg_ptr\": \"0x%04x\"",\
ntohs(tcphdr->source),\
ntohs(tcphdr->dest),\
ntohl(tcphdr->seq),\
ntohl(tcphdr->ack_seq),\
tcphdr->doff*4,\
tcphdr->res1,\
(tcphdr->res2 & 0b001 ? "true" : "false"),\
(tcphdr->res2 & 0b010 ? "true" : "false"),\
(tcphdr->res2 & 0b100 ? "true" : "false"),\
(tcphdr->urg ? "true" : "false"),\
(tcphdr->ack ? "true" : "false"),\
(tcphdr->psh ? "true" : "false"),\
(tcphdr->rst ? "true" : "false"),\
(tcphdr->syn ? "true" : "false"),\
(tcphdr->fin ? "true" : "false"),\
tcphdr->res1 << 10 | tcphdr->res2 << 6 | tcphdr->urg << 5 | tcphdr->ack << 4 | tcphdr->psh << 3 | tcphdr->rst << 2 | tcphdr->syn << 1| tcphdr->fin,\
ntohs(tcphdr->window),\
ntohs(tcphdr->check),\
ntohs(tcphdr->urg_ptr));

    /* Output as hexstring only
    if (tcphdr->doff > 5) //If Options/Padding present (TCP Header longer than 5*4 = 20Byte) print them as hexadecimal string
    {
        char* hex_string = print_hex_string((char*)tcphdr + IP_OR_TCP_HEADER_MINLEN, tcphdr->doff*4 - IP_OR_TCP_HEADER_MINLEN);
        json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), ", \"tcp_options_hex\": \"%s\"", hex_string);
        free(hex_string);

    }
    */
    //Parse TCP options
    if (tcphdr->doff > 5) //If Options/Padding present (TCP Header longer than 5*4 = 20Byte)
    {
        bool eol = false; //EOL reached ?
        bool tainted = false; //Is somethin unparsable inside the packet / options?
        //calculate begin / end of options
        const unsigned char* beginofoptions_addr = (unsigned char*) packet + ETHERNET_HEADER_LEN + iphdr->ihl*4 + IP_OR_TCP_HEADER_MINLEN;       
        const unsigned char* endofoptions_addr = packet + ETHERNET_HEADER_LEN + iphdr->ihl*4 + tcphdr->doff*4;
        //Malformed Paket? : End of header (tcpheader->doff) may be tainted.
        if(endofoptions_addr > (packet + header.caplen))
        {
            endofoptions_addr = (packet + header.caplen); //Repair end of options address
            tainted = true; //mark as tainted, thus do not parse, just dump hexstring
            //printf("1. TCP Tainted: %d\n", tainted); //Debug
        }
        //set pointer to beginning of options
        unsigned char* opt_ptr = (unsigned char*) beginofoptions_addr;
        json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), ", \"tcp_options\": {"); //open json
        while(!tainted && !eol && opt_ptr < (packet + ETHERNET_HEADER_LEN + iphdr->ihl*4 + tcphdr->doff*4))
        {
            //printf("2. TCP Tainted: %d kind:%d\tAddr:0x%lx\n", tainted, *opt_ptr, (long int) opt_ptr); //Debug
            switch(*opt_ptr)
            {
                case MY_TCPOPT_NOP: //EOL is only one byte, so this is hopefully going to be easy.
                    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "\"NOP\": \"\", ");
                    opt_ptr++;
                    break;
                case MY_TCPOPT_EOL: //EOL is only one byte, so this is going to be easy, too
                    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), "\"EOL\": \"\", ");
                    opt_ptr++;
                    eol = true;
                    break;
                case MY_TCPOPT_MSS:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_MSS, MY_TCPOLEN_MSS, "mss" ,&opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_WINDOW:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_WINDOW, MY_TCPOLEN_WINDOW, "window", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_SACK_PERM:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_SACK_PERM, MY_TCPOLEN_SACK_PERM, "sack_perm", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_ECHO:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_ECHO, MY_TCPOLEN_ECHO, "echo", &opt_ptr, beginofoptions_addr, endofoptions_addr);                      
                    break;
                case MY_TCPOPT_ECHOREPLY:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_ECHOREPLY, MY_TCPOLEN_ECHOREPLY, "echo_reply", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_TIMESTAMP:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_TIMESTAMP, MY_TCPOLEN_TIMESTAMP, "timestamp", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_CC:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_CC, MY_TCPOLEN_CC, "cc", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_CCNEW:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_CCNEW, MY_TCPOLEN_CCNEW, "ccnew", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_CCECHO:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_CCECHO, MY_TCPOLEN_CCECHO, "ccecho", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_MD5:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_MD5, MY_TCPOLEN_MD5, "md5", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_SCPS:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_SCPS, MY_TCPOLEN_SCPS, "scps", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_SNACK:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_SNACK, MY_TCPOLEN_SNACK, "snack", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_RECBOUND:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_RECBOUND, MY_TCPOLEN_RECBOUND, "recbound", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_CORREXP:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_CORREXP, MY_TCPOLEN_CORREXP, "correxp", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_QS:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_QS, MY_TCPOLEN_QS, "qs", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                case MY_TCPOPT_USER_TO:
                    tainted = parse_tcpopt_w_length(MY_TCPOPT_USER_TO, MY_TCPOLEN_USER_TO, "user_TO", &opt_ptr, beginofoptions_addr, endofoptions_addr);
                    break;
                default:
                    tainted = true; //Somthing is wrong or not implemented, so this will break the while-loop.
                    break;
           } //End of switch statement
        } //End of loop

        //output tainted status, hex output (even if not tainted, cause padding might be usefull too) and close json
        char* hex_string = print_hex_string(opt_ptr, endofoptions_addr-opt_ptr);
        json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), \
"\"tainted\": %d, \"padding_hex\": \"%s\"}", tainted, hex_string);
            
        free(hex_string);
    } //End of "if options present"
    return data_bytes;
}

