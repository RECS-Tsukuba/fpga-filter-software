/*
 * Copyright (c) 2014 University of Tsukuba
 * Reconfigurable computing systems laboratory
 *
 * Licensed under GPLv3 (http://www.gnu.org/copyleft/gpl.html)
 */
#ifndef FILTER_CORE_CAMERA_H_
#define FILTER_CORE_CAMERA_H_

#include <boost/iterator/iterator_facade.hpp>
#include <opencv2/opencv.hpp>
#include <iterator>
#include <memory>


namespace filter_core {
namespace camera_detail {
class FrameIterator;
}  // namespace camera_detail
}  // namespace filter_core


namespace filter_core {
/*!
 * \class Converter
 * \brief コンバータのためのインターフェース
 */
class Converter {
 public:
  ~Converter() {}
 public:
  virtual cv::Mat convert(cv::Mat src) = 0;
};
/*!
 * \class Grayscaler
 * \brief グレースケール変換
 */
class Grayscaler : public Converter {
 private:
  const cv::Size size_;
  const int interpolation_;
  cv::Mat output_;
  cv::Mat color_converted_;
 public:
  Grayscaler(cv::Size size = {800, 600},
             int interpolation = cv::INTER_LINEAR)
    : size_(size),
      interpolation_(interpolation),
      output_(size, CV_8UC1),
      color_converted_() {}
 public:
  cv::Mat convert(cv::Mat src);
};
/*!
 * \class Resize
 * \brief リサイズ
 */
class Resizer : public Converter {
 private:
  const cv::Size size_;
  const int interpolation_;
  cv::Mat output_;
 public:
  Resizer(cv::Size size = {800, 600},
          int interpolation = cv::INTER_LINEAR)
    : size_(size),
      interpolation_(interpolation),
      output_(size, CV_8UC1) {}
 public:
  cv::Mat convert(cv::Mat src);
};
/*!
 * \class Camera
 * \brief カメラ画像を取得するキャプチャクラス
 */
class Camera {
 private:
  using iterator_type = filter_core::camera_detail::FrameIterator;
 private:
  cv::VideoCapture capture_;
  cv::Mat frame_;
  std::unique_ptr<Converter> converter_;

 public:
  Camera(std::unique_ptr<Converter>&& converter)
    : capture_(0),
      frame_(),
      converter_(std::move(converter)) {}
 public:
  /*!
   * \brief キャプチャ可能である場合、真を返す
   * \returns キャプチャ可能である場合、真
   */
  bool isReady() const { return capture_.isOpened(); }
 public:
  iterator_type begin();
  iterator_type end();
  cv::Mat get();
};
}  // namespace filter_core


namespace filter_core {
namespace camera_detail {
/*!
 * \class FrameIterator
 * \brief フレームイテレータ
 *
 * デレファレンスされるたびにカメラから画像を取得する.range-based
 * for文と組み合わせるために使用.
 */
class FrameIterator :
  public boost::iterator_facade<FrameIterator,
                                const cv::Mat,
                                std::forward_iterator_tag,
                                const cv::Mat> {
 private:
  Camera& camera_;
 public:
  FrameIterator(Camera& c) : camera_(c) {}
 private:
  friend class boost::iterator_core_access;

 private:
  bool equal(const FrameIterator& other) const
    { return !camera_.isReady(); }
  void increment() { /* Do nothing */ }
  const cv::Mat dereference() const { return camera_.get(); }
};
}  // namespace camera_detail
}  // namespace filter_core


namespace filter_core {

std::unique_ptr<filter_core::Converter> MakeConveter(
    bool is_colored, cv::Size size, int interpolation);
}  // namespace filter_core

#endif

