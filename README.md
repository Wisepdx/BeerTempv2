# BeerTempv2
Updated version of Beer Temp Controller

++++++++++++++++++++++++++++++++
+  Beer Temperature Controller +
+          Versions 2          +
++++++++++++++++++++++++++++++++

Beer Fermentation Controller using an Arduino Yun to Post to MySQL database.
Uses PHP to serve pages on the web including real time sensor data and historical data from past brews.

Charts created using HighCharts.

Hardware
----------
Arduino Yun
External Web Server
(2) T12706 Peltiers
(2) Dallas onewire sensors (one chip / one waterproof probe)
(1) Monster moto shield
(1) 12v 360W Power Supply
(1) Custom Aluminum Block for Fermenter with heatsink fans

Project uses 2 Peltiers attached to the side of the fermenter to heat/cool to desired temperature. 
I am using a monster moto shield to drive up to 30A to each peltier as well as control polarity.
I am using 2 dallas one wire temperature sensors to gather batch temperature and ambient air temperature.
