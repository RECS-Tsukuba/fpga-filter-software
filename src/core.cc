#include <admxrc2.h>
#include <boost/optional.hpp>

#include "filter_core/fpga_communicator.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>


using std::exception;
using std::exit;
using std::string;
using filter_core::FPGACommunicator;


int main(void) {
  try {
    const double lclkFreq = 50.0;
    const double mclkFreq = 66.67;
    const string bitstream_filename = "memory-xrc2-2v6000.bit";

    std::cout << "configuring..." << std::flush;
    auto communicator = FPGACommunicator(
        lclkFreq,
        mclkFreq,
        bitstream_filename);
    std::cout << "done" << std::endl;

    uint32_t src[32];
    std::iota(std::begin(src), std::end(src), 0);
    for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(src[i]) << " " ; }
    std::cout << std::endl << std::endl;

    communicator.write(src, 0, 32 * 4, 0);

    uint32_t dst[32];
    communicator.read(dst, 0, 32 * 4, 0);
    for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(dst[i]) << " " ; }
    std::cout << std::endl;

    exit(EXIT_SUCCESS);
  } catch(exception& e) {
    std::cerr << std::endl << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  return 0;
}

