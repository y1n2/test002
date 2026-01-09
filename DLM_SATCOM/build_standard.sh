#!/bin/bash
gcc -Wall -o dlm_satcom_standard dlm_satcom_standard.c -I../DLM_COMMON -I../extensions/app_magic -lpthread
echo "Build complete."
