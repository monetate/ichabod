---
layout: doc
title: Getting Started
category: doc
---

Please ensure the the Ichabod executable is available somewhere in the
PATH. Run ichabod on a known port, with verbosity turned on in order
to watch the activity:

    ichabod --port=7777 --verbosity=1

Note: below we are using python requests to make requests to
ichabod. There are also useful [python bindings]
({% post_url documentation/2015-06-11-python-bindings %}) available.

## Hello, World!

Ichabod accepts POSTed JSON on the listening port. The JSON should
contain at a minum the `html` or `url` field, as well as the `width`,
`height`, `format`, `output` and `js` fields.

- **`html`** - Raw HTML code to be rasterized
- **`url`** - URL from which HTML will be fetched and rasterized
- **`width`** - Width of the virtual browser screen
- **`height`** - Height of the virtual browser screen
- **`format`** - Output rasterization format (e.g., `png` or `gif`)
- **`output`** - Path to local filename where output is written
- **`js`** - javascript to execute once the HTML is loaded


Example:


    python -c \
    "import requests;\
     r = requests.get('http://localhost:7777',\
           data={'html':'<html><head></head><body>helloworld</body></html>',\
                 'width':100,\
                 'height':100,\
                 'output':'/tmp/helloworld.png',\
                 'js':'(function(){ichabod.snapshotPage();ichabod.saveToOutput();})();'});\
     print r.text"


After running the above, ichabod will output the rasterized image, and
also report the amount of time spent processing. A result is returned,
also in JSON format, suitable for consumption by a client. For
example:

    {
       "conversion" : true,
       "convert_elapsed" : 2.303,
       "errors" : null,
       "path" : "/tmp/helloworld.png",
       "result" : null,
       "run_elapsed" : 14.536,
       "warnings" : null
    }

Elapsed processing times are reported for the overall time
(`run_elapsed`) as well as the time spent actually rasterizing
(`convert_elapsed`). 

Each element of the POSTed request serves a purpose. Most are
self-explanatory, however the `js` field deserves some comment. After
loading the HTML, ichabod executes whatever javascript code is
supplied in the `js` field. Security should be kept in mind when
running ichabod on unprotected networks. If no `js` is specified, or
the javascript performs no useful actions, then ichabod does nothing
(the HTML will still be rendered in memory). In order to output
rasterized files, use the `ichabod` object which is injected into the
javascript runtime environment. There are several ways in which the
page, or portions of the page, can be snapshotted before being saved
to disk. See the [API]({% post_url documentation/2015-06-10-api %})
details for further information.

If there are any errors during processing, they will be reported to
the client. Here we neglect to provide the width and height of the
virtual screen. Note that `errors` contains text of the problem and
`conversion` is set to false since no rasterized output was produced.


    python -c \
    "import requests;\
     r = requests.get('http://localhost:7777',\
           data={'html':'<html><head></head><body>helloworld</body></html>',\
                 'output':'/tmp/helloworld.png',\
                 'js':'(function(){ichabod.snapshotPage();ichabod.saveToOutput();})();'});\
     print r.text"

Result:

    {
       "conversion" : false,
       "elapsed" : null,
       "errors" : [ "Bad dimensions" ],
       "output" : null,
       "path" : null,
       "result" : null,
       "warnings" : null
    }


## Return Value

In addition to rasterizing HTML, ichabod can also perform other work
and return values to the calling client. The return value must be able
to be encoded as a JSON string, which will be sent down the wire to
the client in the `result` field. For example:

    python -c \
    "import requests;\
     r = requests.get('http://localhost:7777',\
           data={'html':'<html><head></head><body>helloworld</body></html>',\
                 'width':100,\
                 'height':100,\
                 'output':'/tmp/helloworld.png',\
                 'js':'(function(){ichabod.snapshotPage();ichabod.saveToOutput();return JSON.stringify({"answer":1+1})})();'});\
     print r.text"

Result:

     {
        "conversion" : true,
        "convert_elapsed" : 1.931,
        "errors" : null,
        "path" : "/tmp/helloworld.png",
        "result" : {
          "answer" : 2
         },
         "run_elapsed" : 3.151,
         "warnings" : null
     }
     

