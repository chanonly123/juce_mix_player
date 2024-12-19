import 'dart:io';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart' as path;

class AssetHelper {
  static String? _tempGetApplicationDocumentsDirectoryPath;

  static Future<String> _getApplicationDocumentsDirectoryPath() async =>
      _tempGetApplicationDocumentsDirectoryPath ??= (await path.getApplicationDocumentsDirectory()).path;

  static Future<String> extractAsset(String assetPath) async {
    final documentsDirpath = await _getApplicationDocumentsDirectoryPath();
    String path = "$documentsDirpath/$assetPath";
    ByteData data = await rootBundle.load(assetPath);
    final buffer = data.buffer;
    final file = File(path);
    await file.create(recursive: true);
    await file.writeAsBytes(buffer.asUint8List(data.offsetInBytes, data.lengthInBytes));
    return path;
  }
}
