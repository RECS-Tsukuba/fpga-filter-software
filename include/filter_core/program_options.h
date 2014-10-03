#ifndef FILTER_CORE_PROGRAM_OPTIONS_H_
#define FILTER_CORE_PROGRAM_OPTIONS_H_

#include <boost/optional.hpp>
#include <opencv2/opencv.hpp>
#include <string>


namespace filter_core {

class ImageOptions {
 public:
  const cv::Size size;
  const int type;
  const int conversion;
  const int interpolation;

  const uint32_t total_size;
  const uint32_t width;
  const cv::Size combined_image_size;

 ImageOptions(cv::Size size, int type, int conversion, int interpolation,
              int step)
   : size(size), type(type), conversion(conversion),
     interpolation(interpolation),
     total_size(size.area() * step), width(size.width),
     combined_image_size(size.width * 2, size.height) {}
};

/*
 * \class Options
 * \brief プログラム引数の解析結果
 */
class Options {
 public:
  const std::string filename;
  const double frequency;
  const filter_core::ImageOptions image_options;
  const bool is_with_captured;
  const bool is_debug_mode;
 public:
  Options(const std::string& filename,
          double frequency,
          filter_core::ImageOptions&& image_options,
          bool is_with_captured,
          bool is_debug_mode)
    : filename(filename),
      frequency(frequency),
      image_options(image_options),
      is_with_captured(is_with_captured),
      is_debug_mode(is_debug_mode) {}
};
}  // namespace filter_core

namespace filter_core {

boost::optional<filter_core::Options> GetOptions(int argc, char** argv) noexcept;
void ShowOptions(const filter_core::Options& options);
}  // namespace filter_core
#endif

