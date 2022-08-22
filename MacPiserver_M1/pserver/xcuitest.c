#include<json.h>
#include "curlApi.h"


void formJsonObject(json_object * jobj, char *key, char *value)
{
    /*Creating a json string*/
    json_object *jstring = json_object_new_string(value);
    
    /*Form the json object*/
    json_object_object_add(jobj,key, jstring);
    
}

char sendHomescreenEvent(char *xPort)
{
    char httpEndPointPrefix[] = "http://localhost";
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"%s:%s/wda/homescreen",httpEndPointPrefix,xPort);
    printf("%s\n",httpEndPointUrl);
    postHttpData(httpEndPointUrl,"");
}
char getXCTestSession(char *session, char* xPort, char* osVersion)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/status",xPort);
    struct curl_fetch_st chunk;
    if(!getHttpData(httpEndPointUrl,&chunk,0L))
    {
        char payload[1024] = {0};
        strcpy(payload,chunk.payload);
        printf("payload: \n%s\n",payload);
        json_object *new_obj;
        new_obj = json_tokener_parse(payload);
        if(new_obj != NULL)
        {
            //printf("The Session ID is %s\n", json_object_get_string(json_object_object_get(new_obj, "sessionId")));
            sprintf(session,"%s",json_object_get_string(json_object_object_get(new_obj, "sessionId")));
            printf("%s\n",session);
            sprintf(osVersion,"%s",json_object_get_string(json_object_object_get(json_object_object_get(json_object_object_get(new_obj, "value"), "os"),"version")));
        }
        json_object_put(new_obj);
        return 0;
    }
    return -1;
    
}


void getElementIDFromLabel(char *session, char *labelData, char *elementId, char *xPort)
{
    char httpEndPointUrl[256] = {0};
    sprintf(httpEndPointUrl,"http://localhost:%s/session/%s/elements",xPort,session);
    struct curl_fetch_st chunk;
    if(!postHttpDataCB(httpEndPointUrl,labelData,&chunk))
    {
        if(&chunk!=NULL)
        {
            //printf("%s %d\n", __FUNCTION__, p->size);
            char *payload = (char *)malloc(chunk.size);
            strcpy(payload,chunk.payload);
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
