#ifndef FILTER_CORE_FPGA_COMMUNICATOR_H_
#define FILTER_CORE_FPGA_COMMUNICATOR_H_

#include <admxrc2.h>
#include <array>
#include <cstdint>
#include <memory>


namespace filter_core {

constexpr size_t MAX_BANK = 16;

constexpr size_t BANK_REG = 0x0;
constexpr size_t PAGE_REG = 0x1;
constexpr size_t MEMCTL_REG = 0x2;
constexpr size_t STATUS_REG = 0x4;
constexpr size_t MEMSTAT_REG = 0x6;

auto MODEx_REG = [](size_t n) { return 0x10 + n; };

// ユーザレジスタのインデックス
constexpr size_t REFLESH_REG = 0x40;
constexpr size_t ENABLE_REG = 0x41;
constexpr size_t IMAGE_SIZE_REG = 0x42;
constexpr size_t FINISH_REG = 0x60;
}  // namespace filter_core


namespace filter_core {

using fpga_space_t = volatile uint32_t*;
using BankInfo = std::array<ADMXRC2_BANK_INFO, filter_core::MAX_BANK>;
}  // namespace filter_core


namespace filter_core {
/*!
 * \class FPGACommunicator
 * \brief FPGAボードとの通信クラス
 */
class FPGACommunicator {
 public:
  std::shared_ptr<ADMXRC2_HANDLE> handle_;
  ADMXRC2_CARD_INFO info_;
  filter_core::fpga_space_t space_;
  filter_core::BankInfo bank_info_;

  std::shared_ptr<uint8_t> read_buffer_, write_buffer_;
  std::shared_ptr<ADMXRC2_DMADESC> read_descriptor_, write_descriptor_;
  uint32_t dma_mode_;

 public:
  FPGACommunicator(
      double local_clock_rate,
      double memory_clock_rate,
      const std::string& bitstream_filename,
      size_t buffer_size);

 public:
  /*!
   * \brief ユーザレジスタを読み込む
   * この演算子を使って書き込むことはできない
   *
   * \param i インデックス
   * \return インデックスで指定したユーザレジスタの値
   */
  uint32_t operator[](size_t i) const noexcept { return space_[i]; }
 public:
  void read(void* buffer,
            uint64_t offset,
            unsigned long length,
            uint32_t bank);
  void write(uint32_t i, size_t v) noexcept;
  void write(void* buffer,
             uint64_t offset,
             unsigned long length,
             uint32_t bank);
};
}  // namespace filter_core

#endif

