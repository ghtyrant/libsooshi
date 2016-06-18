// This code has been inspired by
// http://www.barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code

#include "sooshi.h"

static guint32 reflect(guint32 data, guint32 nBits)
{
	guint32 reflection = 0x00000000;
	guchar bit;

	for (bit = 0; bit < nBits; ++bit)
	{
		if (data & 0x01)
			reflection |= (1 << ((nBits - 1) - bit));

		data = (data >> 1);
	}

	return (reflection);
}	

void sooshi_crc32_init(SooshiState *state)
{
    crc32_t remainder;
	gint dividend;
	guchar bit;

    for (dividend = 0; dividend < 256; ++dividend)
    {
        remainder = dividend << (CRC32_WIDTH - 8);

        for (bit = 8; bit > 0; --bit)
        {
            if (remainder & CRC32_TOPBIT)
                remainder = (remainder << 1) ^ CRC32_POLYNOMIAL;
            else
                remainder = (remainder << 1);
        }

        state->crc_table[dividend] = remainder;
    }
}

crc32_t sooshi_crc32_calculate(SooshiState *state, guchar const message[], gint nBytes)
{
    crc32_t remainder = CRC32_INITIAL_REMAINDER;
    guchar data;
	gint byte;

    for (byte = 0; byte < nBytes; ++byte)
    {
        data = ((guchar)reflect(message[byte], 8)) ^ (remainder >> (CRC32_WIDTH - 8));
  		remainder = state->crc_table[data] ^ (remainder << 8);
    }

    return (crc32_t)(reflect(remainder, CRC32_WIDTH) ^ CRC32_FINAL_XOR_VALUE);
}
