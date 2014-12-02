/*
 * Copyright (c) 2014 University of Tsukuba
 * Reconfigurable computing systems laboratory
 *
 * Licensed under GPLv3 (http://www.gnu.org/copyleft/gpl.html)
 */
#include "filter_core/camera.h"

#include <opencv2/opencv.hpp>
#include <memory>
#include <stdexcept>
#include "filter_core/program_options.h"


using std::unique_ptr;
using std::runtime_error;
using cv::cvtColor;
using cv::imshow;
using cv::Mat;
using cv::resize;
using cv::Size;
using filter_core::camera_detail::FrameIterator;


namespace filter_core {
/*!
 * \brief 画像をコンバート.
 * \param src 入力画像.
 * \return コンバートされた画像.
 */
Mat Grayscaler::convert(Mat src) {
  cvtColor(src, color_converted_, CV_BGR2GRAY);
  resize(color_converted_, output_, size_, interpolation_);

  return output_;
}
/*!
 * \brief 画像をリサイズする
 * \param src 入力画像.
 * \return リサイズされた画像.
 */
Mat Resizer::convert(Mat src) {
  resize(src, output_, size_, interpolation_);

  return output_;
}
}  // namespace filter_core


namespace filter_core {
/*!
 * \brief フレームイテレータを返す
 * \return フレームイテレータ
 */
FrameIterator Camera::begin() { return FrameIterator(*this); }
/*!
 * \brief フレームイテレータを返す
 * \return フレームイテレータ
 */
FrameIterator Camera::end() { return FrameIterator(*this); }
/*!
 * \brief キャプチャ画像を取得する
 * \return キャプチャ画像
 */
Mat Camera::get() {
  if (capture_.read(frame_)) { return converter_->convert(frame_); }
  else { throw std::runtime_error("failed to read a frame"); }
}
}  // namespace filter_core


namespace filter_core {
/*!
 * \brief コンバータを生成
 * Cameraクラスのための、コンバータを生成する。
 * \return コンバータ
 */
unique_ptr<Converter> MakeConveter(
    bool is_colored, Size size, int interpolation) {
  if (is_colored) {
    return unique_ptr<Converter>(new Resizer(size, interpolation));
  } else {
    return unique_ptr<Converter>(new Grayscaler(size, interpolation));
  }
}
}  // namespace filter_core

