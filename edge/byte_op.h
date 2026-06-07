#ifndef __BYTE_OP_H__
#define __BYTE_OP_H__

#include <cstdio>
#include <cstring>

/*
 * 정수형 데이터를 Big Endian 형식의 1바이트 데이터로 변환하여
 * 버퍼에 저장한 뒤 포인터를 다음 위치로 이동시킨다.
 */
#define VAR_TO_MEM_1BYTE_BIG_ENDIAN(v, p) \
  *(p++) = v & 0xff;

/*
 * 정수형 데이터를 Big Endian 형식의 2바이트 데이터로 분할하여
 * 버퍼에 저장한 뒤 포인터를 다음 위치로 이동시킨다.
 */
#define VAR_TO_MEM_2BYTES_BIG_ENDIAN(v, p) \
  *(p++) = (v >> 8) & 0xff; *(p++) = v & 0xff;

/*
 * 정수형 데이터를 Big Endian 형식의 4바이트 데이터로 분할하여
 * 버퍼에 저장한 뒤 포인터를 다음 위치로 이동시킨다.
 * 네트워크 전송을 위한 직렬화(Serialization)에 사용된다.
 */
#define VAR_TO_MEM_4BYTES_BIG_ENDIAN(v, p) \
  *(p++) = (v >> 24) & 0xff; *(p++) = (v >> 16) & 0xff; *(p++) = (v >> 8) & 0xff; *(p++) = v & 0xff;

/*
 * 버퍼에 저장된 1바이트 Big Endian 데이터를 읽어
 * 정수형 변수로 복원한 뒤 포인터를 이동시킨다.
 */
#define MEM_TO_VAR_1BYTE_BIG_ENDIAN(p, v) \
  v = (p[0] & 0xff); p += 1;

/*
 * 버퍼에 저장된 2바이트 Big Endian 데이터를 읽어
 * 원래 정수형 값으로 복원한 뒤 포인터를 이동시킨다.
 */
#define MEM_TO_VAR_2BYTES_BIG_ENDIAN(p, v) \
  v = ((p[0] & 0xff) << 8) | (p[1] & 0xff); p += 2;

/*
 * 버퍼에 저장된 4바이트 Big Endian 데이터를 읽어
 * 원래 정수형 값으로 복원한 뒤 포인터를 이동시킨다.
 * 네트워크로 수신한 데이터를 역직렬화(Deserialization)할 때 사용된다.
 */
#define MEM_TO_VAR_4BYTES_BIG_ENDIAN(p, v) \
  v = ((p[0] & 0xff) << 24) | ((p[1] & 0xff) << 16) | ((p[2] & 0xff) << 8) | (p[3] & 0xff); p += 4;

/*
 * 버퍼에 저장된 데이터를 16진수 형태로 출력한다.
 * 직렬화된 패킷이 올바르게 생성되었는지 확인하기 위한 디버깅 용도로 사용된다.
 */
#define PRINT_MEM(p, len) \
  printf("Print buffer:\n  >> "); \
  for (int i=0; i<len; i++) { \
    printf("%02x ", p[i]); \
    if (i % 16 == 15) printf("\n  >> "); \
  } \
  printf("\n");

#endif /* __BYTE_OP_H__ */