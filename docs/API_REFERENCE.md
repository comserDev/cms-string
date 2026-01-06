# cms-embedded-utils API Reference

이 문서는 `cms-embedded-utils` 라이브러리에서 제공하는 주요 클래스 및 함수의 상세 명세입니다. 모든 컴포넌트는 임베디드 환경 최적화를 위해 동적 메모리 할당을 사용하지 않습니다.

---

## 1. cms::String<N> & cms::StringBase
고정 크기 정적 버퍼를 사용하는 UTF-8 안전 문자열 컨테이너입니다.

### 상태 및 정보
- `size_t length()`: 현재 문자열의 바이트 길이를 반환합니다.
- `size_t capacity()`: 버퍼의 전체 물리적 크기를 반환합니다.
- `size_t count()`: UTF-8 인코딩을 인식한 논리적 글자 수를 반환합니다.
- `float utilization()`: 현재 버퍼 사용률(%)을 반환합니다.
- `float peakUtilization()`: 객체 생성 후 도달했던 최대 사용률(%)을 반환합니다. (`CMS_ENABLE_PROFILING` 활성 시)

### 데이터 조작
- `void clear()`: 문자열을 비웁니다.
- `void append(const char* s, size_t len)`: 지정된 길이만큼 데이터를 뒤에 추가합니다.
- `int appendPrintf(const char* format, ...)`: printf 스타일로 문자열을 추가합니다.
- `void trim()`: 양 끝의 공백 및 제어 문자를 제거합니다.
- `void replace(const char* from, const char* to, bool ignoreCase = false)`: 특정 패턴을 찾아 치환합니다.
- `void insert(size_t charIdx, const char* src)`: 특정 글자 위치에 문자열을 삽입합니다.
- `void remove(size_t charIdx, size_t charCount)`: 특정 구간의 글자들을 삭제합니다.

### 검색 및 비교
- `int indexOf(const char* str, size_t startChar = 0)`: 특정 문자열이 처음 나타나는 글자 위치를 반환합니다.
- `int lastIndexOf(const char* target)`: 마지막으로 나타나는 위치를 반환합니다.
- `bool startsWith(const char* prefix)` / `bool endsWith(const char* suffix)`: 접두사/접미사 일치 여부를 확인합니다.
- `bool contains(const char* target)`: 부분 문자열 포함 여부를 확인합니다.
- `bool equals(const char* other, bool ignoreCase = false)`: 내용 일치 여부를 비교합니다.

### 변환 및 추출
- `int toInt()` / `double toFloat()`: 문자열을 숫자로 변환합니다.
- `void substring(StringBase& dest, size_t left, size_t right = 0)`: 글자 단위 범위를 추출하여 `dest`에 저장합니다.
- `void toUpperCase()` / `void toLowerCase()`: 영문 대소문자 변환을 수행합니다.

---

## 2. cms::Queue<T, N> & cms::ThreadSafeQueue<T, N>
고정 크기 원형 버퍼 기반의 FIFO 큐입니다.

### cms::Queue<T, N, IndexType> (기본형)
- 뮤텍스 오버헤드가 없는 순수 원형 버퍼입니다.
- 단일 태스크 내에서 데이터를 임시 보관하거나, 메모리 제약이 극심한 환경에 최적화되어 있습니다.

### cms::ThreadSafeQueue<T, N, IndexType> (스레드 안전형)
- 내부적으로 뮤텍스를 소유하여 멀티태스크 환경에서 데이터 경합을 방지합니다.

### 공통 메서드
- `void enqueue(const T& item)`: 데이터를 추가합니다. 가득 차면 가장 오래된 데이터를 덮어씁니다.
- `bool pop(T& outItem)`: 가장 오래된 데이터를 꺼내 `outItem`에 저장합니다. 비어있으면 `false`를 반환합니다.
- `bool getAt(IndexType index, T& outItem)`: 큐를 비우지 않고 특정 위치의 데이터를 조회합니다.
- `bool isEmpty()` / `bool isFull()`: 큐의 상태를 확인합니다.
- `IndexType size()`: 현재 저장된 데이터의 개수를 반환합니다.

---

## 3. cms::AsyncLogger & cms::LoggerBase
Thin Template 패턴이 적용된 고성능 비동기 로거입니다.

### 설정 및 제어
- `static AsyncLogger& instance()`: 기본 크기(256, 16)의 싱글톤 인스턴스를 반환합니다.
- `void begin(LogLevel level)`: 로거를 초기화하고 최소 출력 레벨을 설정합니다.
- `void setRuntimeLevel(LogLevel level)`: 실행 중에 로그 출력 레벨을 변경합니다.
- `void setUseColor(bool useColor)`: ANSI 색상 코드 사용 여부를 설정합니다.

### 로깅 API
- `d(format, ...)`: Debug 레벨 로그 출력.
- `i(format, ...)`: Info 레벨 로그 출력.
- `w(format, ...)`: Warn 레벨 로그 출력.
- `e(format, ...)`: Error 레벨 로그 출력.
- `log(level, format, ...)`: 지정된 레벨로 로그를 출력합니다.

### 실행 및 확장
- `bool processNextLog()`: 큐에서 로그를 하나 꺼내 실제 출력 장치(`outputLog`)로 보냅니다.
- `virtual bool handleLog(const StringBase& msg)`: 큐 저장 전 필터링 로직을 재정의합니다.
- `virtual void outputLog(const StringBase& msg)`: 실제 출력 매체(Serial, TCP 등)를 재정의합니다.

---

## 4. cms::string (Utility Namespace)
원시 C 문자열(char*)을 직접 다루는 고성능 저수준 함수군입니다. `String` 클래스 없이도 독립적으로 사용 가능합니다.

### UTF-8 및 검증
- `size_t utf8_strlen(const char* str)`: UTF-8 문자열의 실제 글자 수를 계산합니다.
- `bool validateUtf8(const char* str)`: UTF-8 인코딩 유효성을 검사합니다.
- `size_t sanitizeUtf8(char* str, size_t maxLen)`: 깨진 바이트를 정제하고 최종 길이를 반환합니다.

### 변환 및 검사
- `int toInt(const char* str, size_t len = 0)`: 문자열을 정수로 변환합니다.
- `double toFloat(const char* str, size_t len = 0)`: 문자열을 실수로 변환합니다.
- `bool isDigit(const char* str)` / `bool isNumeric(const char* str)`: 숫자 형식 여부를 확인합니다.
- `int hexToInt(const char* str)`: 16진수 문자열(0x... 포함 가능)을 정수로 변환합니다.

### 조작 및 검색
- `size_t trim(char* str)`: 원시 버퍼의 양 끝 공백을 제거합니다. (In-place)
- `const char* strcasestr(const char* haystack, const char* needle)`: 대소문자 무시 부분 문자열 검색.
- `size_t split(const char* str, char delimiter, Token* tokens, size_t maxTokens)`: 비파괴적 분할.
- `size_t replace(char* str, size_t maxLen, size_t curLen, const char* from, const char* to, bool ignoreCase = false)`: 원시 버퍼 내 패턴 치환.

---

## 5. Global Helpers (cmsString.h)
`String<N>` 객체들을 더 효율적으로 다루기 위한 템플릿 함수입니다.

- `size_t copyTokens(const Token* tokens, size_t count, String<N> (&dest)[M])`: `split` 결과인 Token 배열을 실제 `String<N>` 배열로 안전하게 복사합니다.
- `size_t splitTo(const StringBase& src, char delimiter, String<N> (&dest)[M])`: 문자열을 분리하여 즉시 `String<N>` 배열로 변환합니다. (가장 많이 사용됨)

---

## 6. 설계 원칙 (Design Principles)

### Zero-Heap
모든 컴포넌트는 런타임에 `malloc`이나 `new`를 호출하지 않습니다. 이는 메모리 파편화를 방지하고 시스템의 결정론적 동작을 보장합니다.

### Thin Template Pattern
`AsyncLogger`와 `String` 클래스는 템플릿 인자에 따라 코드가 중복 생성되는 것을 방지하기 위해, 실제 로직을 비-템플릿 베이스 클래스(`LoggerBase`, `StringBase`)에 구현하여 바이너리 크기를 최적화합니다.

### UTF-8 Awareness
문자열 조작 시 바이트 단위가 아닌 논리적 글자 단위를 기준으로 동작하여 멀티바이트 문자가 깨지는 것을 방지합니다.
