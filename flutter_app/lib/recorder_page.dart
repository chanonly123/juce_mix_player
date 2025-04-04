import 'dart:developer';
import 'dart:io';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:juce_mix_player/juce_mix_player.dart';
import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';
import 'audio_player_dialog.dart';

class RecorderPage extends StatefulWidget {
  const RecorderPage({super.key});

  @override
  RecorderPageState createState() => RecorderPageState();
}

class RecorderPageState extends State<RecorderPage> {
  final recorder = JuceMixPlayer(record: true, play: false);
  bool isRecording = false;
  bool isRecorderPrepared = false;
  bool isRecorderPreparing = false;
  bool isMicPermissionGranted = false;
  String recordingPath = '';
  DateTime? recordingStartTime;
  Duration recordingDuration = Duration.zero;
  MixerDeviceList deviceList = MixerDeviceList(devices: []);
  JuceMixRecState state = JuceMixRecState.IDLE;
  // in decibels
  double reclevel = 0.0;

  @override
  void initState() {
    super.initState();
    print("RecorderPageState.initState");

    // Check microphone permission
    _checkMicrophonePermission();

    // Set up state update handler
    recorder.setRecStateUpdateHandler((state) {
      log("Recorder state: ${state.toString()}");
      setState(() => this.state = state);

      switch (state) {
        case JuceMixRecState.IDLE:
          setState(() {
            isRecorderPrepared = false;
          });
          break;
        case JuceMixRecState.READY:
          setState(() {
            isRecorderPrepared = true;
          });
          ScaffoldMessenger.of(context).showSnackBar(SnackBar(
            content: Text('Recorder is ready, you can start recording now!'),
            backgroundColor: Colors.green,
          ));
          break;
        case JuceMixRecState.RECORDING:
          setState(() {
            isRecording = true;
            recordingStartTime = DateTime.now();
          });
          break;
        case JuceMixRecState.STOPPED:
          setState(() {
            isRecording = false;
            isRecorderPrepared = false;
            recordingStartTime = null;
            recordingDuration = Duration.zero;
          });
          break;
        case JuceMixRecState.ERROR:
          break;
        // Handle other states as needed
      }
    });

    recorder.setDeviceUpdateHandler((deviceList) {
      setState(() {
        // Filter the deviceList to include only input devices and assign IDs
        // this.deviceList = MixerDeviceList(
        //   devices: deviceList.devices.where((device) => device.isInput).toList(),
        // );
        this.deviceList = deviceList;
      });
      log('devices: ${JsonEncoder.withIndent('  ').convert(this.deviceList.toJson())}');
    });

    // Set up error handler
    recorder.setErrorHandler((error) {
      log('Error: $error');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(
          content: Text(error),
          backgroundColor: Colors.red,
        ));
      }
    });

    recorder.setRecLevelHandler((level) {
      setState(() {
        reclevel = level;
      });
      log("Recorder level: $level");
    });

    // Set up a timer to update recording duration
    _setupDurationTimer();
  }

  @override
  void dispose() {
    // if (isRecording) {
    //   recorder.stopRecording();
    // }
    recorder.dispose();
    super.dispose();
  }

  void _setupDurationTimer() {
    // Update recording duration every second
    Future.delayed(Duration(seconds: 1), () {
      if (mounted) {
        if (isRecording && recordingStartTime != null) {
          setState(() {
            recordingDuration = DateTime.now().difference(recordingStartTime!);
          });
        }
        _setupDurationTimer(); // Schedule next update
      }
    });
  }

  Future<void> _checkMicrophonePermission() async {
    final status = await Permission.microphone.status;
    print("Permission.microphone.status: ${status.toString()}");
    setState(() {
      isMicPermissionGranted = status.isGranted;
    });

    if (!status.isGranted) {
      // Request permission if not granted
      final result = await Permission.microphone.request();
      setState(() {
        isMicPermissionGranted = result.isGranted;
      });

      if (result.isGranted) {
        // If permission is granted, prepare the recorder
        _prepareRecorder();
      } else {
        // Show error message if permission is denied
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(SnackBar(
            content: Text('Microphone permission is required to record audio'),
            backgroundColor: Colors.red,
            action: SnackBarAction(
              label: 'Settings',
              onPressed: () async {
                Navigator.pop(context); // Close dialog
                await openAppSettings();
              },
            ),
          ));
        }
      }
    } else {
      // If permission is already granted, prepare the recorder
      _prepareRecorder();
    }
  }

  Future<void> _prepareRecorder() async {
    if (!isMicPermissionGranted) {
      log('Cannot prepare recorder: Microphone permission not granted');
      return;
    }

    try {
      setState(() {
        isRecorderPreparing = true;
      });
      final directory = await getApplicationDocumentsDirectory();
      final recordingsDir = Directory('${directory.path}/recordings');

      // Create the recordings directory if it doesn't exist
      if (!await recordingsDir.exists()) {
        await recordingsDir.create(recursive: true);
        log('Recordings directory created');
      }

      final timestamp = DateTime.now().millisecondsSinceEpoch;
      recordingPath = '${recordingsDir.path}/$timestamp.wav';

      log('Preparing recorder with path: $recordingPath');
      recorder.prepareRecording(recordingPath);
      log('Recorder prepared successfully');
      setState(() {
        isRecorderPrepared = true;
        isRecorderPreparing = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('Recorder prepared successfully'),
        backgroundColor: Colors.blueAccent,
      ));
    } catch (e) {
      log('Error preparing recorder: $e');
      setState(() {
        isRecorderPreparing = false;
      });
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(
          content: Text('Failed to prepare recorder: $e'),
          backgroundColor: Colors.red,
        ));
      }
    }
  }

  void _toggleRecording() async {
    if (!isMicPermissionGranted) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('Microphone permission is required to record audio'),
        backgroundColor: Colors.red,
        action: SnackBarAction(
          label: 'Settings',
          onPressed: () {
            openAppSettings();
          },
        ),
      ));
      return;
    }

    if (!isRecorderPrepared) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('Recorder is not ready yet, please wait'),
        backgroundColor: Colors.orange,
      ));
      // return;
      if (!isRecorderPreparing) {
        await _prepareRecorder();
      } else {
        return;
      }
    }

    if (isRecording) {
      // Stop recording
      recorder.stopRecording();
      showDialog(
        context: context,
        builder: (context) => AudioPlayerDialog(
          filePath: recordingPath,
          onOkPressed: () {
            ScaffoldMessenger.of(context).showSnackBar(SnackBar(
              content: Text('Recording processing completed'),
              backgroundColor: Colors.green,
            ));
          },
        ),
      );
    } else {
      // Start recording
      recorder.startRecording(recordingPath);
    }
  }

  String _formatDuration(Duration duration) {
    String twoDigits(int n) => n.toString().padLeft(2, '0');
    final minutes = twoDigits(duration.inMinutes.remainder(60));
    final seconds = twoDigits(duration.inSeconds.remainder(60));
    return "$minutes:$seconds";
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Audio Recorder")),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            // Recording duration display
            Text(
              _formatDuration(recordingDuration),
              style: TextStyle(fontSize: 48, fontWeight: FontWeight.bold),
            ),
            SizedBox(height: 20),

            // Recording status indicator
            Text(
              !isMicPermissionGranted
                  ? "Microphone permission required"
                  : (isRecording
                      ? "Recording..."
                      : (isRecorderPrepared
                          ? "Ready to record"
                          : isRecorderPreparing
                              ? "Preparing recorder..."
                              : "Not Prepared")),
              style: TextStyle(fontSize: 16, color: !isMicPermissionGranted ? Colors.red : (isRecording ? Colors.red : Colors.grey)),
            ),
            SizedBox(height: 40),
            Text("State: ${state.toString()}"),
            SizedBox(height: 40),

            // Record button
            GestureDetector(
              onTap: _toggleRecording,
              child: Container(
                width: 80,
                height: 80,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: isRecording ? Colors.red.shade800 : Colors.red,
                ),
                child: Icon(
                  isRecording ? Icons.stop : Icons.mic,
                  color: Colors.white,
                  size: 40,
                ),
              ),
            ),
            SizedBox(height: 40),
            popupMenu()
          ],
        ),
      ),
    );
  }

  PopupMenuButton popupMenu() {
    return PopupMenuButton<MixerDevice>(
      child: Text("DEVICES: ${deviceList.devices.length}"),
      itemBuilder: (context) => deviceList.devices.map((dev) {
        return PopupMenuItem<MixerDevice>(
          value: dev,
          child: Text(getName(dev)),
        );
      }).toList(),
      onSelected: (selectedDevice) {
        deviceList.devices.forEach((d) {
          d.isSelected = false;
        });
        selectedDevice.isSelected = true;
        // Ensure the first device is always selected
        // if (deviceList.devices.isNotEmpty) {
        // deviceList.devices[3].isSelected = true;
        // }
        recorder.setUpdatedDevices(deviceList);
        print("Selected device: ${deviceList.devices.toString()}");
      },
    );
  }

  String getName(MixerDevice device) {
    return "${device.name} ${device.isSelected ? " ✅" : ""}";
  }
}
