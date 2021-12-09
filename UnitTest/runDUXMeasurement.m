% example for using the DUXControl class
clc
clear
close all force

%%
sensor = "test";
start = 1125;
ende = 1275;
v = 440;
f = 500;

frq = 1e6;
ch_num=1;
measure_time = 8;
% frq = 8e4;

%%
% create a DUXControl object
dux = DUXControl(ch_num, frq);
dux.saveInfo = true;
dux.save_filename = './temp/measure.dat';
dux.saveInfo_filename = './temp/measure_info.txt';
dux.trueAsynchron = false;
dux.cmd_filename = 'cmd';

%%
% fig = figure; set(fig,'WindowStyle','docked')

%%
for numberMeas = 1
stamp_posix_ii = posixtime(datetime('now','TimeZone','UTC'));

% perform a recording of 1second and store the data in the specified file
dux.getMeasurement(measure_time);
%
% load the data into the workspace
measure = load(dux.save_filename);
nameMeasurement = sprintf('~/Schreibtisch/temp/duxmes_%0.0f_%s_%0.0fkHz.mat', stamp_posix_ii, sensor, frq/1000);
save(nameMeasurement, 'measure');

x = (measure-2^(12-1))./2^(12-1)*0.75;
% plot the result
for ii = 1:ch_num
    SignalAnalysis.fftPowerSpectrum(measure(:,ii), frq, 'DualPlot', true, 'NoDC', true);
    SignalAnalysis.welchPowerSpectrum(measure(:,ii), 512, 256, 512, frq,'DualPlot', true, 'NoDC', true);
end
%plot(measure)
% ylim([0 2^12-1])
% ylim([1900 2100])
% drawnow
% ylim([-0.75 0.75])

pause(1.1)

end
%%
% other functions:
% ----------------------------------------------------------------

% displays the currently used channels and the current sample rate
dux.dispConfig
% delete('measure.dat');