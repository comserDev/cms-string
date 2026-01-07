# cms-embedded-utils

**High-performance, Zero-Heap, Deterministic Utilities for Embedded Systems.**

`cms-embedded-utils`는 자원이 제한된 임베디드 환경(ESP32, STM32, Arduino 등)에서 힙 파편화(Heap Fragmentation) 없이 안전하고 예측 가능한 시스템을 구축하기 위한 C++ 유틸리티 모음입니다. 모든 컴포넌트는 동적 메모리 할당을 배제하고 정적 버퍼를 기반으로 동작합니다.

## 🛠 Technical Highlights / 주요 특징

- **Zero-Heap Architecture / 제로 힙 구조**: Eliminates runtime memory allocation (`malloc`/`new`) to prevent heap fragmentation. 모든 문자열은 정적 배열에 저장되어 시스템 안정성이 극대화됩니다.
- **UTF-8 Awareness / UTF-8 지원**: Provides logical character-based indexing and slicing, preventing corruption of multi-byte characters. 한글 등 멀티바이트 문자가 깨지는 것을 방지합니다.
- **Thread-Safe Circular Queues / 스레드 안전 큐**: High-performance, lock-protected circular buffers for inter-task communication. 멀티태스킹 환경에서 안전한 데이터 교환을 지원합니다.
- **AsyncLogger (Thin Template) / 비동기 로거**: A lightweight logger that minimizes code bloat using the Thin Template pattern. 템플릿 비대화를 방지하면서도 강력한 스타일링과 비동기 로깅을 제공합니다.
- **Real-time Resource Profiling / 실시간 리소스 프로파일링**: Built-in monitoring for buffer utilization and peak usage (High Water Mark). 버퍼 사용률과 피크치를 실시간으로 모니터링합니다.

## 📦 Installation / 설치 방법

### PlatformIO
Add the repository URL to your `platformio.ini`: / `platformio.ini` 파일의 `lib_deps` 항목에 아래와 같이 추가하세요.

```ini
lib_deps =
    https://github.com/comserDev/cms-embedded-utils.git
```

## 🚀 빠른 시작

### 1. 선언 및 기본 사용

```cpp
#include <cmsString.h>

// 64바이트 고정 크기 버퍼를 가진 문자열 선언
cms::String<64> str = "Hello";

// 스트림 스타일 결합
str << " World! " << 2024 << " [OK]";

Serial.println(str.c_str()); // "Hello World! 2024 [OK]"
```

### 2. UTF-8 안전한 조작

```cpp
cms::String<64> ko = "안녕하세요";

// 논리적 글자 수 반환 (바이트 수가 아님)
size_t len = ko.count(); // 5

// 글자 단위 부분 문자열 추출
cms::String<32> sub;
ko.substring(sub, 0, 2); // "안녕"
```

### 3. 큐 (Queue)

#### 기본 큐 (Single-task / Interrupt-safe 전용)
뮤텍스 잠금이 없어 속도가 매우 빠르며, 단일 루프 내 데이터 보관에 적합합니다.
```cpp
#include <cmsQueue.h>
cms::Queue<int, 5> q;
q.enqueue(10);
```

#### 스레드 안전 큐 (Multi-task 전용)
멀티태스킹 환경에서 태스크 간 데이터 교환 시 사용합니다.
```cpp
// 10개의 정수를 저장할 수 있는 스레드 안전 큐
cms::ThreadSafeQueue<int, 10> queue;

// 데이터 추가 (가득 차면 가장 오래된 데이터 덮어씀)
queue.enqueue(42);

// 데이터 꺼내기
int val;
if (queue.pop(val)) {
    // val 사용
}
```

### 4. 고성능 비동기 로거 (AsyncLogger)

```cpp
#include <cmsAsyncLogger.h>

// 로거 인스턴스 획득 및 설정
auto& logger = cms::AsyncLogger<>::instance();
logger.begin(cms::LogLevel::Debug, true);

// 로그 출력 (자동 스타일링 및 태그 지원)
logger.i("시스템 시작... [Network] 연결됨");
logger.w("센서 데이터 불안정: %d", 404);

// 백그라운드 루프에서 로그 처리
while (logger.update());
```

### 4. 리터럴 최적화

문자열 리터럴을 사용할 경우 컴파일 타임에 길이를 계산하여 런타임 `strlen` 오버헤드를 제거합니다.

```cpp
if (str == "COMMAND") { ... } // 고속 비교
str << "Data";               // 고속 결합
```

## 📊 성능 모니터링

임베디드 시스템의 리소스 최적화를 위해 현재 버퍼 상태를 확인할 수 있습니다.

```cpp
float current = str.utilization();     // 현재 사용률 (%)
float peak = str.peakUtilization();    // 객체 생성 후 최대 도달 사용률 (%)
```

## 🛠 빌드 설정 권장사항

한글 깨짐 방지 및 최신 C++ 기능을 위해 `platformio.ini`에 아래 설정을 추가하는 것을 권장합니다.

```ini
build_flags =
    -std=gnu++17
    -finput-charset=UTF-8
    -fexec-charset=UTF-8
```

## 📄 라이선스

이 프로젝트는 MIT 라이선스 하에 배포됩니다.

---
**Maintainer:** comser.dev
**Repository:** github.com/comserDev/cms-embedded-utils
