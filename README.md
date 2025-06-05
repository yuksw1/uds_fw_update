# uds_fw_update

이 프로젝트는 ISO 14229 UDS 프로토콜을 이용해 ECU 펌웨어 업데이트 과정을 테스트하기 위한 예제입니다. 클라이언트와 서버가 하나의 바이너리에서 동작하며, ISO-TP 계층은 단순 모의 함수로 구현되어 있습니다.

## 빌드 방법 (Linux)
```bash
make -C src clean
make -C src
```

## 실행 방법
소스 루트에서 다음 명령을 실행합니다. 테스트 이미지는 `src/test.dat`을 사용합니다.
```bash
./src/uds_fw_update
```

