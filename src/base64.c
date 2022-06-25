#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char __far b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encsize(int size) {
	return 4 * ((size + 2) / 3);
}

int base64_encode(char *dest, int size, unsigned char *src, int slen) {
	int dlen, i, j;
	uint8_t a, b, c;

	dlen = base64_encsize(slen);

	if (dlen >= size) {
		return -1;
	}
	if (slen == 0) {
		if (size > 0)
		{
			dest[0] = 0;
			return 0;
		}
		return -1;
	}

	for (i = 0, j = 0; i < slen;) {
		a = src[i++];

		// b and c may be off limit
		b = i < slen ? src[i++] : 0;
		c = i < slen ? src[i++] : 0;

		dest[j++] = b64_table[(a >> 2) & 0x3F];
		dest[j++] = b64_table[((b >> 4) | (a << 4)) & 0x3F];
		dest[j++] = b64_table[((c >> 6) | (b << 2)) & 0x3F];
		dest[j++] = b64_table[c & 0x3F];
	}

	// Pad zeroes at the end
	switch (slen % 3) {
	case 1:
		dest[j - 2] = '=';
	case 2:
		dest[j - 1] = '=';
	}

	// Always add \0
	dest[j] = 0;

	return dlen;
}
