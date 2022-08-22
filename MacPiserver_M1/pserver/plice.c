
/**
 * pCloudy.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rfb/rfb.h>

#include <png.h>

#include <string.h>
#include <errno.h>
#include <time.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>

void print_usage(int argc, char **argv);

#define WIDTH  640
#define HEIGHT 960
//#define HEIGHT 1136
#define BPP      4

/* 15 frames per second (if we can) */
#define PICTURE_TIMEOUT (1.0/15.0)

int TimeToTakePicture() {
   return 1;
}

void abort_(const char * s, ...)
{
        va_list args;
        va_start(args, s);
        vfprintf(stderr, s, args);
        fprintf(stderr, "\n");
        va_end(args);
        //abort();
}

/*
 * simulate grabbing a picture from some device
 */
int TakePicture(unsigned char *buffer, screenshotr_client_t shotr)
{
	int abort=0;
	static int last_line=0, fps=0, fcount=0;
	int line=0;
	int i,j;
	struct timeval now;

  	int x, y;
  	int width, height;
  	png_byte color_type;
  	png_byte bit_depth;

  	png_structp png_ptr;
  	png_infop info_ptr;
  	int number_of_passes;
  	png_bytep * row_pointers;

  	char header[8];    // 8 is the maximum size that can be checked

        int result = -1;
  	char *imgdata = NULL;
  	uint64_t imgsize = 0;
	width = WIDTH;
	height = HEIGHT; // TBD read from buffer

        if (screenshotr_take_screenshot(shotr, &imgdata, &imgsize) == SCREENSHOTR_E_SUCCESS) {
    		printf("copying...");
	        for (y=0; y<height; y++) {
			for (x=0; x<width; x++) {
				char *ptr = &imgdata[0]+8+(y*width+x)*BPP;
				//printf("Pixel at position [ %d - %d ] has RGBA values: %d - %d - %d - %d\n",
				 //     x, y, ptr[0], ptr[1], ptr[2], ptr[3]);

				buffer[(y*width+x)*BPP+0]=ptr[0];
				buffer[(y*width+x)*BPP+1]=ptr[1];
				buffer[(y*width+x)*BPP+2]=ptr[2];
                	}
		}
    		printf("done\n");
	} else {
		printf("Could not get screenshot!\n");
	}


  /* frames per second (informational only) */
  fcount++;
  if (last_line > line) {
    fps = fcount;
    fcount = 0;
  }
  last_line = line;
  fprintf(stderr,"%03d/%03d Picture (%03d fps)\r", line, HEIGHT, fps);

  /* success!   We have a new picture! */
  return (1==1);
}


void on_mouse_event(int buttonMask,int x,int y,rfbClientPtr cl)
{
	static int leftClicked=0, down_x,down_y;
	if (buttonMask & 1)	
	{
		if (!leftClicked)
		{
			leftClicked=1;
			down_x=x;
			down_y=y;
		}
	}
	else if (leftClicked) // left button released
	{
		leftClicked=0; 
		if ((x >= down_x-10) && (x<= down_x+10) && (y >= down_y-10) && (y<= down_y+10))
		{
		
    			FILE *fe = fopen("/Users/jaffer/automation/workspace/device01/events", "w");
			if (fe)
			{
    				printf("tap\n");
				fprintf(fe,"T %d %d \r\n",x/2,y/2);
				fflush(fe);
				fclose(fe);
			}
		}
		else
		{
			FILE *fe = fopen("/Users/jaffer/automation/workspace/device01/events", "w");
			if (fe)
			{
				fprintf(fe,"S %d %d %d %d \r\n",down_x/2,down_y/2,x/2,y/2);
				fflush(fe);
				fclose(fe);
			}

		}
	}
}

/* 
 * Single-threaded application that interleaves client servicing with taking
 * pictures from the camera.  This way, we do not update the framebuffer
 * while an encoding is working on it too (banding, and image artifacts).
 */
int main(int argc,char** argv)
{                                       
  	long usec;

        idevice_t device = NULL;
        lockdownd_client_t lckd = NULL;
        screenshotr_client_t shotr = NULL;
        lockdownd_service_descriptor_t service = NULL;
        int result = -1;
        int i;
        const char *udid = NULL;
	char *filename = NULL;

	for (i = 1; i < argc; i++) {
                if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
                        idevice_set_debug_level(1);
                        continue;
                }
                else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--udid")) {
                        i++;
                        if (!argv[i] || (strlen(argv[i]) != 40)) {
                                print_usage(argc, argv);
                                return 0;
                        }
                        udid = argv[i];
                        continue;
                }
                else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                        print_usage(argc, argv);
                        return 0;
                }
                else if (argv[i][0] != '-' && !filename) {
                        filename = strdup(argv[i]);
                        continue;
                }
		else {
                        print_usage(argc, argv);
                        return 0;
                }
        }
        if (IDEVICE_E_SUCCESS != idevice_new(&device, udid)) {
                if (udid) {
                        printf("No device found with udid %s, is it plugged in?\n", udid);
                } else {
                        printf("No device found, is it plugged in?\n");
                }
                return -1;
        }

	if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(device, &lckd, NULL)) {
                idevice_free(device);
                printf("Exiting.\n");
                return -1;
        }

        lockdownd_start_service(lckd, "com.apple.mobile.screenshotr", &service);
        lockdownd_client_free(lckd);

         if (service && service->port > 0) {
                if (screenshotr_client_new(device, service, &shotr) != SCREENSHOTR_E_SUCCESS) {
                        printf("Could not connect to screenshotr!\n");
                } 
        } else {
                printf("Could not start screenshotr service! Remember that you have to mount the Developer disk image on your device if you want to use the screenshotr service.\n");
        }

  rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,WIDTH,HEIGHT,8,3,BPP);
  if(!server)
    return 0;
  server->desktopName = "pCloudy";
  server->frameBuffer=(char*)malloc(WIDTH*HEIGHT*BPP);
  server->alwaysShared=(1==1);
  server->cursor = NULL;
  server->port = 5910;
  server->ipv6port = 5910;
  server->ptrAddEvent = on_mouse_event;
  server->handleEventsEagerly = TRUE;
   printf("def time %d\n",server->deferUpdateTime);

  /* Initialize the server */
  rfbInitServer(server);           

  /* Loop, processing clients and taking pictures */
  while (rfbIsActive(server)) {
    if (TimeToTakePicture())
      if (TakePicture((unsigned char *)server->frameBuffer,shotr))
	{
        rfbMarkRectAsModified(server,0,0,WIDTH,HEIGHT);
	}
	          
    usec = server->deferUpdateTime*1;
    rfbProcessEvents(server,usec);
  }


  screenshotr_client_free(shotr);

  if (service)
 	lockdownd_service_descriptor_free(service);

  idevice_free(device);
  free(filename);

  return(0);
}

void print_usage(int argc, char **argv)
{
        char *name = NULL;

        name = strrchr(argv[0], '/');
        printf("Usage: %s [OPTIONS] [FILE]\n", (name ? name + 1: argv[0]));
        printf("Gets a screenshot from a device.\n");
        printf("The screenshot is saved as a TIFF image with the given FILE name,\n");
        printf("where the default name is \"screenshot-DATE.tiff\", e.g.:\n");
        printf("   ./screenshot-2013-12-31-23-59-59.tiff\n\n");
        printf("NOTE: A mounted developer disk image is required on the device, otherwise\n");
        printf("the screenshotr service is not available.\n\n");
        printf("  -d, --debug\t\tenable communication debugging\n");
        printf("  -u, --udid UDID\ttarget specific device by its 40-digit device UDID\n");
        printf("  -h, --help\t\tprints usage information\n");
        printf("\n");
}
