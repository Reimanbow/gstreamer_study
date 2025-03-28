#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

int tutorial_main(int argc, char *argv[]) {
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;

  /**
   * 最初のGStreamerコマンドとなる必要がある
   * - すべての内部構造を初期化する
   * - 利用可能なプラグインをチェックする
   * - GStremaer用のコマンドラインオプションを実行する
   */
  gst_init(&argc, &argv);

  /**
   * パイプラインの作成
   * - gst_parse_launch()は, GStreamerのパイプラインを簡単に作成できる関数
   * - playbinはGStreamerの便利なエレメント(要素)で, オーディオ・ビデオ再生を簡単に実装できる
   * - uriに動画のURLを指定すると, GStreamerが自動的に適切なデコーダを選び, 再生を行う
   */
  pipeline =
      gst_parse_launch(
        "playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
        NULL
      );

  /**
   * 再生開始
   * 作成したパイプラインをGST_STATE_PLAYINGに設定することで, 動画の再生が開始される
   */
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  /**
   * イベントの待機(エラーやEOS)
   * GStreamerのバスを使って, エラーや終了(EOS)メッセージを待機する
   * - GStreamerのバスは, パイプライン内の要素が発生させるメッセージを管理する仕組み
   * - gst_bus_timed_pop_filtered()は, エラー(GST_MESSAGE_ERROR)またはEOS(GST_MESSAGE_EOS)を受け取るまでブロックする関数
   */
  bus = gst_element_get_bus(pipeline);
  msg =
      gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  /**
   * エラー処理
   * 標準エラー出力にエラーメッセージを表示する
   * GST_DEBUG=*:WARNを設定してプログラムを実行すると, 詳細なデバッグ情報を表示できる
   */
  if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
    g_printerr("An error occurred! Re-run with the GST_DEBUG=*:WARN "
        "environment variable set for more details.\n");
  }

  /**
   * リソースの解放
   * メモリリークを防ぐため, 使用したオブジェクトを解放している
   * - gst_message_unref(msg): 取得したメッセージを解放
   * - gst_object_unref(bus): バスを解放
   * - gst_element_set_state(pipeline, GST_STATE_NULL): パイプラインをNULLにしてリソースを解放
   * - gst_object_unref(pipeline): パイプライン自体も解放
   */
  gst_message_unref(msg);
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  return 0;
}

// main関数
int main(int argc, char *argv[]) {
#if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
  return gst_macos_main((GstMainFunc) tutorial_main, argc, argv, NULL);
#else
  return tutorial_main(argc, argv);
#endif
}