#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <png.h>
#include <rfb/rfb.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>
#include "b64.h"

#include <json.h>
#include "curlApi.h"

char getXCUIScreenShot(unsigned char** imgData, uint64_t* imageSize);

#define BPP	4
extern struct curl_fetch_st *p;
char b_id[50] = "com.pcloudy.ialice";
int flag = 0;
char *sourcePayload = NULL;
//char *p = NULL;
static sig_atomic_t signaled = 0;
static int no_clients = 0;
int please_exit=0;
int scaling = 100;
char xPort[8] ; /*Default port for XCUITTEST webserver*/

typedef struct {
    char *buffer;
    int offset;
} pImgBuffer_t;

static void on_key_event(rfbBool down,rfbKeySym key,rfbClientPtr cli);
static void on_mouse_event(int buttonMask,int x,int y,rfbClientPtr cli);
static void print_usage(int argc, char **argv);

int getScreenDimension(screenshotr_client_t shotr,int *width, int *height);

int captureScreen(unsigned char *buffer, screenshotr_client_t shotr, int*min_x, int*min_y, int*max_x, int*max_y, void*);
#ifdef IOS_9_0
void read_png_buffer(png_structp png_ptr,png_bytep data, png_size_t length);
#endif


char getXCUIScreenShot(unsigned char** imgData, uint64_t* imageSize)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"%s:%s/screenshot","http://localhost",xPort);
    struct curl_fetch_st chunk;
    if(!getHttpData(httpEndPointUrl,&chunk,2L))
    {
        if(p != NULL)
        {
            if(p->size > 0)
            {
                if(sourcePayload!=NULL)
                {
                    free(sourcePayload);
                    sourcePayload = NULL;
                }
                sourcePayload = (char *)malloc(p->size);
                strcpy(sourcePayload,p->payload);
                //printf("sourcePayload: \n%s\n",sourcePayload);
            }
            //printf("source payload size = %d\n",p->size);
            if(strlen(sourcePayload)>0)
            {
                json_object *new_obj;
                                new_obj = json_tokener_parse(sourcePayload);
                if(new_obj != NULL)
                {
                    //printf("value: %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
                    sprintf(sourcePayload,"%s",json_object_get_string(json_object_object_get(new_obj, "value")));
                    //printf("sourcePayload: \n%s\n",sourcePayload);
                    //printf("sourcePayload size = %d\n",strlen(sourcePayload));
                    unsigned char *dec = (unsigned char *)malloc(strlen(sourcePayload));
                    size_t dec_size = 0;

                     b64_decode_ex(sourcePayload, strlen(sourcePayload),&dec_size,dec);

                    *imgData = dec;
                    *imageSize = dec_size;
                    json_object_put(new_obj);

                }
                free(sourcePayload);
                sourcePayload = NULL;
                return 0;
            }
            return 0;
        }
        return 0;
    }
    return 1;

}

void intHandler(int param)
{
	signaled = 1;
}

void on_key_event(rfbBool down,rfbKeySym key,rfbClientPtr cli)
{
	fprintf(stderr,"Key event received\n");
	return;
}

void on_mouse_event(int buttonMask,int x,int y,rfbClientPtr cli)
{
	return;
}

ClientGoneHookPtr clientGoneHook(rfbClientPtr cli)
{
	no_clients--;
	fprintf(stderr, "Client gone ##################\n");
	return 0;
}

rfbNewClientHookPtr clientNewHook(rfbClientPtr cli)
{
	fprintf(stderr, "Client Hooked #################\n");
	if (scaling!=100)
	{
		rfbScalingSetup(cli, cli->screen->width*scaling/100, cli->screen->height*scaling/100);
	}
	no_clients++;
	return (rfbNewClientHookPtr) RFB_CLIENT_ACCEPT;
}

int main(int argc,char** argv)
{
	int vnc_port=5900;
	int i=0;
	int width, height, tmp;
	const char *udid = NULL;
	idevice_t device = NULL;
	lockdownd_client_t lckd = NULL;
    lockdownd_service_descriptor_t service = NULL;	
	screenshotr_client_t shotr = NULL;
	rfbScreenInfoPtr server = NULL;

	signal(SIGINT, intHandler);
	signal(SIGKILL, intHandler);
	signal(SIGHUP, intHandler);
	signal(SIGILL, intHandler);

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug"))
		{
			idevice_set_debug_level(1);
			continue;
		}
		else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--udid"))
		{
			i++;
			if (!argv[i] || (strlen(argv[i]) <= 0))
			{
				print_usage(argc, argv);
				return 0;
			}
			udid = argv[i];
			continue;
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
		{
			print_usage(argc, argv);
			return 0;
		}
		else if (!strcmp(argv[i], "-P") || !strcmp(argv[i], "--port"))
		{
			i++;
			vnc_port=atoi(argv[i]);
			continue;
		}
		else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--scale"))
		{
			i++;
			scaling=atoi(argv[i]);
			continue;
		}else if(!strcmp(argv[i], "-x") || !strcmp(argv[i],"--xport"))
        	{
        	    i++;
            	strcpy(xPort,argv[i]);
            	continue;
        	}
		else if(!strcmp(argv[i], "-w") || !strcmp(argv[i],"--width"))
                {   
                    i++;
		width = atoi(argv[i]);
		printf("width = %d\n", width);
                continue;
                }else if(!strcmp(argv[i], "-h") || !strcmp(argv[i],"--height"))
                {
                    i++;
		height = atoi(argv[i]);
		printf("Height = %d\n", height);
                continue;
                }else
		{
			print_usage(argc, argv);
			return 0;
		}
	}

	if (IDEVICE_E_SUCCESS != idevice_new(&device, udid))
	{
		if (udid) 
		{
			fprintf(stderr,"No device found with udid %s, is it plugged in?\n", udid);
		}
		else
		{
			fprintf(stderr,"No device found, is it plugged in?\n");
		}

		please_exit=1;
	}

	if (!please_exit)
	{
		if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(device, &lckd, NULL))
		{
			fprintf(stderr,"Exiting.\n");
			please_exit=1;
		}
    }

	if (!please_exit)
    {
		lockdownd_start_service(lckd, "com.apple.mobile.screenshotr", &service);
		if (service)
		{
			lockdownd_client_free(lckd);
		}

		if (service && service->port > 0)
		{
			if (screenshotr_client_new(device, service, &shotr) != SCREENSHOTR_E_SUCCESS) 
			{
				fprintf(stderr,"Could not connect to screenshotr!\n");
				please_exit=1;
			}
		} 
		else 
		{
			fprintf(stderr,"Could not start screenshotr service! Remember that you have to mount the Developer disk image on your device if you want to use the screenshotr service.\n");
			please_exit=1;
		}
	}

	if (!please_exit)
	{

		while(flag == 0){
                getScreenDimension(shotr,&width, &height);
                if(width > 0 || height > 0){
                        flag = 1;
                }else{
                        sleep(3);
               	 }
       		 }

		if (width>height)
		{
			tmp=width;
			width=height;
			height=tmp;
		}

		server=rfbGetScreen(&argc,argv,width,height,8,3,BPP);
		if(!server)
		{
			please_exit=1;
		}
	}

	if (!please_exit)
	{
  		server->desktopName = "pCloudy";
  		server->frameBuffer=(char*)malloc(width*height*BPP);
  		server->alwaysShared=(1==1);
  		server->cursor = NULL;
		server->httpDir=NULL;
		server->listenInterface=htonl(INADDR_LOOPBACK);
  		server->port = vnc_port;
		server->listen6Interface="::1";
  		server->ipv6port = vnc_port;
  		server->ptrAddEvent = on_mouse_event;
  		server->kbdAddEvent = on_key_event;
		server->newClientHook = (rfbNewClientHookPtr)clientNewHook;
  		server->handleEventsEagerly = TRUE;
		server->screenData = NULL;

  		// Initialize the server
  		rfbInitServer(server);           

		if (server->listenSock==-1)
		{
			please_exit=1;
		}
	}

	while (!please_exit && !signaled && rfbIsActive(server)) 
	{

		int min_x=width,min_y=height,max_x=-1,max_y=-1;
		if (captureScreen((unsigned char *)server->frameBuffer,shotr,&min_x,&min_y,&max_x,&max_y,NULL))
		{
				min_x--;min_y--;max_x++;max_y++;
				if ((max_x>=min_x)&&(max_y>=min_y))
				{
					rfbMarkRectAsModified(server,min_x,min_y,max_x,max_y);
				}
		}
		rfbCheckFds(server,1000);
		if (no_clients > 0)
		{
			rfbProcessEvents(server,0);
		}
	}

	if(server)
	{
		if(server->frameBuffer)
		{
			free(server->frameBuffer);
			server->frameBuffer = NULL;
		}
		rfbScreenCleanup(server);
	}

	if (shotr) 
		screenshotr_client_free(shotr);
  	if (service)
 		lockdownd_service_descriptor_free(service);
	if (device) 
		idevice_free(device);

	if(please_exit)
	{
		fprintf(stderr,"please_exit is set, Exiting\n");
	}
	else if(signaled)
	{
		fprintf(stderr,"signaled, Exiting\n");
	}

	return 0;
}


#ifdef IOS_9_0
void read_png_buffer(png_structp png_ptr,
   png_bytep data, png_size_t length)
{
    int i;
    pImgBuffer_t *imgBuffer = (pImgBuffer_t *) png_get_io_ptr(png_ptr);
    memcpy(data,imgBuffer->buffer+imgBuffer->offset,length);
    imgBuffer->offset+=length;
}
#endif

int getScreenDimension(screenshotr_client_t shotr,int *width, int *height)
{
  	unsigned char *imgdata = NULL;
  	uint64_t imgsize = 0;
  	uint32_t fbsize = 0;
	uint32_t offset = 0;
	int no_records = 0;
	uint16_t id,type;
	uint32_t count,value;
	int i;

    if ( screenshotr_take_screenshot(shotr,&imgdata, &imgsize) == SCREENSHOTR_E_SUCCESS || (getXCUIScreenShot(&imgdata, &imgsize) == 0))
	{
#ifndef IOS_9_0
		uint32_t *imgdata32= (uint32_t) imgdata;
		offset = __builtin_bswap32(imgdata32[1]);
    	no_records = imgdata[offset]<<8|imgdata[offset+1];

		for (i=0;i<no_records;i++)
		{
			id = imgdata[offset+2+i*12]<<8|imgdata[offset+3+i*12];
			type = imgdata[offset+4+i*12]<<8|imgdata[offset+5+i*12];
			count = imgdata[offset+8+i*12]<<24|imgdata[offset+9+i*12]<<16|imgdata[offset+6+i*12]<<8|imgdata[offset+7+i*12];
			value = imgdata[offset+12+i*12]<<24|imgdata[offset+13+i*12]<<16|imgdata[offset+10+i*12]<<8|imgdata[offset+11+i*12];
			if (id==256)
			{
				*width=value;
			}
			else if(id==257)
			{
				*height=value;
			}
		}
#else
        png_structp png_ptr;
        png_infop info_ptr;
        unsigned int sig_read = 0;
        int color_type, interlace_type;

        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                     NULL, NULL, NULL);

        if (png_ptr == NULL) {
            return -1;
        }

        info_ptr = png_create_info_struct(png_ptr);
        if (info_ptr == NULL) {
            png_destroy_read_struct(&png_ptr, NULL, NULL);
            return -1;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            return -1;
        }

        pImgBuffer_t imgBuffer;
        imgBuffer.buffer = imgdata;
        imgBuffer.offset = 0;

        png_set_read_fn(png_ptr, (void *)&imgBuffer, read_png_buffer);

        png_set_sig_bytes(png_ptr, sig_read);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
       // png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

        int i,bit_depth;
        png_get_IHDR(png_ptr, info_ptr, width, height, &bit_depth, &color_type,
                 &interlace_type, NULL, NULL);

        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#endif
		free(imgdata);
	} 
	else
	{
		fprintf(stderr,"Could not get screenshot!\n");
	}

	return (1==1);
}

int captureScreen(unsigned char *buffer, screenshotr_client_t shotr, int*min_x, int*min_y, int*max_x, int*max_y, void* state )
{
	struct timeval now;

  	int width, height;

  	unsigned char *imgdata = NULL;
  	uint64_t imgsize = 0;
  	uint32_t fbsize = 0;
	uint32_t offset = 0;
	int no_records = 0;
	uint16_t id,type;
	uint32_t count,value;
	int i;
	static int scr_fail_count=0;

    if (screenshotr_take_screenshot(shotr, &imgdata, &imgsize) == SCREENSHOTR_E_SUCCESS || (getXCUIScreenShot(&imgdata, &imgsize) == 0)) 
	{
		if(imgsize > 0)
		{

#ifndef IOS_9_0
			uint32_t *imgdata32= (uint32_t*) imgdata;
			uint32_t *buffer32 = (uint32_t*) buffer;

			fbsize = __builtin_bswap32(imgdata32[1])-8;
			offset = __builtin_bswap32(imgdata32[1]);

			no_records = imgdata[offset]<<8|imgdata[offset+1];
			for (i=0;i<no_records;i++)
			{
				id = imgdata[offset+2+i*12]<<8|imgdata[offset+3+i*12];
				type = imgdata[offset+4+i*12]<<8|imgdata[offset+5+i*12];
				count = imgdata[offset+8+i*12]<<24|imgdata[offset+9+i*12]<<16|imgdata[offset+6+i*12]<<8|imgdata[offset+7+i*12];
				value = imgdata[offset+12+i*12]<<24|imgdata[offset+13+i*12]<<16|imgdata[offset+10+i*12]<<8|imgdata[offset+11+i*12];
				if (id==256)
				{
					width=value;
				}
				else if(id==257)
				{
					height=value;
				}
			}

			if (width<height)
			{
				int x,y;
				for (y=0; y<height; y++) {
					for (x=0; x<width; x++) {
						if (buffer32[y*width+x]!=imgdata32[y*width+x+2])
						{
							buffer32[y*width+x] = imgdata32[y*width+x+2];
							if (x>*max_x) *max_x=x;
							if (x<*min_x) *min_x=x;
							if (y>*max_y) *max_y=y;
							if (y<*min_y) *min_y=y;
						}
		            }
				}
	
			}
			else
			{
				int x,y;
				for (y=0; y<height; y++) {
					for (x=0; x<width; x++) {
						if ( buffer32[x*height+y] != imgdata32[(height-y)*width+x+2])
						{
							buffer32[x*height+y] = imgdata32[(height-y)*width+x+2];
							if (y>*max_x) *max_x=y;
							if (y<*min_x) *min_x=y;
							if (x>*max_y) *max_y=x;
							if (x<*min_y) *min_y=x;
						}
		         	}
				}
			}
#else

		    png_structp png_ptr;
		    png_infop info_ptr;
		    unsigned int sig_read = 0;
		    int color_type, interlace_type;

		    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		                                 NULL, NULL, NULL);

		    if (png_ptr == NULL) 
			{
		        return -1;
		    }

		    info_ptr = png_create_info_struct(png_ptr);
		    if (info_ptr == NULL)
			{
		        png_destroy_read_struct(&png_ptr, NULL, NULL);
		        return -1;
		    }

		    if (setjmp(png_jmpbuf(png_ptr))) 
			{
		        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		        return -1;
		    }

		    pImgBuffer_t imgBuffer;
		    imgBuffer.buffer = imgdata;
		    imgBuffer.offset = 0;

		    //png_init_io(png_ptr, fp);
		    png_set_read_fn(png_ptr, (void *)&imgBuffer, read_png_buffer);

		    png_set_sig_bytes(png_ptr, sig_read);

		    //png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
		png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
			//png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

		    png_uint_32 width, height;
		    int bit_depth;
		    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,&interlace_type, NULL, NULL);

		    //unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);

		    //fprintf(stderr,"width %d,height %d, bit_depth %d, color_type %d, interlace_type %d\n",width,height,bit_depth,color_type,interlace_type);

		    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

		    uint32_t *buffer32 = (uint32_t*) buffer;
		    int x,y;

		    for (i = 0; i < height; i++)
			{
		        for (x = 0; x < width; x++)
		        {
		            if ((buffer[i*width*4+x*4]!=row_pointers[i][3*x])
		                || (buffer[i*width*4+x*4+1]!=row_pointers[i][3*x+1])
		                || (buffer[i*width*4+x*4+2]!=row_pointers[i][3*x+2]))
		            {
		                buffer[i*width*4+x*4]=row_pointers[i][3*x];
		                buffer[i*width*4+x*4+1]=row_pointers[i][3*x+1];
		                buffer[i*width*4+x*4+2]=row_pointers[i][3*x+2];
		                buffer[i*width*4+x*4+3]=0;
		                if (x>*max_x) *max_x=x;
		                if (x<*min_x) *min_x=x;
		                if (i>*max_y) *max_y=i;
		                if (i<*min_y) *min_y=i;
		            }
		        }
		    }

		    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#endif
			free(imgdata);
			scr_fail_count=0;

		}
	}
	else
	{
		scr_fail_count++;
		if (scr_fail_count > 3)
		{
			fprintf(stderr,"Could not get screenshot!\n");	
			please_exit = 1;
			scr_fail_count = 0;
			return 0;
		}
	}
	return 1;
}

void print_usage(int argc, char **argv)
{
        char *name = NULL;

        name = strrchr(argv[0], '/');
        fprintf(stderr,"Usage: %s [OPTIONS]\n", (name ? name + 1: argv[0]));
        fprintf(stderr,"Start pserver\n");
        fprintf(stderr,"NOTE: A mounted developer disk image is required on the device\n");
        fprintf(stderr,"  -P, --port\t\tPort on which to start pserver\n");
        fprintf(stderr,"-s, --scale\t\tScale 50 for half width and height \n");
        fprintf(stderr,"  -u, --udid UDID\ttarget specific device by its 40-digit device UDID\n");
        fprintf(stderr,"  -h, --help\t\tprints usage information\n");
        fprintf(stderr,"\n");
}
