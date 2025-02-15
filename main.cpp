#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <print>
#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <vector>
#include "mio.hpp"
#include "aes_xts.hpp"


static const uint32_t SECTOR_SIZE = 512;

static inline void print_hex(const char * label, const std::vector<uint8_t> &bytes) {
   std::print("{}: ", label);
    for (auto &hex: bytes) {
        std::print("{:02X}", hex);
    }
    std::println("");
}

uint8_t hex_to_nibble(char input) {
  if(input >= '0' && input <= '9')
    return input - '0';
  if(input >= 'A' && input <= 'F')
    return input - 'A' + 10;
  if(input >= 'a' && input <= 'f')
    return input - 'a' + 10;
  throw std::invalid_argument(std::format("Invalid hex digit %c", input));

  // will never get here, but compiler complains about the lack of return
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
    std::ofstream ofs(file_path, std::ios::binary | std::ios::out);
    ofs.seekp(len-1);
    ofs.write("\x00", 1);
    return ofs.rdstate(); // good is 0
}

bool file_exists(const char *file_path) {
    return std::ifstream(file_path).good();
}

int main(int argc, const char *argv[]) {
    std::error_code error;

    if (argc < 6) {
        std::filesystem::path program = argv[0];
        std::println("Usage: {} <XTS_KEY> <XTS_TWEAK> <IV_OFFSET> <in_path> <out_path>", program.filename().string());
        std::println("For details see:");
        std::println("\thttps://www.psdevwiki.com/ps4/Mounting_HDD_in_Linux");
        std::println("\thttps://www.psdevwiki.com/ps4/Partitions");
        return 1;
    }

    auto xts_key = hex_to_bytes(argv[1]);
    auto xts_tweak = hex_to_bytes(argv[2]);
    uint64_t iv_offset = strtoull(argv[3], NULL, 10);
    const char *in_filepath = argv[4];
    const char *out_filepath = argv[5];
    
    if (xts_key.size() != 16) {
        std::println("XTS_KEY must be 16 bytes");
        return -1;
    }

    if (xts_tweak.size() != 16) {
        std::println("XTS_TWEAK must be 16 bytes");
        return -1;
    }

    if (file_exists(out_filepath)) {
        std::println("Output file '{}' exists, please remove.", out_filepath);
        return -1;
    }

    // mmap input file read only with shared semantics
    mio::shared_mmap_source source = mio::make_mmap_source(in_filepath, 0, mio::map_entire_file, error);
    if (error) {
        std::println("Error mapping input file '{}': {}", in_filepath, error.message());
        return -1;
    }

    // create (sparse) output file with the input file size
    uint64_t source_len = source.size();
    int ret = create_sparse(out_filepath, source_len);
    if (ret != 0) {
        std::println("Error creating output file of size {} bytes", source_len);
        return -1;
    }

    // map output file read / write with shared semantics
    mio::shared_mmap_sink output = mio::make_mmap_sink(out_filepath, 0, mio::map_entire_file, error);
    if (error) {
        std::println("Error mapping output file '{}': {}", out_filepath, error.message());
        return -1;
    }

    const auto processor_count = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::promise<void> p;
    auto sf = p.get_future().share();
    std::println("Decrypting '{}' with {} threads using AES-NI", in_filepath, processor_count);

    //print_hex("XTS KEY", xts_key);
    //print_hex("XTS TWK", xts_tweak);

    uint64_t num_sectors = source_len / SECTOR_SIZE;
    uint64_t slice_size = num_sectors / processor_count;
    uint64_t slice_start = 0;

    // create threads
    for (auto i = processor_count; i > 0; --i) {
        uint64_t slice_end = slice_start + slice_size;

        // last thread might get fewer sectors than a normal slice
        if (i == 1) {
            slice_end = num_sectors;
        }
        threads.emplace_back([sf, i, source, slice_start, slice_end, xts_key, xts_tweak, iv_offset](mio::shared_mmap_sink output) {
            sf.wait();
            auto xts = Cipher::AES::XTS_128(xts_key, xts_tweak, SECTOR_SIZE);
            //std::println("Thread {:2d} sectors {:9d} - {:9d}", i, slice_start, slice_end);

            for (uint64_t sector_index = slice_start; sector_index < slice_end; ++sector_index) {
                xts.crypt(Cipher::Mode::Decrypt, sector_index + iv_offset, &source[SECTOR_SIZE * sector_index], &output[SECTOR_SIZE * sector_index]);
            }
        }, output);
        slice_start += slice_size;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // kick off threads
    p.set_value();

    // wait for everything to finish
    for (auto &t: threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    std::println("Decrypted in {}m{:02}s", elapsed_sec / 60, elapsed_sec % 60);
    std::printf("Speed %4.0f MiB/sec\n", source_len / 1024 / 1024.0 / elapsed_sec);
    std::println("Please wait while data is flushed to disk");

    return 0;
}
