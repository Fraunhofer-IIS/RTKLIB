#!/bin/bash -ex

APP_PATH="${APP_PATH:-"."}"
IN_PATH="${IN_PATH:-"."}"
LOG_NAME="${LOG_NAME:-"novatel"}"
LATITUDE="${LATITUDE:-"45.065128652"}"
LONGITUDE="${LONGITUDE:-"7.659363180"}"
FORMAT="${FORMAT:-"nov"}"
EXTENSION="${EXTENSION:-".out"}"

$APP_PATH/convbin -r $FORMAT $IN_PATH/$LOG_NAME$EXTENSION -d $APP_PATH
$APP_PATH/rnx2rtkp -p 0 $LOG_NAME.nav $LOG_NAME.obs > $LOG_NAME.pos

latitude=$(tail -n 1 $LOG_NAME.pos | tr -s ' ' | cut -d ' ' -f 3)
longitude=$(tail -n 1 $LOG_NAME.pos | tr -s ' ' | cut -d ' ' -f 4)

if [[ $latitude != $LATITUDE ]] ; then
    echo "wrong latitude. current: $latitude, expected: $LATITUDE"
    exit 1
fi
if [[ $longitude != $LONGITUDE ]] ; then
    echo "wrong longitude. current: $longitude, expected: $LONGITUDE"
    exit 1
fi
