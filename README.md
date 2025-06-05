# uds_fw_update
- 이 프로젝트는 ISO 14229 UDS를 이용한 firmware update입니다.
- uds client와 uds server로 나누어져있고 uds client가 uds server의 firmware를 update합니다.
- 이 프로젝트의 목적은 uds를 이용한 firmware update구현이기 때문에 uds의 하위 계층인 ISO 15765-2 ISO-TP는 uds client와 server를 연결하는 간단한 mock function으로 구현되어있습니다.

build:
    make clean
    make

test:
    ./uds_fw_update
	