#!/bin/bash
gcc -Wall -o dlm_wifi_standard dlm_wifi_standard.c -I../DLM_COMMON -I../extensions/app_magic -lpthread
echo "Build complete (WIFI)."
