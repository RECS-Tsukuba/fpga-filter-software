#ifndef FILTER_CORE_CAMERA_H_
#define FILTER_CORE_CAMERA_H_

#include <opencv2/opencv.hpp>

namespace filter_core {

class GrayscaledCamera {
 private:
  cv::Size size_;
  cv::VideoCapture capture_;

  cv::Mat frame_;
  cv::Mat gray_scaled_;
  cv::Mat src_;

 public:
  GrayscaledCamera(cv::Size size = {800, 600})
    : size_(size),
      capture_(0),
      frame_(),
      gray_scaled_(size, CV_8UC1),
      src_(size, CV_8UC1) {}
 public:
  cv::Mat get();
  void show(cv::Mat m);
};
}  // namespace filter_core

#endif

