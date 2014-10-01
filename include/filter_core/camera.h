#ifndef FILTER_CORE_CAMERA_H_
#define FILTER_CORE_CAMERA_H_

#include <iterator>
#include <boost/iterator/iterator_facade.hpp>
#include <opencv2/opencv.hpp>


namespace filter_core {
namespace camera_detail {
class FrameIterator;
}  // namespace camera_detail
}  // namespace filter_core


namespace filter_core {

/*!
 * \class Camera
 * \brief カメラ画像を取得するキャプチャクラス
 */
class Camera {
 private:
  using iterator_type = filter_core::camera_detail::FrameIterator;
 private:
  const cv::Size size_;
  const int interpolation_;
  cv::VideoCapture capture_;

  cv::Mat frame_;
  cv::Mat gray_scaled_;
  cv::Mat src_;

 public:
  Camera (cv::Size size = {800, 600}, int interpolation = cv::INTER_LINEAR)
    : size_(size),
      interpolation_(interpolation),
      capture_(0),
      frame_(),
      gray_scaled_(size, CV_8UC1),
      src_(size, CV_8UC1) {}
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

#endif

