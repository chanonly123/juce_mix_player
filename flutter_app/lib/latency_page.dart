import 'package:flutter/material.dart';
import 'package:juce_mix_player/latency_calc.dart';

class LatencyPage extends StatefulWidget {
  @override
  State<StatefulWidget> createState() {
    return LatencyPageState();
  }
}

class LatencyPageState extends State<LatencyPage> {
  String devSettings = '';
  String devOptions = '';
  final recorder = LatencyCalc();
  double latency = -1;
  String errorMessage = '';
  bool isCalculating = false;
  Image? image = null;

  LatencyPageState() {
    devOptions = recorder.setDevSettings('');
  }

  bool isOptionEnabled(String opt) {
    return !devSettings.contains('~' + opt);
  }

  List<String> getOptionList() {
    return devSettings.split(',').where((i) => !i.contains('~')).toList();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Latency Page'),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            SizedBox(
              height: 300,
              child: image != null ? image! : Container(),
            ),
            SizedBox(height: 20),
            Text('Latency: $latency'),
            SizedBox(height: 20),
            Text('$errorMessage'),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: () {
                calculateLatency();
              },
              child: Text(
                  "${isCalculating ? "..." : ""} Calculate Latency ${isCalculating ? "..." : ""}"),
            ),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: () {
                recorder.stop();
              },
              child: Text('Stop'),
            ),
            SizedBox(height: 20),
            Wrap(
              spacing: 10,
              children: [
                for (final opt in getOptionList())
                  ElevatedButton(
                    onPressed: () {
                      final enable = !isOptionEnabled(opt);
                      final result =
                          recorder.setDevSettings(enable ? opt : '~$opt');
                      setState(() {
                        devSettings = result;
                      });
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor:
                          isOptionEnabled(opt) ? Colors.green : Colors.red,
                    ),
                    child: Text('$opt'),
                  ),
              ],
            ),
            SizedBox(height: 20),
          ],
        ),
      ),
    );
  }

  void calculateLatency() {
    setState(() {
      latency = -1;
      errorMessage = '';
      isCalculating = true;
    });
    recorder.startLatencyCalculation((level) {
      setState(() {
        if (level == 'image_buffer_ready') {
          updateImage();
        } else if (double.tryParse(level) != null) {
          isCalculating = false;
          latency = double.parse(level);
        } else {
          isCalculating = false;
          errorMessage = level;
        }
      });
    });
  }

  void updateImage() {
    setState(() {
      image?.image.evict();
      image = Image.memory(recorder.getImageFromBuffer());
      // Optionally refresh devSettings after image update
      devSettings = recorder.setDevSettings('');
    });
  }

  @override
  void initState() {
    super.initState();
    // Initialize devSettings on startup
    devSettings = recorder.setDevSettings('');
  }

  @override
  void dispose() {
    recorder.dispose();
    super.dispose();
  }
}
