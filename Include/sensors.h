void SensorsInit();

void WeatherSensorRead();

#define WIND_FREQ_KNOT_SCALER 0.36

struct SensorsData {
  float temperature;
  int    humidity;
  double pressure;
  float  wind;
  bool   low_battery;
};

extern struct SensorsData ActualData;