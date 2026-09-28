# edu-npu

QEMU `edu` PCI 디바이스(1234:11e8)를 NPU로 쓰는 리눅스 커널 드라이버.

## 빌드

```sh
make KDIR=<커널 빌드 디렉토리>
```

`KDIR`을 빼면 실행 중인 커널의 헤더로 빌드한다. KUnit 테스트는
`CONFIG_KUNIT=y`인 커널로 빌드할 때만 들어간다.

## 실행

`vm/vmlinuz`에 부팅할 커널 이미지를 둔다.

```sh
make KDIR=<커널 빌드 디렉토리> run
```

QEMU 안에서:

```sh
insmod /edu_npu.ko
```

KUnit 결과는 `insmod` 직후 콘솔에 나온다. `poweroff -f`로 끈다.
