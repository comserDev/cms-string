/// @author comser.dev
///
/// 임베디드 시스템의 메모리 안정성을 위해 설계된 정적 및 스레드 안전 큐 라이브러리입니다.

#pragma once // 중복 포함 방지

#include <stddef.h> // size_t 정의
#ifndef ARDUINO // PC 환경(테스트용) 지원
#include <mutex> // 표준 뮤텍스 사용
#endif
#ifdef ARDUINO // ESP32/Arduino 환경
#include <freertos/FreeRTOS.h> // FreeRTOS 커널
#include <freertos/semphr.h> // 세마포어/뮤텍스 API
#endif

namespace cms {

// ==================================================================================================
// [Queue] 개요
// - 왜 존재하는가: 동적 할당 없이 고정된 메모리 내에서 데이터를 관리하기 위해 존재합니다.
// - 어떻게 동작하는가: 헤드와 테일 인덱스를 이용한 원형 버퍼 구조로 동작합니다.
// ==================================================================================================

/// 고정 크기 원형 큐(Circular Queue) 클래스 템플릿입니다.
///
/// Why: 힙 메모리 파편화를 방지하고 예측 가능한 메모리 사용량을 유지하기 위함입니다.
/// How: 배열을 기반으로 인덱스가 순환하며, 가득 찼을 때 가장 오래된 데이터를 덮어쓰는 방식으로 자원을 관리합니다.
///
/// @tparam T 저장할 데이터 타입
/// @tparam N 큐의 최대 용량
template <typename T, size_t N>
class Queue {
public:
    /// 큐의 상태를 초기화합니다.
    ///
    /// 사용 예:
    /// @code
    /// cms::Queue<int, 10> q;
    /// @endcode
    Queue() : _head(0), _tail(0), _count(0) {}

    /// 데이터를 큐에 추가합니다. 큐가 가득 찬 경우 가장 오래된 데이터를 덮어씁니다.
    ///
    /// 새로운 데이터를 수용하기 위해 가장 오래된 데이터를 자동으로 밀어내는 링 버퍼 구조를 가집니다.
    /// 테일 인덱스를 이동시키며 데이터를 쓰고, 가득 찬 경우 헤드를 함께 이동시켜 덮어쓰기를 수행합니다.
    ///
    /// 사용 예:
    /// @code
    /// queue.enqueue(100);
    /// @endcode
    ///
    /// @param item 추가할 데이터 참조
    void enqueue(const T& item) {
        // 큐가 가득 찬 경우 헤드를 밀어내어 공간 확보
        if (isFull()) {
            _head = (_head + 1) % N;
            _count--;
        }
        // 데이터 저장 및 테일 포인터 이동
        _data[_tail] = item;
        _tail = (_tail + 1) % N;
        _count++;
    }

    /// 큐에서 가장 오래된 데이터를 꺼내옵니다.
    ///
    /// 선입선출(FIFO) 원칙에 따라 데이터를 순차적으로 처리하기 위함입니다.
    /// 헤드 인덱스가 가리키는 데이터를 복사한 후 헤드를 다음 위치로 이동시킵니다.
    ///
    /// 사용 예:
    /// @code
    /// int data;
    /// bool success = queue.pop(data);
    /// @endcode
    ///
    /// @param outItem [OUT] 꺼낸 데이터를 저장할 참조 변수
    ///
    /// @return true: 성공, false: 큐가 비어있음
    bool pop(T& outItem) {
        if (isEmpty()) return false;
        // 헤드 위치의 데이터 추출 및 인덱스 갱신
        outItem = _data[_head];
        _head = (_head + 1) % N;
        _count--;
        return true;
    }

    /// 특정 인덱스(상대적 위치)의 데이터를 조회합니다.
    ///
    /// 큐를 비우지 않고 내부 데이터를 순회하거나 특정 시점의 기록을 찾기 위함입니다.
    /// 헤드 위치를 기준으로 오프셋을 계산하여 원형 배열의 물리적 위치를 찾아냅니다.
    ///
    /// 사용 예:
    /// @code
    /// int first;
    /// queue.getAt(0, first);
    /// @endcode
    ///
    /// @param index 조회할 상대적 인덱스 (0: 가장 오래된 데이터)
    /// @param outItem [OUT] 조회된 데이터를 저장할 참조 변수
    ///
    /// @return true: 조회 성공, false: 인덱스 범위 초과
    bool getAt(size_t index, T& outItem) const {
        if (index >= _count) return false;
        // 원형 버퍼의 물리적 위치 계산
        size_t pos = (_head + index) % N;
        outItem = _data[pos];
        return true;
    }

    /// 큐가 비어있는지 확인합니다.
    ///
    /// 사용 예:
    /// @code
    /// if (queue.isEmpty()) { ... }
    /// @endcode
    bool isEmpty() const { return _count == 0; }

    /// 큐가 가득 찼는지 확인합니다.
    ///
    /// 사용 예:
    /// @code
    /// if (queue.isFull()) { ... }
    /// @endcode
    bool isFull() const { return _count == N; }

    /// 현재 저장된 데이터 개수를 반환합니다.
    ///
    /// 사용 예:
    /// @code
    /// size_t n = queue.size();
    /// @endcode
    ///
    /// @return 현재 데이터 개수 (0 ~ N)
    size_t size() const { return _count; }

private:
    /// 데이터를 저장하는 고정 크기 정적 배열.
    T _data[N];
    /// 읽기 작업을 수행할 가장 오래된 데이터의 인덱스.
    size_t _head;
    /// 쓰기 작업을 수행할 다음 데이터의 저장 위치 인덱스.
    size_t _tail;
    /// 현재 큐에 저장된 유효 데이터의 총 개수 (0 ~ N).
    size_t _count;
};

// ==================================================================================================
// [ThreadSafeQueue] 개요
// - 왜 존재하는가: 멀티태스킹 환경에서 여러 태스크가 동시에 큐에 접근할 때 발생하는 데이터 오염을 방지합니다.
// - 어떻게 동작하는가: 내부 큐 조작 전후로 뮤텍스(Mutex)를 사용하여 임계 영역을 보호합니다.
// ==================================================================================================

/// 뮤텍스를 사용하여 스레드 안전(Thread-Safe)을 보장하는 큐 클래스입니다.
///
/// Why: 인터럽트나 멀티태스크 환경에서 데이터 경합(Race Condition)을 방지하기 위함입니다.
/// How: 모든 공개 메서드 실행 시 내부 뮤텍스를 획득(Lock)하고 종료 시 해제(Unlock)합니다.
///
/// @tparam T 저장할 데이터 타입
/// @tparam N 큐의 최대 용량
template <typename T, size_t N>
class ThreadSafeQueue {
public:
    /// 뮤텍스를 생성하고 내부 큐를 초기화합니다.
    ///
    /// 사용 예:
    /// @code
    /// cms::ThreadSafeQueue<int, 5> tsQueue;
    /// @endcode
    ThreadSafeQueue() {
        // 플랫폼별 뮤텍스 초기화
#ifdef ARDUINO
        _mutex = xSemaphoreCreateMutex();
#endif
    }

    /// 할당된 뮤텍스 자원을 시스템에 반환합니다.
    ///
    /// 사용 예:
    /// @code
    /// delete tsQueue; // 소멸자 자동 호출
    /// @endcode
    ~ThreadSafeQueue() {
        // 할당된 뮤텍스 자원 해제
#ifdef ARDUINO
        if (_mutex) {
            vSemaphoreDelete(_mutex);
        }
#endif
    }

    /// 뮤텍스 잠금 후 데이터를 안전하게 추가합니다.
    ///
    /// 사용 예:
    /// @code
    /// tsQueue.enqueue(50);
    /// @endcode
    ///
    /// @param item 추가할 데이터 참조
    void enqueue(const T& item) {
        lock();
        _queue.enqueue(item);
        unlock();
    }

    /// 뮤텍스 잠금 후 데이터를 안전하게 꺼내옵니다.
    ///
    /// 사용 예:
    /// @code
    /// int val;
    /// if (tsQueue.pop(val)) { ... }
    /// @endcode
    ///
    /// @param outItem [OUT] 꺼낸 데이터를 저장할 참조 변수
    ///
    /// @return true: 성공, false: 큐가 비어있음
    bool pop(T& outItem) {
        lock();
        bool ok = _queue.pop(outItem);
        unlock();
        return ok;
    }

    /// 뮤텍스 잠금 후 특정 인덱스의 데이터를 안전하게 조회합니다.
    ///
    /// 사용 예:
    /// @code
    /// int data;
    /// if (tsQueue.getAt(0, data)) { ... }
    /// @endcode
    ///
    /// @param index 조회할 상대적 인덱스
    /// @param outItem [OUT] 조회된 데이터를 저장할 참조 변수
    ///
    /// @return true: 조회 성공, false: 범위 초과
    bool getAt(size_t index, T& outItem) const {
        lock();
        bool ok = _queue.getAt(index, outItem);
        unlock();
        return ok;
    }

    /// 큐가 비어있는지 스레드 안전하게 확인합니다.
    ///
    /// 사용 예:
    /// @code
    /// if (tsQueue.isEmpty()) { ... }
    /// @endcode
    bool isEmpty() const { lock(); bool empty = _queue.isEmpty(); unlock(); return empty; }

    /// 큐가 가득 찼는지 스레드 안전하게 확인합니다.
    ///
    /// 사용 예:
    /// @code
    /// if (tsQueue.isFull()) { ... }
    /// @endcode
    bool isFull() const { lock(); bool full = _queue.isFull(); unlock(); return full; }

    /// 현재 데이터 개수를 스레드 안전하게 조회합니다.
    ///
    /// 사용 예:
    /// @code
    /// size_t s = tsQueue.size();
    /// @endcode
    size_t size() const { lock(); size_t count = _queue.size(); unlock(); return count; }

private:
    /// 뮤텍스를 획득하여 임계 영역에 진입합니다.
    ///
    /// 여러 태스크가 동시에 큐를 수정할 때 발생하는 데이터 오염을 방지합니다.
    /// ARDUINO 환경에서는 FreeRTOS 세마포어를, 그 외에는 std::mutex를 사용합니다.
    void lock() const {
#ifdef ARDUINO
        if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
#else
        _mutex.lock();
#endif
    }

    /// 뮤텍스를 해제하여 임계 영역에서 나옵니다.
    void unlock() const {
#ifdef ARDUINO
        if (_mutex) xSemaphoreGive(_mutex);
#else
        _mutex.unlock();
#endif
    }

#ifdef ARDUINO
    /// FreeRTOS 환경에서 사용하는 뮤텍스 제어 핸들.
    mutable SemaphoreHandle_t _mutex = nullptr;
#else
    /// 표준 C++ 환경에서 사용하는 뮤텍스 객체.
    mutable std::mutex _mutex;
#endif

    /// 실제 데이터 저장 및 인덱스 관리를 담당하는 내부 큐 객체.
    Queue<T, N> _queue;
};

} // namespace cms
