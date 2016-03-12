#!/bin/bash

files="\
 ide.sh\
 index.html\
 start.js\
 css/app.css\
 app/main.js\
 app/app.js\
 app/cams.js\
 app/appv.js\
 app/authv.js\
 app/camsv.js\
 app/stv.js\
"
emacs -mm -bg "snow2" $files -f delete-window &
