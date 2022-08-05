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
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/instruments.h>
#include <json.h>
#include "curlApi.h"

char b_id[50] = {0};
char xPort[8] = {0};
char URL[1024] = {0};
instruments_client_t instr;
lockdownd_client_t lckd = NULL;
uint64_t pid;
char *packagepath;

void print_usage(int argc, char **argv)
{
        char *name = NULL;

        name = strrchr(argv[0], '/');
        printf("Usage: %s [OPTIONS]\n", (name ? name + 1: argv[0]));
        printf("Start launchApp\n");
        printf("NOTE: A mounted developer disk image is required on the device\n");
        printf("  -u, --udid UDID\ttarget specific device by its 40-digit device UDID\n");
        printf("  -b, --bundleid\tBundle id of device\n");
#ifdef IOS_10_0
        printf("  -x, --xPort\t\tLocal Port on which to xctest is listening\n");
        printf("  -U, --url\tURL to open\n");
#endif
        printf("\n");
}


void launchApplication(char* b_id)
{
	instruments_error_t ierr = INSTRUMENTS_E_UNKNOWN_ERROR;
	ierr = instruments_get_pid(instr, 5 , b_id, &pid);
	
	if ((ierr == INSTRUMENTS_E_SUCCESS) && (pid>0))
		{

			ierr = instruments_pid_ops(instr,5,pid,"resumePid:");

			if (ierr != INSTRUMENTS_E_SUCCESS) {
				printf("ERROR: Resume pid failed.\n");
			}

	        //printf("before killpid.\n");
			ierr = instruments_pid_ops(instr,5,pid,"killPid:");
	        if (ierr != INSTRUMENTS_E_SUCCESS)
			{
	                	printf("ERROR: killpid failed.\n");
			}
			sleep(1);
		}
	
	ierr = instruments_launch_app(instr,5,packagepath,b_id,&pid);
		if (ierr != INSTRUMENTS_E_SUCCESS) {
				printf("ERROR: Could not launch app. trying again %d\n",ierr);
				ierr = instruments_launch_app(instr,5,packagepath,b_id,&pid);
				//ierr = instruments_launch_app(instr,5,packagepath,"com.techendeavour.WatchPOC.Retail",&pid);
				if (ierr != INSTRUMENTS_E_SUCCESS) {
					printf("ERROR: Could not launch app. giving up\n");
					exit(0);
				}
			}
			ierr = instruments_pid_ops(instr,5,pid,"resumePid:");

		    if (ierr != INSTRUMENTS_E_SUCCESS) {
				printf("ERROR: Resume pid failed.\n");
			}
		//printf("before launch app\n");
		//sleep(3);
	
}	


instruments_error_t start_input_script(idevice_t device)
{
	instruments_error_t ierr = INSTRUMENTS_E_UNKNOWN_ERROR;
	char please_exit = 0;
	if (!please_exit)
	{
		ierr = instruments_client_start_service(device,&(instr),"instruments");
        if (ierr != INSTRUMENTS_E_SUCCESS)
		{
			printf("ERROR: Could not start service com.apple.instruments.remoteserver.\n");
			please_exit=1;
		}
    }

 	if (!please_exit)
	{       
		//ierr = instruments_send_channel_request(pserver_state.instr, "com.apple.instruments.server.services.capabilities",1);
		ierr = instruments_send_channel_request(instr, "com.apple.instruments.server.services.device.xpccontrol",1);

		if (ierr != INSTRUMENTS_E_SUCCESS)
		{
			printf("ERROR: Could not start sub service capabilities.\n");
			please_exit=1;
		}

		//ierr = instruments_send_channel_request(pserver_state.instr, "com.apple.instruments.server.services.filebrowser",2);
		ierr = instruments_send_channel_request(instr, "com.apple.instruments.server.services.device.applicationListing",2);

        	if (ierr != INSTRUMENTS_E_SUCCESS)
		{
                	printf("ERROR: Could not start sub service filebrowser.\n");
	            	please_exit=1;
		}

		ierr = instruments_send_channel_request(instr, "com.apple.instruments.server.services.deviceinfo",3);

        	if (ierr != INSTRUMENTS_E_SUCCESS)
		{
                	printf("ERROR: Could not start sub service deviceinfo.\n");
			please_exit=1;
		}

		ierr = instruments_send_channel_request(instr, "com.apple.instruments.server.services.launchdaemon",4);

        	if (ierr != INSTRUMENTS_E_SUCCESS)
		{
			printf("ERROR: Could not start sub service launchdeamon.\n");
			please_exit=1;
		}

		ierr = instruments_send_channel_request(instr, "com.apple.instruments.server.services.processcontrol",5);

        	if (ierr != INSTRUMENTS_E_SUCCESS)
		{
                	printf("ERROR: Could not start sub service processcontrol.\n");
			please_exit=1;
		}

		ierr = instruments_send_channel_request(instr, "com.apple.instruments.server.services.processcontrol.posixspawn",6);

        	if (ierr != INSTRUMENTS_E_SUCCESS)
		{
                	printf("ERROR: Could not start sub service processcontrol.posixspawn.\n");
			please_exit=1;
		}

        /*ierr = instruments_send_channel_request(instr, "com.apple.instruments.server.services.mobilenotifications",7);

        if (ierr != INSTRUMENTS_E_SUCCESS)
        {
            printf("ERROR: Could not start sub service mobilenotification.\n");
            please_exit=1;
        }
		*/

		ierr = instruments_send_channel_request(instr, "com.apple.instruments.UIAutomation",9);

        if (ierr != INSTRUMENTS_E_SUCCESS)
		{
        	printf("ERROR: Could not start sub service UIAutomation.\n");
			please_exit=1;
		}
	}

 	if (!please_exit)
	{
		
		ierr = instruments_config_launch_env(instr,9,"./","./vncscript.js");

        if (ierr != INSTRUMENTS_E_SUCCESS)
		{
           	printf("ERROR: Could not configure launch environgment.\n");
			please_exit=1;
		}

		
	}
}


int main(int argc,char** argv)
{
	long usec;

  	int i;
    const char *udid = NULL;
    idevice_t device = NULL;
    lockdownd_service_descriptor_t service = NULL;
    instproxy_client_t ipc = NULL;
	
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--udid")) {
			i++;
			if (!argv[i] || (strlen(argv[i]) < 0)) {
				print_usage(argc, argv);
				return 0;
			}
			udid = argv[i];
			continue;
		}
		else if(!strcmp(argv[i], "-b") || !strcmp(argv[i],"--bundleid"))
		{
			i++;
			strcpy(b_id,argv[i]);
			printf("%s\n",b_id);
			continue;
		}
#ifdef IOS_10_0
        else if(!strcmp(argv[i], "-x") || !strcmp(argv[i],"--xPort"))
        {
            i++;
            strcpy(xPort,argv[i]);
            printf("%s\n",xPort);
            continue;
        }
        else if(!strcmp(argv[i], "-U") || !strcmp(argv[i],"--URL"))
        {
            i++;
            strcpy(URL,argv[i]);
            printf("%s\n",URL);
            continue;
        }
#endif
		else {
			print_usage(argc, argv);
			return 0;
		}
	}
#ifdef IOS_10_0
    char httpEndPointPrefix[] = "http://localhost";
    char httpEndPointUrl[256] = {0};
    char DataBuffer[1024] = {0};
    char sessionId[64] = {0};
    char osVersion[8] = {0};
    /*Send HomeScreen Event*/
    //sendHomescreenEvent(xPort); /*Commenting sending home as it hangs in case of iOS13*/
    getXCTestSession(sessionId, xPort,osVersion);
    printf("sessionId : %s\n",sessionId);
    printf("osVersion : %s\n",osVersion);
    /*Launching Application*/
    if(strlen(b_id)>0)
    {
        memset(httpEndPointUrl,0x00,sizeof(httpEndPointUrl));
	sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/apps/terminate",httpEndPointPrefix,xPort,sessionId);
        sprintf(DataBuffer,"{\"bundleId\":\"%s\"}",b_id);
        printf("%s\n",DataBuffer);
        postHttpData(httpEndPointUrl,DataBuffer);
        usleep(5000);
        memset(httpEndPointUrl,0x00,sizeof(httpEndPointUrl));
        sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/apps/launch",httpEndPointPrefix,xPort,sessionId);
        if(strlen(URL)>0)
        {
            sprintf(DataBuffer,"{\"bundleId\":\"%s\",\"arguments\":[\"-u\",\"%s\"]}", b_id, URL);
        }
        else
        {
            sprintf(DataBuffer,"{\"bundleId\":\"%s\"}",b_id);
        }
        printf("%s\n",DataBuffer);
        postHttpData(httpEndPointUrl,DataBuffer);
        sleep(3);
        
        /*chrome and firefox with URL*/
        if(strcmp("com.pcloudy.pcloudyhelper",b_id)==0)
        {
            /*Click on URL text field*/
            char elementId[64] = {0}, tempLabel[32] = {0};
            char version = atoi(osVersion);
            printf("version: %d\n",version);
            json_object *jobj = NULL;
            jobj = json_object_new_object();
            formJsonObject(jobj,"using","link text");
            if(version>=11)
                formJsonObject(jobj,"value","label=Always Allow");
            else
                formJsonObject(jobj,"value","label=Allow");
            /*Now printing the json object*/
            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId, xPort);
            if(strlen(elementId)>0)
            {
                sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
                postHttpData(httpEndPointUrl,"");
            }
            sendHomescreenEvent(xPort);
            
        }
        if(strcmp("org.mozilla.ios.Firefox",b_id)==0)
        {
            /*Click on URL text field*/
            char elementId[64] = {0}, tempLabel[32] = {0};
            json_object *jobj = NULL;
            jobj = json_object_new_object();
            formJsonObject(jobj,"using","link text");
            formJsonObject(jobj,"value","name=url");
            /*Now printing the json object*/
            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId, xPort);
            sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
            postHttpData(httpEndPointUrl,"");
            
            if(strlen(URL)>0)
            {
                //sprintf( xval,"%s", ptr);
                char kbBuffer[1024]={0};
                sprintf(kbBuffer,"{ \"value\": [\"%s%s\"] }",URL,"\\n");
                printf("kbBuffer %s\n",kbBuffer);
                sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/keys",httpEndPointPrefix,xPort,sessionId);
                postHttpData(httpEndPointUrl,kbBuffer);
                json_object_put(jobj);
            }
            
            
        }
        else if(strcmp("com.google.chrome.ios",b_id)==0)
        {
            /*Click on URL text field*/
            char elementId[64] = {0}, tempLabel[32] = {0};
            json_object *jobj = NULL;
            jobj = json_object_new_object();
            formJsonObject(jobj,"using","link text");
            formJsonObject(jobj,"value","label=Address and search bar");
            /*Now printing the json object*/
            //printf ("The json object created: %s\n",json_object_to_json_string(jobj));
            getElementIDFromLabel(sessionId, json_object_to_json_string(jobj), elementId, xPort);
            sprintf(httpEndPointUrl,"%s:%s/session/%s/element/%s/click",httpEndPointPrefix,xPort,sessionId, elementId);
            postHttpData(httpEndPointUrl,"");
            
            if(strlen(URL)>0)
            {
                char kbBuffer[1024]={0};
                sprintf(kbBuffer,"{ \"value\": [\"%s%s\"] }",URL,"\\n");
                printf("kbBuffer %s\n",kbBuffer);
                sprintf(httpEndPointUrl,"%s:%s/session/%s/wda/keys",httpEndPointPrefix,xPort,sessionId);
                postHttpData(httpEndPointUrl,kbBuffer);
                json_object_put(jobj);
            }
            
            
        }
    }
#else
	 if (IDEVICE_E_SUCCESS != idevice_new(&device, udid)) {
                if (udid) {
                        printf("No device found with udid %s, is it plugged in?\n", udid);
                } else {
                        printf("No device found, is it plugged in?\n");
                }
		exit(0);
    }
    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(device, &lckd, NULL)) {
                printf("Exiting.\n");
                exit(0);
    }
    
    instruments_error_t ierr = INSTRUMENTS_E_UNKNOWN_ERROR;

	
	if ((lockdownd_start_service(lckd, "com.apple.mobile.installation_proxy", &service) != LOCKDOWN_E_SUCCESS) || !service) {
        printf("Could not connect to installation proxy.\n");
        exit(0);
    }
    
	
        ierr = instproxy_client_new(device, service, &ipc);

        if (service) {
            lockdownd_service_descriptor_free(service);
        }
        service = NULL;

        if (ierr != INSTPROXY_E_SUCCESS) {
            fprintf(stderr, "Could not connect to installation_proxy!\n");
            exit(0);
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
                exit(0);
            }

            if (!apps || (plist_get_node_type(apps) != PLIST_ARRAY)) {
                fprintf(stderr,
                    "ERROR: instproxy_browse returnd an invalid plist!\n");
                exit(0);
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
                		exit(0);
            			break;
        			}
        			if (strcmp(s_appid,b_id)==0)
        			//if (strcmp(s_appid,"com.techendeavour.WatchPOC.Retail")==0)
        			{
            			plist_t path = plist_dict_get_item(app, "Path");
            			if (path) {
                			plist_get_string_val(path, &s_path);
                			printf("%s %s\n", s_appid, s_path);
                			packagepath = strdup(s_path);
                			free(s_path);
            			}
        				free(s_appid);
            			break;
        			}
        			
        			free(s_appid);
				}
    			plist_free(apps);
                if (packagepath==NULL)
				{	
                	fprintf(stderr,"Specified Application is not installed ... \n");
					exit(0);
				}
				
			}
        }
	
    start_input_script(device);
    
    launchApplication(b_id);
   
#endif
	
}


