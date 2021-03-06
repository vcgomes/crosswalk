{
  'targets': [
  {
    'target_name': 'echo_extension',
    'type': 'loadable_module',
    'include_dirs': [
      '../..',
    ],
    'sources': [
      'test/echo_extension.c',
    ],
    'product_dir': '<(PRODUCT_DIR)/tests/extension/echo_extension/'
  },
  {
    'target_name': 'bad_extension',
    'type': 'loadable_module',
    'include_dirs': [
      '../..',
    ],
    'sources': [
      'test/bad_extension.c',
    ],
    'product_dir': '<(PRODUCT_DIR)/tests/extension/bad_extension/'
  },
  {
    'target_name': 'multiple_entry_points_extension',
    'type': 'loadable_module',
    'include_dirs': [
      '../..',
    ],
    'sources': [
      'test/multiple_entry_points_extension.c',
    ],
    'product_dir': '<(PRODUCT_DIR)/tests/extension/multiple_extension/'
  },
  ],
}
