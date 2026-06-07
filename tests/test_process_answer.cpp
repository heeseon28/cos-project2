#include "../edge/setting.h"
#include "../edge/edge.h"

#include <iostream>
#include <ctime>

#include "../edge/byte_op.h"

#define BUFLEN 1024

using namespace std;

int main(int argc, char *argv[])
{
  // 데이터셋 및 센서 데이터에 접근하기 위한 객체 선언
  DataReceiver *dr;
  DataSet *ds;
  HouseData *house;
  TemperatureData *tdata;
  HumidityData *hdata;
  PowerData *pdata;

  // 계산 과정에서 사용할 변수들
  int num, tmp;
  time_t curr, ts;

  // 온도, 습도, 전력 데이터의 통계값 저장 변수
  double max_temp, avg_temp, min_temp;
  double max_humid, avg_humid, min_humid;
  double power, sum_power, avg_power, max_power, min_power;
  
  // 직렬화에 사용할 버퍼
  unsigned char buf[BUFLEN];
  unsigned char *p;

 // 2021-01-01 00:00:00에 해당하는 timestamp
  curr = 1609459200;
  // 지정한 시점의 데이터셋을 불러온다.
  dr = new DataReceiver();
  ds = dr->getDataSet(curr);
  
  // 1. Write a statement to get the timestamp value to 'ts' and print out the value (please refer to dataset.h)
  // 데이터셋의 생성 시점을 확인하여 데이터 수집 시간을 파악
  ts = ds->getTimestamp();
  cout << "timestamp: " << ts << endl;

  // 2. Write a statement to get the number of house data that contains the private information and the power value to 'num' (dataset.h)
  // 전력 사용량 정보가 포함된 전체 가구 수를 확인
  num = ds->getNumHouseData();
  cout << "# of house data: " << num << endl;

  // 3. Write a statement to get the first house data to 'house' (please refer to dataset.h) 
  // 첫 번째 가구 데이터 접근 예제
  house = ds->getHouseData(0);
  
  // Write a statement to get the 10th house data to 'house' (dataset.h)
  // 열 번째 가구 데이터 접근 예제
  house = ds->getHouseData(9);
  
  // Get the power data to 'pdata' (house_data.h)
  // 특정 가구의 전력 사용량 데이터에 접근
  pdata = house->getPowerData();
  
  // Get the daily power value to 'power' and print out the value (power_data.h)
  // 개별 가구의 전력 사용량을 확인
  power = pdata->getValue();
  cout << "Power: " << power << endl;
  
  // Explicitly cast the type from double to int and assign it to 'tmp', and print out the value
  // 전송 및 직렬화를 위해 전력 사용량을 정수형으로 변환
  tmp = (int)pdata->getValue();
  cout << "Power (casted): " << tmp << endl;
  
  // Compute the value averaged over all the power data by using 'sum_power' and 'num', 
  // assign the average value to 'avg_power', and print out the value
  sum_power = 0;
  // 전체 가구의 전력 사용량을 평균화하여 대표 소비 패턴을 추출
  for (int i=0; i<num; i++)
  {
    house = ds->getHouseData(i);
    pdata = house->getPowerData();
    sum_power += pdata->getValue();
  }
  avg_power = sum_power / num;
  cout << "Power (avg): " << avg_power << endl;
  
  // Find the maximum value among all the power data 
    // 전체 가구 중 최대 전력 사용량을 계산하여 피크 소비 특성을 추출
  max_power = -1;
  for (int i=0; i<num; i++)
  {
    house = ds->getHouseData(i);
    pdata = house->getPowerData();
    power = pdata->getValue();

    if (power > max_power)
      max_power = power;
  }
  cout << "Power (max): " << max_power << endl;
  
  // Find the minimum value among all the power data
  // 전체 가구 중 최소 전력 사용량을 계산하여 최저 소비 특성을 추출
  min_power = 10000;
  for (int i=0; i<num; i++)
  {
    house = ds->getHouseData(i);
    pdata = house->getPowerData();
    power = pdata->getValue();

    if (power < min_power)
      min_power = power;
  }
  cout << "Power (min): " << min_power << endl;

  // 4. Write a statement to get the temperature data to 'tdata' (dataset.h)
  // 온도 데이터 객체를 가져온다.
  tdata = ds->getTemperatureData();
  
  // Get the maximum value of the daily temperature (temperature_data.h)
  // 온도 데이터를 대표 Feature로 활용하기 위해 최고 기온을 조회
  max_temp = tdata->getMax();
  cout << "Temperature (max): " << max_temp << endl;
  
  // Get the average value of the daily temperature (temperature_data.h)
  // 평균 기온을 이용하여 하루의 전반적인 온도 특성을 추출
  avg_temp = tdata->getValue();
  cout << "Temperature (avg): " << avg_temp << endl;
  
  // Get the minimum value of the daily temperature (temperature_data.h)
  // 최저 기온을 이용하여 온도 변화 범위를 파악
  min_temp = tdata->getMin();
  cout << "Temperature (min): " << min_temp << endl;
  
  // Explicitly cast the type of the maximum value from double to int, assign the resultant value to 'tmp', and print it out
  // 직렬화를 위해 온도 데이터를 정수형으로 변환
  tmp = (int)tdata->getMax();
  cout << "Temperature (max, casted): " << tmp << endl;

  // 5. Write a statement to get the humidity data to 'hdata' (dataset.h)
  // 습도 데이터 객체를 가져온다.
  hdata = ds->getHumidityData();
  
  // Get the maximum value of the daily humidity (humidity_data.h)
  // 최고 습도를 조회하여 환경 특성을 파악
  max_humid = hdata->getMax();
  cout << "Humidity (max): " << max_humid << endl;
  
  // Get the average value of the daily humidity (humidity_data.h)
  // 평균 습도를 대표 환경 Feature로 사용
  avg_humid = hdata->getValue();
  cout << "Humidity (avg): " << avg_humid << endl;
  
  // Get the minimum value of the daily humidity (humidity_data.h)
  // 최저 습도를 이용하여 습도 변화 특성을 파악
  min_humid = hdata->getMin();
  cout << "Humidity (min): " << min_humid << endl;
  
  // Explicitly cast the type of the minimum value from double to int, assign the resultant value to 'tmp', and print it out
  // 직렬화를 위해 습도 데이터를 정수형으로 변환
  tmp = (int)hdata->getMin();
  cout << "Humidity (min, casted): " << tmp << endl;

  // 6. Initialize the buffer 'buf' with zeros (its length is defined as BUFLEN) (use the memset() function, please google it!)
  // 이전 데이터의 영향을 제거하기 위해 버퍼를 초기화
  memset(buf, 0, BUFLEN);

  // 7. Write statements to save the values into 'buf' using 'p' as follows:
  // # of house data (2 bytes) || maximum power (integer) (4 bytes) || maximum temperature (integer) (2 bytes)
  // Print out the buffer
  // Please use the macros defined in edge/byte_op.h
  // Aggregation 결과를 네트워크 전송이 가능한 바이트 스트림 형태로 직렬화
  p = buf;
  // Aggregation 결과를 네트워크 전송이 가능한 바이트 스트림 형태로 변환
  VAR_TO_MEM_2BYTES_BIG_ENDIAN(num, p);
  tmp = (int)min_power;
  // 계산된 Feature를 Big Endian 형식으로 직렬화하여 버퍼에 저장
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(tmp, p);
  tmp = (int)max_temp;
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(tmp, p);
  // 생성된 패킷의 전체 길이를 계산
  tmp = p - buf;
  PRINT_MEM(buf, tmp);
  // 직렬화 결과를 확인하기 위해 버퍼 내용을 출력
	return 0;
}