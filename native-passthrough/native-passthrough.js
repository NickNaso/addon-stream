/*******************************************************************************
 * Copyright (c) 2026 Nicola Del Gobbo
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the license at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY
 * IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
 * MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 *
 * Contributors - initial API implementation:
 * Nicola Del Gobbo <nicoladelgobbo@gmail.com>
 ******************************************************************************/

'use strict'

const { Duplex } = require('stream')
const { PassThrough } = require('bindings')('native_passthrough')

/**
 * A PassThrough stream backed by a native add-on.
 *
 * Every chunk is processed on the libuv thread pool and backpressure is
 * honoured in both directions:
 *  - writable side: when the native queue is full the _write() callback is
 *    held until the native side calls onDrain(), so write() returns false to
 *    the producer;
 *  - readable side: when push() returns false the native side stops
 *    processing until _read() calls resume().
 */
class NativePassThrough extends Duplex {

    constructor(opts = {}) {
        // The native side only deals with bytes.
        super({
            ...opts,
            objectMode: false,
            readableObjectMode: false,
            writableObjectMode: false
        })
        this._writeCb = null
        this._finalCb = null
        this._handle = new PassThrough(this.writableHighWaterMark)
        // The handle looks these up on its own object each time it calls
        // back, so it never holds a strong reference to the stream.
        this._handle.onData = (chunk) => this.push(chunk)
        this._handle.onDrain = () => {
            const cb = this._writeCb
            this._writeCb = null
            cb()
        }
        this._handle.onEnd = () => {
            this.push(null)
            const cb = this._finalCb
            this._finalCb = null
            cb()
        }
        this._handle.onError = (err) => this.destroy(err)
    }

    _write(chunk, encoding, cb) {
        if (this._handle.write(chunk)) {
            cb()
        } else {
            // The native queue is full: holding the callback lets the
            // writable buffer fill up, so the producer is told to wait.
            this._writeCb = cb
        }
    }

    _final(cb) {
        // Every chunk has been handed to the native side: onEnd() fires once
        // all of them have been processed and pushed.
        this._finalCb = cb
        this._handle.end()
    }

    _read(size) {
        this._handle.resume()
    }

    _destroy(err, cb) {
        this._handle.destroy()
        const writeCb = this._writeCb
        this._writeCb = null
        this._finalCb = null
        // Completing the pending write lets the Writable state machine settle
        // the chunks still in its buffer.
        if (writeCb) writeCb(err)
        cb(err)
    }

}

module.exports = NativePassThrough
