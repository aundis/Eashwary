#include <QCoreApplication>

#include "streamer.h"




int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    int width = 0, height = 0;
    QString logoFile, port, x_port ;
    char *udid = NULL;
    if(argc <= 1)
    {
        qDebug() << "How to use:";
        qDebug() << argv[0] << "\n-P <Video Port>\n-x <WDA Port>\n-w <screen width>\n-h <screen height>\n-logo <logo overlay file path>\n";
        exit(0);
    }
    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i],"-u")){            
            i++;
            udid = argv[i];
            continue;
        }
        else if(!strcmp(argv[i],"-P")){
            i++;
            port.append(argv[i]);
            continue;
        }
        else if(!strcmp(argv[i],"-x")){
            i++;
            x_port.append(argv[i]);
            continue;
        }
        else if(!strcmp(argv[i],"-w")){
            i++;
            width =atoi(argv[i]);
            continue;
        }
        else if(!strcmp(argv[i],"-h")){
            i++;
            height = atoi(argv[i]);
            continue;
        }
        else if(!strcmp(argv[i],"-logo")){
            i++;
            logoFile.append(argv[i]);
        }
    }
    Streamer captainStreamer(udid,port,x_port,width,height,logoFile);

    return a.exec();
}
