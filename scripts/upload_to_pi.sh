#!/bin/bash

scp -o MACs=hmac-sha2-256 -r bins/* qnxuser@qnxpi.local:/home/qnxuser/bins

ssh qnxuser@qnxpi.local "chmod +x /home/qnxuser/bins/*"