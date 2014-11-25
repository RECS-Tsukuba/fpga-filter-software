/*
 * Copyright (c) 2014 University of Tsukuba
 * Reconfigurable computing systems laboratory
 *
 * Licensed under GPLv3 (http://www.gnu.org/copyleft/gpl.html)
 */
#ifndef FILTER_CORE_PROGRAM_OPTIONS_H_
#define FILTER_CORE_PROGRAM_OPTIONS_H_

#include <boost/optional.hpp>
#include <opencv2/opencv.hpp>
#include <string>


namespace filter_core {
/*!
 * \var MINIMUN_FREQUENCY
 * 最小のFPGA動作周波数.memtestサンプルを参照すること.
 */
constexpr double MINIMUN_FREQUENCY = 24.0;
/*!
 * \var MAXIMUM_FREQUENCY
 * 最大のFPGA動作周波数.memtestサンプルを参照すること.
 */
constexpr double MAXIMUM_FREQUENCY = 66.0;
}  // namespace filter_core


namespace filter_core {

class ImageOptions {
 public:
  const cv::Size size;
  const int step;
  const int type;
  const int conversion;
  const int interpolation;

  const uint32_t total_size;
  const uint32_t width;
  const cv::Size combined_image_size;

 ImageOptions(cv::Size size, int type, int conversion, int interpolation,
              int step)
   : size(size), step(step), type(type), conversion(conversion),
     interpolation(interpolation),
     total_size(size.area()), width(size.width),
     combined_image_size(size.width * 2, size.height) {}
};

/*!
 * \class Options
 * \brief プログラム引数の解析結果
 */
class Options {
 public:
  const std::string filename;
  const std::string output_directory;
  const double frequency;
  const filter_core::ImageOptions image_options;
  const bool is_with_captured;
  const bool is_debug_mode;
 public:
  Options(const std::string& filename,
          const std::string& output_directory,
          double frequency,
          filter_core::ImageOptions&& image_options,
          bool is_with_captured,
          bool is_debug_mode)
    : filename(filename),
      output_directory(output_directory),
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

