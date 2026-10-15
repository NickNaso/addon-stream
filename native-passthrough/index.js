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

const fs = require('fs')
const path = require('path')
const { pipeline } = require('stream/promises')
const NativePassThrough = require('./native-passthrough')

const FILE_TO_READ = 'nodejs.png'
const FILE_TO_WRITE = 'nodejs-passthrough-copy.png'

async function main() {
    // pipeline() forwards backpressure and errors and destroys every stream
    // on failure, unlike pipe(). The small read chunks make the file go
    // through the native stream in several steps.
    await pipeline(
        fs.createReadStream(path.join(__dirname, FILE_TO_READ), { highWaterMark: 4 * 1024 }),
        new NativePassThrough({ highWaterMark: 8 * 1024 }),
        fs.createWriteStream(path.join(__dirname, FILE_TO_WRITE))
    )
    console.log(`${FILE_TO_READ} copied to ${FILE_TO_WRITE} through the native stream`)
}

main().catch((err) => {
    console.error(err)
    process.exitCode = 1
})
