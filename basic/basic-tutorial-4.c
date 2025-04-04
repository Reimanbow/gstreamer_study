#include <gst/gst.h>

// すべての情報を格納する構造体
typedef struct _CustomData {
	GstElement *playbin;
	// PLAYING状態か
	gboolean playing;
	// 打ち切るべきか
	gboolean terminate;
	// このメディアはシークが有効か?
	gboolean seek_enabled;
	// シークはもう済ませたか
	gboolean seek_done;
	// このメディアの寿命はナノ秒単位でどのくらいか
	gint64 duration;
} CustomData;

static void handle_message(CustomData *data, GstMessage *msg);

int main(int argc, char *argv[]) {
	CustomData data;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;

	data.playing = FALSE;
	data.terminate = FALSE;
	data.seek_enabled = FALSE;
	data.seek_done = FALSE;
	data.duration = GST_CLOCK_TIME_NONE;

	// GStreamerの初期化
	gst_init(&argc, &argv);

	// エレメントの作成
	data.playbin = gst_element_factory_make("playbin", "playbin");

	if (!data.playbin) {
		g_printerr("Not all elements could be created.\n");
		return -1;
	}

	g_object_set(data.playbin, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);

	ret = gst_element_set_state(data.playbin, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Unable to set the pipeline to the playing state.\n");
		gst_object_unref(data.playbin);
		return -1;
	}

	bus = gst_element_get_bus(data.playbin);
	do {
		/**
		 * GStreamerのバスからメッセージを待つ
		 * 100ミリ秒待っても何も来なければNULLを返す(タイムアウト)
		 */
		msg = gst_bus_timed_pop_filtered(bus, 100 * GST_MSECOND,
			GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_DURATION);
		
		// msgがNULLならタイムアウト
		if (msg != NULL) {
			handle_message(&data, msg);
		} else {
			// タイムアウトが切れたことを意味する
			if (data.playing) {
				// 現在の再生位置を取得
				gint64 current = -1;

				// 現在の再生位置をplaybinに問い合わせる. currentに値が入る
				if (!gst_element_query_position(data.playbin, GST_FORMAT_TIME, &current)) {
					g_printerr("Could not query current position.\n");
				}

				/**
				 * GST_CLOCK_TIME_NONEなら, data.durationの長さが不明
				 * playbinに動画の全体時間を問い合わせる
				 * 成功すれば, data.durationに保存される
				 */
				if (!GST_CLOCK_TIME_IS_VALID(data.duration)) {
					if (!gst_element_query_duration(data.playbin, GST_FORMAT_TIME, &data.duration)) {
						g_printerr("Could not query current duration.\n");
					}
				}

				// 今の再生位置と動画の全体の長さを表示する
				g_print("Position %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
					GST_TIME_ARGS(current), GST_TIME_ARGS(data.duration));
				
				/**
				 * シーク処理の条件チェック
				 * - seek_enabled: そのメディアはシーク可能か
				 * - !seek_done: まだシークしていない
				 * - current > 10: 現在再生位置が10秒を超えた
				 * やること
				 * - 30秒地点へジャンプ(シーク)する
				 * - もう1度はやらないようにseek_done = TRUEにする
				 */
				if (data.seek_enabled && !data.seek_done && current > 10 * GST_SECOND) {
					g_print("\nReached 10s, performing seek...\n");
					gst_element_seek_simple(data.playbin, GST_FORMAT_TIME,
						GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 30 * GST_SECOND);
					data.seek_done = TRUE;
				}
			}
		}
	} while (!data.terminate);

	gst_object_unref(bus);
	gst_element_set_state(data.playbin, GST_STATE_NULL);
	gst_object_unref(data.playbin);
	return 0;
}

static void handle_message(CustomData *data, GstMessage *msg) {
	GError *err;
	gchar *debug_info;

	switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &err, &debug_info);
			g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
			g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
			g_clear_error(&err);
			g_free(debug_info);
			data->terminate = TRUE;
			break;
		case GST_MESSAGE_EOS:
			g_print("\nEond-Of-Stream reached.\n");
			data->terminate = TRUE;
			break;
		case GST_MESSAGE_DURATION:
			// 時間が変更され, 現在の期間を無効とする
			data->duration = GST_CLOCK_TIME_NONE;
			break;
		case GST_MESSAGE_STATE_CHANGED:
			GstState old_state, new_state, pending_state;
			gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
			// playbinからのメッセージだけを対象にする
			if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->playbin)) {
				g_print("Pipeline state changed from %s to %s:\n",
					gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
				
				data->playing = (new_state == GST_STATE_PLAYING);

				// 再生中ならシークできるか確認
				if (data->playing) {
					GstQuery *query;
					gint64 start, end;
					query = gst_query_new_seeking(GST_FORMAT_TIME);
					// GStreamerにこのストリームはシークできるかを問い合わせる
					if (gst_element_query(data->playbin, query)) {
						gst_query_parse_seeking(query, NULL, &data->seek_enabled, &start, &end);
						// シークできるなら, 開始位置と終了位置も表示
						if (data->seek_enabled) {
							g_print("Seeking is ENABLED from %" GST_TIME_FORMAT "to %" GST_TIME_FORMAT "\n",
								GST_TIME_ARGS(start), GST_TIME_ARGS(end));
						} else {
							g_print("Seeking is DISABLED for this stream.\n");
						}
					} else {
						g_printerr("Seeking query failed.");
					}
					gst_query_unref(query);
				}
			}
			break;
		default:
			g_printerr("Unexpected message received.\n");
			break;
	}
	gst_message_unref(msg);
}