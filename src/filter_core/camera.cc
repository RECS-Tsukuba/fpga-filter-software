#include "filter_core/camera.h"

#include <opencv2/opencv.hpp>
#include <stdexcept>


using std::runtime_error;
using cv::cvtColor;
using cv::imshow;
using cv::Mat;
using cv::resize;
using filter_core::camera_detail::FrameIterator;


namespace filter_core {

Mat Converter::convert(Mat src) {
  cvtColor(src, color_converted_, conversion_);
  resize(color_converted_, output_, size_, interpolation_);

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
  if (capture_.read(frame_)) { return converter_.convert(frame_); }
  else { throw std::runtime_error("failed to read a frame"); }
}
}  // namespace filter_core

