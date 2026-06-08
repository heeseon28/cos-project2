#ifndef __BYTE_OP_H__
#define __BYTE_OP_H__

#include <cstdio>
#include <cstring>

/*
 * byte_op.h
 * ----------
 * 이 파일은 C++ 정수 값과 network byte buffer 사이의 변환을 담당한다.
 *
 * 네트워크로 숫자를 보낼 때는 int 자체를 그대로 write()하는 대신,
 * 정해진 byte order로 buffer에 직접 쪼개 넣어야 한다. 이 프로젝트는
 * 모든 multi-byte integer를 Big Endian, 즉 가장 높은 자리 byte부터
 * 낮은 자리 byte 순서로 저장한다.
 *
 * VAR_TO_MEM_* 계열:
 *   변수 값을 byte buffer에 저장하고, 포인터 p를 다음 빈 위치로 이동한다.
 *
 * MEM_TO_VAR_* 계열:
 *   byte buffer에서 값을 복원하고, 포인터 p를 읽은 byte 수만큼 이동한다.
 *
 * 주의:
 *   p는 반드시 uint8_t*, unsigned char*, char*처럼 1 byte 단위로 이동하는
 *   buffer pointer여야 한다. int* 같은 포인터를 넘기면 포인터 이동 단위가
 *   달라져 protocol이 깨질 수 있다.
 */

/*
 * Store the lowest 1 byte of v into memory.
 * Big Endian 여부는 1 byte 값에서는 차이가 없지만, protocol 표현을 맞추기
 * 위해 이름을 BIG_ENDIAN으로 유지한다.
 *
 * Before: p -> next empty byte
 * After : p -> byte after the stored value
 */
#define VAR_TO_MEM_1BYTE_BIG_ENDIAN(v, p)   *(p++) = v & 0xff;

/*
 * Store v as 2 bytes in Big Endian order.
 *
 * Example:
 *   v = 0x1234
 *   buffer receives: 0x12 0x34
 */
#define VAR_TO_MEM_2BYTES_BIG_ENDIAN(v, p)   *(p++) = (v >> 8) & 0xff; *(p++) = v & 0xff;

/*
 * Store v as 4 bytes in Big Endian order.
 *
 * Example:
 *   v = 0x12345678
 *   buffer receives: 0x12 0x34 0x56 0x78
 *
 * Edge -> Server payload에서 avg_power, temperature, humidity, month 같은
 * feature를 4 bytes 정수로 보낼 때 이 macro를 사용한다.
 */
#define VAR_TO_MEM_4BYTES_BIG_ENDIAN(v, p)   *(p++) = (v >> 24) & 0xff; *(p++) = (v >> 16) & 0xff; *(p++) = (v >> 8) & 0xff; *(p++) = v & 0xff;

/*
 * Read 1 byte from memory and restore it to v.
 * p is advanced by exactly 1 byte.
 */
#define MEM_TO_VAR_1BYTE_BIG_ENDIAN(p, v)   v = (p[0] & 0xff); p += 1;

/*
 * Read 2 bytes from memory and restore a Big Endian integer.
 *
 * Example:
 *   buffer has: 0x12 0x34
 *   v becomes : 0x1234
 */
#define MEM_TO_VAR_2BYTES_BIG_ENDIAN(p, v)   v = ((p[0] & 0xff) << 8) | (p[1] & 0xff); p += 2;

/*
 * Read 4 bytes from memory and restore a Big Endian integer.
 *
 * Server/test code가 C++에서 만든 packet을 다시 숫자로 해석할 때 사용한다.
 * Python 쪽에서 같은 값을 읽을 때는 int.from_bytes(..., "big")과 같다.
 */
#define MEM_TO_VAR_4BYTES_BIG_ENDIAN(p, v)   v = ((p[0] & 0xff) << 24) | ((p[1] & 0xff) << 16) | ((p[2] & 0xff) << 8) | (p[3] & 0xff); p += 4;

/*
 * Print a byte buffer in hexadecimal form.
 * Packet이 실제로 어떤 byte sequence로 만들어졌는지 눈으로 확인할 때 쓴다.
 */
#define PRINT_MEM(p, len)   printf("Print buffer:\n  >> ");   for (int i=0; i<len; i++) {     printf("%02x ", p[i]);     if (i % 16 == 15) printf("\n  >> ");   }   printf("\n");

#endif /* __BYTE_OP_H__ */
