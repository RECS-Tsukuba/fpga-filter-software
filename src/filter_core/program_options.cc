/*
 * Copyright (c) 2014 University of Tsukuba
 * Reconfigurable computing systems laboratory
 *
 * Licensed under GPLv3 (http://www.gnu.org/copyleft/gpl.html)
 */
#include "filter_core/program_options.h"

#include <boost/program_options.hpp>
#include <opencv2/opencv.hpp>
#include <functional>
#include <limits>
#include <string>


#define nullopt boost::none;


using std::move;
using std::string;
using boost::optional;
using boost::program_options::notify;
using boost::program_options::options_description;
using boost::program_options::parse_command_line;
using boost::program_options::store;
using boost::program_options::value;
using boost::program_options::variables_map;
using cv::Size;


namespace filter_core {
namespace program_options_detail {

boost::program_options::options_description GetDescription();
filter_core::ImageOptions GetImageOptions(
    const boost::program_options::variables_map& vm);
cv::Size GetImageSize(const std::string& size) noexcept;
int GetInterpolation(const std::string& i) noexcept;
boost::program_options::variables_map GetVariablesMap(int argc, char** argv);
void ShowHelp();
}  // namespace program_options_detail
}  // namespace filter_core


namespace filter_core {
namespace program_options_detail {

options_description GetDescription() {
  options_description description;
  description.add_options()
    ("help,h", "show this")
    ("filename,i", value<string>(), "bit filename")
    ("output-directory", value<string>()->default_value("."),
     "directory for image output")
    ("show-source", "show with a captured frame")
    ("frequency", value<double>()->default_value(40.0),
     "set circuit operating frequency")
    ("image-size", value<string>()->default_value(string("middle")),
     "set image size")
    ("interpolation", value<string>()->default_value(string("linear")),
     "set interpolation")
    ("colored", "colored image")
    ("debug", "show debug info");

  return move(description);
}

Size GetImageSize(const string& size) noexcept {
  if (size == "small")
    { return {320, 240}; }
  else if (size == "middle")
    { return {640, 480}; }
  else if (size == "large")
    { return {800, 600}; }
  else { return {640, 480}; }
}

int GetInterpolation(const string& i) noexcept {
  if (i == "nearest" ) { return cv::INTER_NEAREST; }
  else if (i == "linear") { return cv::INTER_LINEAR; }
  else { return cv::INTER_LINEAR; }
}

ImageOptions GetImageOptions(const variables_map& vm) {
  int is_colored = vm.count("colored") > 0;

  return ImageOptions(
      GetImageSize(vm["image-size"].as<string>()),
      (is_colored)? CV_8UC3 : CV_8UC1,
      GetInterpolation(vm["interpolation"].as<string>()),
      (is_colored)? 3 : 1);
}

variables_map GetVariablesMap(int argc, char** argv) {
  variables_map vm;
  store(parse_command_line(argc, argv, GetDescription()), vm);
  notify(vm);

  return move(vm);
}

void ShowHelp() {
  std::cout << "core -i <filename> [OPTION]..." << std::endl;
  std::cout << GetDescription() << std::endl;
}
}  // namespace program_options_detail

/*!
 * \brief プログラム引数を解析する
 * \param argc argc
 * \param argv argv
 * \return
 * プログラム引数が有効である場合、プログラム引数を格納したオブジェクト.
 * 有効でない場合は無効値.
 */
optional<Options> GetOptions(int argc, char** argv) noexcept {
  namespace detail = filter_core::program_options_detail;

  try {
    auto vm = detail::GetVariablesMap(argc, argv);

    if (vm.count("help") > 0 || vm.count("filename") == 0) {
      detail::ShowHelp();
      return nullopt;
    } else if(
        vm["frequency"].as<double>() > MAXIMUM_FREQUENCY ||
        vm["frequency"].as<double>() < MINIMUN_FREQUENCY) {
      std::cerr << "the frequency was out of ragne" << std::endl;
      return nullopt;
    } else {
      return Options(vm["filename"].as<string>(),
                     vm["output-directory"].as<string>(),
                     vm["frequency"].as<double>(),
                     vm.count("colored") > 0,
                     detail::GetImageOptions(vm),
                     vm.count("show-source") > 0,
                     vm.count("debug") > 0);
    }
  } catch (std::exception& e) {
    std::cerr << "invalid program options: " << e.what() << std::endl;
    return nullopt;
  } catch(...) {
    std::cerr << "invalid program options" << std::endl;
    return nullopt;
  }
}
/*!
 * \brief プログラム引数を表示する
 * \param options プログラム引数の解析結果
 */
void ShowOptions(const Options& options) {
  std::cout <<
    "filename: " << options.filename << std::endl <<
    "frequency: " << options.frequency << std::endl <<
    "size: " << options.image_options.size.height << "x" <<
      options.image_options.size.width <<
    std::endl;
}
}  // namespace filter_core

#undef nullopt
