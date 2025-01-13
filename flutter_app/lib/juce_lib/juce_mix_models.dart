class MixerData {
  List<MixerTrack>? tracks;
  String? output;
  double? outputDuration;

  MixerData({
    required this.tracks,
    this.output,
    this.outputDuration,
  });

  factory MixerData.fromJson(Map<String, dynamic> json) => MixerData(
        tracks: (json['tracks'] as List<dynamic>?)?.map((e) => MixerTrack.fromJson(e as Map<String, dynamic>)).toList(),
        output: json['output'],
        outputDuration: json['outputDuration']?.toDouble(),
      );

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    if (tracks != null) json['tracks'] = tracks?.map((e) => e.toJson()).toList();
    if (output != null) json['output'] = output;
    if (outputDuration != null) json['outputDuration'] = outputDuration;
    return json;
  }
}

class MixerTrack {
  double? duration;
  double? volume;
  double? fromTime;
  bool? enabled;
  String? path;
  double? offset;
  String id;

  MixerTrack({
    required this.id,
    required this.path,
    this.offset,
    this.fromTime,
    this.duration,
    this.volume,
    this.enabled,
  });

  factory MixerTrack.fromJson(Map<String, dynamic> json) => MixerTrack(
        id: json['id_'],
        path: json['path'],
        offset: json['offset']?.toDouble(),
        fromTime: json['fromTime']?.toDouble(),
        duration: json['duration']?.toDouble(),
        volume: json['volume']?.toDouble(),
        enabled: json['enabled'],
      );

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{};
    json['id_'] = id;
    json['path'] = path;
    if (offset != null) json['offset'] = offset;
    if (fromTime != null) json['fromTime'] = fromTime;
    if (duration != null) json['duration'] = duration;
    if (volume != null) json['volume'] = volume;
    if (enabled != null) json['enabled'] = enabled;
    return json;
  }
}
