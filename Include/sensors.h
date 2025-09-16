void SensorsInit();

void WeatherSensorRead();

#define WIND_FREQ_KNOT_SCALER 3.6

struct SensorsData {
  float temperature;
  int    humidity;
  double pressure;
  float  wind;
  bool   battery;
};

extern struct SensorsData ActualData;