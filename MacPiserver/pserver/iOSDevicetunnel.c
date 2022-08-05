/*
 * $Id: simple-tcp-proxy.c,v 1.11 2006/08/03 20:30:48 wessels Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <stddef.h>

#include <string.h>
#include <signal.h>
#include <assert.h>
#include <syslog.h>
#include <err.h>


#include <sys/types.h>
#include <sys/select.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <sys/un.h>

#include <netinet/in.h>

#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>


#include <plist/plist.h>
#include <usbmuxd-proto.h>


#ifdef __APPLE__
#include <libkern/OSByteOrder.h>

#define be32toh(x) OSSwapBigToHostInt32(x)
#endif

static int verbose = 0;


#define BUF_SIZE 4096

#define USBMUXD_SOCKET_FILE "/var/run/usbmuxd"

int sock_in,sock_out;

char size_array_in[50] = {0};
char size_array_out[50] = {0};

char client_hostname[64];
char *udid = NULL;

	void
cleanup(int sig)
{
	//syslog(LOG_NOTICE, "Cleaning up...");
	exit(0);
}

	void
sigreap(int sig)
{
	int status;
	pid_t p;
	signal(SIGCHLD, sigreap);
	while ((p = waitpid(-1, &status, WNOHANG)) > 0);
	/* no debugging in signal handler! */
}

void set_nonblock(int fd)
{
	int fl;
	int x;
	fl = fcntl(fd, F_GETFL, 0);
	if (fl < 0) {
		//syslog(LOG_ERR, "fcntl F_GETFL: FD %d: %s", fd, strerror(errno));
		exit(1);
	}
	x = fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	if (x < 0) {
		//syslog(LOG_ERR, "fcntl F_SETFL: FD %d: %s", fd, strerror(errno));
		exit(1);
	}
}


int create_server_sock(char *addr, int port)
{
	int addrlen, s, on = 1, x;
	static struct sockaddr_in client_addr;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		err(1, "socket");

	addrlen = sizeof(client_addr);
	memset(&client_addr, '\0', addrlen);
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	client_addr.sin_port = htons(port);
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, 4);
	x = bind(s, (struct sockaddr *) &client_addr, addrlen);
	if (x < 0)
		err(1, "bind %s:%d", addr, port);

	x = listen(s, 5);
	if (x < 0)
		err(1, "listen %s:%d", addr, port);
	//syslog(LOG_NOTICE, "listening on %s port %d", addr, port);

	return s;
}


int socket_close(int s_close)
{
	close(s_close);
}
//unix socket connection fun

//int open_remote_host(char *host, int port)
int socket_connect_unix(const char *filename)
{
	struct sockaddr_un name;
	int sfd = -1;
	size_t size;
	struct stat fst;
#ifdef SO_NOSIGPIPE
	int yes = 1;
#endif

	// check if socket file exists...
	if (stat(filename, &fst) != 0) {
		if (verbose >= 2)
			fprintf(stderr, "%s: stat '%s': %s\n", __func__, filename,
					strerror(errno));
		return -1;
	}
	// ... and if it is a unix domain socket
	if (!S_ISSOCK(fst.st_mode)) {
		if (verbose >= 2)
			fprintf(stderr, "%s: File '%s' is not a socket!\n", __func__,
					filename);
		return -1;
	}
	// make a new socket
	if ((sfd = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		if (verbose >= 2)
			fprintf(stderr, "%s: socket: %s\n", __func__, strerror(errno));
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
		perror("setsockopt()");
		socket_close(sfd);
		return -1;
	}
#endif

	// and connect to 'filename'
	name.sun_family = AF_LOCAL;
	strncpy(name.sun_path, filename, sizeof(name.sun_path));
	name.sun_path[sizeof(name.sun_path) - 1] = 0;

	size = (offsetof(struct sockaddr_un, sun_path)
			+ strlen(name.sun_path) + 1);

	if (connect(sfd, (struct sockaddr *) &name, size) < 0) {
		socket_close(sfd);
		if (verbose >= 2)
			fprintf(stderr, "%s: connect: %s\n", __func__,
					strerror(errno));
		return -1;
	}
	//set_nonblock(sfd);

	return sfd;
}
int get_hinfo_from_sockaddr(struct sockaddr_in addr, int len, char *fqdn)
{
	struct hostent *hostinfo;

	hostinfo = gethostbyaddr((char *) &addr.sin_addr.s_addr, len, AF_INET);
	if (!hostinfo) {
		sprintf(fqdn, "%s", inet_ntoa(addr.sin_addr));
		return 0;
	}
	if (hostinfo && fqdn)
		sprintf(fqdn, "%s [%s]", hostinfo->h_name, inet_ntoa(addr.sin_addr));
	return 0;
}


int wait_for_connection(int s)
{


	static int newsock;
	static socklen_t len;
	static struct sockaddr_in peer;
	len = sizeof(struct sockaddr);
	//syslog(LOG_INFO, "calling accept FD %d", s);
	newsock = accept(s, (struct sockaddr *) &peer, &len);
	/* dump_sockaddr (peer, len); */
	if (newsock < 0) {
		if (errno != EINTR) {
			//syslog(LOG_NOTICE, "accept FD %d: %s", s, strerror(errno));
			return -1;
		}
	}
	get_hinfo_from_sockaddr(peer, len, client_hostname);
	set_nonblock(newsock);
	return (newsock);
}


struct usbmuxd_size
{
	uint32_t size;
}__attribute__((__packed__));


int read_from_usbmux(int fd, char *buf, int *len,int fd1)
{
	
	
	struct usbmuxd_header headre;
	char *usbmux_buffer = malloc(sizeof(struct usbmuxd_header));

	//	sprintf(size_array_in,"total data is : %d\n",*len);
	//	write(1,size_array_in,sizeof(size_array_in));
	char *bufferr;
	int bufferr_len,x_actual = 0;
	int remaining_len;
	int remain_length = 0;

	struct usbmuxd_header head2 ;
	char *ptr = NULL,*ptr1;
	char *size = malloc(sizeof(struct usbmuxd_size));
	uint32_t *num_size;


	uint32_t pktlen = 0;
	int counter = 0,flag = 0;

	char *message = NULL;
	char *payload = NULL;
	//write(1,buf,*len);
	int payload_size;
	int sent = 0,send_len;

	if(*len == 4)
	{

	//	sprintf(size_array_in,"4 data is : %d\n",*len);
	//	write(1,size_array_in,sizeof(size_array_in));
		//printf("length is : ----- %d \n",*len);
			ptr1 = (char*)buf;

		
			//write(1,buffer,recv_len);
			//printf("recv_len is : %d\n",recv_len);
	
			memcpy(size,ptr1,4);


			struct usbmuxd_size *head = (void*)size;
			int num_size1 = be32toh(head->size);
			// sprintf(size_array_in, "size is $$: %d\n",num_size1);
			// write(1,size_array_in,sizeof(size_array_in));
		//	printf("size is $$$$$%d\n",num_size1);



		int n  = write(fd,buf,*len);
		if (n <= 0) {
						//fprintf(stderr, "send failed: %s\n", strerror(errno));
						//break;
					} else {
						//fprintf(stderr, "only sent %d from %d bytes\n", n, *len);
					}
					//sprintf(size_array_in,"sent data 1 is : %d\n",n);
					//write(1,size_array_in,sizeof(size_array_in));
					*len-= n;
					if(*len == 0)	
						free(size);
						free(usbmux_buffer);
			
					return *len;

	}					
	else if(*len <= 20 && *len != 0)
	{
						
						//printf("length is : ******* %d \n",*len);



					//memcpy(size,ptr1,4);
					//struct usbmuxd_size *head = (void*)size;
					send_len = write(fd,buf,*len);
					//printf("send_len is : %d\n",send_len);


					if (send_len <= 0) {
						//fprintf(stderr, "send failed: %s\n", strerror(errno));
						//break;
					} else {
						//fprintf(stderr, "only sent %d from %d bytes\n", sent, *len);
					}
					*len -= send_len;
					if(*len == 0)	
						free(size);
						free(usbmux_buffer);
				
				//	sprintf(size_array_in,"sent data is  2 : %d\n",send_len);
				//	write(1,size_array_in,sizeof(size_array_in));
					return *len;

					//write(1,buf,*len);
					//printf("recv_len is : %d\n",*len);

	}
	#if 1
	else
	{
		

			plist_t dict = NULL;
			

			//printf("remain length is : %d\n",remain_length);
			ptr = (char*)buf;

			int new_size = atoi(size);
			memcpy(usbmux_buffer,ptr,sizeof(struct usbmuxd_header));
			struct usbmuxd_header *head = (void*)usbmux_buffer;


			payload = (char *)buf + sizeof(struct usbmuxd_header) + remain_length;
			payload_size = head->length;
			plist_from_xml(payload,payload_size,&dict);
			//write(1,payload,payload_size);
			payload = NULL;			
			

			plist_t response_object = plist_dict_get_item(dict,"DeviceList");

			if(response_object != NULL)
			{	
				char *m_buf ;

				x_actual -= head->length;
						
			//	printf("lngth is : %d && length is : %d\n",head->length,*len);


				int device_list = plist_array_get_size(response_object);
				plist_t response_object1 = NULL,response_object2 = NULL, response_object3 = NULL,response_object4 = NULL;
				plist_t dict_device = NULL,devices =NULL,m_type = NULL,props= NULL,dict_t = NULL;
				for(int j = 0 ; j < device_list ;j++)
				{

					
					 response_object1 = plist_array_get_item(response_object,j);
					plist_to_xml(response_object1,&bufferr,&bufferr_len);
											//write(1,bufferr,bufferr_len);

					 response_object2 = plist_dict_get_item(response_object1,"Properties");
					plist_to_xml(response_object2,&bufferr,&bufferr_len);
											//write(1,bufferr,bufferr_len);

					 response_object3 = plist_dict_get_item(response_object2,"SerialNumber");
					plist_to_xml(response_object3,&bufferr,&bufferr_len);
											//					write(1,bufferr,bufferr_len);

					plist_get_string_val(response_object3,&message);


					if(!strcmp(udid,message))
					{
						//printf("it's comparedf: %s\n",message);

						 dict_device = plist_new_dict();
						 devices = plist_new_array();

						plist_to_xml(response_object1,&bufferr,&bufferr_len);
						//write(1,bufferr,bufferr_len);

						  m_type = plist_dict_get_item(response_object1,"DeviceID");

						uint64_t d_id;
						plist_get_uint_val(m_type,&d_id);

						plist_dict_set_item(dict_device, "MessageType", plist_new_string("Attached"));
						plist_dict_set_item(dict_device, "DeviceID", plist_new_uint(d_id));
						 props = plist_new_dict();

						d_id = 0;
						 response_object4 = plist_dict_get_item(response_object2,"ConnectionSpeed");
						plist_get_uint_val(response_object4,&d_id);


						plist_dict_set_item(props, "ConnectionSpeed", plist_new_uint(d_id));
						plist_dict_set_item(props, "ConnectionType", plist_new_string("USB"));

						response_object4 = plist_dict_get_item(response_object2,"DeviceID");
						plist_get_uint_val(response_object4,&d_id);

						plist_dict_set_item(props, "DeviceID", plist_new_uint(d_id));
						
						response_object4 = plist_dict_get_item(response_object2,"LocationID");
						plist_get_uint_val(response_object4,&d_id);
					//	printf("LID -      %d----------\n,",d_id);

						plist_dict_set_item(props, "LocationID", plist_new_uint(d_id));

						response_object4 = plist_dict_get_item(response_object2,"ProductID");
						
						plist_get_uint_val(response_object4,&d_id);
					//	printf(" PID   %d----------\n,",d_id);
						
						plist_dict_set_item(props, "ProductID", plist_new_uint(d_id));
						plist_dict_set_item(props, "SerialNumber", plist_new_string(udid));
					
						plist_dict_set_item(dict_device, "Properties", props);
						plist_array_append_item(devices, dict_device);

						 dict_t = plist_new_dict();

						plist_dict_set_item(dict_t, "DeviceList", devices);

						plist_to_xml(dict_t,&bufferr,&bufferr_len);


						m_buf = malloc(sizeof(struct usbmuxd_header) + bufferr_len);
	
						
						head2.length = bufferr_len + sizeof(struct usbmuxd_header);
						head2.version = head->version;
						head2.message = head->message;
						head2.tag = head->tag;



						memcpy(m_buf,&head2,sizeof(struct usbmuxd_header));
						memcpy(m_buf+sizeof(struct usbmuxd_header),bufferr,bufferr_len);
						
						//plist_free(devices);
						//devices = NULL;
						//plist_free(dict_device);
						//dict_device = NULL;
						//plist_free(response_object4);
						//response_object4 = NULL;
						m_type = NULL;

						break;
						
					}
						//plist_free(response_object1);
						//response_object1 = NULL;
						//plist_free(response_object2);
						//response_object2 = NULL;

						//plist_free(response_object3);
						//response_object3 = NULL;
				

				}
					

					


				sent = write(fd,m_buf,sizeof(struct usbmuxd_header)+bufferr_len);
				//write(1,m_buf,sizeof(struct usbmuxd_header)+ bufferr_len);

					if (sent <= 0) {
						//fprintf(stderr, "send failed: %s\n", strerror(errno));
						//break;
					} else {
						//fprintf(stderr, "only sent %d from %d bytes\n", sent, *len);
					}
				//	sprintf(size_array_in,"sent data is 3 : %d\n",sent);
				//	write(1,size_array_in,sizeof(size_array_in));
					*len -= head->length;
					//free(size);
					//free(usbmux_buffer);
					free(m_buf);
					free(size);
					free(usbmux_buffer);
					plist_free(response_object1);
					response_object1 = NULL;
					plist_free(response_object2);
					response_object2 = NULL;

					plist_free(response_object3);
					response_object3 = NULL;
					plist_free(response_object4);
					response_object4 = NULL;
					plist_free(dict_device);
					dict_device = NULL;
					//plist_free(devices);
					devices = NULL;
					plist_free(dict_t);
					dict_t = NULL;
					plist_free(props);
					props = NULL;

					return 0;


			}
			else
			{
				plist_free(response_object);
				response_object = NULL;


				remain_length += head->length;


				plist_t response_object = plist_dict_get_item(dict,"MessageType");


				if(response_object != NULL)
				{

					plist_get_string_val(response_object,&message);


					if(!strcmp(message,"Attached"))
					{
						//printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$444\n");


//						sprintf(size_array_in," hello is : %d\n",head->length);
//						write(1,size_array_in,sizeof(size_array_in));
						//*len -= head->length;
						plist_t response_object1 = plist_dict_get_item(dict,"Properties");
						plist_t response_object2 = plist_dict_get_item(response_object1,"SerialNumber");
						plist_get_string_val(response_object2,&message);
						//printf("----%s\n",message);
						if(!strcmp(message,udid))
						{
									//sprintf(size_array_in,"comp data is : %d\n",*len);
									//write(1,size_array_in,sizeof(size_array_in));
							char *buffer_udid,*send_buf;
							int buffer_udid_len;
							plist_to_xml(dict,&buffer_udid,&buffer_udid_len);
							struct usbmuxd_header head2;
						
							head2.length = buffer_udid_len + sizeof(struct usbmuxd_header);
							head2.version = head->version;
							head2.message = head->message;
							head2.tag = head->tag;

							send_buf = malloc(sizeof(struct usbmuxd_header) + buffer_udid_len);
							memcpy(send_buf,&head2,sizeof(struct usbmuxd_header));
							memcpy(send_buf+sizeof(struct usbmuxd_header),buffer_udid,buffer_udid_len);
							sent = write(fd,send_buf,sizeof(struct usbmuxd_header)+ buffer_udid_len);
							//write(1,send_buf,sent);
							if (sent <= 0) {
												//fprintf(stderr, "send failed: %s\n", strerror(errno));
												//break;
											} else {
												//fprintf(stderr, "only sent %d from %d bytes\n", sent, *len);
											}

							//printf("buffer_udid_len is %d\n",buffer_udid_len);
							//write(1,send_buf,sent);
							//*len-=sent;
							//free(size);
							//free(usbmux_buffer);
							free(buffer_udid);
							free(send_buf);

							//sprintf(size_array_in,"moving is----  : %d\n",*len);
							//write(1,size_array_in,sizeof(size_array_in));
							memmove(buf, buf+*len,*len);
							*len-=*len;
							//sprintf(size_array_in,"left length is : %d\n",*len);
							//write(1,size_array_in,sizeof(size_array_in));

							if(*len == 0)	
								free(size);
								free(usbmux_buffer);
							return 0;

							//plist_free(response_object1);
							//plist_free(response_object2);

						}
						else
						{
							
							int x_actual_l=0;

							memmove(buf, buf+(head->length), (*len)-head->length);
							*len-=head->length;
							if(*len == 0)	
								free(size);
								free(usbmux_buffer);

							return *len;
				

							//plist_free(response_object);

							//continue;
							

						}


					}
					else if (!strcmp(message,"Result"))
					{
						sent = write(fd,buf,head->length);
						//printf("--------Data-------\n");
						//write(1,send_pack,head->length);

						if (sent <= 0) 
						{
								//fprintf(stderr, "send failed: %s\n", strerror(errno));
								//break;
						} 
						else 
						{
							//fprintf(stderr, "only sent %d from %d bytes\n", sent, *len);
						}
				//		write(1,send_pack,sent);

					if(sent < 0)
						return sent;
					if(sent == 0)
						return sent;
					if(sent != *len)
						memmove(buf, buf+sent, (*len)-sent);
					
					
						//printf("length is now : %d\n",*len);
						//memmove(buf, buf+sent, (*len)-sent);
						//x_actual = (*len)-sent;
					*len -= sent;
					if(*len ==0)
						free(size);
						free(usbmux_buffer);
						return *len;


					}
					else
					{
							sent = write(fd,buf,*len);
							if (sent <= 0) {
									//fprintf(stderr, "send failed: %s\n", strerror(errno));
									//break;
							} else {
									//fprintf(stderr, "only sent %d from %d bytes\n", sent, *len);
								}
						//sprintf(size_array_in,"sent data is 4 : %d\n",sent);
						//write(1,size_array_in,sizeof(size_array_in));
						if(sent < 0)
							return sent;
						if(sent == 0)
							return sent;
						if(sent != *len)
							memmove(buf, buf+sent, (*len)-sent);
					
					//	sprintf(size_array_in,"----- data is  : %d\n",sent);
				//		write(1,size_array_in,sizeof(size_array_in));
						*len-=sent;
						if(*len == 0)	
							free(size);
							free(usbmux_buffer);
					
						return *len;

					}



				}
				else
				{
					
					plist_free(dict);
						dict = NULL;
					plist_free(response_object);
					response_object = NULL;

					sent = write(fd,buf,*len);
					if (sent <= 0) {
						//fprintf(stderr, "send failed: %s\n", strerror(errno));
						//break;
					} else {
						//fprintf(stderr, "only sent %d from %d bytes\n", sent, *len);
					}
					if(sent < 0)
						return sent;
					if(sent == 0)
						return sent;
					if(sent != *len)
						memmove(buf, buf+sent, (*len)-sent);

					//sprintf(size_array_in,"data sent is : %d\n",sent);
					//write(1,size_array_in,sizeof(size_array_in));
					
					*len-=sent;	
					//free(size);
					//free(usbmux_buffer);
					if(*len == 0)	
						free(size);
						free(usbmux_buffer);

					
					return *len;
					//*len -= sent;

							
				}
				//plist_free(dict);
			}




	
	}	
	//sprintf(size_array_in,"data left is : %d\n",*len);
	//write(1,size_array_in,sizeof(size_array_in));

	//free(usbmux_buffer);
	//free(size);
#endif

}

int data_write(int fd,char *buf,int *len)
{
		int x = write(fd, buf, *len);

		sprintf(size_array_in,"actually data is : %d\n",*len);
		write(1,size_array_in,sizeof(size_array_in));

	if (x < 0)
		return x;
	if (x == 0)
		return x;
	if (x != *len)
	{
		memmove(buf, buf+x, (*len)-x);
	
		sprintf(size_array_in,"data left is : %d\n",x);
		write(1,size_array_in,sizeof(size_array_in));
	}
	*len -= x;

	
	return x;
}

int write_to_usbmux(int fd, char *buf, int *len)
{
	//printf("my write length is : %d\n",*len);

	   // sprintf(size_array_in, "size is : %d\n",*len);

	//write(sock_in,size_array_in,sizeof(size_array_in));	
	//write(sock_in,buf,*len);
	int x = write(fd, buf, *len);

	if (x < 0)
		return x;
	if (x == 0)
		return x;
	if (x != *len)
		memmove(buf, buf+x, (*len)-x);
	*len -= x;
	return x;
}

void service_client(int cfd, int sfd)
{
	int maxfd;
	char *sbuf;
	char *cbuf;
	int x, n;
	int cbo = 0;
	int sbo = 0;
	fd_set R;

	sbuf = malloc(BUF_SIZE);
	cbuf = malloc(BUF_SIZE);
	maxfd = cfd > sfd ? cfd : sfd;
	maxfd++;

	while (1) {
		struct timeval to;
		if (cbo) {
			if (write_to_usbmux(sfd, cbuf, &cbo) < 0 && errno != EWOULDBLOCK) {
				//syslog(LOG_ERR, "write %d: %s", sfd, strerror(errno));
				exit(1);
			}
		}
		if (sbo) {
			//sfd
			if (read_from_usbmux(cfd, sbuf, &sbo,sfd) < 0 && errno != EWOULDBLOCK) {
				//syslog(LOG_ERR, "write %d: %s", cfd, strerror(errno));
				exit(1);
			}
		}
		FD_ZERO(&R);
		if (cbo < BUF_SIZE)
			FD_SET(cfd, &R);
		if (sbo < BUF_SIZE)
			FD_SET(sfd, &R);
		to.tv_sec = 0;
		to.tv_usec = 1000;
		x = select(maxfd+1, &R, 0, 0, &to);
		if (x > 0) {
			if (FD_ISSET(cfd, &R)) {
				n = read(cfd, cbuf+cbo, BUF_SIZE-cbo);
				//write(1,cbuf+cbo,BUF_SIZE-cbo);
			//	sprintf(size_array_in,"xcode read is : %d\n",n);
			//	write(1,size_array_in,sizeof(size_array_in));
			//	syslog(LOG_INFO, "read %d bytes from CLIENT (%d)", n, cfd);
				if (n > 0) {
					cbo += n;
				} else {
					close(cfd);
					close(sfd);
					//syslog(LOG_INFO, "exiting");
					_exit(0);
				}
			}
			if (FD_ISSET(sfd, &R)) {
				n = read(sfd, sbuf+sbo, BUF_SIZE-sbo);
				//sprintf(size_array_in,"usbmuxd read is : %d\n",n);
				//write(1,size_array_in,sizeof(size_array_in));
				//printf("read data from pipe is : %d\n",n);
				//syslog(LOG_INFO, "read %d bytes from SERVER (%d)", n, sfd);
				if (n > 0) {
					sbo += n;
				} else {
					close(sfd);
					close(cfd);
					//syslog(LOG_INFO, "exiting");
					_exit(0);
				}
			}
		} else if (x < 0 && errno != EINTR) {
			//syslog(LOG_NOTICE, "select: %s", strerror(errno));
			close(sfd);
			close(cfd);
			//syslog(LOG_NOTICE, "exiting");
			_exit(0);
		}
	}
	free(sbuf);
	free(cbuf);
}


int main(int argc, char *argv[])
{
	char *localaddr = NULL;
	int localport = -1;
	char *remoteaddr = NULL;
	int remoteport = -1;
	int client = -1;
	int server = -1;
	int master_sock = -1;

	if (3 != argc) {
		fprintf(stderr, "usage: %s  lport device_id\n", argv[0]);
		exit(1);
	}

    //sock_in = open("/home/ketan/in_usbmuxd.log", O_RDONLY | O_CREAT | O_WRONLY,0777); 
    //sock_out = open("/home/ketan/out_usbmuxd.log", O_RDONLY | O_CREAT | O_WRONLY,0777); 


	//localaddr = strdup(argv[1]);
	localport = atoi(argv[1]);
	udid = argv[2];
	char ud_id[42] = {0};
	strcpy(ud_id,udid);
	assert(localport > 0);
	write(1,ud_id,sizeof(ud_id));
	//write(1,space,sizeof(space));
	char arr[50] = "Wait for connection";
//	openlog(argv[0], LOG_PID, LOG_LOCAL4);

	//signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, cleanup);
	signal(SIGCHLD, sigreap);

	master_sock = create_server_sock(localaddr, localport);
	for (;;) {
		//write(1,arr,50);

		
		if ((client = wait_for_connection(master_sock)) < 0)
			continue;
		if ((server = socket_connect_unix(USBMUXD_SOCKET_FILE)) < 0) {
			close(client);
			client = -1;
			continue;
		}
		if (0 == fork()) {
			/* child */
			//syslog(LOG_NOTICE, "connection from %s fd=%d", client_hostname, client);
			//syslog(LOG_INFO, "connected to %s:%d fd=%d", remoteaddr, remoteport, server);
			close(master_sock);
			service_client(client, server);
			abort();
		}
		close(client);
		client = -1;
		close(server);
		server = -1;
	}

}
