

/**
 * pCloudy.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <rfb/rfb.h>
#include <sys/ioctl.h>
#include "pqueue.h"
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <json.h> 
#include "pevent.h"
#include "curlApi.h"
#include <libimobiledevice/libimobiledevice.h>


#include <sys/file.h>

void print_usage(int argc, char **argv);
void check(int sig);
void* ProcessEventsThread(void*);
void processEvents(char *msg, char *sessionId);
char getXCTestSession(char *session);
char getWindowSize(char *session);
char getCurrentApp(char **currentApp);
void terminateIfSettingsIsOpen (int signum);
char isAlertPresent(char *session);
// char isApp(char *AppName,char *session,int x, int y);
// char ifFilteredApp(char *session);
char getXCUISource(char *session);
void getElementLabelFromSource(char *session, int x, int y);
void getElementIDFromLabel(char *session, char *labelData, char *elementId);
void* ReadClientData(void* sockfd);
int sockfd;
char *sourcePayload = NULL;
char sessionId[64] = {0};
char isAlert = 0;
char isFilteredApp = NO;
extern struct curl_fetch_st *p;
extern char isXCUILabelFound;
extern char XCUILabel[1024];
extern char processing;
//static mutex_t XCUImutex;
char blockSettings = 1;
int height = 0 ;
int width = 0;
int UIorientation = 1;
int scrollFlag = 0;
char *temp1;
char *temp2;
char *bundleId;

/* 15 frames per second (if we can) */
// #define PICTURE_TIMEOUT (1.0/15.0)

sig_atomic_t signaled = 0;
pthread_t eventFdThread = NULL; /*XCUITEST command processing thread id*/
pServerState_t pserver_state;




char b_id[50] = "com.pcloudy.ialice";
char helperb_id[50] = "com.pcloudy.pcloudyhelper";

char xPort[8] = "8100"; /*Default port for XCUITTEST webserver*/

void intHandler(int param) {
    signaled = 1;
    exit(0);
}

#define MAX 4196 
void* ReadClientData(void *sockfdptr)
{
        int sockfd = *(int*)sockfdptr;
    char *buff[MAX];
    char *fe2 = (char*) malloc(MAX);
    char *fe1 = (char*) malloc(MAX);
    char *temp3[MAX];
    int i = 0;
    int n;
    printf("ReadClientData thread created\n");
    for (;;)
    {
        bzero(buff, MAX);
            n = read(sockfd, buff, sizeof(buff));
        if(n>0)
        {
                printf("%d Bytes From client: %s  ",strlen(buff),buff);
                char *find = NULL;
                find = strstr(buff,"undefined");
		char *temp = buff;
		temp = strstr(buff,"U");
		if((temp != NULL) && (temp == "U"))
			blockSettings = 0;
		temp = strstr(buff,"K");
		if((temp != NULL) && (strlen(buff) > 2))
		{
	        if(find == NULL)
            {
        	    //sprintf(fe2,"%s",buff);
                if(!processing)
                {
				    printf("Inside if and adding to queue %s------\n",fe2);	
                    printf("i %d \n",i);
                    if(i == 1)
                    {
                        temp2 = strtok(buff,"K");
                        temp2 = temp2+1;
                        strcat(temp3,temp2);
                        printf("inside temp3 condition temp3 - %s\n",temp3);
                        //strcpy(buff,temp3);
                        printf("buffer %s \n",temp3);
                        sprintf(fe2,"%s",temp3);
                        printf("assigning the thread \n");
                        thread_queue_add(pserver_state.queue,fe2,1);
                        i = 0;
                    }
                    else
                    {
                        sprintf(fe2,"%s",buff);
                        thread_queue_add(pserver_state.queue,fe2,1);
                    }                        
                }
                else
                {
                        if(i == 0)
                    {

                        printf("Temp3 in else if 1 %s \n", strcpy(temp3,buff));
                        //sprintf(fe2,"%s",buff);
                        //thread_queue_add(pserver_state.queue,fe2,1);
                        i = 1;
                    }
                    else
                    {             
                        temp2 = strtok(buff,"K");
                        printf("length of buffer %d \n string in buff %s \n ",strlen(buff),buff);
                        temp2 = temp2+1;
    				    printf(" temp2 %s \n ", temp2);
                        if(temp2 != NULL)	
                        printf("Temp3 in else if  %s \n", strcat(temp3,temp2));
                    }

                    if(!processing)
                    {
                        sprintf(fe2,"%s",temp3);
                        thread_queue_add(pserver_state.queue,fe2,1);
                        i = 1;
                    }
                        

                }
                /*else if (temp != NULL)
                {
                    
                    printf("string length of temp3 %d \n",temp3);
                    printf("string in temp3 %s \n",temp3);
                    printf("i %d \n",i);
                    
                        printf("adding events to buffer \n");
                         if(i == 0)
                    {

                        printf("Temp3 in else if 1 %s \n", strcpy(temp3,buff));
                        //sprintf(fe2,"%s",buff);
                        //thread_queue_add(pserver_state.queue,fe2,1);
                        i = 1;
                    }
                    else
                    {                            
                        temp2 = strtok(buff,"K");
                        printf("length of buffer %d \n string in buff %s \n ",strlen(buff),buff);
                        temp2 = temp2+1;
    				    printf(" temp2 %s \n ", temp2);
                        if(temp2 != NULL)	
                        printf("Temp3 in else if  %s \n", strcat(temp3,temp2));
                        
                    }
                    

                   // sprintf(fe2,"%s",temp3);
                    //thread_queue_add(pserver_state.queue,fe2,1);   

                       /* for(;;)
                        {
                            if(!processing)
                            {
                                printf("Inside loop \n");
                                //usleep(1000);
                                sprintf(fe2,"%s",temp3);
                                thread_queue_add(pserver_state.queue,fe2,1);
                                i = 0;
                                break;
                            }
                        }*/
                        
                        
                //}
                //}
            }        
		}
		else if(temp == NULL)
		{
            if(i == 1)
            {
                i = 0;
            }
			if(find == NULL)
            {
                sprintf(fe2,"%s",buff);
                if(!processing)
                {
                    thread_queue_add(pserver_state.queue,fe2,1);
                }
             }		
		}
    }
	else if(n == 0){
		printf("client disconnected\n");
		break;
		thread_cancel(pthread_self());
	}
        usleep(100);
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


#endif



int main(int argc,char** argv)
{
    signal(SIGINT, intHandler);
    signal(SIGKILL, intHandler);
    signal(SIGHUP, intHandler);
    signal(SIGILL, intHandler);
    int vnc_port=5900;
    int i;
    const char *udid = NULL;
    char *filename = NULL;
    pserver_state.scaling=100;
    pserver_state.please_exit=0;
    pserver_state.srvSocket = -1;
    int new_socket;

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
            //printf("server socket create failed \n");
            pserver_state.please_exit = 1;
        }
        //printf("server socket create %d \n",pserver_state.srvSocket);
    }


    if (!pserver_state.please_exit)
    {
        printf( " piserver version 2.1.1\n");
        printf("vnc_port %d\n",vnc_port);
        int one = 1;
        memset(&pserver_state.srvSocketAddr, 0, sizeof(struct sockaddr_in));
        pserver_state.srvSocketAddr.sin_family = AF_INET;
        pserver_state.srvSocketAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        pserver_state.srvSocketAddr.sin_port = htons(vnc_port);
        printf("port %d \n ",pserver_state.srvSocketAddr.sin_port);
        if ( setsockopt(pserver_state.srvSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) < 0 )
        {
            printf("set sock opt failed \n");
            
        }
        
        if ( bind(pserver_state.srvSocket, (struct sockaddr *)&pserver_state.srvSocketAddr, sizeof(pserver_state.srvSocketAddr)) < 0)
        {
            printf("bind failed \n");
            pserver_state.please_exit = 1;
            close(pserver_state.srvSocket);
            exit(0);
        }
    }
    if (!pserver_state.please_exit)
    {
        if (listen(pserver_state.srvSocket,10) != 0)
        {
            printf("listen failed \n");
            pserver_state.please_exit = 1;
        }
        fprintf(stderr," Listening for VNC connections on TCP port %d \n",vnc_port);
	    
        thread_queue_init(pserver_state.queue);
        if(thread_new(&eventFdThread,ProcessEventsThread,NULL))
            {
                pserver_state.please_exit = 1;
                fprintf(stderr, "ProcessEventsThread could not be created\n");
                exit;
            
            }
    }
    int len = 0;
    while(1)
    {
	    memset(&pserver_state.srvSocketAddr, 0, sizeof(struct sockaddr_in));
        new_socket = accept(pserver_state.srvSocket,(struct sockaddr * )&pserver_state.srvSocketAddr,&len);
        if(new_socket < 0)
        {    
            printf("the client is connection failed .... ... \n");
            exit;
        }
        else
        {    
            printf("the client is connected .....\n");
	        pthread_t FdThread = NULL;
	        printf("new_socket: %d\n",new_socket);
	        int socket = new_socket;
	        if(thread_new(&FdThread,ReadClientData,(void*)&socket))
            {
                pserver_state.please_exit = 1;
                fprintf(stderr, "ReadClientDataThread could not be created\n");
                exit;

            }
	    //thread_queue_add(pserver_state.queue,'P',1);
            usleep(10);
        }  
    }
    close(pserver_state.srvSocket);

    if(filename)
        free(filename);
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
#endif
void processEvents(char *msg, char *session)
{
    char xval[8] = {0};
    char yval[8] = {0};
    char toX[8] = {0};
    char toY[8] = {0};
    char *ptr = NULL;
    char httpEndPointPrefix[] = "http://localhost";
    char httpEndPointUrl[1024] = {0};
    char *eventString=NULL;
    char orientation[8] = {};
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
    //printf("*************** eventString: %s sessionId: %s\n",eventString, sessionId);
    int len = strlen(eventString);
    if(strlen(sessionId)<=0)
        getXCTestSession(sessionId);


    switch(eventString[0])
    {
        case 'T':				/*TAP*/
            printf("tap case \n");
            ptr = strtok(eventString+2," ");
            if(ptr!=NULL)
            {
                    sprintf( xval,"%s", ptr);
                    ptr = strtok(NULL, " ");
                    sprintf(yval, "%s", ptr);
               
            }
            #if 0
            
            #endif
	    getXCTestSession(sessionId);
            jobj = json_object_new_object();
	        jobj1 = json_object_new_object();
            jobj2 = json_object_new_object();
            jobj3 = json_object_new_object();
            jobj4 = json_object_new_object();
            jobj5 = json_object_new_object();
            jobj6 = json_object_new_object();

	    #if defined OS_14_0
                if(UIorientation == 3)
                {
                    char *curBundleId = (char *)malloc(128);
                    
                    isAlertPresent(sessionId); 
                        if(isAlert)
                        {
                            getXCUISource(sessionId);
                            memset(XCUILabel,0x00,sizeof(XCUILabel));
                            isXCUILabelFound = 0;
                        }
                        if(isAlert)
                        {
                            memset(XCUILabel,0x00,sizeof(XCUILabel));
                            isXCUILabelFound = 0;
                            //mutex_lock(&XCUImutex);
                            printf("bundleId : %s AppBundleId  : %s\n\n",bundleId,curBundleId);
                            if(bundleId != NULL)
                                curBundleId = bundleId;
                            if(!(strcmp(curBundleId,"com.apple.springboard")))
                            {                                         
                                printf("AppBundleId : %s\n",curBundleId);
                                isAlert = 1;
                                printf("isAlert = %d \n",isAlert);

                            }
                            else
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
                                    //mutex_unlock(&XCUImutex);
                                    break;
                                }
                                else
                                {
                                    char elementId[64] = {0}, tempLabel[32] = {0};
                                    jobj = json_object_new_object();
                                    formJsonObject(jobj,"name",XCUILabel);
                                    /*Now printing the json object*/
                                    printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                                    sprintf(httpEndPointUrl,"%s:%s/session/%s/alert/accept",httpEndPointPrefix,xPort,sessionId);
                                    postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
                                    json_object_put(jobj);
                                    //mutex_unlock(&XCUImutex);
                                    break;
                                }
                            }  
                        }
                    
                    getCurrentApp(&curBundleId);
                    bundleId = curBundleId;
                    printf("AppBundleId : %s  And isAlert = %d \n  bundleId = %s",curBundleId,isAlert,bundleId);
                    if((!(strcmp(curBundleId,"com.apple.springboard")))) 
                    {
                        //if(isAlert)
                        //{
			    printf("AppBundleId is alert : %s \n",curBundleId);
                            int y = atoi(xval);
		                    formJsonObject(jobj,"x",yval);
		                    y = height-y;
		                    sprintf(xval,"%d",y);
                            formJsonObject(jobj,"y",xval);
                            isAlert = 0;
                        //}
                        /*else
                        {
			    printf("AppBundleId no alert : %s \n",curBundleId);
                            int y = atoi(xval);
		                    formJsonObject(jobj,"x",yval);
		                    y = height-y;
		                    sprintf(xval,"%d",y);
                            formJsonObject(jobj,"y",xval);
                            isAlert = 0;
                        }*/
                        
                    }
                    else
                    {
                        
                        {
			    printf("AppBundleId not home screen : %s \n",curBundleId);
                            int y = atoi(xval);
		                    formJsonObject(jobj,"x",yval);
		                    y = width-y;
		                    sprintf(xval,"%d",y);
                            formJsonObject(jobj,"y",xval);
                            isAlert = 0;

                        }
                        
                    }
                }
                
	            else
		        {
		            formJsonObject(jobj,"x",xval);
                    formJsonObject(jobj,"y",yval);
		        }
            	array = json_object_new_array();

	            formJsonObject(jobj1,"action","press");
       	        json_object_object_add(jobj1,"options",jobj);

                formJsonObject(jobj2,"action","wait");
                json_object_object_add(jobj2,"options",jobj3);
                formJsonObject(jobj3,"ms","10");
                formJsonObject(jobj4,"action","release");

           	    json_object_array_add(array,jobj1);
                json_object_array_add(array,jobj2);
                json_object_array_add(array,jobj4);
                json_object_array_add(array,jobj);


            	json_object_object_add(jobj6,"actions",array);

            	printf ("The json object created: %s\n",json_object_to_json_string(jobj6));
            	sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/touch/perform",httpEndPointPrefix,xPort,sessionId);
            	postHttpData(httpEndPointUrl,json_object_to_json_string(jobj3));

        #else
            sprintf(orientation,"%d",UIorientation);
            formJsonObject(jobj,"x",xval);
            formJsonObject(jobj,"y",yval);
            formJsonObject(jobj,"orientation",orientation);
            /*Now printing the json object*/
            printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/tap/0",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));

	    #endif
            printf("\n");
            json_object_put(jobj);
            getXCTestSession(sessionId);
	    #if !defined  OS_13_4
            if(blockSettings)
	    {   
		  sleep(1);            
   		  terminateIfSettingsIsOpen(0);
	    }
            #endif
            break;

        case 't':				/*Tap and hold*/
            printf("tap and hold case \n");
            ptr = strtok(eventString+2," ");
            if(ptr!=NULL)
            {
                sprintf( xval,"%s", ptr);
                ptr = strtok(NULL, " ");
                sprintf(yval, "%s", ptr);          
            }
            jobj = json_object_new_object();
            sprintf(orientation,"%d",UIorientation);
            formJsonObject(jobj,"x",xval);
            formJsonObject(jobj,"y",yval);
            formJsonObject(jobj,"duration","3.0");
            formJsonObject(jobj,"orientation",orientation);
            /*Now printing the json object*/
            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/touchAndHold",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            json_object_put(jobj);
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
                printf("Keyboard Case -----  %s \n",ptr);
                //sprintf( xval,"%s", ptr);
                char kbBuffer[4196]={0};
                jobj = json_object_new_object();
                formJsonObject(jobj,"value",ptr);
                if(ptr == "\r")
                {
                    ptr = "\n";
                }
                printf ("The json object created: %s\n",json_object_to_json_string(jobj));
                sprintf(kbBuffer,"{ \"value\": [\"%s\"] }",ptr+1);
                printf("kbBuffer %s\n",kbBuffer);
                sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/keys",httpEndPointPrefix,xPort,sessionId);
                postHttpData(httpEndPointUrl,kbBuffer);
                json_object_put(jobj);
            }
            break;
	case 'U':
        	blockSettings = 0;
        	break;
    	case 'B':
        	blockSettings = 1;
        	break;
        case 'S':				/*Swipe/DragFromTo*/
            printf("Swipe case \n");
	   getXCTestSession(sessionId);
            ptr = strtok(eventString+2," ");
            jobj = json_object_new_object();
        jobj1 = json_object_new_object();
        jobj2 = json_object_new_object();
        jobj3 = json_object_new_object();
        jobj4 = json_object_new_object();
        jobj5 = json_object_new_object();
        jobj6 = json_object_new_object();
        jobj7 = json_object_new_object();

            if(ptr!=NULL)
            {
                    int x,y,x1,y1;
                    sprintf( xval,"%s", ptr);
                    ptr = strtok(NULL, " ");
                    sprintf(yval, "%s", ptr);
                    ptr = strtok(NULL, " ");
                    sprintf(toX, "%s", ptr);
                    ptr = strtok(NULL, " ");
                    sprintf(toY, "%s", ptr);
                    x = atoi(xval);
                    x1 = atoi(toX);
                    y = atoi(yval);
                    y1 = atoi(toY);
                    
		            #if defined OS_14_0
                        array = json_object_new_array();

                        formJsonObject(jobj2,"action","press");
                        json_object_object_add(jobj2,"options",jobj);
	                    if(UIorientation == 3)
        	            {
                            int Xval = atoi(xval);
			                sprintf(xval,"%d",Xval);
			                int toXval = atoi(toX);
    			            sprintf(toX, "%d", toXval);		            
                            formJsonObject(jobj,"x",yval);
                            formJsonObject(jobj,"y",xval);
                            json_object_array_add(array,jobj2);

                            formJsonObject(jobj3,"action","wait");
                            json_object_object_add(jobj3,"options",jobj4);
                            formJsonObject(jobj4,"ms","60");
                            json_object_array_add(array,jobj3);

                            formJsonObject(jobj5,"action","moveTo");
                            json_object_object_add(jobj5,"options",jobj6);
                            if (abs(x - x1) > abs(y - y1)){
                                printf("Swipe up/down.............\n");
                                if (x > x1){
                                    printf("Swipe up.............\n");
                                    int toYval = atoi(yval);
                                    sprintf(toY,"%d",(toYval));
                                    int toXval = atoi(xval);
                                    sprintf(toX, "%d", (toXval+50));
                                    

                
                                }
                                if (x < x1){
                                    printf("Swipe down.............\n");
                                    int toYval = atoi(yval);
                                    sprintf(toY,"%d",(toYval));
                                    int toXval = atoi(xval);
                                    sprintf(toX, "%d", (toXval-50));
                                    
                                }
                            }
                            else
                            {
                                printf("Swipe left/right.............\n");
                                if (y > y1){
                                    printf("Swipe right.............\n");
                                    int toYval = atoi(yval);
                                    //sprintf(toY,"%d",(toYval-50));
                                    int toXval = atoi(xval);
                                    sprintf(toX, "%d", (toXval));
                                    
                                }
                                if (y < y1){
                                    printf("Swipe left.............\n");
                                    int toYval = atoi(yval);
                                    //sprintf(toY,"%d",(toYval+50));
                                    int toXval = atoi(xval);
                                    //sprintf(toX, "%d", (toXval));
                                    
                                }
                            }
                            formJsonObject(jobj6,"x",toY);
                            formJsonObject(jobj6,"y",toX);
                            json_object_array_add(array,jobj5);

                	    }
			            else
			            {
                            formJsonObject(jobj,"x",xval);
                            formJsonObject(jobj,"y",yval);
                            json_object_array_add(array,jobj2);

                            formJsonObject(jobj3,"action","wait");
                            json_object_object_add(jobj3,"options",jobj4);
                            formJsonObject(jobj4,"ms","60");
                            json_object_array_add(array,jobj3);

                            formJsonObject(jobj5,"action","moveTo");
                            json_object_object_add(jobj5,"options",jobj6);
                            
                            if (abs(x - x1) > abs(y - y1)){
                                printf("Swipe left/right.............\n");
                                if (x > x1){
                                    printf("Swipe right.............\n");
                                    int toXval = atoi(xval);
                                }
                                if (x < x1){
                                    printf("Swipe left.............\n");
                                    int toXval = atoi(xval);
                                }
                            }
                            else
                            {
                                printf("Swipe up/down.............\n");
                                if (y > y1){
                                    printf("Swipe up.............\n");
                                    int toYval = atoi(yval);
                                    sprintf(toY,"%d",(toYval-50));
                                }
                                if (y < y1){
                                    printf("Swipe down.............\n");
                                    int toYval = atoi(yval);
                                    sprintf(toY,"%d",(toYval+50));
                                }
                            }
                            formJsonObject(jobj6,"x",toX);
                            formJsonObject(jobj6,"y",toY);
                            json_object_array_add(array,jobj5);
                            
			            }
                    
            

                        formJsonObject(jobj7,"action","release");
                        json_object_array_add(array,jobj7);
            
                        json_object_object_add(jobj1,"actions",array);
                        printf ("The json object created: %s\n",json_object_to_json_string(jobj1));
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/touch/perform",httpEndPointPrefix,xPort,sessionId);
                        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj1));
	    	        #else
	    	        {
			            formJsonObject(jobj,"fromX",xval);
	            	    formJsonObject(jobj,"fromY",yval);
		                formJsonObject(jobj,"toX",toX);
                	    formJsonObject(jobj,"toY",toY);
                        sprintf(orientation,"%d",UIorientation);
                        formJsonObject(jobj,"duration","0.05");
                        formJsonObject(jobj,"orientation",orientation);
	                    printf("THe points of swipe xval %s , yval %s , tox %s , toY %s \n",xval,yval,toX,toY);
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
                        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
		            }
	                #endif
	        }
            
            json_object_put(jobj);
            getXCTestSession(sessionId);
            break;
        case 'D':				/*Tap Ok*/
            break;
        case 'd':				/*Double Tap*/
            printf("double tap case \n");
            ptr = strtok(eventString+2," ");
            if(ptr!=NULL)
            {
                sprintf( xval,"%s", ptr);
                ptr = strtok(NULL, " ");
                sprintf(yval, "%s", ptr);
            }
            jobj = json_object_new_object();
            formJsonObject(jobj,"x",xval);
            formJsonObject(jobj,"y",yval);
            /*Now printing the json object*/
            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/doubleTap",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            json_object_put(jobj);
            break;
        case 'P':
            printf("Potrait case \n");	
            jobj = json_object_new_object();
            formJsonObject(jobj,"orientation","PORTRAIT");
            /*Now printing the json object*/
            printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/orientation",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            json_object_put(jobj);
            //if(height == 0)
                getWindowSize(sessionId);
            UIorientation = 1;
            break;

        case 'L':
            printf("Landscape case \n");
	    if(height == 0)
		    getWindowSize(sessionId);
	    printf("After getting window details \n");
            jobj = json_object_new_object();
	    printf("After creating new object \n");
            formJsonObject(jobj,"orientation","LANDSCAPE");
            /*Now printing the json object*/
            printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            sprintf(httpEndPointUrl,"%s:%s/session/%s/orientation",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            json_object_put(jobj);
            UIorientation = 3;  
            break;
        case 'Z':				/*pinch In*/
            printf("pinch in case \n");
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
            //printf("pinch : %s\n",json_object_to_json_string(jobj));
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            json_object_put(jobj);
            break;
        case 'p':				/*pinch Out*/
            printf("pinch out case \n");
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

        case 'n':				/*Swipe/DragFromTo*/
             printf("Scroll Up case \n");
		     if(height == 0)
		     {
			     getXCTestSession(sessionId);
			     getWindowSize(sessionId);
		     }
            jobj = json_object_new_object();
            jobj1 = json_object_new_object();
            jobj2 = json_object_new_object();
            jobj3 = json_object_new_object();
            jobj4 = json_object_new_object();
            jobj5 = json_object_new_object();
            jobj6 = json_object_new_object();
            jobj7 = json_object_new_object();
            ptr = strtok(eventString+2," ");
            if(ptr!=NULL)
            {
                if(UIorientation == 1)
                {
                    sprintf( xval,"%s", ptr);
                    ptr = strtok(NULL, " ");
                    sprintf(yval, "%s", ptr);
                    int y = atoi(ptr);
                    if(y > height/4)
                    {
                        sprintf(toY, "%s", ptr);
                        sprintf(toX,"%s",xval);
			int yv = atoi(toY);
			#if defined OS_14_0
			{
				sprintf(yval,"%d",(yv-50));		
			}
			#else			
	            sprintf(yval, "%d",(height/4));
			#endif
            }
                    else
                    {
                        sprintf(yval, "%s", ptr);
                        sprintf(toX,"%s",xval);
			            int yv = atoi(yval);
			            #if defined OS_14_0
                        {
                            sprintf(toY,"%d",(yv-50));
                        }
                        #else
	
        	                sprintf(toY, "%d",(height/4));
			            #endif
                    }
                }
                else if(UIorientation == 3)
                {
		            #if defined OS_14_0
			        sprintf(yval,"%s",ptr);
                                int x = atoi(ptr);
                                if(x > 155)
                                {
                                        sprintf(toY, "%s",ptr);
                                        int yv = atoi(toY);
                                        sprintf(yval, "%d",(yv-50));
                                }
                                else
                                {
                                        sprintf(yval, "%s",ptr);
					int YVAL = atoi(yval);
                                        if(YVAL < 55)
                                                sprintf(yval, "%d", (YVAL+(55-YVAL)));
                                        int yv = atoi(yval);
                                        sprintf(toY, "%d",(yv+50));
                                }
                                ptr = strtok(NULL," ");
                                sprintf(xval, "%s",ptr);
                                sprintf(toX,"%s",xval);
                    #else
	                    sprintf( xval,"%s", ptr);
        	            int y = atoi(ptr); 
                	    if(y > height/4)
	                    {	
        	                sprintf(xval, "%s", ptr);
                	        sprintf(toX, "%d",height/4);
	                    }
        	             else
                	    {
                        	sprintf(toX, "%s", ptr);
	                        sprintf(xval, "%d",height/4);
        	            }
                	    ptr = strtok(NULL, " ");
	                    sprintf(yval, "%s", ptr);
        	            sprintf(toY,"%s",yval);
		    #endif
                }
                
                printf("Swipe coordinates %s %s %s %s \n",xval,yval,toX,toY);
            }
            #if defined  OS_14_0
			array = json_object_new_array();
                        formJsonObject(jobj2,"action","press");
                        json_object_object_add(jobj2,"options",jobj);
                        formJsonObject(jobj,"x",xval);
                        formJsonObject(jobj,"y",yval);
                        json_object_array_add(array,jobj2);

                        formJsonObject(jobj3,"action","wait");
                            json_object_object_add(jobj3,"options",jobj4);
                            formJsonObject(jobj4,"ms","60");
                            json_object_array_add(array,jobj3);

                            formJsonObject(jobj5,"action","moveTo");
                            json_object_object_add(jobj5,"options",jobj6);
                            formJsonObject(jobj6,"x",toX);
                            formJsonObject(jobj6,"y",toY);
                            json_object_array_add(array,jobj5);
                            formJsonObject(jobj7,"action","release");
                        json_object_array_add(array,jobj7);
            
                        json_object_object_add(jobj1,"actions",array);
                        printf ("The json object created: %s\n",json_object_to_json_string(jobj1));
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/touch/perform",httpEndPointPrefix,xPort,sessionId);
                        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj1));
            #else

            sprintf(orientation,"%d",UIorientation);
            formJsonObject(jobj,"fromX",xval);
            formJsonObject(jobj,"fromY",yval);
            formJsonObject(jobj,"toX",toX);
            formJsonObject(jobj,"toY",toY);
            formJsonObject(jobj,"duration","0.05");
            formJsonObject(jobj,"orientation",orientation);
            //sprintf(httpEndPointUrl,"%s:%s/session/%s/uiaTarget/0/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            //sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/element/0/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            #endif
            json_object_put(jobj);
            break;

        case 'N':				/*Swipe/DragFromTo*/
            printf("Swipe/Scroll Down case \n");
             if(height == 0)
		     {
			     getXCTestSession(sessionId);
			     getWindowSize(sessionId);
		     }
            
            jobj = json_object_new_object();
            jobj1 = json_object_new_object();
            jobj2 = json_object_new_object();
            jobj3 = json_object_new_object();
            jobj4 = json_object_new_object();
            jobj5 = json_object_new_object();
            jobj6 = json_object_new_object();
            jobj7 = json_object_new_object();

            printf("Starting to fetch device co-ordinates details \n");
            ptr = strtok(eventString+2," ");
            if(ptr!=NULL)
            {
                if(UIorientation == 1)
                {
                    sprintf( xval,"%s", ptr);
                    ptr = strtok(NULL, " ");
                    sprintf(toY, "%s", ptr);
                    int y = atoi(ptr);
                    if(y > height/4)
                    {
                        sprintf(yval, "%s", ptr);
                        sprintf(toX,"%s",xval);
			int yv = atoi(yval);
			#if defined OS_14_0
                        {
                        	
			        sprintf(toY,"%d",(yv-50));
                        }
                        #else

	                        sprintf(toY, "%d",(height/4));
			#endif
                    }
                    else
                    {
                        sprintf(toY, "%s", ptr);
                        sprintf(toX,"%s",xval);
			int yv = atoi(toY);
			 #if defined OS_14_0
                        {
				if((yv-100) < 100)
				{
					yv = yv + (50 - yv);
					sprintf(yval, "%d",yv);
				}
				else

	                                sprintf(yval,"%d",(yv-50));
                        }
                        #else

	                        sprintf(yval, "%d",(height/4));
			#endif
                    }
                }
                else if(UIorientation == 3)
                {
			#if defined OS_14_0
				sprintf(yval,"%s",ptr);
                        int x = atoi(ptr);
                        if(x > 155)
                        {
                                sprintf(yval, "%s",ptr);
                                int yv = atoi(yval);
                                sprintf(toY, "%d",(yv-50));
                        }
                        else
                        {
                                sprintf(toY,"%s",ptr);
                                int yv = atoi(toY);
                                sprintf(yval, "%d",(yv+50));
                        }
                        ptr = strtok(NULL," ");
                        sprintf(xval,"%s", ptr);
                        sprintf(toX, "%s",xval);

			#else
				sprintf (xval,"%s",ptr);
        	                int y = atoi(ptr);
                	        if(y > height/4)
                        	{
	                           sprintf(toX, "%s", ptr);
        	                   sprintf(xval, "%d",height/4);
                	        }
                        	else
	                        {
        	                   sprintf(xval, "%s", ptr);
                	           sprintf(toX, "%d",height/4);
                        	}
	                    ptr = strtok(NULL," ");
        	            sprintf(yval, "%s",ptr); 
                	    sprintf(toY,"%s",yval);	
		     #endif
                }
                
                printf("Swipe coordinates %s %s %s %s \n",xval,yval,toX,toY);
            }
            #if defined  OS_14_0
			array = json_object_new_array();
                        formJsonObject(jobj2,"action","press");
                        json_object_object_add(jobj2,"options",jobj);
                        formJsonObject(jobj,"x",xval);
                        formJsonObject(jobj,"y",yval);
                        json_object_array_add(array,jobj2);

                        formJsonObject(jobj3,"action","wait");
                            json_object_object_add(jobj3,"options",jobj4);
                            formJsonObject(jobj4,"ms","60");
                            json_object_array_add(array,jobj3);

                            formJsonObject(jobj5,"action","moveTo");
                            json_object_object_add(jobj5,"options",jobj6);
                            formJsonObject(jobj6,"x",toX);
                            formJsonObject(jobj6,"y",toY);
                            json_object_array_add(array,jobj5);
                            formJsonObject(jobj7,"action","release");
                        json_object_array_add(array,jobj7);
            
                        json_object_object_add(jobj1,"actions",array);
                        printf ("The json object created: %s\n",json_object_to_json_string(jobj1));
                        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/touch/perform",httpEndPointPrefix,xPort,sessionId);
                        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj1));
            #else

            sprintf(orientation,"%d",UIorientation);
            formJsonObject(jobj,"fromX",xval);
            formJsonObject(jobj,"fromY",yval);
            formJsonObject(jobj,"toX",toX);
            formJsonObject(jobj,"toY",toY);
            formJsonObject(jobj,"duration","0.05");
            formJsonObject(jobj,"orientation",orientation);
            //sprintf(httpEndPointUrl,"%s:%s/session/%s/uiaTarget/0/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            //sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/element/0/dragfromtoforduration",httpEndPointPrefix,xPort,sessionId);
            postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));
            #endif
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
        json_object *new_obj;
        new_obj = json_tokener_parse(payload);
        if(new_obj != NULL)
        {
            sprintf(sessionId,"%s",json_object_get_string(json_object_object_get(new_obj, "sessionId")));
        }
	if(strlen(sessionId) < 10){
              //  fprintf(stderr,"Sessios id getting null \n"); 
		return -1;
         }
        json_object_put(new_obj);
        return 0;
    }
    return -1;

}

char isAlertPresent(char *session)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/session/%s/alert/text",xPort,session);
    char value[1024] = {0};
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

                        sprintf(value,"%s", json_object_get_string(json_object_object_get(new_obj, "value")));
                        json_object *alertValue;
                        alertValue = json_tokener_parse(value);
                        printf("%s  \n",json_object_get_string(json_object_object_get(alertValue,"error")));
                        if(json_object_get_string(json_object_object_get(alertValue,"error")) == NULL)
                        {
                            printf("ALLLLLEEERT \n");
                            printf("ALERT PRESENT\n");
                            json_object_put(new_obj);
                            if(payload != NULL){
                                free(payload);
                                payload = NULL;
                            }
                            isAlert = 1;
                            return 1;
                        }
                        /*if(strlen(json_object_get_string(json_object_object_get(new_obj, "value"))) > 3 )
                        {
                            printf("ALERT PRESENT\n");
                            json_object_put(new_obj);
                            if(payload != NULL){
                                free(payload);
                                payload = NULL;
                            }
                            isAlert = 1;
                            return 1;
                        }*/
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

char getWindowSize(char *session)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/session/%s/window/size",xPort,sessionId);
    struct curl_fetch_st chunk;
    printf("Fetching window details \n");
    if(!getHttpData(httpEndPointUrl,&chunk,0L))
    {
        char payload[2046] = {0};
	printf("initializing the payload \n");
	if(p!=NULL)
        {
            if(p->size > 0)
            {
                char *payload = (char *)malloc(p->size);
        	strcpy(payload,p->payload);
	        printf("payload: \n%s\n",payload);
	
	    printf("Trying to fetch payload \n");
        //strcpy(payload,p->payload);
        //printf("payload: \n%s\n",payload);
        json_object *new_obj;
        new_obj = json_tokener_parse(payload);
        char *value = (char *)malloc(p->size);
        char h[8] = {0};
        char w[8] = {0};
        char o[8] = {0};
	    //free(payload);
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
            width = atof(w);
            sprintf(o,"%s",json_object_get_string(json_object_object_get(new_obj,"orientation")));
            UIorientation = atof(o);
        }
        json_object_put(new_obj);
	    scrollFlag = 0;
	}
        return 0;
}	}
    
    return -1;
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
            //printf("AppBundleId : %s\n",*AppBundleId);
        }
        json_object_put(new_obj);
        free(payload);
	      payload = NULL;
        return 0;
    }
    return -1;

}

void terminateIfSettingsIsOpen (int signum)
{
    char *curBundleId = (char *)malloc(128);
    getCurrentApp(&curBundleId);
    if((!strcmp(curBundleId,"com.apple.Preferences")) || (!strcmp(curBundleId,"com.pcloudy.Screencaster")))
    {
        printf("bundle : %s\n", curBundleId);
        char httpEndPointPrefix[] = "http://localhost";
        char httpEndPointUrl[256] = {0};
        json_object *jobj = json_object_new_object();
        if((!strcmp(curBundleId,"com.apple.Preferences")))
        {
		formJsonObject(jobj,"bundleId","com.apple.Preferences");
	}
        else if((!strcmp(curBundleId,"com.pcloudy.Screencaster")))
	{
		formJsonObject(jobj,"bundleId","com.pcloudy.Screencaster");
	}
	printf ("The json object created: %s\n",json_object_to_json_string(jobj));
        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/apps/terminate",httpEndPointPrefix,xPort,sessionId);
        postHttpData(httpEndPointUrl,json_object_to_json_string(jobj));

    }
    free(curBundleId);
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
            //printf("source payload size = %d\n",p->size);
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

// char ifFilteredApp(char *session)
// {
//     if(isFilteredApp)
//         return YES;
//     else
//         return NO;

//     char elementId[64] = {0}, tempLabel[32] = {0};
//     char httpEndPointUrl[256] = {0};
//     char httpEndPointPrefix[] = "http://localhost";
//     struct curl_fetch_st chunk;
//     json_object *jobj = json_object_new_object();
//     formJsonObject(jobj,"using","class name");
//     //sprintf(tempLabel,"label=%s",XCUILabel);
//     formJsonObject(jobj,"value","XCUIElementTypeApplication");
//     /*Now printing the json object*/
//     //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
//     getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId);
//     if(strlen(elementId)>0)
//     {
//         json_object_put(jobj);
//         sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/attribute/name",httpEndPointPrefix,xPort,sessionId, elementId);
//         if(!getHttpData(httpEndPointUrl,&chunk,2L))
//         {
//             if(p->size > 0){
//                 char payload[1024] = {0};
//                 char appName[1024] = {0};
//                 strcpy(payload,p->payload);
//                 //printf("payload: \n%s\n",payload);
//                 json_object *new_obj;
//                 new_obj = json_tokener_parse(payload);
//                 if(new_obj != NULL)
//                 {
//                     //printf("The appName is %s\n", json_object_get_string(json_object_object_get(new_obj, "value")));
//                     if(json_object_get_string(json_object_object_get(new_obj, "value")) != NULL)
//                     {
//                         sprintf(appName,"%s",json_object_get_string(json_object_object_get(new_obj, "value")));
//     				//printf("%s\n",appName);
//                         json_object_put(new_obj);
//                         if((strcmp(appName,"App Store")==0)||(strlen(appName)<= 1) || (strcmp(appName,"Settings")==0))
//                             return 1;
//                     }
//                     if(new_obj!=NULL)
//                         json_object_put(new_obj);
//                 }
//             }
//             return 0;
//         }
//     }
//     return 0;
    
// }

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
    /*if(getWindowSize(sessionId)<0)
    {
        sleep(10);
        getWindowSize(sessionId);
    }*/
#endif
    while(1)
    {
        if (thread_queue_get(pserver_state.queue,&timeout,&msg)==0)
        {
            processEvents(msg.data,sessionId);
            //std_out = (char*)msg.data;

        }
        timer_handler(0);
        usleep(1000);
    }

}
//#endif
// void* ProcessCheckFdsHandler(void* pserver)
// {
//     pServerState_t * server = (pServerState_t *) pserver;
//     struct threadmsg msg;
//     //instruments_client_t client;
//     struct QData* qData = NULL;

//     if(server != NULL)
//     {
//         rfbScreenInfoPtr screen = (rfbScreenInfoPtr)(server->screen);

//         while(!server->please_exit && screen)
//         {
//             rfbCheckFds(screen,3000);
// #if 0
//             //fprintf(stderr, "Processing events\n");
//             if(server->no_clients > 0)
//                 rfbProcessCheckFds(screen, screen->deferUpdateTime*100);
//             else
//                 rfbProcessCheckFds(screen, screen->deferUpdateTime*1000000);
// #endif
//         }
//     }
 //}

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



