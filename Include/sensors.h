void SensorsInit();

void WeatherSensorRead();

struct SensorsData {
  float temperature;
  int    humidity;
  double pressure;
  float  wind;
  bool   battery;
};

extern struct SensorsData ActualData;