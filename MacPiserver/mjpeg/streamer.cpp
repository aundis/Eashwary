#include "streamer.h"
#include "threader.h"

#include <QThread>
#include <QtDebug>
#include <QImage>
#include <QBuffer>
#include <QTime>
#include <QPainter>

Streamer::Streamer(QString udid, QString port, QString xPort, int width, int height, QString OverlayPath,QObject *parent) : QTcpServer(parent),
                                      m_port(port),m_xport(xPort)
{

    m_udid = udid;
    //qDebug() << width << height;
    if(createImage(width,height,OverlayPath, ImageFile)){
    QString filename = QString("/tmp/testImage.jpg");

//            filename = "new1.jpg" ;
//            filename = "screen.jpg" ;

    //qDebug() << filename << QTime::currentTime().toString() ;

    QThread::msleep(200);

    QImage img_enrll;
    img_enrll.load(filename);
    QBuffer buffer(&CurrentImg);
    buffer.open(QIODevice::WriteOnly);
    img_enrll.save(&buffer, "JPEG");
    }
    StartServer();
    //m_videoClient = new QTcpSocket(this);
    qDebug() << "Waiting For Connection";
}


void Streamer::StartServer(){

    //m_TcpHttpServer = new QTcpServer();

    /*m_TcpHttpServer->connect(m_TcpHttpServer,
                             SIGNAL(newConnection()),
                             this,
                             SLOT(TcpHttpconnected()));
    */
    if(!this->listen(QHostAddress::Any,
                            m_port.toInt())){
        qDebug() << "Could not start server";
    }
    else
    {
        qDebug() << "Listening to port " << m_port << "...";
    }
}
void Streamer::incomingConnection(qintptr socketDescriptor)
{
    // We have a new connection
    qDebug() << socketDescriptor << " Connecting...";

    Threader *thread = new Threader(m_udid, socketDescriptor, m_xport, CurrentImg, this);

    // connect signal/slot
    // once a thread is not needed, it will be beleted later
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();
}

bool Streamer::createImage(int width, int height, QString overlayImagePath, QImage *image)
{
    QImage overlayLogoff(overlayImagePath);
    image = new QImage(width,height,QImage::Format_RGB32);
    //QImage image(QSize();
    QPainter painter(image);
    painter.setBrush(QBrush(Qt::white));
    painter.fillRect(QRectF(0,0,image->width(),image->height()),Qt::white);
    painter.drawImage((image->width()-overlayLogoff.width())/2,(image->height()-overlayLogoff.height())/2,overlayLogoff);
    image->save("/tmp/testImage.jpg");
    return true;

}
