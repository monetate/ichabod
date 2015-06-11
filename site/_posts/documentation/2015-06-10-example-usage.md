---
layout: doc
title: Example Usage
category: doc
---

## Render raw HTML


    python -c \
    "import requests;\
     r = requests.get('http://localhost:7777',\
           data={'html':'<html><head></head><body><h1 id="head">Example: Render raw HTML</h1><p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.</body></html>',\
                'width':250,\
                'height':600,\
                'output':'/tmp/example-html.png',\
                'js':'(function(){ichabod.snapshotPage();ichabod.saveToOutput();})()'});\
     print r.text"

Result:

    {
       "conversion" : true,
       "convert_elapsed" : 6.615,
       "errors" : null,
       "path" : "/tmp/example-html.png",
       "result" : null,
       "run_elapsed" : 8.529999999999999,
       "warnings" : null
    }     

## Render remote URL

    python -c \
    "import requests;\
     r = requests.get('http://localhost:7777',\
           data={'url':'http://ichabod.org',
                'width':500,\
                'height':500,\
                'output':'/tmp/example-url.png',\
                'js':'(function(){ichabod.snapshotPage();ichabod.saveToOutput();})()'});\
     print r.text"

*Note*: Overall execution time increases according to the response time of the external resource(s).

Result:

    {
       "conversion" : true,
       "convert_elapsed" : 20.758,
       "errors" : null,
       "path" : "/tmp/example-url.png",
       "result" : null,
       "run_elapsed" : 619.924,
       "warnings" : null
    }

## Render portion of a page

    python -c \
    "import requests;\
     r = requests.get('http://localhost:7777',\
           data={'html':'<html><head></head><body><h1 id="head">Example: Render portion of HTML</h1><p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.</body></html>',\
                'width':200,\
                'height':100,\
                'output':'/tmp/example-portion.png',\
                'js':'(function(){ichabod.snapshotElements(\"#head\");ichabod.saveToOutput();})()'});\
     print r.text"

Result:

    {
       "conversion" : true,
       "convert_elapsed" : 4.462,
       "errors" : null,
       "path" : "/tmp/example-portion.png",
       "result" : null,
       "run_elapsed" : 23.528,
       "warnings" : null
    }

## Render animated image

    python -c \
    "import requests;\
     r = requests.get('http://localhost:7777',\
           data={'html':'<html><head></head><body style=\"background-color: red;\"><div id=\"word\" style=\"background-color: blue; color: darkgrey;\">hello</div></body></html>',
                'width':200,\
                'height':200,\
                'format': 'gif'
                'output':'/tmp/example-anim.gif',\
                'js':'(function(){ichabod.setTransparent(0); ichabod.snapshotPage(); document.getElementById(\"word\").innerHTML=\"world\"; ichabod.snapshotPage(); ichabod.saveToOutput();})()'});\
     print r.text"



