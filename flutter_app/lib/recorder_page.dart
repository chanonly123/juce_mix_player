import 'dart:developer';
import 'dart:io';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_app/asset_helper.dart';
import 'package:flutter_app/utils.dart';
import 'package:flutter_app/widgets.dart';
import 'package:percent_indicator/percent_indicator.dart';
import 'package:juce_mix_player/juce_mix_player.dart';
import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';
import 'audio_player_dialog.dart';
import 'package:flutter/services.dart';

class RecorderPage extends StatefulWidget {
  const RecorderPage({super.key});

  @override
  RecorderPageState createState() => RecorderPageState();
}

class RecorderPageState extends State<RecorderPage> {
  final recorder = JuceMixPlayer();
  bool isRecording = false;
  bool isRecorderPrepared = false;
  bool isRecorderPreparing = false;
  bool isMicPermissionGranted = false;
  String recordingPath = '';
  DateTime? recordingStartTime;
  double recordingDuration = 0.0;
  MixerDeviceList deviceList = MixerDeviceList(devices: []);
  JuceMixRecState state = JuceMixRecState.IDLE;
  // in Db range -60 to 0
  double currReclevel = -200.0;
  double maxReclevel = -200.0;
  final double minAllowedLevelDb = -24.0;
  final double maxAllowedLevelDb = -3.5;
  bool isLevelTooHigh = false; // Track if level is too high to avoid repeated vibrations
  bool isMetronomeEnabled = false;

  @override
  void initState() {
    super.initState();
    print("RecorderPageState.initState");

    // Check microphone permission
    _checkMicrophonePermission();

    recorder.setRecLevelHandler((level) {
      // log("Recorder level: $level");
      setState(() {
        if (isRecording) {
          currReclevel = level;
          if (level > maxReclevel) {
            maxReclevel = level;
          }

          // Check if level exceeds maximum allowed and trigger haptic feedback
          if (level > maxAllowedLevelDb && !isLevelTooHigh) {
            log("HapticFeedback");
            isLevelTooHigh = true;
            HapticFeedback.heavyImpact();
          } else if (level <= maxAllowedLevelDb) {
            isLevelTooHigh = false;
          }
        }
      });
    });

    recorder.setRecProgressHandler((progress) {
      // log("Recorder progress: $progress");
      setState(() => recordingDuration = progress);
    });

    // Set up state update handler
    recorder.setRecStateUpdateHandler((state) {
      log("Recorder state: ${state.toString()}");
      setState(() => this.state = state);

      switch (state) {
        case JuceMixRecState.IDLE:
          setState(() {
            isRecorderPrepared = false;
            isRecorderPreparing = false;
            isRecording = false;
            recordingStartTime = null;
            recordingDuration = 0.0;
            currReclevel = -200.0;
            maxReclevel = -200.0;
            isLevelTooHigh = false;
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
            // recordingDuration = 0.0;
            isLevelTooHigh = false;
          });
          // Show audio player dialog when recording stops
          if (mounted) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
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
            });
          }
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

    // Set up a timer to update recording duration
    // _setupDurationTimer();
  }

  @override
  void dispose() {
    recorder.stopRecording();
    recorder.dispose();
    super.dispose();
  }

  void showDeviceList() {
    showDialog(
      context: context,
      builder: (context) => DeviceListDialog(devices: deviceList.devices),
    );
  }

  // Reset min/max values when starting a new recording
  void _resetMinMaxLevels() {
    setState(() {
      maxReclevel = -200.0;
      isLevelTooHigh = false; // Reset the level tracking flag
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
      final bgmPath = await AssetHelper.extractAsset('assets/media/tu_hi_re_92_D_sharp_bgm.mp3');
      recorder.setFile(bgmPath);
      if (isMetronomeEnabled) {
        final pathH = await AssetHelper.extractAsset('assets/media/met_h.wav');
        final pathL = await AssetHelper.extractAsset('assets/media/met_l.wav');
        double metVol = 0.5;
        final mixComposeModel = MixerComposeModel(
          tracks: [
            MixerTrack(id: "music", path: bgmPath),
            MixerTrack(id: "met_1", path: pathH, offset: 0, repeat: true, repeatInterval: 2, volume: metVol),
            MixerTrack(id: "met_2", path: pathL, offset: 0.5, repeat: true, repeatInterval: 2, volume: metVol),
            MixerTrack(id: "met_3", path: pathL, offset: 1, repeat: true, repeatInterval: 2, volume: metVol),
            MixerTrack(id: "met_4", path: pathL, offset: 1.5, repeat: true, repeatInterval: 2, volume: metVol)
          ],
        );
        recorder.setMixData(mixComposeModel);
      }
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
      HapticFeedback.heavyImpact();
      recorder.stopRecording();
    } else {
      // Start recording
      HapticFeedback.heavyImpact();
      _resetMinMaxLevels();
      recorder.startRecording(true);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("Audio Recorder"),
        actions: [
          Visibility(
            visible: !isRecording,
            child: IconButton(
              icon: Icon(
                Icons.music_note,
                color: isMetronomeEnabled ? Colors.blue : Colors.grey,
              ),
              onPressed: () {
                setState(() {
                  isMetronomeEnabled = !isMetronomeEnabled;
                });
                _prepareRecorder();
              },
            ),
          ),
        ],
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            // Recording duration display
            Text(
              TimeUtils.formatDuration(recordingDuration),
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
                  color: isRecording ? Colors.red.shade900 : Colors.red,
                ),
                child: Icon(
                  isRecording ? Icons.stop : Icons.mic,
                  color: Colors.white,
                  size: 40,
                ),
              ),
            ),
            SizedBox(height: 40),
            ElevatedButton.icon(
              icon: const Icon(Icons.speaker_group, size: 20),
              label: const Text('Audio Devices'),
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.blue[900],
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(8),
                  side: BorderSide(color: Colors.blue.shade700, width: 1),
                ),
              ),
              onPressed: () => showDialog(
                context: context,
                builder: (context) => DeviceListDialog(devices: deviceList.devices),
              ),
            ),
            SizedBox(height: 40),
            // Recording level in Decibles display Component
            Column(
              children: [
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 16.0),
                  child: Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Text('Current: ${currReclevel.toStringAsFixed(1)} dB', style: TextStyle(fontSize: 12)),
                      Text('Max: ${maxReclevel.toStringAsFixed(1)} dB', style: TextStyle(fontSize: 12)),
                    ],
                  ),
                ),
                SizedBox(height: 8),
                Padding(
                  padding: EdgeInsets.symmetric(horizontal: 16),
                  child: Column(
                    children: [
                      LinearPercentIndicator(
                        lineHeight: 4,
                        percent: ((currReclevel + minAllowedLevelDb.abs()) / 65).clamp(0.0, 1.0),
                        progressColor: _getProgressColor(currReclevel),
                        backgroundColor: Colors.grey.withOpacity(0.1),
                        barRadius: Radius.circular(2),
                        animation: true,
                        animateFromLastPercent: true,
                      ),
                      SizedBox(height: 4),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Text('$minAllowedLevelDb dB', style: TextStyle(fontSize: 10, color: Colors.grey)),
                          Text('$maxAllowedLevelDb dB', style: TextStyle(fontSize: 10, color: Colors.grey)),
                        ],
                      ),
                    ],
                  ),
                ),
              ],
            ),
            SizedBox(height: 20),
            _buildLevelWarning(),
          ],
        ),
      ),
    );
  }

  Color _getProgressColor(double level) {
    if (level < -30) return Colors.blue;
    if (level < -15) return Colors.green;
    if (level < 0) return Colors.orange;
    return Colors.red;
  }

  Widget _buildLevelWarning() {
    return AnimatedSwitcher(
      duration: Duration(milliseconds: 300),
      transitionBuilder: (child, animation) => FadeTransition(
        opacity: animation,
        child: child,
      ),
      child: isRecording ? _buildActualWarning() : Container(height: 40), // Maintain consistent height
    );
  }

  Widget _buildActualWarning() {
    return SizedBox(
      width: 300,
      child: AnimatedContainer(
        duration: Duration(milliseconds: 300),
        curve: Curves.easeInOut,
        padding: isRecording ? EdgeInsets.all(8) : EdgeInsets.zero,
        decoration: _getWarningDecoration(),
        child: isRecording
            ? Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Flexible(
                    child: Text(
                      _getWarningText(),
                      textAlign: TextAlign.center,
                      overflow: TextOverflow.ellipsis,
                      maxLines: 1,
                      style: TextStyle(
                        color: _getTextColor(),
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ),
                ],
              )
            : null,
      ),
    );
  }

  BoxDecoration _getWarningDecoration() {
    if (currReclevel > maxAllowedLevelDb) {
      return BoxDecoration(
        color: Colors.red.withOpacity(0.8),
        borderRadius: BorderRadius.circular(4),
      );
    } else if (currReclevel < minAllowedLevelDb) {
      return BoxDecoration(
        color: Colors.amber.withOpacity(0.8),
        borderRadius: BorderRadius.circular(4),
      );
    }
    return BoxDecoration(
      color: Colors.green.withOpacity(0.8),
      borderRadius: BorderRadius.circular(4),
    );
  }

  String _getWarningText() {
    if (currReclevel > maxAllowedLevelDb) {
      return 'Voice is too loud! Audio may be clipped.';
    }
    if (currReclevel < minAllowedLevelDb) {
      return 'Voice is too low! Speak louder.';
    }
    return 'Voice is in a good level.';
  }

  Color _getTextColor() {
    if (currReclevel > maxAllowedLevelDb) return Colors.white;
    if (currReclevel < minAllowedLevelDb) return Colors.black;
    return Colors.white;
  }
}
