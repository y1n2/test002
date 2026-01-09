#!/bin/bash
gcc -Wall -o dlm_cellular_standard dlm_cellular_standard.c -I../DLM_COMMON -I../extensions/app_magic -lpthread
echo "Build complete (CELLULAR)."
