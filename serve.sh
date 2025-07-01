#!/bin/bash
# DO NOT RUN THIS SCRIPT MANUALLY. USE "make serve" INSTEAD.
# <------------------------->
PORT=8080
# rebuild and serve with live reload
while true; do
  inotifywait -e modify -r input/ || continue
  clear
  ./tinyssg || echo "build failed"
done &

# serve static files
cd output && python3 -m http.server $PORT

