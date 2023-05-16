#!/bin/sh

serverAddr="smtp.gmail.com"

# wait until network comes online
while ! ping -c 1 $serverAddr >/dev/null 2>&1; do
  sleep 3
done

logfile="$(dirname -- "$0")/SmsRouterPi.log"
exefile="$(dirname -- "$0")/SmsRouterPi.out"

while true; do

  if ! pidof SmsRouterPi.out >/dev/null; then

    # start app

    echo "---Start App---" >$logfile

    $exefile >>$logfile &

  fi

  if [ -f $logfile ]; then

    # check log file size, max 10mb

    if [ $(wc -c $logfile | awk '{print $1}') -gt 10485760 ]; then

      # reset logfile

      echo "" >$logfile

    fi

  fi

  sleep 1h

done
