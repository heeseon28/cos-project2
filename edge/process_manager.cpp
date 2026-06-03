// edge.cpp에서 하루치 데이터를 어떻게 처리할 지에 관련된 코드
#include "process_manager.h"
#include "opcode.h"
#include "byte_op.h" // byte operation을 위한 매크로가 정의된 헤더 파일
#include "setting.h"
#include <cstring>
#include <iostream>
#include <ctime>
using namespace std;

ProcessManager::ProcessManager()
{
  this->num = 0; // 데이터셋에 포함된 HouseData의 개수
}

void ProcessManager::init()
{
}

// Modified 2026-06-03 15:06 KST (feature/version2): aggregation changed to [peak_usage_pressure, climate_volatility_index, avg_power, month].
uint8_t *ProcessManager::processData(DataSet *ds, int *dlen)
// *ds : 하루치 데이터 묶음, dlen: 네트워크 매니저로 보낼 데이터의 길이, 반환값: 네트워크 매니저로 보낼 데이터 (byte 형태)
{
  uint8_t *ret, *p; // ret: 네트워크 매니저로 보낼 데이터, p: ret에서 데이터를 저장할 위치를 가리키는 포인터
  int num, len;
  HouseData *house;       // 데이터셋에 포함된 각 집의 데이터
  Info *info;             // 데이터셋에 포함된 정보 (예: timestamp, num_house_data 등)를 저장하는 구조체
  TemperatureData *tdata; // 데이터셋에 포함된 온도 데이터
  HumidityData *hdata;    // 데이터셋에 포함된 습도 데이터
  PowerData *pdata;       // 데이터셋에 포함된 전력 데이터
  char buf[BUFLEN];       // 네트워크 매니저로 보낼 데이터를 저장하는 버퍼 (byte 형태)
  ret = (uint8_t *)malloc(BUFLEN);
  int tmp, avg_power, max_power, max_temp, min_temp, max_humid, min_humid, month; // Modified 2026-06-03 15:06 KST (feature/version2): version2 raw variables
  int peak_usage_pressure, climate_volatility_index;
  long long sum_power;
  time_t ts;
  struct tm *tm;

  tdata = ds->getTemperatureData(); // 데이터셋에서 온도 데이터를 가져옴
  hdata = ds->getHumidityData();    // 데이터셋에서 습도 데이터를 가져옴
  num = ds->getNumHouseData();      // 데이터셋에 포함된 HouseData의 개수를 가져옴

  // Modified 2026-06-03 15:06 KST (feature/version2): feature vector = [peak_usage_pressure, climate_volatility_index, avg_power, month]
  // Feature 1: Peak Usage Pressure = max_power / avg_power.
  //            To keep fractional precision in an integer protocol, it is stored as ratio * 100.
  //            Example: 150 means max_power is 1.50x avg_power.
  // Feature 2: Climate Volatility Index = (max_temp - min_temp) + (max_humid - min_humid).
  // Feature 3: avg_power. Feature 4: month.

  sum_power = 0;
  max_power = 0;
  ds->setIterator();
  for (int i = 0; i < num; i++)
  {
    house = ds->getNextHouseData();
    if (!house)
      break;

    pdata = house->getPowerData();
    tmp = (int)pdata->getValue();
    sum_power += tmp;

    if (tmp > max_power)
      max_power = tmp;
  }
  avg_power = (num > 0) ? (int)(sum_power / num) : 0; // feature 3: average power value among houses
  peak_usage_pressure = (avg_power > 0) ? (int)((max_power * 100LL) / avg_power) : 0; // feature 1, scaled by 100

  max_temp = (int)tdata->getMax();
  min_temp = (int)tdata->getMin();
  max_humid = (int)hdata->getMax();
  min_humid = (int)hdata->getMin();
  climate_volatility_index = (max_temp - min_temp) + (max_humid - min_humid); // feature 2

  // Feature 4: getting the month value from the timestamp
  ts = ds->getTimestamp();
  tm = localtime(&ts);
  month = tm->tm_mon + 1; // tm_mon의 범위는 0~11이므로 1을 더해서 1~12로 만듦

  // Example) initializing the memory to send to the network manager
  // 네트워크 매니저로 보낼 데이터를 저장할 메모리를 초기화하는 과정. ret는 네트워크 매니저로 보낼 데이터를 저장하는 버퍼의 포인터, BUFLEN은 버퍼의 크기 (예: 1024)
  memset(ret, 0, BUFLEN);
  *dlen = 0; // 길이를 알아야 네트워크 매니저로 보낼 때 정확한 길이만큼 보내므로, dlen을 0으로 초기화
  p = ret;

  // Modified 2026-06-03 15:06 KST (feature/version2): save four version2 features as 4-byte signed big-endian integers.
  // payload = peak_usage_pressure(4) || climate_volatility_index(4) || avg_power(4) || month(4) = 16 bytes
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(peak_usage_pressure, p);
  *dlen += 4;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(climate_volatility_index, p);
  *dlen += 4;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(avg_power, p);
  *dlen += 4;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(month, p);
  *dlen += 4;

  return ret; // 네트워크 매니저로 보낼 데이터가 저장된 버퍼의 포인터를 반환
}

/*
Modified 2026-06-03 15:06 KST (feature/version2) summary
1. DataSet에서 온도 데이터 가져오기
2. DataSet에서 습도 데이터 가져오기
3. DataSet 안의 집 개수 가져오기
4. 모든 집을 순회하면서 avg_power와 max_power 계산
5. peak_usage_pressure = max_power / avg_power * 100 계산
6. climate_volatility_index = (max_temp - min_temp) + (max_humid - min_humid) 계산
7. timestamp에서 month 계산
8. payload buffer 초기화
9. peak_usage_pressure, climate_volatility_index, avg_power, month를 각각 4 bytes로 저장
10. 총 길이 dlen = 16으로 설정
11. buffer 반환
*/
