%%
clear
clc

%%
sr = 2e6;

%%

JavaDUX.loadJars();

%%
jd = JavaDUX(1, sr, '192.168.1.152', 2021, '<HOSTNAME>', '<PASSWORD'', '/PATH/TO/duxToTCPServer');


%% get data synchronous from USBDUX via TCP Server

d = jd.getMeasurement(10e6, [], false);

plot(d);


%% get data async from USBDUX via TCP Server
jd.getMeasurementAsync(10e6, 'test.dat');

%%
d = load('test.dat');

%% plot data
plot(d);


%% set channel number
jd.setNumberOfChannels(2);

%% set samplerate
jd.setSampleRate(10e3);

%%
jd.disconnect()

