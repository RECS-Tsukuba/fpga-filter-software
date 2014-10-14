/*
 * Copyright (c) 2014 University of Tsukuba
 * Reconfigurable computing systems laboratory
 *
 * Licensed under GPLv3 (http://www.gnu.org/copyleft/gpl.html)
 */
#ifndef FILTER_CORE_FPGA_COMMUNICATOR_H_
#define FILTER_CORE_FPGA_COMMUNICATOR_H_

#include <admxrc2.h>
#include <opencv2/opencv.hpp>
#include <array>
#include <atomic>
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
constexpr size_t REFRESH_REG = 0x40;
constexpr size_t ENABLE_REG = 0x41;
constexpr size_t IMAGE_SIZE_REG = 0x42;
constexpr size_t IMAGE_WIDTH_REG = 0x43;
constexpr size_t LEFT_BUTTON_CLICK_FLAG_REG = 0x44;
constexpr size_t LEFT_BUTTON_CLICK_X_REG = 0x45;
constexpr size_t LEFT_BUTTON_CLICK_Y_REG = 0x46;

constexpr size_t FINISH_REG = 0x60;

constexpr size_t DEBUG0_REG = 0x7C;
constexpr size_t DEBUG1_REG = 0x7D;
constexpr size_t DEBUG2_REG = 0x7E;
constexpr size_t DEBUG3_REG = 0x7F;
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

class MouseEvent {
 private:
  FPGACommunicator& com_;

  std::atomic<uint32_t>& x_;
  std::atomic<uint32_t>& y_;
  cv::Size image_size_;

  std::atomic<uint32_t> is_clicked_;
 public:
  MouseEvent(filter_core::FPGACommunicator& com,
             std::atomic<uint32_t>& x,
             std::atomic<uint32_t>& y,
             cv::Size image_size)
    : com_(com), x_(x), y_(y), image_size_(image_size), is_clicked_(0) {}
  ~MouseEvent() {
    com_.write(filter_core::LEFT_BUTTON_CLICK_FLAG_REG,
               is_clicked_.load(std::memory_order::memory_order_relaxed) > 0);
    com_.write(filter_core::LEFT_BUTTON_CLICK_X_REG, x_.load());
    com_.write(filter_core::LEFT_BUTTON_CLICK_Y_REG, y_.load());
  }
 public:
  void set(int x, int y) {
    if (x >= 0 && x < image_size_.width && y >= 0 && y < image_size_.height) {
      is_clicked_.fetch_add(1);
      x_ = x;
      y_ = y;
    }
  }
};
}  // namespace filter_core


namespace filter_core {

filter_core::FPGACommunicator& OutputUserRegisters(
    filter_core::FPGACommunicator& com);
filter_core::FPGACommunicator& SendImageSize(filter_core::FPGACommunicator& com,
                                             uint32_t total_size,
                                             uint32_t width);
filter_core::FPGACommunicator& SendRefresh(filter_core::FPGACommunicator& com);
}  // namespace filter_core

#endif

