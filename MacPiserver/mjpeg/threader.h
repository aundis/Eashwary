#ifndef THREADER_H
#define THREADER_H

#include <QThread>
#include <QTcpSocket>
#include <QDebug>
#include <QBuffer>
#include<QPixmap>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include<QImage>
#include<QFile>
#include<QDataStream>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>



class Threader : public QThread
{
    Q_OBJECT
public:
    explicit Threader(QString xudid,qintptr ID, QString m_xport, QByteArray CurrentImg, QObject *parent = 0);

    void run();

signals:
    void error(QTcpSocket::SocketError socketerror);

public slots:
    void readyRead();
    void disconnected();
    void ScreenCaputure();


private:
    QTcpSocket      *socket;
    qintptr         socketDescriptor;
    QTcpSocket*     m_videoClient;
    QByteArray      CurrentImg;
    QByteArray      ScreencapImg;
    QString         m_xport;
    QString         m_udid;
    bool            videoflag = false;
    bool            idevicescreenshotsflag = false;


};


#endif // THREADER_H
