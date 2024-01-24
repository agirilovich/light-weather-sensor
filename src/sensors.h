

void SensorsInit();

void WeatherSensorRead();

struct SensorsData {
  float  temperature;
  float  humidity;
  float  pressure;
  float  light;
};

extern struct SensorsData ActualData;