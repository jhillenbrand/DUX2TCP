function runDUXControlAuto(sensor, param)

close all

%%
% sensor = "p";
start = param(1,1);
ende = param(1,2);
v = param(1,3);
f = param(1,4);
frq = param(1,5);

ch_start=0;
ch_num=1;
saveInfoAtTheEndOfFile = true;

%%
% create a DUXControl object and set the number of used channels to 1 and
% the samplerate to 3MHz
dux = DUXControl(ch_start, frq, ch_num, saveInfoAtTheEndOfFile);
%%
fig = figure; set(fig,'WindowStyle','docked')
%%
for numberMeas = 1:3
    %%
    % perform a recording of 1second and store the data in the specified file
    dux.getMeasurement(4,'measure.dat');
    %
    % load the data into the workspace
    load('measure.dat');
    nameMeasurement = sprintf('./Measurement/meas_%s_%04d_%04d_%03d_%03d_%d_num%d.mat', sensor, start, ende, v, f, frq, numberMeas);
    save(nameMeasurement, 'measure');
    
    x = (measure-2^(12-1))./2^(12-1)*0.75;
    %
    % plot the result
    plot(measure)
    % ylim([0 2^12-1])
    % ylim([1900 2100])
    drawnow
    % ylim([-0.75 0.75])
    
    pause(1.1)
    
    %%
    SignalAnalysis.fftPowerSpectrum(measure, 2e6, 'DualPlot', true, 'NoDC', true);
%     set(fig2, 'units','normalized','outerposition',[0 0 1 1]);
    
%     suptitle(strrep(nameMeasurement, '_', '\_'));
%     annotation('textbox', [0 0.9 1 0.1], ...
%         'String', strrep(strrep(strrep(nameMeasurement, './Measurement/', ''), '.mat', ''), '_', '\_'), ...
%         'EdgeColor', 'none', ...
%         'HorizontalAlignment', 'center')
%     saveas(fig2, strrep(nameMeasurement, '.mat', '.png'));
%     close(fig2);
    
end
%%
% other functions:
% ----------------------------------------------------------------

% displays the currently used channels and the current sample rate
dux.dispConfig
delete('measure.dat');
end