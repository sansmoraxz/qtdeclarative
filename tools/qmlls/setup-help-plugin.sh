#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/qmlls/setup-help-plugin.sh [options]

Build a local qmlls help plugin for distributions that ship qmlls and .qch docs
but do not install QtTools' help/libhelpplugin.so.

Options:
  --qmlls PATH        qmlls binary to wrap
  --build-dir PATH    build directory for the helper plugin
  --wrapper PATH      output wrapper path
  --generator NAME    cmake generator to use
  -h, --help          show this help
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

qmlls_path="${repo_root}/build-qmlls/lib/qt6/bin/qmlls"
build_dir="${repo_root}/build-qmlls-help-plugin"
wrapper_path=""
generator=""

while (($#)); do
    case "$1" in
    --qmlls)
        qmlls_path="$2"
        shift 2
        ;;
    --build-dir)
        build_dir="$2"
        shift 2
        ;;
    --wrapper)
        wrapper_path="$2"
        shift 2
        ;;
    --generator)
        generator="$2"
        shift 2
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        echo "Unknown option: $1" >&2
        usage >&2
        exit 1
        ;;
    esac
done

if [[ ! -x "${qmlls_path}" ]]; then
    echo "qmlls binary not found or not executable: ${qmlls_path}" >&2
    exit 1
fi

if [[ -z "${wrapper_path}" ]]; then
    wrapper_path="${build_dir}/qmlls-with-help"
fi

cmake_args=(
    -S "${repo_root}/tools/qmlls/helpplugin"
    -B "${build_dir}"
    -DQTDECLARATIVE_SOURCE_DIR="${repo_root}"
)

if [[ -n "${generator}" ]]; then
    cmake_args+=(-G "${generator}")
elif command -v ninja >/dev/null 2>&1; then
    cmake_args+=(-GNinja)
fi

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 1)"

cmake "${cmake_args[@]}"
cmake --build "${build_dir}" --parallel "${jobs}"

plugin_root="${build_dir}/plugins"
plugin_path="${plugin_root}/help/libhelpplugin.so"

if [[ ! -f "${plugin_path}" ]]; then
    echo "Expected plugin was not built: ${plugin_path}" >&2
    exit 1
fi

mkdir -p "$(dirname "${wrapper_path}")"

cat > "${wrapper_path}" <<EOF
#!/usr/bin/env bash
set -euo pipefail

export QT_PLUGIN_PATH="${plugin_root}\${QT_PLUGIN_PATH:+:\${QT_PLUGIN_PATH}}"

exec "${qmlls_path}" "\$@"
EOF

chmod +x "${wrapper_path}"

docs_dir=""
if command -v qmake6 >/dev/null 2>&1; then
    docs_dir="$(qmake6 -query QT_INSTALL_DOCS 2>/dev/null || true)"
fi

echo "Built local qmlls help plugin:"
echo "  ${plugin_path}"
echo
echo "Wrapper launcher:"
echo "  ${wrapper_path}"

if [[ -n "${docs_dir}" ]]; then
    echo
    echo "Detected Qt docs dir from qmake6:"
    echo "  ${docs_dir}"
fi

echo
echo "VS Code settings:"
echo "  \"qt-qml.qmlls.customExePath\": \"${wrapper_path}\""
if [[ -n "${docs_dir}" ]]; then
    echo "  \"qt-qml.qmlls.customDocsPath\": \"${docs_dir}\""
fi
