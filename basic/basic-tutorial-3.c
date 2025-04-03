#include <gst/gst.h>

// GStreamerのパイプラインに必要な要素(GstElement)をまとめた構造体
typedef struct _CustomData {
	GstElement *pipeline;
	GstElement *source;
	GstElement *convert;
	GstElement *resample;
	GstElement *sink;
} CustomData;

/**
 * pad-addedシグナルのコールバック関数の宣言
 * uridecodebinは入力されるメディアの種類に応じて動的にパッドを生成する
 * 他の要素と適切に接続するための関数
 */
static void pad_added_handler(GstElement *src, GstPad *pad, CustomData *data);

int main(int argc, char *argv[]) {
	CustomData data;
	// パイプラインのイベントを受け取るためのバス(イベント通信路)
	GstBus *bus;
	// GStreamerのイベントメッセージを格納する変数
	GstMessage *msg;
	// パイプラインの状態遷移結果を受け取る変数
	GstStateChangeReturn ret;
	// whileループの終了フラグ
	gboolean terminate = FALSE;

	// GStreamerの初期化
	gst_init(&argc, &argv);

	// GStreamer要素の作成
	/**
	 * uridecodebin
	 * - URIからメディアを読み込む要素
	 * - 入力メディアに応じて, 適切なデコード処理を自動的に選択
	 */
	data.source = gst_element_factory_make("uridecodebin", "source");
	/**
	 * audioconvert
	 * - 音声データのフォーマットを適切な形式に変換
	 */
	data.convert = gst_element_factory_make("audioconvert", "convert");
	/**
	 * audioresample
	 * - サンプルレートを変換する
	 */
	data.resample = gst_element_factory_make("audioresample", "resample");
	/**
	 * autoaudiosink
	 * - OSの標準出力に自動的に接続して再生
	 */
	data.sink = gst_element_factory_make("autoaudiosink", "sink");

	// パイプライン全体を生成
	data.pipeline = gst_pipeline_new("test-pipeline");

	if (!data.pipeline || !data.source || !data.convert || !data.resample || !data.sink) {
		g_printerr("Not all elements could be created.\n");
		return -1;
	}

	// gst_bin_add_many()を使って, 全ての要素をパイプラインに追加
	gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.convert, data.resample, data.sink, NULL);
	/**
	 * gst_element_link_many()を使って, convert->resample->sinkをリンク
	 * - ただしuridecodebinは動的にパッドを生成するため, ここではリンクできない
	 * - そのため, pad-addedシグナルを使って, 後で適切に接続する
	*/
	if (!gst_element_link_many(data.convert, data.resample, data.sink, NULL)) {
		g_printerr("Elements could not be linked.\n");
		return -1;
	}

	// g_object_set()を使って, uridecodebinの"uri"プロパティにURLを設定
	g_object_set(data.source, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);

	/**
	 * uridecodebinのpad-addedシグナルをpad_added_handlerに接続
	 * - 動的に生成されるパッドをconvertの入力にリンクするための処理
	 */
	g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);

	// gst_element_set_state()でパイプラインを"PLAYING"状態に遷移
	ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Unable to set the pipeline to the playing state.\n");
		gst_object_unref(data.pipeline);
		return -1;
	}

	// パイプラインのバスを監視する
	bus = gst_element_get_bus(data.pipeline);
	do {
		// 特定のイベント(エラー, 終了, 状態遷移)を待つ
		msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
			GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
		
		if (msg != NULL) {
			GError *err;
			gchar *debug_info;

			switch (GST_MESSAGE_TYPE(msg)) {
				// GST_MESSAGE_ERROR->エラーを表示して終了
				case GST_MESSAGE_ERROR:
					gst_message_parse_error(msg, &err, &debug_info);
					g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
					g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
					g_clear_error(&err);
					g_free(debug_info);
					terminate = TRUE;
					break;
				// GST_MESSAGE_EOS->ストリームの終了を検出して終了
				case GST_MESSAGE_EOS:
					g_print("End-Of-Stream reached.\n");
					terminate = TRUE;
					break;
				// GST_MESSAGE_STATE_CHANGED->パイプラインの状態遷移を表示
				case GST_MESSAGE_STATE_CHANGED:
					if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline)) {
						GstState old_state, new_state, pending_state;
						gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
						g_print("Pipeline state changed from %s to %s:\n",
							gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
					}
					break;
				defautl:
					g_printerr("Unexpected message received.\n");
					break;
			}
			gst_message_unref(msg);
		}
	} while (!terminate);

	// クリーンアップ
	gst_object_unref(bus);
	gst_element_set_state(data.pipeline, GST_STATE_NULL);
	gst_object_unref(data.pipeline);
	return 0;
}

/**
 * uridecodebinが動的に新しいパッドを生成した際に呼び出されるコールバック関数
 * 生成されたパッドをaudioconvertの"sink"パッドに適切にリンクする
 * - GstElement *src: uridecodebin要素(パッドの発生源)
 * - GstPad *new_pad: uridecodebinが動的に生成した新しいパッド
 * - CustomData *data: CustomData構造体のポインタ(パイプライン全体の情報を保持)
 */
static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data) {
	// gst_element_get_static_pad()で, sinkパッドを取得
	GstPad *sink_pad = gst_element_get_static_pad(data->convert, "sink");
	GstPadLinkReturn ret;
	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;

	g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

	// gst_pad_is_linked()でsink_padが既にリンクされているか確認
	if (gst_pad_is_linked(sink_pad)) {
		g_print("We are already linked. Ignoring.\n");
		goto exit;
	}

	// new_padの現在のGstCaps(パッドの能力情報)を取得
	new_pad_caps = gst_pad_get_current_caps(new_pad);
	// GstCapsから最初のGstStructureを取得
	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
	// GstStructureからメディアの種類(MIMEタイプ)を取得
	new_pad_type = gst_structure_get_name(new_pad_struct);
	// メディアタイプがaudio/x-raw(未圧縮のPCM音声データを指す)か確認
	if (!g_str_has_prefix(new_pad_type, "audio/x-raw")) {
		g_print("It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
		goto exit;
	}

	// new_padをsink_padにリンク
	ret = gst_pad_link(new_pad, sink_pad);
	if (GST_PAD_LINK_FAILED(ret)) {
		g_print("Type is '%s' but link failed.\n", new_pad_type);
	} else {
		g_print("Link succeded (type '%s').\n", new_pad_type);
	}

	// 後処理と解放
exit:
	if (new_pad_caps != NULL) {
		gst_caps_unref(new_pad_caps);
	}

	gst_object_unref(sink_pad);
}