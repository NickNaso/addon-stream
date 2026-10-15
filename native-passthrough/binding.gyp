{
  "targets": [
    {
      "target_name": "native_passthrough",
      "sources": [
        "src/binding.cc",
        "src/passthrough.cc"
      ],
      "defines": [ "NAPI_VERSION=8" ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').targets\"):node_addon_api_except"
      ]
    }
  ]
}
