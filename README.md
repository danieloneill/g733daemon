# g733daemon
(Qt)DBUS daemon for Logitech G733 headset

Code based on work done by [ashkitten](https://github.com/ashkitten/g933-utils) and [sapd](https://github.com/Sapd/HeadsetControl).

See also [g733systray](https://github.com/danieloneill/g733systray) which handles notifications and control of the headset.

To build:
```
$ mkdir build
$ cd build
$ qmake ..
$ make
```

If running it squeals about permissions, try creating this udev rule at */etc/udev/rules.d/70-g733.rules*:
```
ACTION!="add|change", GOTO="headset_end"
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", ATTRS{idVendor}=="046d", ATTRS{idProduct}=="0ab5", TAG+="uaccess"
LABEL="headset_end"
```

