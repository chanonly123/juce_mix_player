class TimeUtils {
  static String formatDuration(double seconds) {
    final totalSeconds = seconds.round();
    final minutes = (totalSeconds ~/ 60).toString().padLeft(2, '0');
    final remainingSeconds = (totalSeconds % 60).toString().padLeft(2, '0');
    return '$minutes:$remainingSeconds';
  }

  static String formatDurationMs(double ms) {
    final totalSeconds = (ms / 1000).round();
    final minutes = (totalSeconds ~/ 60).toString().padLeft(2, '0');
    final remainingSeconds = (totalSeconds % 60).toString().padLeft(2, '0');
    return '$minutes:$remainingSeconds';
  }
}

class VideoCoordinateUtils {
  /// Convert user coordinates (original video, 0-based) to internal coordinates (with padding)
  static int userToInternal(int userMs, int paddingMs, int latencyAdjustmentMs) {
    return userMs + paddingMs + latencyAdjustmentMs;
  }

  /// Convert internal coordinates (with padding) to user coordinates (original video, 0-based)
  static int internalToUser(int internalMs, int paddingMs, int latencyAdjustmentMs) {
    return internalMs - paddingMs - latencyAdjustmentMs;
  }

  /// Clamp millisecond values to valid range
  static int clampMs(int ms, int min, int max) {
    return ms.clamp(min, max);
  }

  /// Calculate duration between two timestamps
  static int durationMs(int startMs, int endMs) {
    return (endMs - startMs).abs();
  }
}
