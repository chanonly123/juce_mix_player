import 'package:flutter/material.dart';
import 'package:juce_mix_player/juce_mix_player.dart';

class DeviceListDialog extends StatelessWidget {
  final List<MixerDevice> devices;

  const DeviceListDialog({super.key, required this.devices});

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Audio Devices'),
      content: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            for (final device in devices)
              Card(
                margin: const EdgeInsets.symmetric(vertical: 4.0),
                child: Padding(
                  padding: const EdgeInsets.all(12.0),
                  child: Row(
                    children: [
                      Icon(device.isInput ? Icons.mic : Icons.speaker,
                          color: device.isInput ? Colors.orange : Colors.blue),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text('${device.name} (${device.deviceType})',
                                style: const TextStyle(fontWeight: FontWeight.w500)),
                            Text(device.isInput ? 'Input Device' : 'Output Device',
                                style: TextStyle(color: Colors.grey[600])),
                            Text('Sample Rate: ${device.currentSampleRate} Hz'),
                            Text('Channels: ${device.isInput ? 
                              device.inputChannelNames.join(', ') : 
                              device.outputChannelNames.join(', ')}'),
                            if (device.availableSampleRates.isNotEmpty)
                              Text('Available Rates: ${device.availableSampleRates.join(', ')} Hz'),
                          ],
                        ),
                      ),
                      if (device.isSelected)
                        const Icon(Icons.check_circle, color: Colors.green)
                    ],
                  ),
                ),
              ),
          ],
        ),
      ),
      actions: [
        TextButton(
          child: const Text('Close'),
          onPressed: () => Navigator.of(context).pop(),
        ),
      ],
    );
  }
}