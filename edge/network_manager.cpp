#include "network_manager.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

// 이 3개는 네트워크 통신을 위해 필요한 파일들 -> socket(), connect() 등의 함수를 사용하기 위해 필요
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <assert.h>

#include "opcode.h"
using namespace std;

NetworkManager::NetworkManager() // 기본 생성자.
{
  this->sock = -1;   // 소켓이 초기화되지 않았음을 나타내는 값으로 설정
  this->addr = NULL; // 주소가 초기화되지 않았음을 나타내는 값으로 설정
  this->port = -1;   // 포트가 초기화되지 않았음을 나타내는 값으로 설정
}
// 예를 들어, ./edge --addr 127.0.0.1 --port 5555로 하면 addr엔 "127.0.0.1", port = 5555가 들어가게 됨

// line 25~51 : 주소와 포트를 저장하거나 꺼내는 함수들. 이해 불필요.
NetworkManager::NetworkManager(const char *addr, int port)
{
  this->sock = -1;
  this->addr = addr;
  this->port = port;
}

void NetworkManager::setAddress(const char *addr)
{
  this->addr = addr;
}

const char *NetworkManager::getAddress()
{
  return this->addr;
}

void NetworkManager::setPort(int port)
{
  this->port = port;
}

int NetworkManager::getPort()
{
  return this->port;
}

int NetworkManager::init()
{
  struct sockaddr_in serv_addr; // 접속할 서버의 주소와 포트를 저장하는 구조체

  this->sock = socket(PF_INET, SOCK_STREAM, 0); // 새로운 socket을 생성.
  // PF_INET: IPv4 인터넷 프로토콜을 사용, SOCK_STREAM: TCP 소켓을 사용, 0: 기본 프로토콜을 사용 (TCP의 경우)

  if (this->sock == FAILURE)
  {
    cout << "[*] Error: socket() error" << endl;
    cout << "[*] Please try again" << endl;
    exit(1);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(this->addr);
  serv_addr.sin_port = htons(this->port);
  /*
  memset
  → 구조체 전체를 0으로 초기화

  AF_INET
  → IPv4 주소 체계

  inet_addr(this->addr)
  → "127.0.0.1" 같은 문자열 IP를 컴퓨터가 쓰는 숫자 IP로 변환

  htons(this->port)
  → port 번호를 network byte order로 변환
  */

  if (connect(this->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == FAILURE)
  {
    cout << "[*] Error: connect() error" << endl;
    cout << "[*] Please try again" << endl;
    exit(1);
  }

  cout << "[*] Connected to " << this->addr << ":" << this->port << endl;

  return sock;
}

// Modified 2026-06-03: send payload using dlen from ProcessManager.
int NetworkManager::sendData(uint8_t *data, int dlen) // data: 네트워크 매니저로 보낼 데이터, dlen: 네트워크 매니저로 보낼 데이터의 길이
// data는 process_manager.cpp에서 processData()에서 만든 payload
// dlen은 process_manager.cpp에서 processData()에서 만든 payload의 길이
{
  int sock, tbs, sent, offset, num, jlen;
  unsigned char opcode;
  uint8_t n[4];
  uint8_t *p;

  // line 107~120 : 가장 중요한 부분. 서버로 데이터를 보낸다.
  sock = this->sock;
  // Modified 2026-06-03 payload: avg_power(4) || max_temp(4) || min_temp(4) || avg_humid(4) || month(4)
  // edge -> server: opcode (OPCODE_DATA, 1 byte)
  opcode = OPCODE_DATA; // 지금부터 data packet을 보낼 거라는 것을 서버에게 알리는 opcode
  tbs = 1;
  offset = 0;
  while (offset < tbs) // opcode를 서버로 보낼 때, write() 함수가 한번에 모든 데이터를 보내지 못할 수도 있기 때문에, offset을 이용해서 보낸 데이터의 양을 추적하면서, 모든 데이터를 보낼 때까지 반복
  {
    sent = write(sock, &opcode + offset, tbs - offset); // 이번 write에서 보낸 실제 데이터의 양
    if (sent > 0)
      offset += sent;
  }
  assert(offset == tbs);

  // Modified 2026-06-03: payload 길이는 processData()가 계산한 dlen을 그대로 사용한다.
  tbs = dlen;
  offset = 0;
  while (offset < tbs)
  {
    sent = write(sock, data + offset, tbs - offset);
    if (sent > 0)
      offset += sent;
  }
  assert(offset == tbs);

  return 0;
}

// TODO: Please revise or implement this function as you want. You can also remove this function if it is not needed
uint8_t NetworkManager::receiveCommand() // 서버로부터 명령을 받음.
{
  int sock;
  uint8_t opcode; // OPCODE_DONE, OPCODE_QUIT, OPCODE_WAIT 등이 들어갈 수 있음.
  // OPCODE_WAIT는 서버가 아직 명령을 보낼 준비가 안 됐다는 것을 의미.
  // OPCODE_DONE는 서버가 명령을 보낼 준비가 됐고, 명령이 OPCODE_QUIT이 아니라는 것을 의미.
  // OPCODE_QUIT는 서버가 명령을 보낼 준비가 됐고, 명령이 OPCODE_QUIT이라는 것을 의미.
  uint8_t *p;

  sock = this->sock;
  opcode = OPCODE_WAIT;

  while (opcode == OPCODE_WAIT) // 서버가 WAIT을 보내면 계속 기다림. 서버가 DONE이나 QUIT을 보낼 때까지 반복
    read(sock, &opcode, 1);

  // 즉, WAIT 보냄 -> 학습 요청 > 학습 종료 > DONE 보냄 > 이후 반복

  assert(opcode == OPCODE_DONE || opcode == OPCODE_QUIT);

  return opcode;
}
