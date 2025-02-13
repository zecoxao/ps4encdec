#include <fcntl.h>
#include <fstream>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include "mio.hpp"
#include "aes_xts.hpp"

static const uint32_t SECTOR_SIZE = 512;

static inline void print_hex(const char * label, const std::vector<uint8_t> &bytes) {
    printf("%s: ", label);
    for (auto &hex: bytes) {
        printf("%02X", hex);
    }
    printf("\n");
}

uint8_t hex_to_nibble(char input)
{
  if(input >= '0' && input <= '9')
    return input - '0';
  if(input >= 'A' && input <= 'F')
    return input - 'A' + 10;
  if(input >= 'a' && input <= 'f')
    return input - 'a' + 10;
  throw std::invalid_argument(std::format("Invalid hex digit %c", input));
  return 0;
}

std::vector<uint8_t> hex_to_bytes(const char *hex) {
    std::vector<uint8_t> bytes;

    size_t len = strlen(hex);
    if (len % 2 != 0) {
        return bytes;
    }

    for (size_t i = 0; i < len; i += 2) {
        bytes.push_back(hex_to_nibble(hex[i]) << 4 | hex_to_nibble(hex[i+1]));
    }

    return bytes;
}

int create_sparse(const char *file_path, uint64_t len) {
    int fd = open(file_path, O_CREAT | O_WRONLY, 0660);
    if (fd < 0) {
        return -1;
    }
    int ret = ftruncate64(fd, len);
    close(fd);
    return ret;
}

bool file_exists(const char *file_path) {
    return std::ifstream(file_path).good();
}

int handle_error(const std::error_code& error) {
    const auto& errmsg = error.message();
    std::printf("error mapping file: %s, exiting...\n", errmsg.c_str());
    return error.value();
}

int main(int argc, const char *argv[]) {
    std::error_code error;

    if (argc < 6) {
        printf("Usage: %s <XTS_KEY> <XTS_TWEAK> <IV_OFFSET> <in_path> <out_path>\n", basename(argv[0]));
        printf("For details see:\n"
            "\thttps://www.psdevwiki.com/ps4/Mounting_HDD_in_Linux\n"
            "\thttps://www.psdevwiki.com/ps4/Partitions\n");
        return 1;
    }

    auto xts_key = hex_to_bytes(argv[1]);
    auto xts_tweak = hex_to_bytes(argv[2]);
    uint64_t iv_offset = strtoull(argv[3], NULL, 10);
    const char *in_filepath = argv[4];
    const char *out_filepath = argv[5];
    
    if (xts_key.size() != 16) {
        printf("XTS_KEY must be 16 bytes\n");
        return -1;
    }

    if (xts_tweak.size() != 16) {
        printf("XTS_TWEAK must be 16 bytes\n");
        return -1;
    }

    if (file_exists(out_filepath)) {
        printf("Output file '%s' exists, please remove.\n", out_filepath);
        return -1;
    }

    // mmap input file read only
    mio::mmap_source source = mio::make_mmap_source(in_filepath, 0, mio::map_entire_file, error);
    if (error) {
        handle_error(error);
        return -1;
    }

    // create output file with the input file size
    uint64_t source_len = source.size();
    int ret = create_sparse(out_filepath, source_len);
    if (ret != 0) {
        printf("Error creating output file of size %ld bytes", source_len);
        return -1;
    }

    // mmap output file read/write
    mio::mmap_sink output = mio::make_mmap_sink(out_filepath, 0, mio::map_entire_file, error);
    if (error) {
        handle_error(error);
        return -1;
    }

    print_hex("XTS KEY", xts_key);
    print_hex("XTS TWK", xts_tweak);

    auto xts = Cipher::AES::XTS_128(xts_key, xts_tweak, SECTOR_SIZE);

    uint64_t num_sectors = source_len / SECTOR_SIZE;
    for (uint64_t sector_index = 0; sector_index < num_sectors; ++sector_index) {
        xts.crypt(Cipher::Mode::Decrypt, sector_index + iv_offset, &source[SECTOR_SIZE * sector_index], &output[SECTOR_SIZE * sector_index]);
    }

    return 0;
}