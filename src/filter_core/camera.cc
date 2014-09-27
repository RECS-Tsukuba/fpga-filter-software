#include "filter_core/camera.h"

#include <opencv2/opencv.hpp>
#include <stdexcept>


using std::runtime_error;
using cv::cvtColor;
using cv::imshow;
using cv::Mat;
using cv::resize;


namespace filter_core {

cv::Mat GrayscaledCamera::get() {
  if (capture_.read(frame_)) {
    cvtColor(frame_, gray_scaled_, CV_BGR2GRAY);
    resize(gray_scaled_, src_, size_, interpolation_);

    return src_;
  } else {
    throw std::runtime_error("failed to read a frame");
  }
}
}  // namespace filter_core

