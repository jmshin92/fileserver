# File server
## 개요
파일을 업로드, 다운로드, 리스트 할 수 있는 서버를 만든다.
## 요구사항
### Client
* 단일 프로세스
* 사용자의 명령어를 입력받을 cli 인터페이스 구현
* File server와 TCP 통신을 사용
* File server와의 message protocol 정의
* Message serialize, deserialize 기능 구현
* 명령어 기능 구현
* put: 파일을 업로드한다. 이 때, 파일의 이름, 소유자, 사이즈등의 메타정보도 저장한다.
* get: 파일을 다운로드한다. 
* list: 서버에 업로드된 모든 파일의 목록을 받는다. 이 때, 저장한 메타정보를 받는다.
### File server
* 단인 프로세스
* event-driven 구조로 구현
* 하나의 control thread(cthr)을 생성한다. 이 쓰레드가 event manager가 된다.
* 다수의 worker thread(wthr)을 생성한다. 이 쓰레드는 작업 단위(job)을 수행한다.
* 하나의 suspend thread(sthr)을 생성한다. 이 쓰레드는 async I/O의 suspend를 담당한다.
* 작업 단위(job) 정의
* 작업 단위는 I/O 작업을 기준으로 큰 작업을 나눈다.
* 작업 단위는 cthr이 wthr에게 스케쥴링한다.
* 스케쥴링의 조건은 따로 정하지 않으며 수행 시작 순서만 보장한다.
* 대용량의 파일일 경우 메모리에 올려놓고 한번에 메세지를 보낼 수 없다.
* 적절한 sharding을 통해 메모리 사이즈보다 큰 데이터를 처리할 수 있어야한다.
* 파일은 striping과 mirroring이 되어 저장된다.
* 파일의 메타데이터를 가진다.
* 메타데이터는 다수의 쓰레드가 접근하며 파일로 저장되어있어야 한다.
