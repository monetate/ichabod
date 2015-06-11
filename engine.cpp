#include "engine.h"
#include <iostream>
#include <QApplication>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QWebFrame>
#include <QWebElement>
#include <QTimer>
#include <QNetworkCookieJar>

#include <time.h>

#include "conv.h"
#include "quant.h"

Settings::Settings()
{
    in = "";
    fmt = "";
    quality = 50;
    out = "";
    screen_width = -1;
    virtual_width = 10;
    screen_height = -1;
    transparent = false;
    smart_width = true;
    load_timeout_msec = 0;
    min_font_size = -1;
    verbosity = 0;
    engine_verbosity = 0;
    convert_verbosity = 0;
    rasterizer = "ichabod";
    looping = false;
    quantize_method = toQuantizeMethod( "MEDIANCUT" );
    crop_rect = QRect();
    css = "";
    selector = "";
    slow_response_ms = 15000;
    statsd_ns = "ichabod";
    statsd = 0;
}

///////

NetAccess::NetAccess(const Settings& s)
    : settings(s)
{
}

NetAccess::~NetAccess()
{
}

QNetworkReply* NetAccess::createRequest(Operation op, const QNetworkRequest & req, QIODevice * outgoingData) 
{
    // block things here if necessary
    return QNetworkAccessManager::createRequest(op, req, outgoingData);
}

//////

WebPage::WebPage()
    : QWebPage()
{
}

WebPage::~WebPage()
{
}

void WebPage::javaScriptAlert(QWebFrame *, const QString & msg) 
{
    emit alert(QString("Javascript alert: %1").arg(msg));
}

bool WebPage::javaScriptConfirm(QWebFrame *, const QString & msg) 
{
    emit confirm (QString("Javascript confirm: %1").arg(msg));
    return true;
}

bool WebPage::javaScriptPrompt(QWebFrame *, const QString & msg, const QString & defaultValue, QString * result) 
{
    emit prompt (QString("Javascript prompt: %1 (%2)").arg(msg,defaultValue));
    result = (QString*)&defaultValue;
    return true;
}

void WebPage::javaScriptConsoleMessage(const QString & message, int lineNumber, const QString & sourceID) 
{
    emit console(QString("%1:%2 %3").arg(sourceID).arg(lineNumber).arg(message));
}

bool WebPage::shouldInterruptJavaScript() 
{
    emit alert("Javascript alert: slow javascript");
    return false;
}

bool WebPage::supportsExtension ( Extension extension ) const
{
    if (extension == QWebPage::ErrorPageExtension)
    {
        return true;
    }
    return false;
}

bool WebPage::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
    if (extension != QWebPage::ErrorPageExtension)
        return false;
    ErrorPageExtensionOption *errorOption = (ErrorPageExtensionOption*) option;
    if(errorOption->domain == QWebPage::QtNetwork)
    {
        emit alert(QString("Network error (%1)").arg(errorOption->error));
    }
    else if(errorOption->domain == QWebPage::Http)
    {
        emit alert(QString("HTTP error (%1)").arg(errorOption->error));
    }
    else if(errorOption->domain == QWebPage::WebKit)
    {
        emit alert(QString("WebKit error (%1)").arg(errorOption->error));
    }
    return false;
}

//////

Engine::Engine(const Settings& s)
    : web_page(0),
      settings(s),
      net_access(0),
      script_result(""),
      check_done_attempts(0),
      run_code(0),
      run_elapsedms(0.0),
      convert_elapsedms(0.0)
{

    // forward all interesting activity as signals for others to pick
    // up on

    web_page = new WebPage();
    net_access = new NetAccess(s);

    net_access->setCookieJar(new QNetworkCookieJar());
    connect(net_access, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)),
            this, SLOT(netSslErrors(QNetworkReply*, const QList<QSslError>&)));
    connect(net_access, SIGNAL(finished (QNetworkReply *)),
            this, SLOT(netFinished (QNetworkReply *) ) );
    connect(net_access, SIGNAL(warning(const QString &)),
            this, SLOT(netWarning(const QString &)));

    web_page->setNetworkAccessManager(net_access);

    connect(web_page, SIGNAL(loadStarted()), this, SLOT(webPageLoadStarted()));
    connect(web_page->mainFrame(), SIGNAL(loadFinished(bool)), this, SLOT(webPageLoadFinished(bool)));
    connect(web_page, SIGNAL(alert(const QString&)), SIGNAL(warning(const QString&)));
    connect(web_page, SIGNAL(confirm(const QString&)), SIGNAL(warning(const QString&)));
    connect(web_page, SIGNAL(prompt(const QString&)), SIGNAL(warning(const QString&)));
    connect(web_page, SIGNAL(console(const QString&)), SIGNAL(warning(const QString&)));
}

Engine::~Engine()
{
    delete web_page;
    delete net_access;
}

void Engine::netSslErrors(QNetworkReply *reply, const QList<QSslError> &) 
{
    // ignore all
    reply->ignoreSslErrors();
    emit warning("SSL error ignored");
}

void Engine::netWarning(const QString & message)
{
    emit warning(QString("Network warning: %1").arg(message));
}

void Engine::netFinished(QNetworkReply * reply) 
{
    int networkStatus = reply->error();
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if ((networkStatus != 0 && networkStatus != 5) || (httpStatus > 399))
    {
        emit error(QString("Failed to load %1").arg(reply->url().toString()));    
    }
}

void Engine::webPageLoadStarted()
{
    if ( settings.engine_verbosity )
    {
        std::cout << "engine: webPageLoadStarted" << std::endl;
    }
}

void Engine::webPageLoadFinished(bool b)
{
    if ( settings.engine_verbosity )
    {
        std::cout << "engine: webPageLoadFinished: " << b << std::endl;
    }
    if ( b )
    {
        checkDone();
    }
    else
    {
        emit error("Failed to load page");
        stop(-1);
    }
}

#define NANOS 1000000000LL
#define USED_CLOCK CLOCK_MONOTONIC

void Engine::loadDone()
{
    if ( settings.engine_verbosity )
    {
        std::cout << "engine: loadDone" << std::endl;
    }

    // let listeners get ready
    emit javascriptEnvironment(web_page);
    
    timespec time1, time2;
    clock_gettime(USED_CLOCK, &time1);
    long long start = time1.tv_sec*NANOS + time1.tv_nsec;

    // all work performed here
    foreach (const QString & str, settings.run_scripts) 
    {
        if ( settings.engine_verbosity )
        {
            std::cout << "engine: loadDone running: " << str.toLatin1().constData() << std::endl;
        }
        script_result = web_page->mainFrame()->evaluateJavaScript(str).toString();
    }

    clock_gettime(USED_CLOCK, &time2);
    long long diff = time2.tv_sec*NANOS + time2.tv_nsec - start;
    convert_elapsedms = (diff / 1000 + (diff % 1000 >= 500) /*round up halves*/) / 1000.0; 

    if ( settings.statsd )
    {
        settings.statsd->timing("convert", convert_elapsedms);
    }
}

void Engine::stop(int exitcode)
{
    run_code = exitcode;
    event_loop.exit();          // ensure run loop terminates
    event_loop.processEvents(); // permit cleanup events
}

void Engine::checkDone() 
{
    check_done_attempts++;
    bool is_done = true;
    const int wait_count = 20*5;
    if ( check_done_attempts < wait_count && settings.selector.length() && settings.selector != "body" )
    {
        if ( settings.engine_verbosity )
        {
            std::cout << "engine: checkDone: waiting for selector:" << settings.selector.toLatin1().constData() << std::endl;
        }

        // make sure element exists
        web_page->setViewportSize(QSize(settings.virtual_width, 10));
        QWebFrame* frame = web_page->mainFrame();
        QWebElement el = frame->findFirstElement( settings.selector );
        if ( el.isNull() )
        {
            if (check_done_attempts==1)
            {
                emit warning(QString("Unable to find element %1 after page load, continuing to search").arg(settings.selector));
            }
            if (check_done_attempts==(wait_count-1))
            {
                emit error(QString("Unable to find element %1").arg(settings.selector));
            }
            is_done = false;
            QTimer::singleShot(50, this, SLOT(checkDone()));
        }
        else
        {
            // make sure element is visible
            QMap<QString,QVariant> crop = el.evaluateJavaScript( QString("this.getBoundingClientRect()") ).toMap();
            QRect r = QRect( crop["left"].toInt(), crop["top"].toInt(),
                             crop["width"].toInt(), crop["height"].toInt() );       
            QSize s = web_page->viewportSize();
            if ( settings.engine_verbosity )
            {
                std::cout << "engine: checkDone: el.isNull:" << el.isNull() << " selector:" << settings.selector.toLatin1().constData() << " rect:" << crop["left"].toInt() << "," << crop["top"].toInt() << "," <<crop["width"].toInt() << "," <<crop["height"].toInt() << "," << std::endl;
            }
            if ( r.width() <= 0 || r.height() <= 0 )
            {
                if (check_done_attempts==1)
                {
                    emit warning(QString("Element %1 has invalid rect (%2x%3) after page load, waiting for resize").arg(settings.selector).arg(r.width()).arg(r.height()));
                }
                if (check_done_attempts==(wait_count-1))
                {
                    emit error(QString("Element %1 has invalid rect (%2x%3) after page load").arg(settings.selector).arg(r.width()).arg(r.height()));
                }
                is_done = false;
                QTimer::singleShot(50, this, SLOT(checkDone()));
            }
        }
    }
    if ( is_done )
    {
        loadDone();
    }
}



QString Engine::scriptResult() const
{
    return script_result;
}

double Engine::runTime() const
{
    return run_elapsedms;
}

double Engine::convertTime() const
{
    return convert_elapsedms;
}

void Engine::loadTimeout()
{
    if ( settings.engine_verbosity )
    {
        std::cout << "engine: load timeout" << std::endl;
    }
    emit error(QString("Incomplete load: %1").arg(settings.in));
    stop(-2);
}

bool Engine::run()
{
    if ( settings.load_timeout_msec )
    {
        if ( settings.engine_verbosity )
        {
            std::cout << "engine: load_timeout_msec: " << settings.load_timeout_msec << std::endl;
        }
        QTimer::singleShot(settings.load_timeout_msec, this, SLOT(loadTimeout()));
    }
    timespec time1, time2;
    clock_gettime(USED_CLOCK, &time1);
    long long start = time1.tv_sec*NANOS + time1.tv_nsec;

    QUrl url = QUrl::fromUserInput(settings.in);
    if ( !url.isValid() )
    {
        stop(-3);
        return false;
    }
    QNetworkRequest r = QNetworkRequest(url);

    web_page->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    web_page->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    web_page->mainFrame()->load(r);

    if ( settings.engine_verbosity )
    {
        std::cout << "engine: run: " << settings.in.toLatin1().constData() << std::endl;
    }
    event_loop.exec(); // sets run_code, evaluates javascript, calculates elapsed time

    clock_gettime(USED_CLOCK, &time2);
    long long diff = time2.tv_sec*NANOS + time2.tv_nsec - start;
    run_elapsedms = (diff / 1000 + (diff % 1000 >= 500) /*round up halves*/) / 1000.0; 

    return !run_code;
}
