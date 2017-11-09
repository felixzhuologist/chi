#!/bin/bash

tshark -i lo -d tcp.port==776,irc -Y irc -V -O irc -T fields -e irc.request -e irc.response tcp port 7776
