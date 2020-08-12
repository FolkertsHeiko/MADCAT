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
 * UDP port monitor.
 *
 * Netfilter should be configured to block outgoing ICMP Destination unreachable (Port unreachable) packets, e.g.:
 *      iptables -I OUTPUT -p icmp --icmp-type destination-unreachable -j DROP
 *
 * Heiko Folkerts, BSI 2018-2019
*/

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

int analyze_ip_header(const unsigned char* packet, int recv_len)
{
    if(recv_len - (IP_OR_TCP_HEADER_MINLEN) <= 0) return -1; //Malformed Paket
    //printf("Jacked a TCP-SYN with total length of [%d] and captured length [%d]\n\n", header.len, recv_len);
    //print_hex(stdout, packet+14, recv_len-14);

	struct iphdr *iphdr = (struct iphdr *)(packet);
    char* ip_saddr = inttoa(iphdr->saddr); //Must be freed!
    char* ip_daddr = inttoa(iphdr->daddr); //Must be freed!

    //printf("\n\nlength: %d\n version:%x\n TOS: %x\n tot_len: %d\nid: 0x%x\n flags: 0x%04x\n ttl: %d\n protocol: %d\n check: 0x%04x\n src_addr: %s dst_addr: %s\n\n",
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
ip_daddr\
);

    /*
    if (iphdr->ihl > 5) //If Options/Padding present (IP Header bigger than 5*4 = 20Byte) print them as hexadecimal string
    {
        char* hex_string = print_hex_string((char*)iphdr + IP_OR_TCP_HEADER_MINLEN, iphdr->ihl*4 - IP_OR_TCP_HEADER_MINLEN);
        json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), ", \"ip_options_hex\": \"%s\"", hex_string);
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
        if(endofoptions_addr > (packet + recv_len))
        {
            //Malformed Paket
            endofoptions_addr = (packet + recv_len); //Repair end of options address
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

int analyze_udp_header(const unsigned char* packet, int recv_len)
{
    struct iphdr *iphdr = (struct iphdr *)(packet);
    
    if(recv_len - (iphdr->ihl*4 + UDP_HEADER_LEN) <= 0) return -1; //Malformed Paket
    //print_hex(stdout, packet + UDP_HEADER_LEN, recv_len - UDP_HEADER_LEN); //Debug

    struct udphdr *udphdr = (struct udphdr *) (packet + iphdr->ihl*4);

    json_ptr += snprintf(json_ptr, JSON_BUF_SIZE - (json_ptr - global_json), ", \
\"udp\": {\
\"src_port\": %d ,\
\"dest_port\": %d ,\
\"len\": %d ,\
\"checksum\": \"0x%04x\"\
}",\
ntohs(udphdr->source),\
ntohs(udphdr->dest),\
ntohs(udphdr->len),\
ntohs(udphdr->check)\
);
    return 0;
}
