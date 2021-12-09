% VERSION: 0.1
% DEPENDENCY: MyUSBDUX.jar
% AUTHOR: jonas.hillenbrand@kit.edu
classdef JavaDUX < handle
    %JAVADUX creates a object to interface with the duxTCPServer on a
    %   remote machine
    % Example Usage:
    %   jd = JavaDux(1, 2e6, '192.168.1.152', 2020);
    properties
        sampleRate = 2e6;
        channelNum = 1; % the number of channels that can be selected are 1, 2, 3 or 16
        ipAddress = ''; % host IP-Adress
        tcpServerPort = 2020; % needed for tcp remote communication
        tcpPacketSize = 512; % bytes, 512 is the maximum feasible for usb dux fast
        socketTimeout = 500; % [ms]
        myUSBDux = []; % Java MyUSBDUX object
        hostName = ''; % SSH host name
        password = ''; % SSH host password
        sshPath = ''; % Path to file on SSH Host
        
        maxBufferSize = 50000;
        aeFilePostFix = 'ae_dux_int16';
        aeFolderPath = '';
        maxSamplesPerFile = 20e6;
        maxBufferReadTimeout = 200; % [ms]
    end
    
    properties (Constant)
        JAR_FILE = 'MyUSBDUX.jar'; % path of Java file
    end
    
    methods (Static)
        function loadJars()            
            % add required java jars
            javaPaths = javaclasspath;
            doNotAdd = false;
            for j = 1 : length(javaPaths)
                javaPath = javaPaths{j};
                if contains(javaPath, JavaDUX.JAR_FILE)
                    doNotAdd = true;
                    break;
                end
            end
            if (doNotAdd == false)
                classPath = mfilename('fullpath');
                [p, n, e] = fileparts(classPath);
                jarFilePath = [p filesep JavaDUX.JAR_FILE];

                javaaddpath(jarFilePath);

            end
        end
        
        function importPackages()            
            import net.sytes.botg.dux.*;
            %import net.sytes.botg.array.*;
        end
    end
    
    methods
        %% - DUXControl Constructor
        function obj = JavaDUX(chNum, sampleRate, ipAddress, port, hostName, password, sshPath, maxBufferSize, aeFolderPath, aeFilePostFix, maxSamplesPerFile, maxBufferReadTimeout)
            import net.sytes.botg.dux.*;
            if nargin < 12
                maxBufferReadTimeout = 200;
            end
            if nargin < 11
                maxSamplesPerFile = 20000000;
            end
            if nargin < 10
                aeFilePostFix = 'ae_dux_int16';
            end
            if nargin < 9
                aeFolderPath = '';
            end
            if nargin < 8
                maxBufferSize = 250000;
            end
            if nargin < 7
                %obj.L.log('ERROR', 'not enough input arguments');
            end
            % samplerate setup depends on number of channels
            if chNum > 1
                %srn = chNum * sampleRate;
                %obj.sampleRate = srn;
                obj.sampleRate = sampleRate;
            else                 
                if ((sampleRate>0) && (sampleRate<=3300000))
                    obj.sampleRate = sampleRate;
                else
                    %obj.L.log('ERROR', 'Sample rate out of range. Must be between 0 and 3.3e6.')                
                end
            end
            if ((chNum>=1) && (chNum<=16))
                obj.channelNum=chNum;
            else
                error('ERROR: Number of channels out of range. Must be between 1 and 16.')                
            end
            obj.ipAddress = ipAddress;
            obj.tcpServerPort = port;
            obj.hostName = hostName;
            obj.password = password;
            obj.sshPath = sshPath;
            obj.maxBufferSize = maxBufferSize;
            obj.aeFolderPath = aeFolderPath;
            obj.aeFilePostFix = aeFilePostFix;
            obj.maxSamplesPerFile = maxSamplesPerFile;
            obj.maxBufferReadTimeout = maxBufferReadTimeout;
                                    
            obj.startRemoteDuxToTCPServer(obj.ipAddress, obj.hostName, obj.password, obj.sshPath);
            obj.myUSBDux = MyUSBDUX(obj.ipAddress, obj.tcpServerPort, obj.tcpPacketSize, obj.socketTimeout, obj.maxBufferSize, obj.aeFolderPath, obj.aeFilePostFix, obj.maxSamplesPerFile, obj.maxBufferReadTimeout);
            
            obj.myUSBDux.connect();
            
            % forward config to tcp server
            obj.myUSBDux.setNumberOfChannels(obj.channelNum);
            obj.myUSBDux.setSampleRate(obj.sampleRate);
        end
        
        %% - startRemoteDuxTCPServer
        function startRemoteDuxToTCPServer(obj, ipAddress, hostName, password, sshPath)
            if nargin < 4
                error('ERROR: not enough input arguments');
            end
            ssh = SSH(ipAddress, hostName, password);
            ssh.triggerOnControllerAsyncServer(sshPath); 
            %ssh.triggerOnControllerAsyncClient(sshPath); % trigger on remote SSH Server
            disp('Press ENTER in cmd before continue:');
            pause();
        end
        
        %% - getMeasurement
        function data = getMeasurement(obj, nSamples, toFile, convert)
            if nargin < 2
                error('ERROR: not enough input aguments')
            end
            data = [];
            if nargin < 3
                toFile = [];
            end
            if nargin < 4
                convert = false;
            end
            data = obj.myUSBDux.getMeasurement(nSamples);
            if convert
                data = obj.convertBitToVolt(data);
            end
            if ~isempty(toFile)
                save(toFile, data);
            end
            % split up received data into channels
            if obj.channelNum == 1
                % do nothing
            elseif obj.channelNum == 2
                d1 = data(1:2:end);
                d2 = data(2:2:end);
                data = [d1, d2];
                % alternative code [could be faster with larger arrays??
                %rdata = reshape(data, 2, []);
                %data = rdata.';
            elseif obj.channelNum == 3
                d1 = data(1:3:end);
                d2 = data(2:3:end);
                d3 = data(3:3:end);
                indMin = min([length(d1), length(d2), length(d3)]);
                data = [d1(1:indMin, :), d2(1:indMin, :), d3(1:indMin, :)];
                % alternative code [could be faster with larger arrays??
                %rdata = reshape(data, 3, []);
                %data = rdata.';
            elseif obj.channelNum == 16
                rd = reshape(data, obj.channelNum, []);
                data = transpose(rd);
            else
                error(['ERROR: Number of Channels must be either 1, 2, 3 or 16, but JavaDux was configured with ' num2str(obj.channelNum)])
            end
        end
        
        %% - getMeasurementAsync
        function [status,result] = getMeasurementAsync(obj, nSamples, filePath)
            %GETMEASUREMENTASYNC(obj, nSamples, filePath)
            if nargin < 3
                error('ERROR: not enough input arguments');
            end
            obj.myUSBDux.getMeasurementAsync(nSamples, filePath);
        end
                
        %% - convertBitToVolt
        function data = convertBitToVolt(obj, data, range)
            % CONVERTBITTOVOLT
            % range: either -0.75V to 0.75V (1.5V) or -0.5V to 0.5V (1V)
            if nargin < 3
                range = 1.5;
            end
            % 12 bit unsigned ADC --> 2^12 = 4096
            data = (data - 2048) * range / 4096;
        end
        
        %% - disconnect
        function bool = disconnect(obj)
            obj.myUSBDux.disconnect();
        end
        
        %% - setNumberOfSamples
        function bool = setNumberOfSamples(obj, nSamples)
            obj.myUSBDux.setNumberOfSamples(nSamples);
        end
        
        %% - setSampleRate
        function bool = setSampleRate(obj, sampleRate)
            obj.sampleRate = sampleRate;
            obj.myUSBDux.setSampleRate(sampleRate);
        end
        
        %% - setNumberOfChannels
        function bool = setNumberOfChannels(obj, numOfChannels)
            obj.channelNum = numOfChannels;
            obj.myUSBDux.setNumberOfChannels(numOfChannels);
        end
        
        %% - flushBuffer
        function flushBuffer(obj)
            obj.myUSBDux.flush();
        end
        
        %% - startLoop
        function startLoop(obj)
            obj.myUSBDux.startLoop();
        end
        
        %% - stopLoop
        function stopLoop(obj)
            obj.myUSBDux.stopLoop();
        end
        
        %% - setFilePostFix
        function setFilePostFix(obj, filePostFix)
            obj.myUSBDux.setFilePostFix(filePostFix);
        end
        
        %% - setFolderPath
        function setFolderPath(obj, folderPath)
            obj.myUSBDux.setFolderPath(folderPath);
        end
        
        %% - setMaxSamplesPerFile
        function setMaxSamplesPerFile(obj, maxSamplesInFile)
            obj.myUSBDux.setMaxSamplesPerFile(maxSamplesInFile);
        end        
    end
end