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

const test = require('node:test')
const assert = require('node:assert')
const crypto = require('crypto')
const { once } = require('events')
const { Readable } = require('stream')
const { pipeline } = require('stream/promises')
const { setTimeout } = require('timers/promises')
const { PassThrough } = require('bindings')('native_passthrough')
const NativePassThrough = require('./native-passthrough')

function split(buffer, size) {
    const chunks = []
    for (let i = 0; i < buffer.length; i += size) {
        chunks.push(buffer.subarray(i, i + size))
    }
    return chunks
}

async function roundTrip(input, chunkSize, opts) {
    const output = []
    await pipeline(
        Readable.from(split(input, chunkSize)),
        new NativePassThrough(opts),
        async function (source) {
            for await (const chunk of source) output.push(chunk)
        }
    )
    return Buffer.concat(output)
}

test('passes the data through unchanged', async () => {
    const input = crypto.randomBytes(1024 * 1024)
    assert.deepStrictEqual(await roundTrip(input, 1000), input)
})

test('works with a highWaterMark of 0', async () => {
    const input = crypto.randomBytes(16 * 1024)
    assert.deepStrictEqual(await roundTrip(input, 100, { highWaterMark: 0 }), input)
})

test('keeps independent streams apart', async () => {
    const inputs = Array.from({ length: 8 }, () => crypto.randomBytes(256 * 1024))
    const outputs = await Promise.all(inputs.map((input, i) => roundTrip(input, 512 * (i + 1))))
    outputs.forEach((output, i) => assert.deepStrictEqual(output, inputs[i]))
})

test('pushes data asynchronously', async () => {
    const stream = new NativePassThrough()
    let received = false
    stream.on('data', () => { received = true })
    stream.write(Buffer.from('hello'))
    assert.strictEqual(received, false)
    const [chunk] = await once(stream, 'data')
    assert.strictEqual(chunk.toString(), 'hello')
    stream.destroy()
})

test('applies backpressure when the consumer is slow', async () => {
    const hwm = 1024
    const chunkSize = 256
    const input = crypto.randomBytes(32 * 1024)
    const stream = new NativePassThrough({ highWaterMark: hwm })
    let maxNative = 0
    let maxReadable = 0
    let backpressured = false
    const sample = () => {
        maxNative = Math.max(maxNative, stream._handle.bufferedBytes)
        maxReadable = Math.max(maxReadable, stream.readableLength)
    }

    const produce = async () => {
        for (const chunk of split(input, chunkSize)) {
            if (!stream.write(chunk)) {
                backpressured = true
                await once(stream, 'drain')
            }
            sample()
        }
        stream.end()
    }
    const consume = async () => {
        const output = []
        for await (const chunk of stream) {
            sample()
            output.push(chunk)
            await setTimeout(1)
        }
        return Buffer.concat(output)
    }

    const [, output] = await Promise.all([produce(), consume()])
    assert.ok(backpressured, 'write() never returned false')
    assert.ok(maxNative < hwm + chunkSize, `native queue grew to ${maxNative} bytes`)
    assert.ok(maxReadable < hwm + chunkSize, `readable buffer grew to ${maxReadable} bytes`)
    assert.deepStrictEqual(output, input)
})

test('stops processing while nobody reads and finishes after the data is consumed', async () => {
    const hwm = 1024
    const chunkSize = 256
    const input = crypto.randomBytes(16 * 1024)
    const stream = new NativePassThrough({ highWaterMark: hwm })
    for (const chunk of split(input, chunkSize)) stream.write(chunk)
    stream.end()

    await setTimeout(50)
    assert.ok(stream.readableLength < hwm + chunkSize)
    assert.ok(stream._handle.bufferedBytes < hwm + chunkSize)
    assert.ok(stream.writableLength > 0, 'the rest should wait in the writable buffer')
    assert.strictEqual(stream.writableFinished, false)

    const output = Buffer.concat(await stream.toArray())
    assert.deepStrictEqual(output, input)
    assert.strictEqual(stream.writableFinished, true)
})

test('can be destroyed while chunks are being processed', async () => {
    const stream = new NativePassThrough({ highWaterMark: 1024 })
    const results = []
    for (let i = 0; i < 32; i++) {
        stream.write(Buffer.alloc(256, i), (err) => results.push(err))
    }
    stream.on('data', () => assert.fail('data after destroy'))
    stream.destroy()
    await once(stream, 'close')
    // Give the chunk still on the thread pool the time to come back.
    await setTimeout(20)
    assert.strictEqual(results.length, 32, 'every write callback must be called')
    for (const err of results) {
        assert.ok(err == null || err.code === 'ERR_STREAM_DESTROYED', err)
    }
})

test('forwards the error passed to destroy()', async () => {
    const stream = new NativePassThrough()
    stream.write(Buffer.alloc(1024))
    stream.destroy(new Error('boom'))
    const [err] = await once(stream, 'error')
    assert.strictEqual(err.message, 'boom')
})

test('the native handle validates its input', () => {
    assert.throws(() => new PassThrough(), TypeError)
    assert.throws(() => new PassThrough(-1), RangeError)
    const handle = new PassThrough(16)
    assert.throws(() => handle.write('not a buffer'), TypeError)
    handle.end()
    assert.throws(() => handle.write(Buffer.from('late')), /write after end/)
})
