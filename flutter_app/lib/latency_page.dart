import 'package:flutter/material.dart';
import 'package:juce_mix_player/latency_calc.dart';

class LatencyPage extends StatefulWidget {
  @override
  State<StatefulWidget> createState() {
    return LatencyPageState();
  }
}

class LatencyPageState extends State<LatencyPage> {
  final recorder = LatencyCalc();
  double latency = -1;
  String errorMessage = '';
  bool isCalculating = false;
  LatencyPageState() {}

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
            Text('Latency: $latency'),
            SizedBox(height: 20),
            Text('$errorMessage'),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: () {
                setState(() {
                  latency = -1;
                  errorMessage = '';
                  isCalculating = true;
                });
                recorder.startLatencyCalculation((level) {
                  setState(() {
                    if (double.tryParse(level) != null) {
                      isCalculating = false;
                      latency = double.parse(level);
                    } else {
                      errorMessage = level;
                    }
                  });
                });
              },
              child: Text('Calculate Latency'),
            ),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: () {
                recorder.stop();
              },
              child: Text('Stop'),
            ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    recorder.dispose();
    super.dispose();
  }
}
