{
  "targets": [
    {
      "target_name": "node",
      "type": "static_library",
      "sources": [
        "node.cpp"
      ],
    },
    {
      "target_name": "catch-test",
      "type": "executable",
      "include_dirs": [
        "node_modules/catch/single_include",
        "."
      ],
      "sources": [
        "catch/basic.cpp"
      ],
      "dependencies": [
        "node"
      ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          }
        }]
      ]
    }
  ]
}
