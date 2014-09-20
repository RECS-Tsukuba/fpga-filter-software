#include <admxrc2.h>
#include <boost/optional.hpp>

#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>


using std::exception;
using std::exit;
using std::string;
using filter_core::FPGACommunicator;
using filter_core::GrayscaledCamera;


void test(FPGACommunicator& communicator) {
  uint32_t src[32];
  std::iota(std::begin(src), std::end(src), 0);
  for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(src[i]) << " " ; }
  std::cout << std::endl << std::endl;

  communicator.write(src, 0, 32 * 4, 0);

  uint32_t dst[32];
  communicator.read(dst, 0, 32 * 4, 0);
  for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(dst[i]) << " " ; }
  std::cout << std::endl;
}

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

    test(communicator);

    GrayscaledCamera camera;
    cv::Mat dst({800, 600}, CV_8UC1);

    for (;;) {
      auto src = camera.get();

      communicator.write(src.data, 0, 800 * 600, 0);
      communicator.write(src.data, 0, 800 * 600, 1);

      communicator.read(dst.data, 0, 800 * 600, 1);

      camera.show(dst);
      if(cv::waitKey(30) >= 0) break;
    }

    exit(EXIT_SUCCESS);
  } catch(exception& e) {
    std::cerr << std::endl << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  return 0;
}

