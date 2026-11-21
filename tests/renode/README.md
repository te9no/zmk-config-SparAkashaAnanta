# Ananta の設定保存・再起動試験

右の親機に仮想 UART で Studio クライアントを接続し、右のローカル設定と左への転送を検査します。各側で下部6通りと上部2通りを組み合わせ、保存直後の選択値・起動値、その側を再起動した後の両方の値、反対側の設定を照合します。

実機は下部 Cirque に GPIO I²C、上部 IQS に TWI1 を使います。シミュレーターは下部を TWI0 の Cirque モデルに接続し、上部は TWI1 の IQS モデルに接続します。実機用 GPIO I²C の電気的動作、USB・BLE の通信、物理入力、PMW3610 の非同期セルフテストは実機検証事項です。

ワークスペース直下で、Ananta の開発ブランチからプロファイルを初期化し、対応する Web UI の studio-web で npm ci と npm run build を実行します。

```sh
ZMK_CONFIG_ROOT=work/ananta-zeroshift ZMK_CONFIG_NAME=Ananta ZMK_BUILD_MATRIX=build-renode.yaml ZMK_FIRMWARE_TEST_PURPOSE=zeroshift-sim-r01 ./just.sh --profile ananta-zeroshift build-parallel SAA-ZeroShift-sim 2
ZEROSHIFT_SHARED_RPC="$PWD/work/ananta-zeroshift-ui/studio-web/dist/rpc.mjs" python3 -B work/ananta-zeroshift/tests/renode/test_studio.py --renode .zmk-workspace/tools/renode-1.17.0/renode --firmware-dir firmware/testing/Ananta/zeroshift-sim-r01 --log /tmp/ananta-renode.log
```
