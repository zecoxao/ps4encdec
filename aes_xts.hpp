#pragma once

#include <vector>
#include "AES.hpp"

namespace Cipher {
	enum Mode {
		Encrypt = 0,
		Decrypt,
	};

	namespace AES {
		class XTS_128 {
			Aes<128> tweak;
			Aes<128> cipher;
			uint32_t sector_size;
			public:
				XTS_128(const std::vector<uint8_t> &data_key, const std::vector<uint8_t> &tweak_key, uint32_t sector_size);
				int crypt(Mode mode, uint64_t seqno, const char *in, char *out);
		};
	};
};