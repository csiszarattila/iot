class SensorsData {
  final int at;
  final int aqi;
  final double pm1;
  final double pm25;
  final double pm4;
  final double pm10;

  SensorsData(this.at, this.aqi, this.pm1, this.pm25, this.pm4, this.pm10);

  SensorsData.fromJson(Map<String, dynamic> json)
      : at = json['at'] as int,
        aqi = json['aqi'] as int,
        pm1 = json['pm1'] as double,
        pm25 = json['pm25'] as double,
        pm4 = json['pm4'] as double,
        pm10 = json['pm10'] as double;
}