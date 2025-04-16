class TimeUtils {
  static String formatDuration(double seconds) {
    final totalSeconds = seconds.round();
    final minutes = (totalSeconds ~/ 60).toString().padLeft(2, '0');
    final remainingSeconds = (totalSeconds % 60).toString().padLeft(2, '0');
    return '$minutes:$remainingSeconds';
  }
}