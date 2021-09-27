# 水やりデモのサンプルソース

## 使い方

AWS IoT Core上で2つのデバイスで利用するIoT Policy、キーペア、Thingを登録します。

`include/app_config_template.h` のファイルを `include/app_config.h` にリネームして、以下の情報を追記します。

- APP_CONFIG_WIFI_SSID
  - デバイスを接続するWi-FiのSSID(2.4GHzのアクセスポイントが必要です)
- APP_CONFIG_WIFI_PASSWORD
  - Wi-Fiのパスワード
- AWS_IOT_THING_NAME
  - AWS IoT Coreに登録したThing名。topicでも利用されます
- AWS_IOT_ENDPOINT
  - AWS IoT Coreのエンドポイント
- AWS_IOT_CERTIFICATE
  - AWS IoT Coreで発行した証明書
- AWS_IOT_PRIVATE_KEY
  - AWS IoT Coreで発行した秘密鍵
- AWS_ROOT_CA_CERTIFICATE
  - ATSエンドポイント用の証明書です。カスタムエンドポイントを利用する場合は、必要なものに置き換えてください