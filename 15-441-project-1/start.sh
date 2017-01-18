#!/bin/bash
./lisod 8081 8089 lisod.log lisod.lock ../static_site ./flaskr/flaskr.py sslkey.key sslcrt.crt
