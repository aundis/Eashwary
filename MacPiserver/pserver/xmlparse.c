/*
 * file: xmlparse.c
 */
#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef LIBXML_TREE_ENABLED

/*
 *To compile this file using gcc you can type
 *gcc `xml2-config --cflags --libs` -o xmlparse xmlparse.c
 */

/**
 * find_alert_element:
 * @a_node: the initial xml node to consider.
 *
 * Prints the names of the all the xml elements
 * that are siblings or children of a given xml node.
 */
char isXCUILabelFound = 0;
char alertTagFound = 0;
char XCUILabel[1024]={0};
char isCoordinate(int x, int y, int xStart, int yStart, int width, int height)
{
	//printf("x= %d, y=%d ,%d %d %d %d \n", x, y, xStart, yStart, width, height);
	if((x<=(xStart+width)) && (y<=(yStart+height) && (x>=xStart) && (y>=yStart)))
		return 1;
	return 0;
}
static void
get_alert_elements(xmlNode * a_node, int x, int y)
{
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if(alertTagFound){
            if (cur_node->type == XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"XCUIElementTypeTextField")==0) {
                    printf("node type: Element, name: %s\n", cur_node->name);
        			if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
        			{
        				sprintf(XCUILabel,"XCUIElementTypeTextField");
        				isXCUILabelFound = 1;
        			}			
                }
        	else if (cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name,"XCUIElementTypeSecureTextField")==0) {
                    printf("node type: Element, name: %s\n", cur_node->name);
        			if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
        			{
        				sprintf(XCUILabel,"XCUIElementTypeSecureTextField");
        				isXCUILabelFound = 1;
        			}			
                }
        	else if (cur_node->type == XML_ELEMENT_NODE && (strcmp(cur_node->name,"XCUIElementTypeButton")==0)) {
                    printf("node type: Element, name: %s\n", cur_node->name);
        			if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
        			{
        				sprintf(XCUILabel,"%s",xmlGetProp(cur_node, "label"));
        				isXCUILabelFound = 1;
        			}
                }
            else if (cur_node->type == XML_ELEMENT_NODE && (strcmp(cur_node->name,"XCUIElementTypeMenuItem")==0)) {
                printf("node type: Element, name: %s\n", cur_node->name);
                if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
                {
                    sprintf(XCUILabel,"%s",xmlGetProp(cur_node, "label"));
                    isXCUILabelFound = 1;
                }
            }
        }
        if(!isXCUILabelFound)
        	get_alert_elements(cur_node->children,x,y);
    }
}

static void
find_alert_element(xmlNode * a_node, int x, int y)
{
    xmlNode *cur_node = NULL;
    alertTagFound = 0;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name,"XCUIElementTypeAlert")==0) {
            //printf("node type: Element, name: %s\n", cur_node->name);
			//printf("XCUIElementTypeAlert element found \n");
            alertTagFound = 1;
			get_alert_elements(cur_node->children,x,y);
			return;
        }
        
        find_alert_element(cur_node->children, x, y);
    }
}



char
parseXMLTree(char* xmlString,int x, int y)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION
    doc = xmlParseMemory(xmlString,strlen(xmlString));
    
    if (doc == NULL) {
        printf("error: could not parse xml string \n");
    }
    
    /*Get the root element node */
    root_element = xmlDocGetRootElement(doc);
    
    find_alert_element(root_element, x, y);
    printf("%s\n",XCUILabel);
    /*free the document */
    xmlFreeDoc(doc);
    
    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
    
    return 0;
    
}


static void
find_elements(xmlNode * a_node, int x, int y)
{
    xmlNode *cur_node = NULL;
    
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && (strcmp(cur_node->name,"XCUIElementTypeCell")==0)) {
            //printf("node type: Element, name: %s\n", cur_node->name);
            if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
            {
                //printf("%s\n",xmlGetProp(cur_node, "label"));
                if(xmlGetProp(cur_node, "label") != NULL)
                {
                    sprintf(XCUILabel,"%s",xmlGetProp(cur_node, "label"));
                    if((strcmp(XCUILabel,"Touch ID & Passcode")==0) || (strcmp(XCUILabel,"Software Update")==0))
                        isXCUILabelFound = 1;
                }
                else
                    find_elements(cur_node->children,x,y);
                //isXCUILabelFound = 1;//WorkAround if 2 elements present on same coordinate,Eg: tapBar on app description.
                return;
            }
        }
        else if (cur_node->type == XML_ELEMENT_NODE && strcmp((const char*)cur_node->name,"XCUIElementTypeTextField")==0) {
            //printf("node type: Element, name: %s\n", cur_node->name);
            if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
            {
                sprintf(XCUILabel,"XCUIElementTypeTextField");
                isXCUILabelFound = 1;
            }
        }
        else if (cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name,"XCUIElementTypeSecureTextField")==0) {
            //printf("node type: Element, name: %s\n", cur_node->name);
            if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
            {
                sprintf(XCUILabel,"XCUIElementTypeSecureTextField");
                isXCUILabelFound = 1;
            }
        }
        else if (cur_node->type == XML_ELEMENT_NODE && (strcmp(cur_node->name,"XCUIElementTypeButton")==0)) {
            //printf("node type: Element, name: %s\n", cur_node->name);
            if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
            {
                //printf("%s\n",xmlGetProp(cur_node, "label"));
                sprintf(XCUILabel,"%s",xmlGetProp(cur_node, "label"));
                isXCUILabelFound = 1;
            }
        }
        else if (cur_node->type == XML_ELEMENT_NODE && (strcmp(cur_node->name,"XCUIElementTypeIcon")==0)) {
            //printf("node type: Element, name: %s\n", cur_node->name);
            if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
            {
                //printf("%s\n",xmlGetProp(cur_node, "label"));
                sprintf(XCUILabel,"%s",xmlGetProp(cur_node, "label"));
                isXCUILabelFound = 1;
            }
        }
        else if (cur_node->type == XML_ELEMENT_NODE && (strcmp(cur_node->name,"XCUIElementTypeMenuItem")==0)) {
            //printf("node type: Element, name: %s\n", cur_node->name);
            if(isCoordinate(x,y,atoi(xmlGetProp(cur_node, "x")),atoi(xmlGetProp(cur_node, "y")),atoi(xmlGetProp(cur_node, "width")),atoi(xmlGetProp(cur_node, "height"))))
            {
                //printf("%s\n",xmlGetProp(cur_node, "label"));
                sprintf(XCUILabel,"%s",xmlGetProp(cur_node, "label"));
                isXCUILabelFound = 1;
            }
        }
        if(cur_node->children!=NULL)
            find_elements(cur_node->children,x,y);
    }
}

static char
find_app_name(xmlNode * a_node, char *AppName,int x, int y)
{
    xmlNode *cur_node = NULL;
    
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if(cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name,"XCUIElementTypeApplication")==0)
        {
            char curAppName[128] = {0};
            sprintf(curAppName,"%s",xmlGetProp(cur_node, "label"));
            // printf("curAppName = %s, string length %d\n",curAppName,strlen(curAppName));
            if((strcmp(AppName,curAppName)==0)||(strlen(curAppName)<= 1)) //To handle peculiar case where app name is not present when alert pop present on AppStore
            {
                find_elements(cur_node->children,x,y);
                return 1;
            }
            else
                return 0;
        }
        
        //find_alert_element(cur_node->children, x, y);
    }
}


char
parseXMLForAppType(char* xmlString, char *AppName, int x, int y)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    char ret = 0;
    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION
    doc = xmlParseMemory(xmlString,strlen(xmlString));
    
    if (doc == NULL) {
        printf("error: could not parse xml string \n");
    }
    
    /*Get the root element node */
    root_element = xmlDocGetRootElement(doc);
    
    ret = find_app_name(root_element,AppName, x, y);
    /*free the document */
    xmlFreeDoc(doc);
    
    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
    
    return ret;
 
}

#if 0
int
main(int argc, char **argv)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
	int x, y;
    if (argc != 4)
        return(1);
    x = atoi(argv[2]);
    y = atoi(argv[3]);
    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION
	char xml[] ="<breakfast_menu>\
	<food>\
		<name>\"Belgian Waffles\"</name>\
		<price>$5.95</price>\
		<description>\
Two of our famous Belgian Waffles with plenty of real maple syrup\
</description>\
		<calories>650</calories>\
	</food>\
	<food>\
		<name>Strawberry Belgian Waffles</name>\
		<price>$7.95</price>\
		<description>\
Light Belgian waffles covered with strawberries and whipped cream\
</description>\
		<calories>900</calories>\
	</food>\
	<food>\
		<name>Berry-Berry Belgian Waffles</name>\
		<price>$8.95</price>\
		<description>\
Light Belgian waffles covered with an assortment of fresh berries and whipped cream\
</description>\
		<calories>900</calories>\
	</food>\
</breakfast_menu>";
 char array[29999];
 double time;
 FILE *fp;
 unsigned int i = 0;
 fp = fopen("response.xml","r");
 if (fp != NULL) {
    while (!feof(fp)) {
       fscanf(fp,"%c",&array[i]);
       i++;
    }
 }
//printf("%s %d\n",array,strlen(array));
 fclose(fp);
    /*parse the file and get the DOM */
    //doc = xmlReadFile(argv[1], NULL, 0);
    parseXMLTree(array, x, y);
}
#endif
#else
int main(void) {
    fprintf(stderr, "Tree support not compiled in\n");
    exit(1);
}
#endif

