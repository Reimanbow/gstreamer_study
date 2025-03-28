#include <gst/gst.h>

int tutorial_main(int argc, char *argv[]) {
	GstElement *pipeline, *source, *sink;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;

	// Initialize GStreamer
	gst_init(&argc, &argv);

	/**
	 * gst_element_factory_make()により新しいelementを作成する
	 * - 最初のパラメータは, 作成するエレメントのタイプ
	 * - 2番目のパラメータは, インスタンスに付けたい名前. NULLならユニークな名前を付けてくれる
	 * 
	 * videotestsrc: テスト用のビデオパターンを作成するsource element
	 * autovideosink: 受け取った画像をウィンドウに表示するsink elemnent
	 */
	source = gst_element_factory_make("videotestsrc", "source");
	sink = gst_element_factory_make("autovideosink", "sink");

	// elementを含ませるためのパイプライン(GStreamerのデータ処理フロー)を作成する
	pipeline = gst_pipeline_new("test-pipeline");

	// どれかのelementが正しく作成されなかった場合, エラーメッセージを表示して終了
	if (!pipeline || !source || !sink) {
		g_printerr("Not all elements could be created.\n");
		return -1;
	}

	/**
	 * gst_bin_add_many()により, パイプラインにelementを追加する
	 * - GST_BIN(pipeline)にキャストすることで, パイプライン(GstPipeline)がGstBin(要素の集合体)として扱われる
	 * - 第2引数以降, NULLで終わる追加するelementのリストを受け取る
	 * 
	 * まだelement同士がリンクされていないので, gst_element_link()を使う
	 * source(入力)とsink(出力)を接続
	 * - 最初のパラメータがsource
	 * - 2番目のパラメータがdestinationとなる
	 */
	gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
	if (gst_element_link(source, sink) != TRUE) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(pipeline);
		return -1;
	}

	/**
	 * g_object_set()により, プロパティを変更する
	 * videotestsrcの"pattern"プロパティを0に変更する
	 * 0は標準のカラーバー
	 */
	g_object_set(source, "pattern", 0, NULL);

	/**
	 * パイプラインをGST_STATE_PLAYINGに変更
	 */
	ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Unable to set the pipeline to the playing state.\n");
		gst_object_unref(pipeline);
		return -1;
	}

	/**
	 * バス(メッセージ処理)
	 * gst_element_get_bus()でバスを取得
	 * gst_bus_timed_pop_filtered()を使い, エラー(GST_MESSAGE_ERROR)またはストリーム終了(GST_MESSAGE_EOS)を待機
	 */
	bus = gst_element_get_bus(pipeline);
	msg =
		gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
			GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
	
	/**
	 * メッセージの種類に応じて処理を行う
	 * GST_MESSAGE_ERROR
	 * - gst_message_parse_error()でエラー内容を取得して表示
	 * GST_MESSAGE_EOS
	 * - ストリーム終了のメッセージを受け取った場合に"End-Of-Stream reached"を表示
	 */
	if (msg != NULL) {
		GError *err;
		gchar *debug_info;

		switch (GST_MESSAGE_TYPE(msg)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error(msg, &err, &debug_info);
				g_printerr("Error received from element %s: %s\n",
					GST_OBJECT_NAME(msg->src), err->message);
				g_printerr("Debugging information: %s\n",
					debug_info ? debug_info : "none");
				g_clear_error(&err);
				g_free(debug_info);
				break;
			case GST_MESSAGE_EOS:
				g_print("End-Of-Stream reached.\n");
				break;
			default:
				g_printerr("Unexpected message received.\n");
				break;
		}
		gst_message_unref(msg);
	}

	// リソースの解放
	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
	return 0;
}

int main(int argc, char *argv[]) {
	return tutorial_main(argc, argv);
}