

void SensorsInit();

void SensorsRead();

struct SensorsData {
  char device_name;
  float  temperature;
  float  humidity;
  float  pressure;
  float  light;
};

extern struct SensorsData ActualData;