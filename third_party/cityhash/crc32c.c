// CRC-32C (Castagnoli) en logiciel, à la place de l'instruction SSE 4.2 que CityHashCrc
// attend : même résultat sur toutes les machines, ARM compris. Écrit pour Trait64.
#include <stdint.h>

static uint32_t crc32c_table(uint32_t byte)
{
  uint32_t crc = byte;
  for (int k = 0; k < 8; k++)
    crc = (crc >> 1) ^ (0x82F63B78u & (0u - (crc & 1u)));
  return crc;
}

uint64_t _mm_crc32_u64(uint64_t crc, uint64_t v)
{
  static uint32_t table[256];
  static int ready = 0;
  if (!ready) {
    for (uint32_t i = 0; i < 256; i++)
      table[i] = crc32c_table(i);
    ready = 1;
  }
  uint32_t c = (uint32_t)crc;
  for (int i = 0; i < 8; i++, v >>= 8)
    c = (c >> 8) ^ table[(c ^ (uint32_t)v) & 0xFF];
  return c;
}
