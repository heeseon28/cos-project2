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

// Modified 2026-06-03 15:51 KST (feature/version3): aggregation changed to [month, avg_power].
uint8_t *ProcessManager::processData(DataSet *ds, int *dlen)
// *ds : 하루치 데이터 묶음, dlen: 네트워크 매니저로 보낼 데이터의 길이, 반환값: 네트워크 매니저로 보낼 데이터 (byte 형태)
{
  uint8_t *ret, *p; // ret: 네트워크 매니저로 보낼 데이터, p: ret에서 데이터를 저장할 위치를 가리키는 포인터
  int num;
  HouseData *house; // 데이터셋에 포함된 각 집의 데이터
  PowerData *pdata; // 데이터셋에 포함된 전력 데이터
  ret = (uint8_t *)malloc(BUFLEN);
  int tmp, avg_power, month; // Modified 2026-06-03 15:51 KST (feature/version3): 2-feature vector variables
  long long sum_power;
  time_t ts;
  struct tm *tm;

  num = ds->getNumHouseData(); // 데이터셋에 포함된 HouseData의 개수를 가져옴

  // Modified 2026-06-03 15:51 KST (feature/version3): feature vector = [month, avg_power]
  // Feature 1: month, extracted from the dataset timestamp.
  // Feature 2: avg_power, average power value among houses.

  sum_power = 0;
  ds->setIterator();
  for (int i = 0; i < num; i++)
  {
    house = ds->getNextHouseData();
    if (!house)
      break;

    pdata = house->getPowerData();
    tmp = (int)pdata->getValue();
    sum_power += tmp;
  }
  avg_power = (num > 0) ? (int)(sum_power / num) : 0; // feature 2: average power value among houses

  // Feature 1: getting the month value from the timestamp
  ts = ds->getTimestamp();
  tm = localtime(&ts);
  month = tm->tm_mon + 1; // tm_mon의 범위는 0~11이므로 1을 더해서 1~12로 만듦

  // Example) initializing the memory to send to the network manager
  // 네트워크 매니저로 보낼 데이터를 저장할 메모리를 초기화하는 과정. ret는 네트워크 매니저로 보낼 데이터를 저장하는 버퍼의 포인터, BUFLEN은 버퍼의 크기 (예: 1024)
  memset(ret, 0, BUFLEN);
  *dlen = 0; // 길이를 알아야 네트워크 매니저로 보낼 때 정확한 길이만큼 보내므로, dlen을 0으로 초기화
  p = ret;

  // Modified 2026-06-03 15:51 KST (feature/version3): save two version3 features as 4-byte signed big-endian integers.
  // payload = month(4) || avg_power(4) = 8 bytes
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(month, p);
  *dlen += 4;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(avg_power, p);
  *dlen += 4;

  return ret; // 네트워크 매니저로 보낼 데이터가 저장된 버퍼의 포인터를 반환
}

/*
Modified 2026-06-03 15:51 KST (feature/version3) summary
1. DataSet 안의 집 개수 가져오기
2. 모든 집을 순회하면서 avg_power 계산
3. timestamp에서 month 계산
4. payload buffer 초기화
5. month, avg_power를 각각 4 bytes로 저장
6. 총 길이 dlen = 8으로 설정
7. buffer 반환
*/
