#ifndef STREAMER_H
#define STREAMER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QImage>


class Streamer : public QTcpServer
{
    Q_OBJECT
public:
    explicit Streamer(QString udid, QString port, QString xPort, int width, int height, QString overlayPath, QObject *parent = 0 );
    void StartServer();
    bool createImage(int width, int height, QString overlayImagePath, QImage *BackGroundImage);

signals:

public slots:
    //void TcpHttpconnected();
    //void TcpHttpreadyRead();
protected:
    void incomingConnection(qintptr socketDescriptor);


private:
    //QTcpServer*         m_TcpHttpServer;
    //QTcpSocket*         m_TcpHttpClient ;
    //QTcpSocket*         m_videoClient;
    QString              m_port;
    QString              m_xport;
    QImage               *ImageFile;
    QByteArray           CurrentImg;
    QString               m_udid;

};

#endif // STREAMER_H
