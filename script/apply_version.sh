pushd $(dirname $0)/.. &>/dev/null

version_name=$(sed -n '1p' VERSION)
version_code=$(sed -n '2p' VERSION)

echo VERSION: version_name=$version_name version_code=$version_code

echo audio-share-app/app/build.gradle.kts: \
    $(sed -En 's|versionName\s*=\s*"([^"]*)"|\0|p' audio-share-app/app/build.gradle.kts) \
    $(sed -En 's|versionCode\s*=\s*([0-9]*)|\0|p' audio-share-app/app/build.gradle.kts)

sed -Ei "s|versionName\s*=\s*\"([^\"]*)\"|versionName = \"$version_name\"|g" audio-share-app/app/build.gradle.kts
sed -Ei "s|versionCode\s*=\s*([0-9]*)|versionCode = $version_code|g" audio-share-app/app/build.gradle.kts

echo $(sed -En 's|"Audio Share Server|\0|p' audio-share-server/AudioShareServer.rc)

popd &>/dev/null