#!/bin/bash

VERBOSITY=0
PORT=19090


HELLO_FILE=hello.png
ANIM_FILE=hello.gif
ichabod_pid=-1

function cleanup()
{
    rm -f $HELLO_FILE
    rm -f $ANIM_FILE
    while sleep 1
          echo Killing ichabod pid $ichabod_pid on port $PORT
          kill -0 $ichabod_pid >/dev/null 2>&1
    do
        kill $ichabod_pid > /dev/null 2>&1
    done
}

function die()
{
    echo -e "\e[31mERROR: $1\e[0m"
    cleanup
    exit 666    
}

set -e
function error_handler() {
    die "Error trapped during testing!"
}
 
trap error_handler EXIT

./ichabod --verbosity=$VERBOSITY --port=$PORT &
ichabod_pid=$!
sleep 1


function test_simple()
{
    # simple image
    HELLO=$(curl -s -X POST http://localhost:$PORT --data "html=<html><head></head><body style='background-color: red;'><div style='background-color: blue; color: white;'>helloworld</div></body></html>&width=100&height=100&format=png&js=(function(){ichabod.snapshotPage();ichabod.saveToOutput();})();&output=$HELLO_FILE")
    test `echo $HELLO | jq '.conversion'` == "true"  || die "Conversion failed: $HELLO"
    ls $HELLO_FILE > /dev/null || die "Hello world result file missing: [$HELLO_FILE]"

    # animated image
    ANIM=$(curl -s -X POST http://localhost:$PORT --data "html=<html><head></head><body style='background-color: red;'><div id='word' style='background-color: blue; color: darkgrey;'>hello</div></body></html>&width=100&height=100&format=gif&output=$ANIM_FILE&js=(function(){ichabod.setTransparent(0); ichabod.snapshotPage(); document.getElementById('word').innerHTML='world'; ichabod.snapshotPage(); ichabod.saveToOutput(); return 42;})();")
    test `echo $ANIM | jq '.conversion'` == "true"  || die "Conversion failed: $ANIM"
    test `echo $ANIM | jq '.result'` == "42"  || die "Invalid result: $ANIM"
    ls $ANIM_FILE > /dev/null || die "Animated result file missing: [$ANIM_FILE]"
    return 0
}

function test_wait()
{
    MISSING_DIV=$(cat <<EOF
<html>
    <head>
    </head>
    <body>
        <div id='test_1' style='position:absolute;width:100px;height:100px;background-color:yellow;top:50px;left:200px;'></div>
        <div id='test_2' style='position:absolute;width:200px;height:50px;background-color:green;top:250px;left:100px;'></div>
        <div id='test_3' style='position:absolute;width:40px;height:300px;background-color:blue;top:200px;left:400px;'></div>
        <script type='text/javascript'>
            window.setTimeout(function() {
                var newDiv = document.createElement('div');
                newDiv.id = 'test_4';
                newDiv.style.cssText = 'position:absolute;width:25px;height:25px;background-color:red;top:5px;left:5px;';
                document.body.appendChild(newDiv);
            }, 1000);
        </script>
    </body>
</html>
EOF
)

    MISSING=$(curl -s -X POST http://localhost:$PORT --data "html=$MISSING_DIV&width=100&height=100&format=png&output=$HELLO_FILE&selector=#test_4&js=(function(){if (typeof(mt_main) === 'function'){return mt_main();}else{ichabod.snapshotPage();ichabod.saveToOutput();return JSON.stringify([]);}})()")
    test `echo $MISSING | jq '.conversion'` == "true"  || die "Unable to convert missing div: $MISSING"
    test `echo $MISSING | jq '.errors'` == "null"  || die "Unexpected error finding missing div: $MISSING"

    NEW_DIV=$(cat <<EOF
<html>
    <head>
    </head>
    <body>
        <div id="test_1" style="position:absolute;width:100px;height:100px;background-color:yellow;top:50px;left:200px;"></div>
        <div id="test_2" style="position:absolute;width:200px;height:50px;background-color:green;top:250px;left:100px;"></div>
        <div id="test_3" style="position:absolute;width:0px;height:0px;background-color:blue;top:200px;left:400px;"></div>
        <script type="text/javascript">
            window.setTimeout(function() {
                var divToChange = document.getElementById('test_3');
                divToChange.style.height = '20px';
                divToChange.style.width = '20px';
            }, 3000);
        </script>
    </body>
</html>
EOF
)
    NEW=$(curl -s -X POST http://localhost:$PORT --data "html=$NEW_DIV&width=100&height=100&format=png&output=$HELLO_FILE&selector=#test_3&js=(function(){if (typeof(mt_main) === 'function'){return mt_main();}else{ichabod.snapshotPage();ichabod.saveToOutput();return JSON.stringify([]);}})()")
    test `echo $NEW | jq '.conversion'` == "true"  || die "Unable to find new div: $NEW"
    test `echo $NEW | jq '.errors'` == "null"  || die "Unexpected error finding new div: $NEW"

    REALLY_MISSING_DIV=$(cat <<EOF
<html>
    <head>
    </head>
    <body>
        <div id="test_1" style="position:absolute;width:100px;height:100px;background-color:yellow;top:50px;left:200px;"></div>
        <div id="test_2" style="position:absolute;width:200px;height:50px;background-color:green;top:250px;left:100px;"></div>
    </body>
</html>
EOF
)
    REALLYMISSING=$(curl -s -X POST http://localhost:$PORT --data "html=$REALLY_MISSING_DIV&width=100&height=100&format=png&output=$HELLO_FILE&selector=#test_3&js=(function(){if (typeof(mt_main) === 'function'){return mt_main();}else{ichabod.snapshotPage();ichabod.saveToOutput();return JSON.stringify([]);}})()")
    test `echo $REALLYMISSING | jq '.conversion'` == "true"  && die "Unexpected success: $REALLYMISSING"
    test `echo $REALLYMISSING | jq '.errors | length'` == "0"  && die "Unexpected lack of error finding missing div: $REALLYMISSING"
    return 0
}

test_simple
test_wait
cleanup
echo -e "\e[32mTesting successful.\e[0m"

# all done, reset everything
trap - EXIT
exit 0
