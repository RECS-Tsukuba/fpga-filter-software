/*
 * Copyright (c) 2014 University of Tsukuba
 * Reconfigurable computing systems laboratory
 *
 * Licensed under GPLv3 (http://www.gnu.org/copyleft/gpl.html)
 */
#ifndef FILTER_CORE_FRAMERATE_CHECKER_H_
#define FILTER_CORE_FRAMERATE_CHECKER_H_

#include <chrono>


namespace filter_core {
/*!
 * \class FramerateChecker
 * \brief フレームレートの計測クラス
 */
class FramerateChecker {
 public:
  using duration_type = std::chrono::microseconds;
  using time_point_type = std::chrono::time_point<std::chrono::system_clock>;
 private:
  time_point_type& start_;
 public:
  /*!
   * \brief コンストラクタ
   * \param s 共有される現在時間の変数
   */
  FramerateChecker(time_point_type& s) : start_(s) {}
  /*!
   * \brief デストラクタ
   * このオブジェクトが破棄されるときにFPSを出力し、共有変数に格納された現在時
   * 刻を更新する.
   */
  ~FramerateChecker() {
    auto e = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<duration_type>(e - start_);
    std::cout << "\r" << (duration_type::period::den / diff.count()) <<
      "fps     " << std::flush;

    start_ = e;
  }
 private:
  FramerateChecker(const FramerateChecker&) = delete;
  FramerateChecker(FramerateChecker&&) = delete;
  FramerateChecker& operator=(const FramerateChecker&) = delete;
  FramerateChecker& operator=(FramerateChecker&&) = delete;
};
}  // namespace filter_core

#endif  // FILTER_CORE_FRAMERATE_CHECKER_H_

