# QMLLS Help Plugin Workaround

Some distributions ship `qmlls`, `Qt6Help`, and Qt `.qch` documentation files, but do not
install the QtTools help plugin `help/libhelpplugin.so`.

When that plugin is missing, `qmlls` can still resolve definitions from `.qmltypes`, but hover
documentation for builtins such as `Qt.resolvedUrl()` falls back to signatures only.

## Prerequisites

- a `qmlls` binary to wrap, for example `build-qmlls/lib/qt6/bin/qmlls`
- `Qt6Help` development/runtime files
- Qt `.qch` docs, for example `qt6-doc` on Arch
- `cmake`
- a C++ compiler
- `ninja` is optional

## Build The Workaround

Run:

```bash
./tools/qmlls/setup-help-plugin.sh
```

This builds a local `libhelpplugin.so` under `build-qmlls-help-plugin/plugins/help/` and writes a
wrapper launcher at `build-qmlls-help-plugin/qmlls-with-help`.

If your `qmlls` binary lives somewhere else:

```bash
./tools/qmlls/setup-help-plugin.sh --qmlls /path/to/qmlls
```

## Editor Setup

Point your editor at the generated wrapper instead of the raw `qmlls` binary.

For VS Code with `theqtcompany.qt-qml`:

```json
{
  "qt-qml.qmlls.customExePath": "/absolute/path/to/build-qmlls-help-plugin/qmlls-with-help",
  "qt-qml.qmlls.customDocsPath": "/path/to/qt/doc/dir"
}
```

`customDocsPath` must point at the directory that directly contains the `.qch` files, such as
`qtqml.qch` and `qtquick.qch`.

## Notes

- The helper plugin source lives in `tools/qmlls/helpplugin/`.
- The wrapper only prepends the generated plugin directory to `QT_PLUGIN_PATH`.
- Re-run the script after cleaning the helper build directory or moving the wrapped `qmlls`
  binary.
