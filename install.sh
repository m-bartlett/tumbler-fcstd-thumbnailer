#!/usr/bin/env bash
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -B build
cmake --build build
sudo mv -v ./build/tumbler-dir-thumbnailer.so /usr/lib/tumbler-1/plugins/