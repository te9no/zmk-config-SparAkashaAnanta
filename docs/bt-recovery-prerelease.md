# Ananta 無線復旧版

`zmk-0.4` の [Ananta BLE recovery build](https://github.com/te9no/zmk-config-SparAkashaAnanta/actions/workflows/bt-recovery.yml) から、使用する左右のモジュールと IQS の有無に合う `bt-recovery-*` 成果物を選びます。各成果物には UF2 とソース・依存リビジョン・SHA-256 を記録した JSON が入っています。

右親機のスキャン開始、左子機の広告開始が失敗した場合、1・2・4・8・16 秒後に最大5回再試行します。スリープ中は復旧処理を休止します。無線復旧版のビルドは固定した ZMK にブリッジパッチを適用し、左右20構成で復旧機能を有効にします。設定リセット用の1構成では無効です。

通常版は従来のビルドで取得できます。長時間放置後の再接続やスリープ復帰は実機での確認が必要です。
