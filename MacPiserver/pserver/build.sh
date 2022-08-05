make clean
rm *.o
make CPPFLAGS=-DIOS_10_0 CPPFLAGS+=-DXCUITEST CPPFLAGS+=-DKEYQUEUE piserver
cp piserver piserver_10_3 
cp piserver piserver_10_0
cp piserver offlineautomation_10_0

echo "---------------------------------------------------------------------------------------------------------------------------------------"
make clean
rm *.o
make CPPFLAGS=-DIOS_10_0 CPPFLAGS+=-DXCUITEST CPPFLAGS+=-DKEYQUEUE  CPPFLAGS+=-DOS_11_0 piserver
cp piserver piserver_11_0
cp piserver offlineautomation_11_0

echo "---------------------------------------------------------------------------------------------------------------------------------------"

make clean
rm *.o
make CPPFLAGS=-DIOS_10_0 CPPFLAGS+=-DXCUITEST CPPFLAGS+=-DKEYQUEUE CPPFLAGS+=-DOS_11_0 CPPFLAGS+=-DOS_13_4
cp piserver piserver_13_4
cp piserver offlineautomation_13_4

make clean
rm *.o
make CPPFLAGS=-DIOS_10_0 CPPFLAGS+=-DXCUITEST CPPFLAGS+=-DKEYQUEUE CPPFLAGS+=-DOS_11_0  CPPFLAGS+=-DOS_14_1 piserver
cp piserver piserver_14_0
cp piserver offlineautomation_14_0

make clean
rm *.o
make CPPFLAGS=-DIOS_9_0 piserver
cp piserver piserver_9_0

echo "---------------------------------------------------------------------------------------------------------------------------------------"

make clean
rm *.o
make CPPFLAGS=-DIOS_8_3 piserver
cp piserver piserver_8_3

echo "---------------------------------------------------------------------------------------------------------------------------------------"

make clean
rm *.o
make piserver

echo "---------------------------------------------------------------------------------------------------------------------------------------"

#make clean offlineautomationserver
rm *.o
make CPPFLAGS=-DIOS_9_0 offlineautomationserver
cp offlineautomationserver offlineautomationserver_9_0

#make clean offlineautomationserver
rm *.o offlineautomationserver
make offlineautomationserver

#make clean idevicescreenshot
rm *.o idevicescreenshot
make idevicescreenshot


rm *.o launchApp
make CPPFLAGS=-DIOS_10_0 launchApp
cp launchApp launchApp_10_0

#make clean launchApp
rm *.o launchApp
make launchApp

rm *.o idevicelocation
make idevicelocation

rm *.o idevicelaunchdebug
make idevicelaunchdebug

rm *.o iOSDevicetunnel
make iOSDevicetunnel
