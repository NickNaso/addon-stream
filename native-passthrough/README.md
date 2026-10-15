# Native PassThrough stream

A `PassThrough` stream whose data is processed by a native add-on, with
**backpressure** handled in both directions and the processing run
**asynchronously** on the libuv thread pool.

The [passthrough](../passthrough) example calls the add-on synchronously from
`_transform()`: the native side keeps no state, can't do slow work without
blocking the event loop and can't control the pace of data. Here the native
side behaves like a real stream: it has its own bounded buffer, works off the
main thread and tells JavaScript when to slow down.

## Run

```text
$ npm install
$ node-gyp rebuild
$ npm start
$ npm test
```

`npm install` already builds the add-on (`gypfile: true`). Like every node-gyp
build it needs Python and a C++ toolchain.

## How it works

```text
             JavaScript (main thread)                  |   libuv thread pool
                                                       |
 producer --write()--> Duplex._write() --write()--> [native queue] --> Process()
         <--false----- callback held   <--false----    |   (one chunk at a time)
         <--'drain'--- callback called <--onDrain()--  |
                                                       |
 consumer <--'data'--- Duplex push()   <--onData()---- result, back on the main thread
          --read()---> Duplex._read()  --resume()--->  processing restarts
```

* **Writable side.** `write()` on the native handle copies the chunk into a
  queue and returns `false` once the queue reaches the high water mark. The
  stream then holds the `_write()` callback, so its writable buffer fills up
  and `write()` returns `false` to the producer. The native side calls
  `onDrain()` when there is room again.
* **Readable side.** Every processed chunk goes to `onData()`, which returns
  the value of `push()`. When it is `false` the native side stops processing
  until `_read()` calls `resume()`.
* **Asynchronous processing.** Each chunk is processed by a `Napi::AsyncWorker`
  on the thread pool, the same approach `zlib` uses in Node.js core. Only one
  chunk per stream is in flight, so the output keeps the input order, while
  JavaScript can keep filling the queue in the meantime.
* **End and errors.** `_final()` calls `end()`, and `onEnd()` fires only after
  every queued chunk has been pushed, so `'finish'` waits for the data to get
  through. An exception thrown by `Process()` reaches JavaScript through
  `onError()` and destroys the stream.
* **No locks, no leaks.** The native state is only touched on the main thread;
  the worker thread sees nothing but its own chunk. The handle finds the
  callbacks as properties of its own JS object instead of keeping persistent
  references to them, so the stream and the handle are collected together.
  While a chunk is being processed the worker keeps a reference to the handle.

## Files

* [src/passthrough.cc](src/passthrough.cc) - the native stream: queue, flow
  control and the `AsyncWorker`.
* [native-passthrough.js](native-passthrough.js) - the `Duplex` that adapts the
  native handle to the Node.js stream API.
* [index.js](index.js) - copies `nodejs.png` through the stream.
* [test.js](test.js) - data integrity, backpressure, end and destroy.

## Use it for real work

Change `PassThrough::Process()` in [src/passthrough.cc](src/passthrough.cc).
It runs on a worker thread: it gets the chunk bytes and returns the bytes to
push, which may have a different size or be empty. It must not touch
JavaScript values; throw a `std::exception` to fail the stream.
