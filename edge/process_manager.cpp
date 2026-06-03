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

// Modified 2026-06-03: aggregation changed to [avg_power, max_temp, min_temp, avg_humid, month].
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
  int tmp, avg_power, max_temp, min_temp, avg_humid, month; // Modified 2026-06-03: 5-feature vector variables
  long long sum_power;
  time_t ts;
  struct tm *tm;

  tdata = ds->getTemperatureData(); // 데이터셋에서 온도 데이터를 가져옴
  hdata = ds->getHumidityData();    // 데이터셋에서 습도 데이터를 가져옴
  num = ds->getNumHouseData();      // 데이터셋에 포함된 HouseData의 개수를 가져옴

  // Modified 2026-06-03: feature vector = [avg_power, max_temp, min_temp, avg_humid, month]
  // Aggregation types: average(power), max(temperature), min(temperature), average(humidity), month(time feature)

  sum_power = 0;
  ds->setIterator();
  for (int i = 0; i < num; i++)
  {
    house = ds->getNextHouseData();
    if (!house)
      break;

    pdata = house->getPowerData();
    sum_power += (int)pdata->getValue();
  }
  avg_power = (num > 0) ? (int)(sum_power / num) : 0; // feature 1: average power value among houses

  max_temp = (int)tdata->getMax();    // feature 2: maximum daily temperature
  min_temp = (int)tdata->getMin();    // feature 3: minimum daily temperature
  avg_humid = (int)hdata->getValue(); // feature 4: average daily humidity

  // Feature 5: getting the month value from the timestamp
  ts = ds->getTimestamp();
  tm = localtime(&ts);
  month = tm->tm_mon + 1; // tm_mon의 범위는 0~11이므로 1을 더해서 1~12로 만듦

  // Example) initializing the memory to send to the network manager
  // 네트워크 매니저로 보낼 데이터를 저장할 메모리를 초기화하는 과정. ret는 네트워크 매니저로 보낼 데이터를 저장하는 버퍼의 포인터, BUFLEN은 버퍼의 크기 (예: 1024)
  memset(ret, 0, BUFLEN);
  *dlen = 0; // 길이를 알아야 네트워크 매니저로 보낼 때 정확한 길이만큼 보내므로, dlen을 0으로 초기화
  p = ret;

  // Modified 2026-06-03: save five features as 4-byte signed big-endian integers.
  // payload = avg_power(4) || max_temp(4) || min_temp(4) || avg_humid(4) || month(4) = 20 bytes
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(avg_power, p);
  *dlen += 4;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(max_temp, p);
  *dlen += 4;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(min_temp, p);
  *dlen += 4;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(avg_humid, p);
  *dlen += 4;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(month, p);
  *dlen += 4;

  return ret; // 네트워크 매니저로 보낼 데이터가 저장된 버퍼의 포인터를 반환
}

/*
Modified 2026-06-03 summary
1. DataSet에서 온도 데이터 가져오기
2. DataSet에서 습도 데이터 가져오기
3. DataSet 안의 집 개수 가져오기
4. 모든 집을 순회하면서 평균 전력 avg_power 계산
5. 하루 최고 온도 max_temp 계산
6. 하루 최저 온도 min_temp 계산
7. 하루 평균 습도 avg_humid 계산
8. timestamp에서 month 계산
9. payload buffer 초기화
10. avg_power, max_temp, min_temp, avg_humid, month를 각각 4 bytes로 저장
11. 총 길이 dlen = 20으로 설정
12. buffer 반환
*/
