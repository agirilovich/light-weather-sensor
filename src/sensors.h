

void SensorsInit();

void WeatherSensorRead();

struct SensorsData {
  float temperature;
  float  humidity;
  double pressure;
  float  wind;
  bool   battery;
};

extern struct SensorsData ActualData;