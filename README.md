# How to use Stream on Node.js Native Add-ons

How to use and create stream on Node.js native add-ons with N-API

>A stream is an abstract interface for working with streaming data in Node.js. 
>The stream module provides a base API that makes it easy to build objects that 
>implement the stream interface.

There are many stream objects provided by Node.js. For instance, a **[request to an 
HTTP server](https://nodejs.org/dist/latest/docs/api/http.html#http_class_http_incomingmessage)**
and **[process.stdout](https://nodejs.org/dist/latest-v9.x/docs/api/process.html#process_process_stdout)**
are both stream instances.

Streams can be readable, writable, or both. All streams are instances of 
**[EventEmitter](https://nodejs.org/dist/latest/docs/api/events.html)**.

Here you can find an examples where I used a Transform stream to pass and get
back data from a native add-on. This is a simple passthrough example.

In the passthrough directory, run:

```text
$ npm install
$ node-gyp rebuild
$ node index.js or npm start
```

## Examples

* **[Passthrough](/passthrough)**

### Node.js C / C ++ Addons

#### [Documenttation of Node.js Native Addons](https://nodejs.org/dist/latest/docs/api/addons.html)

### N-API 

#### [Documentation for N-API](https://nodejs.org/dist/latest/docs/api/n-api.html)

### Node Addon API
#### Header-only C++ wrapper classes for the ABI-stable Node.js API
#### [Node.js API (N-API) Package](https://github.com/nodejs/node-addon-api/)

### Web resources

* **[How I ported bcrypt to new N-API](https://medium.com/the-node-js-collection/how-i-ported-bcrypt-to-new-n-api-d0b8c9fe6136)**
* **[N-API the next API for Native Addons](https://youtu.be/-Oniup60Afs)**
* **[N-API Next generation Node API for native modules](https://www.slideshare.net/michaeldawson3572846/n-apinode-summit2017final)**
* **[Speed up Your Node.js App with Native Addons](https://medium.com/the-node-js-collection/speed-up-your-node-js-app-with-native-addons-5e76a06f4a40)**
* **[Start with N-API](https://hackernoon.com/n-api-and-getting-started-with-writing-c-addons-for-node-js-cf061b3eae75)**
* **[How to get a performance boost using Node.js native addons](https://medium.com/developers-writing/how-to-get-a-performance-boost-using-node-js-native-addons-fd3a24719c85)**

### Nicola Del Gobbo

<https://github.com/NickNaso/>

<https://www.npmjs.com/~nicknaso>

<https://twitter.com/NickNaso>

## Acknowledgements

Thank you to all people that encourage me every day.

## License

Licensed under [Apache license V2](./LICENSE)