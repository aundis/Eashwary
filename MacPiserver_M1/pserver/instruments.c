/*
 * instruments.c
 * com.apple.instruments.remoteserver implementation.
 *
 */

#include <stdio.h>
#include <plist/plist.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
//#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "instruments.h"
#include <libimobiledevice/lockdown.h>
#include <debug.h>

char commandprocessPath[150];
char commandline_string[1024*1024] = {0,};

extern char b_id[50];
static fptrCommandResponseQ addCommandResponseQFptr =  NULL;
static fptrCommandResponseQ getCommandResponseQFptr =  NULL;
static fptrCommandResponseQ getCommandResponseQLenFptr =  NULL;
struct instruments_worker_thread *iwtScreen = NULL;

void RegisterAddCommandResponseQ(fptrCommandResponseQ fp);
void RegisterGetCommandResponseQ(fptrCommandResponseQ fp);
void RegisterGetCommandResponseQLen(fptrCommandResponseQ fp);

static instruments_error_t instruments_receive_data(
                                                    instruments_client_t client, char **bytes,
                                                    uint32_t *bytes_recv, uint32_t *packet_num);

static instruments_error_t instruments_receive_data_with_timeout(instruments_client_t client, char **bytes, uint32_t *bytes_recv, uint32_t *packet_num, int timeout);

void MemoryFree(void** ptr);
void workerIntHandler(int param);
/*
 sig_atomic_t worker_signaled = 0;
 
 void workerIntHandler(int param) {
 worker_signaled = 1;
 }
 */

struct instruments_worker_thread {
    instruments_client_t client;
    instruments_receive_cb_t cbfunc;
    void *user_data;
    char*packagepath;
    uint32_t reply_packet_num;
};

void *instruments_worker(void *arg)
{
    char *header=NULL;
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    struct instruments_worker_thread *iwt = (struct instruments_worker_thread*)arg;
    iwtScreen = iwt;
    
    if (!iwt)
        return NULL;
    
    RequestPacket *response_header2;
    InstrumentsResponse *response;
    response = (InstrumentsResponse*) malloc (sizeof(InstrumentsResponse));
    if (!response)
        return NULL;
    
    response_header2 = (RequestPacket*) malloc (sizeof(RequestPacket));
    
    if (!response_header2)
    {
        MemoryFree(&response);
        return NULL;
    }
    
    //debug_info("Running");
    
    while (iwt->client->parent)
    {
        //struct timeval tv;
        
        MemoryFree(&header);
        
        //fprintf(stderr, "Inside worker\n");
        
        res = instruments_receive_data(iwt->client, &header, &bytes_recv, &reply_packet_num);
        //debug_info("reply packet no %d\n",reply_packet_num);
        if (res < 0)
        {
            //printf("reply packet no %d result %d\n",reply_packet_num,res);
            break;
        }
        
        if (res==INSTRUMENTS_E_SUCCESS)
        {
            plist_t response_plist=NULL;
            //debug_info("before memcpy\n");
            memcpy(response,header,sizeof(InstrumentsResponse));
            //debug_info("after memcpy\n");
            InstrumentsResponse_from_LE(response);
            //debug_info("response_length %d",response->length);
            //printf("response_length %d response_msg_type %d",response->length,response->msg_type);
            if (response->length>sizeof(RequestPacket) && response->msg_type ==0x1002)
            {
                memcpy(response_header2,header+sizeof(InstrumentsResponse),sizeof(RequestPacket));
                RequestPacket_from_LE(response_header2);
                if (response_header2->bplist_size > 0)
                {
                    plist_from_bin(header+sizeof(InstrumentsResponse)+sizeof(RequestPacket),response_header2->bplist_size,&response_plist);
                    if (response_plist!=NULL)
                    {
                        //debug_plist(response_plist);
                        plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                        plist_t pid_plist = plist_array_get_item(response_object,1);
                        if (plist_get_node_type(pid_plist)==PLIST_STRING)
                        {
                            char *response_string;
                            plist_get_string_val(pid_plist,&response_string);
                            if (strcmp(response_string,"waitbufferprocessing.sh")==0)
                            {
                                char *std_out=NULL;
                                
                                //gettimeofday(&tv,NULL);
                                //fprintf(stderr, "[Instrument worker] #######Time at request to be sent  %u\n", tv.tv_usec);
                                //fprintf(stderr, "[Instrument worker] #######Time at request to be sent in sec %u\n", tv.tv_sec);
                                iwt->cbfunc(&std_out, iwt->user_data,0);
                                if ((std_out) && (std_out[0]=='H'))
                                {
                                    uint64_t pid;
                                    instruments_error_t ierr;
                                    ierr = instruments_launch_app(iwt->client,5,iwt->packagepath,b_id,&pid);
                                    //instruments_error_t ierr = instruments_launch_app(iwt->client,5,iwt->packagepath,"com.techendeavour.WatchPOC.Retail",&pid);
                                    if (ierr != INSTRUMENTS_E_SUCCESS) {
                                        //printf("ERROR: while relaunching app.\n"); // this is normal as the app is already launch
                                    }
                                    ierr = instruments_pid_ops(iwt->client,5,pid,"resumePid:");
                                    
                                    if (ierr != INSTRUMENTS_E_SUCCESS) {
                                        printf("ERROR: Resume pid failed.\n");
                                    }
                                    
                                }
                                else if ((std_out) && (std_out[0]=='X'))
                                {
                                    instruments_stop_script(iwt->client,9);
                                    if (std_out) MemoryFree(&std_out);
                                    std_out=NULL;
                                    if (response_string) MemoryFree(&response_string);
                                    plist_free(response_plist);
                                    break;
                                }
                                instruments_send_stdout(iwt->client,0xfffffff7,reply_packet_num,std_out);
                                //gettimeofday(&tv,NULL);
                                //fprintf(stderr, "[Instrument worker] #######Time at request sent %u\n", tv.tv_usec);
                                //fprintf(stderr, "[Instrument worker] #######Time at request sent in sec %u\n\n", tv.tv_sec);
                                if (std_out) MemoryFree(&std_out);
                            }
                            if (response_string) MemoryFree(&response_string);
                        }
                        plist_free(response_plist);
                        response_plist=NULL;
                    }
                }
            }
            /*
             else if (response->length>0 && response->msg_type == 0x3 )
             {
             plist_from_bin(header+sizeof(InstrumentsResponse),response->length,&response_plist);
             if (response_plist!=NULL)
             {
             plist_t response_object = plist_dict_get_item(response_plist,"$objects");
             int msg_type = 0;
             int response_code_offset=8;
             int time_stamp_offset=6;
             double time_stamp;
             uint64_t response_code ;
             
             // debug_plist(response_plist); // This call leaks memory
             
             plist_t response_type_plist = plist_array_get_item(response_object,5);
             if (plist_get_node_type(response_type_plist)==PLIST_STRING)
             {
             msg_type = 0;
             response_code_offset=8;
             time_stamp_offset=6;
             }
             else
             {
             msg_type = 1;
             response_code_offset=7;
             time_stamp_offset=5;
             }
             
             plist_t response_code_plist = plist_array_get_item(response_object,response_code_offset);
             if (plist_get_node_type(response_code_plist)==PLIST_UINT)
             {
             plist_get_uint_val(response_code_plist,&response_code);
             //printf("response: %d \n",response_code);
             }
             else
             {
             //printf("response no code\n");
             }
             
             plist_t time_stamp_plist_dict = plist_array_get_item(response_object, time_stamp_offset);
             plist_t time_stamp_plist = plist_dict_get_item(time_stamp_plist_dict, "NS.time");
             
             if (plist_get_node_type(time_stamp_plist)==PLIST_REAL)
             {
             time_t seconds;
             char outstr[100];
             plist_get_real_val(time_stamp_plist, &time_stamp);
             seconds = time_stamp+978307200;
             struct tm *tmptr = gmtime(&seconds);
             //printf("timestamp:%f %ld \n",time_stamp,seconds);
             //printf("gmt:%s \n",asctime(tmptr));
             if (response_code>0)
             {
             strftime(outstr,sizeof(outstr),"%F %T %z",tmptr);
             printf("%s ",outstr);
             }
             }
             else
             {
             //printf("timestamp:%d \n",plist_get_node_type(time_stamp_plist));
             }
             
             
             if (response_code>0)
             {
             if (response_code==4)
             {
             printf("Start: ");
             }
             else if (response_code==5)
             {
             printf("Pass: ");
             }
             else if (response_code==1)
             {
             printf("Default: ");
             }
             
             if (msg_type==0 )
             {
             plist_t response_text_plist = plist_array_get_item(response_object,9);
					        if (plist_get_node_type(response_text_plist)==PLIST_STRING)
					        {
             char *response_string;
             plist_get_string_val(response_text_plist,&response_string);
             printf("%s \n",response_string);
             if (response_string) free(response_string);
					        }
             else
             {
             printf("(null) \n");
             }
					        //plist_free(response_plist);
             }
             else
             {
             printf("(null) \n");
             }
             //fflush(stdout);
             }
             plist_free(response_plist);
             }
             }*/
        }
    }
    
    char *std_out=NULL;
    iwt->cbfunc(&std_out, iwt->user_data, 1); // last parameter is to instruct the parent thread to exit
    if (std_out) MemoryFree(&std_out);
    
    if (iwt)
    {
        MemoryFree(&iwt);
    }
    if (header) MemoryFree(&header);
    if (response) MemoryFree(&response);
    if (response_header2) MemoryFree(&response_header2);
    
    //debug_info("Exiting");
    printf("Thread Exiting...\n");
    
    return NULL;
}

void *commandline_worker(void *arg)
{
    /* the command line argument      */
    int dummy;
    long qLen = 0;
    int32_t reply_packet_num;
    
    FILE *fp = NULL;
    register int s, len;
    struct sockaddr_un saun;
    struct pollfd fds;
    
    
    struct instruments_worker_thread *iwt = (struct instruments_worker_thread*)arg;
    
    if (!iwt)
        return NULL;
    
    fprintf(stderr,"[commandline_worker]Commandline Worker Starting\n");
    
    /*
     * Get a socket to work with.  This socket will
     * be in the UNIX domain, and will be a
     * stream socket.
     */
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("client: socket");
        exit(1);
    }
    
    /*
     * Create the address we will be connecting to.
     */
    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, commandprocessPath);
    
    /*
     * Try to connect to the address.  For this to
     * succeed, the server must already have bound
     * this address, and must have issued a listen()
     * request.
     *
     * The third argument indicates the "length" of
     * the structure, not just the length of the
     * socket name.
     */
    len = sizeof(saun.sun_family) + strlen(saun.sun_path);
    
    if (connect(s, &saun, len) < 0)
    {
        perror("client: connect");
        exit(1);
    }
    
    /*
     * We'll use stdio for reading
     * the socket.
     */
    fp = fdopen(s, "r");
    
    while (!iwt->client->exiting )
    {
        getCommandResponseQLenFptr(&qLen, dummy);//reply_packet_num is dummy here
        if(qLen > 0)
        {
            fds.fd = s;
            fds.events = POLLIN;
            static char std_line[INSTRUMENTS_MAX_CMD_STR+1] = {0,};
            int retval;
            
            memset(commandline_string,0,1024*1024*sizeof(char));
            if((reply_packet_num = (int32_t)getCommandResponseQFptr(commandline_string, dummy)) < 0)
            {
                fprintf(stderr, "[commandline_worker]commandResponseQ Failed\n");
                continue;
            }
            
            //fprintf(stderr, "[commandline_worker] reply_packet_num retrieved: %d, %s\n",reply_packet_num, commandline_string);
            
            //Send command string to socket
            if(send(s,commandline_string, strlen(commandline_string)+1, MSG_DONTWAIT) < 0)
            {
                fprintf(stderr, "[commandline_worker]  socket send failed\n");
            }
            
            // Poll for response
            retval = poll(&fds,1,-1);
            if( retval == 1)
            {
                int c,i=0;
                
                while ( ( (c = fgetc(fp) ) != '\0') && (i<INSTRUMENTS_MAX_CMD_STR) )
                {
                    std_line[i] = c;
                    ++i;
                }
                
                if(i < INSTRUMENTS_MAX_CMD_STR)
                {
                    std_line[i] = '\0';
                    
                    fprintf(stderr, "[commandline_worker]Commandline Worker output to be sent: %s\n",std_line);
                    instruments_send_stdout(iwt->client,0xfffffff7,	reply_packet_num,std_line);
                }
            }
            else if(retval < 0)
            {
                fprintf(stderr,"[commandline_worker] Poll socket was not successful\n");
            }
        }
        usleep(100000);
    }
    
    fclose(fp);
    if (iwt->user_data) MemoryFree(&iwt->user_data);
    
    if (iwt)
    {
        MemoryFree(&iwt);
    }
    
    printf("[commandline_worker]Commandline Worker Exiting..\n");
    //fflush(stdout);
    return NULL;
}

instruments_error_t instruments_client_new_with_service_client(service_client_t service_client, instruments_client_t *client)
{
    
    if (!service_client)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_client_t client_loc = (instruments_client_t) malloc(sizeof(struct instruments_client_private));
    client_loc->parent = service_client;
    client_loc->free_parent = 0;
    client_loc->uia_result_path = NULL;
    client_loc->uia_script = NULL;
    client_loc->app_path = NULL;
    client_loc->package_name = NULL;
    client_loc->exiting = 0;
    client_loc->worker = (pthread_t)NULL;
    
    /* allocate a packet */
    client_loc->instruments_packet = (InstrumentsPacket *) malloc(sizeof(InstrumentsPacket));
    if (!client_loc->instruments_packet) {
        MemoryFree(&client_loc);
        return INSTRUMENTS_E_NO_MEM;
    }
    
    client_loc->instruments_packet->packet_num = 0;
    client_loc->instruments_packet->length = 0;
    client_loc->instruments_packet->msg_type = 0;
    client_loc->instruments_packet->channel = 0;
    client_loc->instruments_packet->empty = 0;
    client_loc->instruments_packet->msg_type2 = 0;
    client_loc->instruments_packet->payload_offset = 0 ;
    client_loc->instruments_packet->length2 = 0;
    client_loc->instruments_packet->empty2 = 0;
    memcpy(client_loc->instruments_packet->magic,
           INSTRUMENTS_MAGIC, INSTRUMENTS_MAGIC_LEN);
    client_loc->file_handle = 0;
    client_loc->lock = 0;
    mutex_init(&client_loc->mutex);
    
    *client = client_loc;
    
    return INSTRUMENTS_E_SUCCESS;
}

instruments_error_t instruments_client_new(idevice_t device, lockdownd_service_descriptor_t service, instruments_client_t *client)
{
    if (!device || !service || service->port == 0 || !client || *client)
    {
        return INSTRUMENTS_E_INVALID_ARG;
    }
    /* plain service implementation */
    service_client_t parent = NULL;
    if (service_client_new(device, service, &parent) != SERVICE_E_SUCCESS) {
        return INSTRUMENTS_E_MUX_ERROR;
    }
    
    instruments_error_t err = instruments_client_new_with_service_client(parent, client);
    
    if (err != INSTRUMENTS_E_SUCCESS) {
        service_client_free(parent);
    } else {
        (*client)->free_parent = 1;
    }
    return err;
}

instruments_error_t instruments_client_start_service(idevice_t device, instruments_client_t * client, const char* label)
{
    instruments_error_t err = INSTRUMENTS_E_UNKNOWN_ERROR;
    int32_t er_er = 0;
    service_client_factory_start_service(device, INSTRUMENTS_SERVICE_NAME, (void**)client, label, SERVICE_CONSTRUCTOR(instruments_client_new), &er_er);
    err = (instruments_error_t)er_er;
    return err;
}

instruments_error_t instruments_client_free(instruments_client_t client)
{
    if (!client || !client->instruments_packet)
        return INSTRUMENTS_E_INVALID_ARG;
    
    if (client->free_parent && client->parent) {
        service_client_free(client->parent);
        client->parent = NULL;
    }
    MemoryFree(&client->instruments_packet);
    if (client->uia_result_path)
        MemoryFree(&client->uia_result_path);
    if (client->uia_script)
        MemoryFree(&client->uia_script);
    if (client->app_path)
        MemoryFree(&client->app_path);
    if (client->package_name)
        MemoryFree(&client->package_name);
    mutex_destroy(&client->mutex);
    MemoryFree(&client);
    return INSTRUMENTS_E_SUCCESS;
}

static instruments_error_t instruments_send_packet(instruments_client_t client, uint32_t msg_type, uint32_t channel, uint32_t msg_type2, const char *data, uint32_t data_length, const char* payload, uint32_t payload_length, uint32_t *bytes_sent, uint32_t packet_num)
{
    uint32_t sent = 0;
    
    if (!client || !client->parent || !client->instruments_packet)
        return INSTRUMENTS_E_INVALID_ARG;
    
    *bytes_sent = 0;
    
    if (!data || !data_length)
        data_length = 0;
    if (!payload || !payload_length)
        payload_length = 0;
    
    if (packet_num)
        client->instruments_packet->packet_num=packet_num;
    else
        client->instruments_packet->packet_num++;
    
    client->instruments_packet->msg_type = msg_type;
    client->instruments_packet->channel = channel;
    client->instruments_packet->length = 16 + data_length + payload_length;
    client->instruments_packet->msg_type2 = msg_type2;
    /*if (msg_type2==0x1002)
     {
     client->instruments_packet->empty = 1;
     }*/
    
    client->instruments_packet->payload_offset = data_length;
    client->instruments_packet->length2 = data_length + payload_length ;
    
    /* send Instruments packet header */
    InstrumentsPacket_to_LE(client->instruments_packet);
    //debug_info("packet length = %i", client->instruments_packet->length);
    debug_buffer((char*)client->instruments_packet, sizeof(InstrumentsPacket));
    sent = 0;
    service_send(client->parent, (void*)client->instruments_packet, sizeof(InstrumentsPacket), &sent);
    InstrumentsPacket_from_LE(client->instruments_packet);
    *bytes_sent += sent;
    
    //if (sent < sizeof(InstrumentsPacket)) {
    //        return INSTRUMENTS_E_SUCCESS;
    //}
    
    /* send Instruments packet data (if there's data to send) */
    sent = 0;
    if (data_length > 0) {
        //debug_info("packet data follows");
        //debug_buffer(data, data_length);
        service_send(client->parent, data, data_length, &sent);
    }
    *bytes_sent += sent;
    //if (sent < data_length) {
    //        return INSTRUMENTS_E_SUCCESS;
    //}
    
    sent = 0;
    if (payload_length > 0) {
        //debug_info("packet payload follows");
        //debug_buffer(payload, payload_length);
        service_send(client->parent, payload, payload_length, &sent);
    }
    *bytes_sent += sent;
    if (sent < payload_length) {
        return INSTRUMENTS_E_SUCCESS;
    }
    
    return INSTRUMENTS_E_SUCCESS;
}

static instruments_error_t instruments_receive_data(instruments_client_t client, char **bytes, uint32_t *bytes_recv, uint32_t *packet_num)
{
    return instruments_receive_data_with_timeout(client, bytes, bytes_recv, packet_num,5000);
}

static instruments_error_t instruments_receive_data_with_timeout(instruments_client_t client, char **bytes, uint32_t *bytes_recv, uint32_t *packet_num, int timeout)
{
    InstrumentsPacket header;
    uint32_t entire_len = 0;
    uint32_t this_len = 0;
    uint32_t current_count = 0;
    //uint64_t param1 = -1;
    char* dump_here = NULL;
    //int res=0;
    
    if (bytes_recv) {
        *bytes_recv = 0;
    }
    if (bytes) {
        *bytes = NULL;
    }
    
    /* first, read the Instruments header */
    service_receive_with_timeout(client->parent, (char*)&header, sizeof(InstrumentsPacket)-16, bytes_recv,timeout);
    InstrumentsPacket_from_LE(&header);
    if (*bytes_recv == 0) {
        fprintf(stderr, "%s","[instruments_receive_data_with_timeout] Just didn't get enough bytes\n");
        return INSTRUMENTS_E_MUX_ERROR;
    }
    else if (*bytes_recv < sizeof(InstrumentsPacket)-16)
    {
        int bytesRecv = *bytes_recv, count=0;
        fprintf(stderr, "%s","[instruments_receive_data_with_timeout] Did not even get the InstrumentsPacket header\n");
        
        while(bytesRecv < (sizeof(InstrumentsPacket)-16))
        {
            service_receive_with_timeout(client->parent, ((char*)&header)+bytesRecv, (sizeof(InstrumentsPacket)-16) - bytesRecv, bytes_recv,timeout);
            
            bytesRecv += *bytes_recv;
            count++;
            
            if(count == 50) break;
        }
        
        if(count == 50)
            return INSTRUMENTS_E_MUX_ERROR;
        
        *bytes_recv = bytesRecv;
    }
    
    /* check if it's a valid Instruments header */
    if (strncmp(header.magic, INSTRUMENTS_MAGIC, INSTRUMENTS_MAGIC_LEN)) {
        //debug_info("Invalid Instruments packet received (magic != " INSTRUMENTS_MAGIC ")!");
        fprintf(stderr, "Invalid Instruments packet received (magic != INSTRUMENTS_MAGIC)\n");
    }
    
    /* check if it has the correct packet number */
    if (header.packet_num != client->instruments_packet->packet_num) {
        //debug_info("Warning: Unexpected packet number (%lld != %lld) ignoring.", header.packet_num, client->instruments_packet->packet_num);
        //return AFC_E_OP_HEADER_INVALID;
    }
    
    *packet_num = header.packet_num;
    
    /* then, read the attached packet */
    if (header.length < 16 ) {
        //debug_info("Invalid InstrumentsPacket header received!");
        return INSTRUMENTS_E_OP_HEADER_INVALID;
    }
    /*	 else if (header.length == 16) {
     //debug_info("Empty AFCPacket received!");
     *bytes_recv = 0;
     if (header.msg_type == 0) {
     return INSTRUMENTS_E_SUCCESS;
     } else {
     return INSTRUMENTS_E_IO_ERROR;
     }
     }
     */
    
    entire_len = (uint32_t)(header.length);
    //this_len = (uint32_t)header.this_length - sizeof(AFCPacket);
    this_len = 0;
    
    dump_here = (char*)malloc((entire_len+1) * sizeof(char));
    if (this_len > 0)
    {
        service_receive_with_timeout(client->parent, dump_here, this_len, bytes_recv, timeout);
        if (*bytes_recv <= 0) {
            MemoryFree(&dump_here);
            //debug_info("Did not get packet contents!");
            return INSTRUMENTS_E_NOT_ENOUGH_DATA;
        } else if (*bytes_recv < this_len) {
            MemoryFree(&dump_here);
            //debug_info("Could not receive this_len=%d bytes", this_len);
            return INSTRUMENTS_E_NOT_ENOUGH_DATA;
        }
    }
    
    current_count = 0;
    
    if (entire_len > 0)
    {
        if (entire_len <= INSTRUMENTS_SEGMENTATION_THRESHOLD)
        {
            while (current_count < entire_len)
            {
                service_receive_with_timeout(client->parent, dump_here+current_count, entire_len - current_count, bytes_recv, timeout);
                if (*bytes_recv <= 0)
                {
                    //debug_info("Error receiving data (recv returned %d)", *bytes_recv);
                    break;
                }
                current_count += *bytes_recv;
            }
            if (current_count < entire_len)
            {
                //debug_info("WARNING: could not receive full packet (read %s, size %d)", current_count, entire_len);
            }
        }
        else
        { //large packets are segmented
            while (current_count < entire_len)
            {
                //debug_info("Receiving segments expected total %d, received so far %d)", entire_len, current_count);
                InstrumentsPacket segmentHeader;
                uint32_t segment_len;
                uint32_t segment_received;
                uint32_t current_recv=0;
                char dummyBuff[65504];
                service_receive_with_timeout(client->parent, (char*)&segmentHeader, sizeof(InstrumentsPacket)-16, &current_recv ,timeout);
                if (current_recv <= 0)
                {
                    debug_info("Error receiving segment header (recv returned %d)", current_recv);
                    break;
                }
                
                if(header.packet_num == segmentHeader.packet_num)
                {
                    InstrumentsPacket_from_LE(&segmentHeader);
                    segment_len = (uint32_t)(segmentHeader.length);
                    //fprintf(stderr,"%d\t%d\t%d\t%d\n", entire_len,segment_len,header.packet_num,segmentHeader.packet_num);
                    segment_received = 0;
                    while (segment_received < segment_len)
                    {
                        current_recv = 0;
                        service_receive_with_timeout(client->parent, dump_here+current_count+segment_received, segment_len-segment_received, &current_recv, timeout);
                        if (current_recv <= 0) {
                            //debug_info("Error receiving data (recv returned %d)", current_recv);
                            break;
                        }
                        segment_received += current_recv;
                    }
                    current_count += segment_received;
                }
                else
                {
                    InstrumentsPacket_from_LE(&segmentHeader);
                    segment_len = (uint32_t)(segmentHeader.length);
                    segment_received = 0;
                    while (segment_received < segment_len)
                    {
                        current_recv = 0;
                        service_receive_with_timeout(client->parent,dummyBuff, segment_len-segment_received, &current_recv, timeout);
                        if (current_recv <= 0) {
                            //debug_info("Error receiving data (recv returned %d)", current_recv);
                            break;
                        }
                        segment_received += current_recv;
                    }
                    
                    if(segmentHeader.length == 813)//Host message coming from device in between the screen shot segments
                    {
                        
                        char *std_out=NULL;
                        //struct timeval tv;
                        
                        //gettimeofday(&tv,NULL);
                        //fprintf(stderr, "[Instrument worker] #######Time at request to be sent  %u\n", tv.tv_usec);
                        //fprintf(stderr, "[Instrument worker] #######Time at request to be sent in sec %u\n", tv.tv_sec);
                        if(iwtScreen->cbfunc != NULL)
                        {
                            iwtScreen->cbfunc(&std_out, iwtScreen->user_data,0);
                            instruments_send_stdout(client,0xfffffff7,segmentHeader.packet_num,std_out);
                            
                            //gettimeofday(&tv,NULL);
                            //fprintf(stderr, "[Instrument worker] #######Time at request sent  %u\n", tv.tv_usec);
                            //fprintf(stderr, "[Instrument worker] #######Time at request sent in sec %u\n\n", tv.tv_sec);
                            if (std_out) free(std_out);
                        }
                    }
                }
            }
            if (current_count < entire_len) {
                //debug_info("WARNING: could not receive full packet (read %s, size %d)", current_count, entire_len);
            }
        }
    }
    /*
     if (current_count >= sizeof(uint64_t)) {
     param1 = le64toh(*(uint64_t*)(dump_here));
     }
     */
    
    //debug_info("packet data size = %i", current_count);
    //debug_info("packet data follows");
    //debug_buffer(dump_here, current_count);
    
    if (bytes) {
        dump_here[current_count]='\0';
        *bytes = dump_here;
    } else {
        MemoryFree(&dump_here);
    }
    
    *bytes_recv = current_count;
    return INSTRUMENTS_E_SUCCESS;
}

instruments_error_t instruments_send_channel_request(instruments_client_t client, const char* service, uint32_t channel)
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    ChannelRequestPacket *channel_request;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist1 = plist_new_dict();
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    plist_array_append_item(array, plist_new_string(service));
    
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    /* allocate a packet */
    channel_request = (ChannelRequestPacket *) malloc(sizeof(ChannelRequestPacket));
    if (!channel_request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(channel_request->magic, INSTRUMENTS_MAGIC2, INSTRUMENTS_MAGIC2_LEN);
    
    channel_request->length = plist1_length + 24;
    channel_request->empty = 0;
    channel_request->nl = 10;
    channel_request->channel_magic = 3;
    channel_request->requested_channel = channel;
    channel_request->nl2 = 10;
    channel_request->bplist_magic = 2;
    channel_request->bplist_size = plist1_length;
    
    data_length = plist1_length + sizeof(ChannelRequestPacket);
    data = malloc (data_length*sizeof(char));
    
    ChannelRequestPacket_to_LE(channel_request);
    memcpy(data,channel_request,sizeof(ChannelRequestPacket));
    ChannelRequestPacket_from_LE(channel_request);
    
    memcpy(&data[sizeof(ChannelRequestPacket)],plist1_bin,plist1_length);
    
    plist_t plist2 = plist_new_dict();
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string("_requestChannelWithCode:identifier:"));
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    if (instruments_send_packet(client, 0, 0, 0x1002, data, data_length, plist2_bin, plist2_length, &bytes, 0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&channel_request);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    while ( reply_packet_num < client->instruments_packet->packet_num) {
        
        res = instruments_receive_data(client, &header, &bytes_recv, &reply_packet_num);
        //debug_info("reply packet no %d\n",reply_packet_num);
        MemoryFree(&header);
        if (res < 0) {
            break;
        }
    }
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    return res;
}

instruments_error_t instruments_config_launch_env(instruments_client_t client, uint32_t channel, const char* uia_result_path, const char *uia_script)
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    RequestPacket *request;
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    if (client->uia_result_path) MemoryFree(&client->uia_result_path);
    
    client->uia_result_path = malloc((1+strlen(uia_result_path)) * sizeof(char)) ;
    if (!client->uia_result_path)
        return INSTRUMENTS_E_NO_MEM;
    
    strcpy(client->uia_result_path,uia_result_path);
    
    if (client->uia_script) MemoryFree(&client->uia_script);
    client->uia_script= malloc((1+strlen(uia_script)) * sizeof(char));
    if (!client->uia_script)
        return INSTRUMENTS_E_NO_MEM;
    
    strcpy(client->uia_script,uia_script);
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    
    plist_t class_dict = plist_new_dict();
    plist_dict_set_item(class_dict, "$class",plist_new_uid(6));
    
    plist_t keys_array = plist_new_array();
    plist_array_append_item(keys_array, plist_new_uid(2));
    plist_array_append_item(keys_array, plist_new_uid(3));
    
    plist_dict_set_item(class_dict, "NS.keys",keys_array);
    
    plist_t object_array = plist_new_array();
    plist_array_append_item(object_array, plist_new_uid(4));
    plist_array_append_item(object_array, plist_new_uid(5));
    
    plist_dict_set_item(class_dict, "NS.objects",object_array);
    
    plist_array_append_item(array, class_dict);
    
    plist_array_append_item(array, plist_new_string("UIARESULTPATH"));
    plist_array_append_item(array, plist_new_string("UIASCRIPT"));
    plist_array_append_item(array, plist_new_string(client->uia_result_path));
    plist_array_append_item(array, plist_new_string(client->uia_script));
    
    plist_t classes_dict = plist_new_dict();
    plist_t classes_array = plist_new_array();
    
    plist_array_append_item(classes_array, plist_new_string("NSMutableDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSObject"));
    
    plist_dict_set_item(classes_dict, "$classes",classes_array);
    plist_dict_set_item(classes_dict, "$classname",plist_new_string("NSMutableDictionary"));
    
    plist_array_append_item(array, classes_dict );
    
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    /* allocate a packet */
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC23, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = plist1_length + 12;
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 2;
    request->bplist_size = plist1_length;
    
    data_length = plist1_length + sizeof(RequestPacket);
    data = malloc (data_length * sizeof(char));
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    memcpy(&data[sizeof(RequestPacket)],plist1_bin,plist1_length);
    
    plist_t plist2 = plist_new_dict();
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string("configureLaunchEnvironment"));
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    if (instruments_send_packet(client, 0, channel, 0x1002, data, data_length, plist2_bin, plist2_length, &bytes,0 ) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&request);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    
    while ( reply_packet_num != client->instruments_packet->packet_num) {
        
        res = instruments_receive_data_with_timeout(client, &header, &bytes_recv, &reply_packet_num,10000);
        //debug_info("reply packet no %d\n",reply_packet_num);
        MemoryFree(&header);
        if (res < 0) {
            break;
        }
    }
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    return res;
    //return INSTRUMENTS_E_SUCCESS;
}

instruments_error_t instruments_launch_app(instruments_client_t client, uint32_t channel, const char* app_path, const char * package_name, uint64_t *pid)
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    char *plist3_bin = NULL;
    uint32_t plist3_length = 0;
    char *plist4_bin = NULL;
    uint32_t plist4_length = 0;
    char *plist5_bin = NULL;
    uint32_t plist5_length = 0;
    char *plistn_bin = NULL;
    uint32_t plistn_length = 0;
    uint32_t offset=0;
    
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    RequestPacket *request;
    BPLPacket *bpl_packet;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_SUCCESS;
    if (client->app_path)
        MemoryFree(&client->app_path);
    client->app_path = malloc((1+strlen(app_path)) * sizeof(char));
    if (!client->app_path)
        return INSTRUMENTS_E_NO_MEM;
    
    strcpy(client->app_path,app_path);
    
    if (client->package_name)
        MemoryFree(&client->package_name);
    
    client->package_name = malloc((1+strlen(package_name)) * sizeof(char)) ;
    if (!client->package_name)
        return INSTRUMENTS_E_NO_MEM;
    
    strcpy(client->package_name,package_name);
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array1 = plist_new_array();
    plist_array_append_item(array1, plist_new_string("$null"));
    plist_array_append_item(array1, plist_new_string(client->app_path));
    
    plist_dict_set_item(plist1, "$objects", array1 );
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    plist_t plist2 = plist_new_dict();
    
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    
    plist_t class_dict2 = plist_new_dict();
    plist_dict_set_item(class_dict2,"$class",plist_new_uid(2));
    plist_dict_set_item(class_dict2,"NS.string",plist_new_string(client->package_name));
    plist_array_append_item(array2, class_dict2);
    
    plist_t classes_dict2 = plist_new_dict();
    plist_t classes_array2 = plist_new_array();
    plist_array_append_item(classes_array2,plist_new_string("NSMutableString"));
    plist_array_append_item(classes_array2,plist_new_string("NSString"));
    plist_array_append_item(classes_array2,plist_new_string("NSObject"));
    plist_dict_set_item(classes_dict2,"$classes",classes_array2);
    plist_dict_set_item(classes_dict2,"$classname",plist_new_string("NSMutableString"));
    
    plist_array_append_item(array2, classes_dict2);
    
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    plist_t plist3 = plist_new_dict();
    plist_dict_set_item(plist3, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array3 = plist_new_array();
    plist_array_append_item(array3, plist_new_string("$null"));
    
    plist_t class_dict = plist_new_dict();
    plist_dict_set_item(class_dict, "$class",plist_new_uid(8));
    
    plist_t keys_array = plist_new_array();
    plist_array_append_item(keys_array, plist_new_uid(2));
    plist_array_append_item(keys_array, plist_new_uid(3));
    plist_array_append_item(keys_array, plist_new_uid(4));
    plist_dict_set_item(class_dict, "NS.keys",keys_array);
    
    plist_t object_array = plist_new_array();
    plist_array_append_item(object_array, plist_new_uid(5));
    plist_array_append_item(object_array, plist_new_uid(6));
    plist_array_append_item(object_array, plist_new_uid(7));
    
    plist_dict_set_item(class_dict, "NS.objects",object_array);
    plist_array_append_item(array3, class_dict);
    
    plist_array_append_item(array3, plist_new_string("UIARESULTPATH"));
    plist_array_append_item(array3, plist_new_string("HIPreventRefEncoding"));
    plist_array_append_item(array3, plist_new_string("UIASCRIPT"));
    plist_array_append_item(array3, plist_new_string(client->uia_result_path));
    plist_array_append_item(array3, plist_new_string("1"));
    plist_array_append_item(array3, plist_new_string(client->uia_script));
    
    plist_t classes_dict = plist_new_dict();
    plist_t classes_array = plist_new_array();
    plist_array_append_item(classes_array, plist_new_string("NSMutableDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSObject"));
    plist_dict_set_item(classes_dict, "$classes",classes_array);
    plist_dict_set_item(classes_dict, "$classname",plist_new_string("NSMutableDictionary"));
    plist_array_append_item(array3, classes_dict );
    
    plist_dict_set_item(plist3, "$objects", array3 );
    
    plist_t dict3 = plist_new_dict();
    plist_dict_set_item(dict3, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist3, "$top", dict3);
    
    plist_dict_set_item(plist3, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist3, &plist3_bin, &plist3_length);
    
    //debug_info("plist3 returned length %d\n",plist3_length);
    
    //debug_plist(plist3);
    
    plist_t plist4 = plist_new_dict();
    
    plist_dict_set_item(plist4, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array4 = plist_new_array();
    plist_array_append_item(array4, plist_new_string("$null"));
    
    plist_t class_dict4 = plist_new_dict();
    plist_dict_set_item(class_dict4,"$class",plist_new_uid(2));
    plist_dict_set_item(class_dict4,"NS.objects",plist_new_array());
    plist_array_append_item(array4, class_dict4);
    
    plist_t classes_dict4 = plist_new_dict();
    plist_t classes_array4 = plist_new_array();
    plist_array_append_item(classes_array4,plist_new_string("NSArray"));
    plist_array_append_item(classes_array4,plist_new_string("NSObject"));
    plist_dict_set_item(classes_dict4,"$classes",classes_array4);
    plist_dict_set_item(classes_dict4,"$classname",plist_new_string("NSArray"));
    
    plist_array_append_item(array4, classes_dict4);
    
    plist_dict_set_item(plist4, "$objects", array4 );
    
    plist_t dict4 = plist_new_dict();
    plist_dict_set_item(dict4, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist4, "$top", dict4);
    
    plist_dict_set_item(plist4, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist4, &plist4_bin, &plist4_length);
    
    //debug_info("plist4 returned length %d\n",plist4_length);
    
    //debug_plist(plist4);
    
    plist_t plist5 = plist_new_dict();
    
    plist_dict_set_item(plist5, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array5 = plist_new_array();
    plist_array_append_item(array5, plist_new_string("$null"));
    
    plist_t class_dict5 = plist_new_dict();
    plist_dict_set_item(class_dict5,"$class",plist_new_uid(2));
    plist_dict_set_item(class_dict5,"NS.keys",plist_new_array());
    plist_dict_set_item(class_dict5,"NS.objects",plist_new_array());
    plist_array_append_item(array5, class_dict5);
    
    plist_t classes_dict5 = plist_new_dict();
    plist_t classes_array5 = plist_new_array();
    plist_array_append_item(classes_array5,plist_new_string("NSMutableDictionary"));
    plist_array_append_item(classes_array5,plist_new_string("NSDictioinary"));
    plist_array_append_item(classes_array5,plist_new_string("NSObject"));
    plist_dict_set_item(classes_dict5,"$classes",classes_array5);
    plist_dict_set_item(classes_dict5,"$classname",plist_new_string("NSMutableDictionary"));
    
    plist_array_append_item(array5, classes_dict5);
    
    plist_dict_set_item(plist5, "$objects", array5 );
    
    plist_t dict5 = plist_new_dict();
    plist_dict_set_item(dict5, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist5, "$top", dict5);
    
    plist_dict_set_item(plist5, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist5, &plist5_bin, &plist5_length);
    
    //debug_info("plist5 returned length %d\n",plist5_length);
    
    //debug_plist(plist5);
    
    /* allocate a packet */
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC27, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = plist1_length + plist2_length + plist3_length + plist4_length +plist5_length + 12 + 4*sizeof(BPLPacket);
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 2;
    request->bplist_size = plist1_length;
    
    data_length = plist1_length + plist2_length + plist3_length + plist4_length + plist5_length + sizeof(RequestPacket) + 4*sizeof(BPLPacket);
    data = malloc (data_length * sizeof(char));
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    offset = sizeof(RequestPacket);
    
    memcpy(&data[offset],plist1_bin,plist1_length);
    offset += plist1_length;
    
    bpl_packet = (BPLPacket*) malloc(sizeof(BPLPacket));
    if (!bpl_packet) {
        return INSTRUMENTS_E_NO_MEM;
    }
    bpl_packet->nl = 10;
    bpl_packet->bplist_magic = 2;
    bpl_packet->bplist_size = plist2_length;
    
    BPLPacket_to_LE(bpl_packet);
    memcpy(&data[offset],bpl_packet,sizeof(BPLPacket));
    BPLPacket_from_LE(bpl_packet);
    offset += sizeof(BPLPacket);
    memcpy(&data[offset],plist2_bin,plist2_length);
    offset += plist2_length;
    
    bpl_packet->bplist_size = plist3_length;
    BPLPacket_to_LE(bpl_packet);
    memcpy(&data[offset],bpl_packet,sizeof(BPLPacket));
    BPLPacket_from_LE(bpl_packet);
    offset += sizeof(BPLPacket);
    memcpy(&data[offset],plist3_bin,plist3_length);
    offset += plist3_length;
    
    bpl_packet->bplist_size = plist4_length;
    BPLPacket_to_LE(bpl_packet);
    memcpy(&data[offset],bpl_packet,sizeof(BPLPacket));
    BPLPacket_from_LE(bpl_packet);
    offset += sizeof(BPLPacket);
    memcpy(&data[offset],plist4_bin,plist4_length);
    offset += plist4_length;
    
    bpl_packet->bplist_size = plist5_length;
    BPLPacket_to_LE(bpl_packet);
    memcpy(&data[offset],bpl_packet,sizeof(BPLPacket));
    BPLPacket_from_LE(bpl_packet);
    offset += sizeof(BPLPacket);
    memcpy(&data[offset],plist5_bin,plist5_length);
    offset += plist5_length;
    
    plist_t plistn = plist_new_dict();
    plist_dict_set_item(plistn, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t arrayn = plist_new_array();
    plist_array_append_item(arrayn, plist_new_string("$null"));
    plist_array_append_item(arrayn, plist_new_string("launchSuspendedProcessWithDevicePath:bundleIdentifier:environment:arguments:options:"));
    plist_dict_set_item(plistn, "$objects", arrayn );
    
    plist_t dictn = plist_new_dict();
    plist_dict_set_item(dictn, "root",plist_new_uid(1));
    
    plist_dict_set_item(plistn, "$top", dictn);
    
    plist_dict_set_item(plistn, "$version", plist_new_uint(100000));
    
    plist_to_bin(plistn, &plistn_bin, &plistn_length);
    
    //debug_info("plistn returned length %d\n",plistn_length);
    
    //debug_plist(plistn);
    
    if (instruments_send_packet(client, 0, channel, 0x1002, data, data_length, plistn_bin, plistn_length, &bytes , 0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    plist_free(plist3);
    plist_free(plist4);
    plist_free(plist5);
    plist_free(plistn);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&plist3_bin);
    MemoryFree(&plist4_bin);
    MemoryFree(&plist5_bin);
    MemoryFree(&plistn_bin);
    MemoryFree(&request);
    MemoryFree(&bpl_packet);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    while ((reply_packet_num != client->instruments_packet->packet_num ) && (res==INSTRUMENTS_E_SUCCESS)) {
        if (header) MemoryFree(&header);
        header=NULL;
        res = instruments_receive_data_with_timeout(client, &header, &bytes_recv, &reply_packet_num,60000);
        //debug_info("reply packet no %d\n",reply_packet_num);
        if (res < 0) {
            break;
        }
    }
    
    if (res==INSTRUMENTS_E_SUCCESS)
    {
        InstrumentsResponse *response;
        plist_t response_plist=NULL;
        response = (InstrumentsResponse*) malloc (sizeof(InstrumentsResponse));
        if (!response)
        {
            MemoryFree(&header);
            return INSTRUMENTS_E_NO_MEM;
        }
        
        memcpy(response,header,sizeof(InstrumentsResponse));
        InstrumentsResponse_from_LE(response);
        plist_from_bin(header+sizeof(InstrumentsResponse),response->length,&response_plist);
        if (response_plist)
        {
            plist_t response_object = plist_dict_get_item(response_plist,"$objects");
            //debug_plist(response_plist);
            plist_t pid_plist = plist_array_get_item(response_object,1);
            if (plist_get_node_type(pid_plist)==PLIST_UINT)
            {
                plist_get_uint_val(pid_plist,&client->pid);
                *pid = client->pid;
            }
            else
            {
                //debug_plist(pid_plist);
                res = INSTRUMENTS_E_LAUNCH_FAILED_ERROR;
                *pid = 0;
            }
            plist_free(response_plist);
        }
        else
        {
            res = INSTRUMENTS_E_LAUNCH_FAILED_ERROR;
            *pid = 0;
        }
        MemoryFree(&response);
    }
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    if (header) MemoryFree(&header);
    
    return res;
}

instruments_error_t instruments_get_pid(instruments_client_t client, uint32_t channel, const char* package_name, uint64_t *pid)
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    RequestPacket *request;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    *pid=0;
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    
    plist_t class_dict = plist_new_dict();
    plist_dict_set_item(class_dict, "$class",plist_new_uid(2));
    plist_dict_set_item(class_dict, "NS.string",plist_new_string(package_name));
    
    plist_array_append_item(array, class_dict);
    
    plist_t classes_dict = plist_new_dict();
				plist_t classes_array = plist_new_array();
    plist_array_append_item(classes_array, plist_new_string("NSMutableString"));
    plist_array_append_item(classes_array, plist_new_string("NSString"));
    plist_array_append_item(classes_array, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict, "$classes",classes_array);
    
    plist_dict_set_item(classes_dict, "$classname",plist_new_string("NSMutableString"));
    plist_array_append_item(array, classes_dict );
    
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    /* allocate a packet */
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC2, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = plist1_length + 12;
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 2;
    request->bplist_size = plist1_length;
    
    data_length = plist1_length + sizeof(RequestPacket);
    data = malloc (data_length * sizeof(char));
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    memcpy(&data[sizeof(RequestPacket)],plist1_bin,plist1_length);
    
    plist_t plist2 = plist_new_dict();
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string("processIdentifierForBundleIdentifier:"));
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    client->instruments_packet->packet_num+=10;
    
    if (instruments_send_packet(client, 0, channel, 0x1002, data, data_length, plist2_bin, plist2_length, &bytes , 0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&request);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    while ( reply_packet_num != client->instruments_packet->packet_num) {
        if (header) MemoryFree(&header);
        res = instruments_receive_data(client, &header, &bytes_recv, &reply_packet_num);
        //debug_info("reply packet no %d\n",reply_packet_num);
        if (res < 0) {
            break;
        }
    }
    if (res==INSTRUMENTS_E_SUCCESS)
    {
        InstrumentsResponse *response;
        response = (InstrumentsResponse*) malloc (sizeof(InstrumentsResponse));
        if (!response)
        {
            MemoryFree(&header);
            return INSTRUMENTS_E_NO_MEM;
        }
        
        memcpy(response,header,sizeof(InstrumentsResponse));
        InstrumentsResponse_from_LE(response);
        if (response->length>0)
        {
            plist_t response_plist=NULL;
            plist_from_bin(header+sizeof(InstrumentsResponse),response->length,&response_plist);
            if (response_plist)
            {
                if ((plist_get_node_type(response_plist)==PLIST_DICT))
                {
                    plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                    if (response_object)
                    {
                        plist_t pid_plist = plist_array_get_item(response_object,1);
                        if (plist_get_node_type(pid_plist)==PLIST_UINT)
                        {
                            plist_get_uint_val(pid_plist,&client->pid);
                            *pid = client->pid;
                        }
                        else
                        {
                            //debug_plist(pid_plist);
                            res = INSTRUMENTS_E_LAUNCH_FAILED_ERROR;
                        }
                    }
                }
                plist_free(response_plist);
            }
        }
        MemoryFree(&response);
    }
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    if (header) MemoryFree(&header);
    
    return res;
}

instruments_error_t instruments_pid_ops(instruments_client_t client, uint32_t channel, uint64_t pid, const char* ops)
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    RequestPacket *request;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    plist_array_append_item(array, plist_new_uint(pid));
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    /* allocate a packet */
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC2, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = plist1_length + 12;
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 2;
    request->bplist_size = plist1_length;
    
    data_length = plist1_length + sizeof(RequestPacket);
    data = malloc (data_length * sizeof(char));
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    memcpy(&data[sizeof(RequestPacket)],plist1_bin,plist1_length);
    
    plist_t plist2 = plist_new_dict();
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string(ops));
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    if (instruments_send_packet(client, 0, channel, 0x1002, data, data_length, plist2_bin, plist2_length, &bytes , 0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&request);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    while ( reply_packet_num != client->instruments_packet->packet_num) {
        
        res = instruments_receive_data(client, &header, &bytes_recv, &reply_packet_num);
        //debug_info("reply packet no %d\n",reply_packet_num);
        MemoryFree(&header);
        header=NULL;
        if (res < 0) {
            break;
        }
    }
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    return res;
}

instruments_error_t instruments_set_max(instruments_client_t client, uint32_t channel)
{
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    uint32_t bytes = 0;
    RequestPacket *request;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC2, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = 16;
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 6;
    request->bplist_size = 0x00800000;
    
    data_length = 4 + sizeof(RequestPacket);
    data = malloc (data_length * sizeof(char));
    
    memset(data,0,data_length);
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    plist_t plist2 = plist_new_dict();
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string("setMaxConnectionEnqueue:"));
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    if (instruments_send_packet(client, 0, channel, 0x0002, data, data_length, plist2_bin, plist2_length, &bytes , 0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist2);
    MemoryFree(&plist2_bin);
    MemoryFree(&request);
    MemoryFree(&data);
    return res;
}

instruments_error_t instruments_start_agent(instruments_client_t client, uint32_t channel, uint64_t pid)
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    char *plistn_bin = NULL;
    uint32_t plistn_length = 0;
    uint32_t offset=0;
    
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    RequestPacket *request;
    BPLPacket *bpl_packet;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array1 = plist_new_array();
    plist_array_append_item(array1, plist_new_string("$null"));
    plist_array_append_item(array1, plist_new_uint(pid));
    
    plist_dict_set_item(plist1, "$objects", array1 );
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    //
    
    plist_t plist2 = plist_new_dict();
    
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    
    plist_t class_dict2 = plist_new_dict();
    plist_dict_set_item(class_dict2,"$class",plist_new_uid(2));
    plist_dict_set_item(class_dict2,"NS.string",plist_new_string(client->package_name));
    plist_array_append_item(array2, class_dict2);
    
    plist_t classes_dict2 = plist_new_dict();
				plist_t classes_array2 = plist_new_array();
				plist_array_append_item(classes_array2,plist_new_string("NSMutableString"));
				plist_array_append_item(classes_array2,plist_new_string("NSString"));
				plist_array_append_item(classes_array2,plist_new_string("NSObject"));
    plist_dict_set_item(classes_dict2,"$classes",classes_array2);
    plist_dict_set_item(classes_dict2,"$classname",plist_new_string("NSMutableString"));
    
    plist_array_append_item(array2, classes_dict2);
    
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    /* allocate a packet */
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC27, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = plist1_length + plist2_length + 12 + 1*sizeof(BPLPacket);
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 2;
    request->bplist_size = plist2_length;
    
    data_length = plist1_length + plist2_length + sizeof(RequestPacket) + 1*sizeof(BPLPacket);
    data = malloc (data_length* sizeof(char));
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    offset = sizeof(RequestPacket);
    
    memcpy(&data[offset],plist2_bin,plist2_length);
    offset += plist2_length;
    
    bpl_packet = (BPLPacket*) malloc(sizeof(BPLPacket));
    if (!bpl_packet) {
        return INSTRUMENTS_E_NO_MEM;
    }
    bpl_packet->nl = 10;
    bpl_packet->bplist_magic = 2;
    bpl_packet->bplist_size = plist1_length;
    
    BPLPacket_to_LE(bpl_packet);
    memcpy(&data[offset],bpl_packet,sizeof(BPLPacket));
    BPLPacket_from_LE(bpl_packet);
    offset += sizeof(BPLPacket);
    memcpy(&data[offset],plist1_bin,plist1_length);
    offset += plist1_length;
    
    plist_t plistn = plist_new_dict();
    plist_dict_set_item(plistn, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t arrayn = plist_new_array();
    plist_array_append_item(arrayn, plist_new_string("$null"));
    plist_array_append_item(arrayn, plist_new_string("startAgentForApp:withPID:"));
    plist_dict_set_item(plistn, "$objects", arrayn );
    
    plist_t dictn = plist_new_dict();
    plist_dict_set_item(dictn, "root",plist_new_uid(1));
    
    plist_dict_set_item(plistn, "$top", dictn);
    
    plist_dict_set_item(plistn, "$version", plist_new_uint(100000));
    
    plist_to_bin(plistn, &plistn_bin, &plistn_length);
    
    //debug_info("plistn returned length %d\n",plistn_length);
    
    //debug_plist(plistn);
    
    if (instruments_send_packet(client, 0, channel, 0x1002, data, data_length, plistn_bin, plistn_length, &bytes ,0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    plist_free(plistn);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&plistn_bin);
    MemoryFree(&request);
    MemoryFree(&bpl_packet);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    int received_agent_is_ready = 0;
    while ( !received_agent_is_ready)
    {
        if (header) MemoryFree(&header);
        header=NULL;
        res = instruments_receive_data_with_timeout(client, &header, &bytes_recv, &reply_packet_num,15000);
        //debug_info("reply packet no %d\n",reply_packet_num);
        if (res==INSTRUMENTS_E_SUCCESS)
        {
            InstrumentsResponse *response;
            plist_t response_plist=NULL;
            response = (InstrumentsResponse*) malloc (sizeof(InstrumentsResponse));
            if (!response)
            {
                MemoryFree(&header);
                return INSTRUMENTS_E_NO_MEM;
            }
            memcpy(response,header,sizeof(InstrumentsResponse));
            InstrumentsResponse_from_LE(response);
            if (response->length>0)
            {
                plist_from_bin(header+sizeof(InstrumentsResponse),response->length,&response_plist);
                if ( response_plist )
                {
                    plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                    plist_t pid_plist = plist_array_get_item(response_object,1);
                    if (plist_get_node_type(pid_plist)==PLIST_STRING)
                    {
                        char *response_string;
                        plist_get_string_val(pid_plist,&response_string);
                        if (strcmp(response_string,"agentIsReady")==0)
                        {
                            received_agent_is_ready=1;
                        }
                        else if (strcmp(response_string,"agentIsReady:")==0)
                        {
                            received_agent_is_ready=1;
                        }
                        
                        if (response_string) MemoryFree(&response_string);
                    }
                    plist_free(response_plist);
                }
            }
            MemoryFree(&response);
        }
        if (res < 0) {
            printf("start agent reply packet no %d result %d\n",reply_packet_num,res);
            break;
        }
    }
    
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    if (header) MemoryFree(&header);
    
    return res;
}

instruments_error_t instruments_register_update(instruments_client_t client, uint32_t channel)
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    char *plistn_bin = NULL;
    uint32_t plistn_length = 0;
    uint32_t offset=0;
    
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    RequestPacket *request;
    BPLPacket *bpl_packet;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array1 = plist_new_array();
    plist_array_append_item(array1, plist_new_string("$null"));
    
    plist_t class_dict2 = plist_new_dict();
    plist_dict_set_item(class_dict2,"$class",plist_new_uid(2));
    plist_dict_set_item(class_dict2,"NS.objects",plist_new_array());
    plist_dict_set_item(class_dict2,"NS.keys",plist_new_array());
    
    plist_array_append_item(array1, class_dict2);
    
    plist_t classes_dict2 = plist_new_dict();
				plist_t classes_array2 = plist_new_array();
				plist_array_append_item(classes_array2,plist_new_string("NSDictionary"));
				plist_array_append_item(classes_array2,plist_new_string("NSObject"));
    plist_dict_set_item(classes_dict2,"$classes",classes_array2);
    plist_dict_set_item(classes_dict2,"$classname",plist_new_string("NSDictionary"));
    
    plist_array_append_item(array1, classes_dict2);
    
    plist_dict_set_item(plist1, "$objects", array1 );
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    //
    
    plist_t plist2 = plist_new_dict();
    
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string("everything"));
    
    
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    /* allocate a packet */
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC27, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = plist1_length + plist2_length + 12 + 1*sizeof(BPLPacket);
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 2;
    request->bplist_size = plist2_length;
    
    data_length = plist1_length + plist2_length + sizeof(RequestPacket) + 1*sizeof(BPLPacket);
    data = malloc (data_length* sizeof(char));
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    offset = sizeof(RequestPacket);
    
    memcpy(&data[offset],plist2_bin,plist2_length);
    offset += plist2_length;
    
    bpl_packet = (BPLPacket*) malloc(sizeof(BPLPacket));
    if (!bpl_packet) {
        return INSTRUMENTS_E_NO_MEM;
    }
    bpl_packet->nl = 10;
    bpl_packet->bplist_magic = 2;
    bpl_packet->bplist_size = plist1_length;
    
    BPLPacket_to_LE(bpl_packet);
    memcpy(&data[offset],bpl_packet,sizeof(BPLPacket));
    BPLPacket_from_LE(bpl_packet);
    offset += sizeof(BPLPacket);
    memcpy(&data[offset],plist1_bin,plist1_length);
    offset += plist1_length;
    
    plist_t plistn = plist_new_dict();
    plist_dict_set_item(plistn, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t arrayn = plist_new_array();
    plist_array_append_item(arrayn, plist_new_string("$null"));
    plist_array_append_item(arrayn, plist_new_string("installedApplicationsMatching:registerUpdateToken:"));
    plist_dict_set_item(plistn, "$objects", arrayn );
    
    plist_t dictn = plist_new_dict();
    plist_dict_set_item(dictn, "root",plist_new_uid(1));
    
    plist_dict_set_item(plistn, "$top", dictn);
    
    plist_dict_set_item(plistn, "$version", plist_new_uint(100000));
    
    plist_to_bin(plistn, &plistn_bin, &plistn_length);
    
    //debug_info("plistn returned length %d\n",plistn_length);
    
    //debug_plist(plistn);
    
    if (instruments_send_packet(client, 0, channel, 0x1002, data, data_length, plistn_bin, plistn_length, &bytes ,0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    plist_free(plistn);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&plistn_bin);
    MemoryFree(&request);
    MemoryFree(&bpl_packet);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    int received_agent_is_ready = 0;
    while ( !received_agent_is_ready)
    {
        if (header) MemoryFree(&header);
        header=NULL;
        res = instruments_receive_data_with_timeout(client, &header, &bytes_recv, &reply_packet_num,15000);
        //debug_info("reply packet no %d\n",reply_packet_num);
        if (res==INSTRUMENTS_E_SUCCESS)
        {
            InstrumentsResponse *response;
            plist_t response_plist=NULL;
            response = (InstrumentsResponse*) malloc (sizeof(InstrumentsResponse));
            if (!response)
            {
                MemoryFree(&header);
                return INSTRUMENTS_E_NO_MEM;
            }
            
            memcpy(response,header,sizeof(InstrumentsResponse));
            InstrumentsResponse_from_LE(response);
            if (response->length>0)
            {
                plist_from_bin(header+sizeof(InstrumentsResponse),response->length,&response_plist);
                if ( response_plist )
                {
                    plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                    plist_t pid_plist = plist_array_get_item(response_object,1);
                    if (plist_get_node_type(pid_plist)==PLIST_STRING)
                    {
                        char *response_string;
                        plist_get_string_val(pid_plist,&response_string);
                        if (strcmp(response_string,"agentIsReady")==0)
                            received_agent_is_ready=1;
                        if (response_string) MemoryFree(&response_string);
                    }
                    plist_free(response_plist);
                }
            }
            MemoryFree(&response);
        }
        if (res < 0) {
            printf("start agent reply packet no %d result %d\n",reply_packet_num,res);
            break;
        }
    }
    
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    if (header) MemoryFree(&header);
    
    return res;
}

instruments_error_t instruments_start_script2(instruments_client_t client, uint32_t channel, uint64_t pid, instruments_receive_cb_t callback, void* user_data )
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    void *script = NULL;
    int fd,len;
    RequestPacket *request;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    //Implementation for fixing the bug # PC-500
    struct instruments_worker_thread *iwt = (struct instruments_worker_thread*)malloc(sizeof(struct instruments_worker_thread));
    if (iwt)
    {
        iwt->client = client;
        iwt->cbfunc = NULL;
        iwt->user_data = NULL;
        if (thread_new(&client->worker,commandline_worker, iwt)==0)
        {
            res = INSTRUMENTS_E_SUCCESS;
        }
        else
        {
            MemoryFree(&iwt);
            return INSTRUMENTS_E_UNKNOWN_ERROR;
        }
    }
    
    sleep(1);
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    
    plist_t class_dict = plist_new_dict();
    plist_dict_set_item(class_dict, "$class",plist_new_uid(15));
    
				plist_t keys_array = plist_new_array();
				plist_array_append_item(keys_array, plist_new_uid(2));
				plist_array_append_item(keys_array, plist_new_uid(3));
				plist_array_append_item(keys_array, plist_new_uid(4));
				plist_array_append_item(keys_array, plist_new_uid(5));
    plist_dict_set_item(class_dict, "NS.keys",keys_array);
    
				plist_t object_array = plist_new_array();
				plist_array_append_item(object_array, plist_new_uid(6));
				plist_array_append_item(object_array, plist_new_uid(8));
				plist_array_append_item(object_array, plist_new_uid(9));
				plist_array_append_item(object_array, plist_new_uid(10));
    plist_dict_set_item(class_dict, "NS.objects",object_array);
    plist_array_append_item(array, class_dict);
    
    plist_array_append_item(array, plist_new_string("kUIAServiceScriptBodyKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTargetBundleIDKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceVerbosityOptionsKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceScriptURLKey"));
    
    plist_t class_dict2 = plist_new_dict();
    plist_dict_set_item(class_dict2, "$class",plist_new_uid(7));
    
    fd = open(client->uia_script, O_RDONLY);
    len = lseek(fd, 0, SEEK_END);
    script = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    plist_dict_set_item(class_dict2, "NS.string",plist_new_string(script)) ;
    
    if(munmap(script,len) < 0)
    {
        fprintf(stderr, "%s", "[Startscript2] ###### maunmap failed\n");
        plist_free(plist1);
        close(fd);
        return INSTRUMENTS_E_LAUNCH_FAILED_ERROR;
    }
    close(fd);
    
    plist_array_append_item(array, class_dict2);
    
    plist_t classes_dict = plist_new_dict();
				plist_t classes_array = plist_new_array();
    plist_array_append_item(classes_array, plist_new_string("NSMutableString"));
    plist_array_append_item(classes_array, plist_new_string("NSString"));
    plist_array_append_item(classes_array, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict, "$classes",classes_array);
    
				plist_dict_set_item(classes_dict, "$classname",plist_new_string("NSMutableString"));
    plist_array_append_item(array, classes_dict );
    
    plist_t class_dict3 = plist_new_dict();
    plist_dict_set_item(class_dict3, "$class",plist_new_uid(7));
    plist_dict_set_item(class_dict3, "NS.string",plist_new_string(client->package_name));
    plist_array_append_item(array, class_dict3);
    
    plist_array_append_item(array, plist_new_uint(pid));
    
    plist_t class_dict4 = plist_new_dict();
    plist_dict_set_item(class_dict4, "$class",plist_new_uid(13));
    plist_dict_set_item(class_dict4, "NS.base",plist_new_uid(11));
    plist_dict_set_item(class_dict4, "NS.relative",plist_new_uid(14));
    plist_array_append_item(array, class_dict4);
    
    plist_t class_dict5 = plist_new_dict();
    plist_dict_set_item(class_dict5, "$class",plist_new_uid(13));
    plist_dict_set_item(class_dict5, "NS.base",plist_new_uid(0));
    plist_dict_set_item(class_dict5, "NS.relative",plist_new_uid(12));
    plist_array_append_item(array, class_dict5);
    
    plist_array_append_item(array, plist_new_string("file:///tmp/"));
    
    plist_t classes_dict2 = plist_new_dict();
				plist_t classes_array2 = plist_new_array();
    plist_array_append_item(classes_array2, plist_new_string("NSURL"));
    plist_array_append_item(classes_array2, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict2, "$classes",classes_array2);
    
				plist_dict_set_item(classes_dict2, "$classname",plist_new_string("NSURL"));
    plist_array_append_item(array, classes_dict2 );
    
    plist_array_append_item(array, plist_new_string("46036B25-876E-4AA9-8A87-B05556AA45AA/script.js"));
    
    plist_t classes_dict3 = plist_new_dict();
				plist_t classes_array3 = plist_new_array();
    plist_array_append_item(classes_array3, plist_new_string("NSMutableDictionary"));
    plist_array_append_item(classes_array3, plist_new_string("NSDictionary"));
    plist_array_append_item(classes_array3, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict3, "$classes",classes_array3);
    
				plist_dict_set_item(classes_dict3, "$classname",plist_new_string("NSMutableDictionary"));
    plist_array_append_item(array, classes_dict3 );
    
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    /* allocate a packet */
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request)
    {
        plist_free(plist1);
        MemoryFree(&plist1_bin);
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC2, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = plist1_length + 12;
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 2;
    request->bplist_size = plist1_length;
    
    data_length = plist1_length + sizeof(RequestPacket);
    data = malloc (data_length* sizeof(char));
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    memcpy(&data[sizeof(RequestPacket)],plist1_bin,plist1_length);
    
    plist_t plist2 = plist_new_dict();
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string("startScriptWithInfo:"));
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    if (instruments_send_packet(client, 0, channel, 0x1002, data, data_length, plist2_bin, plist2_length, &bytes ,0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&request);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    int received_script_is_ready = 0;
    while ( !received_script_is_ready)
    {
        if (header) MemoryFree(&header);
        header=NULL;
        res = instruments_receive_data_with_timeout(client, &header, &bytes_recv, &reply_packet_num,30000);
        //debug_info("reply packet no %d\n",reply_packet_num);
        //printf("reply packet no %d\n",reply_packet_num);
        if (res==INSTRUMENTS_E_SUCCESS)
        {
            InstrumentsResponse *response;
            plist_t response_plist=NULL;
            response = (InstrumentsResponse*) malloc (sizeof(InstrumentsResponse));
            if (!response)
            {
                MemoryFree(&header);
                return INSTRUMENTS_E_NO_MEM;
            }
            
            memcpy(response,header,sizeof(InstrumentsResponse));
            InstrumentsResponse_from_LE(response);
            if (response->length>0)
            {
                plist_from_bin(header+sizeof(InstrumentsResponse)+response->payload_offset,response->length-response->payload_offset,&response_plist);
                if ( response_plist )
                {
                    //debug_plist(response_plist);
                    plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                    plist_t pid_plist = plist_array_get_item(response_object,1);
                    if (plist_get_node_type(pid_plist)==PLIST_STRING)
                    {
                        char *response_string;
                        plist_get_string_val(pid_plist,&response_string);
                        //printf("agent:%s\n",response_string);
                        if (strcmp(response_string,"updateScriptStatus:")==0)
                        {
                            plist_t response_plist2 = NULL;
                            RequestPacket *response_header2;
                            response_header2 = (RequestPacket*) malloc (sizeof(RequestPacket));
                            memcpy(response_header2,header+sizeof(InstrumentsResponse),sizeof(RequestPacket));
                            RequestPacket_from_LE(response_header2);
                            if (response_header2->bplist_size > 0)
                            {
                                plist_from_bin(header+sizeof(InstrumentsResponse)+sizeof(RequestPacket),response_header2->bplist_size,&response_plist2);
                                if (response_plist2!=NULL)
                                {
                                    plist_t response_object2 = plist_dict_get_item(response_plist2,"$objects");
                                    plist_t status_code_plist = plist_array_get_item(response_object2,1);
                                    if (plist_get_node_type(status_code_plist)==PLIST_UINT)
                                    {
                                        uint64_t status_code ;
                                        plist_get_uint_val(status_code_plist,&status_code);
                                        //printf("script status: %d \n",status_code);
                                        if (status_code==2)
                                        {
                                            received_script_is_ready=1;
                                        }
                                    }
                                    //debug_plist(response_plist2);
                                    plist_free(response_plist2);
                                }
                            }
                            MemoryFree(&response_header2);
                        }
                        if (response_string) MemoryFree(&response_string);
                    }
                    plist_free(response_plist);
                }
            }
            MemoryFree(&response);
        }
        if (res < 0)
        {
            printf("start agent reply packet no %d result %d\n",reply_packet_num,res);
            break;
        }
    }
    
    //while ( reply_packet_num != client->instruments_packet->packet_num) {
    /*while ( reply_packet_num < client->instruments_packet->packet_num) {
     if (header) free(header);
     header=NULL;
     
     res = instruments_receive_data_with_timeout(client, &header, &bytes_recv, &reply_packet_num,15000);
     debug_info("reply packet no %d\n",reply_packet_num);
     //printf("reply packet no %d\n",reply_packet_num);
     if (res < 0) {
     break;
     }
     
     }
     */
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    if (header) MemoryFree(&header);
    return res;
}

instruments_error_t instruments_start_script(instruments_client_t client, uint32_t channel, uint64_t pid, instruments_receive_cb_t callback, void* user_data, char* packagepath)
{
    char *header=NULL;
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    char *data = NULL;
    uint32_t data_length = 0;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    RequestPacket *request;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    
    plist_t class_dict = plist_new_dict();
    plist_dict_set_item(class_dict, "$class",plist_new_uid(15));
    
				plist_t keys_array = plist_new_array();
				plist_array_append_item(keys_array, plist_new_uid(2));
				plist_array_append_item(keys_array, plist_new_uid(3));
				plist_array_append_item(keys_array, plist_new_uid(4));
				plist_array_append_item(keys_array, plist_new_uid(5));
    plist_dict_set_item(class_dict, "NS.keys",keys_array);
    
				plist_t object_array = plist_new_array();
				plist_array_append_item(object_array, plist_new_uid(6));
				plist_array_append_item(object_array, plist_new_uid(8));
				plist_array_append_item(object_array, plist_new_uid(9));
				plist_array_append_item(object_array, plist_new_uid(10));
    plist_dict_set_item(class_dict, "NS.objects",object_array);
    plist_array_append_item(array, class_dict);
    
    plist_array_append_item(array, plist_new_string("kUIAServiceScriptBodyKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTargetBundleIDKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceVerbosityOptionsKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceScriptURLKey"));
    
    plist_t class_dict2 = plist_new_dict();
    plist_dict_set_item(class_dict2, "$class",plist_new_uid(7));
#if defined IOS_8_3 || defined IOS_9_0
    plist_dict_set_item(class_dict2, "NS.string",plist_new_string("UIATarget.onAlert = function onAlert(alert) {return false;}; var target = UIATarget.localTarget(); var host = target.host(); var firstTimeKeyFlag = 0; var pointState1; var pointState2;while(1) { try {var result = host.performTaskWithPathArgumentsTimeout(\"waitbufferprocessing.sh\",[\"Run 1/scr.png\"],6); if (result) {var parseResult = result.stdout.split(' '); if (parseResult[0] == 'T') { target.tap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] == 't') { target.touchAndHold({x:parseResult[1],y:parseResult[2]},1);} else if(parseResult[0] == 'H'){try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {}; target.frontMostApp().mainWindow().buttons()[\"This window will go away in a few seconds, if it does not please tap anywhere to go to HOME screen\"].tap();}\
                                                                  /****************/ \
                                                                  \
                                                                  else if(parseResult[0] == 'K')\
                                                                  {\
                                                                  var parseLen = parseResult.length; var parseString= parseResult[1]; var stateFlag = 0;\
                                                                  for(var index = 2; index < parseLen; index++)\
                                                                  {\
                                                                  parseString = parseString +' '+ parseResult[index];\
                                                                  }\
                                                                  if(firstTimeKeyFlag == 0)\
                                                                  {\
                                                                  pointState1 = target.frontMostApp().keyboard().buttons()[\"shift\"].hitpoint();\
                                                                  pointState2 = target.frontMostApp().keyboard().keys()[\"more, numbers\"].hitpoint();\
                                                                  firstTimeKeyFlag = 1;\
                                                                  }\
                                                                  for(var i = 0, len = parseString.length; i < len; i++)\
                                                                  {\
                                                                  var keyChar = parseString.charAt(i);\
                                                                  var keyPresent = 0;\
                                                                  var keyNotPresentCount = 0;\
                                                                  \
                                                                  if(keyChar == ' ')\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(\"space\").hitpoint());\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  continue;\
                                                                  }\
                                                                  while(keyPresent == 0)\
                                                                  {\
                                                                  var eleArray = target.frontMostApp().keyboard().keys();\
                                                                  for(var j = 0, lenArr = eleArray.length; j < lenArr; j++)\
                                                                  {\
                                                                  if(keyChar == eleArray[j].name())\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(keyChar).hitpoint());\
                                                                  keyPresent = 1;\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  break;\
                                                                  }\
                                                                  if(keyChar == '&' && eleArray[j].name() == 'ampersand')\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(\"ampersand\").hitpoint());\
                                                                  keyPresent = 1;\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  break;\
                                                                  }\
                                                                  }\
                                                                  \
                                                                  if(keyPresent == 0)\
                                                                  {\
                                                                  if(stateFlag == 0)\
                                                                  {\
                                                                  target.tap(pointState1);\
                                                                  stateFlag = 1;\
                                                                  }\
                                                                  else\
                                                                  {\
                                                                  target.tap(pointState2);\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  keyNotPresentCount++;\
                                                                  }\
                                                                  if(keyNotPresentCount >= 4)\
                                                                  {\
                                                                  break;\
                                                                  }\
                                                                  }\
                                                                  }\
                                                                  }\
                                                                  /***************/\
                                                                  \
                                                                  else if (parseResult[0] =='S') { target.dragFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},0.5);  } else if(parseResult[0] =='L') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_LANDSCAPELEFT); } else if(parseResult[0] =='P') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_PORTRAIT); } else if(parseResult[0] =='D') { try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {};} else if(parseResult[0] =='d') { target.doubleTap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] =='p') { target.pinchOpenFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},1); } else if (parseResult[0] =='q') { target.pinchCloseFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},1); } } } catch(e) { continue; }} ")) ;
#elif defined(IOS_10_0)
    plist_dict_set_item(class_dict2, "NS.string",plist_new_string("UIATarget.onAlert = function onAlert(alert) {return false;}; var target = UIATarget.localTarget(); var host = target.host(); var firstTimeKeyFlag = 0; var pointState1; var pointState2;while(1) { try { var result = host.performTaskWithPathArgumentsTimeout(\"waitbufferprocessing.sh\",[\"Run 1/scr.png\"],6000); if (result) {var parseResult = result.stdout.split(' '); if (parseResult[0] == 'T') { target.tap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] == 't') { target.touchAndHold({x:parseResult[1],y:parseResult[2]},1);} else if(parseResult[0] == 'H'){try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {}; target.frontMostApp().mainWindow().buttons()[\"This window will go away in a few seconds, if it does not please tap anywhere to go to HOME screen\"].tap();}\
                                                                  /****************/ \
                                                                  \
                                                                  else if(parseResult[0] == 'K')\
                                                                  {\
                                                                  var parseLen = parseResult.length; var parseString= parseResult[1]; var stateFlag = 0;\
                                                                  for(var index = 2; index < parseLen; index++)\
                                                                  {\
                                                                  parseString = parseString +' '+ parseResult[index];\
                                                                  }\
                                                                  if(firstTimeKeyFlag == 0)\
                                                                  {\
                                                                  pointState1 = target.frontMostApp().keyboard().buttons()[\"shift\"].hitpoint();\
                                                                  pointState2 = target.frontMostApp().keyboard().keys()[\"more\"].hitpoint();\
                                                                  firstTimeKeyFlag = 1;\
                                                                  }\
                                                                  for(var i = 0, len = parseString.length; i < len; i++)\
                                                                  {\
                                                                  var keyChar = parseString.charAt(i);\
                                                                  var keyPresent = 0;\
                                                                  var keyNotPresentCount = 0;\
                                                                  \
                                                                  if(keyChar == ' ')\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(\"space\").hitpoint());\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  continue;\
                                                                  }\
                                                                  while(keyPresent == 0)\
                                                                  {\
                                                                  var eleArray = target.frontMostApp().keyboard().keys();\
                                                                  for(var j = 0, lenArr = eleArray.length; j < lenArr; j++)\
                                                                  {\
                                                                  if(keyChar == eleArray[j].name())\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(keyChar).hitpoint());\
                                                                  keyPresent = 1;\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  break;\
                                                                  }\
                                                                  if(keyChar == '&' && eleArray[j].name() == 'ampersand')\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(\"ampersand\").hitpoint());\
                                                                  keyPresent = 1;\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  break;\
                                                                  }\
                                                                  }\
                                                                  \
                                                                  if(keyPresent == 0)\
                                                                  {\
                                                                  if(stateFlag == 0)\
                                                                  {\
                                                                  target.tap(pointState1);\
                                                                  stateFlag = 1;\
                                                                  }\
                                                                  else\
                                                                  {\
                                                                  target.tap(pointState2);\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  keyNotPresentCount++;\
                                                                  }\
                                                                  if(keyNotPresentCount >= 4)\
                                                                  {\
                                                                  break;\
                                                                  }\
                                                                  }\
                                                                  }\
                                                                  }\
                                                                  /***************/\
                                                                  \
                                                                  else if (parseResult[0] =='S') { target.dragFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},0.5);  } else if(parseResult[0] =='L') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_LANDSCAPELEFT); } else if(parseResult[0] =='P') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_PORTRAIT); } else if(parseResult[0] =='D') { try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {};} else if(parseResult[0] =='d') { target.doubleTap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] =='p') { target.pinchOpenFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},1); } else if (parseResult[0] =='q') { target.pinchCloseFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},1); } } } catch(e) { continue; }} ")) ;
    
#else
    plist_dict_set_item(class_dict2, "NS.string",plist_new_string("UIATarget.onAlert = function onAlert(alert) {return false;}; try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {} ; var target = UIATarget.localTarget(); var host = target.host(); var firstTimeKeyFlag = 0; var pointState1; var pointState2; while(1) { try {var result = host.performTaskWithPathArgumentsTimeout(\"waitbufferprocessing.sh\",[\"Run 1/scr.png\"],6); if (result) {var parseResult = result.stdout.split(' '); if (parseResult[0] == 'T') { target.tap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] == 't') { target.touchAndHold({x:parseResult[1],y:parseResult[2]},1); } else if(parseResult[0] == 'H'){try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {}; target.frontMostApp().mainWindow().buttons()[\"This window will go away in a few seconds, if it does not please tap anywhere to go to HOME screen\"].tap();}\
                                                                  /****************/ \
                                                                  \
                                                                  else if(parseResult[0] == 'K')\
                                                                  {\
                                                                  var parseLen = parseResult.length; var parseString= parseResult[1]; var stateFlag = 0;\
                                                                  for(var index = 2; index < parseLen; index++)\
                                                                  {\
                                                                  parseString = parseString +' '+ parseResult[index];\
                                                                  }\
                                                                  if(firstTimeKeyFlag == 0)\
                                                                  {\
                                                                  pointState1 = target.frontMostApp().keyboard().buttons()[\"shift\"].hitpoint();\
                                                                  pointState2 = target.frontMostApp().keyboard().keys()[\"more, numbers\"].hitpoint();\
                                                                  firstTimeKeyFlag = 1;\
                                                                  }\
                                                                  for(var i = 0, len = parseString.length; i < len; i++)\
                                                                  {\
                                                                  var keyChar = parseString.charAt(i);\
                                                                  var keyPresent = 0;\
                                                                  var keyNotPresentCount = 0;\
                                                                  \
                                                                  if(keyChar == ' ')\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(\"space\").hitpoint());\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  continue;\
                                                                  }\
                                                                  while(keyPresent == 0)\
                                                                  {\
                                                                  var eleArray = target.frontMostApp().keyboard().keys();\
                                                                  var delay = 5000;\
                                                                  while(delay)\
                                                                  {\
                                                                  delay--;\
                                                                  }\
                                                                  for(var j = 0, lenArr = eleArray.length; j < lenArr; j++)\
                                                                  {\
                                                                  if(keyChar == eleArray[j].name())\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(keyChar).hitpoint());\
                                                                  keyPresent = 1;\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  break;\
                                                                  }\
                                                                  if(keyChar == '&' && eleArray[j].name() == 'ampersand')\
                                                                  {\
                                                                  target.tap(target.frontMostApp().keyboard().keys().firstWithName(\"ampersand\").hitpoint());\
                                                                  keyPresent = 1;\
                                                                  if(stateFlag == 1)\
                                                                  {\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  break;\
                                                                  }\
                                                                  }\
                                                                  \
                                                                  if(keyPresent == 0)\
                                                                  {\
                                                                  if(stateFlag == 0)\
                                                                  {\
                                                                  target.tap(pointState1);\
                                                                  stateFlag = 1;\
                                                                  }\
                                                                  else\
                                                                  {\
                                                                  target.tap(pointState2);\
                                                                  stateFlag = 0;\
                                                                  }\
                                                                  keyNotPresentCount++;\
                                                                  }\
                                                                  if(keyNotPresentCount >= 4)\
                                                                  {\
                                                                  break;\
                                                                  }\
                                                                  }\
                                                                  }\
                                                                  }\
                                                                  /***************/\
                                                                  else if (parseResult[0] =='S') { target.dragFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},0.5);  } else if(parseResult[0] =='L') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_LANDSCAPELEFT); } else if(parseResult[0] =='P') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_PORTRAIT); } else if(parseResult[0] =='D') { try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {};} else if(parseResult[0] =='d') { target.doubleTap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] =='p') { target.pinchOpenFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},1); } else if (parseResult[0] =='q') { target.pinchCloseFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},1); } }} catch(e) { continue; } } ")) ;
#endif
    //plist_dict_set_item(class_dict2, "NS.string",plist_new_string("UIATarget.onAlert = function onAlert(alert) {return false;}; try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {} ; var target = UIATarget.localTarget(); var host = target.host(); while(1) { try { var result = host.performTaskWithPathArgumentsTimeout(\"waitbufferprocessing.sh\",[\"Run 1/scr.png\"],6000); if (result) {var parseResult = result.stdout.split(' '); if (parseResult[0] == 'T') { target.tap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] == 't') { target.setLocation({latitude:10.0,longitude:10.0}); } else if (parseResult[0] =='S') { target.dragFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},0.5);  } else if(parseResult[0] =='L') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_LANDSCAPELEFT); } else if(parseResult[0] =='P') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_PORTRAIT); } else if(parseResult[0] =='D') { try { UIATarget.localTarget().frontMostApp().alert().buttons()[\"OK\"].tap();} catch(e) {};} else if(parseResult[0] =='d') { target.doubleTap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] =='p') { target.pinchOpenFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},1); } else if (parseResult[0] =='q') { target.pinchCloseFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},1); } } } catch(e) { continue; } } ")) ;
    //plist_dict_set_item(class_dict2, "NS.string",plist_new_string("var target = UIATarget.localTarget(); var host = target.host(); UIATarget.localTarget().lockForDuration(1); UIATarget.onAlert = function onAlert(alert) {return false;}; target.tap({x:100,y:100}); while(1) { var result = host.performTaskWithPathArgumentsTimeout(\"waitbufferprocessing.sh\",[\"Run 1/scr.png\"],6000); if (result) {var parseResult = result.stdout.split(' '); if (parseResult[0] == 'T') { target.tap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] =='S') { target.dragFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},0.5);  } else if(parseResult[0] =='L') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_LANDSCAPELEFT); } else if(parseResult[0] =='P') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_PORTRAIT); } } } ")) ;
    //plist_dict_set_item(class_dict2, "NS.string",plist_new_string("UIALogger.logStart(\"starting ...\");UIATarget.onAlert = function(alert) { UIALogger.logMessage(\"alert...\"); var target = UIATarget.localTarget(); target.pushTimeout(10); try { alert.cancelButton().tap();} catch (e) { UIALogger.logMessage(\"caught\"); } target.popTimeout(); return true; }; var target = UIATarget.localTarget(); var host = target.host(); target.delay(2); while(1) { try { var result = UIATarget.localTarget().host().performTaskWithPathArgumentsTimeout(\"waitbufferprocessing.sh\",[\"Run 1/scr.png\"],6000); if (result) {var parseResult = result.stdout.split(' '); if (parseResult[0] == 'T') { target.tap({x:parseResult[1],y:parseResult[2]}); } else if (parseResult[0] =='S') { target.dragFromToForDuration({x:parseResult[1],y:parseResult[2]},{x:parseResult[3],y:parseResult[4]},0.5);  } else if(parseResult[0] =='L') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_LANDSCAPELEFT); } else if(parseResult[0] =='P') { target.setDeviceOrientation(UIA_DEVICE_ORIENTATION_PORTRAIT); } } } catch (e) { continue; }} ")) ;
    
    plist_array_append_item(array, class_dict2);
    
    plist_t classes_dict = plist_new_dict();
				plist_t classes_array = plist_new_array();
    plist_array_append_item(classes_array, plist_new_string("NSMutableString"));
    plist_array_append_item(classes_array, plist_new_string("NSString"));
    plist_array_append_item(classes_array, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict, "$classes",classes_array);
    
				plist_dict_set_item(classes_dict, "$classname",plist_new_string("NSMutableString"));
    plist_array_append_item(array, classes_dict );
    
    plist_t class_dict3 = plist_new_dict();
    plist_dict_set_item(class_dict3, "$class",plist_new_uid(7));
    plist_dict_set_item(class_dict3, "NS.string",plist_new_string(client->package_name));
    plist_array_append_item(array, class_dict3);
    
    plist_array_append_item(array, plist_new_uint(pid));
    
    plist_t class_dict4 = plist_new_dict();
    plist_dict_set_item(class_dict4, "$class",plist_new_uid(13));
    plist_dict_set_item(class_dict4, "NS.base",plist_new_uid(11));
    plist_dict_set_item(class_dict4, "NS.relative",plist_new_uid(14));
    plist_array_append_item(array, class_dict4);
    
    plist_t class_dict5 = plist_new_dict();
    plist_dict_set_item(class_dict5, "$class",plist_new_uid(13));
    plist_dict_set_item(class_dict5, "NS.base",plist_new_uid(0));
    plist_dict_set_item(class_dict5, "NS.relative",plist_new_uid(12));
    plist_array_append_item(array, class_dict5);
    
    plist_array_append_item(array, plist_new_string("file:///tmp/"));
    
    plist_t classes_dict2 = plist_new_dict();
				plist_t classes_array2 = plist_new_array();
    plist_array_append_item(classes_array2, plist_new_string("NSURL"));
    plist_array_append_item(classes_array2, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict2, "$classes",classes_array2);
    
				plist_dict_set_item(classes_dict2, "$classname",plist_new_string("NSURL"));
    plist_array_append_item(array, classes_dict2 );
    
    plist_array_append_item(array, plist_new_string("36036B25-876E-4AA9-8A87-B05556AA45AA/vncscript.js"));
    
    plist_t classes_dict3 = plist_new_dict();
				plist_t classes_array3 = plist_new_array();
    plist_array_append_item(classes_array3, plist_new_string("NSMutableDictionary"));
    plist_array_append_item(classes_array3, plist_new_string("NSDictionary"));
    plist_array_append_item(classes_array3, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict3, "$classes",classes_array3);
    
				plist_dict_set_item(classes_dict3, "$classname",plist_new_string("NSMutableDictionary"));
    plist_array_append_item(array, classes_dict3 );
    
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1);
    
    /* allocate a packet */
    request = (RequestPacket *) malloc(sizeof(RequestPacket));
    if (!request) {
        return INSTRUMENTS_E_NO_MEM;
    }
    
    memcpy(request->magic, INSTRUMENTS_MAGIC2, INSTRUMENTS_MAGIC2_LEN);
    
    request->length = plist1_length + 12;
    request->empty = 0;
    request->nl = 10;
    request->bplist_magic = 2;
    request->bplist_size = plist1_length;
    
    data_length = plist1_length + sizeof(RequestPacket);
    data = malloc (data_length * sizeof(char));
    
    RequestPacket_to_LE(request);
    memcpy(data,request,sizeof(RequestPacket));
    RequestPacket_from_LE(request);
    
    memcpy(&data[sizeof(RequestPacket)],plist1_bin,plist1_length);
    
    plist_t plist2 = plist_new_dict();
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string("startScriptWithInfo:"));
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    if (instruments_send_packet(client, 0, channel, 0x1002, data, data_length, plist2_bin, plist2_length, &bytes ,0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    plist_free(plist2);
    MemoryFree(&plist1_bin);
    MemoryFree(&plist2_bin);
    MemoryFree(&request);
    MemoryFree(&data);
    
    reply_packet_num = 0;
    int received_script_is_ready = 0;
    while ( !received_script_is_ready)
    {
        if (header) MemoryFree(&header);
        header=NULL;
        res = instruments_receive_data_with_timeout(client, &header, &bytes_recv, &reply_packet_num,30000);
        //debug_info("reply packet no %d\n",reply_packet_num);
        //printf("reply packet no %d\n",reply_packet_num);
        if (res==INSTRUMENTS_E_SUCCESS)
        {
            InstrumentsResponse *response;
            plist_t response_plist=NULL;
            response = (InstrumentsResponse*) malloc (sizeof(InstrumentsResponse));
            if (!response)
                return INSTRUMENTS_E_NO_MEM;
            
            memcpy(response,header,sizeof(InstrumentsResponse));
            InstrumentsResponse_from_LE(response);
            if (response->length>0)
            {
                plist_from_bin(header+sizeof(InstrumentsResponse)+response->payload_offset,response->length-response->payload_offset,&response_plist);
                if ( response_plist )
                {
                    //debug_plist(response_plist);
                    plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                    plist_t pid_plist = plist_array_get_item(response_object,1);
                    if (plist_get_node_type(pid_plist)==PLIST_STRING)
                    {
                        char *response_string;
                        plist_get_string_val(pid_plist,&response_string);
                        //printf("agent:%s\n",response_string);
                        if (strcmp(response_string,"updateScriptStatus:")==0)
                        {
                            plist_t response_plist2 = NULL;
                            RequestPacket *response_header2;
                            response_header2 = (RequestPacket*) malloc (sizeof(RequestPacket));
                            memcpy(response_header2,header+sizeof(InstrumentsResponse),sizeof(RequestPacket));
                            RequestPacket_from_LE(response_header2);
                            if (response_header2->bplist_size > 0)
                            {
                                plist_from_bin(header+sizeof(InstrumentsResponse)+sizeof(RequestPacket),response_header2->bplist_size,&response_plist2);
                                if (response_plist2!=NULL)
                                {
                                    plist_t response_object2 = plist_dict_get_item(response_plist2,"$objects");
                                    plist_t status_code_plist = plist_array_get_item(response_object2,1);
                                    if (plist_get_node_type(status_code_plist)==PLIST_UINT)
                                    {
                                        uint64_t status_code ;
                                        plist_get_uint_val(status_code_plist,&status_code);
                                        //printf("script status: %d \n",status_code);
                                        if (status_code==2)
                                        {
                                            received_script_is_ready=1;
                                        }
                                    }
                                    //debug_plist(response_plist2);
                                    plist_free(response_plist2);
                                }
                            }
                            MemoryFree(&response_header2);
                        }
                        if (response_string) MemoryFree(&response_string);
                    }
                    plist_free(response_plist);
                }
            }
            MemoryFree(&response);
        }
        if (res < 0) {
            printf("start agent reply packet no %d result %d\n",reply_packet_num,res);
            break;
        }
    }
    //while ( reply_packet_num != client->instruments_packet->packet_num) {
    /*while ( reply_packet_num < client->instruments_packet->packet_num) {
     if (header) free(header);
     header=NULL;
     
     res = instruments_receive_data(client, &header, &bytes_recv, &reply_packet_num);
     debug_info("reply packet no %d\n",reply_packet_num);
     //printf("inside start script reply packet no %d expecting %d\n",reply_packet_num,client->instruments_packet->packet_num);
     if (res < 0) {
     break;
     }
     
     }
     */
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    if (header) MemoryFree(&header);
    
    if (callback)
    {
        struct instruments_worker_thread *iwt = (struct instruments_worker_thread*)malloc(sizeof(struct instruments_worker_thread));
        if (iwt) {
            iwt->client = client;
            iwt->cbfunc = callback;
            iwt->user_data = user_data;
            iwt->packagepath = packagepath;
            if (thread_new(&client->worker, instruments_worker, iwt)==0)
            {
                res = INSTRUMENTS_E_SUCCESS;
            }
        }
    }
    
    return res;
}

instruments_error_t instruments_stop_script(instruments_client_t client, uint32_t channel)
{
    char *header=NULL;
    char *plist2_bin = NULL;
    uint32_t plist2_length = 0;
    uint32_t bytes = 0;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    if (client->worker)
    {
        //	pthread_kill(client->worker,SIGINT);
    }
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist2 = plist_new_dict();
    plist_dict_set_item(plist2, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_string("$null"));
    plist_array_append_item(array2, plist_new_string("stopScript"));
    plist_dict_set_item(plist2, "$objects", array2 );
    
    plist_t dict2 = plist_new_dict();
    plist_dict_set_item(dict2, "root",plist_new_uid(1));
    
    plist_dict_set_item(plist2, "$top", dict2);
    
    plist_dict_set_item(plist2, "$version", plist_new_uint(100000));
    
    plist_to_bin(plist2, &plist2_bin, &plist2_length);
    
    //debug_info("plist2 returned length %d\n",plist2_length);
    
    //debug_plist(plist2);
    
    if (instruments_send_packet(client, 0, channel, 0x0002, NULL, 0, plist2_bin, plist2_length, &bytes , 0) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist2);
    MemoryFree(&plist2_bin);
    
    reply_packet_num = 0;
    while ( reply_packet_num < client->instruments_packet->packet_num) {
        
        res = instruments_receive_data(client, &header, &bytes_recv, &reply_packet_num);
        //debug_info("reply packet no %d\n",reply_packet_num);
        MemoryFree(&header);
        header=NULL;
        if (res < 0) {
            break;
        }
        
    }
    
    if (reply_packet_num > client->instruments_packet->packet_num)
    {
        client->instruments_packet->packet_num = reply_packet_num;
    }
    
    return res;
}


instruments_error_t instruments_stop_worker(instruments_client_t client)
{
    if (client)
        client->exiting = 1;
    return INSTRUMENTS_E_SUCCESS;
}

instruments_error_t instruments_send_stdout(instruments_client_t client, uint32_t channel, uint32_t packet_num, const char *std_out )
{
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    uint32_t bytes = 0;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    
    plist_t class_dict = plist_new_dict();
    plist_dict_set_item(class_dict, "$class",plist_new_uid(11));
    
				plist_t keys_array = plist_new_array();
				plist_array_append_item(keys_array, plist_new_uid(2));
				plist_array_append_item(keys_array, plist_new_uid(3));
				plist_array_append_item(keys_array, plist_new_uid(4));
				plist_array_append_item(keys_array, plist_new_uid(5));
				plist_array_append_item(keys_array, plist_new_uid(6));
    plist_dict_set_item(class_dict, "NS.keys",keys_array);
    
				plist_t object_array = plist_new_array();
				plist_array_append_item(object_array, plist_new_uid(7));
				plist_array_append_item(object_array, plist_new_uid(8));
				plist_array_append_item(object_array, plist_new_uid(8));
				plist_array_append_item(object_array, plist_new_uid(9));
				plist_array_append_item(object_array, plist_new_uid(10));
    plist_dict_set_item(class_dict, "NS.objects",object_array);
    plist_array_append_item(array, class_dict);
    
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskStandardErrorKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskExitCodeKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskSignalKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskTimedOutKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskStandardOutputKey"));
    plist_array_append_item(array, plist_new_string(""));
    plist_array_append_item(array, plist_new_uint(0));
    plist_array_append_item(array, plist_new_bool(0));
    if (std_out)
        plist_array_append_item(array, plist_new_string(std_out));
    else
        plist_array_append_item(array, plist_new_string(""));
    
    plist_t classes_dict = plist_new_dict();
				plist_t classes_array = plist_new_array();
    plist_array_append_item(classes_array, plist_new_string("NSMutableDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict, "$classes",classes_array);
    
				plist_dict_set_item(classes_dict, "$classname",plist_new_string("NSMutableDictionary"));
    plist_array_append_item(array, classes_dict );
    
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1); //Note this debug shows as leak memory
    
    if (instruments_send_packet(client, 0x01, 0xfffffff7, 0x03, NULL, 0, plist1_bin, plist1_length, &bytes, packet_num ) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    MemoryFree(&plist1_bin);
    
    return res;
}

instruments_error_t instruments_send_timeout(instruments_client_t client, uint32_t channel, uint32_t packet_num )
{
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    uint32_t bytes = 0;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    
    plist_t class_dict = plist_new_dict();
    plist_dict_set_item(class_dict, "$class",plist_new_uid(15));
    
				plist_t keys_array = plist_new_array();
				plist_array_append_item(keys_array, plist_new_uid(2));
				plist_array_append_item(keys_array, plist_new_uid(3));
				plist_array_append_item(keys_array, plist_new_uid(4));
				plist_array_append_item(keys_array, plist_new_uid(5));
				plist_array_append_item(keys_array, plist_new_uid(6));
				plist_array_append_item(keys_array, plist_new_uid(7));
    plist_dict_set_item(class_dict, "NS.keys",keys_array);
    
				plist_t object_array = plist_new_array();
				plist_array_append_item(object_array, plist_new_uid(8));
				plist_array_append_item(object_array, plist_new_uid(9));
				plist_array_append_item(object_array, plist_new_uid(9));
				plist_array_append_item(object_array, plist_new_uid(10));
				plist_array_append_item(object_array, plist_new_uid(8));
				plist_array_append_item(object_array, plist_new_uid(11));
    plist_dict_set_item(class_dict, "NS.objects",object_array);
    plist_array_append_item(array, class_dict);
    
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskStandardErrorKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskExitCodeKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskSignalKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskTimedOutKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskStandardOutputKey"));
    plist_array_append_item(array, plist_new_string("kUIAServiceTaskExceptionKey"));
    plist_array_append_item(array, plist_new_string(""));
    plist_array_append_item(array, plist_new_uint(15));
    plist_array_append_item(array, plist_new_bool(1));
    
    plist_t userinfo_dict = plist_new_dict();
				plist_dict_set_item(userinfo_dict, "NS.userinfo", plist_new_uid(0));
				plist_dict_set_item(userinfo_dict, "NS.reason", plist_new_uid(13));
				plist_dict_set_item(userinfo_dict, "NS.name", plist_new_uid(12));
				plist_dict_set_item(userinfo_dict, "$class", plist_new_uid(14));
    
    plist_array_append_item(array, userinfo_dict );
    
    plist_array_append_item(array, plist_new_string("UIAHostTaskTimeoutException"));
    plist_array_append_item(array, plist_new_string("The task performed on host was terminated due to timeout."));
    plist_t classes_dict_excp = plist_new_dict();
				plist_t classes_array_excp = plist_new_array();
    plist_array_append_item(classes_array_excp, plist_new_string("NSException"));
    plist_array_append_item(classes_array_excp, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict_excp, "$classes", classes_array_excp);
    
				plist_dict_set_item(classes_dict_excp, "$classname",plist_new_string("NSException"));
    plist_array_append_item(array, classes_dict_excp );
    
    
    
    plist_t classes_dict = plist_new_dict();
				plist_t classes_array = plist_new_array();
    plist_array_append_item(classes_array, plist_new_string("NSMutableDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict, "$classes",classes_array);
    
				plist_dict_set_item(classes_dict, "$classname",plist_new_string("NSMutableDictionary"));
    plist_array_append_item(array, classes_dict );
    
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1); //Note this debug shows as leak memory
    
    if (instruments_send_packet(client, 0x01, 0xfffffff7, 0x03, NULL, 0, plist1_bin, plist1_length, &bytes, packet_num ) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    MemoryFree(&plist1_bin);
    
    return res;
}

instruments_error_t instruments_send_agent_exception(instruments_client_t client, uint32_t channel, uint32_t packet_num )
{
    char *plist1_bin = NULL;
    uint32_t plist1_length = 0;
    uint32_t bytes = 0;
    
    if (!client || !client->parent)
        return INSTRUMENTS_E_INVALID_ARG;
    
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    
    plist_t plist1 = plist_new_dict();
    plist_dict_set_item(plist1, "$archiver", plist_new_string("NSKeyedArchiver"));
    
    plist_t array = plist_new_array();
    plist_array_append_item(array, plist_new_string("$null"));
    
				plist_t class_dict = plist_new_dict();
    plist_dict_set_item(class_dict, "NSCode",plist_new_uint(1));
				plist_dict_set_item(class_dict, "NSUserInfo",plist_new_uid(3));
				plist_dict_set_item(class_dict, "NSDomain",plist_new_uid(2));
				plist_dict_set_item(class_dict, "$class",plist_new_uid(10));
    plist_array_append_item(array, class_dict);
    
    plist_array_append_item(array, plist_new_string("DTXMessage"));
    
    plist_t nskey_dict = plist_new_dict();
    plist_t array2 = plist_new_array();
    plist_array_append_item(array2, plist_new_uid(4));
    plist_dict_set_item(nskey_dict, "NS.keys", array2);
    
    plist_t array3 = plist_new_array();
    plist_array_append_item(array3, plist_new_uid(5));
    plist_dict_set_item(nskey_dict, "NS.objects", array3);
    plist_dict_set_item(nskey_dict, "$class",plist_new_uid(9));
    
    plist_array_append_item(array, nskey_dict );
    
    plist_array_append_item(array, plist_new_string("DTXExceptionKey"));
    
    plist_t userinfo_dict = plist_new_dict();
				plist_dict_set_item(userinfo_dict, "NS.userinfo", plist_new_uid(0));
				plist_dict_set_item(userinfo_dict, "NS.reason", plist_new_uid(7));
				plist_dict_set_item(userinfo_dict, "NS.name", plist_new_uid(6));
				plist_dict_set_item(userinfo_dict, "$class", plist_new_uid(8));
    
    plist_array_append_item(array, userinfo_dict );
    
    plist_array_append_item(array, plist_new_string("DTXMessageInvocationException"));
    plist_array_append_item(array, plist_new_string("Unable to invoke -[<UIAInstrument: 0x7f8dfa885000> agentIsReady:] - it does not respond to the selector"));
    plist_t classes_dict_excp = plist_new_dict();
				plist_t classes_array_excp = plist_new_array();
    plist_array_append_item(classes_array_excp, plist_new_string("NSException"));
    plist_array_append_item(classes_array_excp, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict_excp, "$classes", classes_array_excp);
    
				plist_dict_set_item(classes_dict_excp, "$classname",plist_new_string("NSException"));
    plist_array_append_item(array, classes_dict_excp );
    
    
    
    plist_t classes_dict = plist_new_dict();
				plist_t classes_array = plist_new_array();
    plist_array_append_item(classes_array, plist_new_string("NSDictionary"));
    plist_array_append_item(classes_array, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict, "$classes",classes_array);
    
				plist_dict_set_item(classes_dict, "$classname",plist_new_string("NSDictionary"));
    plist_array_append_item(array, classes_dict );
    
    plist_t classes_dict_nserror = plist_new_dict();
				plist_t classes_array_nserror = plist_new_array();
    plist_array_append_item(classes_array_nserror, plist_new_string("NSError"));
    plist_array_append_item(classes_array_nserror, plist_new_string("NSObject"));
				plist_dict_set_item(classes_dict_nserror, "$classes",classes_array_nserror);
    
				plist_dict_set_item(classes_dict_nserror, "$classname",plist_new_string("NSError"));
    plist_array_append_item(array, classes_dict_nserror );
    
    plist_dict_set_item(plist1, "$objects", array );
    
    plist_dict_set_item(plist1, "$version", plist_new_uint(100000));
    
    plist_t dict1 = plist_new_dict();
    plist_dict_set_item(dict1, "root",plist_new_uid(1));
    plist_dict_set_item(plist1, "$top", dict1);
    
    plist_to_bin(plist1, &plist1_bin, &plist1_length);
    
    //debug_info("plist1 returned length %d\n",plist1_length);
    
    //debug_plist(plist1); //Note this debug shows as leak memory
    
    if (instruments_send_packet(client, 0x01, 0xfffffff7, 0x04, NULL, 0, plist1_bin, plist1_length, &bytes, packet_num ) != INSTRUMENTS_E_SUCCESS ) {
        res = INSTRUMENTS_E_MUX_ERROR;
    }
    
    plist_free(plist1);
    MemoryFree(&plist1_bin);
    
    return res;
}

instruments_error_t instruments_get_response(instruments_client_t client)
{
    char *header=NULL;
    char *cmd_str=NULL;
    instruments_error_t res = INSTRUMENTS_E_UNKNOWN_ERROR;
    uint32_t bytes_recv = 0;
    uint32_t reply_packet_num,last_plist_size,last_plist_offset;
    
    RequestPacket *response_header2 = NULL;
    plist_t response_plist=NULL;
    plist_t response_plist2=NULL;
    plist_t response_plist3=NULL;
    plist_t response_plist4=NULL;
    BPLPacket *bpl_packet2 = NULL;
    BPLPacket *bpl_packet3 = NULL;
    
    InstrumentsResponse *response = NULL;
    response = (InstrumentsResponse*) malloc (sizeof(InstrumentsResponse));
    if (!response)
        return res;
    
    response_header2 = (RequestPacket*) malloc (sizeof(RequestPacket));
    
    if (!response_header2)
    {
        MemoryFree(&response);
        return res;
    }
    
    bpl_packet2 = (BPLPacket*) malloc (sizeof(BPLPacket));
    if (!bpl_packet2)
    {
        MemoryFree(&response);
        MemoryFree(&response_header2);
        return res;
    }
    
    bpl_packet3 = (BPLPacket*) malloc (sizeof(BPLPacket));
    if (!bpl_packet3)
    {
        MemoryFree(&response);
        MemoryFree(&response_header2);
        MemoryFree(&bpl_packet2);
        return res;
    }
    
    //debug_info("Running");
    
    res = instruments_receive_data_with_timeout(client, &header, &bytes_recv, &reply_packet_num,10000);
    //debug_info("reply packet no %d\n",reply_packet_num);
    
    //printf("reply packaged no %d, res val %d\n",reply_packet_num,res);
    
    
    if (res==INSTRUMENTS_E_SUCCESS)
    {
        //debug_info("before memcpy\n");
        memcpy(response,header,sizeof(InstrumentsResponse));
        //debug_info("after memcpy\n");
        InstrumentsResponse_from_LE(response);
        //debug_info("response_length %d",response->length);
        //printf("msg_type %d\n",response->msg_type);
        if (response->length>0 && response->msg_type == 0x3 /* sizeof(RequestPacket) && response->msg_type ==0x1002*/)
        {
            plist_from_bin(header+sizeof(InstrumentsResponse),response->length,&response_plist);
            if (response_plist!=NULL)
            {
                plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                int msg_type = 0;
                int response_code_offset=8;
                int time_stamp_offset=6;
                double time_stamp;
                uint64_t response_code=0;
                
                //debug_plist(response_plist);
                
                plist_t response_type_plist = plist_array_get_item(response_object,5);
                if (plist_get_node_type(response_type_plist)==PLIST_STRING)
                {
                    char *string = NULL;
                    plist_get_string_val(response_type_plist,&string);
                    //printf("message type string: %s\n",string);
                    if (strcmp(string,"Screenshot")==0)
                    {
                        msg_type = 2;
                        response_code_offset=9;
                        time_stamp_offset=7;
                    }
                    else
                    {
                        msg_type = 0;
                        response_code_offset=8;
                        time_stamp_offset=6;
                    }
                    if (string) MemoryFree(&string);
                }
                else
                {
                    msg_type = 1;
                    response_code_offset=7;
                    time_stamp_offset=5;
                }
                
                plist_t response_code_plist = plist_array_get_item(response_object,response_code_offset);
                if (msg_type!=2)
                {
                    
                    if (plist_get_node_type(response_code_plist)==PLIST_UINT)
                    {
                        plist_get_uint_val(response_code_plist,&response_code);
                        //printf("response: %d \n",response_code);
                    }
                    else
                    {
                        //printf("response no code\n");
                    }
                }
                else
                {
                    char *screenshot_name_string=NULL;
                    char *screenshot_data=NULL;
                    char screenshot_filename[1024];
                    uint64_t screenshot_size=0;
                    FILE * screenshot_file =  NULL;
                    
                    if (plist_get_node_type(response_code_plist)!=PLIST_STRING)
                    {
                        /* 8.1.x onwards the position of screenshot name, screenshot data and time stamp has changed */
                        response_code_offset=8;
                        time_stamp_offset=11;
                        response_code_plist = plist_array_get_item(response_object,response_code_offset);
                    }
                    plist_t screenshot_dict_plist = plist_array_get_item(response_object,response_code_offset+1);
                    
                    plist_t screenshot_plist = plist_dict_get_item(screenshot_dict_plist, "NS.data");
                    
                    if ((screenshot_dict_plist) && (screenshot_plist))
                    {
                        plist_get_data_val(screenshot_plist,&screenshot_data,&screenshot_size);
                        plist_get_string_val(response_code_plist,&screenshot_name_string);
                        sprintf(screenshot_filename, "%s/Run 1/%s.png",client->uia_result_path,screenshot_name_string);
                        //sprintf(screenshot_filename, "/tmp/appium-instruments/Run 1/%s.png",screenshot_name_string);
                        screenshot_file = fopen(screenshot_filename, "wb" );
                        if (screenshot_file != NULL)
                        {
                            fwrite(screenshot_data, 1, screenshot_size, screenshot_file);
                            fclose(screenshot_file);
                            screenshot_file = NULL;
                        }
                        else
                        {
                            fprintf(stderr,"[instruments_get_response] Screen shot data could not be written to file, file open failed\n");
                        }
                    }
                    if (screenshot_name_string) MemoryFree(&screenshot_name_string);
                    if (screenshot_data) MemoryFree(&screenshot_data);
                }
                plist_t time_stamp_plist_dict = plist_array_get_item(response_object, time_stamp_offset);
                plist_t time_stamp_plist = plist_dict_get_item(time_stamp_plist_dict, "NS.time");
                
                if (plist_get_node_type(time_stamp_plist)==PLIST_REAL)
                {
                    time_t seconds;
                    char outstr[100];
                    plist_get_real_val(time_stamp_plist, &time_stamp);
                    seconds = time_stamp+978307200;
                    struct tm *tmptr = gmtime(&seconds);
                    //printf("timestamp:%f %ld \n",time_stamp,seconds);
                    //printf("gmt:%s \n",asctime(tmptr));
                    if (response_code>0)
                    {
                        strftime(outstr,sizeof(outstr),"%F %T %z",tmptr);
                        //fprintf(stderr,"%s ",outstr);
                    }
                }
                else
                {
                    //printf("timestamp:%d \n",plist_get_node_type(time_stamp_plist));
                }
                
                if (response_code>0)
                {
                    if (response_code==4)
                    {
                        //fprintf(stderr,"Start: ");
                    }
                    else if (response_code==5)
                    {
                        //fprintf(stderr,"Pass: ");
                    }
                    else if (response_code==1)
                    {
                        //fprintf(stderr,"Default: ");
                    }
                    
                    if (msg_type==0 )	
                    {
                        plist_t response_text_plist = plist_array_get_item(response_object,9);
                        if (plist_get_node_type(response_text_plist)==PLIST_STRING)
                        {
                            char *response_string;
                            plist_get_string_val(response_text_plist,&response_string);
                            printf("%s\n",response_string);
                            if (response_string) MemoryFree(&response_string);
                        }
                        else
                        {
                            fprintf(stderr,"(null)\n");
                        }
                        //plist_free(response_plist);
                    }
                    else
                    {
                        fprintf(stderr,"(null)\n");
                    }
                }
                plist_free(response_plist);
            }
        } 
        else if (response->length>sizeof(RequestPacket) && response->msg_type ==0x1002)
        {
            memcpy(response_header2,header+sizeof(InstrumentsResponse),sizeof(RequestPacket));
            RequestPacket_from_LE(response_header2);
            if (response_header2->bplist_size > 0)
            {
                plist_from_bin(header+sizeof(InstrumentsResponse)+sizeof(RequestPacket),response_header2->bplist_size,&response_plist);
                if (response_plist!=NULL)
                {
                    //debug_plist(response_plist);
                    plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                    plist_t pid_plist = plist_array_get_item(response_object,1);
                    if (plist_get_node_type(pid_plist)==PLIST_STRING)
                    {
                        plist_get_string_val(pid_plist,&cmd_str);
                        fprintf(stderr,"[Instrument InstrumentsGetResponse] Command string received = %s\n", cmd_str);
                    }
                    plist_free(response_plist);
                }
            }
            
            //printf("before plist 2\n");
            
            if  (response->length > sizeof(RequestPacket)+response_header2->bplist_size+sizeof(BPLPacket))
            {
                memcpy(bpl_packet2,header+sizeof(InstrumentsResponse)+sizeof(RequestPacket)+response_header2->bplist_size,sizeof(BPLPacket));
                BPLPacket_from_LE(bpl_packet2);
                if (bpl_packet2->bplist_size > 0)
                {
                    plist_from_bin(header+sizeof(InstrumentsResponse)+sizeof(RequestPacket)+response_header2->bplist_size+sizeof(BPLPacket),bpl_packet2->bplist_size,&response_plist2);
                    if (response_plist2!=NULL)
                    {
                        //debug_plist(response_plist2);
                        plist_t response_object2 = plist_dict_get_item(response_plist2,"$objects");
                        uint32_t object2_size = plist_array_get_size(response_object2);
                        uint32_t i;
                        for (i = 1; i<object2_size; i++)
                        {
                            plist_t objectn = plist_array_get_item(response_object2,i);
                            if (plist_get_node_type(objectn)==PLIST_STRING)
                            {
                                char *response_string = NULL,*tmp_ptr = NULL;
                                plist_get_string_val(objectn,&response_string);
                                //printf("%s\n",response_string);
                                if(cmd_str != NULL)
                                {
                                    tmp_ptr = cmd_str;
                                }
                                else		
                                {
                                    fprintf(stderr, "[Instrument InstrumentsGetResponse] ###########Command string NULL##############\n");
                                }
                                cmd_str = (char *) malloc( (strlen(tmp_ptr)+strlen(response_string)+5) * sizeof(char));
                                if(cmd_str == NULL)
                                {
                                    fprintf(stderr, "[Instrument InstrumentsGetResponse] ###########Command string malloc failed##############\n");
                                    MemoryFree(&tmp_ptr);
                                    if (response_string) MemoryFree(&response_string);
                                    continue;
                                }
                                sprintf(cmd_str,"%s %s",tmp_ptr,response_string);
                                fprintf(stderr,"[Instrument InstrumentsGetResponse] Command string formed = %s\n", cmd_str);
                                MemoryFree(&tmp_ptr);
                                if (response_string) MemoryFree(&response_string);
                            }
                        }
                        plist_free(response_plist2);
                    }
                }
            }
            
            if  (response->length > sizeof(RequestPacket)+response_header2->bplist_size+(2*sizeof(BPLPacket))+bpl_packet2->bplist_size)
            {
                memcpy(bpl_packet3,header+sizeof(InstrumentsResponse)+sizeof(RequestPacket)+response_header2->bplist_size+sizeof(BPLPacket)+bpl_packet2->bplist_size,sizeof(BPLPacket));
                BPLPacket_from_LE(bpl_packet3);
                if (bpl_packet3->bplist_size > 0)
                {
                    plist_from_bin(header+sizeof(InstrumentsResponse)+sizeof(RequestPacket)+response_header2->bplist_size+(2*sizeof(BPLPacket))+bpl_packet2->bplist_size,bpl_packet3->bplist_size,&response_plist3);
                    if (response_plist3!=NULL)
                    {
                        //debug_plist(response_plist3);
                        plist_free(response_plist3);
                    }
                }
            }
            //printf("before last plist size \n");
            last_plist_offset = sizeof(RequestPacket)+response_header2->bplist_size+(2*sizeof(BPLPacket))+bpl_packet2->bplist_size+bpl_packet3->bplist_size;
            last_plist_size = response->length - last_plist_offset ;
            //printf("last plist size %d\n",last_plist_size);
            if  (last_plist_size > 0 )
            {
                plist_from_bin(header+sizeof(InstrumentsResponse)+last_plist_offset,last_plist_size,&response_plist4 );
                debug_buffer(header+sizeof(InstrumentsResponse)+last_plist_offset, last_plist_size);
                if (response_plist4!=NULL)
                {
                    //debug_plist(response_plist4);
                    plist_t response_object = plist_dict_get_item(response_plist4,"$objects");
                    plist_t pid_plist = plist_array_get_item(response_object,1);
                    if (plist_get_node_type(pid_plist)==PLIST_STRING)
                    {
                        char *response_string = NULL;
                        plist_get_string_val(pid_plist,&response_string);
                        if (strcmp(response_string,"performTaskOnHost:withArguments:timeout:")==0)	
                        {
                            //Implementation for fixing the bug # PC-500
                            if(addCommandResponseQFptr(cmd_str,reply_packet_num) < 0)
                            {
                                fprintf(stderr,"[Instrument InstrumentsGetResponse] commandResponseQ add failed\n");
                            }
                            MemoryFree(&cmd_str);
                            cmd_str = NULL;
                            fprintf(stderr,"[Instrument InstrumentsGetResponse] Command string freed\n");
                            
                            fprintf(stderr,"[Instrument InstrumentsGetResponse] reply_packet_num received: %d\n",reply_packet_num);
                        }
                        else
                        {
                            if(cmd_str) MemoryFree(&cmd_str);
                            cmd_str=NULL;
                        }
                        if (response_string) MemoryFree(&response_string);
                    }
                    plist_free(response_plist4);
                }
            }
        }
        else if (response->length>sizeof(RequestPacket) && response->msg_type ==0x2 && response->payload_offset > 0)
        {
            memcpy(response_header2,header+sizeof(InstrumentsResponse),sizeof(RequestPacket));
            RequestPacket_from_LE(response_header2);
            if (response_header2->bplist_size > 0)
            {
                plist_from_bin(header+sizeof(InstrumentsResponse)+sizeof(RequestPacket),response_header2->bplist_size,&response_plist);
                if (response_plist!=NULL)
                {
                    //debug_plist(response_plist);
                    /*
                     plist_t response_object = plist_dict_get_item(response_plist,"$objects");
                     plist_t pid_plist = plist_array_get_item(response_object,1);
                     if (plist_get_node_type(pid_plist)==PLIST_STRING)
                     {
                     plist_get_string_val(pid_plist,&cmd_str);
                     //printf("%s\n",cmd_str);
                     }
                     */
                    plist_free(response_plist);
                }
                
                //printf("before last plist size \n");
                last_plist_offset = sizeof(RequestPacket)+response_header2->bplist_size;
                last_plist_size = response->length - last_plist_offset ;
                //printf("last plist size %d\n",last_plist_size);
                if  (last_plist_size > 0 )
                {
                    plist_from_bin(header+sizeof(InstrumentsResponse)+last_plist_offset,last_plist_size,&response_plist4 );
                    debug_buffer(header+sizeof(InstrumentsResponse)+last_plist_offset, last_plist_size);
                    if (response_plist4!=NULL)
                    {
                        //debug_plist(response_plist4);
                        plist_t response_object = plist_dict_get_item(response_plist4,"$objects");
                        plist_t pid_plist = plist_array_get_item(response_object,1);
                        if (plist_get_node_type(pid_plist)==PLIST_STRING)
                        {
                            char *response_string;
                            plist_get_string_val(pid_plist,&response_string);
                            //printf("%s\n",response_string);
                            if (strcmp(response_string,"notifyMonitoredPidIsDead:")==0)
                            {
                                res = INSTRUMENTS_E_APP_DIED_ERROR;
                            }
                            if (response_string) MemoryFree(&response_string);
                        }
                        plist_free(response_plist4);
                    }
                }
            }
        }
    }
    
    if (header) MemoryFree(&header);
    if (response) MemoryFree(&response);
    if (response_header2) MemoryFree(&response_header2);
    if (bpl_packet2)	MemoryFree(&bpl_packet2);
    if (bpl_packet3)	MemoryFree(&bpl_packet3);
    return res;
}

////Implementation for fixing the bug # PC-500

void RegisterAddCommandResponseQ(fptrCommandResponseQ fp)
{
    addCommandResponseQFptr = fp;
}

void RegisterGetCommandResponseQ(fptrCommandResponseQ fp)
{
    getCommandResponseQFptr = fp;
}

void RegisterGetCommandResponseQLen(fptrCommandResponseQ fp)
{
    getCommandResponseQLenFptr = fp;
}

//Fix for the bug PC-456
void MemoryFree(void** ptr)
{
    if((char*)*ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}
