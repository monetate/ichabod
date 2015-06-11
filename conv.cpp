#include "conv.h"
#include "agif.h"
#include <QApplication>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QWebPage>
#include <QWebFrame>
#include <QWebElement>
#include <QDateTime>
#include <iostream>
#include <algorithm> 
#include <time.h>

// overload to allow QString output
std::ostream& operator<<(std::ostream& str, const QString& string) 
{
    return str << string.toLocal8Bit().constData();
}


Converter::Converter(const Engine* engine, const Settings & s)
    : settings(s)
    , activePage(0)
{
    // engine communication to and fro
    connect(engine, SIGNAL(javascriptEnvironment(QWebPage*)), this, SLOT(slotJavascriptEnvironment(QWebPage*)));
    connect(engine, SIGNAL(warning(QString)), this, SLOT(slotJavascriptWarning(QString)));
    connect(engine, SIGNAL(error(QString)), this, SLOT(slotJavascriptError(QString)));
    connect(this, SIGNAL(done(int)), engine, SLOT(stop(int)));
}

Converter::~Converter()
{
}

void Converter::setTransparent( bool t )
{
    settings.transparent = t;
}

void Converter::setQuality( int q )
{
    settings.quality = q;
}

void Converter::setScreen( int w, int h )
{
    settings.screen_width = w;
    settings.screen_height = h;
}

void Converter::setFormat( const QString& fmt )
{
    settings.fmt = fmt;
}

void Converter::setLooping( bool l )
{
    settings.looping = l;
}

void Converter::setQuantizeMethod( const QString& method )
{
    if ( method == "DIFFUSE" )
    {
        settings.quantize_method = QuantizeMethod_DIFFUSE;
    }
    if ( method == "THRESHOLD" )
    {
        settings.quantize_method = QuantizeMethod_THRESHOLD;
    }
    if ( method == "ORDERED" )
    {
        settings.quantize_method = QuantizeMethod_ORDERED;
    }
    if ( method == "MEDIANCUT" )
    {
        settings.quantize_method = QuantizeMethod_MEDIANCUT;
    }
    if ( method == "MEDIANCUT_FLOYD" )
    {
        settings.quantize_method = QuantizeMethod_MEDIANCUT_FLOYD;
    }
}

void Converter::slotJavascriptEnvironment(QWebPage* page)
{
    images.clear();
    delays.clear();
    warningvec.clear();
    errorvec.clear();

    activePage = page;
    // install custom css, if present
    if ( settings.css.length() )
    {
        QByteArray ba = settings.css.toUtf8().toBase64();
        QString data("data:text/css;charset=utf-8;base64,");
        QUrl css_data_url(data + QString(ba.data()));
        activePage->settings()->setUserStyleSheetUrl(css_data_url);
    }
    // register the current environment
    activePage->mainFrame()->addToJavaScriptWindowObject(settings.rasterizer, this);
}

void Converter::slotJavascriptWarning(QString s )
{
    warningvec.push_back( s );
}


void Converter::slotJavascriptError(QString s )
{
    errorvec.push_back( s );
}

void Converter::snapshotElements( const QStringList& ids, int msec_delay )
{
    if ( !crops.size() )
    {
        snapshotPage( msec_delay );
        return;
    }
    QWebFrame* frame = activePage->mainFrame();
    // calculate largest rect encompassing all elements
    QRect crop_rect;
    for( QStringList::const_iterator it = ids.begin();
         it != ids.end();
         ++it )
    {
        QMap<QString,QVariant> crop = frame->evaluateJavaScript( QString("document.getElementById('%1').getBoundingClientRect()").arg(*it) ).toMap();    
        QRect r = QRect( crop["left"].toInt(), crop["top"].toInt(),
                         crop["width"].toInt()+1, crop["height"].toInt() );
        if ( r.isValid() )
        {
            r.adjust( -1 * (std::min(15,r.x())), -1 * (std::min(15,r.y())), 15, 15 ); // padding to account for possible font overflow
            if ( crop_rect.isValid() )
            {
                crop_rect = crop_rect.united( r );
            }
            else
            {
                crop_rect = r;
            }
        }
    }
    internalSnapshot( msec_delay, crop_rect );
}


void Converter::snapshotPage(int msec_delay)
{
    QRect invalid;
    internalSnapshot( msec_delay, invalid );
}

void Converter::internalSnapshot( int msec_delay, const QRect& crop )
{
    QWebFrame* frame = activePage->mainFrame();
    frame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);

    // Calculate a good width for the image
    int highWidth=settings.screen_width;
    activePage->setViewportSize(QSize(highWidth, 10));
    if (settings.smart_width && frame->scrollBarMaximum(Qt::Horizontal) > 0) 
    {
        if (highWidth < 10)
        {
            highWidth=10;
        }
        int lowWidth=highWidth;
        while (frame->scrollBarMaximum(Qt::Horizontal) > 0 && highWidth < 32000) 
        {
            lowWidth = highWidth;
            highWidth *= 2;
            activePage->setViewportSize(QSize(highWidth, 10));
        }
        while (highWidth - lowWidth > 10) 
        {
            int t = lowWidth + (highWidth - lowWidth)/2;
            activePage->setViewportSize(QSize(t, 10));
            if (frame->scrollBarMaximum(Qt::Horizontal) > 0)
            {
                lowWidth = t;
            }
            else
            {
                highWidth = t;
            }
        }
        activePage->setViewportSize(QSize(highWidth, 10));
    }
    activePage->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    //Set the right height
    if (settings.screen_height > 0)
    {
        activePage->setViewportSize(QSize(highWidth, settings.screen_height));
    }
    else
    {
        int content_height = frame->contentsSize().height();
        if ( content_height <= activePage->viewportSize().height() )
        {
            // Sanity check for those sites which deliberately mess
            // with body.scrollHeight. Check all top-level elements
            // and figure out the total scroll height. This code
            // should only fire on a small minority of sites.
            QString s = "a = document.body.childNodes;"
                "var max_child_height = 0;"
                "for ( var i in a ) {"
                "  e = a[i];"
                "  if (typeof e.scrollHeight != 'undefined') {max_child_height+=e.scrollHeight;}"
                "}"
                "max_child_height;";
            int mh = frame->evaluateJavaScript( s ).toInt();
            if ( mh > content_height )
            {
                content_height = mh;
            }
        }
        activePage->setViewportSize(QSize(highWidth, content_height));
    }

    QPainter painter;
    QImage image;

    QRect rect = QRect(QPoint(0,0), activePage->viewportSize());
    if (rect.width() == 0 || rect.height() == 0) 
    {
        //?
    }

    image = QImage(rect.size(), QImage::Format_ARGB32_Premultiplied); // draw as fast as possible
    painter.begin(&image);

    if (settings.transparent) 
    {
        QWebElement e = frame->findFirstElement("body");
        e.setStyleProperty("background-color", "transparent");
        e.setStyleProperty("background-image", "none");
        QPalette pal = activePage->palette();
        pal.setColor(QPalette::Base, QColor(Qt::transparent));
        activePage->setPalette(pal);
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(QRect(QPoint(0,0),activePage->viewportSize()), QColor(0,0,0,0));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    } 
    else 
    {
        painter.fillRect(QRect(QPoint(0,0),activePage->viewportSize()), Qt::white);
    }
    
    painter.translate(-rect.left(), -rect.top());
    frame->render(&painter);
    painter.end();
    
    images.push_back(image);
    delays.push_back(msec_delay);
    crops.push_back( crop );
}

void Converter::saveToOutput()
{
    if ( !images.size() )
    {
        warningvec.push_back( "saveToOutput forcing snapshotPage" );
        snapshotPage();
    }
    if ( settings.convert_verbosity )
    {
        std::cout << "convert: images: " << images.size() << std::endl;
    }
    if ( settings.fmt == "gif" )
    {
        gifWrite( settings.quantize_method, images, delays, crops, settings.out, settings.looping );
    }
    else
    {
        QImage img = images.last();
        // selector, optionally creates initial crop
        if ( settings.selector.length() )
        {
            QWebFrame* frame = activePage->mainFrame();
            QWebElement el = frame->findFirstElement( settings.selector );
            QMap<QString,QVariant> crop = el.evaluateJavaScript( QString("this.getBoundingClientRect()") ).toMap();
            QRect r = QRect( crop["left"].toInt(), crop["top"].toInt(),
                             crop["width"].toInt(), crop["height"].toInt() );
            if ( settings.convert_verbosity )
            {
                std::cout << "convert: selector: " << settings.selector << std::endl;
                std::cout << "convert: selector rect: " << crop["left"].toInt() << "," << crop["top"].toInt() 
                          << " " <<crop["width"].toInt() << "x" <<crop["height"].toInt() << " valid:" << r.isValid() << std::endl;
            }
            if (r.isValid())
            {
                img = img.copy(r);
            }
            else
            {
                img = QImage();
            }
        }
        if ( settings.convert_verbosity )
        {
            std::cout << "convert: crop rect: " << settings.crop_rect.x() << "," << settings.crop_rect.y() 
                      << " " << settings.crop_rect.width() << "x" << settings.crop_rect.height() << std::endl;
        }
        // actual cropping, relative to whatever img is now
        if ( settings.crop_rect.isValid() )
        {
            img = img.copy(settings.crop_rect);
        }
        QFile file;
        file.setFileName(settings.out);
        bool openOk = file.open(QIODevice::WriteOnly);
        if (!openOk) {
            QString err = QString("Failure to open output file: %1").arg(settings.out);
            errorvec.push_back(err);
            std::cerr << err.toLocal8Bit().constData() << std::endl;            
        }
        if ( !img.save(&file,settings.fmt.toLocal8Bit().constData(), settings.quality) )
        {
            QString err = QString("Failure to save output file: %1 as %2 img: %3x%4").arg(settings.out).arg(settings.fmt).arg(img.width()).arg(img.height());
            errorvec.push_back(err);
            std::cerr << err.toLatin1().constData() << std::endl;
        }
    }
    emit done(errorvec.size());
}

void Converter::setSelector( const QString& sel )
{
    settings.selector = sel;
}

void Converter::setCss( const QString& css )
{
    settings.css = css;
}

void Converter::setCropRect( int x, int y, int w, int h )
{
    settings.crop_rect = QRect(x,y,w,h);
}

void Converter::setSmartWidth( bool sw )
{
    settings.smart_width = sw;
}

QVector<QString> Converter::warnings() const
{
    return warningvec;
}

QVector<QString> Converter::errors() const
{
    return errorvec;
}
