#!/bin/sh

uu=$(/usr/bin/id -u)

if [ "z$uu" == "z0" ]; then
    /usr/bin/dbus-send --type=signal --system /Org/Tizen/BusActD/Test org.tizen.busactd.test.Hello
else
    /usr/bin/dbus-send --type=signal /Org/Tizen/BusActD/Test org.tizen.busactd.test.Hello
fi
