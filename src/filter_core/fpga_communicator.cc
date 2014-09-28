#include "filter_core/fpga_communicator.h"

#include <admxrc2.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>


#include <iostream>


using std::get;
using std::make_tuple;
using std::move;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::tuple;
using std::weak_ptr;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;
using filter_core::fpga_space_t;


namespace filter_core {
namespace fpga_communicator {

constexpr size_t STATUS_REG_LCLKLOCKED = 0x1U << 0;
constexpr size_t STATUS_REG_LCLKSTICKY = 0x1U << 1;
constexpr size_t STATUS_REG_SHIFT_LOCKED = 8;
constexpr size_t STATUS_REG_SHIFT_STICKY = 16;

constexpr size_t MEMSTAT_REG_SHIFT_TRAINED = 0;

constexpr size_t MODEx_REG_ZBTSSRAM_PIPELINE = 0x1U << 0;

constexpr size_t PAGE_REG_PAGEMASK = 0x7ffU;
constexpr unsigned long PAGE_SIZE = 0x200000U;
constexpr unsigned long PAGE_SHIFT = 21;

constexpr unsigned long MEMORY_WINDOW_ADDRESS = 0x200000U;   /* In bytes */


std::shared_ptr<uint8_t> AllocateBufferForDMA(size_t size);
void CheckForMemoryLock(filter_core::fpga_space_t space,
                        uint32_t lock_flag_number);
void Configure(std::weak_ptr<ADMXRC2_HANDLE> handle,
               const std::string& filename,
               filter_core::fpga_space_t space);
std::shared_ptr<ADMXRC2_HANDLE> GetCardHandle();
ADMXRC2_CARD_INFO GetCardInfo(std::weak_ptr<ADMXRC2_HANDLE> handle);
BankInfo GetBankInfo(std::weak_ptr<ADMXRC2_HANDLE> handle,
                     const ADMXRC2_CARD_INFO& info);
filter_core::fpga_space_t GetFPGASpace(std::weak_ptr<ADMXRC2_HANDLE> handle);
BankInfo GetBankInfo(std::weak_ptr<ADMXRC2_HANDLE> handle,
                     const ADMXRC2_CARD_INFO& info);
void Read(ADMXRC2_HANDLE handle,
          filter_core::fpga_space_t space,
          ADMXRC2_DMADESC dma_descriptor,
          uint8_t* read_buffer,
          uint32_t dma_mode,
          void* buffer,
          uint64_t offset,
          unsigned long length);
void ResetMemorySystem(filter_core::fpga_space_t space);
void SelectBank(filter_core::fpga_space_t space, uint32_t bank);
bool SetClockRates(std::weak_ptr<ADMXRC2_HANDLE> handle,
                   double local_clock,
                   double memory_clock);
void SetMemoryPortConfiguration(filter_core::fpga_space_t space);
std::shared_ptr<ADMXRC2_DMADESC> SetupDMA(
    std::shared_ptr<ADMXRC2_HANDLE> handle,
    uint8_t* buffer,
    size_t buffer_size);
void WaitForLclkDcm(filter_core::fpga_space_t space);
void Write(ADMXRC2_HANDLE handle,
           filter_core::fpga_space_t space,
           ADMXRC2_DMADESC dma_descriptor,
           uint8_t* write_buffer,
           uint32_t dma_mode,
           void* buffer,
           uint64_t offset,
           unsigned long length);
}  // namespace fpga_communicator 
}  // namespace filter_core


namespace filter_core {
namespace fpga_communicator {

inline void Write(filter_core::fpga_space_t space, uint32_t i, uint32_t value)
  { space[i] = value; space[i]; }
}  // namespace fpga_communicator 
}  // namespace filter_core


namespace filter_core {

FPGACommunicator::FPGACommunicator(double local_clock_rate,
                                   double memory_clock_rate,
                                   const string& bitstream_filename,
                                   size_t buffer_size) {
  namespace detail = fpga_communicator;

  const uint32_t LOCK_FLAG_NUMBER = 3;

  handle_ = detail::GetCardHandle();
  info_ = detail::GetCardInfo(handle_);
  space_ = detail::GetFPGASpace(handle_);
  bank_info_ = detail::GetBankInfo(handle_, info_);

  detail::SetClockRates(handle_, local_clock_rate, memory_clock_rate);
  detail::Configure(handle_, bitstream_filename, space_);
  detail::WaitForLclkDcm(space_);

  read_buffer_ = detail::AllocateBufferForDMA(buffer_size);
  write_buffer_ = detail::AllocateBufferForDMA(buffer_size);

  read_descriptor_ = detail::SetupDMA(handle_,
                                      read_buffer_.get(), buffer_size);
  write_descriptor_ = detail::SetupDMA(handle_,
                                       write_buffer_.get(), buffer_size);

  dma_mode_ = ADMXRC2_BuildDMAModeWord(
      info_.BoardType,
      ADMXRC2_IOWIDTH_32,
      0,
      ADMXRC2_DMAMODE_USEREADY | ADMXRC2_DMAMODE_USEBTERM |
        ADMXRC2_DMAMODE_BURSTENABLE);

  detail::SetMemoryPortConfiguration(space_);
  detail::ResetMemorySystem(space_);
  detail::CheckForMemoryLock(space_, LOCK_FLAG_NUMBER);
}

void FPGACommunicator::read(void* buffer,
                            uint64_t offset,
                            unsigned long length,
                            uint32_t bank) {
  fpga_communicator::SelectBank(space_, bank);
  fpga_communicator::Read(*handle_, space_,
                          *read_descriptor_, read_buffer_.get(), dma_mode_,
                          buffer, offset, length);
}

void FPGACommunicator::write(uint32_t i, size_t v) noexcept {
  fpga_communicator::Write(space_, i, v);
}

void FPGACommunicator::write(void* buffer,
                             uint64_t offset,
                             unsigned long length,
                             uint32_t bank) {
  fpga_communicator::SelectBank(space_, bank);
  fpga_communicator::Write(*handle_, space_,
                           *write_descriptor_, write_buffer_.get(), dma_mode_,
                           buffer, offset, length);
}
}  // namespace filter_core


namespace filter_core {
namespace fpga_communicator {

std::shared_ptr<uint8_t> AllocateBufferForDMA(size_t size) {
  uint8_t* buffer = (uint8_t*) ADMXRC2_Malloc(size);

  if (buffer != nullptr) {
    return std::shared_ptr<uint8_t>(
        buffer,
        [](uint8_t* b) { if (b != nullptr) { ADMXRC2_Free(b); } });
  } else {
    throw runtime_error("failed to allocate buffer for DMA");
  }
}

void CheckForMemoryLock(fpga_space_t space, unsigned int lock_flag_number) {
  uint32_t mask = (1 << lock_flag_number) - 1;
  uint32_t status = space[STATUS_REG];
  uint32_t memstat = space[MEMSTAT_REG];

  if (((status >> STATUS_REG_SHIFT_LOCKED) & mask) == 0)
    { throw runtime_error("Not all memory lock flags asserted"); }
  if (((memstat >> MEMSTAT_REG_SHIFT_TRAINED) & mask) == 0)
    { throw runtime_error("Not all memory banks trained"); }

  /* Clear sticky loss-of-lock / loss-of-trained bits */
  space[STATUS_REG] = 0xffU << STATUS_REG_SHIFT_STICKY;
  space[STATUS_REG];
}

void Configure(weak_ptr<ADMXRC2_HANDLE> handle,
               const string& filename,
               fpga_space_t space) {
  auto status = ADMXRC2_ConfigureFromFile(*handle.lock(), filename.c_str());
  if (status != ADMXRC2_SUCCESS) {
    throw runtime_error(
        string("failed to configure: ") + ADMXRC2_GetStatusString(status));
  }
}

shared_ptr<ADMXRC2_HANDLE> GetCardHandle() {
  ADMXRC2_HANDLE handle;
  auto status = ADMXRC2_OpenCard(0, &handle);

  if (status == ADMXRC2_SUCCESS) {
    return shared_ptr<ADMXRC2_HANDLE>(
        new ADMXRC2_HANDLE(handle),
        [](ADMXRC2_HANDLE* h) {
          if (*h != ADMXRC2_HANDLE_INVALID_VALUE) { ADMXRC2_CloseCard(*h); }
          delete h;
          h = nullptr;
        });
  } else {
    throw runtime_error(
        string("failed to open card: ") + ADMXRC2_GetStatusString(status));
  }
}

ADMXRC2_CARD_INFO GetCardInfo(weak_ptr<ADMXRC2_HANDLE> handle) {
  ADMXRC2_CARD_INFO info;
  auto status = ADMXRC2_GetCardInfo(*handle.lock(), &info);

  if (status == ADMXRC2_SUCCESS) {
    return info;
  } else {
    throw runtime_error(
        string("failed to get card info: ") + ADMXRC2_GetStatusString(status));
  }
}

BankInfo GetBankInfo(std::weak_ptr<ADMXRC2_HANDLE> handle,
                     const ADMXRC2_CARD_INFO& info) {
  BankInfo bank_info;

  for (int i = 0; i < info.NumRAMBank && i < MAX_BANK; ++i) {
    if (info.RAMBanksFitted & (0x1UL << i)) {
      auto status = ADMXRC2_GetBankInfo(*handle.lock(), i, &bank_info[i]);
      if (status != ADMXRC2_SUCCESS) {
        throw runtime_error(
            string("failed to get bank info: ") + ADMXRC2_GetStatusString(status));
      }
    }
  }

  return std::move(bank_info);
}

fpga_space_t GetFPGASpace(weak_ptr<ADMXRC2_HANDLE> handle) {
  ADMXRC2_SPACE_INFO info;
  auto status = ADMXRC2_GetSpaceInfo(*handle.lock(), 0, &info);

  if (status != ADMXRC2_SUCCESS || info.VirtualBase == NULL) {
    throw runtime_error(
        string("failed to get space info: ") + ADMXRC2_GetStatusString(status));
  } else {
    return static_cast<volatile uint32_t*>(info.VirtualBase);
  }
}

void Read(ADMXRC2_HANDLE handle,
          fpga_space_t space,
          ADMXRC2_DMADESC dma_descriptor,
          uint8_t* read_buffer,
          uint32_t dma_mode,
          void* buffer,
          uint64_t offset,
          unsigned long length) {
  uint8_t* dst = (uint8_t*)buffer;

  while (length > 0) {
    unsigned long pgidx = static_cast<unsigned long>(offset >> PAGE_SHIFT);
    unsigned long pgoffs = (unsigned long)offset & (PAGE_SIZE - 1);
    unsigned long chunk = (PAGE_SIZE - pgoffs > length)?
        length: (PAGE_SIZE - pgoffs);

    /* Set the page register */
    Write(space, PAGE_REG, pgidx & PAGE_REG_PAGEMASK);

    auto status = ADMXRC2_DoDMA(handle,
                                dma_descriptor,
                                offset,
                                chunk,
                                MEMORY_WINDOW_ADDRESS + pgoffs,
                                ADMXRC2_LOCALTOPCI,
                                ADMXRC2_DMACHAN_ANY,
                                dma_mode,
                                0, NULL, NULL);
    if (status == ADMXRC2_SUCCESS) {
      memcpy(dst, read_buffer, chunk);

      dst += chunk;
      offset += chunk;
      length -= chunk;
    } else {
      throw runtime_error(
          string("failed to get data") + ADMXRC2_GetStatusString(status));
    }
  }
}

void ResetMemorySystem(fpga_space_t space) {
  Write(space, MEMCTL_REG, 0x1U);
  sleep_for(milliseconds(1));
  Write(space, MEMCTL_REG, 0x0U);
  sleep_for(milliseconds(500));
}

void SelectBank(fpga_space_t space, uint32_t bank)
  { Write(space, BANK_REG, (uint32_t)bank & 0xfU); }

bool SetClockRates(
    weak_ptr<ADMXRC2_HANDLE> handle,
    double local_clock,
    double memory_clock) {
  double actual_memory_clock_frequency, actual_locak_clock_frequency;

  return ADMXRC2_SetClockRate(*handle.lock(),
                              ADMXRC2_CLOCK_LCLK,
                              local_clock * 1.0e6,
                              &actual_locak_clock_frequency) == ADMXRC2_SUCCESS &&
      ADMXRC2_SetClockRate(*handle.lock(),
                           1,
                           memory_clock * 1.0e6,
                           &actual_memory_clock_frequency) == ADMXRC2_SUCCESS;
}

void SetMemoryPortConfiguration(fpga_space_t space) {
  for (int i = 0; i < MAX_BANK; ++i)
    { Write(space, MODEx_REG(i), MODEx_REG_ZBTSSRAM_PIPELINE); }
}

shared_ptr<ADMXRC2_DMADESC> SetupDMA(shared_ptr<ADMXRC2_HANDLE> handle,
                                     uint8_t* buffer,
                                     size_t buffer_size) {
  ADMXRC2_DMADESC descriptor;
  auto status = ADMXRC2_SetupDMA(*handle, buffer, buffer_size, 0, &descriptor);

  if (status == ADMXRC2_SUCCESS) {
    return std::shared_ptr<ADMXRC2_DMADESC>(
        new ADMXRC2_DMADESC(descriptor),
        [handle](ADMXRC2_DMADESC* desc){
          if (handle && *handle != ADMXRC2_HANDLE_INVALID_VALUE) {
            ADMXRC2_UnsetupDMA(*handle, *desc);
            delete desc;
            desc = nullptr;
          }
        });
  } else {
    throw runtime_error(
        string("failed to open DMA channels: ") +
          ADMXRC2_GetStatusString(status));
  }
}

void WaitForLclkDcm(fpga_space_t space) {
  sleep_for(milliseconds(500));

  uint32_t status = space[STATUS_REG];
  if (status & STATUS_REG_LCLKLOCKED) {
    Write(space, STATUS_REG, STATUS_REG_LCLKSTICKY);
  } else { throw runtime_error("LCLK DCM is not locked"); }
}

void Write(ADMXRC2_HANDLE handle,
           fpga_space_t space,
           ADMXRC2_DMADESC dma_descriptor,
           uint8_t* write_buffer,
           uint32_t dma_mode,
           void* buffer,
           uint64_t offset,
           unsigned long length) {
  memcpy(write_buffer, static_cast<uint8_t*>(buffer), length);

  while (length > 0) {
    unsigned long pgidx = static_cast<unsigned long>(offset >> PAGE_SHIFT);
    unsigned long pgoffs = (unsigned long)offset & (PAGE_SIZE - 1);
    unsigned long chunk = (PAGE_SIZE - pgoffs > length)?
        length: (PAGE_SIZE - pgoffs);

    /* Set the page register */
    Write(space, PAGE_REG, pgidx & PAGE_REG_PAGEMASK);

    auto status = ADMXRC2_DoDMA(
        handle,
        dma_descriptor,
        offset,
        chunk,
        MEMORY_WINDOW_ADDRESS + pgoffs,
        ADMXRC2_PCITOLOCAL,
        ADMXRC2_DMACHAN_ANY,
        dma_mode,
        0, NULL, NULL);
    if (status == ADMXRC2_SUCCESS) {
      offset += chunk;
      length -= chunk;
    } else {
      throw runtime_error(
          string("failed to send data") + ADMXRC2_GetStatusString(status));
    }
  }
}
}  // namespace fpga_communicator 
}  // namespace filter_core

