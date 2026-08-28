#!/bin/bash
pushd $(dirname $0)/.. &>/dev/null

version_name=$(bash ./scripts/get_version.sh -n)
version_code=$(bash ./scripts/get_version.sh -c)

echo VERSION: \
    version_name=$version_name \
    version_code=$version_code

if [ -f "android-app/app/build.gradle.kts" ]; then
    sed -Ebi "s|versionName\s*=\s*\"[^\"]*\"|versionName = \"$version_name\"|g" android-app/app/build.gradle.kts
    sed -Ebi "s|versionCode\s*=\s*[0-9]*|versionCode = $version_code|g" android-app/app/build.gradle.kts
fi

if [ -f "server/CMakeLists.txt" ]; then
    sed -Ebi "s|\tVERSION\s+[0-9.]*|\tVERSION $version_name|g" server/CMakeLists.txt
fi

popd &>/dev/null
