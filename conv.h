#ifndef CONVERTER_H
#define CONVERTER_H

#include <QObject>
#include <QWebPage>
#include <QRect>
#include <utility>
#include <iostream>

#include "engine.h"
#include "quant.h"
#include "statsd_client.h"

class Converter : public QObject
{
    Q_OBJECT
public:
    Converter(const Engine* engine, const Settings & settings);
    ~Converter();
                
    QVector<QString> warnings() const;
    QVector<QString> errors() const;

public slots:
    void setTransparent( bool t );
    void setQuality( int q );
    void setScreen( int w, int h );
    void setFormat( const QString& fmt );
    void setLooping( bool l );
    void snapshotPage( int msec_delay = 100);
    void snapshotElements( const QStringList& ids, int msec_delay = 100 );
    void saveToOutput();
    void setQuantizeMethod( const QString& method ); // see quant.h
    void setSelector( const QString& sel );
    void setCss( const QString& css );
    void setCropRect( int x, int y, int w, int h );
    void setSmartWidth( bool sw );

private slots:
    void slotJavascriptEnvironment(QWebPage* page);
    void slotJavascriptWarning(QString s);
    void slotJavascriptError(QString s);

signals:
    void done(int exitcode);

private:
    Settings settings;
    QWebPage* activePage;
    QVector<QImage> images;
    QVector<int> delays;
    QVector<QRect> crops;
    QVector<QString> warningvec;
    QVector<QString> errorvec;
    void internalSnapshot( int msec_delay, const QRect& crop );
};

#endif
