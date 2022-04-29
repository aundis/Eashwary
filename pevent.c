
/**
 * pCloudy.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <png.h>
#include <rfb/rfb.h>
#include <sys/ioctl.h>
#include "pqueue.h"
#include "b64.h"
#include <string.h>
#include <errno.h>
#include <time.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/instruments.h>
#include <thread.h>
#include <json.h> 
#include "pevent.h"
#include "curlApi.h"


#include <sys/file.h>

void print_usage(int argc, char **argv);
void* ProcessCheckFdsHandler(void*);
/*For XCUITEST Support*/
void* ProcessEventsThread(void*);
void* XCUIPopHandleThread(void* ptr);
void processEvents(char *msg, char *sessionId);
char getXCTestSession(char *session);
char getWindowSize(char *session);
char getCurrentApp(char **currentApp);
void terminateIfSettingsIsOpen (int signum);
char getCurrentScaleFactor();
char setScaleFactor(char scaleFactor);
char isAlertPresent(char *session);
char isApp(char *AppName,char *session,int x, int y);
char ifFilteredApp(char *session);
char getXCUISource(char *session);
void getElementLabelFromSource(char *session, int x, int y);
void getElementIDFromLabel(char *session, char *labelData, char *elementId);
char getXCUIScreenShot(unsigned char** imgData, uint64_t* imageSize);
char getImageDumpPath();
char *sourcePayload = NULL;
char sessionId[64] = {0};
char imgPath[1024] = {0};
char isAlert = 0;
char isFilteredApp = NO;
char currentScaleFactor = 0;
char blockSettings = 1;
char *bundleId;
extern struct curl_fetch_st *p;
extern char isXCUILabelFound;
extern char XCUILabel[1024];
extern char processing;
static mutex_t XCUImutex;
#define BPP      4
#define MAX_FDS  2
    int eventcount = 1;
    int height = 0 ;
    int width = 0;
    int UIorientation = 0;
    int scrollFlag = 0;
    int Width = 0;
    int Height = 0;

int flag = 0; //Flag for getting screen Diamentions

/* 15 frames per second (if we can) */
#define PICTURE_TIMEOUT (1.0/15.0)

sig_atomic_t signaled = 0;
pthread_t checkFdThread = NULL;
pthread_t eventFdThread = NULL; /*XCUITEST command processing thread id*/
pthread_t XCUIPopThread = NULL;
pServerState_t pserver_state;

char b_id[50] = "com.pcloudy.ialice";
char helperb_id[50] = "com.pcloudy.pcloudyhelper";

char xPort[8] = "8100"; /*Default port for XCUITTEST webserver*/
char iPort[8] = "6010"; /*Default port for XCUITTEST webserver*/
char deviceServer[32] = {0};
int fd_lock = 0;



void intHandler(int param) {
    signaled = 1;
}

ClientGoneHookPtr clientGoneHook(rfbClientPtr cli)
{
    pServerState_t *pserver_state=cli->screen->screenData;
    if(cli->clientData)
    {
        free(cli->clientData);
    }
    pserver_state->no_clients--;
    fprintf(stderr, "Client gone\n");
    return 0;
}


////////////////////////////////////////////////////////////




rfbNewClientHookPtr clientNewHook(rfbClientPtr cli)
{
    pServerState_t *pserver_state=cli->screen->screenData;
    if (pserver_state->scaling!=100)
    {
        printf("width %d height %d\n",cli->screen->width*pserver_state->scaling/100,cli->screen->height*pserver_state->scaling/100);
        //rfbScalingSetup(cli, 320, 480 );
        width = cli->screen->width*pserver_state->scaling/100;
        Width = width * 2;
        height = cli->screen->height*pserver_state->scaling/100;
        Height = height * 2;

        rfbScalingSetup(cli, cli->screen->width*pserver_state->scaling/100, cli->screen->height*pserver_state->scaling/100);
    }
   	// printf("scalinng %d\n",pserver_state->scaling);

    pClientState_t *cliState = (pClientState_t*) malloc(sizeof(pClientState_t));
    if (cliState)
    {
        cliState->leftClicked=0;
        cliState->rightClicked=0;
        cliState->down_x=0;
        cliState->down_y=0;
        cliState->down_time=time(NULL);
        cli->clientData = (void *) cliState;
        cli->clientGoneHook = (ClientGoneHookPtr) clientGoneHook;
        pserver_state->no_clients++;
        return (rfbNewClientHookPtr) RFB_CLIENT_ACCEPT;
    }
    else
    {
	thread_queue_add(pserver_state->queue,"P",1);
        return (rfbNewClientHookPtr) RFB_CLIENT_REFUSE;
    }
}

static void fetch_events_callback(char **std_out, void*user_data, int please_exit)
{
    struct threadmsg msg;
    static int screenReady = 1;

    pServerState_t *pserver_state=user_data;
    if (please_exit==1)
    {
        //printf("received exit, signaling the main thread to exit \n");
        if ( (pserver_state) && (pserver_state->input_enabled) )
        {
            pserver_state->please_exit=1;
        }
        *std_out=NULL;
        return;
    }

    if (pserver_state)
    {
        struct timespec timeout;

        timeout.tv_sec = 2;
        timeout.tv_nsec = 0;
        //timeout.tv_nsec = 0;

        if(pserver_state->processScreen == 0 && screenReady == 1)
        {
            screenReady = 0;
        }
        else if(pserver_state->processScreen == 0 && screenReady == 0)
        {
            pserver_state->processScreen = 1;
            screenReady = 1;
        }

        if (thread_queue_get(pserver_state->queue,&timeout,&msg)==0)
        {
            *std_out = (char*)msg.data;
        }
        else
        {
            *std_out=NULL;
        }
    }
    else
    {
        *std_out=NULL;
    }
}


instruments_error_t start_input_script(pServerState_t *pserver_state, idevice_t device)
{
	signal(SIGINT, intHandler);
	signal(SIGKILL, intHandler);
	signal(SIGHUP, intHandler);
	signal(SIGILL, intHandler);
  	long usec;
	instruments_error_t ierr = INSTRUMENTS_E_UNKNOWN_ERROR;
        ierr = instruments_send_channel_request(pserver_state->instr, "com.apple.instruments.server.services.processcontrol.posixspawn",6);

        if (ierr != INSTRUMENTS_E_SUCCESS)
        {
            printf("ERROR: Could not start sub service processcontrol.posixspawn.\n");
            pserver_state->please_exit=1;
        }

        /*ierr = instruments_send_channel_request(pserver_state->instr, "com.apple.instruments.server.services.mobilenotifications",7);

        if (ierr != INSTRUMENTS_E_SUCCESS)
        {
            printf("ERROR: Could not start sub service mobilenotification.\n");
            pserver_state->please_exit=1;
        }
                */

        ierr = instruments_send_channel_request(pserver_state->instr, "com.apple.instruments.UIAutomation",9);

        if (ierr != INSTRUMENTS_E_SUCCESS)
        {
            printf("ERROR: Could not start sub service UIAutomation.\n");
            pserver_state->please_exit=1;
        }
    


    if (!pserver_state->please_exit)
    {
        //printf("before config launch env\n");

        ierr = instruments_config_launch_env(pserver_state->instr,9,"./","./vncscript.js");

        if (ierr != INSTRUMENTS_E_SUCCESS)            // once the program is started
        {
            printf("ERROR: Could not configure launch environment.\n");
            pserver_state->please_exit=1;
        }

        //printf("before pid check\n");

#ifndef XCUITEST
        ierr = instruments_get_pid(pserver_state->instr, 5 , b_id, &pserver_state->pid);

        //ierr = instruments_get_pid(pserver_state->instr, 5 , "com.techendeavour.WatchPOC.Retail", &pserver_state->pid);
        if ((ierr == INSTRUMENTS_E_SUCCESS) && (pserver_state->pid>0))
        {

            ierr = instruments_pid_ops(pserver_state->instr,5,pserver_state->pid,"resumePid:");

            if (ierr != INSTRUMENTS_E_SUCCESS) {
                printf("ERROR: Resume pid failed.\n");
            }

            printf("before killpid.\n");
            ierr = instruments_pid_ops(pserver_state->instr,5,pserver_state->pid,"killPid:");
            if (ierr != INSTRUMENTS_E_SUCCESS)
            {
                printf("ERROR: killpid failed.\n");
            }
            sleep(5);
        }
#endif
    }

    //printf("before launch app\n");
    // launchPcloudyHelper(pserver_state);

    if (!pserver_state->please_exit)
    {
#ifndef XCUITEST	
        ierr = instruments_launch_app(pserver_state->instr,5,pserver_state->packagepath,b_id,&pserver_state->pid);
#endif		
        //ierr = instruments_launch_app(pserver_state->instr,5,pserver_state->packagepath,"com.techendeavour.WatchPOC.Retail",&pserver_state->pid);

        if (ierr != INSTRUMENTS_E_SUCCESS) {
            printf("ERROR: Could not launch app. trying again\n");
#ifndef XCUITEST				
            ierr = instruments_launch_app(pserver_state->instr,5,pserver_state->packagepath,b_id,&pserver_state->pid);
#endif			
            //ierr = instruments_launch_app(pserver_state->instr,5,pserver_state->packagepath,"com.techendeavour.WatchPOC.Retail",&pserver_state->pid);
            if (ierr != INSTRUMENTS_E_SUCCESS) {
                printf("ERROR: Could not launch app. giving up\n");
                pserver_state->please_exit=1;
            }
        }
        //printf("App Launched pid is %llu\n",pserver_state->pid);
    }

    //printf("before start resume pid\n");

    if (!pserver_state->please_exit)
    {
        /* comment since this code has no effect
                ierr = instruments_pid_ops(pserver_state->instr,5,pserver_state->pid,"startObservingPid:");

                if (ierr != INSTRUMENTS_E_SUCCESS)
                {
                        printf("ERROR: Start observing pid failed.\n");
                }
                */
#ifndef XCUITEST
        ierr = instruments_pid_ops(pserver_state->instr,5,pserver_state->pid,"resumePid:");

        if (ierr != INSTRUMENTS_E_SUCCESS) {
            printf("ERROR: Resume pid failed.\n");
        }

        //printf("INFO: Before Start agent.\n");
        ierr = instruments_start_agent(pserver_state->instr,9,pserver_state->pid);
        //printf("INFO: After Start agent.\n");

        if (ierr != INSTRUMENTS_E_SUCCESS) {
            printf("ERROR: Start agent failed.\n");
        }
#endif

        //sleep(5);
#ifndef XCUITEST	
        ierr = instruments_start_script(pserver_state->instr,9,pserver_state->pid,fetch_events_callback,pserver_state,pserver_state->packagepath);
#endif		
        if (ierr != INSTRUMENTS_E_SUCCESS) {
            printf("ERROR: Starting script failed.\n");
            pserver_state->please_exit=1;
        }
        else
        {
            char *fe = (char*) malloc(12);
            if (fe)
            {

#if defined IOS_8_3 || defined IOS_9_0 || IOS_10_0
                //T 100printf(fe,": 100 \n");
                sprintf(fe,"D \n");
#else
                sprintf(fe,"T 100 100 \n");
#endif
                thread_queue_add(pserver_state->queue,fe,1);
            }
            char *fe2 = (char*) malloc(12);
            if (fe2)
            {
#if defined IOS_8_3 || defined IOS_9_0 || IOS_10_0
		                sprintf(fe2,"T 100 100 \n");
#else
                sprintf(fe2,"Z \n");
#endif
                thread_queue_add(pserver_state->queue,fe2,1);
            }
        }
    }

    printf("looping\n");
    return ierr;
}



void launchPcloudyHelper(pServerState_t *pserver_state)
{

    instruments_error_t ierr = INSTRUMENTS_E_UNKNOWN_ERROR;
    ierr = instruments_get_pid(pserver_state->instr, 5 , helperb_id, &pserver_state->helper_pid);
    if ((ierr == INSTRUMENTS_E_SUCCESS) && (pserver_state->helper_pid>0))
    {

        ierr = instruments_pid_ops(pserver_state->instr,5,pserver_state->helper_pid,"resumePid:");

        if (ierr != INSTRUMENTS_E_SUCCESS) {
            printf("ERROR: Resume pid failed.\n");
        }

        printf("before killpid.\n");
        ierr = instruments_pid_ops(pserver_state->instr,5,pserver_state->helper_pid,"killPid:");
        if (ierr != INSTRUMENTS_E_SUCCESS)
        {
            printf("ERROR: killpid failed.\n");
        }
        sleep(1);
    }
    if (!pserver_state->please_exit)
    {
        ierr = instruments_launch_app(pserver_state->instr,5,pserver_state->helper_packagepath,helperb_id,&pserver_state->helper_pid);
        if (ierr != INSTRUMENTS_E_SUCCESS) {
            printf("ERROR: Could not launch app. trying again\n");
            ierr = instruments_launch_app(pserver_state->instr,5,pserver_state->helper_packagepath,helperb_id,&pserver_state->helper_pid);
            //ierr = instruments_launch_app(pserver_state->instr,5,pserver_state->packagepath,"com.techendeavour.WatchPOC.Retail",&pserver_state->pid);
            if (ierr != INSTRUMENTS_E_SUCCESS) {
                printf("ERROR: Could not launch app. giving up\n");
                pserver_state->please_exit=1;
            }
        }
        ierr = instruments_pid_ops(pserver_state->instr,5,pserver_state->helper_pid,"resumePid:");

        if (ierr != INSTRUMENTS_E_SUCCESS) {
            printf("ERROR: Resume pid failed.\n");
        }
        printf("before launch app %d \n",__LINE__);
        sleep(3);
    }
}  




#ifdef KEYQUEUE
void timer_handler (int signum)
{
    struct threadmsg msg;

    long q_count;
    q_count = thread_queue_length(pserver_state.k_queue);
    
    if(q_count)
    {
        char *fe = calloc(q_count+128,1);

        int j = 0;
        char keyBuffer[128] = {0};
        struct timespec timeout;

        timeout.tv_sec  = 0;
        timeout.tv_nsec = 0;
        fe[0]='k'; fe[1]=0x20;
        for(j=2;j<q_count+2;j++)
        {
            thread_queue_get(pserver_state.k_queue, &timeout, &msg);
            char *msg_data = (char *)msg.data;
            //sprintf(fe,"");
            if(*msg_data == 0x08)
            {
                strcat(fe,"\\b");
                j += strlen("\\b");
            }
            else if(*msg_data == 0x0D)
            {
                strcat(fe,"\\n");
                j += strlen("\\n");
            }
            else
                fe[j] = *msg_data;

            free(msg_data);
        }
        fe[j] = '\0';
        processEvents(fe,sessionId);
        free(fe);
        //sprintf(keyBuffer,"/system/bin/input text %s", ReplaceString(fe, q_count));
        //system(keyBuffer);
    }

}


void keyevent_handler(void *ptr)
{
    //L("INSIDE KEYEVENT HANDLER\n");
    //queue= (struct threadqueue *) malloc(sizeof(struct threadqueue));
    struct sigaction sa;
    struct itimerval timer;

    /* Install timer_handler as the signal handler for SIGVTALRM. */
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
    sigaction (SIGVTALRM, &sa, NULL);

    /* Configure the timer to expire after 250 msec... */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 250000;
    /* ... and every 250 msec after that. */
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 250000;

    setitimer (ITIMER_VIRTUAL, &timer, NULL);
}

#endif

void on_key_event(rfbBool down,rfbKeySym key,rfbClientPtr cli)
{
    static char kbString[5]= {0};
    pServerState_t *pserver_state=cli->screen->screenData;
    if (pserver_state->input_enabled)
    {
        //printf("key %d\n",key);
        if ((key == 0xff50) && (!down) && (pserver_state->processScreen == 1))
        {
            char *fe = (char*) malloc(4);
            if (fe)
            {
                sprintf(fe,"H \n");
#ifndef XCUITEST
                pserver_state->processScreen = 0;
                thread_queue_add(pserver_state->queue,fe,1);
#else
                thread_queue_add(pserver_state->queue,fe,1);
#endif
            }
        }
#ifndef XCUITEST
#else
        else if((key > 0x00) && (key < 0x7F) && (!down) && (pserver_state->processScreen == 1))
        {
            char *fe = (char*) malloc(4);
            //*fe = (char)key;

#ifdef KEYQUEUE
            sprintf(fe,"%c",(char)key);
            thread_queue_add(pserver_state->k_queue,fe,1);
#else
            sprintf(fe,"k %c",(char)key);
            thread_queue_add(pserver_state->queue,fe,1);
#endif
        }
        else if((key == 0xFF08) && (!down) && (pserver_state->processScreen == 1))
        {
            char *fe = (char*) malloc(4);
            //*fe = (char)key;

#ifdef KEYQUEUE
            //sprintf(fe,"%s","\\b");
            *fe = 0x08;
            thread_queue_add(pserver_state->k_queue,fe,1);
#else
            //sprintf(fe,"%s","\\b");
            *fe = 0x08;
            thread_queue_add(pserver_state->queue,fe,1);
#endif

        }
        else if((key == 0xFF0d) && (!down) && (pserver_state->processScreen == 1))
        {
            char *fe = (char*) malloc(4);
            //*fe = (char)key;

#ifdef KEYQUEUE
            //sprintf(fe,"%s","\\b");
            *fe = 0x0d;
            thread_queue_add(pserver_state->k_queue,fe,1);
#else
            //sprintf(fe,"%s","\\b");
            *fe = 0x0d;
            thread_queue_add(pserver_state->queue,fe,1);
#endif
        
    }
#endif
  }

}

void on_mouse_event(int buttonMask,int x,int y,rfbClientPtr cli)
{
    int xi,yi;
    pServerState_t *pserver_state=cli->screen->screenData;
    if (pserver_state->input_enabled)
    {
        if (pserver_state->orientation==1)
        {
            if(UIorientation == 1 )
            {
                xi=y;
                yi=cli->screen->width-1-x;
                if (xi>=cli->screen->height) xi = cli->screen->height-1;
                if (xi<0) xi = 0;
                if (yi>=cli->screen->width) yi = cli->screen->width-1;
                if (yi<0) yi = 0;
                //printf("x:%d,y:%d,xi:%d,yi:%d \n",x,y,xi,yi);
            }
            else if(UIorientation == 3)
            {
                xi=x; yi=y;
                if (xi>=cli->screen->width) xi = cli->screen->width-1;
                if (xi<0) xi = 0;
                if (yi>=cli->screen->height) yi = cli->screen->height-1;
                if (yi<0) yi = 0;
            }
            
        }
        else
        {
            xi=x; yi=y;
            if (xi>=cli->screen->width) xi = cli->screen->width-1;
            if (xi<0) xi = 0;
            if (yi>=cli->screen->height) yi = cli->screen->height-1;
            if (yi<0) yi = 0;
        }

        pClientState_t *cliState = (pClientState_t *) cli->clientData;
        if (buttonMask & 1)
        {
            if (!cliState->leftClicked)
            {
                cliState->leftClicked=1;
                cliState->down_time=time(NULL);
                cliState->down_x=xi;
                cliState->down_y=yi;
            }
        }
        else if(buttonMask & 4)
        {
            if (!cliState->rightClicked)
            {
                cliState->rightClicked=1;
                cliState->down_time=time(NULL);
                cliState->down_x=xi;
                cliState->down_y=yi;
            }
    	}else if (buttonMask & 8){
        char *su = (char*) malloc(32);
        sprintf(su,"n");
	scrollFlag++;
        if(scrollFlag == 1)
        {
            printf("Mouse Scroll Up \n");
            if(UIorientation == 1)
            {
                if (pserver_state->retina==1)
                {
                    sprintf(su,"S %d %d %d %d ",xi/2,height/4,xi/2,yi/2);   
                }
                else if (pserver_state->retina==2)
                {
                    sprintf(su,"S %d %d %d %d ",xi/3,height/6,xi/3,yi/3);
                }
                else
                {
                    sprintf(su,"S %d %d %d %d ",xi,height/2,xi,yi);
                }
            }
            else if (UIorientation == 3)
            {
                #if defined OS_14_1
                if (pserver_state->retina==1)
                {
                    sprintf(su,"S %d %d %d %d",xi/2,width/4,xi/2,yi/2);
                }
                else if (pserver_state->retina==2)
                {
                    sprintf(su,"S %d %d %d %d ",xi/3,width/6,xi/3,yi/3);
                }
                else
                {
                    sprintf(su,"S %d %d %d %d ",xi,width/2,xi,yi);
                }
                #else
                if (pserver_state->retina==1)
                {
                    sprintf(su,"S %d %d %d %d ",xi/2,yi/2,width/4,yi/2);   
                }
                else if (pserver_state->retina==2)
                {
                    sprintf(su,"S %d %d %d %d ",xi/3,yi/3,width/6,yi/3);
                }
                else
                {
                    sprintf(su,"S %d %d %d %d ",xi,yi,width/2,yi);
                }
                #endif
               
            }
            
            if(!processing)
                thread_queue_add(pserver_state->queue,su,1);
        }

    }else if (buttonMask & 16){
        char *sd = (char*) malloc(32);
        sprintf(sd,"N");
        scrollFlag++;
        // thread_queue_add(pserver_state->queue,sd,1);
        if(scrollFlag == 1)
        {
            printf("Mouse Scroll Down \n");  
            if(UIorientation == 1)
            {
                if (pserver_state->retina==1)
                {
                    sprintf(sd,"S %d %d %d %d ",xi/2,yi/2,xi/2,height/4);
                }
                else if (pserver_state->retina==2)
                {
                    sprintf(sd,"S %d %d %d %d ",xi/3,yi/3,xi/3,height/6);
                }
                else
                {
                    sprintf(sd,"S %d %d %d %d ",xi,yi,xi,height/4);
                }
            }
            else if(UIorientation == 3)
            {
                #if defined OS_14_1
                if (pserver_state->retina==1)
                {
                    sprintf(sd,"S %d %d %d %d ",xi/2,yi/2,xi/2,width/4);  
                }
                else if (pserver_state->retina==2)
                {
                    sprintf(sd,"S %d %d %d %d ",xi/3,yi/3,xi/3,width/6);
                }
                else
                {
                    sprintf(sd,"S %d %d %d %d ",xi,yi,xi,width/2);
                }
                #else
                 if (pserver_state->retina==1)
                {
                    sprintf(sd,"S %d %d %d %d ",width/4,yi/2,xi/2,yi/2);
                }
                else if (pserver_state->retina==2)
                {
                    sprintf(sd,"S %d %d %d %d ",width/6,yi/3,xi/3,yi/3);
                }
                else
                {
                    sprintf(sd,"S %d %d %d %d ",width/2,yi,xi,yi);
                }
                #endif
            }
            
            if(!processing)
                thread_queue_add(pserver_state->queue,sd,1);
        }    
            }else if (cliState->rightClicked) // right button released
    	    {
    		//time_t time_diff = time(NULL) - cliState->down_time ;
    		//printf("time diff %d",time_diff);
    		cliState->rightClicked=0;
    		if ((xi >= cliState->down_x-10) && (xi<= cliState->down_x+10) && (yi >= cliState->down_y-10) && (yi<= cliState->down_y+10))
    		{
    			char *fe = (char*) malloc(14);
    			if (fe)
    			{
    				if (pserver_state->retina==1)
    				{
    					sprintf(fe,"d %d %d \n",xi/2,yi/2);
    				}
    				else if (pserver_state->retina==2)
    				{
    					sprintf(fe,"d %d %d \n",xi/3,yi/3);
    				}
    				else
    				{
    					sprintf(fe,"d %d %d \n",xi,yi);
    				}
                    if(!processing)
                        thread_queue_add(pserver_state->queue,fe,1);
                }
            }
            else
            {
                char *fe = (char*)malloc(26);
                if (fe)
                {
                    int min_x,min_y,max_x,max_y,center_x,center_y;
                    if (cliState->down_x > xi)
                    {
                        if (pserver_state->retina==1)
                        {
                            min_x = xi/2;
                            max_x = cliState->down_x/2;
                        }
                        else if (pserver_state->retina==2)
                        {
                            min_x = xi/3;
                            max_x = cliState->down_x/3;
                        }
                        else
                        {
                            min_x = xi;
                            max_x = cliState->down_x;
                        }
                    }
                    else
                    {
                        if (pserver_state->retina==1)
                        {
                            min_x = cliState->down_x/2;
                            max_x = xi/2;
                        }
                        else if (pserver_state->retina==2)
                        {
                            min_x = cliState->down_x/3;
                            max_x = xi/3;
                        }
                        else
                        {
                            min_x = cliState->down_x;
                            max_x = xi;
                        }
                    }

                    if (cliState->down_y > yi)
                    {
                        if (pserver_state->retina==1)
                        {
                            min_y = yi/2;
                            max_y = cliState->down_y/2;
                        }
                        else if (pserver_state->retina==2)
                        {
                            min_y = yi/3;
                            max_y = cliState->down_y/3;
                        }
                        else
                        {
                            min_y = yi;
                            max_y = cliState->down_y;
                        }
                    }
                    else
                    {
                        if (pserver_state->retina==1)
                        {
                            min_y = cliState->down_y/2;
                            max_y = yi/2;
                        }
                        else if (pserver_state->retina==2)
                        {
                            min_y = cliState->down_y/3;
                            max_y = yi/3;
                        }
                        else
                        {
                            min_y = cliState->down_y;
                            max_y = yi;
                        }
                    }

                    center_x = min_x + (max_x - min_x )/2;
                    center_y = min_y + (max_y - min_y )/2;
                    if (cliState->down_y > yi)
                    {
                        sprintf(fe,"p %d %d %d %d ",center_x - (center_x - min_x)/5, center_y - (center_y-min_y)/5, (center_x+(max_x-center_x)/5),(center_y+(max_y-center_y)/5));
                    }
                    else
                    {
                        sprintf(fe,"q %d %d %d %d ",center_x - (center_x - min_x)/5, center_y - (center_y-min_y)/5, (center_x+(max_x-center_x)/5),(center_y+(max_y-center_y)/5));
                    }
                    if(!processing)
                        thread_queue_add(pserver_state->queue,fe,1);
                }
            }
        }
        else if (cliState->leftClicked) // left button released
        {
            struct timeval now;

            gettimeofday(&now,NULL);
            time_t time_diff = time(NULL) - cliState->down_time ;
            cliState->leftClicked=0;
            if ((xi >= cliState->down_x-10) && (xi<= cliState->down_x+10) && (yi >= cliState->down_y-10) && (yi<= cliState->down_y+10) && (time_diff<=1))
            {
                char *fe = (char*) malloc(14);
                if (fe)
                {
                    //printf("tap\n");
                    //printf("T %d %d \n",xi/2,yi/2);
                    if (pserver_state->retina==1)
                    {
                            #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                            {
                                char *curBundleId = (char *)malloc(128);
                                getCurrentApp(&curBundleId);
                                bundleId = curBundleId;
                                printf("AppBundleId : %s  And isAlert = %d \n  bundleId = %s",curBundleId,isAlert,bundleId);
                                if((!(strcmp(curBundleId,"com.apple.springboard"))))
                                    sprintf(fe,"T %d %d \n",yi/2,((Height/2)-(xi/2)));
                                else    
                                    sprintf(fe,"T %d %d \n",yi/2,((Width/2)-(xi/2)));
                            }
                            else
                                sprintf(fe,"T %d %d \n",xi/2,yi/2);
                            #else
                                sprintf(fe,"T %d %d \n",xi/2,yi/2);
                        #endif
                    }
                    else if (pserver_state->retina==2)
                    {
                         #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                                sprintf(fe,"T %d %d \n",yi/3,((Width/3)-(xi/3)));
                            else
                                sprintf(fe,"T %d %d \n",xi/3,yi/3);
                        #else   
                            sprintf(fe,"T %d %d \n",xi/3,yi/3);
                        #endif
                        
                    }
                    else
                    {
                         #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                                sprintf(fe,"T %d %d \n",yi,Width-xi);
                            else
                                sprintf(fe,"T %d %d \n",xi,yi);
                        #else
                            sprintf(fe,"T %d %d \n",xi,yi);
                        #endif
                        
                    }
                    if(!processing)
                        thread_queue_add(pserver_state->queue,fe,1);
                }
            }
            else if ((xi >= cliState->down_x-10) && (xi<= cliState->down_x+10) && (yi >= cliState->down_y-10) && (yi<= cliState->down_y+10) && (time_diff>1))
            {
                char *fe = (char*) malloc(14);
                if (fe)
                {
                    if (pserver_state->retina==1)
                    {
                        #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                                sprintf(fe,"t %d %d \n",yi/2,(Width/2)-(xi/2));
                            else
                                sprintf(fe,"t %d %d \n",xi/2,yi/2);
                            #else
                                sprintf(fe,"t %d %d \n",xi/2,yi/2);
                        #endif
                    }
                    else if (pserver_state->retina==2)
                    {   
                         #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                                sprintf(fe,"t %d %d \n",yi/3,((Width/3)-(xi/3)));
                            else
                                sprintf(fe,"t %d %d \n",xi/3,yi/3);
                        #else   
                            sprintf(fe,"t %d %d \n",xi/3,yi/3);
                        #endif
                    }
                    else
                    {
                        #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                                sprintf(fe,"t %d %d \n",yi,((Width-xi)));
                            else
                                sprintf(fe,"t %d %d \n",xi,yi);
                        #else
                            sprintf(fe,"t %d %d \n",xi,yi);
                        #endif
                    }
                    if(!processing)
                        thread_queue_add(pserver_state->queue,fe,1);
                }
            }
            else
            {
                char *fe = (char*)malloc(26);
                if (fe)
                {
    				//printf("swipe\n");
                    if (pserver_state->retina==1)
                    {
                        #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                                sprintf(fe,"S %d %d %d %d ",cliState->down_y/2,(Width/2)-cliState->down_x/2,yi/2,((Width/2)-xi/2));
                            else
                            {
                                // if (abs(x - x1) > abs(y - y1)){
                                if((abs((cliState->down_x/2)-(xi/2)) > abs((cliState->down_y/2) - (yi/2))))
                                {
                                    printf("Swipe up/down.............\n");
                                    if ( (cliState->down_x/2) > (xi/2)){
                                    printf("Swipe up.............\n");
                                    //sprintf(toX, "%d", (toXval+50));
                                    sprintf(fe,"S %d %d %d %d ",cliState->down_x/2,cliState->down_y/2,(cliState->down_x/2)+50,cliState->down_y/2);

                                }
                                if ( (cliState->down_x/2) < (xi/2)){
                                    printf("Swipe down.............\n");
                                    //sprintf(toX, "%d", (toXval-50));
                                    sprintf(fe,"S %d %d %d %d ",cliState->down_x/2,cliState->down_y/2,(cliState->down_x/2)-50,cliState->down_y/2);

                                }
                            }
                               // sprintf(fe,"S %d %d %d %d ",cliState->down_x/2,cliState->down_y/2,xi/2,yi/2);
                        #else
                                sprintf(fe,"S %d %d %d %d ",cliState->down_x/2,cliState->down_y/2,xi/2,yi/2);
                        #endif

                    }
                    else if (pserver_state->retina==2)
                    {
                        #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                                sprintf(fe,"S %d %d %d %d ",cliState->down_y/3,((Width/3)-cliState->down_x/3),yi/3,((Width/3)-xi/3));
                            else
                            {
                              // if (abs(x - x1) > abs(y - y1)){
                                if((abs((cliState->down_x/3)-(xi/3)) > abs((cliState->down_y/3) - (yi/3))))
                                {
                                    printf("Swipe up/down.............\n");
                                    if ( (cliState->down_x/3) > (xi/3)){
                                    printf("Swipe up.............\n");
                                    //sprintf(toX, "%d", (toXval+50));
                                    sprintf(fe,"S %d %d %d %d ",cliState->down_x/3,cliState->down_y/3,(cliState->down_x/3)+50,cliState->down_y/3);

                                }
                                if ( (cliState->down_x/3) < (xi/3)){
                                    printf("Swipe down.............\n");
                                    //sprintf(toX, "%d", (toXval-50));
                                    sprintf(fe,"S %d %d %d %d ",cliState->down_x/3,cliState->down_y/3,(cliState->down_x/3)-50,cliState->down_y/3);

                                }
                                }
                            }
                                //sprintf(fe,"S %d %d %d %d ",cliState->down_x/3,cliState->down_y/3,xi/3,yi/3);
                        #else
                                sprintf(fe,"S %d %d %d %d ",cliState->down_x/3,cliState->down_y/3,xi/3,yi/3);
                        #endif

                    }
                    else
                    {
                        #if defined  OS_14_1
                            printf(" for 14.1 version devices ********\n");
                            if(UIorientation ==3)
                                sprintf(fe,"S %d %d %d %d ",cliState->down_y,(Width-cliState->down_x),y,(Width-xi));
                            else
                            {
                                // if (abs(x - x1) > abs(y - y1)){
                                if((abs((cliState->down_x)-(xi)) > abs((cliState->down_y) - (yi))))
                                {
                                    printf("Swipe up/down.............\n");
                                    if ( (cliState->down_x) > (xi)){
                                    printf("Swipe up.............\n");
                                    //sprintf(toX, "%d", (toXval+50));
                                    sprintf(fe,"S %d %d %d %d ",cliState->down_x,cliState->down_y,(cliState->down_x)+50,cliState->down_y);

                                }
                                if ( (cliState->down_x) < (xi))
                                {
                                    printf("Swipe down.............\n");
                                    //sprintf(toX, "%d", (toXval-50));
                                    sprintf(fe,"S %d %d %d %d ",cliState->down_x,cliState->down_y,(cliState->down_x)-50,cliState->down_y);

                                }
                                }
                            }
                                //sprintf(fe,"S %d %d %d %d ",cliState->down_x,cliState->down_y,xi,yi);
                        #else
                                sprintf(fe,"S %d %d %d %d ",cliState->down_x,cliState->down_y,xi,yi);
                        #endif
                    }

                    if(!processing)
                        thread_queue_add(pserver_state->queue,fe,1);
                }
            }
        }
    }
}


#if defined IOS_9_0 || defined IOS_10_0
void read_png_buffer(png_structp png_ptr,
                     png_bytep data, png_size_t length)
{
    int i;
    pImgBuffer_t *imgBuffer = (pImgBuffer_t *) png_get_io_ptr(png_ptr);
    memcpy(data,imgBuffer->buffer+imgBuffer->offset,length);
    imgBuffer->offset+=length;
}
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

int getScreenDimension(screenshotr_client_t shotr,int *width, int *height)
{
    unsigned char *imgdata = NULL;
    uint64_t imgsize = 0;
    uint32_t offset = 0;
    int no_records = 0;
    uint16_t id,type;
    uint32_t count,value;
    int i;
   printf("getXCUIScreenShot calling");

        if ((screenshotr_take_screenshot(shotr, &imgdata, &imgsize) == SCREENSHOTR_E_SUCCESS) || (getXCUIScreenShot(&imgdata, &imgsize) == 0)) {
	printf("getXCUIScreenShot true");
		//if (readPngFile(&imgdata, &imgsize) == 0) {
#if !defined(IOS_9_0) && !defined(IOS_10_0)
        int rem = imgsize%sizeof(uint32_t);
        uint32_t *imgdata32= (uint32_t*)malloc((sizeof(uint32_t) -rem) + imgsize);

	memcpy(imgdata32,imgdata,imgsize);

        offset = __builtin_bswap32(imgdata32[1]);

        no_records = imgdata[offset]<<8|imgdata[offset+1];
        // printf("no_records %d\n",no_records);
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
            //printf("id %d type %d count %d value %d\n",id,type,count,value);
        }
        free(imgdata32);
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

#if defined(IOS_10_0)
        png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
#else
        png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
#endif

        int i,bit_depth;
        png_get_IHDR(png_ptr, info_ptr, width, height, &bit_depth, &color_type,
                     &interlace_type, NULL, NULL);

        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
#endif
        free(imgdata);
        //	free(imgdata32);
        //printf("done\n");
    } else {
        printf("Could not get screenshot!\n");
    }

    return (1==1);
}


int captureScreen(unsigned char *buffer, screenshotr_client_t shotr, int*min_x, int*min_y, int*max_x, int*max_y, pServerState_t *pserver_state )
{
    struct timeval now;

    int width, height;

    unsigned char *imgdata = NULL;
    uint64_t imgsize = 0;
    uint32_t offset = 0;
    int no_records = 0;
    uint16_t id,type;
    uint32_t count,value;
    int i;
    struct QData* qData = NULL;
    struct threadmsg msg;

    //if (screenshotr_take_screenshot(shotr, &imgdata, &imgsize) == SCREENSHOTR_E_SUCCESS)
#ifndef OS_11_0
    	if ((screenshotr_take_screenshot(shotr, &imgdata, &imgsize) == SCREENSHOTR_E_SUCCESS) || (getXCUIScreenShot(&imgdata, &imgsize) == 0))
#else
	if(1)
#endif
	{
        //printf(" screenshotr_take_screenshot: %ld\n", ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));
        if(imgsize > 0)
        {

#if !defined(IOS_9_0) && !defined(IOS_10_0)
            //		uint32_t *imgdata32= (uint32_t*) imgdata;
            uint32_t *buffer32 = (uint32_t*) buffer;
            int rem = imgsize%sizeof(uint32_t);

            uint32_t *imgdata32= (uint32_t *)malloc((sizeof(uint32_t) -rem) + imgsize);
            memcpy(imgdata32,imgdata,imgsize);

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
                //printf("id %d type %d count %d value %d\n",id,type,count,value);
            }
            printf("width %d,height %d\n",width,height);

            if (width<height)
            {
                int x,y;
                pserver_state->orientation=0;
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
                pserver_state->orientation=1;
                for (y=0; y<height; y++) {
                    for (x=0; x<width; x++) {
                        if ( buffer32[x*height+y] != imgdata32[(height-y)*width+x+2])
                        {
                            buffer32[x*height+y] = imgdata32[(height-y)*width+x+2];
                            //last32[x*height+y] = imgdata32[(height-y)*width+x+2];
                            if (y>*max_x) *max_x=y;
                            if (y<*min_x) *min_x=y;
                            if (x>*max_y) *max_y=x;
                            if (x<*min_y) *min_y=x;
                        }
                    }
                }
            }
            free(imgdata32);
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

            //png_init_io(png_ptr, fp);
            png_set_read_fn(png_ptr, (void *)&imgBuffer, read_png_buffer);

            png_set_sig_bytes(png_ptr, sig_read);

#if defined(IOS_10_0)
            png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
#else
            png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);
#endif

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
            //free(qData);
            pserver_state->scr_fail_count=0;

        }
    }
    else
    {
        pserver_state->scr_fail_count++;
        if (pserver_state->scr_fail_count>3)
        {
            printf("Could not get screenshot!\n");
            signaled = 1;
            pserver_state->please_exit=1;
            return 0;
        }
    }
    return 1;
}


int main(int argc,char** argv)
{
    signal(SIGINT, intHandler);
    signal(SIGKILL, intHandler);
    signal(SIGHUP, intHandler);
    signal(SIGILL, intHandler);
    long usec;

    int width, height, tmp;
    struct pollfd fds;
    struct pollfd fdc;
    int vnc_port=5900;
    int relaunch_count=0;
    //pServerState_t pserver_state;
    idevice_t device = NULL;
    lockdownd_client_t lckd = NULL;
    instproxy_client_t ipc = NULL;
    screenshotr_client_t shotr = NULL;
    lockdownd_service_descriptor_t service = NULL;
    rfbScreenInfoPtr server = NULL;
    int i;
    const char *udid = NULL;
    char *filename = NULL;
    static int timeoutPoll = 0;

    pserver_state.no_clients=0;
    pserver_state.orientation=0;
    pserver_state.scaling=100;
    pserver_state.please_exit=0;
    pserver_state.scr_fail_count=0;
    pserver_state.pid=0;
    pserver_state.instr = NULL;
    pserver_state.packagepath = NULL;
    pserver_state.srvSocket = -1;
    pserver_state.cliSocket = -1;
    pserver_state.input_enabled = 1;
    pserver_state.processScreen = 1;
    pserver_state.helper_pid=0;
    pserver_state.helper_packagepath = NULL;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
            idevice_set_debug_level(1);
            continue;
        }
        else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--udid")) {
            i++;
            if (!argv[i] || (strlen(argv[i])  < 0)) {
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
        else if (!strcmp(argv[i], "-P") || !strcmp(argv[i], "--port")) {
            i++;
            vnc_port=atoi(argv[i]);
            continue;
        }
        else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--scale")) {
            i++;
            pserver_state.scaling=atoi(argv[i]);
            continue;
        }
        else if (argv[i][0] != '-' && !filename) {
            filename = strdup(argv[i]);
            continue;
        }
        else if(!strcmp(argv[i], "-b") || !strcmp(argv[i],"--bundleid"))
        {
            i++;
#ifndef XCUITEST
            strcpy(b_id,argv[i]);
#endif
            continue;
        }

        else if(!strcmp(argv[i], "-hb") || !strcmp(argv[i],"--helperbundleid"))
        {
            i++;
            strcpy(helperb_id,argv[i]);
            continue;
        }

#ifndef XCUITEST
#else

        else if(!strcmp(argv[i], "-x") || !strcmp(argv[i],"--xport"))
        {
            i++;
            strcpy(xPort,argv[i]);
            continue;
        }
#endif
        else {
            print_usage(argc, argv);
            return 0;
        }
    }

    pserver_state.queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));

    thread_queue_init(pserver_state.queue);
#ifndef XCUITEST
    pserver_state.t_queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));
    thread_queue_init(pserver_state.t_queue);
#endif
#ifdef KEYQUEUE
    pserver_state.k_queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));
    thread_queue_init(pserver_state.k_queue);
#endif

    if (!pserver_state.please_exit)
    {
        if ( (pserver_state.srvSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("server socket create failed \n");
            pserver_state.please_exit = 1;
        }
        //printf("server socket create %d \n",pserver_state.srvSocket);
    }


    if (!pserver_state.please_exit)
    {
        int one = 1;
        memset(&pserver_state.srvSocketAddr, 0, sizeof(struct sockaddr_in));
        pserver_state.srvSocketAddr.sin_family = AF_INET;
        pserver_state.srvSocketAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        pserver_state.srvSocketAddr.sin_port = htons(vnc_port+10000);
        if ( setsockopt(pserver_state.srvSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) < 0 )
        {
            printf("set sock opt failed \n");

        }
        if ( bind(pserver_state.srvSocket, (struct sockaddr *)&pserver_state.srvSocketAddr, sizeof(struct sockaddr_in)) < 0)
        {
            printf("bind failed \n");
            pserver_state.please_exit = 1;
            close(pserver_state.srvSocket);
        }
    }


    if (!pserver_state.please_exit)
    {
        if (listen(pserver_state.srvSocket, 10 ) < 0)
        {
            printf("listen failed \n");
            pserver_state.please_exit = 1;
        }
        //printf("listening \n");
    }


    if (IDEVICE_E_SUCCESS != idevice_new(&device, udid)) {
        if (udid) {
            printf("No device found with udid %s, is it plugged in?\n", udid);
        } else {
            printf("No device found, is it plugged in?\n");
        }
        pserver_state.please_exit=1;
    }

    if (!pserver_state.please_exit)
    {
        if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(device, &lckd, NULL)) {
            printf("Exiting.\n");
            printf("LOCKDOWN ERROR\n");
            pserver_state.please_exit=1;
        }
    }

    instruments_error_t ierr = INSTRUMENTS_E_UNKNOWN_ERROR;

    if (!pserver_state.please_exit)
    {
        if ((lockdownd_start_service(lckd, "com.apple.mobile.installation_proxy", &service) != LOCKDOWN_E_SUCCESS) || !service) {
            printf("Could not connect to installation proxy.\n");
            pserver_state.please_exit=1;
        }
    }

    if (!pserver_state.please_exit)
    {
        ierr = instproxy_client_new(device, service, &ipc);

        if (service) {
            lockdownd_service_descriptor_free(service);
        }
        service = NULL;

        if (ierr != INSTPROXY_E_SUCCESS) {
            fprintf(stderr, "Could not connect to installation_proxy!\n");
            pserver_state.please_exit=1;
        }
        else
        {
            plist_t client_opts = instproxy_client_options_new();
            instproxy_client_options_add(client_opts, "ApplicationType", "User", NULL);
            plist_t apps = NULL;

            ierr = instproxy_browse(ipc, client_opts, &apps);
            instproxy_client_options_free(client_opts);
            if (ierr != INSTPROXY_E_SUCCESS) {
                fprintf(stderr, "ERROR: instproxy_browse returned %d\n", ierr);
                pserver_state.please_exit=1;
            }

            if (!apps || (plist_get_node_type(apps) != PLIST_ARRAY)) {
                fprintf(stderr,
                        "ERROR: instproxy_browse returnd an invalid plist!\n");
                pserver_state.please_exit=1;
            }
            else
            {
                for (i = 0; i < plist_array_get_size(apps); i++) {
                    plist_t app = plist_array_get_item(apps, i);
                    plist_t p_appid =
                            plist_dict_get_item(app, "CFBundleIdentifier");
                    char *s_appid = NULL;
                    char *s_path = NULL;
                    plist_dict_get_item(app, "CFBundleDisplayName");
                    if (p_appid) {
                        plist_get_string_val(p_appid, &s_appid);
                    }
                    if (!s_appid) {
                        fprintf(stderr, "ERROR: Failed to get APPID!\n");
                        pserver_state.please_exit=1;
                        break;
                    }
                    //#ifndef XCUITEST
                    if (strcmp(s_appid,b_id)==0)
                        //if (strcmp(s_appid,"com.techendeavour.WatchPOC.Retail")==0)
                    {
                        plist_t path = plist_dict_get_item(app, "Path");
                        if (path) {
                            plist_get_string_val(path, &s_path);
                            printf("%s %s\n", s_appid, s_path);
                            pserver_state.packagepath = strdup(s_path);
                            free(s_path);
                        }
                        //free(s_appid);
                        //break;
                    }
                    //#endif
                    if (strcmp(s_appid,helperb_id)==0)
                    {
                        plist_t path = plist_dict_get_item(app, "Path");
                        if (path) {
                            plist_get_string_val(path, &s_path);
                            printf("%s %s\n", s_appid, s_path);
                            pserver_state.helper_packagepath = strdup(s_path);
                            free(s_path);
                        }
                        //free(s_appid);
                        //break;
                    }
                    free(s_appid);
                }
                plist_free(apps);
#ifndef XCUITEST
                if (pserver_state.packagepath==NULL)
                {
                    fprintf(stderr,"ialice missing ... \n");
                    pserver_state.please_exit=1;
                }
#endif
                if (pserver_state.helper_packagepath==NULL)
                {
                    fprintf(stderr,"pcloudy helper missing ... \n");
                    //pserver_state.please_exit=1;
                }
            }
        }
    }

    if (!pserver_state.please_exit)
    {
        lockdownd_start_service(lckd, "com.apple.mobile.screenshotr", &service);
        //lockdownd_start_service(lckd, "com.apple.AirPlay", &service);
        //lockdownd_start_service(lckd, "com.apple.cmio.services.xpccontrol", &service);
        if (service) {
            lockdownd_client_free(lckd);
        }

        if (service && service->port > 0)
        {
            if (screenshotr_client_new(device, service, &shotr) != SCREENSHOTR_E_SUCCESS)
            {
                printf("Could not connect to screenshotr!\n");
                pserver_state.please_exit=1;
            }
        } else {
            printf("Could not start screenshotr service! Remember that you have to mount the Developer disk image on your device if you want to use the screenshotr service.\n");
            pserver_state.please_exit=1;
        }
    }

    if (!pserver_state.please_exit)
    {	

	while(flag == 0){

          // width=1125;
        //   height=2436;
            getXCTestSession(sessionId);
          //  getWindowSize(sessionId);
        	getScreenDimension(shotr,&width, &height);
		if((!strcmp(udid,"00008020-0019050A0CF2002E"))||(!strcmp(udid,"00008020-000A71490198002E"))||(!strcmp(udid,"00008020-001A54A00A08002E"))||(!strcmp(udid,"00008020-000A4D0E2E98002E")))
		{
			width = 1242; 
			height = 2688;
		}	
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
       
        printf("width = %d height = %d\n",width,height);
        if ((width==768) || (width==320))
        {
            pserver_state.retina=0;
        }
        else if ((width==1242) || (width==2208) || (width==1125)|| (width==1170) || (width == 1284)) 
        {
            pserver_state.retina=2;
        }
        else
        {
            pserver_state.retina=1;
        }

        // printf("width %d,height %d\n",width,height);
        server=rfbGetScreen(&argc,argv,width,height,8,3,BPP);

        if(!server)
        {
            pserver_state.please_exit=1;
        }
    }

    if (!pserver_state.please_exit)
    {
        server->desktopName = "pCloudy";
        server->frameBuffer=(char*)malloc(width*height*BPP);
        server->alwaysShared=(1==1);
        server->cursor = NULL;
        server->httpDir=NULL;
        server->listenInterface=htonl(INADDR_ANY);
        server->port = vnc_port;
        server->listen6Interface="::1";
        server->ipv6port = vnc_port;
        server->ptrAddEvent = on_mouse_event;
        server->kbdAddEvent = on_key_event;
        server->newClientHook = (rfbNewClientHookPtr)clientNewHook;
        server->handleEventsEagerly = TRUE;
        server->screenData = &pserver_state;
        // fprintf(stderr,"def time %d\n",server->deferUpdateTime);

        // Initialize the server
        rfbInitServer(server);

        if (server->listenSock==-1)
        {
            pserver_state.please_exit=1;
        }

        pserver_state.screen = server;

#if 0 
        if (thread_new(&checkFdThread,ProcessCheckFdsHandler, &pserver_state))
        {
            pserver_state.please_exit = 1;
            fprintf(stderr, "ProcessCheckFdsHandler couold not be created\n");
        }
#endif
    }

#if !defined(OS_11_0) && !defined(IOS_10_0)
    ierr = start_input_script(&pserver_state, device);
#endif
#if !defined(IOS_9_0) && !defined(IOS_10_0)
    if(ierr != INSTRUMENTS_E_SUCCESS)
    {
        printf("start input script failed : %d\n",ierr);
        pserver_state.please_exit = 1;
    }
#endif
    // Loop, processing clients and taking pictures
    //rfbMarkRectAsModified(server,0,0,width,height);
#ifdef KEYQUEUE
    //keyevent_handler(0);
#endif
    
#ifdef OS_11_0
    sleep(5);
#endif

    while(!pserver_state.please_exit && !signaled && rfbIsActive(server))
    {

        int min_x=width,min_y=height,max_x=-1,max_y=-1;
        
        usleep(1000);
        if (pserver_state.no_clients)
        {
             if (captureScreen((unsigned char *)server->frameBuffer,/*(unsignedchar*)lastBuffer,*/  shotr,&min_x,&min_y,&max_x,&max_y,&pserver_state))
            {
                 min_x--;min_y--;max_x++;max_y++;
                  if ((max_x>=min_x)&&(max_y>=min_y))
                  {
                     //printf("min %d, %d, max %d, %d\n",min_x,min_y,max_x,max_y);
                    rfbMarkRectAsModified(server,min_x,min_y,max_x,max_y);

                  }
              }
          }   

        //printf(" captureScreen: %ld\n", ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));

#if 0
        if (pserver_state.no_clients)
        {
            usec = server->deferUpdateTime*100;
        }
        else
        {
            // no client connected increase the delay
            usec = server->deferUpdateTime*1000000;
        }
#endif
        if (pserver_state.no_clients > 0 && pserver_state.processScreen)
        {
            rfbProcessEvents(server,0);
        }

        if (pserver_state.srvSocket>=0)
        {
            fds.fd = pserver_state.srvSocket;
            fds.events = POLLIN;
            if (poll(&fds,1,0)==1)
            {
                fprintf(stderr, "got data on server socketd\n");
                if(pserver_state.cliSocket>=0)
                {
                    close(pserver_state.cliSocket);
                }
                unsigned int cliLen = sizeof(struct sockaddr_in);
                memset(&pserver_state.cliSocketAddr, 0, sizeof(struct sockaddr_in));
                pserver_state.cliSocket = accept(pserver_state.srvSocket, (struct sockaddr_in *)&pserver_state.cliSocketAddr , &cliLen);
				//printf("client socket %d err %d\n",pserver_state.cliSocket,errno);
            }
        }
        if (pserver_state.cliSocket>=0)
        {
            fdc.fd = pserver_state.cliSocket;
            fdc.events = POLLIN;

            if (poll(&fdc,1,timeoutPoll)==1)
            {
                fprintf(stderr, "got data on client socket, timeouot = %d\n", timeoutPoll);
                char *fe = (char*) malloc(100);
                int recvSize = 0;
                recvSize = recv(pserver_state.cliSocket, fe, 2, MSG_DONTWAIT);
                if (recvSize <= 0)
                {
                    if (fe!=NULL)
                        free(fe);
                    close(pserver_state.cliSocket);
                    pserver_state.cliSocket = -1 ;
                }
                else
                {
                    if ((pserver_state.input_enabled) && (fe[0]=='N'))
                    {
                        pserver_state.input_enabled=0;
                        pserver_state.please_exit=0;
                        printf("disabling input ...\n");
                        char *fe2 = (char*) malloc(4);
                        if (fe2)
                        {
                            sprintf(fe2,"X \n");
                            thread_queue_add(pserver_state.queue,fe2,1);
                        }

                        sleep(1);

                        ierr = instruments_pid_ops(pserver_state.instr,5,pserver_state.pid,"resumePid:");

                        sleep(1);

                        if (ierr != INSTRUMENTS_E_SUCCESS) {
                            printf("ERROR: Resume pid failed.\n");
                        }

                        printf("killling pid %d.\n",pserver_state.pid);

                        ierr = instruments_pid_ops(pserver_state.instr,5,pserver_state.pid,"killPid:");
                        if (ierr != INSTRUMENTS_E_SUCCESS)
                        {
                            printf("ERROR: killpid failed.\n");
                        }
                        if (fe!=NULL) free(fe);

                        if (pserver_state.instr) instruments_client_free(pserver_state.instr);

                    }
                    else if((!pserver_state.input_enabled) && (fe[0]=='I'))
                    {
                        relaunch_count=0;
                        pserver_state.input_enabled=1;
                        pserver_state.please_exit=0;

                        printf("enabling input ...\n");
                        if (pserver_state.queue)
                        {
                            thread_queue_cleanup(pserver_state.queue,1);
                            free(pserver_state.queue);
                        }

                        pserver_state.queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));

                        thread_queue_init(pserver_state.queue);
#ifndef XCUITEST
                        pserver_state.t_queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));
                        thread_queue_init(pserver_state.t_queue);
#endif
#ifdef KEYQUEUE
                        pserver_state.k_queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));
                        thread_queue_init(pserver_state.k_queue);
#endif
#if !defined(OS_11_0) && !defined(IOS_10_0)
                        ierr = start_input_script(&pserver_state, device);
#endif
                        if (fe!=NULL) free(fe);

                    }
                    else if((pserver_state.input_enabled) && ((fe[0]=='h') || (fe[0]=='i')))
                    {
                        char *fe2 = (char*) malloc(4);
                       /* if (fe2)
                        {
                            sprintf(fe2,"H \n");
#ifndef XCUITEST
                            thread_queue_add(pserver_state.queue,fe2,1);
#else
                            thread_queue_add(pserver_state.queue,fe2,1);
#endif
                            //processEvents(fe2,sessionId);

                        }*/
                        if (fe!=NULL) free(fe);
                    }
                    //Keyboard text integration..
                    else if((pserver_state.input_enabled) && (fe[0]=='K') && (fe[1]==' '))
                    {
                        char* ptr = NULL, *ptr1 = NULL, *finalString = NULL;
                        unsigned int stringLen=0, recvLen=0;
                        char len[10];

                        recvSize = recv(pserver_state.cliSocket, fe,100,MSG_DONTWAIT);
                        if (recvSize <= 0)
                        {
                            if (fe!=NULL)
                                free(fe);
                            continue;
                        }

                        ptr = fe;

                        ptr1 = strchr(ptr, ' ');
                        if(ptr1 == NULL)
                        {
                            if (fe!=NULL)
                                free(fe);
                            continue;
                        }

                        strncpy(len,ptr,(unsigned int)(ptr1-ptr));
                        len[(unsigned int)(ptr1-ptr)] = '\0';
                        sscanf(len, "%u", &stringLen);

                        ptr1++; //It now points to exact start of string
                        recvLen = recvSize - (ptr1 - ptr);

                        finalString = (char*) malloc((stringLen+1) * sizeof(char));
                        strncpy(finalString,ptr1,recvLen);
                        finalString[recvLen] = '\0';

                        while(recvLen < stringLen)
                        {
                            recvSize = recv(pserver_state.cliSocket, fe,100,MSG_DONTWAIT);
                            if (recvSize <= 0)
                            {
                                if (finalString!=NULL)
                                {
                                    free(finalString);
                                    finalString = NULL;
                                }
                                break;
                            }
                            recvLen += recvSize;
                            strncat(finalString, fe, recvSize);
                            finalString[recvLen] = '\0';
                        }

                        finalString[stringLen] = '\0';
                        printf("Final String *********************** = %s\n",finalString);
                        //Send string to device here
                        if (finalString)
                        {
                            char *fe2 = (char*) malloc(3 + strlen(finalString));
                            if (fe2)
                            {
                                sprintf(fe2,"K %s", finalString);
                                fprintf(stderr,"%s\n", fe2);
                                thread_queue_add(pserver_state.queue,fe2,1);
                            }

                            free(finalString);
                            finalString = NULL;
                        }
                        if (fe!=NULL)
                        {
                            free(fe);
                            fe = NULL;
                        }
                    }
                    else if((pserver_state.input_enabled) && (fe[0]!='I') && (fe[0]!='N'))
                    {
#if defined(IOS_9_0) || defined(IOS_10_0)
                        if (fe[0]=='P')
                        {
                            pserver_state.orientation=0;
                        }
                        else if(fe[0]=='L')
                        {
                            pserver_state.orientation=1;
                        }
#endif
                        thread_queue_add(pserver_state.queue,fe,1);
                    }
                    else
                    {
                        if (fe!=NULL) free(fe);
                    }
                }
            }
        }
#ifndef XCUITEST 		
        if ((pserver_state.please_exit)&&(!signaled)&&(relaunch_count<7))
        {
            relaunch_count++;
#else
        if ((!signaled)&&(relaunch_count<7))
        {
            relaunch_count=7;
#endif
            printf("starting again...\n");
            if (pserver_state.queue)
            {
                thread_queue_cleanup(pserver_state.queue,1);
                free(pserver_state.queue);
            }

            pserver_state.queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));

            thread_queue_init(pserver_state.queue);
#ifndef XCUITEST
            pserver_state.t_queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));
            thread_queue_init(pserver_state.t_queue);
#endif
#ifdef KEYQUEUE
            pserver_state.k_queue = (struct threadqueue *) malloc(sizeof(struct threadqueue));
            thread_queue_init(pserver_state.k_queue);
#endif
            ierr = instruments_config_launch_env(pserver_state.instr,9,"./","./vncscript.js");

            if (ierr != INSTRUMENTS_E_SUCCESS)
            {
                printf("ERROR: Could not configure launch environment.\n");
                pserver_state.please_exit=1;
            }
            //instruments_stop_script(pserver_state.instr,9);
#ifndef XCUITEST
            ierr = instruments_launch_app(pserver_state.instr,5,pserver_state.packagepath,b_id,&pserver_state.pid);
            //ierr = instruments_launch_app(pserver_state.instr,5,pserver_state.packagepath,"com.techendeavour.WatchPOC.Retail",&pserver_state.pid);
            ierr = instruments_pid_ops(pserver_state.instr,5,pserver_state.pid,"resumePid:");
                
            printf("INFO: Before Start agent.\n");

            ierr = instruments_start_agent(pserver_state.instr,9,pserver_state.pid);
            printf("INFO: After Start agent.\n");

            if (ierr != INSTRUMENTS_E_SUCCESS) {
                printf("ERROR: Start agent failed.\n");
            }

            ierr = instruments_start_script(pserver_state.instr,9,pserver_state.pid,fetch_events_callback,&pserver_state,pserver_state.packagepath);

            printf("Pid during relaunch is %d\n",pserver_state.pid);
            if (ierr != INSTRUMENTS_E_SUCCESS) {
                printf("ERROR: restarting script failed.\n");
                pserver_state.please_exit=1;
            }
            else
#endif
            {
                char *fe = (char*) malloc(12);
                pserver_state.please_exit=0;
                if (fe)
                {
#if defined IOS_8_3 || defined IOS_9_0 || IOS_10_0
                    sprintf(fe,"D \n");
#else
                    sprintf(fe,"T 100 100 \n");
#endif
                    thread_queue_add(pserver_state.queue,fe,1);
                }
                char *fe2 = (char*) malloc(12);
                if (fe2)
                {
#if defined IOS_8_3 || defined IOS_9_0 || IOS_10_0
                    sprintf(fe2,"T 100 100 \n");
#else
                    sprintf(fe2,"Z \n");
#endif
#ifndef XCUITEST
                    thread_queue_add(pserver_state.queue,fe2,1);
#endif
                }
#ifndef XCUITEST
#else
                if(thread_new(&eventFdThread,ProcessEventsThread,NULL))
                {
                    pserver_state.please_exit = 1;
                    fprintf(stderr, "ProcessEventsThread could not be created\n");
                }
#ifdef OS_11_0
		    /*if(thread_new(&XCUIPopThread,XCUIPopHandleThread,NULL))
		    {
			pserver_state.please_exit = 1;
			fprintf(stderr, "XCUIPopHandleThread could not be created\n");
		    }*/
#endif
#endif
                //sleep(2);

                if (thread_new(&checkFdThread,ProcessCheckFdsHandler, &pserver_state))
                {
                    pserver_state.please_exit = 1;
                    fprintf(stderr, "ProcessCheckFdsHandler couold not be created\n");
                }
                timeoutPoll = 100; //set timeout to 100ms after all init for poll
            }
        }
    }

    instruments_stop_script(pserver_state.instr,9);

    if (server)
    {
        pserver_state.please_exit = 1;
        thread_join(checkFdThread);
        if (server->frameBuffer)
            free(server->frameBuffer);

        rfbScreenCleanup(server);
    }

    if (pserver_state.pid>0)
    {
        char *fe = (char*) malloc(4);
        if (fe)
        {
            sprintf(fe,"X \n");
            thread_queue_add(pserver_state.queue,fe,1);
        }

        sleep(1);

        ierr = instruments_pid_ops(pserver_state.instr,5,pserver_state.pid,"resumePid:");

        sleep(1);

        if (ierr != INSTRUMENTS_E_SUCCESS) {
            printf("ERROR: Resume pid failed.\n");
        }

        //printf("killling pid %d.\n",pserver_state.pid);
        ierr = instruments_pid_ops(pserver_state.instr,5,pserver_state.pid,"killPid:");
        if (ierr != INSTRUMENTS_E_SUCCESS)
        {
            printf("ERROR: killpid failed.\n");
        }
    }

    if (shotr) screenshotr_client_free(shotr);

    if (ipc) instproxy_client_free(ipc);

    if (service)
        lockdownd_service_descriptor_free(service);

    if (pserver_state.queue)
    {
        thread_queue_cleanup(pserver_state.queue,1);
        free(pserver_state.queue);
    }

    if (pserver_state.instr) instruments_client_free(pserver_state.instr);
    if (device) idevice_free(device);

    if (filename)
        free(filename);

    if (pserver_state.packagepath)
        free(pserver_state.packagepath);

    return(0);

}

#ifndef XCUITEST
#else
void formJsonObject(json_object * jobj, char *key, char *value)
{
    /*Creating a json string*/
    json_object *jstring = json_object_new_string(value);

    /*Form the json object*/
    json_object_object_add(jobj,key, jstring);

}

int readPngFile(unsigned char** imgData, uint64_t* imageSize)
{
	FILE *file = NULL;
	char *buffer;
	unsigned long fileLen;
	int lock;
	//printf("fdlock at readPngFile before if %d\n",fd_lock);
	lock = flock(fd_lock, LOCK_EX);
	//if (lock == 0)
	{
		//printf("fdlock at readPngFile inside if lock %d\n",fd_lock);
		//printf("readPngFile %s\n", imgPath);
		file = fopen(imgPath,"rb");

		if(file!=NULL)
		{
			fseek(file, 0, SEEK_END);
			fileLen=ftell(file);
			fseek(file, 0, SEEK_SET);
			//printf("fileLen %d\n",fileLen);
			//Allocate memory
			buffer=(char *)malloc(fileLen+1);
			if (!buffer)
			{
				fprintf(stderr, "Memory error!");
				                        fclose(file);
				return 1;
			}

			//Read file contents into buffer
			fread(buffer, fileLen, 1, file);
			*imgData = buffer;
		    *imageSize = (uint64_t)fileLen;
			fclose(file);
			flock(fd_lock, LOCK_UN);
			return 0;
		}
		flock(fd_lock, LOCK_UN);
	}
		
	return 1;
}




void processEvents(char *msg, char *session)
{
    char xval[8] = {0};
    char yval[8] = {0};
    char toX[8] = {0};
    char toY[8] = {0};
    char *ptr = NULL, *ptr1 = NULL;
    char httpEndPointPrefix[] = "http://localhost";
    char httpEndPointUrl[256] = {0};
    char *eventString = NULL;
    json_object *jobj = NULL;
    json_object *jobj1 = NULL;
    json_object *jobj2 = NULL;
    json_object *jobj3 = NULL;
    json_object *jobj4 = NULL;
    json_object *jobj5 = NULL;
    json_object *jobj6 = NULL;
    json_object *jobj7 = NULL;
    json_object *array = NULL;

    eventString = (char *) malloc(strlen(msg));
    strcpy(eventString,msg);
    printf("eventString: %s sessionId: %s\n",eventString, sessionId);
    int len = strlen(eventString);
    if(strlen(sessionId)<=0)
        getXCTestSession(sessionId);
    
        eventcount++;
        if (eventcount >= 100){
            eventcount = 1;
        }
    switch(eventString[0])
    {
    case 'T':				/*TAP*/
        ptr = strstr(&eventString[2]," ");
	#if defined  OS_14_1
	        getXCTestSession(sessionId);
        #endif
//                getXCTestSession(sessionId);

        if(ptr != NULL)
        {    strncpy(xval,&eventString[2],ptr-(eventString+2));
            //printf("%s\n",xval);
            ptr1 = strstr((ptr+1)," ");
            if(ptr1 != NULL)
                strncpy(yval,ptr+1,ptr1-(ptr+1));
            //strncpy(yval,ptr+1,len-((ptr+1)-eventString));
            //printf("%s\n",yval);
        }
        /*
#if defined  OS_13_4
        isAlertPresent(sessionId); 
        if(isAlert)
        {
            getXCUISource(sessionId);
            memset(XCUILabel,0x00,sizeof(XCUILabel));
            isXCUILabelFound = 0;
        }
#endif
        if(isAlert)
        {
            memset(XCUILabel,0x00,sizeof(XCUILabel));
            isXCUILabelFound = 0;
            mutex_lock(&XCUImutex);
            isAlert = 0;
            if(sourcePayload != NULL)
                    getElementLabelFromSource(sessionId,atoi(xval),atoi(yval));
            if(strlen(XCUILabel)>0)
            {
                if((!strcmp(XCUILabel,"XCUIElementTypeSecureTextField")) || (!strcmp(XCUILabel,"XCUIElementTypeTextField")))
                {
                    char elementId[64] = {0};
                        jobj = json_object_new_object();
                        formJsonObject(jobj,"using","class name");
                        formJsonObject(jobj,"value",XCUILabel);
                        //Now printing the json object
                        //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                        getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
                        postHttpData(httpEndPointUrl,"");
                        json_object_put(jobj);
                        mutex_unlock(&XCUImutex);
                        break;
                }
                else
                {
                        char elementId[64] = {0}, tempLabel[32] = {0};
                        jobj = json_object_new_object();
                        formJsonObject(jobj,"name",XCUILabel);
                        //Now printing the json object
                        printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/alert/accept",httpEndPointPrefix,xPort,sessionId);
                        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
                        json_object_put(jobj);
                        mutex_unlock(&XCUImutex);
                        break;
                }
            }  
        }
*/
        //mutex_lock(&XCUImutex);
#if 0
#ifndef OS_11_0
        //isAlertPresent(sessionId); 
        if(ifFilteredApp(sessionId) || isAlert)
        {
            getXCUISource(sessionId);
            memset(XCUILabel,0x00,sizeof(XCUILabel));
            isXCUILabelFound = 0;
#endif
            //mutex_unlock(&XCUImutex);

            if(isAlert)
            {
                memset(XCUILabel,0x00,sizeof(XCUILabel));
                isXCUILabelFound = 0;
#ifdef OS_11_0
                mutex_lock(&XCUImutex);
#endif
                isAlert = 0;
                if(sourcePayload != NULL)
                    getElementLabelFromSource(sessionId,atoi(xval),atoi(yval));
                
                if(strlen(XCUILabel)>0)
                {
                    if((!strcmp(XCUILabel,"XCUIElementTypeSecureTextField")) || (!strcmp(XCUILabel,"XCUIElementTypeTextField")))
                    {
                        char elementId[64] = {0};
                        jobj = json_object_new_object();
                        formJsonObject(jobj,"using","class name");
                        formJsonObject(jobj,"value",XCUILabel);
                        /*Now printing the json object*/
                        //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                        getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
                        postHttpData(httpEndPointUrl,"");
                        json_object_put(jobj);
#ifdef OS_11_0
                        mutex_unlock(&XCUImutex);
#endif
                        break;
                    }
                    else
                    {
                        char elementId[64] = {0}, tempLabel[32] = {0};
                        jobj = json_object_new_object();
                        formJsonObject(jobj,"using","link text");
                        sprintf(tempLabel,"label=%s",XCUILabel);
                        formJsonObject(jobj,"value",tempLabel);
                        /*Now printing the json object*/
                        //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                        getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
                        postHttpData(httpEndPointUrl,"");
                        json_object_put(jobj);
#ifdef OS_11_0
                        mutex_unlock(&XCUImutex);
#endif
                        break;
                    }
                }
#ifdef OS_11_0
                mutex_unlock(&XCUImutex);
#endif
            }
#ifndef OS_11_0
            else if(isApp("App Store", sessionId,atoi(xval),atoi(yval)))
            {
                //mutex_lock(&XCUImutex);
                if(strlen(XCUILabel)>0)
                {
                    if((!strcmp(XCUILabel,"XCUIElementTypeSecureTextField")) || (!strcmp(XCUILabel,"XCUIElementTypeTextField")))
                    {
                        char elementId[64] = {0};
                        jobj = json_object_new_object();
                        formJsonObject(jobj,"using","class name");
                        formJsonObject(jobj,"value",XCUILabel);
                        /*Now printing the json object*/
                        //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                        getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
                        postHttpData(httpEndPointUrl,"");
                        json_object_put(jobj);
                        //mutex_unlock(&XCUImutex);
                        break;
                    }
                    else
                    {
                        printf("%s\n",XCUILabel);
                        char elementId[64] = {0}, tempLabel[1024] = {0};
                        if(strcmp(XCUILabel,"Featured")!=0) /*Disabling featured tab in AppStore*/
                        {
                            jobj = json_object_new_object();
                            formJsonObject(jobj,"using","link text");
                            sprintf(tempLabel,"label=%s",XCUILabel);
                            formJsonObject(jobj,"value",tempLabel);
                            /*Now printing the json object*/
                            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                            getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
                            sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
                            postHttpData(httpEndPointUrl,"");
                            json_object_put(jobj);
                        }
                        if(strcmp(XCUILabel,"App Store")==0) /*In case of AppStore move menu to search to fix hanging issue*/
                        {
                            isFilteredApp = YES;
                            printf("In App Store\n");
                            jobj = json_object_new_object();
                            formJsonObject(jobj,"using","link text");
                            sprintf(tempLabel,"label=%s","Top Charts");
                            formJsonObject(jobj,"value",tempLabel);
                            /*Now printing the json object*/
                            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                            getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
                            sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
                            postHttpData(httpEndPointUrl,"");
                            json_object_put(jobj);
                            jobj = json_object_new_object();
                            formJsonObject(jobj,"using","link text");
                            sprintf(tempLabel,"label=%s","Search");
                            formJsonObject(jobj,"value",tempLabel);
                            /*Now printing the json object*/
                            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                            getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
                            sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
                            postHttpData(httpEndPointUrl,"");
                            json_object_put(jobj);
                        }
                        else
                        {
                            isFilteredApp = NO;
                        }    
                        //mutex_unlock(&XCUImutex);
                        break;
                    }
                }
                //mutex_unlock(&XCUImutex);

            }
            else if(isApp("Settings", sessionId,atoi(xval),atoi(yval)))
            {
                isFilteredApp = YES;
                if(strlen(XCUILabel)>0)
                {
                    if((strcmp(XCUILabel,"Software Update")==0) || (strcmp(XCUILabel,"Touch ID & Passcode")==0))
                    {
                        printf("XCUILabel(%s)\n",XCUILabel);
                        break;
                    }
                    
                }
                
            }
            else
            {
                isFilteredApp = NO;
            }
        }

#endif
#endif
        char orientation[8] = {};
        sprintf(orientation,"%d",UIorientation);
        jobj = json_object_new_object();
        jobj1 = json_object_new_object();
        jobj2 = json_object_new_object();
        jobj3 = json_object_new_object();
        jobj4 = json_object_new_object();
    
        #if defined OS_14_1

            array = json_object_new_array();

            formJsonObject(jobj2,"action","tap");
            json_object_object_add(jobj2,"options",jobj);

            formJsonObject(jobj,"x",xval);
            formJsonObject(jobj,"y",yval);
           json_object_array_add(array,jobj2);
           // json_object_array_add(array,jobj1);
            //formJsonObject(jobj,"orientation",orientation);
            json_object_object_add(jobj3,"actions",array);

            printf ("The json object created: %s\n",json_object_to_json_string(jobj3));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/touch/perform",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj3));


        #else
        
            formJsonObject(jobj,"x",xval);
            formJsonObject(jobj,"y",yval);
	        formJsonObject(jobj,"orientation",orientation);
            printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/tap/0",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));

        #endif
    
        /*Now printing the json object*/
        
        printf("\n");
        json_object_put(jobj);
        getXCTestSession(sessionId);
        #if !defined  OS_13_4
            if(blockSettings)
        	    terminateIfSettingsIsOpen(0);
        #endif
        break;


    case 't':				/*Tap and hold*/
        ptr = strstr(&eventString[2]," ");
	if(ptr != NULL)
        {    strncpy(xval,&eventString[2],ptr-(eventString+2));
            printf("%s\n",xval);
            ptr1 = strstr((ptr+1)," ");
            if(ptr1 != NULL)
                strncpy(yval,ptr+1,ptr1-(ptr+1));
            //strncpy(yval,ptr+1,len-((ptr+1)-eventString));
            printf("%s\n",yval);

            jobj = json_object_new_object();
            formJsonObject(jobj,"x",xval);
            formJsonObject(jobj,"y",yval);
            formJsonObject(jobj,"duration","3.0");
            /*Now printing the json object*/
            printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/touchAndHold",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            json_object_put(jobj);
        }
        break;

    case 'H':				/*HomeScreen*/
        printf("HomeScreen Case\n");
        sprintf(httpEndPointUrl,"%s:%s/wda/homescreen",httpEndPointPrefix,xPort);
        isAlert = 0; //in case user press home button while there is a popup
        isFilteredApp = YES;
        //printf("%s\n",httpEndPointUrl);
        postHttpData(httpEndPointUrl,"");
	    getXCTestSession(sessionId);
        break;

    case 'K':				/*KeyBoard*/
    case 'k':
        ptr = strstr(eventString," ");
        if(ptr!=NULL)
        {
            //sprintf( xval,"%s", ptr);
            char *kbBuffer=NULL;
            kbBuffer = (char*)malloc(strlen(eventString) + 32);
            jobj = json_object_new_object();
            formJsonObject(jobj,"value",ptr);
            printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(kbBuffer,"{ \"value\": [\"%s\"] }",ptr+1);
            printf("kbBuffer %s\n",kbBuffer);
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/keys",httpEndPointPrefix,xPort,sessionId);
            //postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            postHttpData(httpEndPointUrl,kbBuffer);
            json_object_put(jobj);
        }
        break;
    case 'S':				/*Swipe/DragFromTo*/
        ptr = strtok(eventString+2," ");
	    #if defined  OS_14_1
            getXCTestSession(sessionId);
        #endif
//        getXCTestSession(sessionId);

	printf("Swipe \n ");
	//char orientation[8] = {};
        if(ptr!=NULL)
        {
                sprintf( xval,"%s", ptr);
                ptr = strtok(NULL, " ");
                sprintf(yval, "%s", ptr);
                ptr = strtok(NULL, " ");
                sprintf(toX, "%s", ptr);
                ptr = strtok(NULL, " ");
                sprintf(toY, "%s", ptr);
        }
	    sprintf(orientation,"%d",UIorientation);
        jobj = json_object_new_object();
        jobj = json_object_new_object();
        jobj1 = json_object_new_object();
        jobj2 = json_object_new_object();
        jobj3 = json_object_new_object();
        jobj4 = json_object_new_object();
        jobj5 = json_object_new_object();
        jobj6 = json_object_new_object();
        jobj7 = json_object_new_object();
        

       #if defined OS_14_1
            printf("Inside 14.0 \n");
            array = json_object_new_array();
            formJsonObject(jobj2,"action","press");
            json_object_object_add(jobj2,"options",jobj);   	            
            formJsonObject(jobj,"x",xval);
            formJsonObject(jobj,"y",yval);
            json_object_array_add(array,jobj2);

            formJsonObject(jobj3,"action","wait");
            json_object_object_add(jobj3,"options",jobj4);
            formJsonObject(jobj4,"ms","40");
            json_object_array_add(array,jobj3);

            formJsonObject(jobj5,"action","moveTo");
            json_object_object_add(jobj5,"options",jobj6);

            formJsonObject(jobj6,"x",toY);
            formJsonObject(jobj6,"y",toX);
            json_object_array_add(array,jobj5);

            formJsonObject(jobj7,"action","release");
            json_object_array_add(array,jobj7);
            
            json_object_object_add(jobj1,"actions",array);
            printf ("The json object created: %s\n",json_object_to_json_string(jobj1));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/touch/perform",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj1));
            scrollFlag = 0;
        
        #else
        
            formJsonObject(jobj,"fromX",xval);
            formJsonObject(jobj,"fromY",yval);
            formJsonObject(jobj,"toX",toX);
            formJsonObject(jobj,"toY",toY);
	        formJsonObject(jobj,"orientation",orientation);
            formJsonObject(jobj,"duration","0.05");
            printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            //sprintf(httpEndPointUrl,"%s:%s/session/%s/uiaTarget/0/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            //sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/element/0/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
 	        json_object_put(jobj);
            getXCTestSession(sessionId);
            scrollFlag = 0;

        #endif

        break;
    case 'D':				/*Tap Ok*/
        break;
    case 'd':				/*Double Tap*/
        ptr = strstr(&eventString[2]," ");
        if(ptr != NULL)
        {    strncpy(xval,&eventString[2],ptr-(eventString+2));
            printf("%s\n",xval);
            ptr1 = strstr((ptr+1)," ");
            if(ptr1 != NULL)
                strncpy(yval,ptr+1,ptr1-(ptr+1));
            //strncpy(yval,ptr+1,len-((ptr+1)-eventString));
            printf("%s\n",yval);
        }
        jobj = json_object_new_object();
        formJsonObject(jobj,"x",xval);
        formJsonObject(jobj,"y",yval);
        /*Now printing the json object*/
        printf ("The json object created: %s\n",json_object_to_json_string(jobj));
        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/doubleTap",httpEndPointPrefix,xPort,sessionId);
        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
        json_object_put(jobj);
    			break;
    			
    		case 'n': // For Mouse Scroll Up
    			if(eventcount%2 == 1) { 
    			break;  
    			}
    			// getXCUISource(sessionId);
                 scrollFlag = 0;
                memset(XCUILabel,0x00,sizeof(XCUILabel));
                 json_object *jobp = NULL;
                char elementId[64];
                isXCUILabelFound = 0;
                printf("is XCUILabel Found %s", isXCUILabelFound);
                jobj = json_object_new_object();
                formJsonObject(jobj,"using","class name");
                formJsonObject(jobj,"value","XCUIElementTypeApplication");
                printf ("The json object created: %s \n",json_object_to_json_string(jobj));
                getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
                printf("ElementId %s",elementId);
                jobp = json_object_new_object();
                formJsonObject(jobp,"direction","up");
                sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/element/%s/swipe",httpEndPointPrefix,xPort,sessionId,elementId);
                postHttpData(httpEndPointUrl,json_object_to_json_string(jobp));
                json_object_put(jobj);
               

                            
            break;
                
            case 'N':
                if(eventcount%2 == 1) { 
    			break;  
    			}
               // For Mouse Scroll Down
                scrollFlag = 0;
                memset(XCUILabel,0x00,sizeof(XCUILabel));
                isXCUILabelFound = 0;            
                printf("is XCUILabel Found %s", isXCUILabelFound);
                jobj = json_object_new_object();            
                formJsonObject(jobj,"using","class name");            
                formJsonObject(jobj,"value","XCUIElementTypeApplication");                        
                printf ("The json object created: %s \n",json_object_to_json_string(jobj));            
                getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);            
                printf("ElementId %s",elementId);            
                jobp = json_object_new_object();
                formJsonObject(jobp,"direction","down");            
                sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/element/%s/swipe",httpEndPointPrefix,xPort,sessionId,elementId);
                postHttpData(httpEndPointUrl,json_object_to_json_string(jobp));
                json_object_put(jobj);
               

                
            break;	
    case 'P':
          /*  getXCTestSession(sessionId);
            if(currentScaleFactor)
	    	setScaleFactor(currentScaleFactor);*/
        jobj = json_object_new_object();
        if(height == 0.0){
	    getXCTestSession(sessionId);
            getWindowSize(sessionId);
	}
        formJsonObject(jobj,"orientation","PORTRAIT");
        /*Now printing the json object*/
        printf ("The json object created: %s\n",json_object_to_json_string(jobj));
        sprintf(httpEndPointUrl,"%s:%s/session/%s/orientation",httpEndPointPrefix,xPort,sessionId);
        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
        json_object_put(jobj);
	scrollFlag = 0;
	UIorientation = 1; 
        break;

    case 'L':
		/*getCurrentScaleFactor();
		setScaleFactor(100);*/
        jobj = json_object_new_object();
        formJsonObject(jobj,"orientation","LANDSCAPE");
        /*Now printing the json object*/
        printf ("The json object created: %s\n",json_object_to_json_string(jobj));
        sprintf(httpEndPointUrl,"%s:%s/session/%s/orientation",httpEndPointPrefix,xPort,sessionId);
        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
        json_object_put(jobj);
	scrollFlag = 0;
	UIorientation = 3;
        break;
    case 'U':
        blockSettings = 0;
        break;
    case 'B':
        blockSettings = 1;
        break;
    case 'p':				/*pinch In*/
        ptr = strtok(eventString+2," ");
        if(ptr!=NULL)
        {
            sprintf( xval,"%s", ptr);
            ptr = strtok(NULL, " ");
            sprintf(yval, "%s", ptr);
            ptr = strtok(NULL, " ");
            sprintf(toX, "%s", ptr);
            ptr = strtok(NULL, " ");
            sprintf(toY, "%s", ptr);
            
        }
        jobj = json_object_new_object();
        formJsonObject(jobj,"x",xval);
        formJsonObject(jobj,"y",yval);
        formJsonObject(jobj,"sizeX","50");
        formJsonObject(jobj,"sizeY","50");
        formJsonObject(jobj,"velocity","1.0");
        formJsonObject(jobj,"scale","2.0");
        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/element/0/pinch",httpEndPointPrefix,xPort,sessionId);
        printf("pinch : %s\n",json_object_to_json_string(jobj));
        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
        json_object_put(jobj);
        break;
    case 'q':				/*pinch Out*/
        ptr = strtok(eventString+2," ");
        if(ptr!=NULL)
        {
            sprintf( xval,"%s", ptr);
            ptr = strtok(NULL, " ");
            sprintf(yval, "%s", ptr);
            ptr = strtok(NULL, " ");
            sprintf(toX, "%s", ptr);
            ptr = strtok(NULL, " ");
            sprintf(toY, "%s", ptr);
            
        }
        jobj = json_object_new_object();
        formJsonObject(jobj,"x",xval);
        formJsonObject(jobj,"y",yval);
        formJsonObject(jobj,"sizeX","50");
        formJsonObject(jobj,"sizeY","50");
        formJsonObject(jobj,"velocity","1.0");
        formJsonObject(jobj,"scale","0.25");
        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/element/0/pinch",httpEndPointPrefix,xPort,sessionId);
        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
        json_object_put(jobj);
        break;
    }

}

char getXCTestSession(char *session)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/status",xPort);
    struct curl_fetch_st chunk;
    if(!getHttpData(httpEndPointUrl,&chunk,0L))
    {
        char payload[1024] = {0};
        strcpy(payload,p->payload);
        printf("payload: \n%s\n",payload);
        json_object *new_obj;
        new_obj = json_tokener_parse(payload);
        if(new_obj != NULL)
        {
            //printf("The Session ID is %s\n", json_object_get_string(json_object_object_get(new_obj, "sessionId")));
            sprintf(sessionId,"%s",json_object_get_string(json_object_object_get(new_obj, "sessionId")));
            printf("%s\n",sessionId);
	    printf("Length of sessionid  = %zu \n",strlen(sessionId));
	    if(strlen(sessionId) < 10){
		printf("Sessios id getting null \n"); return -1;
		}
        }
	
        json_object_put(new_obj);
            
        return 0;
    }
        return -1;
}

char getWindowSize(char *session)
{	
    if (strlen(session) < 10){
	return -1;
     }
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/session/%s/window/size",xPort,sessionId);
    struct curl_fetch_st chunk;
    if(!getHttpData(httpEndPointUrl,&chunk,0L))
    {
        char payload[1024] = {0};
        strcpy(payload,p->payload);
        printf("payload: \n%s\n",payload);
        json_object *new_obj;
        new_obj = json_tokener_parse(payload);
        char value[1024] = {0};
        char h[8] = {0}, w[8]={0}, o[4]={0};
        if(new_obj != NULL)
        {
            sprintf(value,"%s",json_object_get_string(json_object_object_get(new_obj, "value")));
            printf("%s \n",value);
        }
        new_obj = json_tokener_parse(value);               
        if(new_obj != NULL)
        {
            sprintf(h,"%s",json_object_get_string(json_object_object_get(new_obj,"height")));
            height = atof(h);
	    sprintf(w,"%s",json_object_get_string(json_object_object_get(new_obj,"width")));
            width = atof(h);
           // sprintf(o,"%s",json_object_get_string(json_object_object_get(new_obj,"orientation")));
          //  UIorientation = atof(o);
        }
        json_object_put(new_obj);
	    scrollFlag = 0;
        return 0;
    }
    return -1;
}

char getCurrentScaleFactor()
{
    char httpEndPointUrl[256] = {0};
    char mjpegScalingFactor[128] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/session/%s/appium/settings",xPort,sessionId);
    struct curl_fetch_st chunk;
    if(!getHttpData(httpEndPointUrl,&chunk,0L))
    {
        char payload[1024] = {0};
        strcpy(payload,p->payload);
        printf("payload: \n%s\n",payload);
        json_object *new_obj;
        new_obj = json_tokener_parse(payload);
        if(new_obj != NULL)
        {
            //printf("The Session ID is %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
            new_obj = json_object_object_get(new_obj,"value");

            sprintf(mjpegScalingFactor,"%s",json_object_get_string(json_object_object_get(new_obj, "mjpegScalingFactor")));
            printf("mjpegScalingFactor : %s\n",mjpegScalingFactor);
	    currentScaleFactor = atoi(mjpegScalingFactor);
        }
        json_object_put(new_obj);
        return 0;
    }
    return -1;

}


char setScaleFactor(char scaleFactor)
{
	    char httpEndPointUrl[256] = {0};
	    char *kbBuffer=NULL;
            kbBuffer = (char*)malloc(128);
	    json_object *jobj;
            jobj = json_object_new_object();
            //formJsonObject(jobj,"value",ptr);
            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(kbBuffer,"{ \"settings\": {\"mjpegScalingFactor\":%d} }",scaleFactor);
            printf("kbBuffer %s\n",kbBuffer);
            sprintf(httpEndPointUrl,"http://localhost:%s/session/%s/appium/settings",xPort,sessionId);
            //postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            postHttpData(httpEndPointUrl,kbBuffer);
            json_object_put(jobj);

}

char getCurrentApp(char **AppBundleId)
{
    char httpEndPointUrl[256] = {0};
    //char AppBundleId[128] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/wda/activeAppInfo",xPort);
    struct curl_fetch_st chunk;
    if(!getHttpData(httpEndPointUrl,&chunk,2L))
    {
        char *payload = (char *)malloc(8192);
        strcpy(payload,p->payload);
        printf("payload: \n%s\n",payload);
        json_object *new_obj;
        new_obj = json_tokener_parse(payload);
        if(new_obj != NULL)
        {
            //printf("The Session ID is %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
            new_obj = json_object_object_get(new_obj,"value");
	    sprintf(*AppBundleId,"%s",json_object_get_string(json_object_object_get(new_obj, "bundleId")));
            printf("AppBundleId : %s\n",*AppBundleId);
        }
        json_object_put(new_obj);
        free(payload);
	      payload = NULL;
        return 0;
    }
    return -1;

}
	

char getImageDumpPath()
{
	char httpEndPointUrl[256] = {0};
    printf("xport %s\n",xPort);
    sprintf(httpEndPointUrl,"%s:%s/connect",deviceServer,xPort);
    struct curl_fetch_st chunk;
    if(!getHttpData(httpEndPointUrl,&chunk,0L))
	{
        char payload[1024] = {0};
        strcpy(payload,p->payload);
		printf("payload: \n%s\n",payload);
		json_object *new_obj;
		new_obj = json_tokener_parse(payload);
        if(new_obj != NULL)
		{
            //printf("The image path is %s\n", json_object_get_string(json_object_object_get(new_obj, "img_path")));
			sprintf(imgPath,"%s",json_object_get_string(json_object_object_get(new_obj, "img_path")));
			//printf("imgPath %s\n",imgPath);
			strcat(imgPath,".png");
			printf("%s\n",imgPath);
			
		}
        json_object_put(new_obj);
        return 0;
	}
    return -1;
}
    
char isApp(char *AppName, char *session,int x, int y)
{
    // printf("AppName %s\n",AppName);
    if(sourcePayload != NULL)
    {
        if(strlen(sourcePayload)>0)
        {
            //printf("sourcePayload %s\n",AppName);
            json_object *new_obj;
            new_obj = json_tokener_parse(sourcePayload);
            if(new_obj != NULL)
            {
                char ret;
                //printf("sourcePayload len %d\n", strlen(sourcePayload));
                char *xmlSource = malloc(strlen(sourcePayload));
                //printf("value: %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
                sprintf(xmlSource,"%s",json_object_get_string(json_object_object_get(new_obj, "value")));
                ret = parseXMLForAppType(xmlSource,AppName, x, y);
                free(xmlSource);
                xmlSource = NULL;
                json_object_put(new_obj);
                return ret;
            }
            /*if(sourcePayload!=NULL)
            {
                free(sourcePayload);
                sourcePayload = NULL;
            }*/
            return 0;
        }
    }

}

char isAlertPresent(char *session)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/session/%s/alert/text",xPort,session);
    //char value[1024] = {0};
    struct curl_fetch_st chunk;
    if(!getHttpData(httpEndPointUrl,&chunk,0L))
    {
        //char payload[1024] = {0};
        if(p!=NULL)
        {
            if(p->size > 0)
            {
                char *payload = (char *)malloc(p->size);
                strcpy(payload,p->payload);
                // printf("alert response payload: \n%s\n",payload);
                json_object *new_obj;
                new_obj = json_tokener_parse(payload);
                if(new_obj != NULL)
                {
                    //printf("The value is %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
                    if(json_object_get_string(json_object_object_get(new_obj, "value")) != NULL)
                    {
                        //sprintf(value,"%s", json_object_get_string(json_object_object_get(new_obj, "value")));
                        //printf("%s %d \n",value, strlen(json_object_get_string(json_object_object_get(new_obj, "value"))));
                        if(strlen(json_object_get_string(json_object_object_get(new_obj, "value"))) > 3)
                        {
                            printf("ALERT PRESENT\n");
                            json_object_put(new_obj);
                            if(payload != NULL){
                                free(payload);
                                payload = NULL;
                            }
                            isAlert = 1;
                            return 1;
                        }
                    }
                }
                if(payload != NULL){
                    free(payload);
                    payload = NULL;
                }
            }
            return 0;
        }
    }
    return -1;
}
    
char getXCUISource(char *session)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/source",xPort);
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
            printf("source payload size = %d\n",p->size);
        }
        return 1;
    }
    return 0;
}

void getElementLabelFromSource(char *session, int x, int y)
{
    if(strlen(sourcePayload)>0)
    {
        json_object *new_obj;
        new_obj = json_tokener_parse(sourcePayload);
        if(new_obj != NULL)
        {
            //printf("value: %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
            sprintf(sourcePayload,"%s",json_object_get_string(json_object_object_get(new_obj, "value")));
            parseXMLTree(sourcePayload, x, y);
            json_object_put(new_obj);
        }
        free(sourcePayload);
        sourcePayload = NULL;
        return;
    }
    return;
}

void getElementIDFromLabel(char *session, char *labelData, char *elementId)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/session/%s/elements",xPort,session);
    struct curl_fetch_st chunk;
    if(!postHttpDataCB(httpEndPointUrl,labelData,&chunk))
    {
        if(p!=NULL)
        {
            //printf("%s %d\n", __FUNCTION__, p->size);
            char *payload = (char *)malloc(p->size);
            strcpy(payload,p->payload);
            //printf("payload: \n%s\n",payload);
            json_object *new_obj;
            new_obj = json_tokener_parse(payload);
            if(new_obj != NULL)
            {
                //new_obj = json_object_object_get(new_obj,"value");
                if(strcmp(json_object_get_string(json_object_object_get(new_obj, "value")),"Alert is obstructing view") != 0)
                {   
                    new_obj = json_object_object_get(new_obj,"value");
                    new_obj = json_object_array_get_idx(new_obj, 0);
                    //printf("value: %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
                    //printf("Element: %s\n", json_object_get_string(json_object_object_get(new_obj, "ELEMENT")));
                    if(json_object_get_string(json_object_object_get(new_obj, "ELEMENT")) != NULL)
                    {
                        sprintf(elementId,"%s",json_object_get_string(json_object_object_get(new_obj, "ELEMENT")));
                        printf("elementId %s\n",elementId);
                    }
                }
                json_object_put(new_obj);
            }
            free(payload);
            return;
        }
    }
    return;
}

char ifFilteredApp(char *session)
{
    if(isFilteredApp)
        return YES;
    else
        return NO;

    char elementId[64] = {0}, tempLabel[32] = {0};
    char httpEndPointUrl[256] = {0};
    char httpEndPointPrefix[] = "http://localhost";
    struct curl_fetch_st chunk;
    json_object *jobj = json_object_new_object();
    formJsonObject(jobj,"using","class name");
    //sprintf(tempLabel,"label=%s",XCUILabel);
    formJsonObject(jobj,"value","XCUIElementTypeApplication");
    /*Now printing the json object*/
    //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
    getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
    if(strlen(elementId)>0)
    {
        json_object_put(jobj);
        sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/attribute/name",httpEndPointPrefix,xPort,sessionId, elementId);
        if(!getHttpData(httpEndPointUrl,&chunk,2L))
        {
            if(p->size > 0){
                char payload[1024] = {0};
                char appName[1024] = {0};
                strcpy(payload,p->payload);
                //printf("payload: \n%s\n",payload);
                json_object *new_obj;
                new_obj = json_tokener_parse(payload);
                if(new_obj != NULL)
                {
                    //printf("The appName is %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
                    if(json_object_get_string(json_object_object_get(new_obj, "value")) != NULL)
                    {
                        sprintf(appName,"%s",json_object_get_string(json_object_object_get(new_obj, "value")));
    				//printf("%s\n",appName);
                        json_object_put(new_obj);
                        if((strcmp(appName,"App Store")==0)||(strlen(appName)<= 1) || (strcmp(appName,"Settings")==0))
                            return 1;
                    }
                    if(new_obj!=NULL)
                        json_object_put(new_obj);
                }
            }
            return 0;
        }
    }
    return 0;
    
}
void terminateIfSettingsIsOpen (int signum)
{
    char *curBundleId = (char *)malloc(128);
    getCurrentApp(&curBundleId);
    if(!strcmp(curBundleId,"com.apple.Preferences"))
    {
        printf("bundle : %s\n", curBundleId);
/*  char *fe = (char*) malloc(4);
            if (fe)
            {
                sprintf(fe,"H \n");
#ifndef XCUITEST
                pserver_state.processScreen = 0;
                thread_queue_add(pserver_state.queue,fe,1);
#else
                thread_queue_add(pserver_state.queue,fe,1);
#endif
            }
*/
	char httpEndPointPrefix[] = "http://localhost";
        char httpEndPointUrl[256] = {0};
        json_object *jobj = json_object_new_object();
        formJsonObject(jobj,"bundleId","com.apple.Preferences");
        printf ("The json object created: %s\n",json_object_to_json_string(jobj));
        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/apps/terminate",httpEndPointPrefix,xPort,sessionId);
        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));

    }
    free(curBundleId);
}
void* XCUIPopHandleThread(void* ptr)
{
    mutex_init(&XCUImutex);
    while(1)
    {
        if(!isAlert)
        {
            if(pserver_state.no_clients)
            {
                 mutex_lock(&XCUImutex);
                 if(isAlertPresent(sessionId))
                 {
                      isAlert = 1;
                      getXCUISource(sessionId);
                 }
            else
                isAlert = 0;
            mutex_unlock(&XCUImutex);
            }
        }
        sleep(1);
    }
    
}

void* ProcessEventsThread(void* ptr)
{
    struct threadmsg msg;
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 250000000;
    char *std_out = NULL;
    //char sessionId[64] = {0};
#ifndef XCUITEST
#else	
    if(getXCTestSession(sessionId)<0)
    {
        sleep(10);
        getXCTestSession(sessionId);
    }
    if(getWindowSize(sessionId)<0)
    {
        sleep(10);
        getWindowSize(sessionId);
    }
#endif
    while(1)
    {
        if (thread_queue_get(pserver_state.queue,&timeout,&msg)==0)
        {
            processEvents(msg.data,sessionId);
            //std_out = (char*)msg.data;

        }
        timer_handler(0);
        usleep(2500);
    }

}
#endif
void* ProcessCheckFdsHandler(void* pserver)
{
    pServerState_t * server = (pServerState_t *) pserver;
    struct threadmsg msg;
    instruments_client_t client;
    struct QData* qData = NULL;

    if(server != NULL)
    {
        rfbScreenInfoPtr screen = (rfbScreenInfoPtr)(server->screen);

        while(!server->please_exit && screen)
        {
            rfbCheckFds(screen,3000);
#if 0
            //fprintf(stderr, "Processing events\n");
            if(server->no_clients > 0)
                rfbProcessCheckFds(screen, screen->deferUpdateTime*100);
            else
                rfbProcessCheckFds(screen, screen->deferUpdateTime*1000000);
#endif
        }
    }
}

void print_usage(int argc, char **argv)
{
    char *name = NULL;

    name = strrchr(argv[0], '/');
    printf("Usage: %s [OPTIONS]\n", (name ? name + 1: argv[0]));
    printf("Start pserver\n");
    printf("NOTE: A mounted developer disk image is required on the device\n");
    printf("  -P, --port\t\tPort on which to start pserver\n");
    printf("  -s, --scale\t\tScale 50 for half width and height \n");
    printf("  -u, --udid UDID\ttarget specific device by its 40-digit device UDID\n");
    printf("  -h, --help\t\tprints usage information\n");
#ifndef OS_11_0
    printf("  -b, --bundleid\t\tBundle id of device\n");
#else
#endif
#ifndef XCUITEST
#else
    printf("  -x, --xPort\t\tLocal Port on which to xctest is listening\n");
#endif
    printf("\n");
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





