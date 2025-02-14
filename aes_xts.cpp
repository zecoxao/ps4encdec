#include <vector>
#include "AES.hpp"
#include "aes_xts.hpp"

namespace Cipher::AES {
	typedef union {
		uint64_t dword[2];
		uint8_t  bytes[16];
	} Tweak;

	// update tweak values via GF(2^128) multiply
	void update_tweak(Tweak &tweak) {
		uint8_t carry_in = 0;
		uint8_t carry_out = 0;

		// left shift each element by 1, accounting for carry
		for(uint32_t i = 0; i < 0x10; i++) {
			carry_out = (tweak.bytes[i] & 0x80) != 0;
			tweak.bytes[i] = ((tweak.bytes[i] << 1) + carry_in) & 0xFF;
			carry_in = carry_out;
		}

		// if final carry_out, XOR "special value" of 135 / 0x87
		if(carry_out)
			tweak.bytes[0] ^= 0x87;
	} 

	XTS_128::XTS_128(const std::vector<uint8_t> &data_key, const std::vector<uint8_t> &tweak_key, uint32_t sector_size) : 
		tweak(Cipher::Aes<128>(tweak_key.data())),
		cipher(Cipher::Aes<128>(data_key.data())),
		sector_size(sector_size) {
			// nada
	}

	int XTS_128::crypt(Mode mode, uint64_t tweak_seed, const char *in, char *out) {
		Tweak tweak;
		uint8_t buf[0x10];

		// starting seed for tweak
		tweak.dword[0] = tweak_seed;
		tweak.dword[1] = 0;

		// encrypt the starting value to get our starting tweak values
		this->tweak.encrypt_block(tweak.bytes);

		// process each block in sector
		for(uint32_t i = 0; i < this->sector_size; i += 0x10) {
			// pre-tweak block
			for(uint32_t j = 0; j < 0x10; j++) {
				buf[j] = in[i+j] ^ tweak.bytes[j];
			}

			// apply block cipher
			switch (mode) {
				case Mode::Encrypt:
					this->cipher.encrypt_block(buf);
					break;
				case Mode::Decrypt:
					this->cipher.decrypt_block(buf);
					break;
				default:
					return -2;
			}

			// post-tweak block
			for(uint32_t j = 0; j < 0x10; j++) {
				out[i+j] = buf[j] ^ tweak.bytes[j];
			}

			update_tweak(tweak);
		}

		return 0;
	}
}