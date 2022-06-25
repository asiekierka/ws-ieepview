#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <lzsa.h>
#include <wonderful.h>
#include "fs.h"
#include "base64.h"
#include "qrcodegen.h"
#include "sha1.h"

#define screen ((uint16_t*) 0x3800)
#define qrcode_dataAndTemp ((uint8_t*) 0x2800)

const uint8_t __far hex2chr[16] = {
'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};
const uint8_t __far hex2chr_lower[16] = {
'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};

int16_t ieep_pos = 0;
uint16_t key_last;
uint16_t ieep_max_size;

volatile uint16_t vblank_ticks;
void vblank_int_handler(void);

void wait_for_vbl(void) {
	uint16_t last_vblank_ticks = vblank_ticks;
	while (last_vblank_ticks == vblank_ticks) {
		cpu_halt();
	}
}

void draw_ieep_ui_static(void) {
	for (uint8_t i = 0; i < 16; i++) {
		screen[(i << 5) | 3] = 0xB3;
	}
	for (uint8_t i = 0; i < 28; i++) {
		screen[(16 << 5) | i] = 0xCD;
	}
	screen[(16 << 5) | 3] = 0xCF;
	memset(screen + (32 * 17), 0, 28 * 2);
	if (system_color_active()) {
		screen[(17 << 5) | 27] = 'C';
	}
	screen[(17 << 5) | 0] = 'i';
	screen[(17 << 5) | 1] = 'e';
	screen[(17 << 5) | 2] = 'e';
	screen[(17 << 5) | 3] = 'p';
	screen[(17 << 5) | 4] = 'v';
	screen[(17 << 5) | 5] = 'i';
	screen[(17 << 5) | 6] = 'e';
	screen[(17 << 5) | 7] = 'w';
	screen[(17 << 5) | 9] = '0';
	screen[(17 << 5) | 10] = '.';
	screen[(17 << 5) | 11] = '2';
}

void draw_ieep_data(void) {
	int16_t ieep_pos_local = ieep_pos - 0x40;
	if (ieep_pos_local < 0) {
		ieep_pos_local = 0;
	}
	if (ieep_pos_local > (ieep_max_size - 0x80)) {
		ieep_pos_local = (ieep_max_size - 0x80);
	}
	ieep_pos_local &= (~0x7);
	for (uint8_t i = 0; i < 16; i++) {
		uint16_t addr = ieep_pos_local + (i << 3);
		screen[(i << 5)] = hex2chr[(addr >> 8) & 0xF];
		screen[(i << 5) | 1] = hex2chr[(addr >> 4) & 0xF];
		screen[(i << 5) | 2] = hex2chr[(addr >> 0) & 0xF];

		uint16_t w1 = ieep_read_word(addr);
		uint16_t w2 = ieep_read_word(addr+2);
		uint16_t w3 = ieep_read_word(addr+4);
		uint16_t w4 = ieep_read_word(addr+6);
		uint16_t b0 = (ieep_pos == (addr)) ? 0x200 : 0;
		uint16_t b1 = (ieep_pos == (addr+1)) ? 0x200 : 0;
		uint16_t b2 = (ieep_pos == (addr+2)) ? 0x200 : 0;
		uint16_t b3 = (ieep_pos == (addr+3)) ? 0x200 : 0;
		uint16_t b4 = (ieep_pos == (addr+4)) ? 0x200 : 0;
		uint16_t b5 = (ieep_pos == (addr+5)) ? 0x200 : 0;
		uint16_t b6 = (ieep_pos == (addr+6)) ? 0x200 : 0;
		uint16_t b7 = (ieep_pos == (addr+7)) ? 0x200 : 0;

		screen[(i << 5) | 4] = hex2chr[(w1 >> 4) & 0xF] | b0;
		screen[(i << 5) | 5] = hex2chr[(w1 >> 0) & 0xF] | b0;
		screen[(i << 5) | 7] = hex2chr[(w1 >> 12) & 0xF] | b1;
		screen[(i << 5) | 8] = hex2chr[(w1 >> 8) & 0xF] | b1;

		screen[(i << 5) | 10] = hex2chr[(w2 >> 4) & 0xF] | b2;
		screen[(i << 5) | 11] = hex2chr[(w2 >> 0) & 0xF] | b2;
		screen[(i << 5) | 13] = hex2chr[(w2 >> 12) & 0xF] | b3;
		screen[(i << 5) | 14] = hex2chr[(w2 >> 8) & 0xF] | b3;

		screen[(i << 5) | 17] = hex2chr[(w3 >> 4) & 0xF] | b4;
		screen[(i << 5) | 18] = hex2chr[(w3 >> 0) & 0xF] | b4;
		screen[(i << 5) | 20] = hex2chr[(w3 >> 12) & 0xF] | b5;
		screen[(i << 5) | 21] = hex2chr[(w3 >> 8) & 0xF] | b5;

		screen[(i << 5) | 23] = hex2chr[(w4 >> 4) & 0xF] | b6;
		screen[(i << 5) | 24] = hex2chr[(w4 >> 0) & 0xF] | b6;
		screen[(i << 5) | 26] = hex2chr[(w4 >> 12) & 0xF] | b7;
		screen[(i << 5) | 27] = hex2chr[(w4 >> 8) & 0xF] | b7;
	}
}

static uint16_t keypad_r_scan() {
	uint16_t key = keypad_scan();
	uint16_t keyr = key_last & (~key);
	key_last = key;
	return keyr;
}

void init_font_data(void) {
	const uint8_t __far *font_data = asset_map(ASSET_8X8_CHR);
	for (uint16_t i = 0; i < 2048; i++) {
		((uint16_t*) 0x2000)[i] = font_data[i] * 0x101;
	}
}

void draw_ieep_ui(void) {
	wait_for_vbl();
	outportw(IO_DISPLAY_CTRL, 0);
	init_font_data();
	memset(screen, 0, 32 * 20 * 2);
	draw_ieep_ui_static();
	draw_ieep_data();
	wait_for_vbl();
	outportw(IO_DISPLAY_CTRL, DISPLAY_SCR1_ENABLE);
}

static const char __far msg_loading_data[] = "Loading data...";
static const char __far msg_ieep_sha1[] = "IEEP -> SHA1...";
static const char __far msg_ieep_base64[] = "IEEP -> Base64...";
static const char __far msg_base64_qrcode[] = "Base64 -> QR code...";
static const char __far msg_qrcode0_ready[] = "1/1: (B) Exit";
static const char __far msg_qrcode1_ready[] = " X/N: (X4/X2) Move (B) Exit ";
static const char __far msg_backup_restore[] = "Slot X, Backup/Restore/Exit?";

static void draw_ui_clear_status(void) {
	memset(screen + (32 * 17), 0, 28 * 2);
}

static void draw_ui_status(const char __far *text) {
	draw_ui_clear_status();
	uint8_t slen = strlen(text);
	uint8_t sofs = (28 - slen) >> 1;
	for (uint8_t i = 0; i < slen; i++) {
		screen[32 * 17 + i + sofs] = text[i];
	}
}

static void calc_ieep_sha1(uint8_t *eeprom_data, char *digest_text) {
	SHA1_CTX eeprom_sha1_data;
	uint8_t digest[SHA1_DIGEST_SIZE];
	uint8_t i;

	SHA1_Init(&eeprom_sha1_data);
	SHA1_Update(&eeprom_sha1_data, eeprom_data, ieep_max_size);
	SHA1_Final(&eeprom_sha1_data, digest);
	for (uint8_t k = 0; k < SHA1_DIGEST_SIZE; k++) {
		digest_text[i++] = hex2chr_lower[digest[k] >> 4];
		digest_text[i++] = hex2chr_lower[digest[k] & 0xF];
	}
}

void show_ieep_qrcode(void) {
	uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(27)];
	uint8_t eeprom_data[0x800];
	uint8_t eeprom_bank = 0;
	char sha1_digest[40];

	wait_for_vbl();
	outportw(IO_DISPLAY_CTRL, 0);

	memset(screen, 0, (32 * 2) * 16 + (28 * 2));
	uint16_t i = 128;
	for (uint8_t iy = 0; iy < 16; iy++) {
		for (uint8_t ix = 0; ix < 16; ix++) {
			screen[((iy + 1) << 5) | (ix + 11)] = i++;
		}
	}
	init_font_data();
	memset(MEM_TILE(128), 0, 256 * 16);
	draw_ui_status(msg_loading_data);
	wait_for_vbl();
	outportw(IO_DISPLAY_CTRL,DISPLAY_SCR1_ENABLE);

	ieep_read_data(0x0, eeprom_data, ieep_max_size);

	draw_ui_status(msg_ieep_sha1);
	calc_ieep_sha1(eeprom_data, sha1_digest);

	screen[(5 << 5) | 3] = 'S';
	screen[(5 << 5) | 4] = 'H';
	screen[(5 << 5) | 5] = 'A';
	screen[(5 << 5) | 6] = '1';
	for (uint8_t ix = 0; ix < 8; ix++) {
		screen[(7 << 5) | (ix+1)] = sha1_digest[ix];
		screen[(8 << 5) | (ix+1)] = sha1_digest[ix+8];
		screen[(9 << 5) | (ix+1)] = sha1_digest[ix+16];
		screen[(10 << 5) | (ix+1)] = sha1_digest[ix+24];
		screen[(11 << 5) | (ix+1)] = sha1_digest[ix+32];
	}
	uint8_t qr_max_version = 27;
	uint16_t qr_bytes_per_code = 1026;

	while (true) {
		uint16_t eeprom_bank_count = system_color_active() ? (qr_max_version == 11 ? 9 : 2) : 1;
		uint16_t b_offset = qr_bytes_per_code * eeprom_bank;
		uint16_t b_length = qr_bytes_per_code;
		if ((ieep_max_size - b_offset) < b_length) {
			b_length = ieep_max_size - b_offset;
		}

		draw_ui_status(msg_ieep_base64);
		uint16_t slen = base64_encode(qrcode_dataAndTemp, 1400, eeprom_data + b_offset, b_length);
		draw_ui_status(msg_base64_qrcode);
		qrcodegen_encodeBinary(qrcode_dataAndTemp, slen, qrcode,
			qrcodegen_Ecc_LOW, 3, qr_max_version, qrcodegen_Mask_AUTO, true);
		if (system_color_active()) {
			draw_ui_status(msg_qrcode1_ready);
			screen[(17 << 5) | 1] = hex2chr[eeprom_bank + 1];
			screen[(17 << 5) | 3] = hex2chr[eeprom_bank_count];
		} else {
			draw_ui_status(msg_qrcode0_ready);
		}

		i = 128;
		uint16_t isize = qrcodegen_getSize(qrcode);
		bool zoom = false;
		if (isize < 64) {
			isize *= 2;
			zoom = true;
		}
		uint16_t ixofs = (zoom ? 32 : 64) - (qrcodegen_getSize(qrcode) >> 1);
		uint16_t iyofs = (ixofs < 3) ? 0 : (ixofs - 3);
		for (uint8_t iy = 0; iy < 16; iy++) {
			int16_t iyo = (iy << (zoom ? 2 : 3)) - iyofs;
			for (uint8_t ix = 0; ix < 16; ix++, i++) {
				uint16_t *tptr = (uint16_t*) MEM_TILE(i);
				int16_t ixo = (ix << (zoom ? 2 : 3)) - ixofs;
				if (zoom) {
					for (uint8_t py = 0; py < 4; py++) {
						uint16_t v = 0;
						for (uint8_t px = 0; px < 4; px++) {
							v <<= 2;
							int16_t qx = ixo + px;
							int16_t qy = iyo + py;
							if (qx >= 0 && qy >= 0 && qrcodegen_getModule(qrcode, qx, qy)) {
								v |= 0x0303;
							}
						}
						*(tptr++) = v;
						*(tptr++) = v;
					}
				} else {
					for (uint8_t py = 0; py < 8; py++, tptr++) {
						uint16_t v = 0;
						for (uint8_t px = 0; px < 8; px++) {
							v <<= 1;
							int16_t qx = ixo + px;
							int16_t qy = iyo + py;
							if (qx >= 0 && qy >= 0 && qrcodegen_getModule(qrcode, qx, qy)) {
								v |= 0x0101;
							}
						}
						*tptr = v;
					}
				}
			}
		}

		while (1) {
			wait_for_vbl();

			uint16_t keyr = keypad_r_scan();
			if (keyr & KEY_B) {
				return;
			}
			if (keyr & (KEY_Y4 | KEY_X4)) {
				eeprom_bank = (eeprom_bank == 0) ? (eeprom_bank_count - 1) : (eeprom_bank - 1);
				break;
			}
			if (keyr & (KEY_Y2 | KEY_X2)) {
				eeprom_bank = (eeprom_bank == (eeprom_bank_count - 1)) ? 0 : (eeprom_bank + 1);
				break;
			}
			if (keyr & (KEY_Y1 | KEY_X1)) {
				if (qr_max_version == 27) {
					qr_max_version = 11;
					qr_bytes_per_code = 240;
				} else {
					qr_max_version = 27;
					qr_bytes_per_code = 1026;
				}
				eeprom_bank = 0;
				break;
			}
		}

		memset(MEM_TILE(128), 0, 256 * 16);
	}
}

void show_ieep_write(uint16_t address, uint8_t value) {
	wait_for_vbl();

	draw_ui_clear_status();
	screen[(17 << 5) | 0] = 'S';
	screen[(17 << 5) | 1] = 'e';
	screen[(17 << 5) | 2] = 't';
	screen[(17 << 5) | 3] = ' ';
	screen[(17 << 5) | 4] = '=';
	screen[(17 << 5) | 5] = ' ';

	while (1) {
		wait_for_vbl();

		uint16_t keyr = keypad_r_scan();
		if (keyr & KEY_A) {
			uint16_t word = ieep_read_word(address & (~0x1));
			if (address & 1) {
				word = (word & 0xFF) | (value << 8);
			} else {
				word = (word & 0xFF00) | value;
			}
			ieep_write_word(address & (~0x1), word);
			return;
		}
		if (keyr & KEY_B) {
			return;
		}
		if (keyr & (KEY_X1)) value += 0x01;
		if (keyr & (KEY_X3)) value -= 0x01;
		if (keyr & (KEY_X2)) value += 0x10;
		if (keyr & (KEY_X4)) value -= 0x10;
		screen[(17 << 5) | 6] = hex2chr[value >> 4];
		screen[(17 << 5) | 7] = hex2chr[value & 0xF];
	}
}

void show_ieep_backup_restore(void) {
	uint8_t eeprom_data[0x800];

	uint8_t slot_id = 0;
	uint8_t oper_mode = 2;
	while (1) {
		wait_for_vbl();
		uint16_t keyr = keypad_r_scan();
		draw_ui_status(msg_backup_restore);
		screen[(17 << 5) | 5] = '0' + slot_id;
		if (oper_mode == 0) {
			for (uint8_t i = 8; i < 14; i++) screen[(17 << 5) | i] ^= 0x200;
		} else if (oper_mode == 1) {
			for (uint8_t i = 15; i < 22; i++) screen[(17 << 5) | i] ^= 0x200;
		} else if (oper_mode == 2) {
			for (uint8_t i = 23; i < 27; i++) screen[(17 << 5) | i] ^= 0x200;
		}
		if (keyr & KEY_X1) slot_id = (slot_id + 1) & 3;
		if (keyr & KEY_X3) slot_id = (slot_id - 1) & 3;
		if (keyr & KEY_X2) oper_mode = (oper_mode == 2) ? 0 : (oper_mode + 1);
		if (keyr & KEY_X4) oper_mode = (oper_mode == 0) ? 2 : (oper_mode - 1);
		if (keyr & KEY_B) {
			return;
		}
		if (keyr & KEY_A) {
			break;
		}
	}
	uint16_t slot_seg = 0x1000 + (slot_id * 0x800);
	if (oper_mode == 0) {
		// backup
		for (uint16_t i = 0; i < ieep_max_size; i += 2) {
			uint16_t w = ieep_read_word(i);
			*((uint8_t __far*) MK_FP(slot_seg, i)) = w & 0xFF;
			*((uint8_t __far*) MK_FP(slot_seg, i + 1)) = w >> 8;
		}
	} else if (oper_mode == 1) {
		// restore
		for (uint16_t i = 0; i < ieep_max_size; i += 2) {
			uint16_t w = *((uint8_t __far*) MK_FP(slot_seg, i));
			w |= *((uint8_t __far*) MK_FP(slot_seg, i + 1)) << 8;
			ieep_write_word(i, w);
		}
	}
}

void main() {
	system_set_mode(MODE_COLOR);
	video_shade_lut_set(GRAY_LUT_DEFAULT);
	outportw(IO_SCR_PAL_0, 0x7520);
	outportw(IO_SCR_PAL_1, 0x0257);
	if (system_color_active()) {
		MEM_COLOR_PALETTE(0)[0] = 0x0FFF;
		MEM_COLOR_PALETTE(0)[1] = 0x0AAA;
		MEM_COLOR_PALETTE(0)[2] = 0x0555;
		MEM_COLOR_PALETTE(0)[3] = 0x0000;

		MEM_COLOR_PALETTE(1)[3] = 0x0FFF;
		MEM_COLOR_PALETTE(1)[2] = 0x0AAA;
		MEM_COLOR_PALETTE(1)[1] = 0x0555;
		MEM_COLOR_PALETTE(1)[0] = 0x0000;
		ieep_max_size = IEEP_SIZE_WSC;
	} else {
		ieep_max_size = IEEP_SIZE_WS;
	}

	*((uint16_t*) 0x0038) = FP_OFF(vblank_int_handler);
	*((uint16_t*) 0x003A) = FP_SEG(vblank_int_handler);
	outportb(IO_INT_ENABLE, INTR_ENABLE_VBLANK);
	cpu_irq_enable();

 	outportb(IO_SCR_BASE, SCR1_BASE((uint16_t)screen));
	draw_ieep_ui();
	key_last = 0;

	while (1) {
		wait_for_vbl();

		bool ieep_pos_changed = false;
		uint16_t keyr = keypad_r_scan();
		if (keyr & KEY_X1) {
			ieep_pos -= 8;
			ieep_pos_changed = true;
		}
		if (keyr & KEY_X3) {
			ieep_pos += 8;
			ieep_pos_changed = true;
		}
		if (keyr & KEY_Y1) {
			ieep_pos -= 64;
			ieep_pos_changed = true;
		}
		if (keyr & KEY_Y3) {
			ieep_pos += 64;
			ieep_pos_changed = true;
		}
		if (keyr & KEY_X4) {
			ieep_pos -= 1;
			ieep_pos_changed = true;
		}
		if (keyr & KEY_X2) {
			ieep_pos += 1;
			ieep_pos_changed = true;
		}
		if (ieep_pos_changed) {
			if (ieep_pos < 0) ieep_pos = 0;
			if (ieep_pos >= ieep_max_size) ieep_pos = ieep_max_size - 1;
			draw_ieep_data();
		}
		if (keyr & KEY_A) {
			show_ieep_write(ieep_pos, ieep_read_byte(ieep_pos));
			draw_ieep_ui();
		}

		if (keyr & KEY_Y4) {
			show_ieep_qrcode();
			draw_ieep_ui();
		}
		if (keyr & KEY_Y2) {
			show_ieep_backup_restore();
			draw_ieep_ui();
		}
	}
}
