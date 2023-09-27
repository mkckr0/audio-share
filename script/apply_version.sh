pushd $(dirname $0)/.. &>/dev/null

version_name=$(sed -n '1p' VERSION)
version_code=$(sed -n '2p' VERSION)

echo VERSION: version_name=$version_name version_code=$version_code

sed -Ei "s|versionName\s*=\s*\"([^\"]*)\"|versionName = \"$version_name\"|g" audio-share-app/app/build.gradle.kts
sed -Ei "s|versionCode\s*=\s*([0-9]*)|versionCode = $version_code|g" audio-share-app/app/build.gradle.kts

sed -Ei "s|Audio Share Server, Version ([^\"]*)|Audio Share Server, Version $version_name|g" audio-share-server/AudioShareServer.rc

popd &>/dev/null