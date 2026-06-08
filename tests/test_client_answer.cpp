#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <getopt.h>
#include <stdint.h>

#include "../edge/byte_op.h"

#define BUFLEN        1024
#define OPCODE_SUM    1
#define OPCODE_REPLY  2

void protocol_execution(int sock);
void error_handling(const char *message);

void usage(const char *pname)
{
  printf(">> Usage: %s [options]\n", pname);
  printf("Options\n");
  printf("  -a, --addr       Server's address\n");
  printf("  -p, --port       Server's port\n");
  exit(0);
}

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
  char msg[] = "Hello, World!\n";
	char message[30] = {0, };
	int c, port, tmp, str_len;
  char *pname;
  uint8_t *addr;
  uint8_t eflag;

  pname = argv[0];
  addr = NULL;
  port = -1;
  eflag = 0;

  while (1)
  {
    int option_index = 0;
    static struct option long_options[] = {
      {"addr", required_argument, 0, 'a'},
      {"port", required_argument, 0, 'p'},
      {0, 0, 0, 0}
    };

    const char *opt = "a:p:0";

    c = getopt_long(argc, argv, opt, long_options, &option_index);

    if (c == -1)
      break;

    switch (c)
    {
      case 'a':
        tmp = strlen(optarg);
        addr = (uint8_t *)malloc(tmp);
        memcpy(addr, optarg, tmp);
        break;

      case 'p':
        port = atoi(optarg);
        break;

      default:
        usage(pname);
    }
  }

  if (!addr)
  {
    printf("[*] Please specify the server's address to connect\n");
    eflag = 1;
  }

  if (port < 0)
  {
    printf("[*] Please specify the server's port to connect\n");
    eflag = 1;
  }

  if (eflag)
  {
    usage(pname);
    exit(0);
  }

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		error_handling("socket() error");
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr((const char *)addr);
	serv_addr.sin_port = htons(port);

	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
		error_handling("connect() error");
  printf("[*] Connected to %s:%d\n", addr, port);
  
  protocol_execution(sock);

	close(sock);
	return 0;
}

void protocol_execution(int sock)
{
  // 서버로 전송할 클라이언트 이름을 저정한다.
  char msg[] = "Alice";
  // 송수신 데이터를 임시로 저장하기 위한 버퍼이다.
  char buf[BUFLEN];
  // tbs, tbr, offset을 이용하여 데이터가 부분적으로 송수신되는 상황을 처리한다.
  int tbs, sent, tbr, rcvd, offset;
  int len;

  // tbs: the number of bytes to send
  // tbr: the number of bytes to receive
  // offset: the offset of the message

  // 1. Alice -> Bob: length of the name (4 bytes) || name (length bytes)
  // Send the length information (4 bytes)
  //문자열을 보내기 전에 길이를 먼저 보내서 서버가 이후 몇 바이트를 읽어야하는 알 수 있도록 함
  len = strlen(msg);
  printf("[*] Length information to be sent: %d\n", len);

  // 서로 다른 시스템에서도 같은 값으로 해석디도록 길이 정보를 네트워크 바이트 순서로 변환
  len = htonl(len);
  // 이름 길이는 4바이트 정수로 전송
  tbs = 4;
  //아직 전송한 바이트가 없으므로 offset을 0으로 초기화
  offset = 0;

  while (offset < tbs)
  {
    // write()가 한 번에 모든 데이터를 보낸다는 보장이 없으므로 남은 바이트만큼 반복 전송
    // Cast &len to char* so offset moves by bytes, not by sizeof(int).
    sent = write(sock, ((char *)&len) + offset, tbs - offset);
    if (sent > 0)
      offset += sent;
  }

  // Send the name (Alice)
  // 네트워크 바이트 순서로 바꿨던 길이를 다시 원래 값으로 복원하여 실제 이름 길이로 사용
  tbs = ntohl(len);
  // 이름 문자열도 부분 전송될 수 있으므로 offset을 다시 0으로 초기화한다
  offset = 0;

  printf("[*] Name to be sent: %s\n", msg);
  while (offset < tbs)
  {
    // 계산된 길이만큼 Alice 문자열을 서버로 전송
    sent = write(sock, msg + offset, tbs - offset);
    if (sent > 0)
      offset += sent;
  }

  // 2. Bob -> Alice: length of the name (4 bytes) || name (length bytes)
  // Receive the length information (4 bytes)
  // 서버가 보낼 이름의 크기를 먼저 받아서 이후 수신할 문자열 길이를 결정
  tbr = 4;
  // 아직 수신한 바이트가 없으므로 offset을 0으로 초기화
  offset = 0;

  while (offset < tbr)
  {
    // read() 역시 한 번에 모든 데이터를 받는다는 보장이 없으므로 4바이트를 모두 받을 때까지 반복
    // Cast &len to char* so offset moves one byte at a time.
	  rcvd = read(sock, ((char *)&len) + offset, tbr - offset);
    if (rcvd > 0)
      offset += rcvd;
  }
  //수신한 길이 정보를 호스트 바이트 순서로 변환하여 실제 정수 값으로 사용
  len = ntohl(len);
  printf("[*] Length received: %d\n", len);

  // Receive the name (Bob)
  // 앞에서 받은 길이만큼 Bob의 이름 문자열을 수신
  memset(buf, 0, BUFLEN);
  tbr = len;
  // 문자열 수신을 위해 offset을 다시 초기화
  offset = 0;

  while (offset < tbr)
  {
    // 서버가 보낸 이름 데이터를 버퍼에 누적하여 저장
    rcvd = read(sock, buf + offset, tbr - offset);
    if (rcvd > 0)
      offset += rcvd;
  }

	printf("[*] Name received: %s \n", buf);

  // Implement following the instructions below
  // Let's assume there are two opcodes:
  //     1: summation request for the two arguments
  //     2: reply with the result
  // 3. Alice -> Bob: opcode (4 bytes) || arg1 (4 bytes) || arg2 (4 bytes)
  // The opcode should be 1

  // 버퍼에 값을 순서대로 저장하기 위해 포인터 p를 사용
  char *p;
  int i, arg1, arg2;
  
  // 이전에 수신한 데이터가 남아있지 않도록 버퍼 전체를 0으로 초기화한다.
  memset(buf, 0, BUFLEN);
  // 패킷 직렬화를 버퍼의 시작 위치부터 수행
  p = buf;
  // 서버에 요청할 덧셈 연산의 첫 번째 피연산자
  arg1 = 2;
  // 서버에 요청할 덧셈 연산의 두 번째 피연산자
  arg2 = 5;

  // 서버가 요청 종류를 구분할 수 있도록 덧셈 요청 opcode를 패킷의 첫 부분에 저장한다.
  // 이 test protocol은 opcode도 4 bytes로 약속했으므로 arg1/arg2와 동일하게 4 bytes Big Endian으로 저장한다.
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(OPCODE_SUM, p);
  // 첫 번째 정수 인자를 Big Endian 형식으로 버퍼에 저장
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(arg1, p);
  // 두 번째 정수 인자를 Big Endian 형식으로 버퍼에 저장
  VAR_TO_MEM_4BYTES_BIG_ENDIAN(arg2, p);
  // 포인터가 이동한 거리로 실제 생성된 패킷 크기를 계산
  tbs = p - buf;
  // 패킷 전송을 시작하기 위해 offset을 0으로 초기화
  offset = 0;

  printf("[*] # of bytes to be sent: %d\n", tbs);
  printf("[*] The following bytes will be sent\n");
  for (i=0; i<tbs; i++)
    printf ("%02x ", buf[i]);
  printf("\n");

  while (offset < tbs)
  {
    // 생성한 덧셈 요청 패킷 전체가 전송될 때까지 반복해서 보낸다.
    sent = write(sock, buf + offset, tbs - offset);
    if (sent > 0)
      offset += sent;
  }

  // 4. Bob -> Alice: opcode (4 bytes) || result (4 bytes)
  // The opcode should be 2

  // 서버 응답에서 추출할 opcode와 계산 결과를 저장할 변수
  int opcode, result;

  // 서버 응답은 opcode 4바이트와 result 4바이트로 총 8바이트
  tbr = 8; offset = 0;
  // 응답 패킷을 저장하기 전에 버퍼를 초기화
  memset(buf, 0, BUFLEN);

  printf("[*] # of bytes to be received: %d\n", tbr);
  while (offset < tbr)
  {
    // 서버의 응답 패킷을 8바이트 모두 받을 때까지 반복해서 수신
    // Receiving uses tbr, not tbs, because tbr is the expected response length.
    rcvd = read(sock, buf + offset, tbr - offset);
    if (rcvd > 0)
      offset += rcvd;
  }

  printf("[*] The following bytes is received\n");
  for (i=0; i<tbr; i++)
    printf("%02x ", buf[i]);
  printf("\n");

  // 수신한 바이트 데이터를 다시 변수로 복원하기 위해 포인터를 버퍼 시작 위치로 둔다.
  p = buf;
  // 응답 패킷에서 opcode를 추출하여 서버 응답의 종류를 확인한다.
  MEM_TO_VAR_4BYTES_BIG_ENDIAN(p, opcode);
  printf("[*] Opcode: %d\n", opcode);
  // 응답 패킷에서 덧셈 결과 값을 추출한다
  MEM_TO_VAR_4BYTES_BIG_ENDIAN(p, result);
  printf("[*] Result: %d\n", result);
}

void error_handling(const char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}