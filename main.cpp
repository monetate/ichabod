#include <iostream>
#include <string>

#include <QApplication>
#include <QString>
#include <QtCore>
#include <QTemporaryFile>
#include <QTextStream>
#include <QImage>
#include <QFileInfo>
#include <QProxyStyle>

#include "mongoose.h"
#include "ppm.h"

#include "json/json.h"
#include "statsd_client.h"

#include "version.h"
#include "conv.h"
#include "quant.h"
#include "engine.h"

#define ICHABOD_NAME "ichabod"
#define LOG_STRING "%1 %2x%3 %4 %5 %6 %7x%8+%9+%10 [%11ms] [%12ms]" // input WxH format output selector croprect [convert_elapsedms] [run_elapsedms]

int g_verbosity = 0;
int g_engine_verbosity = 0;
int g_convert_verbosity = 0;
int g_slow_response_ms = 15 * 1000;
QString g_quantize = "MEDIANCUT";
statsd::StatsdClient g_statsd;

void log( struct mg_connection* conn, const char* extra )
{
    std::cerr << conn->uri << " - " << extra << std::endl;
}

// output error and send error back to client
static int send_error(struct mg_connection* conn, const char* err)
{
    log(conn, err);
    mg_send_status(conn, 500);
    mg_send_header(conn, "X-Error-Message", err);

    Json::Value root;
    root["path"] = Json::Value();
    root["elapsed"] = Json::Value();
    root["conversion"] = false;
    root["output"] = Json::Value();
    root["result"] = Json::Value();
    root["warnings"] = Json::Value();
    Json::Value js_errors;
    js_errors.append( err );
    root["errors"] = js_errors;

    Json::StyledWriter writer;
    std::string json = writer.write(root);
    mg_send_data(conn, json.c_str(), json.length());
    return MG_TRUE;
}

// either get the POST variable, or returning empty string
// WARNING: this function is not re-entrant
static QString get_var( struct mg_connection *conn, const char* var_name, const QString default_string = QString() )
{
    static char data[20971520];
    int r = mg_get_var(conn, var_name, data, sizeof(data));
    switch( r )
    {
    case -1: // not found
        return default_string;
    case -2: // too big
        return default_string;
    }
    return QString(data);
}

static void send_headers(struct mg_connection* conn)
{
    mg_send_header(conn, "Content-Type", "application/json");
    static const char kszFormat[] = "ddd, dd MMM yyyy HH:mm:ss 'GMT'";
    QDateTime now = QDateTime::currentDateTime();
    mg_send_header(conn, "Date", now.toString(kszFormat).toLocal8Bit().constData());
    mg_send_header(conn, "Server", QString("%1 %2").arg(ICHABOD_NAME).arg(ICHABOD_VERSION).toLocal8Bit().constData());
}


void debug_settings(const Settings& settings, const QString& script_result, 
                    const QVector<QString>& warnings, const QVector<QString>& errors, 
                    bool success_status, double run_elapsedms, double convert_elapsedms)
{
    int verbosity = settings.verbosity;
    if ( run_elapsedms > settings.slow_response_ms )
    {
        verbosity = 9999;
    }
    if ( verbosity )
    {
        std::cout << "      success: " << success_status << std::endl;
        std::cout << "           in: " << settings.in.toLocal8Bit().constData() << std::endl;
        std::cout << "          out: " << settings.out.toLocal8Bit().constData() << std::endl;
        std::cout << "       run ms: " << run_elapsedms << std::endl;
        std::cout << "   convert ms: " << convert_elapsedms << std::endl;
        std::cout << "     was slow: " << (run_elapsedms > settings.slow_response_ms) << std::endl;
        std::cout << "script result: " << script_result.toLocal8Bit().constData() << std::endl;            
        std::cout << "      quality: " << settings.quality << std::endl;
        std::cout << "     quantize: " << settings.quantize_method << std::endl;
        std::cout << "          fmt: " << settings.fmt.toLocal8Bit().constData() << std::endl;
        std::cout << "  transparent: " << settings.transparent << std::endl;
        std::cout << "  smart width: " << settings.smart_width << std::endl;
        std::cout << "       screen: " << settings.screen_width << "x" << settings.screen_height << std::endl;
        if ( settings.crop_rect.isValid() )
        {
            std::cout << "         crop: " << settings.crop_rect.x() << "," << settings.crop_rect.y()
                      << " " << settings.crop_rect.width() << "x" << settings.crop_rect.height() << std::endl;
        }
        if ( verbosity > 1 )
        {
            QFileInfo fi(settings.out);
            std::cout << "        bytes: " << fi.size() << std::endl;
            QImage img(settings.out, settings.fmt.toLocal8Bit().constData());
            std::cout << "         size: " << img.size().width() << "x" << img.size().height() << std::endl;
            //std::cout << "script result: " << scriptResult() << std::endl;
            for( QVector<QString>::const_iterator it = warnings.begin();
                 it != warnings.end();
                 ++it )
            {
                std::cout << "       warning: " << it->toLocal8Bit().constData() << std::endl;
            }
            for( QVector<QString>::const_iterator it = errors.begin();
                 it != errors.end();
                 ++it )
            {
                std::cout << "         error: " << it->toLocal8Bit().constData() << std::endl;
            }
        }
        if ( verbosity > 2 )
        {
            QFile fil_read(settings.in);
            fil_read.open(QIODevice::ReadOnly);
            QByteArray arr = fil_read.readAll();
            std::cout << "         html: " << arr.data() << std::endl;
            for( QList<QString>::const_iterator it = settings.run_scripts.begin();
                 it != settings.run_scripts.end();
                 ++it )
            {
                std::cout << "           js: " << it->toLocal8Bit().constData() << std::endl;
            }
            std::cout << "     selector: " << settings.selector.toLocal8Bit().constData() << std::endl;
            std::cout << "          css: " << settings.css.toLocal8Bit().constData() << std::endl;
        }
    }
}

static int handle_default(struct mg_connection *conn, Settings& settings)
{
    send_headers(conn);
    if ( !settings.run_scripts.size() )
    {
        return send_error(conn, "Internal error: no scripts");
    }

    // hook up converter to engine
    Engine engine(settings);
    Converter converter(&engine, settings);

    bool conversion_success = engine.run();

    // collect info
    QString result = engine.scriptResult();
    double run_elapsedms = engine.runTime();
    double convert_elapsedms = engine.convertTime();
    QVector<QString> warnings = converter.warnings();
    QVector<QString> errors = converter.errors();
    debug_settings( settings, result, warnings, errors, conversion_success, run_elapsedms, convert_elapsedms );

    // create json return
    Json::Value root;
    root["path"] = settings.out.toLocal8Bit().constData();

    Json::Reader reader;
    Json::Value result_root;
    bool parsingSuccessful = false;
    parsingSuccessful = reader.parse( result.toLocal8Bit().constData(), result_root );
    if ( !parsingSuccessful )
    {
        result_root = Json::Value();
    }
    root["result"] = result_root;
    root["conversion"] = conversion_success;
    root["run_elapsed"] = run_elapsedms;
    root["convert_elapsed"] = convert_elapsedms;
    Json::Value js_warnings;
    for( QVector<QString>::iterator it = warnings.begin();
         it != warnings.end();
         ++it )
    {
        js_warnings.append( it->toLocal8Bit().constData() );
    }
    root["warnings"] = js_warnings;
    Json::Value js_errors;
    for( QVector<QString>::iterator it = errors.begin();
         it != errors.end();
         ++it )
    {
        js_errors.append( it->toLocal8Bit().constData() );
    }
    root["errors"] = js_errors;
    Json::StyledWriter writer;
    std::string json = writer.write(root);
    mg_send_data(conn, json.c_str(), json.length());

    if ( settings.statsd )
    {
        settings.statsd->inc(settings.statsd_ns + "request");
    }

    QString xtra = QString(LOG_STRING).arg(settings.in).arg(settings.screen_width).arg(settings.screen_height).arg(settings.out).arg(settings.fmt)
        .arg(settings.selector.length()?settings.selector:"''")
        .arg(settings.crop_rect.width()).arg(settings.crop_rect.height()).arg(settings.crop_rect.x()).arg(settings.crop_rect.y())
        .arg(run_elapsedms).arg(convert_elapsedms);
    log( conn, xtra.toLocal8Bit().constData() );
    return MG_TRUE;
}

static int handle_health(struct mg_connection *conn)
{
    send_headers(conn);
    char health[4] = {(char)0xF0, (char)0x9F, (char)0x91, (char)0xBB};
    mg_send_data(conn, health, 4);
    return MG_TRUE;
}

// handler for all incoming connections
static int ev_handler(struct mg_connection *conn, enum mg_event ev) 
{
    if (ev == MG_REQUEST) 
    {
        QString canonical_path = "/";
        QStringList pathParts = QString(conn->uri).split("/", QString::SkipEmptyParts);
        if ( pathParts.size() )
        {
            canonical_path = pathParts.last();
        }
        if ( canonical_path == "health" )
        {
            return handle_health(conn);
        }

        QString format = get_var(conn, "format");
        QString html = get_var(conn, "html");
        QString js = get_var(conn, "js");
        QString rasterizer = get_var(conn, "rasterizer");
        QString output = get_var(conn, "output");
        QString url = get_var(conn, "url");
        bool transparent = get_var(conn, "transparent", "1").toInt();
        int width = get_var(conn, "width").toInt();
        int height = get_var(conn, "height", "-1").toInt();
        int crop_x = get_var(conn, "crop_x", "0").toInt();
        int crop_y = get_var(conn, "crop_y", "0").toInt();
        int crop_w = get_var(conn, "crop_w", "0").toInt();
        int crop_h = get_var(conn, "crop_h", "0").toInt();
        int smart_width = get_var(conn, "smart_width", "1").toInt();
        QString css = get_var(conn, "css");
        QString selector = get_var(conn, "selector");
        int load_timeout_msec = get_var(conn, "load_timeout", "0").toInt();
        int enable_statsd = get_var(conn, "enable_statsd", "0").toInt();
        std::string statsd_ns(get_var(conn, "statsd_ns").toLocal8Bit().constData());
        QRect crop_rect;
        if ( crop_x || crop_y || crop_w || crop_h )
        {
            crop_rect = QRect(crop_x, crop_y, crop_w, crop_h);
        }
        if ( !rasterizer.length() )
        {
            rasterizer = ICHABOD_NAME;
        }
        if ( !output.length() )
        {
            return send_error(conn, "No output specified");
        }
        if ( width < 1 )
        {
            return send_error(conn, "Bad dimensions");
        }
        if ( !html.length() && !url.length() )
        {
            return send_error(conn, "Empty document and no URL specified");
        }

        QString input;
        QTemporaryFile file(output + QString("_XXXXXX.html"));
        if ( html.length() ) {
            if ( !file.open() )
            {
                return send_error(conn, (QString("Unable to open:") + output 
                                         + QString("_XXXXXX.html")).toLocal8Bit().constData() );
            }
            QTextStream out(&file);
            out << html;
            out.flush();

            input = file.fileName();
        } else {
            input = url;
        }

        if ( format.startsWith(".") )
        {
            format = format.mid(1);
        }
        if ( format.isEmpty() )
        {
            format = "png";
        }

        Settings settings;
        settings.verbosity = g_verbosity;
        settings.engine_verbosity = g_engine_verbosity;
        settings.convert_verbosity = g_convert_verbosity;
        settings.slow_response_ms = g_slow_response_ms;
        settings.rasterizer = rasterizer;
        settings.fmt = format;
        settings.in = input;
        settings.quality = 50; // reasonable size/speed tradeoff by default
        settings.out = output;
        settings.screen_width = width;
        settings.virtual_width = width;
        settings.screen_height = height;
        settings.transparent = transparent;
        settings.looping = false;
        settings.quantize_method = toQuantizeMethod( g_quantize );
        settings.smart_width = smart_width;
        settings.crop_rect = crop_rect;
        settings.css = css;
        settings.selector = selector;
        settings.selector = selector;
        settings.load_timeout_msec = load_timeout_msec;
        QList<QString> scripts;
        scripts.append(js);
        settings.run_scripts = scripts;
        settings.statsd = 0;
        if ( enable_statsd )
        {
            settings.statsd_ns = statsd_ns + (statsd_ns.length() ? "." : "");
            settings.statsd = &g_statsd;
        }
        return handle_default(conn, settings);
    } 
    else if (ev == MG_AUTH) 
    {
        return MG_TRUE;
    }
    return MG_FALSE;
}

int main(int argc, char *argv[])
{
    bool gui = false;
    QApplication app(argc, argv, gui);
    QProxyStyle * style = new QProxyStyle();
    app.setStyle(style);

    struct statsd_info 
    {
        statsd_info() : host("127.0.0.1"), port(8125), ns(std::string(ICHABOD_NAME)+"."), enabled(false) {}
        std::string host;
        int port;
        std::string ns;
        bool enabled;
    };
    statsd_info statsd;

    int port = 9090;
    QStringList args = app.arguments();
    QRegExp rxPort("--port=([0-9]{1,})");
    QRegExp rxVerbose("--verbosity=([0-9]{1,})");
    QRegExp rxEngineVerbose("--engine-verbosity=([0-9]{1,})");
    QRegExp rxConvertVerbose("--convert-verbosity=([0-9]{1,})");
    QRegExp rxSlowResponseMs("--slow-response-ms=([0-9]{1,})");
    QRegExp rxQuantize("--quantize=([a-zA-Z]{1,})");
    QRegExp rxVersion("--version");
    QRegExp rxShortVersion("-v$");
    QRegExp rxStatsdHost("--statsd-host=([^ ]+)");
    QRegExp rxStatsdPort("--statsd-port=([0-9]{1,})");
    QRegExp rxStatsdNs("--statsd-ns=([^ ]+)");

    for (int i = 1; i < args.size(); ++i) {
        if (rxPort.indexIn(args.at(i)) != -1 )
        {
            port =  rxPort.cap(1).toInt();
        }
        else if (rxVerbose.indexIn(args.at(i)) != -1 ) 
        {
            g_verbosity = rxVerbose.cap(1).toInt();
        }
        else if (rxEngineVerbose.indexIn(args.at(i)) != -1 ) 
        {
            g_engine_verbosity = rxEngineVerbose.cap(1).toInt();
        }
        else if (rxConvertVerbose.indexIn(args.at(i)) != -1 ) 
        {
            g_convert_verbosity = rxConvertVerbose.cap(1).toInt();
        }
        else if (rxSlowResponseMs.indexIn(args.at(i)) != -1 ) 
        {
            g_slow_response_ms = rxSlowResponseMs.cap(1).toInt();
        }
        else if (rxQuantize.indexIn(args.at(i)) != -1 ) 
        {
            g_quantize = rxQuantize.cap(1);
        }
        else if (rxVersion.indexIn(args.at(i)) != -1 ) 
        {
            std::cout << ICHABOD_NAME << " version " << ICHABOD_VERSION << std::endl;
            std::cout << "** The GIFLIB distribution is Copyright (c) 1997  Eric S. Raymond" << std::endl;
            std::cout << "** ppmquant is Copyright (C) 1989, 1991 by Jef Poskanzer." << std::endl;
            std::cout << "** statsd-client-cpp is Copyright (c) 2014, Rex" << std::endl;
            std::cout << "**" << std::endl;
            std::cout << "** Permission to use, copy, modify, and distribute this software and its" << std::endl;
            std::cout << "** documentation for any purpose and without fee is hereby granted, provided" << std::endl;
            std::cout << "** that the above copyright notice appear in all copies and that both that" << std::endl;
            std::cout << "** copyright notice and this permission notice appear in supporting" << std::endl;
            std::cout << "** documentation.  This software is provided \"as is\" without express or" << std::endl;
            std::cout << "** implied warranty." << std::endl;
            std::cout << "**" << std::endl;
            ppm_init( &argc, argv );
            return 0;
        }
        else if (rxShortVersion.indexIn(args.at(i)) != -1)
        {
            std::cout << ICHABOD_VERSION << std::endl;
            return 0;
        }
        else if (rxStatsdHost.indexIn(args.at(i)) != -1)
        {
            statsd.host = rxStatsdHost.cap(1).toLocal8Bit().constData();
            statsd.enabled = true;
        }
        else if (rxStatsdPort.indexIn(args.at(i)) != -1)
        {
            statsd.port = rxStatsdPort.cap(1).toInt();
            statsd.enabled = true;
        }
        else if (rxStatsdNs.indexIn(args.at(i)) != -1)
        {
            statsd.ns = rxStatsdNs.cap(1).toLocal8Bit().constData();
            if ( statsd.ns.length() && *statsd.ns.rbegin() != '.' )
            {
                statsd.ns += ".";
            }
            statsd.enabled = true;
        }
        else 
        {
            std::cerr << "Unknown arg:" << args.at(i).toLocal8Bit().constData() << std::endl;
            return -1;
        }
    }

    ppm_init( &argc, argv );
    
    struct mg_server *server = mg_create_server(NULL, ev_handler);

    const char * err = mg_set_option(server, "listening_port", QString::number(port).toLocal8Bit().constData());
    if ( err )
    {
        std::cerr << "Cannot bind to port:" << port << " [" << err << "], exiting." << std::endl;
        return -1;
    }
    std::cout << ICHABOD_NAME << " " << ICHABOD_VERSION 
              << " (port:" << mg_get_option(server, "listening_port") 
              << " verbosity:" << g_verbosity 
              << " engine verbosity:" << g_engine_verbosity 
              << " convert verbosity:" << g_convert_verbosity 
              << " slow-response:" << g_slow_response_ms << "ms";
    if ( statsd.enabled )
    {
        std::cout << " statsd:" << statsd.host << ":" << statsd.port << "[" << statsd.ns << "]";
        g_statsd.config(statsd.host, statsd.port, statsd.ns);
    }
    std::cout << ")" << std::endl;

    for (;;) 
    {
        mg_poll_server(server, 1000);
    }
    
    mg_destroy_server(&server);
    
    return 0;
}
