// mythread.cpp

#include "threader.h"

Threader::Threader(QString xudid, qintptr ID, QString xport, QByteArray CurrentImageBuffer, QObject *parent) :
    QThread(parent)
{
    this->socketDescriptor = ID;
    m_xport = xport;
    CurrentImg = CurrentImageBuffer;
    m_udid = xudid;
}

void Threader::run()
{
    // thread starts here
    qDebug() << " Thread started";

    socket = new QTcpSocket();

    // set the ID
    if(!socket->setSocketDescriptor(this->socketDescriptor))
    {
        // something's wrong, we just emit a signal
        emit error(socket->error());
        return;
    }

    // connect socket and signal
    // note - Qt::DirectConnection is used because it's multithreaded
    //        This makes the slot to be invoked immediately, when the signal is emitted.

    connect(socket, SIGNAL(readyRead()), this, SLOT(readyRead()), Qt::DirectConnection);
    connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));

    // We'll have multiple clients, we want to know which is which
    qDebug() << socketDescriptor << " Client connected";
    // make this thread a loop,
    // thread will stay alive so that signal/slot to function properly
    // not dropped out in the middle when thread dies
    exec();
}

void Threader::readyRead()
{
    // get the information
    QByteArray Data = socket->readAll();

    // will write on server side window
    //qDebug() << socketDescriptor << " Data in: " << Data;

    /*while (socket->state()) {
        socket->write("Hi\n");
        socket->waitForBytesWritten(1000);
        //qDebug() << "State"<< socket->state();
        usleep(100000);
    }*/

    m_videoClient = new QTcpSocket();
    QByteArray ContentType = ("HTTP/1.0 200 OK\r\nServer: WDA MJPEG Server\r\nConnection: close\r\nMax-Age: 0\r\nExpires: 0\r\nCache-Control: no-cache, private\r\nPragma: no-cache\r\nContent-Type: multipart/x-mixed-replace; boundary=--BoundaryString\r\n\r\n");
    socket->write(ContentType);

    //qDebug() << Q_FUNC_INFO << "2";
    m_videoClient->connectToHost("127.0.0.1",m_xport.toInt());
    while(socket->isOpen()){

        QByteArray videoData;

        if(m_videoClient->waitForConnected(5000))
        {
            qDebug() << "video feed connected";
            m_videoClient->write("Hi");
            m_videoClient->waitForBytesWritten(1000);
            m_videoClient->waitForReadyRead(3000);
            qDebug() << "Reading: " << m_videoClient->bytesAvailable();
            // get the data
            videoData = m_videoClient->readAll();
            if(videoData.contains("WDA MJPEG Server"))
            {
                qDebug() << "Removing Header";
                videoData.remove(0,ContentType.length());
            }
            qDebug() << videoData.length();
            if(videoData.length()>0)
            {
                socket->write(videoData);
                socket->flush();
                videoflag = true;
            }
        }
        else if(!videoflag)
        {
            qDebug() << Q_FUNC_INFO << "Screen capture";
            ScreenCaputure();
            if(!idevicescreenshotsflag){
                QByteArray BoundaryString = ("--BoundaryString\r\n" \
                                             "Content-Type: image/jpeg\r\n" \
                                             "Content-Length: ");
                BoundaryString.append(QString::number(ScreencapImg.length()));
                BoundaryString.append("\r\n\r\n");
                socket->write(BoundaryString);
                socket->write(ScreencapImg);
                // Write The Encoded Image
                socket->flush();

            }else{

                QByteArray BoundaryString = ("--BoundaryString\r\n" \
                                             "Content-Type: image/jpeg\r\n" \
                                             "Content-Length: ");
                BoundaryString.append(QString::number(BoundaryString.length()));
                BoundaryString.append("\r\n\r\n");
                socket->write(BoundaryString);
                socket->write(CurrentImg);
                // Write The Encoded Image
                socket->flush();
                idevicescreenshotsflag = false;
                videoflag = false;
            }
        }
        QThread::msleep(500);
        m_videoClient->connectToHost("127.0.0.1",m_xport.toInt());
    }
}

void Threader::disconnected()
{
    qDebug() << socketDescriptor << " Disconnected";
    socket->deleteLater();
    exit(0);
}

void Threader::ScreenCaputure(){
    idevice_t device = NULL;
    lockdownd_client_t lckd = NULL;
    lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
    screenshotr_client_t shotr = NULL;
    lockdownd_service_descriptor_t service = NULL;
    int result = -1;
    char *imgdata = NULL;
    int i;
    char *udid = NULL;
    char *filename = NULL;

    QImage pngimg,jpgimg;
    const char *fileext = NULL;
    QString savefilename, loadfile;

    QByteArray ba = m_udid.toLatin1();
    udid = ba.data();

    if (IDEVICE_E_SUCCESS != idevice_new(&device, udid)) {


        if (udid) {
            idevicescreenshotsflag = true;
            printf("No device found with udid %s, is it plugged in?\n", udid);
        } else {
            printf("No device found, is it plugged in?\n");
        }
        return ;
    }

    if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(device, &lckd, NULL))) {
        idevice_free(device);
        printf("ERROR: Could not connect to lockdownd, error code %d\n", ldret);
        return ;
    }
    lockdownd_start_service(lckd, "com.apple.mobile.screenshotr", &service);
    lockdownd_client_free(lckd);
    if (service && service->port > 0) {
        if (screenshotr_client_new(device, service, &shotr) != SCREENSHOTR_E_SUCCESS) {
            printf("Could not connect to screenshotr!\n");
        } else {
            uint64_t imgsize = 0;
            if (screenshotr_take_screenshot(shotr, &imgdata, &imgsize) == SCREENSHOTR_E_SUCCESS) {
                if (!filename) {

                    if (memcmp(imgdata, "\x89PNG", 4) == 0) {
                        fileext = ".png";
                    } else if (memcmp(imgdata, "MM\x00*", 4) == 0) {
                        fileext = ".tiff";
                    } else {
                        printf("WARNING: screenshot data has unexpected image format.\n");
                        fileext = ".dat";
                    }

                    /*
                    printf(udid);
                    time_t now = time(NULL);
                    filename = (char*)malloc(36);
                    size_t pos = strftime(filename, 36, "screenshot-%Y-%m-%d-%H-%M-%S", gmtime(&now));
                    sprintf(filename+pos, "%s", fileext);
                    */
                }
                savefilename = "/tmp/";
                savefilename.append(udid);
                savefilename.append(fileext);

                loadfile = "/tmp/";
                loadfile.append(udid);
                loadfile.append(".jpg");

                QByteArray ba = savefilename.toLatin1();
                filename = ba.data();

                /*
                printf(filename);
                printf("\n");
                filename = "/tmp/screencap.png";
                */

                FILE *f = fopen(filename, "wb");
                if (f) {
                    if (fwrite(imgdata, 1, (size_t)imgsize, f) == (size_t)imgsize) {
                        //printf("Screenshot saved to %s\n", filename);

                        result = 0;
                    } else {
                        printf("Could not save screenshot to file %s!\n", filename);
                        idevicescreenshotsflag = true;

                    }
                    fclose(f);
                    pngimg.load(filename);
                    jpgimg = pngimg.convertToFormat(QImage::Format_RGB888);
                    jpgimg.save(loadfile);
                    QImage img_enrll;
                    img_enrll.load(loadfile);
                    QBuffer buffer(&ScreencapImg);
                    buffer.open(QIODevice::WriteOnly);
                    img_enrll.save(&buffer, "JPEG");
                } else {
                    printf("Could not open %s for writing: %s\n", filename, strerror(errno));
                }
            } else {
                printf("Could not get screenshot data !\n");
                idevicescreenshotsflag = true;
            }
            screenshotr_client_free(shotr);
        }
    } else {
        printf("Could not start screenshotr service! Remember that you have to mount the Developer disk image on your device if you want to use the screenshotr service.\n");
    }
    if (service)
        lockdownd_service_descriptor_free(service);
    idevice_free(device);
    return ;
}

