import 'package:flutter/material.dart';

class GstVideoView extends StatelessWidget {
  GstVideoView({super.key});

  @override
  Widget build(BuildContext context) {
    return UiKitView(
      viewType: "gst_video_view",
    );
  }
}
