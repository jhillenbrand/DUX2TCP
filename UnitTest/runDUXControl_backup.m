%%
close all
clear
clc

%%
% create a DUXControl object and set the number of used channels to 1 and
% the samplerate to 2.5MHz
%dataAcqObj = DUXControl(1, 2.5e6);

%% example for usage of the PRUDAQControl class
dataAcqObj = PRUDAQControl('192.168.7.2', 'root' , 'rza');
%% initialize the PRUDAQ system and set the sample rate (0...10MHz). This
% only has to be done once after booting up the system
dataAcqObj = dataAcqObj.initSettings(2.5e6);

trueSignalAnalysis = 1;

%%
for num=1:3
%%
close all

%% setting
name = 'no_damage_grease';
veloc = '100mms';
force = '100N';
% num = '3';

path = 'res/';
mkdir(path);
fulname = [name '_' veloc '_' force '_' num2str(num)];

measureLength = 10;

%% perform a recording of 1second and store the data in the specified file
temp_path = ['data'];
%dataAcqObj.getMeasurement(measureLength, temp_path); 
dataAcqObj.getMeasurement(measureLength, temp_path);
% convert the data and import it into the workspace
[ch0, ch1] = dataAcqObj.convertData(temp_path); % only for Prudaq
data = ch0;  % only for Prudaq
% load the data into the workspace
%load(temp_path); % only USBDUX

figure; plot(ch0);
figure; plot(ch1);

%save data
save([path 'result_' fulname '.mat'], 'data');

% displays the currently used channels and the current sample rate
%dataAcqObj.dispConfig %only USBDUX

%% other functions:
% ----------------------------------------------------------------

if trueSignalAnalysis
    % frequenz analysis
    fig = SignalAnalysis.fftPowerSpectrum(data, 2.5e6, 'DualPlot', true, 'NoDC', true, 'InDB', true);
    set(gcf, 'units','normalized','outerposition',[0 0 1 1]);

    % suptitle(strrep(fulname, '_', '\_'));
    annotation('textbox', [0 0.9 1 0.1], ...
        'String', strrep(fulname, '_', '\_'), ...
        'EdgeColor', 'none', ...
        'HorizontalAlignment', 'center')
    saveas(gcf, [path 'result_' fulname '.png']);
end

pause(3.5)

end


close(gcf);

disp('--------')
disp('---42---')
disp('--------')