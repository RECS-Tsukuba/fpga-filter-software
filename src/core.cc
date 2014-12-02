/*
 * Copyright (c) 2014 University of Tsukuba
 * Reconfigurable computing systems laboratory
 *
 * Licensed under GPLv3 (http://www.gnu.org/copyleft/gpl.html)
 */
#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"
#include "filter_core/framerate_checker.h"
#include "filter_core/program_options.h"

#include <admxrc2.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>


using std::bind;
using std::cref;
using std::string;
using std::to_string;
using std::vector;
using std::chrono::microseconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using cv::Rect;
using cv::Mat;
using cv::setMouseCallback;


namespace filter_core {

using filter_t =
  std::function<void (filter_core::FPGACommunicator&, cv::Mat, cv::Mat)>;
}  // namespace filter_core


namespace filter_core {
/*!
 * @var frame_title 
 * \brief 出力ウィンドウタイトル
 */
constexpr const char* const frame_title = "filter";
}  // namespace filter_core


namespace filter_core {
void test(filter_core::FPGACommunicator& com, cv::Size size, int inter);
}  // namespace filter_core


namespace filter_core {
/*!
 * \brief 画像を結合する
 * \param dst 出力先
 * \param filtered フィルタ画像
 * \param original 元画像
 * \param width 画像幅
 * \param height 画像高さ
 * \return 出力
 */
cv::Mat Combine(cv::Mat dst, cv::Mat filtered, cv::Mat original,
                cv::Size size) {
  filtered.copyTo(Mat(dst, Rect(0, 0, size.width, size.height)));
  original.copyTo(Mat(dst, Rect(size.width, 0, size.width, size.height)));

  return dst;
}
/*!
 * \brief ハードウェアを用いて空間フィルタをかける.
 * \param com FPGAボードとのコミュニケータ
 * \param src 入力画像
 * \param dst 出力画像
 * \param total_size 入力画像サイズ.8ビット画素数=バイト数を指定
 * \param wait_limit finish信号を待つ最大回数.時間にして(250 x wait_limit)ms
 */
void Filter(filter_core::FPGACommunicator& com,
            cv::Mat src, cv::Mat dst,
            const ImageOptions& options,
            int wait_limit) {
  // 画像を送信
  com.write(src.data, 0, options.total_size, 0);
  // refresh信号を送り、enable信号を有効にする
  SendRefresh(com);
  com.write(ENABLE_REG, 1);
  // フィルタリング完了を待つ
  for (int i = 0; i < wait_limit && com[FINISH_REG] == 0; ++i)
    { sleep_for(microseconds(250)); }
  // enableを無効にする
  com.write(ENABLE_REG, 0);
  // 画像を取得
  com.read(dst.data, 0, options.total_size, 1);
}

void FilterColored(filter_core::FPGACommunicator& com,
                   cv::Mat src, cv::Mat dst,
                   const ImageOptions& options,
                   int wait_limit,
                   int channel) {
  vector<Mat> splitted(channel);
  cv::split(src, splitted);

  vector<Mat> filtered(channel);
  for (auto& dst : filtered)
    { dst = Mat(options.size, CV_8UC1); }

  for (uint32_t i = 0; i < channel; ++i) {
    Filter(com, splitted[i], filtered[i], options, wait_limit);
  }

  cv::merge(filtered, dst);

  dst = src;
}
/*!
 * \brief 画像をファイルに出力する.
 * \param output 出力画像
 * \param dir 出力先ディレクトリ名
 */
void OutputImage(cv::Mat output, const char* const dir) {
  // ファイル名を生成
  // TODO: gcc 4.9ではstd::put_timeが使えないため、Cの関数を使用
  char output_filename[1024] = "";

  std::time_t timestamp = system_clock::to_time_t(system_clock::now());
  std::strncat(std::strncpy(output_filename, dir, strlen(dir)), "/", 1);
  std::strftime(output_filename + strlen(dir) + 1,
                1024 - strlen(dir) - 1, "%F-%T",
                std::localtime(&timestamp)),
  std::strncat(output_filename, ".png", 4);

  cv::imwrite(output_filename, output);
}
/*!
 * \brief マウスイベントをセット.
 * \param x x座標
 * \param y y座標
 * \param event マウスイベントハンドラ
 */
void SetMouseEvent(int x, int y, MouseEvent* event)
  { event->set(x, y); }
/*!
 * \brief マウスイベントをハンドルする.
 * \param event マウスイベント
 * \param x x座標
 * \param y y座標
 * \param flags メタフラグ
 * \param userdata マウスイベントハンドラ
 */
void HandleMouseEvent(int event, int x, int y, int flags, void* userdata) {
  if (event == cv::EVENT_LBUTTONUP)
    { SetMouseEvent(x, y, static_cast<MouseEvent*>(userdata)); }
}
/*!
 * \brief mainの実装.
 * \param options プログラム引数の解析結果
 * \return 常にEXIT_SUCCESS
 */
int MainImpl(filter_core::Options&& options) {
  std::locale::global(std::locale("ja_JP.utf8"));

  if (options.is_debug_mode) { ShowOptions(options); }

  const auto& image_options = options.image_options;

  cv::Mat dst(image_options.size, image_options.type);
  cv::Mat combined(image_options.combined_image_size, image_options.type);
  // マウス座標
  std::atomic<uint32_t> mouse_x(0);
  std::atomic<uint32_t> mouse_y(0);

  auto start = system_clock::now();

  filter_t filter = (options.is_colored)?
    static_cast<filter_t>(
        bind(FilterColored,
             _1, _2, _3, cref(image_options), 1000, image_options.step)):
    static_cast<filter_t>(
        bind(Filter, _1, _2, _3, cref(image_options), 1000));

  FPGACommunicator communicator(options.frequency,
                                options.filename,
                                image_options.total_size);

//      filter_core::test(communicator, image_size, options->interpolation);

  // 画像サイズを指定
  SendImageSize(communicator,
                image_options.total_size, image_options.width);

  for (auto src :
       Camera(
           MakeConveter(
               options.is_colored,
               image_options.size, image_options.interpolation))) {
    // マウスイベントを追加.
    MouseEvent mouse_event(communicator, mouse_x, mouse_y, image_options.size);
    setMouseCallback(frame_title, &HandleMouseEvent, &mouse_event);

    if (options.is_debug_mode) {
      filter(
          // ユーザレジスタを表示
          OutputUserRegisters(communicator), src, dst);
    } else {
      // フレームレート計測
      FramerateChecker framerate_checker(start);

      filter(communicator, src, dst);
    }

    // 出力
    cv::Mat output = (options.is_with_captured)?
      Combine(combined, dst, src, image_options.size) : dst;
    cv::imshow(frame_title, output);

    auto key = cv::waitKey(30);
    if (key == 'p' || key == 'P') {
      OutputImage(output, options.output_directory.c_str());
    } else if (key == 'd' || key == 'D') {
      std::cout << "\r";
      OutputUserRegisters(communicator);
      std::cout << std::endl;
    } else if (key >= 0) {
      break;
    }
  }
  std::cout << std::endl;

  return EXIT_SUCCESS;
}
}  // namespace filter_core


int main(int argc, char** argv) {
  try {
    if (auto options = filter_core::GetOptions(argc, argv))
      { std::exit(filter_core::MainImpl(std::move(*options))); }
    else { std::exit(EXIT_FAILURE); }
  } catch(std::exception& e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  } catch(...) {
    std::cerr << "some exceptions were thrown" << std::endl;
    exit(EXIT_FAILURE);
  }
  return 0;
}

