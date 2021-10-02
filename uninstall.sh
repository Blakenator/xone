#!/usr/bin/env sh

if [ "$(id -u)" -ne 0 ]; then
    echo 'This script must be run as root!' >&2
    exit 1
fi

modules=$(lsmod | grep '^xone_' | cut -d ' ' -f 1 | tr '\n' ' ')
version=$(dkms status xone | head -n 1 | tr -s ',:/' ' ' | cut -d ' ' -f 2)

if [ -n "$modules" ]; then
    echo "Unloading modules: $modules..."
    # shellcheck disable=SC2086
    modprobe -r -a $modules
fi

if [ -n "$version" ]; then
    echo "Uninstalling xone $version..."
    if dkms remove xone -v "$version" --all; then
        rm -r "/usr/src/xone-$version"
        rm -f /etc/modprobe.d/xone-blacklist.conf
    fi
else
    echo 'Driver is not installed!' >&2
fi
