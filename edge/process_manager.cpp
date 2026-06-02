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

// TODO: You should implement this function if you want to change the result of the aggregation
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
  int tmp, min_humid, min_temp, min_power, month; // 수정점 1
  time_t ts;
  struct tm *tm;

  tdata = ds->getTemperatureData(); // 데이터셋에서 온도 데이터를 가져옴
  hdata = ds->getHumidityData();    // 데이터셋에서 습도 데이터를 가져옴
  num = ds->getNumHouseData();      // 데이터셋에 포함된 HouseData의 개수를 가져옴

  // line 41~62 : 수정점 2
  // Example) I will give the minimum daily temperature (1 byte), the minimum daily humidity (1 byte),
  // the minimum power data (2 bytes), the month value (1 byte) to the network manager
  //  ㄴ> 네트워크 매니저로 보낼 데이터는 하루치 데이터에서 최소 온도, 최소 습도, 최소 전력 데이터 (2바이트), 월 값을 포함하는 5바이트의 데이터. 이것이 현재 aggregation의 설계

  // Example) getting the minimum daily temperature -> 온도 데이터로부터 최소 온도를 구하는 예시
  min_temp = (int)tdata->getMin(); // feature 1

  // Example) getting the minimum daily humidity -> 습도 데이터로부터 최소 습도를 구하는 예시
  min_humid = (int)hdata->getMin(); // feature 2

  // Example) getting the minimum power value -> 전력 데이터로부터 최소 전력 데이터를 구하는 예시
  min_power = 10000; // feature 3, 초기값은 충분히 큰 값으로 설정 (예: 10000), 2바이트임. 왜? 최댓값은 65535이므로
  for (int i = 0; i < num; i++)
  {
    house = ds->getHouseData(i);
    pdata = house->getPowerData();
    tmp = (int)pdata->getValue();

    if (tmp < min_power)
      min_power = tmp;
  } // 전력값 중 최솟값을 찾는 반복문

  // Example) getting the month value from the timestamp -> 데이터셋의 timestamp로부터 월 값을 구하는 예시
  ts = ds->getTimestamp();
  tm = localtime(&ts);
  month = tm->tm_mon + 1; // tm_mon의 범위는 0~11이므로 1을 더해서 1~12로 만듦

  // Example) initializing the memory to send to the network manager
  // 네트워크 매니저로 보낼 데이터를 저장할 메모리를 초기화하는 과정. ret는 네트워크 매니저로 보낼 데이터를 저장하는 버퍼의 포인터, BUFLEN은 버퍼의 크기 (예: 1024)
  memset(ret, 0, BUFLEN);
  *dlen = 0; // 길이를 알아야 네트워크 매니저로 보낼 때 정확한 길이만큼 보내므로, dlen을 0으로 초기화
  p = ret;

  // line 75~84 : 수정점 3
  // Example) saving the values in the memory -> 그리고 하나씩 길이를 더해서 정확한 길이를 기록
  VAR_TO_MEM_1BYTE_BIG_ENDIAN(min_temp, p);
  *dlen += 1;
  VAR_TO_MEM_1BYTE_BIG_ENDIAN(min_humid, p);
  *dlen += 1;
  VAR_TO_MEM_2BYTES_BIG_ENDIAN(min_power, p);
  *dlen += 2;
  VAR_TO_MEM_1BYTE_BIG_ENDIAN(month, p);
  *dlen += 1;

  return ret; // 네트워크 매니저로 보낼 데이터가 저장된 버퍼의 포인터를 반환
}

/*
1. DataSet에서 온도 데이터 가져오기
2. DataSet에서 습도 데이터 가져오기
3. DataSet 안의 집 개수 가져오기
4. 하루 최저 온도 계산
5. 하루 최저 습도 계산
6. 모든 집을 순회하면서 최저 전력 계산
7. timestamp에서 month 계산
8. payload buffer 초기화
9. min_temp 1 byte 저장
10. min_humid 1 byte 저장
11. min_power 2 bytes 저장
12. month 1 byte 저장
13. 총 길이 dlen = 5로 설정
14. buffer 반환
*/
