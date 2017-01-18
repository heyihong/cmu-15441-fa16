#!/bin/bash
./lisod 8080 8088 lisod.log lisod.lock ../static_site ./flaskr/flaskr.py sslkey.key sslcrt.crt
