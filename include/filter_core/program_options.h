#ifndef FILTER_CORE_PROGRAM_OPTIONS_H_
#define FILTER_CORE_PROGRAM_OPTIONS_H_

#include <boost/optional.hpp>
#include <opencv2/opencv.hpp>
#include <string>


namespace filter_core {

class Options {
 public:
  const std::string filename;
  const double frequency;
  const cv::Size image_size;
  const bool colored;
  const int interpolation;
  const bool is_with_captured;
 public:
  Options(const std::string& filename,
          double frequency,
          cv::Size image_size,
          bool colored,
          int interpolation,
          bool is_with_captured)
    : filename(filename),
      frequency(frequency),
      image_size(image_size),
      colored(colored),
      interpolation(interpolation),
      is_with_captured(is_with_captured) {}
};
}  // namespace filter_core

namespace filter_core {

boost::optional<filter_core::Options> GetOptions(int argc, char** argv) noexcept;
}  // namespace filter_core
#endif

